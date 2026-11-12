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

#ifndef __InstanceSaveMgr_H
#define __InstanceSaveMgr_H

#include "Common.h"
#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Server/DBCEnums.h"
#include "Server/DBCStores.h"
#include "Entities/ObjectGuid.h"
#include "Pools/PoolManager.h"

#include <list>
#include <map>
#include <mutex>

struct InstanceTemplate;
struct MapEntry;
struct GameObjectData;
struct CreatureData;

class Player;
class Group;
class Map;

#define NORMAL_INSTANCE_RESET_TIME 30 * MINUTE

typedef std::set<uint32> CellGuidSet;

// 单个 Cell 内动态附加的生物与游戏对象数据库 GUID 集合
struct MapCellObjectGuids
{
    CellGuidSet creatures;
    CellGuidSet gameobjects;
};

typedef std::unordered_map < uint32/*cell_id*/, MapCellObjectGuids > MapCellObjectGuidsMap;

class MapPersistentStateManager;

// 地图持久化状态基类：保存重生时间、动态对象池状态及副本跨加载期数据
class MapPersistentState
{
        friend class MapPersistentStateManager;
    protected:
        MapPersistentState(uint16 MapId, uint32 InstanceId, Difficulty difficulty);

    public:

        /* Unloaded when m_playerList and m_groupList become empty
           or when the instance is reset
           当绑定玩家列表与队伍列表均为空，或副本重置时卸载 */
        virtual ~MapPersistentState();

        /* A map corresponding to the InstanceId/MapId does not always exist.
        MapPersistentState objects may be created on player logon but the maps are
        created and loaded only when a player actually enters the instance.
        对应 InstanceId/MapId 的 Map 不一定始终存在。
        MapPersistentState 可在玩家登录时即创建，而实际 Map 仅在玩家真正进入副本时才创建加载。 */
        uint32 GetInstanceId() const { return m_instanceid; }
        uint32 GetMapId() const { return m_mapid; }

        MapEntry const* GetMapEntry() const;

        /* currently it is possible to omit this information from this structure
           but that would depend on a lot of things that can easily change in future
           目前理论上可省略难度字段，但会依赖许多未来易变的逻辑，故保留 */
        Difficulty GetDifficulty() const { return m_difficulty; }

        bool IsUsedByMap() const { return m_usedByMap != nullptr; }
        Map* GetMap() const { return m_usedByMap; }         // Can be nullptr if map not loaded for persistent state (若对应地图未加载则可为 nullptr)
        void SetUsedByMapState(Map* map)
        {
            m_usedByMap = map;
            if (!map)
                UnloadIfEmpty();
        }

        time_t GetCreatureRespawnTime(uint32 loguid) const
        {
            RespawnTimes::const_iterator itr = m_creatureRespawnTimes.find(loguid);
            return itr != m_creatureRespawnTimes.end() ? itr->second : 0;
        }
        void SaveCreatureRespawnTime(uint32 loguid, time_t t);
        time_t GetGORespawnTime(uint32 loguid) const
        {
            RespawnTimes::const_iterator itr = m_goRespawnTimes.find(loguid);
            return itr != m_goRespawnTimes.end() ? itr->second : 0;
        }
        void SaveGORespawnTime(uint32 loguid, time_t t);
        time_t GetObjectRespawnTime(uint32 typeId, uint32 loguid) const;
        void SaveObjectRespawnTime(uint32 typeId, uint32 loguid, time_t t);

        // pool system (对象池刷新状态管理)
        void InitPools();
        SpawnedPoolData& GetSpawnedPoolData() { return m_spawnedPoolData; };

        template<typename T>
        bool IsSpawnedPoolObject(uint32 db_guid_or_pool_id) { return GetSpawnedPoolData().IsSpawnedObject<T>(db_guid_or_pool_id); }

