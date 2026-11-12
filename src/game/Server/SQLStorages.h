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

#ifndef MANGOS_SQLSTORAGES_H
#define MANGOS_SQLSTORAGES_H

#include "Database/SQLStorage.h"

/// 全局静态 SQL 数据存储容器声明（对应 world 库各模板与配置表）
extern SQLStorage sCreatureStorage;                         ///< creature_template 生物模板表
extern SQLStorage sCreatureDataAddonStorage;                ///< creature_addon 生物实例附加表
extern SQLStorage sCreatureInfoAddonStorage;                ///< creature_template_addon 生物模板附加表
extern SQLStorage sCreatureModelStorage;                    ///< creature_model_info 生物模型信息表
extern SQLStorage sEquipmentStorage;                        ///< creature_equip_template 生物装备模板表
extern SQLStorage sEquipmentStorageRaw;                     ///< creature_equip_template_raw 原始装备模板表
extern SQLStorage sPageTextStore;                           ///< page_text 书页文本表
extern SQLStorage sItemStorage;                             ///< item_template 物品模板表
extern SQLStorage sInstanceTemplate;                        ///< instance_template 副本模板表
extern SQLStorage sWorldTemplate;                           ///< world_template 世界地图脚本模板表
extern SQLStorage sConditionStorage;                        ///< conditions 通用条件表
extern SQLStorage sSpellTemplate;                           ///< spell_template 法术模板表
extern SQLStorage sSpellCones;                              ///< spell_cone 法术锥形角度表
extern SQLStorage sDungeonEncounterStore;                   ///< instance_dungeon_encounters 地下城首领遭遇表
extern SQLStorage sFactionStore;                            ///< faction_store 阵营扩展存储
extern SQLStorage sCreatureConditionalSpawnStore;           ///< creature_conditional_spawn 条件刷怪表
extern SQLStorage sWorldSafeLocsStore;                      ///< world_safe_locs 墓地安全点表


extern SQLHashStorage sGOStorage;                           ///< gameobject_template 游戏对象哈希存储

extern SQLMultiStorage sSpellScriptTargetStorage;           ///< spell_script_target 法术脚本目标一对多存储

/// 获取只读法术模板存储指针
SQLStorage                 const* GetSpellStore();

#endif
