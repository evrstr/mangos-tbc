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

#ifndef MANGOSSERVER_MOVEPLINE_H
#define MANGOSSERVER_MOVEPLINE_H

#include "spline.h"
#include "Movement/MoveSplineInitArgs.h"

namespace Movement
{
    extern float gravity; // 重力加速度常量

    /// 怪物移动封包中的终点朝向/停止类型枚举
    enum MonsterMoveType
    {
        MonsterMoveNormal = 0,        // 普通移动
        MonsterMoveStop = 1,          // 停止移动
        MonsterMoveFacingSpot = 2,    // 到达后朝向指定坐标点
        MonsterMoveFacingTarget = 3,  // 到达后朝向指定目标
        MonsterMoveFacingAngle = 4    // 到达后朝向指定角度
    };

    /// 带朝向角的三维空间位置
    struct Location : public Vector3
    {
        Location() : orientation(0) {}
        Location(float x, float y, float z, float o) : Vector3(x, y, z), orientation(o) {}
        Location(const Vector3& v) : Vector3(v), orientation(0) {}
        Location(const Vector3& v, float o) : Vector3(v), orientation(o) {}

        float orientation;
    };

    // MoveSpline represents smooth catmullrom or linear curve and point that moves belong it
    // curve can be cyclic - in this case movement will be cyclic
    // point can have vertical acceleration motion componemt(used in fall, parabolic movement)
    // MoveSpline 表示平滑 Catmull-Rom 曲线或折线路径及沿其运动的点；支持循环路径及垂直重力加速度（坠落运动）
    class MoveSpline
    {
        public:
            typedef Spline<int32> MySpline;
            /// 样条状态步进更新结果标志
            enum UpdateResult
            {
                Result_None         = 0x01, // 仍在当前路段中移动
                Result_Arrived      = 0x02, // 已抵达终点并完成样条
                Result_NextCycle    = 0x04, // 进入下一个循环周期
                Result_NextSegment  = 0x08, // 跨入下一段路点
            };
            friend class PacketBuilder;
        protected:
            MySpline        spline;           // 底层样条几何与累计时间表（长度以毫秒计）

            FacingInfo      facing;           // 终点朝向信息

            uint32          m_Id;             // 本次样条移动的唯一 ID

            MoveSplineFlag  splineflags;      // 样条标志位

            float           speed;            // 移动速度

            uint32          time_passed;      // 已流逝的移动时间（毫秒）
            int32           point_Idx;        // 当前所在样条段索引
            int32           point_Idx_offset; // 外部路点索引偏移量

            /// 根据初始化参数构建底层样条及各段时间戳
            void init_spline(const MoveSplineInitArgs& args);
        protected:

            const MySpline::ControlArray& getPath() const { return spline.getPoints();}
            /// 计算坠落状态下的当前 Z 轴高度
            void computeFallElevation(float& el) const;

            /// 推进样条状态，消耗 ms_time_diff 并返回段切换/到达事件
            UpdateResult _updateState(uint32& ms_time_diff);
            uint32 next_timestamp() const { return spline.length(point_Idx + 1);}
            uint32 segment_time_elapsed() const { return next_timestamp() - time_passed;}
            uint32 timeElapsed() const { return Duration() - time_passed;}
            uint32 timePassed() const { return time_passed;}

        public:
            const MySpline& _Spline() const { return spline;}
            int32 _currentSplineIdx() const { return point_Idx;}
            /// 标记样条完成并将游标与耗时置为终点状态
            void _Finalize();
            /// 强制中断当前样条移动
            void _Interrupt() { splineflags.done = true;}
            /// 获取整体移动进度比例 [0, 1]
            float GetElapsedValue() const { return float(time_passed) / Duration(); }

        public:

            /// 使用给定参数初始化或停止样条
            void Initialize(const MoveSplineInitArgs&);
            bool Initialized() const { return !spline.empty();}

            explicit MoveSpline();

            /// 按时间差推进样条状态，并对每次段变更调用 handler 回调
            template<class UpdateHandler>
            void updateState(uint32 difftime, UpdateHandler& handler)
            {
                MANGOS_ASSERT(Initialized());
                do
                    handler(_updateState(difftime));
                while (difftime > 0);
            }

            /// 按时间差推进样条状态（无回调版本）
            void updateState(uint32 difftime)
            {
                MANGOS_ASSERT(Initialized());
                do _updateState(difftime);
                while (difftime > 0);
            }

            /// 计算当前时间点单位的三维位置与朝向
            Location ComputePosition() const;

            uint32 GetId() const { return m_Id;}
            bool Finalized() const { return splineflags.done; } // 是否已完成或未处于移动中
            bool isCyclic() const { return splineflags.cyclic; }
            FacingInfo const& GetFacing() const { return facing; }
            bool isFacing() const { return splineflags.isFacing(); }
            bool isFacingPoint() const { return splineflags.final_point; }
            bool isFacingTarget() const { return splineflags.final_target; }
            bool isFacingAngle() const { return splineflags.final_angle; }
            /// 获取最终目的地坐标
            const Vector3 FinalDestination() const;
            /// 获取当前路段的下一目标顶点
            const Vector3 CurrentDestination() const { return Initialized() ? spline.getPoint(point_Idx + 1) : Vector3();}
            /// 计算对应外部路径点索引
            int32 currentPathIdx() const;
            int32 GetRawPathIndex() const { return point_Idx; }

            /// 返回整条样条的总预计耗时（毫秒）
            uint32 Duration() const { return spline.length();}
            /// 计算从当前时刻到达指定样条顶点 idx 所需的剩余时间（毫秒）
            int32 ComputeTimeToIndex(uint32 idx) const { return spline.length(idx) - time_passed; }

            float Speed() const { return speed; }

            std::string ToString() const;
    };
}
#endif // MANGOSSERVER_MOVEPLINE_H
