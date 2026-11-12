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

#ifndef MANGOS_COMBAT_CONDITION_H
#define MANGOS_COMBAT_CONDITION_H

#include "Common.h"
#include <map>
#include <memory>

class Unit;
class UnitConditionMgr;
class WorldStateExpressionMgr;

/// 战斗条件条目定义（对应数据库 combat_condition 表，组合世界状态、自身/目标单位条件及友方/敌方数量逻辑）
struct CombatConditionEntry
{
    int32 Id;
    uint32 WorldStateConditionId;   // WorldstateExpressionId | 关联的世界状态表达式 ID
    int32 SelfConditionId;          // UnitCondition | 自身单位条件 ID
    int32 TargetConditionId;        // UnitCondition | 当前目标单位条件 ID
    uint8 FriendConditionLogic;     // CombatConditionLogic | 友方双条件组合逻辑（NONE/AND/OR/XOR）
    uint8 EnemyConditionLogic;      // CombatConditionLogic | 敌方双条件组合逻辑（NONE/AND/OR/XOR）
    int32 FriendConditionId[2];     // UnitCondition | 友方单位条件 ID 数组
    uint8 FriendConditionOp[2];     // ConditionOperation | 友方满足人数比较运算符
    uint32 FriendConditionCount[2]; // count of how many friends fulfill condition compared with op | 友方满足条件的目标数量阈值
    int32 EnemyConditionId[2];      // UnitCondition | 敌方单位条件 ID 数组
    uint8 EnemyConditionOp[2];      // ConditionOperation | 敌方满足人数比较运算符
    uint32 EnemyConditionCount[2];  // count of how many enemies fulfill condition compared with op | 敌方满足条件的目标数量阈值
};

/// 战斗条件管理器：加载并按地图数据容器评估 CombatCondition 条件
class CombatConditionMgr
{
    public:
        CombatConditionMgr(UnitConditionMgr& unitConditionMgr, WorldStateExpressionMgr& worldStateExpressionMgr);

        /// 从世界数据库加载 combat_condition 表配置并返回共享只读映射
        std::shared_ptr<std::map<int32, CombatConditionEntry>> Load();

        // radius needed for friend condition | 判定指定单位在给定友方搜索半径 range 内是否满足 CombatCondition Id
        bool Meets(Unit const* self, int32 Id, float range);

        std::shared_ptr<std::map<int32, CombatConditionEntry>> Get() { return m_combatConditions; }
    private:
        std::shared_ptr<std::map<int32, CombatConditionEntry>> m_combatConditions;
        UnitConditionMgr& m_unitConditionMgr;
        WorldStateExpressionMgr& m_worldStateExpressionMgr;
};

#endif