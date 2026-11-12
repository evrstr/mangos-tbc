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

#ifndef _VMAPMANAGER2_H
#define _VMAPMANAGER2_H

#include "IVMapManager.h"

#include <G3D/Vector3.h>

#include <unordered_map>
#include <mutex>

//===========================================================

#define MAP_FILENAME_EXTENSION2 ".vmtree" // 地图静态碰撞树文件扩展名

#define FILENAMEBUFFER_SIZE 500 // 文件名缓冲区最大长度

/**
This is the main Class to manage loading and unloading of maps, line of sight, height calculation and so on.
For each map or map tile to load it reads a directory file that contains the ModelContainer files used by this map or map tile.
Each global map or instance has its own dynamic BSP-Tree.
The loaded ModelContainers are included in one of these BSP-Trees.
Additionally a table to match map ids and map names is used.
VMAP 核心管理器实现：管理各地图 StaticMapTree 的按需加载/卸载、共享 WorldModel 引用计数缓存、视线检测、高度及液体查询
*/

//===========================================================

namespace VMAP
{
    class StaticMapTree;
    class WorldModel;

    // 共享模型引用计数包装器
    class ManagedModel
    {
        public:
            ManagedModel() : iModel(nullptr), iRefCount(0) {}
            void setModel(WorldModel* model) { iModel = model; }
            WorldModel* getModel() const { return iModel; }
            void incRefCount() { ++iRefCount; }
            int decRefCount() { return --iRefCount; }
        protected:
            WorldModel* iModel;                                 // 共享的 WorldModel 实例指针
            int iRefCount;                                      // 当前引用计数（归零时释放）
    };

    typedef std::unordered_map<uint32, StaticMapTree*> InstanceTreeMap;
    typedef std::unordered_map<std::string, ManagedModel> ModelFileMap;

    // IVMapManager 的线程安全实现类
    class VMapManager2 : public IVMapManager
    {
        private:
            std::mutex m_vmStaticMapMutex;                      // 保护 iInstanceMapTrees 增删的互斥锁
            std::mutex m_vmModelMutex;                          // 保护共享模型缓存 iLoadedModelFiles 的互斥锁

            bool m_thread_safe_environment;                     // 若已通过 InitializeThreadUnsafe 预分配全部地图槽位则为 true（可无锁只读查找）

        protected:
            // Tree to check collision (已加载的模型缓存与地图碰撞树表)
            ModelFileMap iLoadedModelFiles;
            InstanceTreeMap iInstanceMapTrees;

            bool _loadMap(uint32 pMapId, const std::string& basePath, uint32 tileX, uint32 tileY); // 加载地图树及指定图块数据
            /* void _unloadMap(uint32 pMapId, uint32 x, uint32 y); */

        public:
            // public for debug (坐标转换及调试辅助接口)
            G3D::Vector3 convertPositionToInternalRep(float x, float y, float z) const; // 将游戏世界坐标转换为 VMAP 内部坐标系（含 32*533.333 偏移）
            InstanceTreeMap::const_iterator GetMapTree(uint32 mapId) const; // 查询地图对应的 StaticMapTree 迭代器
            static std::string getMapFileName(unsigned int pMapId); // 构造地图 .vmtree 文件名

            VMapManager2();
            ~VMapManager2() override;

            void InitializeThreadUnsafe(const std::vector<uint32>& mapIds) override; // 启动期预插入全部地图键，避免运行期哈希表重哈希

            VMAPLoadResult loadMap(const char* pBasePath, unsigned int pMapId, int x, int y) override; // 加载地图图块
            bool IsTileLoaded(uint32 mapId, uint32 x, uint32 y) const override; // 查询图块是否已加载

            void unloadMap(unsigned int pMapId, int x, int y) override; // 卸载指定地图图块
            void unloadMap(unsigned int pMapId) override;               // 卸载整张地图的所有图块及树实例

            bool isInLineOfSight(unsigned int pMapId, float x1, float y1, float z1, float x2, float y2, float z2, bool ignoreM2Model) override; // 判断两点之间是否无模型遮挡
            /**
            fill the hit pos and return true, if an object was hit (返回射线命中回退后的坐标)
            */
            bool getObjectHitPos(unsigned int pMapId, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float& ry, float& rz, float pModifyDist) override;
            float getHeight(unsigned int pMapId, float x, float y, float z, float maxSearchDist) override; // 向下射线查询模型高度

            bool processCommand(char* /*pCommand*/) override { return false; }      // for debug and extensions

            bool getAreaInfo(unsigned int pMapId, float x, float y, float& z, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const override; // 查询模型区域标志与高度
            bool GetLiquidLevel(uint32 pMapId, float x, float y, float z, uint8 ReqLiquidTypeMask, float& level, float& floor, uint32& type) const override; // 查询 WMO 内部液体高度

            WorldModel* acquireModelInstance(const std::string& basepath, const std::string& filename); // 获取或加载共享 WorldModel 并增加引用计数
            void releaseModelInstance(const std::string& filename);     // 递减共享 WorldModel 引用计数并在归零时卸载释放

            // what's the use of this? o.O
            std::string getDirFileName(unsigned int pMapId, int /*x*/, int /*y*/) const override
            {
                return getMapFileName(pMapId);
            }
            bool existsMap(const char* pBasePath, unsigned int pMapId, int x, int y) override; // 检查地图/图块文件是否存在

#ifdef MMAP_GENERATOR
        public:
            void getInstanceMapTree(InstanceTreeMap& instanceMapTree);
#endif
    };
}
#endif
