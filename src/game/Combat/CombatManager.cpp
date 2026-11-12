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

#include "CombatManager.h"

#include "Entities/Pet.h"
#include "Maps/Map.h"
#include "World/World.h"

#define COMBAT_MANAGER_TICK 1200 // 战斗牵引/脱战检查间隔（1.2 秒）

// 构造函数：初始化战斗管理器状态
CombatManager::CombatManager(Unit* owner) : m_owner(owner), m_evadeTimer(0), m_evadeState(EVADE_NONE), m_combatTick(COMBAT_MANAGER_TICK), m_combatTimer(0), m_leashingDisabled(false), m_leashingCheck(nullptr)
{

}

/*
    Creature Evade Timer & Leash range (生物逃脱计时器与牵引范围说明)

    1) Leash timer 15 sec if creature can reach you
    - Timer will be reset upon if player deals damage or if creature does damage to player
    - Only direct damage will reset damage
    (若生物可到达玩家，牵引计时为 15 秒；双方发生直接伤害时重置计时)

    2) There is no timer for creature that cannot reach you
    - Creature appears to be evading while it cannot reach you so player cannot deal any damage
    - After 90yd it will drop combat because player is too far.
    (若生物无法到达玩家，则进入逃脱状态免疫伤害，超出 90 码距离后脱离战斗)

    3) Special creatures have leash range and timer
    - No way to find out range as of now on WoD
    (部分特殊生物配有独立的牵引距离与计时规则)
*/

