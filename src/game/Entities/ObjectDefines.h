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

#ifndef CMANGOS_OBJECTDEFINES_H
#define CMANGOS_OBJECTDEFINES_H

#include "Maps/GridDefines.h"

static constexpr float const CONTACT_DISTANCE               = 1.f;                          // 接触距离（码）
static constexpr float const INTERACTION_DISTANCE           = 5.0f;                         // 交互距离（码）
static constexpr float const ATTACK_DISTANCE                = 5.0f;                         // 攻击距离（码）
static constexpr float const MELEE_LEEWAY                   = 8.0f / 3.0f;                  // Melee attack and melee spell leeway when moving (移动时近战攻击与近战法术的距离宽容值)
static constexpr float const AOE_LEEWAY                     = 2.0f;                         // AOE leeway when moving (移动时范围法术的距离宽容值)
static constexpr float const INSPECT_DISTANCE               = 28.0f;                        // 观察装备距离（码）
static constexpr float const TRADE_DISTANCE                 = 11.11f;                       // 交易距离（码）

static constexpr float const MAX_VISIBILITY_DISTANCE        = SIZE_OF_GRIDS;                // max distance for visible object show, limited in 533 yards (可见对象最大显示距离，上限 533 码)
static constexpr float const VISIBILITY_DISTANCE_GIGANTIC   = 400.0f;                       // 超大型对象可视距离（400 码）
static constexpr float const VISIBILITY_DISTANCE_LARGE      = 200.0f;                       // 大型对象可视距离（200 码）
static constexpr float const VISIBILITY_DISTANCE_NORMAL     = 100.0f;                       // 普通对象可视距离（100 码）
static constexpr float const VISIBILITY_DISTANCE_SMALL      = 50.0f;                        // 小型对象可视距离（50 码）
static constexpr float const VISIBILITY_DISTANCE_TINY       = 25.0f;                        // 微型对象可视距离（25 码）
static constexpr float const DEFAULT_VISIBILITY_DISTANCE    = VISIBILITY_DISTANCE_NORMAL;   // default visible distance, 100 yards on continents (大陆默认可视距离 100 码)
static constexpr float const DEFAULT_VISIBILITY_INSTANCE    = 170.0f;                       // default visible distance in instances, 170 yards (副本默认可视距离 170 码)
static constexpr float const DEFAULT_VISIBILITY_BGARENAS    = 533.0f;                       // default visible distance in BG/Arenas, 533 yards (战场/竞技场默认可视距离 533 码)

static constexpr float const DEFAULT_WORLD_OBJECT_SIZE      = 0.388999998569489f;           // currently used (correctly?) for any non Unit world objects.
                                                                                            // This is actually the bounding_radius, like player/creature from
                                                                                            // creature_model_data
                                                                                            // 非单位世界对象的默认包围半径，对应 creature_model_data 中的 bounding_radius

static constexpr float const DEFAULT_OBJECT_SCALE           = 1.0f;                         // player/item scale as default, npc/go from database, pets from dbc (默认对象缩放比例；NPC/GO 取自数据库，宠物取自 DBC)
static constexpr float const DEFAULT_TAUREN_MALE_SCALE      = 1.35f;                        // Tauren male player scale by default (男性牛头人玩家默认缩放比例)
static constexpr float const DEFAULT_TAUREN_FEMALE_SCALE    = 1.25f;                        // Tauren female player scale by default (女性牛头人玩家默认缩放比例)
static constexpr float const DEFAULT_COLLISION_HEIGHT       = 2.03128f;                     // Most common value in dbc (DBC 中最常见的默认碰撞高度)
static constexpr float const DEFAULT_COLLISION_WIDTH        = 0.5097f;                      // Most common value in dbc (DBC 中最常见的默认碰撞宽度)

static constexpr float const MAX_STEALTH_DETECT_RANGE       = 45.0f;                        // 最大潜行侦测距离（码）
static constexpr float const GRID_ACTIVATION_RANGE          = 45.0f;                        // 网格激活距离（码）

// 区域名称与 WMO 名称覆盖信息
struct AreaNameInfo
{
    const char* areaName;                                   // 区域名称
    const char* wmoNameOverride;                            // WMO 名称覆盖值
};

#endif
