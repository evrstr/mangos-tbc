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

#ifndef MANGOS_SPELLTARGETS_H
#define MANGOS_SPELLTARGETS_H

#include <map>

#include "Spells/SpellTargetDefines.h"
#include "Spells/SpellEffectDefines.h"
#include "Server/DBCEnums.h"
#include "Platform/Define.h"

#include <map>
#include <utility>

// 单个隐式目标类型的元数据描述（名称、语义类别、阵营过滤、收集方式与视线基准）
struct SpellTargetInfo
{
    char const* name;                                       // 目标枚举名称
    SpellTargetImplicitType type;                           // 目标语义类型
    SpellTargetFilter filter;                               // 目标过滤规则（友方/敌方/小队等）
    SpellTargetEnumerator enumerator;                       // 目标枚举方式（单体/链式/AoE/锥形）
    SpellTargetLOS los;                                     // 视线检测基准点

    SpellTargetInfo(char const* name = "", SpellTargetImplicitType type = TARGET_TYPE_UNKNOWN, SpellTargetFilter filter = TARGET_NEUTRAL, SpellTargetEnumerator enumerator = TARGET_ENUMERATOR_UNKNOWN, SpellTargetLOS los = TARGET_LOS_CASTER);
};

// 全局法术目标类型信息表
extern SpellTargetInfo SpellTargetInfoTable[MAX_SPELL_TARGETS];

// 预计算的单个法术目标解析缓存数据（避免施法时重复搜索相同目标列表）
struct SpellTargetingData
{
    SpellTargetImplicitType implicitType[MAX_EFFECT_INDEX]; // 各效果推导出的隐式目标类型
    uint8 targetMask[MAX_EFFECT_INDEX][2] = { {1, 1}, {2, 2}, {4, 4} }; // 目标共享掩码（标明哪些效果复用同一组目标）
    std::pair<bool, bool> ignoredTargets[MAX_EFFECT_INDEX] = { {false, false}, {false, false}, {false, false} }; // 标记可跳过的冗余 A/B 目标槽位
    SpellTargetFilterScheme filteringScheme[MAX_EFFECT_INDEX][2] = { {SCHEME_RANDOM, SCHEME_RANDOM}, {SCHEME_RANDOM, SCHEME_RANDOM}, {SCHEME_RANDOM, SCHEME_RANDOM} }; // 各效果的多目标筛选策略
};

// 法术目标数据管理器（启动时只读预计算，运行期线程安全）
class SpellTargetMgr // thread safe (线程安全)
{
    public:
        static void Initialize(); // precalculates ignored targets and dynamic effect targeting (预计算动态目标类型、冗余目标及筛选策略)
        // 获取指定法术的预计算目标配置数据
        static SpellTargetingData& GetSpellTargetingData(uint32 spellId);
        // 检查给定客户端目标掩码（SpellCastTargetFlags）是否满足指定效果的隐式目标需求
        static bool CanEffectBeFilledWithMask(uint32 spellId, uint32 effIdx, uint32 mask);
        // 检查效果的目标类型是否能够消费给定目标类型
        static bool CanEffectConsumeTarget(SpellTargetImplicitType targetType, SpellTargetImplicitType effectTargetType);

        // temporary helpers (临时辅助方法)
        // 获取指定链式法术的跳跃搜索半径
        static float GetJumpRadius(uint32 spellId);
        // 获取特定法术覆盖的目标筛选排序策略（如最近/最远目标优先）
        static SpellTargetFilterScheme GetSpellTargetingFilterScheme(SpellTargetFilterScheme oldScheme, uint32 spellId);
    private:
        static std::map<uint32, SpellTargetingData> spellTargetingData; // 按法术 ID 索引的目标配置缓存
};

#endif
