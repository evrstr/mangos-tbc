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

#ifndef MANGOSSERVER_MOVESPLINEFLAG_H
#define MANGOSSERVER_MOVESPLINEFLAG_H

#include "typedefs.h"
namespace Movement
{
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

    /// 样条移动标志位集合（32 位紧凑位域布局，与客户端协议字段对应）
    class MoveSplineFlag
    {
        public:
            enum eFlags : uint32
            {
                None         = 0x00000000,
                Done         = 0x00000001,           // 移动已完成或已停止
                Falling      = 0x00000002,           // Affects elevation computation (正在坠落，影响高度计算)
                Unknown3     = 0x00000004,
                Unknown4     = 0x00000008,
                Unknown5     = 0x00000010,
                Unknown6     = 0x00000020,
                Unknown7     = 0x00000040,
                Unknown8     = 0x00000080,
                Runmode      = 0x00000100,           // 奔跑模式（未设置则为步行）
                Flying       = 0x00000200,           // Smooth movement(Catmullrom interpolation mode), flying animation (平滑飞行模式，使用 Catmull-Rom 插值并播放飞行动画)
                No_Spline    = 0x00000400,           // 不使用样条插值
                Unknown12    = 0x00000800,
                Unknown13    = 0x00001000,
                Unknown14    = 0x00002000,
                Unknown15    = 0x00004000,
                Unknown16    = 0x00008000,
                Final_Point  = 0x00010000,           // 到达终点后朝向指定坐标点
                Final_Target = 0x00020000,           // 到达终点后朝向指定目标 GUID
                Final_Angle  = 0x00040000,           // 到达终点后朝向指定角度
                Unknown19    = 0x00080000,           // exists, but unknown what it does (已知存在但作用未知)
                Cyclic       = 0x00100000,           // Movement by cycled spline (沿循环闭合样条移动)
                Enter_Cycle  = 0x00200000,           // Everytimes appears with cyclic flag in monster move packet, erases first spline vertex after first cycle done (与 Cyclic 同现，首轮循环结束后移除首个切入顶点)
                Frozen       = 0x00400000,           // Will never arrive (冻结状态，不会到达终点)
                Unknown23    = 0x00800000,
                Unknown24    = 0x01000000,
                Unknown25    = 0x02000000,          // exists, but unknown what it does (已知存在但作用未知)
                Unknown26    = 0x04000000,
                Unknown27    = 0x08000000,
                Unknown28    = 0x10000000,
                Unknown29    = 0x20000000,
                Unknown30    = 0x40000000,
                Unknown31    = 0x80000000,

                // Masks (组合掩码)
                Mask_Final_Facing = Final_Point | Final_Target | Final_Angle, // 终点朝向类型掩码
                // flags that shouldn't be appended into SMSG_MONSTER_MOVE\SMSG_MONSTER_MOVE_TRANSPORT packet, should be more probably
                // 不应写入 SMSG_MONSTER_MOVE / SMSG_MONSTER_MOVE_TRANSPORT 报文的内部标志掩码
                Mask_No_Monster_Move = Mask_Final_Facing | Done,
                // CatmullRom interpolation mode used (使用 Catmull-Rom 平滑插值的标志掩码)
                Mask_CatmullRom = Flying,
            };

            inline uint32& raw() { return (uint32&) * this;}
            inline const uint32& raw() const { return (const uint32&) * this;}

            MoveSplineFlag() { raw() = 0; }
            MoveSplineFlag(uint32 f) { raw() = f; }
            MoveSplineFlag(const MoveSplineFlag& f) { raw() = f.raw(); }

            // Constant interface (只读查询接口)

            bool isSmooth() const { return (raw() & Mask_CatmullRom) != 0;} // 是否使用平滑 Catmull-Rom 曲线
            bool isFacing() const { return (raw() & Mask_Final_Facing) != 0;} // 是否设置了终点朝向

            bool hasAllFlags(uint32 f) const { return (raw() & f) == f;}
            uint32 operator & (uint32 f) const { return (raw() & f);}
            uint32 operator | (uint32 f) const { return (raw() | f);}
            std::string ToString() const;

            // Not constant interface (状态修改接口)

            void operator &= (uint32 f) { raw() &= f;}
            void operator |= (uint32 f) { raw() |= f;}

            void EnableFacingPoint()    { raw() = (raw() & ~Mask_Final_Facing) | Final_Point;}  // 设置终点朝向指定坐标
            void EnableFacingAngle()    { raw() = (raw() & ~Mask_Final_Facing) | Final_Angle;}  // 设置终点朝向指定角度
            void EnableFacingTarget()   { raw() = (raw() & ~Mask_Final_Facing) | Final_Target;} // 设置终点朝向指定目标

            bool done          : 1;
            bool falling       : 1;
            bool unknown3      : 1;
            bool unknown4      : 1;
            bool unknown5      : 1;
            bool unknown6      : 1;
            bool unknown7      : 1;
            bool unknown8      : 1;
            bool runmode       : 1;
            bool flying        : 1;
            bool no_spline     : 1;
            bool unknown12     : 1;
            bool unknown13     : 1;
            bool unknown14     : 1;
            bool unknown15     : 1;
            bool unknown16     : 1;
            bool final_point   : 1;
            bool final_target  : 1;
            bool final_angle   : 1;
            bool unknown19     : 1;
            bool cyclic        : 1;
            bool enter_cycle   : 1;
            bool frozen        : 1;
            bool unknown23     : 1;
            bool unknown24     : 1;
            bool unknown25     : 1;
            bool unknown26     : 1;
            bool unknown27     : 1;
            bool unknown28     : 1;
            bool unknown29     : 1;
            bool unknown30     : 1;
            bool unknown31     : 1;
    };
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif
}

#endif // MANGOSSERVER_MOVESPLINEFLAG_H
