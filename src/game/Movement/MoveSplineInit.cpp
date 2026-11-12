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

#include "MoveSplineInit.h"
#include "MoveSpline.h"
#include "packet_builder.h"
#include "Entities/Unit.h"
#include "Log/Log.h"
#include "Maps/TransportSystem.h"
#include "Entities/Transports.h"

namespace Movement
{
    static thread_local uint32 splineCounter = 1; // 线程局部样条 ID 递增计数器

    /// 启动样条移动：计算真实起点（含载具局部坐标）、更新移动标志并广播 SMSG_MONSTER_MOVE(_TRANSPORT)
    int32 MoveSplineInit::Launch()
    {
        // show path in the client if need (若开启路点调试标志，则在客户端生成可视化路点标记生物)
        if (unit.HaveDebugFlag(CMDEBUGFLAG_WP_PATH))
        {
            uint32 counter = 0;
            for (auto pt : args.path)
            {
                TempSpawnSettings settings;
                settings.spawner = &unit;
                settings.entry = VISUAL_WAYPOINT;
                settings.x = pt.x; settings.y = pt.y; settings.z = pt.z; settings.ori = 0.0f;
                settings.activeObject = true;
                settings.despawnTime = 30 * IN_MILLISECONDS;
                settings.spawnType = TEMPSPAWN_TIMED_DESPAWN;
                settings.spawnDataEntry = 2;

                settings.tempSpawnMovegen = true;
                settings.waypointId = counter++;

                WorldObject::SummonCreature(settings, unit.GetMap());
            }
        }
        MoveSpline& move_spline = *unit.movespline;
        TransportInfo* transportInfo = unit.GetTransportInfo();
        // TODO: merge these two together (待将 TransportInfo 与 GenericTransport 两套载具接口合并)
        GenericTransport* transport = unit.GetTransport();

        Location real_position(unit.GetPositionX(), unit.GetPositionY(), unit.GetPositionZ(), unit.GetOrientation());

        // If boarded use current local position (若位于载具/传送船上，则转换为载具局部坐标)
        if (transportInfo)
            transportInfo->GetLocalPosition(real_position.x, real_position.y, real_position.z, real_position.orientation);
        if (transport)
            transport->CalculatePassengerOffset(real_position.x, real_position.y, real_position.z, &real_position.orientation);

        // there is a big chance that current position is unknown if current state is not finalized, need compute it
        // this also allows calculate spline position and update map position in much greater intervals
        // 若上一条样条尚未结束，直接按当前耗时插值出精确位置，避免低频位置同步带来的误差
        if (!move_spline.Finalized())
            real_position = move_spline.ComputePosition();

        bool pathEmpty = false;
        if (args.path.empty())
        {
            // should i do the things that user should do? (若调用方未填充路径，则原地生成双点空路径)
            MoveTo(real_position);
            pathEmpty = true;
        }

        // corrent first vertex (修正路径起点为单位当前真实位置，并同步循环入环标志)
        args.path[0] = real_position;
        args.flags.enter_cycle = args.flags.cyclic;
        uint32 moveFlags = unit.m_movementInfo.GetMovementFlags();
        if (args.flags.runmode)
            moveFlags &= ~MOVEFLAG_WALK_MODE;
        else
            moveFlags |= MOVEFLAG_WALK_MODE;

        moveFlags |= (MOVEFLAG_SPLINE_ENABLED | MOVEFLAG_FORWARD);

        // 未指定速度时根据地面奔跑/步行速度自动取值（忽略飞行与游泳标志以兼容通用脚本），并应用战斗减速系数
        if (args.velocity == 0.f) // ignore swim speed and flight speed because its not used in generic scripting - always possible to override
        {
            args.velocity = unit.GetSpeed(MovementInfo::GetSpeedType(MovementFlags(moveFlags & ~(MOVEFLAG_FLYING | MOVEFLAG_SWIMMING))));
            if (args.slowed != 0.f) // when set always > 0.5 (设置时恒大于 0.5)
                args.velocity *= args.slowed;
        }

        if (!args.Validate(&unit))
            return 0;

        // 定身（Root）状态下不允许发起非空位移，输出调试堆栈后中止
        if (moveFlags & MOVEFLAG_ROOT && !pathEmpty)
        {
            sLog.outCustomLog("Invalid movement during root. Entry: %u IsImmobilized %s, moveflags %u", unit.GetEntry(), unit.IsImmobilizedState() ? "true" : "false", moveFlags);
            sLog.traceLog();
            return 0;
        }

        args.splineId = splineCounter++;

        unit.m_movementInfo.SetMovementFlags(MovementFlags(moveFlags));
        move_spline.Initialize(args);

        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit.GetPackGUID();

        if (transportInfo || transport)
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            if (transportInfo)
                data << transportInfo->GetTransportGuid().WriteAsPacked();
            else if (transport)
                data << transport->GetPackGUID();
        }