// 更新战斗计时器、逃脱计时器与牵引距离检测
void CombatManager::Update(const uint32 diff)
{
    if (m_owner->IsInCombat())
    {
        if (m_evadeTimer)
        {
            if (m_evadeTimer <= diff)
            {
                m_evadeTimer = 0;
                m_owner->EvadeTimerExpired();
            }
            else
                m_evadeTimer -= diff;
        }

        if (!m_leashingDisabled)
        {
            if (m_combatTimer)
            {
                if (m_owner->IsPlayerControlled() || !m_owner->IsCrowdControlled())
                {
                    if (m_combatTimer <= diff)
                        m_combatTimer = 0;
                    else
                        m_combatTimer -= diff;
                }
            }
        }

        m_combatTick += diff;
        if (m_combatTick >= COMBAT_MANAGER_TICK)
        {
            m_combatTick -= COMBAT_MANAGER_TICK;
            if (!m_leashingDisabled)
            {
                // disabled in instances except for players in BGs (副本地图默认禁用常规牵引脱战，但受玩家控制的单位除外)
                if (!m_owner->GetMap()->IsDungeon() || m_owner->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
                {
                    if (!m_owner->GetMap()->IsDungeon() && m_owner->IsImmobilizedState())
                        m_owner->getThreatManager().DeleteOutOfRangeReferences();

                    if (!m_combatTimer)
                    {
                        bool check = !m_owner->HasMaster();
                        if (!check)
                        {
                            Unit* master = m_owner->GetMaster();
                            if (!master || !master->IsAlive()) // if charmer alive, he will evade this charm (若主人/魅惑者存活，则由其统一控制脱战)
                                check = true;
                        }
                        if (check)
                        {
                            if (m_owner->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
                            {
                                if (m_owner->getHostileRefManager().getSize() == 0)
                                    m_owner->HandleExitCombat(false, m_owner->IsPlayer());
                            }
                            // if timer ran out and we are far away from last refresh pos, evade (战斗计时归零且目标远离上次刷新位置时触发脱战)
                            else if (m_owner->GetVictim() && m_owner->GetVictim()->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
                            {
                                // immobilized passive mobs ignore last refresh pos (定身且被动的生物直接脱战)
                                if (m_owner->IsImmobilizedState() && m_owner->AI()->GetReactState() == REACT_PASSIVE)
                                    m_owner->HandleExitCombat(false);
                                else if (m_owner->GetVictim()->GetDistance2d(m_lastRefreshPos.GetPositionX(), m_lastRefreshPos.GetPositionY()) > sWorld.getConfig(CONFIG_FLOAT_LEASH_RADIUS))
                                    m_owner->HandleExitCombat(false);
                            }
                        }
                    }
                }
            }

            if (m_owner->IsCreature() && !m_owner->HasCharmer()) // charmer should have leashing check or leash set (被魅惑生物交由魅惑者判断牵引范围)
            {
                Creature* creatureOwner = static_cast<Creature*>(m_owner);
                // If creature is within 30yd from combat start do not exit combat (检查自定义牵引回调或生物表配置的 Leash 最大距离)
                if (!creatureOwner->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
                {
                    if (m_leashingCheck)
                    {
                        float x, y, z;
                        creatureOwner->GetPosition(x, y, z);
                        if (m_leashingCheck(creatureOwner, x, y, z))
                            creatureOwner->HandleExitCombat(true);
                    }
                    else if (creatureOwner->GetCreatureInfo()->Leash) // If creature has set maximum leashing distance (若生物设置了最大牵引距离则校验当前坐标与开战坐标的距离)
                    {
                        Position pos;
                        creatureOwner->GetCombatStartPosition(pos);
                        if (creatureOwner->GetDistance2d(pos.GetPositionX(), pos.GetPositionY()) > creatureOwner->GetCreatureInfo()->Leash)
                            creatureOwner->HandleExitCombat(true);
                    }
                }
            }
        }
    }
}

// 停止当前逃脱状态：优先清除逃脱计时器，否则重置逃脱枚举为 EVADE_NONE
void CombatManager::StopEvade()
{
    if (m_evadeTimer)
    {
        m_evadeTimer = 0;
        return;
    }
    SetEvadeState(EVADE_NONE);
}

// 辅助仿函数：向所有受控单位同步设置逃脱状态
struct SetEvadeHelper
{
    explicit SetEvadeHelper(EvadeState _state) : state(_state) {}
    void operator()(Unit* unit) const { unit->GetCombatManager().SetEvadeState(state); }
    EvadeState state;
};

// 设置逃脱状态，同步更新 AI 命令并向宠物/图腾/守护者等受控单位传播
void CombatManager::SetEvadeState(EvadeState state)
{
    if (m_evadeState == state)
        return;

    if (state == EVADE_NONE)
        m_evadeTimer = 0;
    if (state == EVADE_NONE && m_evadeState == EVADE_HOME)
        m_owner->AI()->SetAIOrder(ORDER_NONE);
    else if (state == EVADE_HOME)
        m_owner->AI()->SetAIOrder(ORDER_EVADE);
    m_evadeState = state;

    // Do not propagate during charm (处于被魅惑状态时不向自身受控单位传播)
    if (!m_owner->HasCharmer())
        m_owner->CallForAllControlledUnits(SetEvadeHelper(state), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

// 开战时使下一次 Update 立即执行牵引检测
void CombatManager::OnCombatStart()
{
    m_combatTick = COMBAT_MANAGER_TICK;
}

// 若目标具备仇恨列表，从攻击者视角刷新目标的战斗计时器
void CombatManager::TriggerCombatTimer(Unit* target)
{
    if (target->CanHaveThreatList()) // From attackers PoV if we have victim and victim can have a threat list then set his timer
        target->GetCombatManager().TriggerCombatTimer(target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED) && m_owner->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED));
}

// 按 PVP(5 秒) 或 PVE(单位 Pursuit 追击时长) 刷新战斗计时器
void CombatManager::TriggerCombatTimer(bool pvp)
{
    m_combatTimer = pvp ? 5000 : m_owner->GetPursuit();
    m_lastRefreshPos = m_owner->GetPosition();
}

// 使用指定时间刷新战斗计时器并记录位置
void CombatManager::TriggerCombatTimer(uint32 timer)
{
    m_combatTimer = timer;
    m_lastRefreshPos = m_owner->GetPosition();
}
