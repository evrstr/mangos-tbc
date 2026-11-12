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

#ifndef MANGOS_CONDITIONS_H
#define MANGOS_CONDITIONS_H

#include "Globals/SharedDefines.h"

class Map;
class WorldObject;

/// 通用条件系统条件类型枚举（对应数据库 conditions 表 type 字段）
enum ConditionType
{
    //                                                      // value1       value2  for the Condition enumed | 各条件类型对应的 value1/value2 参数语义
    CONDITION_NOT                   = -3,                   // cond-id-1    0          returns !cond-id-1 | 逻辑非：返回 !cond-id-1
    CONDITION_OR                    = -2,                   // cond-id-1    cond-id-2  returns cond-id-1 OR cond-id-2 | 逻辑或：支持最多 4 个子条件
    CONDITION_AND                   = -1,                   // cond-id-1    cond-id-2  returns cond-id-1 AND cond-id-2 | 逻辑与：支持最多 4 个子条件
    CONDITION_NONE                  = 0,                    // 0            0 | 空条件（恒为真）
    CONDITION_AURA                  = 1,                    // spell_id     effindex | 目标拥有指定法术与效果索引的光环
    CONDITION_ITEM                  = 2,                    // item_id      count   check present req. amount items in inventory | 背包内拥有指定数量物品
    CONDITION_ITEM_EQUIPPED         = 3,                    // item_id      0 | 已装备指定物品或镶嵌对应宝石
    CONDITION_AREAID                = 4,                    // area_id      0, 1 (0: in (sub)area, 1: not in (sub)area) | 处于（或未处于）指定区域/子区域
    CONDITION_REPUTATION_RANK_MIN   = 5,                    // faction_id   min_rank | 阵营声望等级不低于 min_rank
    CONDITION_TEAM                  = 6,                    // player_team  0,      (469 - Alliance 67 - Horde) | 玩家所属阵营（469 联盟 / 67 部落）
    CONDITION_SKILL                 = 7,                    // skill_id     skill_value | 具备指定技能且基础等级 >= skill_value
    CONDITION_QUESTREWARDED         = 8,                    // quest_id     0 | 已完成并交还指定任务
    CONDITION_QUESTTAKEN            = 9,                    // quest_id     0,1,2   for condition true while quest active (0 any state, 1 if quest incomplete, 2 if quest completed). | 当前任务日志中处于指定任务状态（0 任意 / 1 未完成 / 2 已达成）
    CONDITION_AD_COMMISSION_AURA    = 10,                   // 0            0,      for condition true while one from AD commission aura active | 佩戴银色黎明委任徽章光环
    CONDITION_PVP_RANK              = 11,                   // reserved - used only in vanilla | 预留字段（仅经典旧世使用，TBC 恒返回 false）
    CONDITION_ACTIVE_GAME_EVENT     = 12,                   // event_id     0 | 指定游戏事件当前处于激活状态
    CONDITION_AREA_FLAG             = 13,                   // area_flag    area_flag_not | 所在区域具备 value1 标志且不具备 value2 标志
    CONDITION_RACE_CLASS            = 14,                   // race_mask    class_mask | 匹配种族掩码与职业掩码
    CONDITION_LEVEL                 = 15,                   // level        0, 1 or 2 (0: equal to, 1: equal or higher than, 2: equal or less than) | 单位等级比较（0 等于 / 1 大于等于 / 2 小于等于）
    CONDITION_UNUSED_2              = 16,
    CONDITION_SPELL                 = 17,                   // spell_id     0, 1 (0: has spell, 1: hasn't spell) | 是否已学会指定法术（0 已学 / 1 未学）
    CONDITION_INSTANCE_SCRIPT       = 18,                   // map_id       instance_condition_id (instance script specific enum) | 由副本脚本 InstanceData 自定义判定
    CONDITION_QUESTAVAILABLE        = 19,                   // quest_id     0       for case when loot/gossip possible only if player can start quest | 玩家当前满足接取指定任务的条件
    CONDITION_RESERVED_1            = 20,                   // reserved for 3.x and later
    CONDITION_RESERVED_2            = 21,                   // reserved for 3.x and later
    CONDITION_QUEST_NONE            = 22,                   // quest_id     0 (quest did not take and not rewarded) | 未接取且未完成过指定任务
    CONDITION_ITEM_WITH_BANK        = 23,                   // item_id      count   check present req. amount items in inventory or bank | 背包与银行合计拥有指定数量物品
    CONDITION_UNUSED_3              = 24,
    CONDITION_UNUSED_4              = 25,
    CONDITION_ACTIVE_HOLIDAY        = 26,                   // holiday_id   0       preferred use instead CONDITION_ACTIVE_GAME_EVENT when possible | 指定节日当前处于激活状态
    CONDITION_UNUSED_5              = 27,
    CONDITION_LEARNABLE_ABILITY     = 28,                   // spell_id     0 or item_id | 玩家尚未学会且未拥有对应配方物品，且种族/职业/专业等级满足学习条件
                                                            // True when player can learn ability (using min skill value from SkillLineAbility).
                                                            // Item_id can be defined in addition, to check if player has one (1) item in inventory or bank.
                                                            // When player has spell or has item (when defined), condition return false.
    CONDITION_SKILL_BELOW           = 29,                   // skill_id     skill_value | 技能等级低于 skill_value（若 skill_value==1 则要求未学该技能）
                                                            // True if player has skill skill_id and skill less than (and not equal) skill_value (for skill_value > 1)
                                                            // If skill_value == 1, then true if player has not skill skill_id
    CONDITION_REPUTATION_RANK_MAX   = 30,                   // faction_id   max_rank | 阵营声望等级不超过 max_rank
    CONDITION_COMPLETED_ENCOUNTER   = 31,                   // encounter_id encounter_id2       encounter_id[2] = DungeonEncounter(dbc).id (if value2 provided it will return value1 OR value2) | 副本中已完成指定首领战斗（支持双 ID 或逻辑）
    CONDITION_UNUSED_6              = 32,
    CONDITION_LAST_WAYPOINT         = 33,                   // waypointId   0 = exact, 1: wp <= waypointId, 2: wp > waypointId  Use to check what waypoint was last reached | 检查源生物最近到达的路点编号
    CONDITION_RESERVED_4            = 34,                   // reserved for 3.x and later
    CONDITION_GENDER                = 35,                   // 0=male, 1=female, 2=none (see enum Gender) | 单位性别匹配
    CONDITION_DEAD_OR_AWAY          = 36,                   // value1: 0=player dead, 1=player is dead (with group dead), 2=player in instance are dead, 3=creature is dead | 玩家/队伍/副本全员/生物死亡或超出距离判定
                                                            // value2: if != 0 only consider players in range of this value
    CONDITION_CREATURE_IN_RANGE     = 37,                   // value1: creature entry; value2: range; returns only alive creatures | 目标周围指定半径内存在存活的指定 Entry 生物
    CONDITION_PVP_SCRIPT            = 38,                   // value1: zoneId; value2: conditionId; | 野外 PvP 或战场脚本自定义条件
    CONDITION_SPAWN_COUNT           = 39,                   // value1: creatureId; value2: count; | 当前地图中指定 Entry 生物已刷出数量 >= count
    CONDITION_WORLD_SCRIPT          = 40,                   // 全服世界状态脚本条件 (sWorldState)
    CONDITION_UNUSED_7              = 41,
    CONDITION_WORLDSTATE            = 42,                   // value1: worldstateId; value2: sign; value3: otherOperand - example, 10000; =; 0 - true when 10000 is zero | 地图世界状态变量比较表达式
    CONDITION_IS_IN_COMBAT          = 43,                   // value1: 0 - in combat, 1 - out of combat, target unit | 目标单位处于战斗中（0）或脱战（1）
};

