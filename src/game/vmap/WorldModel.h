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

#ifndef _WORLDMODEL_H
#define _WORLDMODEL_H

#include <G3D/Vector3.h>
#include <G3D/AABox.h>
#include <G3D/Ray.h>
#include "BIH.h"

#include "Platform/Define.h"

namespace VMAP
{
    class TreeNode;
    struct AreaInfo;
    struct GroupLocationInfo;
    struct LocationInfo;

    // 三角面片顶点索引三元组
    class MeshTriangle
    {
        public:
            MeshTriangle(): idx0(0), idx1(0), idx2(0)
            {};
            MeshTriangle(uint32 na, uint32 nb, uint32 nc): idx0(na), idx1(nb), idx2(nc) {};

            uint32 idx0;
            uint32 idx1;
            uint32 idx2;
    };

    // WMO 内部液体网格数据（存储每个液体单元的开关标志及水面高度场）
    class WmoLiquid
    {
        public:
            WmoLiquid(uint32 width, uint32 height, const Vector3& corner, uint32 type);
            WmoLiquid(const WmoLiquid& other);
            ~WmoLiquid();
            WmoLiquid& operator=(const WmoLiquid& other);
            bool GetLiquidHeight(const Vector3& pos, float& liqHeight) const; // 查询局部坐标处的水面高度
            uint32 GetType() const { return iType; }
            float* GetHeightStorage() const { return iHeight; }
            uint8* GetFlagsStorage() const { return iFlags; }
            uint32 GetFileSize() const;
            bool writeToFile(FILE* wf);                                 // 序列化液体网格到文件
            static bool readFromFile(FILE* rf, WmoLiquid*& out);        // 从文件反序列化液体网格
        private:
            WmoLiquid() : iTilesX(0), iTilesY(0), iType(0), iHeight(nullptr), iFlags(nullptr) {};
            uint32 iTilesX;  //!< number of tiles in x direction, each (X 方向液体格数)
            uint32 iTilesY;                                             // Y 方向液体格数
            Vector3 iCorner; //!< the lower corner (液体网格左下基准角点坐标)
            uint32 iType;    //!< liquid type (液体类型 ID，如水/岩浆/毒液)
            float* iHeight;  //!< (tilesX + 1)*(tilesY + 1) height values (网格顶点高度数组)
            uint8* iFlags;   //!< info if liquid tile is used (各格子是否包含液体的标记数组)
#ifdef MMAP_GENERATOR
        public:
            void getPosInfo(uint32& tilesX, uint32& tilesY, Vector3& corner) const;
#endif
    };

    /*! holding additional info for WMO group files (WMO 子组几何模型：包含顶点、三角面 BIH 树与可选液体数据) */
    class GroupModel
    {
        public:
            GroupModel() : iMogpFlags(0), iGroupWMOID(0), iLiquid(nullptr) {}
            GroupModel(const GroupModel& other);
            GroupModel(uint32 mogpFlags, uint32 groupWMOID, const AABox& bound):
                iBound(bound), iMogpFlags(mogpFlags), iGroupWMOID(groupWMOID), iLiquid(nullptr) {}
            ~GroupModel() { delete iLiquid; }

            //! pass mesh data to object and create BIH. Passed vectors get get swapped with old geometry! (交换写入网格数据并构建三角面 BIH 树)
            void setMeshData(std::vector<Vector3>& vert, std::vector<MeshTriangle>& tri);
            void setLiquidData(WmoLiquid*& liquid) { iLiquid = liquid; liquid = nullptr; }
            bool IntersectRay(const G3D::Ray& ray, float& distance, bool stopAtFirstHit, bool ignoreM2Model = false) const; // 射线与组网格求交
            bool IsInsideObject(const Vector3& pos, const Vector3& down, float& z_dist) const; // 判断点是否处于该封闭/半封闭组模型内部
            bool GetLiquidLevel(const Vector3& pos, float& liqHeight) const; // 查询组内液体高度
            uint32 GetLiquidType() const;
            bool writeToFile(FILE* wf);
            bool readFromFile(FILE* rf);
            const G3D::AABox& GetBound() const { return iBound; }
            uint32 GetMogpFlags() const { return iMogpFlags; }
            uint32 GetWmoID() const { return iGroupWMOID; }
        protected:
            G3D::AABox iBound;                                          // 组模型局部包围盒
            uint32 iMogpFlags;// 0x8 outdor; 0x2000 indoor (MOGP 室内/室外标志位)
            uint32 iGroupWMOID;                                         // 组 WMO 唯一 ID
            std::vector<Vector3> vertices;                              // 顶点坐标列表
            std::vector<MeshTriangle> triangles;                        // 三角面索引列表
            BIH meshTree;                                               // 三角面 BIH 加速树
            WmoLiquid* iLiquid;                                         // 组内液体数据指针（可选）

#ifdef MMAP_GENERATOR
        public:
            void getMeshData(std::vector<Vector3>& vertices, std::vector<MeshTriangle>& triangles, WmoLiquid*& liquid);
#endif
    };
    /*! Holds a model (converted M2 or WMO) in its original coordinate space (世界几何模型资产：由一个或多个 GroupModel 及其顶层 BIH 构成) */
    class WorldModel
    {
        public:
            WorldModel(): RootWMOID(0), modelFlags(0) {}

            //! pass group models to WorldModel and create BIH. Passed vector is swapped with old geometry! (设置子组模型列表并构建组级别 BIH 树)
            void setGroupModels(std::vector<GroupModel>& models);
            void setRootWmoID(uint32 id) { RootWMOID = id; }
            bool IntersectRay(const G3D::Ray& ray, float& distance, bool stopAtFirstHit, bool ignoreM2Model = false) const; // 射线与世界模型求交
            bool IntersectPoint(const G3D::Vector3& p, const G3D::Vector3& down, float& dist, AreaInfo& info) const; // 空间点查询所属组模型区域标志与地面高度
            bool GetLocationInfo(const G3D::Vector3& p, const G3D::Vector3& down, float& dist, GroupLocationInfo& info) const; // 空间点查询命中的组模型及 Root ID
            bool writeFile(const std::string& filename);                // 写入 .vmo 成品模型文件
            bool readFile(const std::string& filename);                 // 从 .vmo 文件加载模型
            void setModelFlags(uint32 newFlags) { modelFlags = newFlags; }
            uint32 getModelFlags() const { return modelFlags; }
        protected:
            uint32 RootWMOID;                                           // Root WMO ID
            std::vector<GroupModel> groupModels;                        // 子组模型数组
            BIH groupTree;                                              // 子组模型 BIH 加速树
            uint32 modelFlags;                                          // 模型标志（如 MOD_M2）

#ifdef MMAP_GENERATOR
        public:
            void getGroupModels(std::vector<GroupModel>& groupModels);
#endif
    };
} // namespace VMAP

#endif // _WORLDMODEL_H
