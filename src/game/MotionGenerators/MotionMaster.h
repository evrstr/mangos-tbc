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

#ifndef MANGOS_MOTIONMASTER_H
#define MANGOS_MOTIONMASTER_H

#include "Common.h"
#include "Globals/SharedDefines.h"
#include "MotionGenerators/WaypointManager.h"
#include "Entities/ObjectGuid.h"
#include "Maps/SpawnGroupDefines.h"

#include <stack>
#include <vector>

class MovementGenerator;
class Unit;
struct Position;

namespace G3D
{
    class Vector2;
    class Vector3;
    class Vector4;
}

// Creature Entry ID used for waypoints show, visible only for GMs (GM 调试查看路点时生成的视觉标记生物 Entry)
#define VISUAL_WAYPOINT 1

#define PET_FOLLOW_DIST  1.0f           // 宠物默认跟随距离
#define PET_FOLLOW_ANGLE (M_PI_F / 2.0f) // 宠物默认跟随角度（侧方 90 度）

// values 0 ... MAX_DB_MOTION_TYPE-1 used in DB (0 ~ MAX_DB_MOTION_TYPE-1 可配置于数据库默认移动类型)
enum MovementGeneratorType
{
    IDLE_MOTION_TYPE                = 0,                    // IdleMovementGenerator.h (原地待机)
    RANDOM_MOTION_TYPE              = 1,                    // RandomMovementGenerator.h (随机漫步)
    WAYPOINT_MOTION_TYPE            = 2,                    // WaypointMovementGenerator.h (循环路点巡逻)
    PATH_MOTION_TYPE                = 3,                    // PathMovementGenerator.h (自定义平滑路径)
    LINEAR_WP_MOTION_TYPE           = 4,                    // WaypointMovementGenerator.h (线性路点移动)

    MAX_DB_MOTION_TYPE              = 5,                    // *** this and below motion types can't be set in DB. (此及以下类型不可在数据库配置)

    DISTRACT_MOTION_TYPE            = 5,                    // IdleMovementGenerator.h (被扰乱原地朝向)
    STAY_MOTION_TYPE                = 6,                    // PointMovementGenerator.h (驻守指定坐标)
    FOLLOW_MOTION_TYPE              = 7,                    // TargetedMovementGenerator.h (跟随目标)
    CHASE_MOTION_TYPE               = 8,                    // TargetedMovementGenerator.h (追击目标)
    RETREAT_MOTION_TYPE             = 9,                    // PointMovementGenerator.h (撤退至指定点)
    TIMED_FLEEING_MOTION_TYPE       = 10,                   // RandomMovementGenerator.h (限时逃跑)
    POINT_MOTION_TYPE               = 11,                   // PointMovementGenerator.h (点对点移动)
    HOME_MOTION_TYPE                = 12,                   // HomeMovementGenerator.h (脱战返巢)
    FLEEING_MOTION_TYPE             = 13,                   // RandomMovementGenerator.h (恐惧/求援逃跑)
    CONFUSED_MOTION_TYPE            = 14,                   // RandomMovementGenerator.h (迷惑乱走)
    EFFECT_MOTION_TYPE              = 15,                   // WrapperMovementGenerator.h (击退/冲锋/跳跃等法术效果位移)
    TAXI_MOTION_TYPE                = 16,                   // WaypointMovementGenerator.h (飞行坐骑航线)
    TIMED_RANDOM_MOTION_TYPE        = 17,                   // RandomMovementGenerator.h (限时随机漫步)
    FALL_MOTION_TYPE                = 18,                   // WrapperMovementGenerator.h (空中坠落)

