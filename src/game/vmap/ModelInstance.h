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

#ifndef _MODELINSTANCE_H_
#define _MODELINSTANCE_H_

#include <G3D/Matrix3.h>
#include <G3D/Vector3.h>
#include <G3D/AABox.h>
#include <G3D/Ray.h>

#include "Platform/Define.h"

namespace VMAP
{
    class WorldModel;
    struct AreaInfo;
    struct LocationInfo;

    // 模型生成属性标志位
    enum ModelFlags
    {
        MOD_M2 = 1,                                             // 小型装饰/树木等 M2 模型（支持视线检测时忽略）
        MOD_WORLDSPAWN = 1 << 1,                                // 地图全局建筑模型
        MOD_HAS_BOUND = 1 << 2                                  // 是否显式序列化存储了世界空间包围盒
    };

    // 地图静态模型放置描述（对应客户端 ADT/WDT 摆放信息：位置、旋转、缩放、包围盒与模型名）
    class ModelSpawn
    {
        public:
            // mapID, tileX, tileY, Flags, ID, Pos, Rot, Scale, Bound_lo, Bound_hi, name
            uint32 flags;                                       // ModelFlags 标志位
            uint16 adtId;                                       // 所属 ADT 区域编号
            uint32 ID;                                          // 唯一放置 ID
            G3D::Vector3 iPos;                                  // 世界坐标位置
            G3D::Vector3 iRot;                                  // 欧拉角旋转（度）
            float iScale;                                       // 缩放系数
            G3D::AABox iBound;                                  // 世界坐标轴对齐包围盒
            std::string name;                                   // 模型文件名称 (.vmo)
            bool operator==(const ModelSpawn& other) const { return ID == other.ID; }
            // uint32 hashCode() const { return ID; }
            // temp?
            const G3D::AABox& getBounds() const { return iBound; }


            static bool readFromFile(FILE* rf, ModelSpawn& spawn);        // 从二进制文件读取模型放置描述
            static bool writeToFile(FILE* wf, const ModelSpawn& spawn);   // 将模型放置描述写入二进制文件
    };

    // 运行期模型实例：持有共享 WorldModel 指针与预计算的逆旋转矩阵，负责世界/局部坐标空间转换及求交
    class ModelInstance: public ModelSpawn
    {
        public:
            ModelInstance(): iInvScale(0), iModel(nullptr) {}
            ModelInstance(const ModelSpawn& spawn, WorldModel* model);
            void setUnloaded() { iModel = nullptr; }            // 置空模型指针（卸载图块时调用）
            bool intersectRay(const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit, bool ignoreM2Model = false) const; // 射线求交
            void intersectPoint(const G3D::Vector3& p, AreaInfo& info) const; // 点查询所属 WMO 区域信息
            bool GetLocationInfo(const G3D::Vector3& p, LocationInfo& info) const; // 点查询所在组模型与地面高度
            bool GetLiquidLevel(const G3D::Vector3& p, LocationInfo& info, float& liqHeight) const; // 查询局部位置液体高度
        protected:
            G3D::Matrix3 iInvRot;                               // 逆旋转矩阵
            float iInvScale;                                    // 缩放倒数 (1 / iScale)
            WorldModel* iModel;                                 // 共享的世界几何体模型指针

#ifdef MMAP_GENERATOR
        public:
            WorldModel* const getWorldModel();
#endif
    };
} // namespace VMAP

#endif // _MODELINSTANCE
