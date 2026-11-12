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

#ifndef MANGOSSERVER_CAMERA_H
#define MANGOSSERVER_CAMERA_H

#include "Common.h"
#include "Maps/GridDefines.h"
#include "Entities/EntitiesMgr.h"

class ViewPoint;
class UpdateData;
class WorldPacket;

/// Camera - object-receiver. Receives broadcast packets from nearby worldobjects, object visibility changes and sends them to client
/// 摄像机（视角接收器）：接收观察点周边世界对象的广播数据包与可见性变化并转发给所属玩家客户端
class Camera
{
        friend class ViewPoint;
    public:

        explicit Camera(Player* pl);
        ~Camera();

        WorldObject* GetBody() const { return m_source;}         // 获取当前观察的实体对象（玩家自身或鹰眼/远视目标）
        Player* GetOwner() const { return &m_owner;}             // 获取摄像机所属玩家

        // set camera's view to any worldobject
        // Note: this worldobject must be in same map, in same phase with camera's owner(player)
        // client supports only unit and dynamic objects as farsight objects
        // 将摄像机视角切换到指定世界对象（需与玩家同地图同相位，客户端仅支持 Unit 与 DynamicObject 作为远视对象）
        void SetView(WorldObject* obj, bool update_far_sight_field = true);

        // set view to camera's owner (将视角重置回摄像机所属玩家自身)
        void ResetView(bool update_far_sight_field = true);

        // 更新指定目标对摄像机所属玩家的可见性
        template<class T>
        void UpdateVisibilityOf(T* target, UpdateData& data, WorldObjectSet& vis);
        void UpdateVisibilityOf(WorldObject* target, UpdateData& data) const;

        // 转发数据包至所属玩家客户端
        void ReceivePacket(WorldPacket const& data) const;

        // updates visibility of worldobjects around viewpoint for camera's owner
        // 更新观察点周边世界对象对所属玩家的可见性
        void UpdateVisibilityForOwner(UpdateDataMapType& update_players);
        void UpdateVisibilityForOwner(bool addToWorld, UpdateData& data);

        bool IsSendInProgress() const { return m_sendInProgress; }

    private:
        // called when viewpoint changes visibility state (观察点可见性或网格状态变化时触发的回调)
        void Event_AddedToWorld(UpdateData* data);
        void Event_RemovedFromWorld();
        void Event_Moved();
        void Event_ViewPointVisibilityChanged();

        Player& m_owner;                                         // 摄像机所属玩家引用
        WorldObject* m_source;                                   // 当前观察点实体指针

        void UpdateForCurrentViewPoint();

    public:
        GridReference<Camera>& GetGridRef() { return m_gridRef; }
        bool isActiveObject() const { return false; }
    private:
        GridReference<Camera> m_gridRef;                         // 摄像机在地图网格中的引用节点
        bool m_sendInProgress;                                   // 防止可见性更新重入发送的标志位
};

/// Object-observer, notifies farsight object state to cameras that attached to it
/// 观察点对象：挂接在 WorldObject 上，将实体的世界事件与状态变化通知给所有附着的 Camera
class ViewPoint
{
        friend class Camera;

        typedef std::list<Camera*> CameraList;

        CameraList m_cameras;                                    // 当前附着在此观察点上的摄像机列表
        GridType* m_grid;                                        // 观察点当前所在的地图网格指针

        void Attach(Camera* c) { m_cameras.push_back(c); }
        void Detach(Camera* c) { m_cameras.remove(c); }

        // 遍历并安全调用所有附着摄像机的处理函数
        template <typename F>
        void CameraCall(F handler)
        {
            if (!m_cameras.empty())
            {
                for (CameraList::iterator itr = m_cameras.begin(); itr != m_cameras.end();)
                {
                    Camera* c = *(itr++);
                    handler(c);
                }
            }
        }

    public:

        ViewPoint() : m_grid(nullptr) {}
        ~ViewPoint();

        bool hasViewers() const { return !m_cameras.empty(); }

        // these events are called when viewpoint changes visibility state
        // 观察点加入世界、离开世界、跨网格移动或可见性变化时广播给附着摄像机
        void Event_AddedToWorld(GridType* grid, UpdateData* data)
        {
            m_grid = grid;
            CameraCall([&](Camera* c) { c->Event_AddedToWorld(data); });
        }

        void Event_RemovedFromWorld()
        {
            m_grid = nullptr;
            CameraCall([&](Camera* c) { c->Event_RemovedFromWorld(); });
        }

        void Event_GridChanged(GridType* grid)
        {
            m_grid = grid;
            CameraCall([&](Camera* c) { c->Event_Moved(); });
        }

        void Event_ViewPointVisibilityChanged()
        {
            CameraCall([&](Camera* c) { c->Event_ViewPointVisibilityChanged(); });
        }

        void Call_UpdateVisibilityForOwner(UpdateDataMapType& update_players)
        {
            CameraCall([&](Camera* c) { c->UpdateVisibilityForOwner(update_players); });
        }
};

#endif
