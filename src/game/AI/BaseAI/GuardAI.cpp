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

#include "GuardAI.h"
#include "Entities/Creature.h"
#include "Entities/Player.h"
#include "World/World.h"

// 许可度检查：仅守卫类型生物适用 GuardAI
int GuardAI::Permissible(const Creature* creature)
{
    if (creature->IsGuard())
        return PERMIT_BASE_SPECIAL;

    return PERMIT_BASE_NO;
}

GuardAI::GuardAI(Creature* creature) : CreatureAI(creature)
{
}

// 视野索敌逻辑：优先协助受攻击的玩家、宠物、守卫与平民，其次按警戒半径主动攻击敌对单位
void GuardAI::MoveInLineOfSight(Unit* who)
{
    // Ignore Z for flying creatures (对不可飞行的守卫，忽略高度差过大的飞行目标)
    if (!m_creature->CanFly() && who->IsFlying() && m_creature->GetDistanceZ(who) > (IsRangedUnit() ? CREATURE_Z_ATTACK_RANGE_RANGED : CREATURE_Z_ATTACK_RANGE_MELEE))
        return;

    if (m_creature->GetVictim())
        return;

    if (who->IsInCombat() && m_creature->CanAssist(who))
    {
        Unit* victim = who->getAttackerForHelper();

        if (!victim)
            return;

        if (m_creature->CanInitiateAttack() && m_creature->CanAttackOnSight(victim) && victim->isInAccessablePlaceFor(m_creature))
        {
            // 友方玩家被非玩家怪物攻击：需距离玩家 5 码、怪物 10 码内且通视才出手相助
            if (who->GetTypeId() == TYPEID_PLAYER && victim->GetTypeId() != TYPEID_PLAYER)
            {
                if (m_creature->IsWithinDistInMap(who, 5.0) && m_creature->IsWithinDistInMap(victim, 10.0) && m_creature->IsWithinLOSInMap(victim, true))
                {
                    AttackStart(victim);
                }
            }
            // PVP 战斗（玩家或玩家宠物互殴）：守卫在 30 码内通视即可介入
            else if ((who->GetTypeId() == TYPEID_PLAYER && victim->GetTypeId() == TYPEID_PLAYER) || (victim->GetObjectGuid().IsCreature() && ((Creature*)victim)->IsPet() && ((Creature*)victim)->GetOwnerGuid().IsPlayer()))
            {
                if (m_creature->IsWithinDistInMap(who, 30.0) && m_creature->IsWithinLOSInMap(who, true))
                {
                    AttackStart(victim);
                }
            }
            // 同阵营守卫或平民遇袭：20 码内通视即前往支援
            else if (who->GetObjectGuid().IsCreature())
            {
                if (((Creature*)who)->IsGuard() || ((Creature*)who)->IsCivilian())
                {
                    if (m_creature->IsWithinDistInMap(who, 20.0) && m_creature->IsWithinLOSInMap(who, true))
                    {
                        AttackStart(victim);
                    }
                }
            }
        }
    }
    else
    {
        // 非战斗单位进入普通攻击警戒距离则主动攻击
        if (m_creature->CanInitiateAttack() && m_creature->CanAttackOnSight(who) && who->isInAccessablePlaceFor(m_creature))
        {
            float attackRadius = m_creature->GetAttackDistance(who);
            if (m_creature->IsWithinDistInMap(who, attackRadius) && m_creature->IsWithinLOSInMap(who, true))
            {
                AttackStart(who);
            }
        }
    }
}
