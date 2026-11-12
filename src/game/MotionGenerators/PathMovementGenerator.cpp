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

#include "MotionGenerators/PathMovementGenerator.h"
#include "Movement/MoveSpline.h"
#include "Movement/MoveSplineInit.h"
#include "Entities/Creature.h"
#include "Entities/TemporarySpawn.h"
#include "AI/BaseAI/UnitAI.h"
#include "Entities/Player.h"

#include <algorithm>

AbstractPathMovementGenerator::AbstractPathMovementGenerator(const Movement::PointsArray& path, std::optional<float> orientation, int32 offset /* = 0*/, bool cyclic /* = true*/) :
    m_pathIndex(offset), m_orientation(orientation), m_cyclic(cyclic), m_firstCycle(false), m_startPoint(0), m_speedChanged(false)
{
    for (size_t i = 0; i < path.size(); ++i)
        m_path[i] = { path[i].x, path[i].y, path[i].z, ((i + 1) == path.size() ? orientation : 0), 0, 0 };

    // Current spline points array and spline end orientation are set in Initialize() method
    m_spline.reserve(m_path.size());
}

AbstractPathMovementGenerator::AbstractPathMovementGenerator(const WaypointPath* path, int32 offset/* = 0*/, bool cyclic/* = false*/, ObjectGuid guid /*= ObjectGuid()*/) :
    m_pathIndex(offset), m_orientation(std::nullopt), m_cyclic(cyclic), m_firstCycle(false), m_startPoint(0), m_guid(guid), m_speedChanged(false)
{
    if (!path)
        return;
    // NOTE:
    // Data in WaypointManager can be removed at run time, we cant really reference it and have to make a copy just in case
    // Possibly remake storage in WaypointManager with smart pointers so we can reference it instead
    // WaypointManager 中的数据可能在运行时被重载/删除，因此此处做深拷贝；若任一节点有延迟等待则自动禁用循环模式
    m_path = *path;
    for (auto& data : m_path)
    {
        if (data.second.delay)
        {
            m_cyclic = false;
            break;
        }
    }

    // Current spline points array, spline end orientation, and spline point index are set in Initialize() method
    m_spline.reserve(m_path.size());
}

/// 初始化分段样条：停止旧移动，按是否循环抽取从 m_pathIndex 到下一个停顿点（或全程闭环）的控制点
void AbstractPathMovementGenerator::Initialize(Unit& unit)
{
    // Stop any previously dispatched splines no matter the source (先中止之前的残余样条)
    if (!unit.movespline->Finalized())
    {
        if (unit.IsClientControlled())
            unit.StopMoving(true);
        else
            unit.InterruptMoving();
    }

    if (m_path.empty())
    {
        sLog.outError("AbstractPathMovementGenerator::Initialize Path empty for unit name %s entry %u dbguid %u.", unit.GetName(), unit.GetEntry(), unit.GetDbGuid());
        return;
    }

    // Reload current spline points array and end orientation from path: (重新填充当前分段控制点及终点朝向)
    m_spline.clear();
    m_spline.push_back(Vector3()); // will be set to current position (索引 0 留作起点，Launch 时自动填为单位当前坐标)

    if (!m_cyclic)
    {
        for (auto itr = std::next(m_path.begin(), m_pathIndex); itr != m_path.end(); ++itr)
        {
            auto& node = (*itr).second;
            m_spline.push_back({ node.x, node.y, node.z });

            if ((node.delay || std::next(itr, 1) == m_path.end()))
            {
                m_timer.Reset(node.delay);
                m_orientation = node.orientation;
                break;
            }
        }
    }
    else // start at last point and cycle through it all (循环模式：从当前偏移开始绕行整条闭合路径)
    {
        uint32 count = 0;
        for (auto itr = std::next(m_path.begin(), m_pathIndex); itr != m_path.end(); ++itr, ++count)
        {
            auto& node = (*itr).second;
            m_spline.push_back({ node.x, node.y, node.z });
        }
        if (count < m_path.size())
        {
            for (auto itr = m_path.begin(); itr != m_path.end() && count < m_path.size(); ++itr, ++count)
            {
                auto& node = (*itr).second;
                m_spline.push_back({ node.x, node.y, node.z });
            }
        }
    }

    m_speedChanged = false;
}