        // grid objects (Dynamic map/instance specific added/removed grid spawns from pool system/etc)
        // 网格对象数据（由对象池系统等按地图实例动态增删的网格刷怪 GUID）
        MapCellObjectGuids const& GetCellObjectGuids(uint32 cell_id) { return m_gridObjectGuids[cell_id]; }
        void AddCreatureToGrid(uint32 guid, CreatureData const* data);
        void RemoveCreatureFromGrid(uint32 guid, CreatureData const* data);
        void AddGameobjectToGrid(uint32 guid, GameObjectData const* data);
        void RemoveGameobjectFromGrid(uint32 guid, GameObjectData const* data);

        virtual uint32 GetCompletedEncountersMask() const { return 0; }
    protected:
        virtual bool CanBeUnload() const = 0;               // body provided for subclasses (由子类实现具体可卸载判定)

        bool UnloadIfEmpty();
        void ClearRespawnTimes();
        bool HasRespawnTimes() const { return !m_creatureRespawnTimes.empty() || !m_goRespawnTimes.empty(); }

    private:
        void SetCreatureRespawnTime(uint32 loguid, time_t t);
        void SetGORespawnTime(uint32 loguid, time_t t);

    private:
        typedef std::unordered_map<uint32, time_t> RespawnTimes;

        uint32 m_instanceid;
        uint32 m_mapid;
        Difficulty m_difficulty;
        Map* m_usedByMap;                                   // nullptr if map not loaded, non-nullptr lock MapPersistentState from unload (若地图未加载则为 nullptr；非空时锁定持久化状态防止卸载)

        // persistent data (持久化数据)
        RespawnTimes m_creatureRespawnTimes;                // lock MapPersistentState from unload, for example for temporary bound dungeon unload delay (生物重生时间表；非空时可延迟副本持久化状态卸载)
        RespawnTimes m_goRespawnTimes;                      // lock MapPersistentState from unload, for example for temporary bound dungeon unload delay (游戏对象重生时间表；非空时可延迟副本持久化状态卸载)
        MapCellObjectGuidsMap m_gridObjectGuids;            // Single map copy specific grid spawn data, like pool spawns (单张地图副本专属的网格刷新 GUID，如对象池刷出)

        SpawnedPoolData m_spawnedPoolData;                  // Pools spawns state for map copy (该地图副本的对象池当前激活状态)
};

inline bool MapPersistentState::CanBeUnload() const
{
    // prevent unload if used for loaded map (若正被已加载地图使用则阻止卸载)
    return !m_usedByMap;
}

// 世界地图（非副本）持久化状态类
class WorldPersistentState : public MapPersistentState
{
    public:
        /* Created either when:
           - any new non-instanceable map created
           - respawn data loading for non-instanceable map
           在创建非副本地图或为其加载重生数据时创建
        */
        explicit WorldPersistentState(uint16 MapId) : MapPersistentState(MapId, 0, REGULAR_DIFFICULTY) {}

        ~WorldPersistentState() {}
    protected:
        bool CanBeUnload() const override;                  // overwrite MapPersistentState::CanBeUnload (重写基类可卸载判定)
};

/*
    Holds the information necessary for creating a new map for an existing instance
    Is referenced in three cases:
    - player-instance binds for solo players (not in group)
    - player-instance binds for permanent heroic/raid saves
    - group-instance binds (both solo and permanent) cache the player binds for the group leader

    Used for InstanceMap only
    保存为已有副本创建新地图实例所需的全部状态信息，在以下三种场景中被引用：
    - 独狼玩家（未组队）的个人副本进度绑定
    - 英雄本/团本的永久副本进度绑定
    - 队伍副本绑定（缓存队长的副本进度绑定，含临时与永久）
    仅用于副本实例地图。
*/
class DungeonPersistentState : public MapPersistentState
{
    public:
        /* Created either when:
           - any new instance is being generated
           - the first time a player bound to InstanceId logs in
           - when a group bound to the instance is loaded
           在生成新副本、绑定玩家首次登录或加载绑定队伍时创建 */
        DungeonPersistentState(uint16 MapId, uint32 InstanceId, Difficulty difficulty, time_t resetTime, bool canReset, uint32 completedEncountersMask);

