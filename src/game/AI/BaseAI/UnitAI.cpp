/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "UnitAI.h"
#include "Entities/Creature.h"
#include "Server/DBCStores.h"
#include "Spells/Spell.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"
#include "Spells/SpellMgr.h"
#include "World/World.h"
#include "MotionGenerators/MovementGenerator.h"
#include "Globals/ObjectMgr.h"
#include <limits>

static_assert(MAXIMAL_AI_EVENT_EVENTAI <= 32, "Maximal 32 AI_EVENTs supported with EventAI");

UnitAI::UnitAI(Unit* unit) : UnitAI(unit, 0)
{
}

// 构造函数：初始化默认追击参数、施法者 AI 状态并注册内置距离/法术列表冷却定时器
UnitAI::UnitAI(Unit* unit, uint32 combatActions) :
    CombatActions(combatActions),
    m_unit(unit),
    m_attackDistance(0.0f),
    m_attackAngle(0.0f),
    m_moveFurther(false),
    m_chaseRun(true),
    m_visibilityDistance(VISIBLE_RANGE),
    m_combatMovementStarted(false),
    m_dismountOnAggro(true),
    m_meleeEnabled(true),
    m_combatOnlyRoot(false),
    m_reactState(REACT_AGGRESSIVE),
    m_combatScriptHappening(false),
    m_currentAIOrder(ORDER_NONE),
    m_currentSpell(nullptr), m_teleportUnreachable(false),
    // Caster AI components (施法者 AI 参数初始化)
    m_rangedMode(false), m_rangedModeSetting(TYPE_NONE), m_chaseDistance(0.f), m_currentRangedMode(false),
    m_mainSpellId(0), m_mainSpellCost(0), m_mainSpellInfo(nullptr), m_mainSpellMinRange(0.f),
    m_mainAttackMask(SPELL_SCHOOL_MASK_NONE), m_distancingCooldown(false), m_spellListCooldown(true)
{
    AddCustomAction(GENERIC_ACTION_DISTANCE, true, [&]() { m_distancingCooldown = false; });
    AddCustomAction(GENERIC_ACTION_SPELL_LIST, true, [&]() { m_spellListCooldown = false; });
}

UnitAI::~UnitAI()
{
}

// 视野内单位移动检测：处理主动求援协助检查、潜行警觉以及主动怪索敌开战
void UnitAI::MoveInLineOfSight(Unit* who)
{
    if (GetReactState() < REACT_DEFENSIVE)
        return;

    if (!m_unit->CanFly() && who->IsFlying())
        if (m_unit->GetDistanceZ(who) > (IsRangedUnit() ? CREATURE_Z_ATTACK_RANGE_RANGED : CREATURE_Z_ATTACK_RANGE_MELEE))
            return;

    if (m_unit->GetVictim() && !m_unit->GetMap()->IsDungeon())
        return;

    if (m_unit->IsNeutralToAll())
        return;

    if (who->GetObjectGuid().IsCreature() && who->IsInCombat())
        CheckForHelp(who, m_unit, sWorld.getConfig(CONFIG_FLOAT_CREATURE_CHECK_FOR_HELP_RADIUS));

    if (!HasReactState(REACT_AGGRESSIVE)) // mobs who are aggressive can still assist (仅主动姿态继续后续主动索敌)
        return;

    if (!m_unit->CanInitiateAttack())
        return;

    if (AssistPlayerInCombat(who))
        return;

    if (!m_unit->CanAttackOnSight(who))
        return;

    if (!who->isInAccessablePlaceFor(m_unit))
        return;

    DetectOrAttack(who);
}

// 进入战斗时启用法术列表并设置开场技能初始冷却
void UnitAI::EnterCombat(Unit*)
{
    if (!GetSpellList().Disabled && !GetSpellList().Spells.empty())
    {
        m_spellListCooldown = false;
        AddInitialCooldowns();
    }
}

// 脱离战斗：清除仅战斗定身、移除脱战应清除的光环、停止战斗并跑回出生点
void UnitAI::EnterEvadeMode()
{
    ClearCombatOnlyRoot();
    m_unit->RemoveAllAurasOnEvade();
    m_unit->CombatStopWithPets(true);

    bool triggerHome = false;

    // only alive creatures that are not on transport can return to home position (仅存活且未搭乘交通工具的单位跑回出生点；若已被永久定身则直接触发归位事件)
    if (GetReactState() != REACT_PASSIVE && m_unit->IsAlive() && !m_unit->IsBoarded())
    {
        if (!m_unit->IsImmobilizedState()) // if still rooted after aura removal - permarooted
            m_unit->GetMotionMaster()->MoveTargetedHome();
        else
            triggerHome = true;
    }

    m_unit->TriggerEvadeEvents();

    if (triggerHome)
        m_unit->TriggerHomeEvents();
}

// 死亡时清理仅战斗期间生效的定身状态
void UnitAI::JustDied(Unit* /*killer*/)
{
    ClearCombatOnlyRoot();
}

// 遭到攻击时若未处于战斗中且无目标，则反击攻击者
void UnitAI::AttackedBy(Unit* attacker)
{
    if (!m_unit->IsInCombat() && !m_unit->GetVictim())
        AttackStart(attacker);
}

