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

#include "Maps/MapManager.h"
#include "Maps/MapPersistentStateMgr.h"
#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Log/Log.h"
#include "Entities/Transports.h"
#include "Maps/GridDefines.h"
#include "World/World.h"
#include "Grids/CellImpl.h"
#include "Globals/ObjectMgr.h"
#include "Maps/MapWorkers.h"
#include "BattleGround/BattleGroundMgr.h"
#include <future>

#define CLASS_LOCK MaNGOS::ClassLevelLockable<MapManager, std::recursive_mutex>
INSTANTIATE_SINGLETON_2(MapManager, CLASS_LOCK);
INSTANTIATE_CLASS_MUTEX(MapManager, std::recursive_mutex);

MapManager::MapManager()
    : i_gridCleanUpDelay(sWorld.getConfig(CONFIG_UINT32_INTERVAL_GRIDCLEAN))
{
    i_timer.SetInterval(sWorld.getConfig(CONFIG_UINT32_INTERVAL_MAPUPDATE));
}

MapManager::~MapManager()
{
    i_maps.clear();

    DeleteStateMachine();
}

// 初始化地图管理器：构建网格状态机、读取最大副本 ID、创建大陆地图并启动更新线程池
void MapManager::Initialize()
{
    InitStateMachine();
    InitMaxInstanceId();
    CreateContinents();

    int num_threads(sWorld.getConfig(CONFIG_UINT32_NUM_MAP_THREADS));
    if (num_threads > 0)
        m_updater.activate(num_threads);
}

// 初始化四种网格状态处理器实例
void MapManager::InitStateMachine()
{
    si_GridStates[GRID_STATE_INVALID] = new InvalidState;
    si_GridStates[GRID_STATE_ACTIVE] = new ActiveState;
    si_GridStates[GRID_STATE_IDLE] = new IdleState;
    si_GridStates[GRID_STATE_REMOVAL] = new RemovalState;
}

void MapManager::DeleteStateMachine()
{
    delete si_GridStates[GRID_STATE_INVALID];
    delete si_GridStates[GRID_STATE_ACTIVE];
    delete si_GridStates[GRID_STATE_IDLE];
    delete si_GridStates[GRID_STATE_REMOVAL];
}

void MapManager::UpdateGridState(grid_state_t state, Map& map, NGridType& ngrid, GridInfo& ginfo, const uint32& x, const uint32& y, const uint32& t_diff)
{
    // TODO: The grid state array itself is static and therefore 100% safe, however, the data
    // the state classes in it accesses is not, since grids are shared across maps (for example
    // in instances), so some sort of locking will be necessary later.
    // 待办：网格状态机数组本身为静态且安全，但状态类访问的共享网格数据尚缺乏跨地图锁机制

    si_GridStates[state]->Update(map, ngrid, ginfo, x, y, t_diff);
}

void MapManager::InitializeVisibilityDistanceInfo()
{
    for (auto& i_map : i_maps)
        i_map.second->InitVisibilityDistance();
}

// 并行预创建并初始化三块大陆地图（东部王国 0、卡利姆多 1、外域 530）
void MapManager::CreateContinents()
{
    std::mutex mmapMutex; // all of initialize can be in parallel but mmap creation (除 MMap 创建外其余初始化可并行)
    std::vector<std::future<void>> futures;
    uint32 continents[] = { 0, 1, 530 };
    for (auto id : continents)
    {
        Map* m = new WorldMap(id, i_gridCleanUpDelay, 0);
        // add map into container (将新建地图纳入容器并设置弱引用)
        MaNGOS::unique_trackable_ptr<Map>& ptr = i_maps[MapID(id)];
        ptr.reset(m);
        m->SetWeakPtr(ptr);

        // non-instanceable maps always expected have saved state (非副本地图始终拥有持久化状态)
        futures.push_back(std::async(std::launch::async, std::bind(&Map::Initialize, m, &mmapMutex, true)));
    }

    for (auto& futurItr : futures)
        futurItr.wait();
}