    EXTERNAL_WAYPOINT_MOVE          = 18,                   // Only used in UnitAI::MovementInform when a waypoint is reached. The pathId >= 0 is added as additonal value (仅用于 AI 到达路点回调通知)
    EXTERNAL_WAYPOINT_MOVE_START    = 19,                   // Only used in UnitAI::MovementInform when a waypoint is started. The pathId >= 0 is added as additional value (仅用于 AI 开始前往路点回调通知)
    EXTERNAL_WAYPOINT_FINISHED_LAST = 20,                   // Only used in UnitAI::MovementInform when the waittime of the last wp is finished The pathId >= 0 is added as additional value (仅用于 AI 末路点等待结束回调通知)

    FORMATION_MOTION_TYPE           = 21,                   // TargetedMovementGenerator.h (编队跟随)
    MOVEMENT_TYPE_MAX
};

/// 清理/过期操作执行模式标志
enum MMCleanFlag
{
    MMCF_NONE   = 0,
    MMCF_UPDATE = 1,                                        // Clear or Expire called from update (在 Update 迭代中触发，需延迟销毁防止迭代器失效)
    MMCF_RESET  = 2                                         // Flag if need top()->Reset() (清理后是否需要重置恢复栈顶生成器)
};

/// 强制移动模式覆盖选项
enum ForcedMovement
{
    FORCED_MOVEMENT_NONE    = 0, // 不强制，沿用单位当前状态
    FORCED_MOVEMENT_WALK    = 1, // 强制步行
    FORCED_MOVEMENT_RUN     = 2, // 强制奔跑
    FORCED_MOVEMENT_FLIGHT  = 3, // 强制飞行
};

/// 单位运动控制器：基于栈管理各类 MovementGenerator，支持延迟清栈与高优先级压栈抢占
class MotionMaster : private std::stack<MovementGenerator*>
{
    private:
        typedef std::stack<MovementGenerator*> Impl;
        typedef std::vector<MovementGenerator*> ExpireList;


    public:
        explicit MotionMaster(Unit* unit) : m_owner(unit), m_expList(nullptr), m_cleanFlag(MMCF_NONE), m_defaultPathId(0), m_currentPathId(0) {}
        ~MotionMaster();

        void Initialize();

        MovementGenerator const* GetCurrent() const { return top(); }

        using Impl::top;
        using Impl::empty;


        typedef Impl::container_type::const_iterator const_iterator;
        const_iterator begin() const { return Impl::c.begin(); }
        const_iterator end() const { return Impl::c.end(); }

        void UpdateMotion(uint32 diff);
        /// 清空非栈底的移动生成器（all=true 时连同底层默认生成器一并清除）；Update 内调用自动延迟执行
        void Clear(bool reset = true, bool all = false)
        {
            if (m_cleanFlag & MMCF_UPDATE)
                DelayedClean(reset, all);
            else
                DirectClean(reset, all);
        }
        /// 使当前栈顶移动生成器过期退出，并恢复下层生成器
        void MovementExpired(bool reset = true)
        {
            if (m_cleanFlag & MMCF_UPDATE)
                DelayedExpire(reset);
            else
                DirectExpire(reset);
        }

