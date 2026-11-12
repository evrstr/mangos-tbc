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

#include "PetAI.h"
#include "Util/Errors.h"
#include "Entities/Pet.h"
#include "Entities/Player.h"
#include "Spells/Spell.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Entities/Creature.h"
#include "World/World.h"
#include "Util/Util.h"

// 许可度检查：仅宠物类型生物适用 PetAI
int PetAI::Permissible(const Creature* creature)
{
    if (creature->IsPet())
        return PERMIT_BASE_SPECIAL;

    return PERMIT_BASE_NO;
}

PetAI::PetAI(Creature* creature) : PetAI(creature, 0)
{
}

// 构造函数：配置宠物队形跟随参数、特定法术召唤宠物（如小鬼/水元素）的远程攻击模式及警卫视野
PetAI::PetAI(Creature* creature, uint32 combatActions) : CreatureAI(creature, combatActions), m_inCombat(false)
{
    m_followAngle = M_PI_F / 2;
    m_followDist = 1.5f;
    m_pet = creature->IsPet() ? static_cast<Pet*>(creature) : nullptr;

    if (m_pet && m_pet->isControlled())
    {
        if (sWorld.getConfig(CONFIG_BOOL_PET_ATTACK_FROM_BEHIND))
            m_attackAngle = M_PI_F;

        if (Unit* owner = m_unit->GetOwner())
            RequestFollow(owner);
    }

    switch (creature->GetUInt32Value(UNIT_CREATED_BY_SPELL))
    {
        case 688: // imp (术士小鬼：25 码远程攻击，禁用近战)
            m_attackDistance = 25.f;
            m_meleeEnabled = false;
            break;
        case 31687: // water ele (法师水元素：30 码远程攻击，禁用近战)
            m_attackDistance = 30.f;
            m_meleeEnabled = false;
            break;
    }

    if (m_creature->IsGuard() || m_unit->GetCharmInfo()) // guards and charmed targets (守卫与受控宠物使用警卫视野半径)
        m_visibilityDistance = sWorld.getConfig(CONFIG_FLOAT_SIGHT_GUARDER);
}

// 视野索敌：处于主动攻击姿态且未处于撤退/控制禁用状态时，自动攻击合法敌对目标（避开控场光环）
void PetAI::MoveInLineOfSight(Unit* who)
{
    CharmInfo* charmInfo = m_unit->GetCharmInfo();
    if (CanHandleCharm() && !charmInfo)
    {
        CreatureAI::MoveInLineOfSight(who);
        return;
    }

    MANGOS_ASSERT(charmInfo);

    Unit* victim = m_unit->GetVictim();
    // Don't search for victim when the current victim is alive or retreating (当前目标仍存活或宠物正在撤退回主人身边时不索敌)
    if (victim && victim->IsAlive() || charmInfo->GetIsRetreating())
        return;

    if (HasReactState(REACT_AGGRESSIVE)
            && !(m_pet && m_pet->HasActionsDisabled())
            && !(who->GetTypeId() == TYPEID_UNIT && static_cast<Creature*>(who)->IsCivilian())
            && m_creature->CanAttackOnSight(who) && who->isInAccessablePlaceFor(m_unit)
            && m_unit->IsWithinDistInMap(who, m_unit->GetAttackDistance(who))
            && m_unit->GetDistanceZ(who) <= CREATURE_Z_ATTACK_RANGE_MELEE
            && m_unit->IsWithinLOSInMap(who, true)
            && !GetCombatScriptStatus()
            && !who->IsCrowdControlled()
            && !who->HasAuraPetShouldAvoidBreaking(m_pet, charmInfo->GetPetLastAttackCommandTime()))
    {
        AttackStart(who);

        if (Unit* owner = m_unit->GetOwner())
            owner->SetInCombatState(true, who);
    }
}

// 开始攻击目标并启动追击移动
void PetAI::AttackStart(Unit* who)
{
    if (m_pet && m_pet->HasActionsDisabled())
        return;

    if (GetCombatScriptStatus())
        return;

    if (m_unit->Attack(who, m_meleeEnabled))
    {
        // TMGs call CreatureRelocation which via MoveInLineOfSight can call this function
        // thus with the following clear the original TMG gets invalidated and crash, doh
        // hope it doesn't start to leak memory without this :-/
        // i_pet->Clear();
        HandleMovementOnAttackStart(who, false);

        m_inCombat = true;
    }
}

