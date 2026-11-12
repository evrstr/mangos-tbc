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

#ifndef MANGOS_SPELLTARGETDEFINES_H
#define MANGOS_SPELLTARGETDEFINES_H

// DBC 隐式法术目标类型枚举（EffectImplicitTargetA / EffectImplicitTargetB）
enum SpellTarget
{
    TARGET_NONE                                        = 0,  // 无目标
    TARGET_UNIT_CASTER                                 = 1,  // 施法者自身
    TARGET_UNIT_ENEMY_NEAR_CASTER                      = 2,  // 施法者附近敌方单位（链式/最近）
    TARGET_UNIT_FRIEND_NEAR_CASTER                     = 3,  // 施法者附近友方单位
    TARGET_UNIT_NEAR_CASTER                            = 4,  // 施法者附近任意单位
    TARGET_UNIT_CASTER_PET                             = 5,  // 施法者的宠物
    TARGET_UNIT_ENEMY                                  = 6,  // 选中的敌方单体（或链式跳跃起点）
    TARGET_ENUM_UNITS_SCRIPT_AOE_AT_SRC_LOC            = 7,  // 源点周围脚本筛选的 AoE 单位列表
    TARGET_ENUM_UNITS_SCRIPT_AOE_AT_DEST_LOC           = 8,  // 目标点周围脚本筛选的 AoE 单位列表
    TARGET_LOCATION_CASTER_HOME_BIND                   = 9,  // 施法者炉石绑定坐标
    TARGET_LOCATION_CASTER_DIVINE_BIND_NYI             = 10, // 神圣绑定位置（未实现）
    TARGET_PLAYER_NYI                                  = 11, // 玩家目标（未实现）
    TARGET_PLAYER_NEAR_CASTER_NYI                      = 12, // 施法者附近玩家（未实现）
    TARGET_PLAYER_ENEMY_NYI                            = 13, // 敌方玩家（未实现）
    TARGET_PLAYER_FRIEND_NYI                           = 14, // 友方玩家（未实现）
    TARGET_ENUM_UNITS_ENEMY_AOE_AT_SRC_LOC             = 15, // 施法者/源点周围敌方 AoE 单位
    TARGET_ENUM_UNITS_ENEMY_AOE_AT_DEST_LOC            = 16, // 目标点周围敌方 AoE 单位
    TARGET_LOCATION_DATABASE                           = 17, // 数据库表指定的目标坐标
    TARGET_LOCATION_CASTER_DEST                        = 18, // 施法者相对偏移坐标
    TARGET_UNK_19                                      = 19, // 未知目标类型 19
    TARGET_ENUM_UNITS_PARTY_WITHIN_CASTER_RANGE        = 20, // 施法者范围内所有小队成员
    TARGET_UNIT_FRIEND                                 = 21, // 选中的友方单体
    TARGET_LOCATION_CASTER_SRC                         = 22, // 施法者当前位置（作为源坐标）
    TARGET_GAMEOBJECT                                  = 23, // 选中的游戏对象
    TARGET_ENUM_UNITS_ENEMY_IN_CONE_24                 = 24, // 施法者前方锥形范围内敌方单位
    TARGET_UNIT                                        = 25, // 选中的任意单位
    TARGET_LOCKED                                      = 26, // 被开锁/交互锁定的对象（物品或游戏对象）
    TARGET_UNIT_CASTER_MASTER                          = 27, // 施法者的主人
    TARGET_ENUM_UNITS_ENEMY_AOE_AT_DYNOBJ_LOC          = 28, // 动态对象周围敌方 AoE 单位
    TARGET_ENUM_UNITS_FRIEND_AOE_AT_DYNOBJ_LOC         = 29, // 动态对象周围友方 AoE 单位
    TARGET_ENUM_UNITS_FRIEND_AOE_AT_SRC_LOC            = 30, // 源点周围友方 AoE 单位
    TARGET_ENUM_UNITS_FRIEND_AOE_AT_DEST_LOC           = 31, // 目标点周围友方 AoE 单位
    TARGET_LOCATION_UNIT_MINION_POSITION               = 32, // 召唤物生成位置（施法者附近）
    TARGET_ENUM_UNITS_PARTY_AOE_AT_SRC_LOC             = 33, // 源点周围小队成员 AoE
    TARGET_ENUM_UNITS_PARTY_AOE_AT_DEST_LOC            = 34, // 目标点周围小队成员 AoE
    TARGET_UNIT_PARTY                                  = 35, // 选中的小队成员
    TARGET_ENUM_UNITS_ENEMY_WITHIN_CASTER_RANGE        = 36, // TODO: only used with dest-effects - reinvestigate naming (施法者范围内敌方单位)
    TARGET_UNIT_FRIEND_AND_PARTY                       = 37, // 友方及其小队成员（链式/群体）
    TARGET_UNIT_SCRIPT_NEAR_CASTER                     = 38, // 施法者附近脚本指定单位
    TARGET_LOCATION_CASTER_FISHING_SPOT                = 39, // 施法者前方钓鱼水面坐标
    TARGET_GAMEOBJECT_SCRIPT_NEAR_CASTER               = 40, // 施法者附近脚本指定游戏对象
    TARGET_LOCATION_CASTER_FRONT_RIGHT                 = 41, // 施法者右前方坐标
    TARGET_LOCATION_CASTER_BACK_RIGHT                  = 42, // 施法者右后方坐标
    TARGET_LOCATION_CASTER_BACK_LEFT                   = 43, // 施法者左后方坐标
    TARGET_LOCATION_CASTER_FRONT_LEFT                  = 44, // 施法者左前方坐标
    TARGET_UNIT_FRIEND_CHAIN_HEAL                      = 45, // 治疗链式跳跃目标（优先低血量友方）
    TARGET_LOCATION_SCRIPT_NEAR_CASTER                 = 46, // 施法者附近脚本指定坐标
    TARGET_LOCATION_CASTER_FRONT                       = 47, // 施法者正前方坐标
    TARGET_LOCATION_CASTER_BACK                        = 48, // 施法者正后方坐标
    TARGET_LOCATION_CASTER_LEFT                        = 49, // 施法者正左方坐标
    TARGET_LOCATION_CASTER_RIGHT                       = 50, // 施法者正右方坐标
    TARGET_ENUM_GAMEOBJECTS_SCRIPT_AOE_AT_SRC_LOC      = 51, // 源点周围脚本筛选的游戏对象列表
    TARGET_ENUM_GAMEOBJECTS_SCRIPT_AOE_AT_DEST_LOC     = 52, // 目标点周围脚本筛选的游戏对象列表
    TARGET_LOCATION_CASTER_TARGET_POSITION             = 53, // 施法者当前选中目标的坐标
    TARGET_ENUM_UNITS_ENEMY_IN_CONE_54                 = 54, // 施法者前方锥形范围内敌方单位（变体）
    TARGET_LOCATION_CASTER_FRONT_LEAP                  = 55, // 施法者前方跳跃落点（如闪现术）
    TARGET_ENUM_UNITS_RAID_WITHIN_CASTER_RANGE         = 56, // 施法者范围内所有团队成员
    TARGET_UNIT_RAID                                   = 57, // 选中的团队成员
    TARGET_UNIT_RAID_NEAR_CASTER                       = 58, // 施法者附近团队成员
    TARGET_ENUM_UNITS_FRIEND_IN_CONE                   = 59, // 施法者前方锥形范围内友方单位
    TARGET_ENUM_UNITS_SCRIPT_IN_CONE_60                = 60, // 施法者前方锥形范围内脚本筛选单位
    TARGET_UNIT_RAID_AND_CLASS                         = 61, // 与目标同团队且同职业的成员
    TARGET_PLAYER_RAID_NYI                             = 62, // 团队玩家（未实现）
    TARGET_LOCATION_UNIT_POSITION                      = 63, // 目标单位所在位置
    TARGET_LOCATION_UNIT_FRONT                         = 64, // 目标单位正前方坐标
    TARGET_LOCATION_UNIT_BACK                          = 65, // 目标单位正后方坐标
    TARGET_LOCATION_UNIT_RIGHT                         = 66, // 目标单位正右方坐标
    TARGET_LOCATION_UNIT_LEFT                          = 67, // 目标单位正左方坐标
    TARGET_LOCATION_UNIT_FRONT_RIGHT                   = 68, // 目标单位右前方坐标
    TARGET_LOCATION_UNIT_BACK_RIGHT                    = 69, // 目标单位右后方坐标
    TARGET_LOCATION_UNIT_BACK_LEFT                     = 70, // 目标单位左后方坐标
    TARGET_LOCATION_UNIT_FRONT_LEFT                    = 71, // 目标单位左前方坐标
    TARGET_LOCATION_CASTER_RANDOM_SIDE                 = 72, // 施法者周围随机一侧坐标
    TARGET_LOCATION_CASTER_RANDOM_CIRCUMFERENCE        = 73, // 施法者周围随机圆周坐标
    TARGET_LOCATION_UNIT_RANDOM_SIDE                   = 74, // 目标单位周围随机一侧坐标
    TARGET_LOCATION_UNIT_RANDOM_CIRCUMFERENCE          = 75, // 目标单位周围随机圆周坐标
    TARGET_LOCATION_CHANNEL_TARGET_DEST                = 76, // 引导目标位置
    TARGET_UNIT_CHANNEL_TARGET                         = 77, // 正在引导的目标单位
    TARGET_LOCATION_NORTH                              = 78, // 正北方向坐标
    TARGET_LOCATION_SOUTH                              = 79, // 正南方向坐标
    TARGET_LOCATION_EAST                               = 80, // 正东方向坐标
    TARGET_LOCATION_WEST                               = 81, // 正西方向坐标
    TARGET_LOCATION_NE                                 = 82, // 东北方向坐标
    TARGET_LOCATION_NW                                 = 83, // 西北方向坐标
    TARGET_LOCATION_SE                                 = 84, // 东南方向坐标
    TARGET_LOCATION_SW                                 = 85, // 西南方向坐标
    TARGET_LOCATION_RANDOM_SIDE                        = 86, // 随机一侧位置
    TARGET_LOCATION_CURRENT_REFERENCE                  = 87, // 当前参考坐标点
    TARGET_ENUM_UNITS_SCRIPT_AOE_AT_DYNOBJ_LOC         = 88, // 动态对象周围脚本筛选单位
    TARGET_LOCATION_TRAJECTORY                         = 89, // 弹道轨迹落点坐标
    TARGET_UNIT_CASTER_COMPANION                       = 90, // 施法者的小动物/非战斗宠物
    TARGET_LOCATION_RANDOM_CIRCUMFERENCE               = 91, // 相对基准点周围随机圆周坐标
    TARGET_UNIT_CASTER_SUMMONER                        = 92, // 施法者的召唤者
    TARGET_CORPSE_ENEMY_NEAR_CASTER                = 93, // TODO: Implement (施法者附近敌方尸体，待实现)

