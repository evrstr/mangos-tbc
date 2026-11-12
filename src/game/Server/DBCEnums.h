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

#ifndef DBCENUMS_H
#define DBCENUMS_H

// Client expected level limitation, like as used in DBC item max levels for "until max player level"
// use as default max player level, must be fit max level for used client
// also see MAX_LEVEL and STRONG_MAX_LEVEL define
// 客户端期望的默认最高等级上限（TBC 为 70 级，用于 DBC 物品等级上限等逻辑）
#define DEFAULT_MAX_LEVEL 70

// client supported max level for player/pets/etc. Avoid overflow or client stability affected.
// also see GT_MAX_LEVEL define
// 客户端支持的角色/宠物最高等级上限（避免数值溢出影响客户端稳定性）
#define MAX_LEVEL    100

// Server side limitation. Base at used code requirements.
// also see MAX_LEVEL and GT_MAX_LEVEL define
// 服务端内部等级数值上限（受限于代码类型与协议字段范围）
#define STRONG_MAX_LEVEL 255

#pragma pack(push, 1)

/// DBC 二维平面坐标结构
struct DBCPosition2D
{
    float X;
    float Y;
};

/// DBC 三维空间坐标结构
struct DBCPosition3D
{
    float X;
    float Y;
    float Z;
};

#pragma pack(pop)

/// 区域归属阵营枚举
enum AreaTeams
{
    AREATEAM_NONE  = 0,
    AREATEAM_ALLY  = 2,
    AREATEAM_HORDE = 4
};

/// AreaTable.dbc 区域属性标志位
enum AreaFlags
{
    AREA_FLAG_SNOW                  = 0x00000001,           // snow (only Dun Morogh, Naxxramas, Razorfen Downs and Winterspring) | 降雪区域
    AREA_FLAG_UNK1                  = 0x00000002,           // unknown, (only Naxxramas and Razorfen Downs) | 未知标志
    AREA_FLAG_UNK2                  = 0x00000004,           // Only used on development map | 仅用于开发测试地图
    AREA_FLAG_SLAVE_CAPITAL         = 0x00000008,           // slave capital city flag? | 附属主城标志
    AREA_FLAG_UNK3                  = 0x00000010,           // unknown | 未知标志
    AREA_FLAG_SLAVE_CAPITAL2        = 0x00000020,           // slave capital city flag? | 附属主城标志2
    AREA_FLAG_DUEL                  = 0x00000040,           // zones where duels allowed | 允许决斗的区域
    AREA_FLAG_ARENA                 = 0x00000080,           // arena, both instanced and world arenas | 竞技场区域（含副本与野外竞技场）
    AREA_FLAG_CAPITAL               = 0x00000100,           // main capital city flag | 主城标志（休息区/交易频道等）
    AREA_FLAG_CITY                  = 0x00000200,           // only for one zone named "City" (where it located?) | 城市区域标志
    AREA_FLAG_OUTLAND               = 0x00000400,           // outland zones? (only Eye of the Storm not have this flag, but have 0x00004000 flag) | 外域区域标志
    AREA_FLAG_SANCTUARY             = 0x00000800,           // sanctuary area (PvP disabled) | 圣殿安全区（禁止 PvP）
    AREA_FLAG_NEED_FLY              = 0x00001000,           // only Netherwing Ledge, Socrethar's Seat, Tempest Keep, The Arcatraz, The Botanica, The Mechanar, Sorrow Wing Point, Dragonspine Ridge, Netherwing Mines, Dragonmaw Base Camp, Dragonmaw Skyway | 需要飞行坐骑到达的区域
    AREA_FLAG_UNUSED1               = 0x00002000,           // not used now (no area/zones with this flag set in 2.4.2) | 未使用标志
    AREA_FLAG_OUTLAND2              = 0x00004000,           // outland zones? (only Circle of Blood Arena not have this flag, but have 0x00000400 flag) | 外域区域标志2
    AREA_FLAG_PVP                   = 0x00008000,           // pvp objective area? (Death's Door also has this flag although it's no pvp object area) | 野外 PvP 目标区域
    AREA_FLAG_ARENA_INSTANCE        = 0x00010000,           // used by instanced arenas only | 仅用于副本化竞技场
    AREA_FLAG_UNUSED2               = 0x00020000,           // not used now (no area/zones with this flag set in 2.4.2) | 未使用标志
    AREA_FLAG_UNK5                  = 0x00040000,           // just used for Amani Pass, Hatchet Hills | 未知标志（祖阿曼通道等）
    AREA_FLAG_LOWLEVEL              = 0x00100000,           // used for some starting areas with area_level <=15 | 低等级新手区域标志（<=15级）
};