// 校验目标生物类型是否符合法术要求及是否重复激活下一击近战技能
CanCastResult UnitAI::CanCastSpell(Unit* target, const SpellEntry* spellInfo, bool isTriggered) const
{
    // If not triggered, we check
    if (!isTriggered)
    {
        if (target && !Spell::CheckTargetCreatureType(target, spellInfo))
            return CAST_FAIL_OTHER;

        // already active next melee swing spell (若同 ID 下一击近战法术已激活则避免重复挂起)
        if (IsNextMeleeSwingSpell(spellInfo))
            if (Spell* autorepeatSpell = m_unit->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
                if (autorepeatSpell->m_spellInfo->Id == spellInfo->Id)
                    return CAST_FAIL_OTHER;
    }
    return CAST_OK;
}

// 按 CastFlags 尝试施放法术，将 SpellCastResult 映射为 CanCastResult 并触发施法结果回调
CanCastResult UnitAI::DoCastSpellIfCan(Unit* target, uint32 spellId, uint32 castFlags)
{
    Unit* caster = m_unit;

    if (target)
    {
        if (castFlags & CAST_SWITCH_CASTER_TARGET)
            std::swap(caster, target);

        if (castFlags & CAST_FORCE_TARGET_SELF)
            caster = target;
    }
    else if (castFlags & (CAST_FORCE_TARGET_SELF | CAST_SWITCH_CASTER_TARGET))
        return CAST_FAIL_OTHER;

    if (GetAIOrder() == ORDER_EVADE && !(castFlags & CAST_TRIGGERED))
        return CAST_FAIL_EVADE;

    // Allowed to cast only if not casting (unless we interrupt ourself) or if spell is triggered (未在施法、或带有打断/触发标志时才允许施放)
    if (!caster->IsNonMeleeSpellCasted(false) || (castFlags & (CAST_TRIGGERED | CAST_INTERRUPT_PREVIOUS)))
    {
        if (const SpellEntry* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId))
        {
            // If cast flag CAST_AURA_NOT_PRESENT is active, check if target already has aura on them (若带有不重复叠加光环标志则先检查光环)
            if (castFlags & CAST_AURA_NOT_PRESENT)
            {
                if (!target)
                {
                    if (caster->HasAura(spellId))
                        return CAST_FAIL_TARGET_AURA;
                }
                else if (target->HasAura(spellId))
                    return CAST_FAIL_TARGET_AURA;
            }

            // Check if cannot cast spell
            if (!(castFlags & (CAST_FORCE_TARGET_SELF | CAST_FORCE_CAST)))
            {
                CanCastResult castResult = CanCastSpell(target, spellInfo, (castFlags & CAST_TRIGGERED) != 0);

                if (castResult != CAST_OK)
                    return castResult;
            }

            // Interrupt any previous spell
            if (castFlags & CAST_INTERRUPT_PREVIOUS && caster->IsNonMeleeSpellCasted(false))
                caster->InterruptNonMeleeSpells(false);

            // Creature should interrupt any current melee spell
            caster->InterruptSpell(CURRENT_MELEE_SPELL);

            // Creature should stop wielding weapon while casting
            // caster->SetSheath(SHEATH_STATE_UNARMED);

            uint32 flags = (castFlags & CAST_TRIGGERED ? TRIGGERED_OLD_TRIGGERED : TRIGGERED_NONE) | (castFlags & CAST_IGNORE_UNSELECTABLE_TARGET ? TRIGGERED_IGNORE_UNSELECTABLE_FLAG : TRIGGERED_NONE);

            if (flags == TRIGGERED_NONE)
                flags |= TRIGGERED_NORMAL_COMBAT_CAST;

            SpellCastResult result;
            if (castFlags & CAST_ONLY_XYZ)
                result = caster->CastSpell(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), spellInfo, flags);
            else
                result = caster->CastSpell(target, spellInfo, flags);
            if (result != SPELL_CAST_OK)
            {
                CanCastResult canCastResult = CAST_FAIL_OTHER;
                switch (result) // temporary adapter (将底层 SpellCastResult 适配转换至 CanCastResult)
                {
                    case SPELL_FAILED_OUT_OF_RANGE:
                        canCastResult = CAST_FAIL_TOO_FAR;
                        break;
                    case SPELL_FAILED_TOO_CLOSE:
                        canCastResult = CAST_FAIL_TOO_CLOSE;
                        break;
                    case SPELL_FAILED_LINE_OF_SIGHT:
                        canCastResult = CAST_FAIL_NOT_IN_LOS;
                        break;
                    case SPELL_FAILED_PACIFIED:
                    case SPELL_FAILED_SILENCED:
                        canCastResult = CAST_FAIL_CAST_PREVENTED;
                        break;
                    case SPELL_FAILED_PREVENTED_BY_MECHANIC:
                        if (!caster->IsCrowdControlled())
                        {
                            canCastResult = CAST_FAIL_CAST_PREVENTED;
                            break;
                        }
                        [[fallthrough]];
                    case SPELL_FAILED_STUNNED:
                    case SPELL_FAILED_CONFUSED:
                    case SPELL_FAILED_FLEEING:
                        canCastResult = CAST_FAIL_STATE;
                        break;
                    case SPELL_FAILED_NOT_READY:
                        canCastResult = CAST_FAIL_COOLDOWN;
                        break;
                    case SPELL_FAILED_NO_POWER:
                        canCastResult = CAST_FAIL_POWER;
                        break;
                    case SPELL_FAILED_CASTER_AURASTATE: // valid - doesnt need logging (常规条件不满足，无需刷屏日志)
                    case SPELL_FAILED_TARGET_AURASTATE:
                    case SPELL_FAILED_TARGET_NO_WEAPONS:
                    case SPELL_FAILED_BAD_TARGETS:
                    case SPELL_FAILED_DONT_REPORT:
                    case SPELL_FAILED_NOT_BEHIND:
                        canCastResult = CAST_FAIL_MISCELLANEOUS;
                        break;
                    default: break;
                }

                if (canCastResult == CAST_FAIL_OTHER)
                    sLog.outBasic("DoCastSpellIfCan by %s attempt to cast spell %u but spell failed due to unknown result %u : %s.", m_unit->GetObjectGuid().GetString().c_str(), spellId, result, GetSpellCastResultString(result));
                return HandleSpellCastResult(canCastResult, spellInfo);
            }
            return HandleSpellCastResult(CAST_OK, spellInfo);
        }
        sLog.outErrorDb("DoCastSpellIfCan by %s attempt to cast spell %u but spell does not exist.", m_unit->GetGuidStr().c_str(), spellId);
        return CAST_FAIL_OTHER;
    }
    return CAST_FAIL_IS_CASTING;
}

// 开始攻击并建立与目标的战斗关系及追击移动
void UnitAI::AttackStart(Unit* who)
{
    if (!who || HasReactState(REACT_PASSIVE))
        return;

    bool targetChange = m_unit->GetVictim() != nullptr && m_unit->GetVictim() != who;
    if (m_unit->Attack(who, m_meleeEnabled))
    {
        m_unit->EngageInCombatWith(who);
        HandleMovementOnAttackStart(who, targetChange);
    }
}

// 若处于近战攻击状态且无高优先级移动命令，推进近战平砍状态机
bool UnitAI::DoMeleeAttackIfReady() const
{
    return m_unit->hasUnitState(UNIT_STAT_MELEE_ATTACKING) && GetAIOrder() == ORDER_NONE && m_unit->UpdateMeleeAttackingState();
}

