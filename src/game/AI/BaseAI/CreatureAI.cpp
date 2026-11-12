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

#include "AI/BaseAI/CreatureAI.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"
#include "World/World.h"
#include "Entities/Creature.h"

CreatureAI::CreatureAI(Creature* creature) : CreatureAI(creature, 0) { }

// 构造函数：根据生物静态标志与模板标志初始化下马、近战禁用、定身、反应姿态及视野范围
CreatureAI::CreatureAI(Creature* creature, uint32 combatActions) :
    UnitAI(creature, combatActions),
    m_creature(creature),
    m_deathPrevented(false), m_followAngle(0.f), m_followDist(0.f)
{
    m_dismountOnAggro = !(m_creature->GetCreatureInfo()->HasFlag(CreatureTypeFlags::ALLOW_MOUNTED_COMBAT));
    SetMeleeEnabled(!(m_creature->GetSettings().HasFlag(CreatureStaticFlags::NO_MELEE_FLEE)
        || m_creature->GetSettings().HasFlag(CreatureStaticFlags4::NO_MELEE_APPROACH) || m_creature->GetCreatureInfo()->ExtraFlags & CREATURE_EXTRA_FLAG_NO_MELEE));
    if (m_creature->GetSettings().HasFlag(CreatureStaticFlags::SESSILE))
        SetAIImmobilizedState(true);

    if (m_creature->IsNoAggroOnSight())
        SetReactState(REACT_DEFENSIVE);
    if (m_creature->GetSettings().HasFlag(CreatureStaticFlags2::SPAWN_DEFENSIVE))
        SetReactState(REACT_DEFENSIVE);
    else if (m_creature->GetSettings().HasFlag(CreatureStaticFlags::IGNORE_COMBAT))
        m_creature->SetCanEnterCombat(false);
    if (m_creature->IsGuard() || m_unit->GetCharmInfo()) // guards and charmed targets (守卫与受控单位使用更大的警卫视野配置)
        m_visibilityDistance = sWorld.getConfig(CONFIG_FLOAT_SIGHT_GUARDER);
}

// 重置远程模式与攻击追击距离至默认配置
void CreatureAI::Reset()
{
    m_currentRangedMode = m_rangedMode;
    m_attackDistance = m_chaseDistance;
}

// 进入战斗回调：若为守卫或平民则向本地/世界防务频道发送遇袭警告
void CreatureAI::EnterCombat(Unit* enemy)
{
    UnitAI::EnterCombat(enemy);
    if (enemy && (m_creature->IsGuard() || m_creature->IsCivilian()))
    {
        // Send Zone Under Attack message to the LocalDefense and WorldDefense Channels (发送区域遇袭通知到防务频道)
        if (Player* pKiller = enemy->GetBeneficiaryPlayer())
            m_creature->SendZoneUnderAttackMessage(pKiller);
    }
}

// 脱离战斗：清理状态、重置战斗计时器并调用 Reset()
void CreatureAI::EnterEvadeMode()
{
    UnitAI::EnterEvadeMode();
    ResetTimersOnEvade();
    Reset();
}

// 尝试开始攻击指定目标（处理战斗小地图信号、调用攻击、呼叫卫兵及追击移动）
void CreatureAI::AttackStart(Unit* who)
{
    if (m_creature->GetSettings().HasFlag(CreatureStaticFlags::COMBAT_PING))
    {
        if (Player* owner = dynamic_cast<Player*>(m_creature->GetSpawner()))
        {
            WorldPacket data(MSG_MINIMAP_PING, (8 + 4 + 4));
            data << m_creature->GetObjectGuid();
            data << m_creature->GetPositionX();
            data << m_creature->GetPositionY();
            owner->SendDirectMessage(data);
        }
    }

    if (!who || HasReactState(REACT_PASSIVE))
        return;

    bool targetChange = m_unit->GetVictim() != nullptr && m_unit->GetVictim() != who;
    if (m_creature->Attack(who, m_meleeEnabled))
    {
        m_creature->EngageInCombatWith(who);

        // Cast "Spawn Guard" to help Civilian (若具备呼叫卫兵标志则施放召唤卫兵法术)
        if (m_creature->GetSettings().HasFlag(CreatureStaticFlags::CALLS_GUARDS))
            m_creature->CastSpell(nullptr, 43783, TRIGGERED_OLD_TRIGGERED);

        HandleMovementOnAttackStart(who, targetChange);
    }
}

