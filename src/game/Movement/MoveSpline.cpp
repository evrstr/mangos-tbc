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

#include "MoveSpline.h"
#include <sstream>
#include "Log/Log.h"
#include "Entities/Unit.h"

namespace Movement
{
    extern float computeFallTime(float path_length, bool isSafeFall);
    extern float computeFallElevation(float time_passed, bool isSafeFall, float start_velocy);
    extern float computeFallElevation(float time_passed);

    /// 根据当前已流逝时间计算样条上的三维坐标与朝向
    Location MoveSpline::ComputePosition() const
    {
        MANGOS_ASSERT(Initialized());

        float u = 1.f;
        int32 seg_time = spline.length(point_Idx, point_Idx + 1);
        if (seg_time > 0)
            u = (time_passed - spline.length(point_Idx)) / (float)seg_time;
        Location c;
        spline.evaluate_percent(point_Idx, u, c);

        if (splineflags.falling)
            computeFallElevation(c.z);

        if (splineflags.done && splineflags.isFacing())
        {
            if (splineflags.final_angle)
                c.orientation = facing.angle;
            else if (splineflags.final_point)
                c.orientation = atan2(facing.f.y - c.y, facing.f.x - c.x);
            // nothing to do for MoveSplineFlag::Final_Target flag (Final_Target 无需在此计算朝向)
        }
        else
        {
            Vector3 hermite;
            spline.evaluate_derivative(point_Idx, u, hermite);
            c.orientation = atan2(hermite.y, hermite.x);
        }
        return c;
    }

    /// 结合自由落体公式与终点高度钳位计算当前 Z 轴高度
    void MoveSpline::computeFallElevation(float& el) const
    {
        float z_now = spline.getPoint(spline.first()).z - Movement::computeFallElevation(MSToSec(time_passed));
        float final_z = FinalDestination().z;
        if (z_now < final_z)
            el = final_z;
        else
            el = z_now;
    }

    inline uint32 computeDuration(float length, float velocity)
    {
        return SecToMS(length / velocity);
    }

    /// 坠落样条时间戳计算器：按自由落体耗时累加段长度
    struct FallInitializer
    {
        FallInitializer(float _start_elevation) : start_elevation(_start_elevation) {}
        float start_elevation;
        int32 operator()(Spline<int32>& s, int32 i)
        {
            return Movement::computeFallTime(start_elevation - s.getPoint(i + 1).z, false) * 1000.f;
        }
    };

    enum
    {
        minimal_duration = 1, // 样条最小有效持续时间（1 毫秒），避免除以零
    };

    /// 常规样条时间戳计算器：按各段弧长与速度换算毫秒耗时
    struct CommonInitializer
    {
        CommonInitializer(float _velocity) : velocityInv(1000.f / _velocity), time(minimal_duration) {}
        float velocityInv;
        int32 time;
        inline int32 operator()(Spline<int32>& s, int32 i)
        {
            time += (s.SegLength(i) * velocityInv);
            return time;
        }
    };

    /// 初始化底层几何样条及每段的毫秒时间戳表
    void MoveSpline::init_spline(const MoveSplineInitArgs& args)
    {
        const SplineBase::EvaluationMode modes[2] = {SplineBase::ModeLinear, SplineBase::ModeCatmullrom};
        if (args.flags.cyclic)
        {
            uint32 cyclic_point = 0;
            if (splineflags.enter_cycle)
                cyclic_point = 1;   // shouldn't be modified, came from client (不可修改，客户端协议固定跳过首点入环)
            spline.init_cyclic_spline(&args.path[0], args.path.size(), modes[args.flags.isSmooth()], cyclic_point);
        }
        else
        {
            spline.init_spline(&args.path[0], args.path.size(), modes[args.flags.isSmooth()]);
        }

        // init spline timestamps (初始化样条各段时间戳)
        if (splineflags.falling)
        {
            FallInitializer init(spline.getPoint(spline.first()).z);
            spline.initLengths(init);
        }
        else
        {
            CommonInitializer init(args.velocity);
            spline.initLengths(init);
        }

        // TODO: what to do in such cases? problem is in input data (all points are at same coords)
        // 若所有控制点重合导致总时长不足最小时长，则保底设置时长并记录错误日志
        if (spline.length() < minimal_duration)
        {
            sLog.outError("MoveSpline::init_spline: zero length spline, wrong input data?");
            spline.set_length(spline.last(), spline.isCyclic() ? 1000 : 1);
        }
        point_Idx = spline.first();
        speed = args.velocity;
    }

    /// 载入 MoveSplineInitArgs 参数并启动或停止样条
    void MoveSpline::Initialize(const MoveSplineInitArgs& args)
    {
        splineflags = args.flags;
        facing = args.facing;
        m_Id = args.splineId;
        point_Idx_offset = args.path_Idx_offset;
        time_passed = 0;

        // detect Stop command (检测到 Done 标志则视为停止移动指令，清空样条)
        if (splineflags.done)
        {
            spline.clear();
            return;
        }

        init_spline(args);
    }

    MoveSpline::MoveSpline() : m_Id(0), speed(0), time_passed(0), point_Idx(0), point_Idx_offset(0)
    {
        splineflags.done = true;
    }

/// ============================================================================================