void AbstractPathMovementGenerator::Finalize(Unit& unit)
{
    // Stop any previously dispatched splines no matter the source
    if (!unit.movespline->Finalized())
    {
        if (unit.IsClientControlled())
            unit.StopMoving(true);
        else
            unit.InterruptMoving();
    }
}

void AbstractPathMovementGenerator::Interrupt(Unit& unit)
{
    unit.InterruptMoving();
}

void AbstractPathMovementGenerator::Reset(Unit& unit)
{
    Initialize(unit);
}

/// 驱动路径推进：逐个派发已跨越路点的通知，并在段末等待结束后切入下一段
bool AbstractPathMovementGenerator::Update(Unit& unit, const uint32& diff)
{
    if (!m_spline.empty())
    {
        // Foolproofing if current spline was tampered with or ran out of splines in the path:
        // 防呆检查：若当前样条被外部篡改或提前终止，尝试重新初始化或退出生成器
        if (unit.movespline->FinalDestination() != m_spline.back())
        {
            if (unit.movespline->Finalized()) // stopped movement evidence
                Initialize(unit);
            else
                return false;
        }

        if (m_cyclic)
        {
            for (; m_pathIndex != unit.movespline->currentPathIdx();)
            {
                if (!m_firstCycle || m_pathIndex != 0)
                    MovementInform(unit);
                m_pathIndex = (m_pathIndex + 1) % (m_path.size() + 1);
                if (unit.movespline->Finalized())
                    return true;
            }
            if (m_pathIndex == m_startPoint)
                m_firstCycle = false;
        }
        else
        {
            for (; m_pathIndex < unit.movespline->currentPathIdx(); ++m_pathIndex)
                MovementInform(unit);
        }

        if (!unit.movespline->Finalized())
        {
            if (m_speedChanged)
                Initialize(unit);

            return true;
        }
    }

    m_timer.Update(diff);

    // Wait for timer: (节点停顿计时未到，继续原地等待)
    if (!m_timer.Passed())
        return true;

    // Load next spline from the path if still needed (停顿结束且仍有后续路点时加载下一段样条)
    if (size_t(m_pathIndex) < m_path.size())
    {
        Initialize(unit);
        return true;
    }
    return false;
}

void AbstractPathMovementGenerator::MovementInform(Unit& unit)
{
    const uint32 index = (*std::next(m_path.begin(), ((m_pathIndex + (m_firstCycle ? 0 : 1)) % m_path.size()))).first;

    WaypointNode const& node = m_path[index];

    if (node.script_id)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature movement start script %u at point %u for %s.", node.script_id, index, unit.GetGuidStr().c_str());
        unit.GetMap()->ScriptsStart(SCRIPT_TYPE_CREATURE_MOVEMENT, node.script_id, &unit, m_guid ? unit.GetMap()->GetWorldObject(m_guid) : &unit);
    }

    // unit.SummonCreature(1, node.x, node.y, node.z, node.orientation, TEMPSPAWN_TIMED_DESPAWN, 5000);

    MovementGeneratorType const type = GetMovementGeneratorType();

    if (UnitAI* ai = unit.AI())
        ai->MovementInform(type, index);

    if (unit.GetTypeId() == TYPEID_UNIT && static_cast<Creature&>(unit).IsTemporarySummon())
    {
        if (unit.GetSpawnerGuid().IsCreatureOrPet())
        {
            if (Creature* pSummoner = unit.GetMap()->GetAnyTypeCreature(unit.GetSpawnerGuid()))
            {
                if (UnitAI* ai = pSummoner->AI())
                    ai->SummonedMovementInform(static_cast<Creature*>(&unit), type, index);
            }
        }
    }
}

FixedPathMovementGenerator::FixedPathMovementGenerator(Unit& creature, int32 pathId, WaypointPathOrigin wpOrigin, ForcedMovement forcedMovement, bool flying, float speed, int32 offset, bool cyclic, ObjectGuid guid) :
    AbstractPathMovementGenerator((pathId || wpOrigin != PATH_NO_PATH ? sWaypointMgr.GetPathFromOrigin(creature.GetEntry(), creature.GetDbGuid(), pathId, (wpOrigin == PATH_NO_PATH && pathId ? PATH_FROM_ENTRY : wpOrigin))
        : sWaypointMgr.GetDefaultPath(creature.GetEntry(), creature.GetDbGuid())), offset, cyclic, guid), m_flying(flying), m_speed(speed), m_forcedMovement(forcedMovement)
{
}

