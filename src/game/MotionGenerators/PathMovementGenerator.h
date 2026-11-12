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

#ifndef MANGOS_PATHMOVEMENTGENERATOR_H
#define MANGOS_PATHMOVEMENTGENERATOR_H

#include "MotionGenerators/MovementGenerator.h"
#include "MotionGenerators/WaypointManager.h"
#include "Movement/MoveSplineInit.h"
#include "Entities/ObjectGuid.h"

#include <vector>

/// 抽象路径移动生成器基类：按分段样条顺序执行 WaypointPath（支持单程分段等待与无等待循环路径）
class AbstractPathMovementGenerator : public MovementGenerator
{
    public:
        explicit AbstractPathMovementGenerator(const Movement::PointsArray& path, std::optional<float> orientation, int32 offset = 0, bool cyclic = true);
        explicit AbstractPathMovementGenerator(const WaypointPath* path, int32 offset = 0, bool cyclic = false, ObjectGuid guid = ObjectGuid());

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;
        bool Update(Unit& owner, const uint32&/* diff*/) override;

        void UnitSpeedChanged() override { m_speedChanged = true; }

    protected:
        /// 到达某路点时触发节点脚本与 AI MovementInform 通知
        virtual void MovementInform(Unit& unit);

    protected:
        WaypointPath m_path;                 // 完整路点表（本地拷贝，避免运行时 WaypointManager 改写导致悬垂指针）
        int32 m_pathIndex;                   // 当前待到达的路点索引
        Movement::PointsArray m_spline;      // 当前分段样条控制点数组
        std::optional<float> m_orientation;  // 本段终点朝向
        ShortTimeTracker m_timer;            // 路点停留计时器
        bool m_cyclic;                       // 是否整条路径闭环循环
        bool m_firstCycle;                   // 是否仍处于首个循环周期
        uint32 m_startPoint;                 // 首轮入环起始路点索引
        ObjectGuid m_guid;                   // 关联脚本触发目标对象 GUID

    private:
        bool m_speedChanged;
};

/// 固定路径移动生成器（PATH_MOTION_TYPE）：沿预设点列或数据库路径进行平滑/线性分段移动
class FixedPathMovementGenerator : public AbstractPathMovementGenerator
{
    public:
        FixedPathMovementGenerator(const Movement::PointsArray& path, std::optional<float> orientation, uint32 forcedMovement, bool flying = false, float speed = 0, int32 offset = 0, bool cyclic = true) :
            AbstractPathMovementGenerator(path, orientation, offset, cyclic), m_flying(flying), m_speed(speed), m_forcedMovement(forcedMovement) {}
        FixedPathMovementGenerator(const Movement::PointsArray& path, uint32 forcedMovement, bool flying = false, float speed = 0, int32 offset = 0, bool cyclic = true) :
            FixedPathMovementGenerator(path, 0, forcedMovement, flying, speed, offset, cyclic) {}
        FixedPathMovementGenerator(Unit& unit, int32 pathId, WaypointPathOrigin wpOrigin, ForcedMovement forcedMovement, bool flying = false, float speed = 0, int32 offset = 0, bool cyclic = false, ObjectGuid guid = ObjectGuid());
        FixedPathMovementGenerator(Creature& creature);

        void Initialize(Unit& unit) override;
        void Finalize(Unit& unit) override;
        void Interrupt(Unit& unit) override;
        void Reset(Unit& unit) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return PATH_MOTION_TYPE; }

        /// 动态调整当前路点的暂停等待时间
        void AddToPathPauseTime(int32 waitTimeDiff, bool force);

    protected:
        /// 使用 MoveSplineInit 发射当前分段样条
        virtual bool Move(Unit& unit) const;

    protected:
        bool m_flying;
        float m_speed;
        uint32 m_forcedMovement;
};

/// 飞行管理员航线移动生成器（TAXI_MOTION_TYPE）：接管客户端控制并沿玩家航线节点飞行
class TaxiMovementGenerator : public AbstractPathMovementGenerator
{
    public:
        explicit TaxiMovementGenerator() :
            AbstractPathMovementGenerator(Movement::PointsArray(), std::nullopt) {}

        void Initialize(Unit& unit) override;
        void Finalize(Unit& unit) override;
        void Interrupt(Unit& unit) override;
        bool Update(Unit& unit, const uint32& diff) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return TAXI_MOTION_TYPE; }

        /// 加载下一段飞行航线样条并继续飞行
        bool Resume(Unit& unit);

    protected:
        virtual bool Move(Unit& unit);
};

#endif
