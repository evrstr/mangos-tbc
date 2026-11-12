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

#ifndef MANGOS_RANDOMMOTIONGENERATOR_H
#define MANGOS_RANDOMMOTIONGENERATOR_H

#include "MotionGenerators/MovementGenerator.h"
#include "Entities/ObjectGuid.h"

class PathFinder;

/// 随机移动生成器基类：在指定半径内周期性随机选点并寻路移动（派生出迷惑、漫步、逃跑等模式）
class AbstractRandomMovementGenerator : public MovementGenerator
{
    public:
        explicit AbstractRandomMovementGenerator(uint32 stateActive, uint32 stateMotion, uint32 delayMin, uint32 delayMax, uint32 movesMax = 1, bool walk = true) :
            i_x(0.0f), i_y(0.0f), i_z(0.0f), i_radius(0.0f), i_verticalZ(0.0f), i_pathLength(0.0f), i_walk(walk), i_randomRunWander(false),
            i_nextMoveTimer(0), i_nextMoveCount(1), i_nextMoveCountMax(movesMax),
            i_nextMoveDelayMin(delayMin), i_nextMoveDelayMax(delayMax),
            i_stateActive(stateActive), i_stateMotion(stateMotion)
        {
        }

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;
        bool Update(Unit& owner, const uint32& diff) override;

    protected:
        /// 选取下一个随机目标点并启动样条移动，返回预计耗时（0 表示寻路失败）
        virtual int32 _setLocation(Unit& owner);

        float i_x, i_y, i_z;          // 随机活动中心坐标（逃跑模式中为威胁源坐标）
        float i_radius;               // 随机选点最大半径
        float i_verticalZ;
        float i_pathLength;           // 寻路长度上限（0 为不限制）
        bool i_walk;                  // 是否步行
        bool i_randomRunWander;       // 漫步时是否有一定概率随机奔跑

        std::unique_ptr<PathFinder> m_pathFinder;
        ShortTimeTracker i_nextMoveTimer; // 两次随机移动之间的停顿计时器
        uint32 i_nextMoveCount, i_nextMoveCountMax; // 连续移动段数及上限
        uint32 i_nextMoveDelayMin, i_nextMoveDelayMax; // 停顿时间随机范围 [min, max]
        uint32 i_stateActive, i_stateMotion; // 激活状态位与正在移动状态位
};

/// 迷惑移动生成器（CONFUSED_MOTION_TYPE）：在原地小半径内频繁无序游走
class ConfusedMovementGenerator : public AbstractRandomMovementGenerator
{
    public:
        explicit ConfusedMovementGenerator(Unit const& owner);
        ConfusedMovementGenerator(float x, float y, float z);

        MovementGeneratorType GetMovementGeneratorType() const override { return CONFUSED_MOTION_TYPE; }
};

/// 随机漫步移动生成器（RANDOM_MOTION_TYPE）：NPC 在刷新点或指定中心周围自由巡游
class WanderMovementGenerator : public AbstractRandomMovementGenerator
{
    public:
        explicit WanderMovementGenerator(Creature const& npc);
        WanderMovementGenerator(Unit const& unit, float x, float y, float z, float radius, float verticalZ = 0.0f, bool walk = true);

        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;

        /// 调整当前漫步暂停等待时间
        void AddToRandomPauseTime(int32 waitTimeDiff, bool force);

        MovementGeneratorType GetMovementGeneratorType() const override { return RANDOM_MOTION_TYPE; }
};

/// 限时随机漫步移动生成器（TIMED_RANDOM_MOTION_TYPE）
class TimedWanderMovementGenerator : public WanderMovementGenerator
{
    public:
        explicit TimedWanderMovementGenerator(Unit const& npc, uint32 timer, float radius, float verticalZ = 0.0f);
        TimedWanderMovementGenerator(Unit const& unit, uint32 timer, float x, float y, float z, float radius, float verticalZ = 0.0f, bool walk = true);

        bool Update(Unit& owner, const uint32& diff) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return TIMED_RANDOM_MOTION_TYPE; }

    private:
        ShortTimeTracker m_durationTimer;
};

/// 逃跑/恐惧移动生成器（FLEEING_MOTION_TYPE）：以危险源为中心向外围安全距离逃窜
class FleeingMovementGenerator : public AbstractRandomMovementGenerator
{
    public:
        explicit FleeingMovementGenerator(Unit const& source);

        virtual int32 _setLocation(Unit& owner) override;
        MovementGeneratorType GetMovementGeneratorType() const override { return FLEEING_MOTION_TYPE; }
};

/// 限时恐慌逃跑移动生成器（TIMED_FLEEING_MOTION_TYPE）：用于怪物低血量求援或限时恐惧效果
class PanicMovementGenerator : public FleeingMovementGenerator
{
    public:
        PanicMovementGenerator(Unit const& source, uint32 time) : FleeingMovementGenerator(source), m_fleeingTimer(time) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        bool Update(Unit& owner, const uint32& diff) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return TIMED_FLEEING_MOTION_TYPE; }

    private:
        ShortTimeTracker m_fleeingTimer;
};

#endif
