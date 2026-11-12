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

#ifndef _MOVE_MAP_SHARED_DEFINES_H
#define _MOVE_MAP_SHARED_DEFINES_H

#include "Platform/Define.h"
#include <Detour/Include/DetourNavMesh.h>

#define MMAP_MAGIC 0x4d4d4150   // 'MMAP' 魔数标识
#define MMAP_VERSION 8          // MMAP 导航网格文件版本号

/// 单个 .mmtile 瓦片文件头部信息
struct MmapTileHeader
{
    uint32 mmapMagic;     // 魔数 'MMAP'
    uint32 dtVersion;     // Detour 导航网格数据格式版本
    uint32 mmapVersion;   // MaNGOS MMAP 格式版本
    uint32 size;          // 瓦片数据体字节长度
    uint32 usesLiquids;   // 是否包含水体/岩浆等液体区域标记

    MmapTileHeader() : mmapMagic(MMAP_MAGIC), dtVersion(DT_NAVMESH_VERSION),
        mmapVersion(MMAP_VERSION), size(0), usesLiquids(0) {}
};

/// 导航网格多边形区域类型 ID（值越大在体素合并时优先级越高）
enum NavArea
{
    NAV_AREA_EMPTY          = 0,
    // areas 1-60 will be used for destructible areas (currently skipped in vmaps, WMO with flag 1)
    // ground is the highest value to make recast choose ground over water when merging surfaces very close to each other (shallow water would be walkable)
    // 地面取最高优先级，使浅水区在体素合并时保留可行走的地面属性
    NAV_AREA_GROUND         = 11,
    NAV_AREA_GROUND_STEEP   = 10, // unused in cmangos - keeping it for structure (陡坡，cmangos 中保留占位未使用)
    NAV_AREA_WATER          = 9,  // 水域
    NAV_AREA_MAGMA_SLIME    = 8, // don't need to differentiate between them (岩浆/史莱姆危险液体，无需细分)
    NAV_AREA_MAX_VALUE      = NAV_AREA_GROUND,
    NAV_AREA_MIN_VALUE      = NAV_AREA_MAGMA_SLIME,
    NAV_AREA_ALL_MASK       = 0x3F // max allowed value
};

/// 导航多边形通行标志位（与 Detour PolyFlags 对应）
enum NavTerrainFlag
{
    NAV_EMPTY        = 0x00,
    NAV_GROUND       = 1 << (NAV_AREA_MAX_VALUE - NAV_AREA_GROUND),       // 普通陆地
    NAV_GROUND_STEEP = 1 << (NAV_AREA_MAX_VALUE - NAV_AREA_GROUND_STEEP), // 陡坡
    NAV_WATER        = 1 << (NAV_AREA_MAX_VALUE - NAV_AREA_WATER),        // 水面/水下
    NAV_MAGMA_SLIME  = 1 << (NAV_AREA_MAX_VALUE - NAV_AREA_MAGMA_SLIME)   // 岩浆/毒液
};

#endif  // _MOVE_MAP_SHARED_DEFINES_H