// 脱离战斗处理：若正在连续拉怪切换新目标则只清理旧仇恨而不中断当前攻击，否则执行完整 CombatStop
void PetAI::EnterEvadeMode()
{
    CharmInfo* charminfo = m_unit->GetCharmInfo();
    if (CanHandleCharm() && !charminfo)
    {
        CreatureAI::EnterEvadeMode();
        return;
    }

    // check for "chain pull" scenario - pet has already been sent to attack while exiting from an earlier combat
    // avoid AttackStop in CombatStop so that pet doesn't lose current target and return to follow owner in this case
    // 处理连续攻击场景：若宠物已锁定新目标，避免调用 AttackStop 导致丢失新目标而返回主人身边
    if (m_unit->GetTarget() && m_unit->GetVictim())
    {
        m_unit->RemoveAllAttackers();
        m_unit->DeleteThreatList();
        m_unit->GetCombatManager().StopCombatTimer();
        m_unit->ClearInCombat();
    }
    else
        m_unit->CombatStop();
}

// 宠物主更新循环：校验目标有效性、处理撤退跟随、自动施法队列、近战平砍及非战斗停留/跟随指令
void PetAI::UpdateAI(const uint32 diff)
{
    Unit* owner = m_unit->GetMaster();

    if (CanHandleCharm() && !owner)
    {
        CreatureAI::UpdateAI(diff);
        return;
    }

    UpdateTimers(diff, m_creature->IsInCombat());

    if (!owner)
        return;

    if (GetCombatScriptStatus())
    {
        m_inCombat = false;
        return;
    }

    Unit* victim = m_pet && m_pet->HasActionsDisabled() ? nullptr : m_unit->GetVictim();

    CharmInfo* charmInfo = m_unit->GetCharmInfo();
    MANGOS_ASSERT(charmInfo);

    // Do not continue attacking if victim is moving home (目标正在跑回出生点脱战时放弃攻击)
    if (victim && victim->GetCombatManager().IsEvadingHome())
        victim = nullptr;
    // Do not continue attacking when victim is CCed (目标处于应避免打破的控场状态时放弃攻击)
    if (victim && victim->IsCrowdControlled() && victim->HasAuraPetShouldAvoidBreaking(m_pet, charmInfo->GetPetLastAttackCommandTime()))
        victim = nullptr;

    // Stop auto attack and chase if victim was dropped (若目标失效则停止攻击并中止追击移动)
    if (m_inCombat && !victim)
    {
        m_unit->AttackStop(true, true);
        m_inCombat = false;

        MotionMaster* mm = m_unit->GetMotionMaster();

        if (mm->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
            mm->MovementExpired();
    }

    // 撤退返回主人途中：到达主人 2 倍跟随距离内后清除撤退标记
    if (charmInfo->GetIsRetreating())
    {
        if (!owner->IsWithinDistInMap(m_unit, (PET_FOLLOW_DIST * 2)))
        {
            if (!m_unit->hasUnitState(UNIT_STAT_FOLLOW))
                m_unit->GetMotionMaster()->MoveFollow(owner, m_followDist, m_followAngle);

            return;
        }
        charmInfo->SetIsRetreating();
    }

    ExecuteActions();

    // Stop here if ranged spell casting is in progress (正在施法或无法施法时跳过后续自动施法)
    if (!CanCastSpell())
        return;

    std::vector<std::tuple<SpellEntry const*, Unit*, bool>> nonBlockingSpells = PickSpellWithTarget(owner, victim, charmInfo);
    // Cast either an opener spell or an autocast spell (执行开怪法术或自动施法列表中的技能)
    for (auto& nonBlockingSpell : nonBlockingSpells)
    {
        Cast(nonBlockingSpell);
    }

    if (victim)
    {
        // i_pet.getVictim() can't be used for check in case stop fighting, i_pet.GetVictim() clear at Unit death etc.
        // This is needed for charmed creatures, as once their target was reset other effects can trigger threat
        if (!m_unit->CanAttack(victim))
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "PetAI (guid = %u) is stopping attack.", m_unit->GetGUIDLow());
            m_unit->CombatStop();
            m_inCombat = false;

            return;
        }

        // if pet misses its target, it will also be the first in threat list (在近战范围内转向目标并执行近战平砍，否则继续追击)
        if (m_inCombat && m_meleeEnabled && m_unit->CanReachWithMeleeAttack(victim))
        {
            if (!m_unit->HasInArc(victim, 2 * M_PI_F / 3))
                m_unit->SetFacingToObject(victim);

            DoMeleeAttackIfReady();
        }
        else if (!m_unit->hasUnitState(UNIT_STAT_MOVING))
            AttackStart(victim);
    }
    else if (!m_unit->hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
    {
        const bool staying = charmInfo->HasCommandState(COMMAND_STAY);
        const bool following = !staying && charmInfo->HasCommandState(COMMAND_FOLLOW);

        // If not commanded to stay, try to assist owner first (未收到“停留”指令且主人正在战斗时，非被动宠物自动协助攻击)
        if (!staying && owner->IsInCombat() && !HasReactState(REACT_PASSIVE))
            AttackStart(owner->getAttackerForHelper());

        // Handle non-combat movement (非战斗状态下处理原地停留或跟随主人移动)
        if (!m_inCombat && !m_unit->hasUnitState(UNIT_STAT_NO_FREE_MOVE | UNIT_STAT_CHASE))
        {
            MotionMaster* mm = m_unit->GetMotionMaster();

            if (staying && !m_unit->hasUnitState(UNIT_STAT_STAY))
            {
                // If stay command is set, but we don't have stay pos yet: establish current pos as stay position, adjust orientation
                // 若首次进入停留指令则记录当前坐标与朝向作为停留点
                if (charmInfo->UpdateStayPosition())
                    mm->MoveStay(charmInfo->GetStayPosX(), charmInfo->GetStayPosY(), charmInfo->GetStayPosZ(), charmInfo->GetStayPosO());
                 else
                    mm->MoveStay(charmInfo->GetStayPosX(), charmInfo->GetStayPosY(), charmInfo->GetStayPosZ());
            }
            else if (following && !m_unit->hasUnitState(UNIT_STAT_FOLLOW))
                mm->MoveFollow(owner, m_followDist, m_followAngle);
        }
    }
}