/// 条件执行控制标志位
enum ConditionFlags
{
    CONDITION_FLAG_REVERSE_RESULT = 0x1,                    /// 对条件结果取反
    CONDITION_FLAG_SWAP_TARGETS   = 0x2                     /// 评估前交换 source 与 target 参数
};

/// 条件调用来源系统枚举（用于日志追踪与部分上下文特判）
enum ConditionSource                                        // From where was the condition called?
{
    CONDITION_FROM_LOOT                 = 0,                    // Used to check a *_loot_template entry | 战利品掉落模板
    CONDITION_FROM_REFERING_LOOT        = 1,                    // Used to check a entry refering to a reference_loot_template entry | 引用战利品模板
    CONDITION_FROM_GOSSIP_MENU          = 2,                    // Used to check a gossip menu menu-text | 对话菜单文本
    CONDITION_FROM_GOSSIP_OPTION        = 3,                    // Used to check a gossip menu option-item | 对话菜单选项
    CONDITION_FROM_EVENTAI              = 4,                    // Used to check EventAI Event "On Receive Emote" | EventAI 接收表情事件
    CONDITION_FROM_HARDCODED            = 5,                    // Used to check a hardcoded event - not actually a condition | 硬编码逻辑检查
    CONDITION_FROM_VENDOR               = 6,                    // Used to check a condition from a vendor | 商人售卖物品列表
    CONDITION_FROM_SPELL_AREA           = 7,                    // Used to check a condition from spell_area table | 区域法术 (spell_area)
    CONDITION_FROM_RESERVED_1           = 8,                    // reserved for 3.x and later
    CONDITION_FROM_DBSCRIPTS            = 9,                    // Used to check a condition from DB Scripts Engine | 数据库脚本引擎
    CONDITION_FROM_TRAINER              = 10,                   // Used to check a condition from npc_trainer and npc_trainer_template | 训练师法术学习
    CONDITION_FROM_AREATRIGGER_TELEPORT = 11,                   // Used to check a condition from areatrigger_teleport | 区域传送门
    CONDITION_FROM_QUEST                = 12,                   // Used to check a condition from quest_template | 任务模板
    CONDITION_FROM_WORLDSTATE           = 13,                   // Used to check a condition from spawn_group | 刷新组世界状态
};

