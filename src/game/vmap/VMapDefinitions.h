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

#ifndef _VMAPDEFINITIONS_H
#define _VMAPDEFINITIONS_H

// 液体网格单个格子的世界边长（约 4.1667 码）
#define LIQUID_TILE_SIZE (533.333f / 128.f)

namespace VMAP
{
    const char VMAP_MAGIC[] = "VMAP_7.0";                   // used in final vmap files (成品 VMAP 文件魔数版本标识)
    const char RAW_VMAP_MAGIC[] = "VMAPs05";                // used in extracted vmap files with raw data (原始提取文件魔数版本标识)
    const char GAMEOBJECT_MODELS[] = "temp_gameobject_models"; // GameObject 碰撞模型清单文件名

    // defined in TileAssembler.cpp currently... (从文件读取指定块魔数并校验)
    bool readChunk(FILE* rf, char* dest, const char* compare, uint32 len);
}

// 核心服务与独立工具（如 mmap_generator）的断言/日志宏适配
#ifndef NO_CORE_FUNCS
#include "Util/Errors.h"
#include "Log/Log.h"
#define ERROR_LOG(...) sLog.outError(__VA_ARGS__);
#elif defined MMAP_GENERATOR
#include <assert.h>
#define MANGOS_ASSERT(x) assert(x)
#define DEBUG_LOG(...) 0
#define DETAIL_LOG(...) 0
#define LOG_FILTER_MAP_LOADING true
#define DEBUG_FILTER_LOG(F,...) do{ if (F) DEBUG_LOG(__VA_ARGS__); } while(0)
#define ERROR_LOG(...) do{ printf("ERROR:"); printf(__VA_ARGS__); printf("\n"); } while(0)
#else
#include <assert.h>
#define MANGOS_ASSERT(x) assert(x)
#define DEBUG_LOG(...) do{ printf(__VA_ARGS__); printf("\n"); } while(0)
#define DETAIL_LOG(...) do{ printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOG_FILTER_MAP_LOADING true
#define DEBUG_FILTER_LOG(F,...) do{ if (F) DEBUG_LOG(__VA_ARGS__); } while(0)
#define ERROR_LOG(...) do{ printf("ERROR:"); printf(__VA_ARGS__); printf("\n"); } while(0)
#endif

#endif // _VMAPDEFINITIONS_H