// 筛选宠物本次可施放的开怪法术或自动施法法术及其对应目标
std::vector<std::tuple<SpellEntry const*, Unit*, bool>> PetAI::PickSpellWithTarget(Unit* owner, Unit* victim, CharmInfo* charmInfo)
{
    std::vector<std::tuple<SpellEntry const*, Unit*, bool>> nonblockingSpells;
    // Cast an opener spell, if stored (优先处理待施放的起手追击法术)
    if (charmInfo->GetSpellOpener() != 0)
    {
        uint32 minRange = charmInfo->GetSpellOpenerMinRange();

        // Clear the spell opener in case target is too close (目标过近低于最小射程则放弃起手技)
        if (minRange != 0 && m_unit->IsWithinDistInMap(victim, minRange))
            charmInfo->SetSpellOpener();
        // Use the spell opener if target is within the range and line of sight.
        // But do not drop the spell opener in case the condition is not met to give
        // opportunity to cast it when the victim gets visible or closer.
        // 若目标在最大射程且通视范围内则施放并清除；否则保留等待走近后触发
        else if (m_unit->IsWithinDistInMap(victim, charmInfo->GetSpellOpenerMaxRange())
                 && m_unit->IsWithinLOSInMap(victim, true))
        {
            uint32 spellId = charmInfo->GetSpellOpener();

            // Clear spell opener
            charmInfo->SetSpellOpener();

            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
            if (!spellInfo)
                return nonblockingSpells;

            nonblockingSpells.emplace_back(spellInfo, victim, true);
            return nonblockingSpells;
        }
    }

    uint32 creatureEntry = (m_pet && m_pet->getPetType() == HUNTER_PET) ? 1 : m_creature->GetCreatureInfo()->Entry;
    // Cast a spell from autocast selection (遍历宠物已开启自动施法的技能栏位)
    for (uint8 i = 0; i < m_creature->GetPetAutoSpellSize(); ++i)
    {
        uint32 spellId = m_creature->GetPetAutoSpellOnPos(i);
        if (!spellId)
            continue;

        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);

        PetAutocastSpellList* spellList = m_creature->GetMap()->GetMapDataContainer().GetPetAutocastSpellList(creatureEntry, spellId);

        if (spellList)
        {
            if (spellList->combatCondition != -1 && spellList->combatCondition)
            {
                float maxRange = CalculateSpellRange(spellInfo);
                if (!sObjectMgr.IsCombatConditionSatisfied(spellList->combatCondition, m_unit, maxRange))
                    continue;
            }

            CreatureSpellListTargeting* targeting = m_creature->GetMap()->GetMapDataContainer().GetCreatureSpellTargeting(spellList->targetId);

            bool result;
            Unit* target;
            std::tie(result, target) = ChooseTarget(targeting, spellId);
            if (!result)
                continue;

            if (!CanAutoCastAreaAura(spellInfo, target)) // prevent area aura spam (避免重复叠加区域光环)
                continue;

            nonblockingSpells.emplace_back(spellInfo, target, false);
            continue;
        }

        // Ignore spells that are:
        // 1. Not ready.
        // 2. Non-combat spells while in combat (f.e., Succubus's Lesser Invisibility).
        // 3. Combat spells while not in combat.
        // 过滤未冷却、战斗中不能施放的非战斗技能、以及脱战后不需要施放的战斗技能
        if (!spellInfo || !m_unit->IsSpellReady(*spellInfo) || (m_inCombat && IsNonCombatSpell(spellInfo)) || (!m_inCombat && !IsPositiveSpell(spellInfo)))
            continue;

        // Try to cast a spell on self if the spell allows only targeting self (Lesser Invisibility, Blood Pact, Dash)
        // 仅限自身目标法术（如次级隐形术、血之契印、急奔）
        if (IsOnlySelfTargeting(spellInfo))
        {
            // Skip the spell in case it's already applied to self or doesn't meet specific conditions
            if (!ShouldCast(spellInfo, victim) || !CanAutoCast(spellInfo, m_unit))
                continue;

            nonblockingSpells.emplace_back(spellInfo, m_unit, false);
            continue;
        }
        // Try to cast a spell if the spell is AoE (范围法术：攻击类需目标在近战内，有益类仅在战斗中施放)
        else if (IsAreaOfEffectSpell(spellInfo))
        {
            // If it's a harmful spell (Thunderstomp, Suffering)
            if (!IsPositiveSpell(spellInfo))
            {
                // Keep spell until the victim is in melee range
                if (!victim || !m_unit->CanReachWithMeleeAttack(victim))
                    continue;
            }
            else
            // If it's a positive spell (Furious Howl)
            {
                // Use it only in combat
                if (!m_inCombat)
                    continue;
            }

            // Target is not required for pet AoE spells
            nonblockingSpells.emplace_back(spellInfo, nullptr, false);
            continue;
        }
        // In all other cases, try to find a good use for the spell (按目标 -> 自身 -> 主人 -> 队伍成员顺序匹配可用目标)
        else
        {
            // Cast a spell on the victim, if present
            if (victim && ShouldCast(spellInfo, victim) && CanAutoCast(spellInfo, victim))
            {
                nonblockingSpells.emplace_back(spellInfo, victim, false);
                continue;
            }
            // Cast the spell on self, if applicable
            if (CanAutoCast(spellInfo, m_unit))
            {
                nonblockingSpells.emplace_back(spellInfo, m_unit, false);
                continue;
            }
            // Cast the spell on the pet owner, if applicable
            else if (CanAutoCast(spellInfo, owner))
            {
                nonblockingSpells.emplace_back(spellInfo, owner, false);
                continue;
            }
            // If pet owner is a player, get the group and cast the spell on anyone in the group
            else if (owner->IsPlayer())
            {
                if (Player* target = PickGroupMemberForSpell(static_cast<Player*>(owner), spellInfo))
                {
                    nonblockingSpells.emplace_back(spellInfo, target, false);
                    continue;
                }
            }
        }
    }
    return nonblockingSpells;
}