    MAX_SPELL_TARGETS                                  // 法术目标类型总数
};

// 客户端/服务端施法目标数据包标志位（决定封包中包含哪些目标字段）
enum SpellCastTargetFlags
{
    TARGET_FLAG_SELF            = 0x00000000,               // 自身（无额外目标数据）
    TARGET_FLAG_UNUSED1         = 0x00000001,               // not used in any spells as of 2.4.3 (can be set dynamically) (2.4.3 中未在法术中静态使用，可动态设置)
    TARGET_FLAG_UNIT            = 0x00000002,               // pguid (携带单位 GUID)
    TARGET_FLAG_UNIT_RAID       = 0x00000004,               // not used in any spells as of 2.4.3 (can be set dynamically) - raid member (团队成员标志)
    TARGET_FLAG_UNIT_PARTY      = 0x00000008,               // not used in any spells as of 2.4.3 (can be set dynamically) - party member (小队成员标志)
    TARGET_FLAG_ITEM            = 0x00000010,               // pguid (携带物品 GUID)
    TARGET_FLAG_SOURCE_LOCATION = 0x00000020,               // 3xfloat (携带源点三维坐标)
    TARGET_FLAG_DEST_LOCATION   = 0x00000040,               // 3xfloat (携带目标点三维坐标)
    TARGET_FLAG_UNIT_ENEMY      = 0x00000080,               // CanAttack == true (敌方单位)
    TARGET_FLAG_UNIT_ALLY       = 0x00000100,               // CanAssist == true (友方单位)
    TARGET_FLAG_CORPSE_ENEMY    = 0x00000200,               // pguid, CanAssist == false (敌方尸体 GUID)
    TARGET_FLAG_UNIT_DEAD       = 0x00000400,               // skinning-like effects (死亡单位，如剥皮类效果)
    TARGET_FLAG_GAMEOBJECT      = 0x00000800,               // pguid, 0 spells in 2.4.3 (游戏对象 GUID)
    TARGET_FLAG_TRADE_ITEM      = 0x00001000,               // pguid, 0 spells (交易栏物品 GUID)
    TARGET_FLAG_STRING          = 0x00002000,               // string, 0 spells (字符串目标)
    TARGET_FLAG_LOCKED          = 0x00004000,               // 199 spells, opening object/lock (带锁对象：开锁目标)
    TARGET_FLAG_CORPSE_ALLY     = 0x00008000,               // pguid, CanAssist == true (友方尸体 GUID)
    TARGET_FLAG_UNIT_MINIPET    = 0x00010000,               // pguid, not used in any spells as of 2.4.3 (can be set dynamically) (小宠物 GUID)
};

