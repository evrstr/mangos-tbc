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

#include "MotionGenerators/WrapperMovementGenerator.h"
#include "Entities/Creature.h"
#include "AI/BaseAI/UnitAI.h"
#include "Entities/TemporarySpawn.h"
#include "Movement/MoveSpline.h"
#include "Movement/MoveSplineInit.h"

/// 初始化包装生成器：中止旧样条、按需立即发射样条，并在非玩家冲锋面对目标时接战
void AbstractWrapperMovementGenerator::Initialize(Unit& owner)
{
    // Stop any previously dispatched splines no matter the source (停止之前残留样条)
    if (!owner.movespline->Finalized())
    {
        if (owner.IsClientControlled())
            owner.StopMoving(true);
        else
            owner.InterruptMoving();
    }

    if (!i_delayed && !i_dispatched)
    {
        i_spline.Launch();
        i_dispatched = true;
    }

    if (!owner.IsPlayerControlled())
    {
        if (i_id != 43153 && i_id != 30751) // Lynx Rush exception | Blade Dance Charge - can be done in AI - here for research (山猫冲锋/剑舞冲锋例外：不附加落地等待计时器)
            i_useTimer = true;
        if (owner.movespline->isFacingTarget())
        {
            if (Unit* target = ObjectAccessor::GetUnit(owner, ObjectGuid(owner.movespline->GetFacing().target)))
            {
                if (owner.CanAttackInCombat(target, false, false))
                {
                    owner.Attack(target, owner.AI()->IsMeleeEnabled());
                    owner.SetTarget(target);
                }
            }
        }
    }
}

/// 销毁前清理未完成的样条或补发到达通知
void AbstractWrapperMovementGenerator::Finalize(Unit& owner)
{
    // Stop any previously dispatched splines no matter the source (若样条未完成则停止移动，否则派发通知)
    if (!owner.movespline->Finalized())
    {
        if (owner.IsClientControlled())
            owner.StopMoving(true);
        else
            owner.InterruptMoving();
    }
    // Inform AI about spline completed (样条已走完且尚未通知时回调 AI)
    else if (!i_informed)
        Inform(owner);
}

void AbstractWrapperMovementGenerator::Interrupt(Unit& owner)
{
    i_dispatched = (!i_resident || owner.movespline->Finalized());
    owner.InterruptMoving();
}

void AbstractWrapperMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

/// 驱动延迟样条启动及 NPC 样条结束后的 1 秒缓冲计时器
bool AbstractWrapperMovementGenerator::Update(Unit& owner, const uint32& diff)
{
    if (i_delayed && !i_dispatched)
    {
        i_spline.Launch();
        i_dispatched = true;
    }

    bool finalized = owner.movespline->Finalized();
    if (finalized && i_useTimer && !i_informed)
        Inform(owner);

    if (finalized && i_useTimer)
    {
        if (i_timerSet)
        {
            m_delayTimer.Update(diff);
            return !m_delayTimer.Passed();
        }
        else
        {
            m_delayTimer.Reset(1000);
            i_timerSet = true;
            return true;
        }
    }

    return !finalized;
}

/// 向自身 AI、召唤者 AI 及关联 Relay 脚本发送位移完成事件
void AbstractWrapperMovementGenerator::Inform(Unit& owner)
{
    i_informed = true;

    const MovementGeneratorType type = GetMovementGeneratorType();

    if (UnitAI* ai = owner.AI())
        ai->MovementInform(type, i_id);

    if (owner.GetTypeId() == TYPEID_UNIT && static_cast<Creature&>(owner).IsTemporarySummon())
    {
        ObjectGuid const& spawnerGuid = owner.GetSpawnerGuid();

        if (spawnerGuid.IsCreatureOrPet())
        {
            if (Creature* spawner = owner.GetMap()->GetAnyTypeCreature(spawnerGuid))
            {
                if (UnitAI* ai = spawner->AI())
                    ai->SummonedMovementInform(&static_cast<Creature&>(owner), type, i_id);
            }
        }
    }

    if (m_relayId)
        owner.GetMap()->ScriptsStart(SCRIPT_TYPE_RELAY, m_relayId, &owner, m_guid ? owner.GetMap()->GetWorldObject(m_guid) : nullptr);
}

void EffectMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_PROPELLED);

    if (!i_delayed && !i_dispatched)
        owner.addUnitState(UNIT_STAT_ROAMING_MOVE);

    AbstractWrapperMovementGenerator::Initialize(owner);
}

void EffectMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE | UNIT_STAT_PROPELLED);
    AbstractWrapperMovementGenerator::Finalize(owner);
}

void EffectMovementGenerator::Interrupt(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_ROAMING_MOVE);
    AbstractWrapperMovementGenerator::Interrupt(owner);
}

bool EffectMovementGenerator::Update(Unit &owner, const uint32 &diff)
{
    if (i_delayed && !i_dispatched)
        owner.addUnitState(UNIT_STAT_ROAMING_MOVE);

    return AbstractWrapperMovementGenerator::Update(owner, diff);
}