// 开启或关闭战斗追击移动，并按需立即启动或停止当前 Chase 移动
void UnitAI::SetCombatMovement(bool enable, bool stopOrStartMovement /*=false*/)
{
    if (enable)
        m_unit->clearUnitState(UNIT_STAT_NO_COMBAT_MOVEMENT);
    else
        m_unit->addUnitState(UNIT_STAT_NO_COMBAT_MOVEMENT);

    if (stopOrStartMovement && m_unit->GetVictim())     // Only change current movement while in combat (战斗中立即同步更新追击移动)
    {
        if (!m_unit->IsCrowdControlled())
        {
            if (enable)
                DoStartMovement(m_unit->GetVictim());
            else if (m_unit->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
                m_unit->InterruptMoving();
        }
    }
}

// 开关跟随移动状态
void UnitAI::SetFollowMovement(bool enable)
{
    if (enable)
        m_unit->clearUnitState(UNIT_STAT_NO_FOLLOW_MOVEMENT);
    else
        m_unit->addUnitState(UNIT_STAT_NO_FOLLOW_MOVEMENT);

    if (m_unit->IsMoving() && m_unit->GetMotionMaster()->GetCurrentMovementGeneratorType() == FOLLOW_MOTION_TYPE)
        m_unit->InterruptMoving();
}

bool UnitAI::IsCombatMovement() const
{
    return !m_unit->hasUnitState(UNIT_STAT_NO_COMBAT_MOVEMENT);
}

// 开战或切换目标时：自动下马并按远程/近战距离参数发起 MoveChase 追击
void UnitAI::HandleMovementOnAttackStart(Unit* victim, bool targetChange) const
{
    if (!m_unit->hasUnitState(UNIT_STAT_CAN_NOT_REACT))
    {
        if (m_dismountOnAggro)
            m_unit->Unmount(); // all ais should unmount here (所有 AI 在此统一自动下马)

        MotionMaster* creatureMotion = m_unit->GetMotionMaster();

        if (!m_unit->hasUnitState(UNIT_STAT_PROPELLED))
            creatureMotion->MoveChase(victim, m_attackDistance, m_attackAngle, m_moveFurther, false, true, targetChange);
    }
}

// 法术施法状态变化通知：处理打断移动、施法朝向锁定目标以及施法结束恢复注视目标
void UnitAI::OnSpellCastStateChange(Spell const* spell, bool state, WorldObject* target)
{
    if (!state && spell != m_currentSpell)
        return;

    SpellEntry const* spellInfo = spell->m_spellInfo;
    if (spellInfo->HasAttribute(SPELL_ATTR_EX4_ALLOW_CAST_WHILE_CASTING) || spellInfo->HasAttribute(SPELL_ATTR_ON_NEXT_SWING_NO_DAMAGE) || spellInfo->HasAttribute(SPELL_ATTR_ON_NEXT_SWING))
        return;

    // Creature should always stop before it will cast a non-instant spell (施放非瞬发/移动打断型法术前停步)
    if (state)
        if ((spell->GetCastTime() && spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT) || (IsChanneledSpell(spellInfo) && spellInfo->ChannelInterruptFlags & AURA_INTERRUPT_FLAG_MOVING))
            m_unit->InterruptMoving();

    bool forceTarget = false;

    // Targeting seems to be directly affected by eff index 0 targets, client does the same thing (spell id 38523 exception)
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (spellInfo->EffectImplicitTargetA[EFFECT_INDEX_0])
        {
            case TARGET_ENUM_UNITS_ENEMY_IN_CONE_24: // ignores everything and keeps turning
                return;
            case TARGET_LOCATION_CASTER_SRC:
            case TARGET_LOCATION_CASTER_DEST:
                switch (spellInfo->EffectImplicitTargetB[EFFECT_INDEX_0])
                {
                    case TARGET_ENUM_UNITS_ENEMY_AOE_AT_SRC_LOC: // ignores everything and keeps turning if instant
                    case TARGET_ENUM_UNITS_ENEMY_AOE_AT_DEST_LOC:
                        if (!spell->GetCastTime())
                            return;
                        break;
                }
                break;
            case TARGET_UNIT_FRIEND:
            case TARGET_UNIT_ENEMY: forceTarget = true; break;
            case TARGET_UNIT_SCRIPT_NEAR_CASTER:
            default: break;
        }
        if (forceTarget)
            break;
    }

    if (state)
    {
        if (!spellInfo->HasAttribute(SPELL_ATTR_EX5_AI_DOESNT_FACE_TARGET) && target && (m_unit != target || forceTarget))
        {
            m_unit->SetTarget(target);
            if (m_unit != target)
                m_unit->SetOrientation(m_unit->GetAngle(target));
            else
                m_unit->SetOrientation(m_unit->GetOrientation());
        }
        else
        {
            if (m_unit->HasTarget())
            {
                m_unit->SetFacingTo(m_unit->GetOrientation());
                m_unit->SetTarget(nullptr);
            }
        }
    }
    else
    {
        if (!spell->GetCastTime() && forceTarget)
        {
            HandleDelayedInstantAnimation(spellInfo);
        }
        else
        {
            if (m_unit->GetVictim() && !IsTargetingRestricted())
                m_unit->SetTarget(m_unit->GetVictim());
            else
                m_unit->SetTarget(nullptr);
        }
    }

    if (state)
    {
        m_currentSpell = spell;
    }
}

// 引导法术状态变化通知：按是否允许引导期间追踪目标更新朝向与注视目标
void UnitAI::OnChannelStateChange(Spell const* spell, bool state, WorldObject* target)
{
    if (state)
        m_currentSpell = nullptr;

    SpellEntry const* spellInfo = spell->m_spellInfo;
    // TODO: Determine if CHANNEL_FLAG_MOVEMENT is worth implementing
    if (!spellInfo->HasAttribute(SPELL_ATTR_EX_TRACK_TARGET_IN_CHANNEL))
    {
        if (spellInfo->HasAttribute(SPELL_ATTR_EX4_ALLOW_CAST_WHILE_CASTING))
            return;
    }

    if (spellInfo->Id == 45661) // Felmyst Encapsulate - cast time tracks target but channel time ignores him (菲米丝压缩技能特例：读条追踪但引导不转向)
        return;

    bool forceTarget = true; // Different default than normal cast

    // Targeting seems to be directly affected by eff index 0 targets, client does the same thing
    switch (spellInfo->EffectImplicitTargetA[EFFECT_INDEX_0])
    {
        case TARGET_UNIT:
        case TARGET_UNIT_SCRIPT_NEAR_CASTER: forceTarget = false; break;
        case TARGET_UNIT_ENEMY:
        default: break;
    }

    if (spellInfo->HasAttribute(SPELL_ATTR_EX_TRACK_TARGET_IN_CHANNEL))
        forceTarget = true;

    if (state)
    {
        if ((spellInfo->ChannelInterruptFlags & AURA_INTERRUPT_FLAG_TURNING && !spellInfo->HasAttribute(SPELL_ATTR_EX_TRACK_TARGET_IN_CHANNEL)) || !forceTarget)
        {
            m_unit->SetFacingTo(m_unit->GetOrientation());
            m_unit->SetTarget(nullptr);
        }
        else if (target && m_unit != target)
        {
            m_unit->SetTarget(target);
            m_unit->SetOrientation(m_unit->GetAngle(target));
        }
        else
        {
            m_unit->SetFacingTo(m_unit->GetOrientation());
            m_unit->SetTarget(nullptr);
        }
    }
    else
    {
        if (m_unit->GetVictim() && !IsTargetingRestricted())
            m_unit->SetTarget(m_unit->GetVictim());
        else
            m_unit->SetTarget(nullptr);
    }
}

// 瞬发法术完成后的注视目标恢复处理
void UnitAI::HandleDelayedInstantAnimation(SpellEntry const* /*spellInfo*/)
{
    if (m_unit->GetVictim() && !IsTargetingRestricted())
        m_unit->SetTarget(m_unit->GetVictim());
    else
        m_unit->SetTarget(nullptr);
}

// 检查附近友方生物是否处于战斗中，若满足距离与视线条件则主动加入战斗协助之
void UnitAI::CheckForHelp(Unit* who, Unit* me, float distance)
{
    Unit* victim = who->getAttackerForHelper();

    if (!victim)
        return;

    if (me->IsInCombat() || !me->CanCallForAssistance() || !who->CanCallForAssistance() || !me->CanCheckForHelp() || !who->CanCheckForHelp())
        return;

    // pulling happens once panic/retreating ends
    // current theory is that help aggro is never done if owner has suspended AI function during CC
    // 若友方正处于撤退、迷惑或昏迷状态，暂不触发连带进战（等恐慌/撤退结束或交由呼救逻辑）
    if (who->hasUnitState(UNIT_STAT_RETREATING) || who->IsConfused() || who->IsStunned())
        return;

    if (me->CanInitiateAttack() && me->CanAttackOnSight(victim) && victim->isInAccessablePlaceFor(me) && victim->IsVisibleForOrDetect(me, me, false))
    {
        if (me->IsWithinDistInMap(who, distance) && me->IsWithinLOSInMap(who, true))
        {
            if (me->CanAssistInCombatAgainst(who, victim))
            {
                AttackStart(victim);
                if (who->AI() && who->AI()->GetAIOrder() == ORDER_FLEEING)
                    who->GetMotionMaster()->InterruptPanic();
            }
        }
    }
}