/// @param id - MapId of the to be created map. @param obj WorldObject for which the map is to be created. Must be player for Instancable maps.
/// 创建或获取指定地图实例；对于可实例化地图，obj 必须为玩家对象
Map* MapManager::CreateMap(uint32 id, const WorldObject* obj)
{
    Guard _guard(*this);

    Map* m = nullptr;

    const MapEntry* entry = sMapStore.LookupEntry(id);
    if (!entry)
        return nullptr;

    if (entry->Instanceable())
    {
        MANGOS_ASSERT(obj && obj->GetTypeId() == TYPEID_PLAYER);
        // create DungeonMap object (创建或获取副本/战场地图实例)
        m = CreateInstance(id, (Player*)obj);
    }
    else
    {
        uint32 instanceId = 0;
        // create regular non-instanceable map (获取或创建常规非副本地图)
        m = FindMap(id);
        if (m == nullptr)
        {
            std::lock_guard<std::mutex> lock(m_lock);
            m = new WorldMap(id, i_gridCleanUpDelay, instanceId);
            // add map into container (加入地图容器)
            MaNGOS::unique_trackable_ptr<Map>& ptr = i_maps[MapID(id, instanceId)];
            ptr.reset(m);
            m->SetWeakPtr(ptr);

            // non-instanceable maps always expected have saved state (非副本地图始终加载持久化状态)
            m->Initialize(nullptr); // can be nullptr because already safeguarded by guard (已有外部递归锁保护，此处可传 nullptr)
        }
    }

    return m;
}

Map* MapManager::CreateBgMap(uint32 mapid, uint32 instanceId, BattleGround* bg)
{
    sTerrainMgr.LoadTerrain(mapid);

    Guard _guard(*this);
    return CreateBattleGroundMap(mapid, instanceId, bg);
}

// 按 mapid 与 instanceId 查找已加载的地图实例
Map* MapManager::FindMap(uint32 mapid, uint32 instanceId) const
{
    Guard guard(*this);

    MapMapType::const_iterator iter = i_maps.find(MapID(mapid, instanceId));
    if (iter == i_maps.end())
        return nullptr;

    // this is a small workaround for transports (针对运输载具查找的保护：副本地图不应以 instanceId == 0 查询)
    if (instanceId == 0 && iter->second->Instanceable())
    {
        assert(false);
        return nullptr;
    }

    return iter->second.get();
}

// 从容器摘除并卸载指定的副本实例
void MapManager::DeleteInstance(uint32 mapid, uint32 instanceId)
{
    Guard _guard(*this);

    MapMapType::iterator iter = i_maps.find(MapID(mapid, instanceId));
    if (iter != i_maps.end())
    {
        if (iter->second->Instanceable())
        {
            auto node = i_maps.extract(iter);

            node.mapped()->UnloadAll(true);
        }
    }
}

// 定时更新所有地图实例，并卸载超时的空副本
void MapManager::Update(uint32 diff)
{
    i_timer.Update(diff);
    if (!i_timer.Passed())
        return;

    for (auto& map : i_maps)
    {
        if (m_updater.activated())
            m_updater.schedule_update(new MapUpdateWorker(*map.second, (uint32)i_timer.GetCurrent(), m_updater));
        else
            map.second->Update((uint32)i_timer.GetCurrent());
    }

    if (m_updater.activated())
        m_updater.wait();

    // remove all maps which can be unloaded (移除并卸载所有达到卸载倒计时条件的地图)
    MapMapType::iterator iter = i_maps.begin();
    while (iter != i_maps.end())
    {
        // check if map can be unloaded
        if (iter->second->CanUnload((uint32)i_timer.GetCurrent()))
        {
            auto node = i_maps.extract(iter++);

            node.mapped()->UnloadAll(true);
        }
        else
            ++iter;
    }

    i_timer.SetCurrent(0);
}

