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

#include <iomanip>
#include <string>
#include <sstream>
#include "VMapManager2.h"
#include "MapTree.h"
#include "ModelInstance.h"
#include "WorldModel.h"
#include "VMapDefinitions.h"
#include "Maps/GridMapDefines.h"

using G3D::Vector3;

namespace VMAP
{
    //=========================================================

    VMapManager2::VMapManager2() : m_thread_safe_environment(true)
    {
    }

    //=========================================================
    // 析构时释放全部地图碰撞树与共享模型资源
    VMapManager2::~VMapManager2(void)
    {
        for (auto& iInstanceMapTree : iInstanceMapTrees)
        {
            delete iInstanceMapTree.second;
        }
        for (auto& iLoadedModelFile : iLoadedModelFiles)
        {
            delete iLoadedModelFile.second.getModel();
        }
    }

    // 启动期单线程预创建所有地图 ID 哈希桶，保证运行期多线程只读查询时无需对 map 桶结构加锁
    void VMapManager2::InitializeThreadUnsafe(const std::vector<uint32>& mapIds)
    {
        // the caller must pass the list of all mapIds that will be used in the VMapManager2 lifetime
        for (const uint32& mapId : mapIds)
            iInstanceMapTrees.insert(InstanceTreeMap::value_type(mapId, nullptr));

        m_thread_safe_environment = false;
    }

    //=========================================================
    // 将游戏世界坐标转换为 VMAP 内部坐标系（以地图中心 32*533.3333 码为对称基准）
    Vector3 VMapManager2::convertPositionToInternalRep(float x, float y, float z) const
    {
        Vector3 pos;
        const float mid = 0.5f * 64.0f * 533.33333333f;
        pos.x = mid - x;
        pos.y = mid - y;
        pos.z = z;

        return pos;
    }

    // 查找指定地图的已初始化 StaticMapTree（未加载则返回 cend）
    InstanceTreeMap::const_iterator VMapManager2::GetMapTree(uint32 mapId) const
    {
        // return the iterator if found or end() if not found/NULL
        InstanceTreeMap::const_iterator itr = iInstanceMapTrees.find(mapId);
        if (itr != iInstanceMapTrees.cend() && !itr->second)
            itr = iInstanceMapTrees.cend();

        return itr;
    }

    // move to MapTree too?
    // 构造地图 .vmtree 文件名（3 位补零）
    std::string VMapManager2::getMapFileName(unsigned int pMapId)
    {
        std::stringstream fname;
        fname.width(3);
        fname << std::setfill('0') << pMapId << std::string(MAP_FILENAME_EXTENSION2);
        return fname.str();
    }

    //=========================================================
    // 公共入口：按开关状态加载指定地图及图块
    VMAPLoadResult VMapManager2::loadMap(const char* pBasePath, unsigned int pMapId, int x, int y)
    {
        VMAPLoadResult result = VMAP_LOAD_RESULT_IGNORED;
        if (isMapLoadingEnabled())
        {
            if (_loadMap(pMapId, pBasePath, x, y))
                result = VMAP_LOAD_RESULT_OK;
            else
                result = VMAP_LOAD_RESULT_ERROR;
        }
        return result;
    }