        ~DungeonPersistentState();

        InstanceTemplate const* GetTemplate() const;

        uint8 GetPlayerCount() const { return m_playerList.size(); }
        uint8 GetGroupCount() const { return m_groupList.size(); }

        /* online players bound to the instance (perm/solo)
           does not include the members of the group unless they have permanent saves
           绑定到此副本的在线玩家（永久或单人进度；不含仅随队但无永久进度的队员） */
        void AddPlayer(Player* player) { m_playerList.push_back(player); }
        bool RemovePlayer(Player* player) { m_playerList.remove(player); return UnloadIfEmpty(); }
        /* all groups bound to the instance (绑定到此副本的所有队伍) */
        void AddGroup(Group* group) { m_groupList.push_back(group); }
        bool RemoveGroup(Group* group) { m_groupList.remove(group); return UnloadIfEmpty(); }

        /* for normal instances this corresponds to max(creature respawn time) + 30 minutes
           for raid/heroic instances this caches the global respawn time for the map
           普通副本对应 max(生物重生时间) + 30 分钟；团本/英雄本则缓存全服统一的重置时间 */
        time_t GetResetTime() const { return m_resetTime; }
        void SetResetTime(time_t resetTime) { m_resetTime = resetTime; }
        time_t GetResetTimeForDB() const;

        /* instances cannot be reset (except at the global reset time)
           if there are players permanently bound to it
           this is cached for the case when those players are offline
           若有玩家永久绑定该副本，则除全局重置外不可手动重置；该标志在玩家离线时缓存状态 */
        bool CanReset() const { return m_canReset; }
        void SetCanReset(bool canReset) { m_canReset = canReset; }

        // DBC encounter state update at kill/spell cast (击杀首领或施放法术时更新 DBC 遭遇战进度掩码)
        void UpdateEncounterState(EncounterCreditType type, uint32 creditEntry);

        // mask of completed encounters (已完成的遭遇战掩码)
        uint32 GetCompletedEncountersMask() const override { return m_completedEncountersMask; }

        /* Saved when the instance is generated for the first time (首次生成副本时保存到数据库) */
        void SaveToDB();
        /* When the instance is being reset (permanently deleted) (副本重置永久删除时从数据库移除) */
        void DeleteFromDB() const;
        /* Delete respawn data at dungeon reset (副本重置时清空重生时间记录) */
        void DeleteRespawnTimes();
        /* Remove players bind to this state (解除所有玩家和队伍对此状态的绑定) */
        void UnbindThisState();

    protected:
        bool CanBeUnload() const override;                  // overwrite MapPersistentState::CanBeUnload (重写基类可卸载判定)
        bool HasBounds() const { return !m_playerList.empty() || !m_groupList.empty(); }

    private:
        typedef PlayerList PlayerListType;
        typedef std::list<Group*> GroupListType;

        time_t m_resetTime;
        bool m_canReset;

        /* the only reason the instSave-object links are kept is because
           the object-instSave links need to be broken at reset time
           TODO: maybe it's enough to just store the number of players/groups
           保留绑定对象引用的唯一原因是在重置时需断开对象与存档的关联；
           待办：或许仅记录玩家/队伍数量即可 */
        PlayerListType m_playerList;                        // lock MapPersistentState from unload (锁定持久化状态防止卸载)
        GroupListType m_groupList;                          // lock MapPersistentState from unload (锁定持久化状态防止卸载)

        uint32 m_completedEncountersMask;                   // completed encounter mask, bit indexes are DungeonEncounter.dbc boss numbers, used for packets (已完成遭遇战掩码，位索引对应 DungeonEncounter.dbc 首领编号，用于封包下发)
};

