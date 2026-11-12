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

#ifndef TRANSPORTS_H
#define TRANSPORTS_H

#include "Entities/GameObject.h"
#include "Maps/TransportMgr.h"

#include <map>
#include <set>

// 载具上的乘客对象集合类型
typedef std::set<WorldObject*> PassengerSet;

// 通用运输载具基类（提供乘客上下船、相对坐标与世界坐标换算等公共能力）
class GenericTransport : public GameObject
{
    public:
        GenericTransport() : m_passengerTeleportIterator(m_passengers.end()), m_pathProgress(0), m_movementStarted(0) {}
        bool AddPassenger(Unit* passenger, bool adjustCoords = true);
        bool RemovePassenger(Unit* passenger);
        bool AddPetToTransport(Unit* passenger, Pet* pet);

        void UpdatePosition(float x, float y, float z, float o);
        void UpdatePassengerPosition(WorldObject* object);

        PassengerSet& GetPassengers() { return m_passengers; }

        /// This method transforms supplied transport offsets into global coordinates (将载具局部偏移坐标转换为全局世界坐标)
        void CalculatePassengerPosition(float& x, float& y, float& z, float* o = nullptr) const
        {
            CalculatePassengerPosition(x, y, z, o, GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        }

        /// This method transforms supplied global coordinates into local offsets (将全局世界坐标转换为载具局部偏移坐标)
        void CalculatePassengerOffset(float& x, float& y, float& z, float* o = nullptr) const
        {
            CalculatePassengerOffset(x, y, z, o, GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        }

        void CalculatePassengerOrientation(float& o) const;

        static void CalculatePassengerPosition(float& x, float& y, float& z, float* o, float transX, float transY, float transZ, float transO);
        static void CalculatePassengerOffset(float& x, float& y, float& z, float* o, float transX, float transY, float transZ, float transO);

        uint32 GetPathProgress() const { return m_pathProgress; }
    protected:
        void UpdatePassengerPositions(PassengerSet& passengers);

        PassengerSet m_passengers;                          // 当前乘坐在载具上的单位集合
        PassengerSet::iterator m_passengerTeleportIterator; // 跨地图传送乘客时使用的安全迭代器

        uint32 m_pathProgress; // for MO transport its full time since start for normal time in cycle (路径时间进度：MO 运输载具为启动后累计时间，电梯为周期内时间)
        uint32 m_movementStarted;                           // 运动起始时间戳（毫秒）
};

// 升降梯/局部往复运输工具类（基于 TransportAnimation 关键帧插值移动）
class ElevatorTransport : public GenericTransport
{
    public:
        bool Create(uint32 dbGuid, uint32 guidlow, uint32 name_id, Map* map, float x, float y, float z, float ang,
            float rotation0 = 0.0f, float rotation1 = 0.0f, float rotation2 = 0.0f, float rotation3 = 0.0f, uint32 animprogress = GO_ANIMPROGRESS_DEFAULT, GOState go_state = GO_STATE_READY) override;
        void Update(const uint32 diff) override;

    private:
        TransportAnimation const* m_animationInfo;          // 动画轨迹数据指针
        uint32 m_currentSeg;                                // 当前动画时间段起始偏移
};

// 跨地图大型运输工具类（如船只、飞艇，按关键帧样条航行并支持跨地图传送）
class Transport : public GenericTransport
{
    public:
        explicit Transport(TransportTemplate const& transportTemplate);

        static void LoadTransport(TransportTemplate const& transportTemplate, Map* map);
        bool Create(uint32 guidlow, uint32 mapid, float x, float y, float z, float ang, uint32 animprogress);
        void Update(const uint32 diff) override;

        uint32 GetPeriod() const { return m_period; }
        void SetPeriod(uint32 period) { m_period = period; }

        KeyFrameVec const& GetKeyFrames() const { return m_transportTemplate.keyFrames; }
    private:
        void TeleportTransport(uint32 newMapid, float x, float y, float z, float o);
        void UpdateForMap(Map const* targetMap, bool newMap);
        void DoEventIfAny(TaxiPathNodeEntry const& node, bool departure);
        void MoveToNextWayPoint();                          // move m_next/m_cur to next points (推进当前与下一关键帧迭代器)
        float CalculateSegmentPos(float perc);

        bool IsMoving() const { return m_isMoving; }
        void SetMoving(bool val) { m_isMoving = val; }

        ShortTimeTracker m_positionChangeTimer;             // 位置更新节流计时器
        bool m_isMoving;                                    // 当前是否处于航行状态
        bool m_pendingStop;                                 // 是否存在待处理的停靠请求

        KeyFrameVec::const_iterator m_currentFrame;         // 当前所在路径关键帧迭代器
        KeyFrameVec::const_iterator m_nextFrame;            // 下一路径关键帧迭代器
        uint32 m_pathTime;                                  // 路径总时间

        uint32 m_period;                                    // 完整循环周期（毫秒）

        TransportTemplate const& m_transportTemplate;       // 运输载具模板引用
};
#endif
