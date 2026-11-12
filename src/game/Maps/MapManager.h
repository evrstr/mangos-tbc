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

#ifndef MANGOS_MAPMANAGER_H
#define MANGOS_MAPMANAGER_H

#include "Common.h"
#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include "Map.h"
#include "Maps/Map.h"
#include "Grids/GridStates.h"
#include "Maps/MapUpdater.h"
#include "Util/UniqueTrackablePtr.h"

#include <functional>

class Transport;
class BattleGround;
struct TransportTemplate;

// 地图唯一标识键（由地图 ID 与副本实例 ID 组成）
struct MapID
{
    explicit MapID(uint32 id) : nMapId(id), nInstanceId(0) {}
    MapID(uint32 id, uint32 instid) : nMapId(id), nInstanceId(instid) {}

    bool operator<(const MapID& val) const
    {
        if (nMapId == val.nMapId)
            return nInstanceId < val.nInstanceId;

        return nMapId < val.nMapId;
    }

    bool operator==(const MapID& val) const { return nMapId == val.nMapId && nInstanceId == val.nInstanceId; }

    uint32 nMapId;
    uint32 nInstanceId;
};

// 全局地图管理器单例：负责所有大陆、地下城、战场地图的创建、查找、定时更新、网格状态切换与运输载具加载
class MapManager : public MaNGOS::Singleton<MapManager, MaNGOS::ClassLevelLockable<MapManager, std::recursive_mutex> >
{
        friend class MaNGOS::OperatorNew<MapManager>;

        typedef std::recursive_mutex LOCK_TYPE;
        typedef std::lock_guard<LOCK_TYPE> LOCK_TYPE_GUARD;
        typedef MaNGOS::ClassLevelLockable<MapManager, std::recursive_mutex>::Lock Guard;

    public:
        typedef std::map<MapID, MaNGOS::unique_trackable_ptr<Map> > MapMapType;

        void CreateContinents();
        Map* CreateMap(uint32, const WorldObject* obj);
        Map* CreateBgMap(uint32 mapid, uint32 instanceId, BattleGround* bg);
        Map* FindMap(uint32 mapid, uint32 instanceId = 0) const;

        void UpdateGridState(grid_state_t state, Map& map, NGridType& ngrid, GridInfo& ginfo, const uint32& x, const uint32& y, const uint32& t_diff);

        // only const version for outer users (仅对外提供安全删除副本实例接口)
        void DeleteInstance(uint32 mapid, uint32 instanceId);

        void Initialize(); // 初始化网格状态机、加载传输载具并启动地图更新工作线程
        void Update(uint32); // 推进所有活跃地图的定时更新与网格状态机

        void SetGridCleanUpDelay(uint32 t)
        {
            if (t < MIN_GRID_DELAY)
                i_gridCleanUpDelay = MIN_GRID_DELAY;
            else
                i_gridCleanUpDelay = t;
        }

        void SetMapUpdateInterval(uint32 t)
        {
            if (t < MIN_MAP_UPDATE_DELAY)
                t = MIN_MAP_UPDATE_DELAY;

            i_timer.SetInterval(t);
            i_timer.Reset();
        }

        void UnloadAll();

        static bool ExistMapAndVMap(uint32 mapid, float x, float y);
        static bool IsValidMAP(uint32 mapid);

        static bool IsValidMapCoord(uint32 mapid, float x, float y)
        {
            return IsValidMAP(mapid) && MaNGOS::IsValidMapCoord(x, y);
        }

        static bool IsValidMapCoord(uint32 mapid, float x, float y, float z)
        {
            return IsValidMAP(mapid) && MaNGOS::IsValidMapCoord(x, y, z);
        }

        static bool IsValidMapCoord(uint32 mapid, float x, float y, float z, float o)
        {
            return IsValidMAP(mapid) && MaNGOS::IsValidMapCoord(x, y, z, o);
        }

        static bool IsValidMapCoord(WorldLocation const& loc)
        {
            return IsValidMapCoord(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation);
        }

        // modulos a radian orientation to the range of 0..2PI
        // 将弧度朝向归一化到 [0, 2*PI) 区间
        static float NormalizeOrientation(float o)
        {
            // fmod only supports positive numbers. Thus we have
            // to emulate negative numbers
            // fmod 对负数取余结果为负，因此需要手动将负数转换为正角度区间
            if (o < 0)
            {
                float mod = o * -1;
                mod = std::fmod(mod, 2.0f * M_PI_F);
                mod = -mod + 2.0f * M_PI_F;
                return mod;
            }
            return std::fmod(o, 2.0f * M_PI_F);
        }

        void RemoveAllObjectsInRemoveList();

        void LoadTransports();

        typedef std::map<uint32, std::vector<const TransportTemplate*>> TransportMap;
        TransportMap m_transportsByMap;

        uint32 GenerateInstanceId() { return ++i_MaxInstanceId; }
        void InitMaxInstanceId();
        void InitializeVisibilityDistanceInfo();
        /* statistics (地图与副本统计信息) */
        uint32 GetNumInstances();
        uint32 GetNumPlayersInInstances();

        // get list of all maps (获取所有已加载地图的映射表)
        const MapMapType& Maps() const { return i_maps; }

        template<typename Check> inline WorldObject* SearchOnAllLoadedMap(Check& check);
        void DoForAllMaps(const std::function<void(Map*)>& worker);
        void DoForAllMapsWithMapId(uint32 mapId, const std::function<void(Map*)> worker);

    private:

        // debugging code, should be deleted some day (调试用网格状态机数组与错误计数)
        GridState* si_GridStates[MAX_GRID_STATE];
        int i_GridStateErrorCount;

    private:

        MapManager();
        ~MapManager();

        MapManager(const MapManager&);
        MapManager& operator=(const MapManager&);

        void InitStateMachine();
        void DeleteStateMachine();

        Map* CreateInstance(uint32 id, Player* player);
        DungeonMap* CreateDungeonMap(uint32 id, uint32 InstanceId, Difficulty difficulty, DungeonPersistentState* save = nullptr);
        BattleGroundMap* CreateBattleGroundMap(uint32 id, uint32 InstanceId, BattleGround* bg);

        std::mutex m_lock;
        uint32 i_gridCleanUpDelay;
        MapMapType i_maps;
        IntervalTimer i_timer;

        std::atomic<uint32> i_MaxInstanceId;
        MapUpdater m_updater;
};

template<typename Check>
inline WorldObject* MapManager::SearchOnAllLoadedMap(Check& check)
{
    for (auto& mapItr : i_maps)
    {
        WorldObject* result = check(mapItr.second.get());
        if (result)
            return result;
    }
    return nullptr;
}

#define sMapMgr MapManager::Instance()

#endif
