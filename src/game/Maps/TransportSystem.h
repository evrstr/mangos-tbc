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

/**
 * @addtogroup TransportSystem to provide abstract support for transported entities
 * The Transport System in MaNGOS consists of these files:
 * - TransportSystem.h to provide the basic classes TransportBase and TransportInfo
 * - TransportSystem.cpp which implements these classes
 * - Transports.h to implement the MOTransporter (subclas of gameobject) - Remains TODO
 * as well of
 * - impacts to various files
 *
 * @{
 *
 * @file TransportSystem.h
 * This file contains the headers for base clases needed for MaNGOS to handle passengers on transports
 *
 */

#ifndef _TRANSPORT_SYSTEM_H
#define _TRANSPORT_SYSTEM_H

#include "Common.h"
#include "Entities/Object.h"

class TransportInfo;

typedef std::unordered_map<WorldObject* /*passenger*/, TransportInfo* /*passengerInfo*/> PassengerMap;

/**
 * A class to provide basic support for each transporter. This includes
 * - Storing a list of passengers
 * - Providing helper for calculating between local and global coordinates
 * 载具基础支持类，为可搭载乘客的运输体提供通用功能：
 * - 维护乘客列表
 * - 提供局部坐标与世界全局坐标之间的转换辅助函数
 */

class TransportBase
{
    public:
        explicit TransportBase(WorldObject* owner);
        virtual ~TransportBase();

        void Update(uint32 diff);
        void UpdateGlobalPositions();
        void UpdateGlobalPositionOf(WorldObject* passenger, float lx, float ly, float lz, float lo) const;

        WorldObject* GetOwner() const { return m_owner; }

        // Helper functions to calculate positions (位置与旋转坐标计算辅助函数)
        void RotateLocalPosition(float lx, float ly, float& rx, float& ry) const;
        void NormalizeRotatedPosition(float rx, float ry, float& lx, float& ly) const;

        void CalculatePassengerPosition(float& x, float& y, float& z, float* o = nullptr) const;

        /// This method transforms supplied global coordinates into local offsets
        /// 将给定的世界全局坐标转换为相对于载具的局部偏移坐标
        void CalculatePassengerOffset(float& x, float& y, float& z, float* o = nullptr) const;

    protected:
        // Helper functions to add/ remove a passenger from the list (添加/移除乘客的辅助函数)
        void BoardPassenger(WorldObject* passenger, float lx, float ly, float lz, float lo);
        void UnBoardPassenger(WorldObject* passenger);

        WorldObject* m_owner;                               ///< The transporting unit (承载乘客的运输实体)
        PassengerMap m_passengers;                          ///< List of passengers and their transport-information (乘客及其乘载信息映射表)

        // Helpers to speedup position calculations (缓存上次位置与三角函数值以加速坐标变换)
        Position m_lastPosition;
        float m_sinO, m_cosO;
        uint32 m_updatePositionsTimer;                      ///< Timer that is used to trigger updates for global coordinate calculations (触发批量全局坐标更新的计时器)
};

/**
 * A class to provide basic information for each transported passenger. This includes
 * - local positions
 * - Accessors to get the transporter
 * 单个被运输乘客的信息类：
 * - 存储乘客在载具上的局部坐标
 * - 提供获取所属载具及坐标转换的访问接口
 */

class TransportInfo
{
    public:
        explicit TransportInfo(WorldObject* owner, TransportBase* transport, float lx, float ly, float lz, float lo);

        // Set local positions (设置局部坐标)
        void SetLocalPosition(float lx, float ly, float lz, float lo);

        // Accessors (载具访问器)
        WorldObject* GetTransport() const { return m_transport->GetOwner(); }
        ObjectGuid GetTransportGuid() const { return m_transport->GetOwner()->GetObjectGuid(); }

        // Get local position (获取局部坐标)
        float GetLocalOrientation() const { return m_localPosition.o; }
        float GetLocalPositionX() const { return m_localPosition.x; }
        float GetLocalPositionY() const { return m_localPosition.y; }
        float GetLocalPositionZ() const { return m_localPosition.z; }
        void GetLocalPosition(float& lx, float& ly, float& lz, float& lo) const
        {
            lx = m_localPosition.x;
            ly = m_localPosition.y;
            lz = m_localPosition.z;
            lo = m_localPosition.o;
        }

        void CalculatePassengerPosition(float& x, float& y, float& z, float* o = nullptr) const
        {
            m_transport->CalculatePassengerPosition(x, y, z, o);
        }
        void CalculatePassengerOffset(float& x, float& y, float& z, float* o = nullptr) const
        {
            m_transport->CalculatePassengerOffset(x, y, z, o);
        }

    private:
        WorldObject* m_owner;                               ///< Passenger
        TransportBase* m_transport;                         ///< Transporter
        Position m_localPosition;
};

#endif
/*! @} */