/// 地下城难度枚举
enum Difficulty
{
    REGULAR_DIFFICULTY           = 0,

    DUNGEON_DIFFICULTY_NORMAL    = 0,                       ///< 普通难度
    DUNGEON_DIFFICULTY_HEROIC    = 1,                       ///< 英雄难度
};

#define MAX_DIFFICULTY             2

/// 刷怪难度掩码
enum SpawnMask
{
    SPAWNMASK_REGULAR           = (1 << REGULAR_DIFFICULTY),// any any maps without spawn modes (continents/subway) or in minimal spawnmode | 常规地图（大地图/地铁）或最低难度刷怪掩码

    SPAWNMASK_DUNGEON_NORMAL    = (1 << DUNGEON_DIFFICULTY_NORMAL),
    SPAWNMASK_DUNGEON_HEROIC    = (1 << DUNGEON_DIFFICULTY_HEROIC),
    SPAWNMASK_DUNGEON_ALL       = (SPAWNMASK_DUNGEON_NORMAL | SPAWNMASK_DUNGEON_HEROIC)
};

/// 阵营模板行为标志位
enum FactionTemplateFlags
{
    FACTION_TEMPLATE_RESPOND_TO_CALL_FOR_HELP       = 0x00000001,
    FACTION_TEMPLATE_BROADCAST_TO_ENEMIES_LOW_PRIO  = 0x00000002,
    FACTION_TEMPLATE_BROADCAST_TO_ENEMIES_MED_PRIO  = 0x00000004,
    FACTION_TEMPLATE_BROADCAST_TO_ENEMIES_HIG_PRIO  = 0x00000008,
    FACTION_TEMPLATE_SEARCH_FOR_ENEMIES_LOW_PRIO    = 0x00000010,
    FACTION_TEMPLATE_SEARCH_FOR_ENEMIES_MED_PRIO    = 0x00000020,
    FACTION_TEMPLATE_SEARCH_FOR_ENEMIES_HIG_PRIO    = 0x00000040,
    FACTION_TEMPLATE_SEARCH_FOR_FRIENDS_LOW_PRIO    = 0x00000080,
    FACTION_TEMPLATE_SEARCH_FOR_FRIENDS_MED_PRIO    = 0x00000100,
    FACTION_TEMPLATE_SEARCH_FOR_FRIENDS_HIG_PRIO    = 0x00000200,
    FACTION_TEMPLATE_FLEE_FROM_CALL_FOR_HELP        = 0x00000400,
    FACTION_TEMPLATE_FLAG_ASSIST_PLAYERS            = 0x00000800,   // flagged for PvP | 协助玩家时使玩家标记为 PvP
    FACTION_TEMPLATE_FLAG_ATTACK_PVP_ACTIVE_PLAYERS = 0x00001000,   // faction will attack players that were involved in PvP combats | 攻击处于 PvP 战斗中的玩家
};

/// 阵营分组掩码
enum FactionGroupMask
{
    FACTION_GROUP_MASK_PLAYER   = 1,                              // any player | 任意玩家
    FACTION_GROUP_MASK_ALLIANCE = 2,                              // player or creature from alliance team | 联盟阵营玩家或生物
    FACTION_GROUP_MASK_HORDE    = 4,                              // player or creature from horde team | 部落阵营玩家或生物
    FACTION_GROUP_MASK_MONSTER  = 8                               // aggressive creature from monster team | 敌对怪物阵营（全零表示中立非主动攻击生物）
                                  // if none flags set then non-aggressive creature
};

/// 地图类型枚举（对应 Map.dbc）
enum MapTypes
{
    MAP_COMMON          = 0,                                ///< 普通大地图/主城
    MAP_INSTANCE        = 1,                                ///< 5人地下城副本
    MAP_RAID            = 2,                                ///< 团队副本
    MAP_BATTLEGROUND    = 3,                                ///< 战场
    MAP_ARENA           = 4                                 ///< 竞技场
};

