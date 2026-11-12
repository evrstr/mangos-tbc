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

#ifndef MANGOS_MAP_H
#define MANGOS_MAP_H

#include "Common.h"
#include "Platform/Define.h"
#include "Policies/ThreadingModel.h"

#include "Server/DBCStructure.h"
#include "Maps/GridDefines.h"
#include "Grids/Cell.h"
#include "Entities/Object.h"
#include "Globals/SharedDefines.h"
#include "Maps/GridMap.h"
#include "GameSystem/GridRefManager.h"
#include "MapRefManager.h"
#include "DBScripts/ScriptMgr.h"
#include "Entities/CreatureLinkingMgr.h"
#include "vmap/DynamicTree.h"
#include "Multithreading/Messager.h"
#include "Globals/GraveyardManager.h"
#include "Maps/SpawnManager.h"
#include "Maps/MapDataContainer.h"
#include "Maps/MapGuidAllocator.h"
#include "Util/UniqueTrackablePtr.h"
#include "World/WorldStateVariableManager.h"

#include <bitset>
#include <functional>
#include <list>

struct CreatureInfo;
class Creature;
class Unit;
class WorldPacket;
class InstanceData;
class Group;
class MapPersistentState;
class WorldPersistentState;
class DungeonPersistentState;
class BattleGroundPersistentState;
struct ScriptInfo;
class BattleGround;
class GridMap;
class GameObjectModel;
class WeatherSystem;
class GenericTransport;
namespace MaNGOS { struct ObjectUpdater; }
class Transport;

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
// GCC 使用不同的 #pragma pack(N) 语法，旧版 GCC 不支持 pack(push, N)
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

// 副本模板信息，定义副本入口父地图、等级限制、最大人数、重置周期与脚本 ID
struct InstanceTemplate
{
    uint32 map;                                             // instance map (副本地图 ID)
    uint32 parent;                                          // non-continent parent instance (for instance with entrance in another instances) (非大陆父副本 ID，用于入口位于另一副本内的情况；0 表示与大陆 0 无关联)
    // or 0 (not related to continent 0 map id)
    uint32 levelMin;
    uint32 levelMax;
    uint32 maxPlayers;
    uint32 reset_delay;                                     // in days (重置间隔天数)
    uint32 script_id;
    bool   mountAllowed;
};

// 世界（非副本）地图模板信息
struct WorldTemplate
{
    uint32 map;                                             // non-instance map (非副本地图 ID)
    uint32 script_id;
};

// 英雄难度等级要求枚举
enum LevelRequirementVsMode
{
    LEVELREQUIREMENT_HEROIC = 70
};

// 经验值倍率修饰类型
enum class RateModType
{
    KILL    = 0,
    QUEST   = 1,
    EXPLORE = 2,
    PETKILL = 3,
    MAX     = 4
};

// 区域动态环境信息（背景音乐、天气与光照覆盖）
struct ZoneDynamicInfo
{
    ZoneDynamicInfo() : musicId(0), weatherId(0), weatherGrade(0.0f),
        overrideLightId(0), lightFadeInTime(0) { }

    uint32 musicId;
    uint32 weatherId;
    float  weatherGrade;
    uint32 overrideLightId;
    uint32 lightFadeInTime;
};

#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

#define MIN_UNLOAD_DELAY      1                             // immediate unload
#define UPDATE_TICK         400

typedef std::unordered_map<uint32 /*zoneId*/, ZoneDynamicInfo> ZoneDynamicInfoMap;

// 地图核心基类：管理网格加载/卸载、对象增删与更新、视野同步、广播、脚本调度及动态碰撞树
class Map : public GridRefManager<NGridType>
{
        friend class MapReference;
        friend class ObjectGridLoader;
        friend class ObjectWorldLoader;

    protected:
        Map(uint32 id, time_t, uint32 InstanceId, uint8 SpawnMode);

    public:
        virtual ~Map();

        // currently unused for normal maps (普通地图当前未使用该延迟卸载判定)
        bool CanUnload(uint32 diff)
        {
            if (!m_unloadTimer) return false;
            if (m_unloadTimer <= diff) return true;
            m_unloadTimer -= diff;
            return false;
        }

        virtual void Initialize(std::mutex* mmapMutex, bool loadInstanceData = true);

