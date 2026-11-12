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

#ifndef _TILEASSEMBLER_H_
#define _TILEASSEMBLER_H_

#include <G3D/Vector3.h>
#include <G3D/Matrix3.h>
#include <map>
#include <set>

#include "ModelInstance.h"
#include "WorldModel.h"

namespace VMAP
{
    /**
    This Class is used to convert raw vector data into balanced BSP-Trees.
    To start the conversion call convertWorld().
    图块装配器相关类：将客户端提取的原始模型与放置数据汇编为均衡空间索引树 (.vmtree/.vmtile/.vmo)
    */
    //===============================================

    // 模型局部空间到世界空间的位置/旋转/缩放变换辅助器
    class ModelPosition
    {
        private:
            G3D::Matrix3 iRotation;
        public:
            G3D::Vector3 iPos;
            G3D::Vector3 iDir;
            float iScale;
            void init()                                         // 由欧拉角构建旋转矩阵
            {
                iRotation = G3D::Matrix3::fromEulerAnglesZYX(G3D::pi() * iDir.y / 180.f, G3D::pi() * iDir.x / 180.f, G3D::pi() * iDir.z / 180.f);
            }
            G3D::Vector3 transform(const G3D::Vector3& pIn) const; // 执行缩放、旋转和平移变换
            void moveToBasePos(const G3D::Vector3& pBasePos) { iPos -= pBasePos; }
    };

    typedef std::map<uint32, ModelSpawn> UniqueEntryMap;        // 地图唯一放置 ID -> ModelSpawn 映射
    typedef std::multimap<uint32, uint32> TileMap;              // 打包图块 ID -> 放置 ID 多重映射

    // 单张地图的所有模型放置汇总数据
    struct MapSpawns
    {
        UniqueEntryMap UniqueEntries;
        TileMap TileEntries;
    };

    typedef std::map<uint32, MapSpawns*> MapData;
    //===============================================

    // 原始 WMO 组模型数据（未建树前的顶点、三角面与液体信息）
    struct GroupModel_Raw
    {
        uint32 mogpflags;
        uint32 GroupWMOID;

        G3D::AABox bounds;
        uint32 liquidflags;
        std::vector<MeshTriangle> triangles;
        std::vector<G3D::Vector3> vertexArray;
        class WmoLiquid* liquid;

        GroupModel_Raw() : mogpflags(0), GroupWMOID(0), liquidflags(0), liquid(nullptr) {}
        ~GroupModel_Raw();

        bool Read(FILE* rf);                                    // 从原始提取文件读取组网格与液体块
    };

    // 原始世界模型数据（包含所有子组模型数组）
    struct WorldModel_Raw
    {
        uint32 RootWMOID;
        std::vector<GroupModel_Raw> groupsArray;

        bool Read(const char* path);                            // 从磁盘读取原始 .vmo 提取文件
    };

    // 图块装配器：读取原始提取目录，计算世界包围盒、构建并写出 .vmtree/.vmtile/.vmo 与 GO 模型列表
    class TileAssembler
    {
        private:
            std::string iDestDir;                               // 输出 vmaps 目录
            std::string iSrcDir;                                // 原始数据输入目录
            MapData mapData;                                    // 按地图 ID 分组的模型放置记录
            std::set<std::string> spawnedModelFiles;            // 需转换的唯一模型文件名集合

        public:
            TileAssembler(const std::string& pSrcDirName, const std::string& pDestDirName);
            virtual ~TileAssembler();

            bool convertWorld2();                               // 主转换入口：汇编全部地图树、图块及模型文件
            bool readMapSpawns();                               // 读取 dir_bin 地图模型放置清单
            bool calculateTransformedBound(ModelSpawn& spawn);  // 读取原始模型几何并计算变换后的世界包围盒

            void exportGameobjectModels();                      // 导出 GameObject 模型包围盒清单
            bool convertRawFile(const std::string& pModelFilename); // 将单个原始模型文件转换为带 BIH 树的成品 WorldModel
    };
}                                                           // VMAP
#endif                                                      /*_TILEASSEMBLER_H_*/
