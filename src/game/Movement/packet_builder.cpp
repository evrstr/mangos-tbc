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

#include "packet_builder.h"
#include "MoveSpline.h"
#include "Server/WorldPacket.h"

namespace Movement
{
    inline void operator << (ByteBuffer& b, const Vector3& v)
    {
        b << v.x << v.y << v.z;
    }

    inline void operator >> (ByteBuffer& b, Vector3& v)
    {
        b >> v.x >> v.y >> v.z;
    }

    /// 写入怪物移动封包的公共头部（当前位置、样条 ID、朝向参数、样条标志与总耗时）
    void PacketBuilder::WriteCommonMonsterMovePart(const MoveSpline& move_spline, WorldPacket& data)
    {
        MoveSplineFlag splineflags = move_spline.splineflags;

        /*if (mov.IsBoarded())
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            data << mov.GetTransport()->Owner.GetPackGUID();
        }*/

        data << move_spline.spline.getPoint(move_spline.spline.first());
        data << move_spline.GetId();

        switch (splineflags & MoveSplineFlag::Mask_Final_Facing)
        {
            default:
                data << uint8(MonsterMoveNormal);
                break;
            case MoveSplineFlag::Final_Target:
                data << uint8(MonsterMoveFacingTarget);
                data << move_spline.facing.target;
                break;
            case MoveSplineFlag::Final_Angle:
                data << uint8(MonsterMoveFacingAngle);
                data << move_spline.facing.angle;
                break;
            case MoveSplineFlag::Final_Point:
                data << uint8(MonsterMoveFacingSpot);
                data << move_spline.facing.f.x << move_spline.facing.f.y << move_spline.facing.f.z;
                break;
        }

        // add fake Runmode flag - client has strange issues without that flag
        // 强制附加 Runmode 标志：客户端缺少该位时会出现异常表现
        data << uint32((splineflags & ~MoveSplineFlag::Mask_No_Monster_Move) | MoveSplineFlag::Runmode);
        data << move_spline.Duration();
    }

    /// 序列化线性路径顶点：先写终点坐标，中间节点按与中点的 11-bit 压缩偏移量写入
    void WriteLinearPath(const Spline<int32>& spline, ByteBuffer& data)
    {
        uint32 last_idx = spline.getPointCount() - 3;
        const Vector3* real_path = &spline.getPoint(1);

        data << last_idx;
        data << real_path[last_idx];   // destination (终点绝对坐标)
        if (last_idx > 1)
        {
            Vector3 middle = (real_path[0] + real_path[last_idx]) / 2.f;
            // first and last points already appended (首尾点已分别写入公共头部与终点字段，此处仅打包中间偏移点)
            for (uint32 i = 1; i < last_idx; ++i)
            {
                Vector3 offset = middle - real_path[i];
                data.appendPackXYZ(offset.x, offset.y, offset.z);
            }
        }
    }

    /// 序列化非循环 Catmull-Rom 路径顶点（跳过首尾虚拟控制点，直接写入原始 Vector3）
    void WriteCatmullRomPath(const Spline<int32>& spline, ByteBuffer& data)
    {
        uint32 count = spline.getPointCount() - 3;
        data << count;
        data.append<Vector3>(&spline.getPoint(2), count);
    }

    /// 序列化循环 Catmull-Rom 路径顶点（去掉首尾虚拟及闭合重复点）
    void WriteCatmullRomCyclicPath(Spline<int32> const& spline, ByteBuffer& data)
    {
        uint32 count = spline.getPointCount() - 4;
        data << count;
        data.append<Vector3>(&spline.getPoint(2), count);
    }

    /// 写入完整的 SMSG_MONSTER_MOVE 封包体（公共头部 + 线性或 Catmull-Rom 路径点列表）
    void PacketBuilder::WriteMonsterMove(const MoveSpline& move_spline, WorldPacket& data)
    {
        WriteCommonMonsterMovePart(move_spline, data);

        const Spline<int32>& spline = move_spline.spline;
        MoveSplineFlag splineflags = move_spline.splineflags;
        if (splineflags & MoveSplineFlag::Mask_CatmullRom)
        {
            if (splineflags.cyclic)
                WriteCatmullRomCyclicPath(spline, data);
            else
                WriteCatmullRomPath(spline, data);
        }
        else
            WriteLinearPath(spline, data);
    }

    /// 写入对象创建块（SMSG_UPDATE_OBJECT）中的运动样条完整状态快照
    void PacketBuilder::WriteCreate(const MoveSpline& move_spline, ByteBuffer& data)
    {
        // WriteClientStatus(mov,data);
        // data.append<float>(&mov.m_float_values[SpeedWalk], SpeedMaxCount);
        // if (mov.SplineEnabled())
        {
            MoveSplineFlag splineFlags = move_spline.splineflags;

            data << splineFlags.raw();

            if (splineFlags.final_angle)
            {
                data << move_spline.facing.angle;
            }
            else if (splineFlags.final_target)
            {
                data << move_spline.facing.target;
            }
            else if (splineFlags.final_point)
            {
                data << move_spline.facing.f.x << move_spline.facing.f.y << move_spline.facing.f.z;
            }

            data << move_spline.timePassed();
            data << move_spline.Duration();
            data << move_spline.GetId();

            uint32 nodes = move_spline.getPath().size();
            data << nodes;
            data.append<Vector3>(&move_spline.getPath()[0], nodes);
            data << (move_spline.isCyclic() ? Vector3::zero() : move_spline.FinalDestination());
        }
    }
}