void MapManager::RemoveAllObjectsInRemoveList()
{
    for (auto& i_map : i_maps)
        i_map.second->RemoveAllObjectsInRemoveList();
}

bool MapManager::ExistMapAndVMap(uint32 mapid, float x, float y)
{
    GridPair p = MaNGOS::ComputeGridPair(x, y);

    int gx = 63 - p.x_coord;
    int gy = 63 - p.y_coord;

    return GridMap::ExistMap(mapid, gx, gy) && GridMap::ExistVMap(mapid, gx, gy);
}

bool MapManager::IsValidMAP(uint32 mapid)
{
    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);
    return mEntry && (!mEntry->IsDungeon() || ObjectMgr::GetInstanceTemplate(mapid));
    // TODO: add check for battleground template
}

void MapManager::UnloadAll()
{
    for (auto& i_map : i_maps)
        i_map.second->UnloadAll(true);

    i_maps.clear();

    if (m_updater.activated())
        m_updater.deactivate();

    TerrainManager::Instance().UnloadAll();
}

void MapManager::InitMaxInstanceId()
{
    i_MaxInstanceId = 0;

    auto queryResult = CharacterDatabase.Query("SELECT MAX(id) FROM instance");
    if (queryResult)
    {
        i_MaxInstanceId = queryResult->Fetch()[0].GetUInt32();
    }
}

uint32 MapManager::GetNumInstances()
{
    std::lock_guard<std::mutex> lock(m_lock);

    uint32 ret = 0;
    for (auto& i_map : i_maps)
    {
        Map* map = i_map.second.get();
        if (!map->IsDungeon()) continue;
        ret += 1;
    }
    return ret;
}

uint32 MapManager::GetNumPlayersInInstances()
{
    std::lock_guard<std::mutex> lock(m_lock);
    uint32 ret = 0;
    for (auto& i_map : i_maps)
    {
        Map* map = i_map.second.get();
        if (!map->IsDungeon()) continue;
        ret += map->GetPlayers().getSize();
    }
    return ret;
}

///// returns a new or existing Instance
///// in case of battlegrounds it will only return an existing map, those maps are created by bg-system
///// 为玩家返回新建或已存在的副本/战场地图；战场仅返回已由战场系统创建的地图
Map* MapManager::CreateInstance(uint32 id, Player* player)
{
    Map* map = nullptr;
    Map* pNewMap = nullptr;
    uint32 NewInstanceId = 0;                               // instanceId of the resulting map (目标地图实例 ID)
    const MapEntry* entry = sMapStore.LookupEntry(id);

    if (entry->IsBattleGroundOrArena())
    {
        // find existing bg map for player (查找玩家当前绑定的战场地图)
        NewInstanceId = player->GetBattleGroundId();
        MANGOS_ASSERT(NewInstanceId);
        map = FindMap(id, NewInstanceId);
        MANGOS_ASSERT(map);
    }
    else if (DungeonPersistentState* pSave = player->GetBoundInstanceSaveForSelfOrGroup(id))
    {
        // solo/perm/group (个人/永久/队伍已有副本存档：复用意图中的实例 ID，若地图未加载则重建)
        NewInstanceId = pSave->GetInstanceId();
        map = FindMap(id, NewInstanceId);
        // it is possible that the save exists but the map doesn't (可能存档存在而地图尚未加载)
        if (!map)
            pNewMap = CreateDungeonMap(id, NewInstanceId, pSave->GetDifficulty(), pSave);
    }
    else
    {
        // if no instanceId via group members or instance saves is found
        // the instance will be created for the first time
        // 队伍与个人均无绑定存档时，分配新实例 ID 并首次创建地下城地图
        NewInstanceId = GenerateInstanceId();

        Difficulty diff = player->GetGroup() ? player->GetGroup()->GetDifficulty() : player->GetDifficulty();
        pNewMap = CreateDungeonMap(id, NewInstanceId, diff);
    }

    // add a new map object into the registry (将新创建的地图注册到管理器映射表中)
    if (pNewMap)
    {
        MaNGOS::unique_trackable_ptr<Map>& ptr = i_maps[MapID(id, NewInstanceId)];
        ptr.reset(pNewMap);
        pNewMap->SetWeakPtr(ptr);
        map = pNewMap;
    }

    return map;
}

