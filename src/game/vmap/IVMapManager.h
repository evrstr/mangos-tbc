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

#ifndef _IVMAPMANAGER_H
#define _IVMAPMANAGER_H

#include <string>
#include <Platform/Define.h>
#include <vector>

//===========================================================

/**
This is the minimum interface to the VMapMamager.
VMapManager 最小公共抽象接口定义
*/

namespace VMAP
{
    // VMAP 地图块加载结果枚举
    enum VMAPLoadResult
    {
        VMAP_LOAD_RESULT_ERROR,                                         // 加载失败（文件损坏或读取错误）
        VMAP_LOAD_RESULT_OK,                                            // 加载成功
        VMAP_LOAD_RESULT_IGNORED,                                       // 已忽略（如配置关闭或该图块无静态模型）
    };

#define VMAP_INVALID_HEIGHT       -100000.0f            // for check (高度有效性检查阈值)
#define VMAP_INVALID_HEIGHT_VALUE -200000.0f            // real assigned value in unknown height case (无法探测到地表/建筑高度时的无效哨兵值)

    //===========================================================
    // 虚拟地图 (VMAP) 管理器抽象接口：提供地图静态几何体加载、视线检测、高度采样与区域/液体查询
    class IVMapManager
    {
        private:
            bool iEnableLineOfSightCalc;                                // 是否启用视线遮挡计算
            bool iEnableHeightCalc;                                     // 是否启用建筑/模型高度计算

        public:
            IVMapManager() : iEnableLineOfSightCalc(true), iEnableHeightCalc(true) {}

            virtual ~IVMapManager(void) {}

            virtual void InitializeThreadUnsafe(const std::vector<uint32>& mapIds) = 0; // 启动期单线程预初始化分片或地图表

            virtual VMAPLoadResult loadMap(const char* pBasePath, unsigned int pMapId, int x, int y) = 0; // 加载指定地图指定图块

            virtual bool existsMap(const char* pBasePath, unsigned int pMapId, int x, int y) = 0; // 检查指定图块文件是否存在

            virtual void unloadMap(unsigned int pMapId, int x, int y) = 0; // 卸载指定图块
            virtual void unloadMap(unsigned int pMapId) = 0;            // 卸载整张地图的所有图块与模型树

            virtual bool isInLineOfSight(unsigned int pMapId, float x1, float y1, float z1, float x2, float y2, float z2, bool ignoreM2Model) = 0; // 判断两点之间视线是否通畅
            virtual float getHeight(unsigned int pMapId, float x, float y, float z, float maxSearchDist) = 0; // 获取给定坐标下方最近的模型地面高度
            /**
            test if we hit an object. return true if we hit one. rx,ry,rz will hold the hit position or the dest position, if no intersection was found
            return a position, that is pReduceDist closer to the origin
            检测射线是否命中静态模型：命中返回 true 并写入经 pModifyDist 微调回退后的碰撞点，未命中则返回终点
            */
            virtual bool getObjectHitPos(unsigned int pMapId, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float& ry, float& rz, float pModifyDist) = 0;
            /**
            send debug commands (执行调试命令接口)
            */
            virtual bool processCommand(char* pCommand) = 0;

            /**
            Enable/disable LOS calculation
            It is enabled by default. If it is enabled in mid game the maps have to loaded manualy
            启用/禁用视线遮挡计算
            */
            void setEnableLineOfSightCalc(bool pVal) { iEnableLineOfSightCalc = pVal; }
            /**
            Enable/disable model height calculation
            It is enabled by default. If it is enabled in mid game the maps have to loaded manualy
            启用/禁用模型高度计算
            */
            void setEnableHeightCalc(bool pVal) { iEnableHeightCalc = pVal; }

            bool isLineOfSightCalcEnabled() const { return iEnableLineOfSightCalc; }
            bool isHeightCalcEnabled() const { return iEnableHeightCalc; }
            bool isMapLoadingEnabled() const { return iEnableLineOfSightCalc || iEnableHeightCalc; }

            virtual std::string getDirFileName(unsigned int pMapId, int x, int y) const = 0; // 生成图块文件名
            virtual bool IsTileLoaded(uint32 mapId, uint32 x, uint32 y) const = 0; // 查询指定图块是否已加载
            /**
            Query world model area info.
            \param z gets adjusted to the ground height for which this are info is valid
            查询世界模型区域信息（WMO 区域标志、ADT ID、Root/Group ID），同时将 z 修正为有效地面高度
            */
            virtual bool getAreaInfo(unsigned int pMapId, float x, float y, float& z, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const = 0;
            virtual bool GetLiquidLevel(uint32 pMapId, float x, float y, float z, uint8 ReqLiquidType, float& level, float& floor, uint32& type) const = 0; // 查询模型内部液体高度（如城市水池、地下河）
    };
}
#endif