/// 技能学习触发方式
enum AbilytyLearnType
{
    ABILITY_LEARNED_ON_GET_PROFESSION_SKILL     = 1,
    ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL  = 2
};

/// SkillRaceClassInfo.dbc 技能标志位
enum SkillRaceClassInfoFlags
{
    SKILL_FLAG_NONE                     = 0x000,
    SKILL_FLAG_UNK0                     = 0x001,
    SKILL_FLAG_DISPLAY_SILENCED         = 0x002,    // Clientside: no skillup messages | 客户端不显示技能提升提示
    SKILL_FLAG_UNK2                     = 0x004,
    SKILL_FLAG_UNK3                     = 0x008,
    SKILL_FLAG_MAXIMIZED                = 0x010,    // Always at max value | 始终处于上限值
    SKILL_FLAG_CAN_UNLEARN              = 0x020,
    SKILL_FLAG_UNK6                     = 0x040,
    SKILL_FLAG_DISPLAY_SORTED           = 0x080,    // Clientside: affects spellbook sorting for dependent spells | 客户端法术书排序标志
    SKILL_FLAG_NOT_TRAINABLE            = 0x100,
    SKILL_FLAG_UNK9                     = 0x200,
    SKILL_FLAG_DISPLAY_AS_MONO          = 0x400     // Clientside: appear as greyed out bar (1/1) | 客户端显示为灰条（1/1）
};

/// 物品附魔效果类型
enum ItemEnchantmentType
{
    ITEM_ENCHANTMENT_TYPE_NONE             = 0,
    ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL     = 1,
    ITEM_ENCHANTMENT_TYPE_DAMAGE           = 2,
    ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL      = 3,
    ITEM_ENCHANTMENT_TYPE_RESISTANCE       = 4,
    ITEM_ENCHANTMENT_TYPE_STAT             = 5,
    ITEM_ENCHANTMENT_TYPE_TOTEM            = 6
};

/// 物品附魔视觉光环 ID
enum ItemEnchantmentAuraId
{
    ITEM_ENCHANTMENT_AURAID_POISON     = 26,
    ITEM_ENCHANTMENT_AURAID_NORMAL     = 28,
    ITEM_ENCHANTMENT_AURAID_FIRE       = 32,
    ITEM_ENCHANTMENT_AURAID_FROST      = 33,
    ITEM_ENCHANTMENT_AURAID_NATURE     = 81,
    ITEM_ENCHANTMENT_AURAID_SHADOW     = 107
};

/// 图腾/工具类别枚举
enum TotemCategoryType
{
    TOTEM_CATEGORY_TYPE_KNIFE   = 1,
    TOTEM_CATEGORY_TYPE_TOTEM   = 2,
    TOTEM_CATEGORY_TYPE_ROD     = 3,
    TOTEM_CATEGORY_TYPE_PICK    = 21,
    TOTEM_CATEGORY_TYPE_STONE   = 22,
    TOTEM_CATEGORY_TYPE_HAMMER  = 23,
    TOTEM_CATEGORY_TYPE_SPANNER = 24
};

// SummonProperties.dbc, col 0          == Id               (m_id)
// SummonProperties.dbc, col 1          == Group            (m_control)
/// 召唤物控制分组（对应 SummonProperties.dbc 第 1 列）
enum SummonPropGroup
{
    SUMMON_PROP_GROUP_WILD              = 0,
    SUMMON_PROP_GROUP_FRIENDLY          = 1,
    SUMMON_PROP_GROUP_PETS              = 2,
    SUMMON_PROP_GROUP_CONTROLLABLE      = 3,
};