        virtual bool Add(Player*);
        virtual void Remove(Player*, bool);
        template<class T> void Add(T*);
        template<class T> void Remove(T*, bool);

        static void DeleteFromWorld(Player* pl);        // player object will deleted at call (从世界删除并释放玩家对象)

        void VisitNearbyCellsOf(WorldObject* obj, TypeContainerVisitor<MaNGOS::ObjectUpdater, GridTypeMapContainer> &gridVisitor, TypeContainerVisitor<MaNGOS::ObjectUpdater, WorldTypeMapContainer> &worldVisitor); // 访问对象附近活动 Cell 内的网格/世界对象
        virtual void Update(const uint32&); // 地图主更新循环（处理异步消息、刷怪、脚本、对象更新、视野与客户端同步）

        uint64 PerformObjectUpdate(uint32 t_diff, WorldObjectUnSet& objToUpdate);

        // 消息广播与区域玩家回调遍历方法
        void MessageBroadcast(Player const*, WorldPacket const&, bool to_self);
        void MessageBroadcast(WorldObject const*, WorldPacket const&);
        void MessageDistBroadcast(Player const*, WorldPacket const&, float dist, bool to_self, bool own_team_only = false);
        void MessageDistBroadcast(WorldObject const*, WorldPacket const&, float dist);
        void MessageMapBroadcast(WorldObject const* obj, WorldPacket const& msg);
        void MessageMapBroadcastZone(WorldObject const* obj, WorldPacket const& msg, uint32 zoneId);
        void MessageMapBroadcastArea(WorldObject const* obj, WorldPacket const& msg, uint32 areaId);

        void ExecuteDistWorker(WorldObject const* obj, float dist, std::function<void(Player*)> const& worker);
        void ExecuteMapWorker(std::function<void(Player*)> const& worker);
        void ExecuteMapWorkerZone(uint32 zoneId, std::function<void(Player*)> const& worker);
        void ExecuteMapWorkerArea(uint32 areaId, std::function<void(Player*)> const& worker);

        float GetVisibilityDistance() const { return m_VisibleDistance; }
        // function for setting up visibility distance for maps on per-type/per-Id basis
        // 按地图类型或地图 ID 初始化默认视野距离
        virtual void InitVisibilityDistance();
        void VisiblityDistanceChanged(WorldObject* obj, float oldVisibility, VisibilityDistanceType newVisiblity);

        // 玩家、生物及游戏对象的移动与 Cell 重定位
        void PlayerRelocation(Player*, float x, float y, float z, float orientation);
        void CreatureRelocation(Creature* creature, float x, float y, float z, float ang);
        void GameObjectRelocation(GameObject* go, float x, float y, float z, float orientation, bool respawnRelocationOnFail = true);

        template<class T, class CONTAINER> void Visit(const Cell& cell, TypeContainerVisitor<T, CONTAINER>& visitor);

        bool IsRemovalGrid(float x, float y) const
        {
            GridPair p = MaNGOS::ComputeGridPair(x, y);
            return (!getNGrid(p.x_coord, p.y_coord) || getNGrid(p.x_coord, p.y_coord)->GetGridState() == GRID_STATE_REMOVAL);
        }

        bool IsLoaded(float x, float y) const
        {
            GridPair p = MaNGOS::ComputeGridPair(x, y);
            return loaded(p);
        }

        bool GetUnloadLock(const GridPair& p) const { return getNGrid(p.x_coord, p.y_coord)->getUnloadLock(); }
        void SetUnloadLock(const GridPair& p, bool on) { getNGrid(p.x_coord, p.y_coord)->setUnloadExplicitLock(on); }
        void ForceLoadGrid(float x, float y);
        void LoadGrid(float x, float y);
        bool UnloadGrid(const uint32& x, const uint32& y, bool pForce);
        virtual void UnloadAll(bool pForce);

        void ResetGridExpiry(NGridType& grid, float factor = 1) const
        {
            grid.ResetTimeTracker((time_t)((double)i_gridExpiry * factor));
        }

        time_t GetGridExpiry(void) const { return i_gridExpiry; }
        uint32 GetId(void) const { return i_id; }

        // some calls like isInWater should not use vmaps due to processor power
        // can return INVALID_HEIGHT if under z+2 z coord not found height
        // 某些调用（如 isInWater）因性能开销不应使用 VMap；若 z+2 以下未找到高度则可能返回 INVALID_HEIGHT

