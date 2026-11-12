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

#include "GuardianAI.h"
#include "Grids/Cell.h"
#include "Grids/CellImpl.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"

// 构造函数：绑定跟随主人的默认移动模式与队形角度/距离
GuardianAI::GuardianAI(Creature* creature) : CreatureEventAI(creature)
{
    Unit* owner = creature->GetOwner();
    MANGOS_ASSERT(owner);
    m_defaultMovement = FOLLOW_MOTION_TYPE;
    m_followAngle = M_PI_F / 2;
    m_followDist = 1.5f;

    switch (creature->GetUInt32Value(UNIT_CREATED_BY_SPELL))
    {
        case 11939: // Thule Ravenclaw - Imp (特定任务小鬼使用更大的跟随距离)
            m_followDist = 3.f;
            break;
    }
}

// 重生/召唤就绪时初始化事件并开始跟随主人
void GuardianAI::JustRespawned()
{
    Unit* owner = m_creature->GetOwner();

    if (!owner)
        return;

    CreatureEventAI::JustRespawned();

    if (GetDefaultMovement() == FOLLOW_MOTION_TYPE)
        RequestFollow(owner);
}

// 每帧更新：在主动/防御姿态下自动协助处于战斗中的主人，随后执行 EventAI 主循环
void GuardianAI::UpdateAI(const uint32 diff)
{
    switch (m_reactState)
    {
        case REACT_AGGRESSIVE:
        case REACT_DEFENSIVE:
        {
            Unit* owner = m_creature->GetOwner();
            if (!owner)
                break;

            if (!m_creature->IsInCombat() && owner->IsInCombat() && !(m_creature->IsPet() && ((Pet*)m_creature)->getPetType() == MINI_PET))
                AttackStart(owner->getAttackerForHelper());   // check for getAttackerForHelper() == nullpter in AttackStart() (在 AttackStart 中已对空指针判空)
            break;
        }
        default:
            break;
    }

    CreatureEventAI::UpdateAI(diff);
}

// 许可度检查：仅限 GUARDIAN_PET 类型守护者宠物使用
int GuardianAI::Permissible(const Creature* creature)
{
    if (creature->IsPet() && ((Pet*)creature)->getPetType() == GUARDIAN_PET)
        return PERMIT_BASE_SPECIAL;

    return PERMIT_BASE_NO;
}

// 死亡时释放主人跟随槽位
void GuardianAI::JustDied(Unit* killer)
{
    CreatureEventAI::JustDied(killer);
    RelinquishFollow(m_unit->GetOwnerGuid());
}

// 取消召唤时释放主人跟随槽位
void GuardianAI::OnUnsummon()
{
    CreatureEventAI::OnUnsummon();
    RelinquishFollow(m_unit->GetOwnerGuid());
}

// 脱战重置并触发 EventAI 中的 EVENT_T_EVADE 事件
void GuardianAI::EnterEvadeMode()
{
    UnitAI::EnterEvadeMode();

    // Handle Evade events (执行 EAI 脱战事件)
    IncreaseDepthIfNecessary();
    for (auto& i : m_CreatureEventAIList)
    {
        if (i.event.event_type == EVENT_T_EVADE)
            CheckAndReadyEventForExecution(i);
    }
    ProcessEvents();
}
