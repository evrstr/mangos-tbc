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

#ifndef _MOVE_MAP_H
#define _MOVE_MAP_H

#include "Common.h"
#include <Detour/Include/DetourAlloc.h>
#include <Detour/Include/DetourNavMesh.h>
#include <Detour/Include/DetourNavMeshQuery.h>

#include <memory>
#include <mutex>

class Unit;

//  memory management (Detour 自定义内存分配/释放回调)
inline void* dtCustomAlloc(size_t size, dtAllocHint /*hint*/)
{
    return (void*)new unsigned char[size];
}

inline void dtCustomFree(void* ptr)
{
    delete[](unsigned char*)ptr;
}

//  move map related classes (MMAP 寻路导航网格加载与查询管理模块)
namespace MMAP
{
    typedef std::unordered_map<uint32, dtTileRef> MMapTileSet;
    typedef std::unordered_map<uint32, dtNavMeshQuery*> NavMeshQuerySet;
    typedef std::unordered_map<std::thread::id, dtNavMeshQuery*> NavMeshGOQuerySet;

    // dummy struct to hold map's mmap data (单张地图的导航网格及各副本实例查询对象容器)
    struct MMapData
    {
        MMapData(dtNavMesh* mesh) : navMesh(mesh), fullLoaded(false) {}
        ~MMapData()
        {
            for (auto& navMeshQuerie : navMeshQueries)
                dtFreeNavMeshQuery(navMeshQuerie.second);

            if (navMesh)
                dtFreeNavMesh(navMesh);
        }

        dtNavMesh* navMesh;

        // we have to use single dtNavMeshQuery for every instance, since those are not thread safe
        // 每个地图实例独占一个 dtNavMeshQuery（Detour 查询对象非线程安全）
        NavMeshQuerySet navMeshQueries;     // instanceId to query (实例 ID -> 查询句柄)
        MMapTileSet mmapLoadedTiles;        // maps [map grid coords] to [dtTile] (已加载的网格坐标 -> 瓦片句柄)

        bool fullLoaded;
    };

    /// 动态 GameObject 模型（如可开关门/船/电梯）的导航网格数据
    struct MMapGOData
    {
        MMapGOData(dtNavMesh* mesh) : navMesh(mesh) {}
        ~MMapGOData()
        {
            for (auto& navMeshQuerie : navMeshGOQueries)
                dtFreeNavMeshQuery(navMeshQuerie.second);

            if (navMesh)
                dtFreeNavMesh(navMesh);
        }

        dtNavMesh* navMesh;

        // we have to use single dtNavMeshQuery for every instance, since those are not thread safe
        // 按线程 ID 隔离 dtNavMeshQuery，避免多线程并发查询冲突
        NavMeshGOQuerySet navMeshGOQueries;  // instanceId to query (线程 ID -> 查询句柄)
    };


    // singleton class
    // holds all access to mmap loading unloading and meshes
    // 导航网格管理器单例：负责地图瓦片与 GameObject 模型网格的加载、卸载及查询分配
    class MMapManager
    {
        public:
            MMapManager() : loadedTiles(0), m_enabled(true) {}
            ~MMapManager();

            /// 一次性加载指定地图的全部瓦片（用于开启全图预加载时）
            void loadAllMapTiles(std::string const& basePath, uint32 mapId);
            /// 加载指定地图 (x, y) 网格坐标对应的 .mmtile 瓦片
            bool loadMap(std::string const& basePath, uint32 mapId, int32 x, int32 y);
            /// 内部瓦片文件读取与注入实现
            bool loadMapInternal(const char* filePath, const std::unique_ptr<MMapData>& mmapData, uint32 packedGridPos, uint32 mapId, int32 x, int32 y);
            /// 预加载所有 GameObject 显示模型的独立导航网格
            void loadAllGameObjectModels(std::string const& basePath, std::vector<uint32> const& displayIds);
            /// 加载单个 GameObject 模型的导航网格
            bool loadGameObject(std::string const& basePath, uint32 displayId);
            /// 为地图副本实例创建独立的 dtNavMeshQuery
            bool loadMapInstance(std::string const& basePath, uint32 mapId, uint32 instanceId);
            /// 卸载指定地图网格瓦片
            bool unloadMap(uint32 mapId, int32 x, int32 y);
            /// 卸载整张地图的所有导航网格数据
            bool unloadMap(uint32 mapId);
            /// 卸载并释放指定副本实例的查询句柄
            bool unloadMapInstance(uint32 mapId, uint32 instanceId);
            bool IsMMapIsLoaded(uint32 mapId, uint32 x, uint32 y) const;

            // the returned [dtNavMeshQuery const*] is NOT threadsafe (返回的查询对象非线程安全，仅限对应地图线程使用)
            dtNavMeshQuery const* GetNavMeshQuery(uint32 mapId, uint32 instanceId);
            dtNavMeshQuery const* GetModelNavMeshQuery(uint32 displayId);
            dtNavMesh const* GetNavMesh(uint32 mapId);
            dtNavMesh const* GetGONavMesh(uint32 displayId);

            uint32 getLoadedTilesCount() const { return loadedTiles; }
            uint32 getLoadedMapsCount() const { return loadedMMaps.size(); }

            void SetEnabled(bool state) { m_enabled = state; }
            bool IsEnabled() const { return m_enabled; }
        private:
            /// 加载地图全局 .mmap 头信息并初始化 dtNavMesh
            bool loadMapData(std::string const& basePath, uint32 mapId);
            /// 将二维网格坐标 (x, y) 打包为 32 位瓦片键值
            uint32 packTileID(int32 x, int32 y) const;

            std::unordered_map<uint32, std::unique_ptr<MMapData>> loadedMMaps;
            std::atomic<uint32> loadedTiles;

            std::unordered_map<uint32, std::unique_ptr<MMapGOData>> m_loadedModels;
            std::mutex m_modelsMutex;

            bool m_enabled;
    };

    // static class
    // holds all mmap global data
    // access point to MMapManager singleton
    // MMAP 全局工厂与静态开关入口（控制禁寻路地图列表及全局实例生命周期）
    class MMapFactory
    {
        public:
            static MMapManager* createOrGetMMapManager();
            static void clear();
            static void preventPathfindingOnMaps(const char* ignoreMapIds);
            static bool IsPathfindingEnabled(uint32 mapId, const Unit* unit);
    };
}

#endif  // _MOVE_MAP_H