        virtual void RemoveAllObjectsInRemoveList();

        bool CreatureRespawnRelocation(Creature* c);        // used only in CreatureRelocation and ObjectGridUnloader (仅在生物重定位与网格卸载时将生物移回出生点)

        uint32 GetInstanceId() const { return i_InstanceId; }

        MaNGOS::unique_weak_ptr<Map> GetWeakPtr() const { return m_weakRef; }
        void SetWeakPtr(MaNGOS::unique_weak_ptr<Map> weakRef) { m_weakRef = std::move(weakRef); }

        virtual bool CanEnter(Player* player);
        const char* GetMapName() const;

        // _currently_ spawnmode == difficulty, but this can be changes later, so use appropriate spawnmode/difficult functions
        // for simplify later code support
        // regular difficulty = continent/dungeon normal/raid normal difficulty
        // 当前 spawnMode 等价于 difficulty，但后续可能调整，请使用封装函数以便兼容扩展
        // 普通难度对应大陆地图、普通地下城与普通团队副本
        uint8 GetSpawnMode() const { return (i_spawnMode); }
        Difficulty GetDifficulty() const { return Difficulty(GetSpawnMode()); }
        bool IsRegularDifficulty() const { return GetDifficulty() == REGULAR_DIFFICULTY; }
        uint32 GetMaxPlayers() const;
        uint32 GetMaxResetDelay() const;

        uint32 GetExpansion() const { return (i_mapEntry) ? i_mapEntry->Expansion() : 0u; }
        float GetXPModRate(RateModType type) const;

        MapEntry const* GetEntry() const { return i_mapEntry; }
        bool Instanceable() const { return i_mapEntry && i_mapEntry->Instanceable(); }
        bool IsDungeon() const { return i_mapEntry && i_mapEntry->IsDungeon(); }
        bool IsRaid() const { return i_mapEntry && i_mapEntry->IsRaid(); }
        bool IsNoRaid() const { return i_mapEntry && i_mapEntry->IsNonRaidDungeon(); }
        bool IsRaidOrHeroicDungeon() const { return IsRaid() || GetDifficulty() > DUNGEON_DIFFICULTY_NORMAL; }
        bool IsBattleGround() const { return i_mapEntry && i_mapEntry->IsBattleGround(); }
        bool IsBattleArena() const { return i_mapEntry && i_mapEntry->IsBattleArena(); }
        bool IsBattleGroundOrArena() const { return i_mapEntry && i_mapEntry->IsBattleGroundOrArena(); }
        bool IsContinent() const { return i_mapEntry && i_mapEntry->IsContinent(); }
        bool IsMountAllowed() const;
        bool IsDynguidForced() const;

        // can't be nullptr for loaded map (已加载地图的持久化状态指针不可为空)
        MapPersistentState* GetPersistentState() const { return m_persistentState; }

        void AddObjectToRemoveList(WorldObject* obj);
        void RemoveObjectFromRemoveList(WorldObject* obj);
        bool IsInRemoveList(WorldObject* obj) const;

        void UpdateObjectVisibility(WorldObject* obj, Cell cell, const CellPair& cellpair);

        void resetMarkedCells() { marked_cells.reset(); }
        bool isCellMarked(uint32 pCellId) const { return marked_cells.test(pCellId); }
        void markCell(uint32 pCellId) { marked_cells.set(pCellId); }

        bool HavePlayers() const { return !m_mapRefManager.isEmpty(); }
        uint32 GetPlayersCountExceptGMs() const;
        bool ActiveObjectsNearGrid(uint32 x, uint32 y) const;

        /// Send a Packet to all players on a map (向地图内所有玩家发送数据包)
        void SendToPlayers(WorldPacket const& data) const;
        /// Send a Packet to all players in a zone. Return false if no player found (向指定区域内所有玩家发送数据包，若无玩家返回 false)
        bool SendToPlayersInZone(WorldPacket const& data, uint32 zoneId) const;

        typedef MapRefManager PlayerList;
        PlayerList const& GetPlayers() const { return m_mapRefManager; }