// 在视野中探测目标：若触发潜行警觉则原地转身张望，否则立即发起攻击或入战
void UnitAI::DetectOrAttack(Unit* who)
{
    float attackRadius = m_unit->GetAttackDistance(who);
    if (m_unit->GetDistance(who, true, DIST_CALC_NONE) > attackRadius * attackRadius)
        return;

    if (!m_unit->IsEnemyCheckIgnoresLos() && !m_unit->IsWithinLOSInMap(who, true))
        return;

    if (!m_unit->GetVictim() && !m_unit->IsInCombat())
    {
        if (CanTriggerStealthAlert(who, attackRadius))
        {
            m_unit->SendAIReaction(AI_REACTION_ALERT);
            m_unit->SetFacingTo(m_unit->GetAngle(who));
            m_unit->GetMotionMaster()->MoveDistract(TIME_INTERVAL_LOOK);
            OnStealthAlert(who);
            return;
        }

        AttackStart(who);
    }
    else if (m_unit->GetMap()->IsDungeon())
        m_unit->EngageInCombatWith(who);
}

// 判定潜行玩家是否进入了“听到动静但尚未暴露”的警觉距离区间
bool UnitAI::CanTriggerStealthAlert(Unit* who, float /*attackRadius*/) const
{
    if (who->GetTypeId() != TYPEID_PLAYER)
        return false;
    if (m_unit->hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
        return false;

    if (m_unit->hasUnitState(UNIT_STAT_DISTRACTED))
        return false;

    return who->HasStealthAura() && m_unit->GetDistance(who, true, DIST_CALC_NONE) > who->GetVisibilityData().GetStealthVisibilityDistance(m_unit);
}

// ////////////////////////////////////////////////////////////////////////////////////////////////
//                                      Event system (AI 延迟事件系统)
// ////////////////////////////////////////////////////////////////////////////////////////////////

// 延迟向周围生物列表派发 AI 事件的事件处理器（按 GUID 存储防止延迟期间生物消失导致野指针）
class AiDelayEventAround : public BasicEvent
{
    public:
        AiDelayEventAround(AIEventType eventType, ObjectGuid invokerGuid, Unit& owner, CreatureList const& receivers, uint32 miscValue) :
            BasicEvent(),
            m_eventType(eventType),
            m_invokerGuid(invokerGuid),
            m_owner(owner),
            m_miscValue(miscValue)
        {
            // Pushing guids because in delay can happen some creature gets despawned => invalid pointer
            m_receiverGuids.reserve(receivers.size());
            for (auto receiver : receivers)
                m_receiverGuids.push_back(receiver->GetObjectGuid());
        }

        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override
        {
            Unit* pInvoker = m_owner.GetMap()->GetUnit(m_invokerGuid);
            if (!pInvoker)
                return true;

            for (ObjectGuid guid : m_receiverGuids)
            {
                if (Creature* pReceiver = m_owner.GetMap()->GetAnyTypeCreature(guid))
                {
                    pReceiver->AI()->ReceiveAIEvent(m_eventType, &m_owner, pInvoker, m_miscValue);
                    // Special case for type 0 (call-assistance)
                    if (m_eventType == AI_EVENT_CALL_ASSISTANCE)
                        pReceiver->AI()->HandleAssistanceCall(&m_owner, pInvoker);
                }
            }
            m_receiverGuids.clear();

            return true;
        }

    private:
        AiDelayEventAround();

        AIEventType m_eventType;
        ObjectGuid m_invokerGuid;
        Unit&  m_owner;
        uint32 m_miscValue;

        GuidVector m_receiverGuids;
};

// 向周围指定半径内的友方或所有生物广播 AI 事件（支持立即触发或延迟投递）
void UnitAI::SendAIEventAround(AIEventType eventType, Unit* invoker, uint32 delay, float radius, uint32 miscValue /*=0*/) const
{
    if (radius > 0)
    {
        CreatureList receiverList;

        // Allow sending custom AI events to all units in range (自定义事件或护送事件可广播至范围内所有生物)
        if ((eventType >= AI_EVENT_CUSTOM_EVENTAI_A && eventType <= AI_EVENT_CUSTOM_EVENTAI_F && eventType != AI_EVENT_GOT_CCED) || eventType > AI_EVENT_START_ESCORT)
        {
            MaNGOS::AnyUnitInObjectRangeCheck u_check(m_unit, radius);
            MaNGOS::CreatureListSearcher<MaNGOS::AnyUnitInObjectRangeCheck> searcher(receiverList, u_check);
            Cell::VisitAllObjects(m_unit, searcher, radius);
        }
        else // TODO: Expand functionality in future if needed (默认按可协助战斗生物范围筛选)
        {
            MaNGOS::AnyAssistCreatureInRangeCheck u_check(m_unit, invoker, radius);
            MaNGOS::CreatureListSearcher<MaNGOS::AnyAssistCreatureInRangeCheck> searcher(receiverList, u_check);
            Cell::VisitAllObjects(m_unit, searcher, radius);
        }

        if (!receiverList.empty())
        {
            if (delay)
            {
                AiDelayEventAround* e = new AiDelayEventAround(eventType, invoker ? invoker->GetObjectGuid() : ObjectGuid(), *m_unit, receiverList, miscValue);
                m_unit->m_events.AddEvent(e, m_unit->m_events.CalculateTime(delay));
            }
            else
            {
                for (Creature* receiver : receiverList)
                {
                    receiver->AI()->ReceiveAIEvent(eventType, m_unit, invoker, miscValue);
                    // Special case for type 0 (call-assistance)
                    if (eventType == AI_EVENT_CALL_ASSISTANCE)
                        receiver->AI()->HandleAssistanceCall(m_unit, invoker);
                }
            }
        }
    }
}

// 向单一接收者单位发送 AI 事件
void UnitAI::SendAIEvent(AIEventType eventType, Unit* invoker, Unit* receiver, uint32 miscValue /*=0*/) const
{
    MANGOS_ASSERT(receiver);
    receiver->AI()->ReceiveAIEvent(eventType, m_unit, invoker, miscValue);
}

// 判断目标单位是否在当前 AI 视野距离内且相互可见
bool UnitAI::IsVisible(Unit* pl) const
{
    return m_unit->IsWithinDist(pl, m_visibilityDistance) && pl->IsVisibleForOrDetect(m_unit, m_unit, true);
}

// 在范围内搜索损失血量最多（或百分比最低）的友方单位
Unit* UnitAI::DoSelectLowestHpFriendly(float range, float minMissing, bool percent, bool targetSelf) const
{
    Unit* unit = nullptr;

    if (percent)
    {
        MaNGOS::MostHPPercentMissingInRangeCheck u_check(m_unit, range, minMissing, true, targetSelf);
        MaNGOS::UnitLastSearcher<MaNGOS::MostHPPercentMissingInRangeCheck> searcher(unit, u_check);

        Cell::VisitGridObjects(m_unit, searcher, range);
    }
    else
    {
        MaNGOS::MostHPMissingInRangeCheck u_check(m_unit, range, minMissing, true, targetSelf);
        MaNGOS::UnitLastSearcher<MaNGOS::MostHPMissingInRangeCheck> searcher(unit, u_check);

        Cell::VisitGridObjects(m_unit, searcher, range);
    }

    return unit;
}

// 在范围内查找满足指定 UnitCondition ID 的友方单位
Unit* UnitAI::DoSelectConditionalFriendly(float range, int32 unitConditionId) const
{
    Unit* unit = nullptr;

    MaNGOS::FriendlyEligibleUnitConditionCheck u_check(m_unit, range, unitConditionId);
    MaNGOS::UnitLastSearcher<MaNGOS::FriendlyEligibleUnitConditionCheck> searcher(unit, u_check);

    Cell::VisitGridObjects(m_unit, searcher, range);

    return unit;
}

// 计算法术的最大施放距离（含施法者的射程修正）
float UnitAI::CalculateSpellRange(SpellEntry const* spellInfo) const
{
    // optimized duplicate of Spell::GetMinMaxRange for just max range
    SpellRangeEntry const* spellRange = sSpellRangeStore.LookupEntry(spellInfo->rangeIndex);
    float maxRange = GetSpellMaxRange(spellRange);
    if (Player* modOwner = m_unit->GetSpellModOwner()) // Player AI support (支持玩家 AI 的射程修正)
        modOwner->ApplySpellMod(spellInfo->Id, SPELLMOD_RANGE, maxRange);
    return maxRange;
}

CreatureList UnitAI::DoFindFriendlyEligibleDispel(uint32 spellId, bool self) const
{
    return DoFindFriendlyEligibleDispel(sSpellTemplate.LookupEntry<SpellEntry>(spellId), self);
}

// 根据驱散法术提取驱散类型/机制掩码并搜寻范围内可驱散友方生物
CreatureList UnitAI::DoFindFriendlyEligibleDispel(SpellEntry const* spellInfo, bool self) const
{
    uint32 dispelMask = 0;
    uint32 mechanicMask = 0;
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->Effect[i] == SPELL_EFFECT_DISPEL)
            dispelMask |= GetDispellMask(DispelType(spellInfo->EffectMiscValue[i]));
        else if (spellInfo->Effect[i] == SPELL_EFFECT_DISPEL_MECHANIC)
            mechanicMask |= convertEnumToFlag(spellInfo->EffectMiscValue[i]);
    }            
    float maxRange = CalculateSpellRange(spellInfo);
    return DoFindFriendlyEligibleDispel(maxRange, dispelMask, mechanicMask, self);
}