// 特殊宠物法术的释放时机判断（如法术封锁、俯冲/急奔、龟壳护盾）
bool PetAI::ShouldCast(SpellEntry const* spellInfo, Unit* victim) // essentially emulates CombatCondition code flow
{
    switch (spellInfo->Id)
    {
        case 19244: // Spell lock Rank 1-2 (地狱猎犬法术封锁：仅目标正在施法时施放)
        case 19647:
            return victim->IsNonMeleeSpellCasted(false);
        case 23145: // Dive Rank 1-3 (俯冲/急奔：仅目标不在近战范围内时加速追击)
        case 23147:
        case 23148:
        case 23099: // Dash Rank 1-3
        case 23109:
        case 23110:
            // Cast only when there is a victim and it is not within melee range
            return victim && !m_unit->CanReachWithMeleeAttack(victim);
        case 26064: // Shell Shield Rank 1 (甲壳护盾：仅战斗中且血量低于 50% 时施放)
            // Cast only in combat and when HP is lower than 50%
            return m_inCombat && m_unit->GetHealthPercent() < 50;
    }
    return true;
}

// 检查区域光环效果是否已存在于自身或目标，避免重复刷新
bool PetAI::CanAutoCastAreaAura(SpellEntry const* spellInfo, Unit* target) const
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (IsAreaAuraEffect(spellInfo->Effect[i]))
        {
            // selfcast case
            if (spellInfo->EffectImplicitTargetA[i] == TARGET_UNIT_CASTER || spellInfo->EffectImplicitTargetA[i] == TARGET_NONE)
            {
                if (m_creature->HasAura(spellInfo->Id, SpellEffectIndex(i)))
                    return false;
            }
            else
            {
                if (!target)
                    return false;

                if (target->HasAura(spellInfo->Id, SpellEffectIndex(i)))
                    return false;
            }
        }
    }
    return true;
}