        // per-map script storage (地图独立脚本调度与去重执行参数)
        enum ScriptExecutionParam
        {
            SCRIPT_EXEC_PARAM_NONE                    = 0x00,   // Start regardless if already started (无论是否已启动均直接执行)
            SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE        = 0x01,   // Start Script only if not yet started (uniqueness identified by id and source) (仅当同 id 与 source 的脚本未运行时启动)
            SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET        = 0x02,   // Start Script only if not yet started (uniqueness identified by id and target) (仅当同 id 与 target 的脚本未运行时启动)
            SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE_TARGET = 0x03,   // Start Script only if not yet started (uniqueness identified by id, source and target) (仅当同 id、source 与 target 的脚本未运行时启动)
        };
        bool ScriptsStart(ScriptMapType scriptType, uint32 id, Object* source, Object* target, ScriptExecutionParam execParams = SCRIPT_EXEC_PARAM_NONE);
        void ScriptCommandStart(ScriptInfo const& script, uint32 delay, Object* source, Object* target);

        // must called with AddToWorld (必须配合 AddToWorld 调用，加入活跃非玩家对象集合)
        void AddToActive(WorldObject* obj);
        // must called with RemoveFromWorld (必须配合 RemoveFromWorld 调用，移出活跃非玩家对象集合)
        void RemoveFromActive(WorldObject* obj);

        // Game Event notification system (游戏事件通知订阅系统)
        void AddToOnEventNotified(WorldObject* obj);
        void RemoveFromOnEventNotified(WorldObject* obj);
        void OnEventHappened(uint16 event_id, bool activate, bool resume);

        Player* GetPlayer(ObjectGuid guid);
        Creature* GetCreature(ObjectGuid guid);
        Creature* GetCreatureByEntry(uint32 entry);
        Pet* GetPet(ObjectGuid guid);
        Creature* GetAnyTypeCreature(ObjectGuid guid);      // normal creature or pet (普通生物或宠物)
        GameObject* GetGameObject(ObjectGuid guid);
        DynamicObject* GetDynamicObject(ObjectGuid guid);
        Corpse* GetCorpse(ObjectGuid guid) const;                 // !!! find corpse can be not in world (注意：查找到的尸体可能已不在世界中)
        Unit* GetUnit(ObjectGuid guid);                     // only use if sure that need objects at current map, specially for player case (仅在确认需要当前地图内对象时使用，尤其针对玩家)
        WorldObject* GetWorldObject(ObjectGuid guid);       // only use if sure that need objects at current map, specially for player case (仅在确认需要当前地图内对象时使用，尤其针对玩家)
        // dbguid methods (基于数据库 GUID 或 StringId 的对象查找接口)
        Creature* GetCreature(uint32 dbguid) const;
        GameObject* GetGameObject(uint32 dbguid) const;
        std::vector<WorldObject*> const* GetWorldObjects(std::string stringId) const;
        std::vector<Creature*> const* GetCreatures(std::string stringId) const;
        std::vector<GameObject*> const* GetGameObjects(std::string stringId) const;
        std::vector<WorldObject*> const* GetWorldObjects(uint32 stringId) const;
        std::vector<Creature*> const* GetCreatures(uint32 stringId) const;
        std::vector<GameObject*> const* GetGameObjects(uint32 stringId) const;
        WorldObject* GetWorldObject(std::string stringId) const;
        Creature* GetCreature(std::string stringId) const;
        GameObject* GetGameObject(std::string stringId) const;

        void AddDbGuidObject(WorldObject* obj);
        void RemoveDbGuidObject(WorldObject* obj);

        void AddStringIdObject(uint32 stringId, WorldObject* obj);
        void RemoveStringIdObject(uint32 stringId, WorldObject* obj);

        typedef TypeUnorderedMapContainer<AllMapStoredObjectTypes, ObjectGuid> MapStoredObjectTypesContainer;
        MapStoredObjectTypesContainer& GetObjectsStore() { return m_objectsStore; }
        std::map<uint32, uint32>& GetTempCreatures() { return m_tempCreatures; }
        std::map<uint32, uint32>& GetTempPets() { return m_tempPets; }
        