CreatureList UnitAI::DoFindFriendlyMissingBuff(float /*range*/, uint32 spellId, bool inCombat, bool self, uint32 spawnGroupId) const
{
    return DoFindFriendlyMissingBuff(sSpellTemplate.LookupEntry<SpellEntry>(spellId), inCombat, self, spawnGroupId);
}

// 在法术射程内查找缺少指定增益光环的友方生物列表
CreatureList UnitAI::DoFindFriendlyMissingBuff(SpellEntry const* spellInfo, bool inCombat, bool self, uint32 spawnGroupId) const
{
    CreatureList list;
    float maxRange = CalculateSpellRange(spellInfo);
    if (inCombat == false)
    {
        MaNGOS::FriendlyMissingBuffInRangeNotInCombatCheck u_check(m_unit, maxRange, spellInfo->Id, spawnGroupId);
        MaNGOS::CreatureListSearcher<MaNGOS::FriendlyMissingBuffInRangeNotInCombatCheck> searcher(list, u_check);
        Cell::VisitGridObjects(m_unit, searcher, maxRange);
    }
    else if (inCombat == true)
    {
        MaNGOS::FriendlyMissingBuffInRangeInCombatCheck u_check(m_unit, maxRange, spellInfo->Id, spawnGroupId);
        MaNGOS::CreatureListSearcher<MaNGOS::FriendlyMissingBuffInRangeInCombatCheck> searcher(list, u_check);
        Cell::VisitGridObjects(m_unit, searcher, maxRange);
    }

    if (!self) // just fooling compiler if non-unit - safe because we dont access any actual members/functions
        list.erase(std::remove(list.begin(), list.end(), (Creature*)m_unit), list.end());
    return list;
}

// 按自定义范围、驱散类型及机制掩码搜寻需要驱散的友方生物
CreatureList UnitAI::DoFindFriendlyEligibleDispel(float range, uint32 dispelMask, uint32 mechanicMask, bool self) const
{
    CreatureList list;
    MaNGOS::FriendlyEligibleDispelInRangeCheck u_check(m_unit, range, dispelMask, mechanicMask, self);
    MaNGOS::CreatureListSearcher<MaNGOS::FriendlyEligibleDispelInRangeCheck> searcher(list, u_check);
    Cell::VisitGridObjects(m_unit, searcher, range);
    return list;
}

// 将仇恨列表所有敌对目标的仇恨清零
void UnitAI::DoResetThreat()
{
    if (!m_unit->CanHaveThreatList() || m_unit->getThreatManager().isThreatListEmpty())
    {
        script_error_log("DoResetThreat called for creature that either cannot have threat list or has empty threat list (m_creature entry = %d)", m_unit->GetEntry());
        return;
    }

    m_unit->getThreatManager().modifyAllThreatPercent(-100);
}

// 检查当前是否处于可自由执行战斗技能动作的状态
bool UnitAI::CanExecuteCombatAction()
{
    return m_unit->CanReactInCombat() && !(m_unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED) && m_unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED)) && !m_unit->hasUnitState(UNIT_STAT_PROPELLED | UNIT_STAT_RETREATING) && !m_unit->IsNonMeleeSpellCasted(false) && !m_combatScriptHappening;
}

// 检查当前是否可尝试施放法术
bool UnitAI::CanCastSpell()
{
    return m_unit->CanCastSpellInCombat() && !m_unit->hasUnitState(UNIT_STAT_PROPELLED | UNIT_STAT_RETREATING) && !m_unit->IsNonMeleeSpellCasted(false) && !m_combatScriptHappening;
}

// 开启/关闭普通近战攻击并即时同步 MeleeAttackStart/Stop
void UnitAI::SetMeleeEnabled(bool state)
{
    if (state == m_meleeEnabled)
        return;

    m_meleeEnabled = state;
    if (m_unit->IsInCombat())
    {
        if (m_meleeEnabled && !m_unit->hasUnitState(UNIT_STAT_MELEE_ATTACKING))
        {
            if (m_unit->GetVictim())
                m_unit->MeleeAttackStart(m_unit->GetVictim());
        }
        else if (m_unit->hasUnitState(UNIT_STAT_MELEE_ATTACKING))
            m_unit->MeleeAttackStop(m_unit->GetVictim());
    }
}

// 启动朝向目标的 Chase 追击移动
void UnitAI::DoStartMovement(Unit* victim)
{
    if (victim)
        m_unit->GetMotionMaster()->MoveChase(victim, m_attackDistance, m_attackAngle, m_moveFurther, !m_chaseRun);
}

// 定时逃跑结束回调：恢复正常 AI 指令并重新追击目标
void UnitAI::TimedFleeingEnded()
{
    if (GetAIOrder() != ORDER_FLEEING)
        return; // prevent stack overflow by cyclic calls - TODO: remove once Motion Master is human again
    SetAIOrder(ORDER_NONE);
    SetCombatScriptStatus(false);
    if (!m_unit->IsAlive())
        return;
    DoStartMovement(m_unit->GetVictim());
}

// 触发指定持续时间的受惊逃跑移动
bool UnitAI::DoFlee(uint32 duration)
{
    if (!duration)
        duration = sWorld.getConfig(CONFIG_UINT32_CREATURE_FAMILY_FLEE_DELAY);

    // call the fear method and check if fear method succeed
    if (!m_unit->SetInPanic(duration))
        return false;

    // set the ai state to feared so it can reset movegen and ai state at the end of the fear
    SetAIOrder(ORDER_FLEEING);
    SetCombatScriptStatus(true);
    return true;
}

