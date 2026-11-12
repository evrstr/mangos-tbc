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

#include "MotionGenerators/PointMovementGenerator.h"
#include "Movement/MoveSpline.h"
#include "Movement/MoveSplineInit.h"
#include "Entities/Creature.h"
#include "Entities/TemporarySpawn.h"
#include "AI/BaseAI/UnitAI.h"

//----- Point Movement Generator (点对点移动生成器实现)

/// 初始化点移动：先停止旧样条，添加漫游状态并下发移动指令
void PointMovementGenerator::Initialize(Unit& unit)
{
    if (unit.hasUnitState(UNIT_STAT_NO_FREE_MOVE | UNIT_STAT_NOT_MOVE))
        return;

    // Stop any previously dispatched splines no matter the source (停止之前任意来源残留的未完成样条)
    if (!unit.movespline->Finalized() && !m_speedChanged)
    {
        if (unit.IsClientControlled())
            unit.StopMoving(true);
        else
            unit.InterruptMoving();
    }

    unit.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    Move(unit);

    m_speedChanged = false;
}

/// 结束点移动：清除漫游状态；若未到达则中断移动，已到达则触发 MovementInform
void PointMovementGenerator::Finalize(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    // Stop any previously dispatched splines no matter the source (若样条尚未结束则强制停止，否则派发到达通知)
    if (!unit.movespline->Finalized())
    {
        if (unit.IsClientControlled())
            unit.StopMoving(true);
        else
            unit.InterruptMoving();
    }
    else
        MovementInform(unit);
}

void PointMovementGenerator::Interrupt(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    unit.InterruptMoving();
}

void PointMovementGenerator::Reset(Unit& unit)
{
    unit.addUnitState(UNIT_STAT_ROAMING);
    Initialize(unit);
}

/// 每帧检测：若受控无法移动则清除移动子状态；若速度变化或需重启则重新下发样条
bool PointMovementGenerator::Update(Unit& unit, const uint32&/* diff*/)
{
    if (unit.hasUnitState(UNIT_STAT_CAN_NOT_MOVE))
    {
        unit.clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return true;
    }

    if ((!unit.hasUnitState(UNIT_STAT_ROAMING_MOVE) && unit.movespline->Finalized()) || m_speedChanged)
        Initialize(unit);

    return !unit.movespline->Finalized();
}

/// 组装 MoveSplineInit 并启动前往目标坐标的移动
void PointMovementGenerator::Move(Unit& unit)
{
    Movement::MoveSplineInit init(unit);
    init.MoveTo(m_x, m_y, m_z, m_generatePath);
    if (m_forcedMovement == FORCED_MOVEMENT_WALK)
        init.SetWalk(true);
    else if (m_forcedMovement == FORCED_MOVEMENT_RUN)
        init.SetWalk(false);
    else
        init.SetWalk(!unit.hasUnitState(UNIT_STAT_RUNNING));
    if (m_forcedMovement == FORCED_MOVEMENT_FLIGHT)
        init.SetFly();
    if (m_o != 0.f)
        init.SetFacing(m_o);
    init.SetVelocity(m_speed);
    init.Launch();
}

/// 到达目标点后依次回调自身 AI、召唤者 AI 及关联 Relay 脚本
void PointMovementGenerator::MovementInform(Unit& unit)
{
    MovementGeneratorType const type = GetMovementGeneratorType();

    if (UnitAI* ai = unit.AI())
        ai->MovementInform(type, m_id);

    if (unit.GetTypeId() == TYPEID_UNIT && static_cast<Creature&>(unit).IsTemporarySummon())
    {
        if (unit.GetSpawnerGuid().IsAnyTypeCreature())
            if (Creature* pSummoner = unit.GetMap()->GetAnyTypeCreature(unit.GetSpawnerGuid()))
            {
                if (UnitAI* ai = pSummoner->AI())
                    ai->SummonedMovementInform(static_cast<Creature*>(&unit), type, m_id);
            }
    }

    if (m_relayId)
        unit.GetMap()->ScriptsStart(SCRIPT_TYPE_RELAY, m_relayId, &unit, m_guid ? unit.GetMap()->GetWorldObject(m_guid) : nullptr);
}

/// 初始化撤退移动：非玩家控制单位清除当前目标，并开始前往撤退点
void RetreatMovementGenerator::Initialize(Unit& unit)
{
    // Non-client controlled unit with an AI should drop target (非客户端控制单位进入撤退时放弃当前目标)
    if (unit.AI() && !unit.IsClientControlled())
    {
        unit.SetTarget(nullptr);
        // FIXME: do we need to send attack stop here? verify protocol if possible
    }

    unit.addUnitState(UNIT_STAT_RETREATING);

    if (!m_arrived)
        PointMovementGenerator::Initialize(unit);
}

void RetreatMovementGenerator::Finalize(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_RETREATING);

    PointMovementGenerator::Finalize(unit);

    if (UnitAI* ai = unit.AI())
        ai->RetreatingEnded();
}

void RetreatMovementGenerator::Interrupt(Unit& unit)
{
    PointMovementGenerator::Interrupt(unit);

    if (UnitAI* ai = unit.AI())
        ai->RetreatingEnded();
}

void RetreatMovementGenerator::Reset(Unit& unit)
{
    if (!m_arrived)
        PointMovementGenerator::Reset(unit);
}

/// 到达撤退点后通知 AI RetreatingArrived，并开始消耗驻留延迟计时器
bool RetreatMovementGenerator::Update(Unit& unit, const uint32& diff)
{
    if (!PointMovementGenerator::Update(unit, diff))
    {
        if (!m_arrived)
        {
            m_arrived = true;

            if (UnitAI* ai = unit.AI())
                ai->RetreatingArrived();
        }

        m_delayTimer.Update(diff);
        return !m_delayTimer.Passed();
    }

    return true;
}

void StayMovementGenerator::Initialize(Unit &unit)
{
    unit.addUnitState(UNIT_STAT_STAY);
    PointMovementGenerator::Initialize(unit);
}

void StayMovementGenerator::Finalize(Unit &unit)
{
    unit.clearUnitState(UNIT_STAT_STAY);
    PointMovementGenerator::Finalize(unit);
}

void StayMovementGenerator::Interrupt(Unit& unit)
{
    m_arrived = false;
    PointMovementGenerator::Interrupt(unit);
}

bool StayMovementGenerator::Update(Unit& unit, const uint32& diff)
{
    if (!m_arrived && !PointMovementGenerator::Update(unit, diff))
    {
        unit.clearUnitState(UNIT_STAT_ROAMING_MOVE);
        m_arrived = true;
    }
    return true;
}

void PointTOLMovementGenerator::Move(Unit& unit)
{
    Movement::MoveSplineInit init(unit);
    init.MoveTo(m_x, m_y, m_z, false);
    if (m_forcedMovement == FORCED_MOVEMENT_WALK)
        init.SetWalk(true);
    else
        init.SetWalk(!unit.hasUnitState(UNIT_STAT_RUNNING));
    if (m_o != 0.f)
        init.SetFacing(m_o);
    init.SetVelocity(m_speed);
    init.SetFly();
    init.SetWalk(!unit.hasUnitState(UNIT_STAT_RUNNING));
    init.Launch();
}
