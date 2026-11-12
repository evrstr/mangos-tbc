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

#ifndef MANGOS_TARGETEDMOVEMENTGENERATOR_H
#define MANGOS_TARGETEDMOVEMENTGENERATOR_H

#include "Movement/MoveSplineInit.h"
#include "MotionGenerators/MovementGenerator.h"
#include "MotionGenerators/FollowerReference.h"
#include "Entities/ObjectGuid.h"
#include "Entities/Object.h"

class PathFinder;

/// 目标跟踪移动生成器基类：持有对目标 Unit 的 FollowerReference 弱引用
class TargetedMovementGeneratorBase
{
    public:
        TargetedMovementGeneratorBase(Unit& target) { i_target.link(&target, this); }
        void stopFollowing() { }
        void SetNewTarget(Unit& target) { i_target.link(&target, this); }
    protected:
        FollowerReference i_target; // 关联目标单位的引用节点
};

/// 目标移动生成器通用中间模板（由 Chase / Follow / Formation 继承）：封装重算检测、距离判断与路径器
template<class T, typename D>
class TargetedMovementGeneratorMedium
    : public MovementGeneratorMedium< T, D >, public TargetedMovementGeneratorBase
{
    protected:
        TargetedMovementGeneratorMedium(Unit& target, float offset, float angle) :
            TargetedMovementGeneratorBase(target),
            i_recheckDistance(0),
            i_offset(offset), i_angle(angle),
            i_speedChanged(false), i_targetReached(false),
            i_faceTarget(true), i_path(nullptr)
        {
        }
        ~TargetedMovementGeneratorMedium() { delete i_path; }

    public:
        bool Update(T&, const uint32&) override;

        virtual bool IsReachable() const override;

        Unit* GetCurrentTarget() const override { return i_target.getTarget(); }
        float GetOffset() const { return i_offset; }
        float GetAngle() const { return i_angle; }

        virtual void UnitSpeedChanged() override { i_speedChanged = true; }

        virtual bool RemoveOnInvalid() const { return true; };

    protected:
        /// 判断目标是否移动或距离变化到需要重新规划位置
        virtual bool RequiresNewPosition(T& owner, Position pos) const;
        /// 获取当前动态目标距离（如含包络半径、近战追击偏移等）
        virtual float GetDynamicTargetDistance(T& /*owner*/, bool /*forRangeCheck*/) const { return i_offset; }
        /// 到达后是否需要自动转向面对目标
        virtual bool ShouldFaceTarget() const { return i_faceTarget; }
        virtual void HandleTargetedMovement(T& owner, const uint32& time_diff) = 0;
        virtual void HandleFinalizedMovement(T& owner) = 0;
        virtual void HandleMovementFailure(T& owner) = 0;

        virtual bool _hasUnitStateNotMove(Unit& owner) = 0;
        virtual void _clearUnitStateMove(Unit& owner) = 0;
        virtual void _addUnitStateMove(Unit& owner) = 0;

        ShortTimeTracker i_recheckDistance; // 周期性距离重检计时器
        float i_offset;                     // 与目标的期望距离偏移
        float i_angle;                      // 相对目标的期望角度偏移
        G3D::Vector3 i_lastTargetPos;       // 上次计算路径时目标所处坐标
        bool i_speedChanged : 1;
        bool i_targetReached : 1;
        bool i_faceTarget : 1;

        PathFinder* i_path;                 // 寻路器实例（按需延迟创建）
};

/*
Chases after target
Has these custom features:
Backpedaling - when player is standing inside model, mob backs away with 2 second delay, likely for smoothness
Fanning - likewise after 2 seconds, all mobs spread out at a given target - has to be coordinated with all targets which attack same victim
TODO: Implement variable chase speed for pet chase
目标追击生成器：
- 退步（Backpedaling）：玩家站进怪物模型内部时，怪物在约 2 秒延迟后小幅后退以保持攻击距离与视觉平滑
- 扇形散开（Fanning）：多个怪物攻击同一目标时，经 2 秒后按占用扇区散开站位，避免完全重叠
*/
enum ChaseMovementMode
{
    CHASE_MODE_NORMAL, // chasing target (正常追击目标)
    CHASE_MODE_BACKPEDAL, // collision movement (模型重叠后退避让)
    CHASE_MODE_DISTANCING, // running away from melee (远程拉距后撤)
    CHASE_MODE_FANNING, // mob collision movement (多怪围绕目标扇形散开)
};

