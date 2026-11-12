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

#include "Entities/Creature.h"
#include "Entities/Player.h"
#include "Movement/MoveSplineInit.h"
#include "Movement/MoveSpline.h"
#include "MotionGenerators/RandomMovementGenerator.h"

/// 初始化随机移动：接管玩家控制或令 AI 脱靶停攻，并中断既有样条
void AbstractRandomMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(i_stateActive);

    m_pathFinder = std::make_unique<PathFinder>(&owner);

    // Client-controlled unit should have control removed (客户端控制单位需临时取消本地操控)
    if (const Player* controllingClientPlayer = owner.GetClientControlling())
        controllingClientPlayer->UpdateClientControl(&owner, false);
    // Non-client controlled unit with an AI should drop target (AI 单位取消目标并停止近战攻击)
    else if (owner.AI())
    {
        owner.SetTarget(nullptr);
        owner.MeleeAttackStop(owner.GetVictim());
    }

    // Stop any previously dispatched splines no matter the source (停止之前残留的移动样条)
    if (!owner.movespline->Finalized())
    {
        if (owner.IsClientControlled())
            owner.StopMoving(true);
        else
            owner.InterruptMoving();
    }
}

/// 结束随机移动：清除状态、归还客户端控制权并停止样条
void AbstractRandomMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(i_stateActive | i_stateMotion);

    // Client-controlled unit should have control restored (归还客户端控制权)
    if (const Player* controllingClientPlayer = owner.GetClientControlling())
        controllingClientPlayer->UpdateClientControl(&owner, true);

    // Stop any previously dispatched splines no matter the source
    if (!owner.movespline->Finalized())
    {
        if (owner.IsClientControlled())
            owner.StopMoving(true);
        else
            owner.InterruptMoving();
    }
}

void AbstractRandomMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();

    owner.clearUnitState(i_stateMotion);
}

void AbstractRandomMovementGenerator::Reset(Unit& owner)
{
    i_nextMoveTimer.Reset(0);

    Initialize(owner);
}

/// 每帧更新：当当前段样条走完且等待计时器到期后，选取下一随机目标点继续移动
bool AbstractRandomMovementGenerator::Update(Unit& owner, const uint32& diff)
{
    if (!owner.IsAlive())
        return false;

    if (owner.hasUnitState(UNIT_STAT_NO_FREE_MOVE & ~i_stateActive))
    {
        i_nextMoveTimer.Update(diff);
        owner.clearUnitState(i_stateMotion);
        return true;
    }

    if (owner.movespline->Finalized())
    {
        i_nextMoveTimer.Update(diff);

        if (i_nextMoveTimer.Passed())
        {
            if (_setLocation(owner))
            {
                if (i_nextMoveCount > 1)
                    --i_nextMoveCount;
                else
                {
                    i_nextMoveCount = urand(1, i_nextMoveCountMax);
                    i_nextMoveTimer.Reset(urand(i_nextMoveDelayMin, i_nextMoveDelayMax));
                }
            }
            else
                i_nextMoveTimer.Reset(owner.HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED) ? 100 : 500);
        }
    }

    return true;
}

/// 通过 PathFinder 在中心周围随机取点并下发移动样条
int32 AbstractRandomMovementGenerator::_setLocation(Unit& owner)
{
    // Look for a random location within certain radius of initial position (在初始中心半径范围内寻找随机可达点)
    float x = i_x, y = i_y, z = i_z;

    if (i_pathLength != 0.0f)
        m_pathFinder->setPathLengthLimit(i_pathLength);

    m_pathFinder->ComputePathToRandomPoint(Vector3(x, y, z), i_radius);

    if ((m_pathFinder->getPathType() & PATHFIND_NOPATH) != 0)
        return 0;

    Movement::MoveSplineInit init(owner);
    init.MovebyPath(m_pathFinder->getPath());

    if (i_randomRunWander)
        init.SetWalk(urand(0, 99) >= 15);
    else
        init.SetWalk(i_walk);

    if (owner.IsSlowedInCombat())
        init.SetCombatSlowed(1.f - ((30.f - std::min(owner.GetHealthPercent(), 30.f)) * 1.67) / 100);

    int32 duration = init.Launch();

    if (duration)
        owner.addUnitState(i_stateMotion);

    return duration;
}

ConfusedMovementGenerator::ConfusedMovementGenerator(float x, float y, float z) :
    AbstractRandomMovementGenerator(UNIT_STAT_CONFUSED, UNIT_STAT_CONFUSED_MOVE, 500, 1500)
{
    i_radius = 2.5f;
    i_x = x;
    i_y = y;
    i_z = z;
}

ConfusedMovementGenerator::ConfusedMovementGenerator(const Unit& owner) :
    ConfusedMovementGenerator(owner.GetPositionX(), owner.GetPositionY(), owner.GetPositionZ())
{
}

WanderMovementGenerator::WanderMovementGenerator(Unit const& unit, float x, float y, float z, float radius, float verticalZ, bool walk) :
    AbstractRandomMovementGenerator(UNIT_STAT_ROAMING, UNIT_STAT_ROAMING_MOVE, 3000, 10000, 3, walk)
{
    i_x = x;
    i_y = y;
    i_z = z;
    i_radius = radius;
    i_verticalZ = verticalZ;
    if (unit.IsCreature())
        i_randomRunWander = static_cast<Creature const&>(unit).GetCreatureInfo()->HasFlag(CREATURE_EXTRA_FLAG_RUN_DURING_WANDER);
}