// 战场/竞技场持久化状态类
class BattleGroundPersistentState : public MapPersistentState
{
    public:
        /* Created either when:
           - any new BG/arena is being generated
           在生成新的战场或竞技场实例时创建
        */
        BattleGroundPersistentState(uint16 MapId, uint32 InstanceId, Difficulty difficulty)
            : MapPersistentState(MapId, InstanceId, difficulty) {}

        ~BattleGroundPersistentState() {}
    protected:
        bool CanBeUnload() const override;                  // overwrite MapPersistentState::CanBeUnload (重写基类可卸载判定)
};

// 副本重置预警与执行事件类型
enum ResetEventType
{
    RESET_EVENT_NORMAL_DUNGEON      = 0,                    // no fixed reset time (普通地下城，无固定全球重置时间)
    RESET_EVENT_INFORM_1            = 1,                    // raid/heroic warnings (团本/英雄本重置预警 1~4)
    RESET_EVENT_INFORM_2            = 2,
    RESET_EVENT_INFORM_3            = 3,
    RESET_EVENT_INFORM_LAST         = 4,
    RESET_EVENT_FORCED_INFORM_1     = 5,
    RESET_EVENT_FORCED_INFORM_2     = 6,
    RESET_EVENT_FORCED_INFORM_3     = 7,
    RESET_EVENT_FORCED_INFORM_LAST  = 8,
};

#define MAX_RESET_EVENT_TYPE   9

/* resetTime is a global propery of each (raid/heroic) map
    all instances of that map reset at the same time
    resetTime 是每张团本/英雄本地图的全局属性，同地图所有实例在同一时刻重置 */
struct DungeonResetEvent
{
    ResetEventType type   : 8;                              // if RESET_EVENT_DUNGEON then InstanceID == 0 and applied to all instances for map) (若为全球重置事件则 InstanceID == 0，作用于该地图所有实例)
    uint16 mapid;                                           // used with mapid used as for select reset for global cooldown instances (instanceid==0 for event) (目标地图 ID)
    uint32 instanceId;                                      // used for select reset for normal dungeons (普通地下城重置时使用的具体实例 ID)

    DungeonResetEvent() : type(RESET_EVENT_NORMAL_DUNGEON), mapid(0), instanceId(0) {}
    DungeonResetEvent(ResetEventType t, uint32 _mapid, uint32 _instanceid)
        : type(t), mapid(_mapid), instanceId(_instanceid) {}

    bool operator == (const DungeonResetEvent& e) const { return e.mapid == mapid && e.instanceId == instanceId; }
};

// 副本重置调度器：维护各副本全局重置时间表与按时间排序的预警/重置事件队列
class DungeonResetScheduler
{
    public:                                                 // constructors
        explicit DungeonResetScheduler(MapPersistentStateManager& mgr) : m_InstanceSaves(mgr) {}
        void LoadResetTimes();

    public:                                                 // accessors
        time_t GetResetTimeFor(uint32 mapid) { return m_resetTimeByMapId[mapid]; }

        static uint32 GetMaxResetTimeFor(InstanceTemplate const* temp);
        static time_t CalculateNextResetTime(InstanceTemplate const* temp, time_t prevResetTime);
    public:                                                 // modifiers
        void SetResetTimeFor(uint32 mapid, time_t t)
        {
            m_resetTimeByMapId[mapid] = t;
        }

        void ScheduleReset(bool add, time_t time, DungeonResetEvent event);

        void Update();

        void ResetAllRaid();
    private:                                                // fields
        MapPersistentStateManager& m_InstanceSaves;

        // fast lookup for reset times (always use existing functions for access/set) (按地图 ID 快速索引重置时间)
        typedef std::vector < time_t /*resetTime*/ > ResetTimeVector;
        ResetTimeVector m_resetTimeByMapId;

        typedef std::multimap < time_t /*resetTime*/, DungeonResetEvent > ResetTimeQueue;
        ResetTimeQueue m_resetTimeQueue;
};