        // schedule for update object create change (加入客户端创建对象更新队列)
        void AddUpdateCreateObject(Object* obj) { m_objectsToClientCreateUpdate.insert({ obj, obj->GetObjectGuid() }); }
        void RemoveUpdateCreateObject(Object* obj) { m_objectsToClientCreateUpdate.erase({ obj, obj->GetObjectGuid() }); }
        // schedule for update object values change (加入客户端属性数值更新队列)
        void AddUpdateObject(Object* obj) { m_objectsToClientUpdate.insert(obj); }
        void RemoveUpdateObject(Object* obj) { m_objectsToClientUpdate.erase(obj); }
        // schedule for update object visibility change (加入客户端移动状态更新队列)
        void AddUpdateMovementObject(Object* obj) { m_objectsToClientMovementUpdate.insert(obj); }
        void RemoveUpdateMovementObject(Object* obj) { m_objectsToClientMovementUpdate.erase(obj); }
        // schedule update object destruction of object (加入客户端销毁对象通知队列)
        void AddUpdateRemoveObject(GuidSet& visible, ObjectGuid guid);
        void AddUpdateRemoveObject(GuidSet&& visible, ObjectGuid guid);

        void AddCreateAtClientObject(Player* player, Object* obj);
        void AddCreateAtClientObjects(PlayerSet const& players, Object* obj);

        void AddCameraToWorld(WorldObject* obj);

        // entry: the one written in the guid (the pet number for pets), not used for dynamic objects
        // Stops the server if no counter is free for this entry after a full round (see MapGuidAllocator)
        // entry：写入 GUID 的条目编号（宠物为其编号），动态对象不使用；
        // 若完整遍历一轮后该条目仍无可用计数器则会停止服务器（详见 MapGuidAllocator）
        uint32 GenerateLocalLowGuid(HighGuid guidhigh, uint32 entry = 0);

        // get corresponding TerrainData object for this particular map (获取当前地图共享的地形数据对象)
        const TerrainInfo* GetTerrain() const { return m_TerrainData; }

        void CreateInstanceData(bool load);
        InstanceData* GetInstanceData() const { return i_data; }
        uint32 GetScriptId() const { return i_script_id; }

        void MonsterYellToMap(ObjectGuid guid, int32 textId, ChatMsg chatMsg, Language language, Unit const* target) const;
        void MonsterYellToMap(CreatureInfo const* cinfo, int32 textId, ChatMsg chatMsg, Language language, Unit const* target, uint32 senderLowGuid = 0) const;
        void PlayDirectSoundToMap(uint32 soundId, uint32 zoneId = 0) const;

