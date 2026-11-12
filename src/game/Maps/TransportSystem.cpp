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
 * @addtogroup TransportSystem
 * @{
 *
 * @file TransportSystem.cpp
 * This file contains the code needed for MaNGOS to provide abstract support for transported entities
 * Currently implemented
 * - Calculating between local and global coords
 * - Abstract storage of passengers (added by BoardPassenger, UnboardPassenger)
 */

#include "Maps/TransportSystem.h"
#include "Entities/Unit.h"
#include "Maps/MapManager.h"
#include "Entities/Transports.h"

/* **************************************** TransportBase ****************************************/

TransportBase::TransportBase(WorldObject* owner) :
    m_owner(owner),
    m_lastPosition(owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ(), owner->GetOrientation()),
    m_sinO(sin(m_lastPosition.o)),
    m_cosO(cos(m_lastPosition.o)),
    m_updatePositionsTimer(500)
{
    MANGOS_ASSERT(m_owner);
}

TransportBase::~TransportBase()
{
    MANGOS_ASSERT(m_passengers.empty());
}

// Update every now and then (after some change of transporter's position)
// This is used to calculate global positions (which don't have to be exact, they are only required for some server-side calculations
// 定期检查载具位移变化（每 500ms 检测一次位移或朝向改变，按需重算所有乘客的全局位置用于服务端判定）
void TransportBase::Update(uint32 diff)
{
    if (m_updatePositionsTimer < diff)
    {
        if (fabs(m_owner->GetPositionX() - m_lastPosition.x) +
                fabs(m_owner->GetPositionY() - m_lastPosition.y) +
                fabs(m_owner->GetPositionZ() - m_lastPosition.z) > 1.0f ||
                MapManager::NormalizeOrientation(m_owner->GetOrientation() - m_lastPosition.o) > 0.01f)
            UpdateGlobalPositions();

        m_updatePositionsTimer = 500;
    }
    else
        m_updatePositionsTimer -= diff;
}

// Update the global positions of all passengers (更新载具上所有乘客的世界全局坐标)
void TransportBase::UpdateGlobalPositions()
{
    Position pos(m_owner->GetPositionX(), m_owner->GetPositionY(),
                 m_owner->GetPositionZ(), m_owner->GetOrientation());

    // Calculate new direction multipliers (若载具朝向变化则重新缓存 sin/cos)
    if (MapManager::NormalizeOrientation(pos.o - m_lastPosition.o) > 0.01f)
    {
        m_sinO = sin(pos.o);
        m_cosO = cos(pos.o);
    }

    // Update global positions (遍历乘客并同步更新全局位置)
    for (PassengerMap::const_iterator itr = m_passengers.begin(); itr != m_passengers.end(); ++itr)
        UpdateGlobalPositionOf(itr->first, itr->second->GetLocalPositionX(), itr->second->GetLocalPositionY(),
                               itr->second->GetLocalPositionZ(), itr->second->GetLocalOrientation());

    m_lastPosition = pos;
}

// Update the global position of a passenger (根据局部偏移重算并重定位单个乘客的地图全局坐标)
void TransportBase::UpdateGlobalPositionOf(WorldObject* passenger, float lx, float ly, float lz, float lo) const
{
    Position transportPos = m_owner->GetPosition();
    GenericTransport::CalculatePassengerPosition(lx, ly, lz, &lo, transportPos.x, transportPos.y, transportPos.z, transportPos.o);

    if (passenger->GetTypeId() == TYPEID_PLAYER || passenger->GetTypeId() == TYPEID_UNIT)
    {
        if (passenger->GetTypeId() == TYPEID_PLAYER)
        {
            m_owner->GetMap()->PlayerRelocation((Player*)passenger, lx, ly, lz, lo);
        }
        else
            m_owner->GetMap()->CreatureRelocation((Creature*)passenger, lx, ly, lz, lo);
    }
    // ToDo: Add gameobject relocation (待办：补充游戏对象重定位)
    // ToDo: Add passenger relocation for MO transports (待办：补充 MO 载具乘客重定位)
}

// This rotates the vector (lx, ly) by transporter->orientation (将局部二维向量按载具朝向旋转为世界相对偏移)
void TransportBase::RotateLocalPosition(float lx, float ly, float& rx, float& ry) const
{
    rx = lx * m_cosO - ly * m_sinO;
    ry = lx * m_sinO + ly * m_cosO;
}

// This rotates the vector (rx, ry) by -transporter->orientation (将旋转后的二维向量逆旋转还原为载具局部坐标)
void TransportBase::NormalizeRotatedPosition(float rx, float ry, float& lx, float& ly) const
{
    lx = rx * -m_cosO - ry * -m_sinO;
    ly = rx * -m_sinO + ry * -m_cosO;
}

void TransportBase::CalculatePassengerPosition(float& x, float& y, float& z, float* o) const
{
    GenericTransport::CalculatePassengerPosition(x, y, z, o, m_owner->GetPositionX(), m_owner->GetPositionY(), m_owner->GetPositionZ(), m_owner->GetOrientation());
}

void TransportBase::CalculatePassengerOffset(float& x, float& y, float& z, float* o) const
{
    GenericTransport::CalculatePassengerOffset(x, y, z, o, m_owner->GetPositionX(), m_owner->GetPositionY(), m_owner->GetPositionZ(), m_owner->GetOrientation());
}

// 乘客登上载具：创建 TransportInfo 并建立双向关联
void TransportBase::BoardPassenger(WorldObject* passenger, float lx, float ly, float lz, float lo)
{
    TransportInfo* transportInfo = new TransportInfo(passenger, this, lx, ly, lz, lo);

    // Insert our new passenger (插入新乘客记录)
    m_passengers.insert(PassengerMap::value_type(passenger, transportInfo));

    // The passenger needs fast access to transportInfo (将载具信息指针绑定到乘客对象以便快速访问)
    passenger->SetTransportInfo(transportInfo);
}

// 乘客离开载具：解绑并释放对应的 TransportInfo
void TransportBase::UnBoardPassenger(WorldObject* passenger)
{
    PassengerMap::iterator itr = m_passengers.find(passenger);

    if (itr == m_passengers.end())
        return;

    // Set passengers transportInfo to nullptr (清空乘客的载具信息指针)
    passenger->SetTransportInfo(nullptr);

    // Delete transportInfo (释放 TransportInfo 内存)
    delete itr->second;

    // Unboard finally (从乘客映射表中移除)
    m_passengers.erase(itr);
}

/* **************************************** TransportInfo ****************************************/

TransportInfo::TransportInfo(WorldObject* owner, TransportBase* transport, float lx, float ly, float lz, float lo) :
    m_owner(owner),
    m_transport(transport),
    m_localPosition(lx, ly, lz, lo)
{
    MANGOS_ASSERT(owner && m_transport);
}

// 更新乘客相对载具的局部坐标，并立即同步更新其在地图上的全局位置
void TransportInfo::SetLocalPosition(float lx, float ly, float lz, float lo)
{
    m_localPosition.x = lx;
    m_localPosition.y = ly;
    m_localPosition.z = lz;
    m_localPosition.o = lo;

    // Update global position (同步刷新全局坐标)
    m_transport->UpdateGlobalPositionOf(m_owner, lx, ly, lz, lo);
}

/*! @} */
