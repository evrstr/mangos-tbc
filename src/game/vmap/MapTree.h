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

#ifndef _MAPTREE_H
#define _MAPTREE_H

#include "BIH.h"

#include <unordered_map>

namespace VMAP
{
    class ModelInstance;
    class GroupModel;
    class VMapManager2;

    // 组模型命中定位信息（命中 GroupModel 与所属 Root WMO ID）
    struct GroupLocationInfo
    {
        const GroupModel* hitModel = nullptr;
        int32 rootId = -1;
    };

    // 完整位置查询信息（实例、组模型、地面高度及 Root ID）
    struct LocationInfo
    {
        LocationInfo(): hitInstance(nullptr), hitModel(nullptr), ground_Z(-G3D::inf()), rootId(-1){};
        const ModelInstance* hitInstance;
        const GroupModel* hitModel;
        float ground_Z;
        int32 rootId;
    };

    // 静态地图碰撞树：管理单张地图的 .vmtree 顶层 BIH 索引以及按需加载/卸载的 .vmtile 模型实例引用
    class StaticMapTree
    {
            typedef std::unordered_map<uint32, bool> loadedTileMap;
            typedef std::unordered_map<uint32, uint32> loadedSpawnMap;
        private:
            uint32 iMapID;                                              // 地图 ID
            bool iIsTiled;                                              // 是否为分块地图（非分块地图如副本整个加载在唯一树中）
            BIH iTree;                                                  // 整张地图的静态模型实例 BIH 树
            ModelInstance* iTreeValues; // the tree entries (静态模型实例数组，按图块引用按需填充)
            uint32 iNTreeValues;                                        // 模型实例槽位总数

            // Store all the map tile idents that are loaded for that map
            // some maps are not splitted into tiles and we have to make sure, not removing the map before all tiles are removed
            // empty tiles have no tile file, hence map with bool instead of just a set (consistency check)
            // 已加载图块映射（即使空图块也记录为 false 以确保一致性卸载计数）
            loadedTileMap iLoadedTiles;
            // stores <tree_index, reference_count> to invalidate tree values, unload map, and to be able to report errors
            // 记录 <模型实例索引, 图块引用计数>，跨越多个图块的大型建筑在所有相关图块卸载后才释放
            loadedSpawnMap iLoadedSpawns;
            std::string iBasePath;                                      // vmaps 文件根目录路径

        private:
            bool getIntersectionTime(const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit = false, bool ignoreM2Model = false) const; // 执行射线求交计算最短碰撞时间
            // bool containsLoadedMapTile(unsigned int pTileIdent) const { return(iLoadedMapTiles.containsKey(pTileIdent)); }
        public:
            static std::string getTileFileName(uint32 mapID, uint32 tileX, uint32 tileY); // 构造图块文件名 (<map>_<y>_<x>.vmtile)
            static uint32 packTileID(uint32 tileX, uint32 tileY) { return tileX << 16 | tileY; } // 打包图块坐标
            static void unpackTileID(uint32 ID, uint32& tileX, uint32& tileY) { tileX = ID >> 16; tileY = ID & 0xFF; } // 解包图块坐标
            static bool CanLoadMap(const std::string& vmapPath, uint32 mapID, uint32 tileX, uint32 tileY); // 检查静态树或指定图块文件是否存在

            StaticMapTree(uint32 mapID, const std::string& basePath);
            ~StaticMapTree();

            bool isInLineOfSight(const G3D::Vector3& pos1, const G3D::Vector3& pos2, bool ignoreM2Model) const; // 视线检测
            bool getObjectHitPos(const G3D::Vector3& pPos1, const G3D::Vector3& pPos2, G3D::Vector3& pResultHitPos, float pModifyDist) const; // 获取碰撞回退点
            float getHeight(const G3D::Vector3& pPos, float maxSearchDist) const; // 向下发射射线探测模型地面高度
            bool getAreaInfo(G3D::Vector3& pos, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const; // 点查询所在 WMO 区域与地面高度
            bool GetLocationInfo(const Vector3& pos, LocationInfo& info) const; // 点查询所在模型实例与组模型位置信息

            bool InitMap(const std::string& fname, VMapManager2* vm);   // 从 .vmtree 文件初始化地图树（非分块地图同步加载全部模型）
            void UnloadMap(VMapManager2* vm);                           // 卸载整张地图的所有图块及模型引用
            bool LoadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm); // 加载单个 .vmtile 图块的模型实例并增加引用计数
            bool IsTileLoaded(uint32 x, uint32 y) const                 // 判断图块是否已加载（非分块地图恒为 true）
            {
                if (!iIsTiled)
                    return true;

                auto itr = iLoadedTiles.find(packTileID(x, y));
                if (itr == iLoadedTiles.end())
                    return false;
                return itr->second;
            }
            // 查询指定图块 (x,y) 是否存在登记记账（区分于 IsTileLoaded 对非分块图恒为 true）
            bool IsTileRegistered(uint32 x, uint32 y) const
            {
                return iLoadedTiles.find(packTileID(x, y)) != iLoadedTiles.end();
            }
            void UnloadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm); // 卸载单个图块并递减模型实例引用计数
            bool isTiled() const { return iIsTiled; }
            uint32 numLoadedTiles() const { return iLoadedTiles.size(); }

#ifdef MMAP_GENERATOR
        public:
            void getModelInstances(ModelInstance*& models, uint32& count);
#endif
    };

    // WMO 区域查询输出结构
    struct AreaInfo
    {
        AreaInfo(): result(false), ground_Z(-G3D::inf()), flags(0), adtId(0), rootId(0), groupId(0)
        {};
        bool result;
        float ground_Z;
        uint32 flags;
        int32 adtId;
        int32 rootId;
        int32 groupId;
    };
}                                                           // VMAP

#endif // _MAPTREE_H