        // Dynamic VMaps (动态 VMap 高度、视线与碰撞检测接口)
        float GetHeight(float x, float y, float z, bool swim = false) const;
        bool GetHeightInRange(float x, float y, float& z, float maxSearchDist = 4.0f) const;
        bool IsInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2, bool ignoreM2Model) const;
        bool GetHitPosition(float srcX, float srcY, float srcZ, float& destX, float& destY, float& destZ, float modifyDist) const;

        // Object Model insertion/remove/test for dynamic vmaps use (向动态 VMap 碰撞树插入/移除/查询游戏对象模型)
        void InsertGameObjectModel(const GameObjectModel& mdl);
        void RemoveGameObjectModel(const GameObjectModel& mdl);
        bool ContainsGameObjectModel(const GameObjectModel& mdl) const;

        // Get Holder for Creature Linking (获取生物链接关系管理器)
        CreatureLinkingHolder* GetCreatureLinkingHolder() { return &m_creatureLinkingHolder; }

        // Teleport all players in that map to choosed location (将地图内所有玩家传送到指定位置)
        void TeleportAllPlayersTo(TeleportLocation loc);

        // WeatherSystem (天气系统：设置指定区域的天气类型、强度及是否永久)
        WeatherSystem* GetWeatherSystem() const { return m_weatherSystem; }
        /** Set the weather in a zone on this map
         * @param zoneId set the weather for which zone
         * @param type What weather to set
         * @param grade how strong the weather should be
         * @param permanently set the weather permanently?
         */
        void SetWeather(uint32 zoneId, WeatherType type, float grade, bool permanently);

        // Random on map generation (地图随机可达点/地面点/空中点/水下点生成)
        bool GetReachableRandomPosition(Unit* unit, float& x, float& y, float& z, float radius, bool randomRange = true) const;
        bool GetReachableRandomPointOnGround(float& x, float& y, float& z, float radius, bool randomRange = true) const;
        bool GetRandomPointInTheAir(float& x, float& y, float& z, float radius, bool randomRange = true) const;
        bool GetRandomPointUnderWater(float& x, float& y, float& z, float radius, GridMapLiquidData& liquid_status, bool randomRange = true) const;

        uint32 SpawnedCountForEntry(uint32 entry);
        void AddToSpawnCount(const ObjectGuid& guid);
        void RemoveFromSpawnCount(const ObjectGuid& guid);

        uint32 GetCurrentMSTime() const;
        TimePoint GetCurrentClockTime() const;
        uint32 GetCurrentDiff() const;
        time_t GetCurrentTime_t() const;
        tm GetCurrentTime_tm() const;

        void SetZoneMusic(uint32 zoneId, uint32 areaId, uint32 musicId);
        void SetZoneWeather(uint32 zoneId, uint32 areaId, uint32 weatherId, float weatherGrade);
        void SetZoneOverrideLight(uint32 zoneId, uint32 areaId, uint32 lightId, uint32 fadeInTime);
        void SendZoneDynamicInfo(Player* player, bool zoneUpdated, bool areaUpdated) const;

        void CreatePlayerOnClient(Player* player);

        uint32 GetLoadedGridsCount();

        Messager<Map>& GetMessager() { return m_messager; }

        typedef std::set<Transport*> TransportSet;
        GenericTransport* GetTransport(ObjectGuid guid);
        TransportSet const& GetTransports() { return m_transports; }

        GraveyardManager& GetGraveyardManager() { return m_graveyardManager; }

        void AddTransport(Transport* transport);
        void RemoveTransport(Transport* transport);

        bool CanSpawn(TypeID typeId, uint32 dbGuid);

        SpawnManager& GetSpawnManager() { return m_spawnManager; }

        MapDataContainer& GetMapDataContainer() { return m_dataContainer; }
        MapDataContainer const& GetMapDataContainer() const { return m_dataContainer; }
        WorldStateVariableManager& GetVariableManager() { return m_variableManager; }
        WorldStateVariableManager const& GetVariableManager() const { return m_variableManager; }

        virtual BattleGround* GetBG() const { return nullptr; }

        // debug (调试用对象移除追踪列表，最终可能占用过多内存，仅用于排查 VisibleNotifier::Notify() 泄漏)
        std::set<ObjectGuid> m_objRemoveList; // this will eventually eat up too much memory - only used for debugging VisibleNotifier::Notify() customlog leak

#ifdef ENABLE_PLAYERBOTS
        bool HasRealPlayers() { return hasRealPlayers; }
        bool HasActiveZones() { return !m_activeZones.empty(); }
        bool HasActiveZone(uint32 zoneId) { return find(m_activeZones.begin(), m_activeZones.end(), zoneId) != m_activeZones.end(); }