// 地图持久化状态管理器单例：统一管理世界地图与副本实例存档、数据库清理与重置调度
class MapPersistentStateManager : public MaNGOS::Singleton<MapPersistentStateManager, MaNGOS::ClassLevelLockable<MapPersistentStateManager, std::mutex> >
{
        friend class DungeonResetScheduler;
    public:                                                 // constructors
        MapPersistentStateManager();
        ~MapPersistentStateManager();

    public:                                                 // common for all MapPersistentState (sub)classes
        // For proper work pool systems with shared pools state for non-instanceable maps need
        // load persistent map states for any non-instanceable maps before init pool system
        // 为使非副本地图的共享对象池状态正常运作，需在初始化对象池系统前先加载所有非副本地图持久化状态
        void InitWorldMaps();
        void LoadCreatureRespawnTimes();
        void LoadGameobjectRespawnTimes();

        // auto select appropriate MapPersistentState (sub)class by MapEntry, and autoselect appropriate way store (by instance/map id)
        // always return != nullptr
        // 根据 MapEntry 自动选择对应的 MapPersistentState 子类并按 instanceId 或 mapId 存入映射表，返回值永不为空
        MapPersistentState* AddPersistentState(MapEntry const* mapEntry, uint32 instanceId, Difficulty difficulty, time_t resetTime, bool canReset, bool load = false, uint32 completedEncountersMask = 0);

        // search stored state, can be nullptr in result (查找已保存的状态，未找到则返回 nullptr)
        MapPersistentState* GetPersistentState(uint32 mapId, uint32 instanceId);

        void RemovePersistentState(uint32 mapId, uint32 instanceId);

        template<typename Do>
        void DoForAllStatesWithMapId(uint32 mapId, Do& _do);

    public:                                                 // DungeonPersistentState specific (副本持久化状态专用接口)
        void CleanupInstances();
        void PackInstances() const;

        DungeonResetScheduler& GetScheduler() { return m_Scheduler; }

        static void DeleteInstanceFromDB(uint32 instanceid);

        void GetStatistics(uint32& numStates, uint32& numBoundPlayers, uint32& numBoundGroups);

        void Update() { m_Scheduler.Update(); }
    private:
        typedef std::unordered_map < uint32 /*InstanceId or MapId*/, MapPersistentState* > PersistentStateMap;

        //  called by scheduler for DungeonPersistentStates (由调度器回调，执行预警广播、实例重置与过期清理)
        void _ResetOrWarnAll(uint32 mapid, bool warn, uint32 timeLeft);
        void _ResetInstance(uint32 mapid, uint32 instanceId);
        void _CleanupExpiredInstancesAtTime(time_t t);

        void _ResetSave(PersistentStateMap& holder, PersistentStateMap::iterator& itr);
        void _DelHelper(DatabaseType& db, const char* fields, const char* table, const char* queryTail, ...) const;

        // used during global instance resets (全局副本重置期间锁定列表防止迭代器失效)
        bool lock_instLists;
        // fast lookup by instance id for instanceable maps (副本地图按 instanceId 快速索引)
        PersistentStateMap m_instanceSaveByInstanceId;
        // fast lookup by map id for non-instanceable maps (非副本地图按 mapId 快速索引)
        PersistentStateMap m_instanceSaveByMapId;

        DungeonResetScheduler m_Scheduler;
};

template<typename Do>
inline void MapPersistentStateManager::DoForAllStatesWithMapId(uint32 mapId, Do& _do)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    if (!mapEntry)
        return;

    if (mapEntry->Instanceable())
    {
        for (PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.begin(); itr != m_instanceSaveByInstanceId.end();)
        {
            if (itr->second->GetMapId() == mapId)
                _do((itr++)->second);
            else
                ++itr;
        }
    }
    else
    {
        if (MapPersistentState* state = GetPersistentState(mapId, 0))
            _do(state);
    }
}

#define sMapPersistentStateMgr MaNGOS::Singleton<MapPersistentStateManager>::Instance()
#endif
