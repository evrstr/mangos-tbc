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

#ifndef MANGOS_OBJECTACCESSOR_H
#define MANGOS_OBJECTACCESSOR_H

#include "Common.h"
#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include "Policies/ThreadingModel.h"

#include "Entities/UpdateData.h"

#include "Maps/GridDefines.h"
#include "Entities/Object.h"
#include "Entities/Player.h"
#include "Entities/Corpse.h"

#include <functional>
#include <mutex>

class Unit;
class WorldObject;
class Map;

/// 线程安全的全局对象哈希表容器模板（用于索引全服 Player 与 Corpse 实例）
template <class T>
class HashMapHolder
{
    public:

        typedef std::unordered_map<ObjectGuid, T*>   MapType;
        typedef std::mutex LockType;
        typedef std::lock_guard<std::mutex> ReadGuard;
        typedef std::lock_guard<std::mutex> WriteGuard;

        static void Insert(T* o);

        static void Remove(T* o);

        static T* Find(ObjectGuid guid);

        static MapType& GetContainer();

        static LockType& GetLock();

    private:

        // Non instanceable only static | 仅提供静态方法，禁止实例化
        HashMapHolder() {}

        static LockType i_lock;
        static MapType  m_objectMap;
};

/// 全局对象访问器单例：提供跨地图玩家查找、全服玩家遍历及玩家尸体生命周期管理
class ObjectAccessor : public MaNGOS::Singleton<ObjectAccessor, MaNGOS::ClassLevelLockable<ObjectAccessor, std::mutex> >
{
        friend class MaNGOS::OperatorNew<ObjectAccessor>;

        ObjectAccessor();
        ~ObjectAccessor();
        ObjectAccessor(const ObjectAccessor&);
        ObjectAccessor& operator=(const ObjectAccessor&);

    public:
        typedef std::unordered_map<ObjectGuid, Corpse*> Player2CorpsesMapType;

        // Search player at any map in world and other objects at same map with `obj` | 跨全服查找玩家，或在 `u` 所在同一地图内查找其他单位
        // Note: recommended use Map::GetUnit version if player also expected at same map only | 注意：若仅限同地图查找，建议直接调用 Map::GetUnit
        static Unit* GetUnit(WorldObject const& u, ObjectGuid guid);

        // Player access | 全局玩家查找与批量操作接口
        static Player* FindPlayer(ObjectGuid guid, bool inWorld = true);// if need player at specific map better use Map::GetPlayer | 按 GUID 查找玩家（若需限定特定地图请使用 Map::GetPlayer）
        static Player* FindPlayerByName(const char* name);
        static void KickPlayer(ObjectGuid guid);

        HashMapHolder<Player>::MapType& GetPlayers() const
        {
            return HashMapHolder<Player>::GetContainer();
        }

        /// 向所有在线玩家所在地图投递存盘消息（离线转换期玩家直接存盘）
        void SaveAllPlayers() const;
        /// 对全服所有登记玩家执行回调操作（调用期间持有玩家容器锁）
        void ExecuteOnAllPlayers(std::function<void(Player*)> executor);

        // Corpse access | 玩家尸体管理接口
        Corpse* GetCorpseForPlayerGUID(ObjectGuid guid);
        static Corpse* GetCorpseInMap(ObjectGuid guid, uint32 mapid);
        void RemoveCorpse(Corpse* corpse);
        void AddCorpse(Corpse* corpse);
        void AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map);
        /// 将玩家可复活尸体转化为白骨或直接销毁
        Corpse* ConvertCorpseForPlayer(ObjectGuid player_guid, bool insignia = false);
        /// 清理全服所有已过期的玩家尸体
        void RemoveOldCorpses();

        // For call from Player/Corpse AddToWorld/RemoveFromWorld only | 仅供 Player/Corpse 在进入/离开世界时内部调用
        void AddObject(Corpse* object) { HashMapHolder<Corpse>::Insert(object); }
        void AddObject(Player* object) { HashMapHolder<Player>::Insert(object); }
        void RemoveObject(Corpse* object) { HashMapHolder<Corpse>::Remove(object); }
        void RemoveObject(Player* object) { HashMapHolder<Player>::Remove(object); }

    private:

        Player2CorpsesMapType   i_player2corpse;

        typedef std::mutex LockType;
        typedef MaNGOS::GeneralLock<LockType > Guard;

        LockType i_playerGuard;
        LockType i_corpseGuard;
};

#define sObjectAccessor ObjectAccessor::Instance()

#endif