extern const char* ChaseModes[];

/// 追击移动生成器（CHASE_MOTION_TYPE）：用于战斗追击、后退避让、拉距与多怪散开
class ChaseMovementGenerator : public TargetedMovementGeneratorMedium<Unit, ChaseMovementGenerator >
{
    using TargetedMovementGeneratorMedium<Unit, ChaseMovementGenerator>::i_offset;
    using TargetedMovementGeneratorMedium<Unit, ChaseMovementGenerator>::i_angle;
    public:
        ChaseMovementGenerator(Unit& target, float offset, float angle, bool moveFurther = true, bool walk = false, bool combat = true)
            : TargetedMovementGeneratorMedium<Unit, ChaseMovementGenerator >(target, offset, angle),
            m_moveFurther(moveFurther), m_walk(walk), m_combat(combat), m_reachable(true),
            m_fanningEnabled(true), m_backpedalEnabled(true), m_closenessAndFanningTimer(0), m_closenessExpired(false), m_currentMode(CHASE_MODE_NORMAL) {}
        ~ChaseMovementGenerator() {}

        MovementGeneratorType GetMovementGeneratorType() const override { return CHASE_MOTION_TYPE; }

        void Initialize(Unit&) override;
        void Finalize(Unit&) override;
        void Interrupt(Unit&) override;
        void Reset(Unit&) override;
        void SetOffsetAndAngle(float offset, float angle, bool moveFurther);
        void DistanceYourself(Unit& owner, float distance);
        void FanOut(Unit& owner);

        bool EnableWalking() const { return m_walk;}
        bool _lostTarget(Unit& u) const;
        bool RemoveOnInvalid() const override { return false; }
        void _reachTarget(Unit&);
        bool GetResetPosition(Unit& /*u*/, Position& /*pos*/) const override { return false; }
        void HandleMovementFailure(Unit& owner) override;

        ChaseMovementMode GetCurrentMode() const { return m_currentMode; }
        virtual bool IsReachable() const override;

        virtual bool IsRemovedOnExpire() const override { return true; }

        std::pair<std::string, std::string> GetPrintout() const;

        void AddDelay();

    protected:
        float GetDynamicTargetDistance(Unit& owner, bool forRangeCheck) const override;
        void HandleTargetedMovement(Unit& owner, const uint32& time_diff) override;
        void HandleFinalizedMovement(Unit& owner) override;
        bool RequiresNewPosition(Unit& owner, Position pos) const override;

        bool _hasUnitStateNotMove(Unit& u) override;
        void _clearUnitStateMove(Unit& u) override;
        void _addUnitStateMove(Unit& u) override;

    private:
        virtual bool _getLocation(Unit& owner, float& x, float& y, float& z) const;
        virtual void _setLocation(Unit& owner);

        bool IsReachablePositionToTarget(Unit& owner, float x, float y, float z, Unit& target);

        bool DispatchSplineToPosition(Unit& owner, float x, float y, float z, bool walk, bool cutPath, bool target = false, bool checkReachable = false);
        void CutPath(Unit& owner, PointsArray& path);
        void Backpedal(Unit& owner);

        bool m_moveFurther;
        bool m_walk;
        bool m_combat;
        bool m_reachable;
        bool m_fanningEnabled;
        bool m_backpedalEnabled;

        uint32 m_closenessAndFanningTimer;
        bool m_closenessExpired;

        ChaseMovementMode m_currentMode;

        GuidVector m_spawns;
};

