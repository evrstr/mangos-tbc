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

#ifndef MANGOS_POINTMOVEMENTGENERATOR_H
#define MANGOS_POINTMOVEMENTGENERATOR_H

#include "MotionGenerators/MovementGenerator.h"

/// 点对点移动生成器：控制单位移动到指定三维坐标（可选寻路、速度与终点朝向）
class PointMovementGenerator : public MovementGenerator
{
    public:
        PointMovementGenerator(uint32 id, float x, float y, float z, float o, bool generatePath, uint32 forcedMovement, float speed = 0.f, ObjectGuid guid = ObjectGuid(), uint32 relayId = 0) :
            m_x(x), m_y(y), m_z(z), m_o(o), m_speed(speed), m_generatePath(generatePath), m_forcedMovement(forcedMovement), m_id(id), m_speedChanged(false),
            m_guid(guid), m_relayId(relayId) {}
        PointMovementGenerator(uint32 id, float x, float y, float z, bool generatePath, uint32 forcedMovement, float speed = 0.f) :
            PointMovementGenerator(id, x, y, z, 0, generatePath, forcedMovement, speed) {}

        void Initialize(Unit& unit) override;
        void Finalize(Unit& unit) override;
        void Interrupt(Unit& unit) override;
        void Reset(Unit& unit) override;
        bool Update(Unit& unit, const uint32&/* diff*/) override;

        void UnitSpeedChanged() override { m_speedChanged = true; }

        MovementGeneratorType GetMovementGeneratorType() const override { return POINT_MOTION_TYPE; }

    protected:
        /// 构建并下发 MoveSplineInit 到目标坐标
        virtual void Move(Unit& unit);
        /// 到达目标点后通知 AI、召唤者及触发 Relay 脚本
        virtual void MovementInform(Unit& unit);

    protected:
        float m_x, m_y, m_z, m_o, m_speed;
        bool m_generatePath;
        uint32 m_forcedMovement;

    private:
        uint32 m_id;
        bool m_speedChanged;
        ObjectGuid m_guid;
        uint32 m_relayId;
};

/// 撤退移动生成器：AI 脱离目标并移动到指定点，到达后等待 delay 毫秒再结束
class RetreatMovementGenerator : public PointMovementGenerator
{
    public:
        RetreatMovementGenerator(float x, float y, float z, float o, uint32 delay) :
            PointMovementGenerator(0, x, y, z, o, true, 0), m_delayTimer(delay), m_arrived(false) {}

        void Initialize(Unit& unit) override;
        void Finalize(Unit& unit) override;
        void Interrupt(Unit& unit) override;
        void Reset(Unit& unit) override;
        bool Update(Unit& unit, const uint32& diff) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return RETREAT_MOTION_TYPE; }

    protected:
        void MovementInform(Unit&) override {}

    private:
        ShortTimeTracker m_delayTimer; // 到达撤退点后的驻留倒计时
        bool m_arrived;
};

/// 驻守移动生成器：移动到目标点后持续驻留（Update 恒返回 true 直到被外部替换）
class StayMovementGenerator : public PointMovementGenerator
{
    public:
        StayMovementGenerator(float x, float y, float z, float o = 0) :
            PointMovementGenerator(0, x, y, z, o, true, FORCED_MOVEMENT_RUN), m_arrived(false) {}

        void Initialize(Unit& unit) override;
        void Finalize(Unit& unit) override;
        void Interrupt(Unit& unit) override;
        bool Update(Unit& unit, const uint32& diff) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return STAY_MOTION_TYPE; }

    protected:
        void MovementInform(Unit&) override {}

    private:
        bool m_arrived;
};

/// 起飞/降落点移动生成器（TakeOff/Land）：强制使用飞行样条直线前往目标点
class PointTOLMovementGenerator : public PointMovementGenerator
{
    public:
        PointTOLMovementGenerator(uint32 id, float x, float y, float z, bool takeOff, uint32 forcedMovement, float speed = 0) :
            PointMovementGenerator(id, x, y, z, false, forcedMovement, speed), m_takeOff(takeOff) {}

    protected:
        void Move(Unit& unit) override;

    private:
        bool m_takeOff;
};

#endif