        PacketBuilder::WriteMonsterMove(move_spline, data);
        unit.SendMessageToAllWhoSeeMeMove(data, ObjectGuid());

        return move_spline.Duration();
    }

    /// 停止单位当前样条移动并向周围可见玩家广播 MonsterMoveStop 封包
    void MoveSplineInit::Stop(bool forceSend /*= false*/)
    {
        MoveSpline& move_spline = *unit.movespline;

        // No need to stop if we are not moving (当前未处于移动中且未强制发送时无需广播停止包)
        if (!forceSend && move_spline.Finalized())
            return;

        TransportInfo* transportInfo = unit.GetTransportInfo();
        // TODO: merge these two together (待将两套载具接口合并)
        GenericTransport* transport = unit.GetTransport();

        Location real_position(unit.GetPositionX(), unit.GetPositionY(), unit.GetPositionZ(), unit.GetOrientation());

        // If boarded use current local position (载具上使用局部坐标)
        if (transportInfo)
            transportInfo->GetLocalPosition(real_position.x, real_position.y, real_position.z, real_position.orientation);
        if (transport)
            transport->CalculatePassengerOffset(real_position.x, real_position.y, real_position.z, &real_position.orientation);

        // there is a big chance that current position is unknown if current state is not finalized, need compute it
        // this also allows calculate spline position and update map position in much greater intervals
        // 若样条尚未结束，先插值算出当前精确坐标作为停止位置
        if (!move_spline.Finalized())
            real_position = move_spline.ComputePosition();

        if (args.path.empty())
        {
            // should i do the things that user should do? (若路径为空则填充当前位置)
            MoveTo(real_position);
        }

        // current first vertex (修正首顶点为停止点坐标)
        args.path[0] = real_position;

        args.splineId = splineCounter++;

        args.flags = MoveSplineFlag::Done;
        unit.m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_FORWARD | MOVEFLAG_SPLINE_ENABLED));
        move_spline.Initialize(args);

        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit.GetPackGUID();

        if (transportInfo || transport)
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            if (transportInfo)
                data << transportInfo->GetTransportGuid().WriteAsPacked();
            else
                data << transport->GetPackGUID();
        }

        data << real_position.x << real_position.y << real_position.z;
        data << move_spline.GetId();
        data << uint8(MonsterMoveStop);
        unit.SendMessageToAllWhoSeeMeMove(data, ObjectGuid());
    }

    MoveSplineInit::MoveSplineInit(Unit& m) : unit(m)
    {
        // mix existing state into new (从单位当前移动状态继承步行与飞行默认标志)
        args.flags.runmode = !unit.m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE);
        args.flags.flying = unit.m_movementInfo.HasMovementFlag((MovementFlags)(MOVEFLAG_FLYING | MOVEFLAG_HOVER | MOVEFLAG_CAN_FLY | MOVEFLAG_LEVITATING));
    }

    void MoveSplineInit::SetFacing(const WorldObject* target)
    {
        args.flags.EnableFacingTarget();
        args.facing.target = target->GetObjectGuid().GetRawValue();
    }

    void MoveSplineInit::SetFacing(float angle)
    {
        args.facing.angle = G3D::wrap(angle, 0.f, (float)G3D::twoPi());
        args.flags.EnableFacingAngle();
    }
}