// 隐式目标语义类别（用于效果与目标匹配校验）
enum SpellTargetImplicitType
{
    TARGET_TYPE_UNKNOWN         = 0,                        // 未知目标类别
    TARGET_TYPE_LOCATION_SRC,                               // 源点坐标
    TARGET_TYPE_LOCATION_DEST,                              // 目标点坐标
    TARGET_TYPE_GAMEOBJECT,                                 // 游戏对象
    TARGET_TYPE_UNIT,                                       // 单位
    TARGET_TYPE_PLAYER,                                     // 玩家
    TARGET_TYPE_CORPSE,                                     // 尸体
    TARGET_TYPE_LOCK,                                       // 锁（游戏对象或物品）
    // effect custom types (效果自定义目标类别)
    TARGET_TYPE_NONE,                                       // 无需目标
    TARGET_TYPE_ITEM, // TARGET_FLAG_ITEM                   // 物品目标
    TARGET_TYPE_UNIT_DEST,                                  // 单位与目标点组合
    TARGET_TYPE_DYNAMIC, // changes based on other targets  // 动态推断（随关联目标变化）
    TARGET_TYPE_DYNAMIC_ANY, // cant be purely targetless     // 动态推断（不可完全无目标）
    TARGET_TYPE_SPECIAL_DEST, // overrides existing targets with custom logic - Persistent AA (特殊目标点：如持续性区域光环)
    TARGET_TYPE_SPECIAL_UNIT, // Area Auras                 // 特殊单位：如区域光环
    TARGET_TYPE_SPECIAL_SCRIPT,                             // 脚本特殊目标
};

