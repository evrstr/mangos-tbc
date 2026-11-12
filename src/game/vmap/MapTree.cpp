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

#include "MapTree.h"
#include "ModelInstance.h"
#include "VMapManager2.h"
#include "VMapDefinitions.h"
#include "WorldModel.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <limits>

using G3D::Vector3;

namespace VMAP
{
    // 静态地图射线相交回调：将 BIH 节点索引映射至 ModelInstance::intersectRay
    class MapRayCallback
    {
        public:
            MapRayCallback(ModelInstance* val): prims(val), hit(false) {}
            bool operator()(const G3D::Ray& ray, uint32 entry, float& distance, bool pStopAtFirstHit = true, bool ignoreM2Model = false)
            {
                bool result = prims[entry].intersectRay(ray, distance, pStopAtFirstHit, ignoreM2Model);
                if (result)
                    hit = true;
                return result;
            }
            bool didHit() const { return hit; }
        protected:
            ModelInstance* prims;
            bool hit;
    };

    // WMO 区域信息点查询回调
    class AreaInfoCallback
    {
        public:
            AreaInfoCallback(ModelInstance* val): prims(val) {}
            void operator()(const Vector3& point, uint32 entry)
            {
#ifdef VMAP_DEBUG
                DEBUG_LOG("trying to intersect '%s'", prims[entry].name.c_str());
#endif
                prims[entry].intersectPoint(point, aInfo);
            }

            ModelInstance* prims;
            AreaInfo aInfo;
    };

    // 组模型及液体定位点查询回调
    class LocationInfoCallback
    {
        public:
            LocationInfoCallback(ModelInstance* val, LocationInfo& info): prims(val), locInfo(info), result(false) {}
            void operator()(const Vector3& point, uint32 entry)
            {
#ifdef VMAP_DEBUG
                DEBUG_LOG("trying to intersect '%s'", prims[entry].name.c_str());
#endif
                if (prims[entry].GetLocationInfo(point, locInfo))
                    result = true;
            }

            ModelInstance* prims;
            LocationInfo& locInfo;
            bool result;
    };


    //=========================================================
    // 构造图块文件名（注意文件名顺序为 mapID_Y_X.vmtile）
    std::string StaticMapTree::getTileFileName(uint32 mapID, uint32 tileX, uint32 tileY)
    {
        std::stringstream tilefilename;
        tilefilename.fill('0');
        tilefilename << std::setw(3) << mapID << "_";
        // tilefilename << std::setw(2) << tileX << "_" << std::setw(2) << tileY << ".vmtile";
        tilefilename << std::setw(2) << tileY << "_" << std::setw(2) << tileX << ".vmtile";
        return tilefilename.str();
    }