/// 跟随移动生成器（FOLLOW_MOTION_TYPE）：用于宠物、护送 NPC 等按距离与夹角持续跟随领队
class FollowMovementGenerator : public TargetedMovementGeneratorMedium<Unit, FollowMovementGenerator>
{
    public:
        FollowMovementGenerator(Unit& target, float offset, float angle, bool main, bool possess, bool alwaysBoost = false)
            : TargetedMovementGeneratorMedium<Unit, FollowMovementGenerator>(target, offset, angle),
              m_main(main), m_possess(possess), m_boost(alwaysBoost), m_targetMoving(false), m_targetFaced(false)
        {
            i_faceTarget = (angle == 0.0f);
        }
        ~FollowMovementGenerator() {}

        MovementGeneratorType GetMovementGeneratorType() const override { return FOLLOW_MOTION_TYPE; }

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        virtual void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        bool GetResetPosition(Unit& owner, Position& pos) const override;

        virtual bool EnableWalking() const;

        bool _lostTarget(Unit& owner) const;
        void _reachTarget(Unit& owner);

        void HandleMovementFailure(Unit& owner) override;

        virtual bool IsRemovedOnExpire() const override { return !m_main; }

        void MarkMovegen();

    protected:
        float GetDynamicTargetDistance(Unit& owner, bool forRangeCheck) const override;
        void HandleTargetedMovement(Unit& owner, const uint32& time_diff) override;
        void HandleFinalizedMovement(Unit& owner) override;

        bool _hasUnitStateNotMove(Unit& owner) override;
        void _clearUnitStateMove(Unit& owner) override;
        void _addUnitStateMove(Unit& owner) override;

        virtual float GetSpeed(Unit& owner) const;

        virtual bool IsBoostAllowed(Unit& owner) const;
        virtual bool IsUnstuckAllowed(Unit& owner) const;

        virtual bool Move(Unit& owner, float x, float y, float z);

    protected:
        virtual bool _getOrientation(Unit& owner, float& o) const;
        virtual bool _getLocation(Unit& owner, float& x, float& y, float& z, bool movingNow) const;
        virtual void _setOrientation(Unit& owner);
        virtual void _setLocation(Unit& owner, bool movingNow);

        static inline uint32 _getPollRateBase() { return 400; }
        static inline uint32 _getPollRateMax() { return 800; }
        virtual uint32 _getPollRateMultiplier(Unit& owner, bool targetMovingNow, bool targetMovedBefore = true) const;
        virtual uint32 _getPollRate(Unit& owner, bool movingNow, bool movingBefore = true) const;

        bool m_main;
        bool m_possess;
        bool m_boost;

        bool m_targetMoving;
        bool m_targetFaced;
};

// to be able to compute new path before the end of the current path (in milliseconds)
// 编队移动提前预判重算路径的时间窗口（毫秒）
#define FORMATION_FOLLOWERS_CHECK_OVERRIDE 400

/// 编队移动生成器（FORMATION_MOTION_TYPE）：继承跟随生成器，按编队槽位与预测位置平滑同步移动
class FormationMovementGenerator : public FollowMovementGenerator
{
    public:
        FormationMovementGenerator(FormationSlotDataSPtr& sData, bool main);
        ~FormationMovementGenerator();

        MovementGeneratorType GetMovementGeneratorType() const override { return FORMATION_MOTION_TYPE; }

        bool Update(Unit&, const uint32&) override;
        void Interrupt(Unit& owner) override;
        bool GetResetPosition(Unit&, Position& pos) const override;

    protected:
        void HandleTargetedMovement(Unit& owner, const uint32& time_diff) override;
        void HandleFinalizedMovement(Unit& owner) override;

    private:
        virtual void _setLocation(Unit& owner, bool catchup) override;
        float BuildPath(Unit& owner, PointsArray& path);
        bool HandleMasterDistanceCheck(Unit& owner, const uint32& time_diff);
        FormationSlotDataSPtr m_slot;

        // used to smooth angle change
        float m_lastAngle;

        // running to master
        bool m_headingToMaster;

        // distance from where the follower will be teleported to master
        float m_tpDistance;

        // distance from where the follower will run to master.
        float m_moveToMasterDistance;

        // reset position if this is interrupted
        Position m_resetPoint;
};

#endif