#endif

        bool IsUpdateObjectTick() const;
        bool IsStealthTick() const;

        void AddWaypointingNpc(Unit* npc);
        void RemoveWaypointingNpc(Unit* npc);

        void UpdateInfinite(Player& player, UpdateData& updateData, GuidSet& clientGUIDs, WorldObjectSet& visibleNow) const;
    private:
        void LoadMapAndVMap(int gx, int gy);

        void SetTimer(uint32 t) { i_gridExpiry = t < MIN_GRID_DELAY ? MIN_GRID_DELAY : t; }

        void SendInitBeforeGrid(Player* player, UpdateData& updateData) const;
        void SendInitSelf(Player* player, UpdateData& updateData) const;

        void SendInitInfiniteObjects(Player* player, UpdateData& updateData) const;
        void SendRemoveInfinite(Player* player) const;
        void LoadTransports();

        bool CreatureCellRelocation(Creature* c, const Cell& new_cell);

        bool loaded(const GridPair&) const;
        void EnsureGridCreated(const GridPair&);
        bool EnsureGridLoaded(Cell const&);
        void EnsureGridLoadedAtEnter(Cell const&, Player* player = nullptr);

        void buildNGridLinkage(NGridType* pNGridType) { pNGridType->link(this); }

        NGridType* getNGrid(uint32 x, uint32 y) const
        {
            MANGOS_ASSERT(x < MAX_NUMBER_OF_GRIDS);
            MANGOS_ASSERT(y < MAX_NUMBER_OF_GRIDS);
            return i_grids[x][y];
        }

        bool isGridObjectDataLoaded(uint32 x, uint32 y) const { return getNGrid(x, y)->isGridObjectDataLoaded(); }
        void setGridObjectDataLoaded(bool pLoaded, uint32 x, uint32 y) { getNGrid(x, y)->setGridObjectDataLoaded(pLoaded); }

        void setNGrid(NGridType* grid, uint32 x, uint32 y);
        void ScriptsProcess();

        void UpdateVisibility(UpdateDataMapType& update_players);
        void SendObjectUpdates();
        std::set<Object*> m_objectsToClientUpdate;
        std::set<std::pair<Object*, ObjectGuid>> m_objectsToClientCreateUpdate;
        std::set<Object*> m_objectsToClientMovementUpdate;
        std::vector<std::pair<GuidSet, ObjectGuid>> m_objectsToClientRemove;
        std::unordered_map<Object*, PlayerSet> m_visibilityAdded;

        std::set<WorldObject*> m_largeObjects;
        std::set<WorldObject*> m_infiniteObjects;
        std::set<Unit*> m_waypointingNpcs;

    protected:
        MapEntry const* i_mapEntry;
        uint8 i_spawnMode;
        uint32 i_id;
        uint32 i_InstanceId;
        MaNGOS::unique_weak_ptr<Map> m_weakRef;
        uint32 m_unloadTimer;
        uint32 m_clientUpdateTimer;
        uint32 m_clientUpdateTick;
        float m_VisibleDistance;
        MapPersistentState* m_persistentState;

        MapRefManager m_mapRefManager;
        MapRefManager::iterator m_mapRefIter;

        typedef WorldObjectSet ActiveNonPlayers;
        ActiveNonPlayers m_activeNonPlayers;
        ActiveNonPlayers::iterator m_activeNonPlayersIter;
        MapStoredObjectTypesContainer m_objectsStore;
        std::map<uint32, uint32> m_tempCreatures;
        std::map<uint32, uint32> m_tempPets;
        std::map<std::pair<HighGuid, uint32>, std::vector<WorldObject*>> m_dbGuidObjects;

        WorldObjectSet m_onEventNotifiedObjects;
        WorldObjectSet::iterator m_onEventNotifiedIter;

        Messager<Map> m_messager;

        GraveyardManager m_graveyardManager;
    private:
        time_t i_gridExpiry;
        time_t m_curTime;
        tm m_curTimeTm;

        NGridType* i_grids[MAX_NUMBER_OF_GRIDS][MAX_NUMBER_OF_GRIDS];

        // Shared geodata object with map coord info... (共享地图地理地形数据对象)
        TerrainInfo* const m_TerrainData;
        bool m_bLoadedGrids[MAX_NUMBER_OF_GRIDS][MAX_NUMBER_OF_GRIDS];

        std::bitset<TOTAL_NUMBER_OF_CELLS_PER_MAP* TOTAL_NUMBER_OF_CELLS_PER_MAP> marked_cells;

        WorldObjectSet i_objectsToRemove;

        typedef std::multimap<TimePoint, ScriptAction> ScriptScheduleMap;
        ScriptScheduleMap m_scriptSchedule;

        InstanceData* i_data;
        uint32 i_script_id;

        // Map local low guid counters (地图局部低位 GUID 分配器)
        MapGuidAllocator m_CreatureGuids;
        MapGuidAllocator m_GameObjectGuids;
        ObjectGuidGenerator<HIGHGUID_TRANSPORT> m_transportGuids;
        MapGuidAllocator m_DynObjectGuids;
        MapGuidAllocator m_PetGuids;

        // Type specific code for add/remove to/from grid (按对象类型向网格中添加或移除对象的模板方法)
        template<class T>
        void AddToGrid(T*, NGridType*, Cell const&);

        template<class T>
        void RemoveFromGrid(T*, NGridType*, Cell const&);
        // Holder for information about linked mobs (生物链接信息容器)
        CreatureLinkingHolder m_creatureLinkingHolder;

        // Dynamic Map tree object (动态地图碰撞树)
        DynamicMapTree m_dyn_tree;

        // WeatherSystem (天气系统实例)
        WeatherSystem* m_weatherSystem;

        // Transports (当前地图中的运输载具集合)
        TransportSet m_transports;
        TransportSet::iterator m_transportsIterator;

        std::unordered_map<uint32, std::set<ObjectGuid>> m_spawnedCount;

        // spawning (地图刷新管理器)
        SpawnManager m_spawnManager;

        struct StringIdMapStorage
        {
            std::vector<WorldObject*> worldObjects;
            std::vector<Creature*> creatures;
            std::vector<GameObject*> gameobjects;
        };

        std::unordered_map<uint32, StringIdMapStorage> m_objectsPerStringId;

        MapDataContainer m_dataContainer;
        std::shared_ptr<CreatureSpellListContainer> m_spellListContainer;

        WorldStateVariableManager m_variableManager;

