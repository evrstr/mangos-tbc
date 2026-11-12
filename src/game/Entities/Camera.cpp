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

#include "Camera.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"
#include "Log/Log.h"
#include "Util/Errors.h"
#include "Entities/Player.h"

// 构造玩家摄像机并默认附着到玩家自身观察点
Camera::Camera(Player* pl) : m_owner(*pl), m_source(pl), m_sendInProgress(false)
{
    m_source->GetViewPoint().Attach(this);
}

// 析构前需确保视角已重置回玩家自身，并解除与观察点的关联
Camera::~Camera()
{
    // view of camera should be already reseted to owner (RemoveFromWorld -> Event_RemovedFromWorld -> ResetView)
    // 摄像机视角应当已重置回所有者（经由 RemoveFromWorld -> Event_RemovedFromWorld -> ResetView）
    MANGOS_ASSERT(m_source == &m_owner);

    // for symmetry with constructor and way to make viewpoint's list empty (与构造函数对称，清空观察点中的引用)
    m_source->GetViewPoint().Detach(this);
}

// 将接收到的数据包直接发送给摄像机所属玩家
void Camera::ReceivePacket(WorldPacket const& data) const
{
    m_owner.SendDirectMessage(data);
}

// 将摄像机挂接到当前观察点所在网格，并向目标玩家下发观察点创建更新
void Camera::UpdateForCurrentViewPoint()
{
    m_gridRef.unlink();

    if (GridType* grid = m_source->GetViewPoint().m_grid)
        grid->AddWorldObject(this);

    m_source->GetMap()->AddUpdateCreateObject(m_source);
}

// 切换摄像机观察点至指定世界对象（用于鹰眼、心灵视界等远视法术）
void Camera::SetView(WorldObject* obj, bool update_far_sight_field /*= true*/)
{
    MANGOS_ASSERT(obj);

    if (m_source == obj)
        return;

    if (!m_owner.IsInMap(obj))
    {
        sLog.outError("Camera::SetView, viewpoint is not in map with camera's owner");
        return;
    }

    if (!obj->isType(TypeMask(TYPEMASK_DYNAMICOBJECT | TYPEMASK_UNIT)))
    {
        sLog.outError("Camera::SetView, viewpoint type is not available for client");
        return;
    }

    // detach and deregister from active objects if there are no more reasons to be active
    // 从旧观察点分离；若旧观察点本身不是主动对象且无其他需要，将其移出地图主动对象集合
    m_source->GetViewPoint().Detach(this);
    if (!m_source->isActiveObject())
        m_source->GetMap()->RemoveFromActive(m_source);

    m_source = obj;

    if (!m_source->isActiveObject())
        m_source->GetMap()->AddToActive(m_source);

    m_source->GetViewPoint().Attach(this);

    if (update_far_sight_field)
        m_owner.SetGuidValue(PLAYER_FARSIGHT, (m_source == &m_owner ? ObjectGuid() : m_source->GetObjectGuid()));

    UpdateForCurrentViewPoint();
}

// 若当前远视目标在客户端已不可见，自动将视角重置回玩家自身
void Camera::Event_ViewPointVisibilityChanged()
{
    if (!m_owner.HasAtClient(m_source))
        ResetView();
}

// 将摄像机视角重置回玩家自身
void Camera::ResetView(bool update_far_sight_field /*= true*/)
{
    SetView(&m_owner, update_far_sight_field);
}

// 观察点加入世界时将摄像机注册到对应网格并刷新周边可见性
void Camera::Event_AddedToWorld(UpdateData* data)
{
    GridType* grid = m_source->GetViewPoint().m_grid;
    MANGOS_ASSERT(grid);
    grid->AddWorldObject(this);

    if (data)
        UpdateVisibilityForOwner(true, *data);
}

// 观察点移出世界时解除网格引用；若正在远视其他对象则重置回玩家自身
void Camera::Event_RemovedFromWorld()
{
    if (m_source == &m_owner)
    {
        m_gridRef.unlink();
        return;
    }

    ResetView();
}

// 观察点跨网格移动时同步迁移摄像机的网格挂接
void Camera::Event_Moved()
{
    m_gridRef.unlink();
    m_source->GetViewPoint().m_grid->AddWorldObject(this);
}

// 更新单个通用 WorldObject 对所属玩家的可见性
void Camera::UpdateVisibilityOf(WorldObject* target, UpdateData& data) const
{
    m_owner.UpdateVisibilityOf(m_source, target, data);
}

// 更新指定具体类型实体对所属玩家的可见性（含可见对象集合维护）
template<class T>
void Camera::UpdateVisibilityOf(T* target, UpdateData& data, WorldObjectSet& vis)
{
    m_owner.UpdateVisibilityOf<T>(m_source, target, data, vis);
}

template void Camera::UpdateVisibilityOf(Player*, UpdateData&, WorldObjectSet&);
template void Camera::UpdateVisibilityOf(Creature*, UpdateData&, WorldObjectSet&);
template void Camera::UpdateVisibilityOf(Corpse*, UpdateData&, WorldObjectSet&);
template void Camera::UpdateVisibilityOf(GameObject*, UpdateData&, WorldObjectSet&);
template void Camera::UpdateVisibilityOf(DynamicObject*, UpdateData&, WorldObjectSet&);

// 为所属玩家分配 UpdateData 并触发周边对象可见性扫描
void Camera::UpdateVisibilityForOwner(UpdateDataMapType& update_players)
{
    UpdateDataMapType::iterator iter = update_players.find(GetOwner());

    if (iter == update_players.end())
    {
        std::pair<UpdateDataMapType::iterator, bool> p = update_players.insert(UpdateDataMapType::value_type(GetOwner(), UpdateData()));
        MANGOS_ASSERT(p.second);
        iter = p.first;
    }
    UpdateVisibilityForOwner(true, iter->second);
}

// 访问当前观察点周围可见范围内的所有对象并通知可见性变更
void Camera::UpdateVisibilityForOwner(bool addToWorld, UpdateData& data)
{
    m_sendInProgress = true;
    MaNGOS::VisibleNotifier notifier(*this, data, !addToWorld);
    Cell::VisitAllObjects(m_source, notifier, addToWorld ? MAX_VISIBILITY_DISTANCE : m_source->GetVisibilityData().GetVisibilityDistance(), false);
    notifier.Notify();
    m_sendInProgress = false;
}

//////////////////

// 观察点析构校验：此时不应仍有未分离的摄像机引用
ViewPoint::~ViewPoint()
{
    if (!m_cameras.empty())
    {
        sLog.outError("ViewPoint destructor called, but some cameras referenced to it");
    }
}