        // 各类高级移动指令接口
        void MoveIdle();
        void MoveRandomAroundPoint(float x, float y, float z, float radius, float verticalZ = 0.0f, uint32 timer = 0, bool walk = true);
        void MoveTargetedHome(bool runHome = true);
        void MoveFollow(Unit* target, float dist, float angle, bool asMain = false, bool alwaysBoost = false);
        void MoveInFormation(FormationSlotDataSPtr& sData, bool asMain = false);
        void MoveStay(float x, float y, float z, float o = 0, bool asMain = false);
        void MoveChase(Unit* target, float dist = 0.0f, float angle = 0.0f, bool moveFurther = false, bool walk = false, bool combat = true, bool delayed = false);
        void DistanceYourself(float dist);
        void MoveConfused();
        void MoveFleeing(Unit* enemy, uint32 time = 0);
        void MovePoint(uint32 id, Position const& position, ForcedMovement forcedMovement = FORCED_MOVEMENT_NONE, float speed = 0.f, bool generatePath = true, ObjectGuid guid = ObjectGuid(), uint32 relayId = 0);
        void MovePoint(uint32 id, float x, float y, float z, ForcedMovement forcedMovement = FORCED_MOVEMENT_NONE, bool generatePath = true);
        void MovePointTOL(uint32 id, float x, float y, float z, bool takeOff, ForcedMovement forcedMovement = FORCED_MOVEMENT_NONE);
        void MovePath(std::vector<G3D::Vector3>& path, ForcedMovement forcedMovement = FORCED_MOVEMENT_NONE, bool flying = false, bool cyclic = true);
        void MovePath(std::vector<G3D::Vector3>& path, float o, ForcedMovement forcedMovement = FORCED_MOVEMENT_NONE, bool flying = false, bool cyclic = true);
        // MovePath can not change speed or flying mid path due to how it works - if you wish to do that, split it into two paths
        // MovePath 运行期间无法中途变速或切换飞行状态，如需切换请拆分为两段路径
        void MovePath(int32 pathId = 0, WaypointPathOrigin wpOrigin = PATH_NO_PATH, ForcedMovement forcedMovement = FORCED_MOVEMENT_NONE, bool flying = false, float speed = 0.f, bool cyclic = false, ObjectGuid guid = ObjectGuid());
        void MoveRetreat(float x, float y, float z, float o, uint32 delay);
        void MoveWaypoint(uint32 pathId = 0, uint32 source = 0, uint32 initialDelay = 0, uint32 overwriteEntry = 0, ForcedMovement forcedMovement = FORCED_MOVEMENT_NONE, ObjectGuid guid = ObjectGuid());
        void MoveLinearWP(uint32 pathId = 0, uint32 source = 0, uint32 initialDelay = 0, uint32 overwriteEntry = 0, ForcedMovement forcedMovement = FORCED_MOVEMENT_NONE, ObjectGuid guid = ObjectGuid());
        void MoveTaxi();
        void MoveDistract(uint32 timer);
        void MoveCharge(float x, float y, float z, float speed, uint32 id = EVENT_CHARGE);
        void MoveCharge(Unit& target, float speed, uint32 id = EVENT_CHARGE);
        bool MoveFall(ObjectGuid guid = ObjectGuid(), uint32 relayId = 0);
        void MoveJump(float x, float y, float z, float horizontalSpeed, float max_height, uint32 id = EVENT_JUMP);
        void MoveJumpFacing(float x, float y, float z, float o, float horizontalSpeed, float max_height, uint32 id = EVENT_JUMP);

        MovementGeneratorType GetCurrentMovementGeneratorType() const;

        /// 通知当前激活的移动生成器单位速度已改变
        void propagateSpeedChange();
        bool SetNextWaypoint(uint32 pointId);

        uint32 getLastReachedWaypoint() const;
        void GetWaypointPathInformation(std::ostringstream& oss) const;
        bool GetDestination(float& x, float& y, float& z) const;

        void SetDefaultPathId(uint32 pathId) { m_defaultPathId = pathId; }
        uint32 GetPathId() const { return m_currentPathId; }

        void InterruptPanic();

        void PauseWaypoints(uint32 time);
        void UnpauseWaypoints();

        void UnMarkFollowMovegens();

    private:
        /// 内部压栈并激活新的移动生成器（外部请调用 Move* 系列接口）
        void Mutate(MovementGenerator* m);                  // use Move* functions instead
        /// 按移动类型刷新对象的更新频率计时器
        void InitializeObjectUpdateTimer(const MovementGeneratorType type);

        void DirectClean(bool reset, bool all);
        void DelayedClean(bool reset, bool all);

        void DirectExpire(bool reset);
        void DelayedExpire(bool reset);

        Unit*       m_owner;
        ExpireList* m_expList;
        uint8       m_cleanFlag;

        uint32      m_defaultPathId;
        uint32      m_currentPathId;
};
#endif