// 检查指定法术是否满足对 target 的自动施法条件（不重复叠加光环且通过 CheckPetCast）
bool PetAI::CanAutoCast(SpellEntry const* spellInfo, Unit* target) const
{
    if (!CanAutoCastAreaAura(spellInfo, target))
        return false;

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        if (spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AURA)
        {
            if (spellInfo->StackAmount <= 1)
            {
                if (target->HasAura(spellInfo->Id, SpellEffectIndex(j)))
                    return false;
            }
            else
            {
                if (Aura* aura = target->GetAura(spellInfo->Id, SpellEffectIndex(j)))
                    if (aura->GetStackAmount() >= spellInfo->StackAmount)
                        return false;
            }
        }
    }

    SpellCastResult result = CheckPetCast(spellInfo, target);

    if (result == SPELL_CAST_OK || result == SPELL_FAILED_UNIT_NOT_INFRONT)
        return true;
    return false; // target invalid
}

// 校验宠物施放法术的合法性（施法状态、战斗限制、主人存活、隐式目标及敌友校验）
SpellCastResult PetAI::CheckPetCast(SpellEntry const* spellInfo, Unit* target) const
{
    if (m_creature->IsNonMeleeSpellCasted(false) && !spellInfo->HasAttribute(SPELL_ATTR_EX4_ALLOW_CAST_WHILE_CASTING)) // prevent spellcast interruption by another spellcast (避免打断当前正在施放的法术)
        return SPELL_FAILED_SPELL_IN_PROGRESS;
    if (m_creature->IsInCombat() && IsNonCombatSpell(spellInfo))
        return SPELL_FAILED_AFFECTING_COMBAT;

    if (m_creature->IsCreature() && (static_cast<Creature*>(m_creature)->IsPet() || m_creature->HasCharmer()))
    {
        // dead owner (pets still alive when owners ressed?) (主人死亡时禁止施法)
        if (m_creature->GetMaster() && !m_creature->GetMaster()->IsAlive())
            return SPELL_FAILED_CASTER_DEAD;

        bool need = false;
        bool script = false;
        for (unsigned int i : spellInfo->EffectImplicitTargetA)
        {
            SpellTargetInfo& targetData = SpellTargetInfoTable[i];
            
            if (targetData.filter == TARGET_SCRIPT)
            {
                script = true;
            }
            else if (targetData.type == TARGET_TYPE_UNIT || targetData.type == TARGET_TYPE_PLAYER)
            {
                need = true;
                if (!target)
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                break;
            }
        }

        if (script || !need)
            return SPELL_CAST_OK;

        if (target) // for target dead/target not valid (校验目标生死与友方/敌对阵营合法性)
        {
            if (target->IsDead() && !spellInfo->HasAttribute(SPELL_ATTR_EX2_ALLOW_DEAD_TARGET))
                return SPELL_FAILED_BAD_TARGETS;

            if (IsPositiveSpell(spellInfo->Id, m_creature, target))
            {
                if (!m_creature->CanAssistSpell(target, spellInfo))
                    return SPELL_FAILED_BAD_TARGETS;
            }
            else
            {
                bool duelvsplayertar = false;
                for (unsigned int j : spellInfo->EffectImplicitTargetA)
                {
                    // TARGET_UNIT is positive AND negative
                    duelvsplayertar |= (j == TARGET_UNIT);
                }
                if (!m_creature->CanAttack(target) && !duelvsplayertar)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
            }
        }
    }

    return SPELL_CAST_OK;
}