// SummonProperties.dbc, col 2          == FactionId        (m_faction)
// SummonProperties.dbc, col 3          == Title            (m_title)
/// 召唤物称号显示类型（对应 SummonProperties.dbc 第 3 列）
enum UnitNameSummonTitle
{
    UNITNAME_SUMMON_TITLE_NONE          = 0,                // no default title, different summons, 730 spells in 2.4.3 | 无默认称号
    UNITNAME_SUMMON_TITLE_PET           = 1,                // 's Pet,           generic summons, 59 spells in 2.4.3 | “xxx 的宠物”
    UNITNAME_SUMMON_TITLE_GUARDIAN      = 2,                // 's Guardian,      summon guardian, 311 spells in 2.4.3 | “xxx 的守护者”
    // UNITNAME_SUMMON_TITLE_MINION      = 3,               // 's Minion,        summon army, 0 spells in 2.4.3 | “xxx 的仆从”
    UNITNAME_SUMMON_TITLE_TOTEM         = 4,                // 's Totem,         summon totem, 148 spells in 2.4.3 | “xxx 的图腾”
    UNITNAME_SUMMON_TITLE_COMPANION     = 5,                // 's Companion,     critter/minipet, 159 spells in 2.4.3 | “xxx 的伙伴（小宠物）”
};

// SummonProperties.dbc, col 4          == Slot             (m_slot)
/// 召唤物槽位类型（图腾槽/小宠物槽，对应 SummonProperties.dbc 第 4 列）
enum SummonPropSlot
{
    SUMMON_PROP_SLOT_NONE                   = 0,
    SUMMON_PROP_SLOT_TOTEM_1                = 1,
    SUMMON_PROP_SLOT_TOTEM_2                = 2,
    SUMMON_PROP_SLOT_TOTEM_3                = 3,
    SUMMON_PROP_SLOT_TOTEM_4                = 4,
    SUMMON_PROP_SLOT_CRITTER                = 5,
    SUMMON_PROP_SLOT_ANY_AVAILABLE_TOTEM    = -1,
};

// SummonProperties.dbc, col 5          == Flags            (m_flags)
/// 召唤属性标志位（对应 SummonProperties.dbc 第 5 列）
enum SummonPropFlags
{
    SUMMON_PROP_FLAG_NONE                           = 0x0000,
    SUMMON_PROP_FLAG_ATTACK_SUMMONER                = 0x0001,
    SUMMON_PROP_FLAG_HELP_WHEN_SUMMONED_IN_COMBAT   = 0x0002,
    SUMMON_PROP_FLAG_USE_LEVEL_OFFSET               = 0x0004, // Implemented differently in tbc core | 使用等级偏移
};

/// 法术效果索引（每个法术最多 3 个效果槽位）
enum SpellEffectIndex
{
    EFFECT_INDEX_0 = 0,
    EFFECT_INDEX_1 = 1,
    EFFECT_INDEX_2 = 2
};

#define MAX_EFFECT_INDEX 3

/// 法术效果索引位掩码
enum SpellEffectIndexMask
{
    EFFECT_MASK_INDEX_0 = (1 << EFFECT_INDEX_0),
    EFFECT_MASK_INDEX_1 = (1 << EFFECT_INDEX_1),
    EFFECT_MASK_INDEX_2 = (1 << EFFECT_INDEX_2),
    EFFECT_MASK_ALL = (EFFECT_MASK_INDEX_0 | EFFECT_MASK_INDEX_1 | EFFECT_MASK_INDEX_2)
};

/// 法术所属职业/系统家族（对应 Spell.dbc SpellFamilyName）
enum SpellFamily
{
    SPELLFAMILY_GENERIC     = 0,
    SPELLFAMILY_UNK1        = 1,                            // events, holidays | 节日/世界事件法术
    // 2 - unused | 2 - 未使用
    SPELLFAMILY_MAGE        = 3,
    SPELLFAMILY_WARRIOR     = 4,
    SPELLFAMILY_WARLOCK     = 5,
    SPELLFAMILY_PRIEST      = 6,
    SPELLFAMILY_DRUID       = 7,
    SPELLFAMILY_ROGUE       = 8,
    SPELLFAMILY_HUNTER      = 9,
    SPELLFAMILY_PALADIN     = 10,
    SPELLFAMILY_SHAMAN      = 11,
    SPELLFAMILY_UNK2        = 12,
    SPELLFAMILY_POTION      = 13,
    // 14 - unused | 14 - 未使用
    SPELLFAMILY_DEATHKNIGHT = 15,
    // 16 - unused | 16 - 未使用
    SPELLFAMILY_UNK3        = 17
};

#endif
