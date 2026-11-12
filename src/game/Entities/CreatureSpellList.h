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

#ifndef CREATURE_SPELL_LIST_H
#define CREATURE_SPELL_LIST_H

#include "Platform/Define.h"
#include <map>
#include <string>

// 生物法术列表选目标类型
enum SpellListTargetingType
{
    SPELL_LIST_TARGETING_HARDCODED = 0,                     // 硬编码预设目标规则
    SPELL_LIST_TARGETING_ATTACK    = 1,                     // 敌对攻击选目标规则（按仇恨/距离等参数）
    SPELL_LIST_TARGETING_SUPPORT   = 2,                     // 友方辅助选目标规则（治疗/驱散/增益等）
    SPELL_LIST_TARGETING_MAX
};

// 硬编码法术目标规则枚举
enum SpellListTargetingHardcoded
{
    SPELL_LIST_TARGET_NONE    = 0,                          // 无显式目标
    SPELL_LIST_TARGET_CURRENT = 1,                          // 当前仇恨目标
    SPELL_LIST_TARGET_SELF    = 2,                          // 自身
    SPELL_LIST_TARGET_DISPELLABLE_FRIENDLY = 3,             // 可驱散的友方目标（含自身）
    SPELL_LIST_TARGET_DISPELLABLE_FRIENDLY_NO_SELF = 4,     // 可驱散的友方目标（不含自身）
    SPELL_LIST_TARGET_FRIENDLY_MISSING_BUFF = 5,            // 缺少增益的友方目标（含自身）
    SPELL_LIST_TARGET_FRIENDLY_MISSING_BUFF_NO_SELF = 6,    // 缺少增益的友方目标（不含自身）
    SPELL_LIST_TARGET_CURRENT_NOT_ALONE = 7, // current target exists and more than 1 on threat list (存在当前目标且仇恨列表人数大于 1)
    SPELL_LIST_HARDCODED_MAX  = 100,
};

// 生物法术列表的目标筛选配置条目（对应 creature_spell_targeting 表）
struct CreatureSpellListTargeting
{
    uint32 Id;                                              // 目标规则 ID
    uint32 Type;                                            // 规则类型（SpellListTargetingType）
    uint32 Param1;                                          // 规则参数 1
    uint32 Param2;                                          // 规则参数 2
    uint32 Param3;                                          // 规则参数 3
    int32  UnitCondition;                                   // 附加单位条件 ID
    std::string Comment;                                    // 备注说明
};

// 生物法术列表单条法术的行为标志
enum SpellListFlags
{
    SPELL_LIST_FLAG_SUPPORT_ACTION      = 1,                // 属于辅助动作（按 ChanceSupportAction 概率触发）
    SPELL_LIST_FLAG_RANGED_ACTION       = 2, // previously known as main ranged spell in EAI (主远程法术动作，原 EAI 主远程法术)
    SPELL_LIST_FLAG_CATEGORY_COOLDOWN   = 4, // imposes category cooldown instead of normal cooldown (施加类别冷却而非普通冷却)
    SPELL_LIST_FLAG_NON_BLOCKING        = 8, // executes spell outside of rolling for one action, should be instant TODO: check if its not identical to SPELL_ATTR_EX4_ALLOW_CAST_WHILE_CASTING (非阻塞瞬发动作，不参与单轮动作互斥抽取)
};

// 生物法术列表中的单条法术条目配置
struct CreatureSpellListSpell
{
    uint32 Id; // list id (所属法术列表 ID)
    uint32 Position;                                        // 列表中的顺序位置/索引
    uint32 SpellId;                                         // 施放的法术 ID
    uint32 Flags;                                           // 标志位（SpellListFlags）
    int32 CombatCondition;                                  // 战斗条件 ID
    CreatureSpellListTargeting* Target;                     // 关联的目标筛选配置指针
    uint32 ScriptId;                                        // 触发脚本 ID
    uint32 Availability;                                    // 可用阶段/难度掩码
    uint32 Probability;                                     // 权重/触发概率
    uint32 InitialMin;                                      // 初始冷却最小值（毫秒）
    uint32 InitialMax;                                      // 初始冷却最大值（毫秒）
    uint32 RepeatMin;                                       // 重复冷却最小值（毫秒）
    uint32 RepeatMax;                                       // 重复冷却最大值（毫秒）
    bool DisabledForAI;                                     // 是否对 AI 禁用
};

// 生物法术列表集合（对应一套完整的生物技能配置）
struct CreatureSpellList
{
    uint32 Id;                                              // 列表唯一 ID
    std::string Name;                                       // 列表名称
    uint32 ChanceSupportAction;                             // 辅助动作触发概率
    uint32 ChanceRangedAttack;                              // 远程攻击触发概率
    bool Disabled; // Compatibility layer until creature_template_spells is removed (禁用标记，兼容旧 creature_template_spells 的过渡字段)
    std::map<uint32, CreatureSpellListSpell> Spells;        // 按 Position 索引的法术条目映射
};

// 全局生物法术列表与目标规则容器
struct CreatureSpellListContainer
{
    std::map<uint32, CreatureSpellList> spellLists;         // 所有法术列表映射
    std::map<uint32, CreatureSpellListTargeting> targeting; // 所有目标规则映射
};

#endif