    // 点查询所在 WMO 的区域标志、ADT ID、Root/Group ID，并将 pos.z 修正为模型地面高度
    bool StaticMapTree::getAreaInfo(Vector3& pos, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const
    {
        AreaInfoCallback intersectionCallBack(iTreeValues);
        iTree.intersectPoint(pos, intersectionCallBack);
        if (intersectionCallBack.aInfo.result)
        {
            flags = intersectionCallBack.aInfo.flags;
            adtId = intersectionCallBack.aInfo.adtId;
            rootId = intersectionCallBack.aInfo.rootId;
            groupId = intersectionCallBack.aInfo.groupId;
            pos.z = intersectionCallBack.aInfo.ground_Z;
            return true;
        }
        return false;
    }

    // 点查询所在静态模型实例、组模型及地面高度
    bool StaticMapTree::GetLocationInfo(const Vector3& pos, LocationInfo& info) const
    {
        LocationInfoCallback intersectionCallBack(iTreeValues, info);
        iTree.intersectPoint(pos, intersectionCallBack);
        return intersectionCallBack.result;
    }

    // 构造函数：规范化 vmaps 根目录路径末尾斜杠
    StaticMapTree::StaticMapTree(uint32 mapID, const std::string& basePath):
        iMapID(mapID), iIsTiled(false), iTreeValues(nullptr), iNTreeValues(0), iBasePath(basePath)
    {
        if (iBasePath.length() > 0 && (iBasePath[iBasePath.length() - 1] != '/' && iBasePath[iBasePath.length() - 1] != '\\'))
        {
            iBasePath.append("/");
        }
    }

    //=========================================================
    //! Make sure to call unloadMap() to unregister acquired model references before destroying (析构前务必先调用 UnloadMap 释放共享模型引用)
    StaticMapTree::~StaticMapTree()
    {
        delete[] iTreeValues;
    }

    //=========================================================
    /**
    If intersection is found within pMaxDist, sets pMaxDist to intersection distance and returns true.
    Else, pMaxDist is not modified and returns false;
    利用静态 BIH 树计算射线最短命中距离
    */

    bool StaticMapTree::getIntersectionTime(const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit, bool ignoreM2Model) const
    {
        float distance = pMaxDist;
        MapRayCallback intersectionCallBack(iTreeValues);
        iTree.intersectRay(pRay, intersectionCallBack, distance, pStopAtFirstHit, ignoreM2Model);
        if (intersectionCallBack.didHit())
            pMaxDist = distance;
        return intersectionCallBack.didHit();
    }
    //=========================================================

    // 两点间静态模型视线遮挡检测
    bool StaticMapTree::isInLineOfSight(const Vector3& pos1, const Vector3& pos2, bool ignoreM2Model) const
    {
        float maxDist = (pos2 - pos1).magnitude();
        // valid map coords should *never ever* produce float overflow, but this would produce NaNs too:
        MANGOS_ASSERT(maxDist < std::numeric_limits<float>::max());
        MANGOS_ASSERT(!std::isnan(maxDist));
        MANGOS_ASSERT(std::isfinite(maxDist));
        // prevent NaN values which can cause BIH intersection to enter infinite loop
        if (maxDist < 1e-10f)
            return true;
        // direction with length of 1
        G3D::Ray ray = G3D::Ray::fromOriginAndDirection(pos1, (pos2 - pos1) / maxDist);
        return !getIntersectionTime(ray, maxDist, true, ignoreM2Model);
    }
    //=========================================================
    /**
    When moving from pos1 to pos2 check if we hit an object. Return true and the position if we hit one
    Return the hit pos or the original dest pos
    检测射线与静态模型碰撞点，并根据 pModifyDist 微调回退位置
    */

    bool StaticMapTree::getObjectHitPos(const Vector3& pPos1, const Vector3& pPos2, Vector3& pResultHitPos, float pModifyDist) const
    {
        float maxDist = (pPos2 - pPos1).magnitude();
        // valid map coords should *never ever* produce float overflow, but this would produce NaNs too:
        MANGOS_ASSERT(maxDist < std::numeric_limits<float>::max());
        MANGOS_ASSERT(!std::isnan(maxDist));
        MANGOS_ASSERT(std::isfinite(maxDist));
        // prevent NaN values which can cause BIH intersection to enter infinite loop
        if (maxDist < 1e-10f)
        {
            pResultHitPos = pPos2;
            return false;
        }
        Vector3 dir = (pPos2 - pPos1) / maxDist;            // direction with length of 1
        G3D::Ray ray(pPos1, dir);
        float dist = maxDist;
        if (getIntersectionTime(ray, dist))
        {
            pResultHitPos = pPos1 + dir * dist;
            if (pModifyDist < 0)
            {
                if ((pResultHitPos - pPos1).magnitude() > -pModifyDist)
                {
                    pResultHitPos = pResultHitPos + dir * pModifyDist;
                }
                else
                {
                    pResultHitPos = pPos1;
                }
            }
            else
            {
                pResultHitPos = pResultHitPos + dir * pModifyDist;
            }
            return true;
        }
        pResultHitPos = pPos2;
        return false;
    }

    //=========================================================
    // 沿垂直方向（maxSearchDist >= 0 向下，否则向上）探测静态模型表面高度
    float StaticMapTree::getHeight(const Vector3& pPos, float maxSearchDist) const
    {
        MANGOS_ASSERT(maxSearchDist < std::numeric_limits<float>::max());
        MANGOS_ASSERT(maxSearchDist > std::numeric_limits<float>::lowest());
        MANGOS_ASSERT(!std::isnan(maxSearchDist));
        MANGOS_ASSERT(std::isfinite(maxSearchDist));

        float height = G3D::inf();
        Vector3 dir;
        if (maxSearchDist >= 0.f)
            dir = Vector3(0, 0, -1);
        else
            dir = Vector3(0, 0, 1);
        G3D::Ray ray(pPos, dir); // direction with length of 1
        float maxDist = std::abs(maxSearchDist);
        if (getIntersectionTime(ray, maxDist))
        {
            if (maxSearchDist >= 0.f)
                height = pPos.z - maxDist;
            else
                height = pPos.z + maxDist;
        }
        return height;
    }

    //=========================================================
    // 校验 .vmtree 文件魔数；若地图为分块模式，进一步校验对应 .vmtile 文件是否存在且魔数正确
    bool StaticMapTree::CanLoadMap(const std::string& vmapPath, uint32 mapID, uint32 tileX, uint32 tileY)
    {
        std::string basePath = vmapPath;
        if (basePath.length() > 0 && (basePath[basePath.length() - 1] != '/' && basePath[basePath.length() - 1] != '\\'))
            basePath.append("/");
        std::string fullname = basePath + VMapManager2::getMapFileName(mapID);
        bool success = true;
        FILE* rf = fopen(fullname.c_str(), "rb");
        if (!rf)
            return false;
        // TODO: check magic number when implemented...
        char tiled;
        char chunk[8];
        if (!readChunk(rf, chunk, VMAP_MAGIC, 8) || fread(&tiled, sizeof(char), 1, rf) != 1)
        {
            fclose(rf);
            return false;
        }
        if (tiled)
        {
            std::string tilefile = basePath + getTileFileName(mapID, tileX, tileY);
            FILE* tf = fopen(tilefile.c_str(), "rb");
            if (!tf)
                success = false;
            else
            {
                if (!readChunk(tf, chunk, VMAP_MAGIC, 8))
                    success = false;
                fclose(tf);
            }
        }
        fclose(rf);
        return success;
    }

    //=========================================================
    // 从 .vmtree 文件加载地图 BIH 树结构；若为非分块地图则直接载入全部全局模型实例

    bool StaticMapTree::InitMap(const std::string& fname, VMapManager2* vm)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "Initializing StaticMapTree '%s'", fname.c_str());
        bool success = true;
        std::string fullname = iBasePath + fname;
        FILE* rf = fopen(fullname.c_str(), "rb");
        if (!rf)
            return false;
        else
        {
            char chunk[8];
            // general info
            if (!readChunk(rf, chunk, VMAP_MAGIC, 8)) success = false;
            char tiled = 0;
            if (success && fread(&tiled, sizeof(char), 1, rf) != 1) success = false;
            iIsTiled = !!tiled;
            // Nodes
            if (success && !readChunk(rf, chunk, "NODE", 4)) success = false;
            if (success) success = iTree.readFromFile(rf);
            if (success)
            {
                iNTreeValues = iTree.primCount();
                iTreeValues = new ModelInstance[iNTreeValues];
            }

            if (success && !readChunk(rf, chunk, "GOBJ", 4)) success = false;
            // global model spawns
            // only non-tiled maps have them, and if so exactly one (so far at least...)
#ifdef VMAP_DEBUG
            DEBUG_LOG("Map isTiled: %u", static_cast<uint32>(iIsTiled));
#endif
            if (!iIsTiled)
            {
                // read model spawns
                ModelSpawn spawn;
                while (ModelSpawn::readFromFile(rf, spawn))
                {
                    // acquire model instance
                    WorldModel* model = vm->acquireModelInstance(iBasePath, spawn.name);
                    if (model)
                        model->setModelFlags(spawn.flags);
                    else
                        ERROR_LOG("StaticMapTree::LoadMapTile() could not acquire WorldModel pointer for '%s'!", spawn.name.c_str());

                    // update tree
                    uint32 referencedVal;

                    fread(&referencedVal, sizeof(uint32), 1, rf);
                    if (!iLoadedSpawns.count(referencedVal))
                    {
                        if (referencedVal > iNTreeValues)
                        {
                            ERROR_LOG("invalid tree element! (%u/%u)", referencedVal, iNTreeValues);
                            continue;
                        }

                        iTreeValues[referencedVal] = ModelInstance(spawn, model);
                        iLoadedSpawns[referencedVal] = 1;
                    }
                    else
                    {
                        ++iLoadedSpawns[referencedVal];
#ifdef VMAP_DEBUG
                        if (iTreeValues[referencedVal].ID != spawn.ID)
                            DEBUG_LOG("Error: trying to load wrong spawn in node!");
                        else if (iTreeValues[referencedVal].name != spawn.name)
                            DEBUG_LOG("Error: name mismatch on GUID=%u", spawn.ID);
#endif
                    }
                }
            }

            fclose(rf);
        }
        return success;
    }

    //=========================================================
    // 卸载整张地图的所有已加载模型引用并清空图块与放置记录

    void StaticMapTree::UnloadMap(VMapManager2* vm)
    {
        for (auto& iLoadedSpawn : iLoadedSpawns)
        {
            iTreeValues[iLoadedSpawn.first].setUnloaded();
            for (uint32 refCount = 0; refCount < iLoadedSpawn.second; ++refCount)
                vm->releaseModelInstance(iTreeValues[iLoadedSpawn.first].name);
        }
        iLoadedSpawns.clear();
        iLoadedTiles.clear();
    }

    //=========================================================
    // 加载单个 .vmtile 图块中的所有模型实例并递增对应树节点的引用计数

    bool StaticMapTree::LoadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm)
    {
        if (!iIsTiled)
        {
            // currently, core creates grids for all maps, whether it has terrain tiles or not
            // so we need "fake" tile loads to know when we can unload map geometry
            iLoadedTiles[packTileID(tileX, tileY)] = false;
            return true;
        }
        if (!iTreeValues)
        {
            ERROR_LOG("StaticMapTree::LoadMapTile(): Tree has not been initialized! [%u,%u]", tileX, tileY);
            return false;
        }
        bool result = true;

        std::string tilefile = iBasePath + getTileFileName(iMapID, tileX, tileY);
        FILE* tf = fopen(tilefile.c_str(), "rb");
        if (tf)
        {
            char chunk[8];
            if (!readChunk(tf, chunk, VMAP_MAGIC, 8))
                result = false;
            uint32 numSpawns = 0;
            if (result && fread(&numSpawns, sizeof(uint32), 1, tf) != 1)
                result = false;
            for (uint32 i = 0; i < numSpawns && result; ++i)
            {
                // read model spawns
                ModelSpawn spawn;
                result = ModelSpawn::readFromFile(tf, spawn);
                if (result)
                {
                    // acquire model instance
                    WorldModel* model = vm->acquireModelInstance(iBasePath, spawn.name);
                    if (model)
                        model->setModelFlags(spawn.flags);
                    else
                        ERROR_LOG("StaticMapTree::LoadMapTile() could not acquire WorldModel pointer for '%s'!", spawn.name.c_str());

                    // update tree
                    uint32 referencedVal;

                    fread(&referencedVal, sizeof(uint32), 1, tf);
                    if (!iLoadedSpawns.count(referencedVal))
                    {
                        if (referencedVal > iNTreeValues)
                        {
                            ERROR_LOG("invalid tree element! (%u/%u)", referencedVal, iNTreeValues);
                            continue;
                        }

                        iTreeValues[referencedVal] = ModelInstance(spawn, model);
                        iLoadedSpawns[referencedVal] = 1;
                    }
                    else
                    {
                        ++iLoadedSpawns[referencedVal];
#ifdef VMAP_DEBUG
                        if (iTreeValues[referencedVal].ID != spawn.ID)
                            DEBUG_LOG("Error: trying to load wrong spawn in node!");
                        else if (iTreeValues[referencedVal].name != spawn.name)
                            DEBUG_LOG("Error: name mismatch on GUID=%u", spawn.ID);
#endif
                    }
                }
            }
            iLoadedTiles[packTileID(tileX, tileY)] = true;
            fclose(tf);
        }
        else
            iLoadedTiles[packTileID(tileX, tileY)] = false;
        return result;
    }

    //=========================================================
    // 卸载单个图块：递减所关联模型实例的引用计数，并在归零时释放节点与共享模型

    void StaticMapTree::UnloadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm)
    {
        uint32 tileID = packTileID(tileX, tileY);
        loadedTileMap::iterator tile = iLoadedTiles.find(tileID);
        if (tile == iLoadedTiles.end())
        {
            //ERROR_LOG("StaticMapTree::UnloadMapTile(): Trying to unload non-loaded tile. Map:%u X:%u Y:%u", iMapID, tileX, tileY);
            return;
        }
        if (tile->second) // file associated with tile
        {
            std::string tilefile = iBasePath + getTileFileName(iMapID, tileX, tileY);
            FILE* tf = fopen(tilefile.c_str(), "rb");
            if (tf)
            {
                bool result = true;
                char chunk[8];
                if (!readChunk(tf, chunk, VMAP_MAGIC, 8))
                    result = false;
                uint32 numSpawns;
                if (fread(&numSpawns, sizeof(uint32), 1, tf) != 1)
                    result = false;
                for (uint32 i = 0; i < numSpawns && result; ++i)
                {
                    // read model spawns
                    ModelSpawn spawn;
                    result = ModelSpawn::readFromFile(tf, spawn);
                    if (result)
                    {
                        // release model instance
                        vm->releaseModelInstance(spawn.name);

                        // update tree
                        uint32 referencedNode;

                        fread(&referencedNode, sizeof(uint32), 1, tf);
                        if (!iLoadedSpawns.count(referencedNode))
                        {
                            ERROR_LOG("Trying to unload non-referenced model '%s' (ID:%u)", spawn.name.c_str(), spawn.ID);
                        }
                        else if (--iLoadedSpawns[referencedNode] == 0)
                        {
                            iTreeValues[referencedNode].setUnloaded();
                            iLoadedSpawns.erase(referencedNode);
                        }
                    }
                }
                fclose(tf);
            }
        }
        iLoadedTiles.erase(tile);
    }
}
