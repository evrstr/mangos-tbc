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

#ifndef MANGOSSERVER_MOVESPLINEINIT_H
#define MANGOSSERVER_MOVESPLINEINIT_H

#include "Entities/Object.h"
#include "Movement/MoveSplineInitArgs.h"
#include "MotionGenerators/PathFinder.h"
#include "MotionGenerators/MotionMaster.h"

class Unit;
class WorldObject;

namespace Movement
{
    /*  Initializes and launches spline movement
     *  初始化并启动单位的样条移动（构建路径、设置标志并广播 SMSG_MONSTER_MOVE）
     */
    class MoveSplineInit
    {
        public:

            explicit MoveSplineInit(Unit& m);

            /* Final pass of initialization that launches spline movement.
             * @return duration - estimated travel time
             * 完成最终初始化并启动样条移动，返回预计移动总耗时（毫秒）
             */
            int32 Launch();

            /* Stop any creature movement */
            /// 停止生物当前的样条移动并通知周围客户端
            void Stop(bool forceSend = false);

            /* Adds final facing animation
             * sets unit's facing to specified point/angle after all path done
             * you can have only one final facing: previous will be overriden
             * 添加终点朝向动作：路径走完后使单位朝向指定角度/坐标/目标（会覆盖之前的设置）
             */
            void SetFacing(float angle);
            void SetFacing(Vector3 const& spot);
            void SetFacing(const WorldObject* target);

            /* Initializes movement by path
             * @param path - array of points, shouldn't be empty
             * @param pointId - Id of fisrt point of the path. Example: when third path point will be done it will notify that pointId + 3 done
             * 按给定路径点数组初始化移动；path_offset 为首点索引偏移，用于分段路点完成通知
             */
            void MovebyPath(const PointsArray& controls, int32 path_offset = 0);

            /* Initializes simple A to B mition, A is current unit's position, B is destination
             * 初始化简单的 A 到 B 直线/寻路移动（A 为单位当前位置，B 为目标点）
             */
            void MoveTo(const Vector3& dest, bool generatePath = false, bool forceDestination = false);
            void MoveTo(float x, float y, float z, bool generatePath = false, bool forceDestination = false);

            /* Sets Id of fisrt point of the path. When N-th path point will be done ILisener will notify that pointId + N done
             * Needed for waypoint movement where path splitten into parts
             * 设置路径首点 ID（分段路径到达第 N 个点时通知 pointId + N）
             */
            void SetFirstPointId(int32 pointId) { args.path_Idx_offset = pointId; }

            /* Enables CatmullRom spline interpolation mode, enables flying animation. Disabled by default
             * 启用 Catmull-Rom 平滑样条插值与飞行动画模式（默认关闭）
             */
            void SetFly();
            /* Enables walk mode. Disabled by default
             * 启用步行模式（默认关闭，即默认为奔跑）
             */
            void SetWalk(bool enable);
            /* Makes movement cyclic. Disabled by default
             * 启用循环移动模式（默认关闭）
             */
            void SetCyclic();
            /* Enables falling mode. Disabled by default
             * 启用坠落模式（默认关闭）
             */
            void SetFall();

            /* Sets the zspeed (in case you want to have custom movement zspeed)
             * if no set, speed will be selected based on unit's speeds and current movement mode
             * Has no effect if falling mode enabled
             * zspeed shouldn't be negative
             * 设置自定义移动速度；未设置时按单位当前移动模式自动选取速度（坠落模式下无效，不可为负）
             */
            void SetVelocity(float vel);

            PointsArray& Path() { return args.path; }

            /// 设置战斗减速比例（用于追击/移动速度缩放）
            void SetCombatSlowed(float slowed) { args.slowed = slowed; }
            /// 校验路径点坐标偏移是否在协议允许范围内
            bool CheckBounds() const { return args._checkPathBounds(); }
        protected:

            MoveSplineInitArgs args;
            Unit&  unit;
    };

    inline void MoveSplineInit::SetFly() { args.flags.flying = true;}
    inline void MoveSplineInit::SetWalk(bool enable) { args.flags.runmode = !enable;}
    inline void MoveSplineInit::SetCyclic() { args.flags.cyclic = true;}
    inline void MoveSplineInit::SetFall() { args.flags.falling = true;}
    inline void MoveSplineInit::SetVelocity(float vel) { args.velocity = vel;}

    inline void MoveSplineInit::MovebyPath(const PointsArray& controls, int32 path_offset)
    {
        args.path_Idx_offset = path_offset;
        args.path.assign(controls.begin(), controls.end());
    }

    inline void MoveSplineInit::MoveTo(float x, float y, float z, bool generatePath, bool forceDestination)
    {
        Vector3 v(x, y, z);
        MoveTo(v, generatePath, forceDestination);
    }

    inline void MoveSplineInit::MoveTo(const Vector3& dest, bool generatePath, bool forceDestination)
    {
        if (generatePath)
        {
            PathFinder path(&unit);
            path.calculate(dest.x, dest.y, dest.z, forceDestination);
            MovebyPath(path.getPath());
        }
        else
        {
            args.path_Idx_offset = 0;
            args.path.resize(2);
            args.path[1] = dest;
        }
    }

    inline void MoveSplineInit::SetFacing(Vector3 const& spot)
    {
        args.facing.f.x = spot.x;
        args.facing.f.y = spot.y;
        args.facing.f.z = spot.z;
        args.flags.EnableFacingPoint();
    }
}
#endif // MANGOSSERVER_MOVESPLINEINIT_H