// 若处于远程模式且主法术因缺蓝/过近/被沉默等失败，自动切回近战追击模式
CanCastResult UnitAI::HandleSpellCastResult(CanCastResult result, SpellEntry const* spellInfo)
{
    if (m_rangedMode)
    {
        if (m_currentRangedMode)
        {
            if (m_mainSpells.find(spellInfo->Id) != m_mainSpells.end())
            {
                switch (result)
                {
                    case CAST_FAIL_POWER:
                    case CAST_FAIL_TOO_CLOSE:
                    case CAST_FAIL_CAST_PREVENTED:
                        SetCurrentRangedMode(false);
                        break;
                    case CAST_OK:
                    default:
                        break;
                }
            }
        }
    }
    return result;
}

// 遍历仇恨列表并使用外部比较器选定敌人开战
void UnitAI::AttackSpecificEnemy(std::function<void(Unit*, Unit*&)> check)
{
    Unit* chosenEnemy = nullptr;
    ThreatList const& list = m_unit->getThreatManager().getThreatList();
    for (auto& data : list)
    {
        Unit* enemy = data->getTarget();
        check(enemy, chosenEnemy);
    }

    AttackStart(chosenEnemy);
}

// 攻击仇恨列表中距离最近的敌对单位
void UnitAI::AttackClosestEnemy()
{
    float distance = std::numeric_limits<float>::max();
    AttackSpecificEnemy([&](Unit* enemy, Unit*& closestEnemy)
    {
        float curDistance = enemy->GetDistance(m_unit, true, DIST_CALC_NONE);
        if (!closestEnemy || curDistance < distance)
        {
            closestEnemy = enemy;
            distance = curDistance;
        }
    });
}

// 设置 AI 主动定身状态（combatOnly=true 时脱战自动解除）
void UnitAI::SetAIImmobilizedState(bool apply, bool combatOnly)
{
    if (combatOnly)
        m_combatOnlyRoot = apply;

    if (apply)
        m_unit->addUnitState(UNIT_STAT_AI_ROOT);
    else
        m_unit->clearUnitState(UNIT_STAT_AI_ROOT);

    m_unit->SetImmobilizedState(apply);
}

// 清除仅限战斗期间的 AI 定身
void UnitAI::ClearCombatOnlyRoot()
{
    if (m_combatOnlyRoot)
    {
        m_unit->clearUnitState(UNIT_STAT_AI_ROOT);
        m_unit->SetImmobilizedState(false);
        m_combatOnlyRoot = false;
    }
}

// 强制销毁列表中所有已生成生物和游戏对象并清空 GUID 集合
void UnitAI::DespawnGuids(GuidVector& spawns)
{
    for (ObjectGuid& guid : spawns)
    {
        if (guid.IsAnyTypeCreature())
        {
            if (Creature* spawn = m_unit->GetMap()->GetAnyTypeCreature(guid))
                spawn->ForcedDespawn();
        }
        else if (guid.IsGameObject())
        {
            if (GameObject* spawn = m_unit->GetMap()->GetGameObject(guid))
            {
                spawn->SetLootState(GO_JUST_DEACTIVATED);
                spawn->SetForcedDespawn();
            }
        }
    }
    spawns.clear();
}

// 执行所有已就绪的 CombatActions 战斗动作，并在必要时触发不可达目标传送
void UnitAI::ExecuteActions()
{
    if (!CanExecuteCombatAction())
        return;

    for (uint32 i = 0; i < GetCombatActionCount(); ++i)
    {
        // can be changed on any action - prevent all additional ones (动作执行中若进入脚本接管状态则终止本轮剩余动作)
        if (GetCombatScriptStatus())
            return;

        if (GetActionReadyStatus(i))
            ExecuteAction(i);
    }

    if (m_teleportUnreachable)
        if (m_unit->GetVictim() && IsCombatMovement() && !m_unit->GetMotionMaster()->GetCurrent()->IsReachable())
            m_unit->CastSpell(m_unit->GetVictim(), 21727, TRIGGERED_OLD_TRIGGERED);
}

// 主法术进入冷却后（纯施法者模式下）重新恢复远程站位模式
void UnitAI::OnSpellCooldownAdded(SpellEntry const* spellInfo)
{
    if (m_rangedModeSetting == TYPE_FULL_CASTER && m_mainSpells.find(spellInfo->Id) != m_mainSpells.end())
        SetCurrentRangedMode(true);
}

// 注册远程主攻击法术（首个注册的法术用于推导蓝耗、最小射程及学派掩码）
void UnitAI::AddMainSpell(uint32 spellId)
{
    if (!m_mainSpellId) // only for first
    {
        m_mainSpellId = spellId;
        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(m_mainSpellId);
        m_mainSpellCost = Spell::CalculatePowerCost(spellInfo, m_unit);
        m_mainSpellMinRange = GetSpellMinRange(sSpellRangeStore.LookupEntry(spellInfo->rangeIndex));
        // 非近战主法术设置追击距离为其最大射程，防止远程施法怪无谓贴近或过早进入近战/逃脱
        if (sSpellRangeStore.LookupEntry(spellInfo->rangeIndex)->Flags != static_cast<uint32>(SpellRangeFlags::SPELL_RANGE_FLAG_MELEE))
            m_chaseDistance = GetSpellMaxRange(sSpellRangeStore.LookupEntry(spellInfo->rangeIndex));
        m_mainAttackMask = SpellSchoolMask(m_mainAttackMask + spellInfo->SchoolMask);
        m_mainSpellInfo = spellInfo;
    }
    m_mainSpells.insert(spellId);
}

// 启用/禁用远程施法模式并设置期望追击距离与策略类型
void UnitAI::SetRangedMode(bool state, float distance, RangeModeType type)
{
    if (m_rangedMode == state)
        return;

    m_rangedMode = state;
    m_chaseDistance = distance;
    m_rangedModeSetting = type;

    if (m_unit->IsInCombat())
        SetCurrentRangedMode(state);
    else
    {
        m_currentRangedMode = true;
        m_attackDistance = m_chaseDistance;
    }
}

// 动态切换当前处于远程追击还是近战追击（同步更新追击距离、近战开关与定身忽略远程标记）
void UnitAI::SetCurrentRangedMode(bool state)
{
    if (state)
    {
        m_currentRangedMode = true;
        m_attackDistance = m_chaseDistance;
        m_unit->SetIgnoreRangedTargets(false);
        DoStartMovement(m_unit->GetVictim());
    }
    else
    {
        if (m_rangedModeSetting == TYPE_NO_MELEE_MODE)
            return;

        m_currentRangedMode = false;
        m_attackDistance = 0.f;
        m_unit->SetIgnoreRangedTargets(m_unit->hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_ROOT));
        DoStartMovement(m_unit->GetVictim());
        if (m_meleeEnabled && !m_unit->hasUnitState(UNIT_STAT_MELEE_ATTACKING))
            m_unit->MeleeAttackStart(m_unit->GetVictim());
    }
}

enum EAIPoints
{
    POINT_MOVE_DISTANCE
};

// 若主目标处于近战范围且自身具备后撤条件（蓝量充足），主动向安全射程外拉扯移动
void UnitAI::DistanceYourself()
{
    Unit* victim = m_unit->GetVictim();
    if (!victim->CanReachWithMeleeAttack(m_unit))
        return;

    if (!IsEligibleForDistancing())
        return;

    float combatReach = m_unit->GetCombinedCombatReach(victim, true);
    float distance = DISTANCING_CONSTANT + std::max(combatReach * 1.5f, combatReach + m_mainSpellMinRange);
    m_unit->GetMotionMaster()->DistanceYourself(distance);
}