// 创建并初始化地下城/团队副本实例地图
DungeonMap* MapManager::CreateDungeonMap(uint32 id, uint32 InstanceId, Difficulty difficulty, DungeonPersistentState* save)
{
    // make sure we have a valid map id (校验地图 ID 与副本模板有效性)
    const MapEntry* entry = sMapStore.LookupEntry(id);
    if (!entry)
    {
        sLog.outError("CreateDungeonMap: no entry for map %d", id);
        MANGOS_ASSERT(false);
    }
    if (!ObjectMgr::GetInstanceTemplate(id))
    {
        sLog.outError("CreateDungeonMap: no instance template for map %d", id);
        MANGOS_ASSERT(false);
    }

    // some instances only have one difficulty (不支持英雄难度的副本强制回退为普通难度)
    if (entry && !entry->SupportsHeroicMode())
        difficulty = DUNGEON_DIFFICULTY_NORMAL;

    DEBUG_LOG("MapInstanced::CreateDungeonMap: %s map instance %d for %d created with difficulty %d", save ? "" : "new ", InstanceId, id, difficulty);

    DungeonMap* map = new DungeonMap(id, i_gridCleanUpDelay, InstanceId, difficulty);

    // Dungeons can have saved instance data (若存在既有存档则加载副本进度数据)
    bool load_data = save != nullptr;
    map->Initialize(nullptr, load_data); // can be nullptr because already safeguarded by guard one function up

    return map;
}

// 创建并初始化战场/竞技场地图实例
BattleGroundMap* MapManager::CreateBattleGroundMap(uint32 id, uint32 InstanceId, BattleGround* bg)
{
    DEBUG_LOG("MapInstanced::CreateBattleGroundMap: instance:%d for map:%d and bgType:%d created.", InstanceId, id, bg->GetTypeId());

    uint8 spawnMode = uint8(DUNGEON_DIFFICULTY_NORMAL);
    if (bg->GetTypeId() == BATTLEGROUND_AV && sBattleGroundMgr.GetMinLevelForBattleGroundBracketId(bg->GetBracketId(), bg->GetTypeId()) >= 61)
        spawnMode = uint8(DUNGEON_DIFFICULTY_HEROIC);        

    BattleGroundMap* map = new BattleGroundMap(id, i_gridCleanUpDelay, InstanceId, spawnMode);
    MANGOS_ASSERT(map->IsBattleGroundOrArena());
    map->SetBG(bg);
    bg->SetBgMap(map);

    // add map into map container
    MaNGOS::unique_trackable_ptr<Map>& ptr = i_maps[MapID(id, InstanceId)];
    ptr.reset(map);
    map->SetWeakPtr(ptr);

    // BGs/Arenas not have saved instance data
    map->Initialize(nullptr, false);

    return map;
}

void MapManager::DoForAllMapsWithMapId(uint32 mapId, const std::function<void(Map*)> worker)
{
    MapMapType::const_iterator start = i_maps.lower_bound(MapID(mapId, 0));
    MapMapType::const_iterator end = i_maps.lower_bound(MapID(mapId + 1, 0));
    for (MapMapType::const_iterator itr = start; itr != end; ++itr)
        worker(itr->second.get());
}

void MapManager::DoForAllMaps(const std::function<void(Map*)>& worker)
{
    for (MapMapType::const_iterator itr = i_maps.begin(); itr != i_maps.end(); ++itr)
        worker(itr->second.get());
}