// 目标枚举/收集方式
enum SpellTargetEnumerator
{
    TARGET_ENUMERATOR_UNKNOWN   = 0,                        // 未知枚举方式
    TARGET_ENUMERATOR_SINGLE,                               // 单体目标
    TARGET_ENUMERATOR_CHAIN,                                  // Sorted list (链式目标，有序列表)
    TARGET_ENUMERATOR_AOE,                                    // Unsorted list (区域 AoE 目标，无序列表)
    TARGET_ENUMERATOR_CONE,                                   // Unsorted list (锥形范围目标，无序列表)
};

// 目标阵营/合法性过滤规则
enum SpellTargetFilter
{
    TARGET_NEUTRAL              = 0,                        // 中立/不限阵营
    TARGET_HARMFUL,                                         // 仅敌对可攻击目标
    TARGET_HELPFUL,                                         // 仅友好可协助目标
    TARGET_PARTY,                                            // Same party (subgroup) only (仅限同一小队)
    TARGET_GROUP,                                            // Same group only (仅限同一团队/队伍)
    TARGET_SCRIPT,                                          // 由数据库 spell_script_target 决定
};

// 多目标筛选/排序策略
enum SpellTargetFilterScheme
{
    SCHEME_RANDOM = 0,                                      // 随机筛选
    SCHEME_RANDOM_CHAIN,                                    // 随机链式跳跃
    SCHEME_CLOSEST,                                         // 优先最近目标
    SCHEME_CLOSEST_CHAIN,                                   // 按最近距离链式跳跃
    SCHEME_FURTHEST,                                        // 优先最远目标
    SCHEME_HIGHEST_HP,                                      // 优先最高生命值目标
    SCHEME_LOWEST_HP_CHAIN,                                 // 按最低生命值链式跳跃（如治疗链）
    SCHEME_PRIORITIZE_HEALTH,                               // 优先低血量/按生命值排序
    SCHEME_PRIORITIZE_MANA,                                 // 优先法力值目标
};

// 视线（LOS）检测基准点类型
enum SpellTargetLOS
{
    TARGET_LOS_CASTER           = 0,                        // 从施法者位置检测视线
    TARGET_LOS_SRC,                                         // 从源点坐标检测视线
    TARGET_LOS_DEST,                                        // 从目标点坐标检测视线
};

#endif