WanderMovementGenerator::WanderMovementGenerator(const Creature& npc) :
    AbstractRandomMovementGenerator(UNIT_STAT_ROAMING, UNIT_STAT_ROAMING_MOVE, 3000, 10000, 3)
{
    i_randomRunWander = npc.GetCreatureInfo()->HasFlag(CREATURE_EXTRA_FLAG_RUN_DURING_WANDER);
    npc.GetRespawnCoord(i_x, i_y, i_z, nullptr, &i_radius);
}

void WanderMovementGenerator::Finalize(Unit& owner)
{
    AbstractRandomMovementGenerator::Finalize(owner);

    if (owner.GetTypeId() == TYPEID_UNIT)
        static_cast<Creature&>(owner).SetWalk(!owner.hasUnitState(UNIT_STAT_RUNNING_STATE), false);
}

void WanderMovementGenerator::Interrupt(Unit& owner)
{
    AbstractRandomMovementGenerator::Interrupt(owner);

    if (owner.GetTypeId() == TYPEID_UNIT)
        static_cast<Creature&>(owner).SetWalk(!owner.hasUnitState(UNIT_STAT_RUNNING_STATE), false);
}

void WanderMovementGenerator::AddToRandomPauseTime(int32 waitTimeDiff, bool force)
{
    if (force)
        i_nextMoveTimer.Reset(waitTimeDiff);
    else if (!i_nextMoveTimer.Passed())
    {
        // creature is stopped already
        // Prevent <= 0, the code in Update requires to catch the change from moving to not moving
        int32 newWaitTime = i_nextMoveTimer.GetExpiry() + waitTimeDiff;
        i_nextMoveTimer.Reset(newWaitTime > 0 ? newWaitTime : 1);
    }
}

TimedWanderMovementGenerator::TimedWanderMovementGenerator(Unit const& npc, uint32 timer, float radius, float verticalZ)
    : TimedWanderMovementGenerator(npc, timer, npc.GetPositionX(), npc.GetPositionY(), npc.GetPositionZ(), radius, verticalZ)
{

}

TimedWanderMovementGenerator::TimedWanderMovementGenerator(Unit const& unit, uint32 timer, float x, float y, float z, float radius, float verticalZ, bool walk)
    : WanderMovementGenerator(unit, x, y, z, radius, verticalZ, walk), m_durationTimer(timer)
{

}

bool TimedWanderMovementGenerator::Update(Unit& owner, const uint32& diff)
{
    m_durationTimer.Update(diff);
    if (m_durationTimer.Passed())
        return false;

    return WanderMovementGenerator::Update(owner, diff);
}

FleeingMovementGenerator::FleeingMovementGenerator(Unit const& source) :
    AbstractRandomMovementGenerator(UNIT_STAT_FLEEING, UNIT_STAT_FLEEING_MOVE, 500, 1500)
{
    source.GetPosition(i_x, i_y, i_z);
    i_pathLength = 30;
    i_walk = false;
}

#define MIN_QUIET_DISTANCE 28.0f // 逃跑安全距离下限
#define MAX_QUIET_DISTANCE 43.0f // 逃跑安全距离上限

/// 根据与威胁源的距离动态决定逃跑选点半径
int32 FleeingMovementGenerator::_setLocation(Unit& owner)
{
    float dist_from_source = owner.GetDistance(i_x, i_y, i_z);

    if (dist_from_source < MIN_QUIET_DISTANCE)
        i_radius = frand(0.4f, 1.3f) * (MIN_QUIET_DISTANCE - dist_from_source);
    else if (dist_from_source > MAX_QUIET_DISTANCE)
        i_radius = frand(0.4f, 1.0f) * (MAX_QUIET_DISTANCE - MIN_QUIET_DISTANCE);
    else    // we are inside quiet range (已处于安全区间内，在附近小幅游走避险)
        i_radius = frand(0.6f, 1.2f) * (MAX_QUIET_DISTANCE - MIN_QUIET_DISTANCE);

    return AbstractRandomMovementGenerator::_setLocation(owner);
}

void PanicMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_PANIC);

    FleeingMovementGenerator::Initialize(owner);
}

void PanicMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_PANIC);

    // Since two fleeing mmgens are mutually exclusive, we are also responsible for the removal of that flag, nobody will clear this for us
    // 两种逃跑生成器互斥，需由恐慌生成器自行移除 UNIT_FLAG_FLEEING 并通知 AI 结束
    owner.RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_FLEEING);

    FleeingMovementGenerator::Finalize(owner);

    if (owner.AI())
        owner.AI()->TimedFleeingEnded();
}

void PanicMovementGenerator::Interrupt(Unit& owner)
{
    FleeingMovementGenerator::Interrupt(owner);

    if (owner.AI())
        owner.AI()->TimedFleeingEnded();
}

bool PanicMovementGenerator::Update(Unit& owner, const uint32& diff)
{
    m_fleeingTimer.Update(diff);
    if (m_fleeingTimer.Passed())
        return false;

    return FleeingMovementGenerator::Update(owner, diff);
}