    /// 校验样条初始化参数（至少 2 个顶点且速度大于 0）
    bool MoveSplineInitArgs::Validate(Unit* unit) const
    {
#define CHECK(exp) \
    if (!(exp))\
    {\
        if (unit)\
            sLog.outError("MoveSplineInitArgs::Validate: expression '%s' failed for %s", #exp, unit->GetGuidStr().c_str());\
        else\
            sLog.outError("MoveSplineInitArgs::Validate: expression '%s' failed for cyclic spline continuation", #exp);\
        return false;\
    }
        CHECK(path.size() > 1);
        CHECK(velocity > 0.f);
        // CHECK(_checkPathBounds());
        return true;
#undef CHECK
    }

// MONSTER_MOVE packet format limitation for not CatmullRom movement:
// each vertex offset packed into 11 bytes
// 非 Catmull-Rom 模式下 MONSTER_MOVE 封包格式限制：中间点偏移按 11-bit 有符号定点数打包（精度 0.25 码）
    bool MoveSplineInitArgs::_checkPathBounds() const
    {
        if (!(flags & MoveSplineFlag::Mask_CatmullRom) && path.size() > 2)
        {
            enum
            {
                MAX_OFFSET = (1 << 11) / 2,
            };
            Vector3 middle = (path.front() + path.back()) / 2;
            for (uint32 i = 1; i < path.size() - 1; ++i)
            {
                Vector3 offset = path[i] - middle;
                if (fabs(offset.x / 0.25f) >= float(MAX_OFFSET) || fabs(offset.y / 0.25f) >= float(MAX_OFFSET) || fabs(offset.z / 0.25f) >= float(MAX_OFFSET))
                {
                    sLog.outError("MoveSplineInitArgs::_checkPathBounds check failed");
                    return false;
                }
            }
        }
        return true;
    }

/// ============================================================================================

    /// 步进更新样条时间轴，处理跨段、循环回绕（含移除首轮切入点）以及到达终点
    MoveSpline::UpdateResult MoveSpline::_updateState(uint32& ms_time_diff)
    {
        if (Finalized())
        {
            ms_time_diff = 0;
            return Result_Arrived;
        }

        UpdateResult result = Result_None;

        uint32 minimal_diff = std::min(ms_time_diff, segment_time_elapsed());
        time_passed += minimal_diff;
        ms_time_diff -= minimal_diff;

        if (time_passed >= next_timestamp())
        {
            ++point_Idx;
            if (point_Idx < spline.last())
            {
                result = Result_NextSegment;
            }
            else
            {
                if (spline.isCyclic())
                {
                    point_Idx = spline.first();
                    time_passed = time_passed % Duration();
                    result = Result_NextSegment;

                    // Remove first point from the path after one full cycle.
                    // That point was the position of the unit prior to entering the cycle and it shouldn't be repeated with continuous cycles.
                    // 第一轮循环结束后移除首点（该点为入环前起始位置，后续循环不再重复经过）
                    if (splineflags.enter_cycle)
                    {
                        splineflags.enter_cycle = false;

                        MoveSplineInitArgs args{ (size_t)spline.getPointCount() };
                        args.path.assign(spline.getPoints().begin() + spline.first() + 1, spline.getPoints().begin() + spline.last());
                        args.facing = facing;
                        args.flags = splineflags;
                        args.path_Idx_offset = point_Idx_offset;
                        // MoveSplineFlag::Parabolic | MoveSplineFlag::Animation not supported currently (暂不支持抛物线与附加动画标志)
                            //args.parabolic_amplitude = ?;
                            //args.time_perc = ?;
                        args.splineId = m_Id;
                        args.velocity = 1.0f; // Calculated below (占位速度，下方按原周期总时长反推实际速度)
                        if (args.Validate(nullptr))
                        {
                            // New cycle should preserve previous cycle's duration for some weird reason, even though
                            // the path is really different now. Developers are weird. Or this was just a simple oversight.
                            // Since our splines precalculate length with zspeed in mind, if we want to find the desired
                            // zspeed, we have to make a fake spline, calculate its duration and then compare it to the
                            // desired duration, thus finding out how much the zspeed has to be increased for them to match.
                            // 客户端行为要求后续循环保持首轮总时长，因此先用单位速度构造临时样条算出时长比例再重算速度
                            MoveSpline tempSpline;
                            tempSpline.Initialize(args);
                            args.velocity = (float)tempSpline.Duration() / Duration();

                            if (args.Validate(nullptr))
                                init_spline(args);
                        }
                    }
                }
                else
                {
                    _Finalize();
                    ms_time_diff = 0;
                    result = Result_Arrived;
                }
            }
        }

        return result;
    }

    std::string MoveSpline::ToString() const
    {
        std::stringstream str;
        str << "MoveSpline" << std::endl;
        str << "spline Id: " << GetId() << std::endl;
        str << "flags: " << splineflags.ToString() << std::endl;
        if (splineflags.final_angle)
            str << "facing  angle: " << facing.angle;
        else if (splineflags.final_target)
            str << "facing target: " << facing.target;
        else if (splineflags.final_point)
            str << "facing  point: " << facing.f.x << " " << facing.f.y << " " << facing.f.z;
        str << std::endl;
        str << "time passed: " << time_passed << std::endl;
        str << "total  time: " << Duration() << std::endl;
        str << "spline point Id: " << point_Idx << std::endl;
        str << "path  point  Id: " << currentPathIdx() << std::endl;
        str << spline.ToString();
        return str.str();
    }

    void MoveSpline::_Finalize()
    {
        splineflags.done = true;
        point_Idx = spline.last() - 1;
        time_passed = Duration();
    }

    const Vector3 MoveSpline::FinalDestination() const
    {
        if (Initialized())
        {
            if (isCyclic())
                return spline.getPoint(spline.last() - 1);
            else
                return spline.getPoint(spline.last());
        }
        return Vector3();
    }

    int32 MoveSpline::currentPathIdx() const
    {
        int32 point = point_Idx_offset + point_Idx - spline.first() + (int)Finalized();
        if (isCyclic())
            point = point % (spline.last() - spline.first());
        return point;
    }
}