FixedPathMovementGenerator::FixedPathMovementGenerator(Creature& creature) :
    AbstractPathMovementGenerator((creature.GetMotionMaster()->GetPathId() ? sWaypointMgr.GetPathFromOrigin(creature.GetEntry(), creature.GetDbGuid(), creature.GetMotionMaster()->GetPathId(), PATH_FROM_ENTRY)
        : sWaypointMgr.GetDefaultPath(creature.GetEntry(), creature.GetDbGuid())), 0, true), m_flying(true), m_speed(0.f), m_forcedMovement(FORCED_MOVEMENT_NONE)
{
}

void FixedPathMovementGenerator::Initialize(Unit& unit)
{
    unit.addUnitState(UNIT_STAT_ROAMING);

    AbstractPathMovementGenerator::Initialize(unit);

    if (m_spline.size() && Move(unit))
    {
        m_firstCycle = true;
        m_startPoint = m_pathIndex;
        unit.addUnitState(UNIT_STAT_ROAMING_MOVE);
    }
}

void FixedPathMovementGenerator::Finalize(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    AbstractPathMovementGenerator::Finalize(unit);
}

void FixedPathMovementGenerator::Interrupt(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    AbstractPathMovementGenerator::Interrupt(unit);
}

void FixedPathMovementGenerator::Reset(Unit& unit)
{
    unit.addUnitState(UNIT_STAT_ROAMING);
    AbstractPathMovementGenerator::Reset(unit);
}

/// 构造并启动固定路径的 MoveSplineInit（非飞行模式下校验 11-bit 偏移边界）
bool FixedPathMovementGenerator::Move(Unit& unit) const
{
    Movement::MoveSplineInit init(unit);
    init.MovebyPath(m_spline);
    if (m_forcedMovement == FORCED_MOVEMENT_WALK)
        init.SetWalk(true);
    else if (m_forcedMovement == FORCED_MOVEMENT_RUN)
        init.SetWalk(false);
    else
        init.SetWalk(!unit.hasUnitState(UNIT_STAT_RUNNING));
    if (m_flying)
        init.SetFly();
    else
    {
        // non catmullrom (非平滑模式使用 11-bit 压缩偏移，需先更新当前样条位置并检查封包边界)
        if (!unit.movespline->Finalized())
            unit.UpdateSplinePosition(true);
        Position pos = unit.GetPosition(unit.GetTransport());
        init.Path()[0].x = pos.x; init.Path()[0].y = pos.y; init.Path()[0].z = pos.z;
        if (!init.CheckBounds())
            ERROR_DB_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Path movement non-catmullrom pathId %u point %u for %s has invalid out of bounds spline. Likely meant to be split into several. (by script action or waittime)", unit.GetMotionMaster()->GetPathId(), m_pathIndex, unit.GetName());
    }
    if (m_orientation)
        init.SetFacing(*m_orientation);
    if (m_speed != 0.f)
        init.SetVelocity(m_speed);
    if (m_cyclic)
        init.SetCyclic();
    init.SetFirstPointId(m_pathIndex);
    return bool(init.Launch());
}

void FixedPathMovementGenerator::AddToPathPauseTime(int32 waitTimeDiff, bool force)
{
    if (force)
    {
        m_timer.Reset(waitTimeDiff);
        m_spline.clear();
    }
    else if (!m_timer.Passed())
    {
        // creature is stopped already
        // Prevent <= 0, the code in Update requires to catch the change from moving to not moving
        // 避免等待时间归零或为负，确保 Update 能捕获到等待结束的状态转换
        int32 newWaitTime = m_timer.GetExpiry() + waitTimeDiff;
        m_timer.Reset(newWaitTime > 0 ? newWaitTime : 1);
    }
    // TODO: Add AI delay future support
}