#ifdef ENABLE_PLAYERBOTS
        std::vector<uint32> m_activeZones;
        uint32 m_activeZonesTimer;
        bool hasRealPlayers;
#endif

        ZoneDynamicInfoMap m_zoneDynamicInfo;
        ZoneDynamicInfoMap m_areaDynamicInfo;
        uint32 m_defaultLight;
};

// 世界大陆地图子类（非副本大地图）
class WorldMap : public Map
{
    private:
        using Map::GetPersistentState;                      // hide in subclass for overwrite (在子类中隐藏基类接口以提供精确返回类型)
    public:
        WorldMap(uint32 id, time_t expiry, uint32 InstanceId) : Map(id, expiry, InstanceId, REGULAR_DIFFICULTY) {}
        ~WorldMap() {}

        // can't be nullptr for loaded map (已加载地图的持久化状态不可为空)
        WorldPersistentState* GetPersistanceState() const;
};

// 地下城/团队副本实例地图子类，处理进入权限、队伍阵营锁定、重置调度与持久化绑定
class DungeonMap : public Map
{
    private:
        using Map::GetPersistentState;                      // hide in subclass for overwrite (在子类中隐藏基类接口以提供精确返回类型)
    public:
        DungeonMap(uint32 id, time_t, uint32 InstanceId, uint8 SpawnMode);
        ~DungeonMap();
        bool Add(Player*) override;
        void Remove(Player*, bool) override;
        void Update(const uint32&) override;
        bool Reset(InstanceResetMethod method);
        void PermBindAllPlayers(Player* player = nullptr);
        void UnloadAll(bool pForce) override;
        void SendResetWarnings(uint32 timeLeft) const;
        void SetResetSchedule(bool on);

        Team GetInstanceTeam() { return m_team; };

        // can't be nullptr for loaded map (已加载副本的持久化状态不可为空)
        DungeonPersistentState* GetPersistanceState() const;

        virtual void InitVisibilityDistance() override;
    private:
        bool m_resetAfterUnload;
        bool m_unloadWhenEmpty;
        Team m_team;
};

// 战场/竞技场实例地图子类，关联 BattleGround 对象与战局生命周期
class BattleGroundMap : public Map
{
    private:
        using Map::GetPersistentState;                      // hide in subclass for overwrite (在子类中隐藏基类接口以提供精确返回类型)
    public:
        BattleGroundMap(uint32 id, time_t, uint32 InstanceId, uint8 spawnMode);
        ~BattleGroundMap();

        virtual void Initialize(std::mutex* mmapMutex, bool) override;
        void Update(const uint32&) override;
        bool Add(Player*) override;
        void Remove(Player*, bool) override;
        bool CanEnter(Player* player) override;
        void SetUnload();
        void UnloadAll(bool pForce) override;

        virtual void InitVisibilityDistance() override;
        BattleGround* GetBG() const override { return m_bg; }
        void SetBG(BattleGround* bg) { m_bg = bg; }

        // can't be nullptr for loaded map (已加载战场地图的持久化状态不可为空)
        BattleGroundPersistentState* GetPersistanceState() const;

    private:
        BattleGround* m_bg;
};

template<class T, class CONTAINER>
inline void
Map::Visit(const Cell& cell, TypeContainerVisitor<T, CONTAINER>& visitor)
{
    const uint32 x = cell.GridX();
    const uint32 y = cell.GridY();
    const uint32 cell_x = cell.CellX();
    const uint32 cell_y = cell.CellY();

    if (!cell.NoCreate() || loaded(GridPair(x, y)))
    {
        EnsureGridLoaded(cell);
        getNGrid(x, y)->Visit(cell_x, cell_y, visitor);
    }
}
#endif