void UnitAI::DistancingStarted()
{
    SetCombatScriptStatus(true);
}

void UnitAI::DistancingEnded()
{
    SetCombatScriptStatus(false);
    if (!m_currentRangedMode)
        SetCurrentRangedMode(true);
}

// 当通过控场技能限制住目标移动时，若该技能属于拉扯触发技能则顺势拉开距离
void UnitAI::JustStoppedMovementOfTarget(SpellEntry const* spellInfo, Unit* victim)
{
    if (m_unit->GetVictim() != victim)
        return;
    if (m_distanceSpells.find(spellInfo->Id) != m_distanceSpells.end())
        DistanceYourself();
}

// 主法术被打断（反制/沉默等）时，若所有主法术均不可用，则切换为近战肉搏模式
void UnitAI::OnSpellInterrupt(SpellEntry const* spellInfo)
{
    if (m_mainSpells.find(spellInfo->Id) != m_mainSpells.end())
    {
        if (m_rangedMode && m_rangedModeSetting != TYPE_NO_MELEE_MODE && IsMainSpellPrevented(spellInfo))
        {
            // infrequently mobs have multiple main spells and only go into melee on interrupt when all are on cooldown
            // 若怪物拥有多个主法术，仅当全部主法术均不可用时才转入近战模式
            if (m_mainSpells.size() > 1)
            {
                bool success = false;
                for (uint32 spellId : m_mainSpells)
                {
                    if (spellId != spellInfo->Id)
                    {
                        SpellEntry const* otherSpellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
                        if (!IsMainSpellPrevented(otherSpellInfo))
                        {
                            success = true;
                            break;
                        }
                    }
                }
                if (success)
                    return; // at least one main spell is off cooldown (至少还有一个主法术未进入冷却)
            }
            SetCurrentRangedMode(false);
        }
    }
}

// UnitAI 主更新循环：更新计时器、刷新仇恨目标、维护远程/近战模式切换、执行动作与法术列表、推进近战平砍
void UnitAI::UpdateAI(const uint32 diff)
{
    UpdateTimers(diff, m_unit->IsInCombat());

    bool combat = m_unit->SelectHostileTarget();

    UpdateEventTimers(diff);

    if (!combat)
        return;

    Unit* victim = m_unit->GetVictim();
    if (m_rangedMode && victim && CanExecuteCombatAction())
    {
        if (m_rangedModeSetting == TYPE_PROXIMITY || m_rangedModeSetting == TYPE_DISTANCER)
        {
            if (!m_currentRangedMode && victim->IsImmobilizedState() && IsCombatMovement() && IsEligibleForDistancing() && !IsMainSpellPrevented(m_mainSpellInfo))
                DistanceYourself();
            else if (m_currentRangedMode && m_unit->CanReachWithMeleeAttack(victim))
                SetCurrentRangedMode(false);
            else if (!m_currentRangedMode && !m_unit->CanReachWithMeleeAttack(victim, 3.f) && IsEligibleForDistancing() && !IsMainSpellPrevented(m_mainSpellInfo))
                SetCurrentRangedMode(true);
            else if (m_rangedModeSetting == TYPE_DISTANCER && !m_distancingCooldown)
            {
                m_distancingCooldown = true;
                ResetTimer(GENERIC_ACTION_DISTANCE, 5000);
            }
        }
        // casters only display melee animation when in ranged mode when someone is actually close enough
        // 远程施法者在处于远程模式时，仅当敌人贴近到近战范围才维持近战挥砍姿态
        if (m_currentRangedMode && m_meleeEnabled)
        {
            if (m_unit->hasUnitState(UNIT_STAT_MELEE_ATTACKING) && !m_unit->CanReachWithMeleeAttack(victim))
                m_unit->MeleeAttackStop(m_unit->GetVictim());
            else if (!m_unit->hasUnitState(UNIT_STAT_MELEE_ATTACKING) && m_unit->CanReachWithMeleeAttack(victim))
                m_unit->MeleeAttackStart(m_unit->GetVictim());
        }
    }

    ExecuteActions(); // before lists so phase transitions have higher priority (先执行脚本/阶段动作，再轮询通用法术列表)

    UpdateSpellLists();

    DoMeleeAttackIfReady();
}

// 返回主攻击学派掩码（远程模式取主法术学派，否则取近战武器伤害学派）
SpellSchoolMask UnitAI::GetMainAttackSchoolMask() const
{
    return m_currentRangedMode ? m_mainAttackMask : m_unit->GetMeleeDamageSchoolMask();
}

// 判断法术当前是否因冷却、沉默或安抚而无法施放
bool UnitAI::IsMainSpellPrevented(SpellEntry const* spellInfo) const
{
    if (!m_unit->IsSpellReady(*spellInfo))
        return true;

    if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE && m_unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
        return true;
    if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY && m_unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
        return true;

    // 能量/法力值不足以施放主法术时视为被阻断
    if (m_unit->GetPower(static_cast<Powers>(spellInfo->powerType)) < m_mainSpellCost)
        return true;

    return false;
}

// 判断单位是否有足够法力值支持远离拉扯（要求法力值大于主法术耗蓝的 2 倍）
bool UnitAI::IsEligibleForDistancing() const
{
    if (m_unit->GetPowerType() != POWER_MANA)
        return true;

    return m_mainSpellInfo && m_mainSpellCost * 2 < m_unit->GetPower(POWER_MANA);
}

// 生物法术列表切换时重新收集远程主技能并刷新冷却状态
void UnitAI::SpellListChanged()
{
    m_mainSpellId = 0;
    m_mainSpellCost = 0;
    m_mainSpellMinRange = 0;
    m_mainAttackMask = SPELL_SCHOOL_MASK_NONE;
    m_mainSpellInfo = nullptr;
    m_mainSpells.clear();

    CreatureSpellList const& spells = GetSpellList();
    if (spells.Disabled)
        return;

    for (auto& data : spells.Spells)
    {
        if (data.second.Flags & SPELL_LIST_FLAG_RANGED_ACTION)
        {
            AddMainSpell(data.second.SpellId);
        }
    }
    if (!spells.Spells.empty())
        m_spellListCooldown = false;
    if (m_unit->IsInCombat())
        AddInitialCooldowns();
}