/// 初始化航线飞行：剥夺客户端移动控制权并标记 TAXI_FLIGHT 状态
void TaxiMovementGenerator::Initialize(Unit& unit)
{
    AbstractPathMovementGenerator::Initialize(unit);

    // Client-controlled unit should have control removed (飞行过程中临时接管玩家控制权)
    if (const Player* controllingClientPlayer = unit.GetClientControlling())
        controllingClientPlayer->UpdateClientControl(&unit, false);

    unit.addUnitState(UNIT_STAT_TAXI_FLIGHT);
    unit.SetFlag(UNIT_FIELD_FLAGS, (UNIT_FLAG_CLIENT_CONTROL_LOST | UNIT_FLAG_TAXI_FLIGHT));
}

/// 结束航线飞行：清除状态标志并归还客户端控制权
void TaxiMovementGenerator::Finalize(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_TAXI_FLIGHT);
    unit.RemoveFlag(UNIT_FIELD_FLAGS, (UNIT_FLAG_CLIENT_CONTROL_LOST | UNIT_FLAG_TAXI_FLIGHT));

    if (unit.GetTypeId() == TYPEID_PLAYER)
        unit.RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_TAXI_BENCHMARK);

    // Client-controlled unit should have control restored (归还玩家客户端控制权)
    if (const Player* controllingClientPlayer = unit.GetClientControlling())
        controllingClientPlayer->UpdateClientControl(&unit, true);

    AbstractPathMovementGenerator::Finalize(unit);
}

void TaxiMovementGenerator::Interrupt(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_TAXI_FLIGHT);
    AbstractPathMovementGenerator::Interrupt(unit);
}

/// 更新航线飞行进度，并在当前样条走完时切入下一段航线
bool TaxiMovementGenerator::Update(Unit& unit, const uint32& diff)
{
    if (unit.GetTypeId() != TYPEID_PLAYER)
        return (AbstractPathMovementGenerator::Update(unit, diff) || Resume(unit));

    Player& player = static_cast<Player&>(unit);

    Movement::MoveSpline* spline = player.movespline;
    const bool movement = !spline->Finalized();

    if (!player.OnTaxiFlightUpdate(size_t(spline->currentPathIdx()), movement))
    {
        // Some other code messed with our spline before it was completed. Have it your way, no taxi for you!
        // 若飞行样条在未完成前被其他逻辑意外打断，直接弹出坐骑结束飞行
        player.OnTaxiFlightEject();
        return false;
    }

    return (movement || Resume(player));
}

#define TAXI_FLIGHT_SPEED        32.0f // 飞行坐骑固定飞行速度（码/秒）

bool TaxiMovementGenerator::Move(Unit& unit)
{
    Movement::MoveSplineInit init(unit);
    Movement::PointsArray& spline = init.Path();
    std::copy((m_spline.begin() + m_pathIndex), m_spline.end(), back_inserter(spline));
    init.SetFirstPointId(m_pathIndex);
    init.SetFly();
    init.SetVelocity(TAXI_FLIGHT_SPEED);
    return bool(init.Launch());
}

/// 从 PlayerTaxi 中加载下一段跨地图/多跳航线节点并续飞
bool TaxiMovementGenerator::Resume(Unit& unit)
{
    if (unit.GetTypeId() == TYPEID_PLAYER)
    {
        Player& player = static_cast<Player&>(unit);

        if (player.GetMountID())
            player.OnTaxiFlightSplineEnd();

        if (!player.OnTaxiFlightSplineUpdate())
            return false;

        // Load and execute the spline (transitional populating the parent movegen: to be reworked in the future)

        auto nodes = player.GetTaxiPathSpline();
        m_path.clear();
        m_spline.clear();
        m_spline.reserve(nodes.size());

        uint32 i = 0;
        for (auto itr = nodes.begin(); itr != nodes.end(); ++itr, ++i)
        {
            m_path.emplace(i, WaypointNode((*itr)->x, (*itr)->y, (*itr)->z , std::nullopt, 0, 0));
            m_spline.push_back({ (*itr)->x, (*itr)->y, (*itr)->z });
        }

        m_pathIndex = player.GetTaxiPathSplineOffset();
    }

    return Move(unit);
}