    //=========================================================
    // Check if specified map have tile loaded (检查指定地图的指定图块是否已加载)
    bool VMapManager2::IsTileLoaded(uint32 mapId, uint32 x, uint32 y) const
    {
        InstanceTreeMap::const_iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree == iInstanceMapTrees.end() || instanceTree->second == nullptr)
            return false;
        return instanceTree->second->IsTileLoaded(x, y);
    }

    //=========================================================
    // load one tile (internal use only) (内部加载地图树并按需加载单个 .vmtile 图块)

    bool VMapManager2::_loadMap(unsigned int mapId, const std::string& basePath, uint32 tileX, uint32 tileY)
    {
        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(mapId);
        if (instanceTree == iInstanceMapTrees.end())
        {
            if (m_thread_safe_environment)
                instanceTree = iInstanceMapTrees.insert(InstanceTreeMap::value_type(mapId, nullptr)).first;
            else
                MANGOS_ASSERT(false && "Invalid mapId passed to VMapManager2 after startup in thread unsafe environment");
        }

        if (!instanceTree->second)
        {
            std::string mapFileName = getMapFileName(mapId);
            StaticMapTree* newTree = new StaticMapTree(mapId, basePath);
            if (!newTree->InitMap(mapFileName, this))
            {
                delete newTree;
                return false;
            }

            // insert new data (加锁写入新创建的地图树指针)
            {
                std::lock_guard<std::mutex> lock(m_vmStaticMapMutex);
                instanceTree->second = newTree;
            }
        }
        return instanceTree->second->LoadMapTile(tileX, tileY, this);
    }

    //=========================================================
    // 卸载整张地图的所有资源并在无剩余图块时销毁 StaticMapTree

    void VMapManager2::unloadMap(unsigned int pMapId)
    {
        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(pMapId);
        if (instanceTree != iInstanceMapTrees.end() && instanceTree->second)
        {
            instanceTree->second->UnloadMap(this);
            if (instanceTree->second->numLoadedTiles() == 0)
            {
                delete instanceTree->second;
                instanceTree->second = nullptr;
            }
        }
    }

    //=========================================================
    // 卸载指定地图单个图块，若该图块是最后一块则同步释放 StaticMapTree

    void VMapManager2::unloadMap(unsigned int  pMapId, int x, int y)
    {
        InstanceTreeMap::iterator instanceTree = iInstanceMapTrees.find(pMapId);
        if (instanceTree != iInstanceMapTrees.end() && instanceTree->second)
        {
            instanceTree->second->UnloadMapTile(x, y, this);
            if (instanceTree->second->numLoadedTiles() == 0)
            {
                delete instanceTree->second;
                instanceTree->second = nullptr;
            }
        }
    }

    //==========================================================
    // 判断指定地图上两点间是否存在静态模型遮挡（可选忽略 M2 小型模型）

    bool VMapManager2::isInLineOfSight(unsigned int mapId, float x1, float y1, float z1, float x2, float y2, float z2, bool ignoreM2Model)
    {
        if (!isLineOfSightCalcEnabled()) return true;
        bool result = true;
        InstanceTreeMap::const_iterator instanceTree = GetMapTree(mapId);
        if (instanceTree != iInstanceMapTrees.end())
        {
            Vector3 pos1 = convertPositionToInternalRep(x1, y1, z1);
            Vector3 pos2 = convertPositionToInternalRep(x2, y2, z2);
            if (pos1 != pos2)
            {
                result = instanceTree->second->isInLineOfSight(pos1, pos2, ignoreM2Model);
            }
        }
        return result;
    }
    //=========================================================
    /**
    get the hit position and return true if we hit something
    otherwise the result pos will be the dest pos
    检测两点间静态模型碰撞点并将结果转换回游戏世界坐标
    */
    bool VMapManager2::getObjectHitPos(unsigned int mapId, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float& ry, float& rz, float pModifyDist)
    {
        bool result = false;
        rx = x2;
        ry = y2;
        rz = z2;
        if (isLineOfSightCalcEnabled())
        {
            InstanceTreeMap::const_iterator instanceTree = GetMapTree(mapId);
            if (instanceTree != iInstanceMapTrees.end())
            {
                Vector3 pos1 = convertPositionToInternalRep(x1, y1, z1);
                Vector3 pos2 = convertPositionToInternalRep(x2, y2, z2);
                Vector3 resultPos;
                result = instanceTree->second->getObjectHitPos(pos1, pos2, resultPos, pModifyDist);
                resultPos = convertPositionToInternalRep(resultPos.x, resultPos.y, resultPos.z);
                rx = resultPos.x;
                ry = resultPos.y;
                rz = resultPos.z;
            }
        }
        return result;
    }

    //=========================================================
    /**
    get height or INVALID_HEIGHT if no height available
    查询坐标处的静态模型表面高度，未命中模型时返回 VMAP_INVALID_HEIGHT_VALUE
    */

    float VMapManager2::getHeight(unsigned int mapId, float x, float y, float z, float maxSearchDist)
    {
        float height = VMAP_INVALID_HEIGHT_VALUE;           // no height
        if (isHeightCalcEnabled())
        {
            InstanceTreeMap::const_iterator instanceTree = GetMapTree(mapId);
            if (instanceTree != iInstanceMapTrees.end())
            {
                Vector3 pos = convertPositionToInternalRep(x, y, z);
                height = instanceTree->second->getHeight(pos, maxSearchDist);
                if (!(height < G3D::inf()))
                {
                    height = VMAP_INVALID_HEIGHT_VALUE;     // no height
                }
            }
        }
        return height;
    }

    //=========================================================
    // 查询坐标所在的 WMO 区域标志、ADT/Root/Group ID 与地面高度

    bool VMapManager2::getAreaInfo(unsigned int mapId, float x, float y, float& z, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const
    {
        bool result = false;
        InstanceTreeMap::const_iterator instanceTree = GetMapTree(mapId);
        if (instanceTree != iInstanceMapTrees.end())
        {
            Vector3 pos = convertPositionToInternalRep(x, y, z);
            result = instanceTree->second->getAreaInfo(pos, flags, adtId, rootId, groupId);
            // z is not touched by convertPositionToMangosRep(), so just copy
            z = pos.z;
        }
        return result;
    }

    // 将 WMO 液体类型编号映射为核心液体类型位掩码
    uint8 GetLiquidMask(uint32 type) // wotlk uses dbc
    {
        switch (type)
        {
            case 0: return MAP_LIQUID_TYPE_NO_WATER;
            case 1: return MAP_LIQUID_TYPE_WATER;
            case 2: return MAP_LIQUID_TYPE_OCEAN;
            case 3: return MAP_LIQUID_TYPE_MAGMA;
            case 4: return MAP_LIQUID_TYPE_SLIME;
            case 21: return MAP_LIQUID_TYPE_SLIME;
            case 41: return MAP_LIQUID_TYPE_WATER;
            case 61: return MAP_LIQUID_TYPE_WATER;
            default: return 0;
        }
    }

    // 查询世界坐标所在 WMO 建筑内部的液体高度、地面高度与液体类型
    bool VMapManager2::GetLiquidLevel(uint32 pMapId, float x, float y, float z, uint8 ReqLiquidTypeMask, float& level, float& floor, uint32& type) const
    {
        InstanceTreeMap::const_iterator instanceTree = iInstanceMapTrees.find(pMapId);
        if (instanceTree != iInstanceMapTrees.end() && instanceTree->second)
        {
            LocationInfo info;
            Vector3 pos = convertPositionToInternalRep(x, y, z);
            if (instanceTree->second->GetLocationInfo(pos, info))
            {
                floor = info.ground_Z;
                type = info.hitModel->GetLiquidType();
                if (ReqLiquidTypeMask && !(GetLiquidMask(type) & ReqLiquidTypeMask))
                    return false;
                if (info.hitInstance->GetLiquidLevel(pos, info, level))
                    return true;
            }
        }
        return false;
    }

    //=========================================================
    // 线程安全地获取（或从磁盘加载）共享 WorldModel 实例并递增引用计数

    WorldModel* VMapManager2::acquireModelInstance(const std::string& basepath, const std::string& filename)
    {
        std::lock_guard<std::mutex> lock(m_vmModelMutex);
        ModelFileMap::iterator model = iLoadedModelFiles.find(filename);
        if (model == iLoadedModelFiles.end())
        {
            WorldModel* worldmodel = new WorldModel();
            if (!worldmodel->readFile(basepath + filename + ".vmo"))
            {
                ERROR_LOG("VMapManager2: could not load '%s%s.vmo'!", basepath.c_str(), filename.c_str());
                delete worldmodel;
                return nullptr;
            }

            // insert new data
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "VMapManager2: loading file '%s%s'.", basepath.c_str(), filename.c_str());
            model = iLoadedModelFiles.insert(std::pair<std::string, ManagedModel>(filename, ManagedModel())).first;
            model->second.setModel(worldmodel);
        }
        model->second.incRefCount();
        return model->second.getModel();
    }

    // 递减共享 WorldModel 引用计数，当引用归零时释放内存并移出缓存表
    void VMapManager2::releaseModelInstance(const std::string& filename)
    {
        ModelFileMap::iterator model = iLoadedModelFiles.find(filename);
        if (model == iLoadedModelFiles.end())
        {
            ERROR_LOG("VMapManager2: trying to unload non-loaded file '%s'!", filename.c_str());
            return;
        }
        if (model->second.decRefCount() == 0)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "VMapManager2: unloading file '%s'", filename.c_str());
            delete model->second.getModel();
            iLoadedModelFiles.erase(model);
        }
    }
    //=========================================================

    bool VMapManager2::existsMap(const char* pBasePath, unsigned int mapId, int x, int y)
    {
        return StaticMapTree::CanLoadMap(std::string(pBasePath), mapId, x, y);
    }
} // namespace VMAP
