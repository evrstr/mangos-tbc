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

#ifndef MANGOSSERVER_MOVESPLINEINIT_ARGS_H
#define MANGOSSERVER_MOVESPLINEINIT_ARGS_H

#include "MoveSplineFlag.h"
#include <G3D/Vector3.h>

class Unit;

namespace Movement
{
    typedef std::vector<Vector3> PointsArray; // 路径三维顶点数组

    /// 终点朝向信息联合体（点坐标 / 目标 GUID / 朝向角度）
    union FacingInfo
    {
        struct
        {
            float x, y, z;
        } f;
        uint64  target;
        float   angle;

        FacingInfo(float o) : angle(o) {}
        FacingInfo(uint64 t) : target(t) {}
        FacingInfo() : target(0) {}
    };

    /// 构造 MoveSpline 所需的初始化参数包
    struct MoveSplineInitArgs
    {
            MoveSplineInitArgs(size_t path_capacity = 16) : path_Idx_offset(0),
                velocity(0), splineId(0), slowed(0.f)
            {
                path.reserve(path_capacity);
            }

            PointsArray path;             // 移动路径控制点序列
            FacingInfo facing;            // 移动结束时的朝向设置
            MoveSplineFlag flags;         // 样条移动标志位
            int32 path_Idx_offset;        // 路径点起始索引偏移（用于分段路径通知）
            float velocity;               // 移动速度（为 0 时自动根据单位当前速度选取）
            uint32 splineId;              // 分配的样条唯一 ID
            float slowed;                 // 战斗减速系数（非 0 时按比例缩放速度）

            /** Returns true to show that the arguments were configured correctly and MoveSpline initialization will succeed. */
            /// 校验参数是否合法，返回 true 表示可用于初始化 MoveSpline
            bool Validate(Unit* unit) const;
            /// 检查非 Catmull-Rom 路径点偏移是否超出封包 11-bit 压缩范围
            bool _checkPathBounds() const;
    };
}

#endif // MANGOSSERVER_MOVESPLINEINIT_ARGS_H