// 从同小队成员中挑选适合接收自动施法有益法术的玩家
Player* PetAI::PickGroupMemberForSpell(Player* player, SpellEntry const* spellInfo)
{
    Group* group = player->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->getSource();
        if (member && group->SameSubGroup(player, member) && member->GetObjectGuid() != player->GetObjectGuid() && CanAutoCast(spellInfo, member))
            return member;
    }

    return nullptr;
}

// 构造 Spell 对象并执行宠物施法（处理起手技标志、朝向调整及猎人宠物学习检测）
bool PetAI::Cast(std::tuple<SpellEntry const*, Unit*, bool> spellWithTarget)
{
    // Unpack the pair, check the spell and try to cast.
    SpellEntry const* spellInfo; Unit* target; bool opener;
    std::tie(spellInfo, target, opener) = spellWithTarget;

    uint32 flags = TRIGGERED_NORMAL_COMBAT_CAST;
    if (opener && m_creature->IsPlayerControlled())
        flags |= TRIGGERED_PET_CAST;
    Spell* spell = new Spell(m_unit, spellInfo, flags);
    SpellCastTargets targets;
    if (target)
    {
        targets.setUnitTarget(target);

        if (!m_unit->HasInArc(target))
            m_unit->SetFacingToObject(target);
    }

    if (m_pet)
        m_pet->CheckLearning(spell->m_spellInfo->Id);

    SpellCastResult result = spell->SpellStart(&targets);
    return result == SPELL_CAST_OK;
}

// 取消召唤时解除玩家阵型跟随
void PetAI::OnUnsummon()
{
    CreatureAI::OnUnsummon();
    RelinquishFollow(m_unit->GetOwnerGuid());
}

// 宠物死亡时解除玩家阵型跟随
void PetAI::JustDied(Unit* killer)
{
    CreatureAI::JustDied(killer);
    RelinquishFollow(m_unit->GetOwnerGuid());
}

// 宠物重生/召唤时回调（若可处理魅惑则转交基类重置）
void PetAI::JustRespawned()
{
    if (CanHandleCharm())
    {
        CreatureAI::JustRespawned();
        return;
    }
}

// 被攻击时若无当前目标且非被动姿态，立即反击攻击者
void PetAI::AttackedBy(Unit* attacker)
{
    CharmInfo* charmInfo = m_unit->GetCharmInfo();
    if (CanHandleCharm() && !charmInfo)
    {
        CreatureAI::AttackedBy(attacker);
        return;
    }

    MANGOS_ASSERT(charmInfo);
    // when attacked, fight back if no victim unless we have a charm state set to passive (非被动姿态且空闲时反击攻击者)
    if (!(m_unit->GetVictim() || GetCombatScriptStatus() || charmInfo->GetIsRetreating() || HasReactState(REACT_PASSIVE)))
        AttackStart(attacker);
}