// 受伤回调：若生物带有 UNKILLABLE 标志且非秒杀伤害，首次即将致死时触发 JustPreventedDeath
void CreatureAI::DamageTaken(Unit* dealer, uint32& damage, DamageEffectType damageType, SpellEntry const* /*spellInfo*/)
{
    if (m_creature->GetSettings().HasFlag(CreatureStaticFlags::UNKILLABLE) && damageType != INSTAKILL)
    {
        if (m_creature->GetHealth() <= damage) // the damage will be reduced in Unit::DealDamage (实际锁血扣减在 Unit::DealDamage 中完成)
        {
            if (!m_deathPrevented)
            {
                m_deathPrevented = true;
                JustPreventedDeath(dealer);
            }
        }        
    }
}

// 跑回出生点到达后恢复原本坐骑外观
void CreatureAI::JustReachedHome()
{
    if (m_dismountOnAggro)
        if (CreatureInfo const* mountInfo = m_creature->GetMountInfo())
            m_creature->Mount(Creature::ChooseDisplayId(mountInfo));
}

// 重生时重置所有计时器与状态
void CreatureAI::JustRespawned()
{
    ResetAllTimers();
    Reset();
}

// 动态开启或关闭 UNKILLABLE 防死亡标志
void CreatureAI::SetDeathPrevention(bool state)
{
    if (state)
        m_creature->GetSettings().SetFlag(CreatureStaticFlags::UNKILLABLE);
    else
        m_creature->GetSettings().RemoveFlag(CreatureStaticFlags::UNKILLABLE);
}

// 执行假死演出：打断施法与移动、清除光环与目标、设为不可选中并播放假死法术
void CreatureAI::DoFakeDeath(uint32 spellId)
{
    m_creature->InterruptNonMeleeSpells(false);
    m_creature->InterruptMoving();
    m_creature->ClearComboPointHolders();
    m_creature->RemoveAllAurasOnDeath();
    m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);
    m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, false);
    m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
    m_creature->ClearAllReactives();
    m_creature->SetTarget(nullptr);
    m_creature->GetMotionMaster()->Clear();
    m_creature->GetMotionMaster()->MoveIdle();

    if (spellId)
        DoCastSpellIfCan(nullptr, spellId, CAST_INTERRUPT_PREVIOUS);
}

// 撤退到达友军附近时解锁并触发呼叫支援
void CreatureAI::RetreatingArrived()
{
    m_creature->SetNoCallAssistance(false);
    m_creature->CallAssistance();
}

// 撤退动作结束时清除状态并恢复追击目标
void CreatureAI::RetreatingEnded()
{
    if (GetAIOrder() != ORDER_RETREATING)
        return; // prevent stack overflow by cyclic calls - TODO: remove once Motion Master is human again (防止循环调用栈溢出)
    SetAIOrder(ORDER_NONE);
    SetCombatScriptStatus(false);
    if (!m_creature->IsAlive())
        return;
    DoStartMovement(m_creature->GetVictim());
}

// 在指定半径内寻找最近的可协助友军并发起撤退求援移动
bool CreatureAI::DoRetreat()
{
    Unit* victim = m_creature->GetVictim();
    if (!victim)
        return false;

    float radius = sWorld.getConfig(CONFIG_FLOAT_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS);
    if (radius <= 0)
        return false;

    Creature* ally = nullptr;

    MaNGOS::NearestAssistCreatureInCreatureRangeCheck check(m_creature, victim, radius);
    MaNGOS::CreatureLastSearcher<MaNGOS::NearestAssistCreatureInCreatureRangeCheck> searcher(ally, check);
    Cell::VisitGridObjects(m_creature, searcher, radius);

    // Check if an ally to call for was found (未找到可呼救的友军则取消撤退)
    if (!ally)
        return false;

    uint32 delay = sWorld.getConfig(CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY);

    Position pos;
    ally->GetFirstCollisionPosition(pos, ally->GetCombatReach(), ally->GetAngle(m_creature));
    m_creature->GetMotionMaster()->MoveRetreat(pos.x, pos.y, pos.z, ally->GetAngle(victim), delay);

    SetAIOrder(ORDER_RETREATING);
    SetCombatScriptStatus(true);
    return true;
}

