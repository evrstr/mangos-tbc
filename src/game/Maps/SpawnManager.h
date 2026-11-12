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

#ifndef MANGOS_SPAWN_MANAGER_H
#define MANGOS_SPAWN_MANAGER_H

#include "Common.h"
#include "Entities/ObjectGuid.h"
#include "Maps/SpawnGroup.h"

#include <string>

class Map;

// 单个待刷新对象的调度信息（记录复活时间点、数据库 GUID、类型及使用状态）
class SpawnInfo
{
    public:
        SpawnInfo(TimePoint when, uint32 dbguid, HighGuid high) : m_respawnTime(when), m_dbguid(dbguid), m_high(high), m_used(false), m_inUse(false){}
        TimePoint const& GetRespawnTime() const { return m_respawnTime; }
        void SetRespawnTime(TimePoint const& time) { m_respawnTime = time; }
        bool ConstructForMap(Map& map); // can fail due to linking, pooling not supported (在地图上构造生成该对象；可能因生物链接失败，不支持对象池)
        uint32 GetDbGuid() const { return m_dbguid; }
        HighGuid GetHighGuid() const { return m_high; }
        void SetUsed() { m_used = true; }
        bool IsUsed() const { return m_inUse || m_used; }
    private:
        TimePoint m_respawnTime;
        uint32 m_dbguid;
        HighGuid m_high;
        bool m_used;
        bool m_inUse;
};

bool operator<(SpawnInfo const& lhs, SpawnInfo const& rhs);

// 地图刷新管理器，负责地图内生物/游戏对象的延迟刷新调度与刷怪组（SpawnGroup）管理
class SpawnManager
{
    public:
        SpawnManager(Map& map) : m_map(map), m_updated(false) {}
        ~SpawnManager();
        void Initialize(); // 初始化地图内的所有刷怪组并加载关联的生物与游戏对象

        void AddCreature(uint32 dbguid);
        void AddGameObject(uint32 dbguid);

        void RespawnCreature(uint32 dbguid, uint32 respawnDelay = 0); // seconds (单位：秒)
        void RespawnGameObject(uint32 dbguid, uint32 respawnDelay = 0); // seconds (单位：秒)

        void RemoveSpawns(std::vector<uint32> const& creatureDbGuids, std::vector<uint32> const& goDbGuids);
        void RemoveSpawn(uint32 dbguid, HighGuid high);

        // boilerplate methods for dynguid transition period
        // 动态 GUID 过渡期的样板方法：追踪事件激活的数据库 GUID
        void AddEventGuid(uint32 dbguid, HighGuid high);
        void RemoveEventGuid(uint32 dbguid, HighGuid high);
        bool IsEventGuid(uint32 dbguid, HighGuid high) const;

        void RespawnAll(); // 立即重生所有待刷新对象

        void Update(); // 每帧推进刷怪组与到期刷新队列

        std::string GetRespawnList();

        SpawnGroup* GetSpawnGroup(uint32 Id);
        SpawnGroup* GetSpawnGroup(std::string const& stringId);

        void RespawnSpawnGroupsInVicinity(Position pos, float range);
    private:
        Map& m_map;

        std::vector<SpawnInfo> m_deferredSpawns;
        std::vector<SpawnInfo> m_spawns; // must only be erased from in Update (仅允许在 Update 中移除元素)
        std::map<uint32, SpawnGroup*> m_spawnGroups;
        std::map<uint32, SpawnGroup*> m_spawnGroupsPerStringId;
        bool m_updated;

        std::set<uint32> m_eventCreatureDbGuids;
        std::set<uint32> m_eventGoDbGuids;
};

#endif