/// 每种条件类型所需的入参对象约束
enum ConditionRequirement
{
    CONDITION_REQ_NONE,
    CONDITION_REQ_TARGET_WORLDOBJECT,
    CONDITION_REQ_TARGET_GAMEOBJECT,
    CONDITION_REQ_TARGET_UNIT,
    CONDITION_REQ_TARGET_UNIT_OR_CORPSE,
    CONDITION_REQ_TARGET_CREATURE,
    CONDITION_REQ_TARGET_PLAYER,
    CONDITION_REQ_TARGET_PLAYER_OR_CORPSE,
    CONDITION_REQ_SOURCE_WORLDOBJECT,
    CONDITION_REQ_SOURCE_GAMEOBJECT,
    CONDITION_REQ_SOURCE_UNIT,
    CONDITION_REQ_SOURCE_CREATURE,
    CONDITION_REQ_SOURCE_PLAYER,
    CONDITION_REQ_ANY_WORLDOBJECT,
    CONDITION_REQ_MAP_OR_WORLDOBJECT,
    CONDITION_REQ_BOTH_WORLDOBJECTS,
    CONDITION_REQ_BOTH_GAMEOBJECTS,
    CONDITION_REQ_BOTH_UNITS,
    CONDITION_REQ_BOTH_PLAYERS,
};

// DO NOT ALTER - db2 op struct | 禁止修改枚举数值：与 DB2/数据库比较运算符定义保持一致
enum class ConditionOperation : uint8
{
    NONE                        = 0,
    EQUAL_TO                    = 1,
    NOT_EQUAL_TO                = 2,
    LESS_THAN                   = 3,
    LESS_THAN_OR_EQUAL_TO       = 4,
    GREATER_THAN                = 5,
    GREATER_THAN_OR_EQUAL_TO    = 6,
    MAX
};

// DO NOT ALTER - db2 logic struct | 禁止修改枚举数值：与 DB2/数据库组合逻辑定义保持一致
enum class ConditionLogic : uint8
{
    NONE = 0, // only execute cond 0 | 仅评估条件 0
    AND  = 1, // both conditions must be true | 逻辑与：两个条件均须成立
    OR   = 2, // one condition must be true | 逻辑或：任一条件成立即可
    XOR  = 3, // condition results must not be equal | 逻辑异或：两个条件结果必须相反
};

/// 单条数据库条件记录及其评估入口
class ConditionEntry
{
    public:
        // Default constructor, required for SQL Storage (Will give errors if used elsewise) | SQLStorage 反射所需的默认构造函数
        ConditionEntry() : m_entry(0), m_condition(CONDITION_AND), m_value1(0), m_value2(0), m_value3(0), m_value4(0), m_flags(0) {}

        ConditionEntry(uint32 _entry, int16 _condition, uint32 _value1, uint32 _value2, uint32 _value3, uint32 _value4, uint8 _flags)
            : m_entry(_entry), m_condition(ConditionType(_condition)), m_value1(_value1), m_value2(_value2), m_value3(_value3), m_value4(_value4), m_flags(_flags) {}

        // Checks correctness of values | 加载阶段校验条件参数与引用目标合法性
        bool IsValid() const;
        /// 判断该条件（含组合子条件）是否可在不传入玩家目标的情况下安全求值
        static bool CanBeUsedWithoutPlayer(uint32 entry);

        // Checks if the condition is met | 在给定上下文中求值判断条件是否满足
        bool Meets(WorldObject const* target, Map const* map, WorldObject const* source, ConditionSource conditionSourceType) const;

        /// 通用数值比较运算分发器
        static bool CheckOp(ConditionOperation op, int32 value, int32 operand);
    private:
        void DisableCondition() { m_condition = CONDITION_NONE; m_flags ^= CONDITION_FLAG_REVERSE_RESULT; }
        bool CheckParamRequirements(WorldObject const* target, Map const* map, WorldObject const* source) const;
        bool inline Evaluate(WorldObject const* target, Map const* map, WorldObject const* source, ConditionSource conditionSourceType) const;
        uint32 m_entry;                                     // entry of the condition | 条件记录 ID
        ConditionType m_condition;                          // additional condition type | 条件类型
        uint32 m_value1;                                    // data for the condition - see ConditionType definition | 条件参数 1~4
        uint32 m_value2;
        uint32 m_value3;
        uint32 m_value4;
        uint8 m_flags;                                      /// 条件标志位（取反/交换目标）
};

// Check if a player meets condition conditionId | 便捷全局函数：按条件 ID 检查给定对象/地图上下文是否满足条件
bool IsConditionSatisfied(uint32 conditionId, WorldObject const* target, Map const* map, WorldObject const* source, ConditionSource conditionSourceType);

#endif