// 向指定半径范围内的友军发出呼救
void CreatureAI::DoCallForHelp(float radius)
{
    m_creature->CallForHelp(radius);
}

// 响应呼救：根据阵营模板判定是受惊逃跑还是直接加入战斗攻击敌人
void CreatureAI::OnCallForHelp(Unit* enemy)
{
    if (FactionTemplateEntry const* factionTemplate = m_creature->GetFactionTemplateEntry())
    {
        if (factionTemplate->IsFleeFromCallForHelp())
        {
            if (m_creature->SetInPanic(10000))
                SetAIOrder(ORDER_FLEE_FROM_CALL_FOR_HELP);
            return;
        }
        if (!factionTemplate->IsRespondToCallForHelp())
            return;
    }
    AttackStart(enemy);
}

// 处理来自友军的援助呼叫事件，检查可协助性与视野条件后标记防连环呼救并响应
void CreatureAI::HandleAssistanceCall(Unit* sender, Unit* invoker)
{
    if (!invoker || m_creature->IsCritter())
        return;
    if (m_creature->CanAssistInCombatAgainst(sender, invoker) && m_creature->CanJoinInAttacking(invoker) && invoker->IsVisibleForOrDetect(m_creature, m_creature, false) && !m_creature->GetNoCallAssistance())
    {
        // only a creature that answers is marked: one whose faction ignores the call still calls for help when it is pulled
        // 只有实际响应呼救的生物才设置 NoCallAssistance 防止链式扩散
        FactionTemplateEntry const* factionTemplate = m_creature->GetFactionTemplateEntry();
        if (!factionTemplate || factionTemplate->IsFleeFromCallForHelp() || factionTemplate->IsRespondToCallForHelp())
            m_creature->SetNoCallAssistance(true);
        OnCallForHelp(invoker);
    }
}

// 开启目标不可达时传送目标的检测开关
void CreatureAI::AddUnreachabilityCheck()
{
    m_teleportUnreachable = true;
}

// 返回生物配置的法术列表
CreatureSpellList const& CreatureAI::GetSpellList() const
{
    return m_creature->GetSpellList();
}

// 定时逃跑结束：若为呼救惊慌或小动物逃跑则直接脱战，否则恢复正常 AI 指令
void CreatureAI::TimedFleeingEnded()
{
    UnitAI::TimedFleeingEnded();
    if (GetAIOrder() == ORDER_FLEE_FROM_CALL_FOR_HELP && m_creature->IsAlive())
    {
        if (FactionTemplateEntry const* factionTemplate = m_creature->GetFactionTemplateEntry())
            if (factionTemplate->IsFleeFromCallForHelp())
                EnterEvadeMode();
    }
    if (GetAIOrder() == ORDER_CRITTER_FLEE && m_creature->IsAlive())
    {
        SetCombatScriptStatus(false);
        EnterEvadeMode();
    }
    SetAIOrder(ORDER_NONE);
}

// 向指定目标发起跟随；若目标为玩家则向玩家申请阵型槽位角度与距离
void CreatureAI::RequestFollow(Unit* followee)
{
    if (followee->IsPlayer())
    {
        auto data = static_cast<Player*>(followee)->RequestFollowData(m_creature->GetObjectGuid());
        m_followAngle = data.first;
        m_followDist = data.second;
        m_requestedFollower = followee->GetObjectGuid();
    }
    m_creature->GetMotionMaster()->MoveFollow(followee, m_followDist, m_followAngle, m_creature->GetDefaultMovementType() == FOLLOW_MOTION_TYPE, false);
}

// 释放向玩家申请的跟随阵型槽位
void CreatureAI::RelinquishFollow(ObjectGuid follower)
{
    if (m_requestedFollower && (!follower || m_requestedFollower == follower))
        if (Unit* owner = m_creature->GetMap()->GetUnit(m_requestedFollower))
            if (owner->IsPlayer())
                static_cast<Player*>(owner)->RelinquishFollowData(m_creature->GetObjectGuid());
}