// 法术列表轮询施法：按冷却、战斗条件、概率权重或顺序优先级筛选并施放技能
void UnitAI::UpdateSpellLists()
{
    if (m_spellListCooldown)
        return;

    ResetTimer(GENERIC_ACTION_SPELL_LIST, 1200);
    m_spellListCooldown = true;

    if (!m_unit->IsInCombat() || !CanCastSpell())
        return;

    CreatureSpellList const& spells = GetSpellList();

    // when probability is 0 for all spells, they will use priority based on positions
    // 当所有法术概率权重之和为 0 时按列表位置顺序优先施放，否则按权重随机抽取
    std::vector<std::tuple<uint32, uint32, uint32, Unit*>> eligibleSpells;
    std::vector<std::tuple<uint32, uint32, uint32, Unit*>> nonBlockingSpells;
    uint32 sum = 0;

    // one roll due to multiple spells (整轮辅助/远程动作共享一次掷骰判定)
    uint32 supportActionRoll = urand(0, 100);
    uint32 rangedActionRoll = urand(0, 100);
    for (auto& data : spells.Spells)
    {
        CreatureSpellListSpell const& spell = data.second;

        if (spell.DisabledForAI)
            continue;

        if (spell.Flags & SPELL_LIST_FLAG_SUPPORT_ACTION)
            if (supportActionRoll > spells.ChanceSupportAction)
                continue;

        if (spell.CombatCondition != -1 && spell.CombatCondition)
        {
            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spell.SpellId);
            float maxRange = CalculateSpellRange(spellInfo);
            if (!sObjectMgr.IsCombatConditionSatisfied(spell.CombatCondition, m_unit, maxRange))
                continue;
        }

        // chance is either base in ranged mode or chance - 50 in melee mode
        // meant to simulate chaincasting in ranged mode and mostly not chaincasting in melee mode
        // 远程法术在远程模式下采用基础概率，在近战模式下概率减 50%，以模拟远程连发与近战偶发施法行为
        if (spell.Flags & SPELL_LIST_FLAG_RANGED_ACTION)
            if (rangedActionRoll > (GetCurrentRangedMode() ? spells.ChanceRangedAttack : std::max((int32(spells.ChanceRangedAttack) - 50), 0)))
                continue;

        if (!m_unit->IsSpellReady(spell.SpellId))
            continue;

        bool result; Unit* target;
        std::tie(result, target) = ChooseTarget(spell.Target, spell.SpellId);
        if (!result)
            continue;

        if (spell.Flags & SPELL_LIST_FLAG_NON_BLOCKING)
            nonBlockingSpells.emplace_back(spell.SpellId, spell.Probability, spell.ScriptId, target);
        else
        {
            eligibleSpells.emplace_back(spell.SpellId, spell.Probability, spell.ScriptId, target);
            sum += spell.Probability;
        }
    }

    if (eligibleSpells.size() > 1 && sum != 0) // sum == 0 is meant to be priority based (lower position, higher priority)
        std::shuffle(eligibleSpells.begin(), eligibleSpells.end(), *GetRandomGenerator());

    auto executeSpell = [&](uint32 spellId, uint32 scriptId, Unit* target) -> bool
    {
        CanCastResult castResult = DoCastSpellIfCan(target, spellId);
        if (castResult == CAST_OK)
        {
            OnSpellCast(sSpellTemplate.LookupEntry<SpellEntry>(spellId), target);
            if (scriptId)
                m_unit->GetMap()->ScriptsStart(SCRIPT_TYPE_RELAY, scriptId, m_unit, target);
            return true;
        }
        return false;
    };

    // 先触发所有非阻塞技能，再从阻塞队列中选出一个可成功施放的技能
    for (auto& data : nonBlockingSpells)
    {
        uint32 spellId; uint32 probability; uint32 scriptId; Unit* target;
        std::tie(spellId, probability, scriptId, target) = data;
        executeSpell(spellId, scriptId, target);
    }

    // will hit first eligible spell when sum is 0 because roll -1 < probability 0
    int32 spellRoll = sum == 0 ? -1 : irand(0, sum - 1);
    bool success = false;
    // loop until either one spell was cast successfully or ran out of eligible spells (持续尝试直至成功施放一个技能或候选耗尽)
    do
    {
        for (auto itr = eligibleSpells.begin(); itr != eligibleSpells.end();)
        {
            uint32 spellId; uint32 probability; uint32 scriptId; Unit* target;
            std::tie(spellId, probability, scriptId, target) = *itr;
            if (spellRoll < int32(probability))
            {
                success = executeSpell(spellId, scriptId, target);
                itr = eligibleSpells.erase(itr);
            }
            else
                ++itr;
            spellRoll -= probability;
        }
    }
    while (!success && !eligibleSpells.empty());
}

// 根据法术列表目标配置（硬编码目标、敌对点名或友方支援）选择施法对象
std::pair<bool, Unit*> UnitAI::ChooseTarget(CreatureSpellListTargeting* targetData, uint32 spellId) const
{
    Unit* target = nullptr;
    bool result = true;
    switch (targetData->Type)
    {
        case SPELL_LIST_TARGETING_HARDCODED:
            switch (targetData->Id)
            {
                case SPELL_LIST_TARGET_NONE: result = true; target = nullptr; break;
                case SPELL_LIST_TARGET_CURRENT: result = true; target = m_unit->GetVictim(); break;
                case SPELL_LIST_TARGET_SELF: result = true; target = m_unit; break;
                case SPELL_LIST_TARGET_DISPELLABLE_FRIENDLY:
                case SPELL_LIST_TARGET_DISPELLABLE_FRIENDLY_NO_SELF:
                {
                    CreatureList list = DoFindFriendlyEligibleDispel(spellId, targetData->Id == SPELL_LIST_TARGET_DISPELLABLE_FRIENDLY);
                    if (list.empty())
                        result = false;
                    else
                    {
                        auto itr = list.begin();
                        std::advance(itr, urand(0, list.size() - 1));
                        target = *itr;
                    }
                    break;
                }
                case SPELL_LIST_TARGET_FRIENDLY_MISSING_BUFF:
                case SPELL_LIST_TARGET_FRIENDLY_MISSING_BUFF_NO_SELF:
                {
                    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
                    CreatureList list = DoFindFriendlyMissingBuff(spellInfo, true, targetData->Id == SPELL_LIST_TARGET_FRIENDLY_MISSING_BUFF);
                    if (list.empty())
                        result = false;
                    else
                    {
                        auto itr = list.begin();
                        std::advance(itr, urand(0, list.size() - 1));
                        target = *itr;
                    }
                    break;
                }
                case SPELL_LIST_TARGET_CURRENT_NOT_ALONE:
                    result = m_unit->getThreatManager().getThreatList().size() > 1;
                    target = m_unit->GetVictim();
                    break;
            }
            break;
        case SPELL_LIST_TARGETING_ATTACK:
            target = m_unit->SelectAttackingTarget(AttackingTarget(targetData->Param1), targetData->Param2, spellId, targetData->Param3, SelectAttackingTargetParams(), targetData->UnitCondition != -1 ? targetData->UnitCondition : 0);
            if (!target)
                result = false;
            if (targetData->Param3 & (SELECT_FLAG_USE_EFFECT_RADIUS | SELECT_FLAG_USE_EFFECT_RADIUS_OF_TRIGGERED_SPELL)) // these select flags only check if target exists but doesnt pass it to cast (仅检查范围内存在合法目标，施法时无需传目标指针)
                target = nullptr;
            break;
        case SPELL_LIST_TARGETING_SUPPORT:
            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
            float maxRange = CalculateSpellRange(spellInfo);
            // Combat Range is added in the grid search
            if (targetData->UnitCondition != -1)
                target = DoSelectConditionalFriendly(maxRange, targetData->UnitCondition);
            else
                target = DoSelectLowestHpFriendly(maxRange, float(targetData->Param1), bool(targetData->Param2), bool(targetData->Param3));
            if (!target)
                result = false;
            break;
    }
    return { result, target };
}

// 进战时为法术列表中的所有技能施加初始随机冷却
void UnitAI::AddInitialCooldowns()
{
    CreatureSpellList const& spells = GetSpellList();
    for (auto& data : spells.Spells)
    {
        uint32 cooldown = urand(data.second.InitialMin, data.second.InitialMax);
        if (cooldown)
        {
            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(data.second.SpellId);
            m_unit->AddCooldown(*spellInfo, nullptr, false, cooldown, true);
        }
    }
}

