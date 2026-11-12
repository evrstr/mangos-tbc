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

#ifndef _SPELLMGR_H
#define _SPELLMGR_H

// 静态法术数据与服务端启动时加载的法术配置管理
// 提供针对 sSpellTemplate 法术模板的高层辅助查询接口
// For static or at-server-startup loaded spell data
// For more high level function for sSpellTemplate data

#include "Common.h"
#include "Globals/SharedDefines.h"
#include "Spells/SpellAuraDefines.h"
#include "Spells/SpellTargets.h"
#include "Server/DBCStructure.h"
#include "Server/DBCStores.h"
#include "Entities/DynamicObject.h"
#include "Entities/GameObject.h"
#include "Entities/Corpse.h"
#include "Entities/Unit.h"
#include "Entities/Player.h"
#include "Spells/SpellAuras.h"
#include "Server/SQLStorages.h"
#include "Spells/SpellEffectDefines.h"

#include <map>

class Player;
class Spell;
class Unit;
struct SpellModifier;

// only used in code
/// 代码内部使用的法术分类枚举
enum SpellCategories
{
    SPELLCATEGORY_HEALTH_MANA_POTIONS = 4,                  // 生命与法力药水分类
    SPELLCATEGORY_DEVOUR_MAGIC        = 12                          // 吞噬魔法分类
};

// Different spell properties
/// 获取法术作用半径（为空时返回 0）
inline float GetSpellRadius(SpellRadiusEntry const* radius) { return (radius ? radius->Radius : 0); }
/// 获取法术施法时间（综合施法者急速、天赋修饰及消耗触发状态）
uint32 GetSpellCastTime(SpellEntry const* spellInfo, WorldObject* caster, Spell* spell = nullptr, bool consume = false);
/// 获取用于计算法术加成系数的有效施法时间
uint32 GetSpellCastTimeForBonus(SpellEntry const* spellProto, DamageEffectType damagetype);
/// 根据法术施法时间与伤害类型计算默认法术强度/治疗加成系数
float CalculateDefaultCoefficient(SpellEntry const* spellProto, DamageEffectType const damagetype);
/// 获取法术最小施法距离
inline float GetSpellMinRange(SpellRangeEntry const* range) { return (range ? range->minRange : 0); }
/// 获取法术最大施法距离
inline float GetSpellMaxRange(SpellRangeEntry const* range) { return (range ? range->maxRange : 0); }
/// 获取法术冷却恢复时间（取自身冷却与分类冷却中的较大值）
inline uint32 GetSpellRecoveryTime(SpellEntry const* spellInfo) { return spellInfo->RecoveryTime > spellInfo->CategoryRecoveryTime ? spellInfo->RecoveryTime : spellInfo->CategoryRecoveryTime; }
/// 获取法术基础持续时间（毫秒）
int32 GetSpellDuration(SpellEntry const* spellInfo);
/// 获取法术最大持续时间（毫秒）
int32 GetSpellMaxDuration(SpellEntry const* spellInfo);
/// 计算最终法术持续时间（含递减、连击点、天赋修饰及光环脚本调整）
int32 CalculateSpellDuration(SpellEntry const* spellInfo, Unit const* caster, Unit const* target, AuraScript* auraScript);
/// 计算法术光环最大周期跳数（按 SpellEntry）
uint16 GetSpellAuraMaxTicks(SpellEntry const* spellInfo);
/// 计算法术光环最大周期跳数（按法术 ID）
uint16 GetSpellAuraMaxTicks(uint32 spellId);
/// 获取法术对应的武器攻击类型（主手/副手/远程）
WeaponAttackType GetWeaponAttackType(SpellEntry const* spellInfo);

/// 判断法术是否包含指定类型的 SpellEffects
inline bool IsSpellHaveEffect(SpellEntry const* spellInfo, SpellEffects effect)
{
    for (unsigned int i : spellInfo->Effect)
        if (SpellEffects(i) == effect)
            return true;
    return false;
}

/// 判断法术指定索引的效果是否为施加光环类效果
inline bool IsAuraApplyEffect(SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    switch (spellInfo->Effect[effecIdx])
    {
        case SPELL_EFFECT_APPLY_AURA:
        case SPELL_EFFECT_PERSISTENT_AREA_AURA:
        case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
        case SPELL_EFFECT_APPLY_AREA_AURA_PET:
        case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
        case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
        case SPELL_EFFECT_APPLY_AREA_AURA_OWNER:
            return true;
    }
    return false;
}

/// 判断法术掩码指定的各效果是否全部为施加光环类效果（至少含一个非空光环效果）
inline bool IsAuraApplyEffects(SpellEntry const* entry, SpellEffectIndexMask mask)
{
    if (!entry)
        return false;
    uint32 emptyMask = 0;
    for (uint32 i = EFFECT_INDEX_0; i < MAX_EFFECT_INDEX; ++i)
    {
        const uint32 current = (1 << i);
        if (entry->Effect[i])
        {
            if ((mask & current) && !IsAuraApplyEffect(entry, SpellEffectIndex(i)))
                return false;
        }
        else
            emptyMask |= current;
    }
    // Check if all queried effects are actually empty (检查被查询的效果是否全部为空)
    const bool empty = !(mask & ~emptyMask);
    return !empty;
}

/// 判断法术在指定效果掩码内是否至少施加一个光环
inline bool IsSpellAppliesAura(SpellEntry const* spellInfo, uint32 effectMask = ((1 << EFFECT_INDEX_0) | (1 << EFFECT_INDEX_1) | (1 << EFFECT_INDEX_2)))
{
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (effectMask & (1 << i))
            if (IsAuraApplyEffect(spellInfo, SpellEffectIndex(i)))
                return true;

    return false;
}

/// 判断法术是否包含非光环类的直接效果
inline bool IsSpellWithNonAuraEffect(SpellEntry const* spellInfo)
{
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (!IsAuraApplyEffect(spellInfo, SpellEffectIndex(i)))
            return true;

    return false;
}

/// 获取法术中所有施加光环效果的索引位掩码
inline uint32 GetAuraEffectMask(SpellEntry const* spellInfo)
{
    uint32 mask = 0;
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (IsAuraApplyEffect(spellInfo, SpellEffectIndex(i)))
            mask |= (1 << i);

    return mask;
}

/// 判断法术效果是否在法术发射/命中瞬间立即结算（直接伤害、治疗、冲锋等）
inline bool IsEffectHandledImmediatelySpellLaunch(SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    switch (spellInfo->Effect[effecIdx])
    {
        case SPELL_EFFECT_SCHOOL_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
        case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE:
        case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_HEAL_MECHANICAL:
        case SPELL_EFFECT_HEAL_PCT:
        case SPELL_EFFECT_CHARGE:
        case SPELL_EFFECT_CHARGE_DEST:
            return true;
        default:
            return false;
    }
}

/// 判断法术效果是否为周期性回复类光环（回蓝/回血/生命通道）
inline bool IsPeriodicRegenerateEffect(SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    switch (AuraType(spellInfo->EffectApplyAuraName[effecIdx]))
    {
        case SPELL_AURA_PERIODIC_ENERGIZE:
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
            return true;
        default:
            return false;
    }
}

/// 判断是否为施法结束时触发的修饰光环
bool IsCastEndProcModifierAura(SpellEntry const* spellInfo, SpellEffectIndex effecIdx, SpellEntry const* procSpell);

/// 判断法术在指定效果掩码内是否包含指定类型的光环
inline bool IsSpellHaveAura(SpellEntry const* spellInfo, AuraType aura, uint32 effectMask = (1 << EFFECT_INDEX_0) | (1 << EFFECT_INDEX_1) | (1 << EFFECT_INDEX_2))
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (effectMask & (1 << i))
            if (AuraType(spellInfo->EffectApplyAuraName[i]) == aura)
                return true;
    return false;
}

/// 判断是否为下一次近战攻击触发类法术（如英勇打击、顺劈斩）
inline bool IsNextMeleeSwingSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_ON_NEXT_SWING_NO_DAMAGE) || spellInfo->HasAttribute(SPELL_ATTR_ON_NEXT_SWING);
}

/// 判断指定效果索引是否为该法术的最后一个光环效果
inline bool IsSpellLastAuraEffect(SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    for (int i = effecIdx + 1; i < MAX_EFFECT_INDEX; ++i)
        if (spellInfo->EffectApplyAuraName[i])
            return false;
    return true;
}

/// 判断光环在堆叠时是否移除旧光环（部分属性类光环可叠加刷新而不移除）
inline bool IsAuraRemoveOnStacking(SpellEntry const* spellInfo, int32 effIdx) // TODO: extend to all effects
{
    switch (spellInfo->EffectApplyAuraName[effIdx])
    {
        case SPELL_AURA_MOD_INCREASE_ENERGY:
        case SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT:
        case SPELL_AURA_MOD_INCREASE_HEALTH:
        case SPELL_AURA_MOD_STAT:
            return false;
        default:
            return true;
    }
}

/// 判断指定效果是否为魅惑/控制/附身类光环
inline bool IsCharmAura(SpellEntry const* spellInfo, int32 effIdx) // TODO: extend to all effects
{
    switch (spellInfo->EffectApplyAuraName[effIdx])
    {
        case SPELL_AURA_MOD_CHARM:
        case SPELL_AURA_AOE_CHARM:
        case SPELL_AURA_MOD_POSSESS:
            return true;
        default:
            return false;
    }
}

/// 判断指定光环效果在重施时是否直接刷新而非重新施放
inline bool IsAuraRefreshInsteadOfRecast(SpellEntry const* spellInfo, int32 effIdx) // TODO: extend to all effects
{
    switch (spellInfo->EffectApplyAuraName[effIdx])
    {
        case SPELL_AURA_NONE:
        case SPELL_AURA_MOD_STAT:
        case SPELL_AURA_MOD_RESISTANCE:
        case SPELL_AURA_MOD_BASE_RESISTANCE:
        case SPELL_AURA_MOD_RESISTANCE_EXCLUSIVE:
        case SPELL_AURA_MOD_INCREASE_ENERGY:
        case SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE:
        case SPELL_AURA_MOD_THREAT:
        case SPELL_AURA_MOD_DAMAGE_TAKEN:
        case SPELL_AURA_PROC_TRIGGER_DAMAGE:
            return true;
        case SPELL_AURA_DUMMY:
        {
            switch (spellInfo->Id)
            {
                case 39921:                             // Vim'Gol Pentagram Beam (维姆戈尔五芒星光束)
                    return true;
                default:
                    return false;
            }
        }
        default:
            return false;
    }
}

/// 判断法术的所有光环效果在重施时是否均采用直接刷新策略
inline bool IsAuraSpellRefreshInsteadOfRecast(SpellEntry const* spellInfo)
{
    if (IsAuraRefreshInsteadOfRecast(spellInfo, 0) && IsAuraRefreshInsteadOfRecast(spellInfo, 1) && IsAuraRefreshInsteadOfRecast(spellInfo, 2))
        return true;

    return false;
}

/// 判断法术是否允许以死亡目标/尸体为施法对象
inline bool IsAllowingDeadTarget(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX2_ALLOW_DEAD_TARGET) || spellInfo->HasAttribute(SPELL_ATTR_PASSIVE) || spellInfo->Targets & (TARGET_FLAG_CORPSE_ENEMY | TARGET_FLAG_UNIT_DEAD | TARGET_FLAG_CORPSE_ALLY);
}

/// 判断法术是否为圣骑士圣印类法术（基于家族标志与自身目标校验）
inline bool IsSealSpell(SpellEntry const* spellInfo)
{
    // Collection of all the seal family flags. No other paladin spell has any of those. (所有圣印家族标志位的集合，其他圣骑士法术均不包含这些位)
    return spellInfo->IsFitToFamily(SPELLFAMILY_PALADIN, uint64(0x000004000A000200)) &&
           // avoid counting target triggered effect as seal for avoid remove it or seal by it. (避免将圣印对目标触发的伤害/效果也误判为圣印本身)
           spellInfo->EffectImplicitTargetA[0] == TARGET_UNIT_CASTER;
}

/// 判断法术指定效果是否为触发子法术类效果
inline bool IsSpellEffectTriggerSpell(const SpellEntry* entry, SpellEffectIndex effIndex)
{
    if (!entry)
        return false;

    switch (entry->Effect[effIndex])
    {
        case SPELL_EFFECT_TRIGGER_MISSILE:
        case SPELL_EFFECT_TRIGGER_SPELL:
        case SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE:
        case SPELL_EFFECT_TRIGGER_SPELL_2:
            return true;
    }
    return false;
}

/// 判断法术指定效果是否为光环触发子法术/伤害类效果
inline bool IsSpellEffectTriggerSpellByAura(const SpellEntry* entry, SpellEffectIndex effIndex)
{
    if (!entry || !IsAuraApplyEffect(entry, effIndex))
        return false;

    switch (entry->EffectApplyAuraName[effIndex])
    {
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
        case SPELL_AURA_PROC_TRIGGER_SPELL:
        case SPELL_AURA_PROC_TRIGGER_DAMAGE:
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
        case SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE:
            return true;
    }
    return false;
}

/// 判断法术的单个效果是否能够产生暴击
inline bool IsSpellEffectAbleToCrit(const SpellEntry* entry, SpellEffectIndex index)
{
    if (!entry || entry->HasAttribute(SPELL_ATTR_EX2_CANT_CRIT))
        return false;

    switch (entry->Effect[index])
    {
        case SPELL_EFFECT_SCHOOL_DAMAGE:
        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
        case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE:
        case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            return true;
        case SPELL_EFFECT_ENERGIZE: // Mana Potion and similar spells, Lay on hands
            return (entry->EffectMiscValue[index] == POWER_MANA && entry->SpellFamilyName && entry->DmgClass);
    }
    return false;
}

/// 判断法术是否至少有一个效果能够产生暴击
inline bool IsSpellAbleToCrit(const SpellEntry* entry)
{
    if (!entry || entry->HasAttribute(SPELL_ATTR_EX2_CANT_CRIT))
        return false;

    for (uint32 i = EFFECT_INDEX_0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (entry->Effect[i] && IsSpellEffectAbleToCrit(entry, SpellEffectIndex(i)))
            return true;
    }
    return false;
}

/// 判断法术是否为被动法术（按 SpellEntry）
inline bool IsPassiveSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_PASSIVE);
}

/// 判断法术是否为被动法术（按法术 ID）
inline bool IsPassiveSpell(uint32 spellId)
{
    const SpellEntry* entry = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    return (entry && IsPassiveSpell(entry));
}

/// 判断不同等级的被动法术是否允许同时叠加生效（非光环类被动）
inline bool IsPassiveSpellStackableWithRanks(SpellEntry const* spellProto)
{
    if (!IsPassiveSpell(spellProto))
        return false;

    return !IsSpellHaveEffect(spellProto, SPELL_EFFECT_APPLY_AURA) && !IsSpellHaveEffect(spellProto, SPELL_EFFECT_APPLY_AREA_AURA_PARTY);
}

/// 判断法术是否允许宠物/AI 自动施放（按 SpellEntry）
inline bool IsAutocastable(SpellEntry const* spellInfo)
{
    return !(spellInfo->HasAttribute(SPELL_ATTR_EX_NO_AUTOCAST_AI) || spellInfo->HasAttribute(SPELL_ATTR_PASSIVE));
}

/// 判断法术是否允许宠物/AI 自动施放（按法术 ID）
inline bool IsAutocastable(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellInfo)
        return false;
    return IsAutocastable(spellInfo);
}

/// 判断法术是否需要在对象更新时同步发送给客户端
inline bool IsSpellNeedSendOnObjectUpdate(uint32 spellId)
{
    switch (spellId)
    {
        case 39123:
            return true;
        default: return false;
    }
}

/// 判断法术是否移除所有移动限制与控制类效果（如徽章/无敌类净化）
inline bool IsSpellRemoveAllMovementAndControlLossEffects(SpellEntry const* spellProto)
{
    return spellProto->EffectApplyAuraName[EFFECT_INDEX_0] == SPELL_AURA_MECHANIC_IMMUNITY &&
           spellProto->EffectMiscValue[EFFECT_INDEX_0] == 1 &&
           spellProto->EffectApplyAuraName[EFFECT_INDEX_1] == 0 &&
           spellProto->EffectApplyAuraName[EFFECT_INDEX_2] == 0 &&
           spellProto->HasAttribute(SPELL_ATTR_EX_IMMUNITY_PURGES_EFFECT);
}

/// 提取法术提供的机制免疫位掩码
inline uint32 GetAllowedMechanicMask(SpellEntry const* spellProto)
{
    uint32 mask = 0;
    for (uint8 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!spellProto->Effect[i])
            continue;

        if (spellProto->EffectApplyAuraName[i] == SPELL_AURA_MECHANIC_IMMUNITY)
            mask |= 1 << uint32(spellProto->EffectMiscValue[i] - 1);
        else if (spellProto->EffectApplyAuraName[i] == SPELL_AURA_MECHANIC_IMMUNITY_MASK)
            mask |= uint32(spellProto->EffectMiscValue[i]);
    }
    return mask;
}

// based on client Spell_C::CancelsAuraEffect
/// 判断施放法术的免疫净化属性是否会驱散目标身上已有的光环效果
inline bool SpellCancelsAuraEffect(SpellEntry const* spellInfo, SpellEntry const* auraSpellInfo, uint8 auraEffIndex)
{
    if (!spellInfo->HasAttribute(SPELL_ATTR_EX_IMMUNITY_PURGES_EFFECT))
        return false;

    if (auraSpellInfo->HasAttribute(SPELL_ATTR_NO_IMMUNITIES))
        return false;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effect[i] != SPELL_EFFECT_APPLY_AURA)
            continue;

        uint32 const miscValue = static_cast<uint32>(spellInfo->EffectMiscValue[i]);
        switch (spellInfo->EffectApplyAuraName[i])
        {
            case SPELL_AURA_STATE_IMMUNITY:
                if (miscValue != auraSpellInfo->EffectApplyAuraName[auraEffIndex])
                    continue;
                break;
            case SPELL_AURA_SCHOOL_IMMUNITY:
                if (auraSpellInfo->HasAttribute(SPELL_ATTR_EX2_NO_SCHOOL_IMMUNITIES) || !(auraSpellInfo->SchoolMask & miscValue))
                    continue;
                break;
            case SPELL_AURA_DISPEL_IMMUNITY:
                if (miscValue != auraSpellInfo->Dispel)
                    continue;
                break;
            case SPELL_AURA_MECHANIC_IMMUNITY:
                if (miscValue != auraSpellInfo->Mechanic)
                    if (miscValue != auraSpellInfo->EffectMechanic[auraEffIndex])
                        continue;
                break;
            default:
                continue;
        }

        return true;
    }

    return false;
}

/// 判断法术是否仅能对幽灵/死亡状态施放
inline bool IsDeathOnlySpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX3_ONLY_ON_GHOSTS) || spellInfo->Id == 2584;
}

/// 判断光环在目标死亡后是否依然保留
inline bool IsDeathPersistentSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX3_ALLOW_AURA_WHILE_DEAD);
}

/// 判断是否为非战斗专用平和法术
inline bool IsNonCombatSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_NOT_IN_COMBAT_ONLY_PEACEFUL);
}

// some creatures should run immediately after being summoned by spell
/// 判断被该法术召唤的生物生成后是否应立即设为奔跑模式
inline bool IsSpellSetRun(SpellEntry const* spellInfo)
{
    switch (spellInfo->Id)
    {
        case 30445: // Stillpine Ancestor Yor
        case 39163: // [DND]Rescue Wyvern
        case 43486: // Summon Amani'shi Warriors
        case 43487: // Summon Amani Eagle
        case 46214: // Summon Imp
        case 46245: // Summon Shadowsword Deathbringer
            return true;
        default:
            return false;
    }
}

/// 判断生物脱离战斗（Evade）时是否应移除该法术光环
inline bool IsSpellRemovedOnEvade(SpellEntry const* spellInfo)
{
    if (IsSpellHaveAura(spellInfo, SPELL_AURA_FLY))
        return false;

    if (IsSpellHaveAura(spellInfo, SPELL_AURA_MOD_CHARM))
        return false;

    if (IsSpellHaveAura(spellInfo, SPELL_AURA_MOD_POSSESS))
        return false;

    if (spellInfo->HasAttribute(SPELL_ATTR_SS_IGNORE_EVADE))
        return false;

    if (spellInfo->HasAttribute(SPELL_ATTR_EX_AURA_STAYS_AFTER_COMBAT))
        return false;

    switch (spellInfo->Id)
    {
        case 588:           // Inner Fire (Rank 1)
        case 3235:          // Rancid Blood
        case 3284:          // Violent Shield
        case 3417:          // Thrash
        case 3418:          // Improved Blocking
        case 3509:          // Sloth Passive
        case 3512:          // Sludge Passive
        case 3582:          // Torch Burst
        case 3616:          // Poison Proc
        case 3637:          // Improved Blocking III
        case 4148:          // Growl of Fortitude
        case 5111:          // Living Flame Passive
        case 5301:          // Defensive State (DND)
        case 5680:          // Torch Burn
        case 6488:          // Sarilus's Elementals Passive
        case 6498:          // Feed Sarilus Passive
        case 6718:          // Phasing Stealth
        case 6752:          // Weak Poison Proc
        case 6820:          // Corrupted Agility Passive
        case 6821:          // Corrupted Strength Passive
        case 6822:          // Corrupted Stamina Passive
        case 6823:          // Corrupted Intellect Passive
        case 6923:          // Growl of Fortitude Proc
        case 6947:          // Curse of the Bleakheart Proc
        case 6961:          // Knockdown Proc
        case 7056:          // Pacified
        case 7090:          // Bear Form (Shapeshift)
        case 7095:          // Knockdown Proc
        case 7165:          // Battle Stance (Rank 1)
        case 7276:          // Poison Proc
        case 7999:          // Soot Covering
        case 8247:          // Wandering Plague
        case 8279:          // Stealth Detection
        case 8393:          // Barbs
        case 8599:          // Enrage
        case 8601:          // Slowing Poison
        case 8876:          // Thrash
        case 8909:          // Unholy Shield
        case 8990:          // Retribution Aura (Rank 1)
        case 9205:          // Hate to Zero (Hate to Zero)
        case 9460:          // Corrosive Ooze
        case 9464:          // Barbs
        case 9617:          // Ghost Visual
        case 9769:          // Radiation
        case 9941:          // Spell Reflection
        case 10022:         // Deadly Poison
        case 10072:         // Splintered Obsidian
        case 10074:         // Spell Reflection
        case 10095:         // Hate to Zero (Hate to Zero)
        case 11838:         // Hate to Zero (Hate to Zero)
        case 11919:         // Poison Proc
        case 11959:         // Poison Proc
        case 11964:         // Fevered Fatigue
        case 11966:         // Fire Shield
        case 11984:         // Immolate
        case 12099:         // Shield Spike
        case 12187:         // Disease Cloud
        case 12246:         // Infected Spine
        case 12529:         // Chilling Touch
        case 12539:         // Ghoul Rot
        case 12544:         // Frost Armor
        case 12546:         // Spitelash (Spitelash)
        case 12550:         // Lightning Shield
        case 12556:         // Frost Armor
        case 12627:         // Disease Cloud
        case 12787:         // Thrash
        case 12898:         // Smoke Aura Visual
        case 13260:         // Pet Bomb Passive
        case 13299:         // Poison Proc
        case 13616:         // Wracking Pains Proc
        case 13767:         // Hate to Zero (Hate to Zero)
        case 13787:         // Demon Armor
        case 14111:         // Bloodpetal Poison Proc
        case 14133:         // Muculent Fever Proc
        case 14178:         // Sticky Tar
        case 15088:         // Flurry
        case 15097:         // Enrage
        case 15506:         // Immolate
        case 15876:         // Ice Blast
        case 16140:         // Exploding Cadaver (Exploding Cadaver)
        case 16345:         // Disease Cloud
        case 16563:         // Drowning Death
        case 16577:         // Disease Cloud
        case 16592:         // Shadowform
        case 17327:         // Spirit Particles
        case 17467:         // Unholy Aura
        case 18148:         // Static Field
        case 18268:         // Fire Shield
        case 18847:         // Fevered Fatigue
        case 18943:         // Double Attack
        case 18968:         // Fire Shield
        case 19030:         // Bear Form (Shapeshift)
        case 18950:         // Invisibility and Stealth Detection
        case 19194:         // Double Attack
        case 19195:         // Hate to 90% (Hate to 90%)
        case 19396:         // Incinerate (Incinerate)
        case 19483:         // Immolation
        case 19514:         // Lightning Shield
        case 19626:         // Fire Shield (Fire Shield)
        case 19640:         // Pummel (Pummel)
        case 19817:         // Double Attack
        case 19818:         // Double Attack
        case 20514:         // Ruul Snowhoof Shapechange (DND)
        case 21061:         // Putrid Breath
        case 21857:         // Lava Shield
        case 21862:         // Radiation
        case 22128:         // Thorns
        case 22578:         // Glowy (Black)
        case 22650:         // Ghost Visual
        case 22735:         // Spirit of Runn Tum
        case 22781:         // Thornling
        case 22788:         // Grow
        case 22856:         // Ice Lock (Guard Slip'kik ice trap in Dire Maul)
        case 23255:         // Deep Wounds
        case 24313:         // Shade Visual
        case 25039:         // Green Ghost Visual
        case 25592:         // Hate to Zero (Hate to Zero)
        case 26341:         // Saurfang's Rage
        case 27578:         // Battle Shout
        case 27793:         // Disease Cloud
        case 27987:         // Unholy Aura
        case 28002:         // Ghost Visual
        case 28126:         // Spirit Particles (purple)
        case 28156:         // Disease Cloud
        case 28362:         // Disease Cloud
        case 28370:         // Toxic Gas
        case 28902:         // Bloodlust
        case 29406:         // Shadowform
        case 29526:         // Hate to Zero (Hate to Zero)
        case 30074:         // Toxic Gas
        case 30205:         // Shadow Cage - Magtheridon
        case 30982:         // Crippling Poison
        case 30987:         // Ghost Visual (Red)
        case 31332:         // Dire Wolf Visual
        case 31387:         // Time Rift Channel
        case 31607:         // Disease Cloud
        case 31690:         // Putrid Mushroom
        case 31722:         // Immolation
        case 31757:         // Pulverize
        case 31792:         // Bear Form (Shapeshift)
        case 32007:         // Mo'arg Engineer Transform Visual
        case 32064:         // Battle Shout
        case 32732:         // Flay
        case 32900:         // Bone Shards Proc
        case 32912:         // Windfury
        case 32939:         // Phase Burst
        case 32942:         // Phasing Invisibility
        case 33460:         // Inhibit Magic
        case 33483:         // Mana Tap
        case 33839:         // Vir'aani Concentration
        case 33900:         // Shroud of Death
        case 33908:         // Burning Spikes
        case 34343:         // Thorns
        case 35184:         // Unstable Affliction Proc
        case 35186:         // Melt Armor Proc
        case 35188:         // Chilling Touch
        case 35194:         // Shadowform
        case 35281:         // Raging Flames
        case 35319:         // Electric Skin
        case 35408:         // Fear Proc
        case 35596:         // Power of the Legion
        case 35747:         // Flame Buffet
        case 35838:         // Ghost Visual
        case 35841:         // Draenei Spirit Visual
        case 35847:         // Ghost Visual Red
        case 35850:         // Draenei Spirit Visual 2
        case 35917:         // Firey Intellect
        case 36006:         // Fel Fire Aura
        case 36118:         // Phasing Invisibility
        case 36635:         // Fiery Weapon Proc
        case 36640:         // Arcane Shield
        case 36784:         // Entropic Aura
        case 36788:         // Diminish Soul
        case 37119:         // Spirit Particles (Spawn)
        case 37256:         // Disease Cloud
        case 37266:         // Disease Cloud
        case 37411:         // Skettis Corrupted Ghosts
        case 37497:         // Shadowmoon Ghost Invisibility (Ghostrider of Karabor in SMV) 
        case 37509:         // Ghostly Facade
        case 37816:         // Shadowform
        case 38732:         // Fire Shield
        case 37863:         // Disease Cloud
        case 38844:         // Unholy Aura
        case 38847:         // Diminish Soul
        case 38857:         // Spell Ground
        case 39102:         // Glowy (Black)
        case 39311:         // Scrapped Fel Reaver transform aura that is never removed even on evade
        case 39918:         // Soulgrinder Ritual Visual ( in progress)
        case 39920:         // Soulgrinder Ritual Visual ( beam)
        case 40453:         // Aura of Fear
        case 40816:         // Saber Lash - Mother Shahraz
        case 40899:         // Felfire Proc
        case 41290:         // Disease Cloud
        case 41634:         // Invisibility and Stealth Detection
        case 42459:         // Dual Wield (Passive)
        case 44118:         // Fists of Arcane Fury
        case 44480:         // Seal of Wrath
        case 44505:         // Drink Fel Infusion
        case 44520:         // Fel Armor (Rank 2)
        case 44537:         // Fel Lightning
        case 44604:         // Enchantment of Spell Haste
        case 44855:         // Out of Phase
        case 44962:         // Archer - BE Male Transform Tier 1
        case 44918:         // Archer - BE Male Transform Tier 2
        case 44919:         // Archer - BE Male Transform Tier 3
        case 44920:         // Archer - BE Male Transform Tier 4
        case 44921:         // Archer - BE Female Transform Tier 1
        case 44922:         // Archer - BE Female Transform Tier 2
        case 44923:         // Archer - BE Female Transform Tier 3
        case 44924:         // Archer - BE Female Transform Tier 4
        case 44925:         // Archer - Draenei Male Transform Tier 1
        case 44926:         // Archer - Draenei Male Transform Tier 2
        case 44927:         // Archer - Draenei Male Transform Tier 3
        case 44928:         // Archer - Draenei Male Transform Tier 4
        case 44929:         // Archer - Draenei Female Transform Tier 1
        case 44930:         // Archer - Draenei Female Transform Tier 2
        case 44931:         // Archer - Draenei Female Transform Tier 3
        case 44932:         // Archer - Draenei Female Transform Tier 4
        case 44977:         // Fel Armor (Rank 2)
        case 45033:         // Abyssal Transformation
        case 45155:         // Warrior - BE Female Transform Tier 1
        case 45156:         // Warrior - BE Female Transform Tier 2
        case 45157:         // Warrior - BE Female Transform Tier 3
        case 45158:         // Warrior - BE Female Transform Tier 4
        case 45159:         // Warrior - BE Male Transform Tier 1
        case 45160:         // Warrior - BE Male Transform Tier 2
        case 45161:         // Warrior - BE Male Transform Tier 3
        case 45162:         // Warrior - BE Male Transform Tier 4
        case 45163:         // Warrior - Draenei Female Transform Tier 1
        case 45164:         // Warrior - Draenei Female Transform Tier 2
        case 45165:         // Warrior - Draenei Female Transform Tier 3
        case 45166:         // Warrior - Draenei Female Transform Tier 4
        case 45167:         // Warrior - Draenei Male Transform Tier 1
        case 45168:         // Warrior - Draenei Male Transform Tier 2
        case 45169:         // Warrior - Draenei Male Transform Tier 3
        case 45170:         // Warrior - Draenei Male Transform Tier 4
        case 45187:         // Dawnblade Attack
        case 45822:         // Iceblood Warmaster
        case 45823:         // Tower Point Warmaster
        case 45824:         // West Frostwolf Warmaster
        case 45826:         // East Frostwolf Warmaster
        case 45828:         // Dun Baldar North Marshal
        case 45829:         // Dun Baldar South Marshal
        case 45830:         // Stonehearth Marshal
        case 45831:         // Icewing Marshal
        case 46030:         // Seal of Wrath
        case 46048:         // Fel Lightning
        case 46277:         // Bring Pain
        case 46308:         // Burning Winds
        case 46565:         // Holyform
        case 46744:         // Chilling Touch
        case 47287:         // Burning Destruction
        case 47399:         // Frenzy
            return false;
        default:
            return true;
    }
}

/// 判断引导法术是否属于延迟生效型引导
inline bool IsChanneledDelayedSpell(SpellEntry const* spellInfo)
{
    if (!spellInfo->HasAttribute(SPELL_ATTR_EX_IS_CHANNELED) && !spellInfo->HasAttribute(SPELL_ATTR_EX_IS_SELF_CHANNELED))
        return false;

    switch (spellInfo->Id)
    {
        case 39238: // Fumping
        case 39246:
            return true;
        default: return false;
    }
}

/// 判断目标类型是否明确为正面（友方）目标
bool IsExplicitPositiveTarget(uint32 targetA);
/// 判断目标类型是否明确为负面（敌方）目标
bool IsExplicitNegativeTarget(uint32 targetA);

/// 判断法术是否可被抗性减免或抵抗
inline bool IsResistableSpell(const SpellEntry* entry)
{
    return entry->DmgClass != SPELL_DAMAGE_CLASS_NONE;
}

/// 判断法术指定效果是否为伤害类效果（含直接伤害与 DoT 周期伤害）
inline bool IsSpellEffectDamage(SpellEntry const& spellInfo, SpellEffectIndex i)
{
    if (!spellInfo.EffectApplyAuraName[i])
    {
        // If its not an aura effect, check for damage effects
        switch (spellInfo.Effect[i])
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_ENVIRONMENTAL_DAMAGE:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            //   SPELL_EFFECT_POWER_BURN: deals damage for power burned, but its either full damage or resist?
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                return true;
        }
    }
    else
    {
        // If its an aura effect, check for DoT auras
        switch (spellInfo.EffectApplyAuraName[i])
        {
            case SPELL_AURA_PERIODIC_DAMAGE:
            case SPELL_AURA_PERIODIC_LEECH:
            //   SPELL_AURA_POWER_BURN_MANA: deals damage for power burned, but not really a DoT?
            case SPELL_AURA_PERIODIC_MANA_LEECH: // confirmed via 31447
            case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                return true;
        }
    }
    return false;
}

/// 判断法术指定效果是否为 Dummy 占位/脚本效果或 Dummy 光环
inline bool IsSpellEffectDummy(SpellEntry const& spellInfo, SpellEffectIndex i)
{
    if (!spellInfo.EffectApplyAuraName[i])
    {
        switch (spellInfo.Effect[i])
        {
            case SPELL_EFFECT_DUMMY:
                return true;
        }
    }
    else
    {
        switch (spellInfo.EffectApplyAuraName[i])
        {
            case SPELL_AURA_DUMMY:
            case SPELL_AURA_PERIODIC_DUMMY:
                return true;
        }
    }
    return false;
}

/// 判断法术指定掩码内的所有非空效果是否均为伤害类效果
inline bool IsSpellEffectsDamage(SpellEntry const& spellInfo, uint8 effectMask = EFFECT_MASK_ALL)
{
    for (uint8 i = EFFECT_INDEX_0; (i < MAX_EFFECT_INDEX && effectMask); (++i, (effectMask >>= 1)))
    {
        if (spellInfo.Effect[i] && (effectMask & 1) && !IsSpellEffectDamage(spellInfo, SpellEffectIndex(i)))
            return false;
    }
    return true;
}

/// 判断法术是否为二元法术（Binary Spell：非全抵抗即全命中，无部分抵抗）
inline bool IsBinarySpell(SpellEntry const& spellInfo, uint8 effectMask = EFFECT_MASK_ALL)
{
    // Spell is considered binary if:
    // * (Pre-WotLK): It contains non-damage effects or auras
    // * (WotLK+): It contains no damage effects or auras
    uint8 validmask = 0;    // A bitmask of effects: set bits are valid effects
    uint8 nondmgmask = 0;   // A bitmask of effects: set bits are non-damage effects
    uint8 auramask = 0;     // A bitmask of aura effcts: set bits are auras
    for (uint32 i = EFFECT_INDEX_0; i < MAX_EFFECT_INDEX; ++i)
    {
        const uint8 thisMask = uint8(1 << i);

        if (!spellInfo.Effect[i] || !(effectMask & thisMask))
            continue;

        if (IsSpellEffectDummy(spellInfo, SpellEffectIndex(i)) || IsSpellEffectTriggerSpell(&spellInfo, SpellEffectIndex(i)))
            continue;

        validmask |= thisMask;

        if (spellInfo.EffectApplyAuraName[i])
            auramask |= thisMask;
        if (!IsSpellEffectDamage(spellInfo, SpellEffectIndex(i)))
            nondmgmask |= thisMask;
    }
    // No valid effects: treat as non-binary
    if (!validmask)
        return false;
    // All effects are non-damage: treat as binary
    // All effects are auras: treat as binary (even pure DoTs are treated as binary on initial application)
    if (nondmgmask == validmask || auramask == validmask)
        return true;
    const uint8 dmgmask = (validmask & ~nondmgmask);
    const uint8 dotmask = (dmgmask & auramask);
    // Just in case, if all damage effects are DoTs: treat as binary
    if (dmgmask == dotmask)
        return true;
    // If we ended up here, we have at least one non-aura damage effect
    // Pre-WotLK: check if at least one non-damage effect hits the same target as damage effect (e.g. Frostbolt) and treat as binary
    if (nondmgmask)
    {
        uint8 directmask = (dmgmask & ~dotmask);
        for (uint8 i = EFFECT_INDEX_0; directmask; (++i, (directmask >>= 1)))
        {
            if (directmask & 1)
            {
                uint8 jmask = nondmgmask;
                for (uint8 j = EFFECT_INDEX_0; jmask; (++j, (jmask >>= 1)))
                {
                    if (jmask & 1)
                    {
                        if (spellInfo.EffectImplicitTargetA[i] == spellInfo.EffectImplicitTargetA[j] &&
                            spellInfo.EffectImplicitTargetB[i] == spellInfo.EffectImplicitTargetB[j])
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

/// 判断目标类型是否以施法者自身作为位置/范围参考源
inline bool IsCasterSourceTarget(uint32 target)
{
    switch (target)
    {
        case TARGET_UNIT_CASTER:
        case TARGET_UNIT_CASTER_PET:
        case TARGET_ENUM_UNITS_PARTY_WITHIN_CASTER_RANGE:
        case TARGET_ENUM_UNITS_ENEMY_IN_CONE_24:
        case TARGET_UNIT_CASTER_MASTER:
        case TARGET_LOCATION_UNIT_MINION_POSITION:
        case TARGET_ENUM_UNITS_PARTY_AOE_AT_SRC_LOC:
        case TARGET_ENUM_UNITS_PARTY_AOE_AT_DEST_LOC:
        case TARGET_LOCATION_CASTER_FISHING_SPOT:
        case TARGET_LOCATION_CASTER_FRONT_RIGHT:
        case TARGET_LOCATION_CASTER_BACK_RIGHT:
        case TARGET_LOCATION_CASTER_BACK_LEFT:
        case TARGET_LOCATION_CASTER_FRONT_LEFT:
        case TARGET_ENUM_GAMEOBJECTS_SCRIPT_AOE_AT_DEST_LOC:
        case TARGET_LOCATION_CURRENT_REFERENCE:
        case TARGET_UNIT_CASTER_COMPANION:
            return true;
        default:
            break;
    }
    return false;
}

/// 判断法术是否使用脚本指定的附近单位作为隐式目标
inline bool IsSpellWithScriptUnitTarget(SpellEntry const* spellInfo)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!spellInfo->Effect[i])
            continue;
        switch (spellInfo->EffectImplicitTargetA[i])
        {
            case TARGET_UNIT_SCRIPT_NEAR_CASTER:
                return true;
        }
    }

    return false;
}

/// 判断法术的所有非空效果是否仅使用以施法者为源的目标模式
inline bool IsSpellWithCasterSourceTargetsOnly(SpellEntry const* spellInfo)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!spellInfo->Effect[i])                          // Skip junk in DBC
            continue;

        uint32 targetA = spellInfo->EffectImplicitTargetA[i];
        if (targetA && !IsCasterSourceTarget(targetA))
            return false;

        uint32 targetB = spellInfo->EffectImplicitTargetB[i];
        if (targetB && !IsCasterSourceTarget(targetB))
            return false;

        if (!targetA && !targetB)
            return false;
    }
    return true;
}

/// 判断目标类型是否为坐标点（位置）目标
inline bool IsPointEffectTarget(SpellTarget target)
{
    switch (target)
    {
        case TARGET_LOCATION_CASTER_HOME_BIND:
        case TARGET_LOCATION_DATABASE:
        case TARGET_LOCATION_CASTER_SRC:
        case TARGET_LOCATION_SCRIPT_NEAR_CASTER:
        case TARGET_LOCATION_CASTER_TARGET_POSITION:
        case TARGET_LOCATION_UNIT_POSITION:
        case TARGET_LOCATION_CHANNEL_TARGET_DEST:
        case TARGET_LOCATION_NORTH:
        case TARGET_LOCATION_SOUTH:
        case TARGET_LOCATION_EAST:
        case TARGET_LOCATION_WEST:
        case TARGET_LOCATION_NE:
        case TARGET_LOCATION_NW:
        case TARGET_LOCATION_SE:
        case TARGET_LOCATION_SW:
            return true;
        default:
            break;
    }
    return false;
}

/// 判断目标类型是否为范围正面（友方/小队/团队）目标
inline bool IsAreaEffectPositiveTarget(SpellTarget target)
{
    switch (target)
    {
        case TARGET_ENUM_UNITS_PARTY_WITHIN_CASTER_RANGE:
        case TARGET_ENUM_UNITS_FRIEND_AOE_AT_SRC_LOC:
        case TARGET_ENUM_UNITS_FRIEND_AOE_AT_DEST_LOC:
        case TARGET_ENUM_UNITS_PARTY_AOE_AT_SRC_LOC:
        case TARGET_ENUM_UNITS_PARTY_AOE_AT_DEST_LOC:
        case TARGET_UNIT_FRIEND_AND_PARTY:
        case TARGET_ENUM_UNITS_RAID_WITHIN_CASTER_RANGE:
        case TARGET_UNIT_RAID_AND_CLASS:
            return true;
        default:
            break;
    }
    return false;
}

/// 判断目标类型是否为区域范围（AOE）目标
inline bool IsAreaEffectTarget(SpellTarget target)
{
    switch (target)
    {
        case TARGET_ENUM_UNITS_SCRIPT_AOE_AT_SRC_LOC:
        case TARGET_ENUM_UNITS_SCRIPT_AOE_AT_DEST_LOC:
        case TARGET_ENUM_UNITS_ENEMY_AOE_AT_SRC_LOC:
        case TARGET_ENUM_UNITS_ENEMY_AOE_AT_DEST_LOC:
        case TARGET_ENUM_UNITS_PARTY_WITHIN_CASTER_RANGE:
        case TARGET_ENUM_UNITS_ENEMY_IN_CONE_24:
        case TARGET_ENUM_UNITS_ENEMY_AOE_AT_DYNOBJ_LOC:
        case TARGET_ENUM_UNITS_FRIEND_AOE_AT_SRC_LOC:
        case TARGET_ENUM_UNITS_FRIEND_AOE_AT_DEST_LOC:
        case TARGET_ENUM_UNITS_PARTY_AOE_AT_SRC_LOC:
        case TARGET_ENUM_UNITS_PARTY_AOE_AT_DEST_LOC:
        case TARGET_UNIT_FRIEND_AND_PARTY:
        case TARGET_ENUM_GAMEOBJECTS_SCRIPT_AOE_AT_DEST_LOC:
        case TARGET_ENUM_UNITS_RAID_WITHIN_CASTER_RANGE:
        case TARGET_UNIT_RAID_AND_CLASS:
            return true;
        default:
            break;
    }
    return false;
}

/// 判断法术是否为范围（AOE）法术
inline bool IsAreaOfEffectSpell(SpellEntry const* spellInfo)
{
    if (IsAreaEffectTarget(SpellTarget(spellInfo->EffectImplicitTargetA[EFFECT_INDEX_0])) || IsAreaEffectTarget(SpellTarget(spellInfo->EffectImplicitTargetB[EFFECT_INDEX_0])))
        return true;
    if (IsAreaEffectTarget(SpellTarget(spellInfo->EffectImplicitTargetA[EFFECT_INDEX_1])) || IsAreaEffectTarget(SpellTarget(spellInfo->EffectImplicitTargetB[EFFECT_INDEX_1])))
        return true;
    if (IsAreaEffectTarget(SpellTarget(spellInfo->EffectImplicitTargetA[EFFECT_INDEX_2])) || IsAreaEffectTarget(SpellTarget(spellInfo->EffectImplicitTargetB[EFFECT_INDEX_2])))
        return true;
    return false;
}

/// 判断法术效果ID是否为区域光环（Area Aura）效果
inline bool IsAreaAuraEffect(uint32 effect)
{
    if (effect == SPELL_EFFECT_APPLY_AREA_AURA_PARTY    ||
            effect == SPELL_EFFECT_APPLY_AREA_AURA_FRIEND   ||
            effect == SPELL_EFFECT_APPLY_AREA_AURA_ENEMY    ||
            effect == SPELL_EFFECT_APPLY_AREA_AURA_PET      ||
            effect == SPELL_EFFECT_APPLY_AREA_AURA_OWNER)
        return true;
    return false;
}

/// 判断法术是否包含任意区域光环效果
inline bool HasAreaAuraEffect(SpellEntry const* spellInfo)
{
    for (unsigned int i : spellInfo->Effect)
        if (IsAreaAuraEffect(i))
            return true;
    return false;
}

/// 判断法术效果ID是否为持久性地面区域光环
inline bool IsPersistentAuraEffect(uint32 effect)
{
    return effect == SPELL_EFFECT_PERSISTENT_AREA_AURA;
}

/// 判断法术是否包含持久性地面区域光环效果
inline bool HasPersistentAuraEffect(SpellEntry const* spellInfo)
{
    for (unsigned int i : spellInfo->Effect)
        if (IsPersistentAuraEffect(i))
            return true;
    return false;
}

/// 判断法术是否仅以施法者自身为目标
inline bool IsOnlySelfTargeting(SpellEntry const* spellInfo)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!spellInfo->Effect[i])
            return true;

        switch (spellInfo->EffectImplicitTargetA[i])
        {
            case TARGET_UNIT_CASTER:
            case TARGET_LOCATION_CURRENT_REFERENCE:
                break;
            default:
                return false;
        }
        switch (spellInfo->EffectImplicitTargetB[i])
        {
            case TARGET_UNIT_CASTER:
            case TARGET_LOCATION_CURRENT_REFERENCE:
            case TARGET_NONE:
                break;
            default:
                return false;
        }
    }
    return true;
}
 
/// 判断法术是否包含直接伤害效果
inline bool IsDirectDamageSpell(SpellEntry const* spellInfo)
{
    if (spellInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_SCHOOL_DAMAGE || spellInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL)
        return true;

    if (spellInfo->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_SCHOOL_DAMAGE || spellInfo->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL)
        return true;

    if (spellInfo->Effect[EFFECT_INDEX_2] == SPELL_EFFECT_SCHOOL_DAMAGE || spellInfo->Effect[EFFECT_INDEX_2] == SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL)
        return true;

    return false;
}

/// 判断目标类型在 CheckCast 阶段是否需要校验可攻击/可协助性
inline bool IsCheckCastTarget(uint32 target)
{
    // copy of checkcast block for attackability and assistability
    auto& data = SpellTargetInfoTable[target];
    if (data.type == TARGET_TYPE_UNIT && data.filter != TARGET_SCRIPT && (data.enumerator == TARGET_ENUMERATOR_SINGLE || data.enumerator == TARGET_ENUMERATOR_CHAIN))
    {
        switch (target)
        {
            case TARGET_UNIT_ENEMY_NEAR_CASTER:
            case TARGET_UNIT_FRIEND_NEAR_CASTER:
            case TARGET_UNIT_NEAR_CASTER:
            case TARGET_UNIT_CASTER_MASTER:
            case TARGET_UNIT_CASTER: break; // never check anything
            default: return true;
        }
    }

    return false;
}

/// 获取需要在 CheckCast 阶段校验目标合法性的效果掩码
inline uint32 GetCheckCastEffectMask(SpellEntry const* spellInfo)
{
    uint32 resultingMask = 0;
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (IsCheckCastTarget(spellInfo->EffectImplicitTargetA[i]) || IsCheckCastTarget(spellInfo->EffectImplicitTargetB[i]))
            resultingMask |= (1 << i);
    return resultingMask;
}

/// 判断法术是否包含指定的隐式目标 A 类型
inline bool HasSpellTarget(SpellEntry const* spellInfo, uint32 target)
{
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (spellInfo->EffectImplicitTargetA[i] == target)
            return true;

    return false;
}

/// 获取仅以施法者自身为目标 A 的效果掩码
inline uint32 GetCheckCastSelfEffectMask(SpellEntry const* spellInfo)
{
    uint32 resultingMask = 0;
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (spellInfo->EffectImplicitTargetA[i] == TARGET_UNIT_CASTER)
            resultingMask |= (1 << i);
    return resultingMask;
}

/// 判断目标类型是否需要客户端/服务端显式或隐式提供单位/方向目标
inline bool IsUnitTargetTarget(uint32 target)
{
    switch (target)
    {
        case TARGET_UNIT_ENEMY:
        case TARGET_UNIT:
        case TARGET_UNIT_FRIEND:
        case TARGET_UNIT_FRIEND_CHAIN_HEAL:
        case TARGET_UNIT_PARTY:
        case TARGET_UNIT_RAID:
        case TARGET_UNIT_FRIEND_AND_PARTY:
        case TARGET_LOCATION_CASTER_TARGET_POSITION:
        case TARGET_LOCATION_UNIT_FRONT:
        case TARGET_LOCATION_UNIT_BACK:
        case TARGET_LOCATION_UNIT_RIGHT:
        case TARGET_LOCATION_UNIT_LEFT:
        case TARGET_LOCATION_UNIT_FRONT_RIGHT:
        case TARGET_LOCATION_UNIT_BACK_RIGHT:
        case TARGET_LOCATION_UNIT_BACK_LEFT:
        case TARGET_LOCATION_UNIT_FRONT_LEFT:
            return true;
        default: return false;
    }
}

/// 判断法术是否存在客户端未检查到的缺失目标（客户端仅检查 Effect 0 Target A）
inline bool HasMissingTargetFromClient(SpellEntry const* spellInfo)
{
    // client only checks effect 0 target A (客户端仅校验第 0 号效果的 Target A)
    if (IsUnitTargetTarget(spellInfo->EffectImplicitTargetA[EFFECT_INDEX_0]))
        return false;

    if (IsUnitTargetTarget(spellInfo->EffectImplicitTargetA[EFFECT_INDEX_1]) || IsUnitTargetTarget(spellInfo->EffectImplicitTargetA[EFFECT_INDEX_2]))
        return true;

    if (IsUnitTargetTarget(spellInfo->EffectImplicitTargetB[EFFECT_INDEX_0]) || IsUnitTargetTarget(spellInfo->EffectImplicitTargetB[EFFECT_INDEX_1]) || IsUnitTargetTarget(spellInfo->EffectImplicitTargetB[EFFECT_INDEX_2]))
        return true;

    return false;
}

/// 判断法术是否需要目标对象
inline bool IsSpellRequireTarget(SpellEntry const* spellInfo)
{
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (IsUnitTargetTarget(spellInfo->EffectImplicitTargetA[i]) || IsUnitTargetTarget(spellInfo->EffectImplicitTargetB[i]))
            return true;

    return false;
}

/// 判断目标过滤类型是否为脚本目标
inline bool IsScriptTarget(uint32 target)
{
    return (target < MAX_SPELL_TARGETS ? (SpellTargetInfoTable[target].filter == TARGET_SCRIPT) : false);
}

/// 判断目标过滤类型是否为中立目标
inline bool IsNeutralTarget(uint32 target)
{
    return (target < MAX_SPELL_TARGETS ? (SpellTargetInfoTable[target].filter == TARGET_NEUTRAL) : false);
}

/// 判断目标过滤类型是否为友方目标
inline bool IsFriendlyTarget(uint32 target)
{
    return (target < MAX_SPELL_TARGETS ? (SpellTargetInfoTable[target].filter == TARGET_HELPFUL) : false);
}

/// 判断目标过滤类型是否为敌方目标
inline bool IsHostileTarget(uint32 target)
{
    return (target < MAX_SPELL_TARGETS ? (SpellTargetInfoTable[target].filter == TARGET_HARMFUL) : false);
}

/// 判断效果目标 A/B 是否包含脚本目标
inline bool IsEffectTargetScript(uint32 targetA, uint32 targetB)
{
    return (IsScriptTarget(targetA) || IsScriptTarget(targetB));
}

/// 判断效果目标 A/B 是否均为中立目标
inline bool IsEffectTargetNeutral(uint32 targetA, uint32 targetB)
{
    return (IsNeutralTarget(targetA) && IsNeutralTarget(targetB));
}

/// 判断效果目标 A/B 是否包含友方正面目标
inline bool IsEffectTargetPositive(uint32 targetA, uint32 targetB)
{
    return (IsFriendlyTarget(targetA) || IsFriendlyTarget(targetB));
}

/// 判断效果目标 A/B 是否包含敌方负面目标
inline bool IsEffectTargetNegative(uint32 targetA, uint32 targetB)
{
    return (IsHostileTarget(targetA) || IsHostileTarget(targetB));
}

/// 在中立目标模式下，根据施法者与目标的敌对关系判定该效果是否视为正面效果
inline bool IsNeutralEffectTargetPositive(uint32 etarget, const WorldObject* caster = nullptr, const WorldObject* target = nullptr)
{
    if (etarget < MAX_SPELL_TARGETS && SpellTargetInfoTable[etarget].type != TARGET_TYPE_UNIT)
        return true; // Some gameobjects or coords, who cares

    if (!target || (target->GetTypeId() != TYPEID_PLAYER && target->GetTypeId() != TYPEID_UNIT))
        return true;

    if (caster == target)
        return true; // Early self-cast detection

    if (!caster)
        return true;

    return !caster->CanAttackSpell(static_cast<const Unit*>(target));
}

/// 基于目标模式判定法术指定效果是否为正面效果（含触发子法术一层递归检测）
inline bool IsPositiveEffectTargetMode(const SpellEntry* entry, SpellEffectIndex effIndex, const WorldObject* caster = nullptr, const WorldObject* target = nullptr, bool recursive = false)
{
    if (!entry)
        return false;

    // Triggered spells case: prefer child spell via IsPositiveSpell()-like scan for triggered spell
    if (IsSpellEffectTriggerSpell(entry, effIndex))
    {
        const uint32 spellid = entry->EffectTriggerSpell[effIndex];
        // Its possible to go infinite cycle with triggered spells. We are interested to peek only at the first layer so far
        if (!recursive && spellid && (spellid != entry->Id))
        {
            if (const SpellEntry* triggered = sSpellTemplate.LookupEntry<SpellEntry>(spellid))
            {
                for (uint32 i = EFFECT_INDEX_0; i < MAX_EFFECT_INDEX; ++i)
                {
                    if (!IsPositiveEffectTargetMode(triggered, SpellEffectIndex(i), caster, target, true))
                        return false;
                }
            }
        }
        // For trigger effects target modes are inconsistent: we have invalid and coflicting ones
        // Let's try to ignore them completely
        return true;
    }

    const uint32 a = entry->EffectImplicitTargetA[effIndex];
    const uint32 b = entry->EffectImplicitTargetB[effIndex];

    if ((!a && !b) || IsEffectTargetPositive(a, b) || IsEffectTargetScript(a, b))
        return true;
    if (IsEffectTargetNegative(a, b))
    {
        // Workaround: Passive talents with negative target modes are getting removed by ice block and similar effects (临时处理：带负面目标模式的被动天赋可能被冰箱等效果错误移除)
        // TODO: Fix removal of passives in appropriate places and remove the check below afterwards
        return entry->HasAttribute(SPELL_ATTR_PASSIVE);
    }
    if (IsEffectTargetNeutral(a, b))
        return (IsPointEffectTarget(SpellTarget(b ? b : a)) || IsNeutralEffectTargetPositive((b ? b : a), caster, target));

    // If we ever get to this point, we have unhandled target. Gotta say something about it.
    if (entry->Effect[effIndex])
        DETAIL_LOG("IsPositiveEffectTargetMode: Spell %u's effect %u has unhandled targets (A:%u B:%u)", entry->Id, effIndex,
                   entry->EffectImplicitTargetA[effIndex], entry->EffectImplicitTargetB[effIndex]);
    return true;
}

/// 综合法术 ID 特例、效果类型与目标模式判定法术指定效果是否为正面效果
inline bool IsPositiveEffect(const SpellEntry* spellproto, SpellEffectIndex effIndex, const WorldObject* caster = nullptr, const WorldObject* target = nullptr)
{
    if (!spellproto)
        return false;

    switch (spellproto->Id) // Spells whose effects are always positive
    {
        case 24742: // Magic Wings
        case 29880: // Mana Shield - Arcane Anomaly 16488
        case 42867:
        case 34786: // Temporal Analysis - factions and unitflags of target/caster verified, should not incur combat
        case 39384: // Fury Of Medivh visual - Burning Flames - Fury of medivh is friendly to all, and it hits all chess pieces, basically friendly fire damage
        case 37277: // Summon Infernal - neutral spell with TARGET_UNIT which evaluates as hostile due to neutral factions, with delay and gets removed by !IsPositiveSpell check
        case 42399: // Neutral spell with TARGET_UNIT, caster faction 14, target faction 14, evaluates as negative spell
        case 39995: // Four Dragons: Dummy to Dragon - Dummy effect need to trigger even if target is immune
                    // because of POS/NEG decision, should in fact be NEUTRAL decision TODO: Increase check fidelity
        case 33637: // Infernal spells - Neutral targets - in sniff never put into combat - Maybe neutral spells do not put into combat?
        case 33241:
        case 33045: // Wrath of the Astromancer - stacks like positive
        case 35424: // Soul Shadows - used by Shade of Mal'druk on Mal'druk the Soulrender
        case 42628: // Zul'Aman - Fire Bomb - Neutral spell
        case 44406: // Energy Infusion - supposed to be buff despite negative targeting
            return true;
        case 43101: // Headless Horseman Climax - Command, Head Requests Body - must be negative so that SPELL_ATTR_NO_IMMUNITIES isn't ignored, Headless Horseman script target is immune
        case 34190: // Arcane Orb - should be negative
                    /*34172 is cast onto friendly target, and fails bcs its delayed and we remove negative delayed on friendlies due to Duel code, if we change target pos code
                    bcs 34190 will be evaled as neg, 34172 will be evaled as neg, and hence be removed cos its negative delayed on a friendly*/
        case 34700: // Allergic Reaction - Neutral target type - needs to be a debuff
        case 36717: // Neutral spells with SPELL_ATTR_EX3_TARGET_ONLY_PLAYER as a filter
        case 38829:
        case 40055: // Introspection - Apexis Relic
        case 40165:
        case 40166:
        case 40167:
        case 36812: // Soaring
        case 37910: // overriding because immunity is preventing the spellcast - maybe neutral spells should be judged like negatives?
        case 37940:
        case 37962:
        case 37968:
        case 35734: // Dropping Phase Disruptor
            return false;
    }

    switch (spellproto->Effect[effIndex])
    {
        case SPELL_EFFECT_SEND_TAXI:                // Some NPCs that send taxis are neutral, so target mode fails
        case SPELL_EFFECT_QUEST_COMPLETE:           // TODO: Spells with these effects should be casted by a proper caster to meet target mode.
        case SPELL_EFFECT_KILL_CREDIT_GROUP:        // Until then switch makes them all positive
        case SPELL_EFFECT_TELEPORT_UNITS:
        case SPELL_EFFECT_CREATE_ITEM:
        case SPELL_EFFECT_SUMMON_CHANGE_ITEM:
            return true;
        case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:    // Always hostile effects
            return false;
        case SPELL_EFFECT_DUMMY:
            // some explicitly required dummy effect sets
            switch (spellproto->Id)
            {
                case 18153:                                 // Kodo Kombobulator
                    return true;
                default:
                    break;
            }
            break;
        case SPELL_EFFECT_SCRIPT_EFFECT:
            // some explicitly required script effect sets
            switch (spellproto->Id)
            {
                case 42436:                                 // Drink!
                    return true;
                default:
                    break;
            }
            break;
        case SPELL_EFFECT_SCHOOL_DAMAGE:
        {
            //switch (spellproto->Id)
            //{
            //    default:
            //        break;
            //}
            break;
        }
        // Aura exceptions:
        case SPELL_EFFECT_APPLY_AURA:
        case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
        {
            switch (spellproto->EffectApplyAuraName[effIndex])
            {
                case SPELL_AURA_DUMMY:
                {
                    // dummy aura can be positive or negative dependent from casted spell
                    switch (spellproto->Id)
                    {
                        case 13139:                         // net-o-matic special effect
                        case 44877:                         // Living Flare Master
                            return false;
                        case 39834:                         // Vimgol population tester
                        case 39851:                         // Vimgol population tester
                        case 39852:                         // Vimgol population tester
                        case 39853:                         // Vimgol population tester
                        case 39854:                         // Vimgol population tester
                        case 39921:                         // Vimgol Pentagram Beam
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                case SPELL_AURA_MOD_MELEE_HASTE:
                {
                    switch (spellproto->Id)
                    {
                        case 38449:                         // Blessing of the Tides
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case SPELL_EFFECT_PERSISTENT_AREA_AURA:
        {
            switch (spellproto->EffectApplyAuraName[effIndex])
            {
                case SPELL_AURA_PERIODIC_DAMAGE: // possible TODO: make all return false
                {
                    switch (spellproto->Id)
                    {
                        case 37465: // chess rain of fire and poison cloud spells
                        case 37469:
                        case 37775:
                            return false;
                    }
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }

    // Generic effect check: negative on negative targets, positive on positive targets (通用检查：根据目标模式判定正面或负面)
    return IsPositiveEffectTargetMode(spellproto, effIndex, caster, target);
}

/// 判定光环效果本身是否为正面增益（依据 Buff/Debuff 属性及目标类型）
inline bool IsPositiveAuraEffect(const SpellEntry* entry, SpellEffectIndex effIndex, const WorldObject* /*caster*/ = nullptr, const WorldObject* /*target*/ = nullptr)
{
    if (IsAuraApplyEffect(entry, effIndex))
    {
        if (entry->HasAttribute(SPELL_ATTR_EX4_AURA_IS_BUFF))
            return true;

        if (entry->HasAttribute(SPELL_ATTR_AURA_IS_DEBUFF))
            return false;

        return !IsEffectTargetNegative(entry->EffectImplicitTargetA[effIndex], entry->EffectImplicitTargetB[effIndex]) && entry->Effect[effIndex] != SPELL_EFFECT_APPLY_AREA_AURA_ENEMY;
    }

    return false;
}

/// 针对特定施法者与目标，按效果掩码判定法术目标模式是否为正面
inline bool IsPositiveSpellTargetModeForSpecificTarget(const SpellEntry* entry, uint8 effectMask, const WorldObject* caster = nullptr, const WorldObject* target = nullptr)
{
    if (!entry)
        return false;
    // spells with at least one negative effect are considered negative
    // some self-applied spells have negative effects but in self casting case negative check ignored.
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (effectMask & (1 << i))
            if (entry->Effect[i] && !IsPositiveEffectTargetMode(entry, SpellEffectIndex(i), caster, target))
                return false;
    return true;
}

/// 按法术 ID 与效果掩码判定针对特定目标是否为正面法术
inline bool IsPositiveSpellTargetModeForSpecificTarget(uint32 spellId, uint8 effectMask, const WorldObject* caster, const WorldObject* target)
{
    if (!spellId)
        return false;
    return IsPositiveSpellTargetModeForSpecificTarget(sSpellTemplate.LookupEntry<SpellEntry>(spellId), effectMask, caster, target);
}

/// 基于全部非空效果的目标模式判定法术是否为正面法术
inline bool IsPositiveSpellTargetMode(const SpellEntry* entry, const WorldObject* caster = nullptr, const WorldObject* target = nullptr)
{
    if (!entry)
        return false;
    // spells with at least one negative effect are considered negative
    // some self-applied spells have negative effects but in self casting case negative check ignored.
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (entry->Effect[i] && !IsPositiveEffectTargetMode(entry, SpellEffectIndex(i), caster, target))
            return false;
    return true;
}

/// 按法术 ID 基于目标模式判定是否为正面法术
inline bool IsPositiveSpellTargetMode(uint32 spellId, const WorldObject* caster, const WorldObject* target)
{
    if (!spellId)
        return false;
    return IsPositiveSpellTargetMode(sSpellTemplate.LookupEntry<SpellEntry>(spellId), caster, target);
}

/// 判断法术整体是否为正面法术（全部非空效果均为正面才返回 true）
inline bool IsPositiveSpell(const SpellEntry* entry, const WorldObject* caster = nullptr, const WorldObject* target = nullptr)
{
    if (!entry)
        return false;
    // spells with at least one negative effect are considered negative
    // some self-applied spells have negative effects but in self casting case negative check ignored.
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (entry->Effect[i] && !IsPositiveEffect(entry, SpellEffectIndex(i), caster, target))
            return false;
    return true;
}

// this is propably the correct check for most positivity/negativity decisions
/// 判断法术指定效果掩码内是否全为正面效果
inline bool IsPositiveEffectMask(const SpellEntry* entry, uint8 effectMask, const WorldObject* caster = nullptr, const WorldObject* target = nullptr)
{
    if (!entry)
        return false;
    // spells with at least one negative effect are considered negative
    // some self-applied spells have negative effects but in self casting case negative check ignored.
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (entry->Effect[i] && (effectMask & (1 << i)) && !IsPositiveEffect(entry, SpellEffectIndex(i), caster, target))
            return false;
    return true;
}

/// 按法术 ID 判断法术整体是否为正面法术
inline bool IsPositiveSpell(uint32 spellId, const WorldObject* caster = nullptr, const WorldObject* target = nullptr)
{
    if (!spellId)
        return false;
    return IsPositiveSpell(sSpellTemplate.LookupEntry<SpellEntry>(spellId), caster, target);
}

/// 判断施放法术是否能穿透免疫光环（如无视免疫或特定机制免疫穿透）
inline bool CanPierceImmuneAura(SpellEntry const* spellInfo, SpellEntry const* auraSpellInfo, uint8 effectMask = 0, SpellEffectIndex effIdx = EFFECT_INDEX_0)
{
    // aura can't be pierced (目标光环本身不可被穿透)
    if (auraSpellInfo && auraSpellInfo->HasAttribute(SPELL_ATTR_NO_IMMUNITIES))
        return false;

    // these spells pierce all available spells (Resurrection Sickness for example) (此法术可穿透所有光环，例如复活虚弱)
    if (spellInfo->HasAttribute(SPELL_ATTR_NO_IMMUNITIES))
        return true;

    if (!auraSpellInfo)
        return false;

    // these spells (Cyclone for example) can pierce all... (此类法术如飓风术可穿透多数免疫...)
    if (spellInfo->HasAttribute(SPELL_ATTR_EX_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS) || spellInfo->HasAttribute(SPELL_ATTR_EX2_NO_SCHOOL_IMMUNITIES))
    {
        // ...but not these (Divine shield, Ice block, Cyclone and Banish for example) (...但不能穿透圣盾术、寒冰屏障、飓风与放逐等机制)
        if (auraSpellInfo->Mechanic != MECHANIC_IMMUNE_SHIELD &&
            auraSpellInfo->Mechanic != MECHANIC_INVULNERABILITY &&
            auraSpellInfo->Mechanic != MECHANIC_BANISH)
            return true;
    }

    if (spellInfo->HasAttribute(SPELL_ATTR_EX_IMMUNITY_PURGES_EFFECT))
    {
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            uint32 miscValue = (uint32)spellInfo->EffectMiscValue[i];
            if (spellInfo->Effect[i] && spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MECHANIC_IMMUNITY_MASK)
                if (effectMask & (1 << i))
                    if (miscValue & (1 << auraSpellInfo->Mechanic) || miscValue & (1 << auraSpellInfo->EffectMechanic[effIdx]))
                        return true;
        }
    }

    // TODO: Add SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY logic

    return false;
}

/// 计算链式法术（如闪电链、治疗链）的跳跃搜索距离
inline void GetChainJumpRange(SpellEntry const* spellInfo, SpellEffectIndex effIdx, float& minSearchRangeCaster, float& maxSearchRangeTarget)
{ 
    const SpellRangeEntry* range = sSpellRangeStore.LookupEntry(spellInfo->rangeIndex);
    if (spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE)
        maxSearchRangeTarget = range->maxRange;
    else
        // FIXME: This very like horrible hack and wrong for most spells
        maxSearchRangeTarget = spellInfo->EffectChainTarget[effIdx] * CHAIN_SPELL_JUMP_RADIUS;

    if (range->ID == 114)   // Hunter Search range
        minSearchRangeCaster = 5;

    switch (spellInfo->Id)
    {
        case 32445: // Holy Wrath - Maiden of Virtue
            maxSearchRangeTarget = 100.f;
            break;
        case 40827: // Beams - Mother Shahraz
        case 40859:
        case 40860:
        case 40861:
            minSearchRangeCaster = 0.f;
            maxSearchRangeTarget = 150.f;
            break;
        case 44537: // Fel Lightning - MgT / SWP
        case 46048:
        case 46480:
            maxSearchRangeTarget = 50.f;
            break;
        default:   // default jump radius
            break;
    }
}

/// 判断法术是否包含驱散效果
inline bool IsDispelSpell(SpellEntry const* spellInfo)
{
    return IsSpellHaveEffect(spellInfo, SPELL_EFFECT_DISPEL);
}

/// 判断施放该法术是否会打破潜行状态
inline bool isSpellBreakStealth(SpellEntry const* spellInfo)
{
    return !spellInfo->HasAttribute(SPELL_ATTR_EX_ALLOW_WHILE_STEALTHED);
}

/// 判断法术是否为自动重复远程攻击（如自动射击、魔杖射击）
inline bool IsAutoRepeatRangedSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_USES_RANGED_SLOT) && spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTO_REPEAT);
}

/// 判断法术是否依赖远程攻击强度（猎人非近战技能）
inline bool IsSpellRequiresRangedAP(SpellEntry const* spellInfo)
{
    return (spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER && spellInfo->DmgClass != SPELL_DAMAGE_CLASS_MELEE);
}

/// 检查在当前变形姿态下施放法术是否允许，返回对应错误码
SpellCastResult GetErrorAtShapeshiftedCast(SpellEntry const* spellInfo, uint32 form);

/// 判断法术是否为引导法术
inline bool IsChanneledSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX_IS_CHANNELED) || spellInfo->HasAttribute(SPELL_ATTR_EX_IS_SELF_CHANNELED);
}

/// 判断进入指定变形姿态时是否需要自动施放该被动/隐藏法术
inline bool IsNeedCastSpellAtFormApply(SpellEntry const* spellInfo, ShapeshiftForm form)
{
    if ((!spellInfo->HasAttribute(SPELL_ATTR_PASSIVE) && !spellInfo->HasAttribute(SPELL_ATTR_DO_NOT_DISPLAY)) || !form)
        return false;

    // passive spells with SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT are already active without shapeshift, do no recast!
    return (spellInfo->CanBeUsedInForm(form) && !spellInfo->HasAttribute(SPELL_ATTR_EX2_ALLOW_WHILE_NOT_SHAPESHIFTED));
}

/// 判断法术是否需要在户外环境自动触发被动施放
inline bool IsNeedCastSpellAtOutdoor(SpellEntry const* spellInfo)
{
    return (spellInfo->HasAttribute(SPELL_ATTR_ONLY_OUTDOORS) && spellInfo->HasAttribute(SPELL_ATTR_PASSIVE));
}

/// 判断法术是否可被法术反射（魔法类、非能力、非被动且为负面）
inline bool IsReflectableSpell(SpellEntry const* spellInfo)
{
    return spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MAGIC && !spellInfo->HasAttribute(SPELL_ATTR_IS_ABILITY)
           && !spellInfo->HasAttribute(SPELL_ATTR_EX_NO_REFLECTION) && !spellInfo->HasAttribute(SPELL_ATTR_NO_IMMUNITIES)
           && !spellInfo->HasAttribute(SPELL_ATTR_PASSIVE) && !IsPositiveSpell(spellInfo);
}

// Mostly required by spells that target a creature inside GO
/// 判断法术是否整体忽略视线（LoS）检测
inline bool IsIgnoreLosSpell(SpellEntry const* spellInfo)
{
    if (spellInfo->HasAttribute(SPELL_ATTR_EX5_ALWAYS_LINE_OF_SIGHT))
        return false;

    switch (spellInfo->Id)
    {
        case 8733:                                  // Blessing of Blackfathom - potentially hints at trap GO casts not respecting los
        case 8735:                                  // Blackfathom Teleport - same
        case 24742:                                 // Magic Wings
        case 31628:                                 // Green Beam
        case 31630:                                 // Green Beam
        case 31631:                                 // Green Beam
        case 36795:                                 // Cannon Channel
        case 40639:                                 // Arcane Beam - Channel target inside gameobject
        case 42867:                                 // both need LOS, likely TARGET_UNIT should use LOS ignore from normal radius, not per-effect radius WIP
            return true;
        default:
            break;
    }

    return spellInfo->HasAttribute(SPELL_ATTR_EX2_IGNORE_LINE_OF_SIGHT);
}

/// 判断法术施放检查阶段是否忽略视线（LoS）检测
inline bool IsIgnoreLosSpellCast(SpellEntry const* spellInfo)
{
    return spellInfo->rangeIndex == 13 || IsIgnoreLosSpell(spellInfo);
}

/// 判断法术指定效果在目标筛选时是否忽略视线（LoS）检测
inline bool IsIgnoreLosSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex effIdx, bool targetB)
{
    if (spellInfo->HasAttribute(SPELL_ATTR_EX5_ALWAYS_LINE_OF_SIGHT))
        return false;

    // TODO: Move this to target logic
    switch (spellInfo->EffectImplicitTargetA[effIdx])
    {
        case TARGET_UNIT_FRIEND_CHAIN_HEAL: // checked for LOS but in a custom chain way
        case TARGET_UNIT_FRIEND_AND_PARTY:
        case TARGET_UNIT_RAID_AND_CLASS:
        case TARGET_ENUM_UNITS_PARTY_WITHIN_CASTER_RANGE: return true;
        default:
            if (IsCheckCastTarget(targetB ? spellInfo->EffectImplicitTargetB[effIdx] : spellInfo->EffectImplicitTargetA[effIdx]))
                return IsIgnoreLosSpellCast(spellInfo);
            break;
    }

    return spellInfo->EffectRadiusIndex[effIdx] == 28 || IsIgnoreLosSpell(spellInfo);
}

// applied when item is received/looted/equipped
/// 判断法术是否为物品获得/拾取/装备时自动施加的光环
inline bool IsItemAura(SpellEntry const* spellInfo)
{
    switch (spellInfo->Id)
    {
        case 38132:
            return true;
        default:
            return false;
    }
}

/// 判断法术是否需要并消耗连击点数（终结技）
inline bool NeedsComboPoints(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX_FINISHING_MOVE_DAMAGE) || spellInfo->HasAttribute(SPELL_ATTR_EX_FINISHING_MOVE_DURATION);
}

/// 获取法术的法术学派掩码
inline SpellSchoolMask GetSpellSchoolMask(SpellEntry const* spellInfo)
{
    return SpellSchoolMask(spellInfo->SchoolMask);
}

/// 获取法术整体及指定效果掩码内所有机制（Mechanic）的位掩码
inline uint32 GetSpellMechanicMask(SpellEntry const* spellInfo, uint32 effectMask)
{
    uint32 mask = 0;
    if (spellInfo->Mechanic)
        mask |= convertEnumToFlag(spellInfo->Mechanic);

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!(effectMask & (1 << i)))
            continue;

        if (spellInfo->EffectMechanic[i])
            mask |= convertEnumToFlag(spellInfo->EffectMechanic[i]);
    }

    return mask;
}

/// 获取法术所有效果及全局机制的完整位掩码
inline uint32 GetAllSpellMechanicMask(SpellEntry const* spellInfo)
{
    uint32 mask = 0;
    if (spellInfo->Mechanic)
        mask |= convertEnumToFlag(spellInfo->Mechanic);
    for (unsigned int i : spellInfo->EffectMechanic)
        if (i)
            mask |= convertEnumToFlag(i);
    return mask;
}

/// 获取法术指定效果的机制类型（优先取效果自身机制，否则回退到法术全局机制）
inline Mechanics GetEffectMechanic(SpellEntry const* spellInfo, SpellEffectIndex effect)
{
    if (spellInfo->EffectMechanic[effect])
        return Mechanics(spellInfo->EffectMechanic[effect]);
    if (spellInfo->Mechanic)
        return Mechanics(spellInfo->Mechanic);
    return MECHANIC_NONE;
}

/// 判断法术是否包含定身免疫且清除定身效果的属性
inline bool IsIgnoreRootSpell(SpellEntry const* spellInfo)
{
    if (!spellInfo->HasAttribute(SPELL_ATTR_EX_IMMUNITY_PURGES_EFFECT))
        return false;

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA && spellInfo->EffectImplicitTargetA[i] == TARGET_UNIT_CASTER &&
            spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MECHANIC_IMMUNITY && spellInfo->EffectMiscValue[i] == MECHANIC_ROOT)
            return true;

    return false;
}

/// 判断法术是否使用武器技能进行命中/熟练度计算
inline bool IsSpellUseWeaponSkill(SpellEntry const* spellInfo)
{
    // note: this is not a mirror of client function - that function does not work for Bloodthirst edgecase
    return spellInfo->EquippedItemClass == ITEM_CLASS_WEAPON || spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE || spellInfo->DmgClass == SPELL_DAMAGE_CLASS_RANGED && spellInfo->HasAttribute(SPELL_ATTR_USES_RANGED_SLOT);
}

/// 将驱散类型转换为驱散位掩码
inline uint32 GetDispellMask(DispelType dispel)
{
    // If dispell all (若为全部驱散类型，返回全驱散掩码)
    if (dispel == DISPEL_ALL)
        return DISPEL_ALL_MASK;
    return (1 << dispel);
}

/// 判断指定法术 ID 是否施加了指定的光环类型
inline bool IsAuraAddedBySpell(uint32 auraType, uint32 spellId)
{
    SpellEntry const* spellproto = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellproto) return false;

    for (int i = 0; i < 3; i++)
        if (spellproto->EffectApplyAuraName[i] == auraType)
            return true;
    return false;
}

/// 判断目标类型是否为小队或团队范围目标
inline bool IsPartyOrRaidTarget(uint32 target)
{
    switch (target)
    {
        case TARGET_UNIT_FRIEND_NEAR_CASTER:
        case TARGET_ENUM_UNITS_PARTY_WITHIN_CASTER_RANGE:
        case TARGET_ENUM_UNITS_PARTY_AOE_AT_SRC_LOC:
        case TARGET_ENUM_UNITS_PARTY_AOE_AT_DEST_LOC:
        case TARGET_UNIT_PARTY:
        case TARGET_UNIT_FRIEND_AND_PARTY:
        case TARGET_ENUM_UNITS_RAID_WITHIN_CASTER_RANGE:
        case TARGET_UNIT_RAID:
        case TARGET_UNIT_RAID_NEAR_CASTER:
        case TARGET_UNIT_RAID_AND_CLASS:
            return true;
        default:
            return false;
    }
}

/// 判断增益法术是否受限于同一小队/团队（如灵魂石复活）
inline bool IsGroupRestrictedBuff(SpellEntry const* spellInfo)
{
    switch (spellInfo->Id)
    {
        // Soulstone Ressurection - Patch 2.1.0 (灵魂石复活 - 2.1.0 版本后限定小队/团队)
        case 20707:
        case 20762:
        case 20763:
        case 20764:
        case 20765:
        case 27239:
            return true;
    }

    return false;
}

/// 判断两个法术的指定效果是否为同类型且同正负性的相似光环
inline bool IsSimilarAuraEffect(SpellEntry const* entry, uint32 effect, SpellEntry const* entry2, uint32 effect2)
{
    return (entry2->EffectApplyAuraName[effect2] && entry->EffectApplyAuraName[effect] &&
            entry2->Effect[effect2] == entry->Effect[effect] &&
            entry2->EffectApplyAuraName[effect2] == entry->EffectApplyAuraName[effect] &&
            IsPositiveEffect(entry2, SpellEffectIndex(effect2)) == IsPositiveEffect(entry, SpellEffectIndex(effect)));
}

/// 判断已存在的光环持有者（SpellAuraHolder）是否比新光环持有者更强
inline bool IsSimilarExistingAuraStronger(const SpellAuraHolder* holder, const SpellAuraHolder* existing)
{
    if (!holder || !existing)
        return false;
    const SpellEntry* entry = holder->GetSpellProto();
    const SpellEntry* entry2 = existing->GetSpellProto();
    if (!entry || !entry2)
        return false;

    // Already compared effects masks to avoid re-entrance (记录已比对的效果掩码，防止重复匹配)
    uint32 effectmask1 = 0;
    uint32 effectmask2 = 0;

    for (uint32 e = EFFECT_INDEX_0; e < MAX_EFFECT_INDEX; ++e)
    {
        for (uint32 e2 = EFFECT_INDEX_0; e2 < MAX_EFFECT_INDEX; ++e2)
        {
            if (IsSimilarAuraEffect(entry, e, entry2, e2) && !(effectmask1 & (1 << e)) && !(effectmask2 & (1 << e2)))
            {
                effectmask1 |= (1 << e);
                effectmask2 |= (1 << e2);
                Aura* aura1 = holder->GetAuraByEffectIndex(SpellEffectIndex(e));
                Aura* aura2 = existing->GetAuraByEffectIndex(SpellEffectIndex(e2));
                int32 value = aura1 ? (aura1->GetModifier()->m_amount / int32(aura1->GetStackAmount())) : 0;
                int32 value2 = aura2 ? (aura2->GetModifier()->m_amount / int32(aura2->GetStackAmount())) : 0;
                if (value < 0 && value2 < 0)
                {
                    value = abs(value);
                    value2 = abs(value2);
                }
                if (value2 > value)
                    return true;
            }
        }
    }
    return false;
}

/// 使用预计算效果数值判断已存在的光环持有者是否比即将施加的新光环更强
inline bool IsSimilarExistingAuraStronger(const Unit* caster, const SpellEntry* entry, const SpellAuraHolder* existing, uint32 affectedMask, int32 amounts[MAX_EFFECT_INDEX])
{
    if (!caster || !existing)
        return false;
    const SpellEntry* entry2 = existing->GetSpellProto();
    if (!entry || !entry2)
        return false;

    // Already compared effects masks to avoid re-entrance (记录已比对的效果掩码，防止重复匹配)
    uint32 effectmask1 = 0;
    uint32 effectmask2 = 0;

    for (uint32 e = EFFECT_INDEX_0; e < MAX_EFFECT_INDEX; ++e)
    {
        if (affectedMask & (1 << e))
        {
            for (uint32 e2 = EFFECT_INDEX_0; e2 < MAX_EFFECT_INDEX; ++e2)
            {
                if (IsSimilarAuraEffect(entry, e, entry2, e2) && !(effectmask1 & (1 << e)) && !(effectmask2 & (1 << e2)))
                {
                    effectmask1 |= (1 << e);
                    effectmask2 |= (1 << e2);
                    Aura* aura = existing->GetAuraByEffectIndex(SpellEffectIndex(e2));
                    int32 value = amounts[e];
                    int32 value2 = aura ? (aura->GetModifier()->m_amount / int32(aura->GetStackAmount())) : 0;
                    if (value < 0 && value2 < 0)
                    {
                        value = abs(value);
                        value2 = abs(value2);
                    }
                    if (value2 > value)
                        return true;
                }
            }
        }
    }
    return false;
}

// Diminishing Returns interaction with spells
/// 获取法术对应的控制递减（Diminishing Returns）组别
DiminishingGroup GetDiminishingReturnsGroupForSpell(SpellEntry const* spellproto, bool triggered);
/// 判断指定递减组别是否有最大持续时间上限
bool IsDiminishingReturnsGroupDurationLimited(DiminishingGroup group);
/// 判断指定递减组别在 PvP 或 PvE 下是否受持续时间递减影响
bool IsDiminishingReturnsGroupDurationDiminished(DiminishingGroup group, bool pvp);
/// 获取指定递减组别的递减计算类型
DiminishingReturnsType GetDiminishingReturnsGroupType(DiminishingGroup group);
/// 判断指定递减组别是否受递减等级追踪
bool IsSubjectToDiminishingLevels(DiminishingGroup group, bool pvp);
/// 判断法术是否为生物触发的控制递减法术
bool IsCreatureDRSpell(SpellEntry const* spellInfo);

// Spell affects related declarations (accessed using SpellMgr functions)
typedef std::map<uint32, uint64> SpellAffectMap;

// Spell proc event related declarations (accessed using SpellMgr functions)
/// 法术触发事件标志位枚举（定义何种战斗/施法/状态行为可触发被动或光环事件）
enum ProcFlags : uint32
{
    PROC_FLAG_NONE                          = 0x00000000,

    PROC_FLAG_HEARTBEAT                     = 0x00000001,   // 00 Heartbeat (心跳周期触发)
    PROC_FLAG_KILL                          = 0x00000002,   // 01 Kill target (in most cases need XP/Honor reward, see Unit::IsTriggeredAtSpellProcEvent for additinoal check) (击杀目标，通常需提供经验或荣誉)

    PROC_FLAG_DEAL_MELEE_SWING              = 0x00000004,   // 02 Successful melee auto attack (近战平砍命中)
    PROC_FLAG_TAKE_MELEE_SWING              = 0x00000008,   // 03 Taken damage from melee auto attack hit (受到近战平砍攻击)

    PROC_FLAG_DEAL_MELEE_ABILITY            = 0x00000010,   // 04 Successful attack by Spell that use melee weapon (近战武器技能命中)
    PROC_FLAG_TAKE_MELEE_ABILITY            = 0x00000020,   // 05 Taken damage by Spell that use melee weapon (受到近战武器技能攻击)

    PROC_FLAG_DEAL_RANGED_ATTACK            = 0x00000040,   // 06 Successful Ranged auto attack (远程自动攻击命中)
    PROC_FLAG_TAKE_RANGED_ATTACK            = 0x00000080,   // 07 Taken damage from ranged auto attack (受到远程自动攻击)

    PROC_FLAG_DEAL_RANGED_ABILITY           = 0x00000100,   // 08 Successful Ranged attack by Spell that use ranged weapon (远程武器技能命中)
    PROC_FLAG_TAKE_RANGED_ABILITY           = 0x00000200,   // 09 Taken damage by Spell that use ranged weapon (受到远程武器技能攻击)

    PROC_FLAG_DEAL_HELPFUL_ABILITY          = 0x00000400,   // 10 Done positive spell that has dmg class none (施放无伤害类型的有益技能)
    PROC_FLAG_TAKE_HELPFUL_ABILITY          = 0x00000800,   // 11 Taken positive spell that has dmg class none (受到无伤害类型的有益技能)

    PROC_FLAG_DEAL_HARMFUL_ABILITY          = 0x00001000,   // 12 Done negative spell that has dmg class none (施放无伤害类型的有害技能)
    PROC_FLAG_TAKE_HARMFUL_ABILITY          = 0x00002000,   // 13 Taken negative spell that has dmg class none (受到无伤害类型的有害技能)

    PROC_FLAG_DEAL_HELPFUL_SPELL            = 0x00004000,   // 14 Successful cast positive spell (by default only on healing) (施放有益法术，默认仅限治疗)
    PROC_FLAG_TAKE_HELPFUL_SPELL            = 0x00008000,   // 15 Taken positive spell hit (by default only on healing) (受到有益法术命中，默认仅限治疗)

    PROC_FLAG_DEAL_HARMFUL_SPELL            = 0x00010000,   // 16 Successful negative spell cast (by default only on damage) (施放有害法术，默认仅限伤害)
    PROC_FLAG_TAKE_HARMFUL_SPELL            = 0x00020000,   // 17 Taken negative spell (by default only on damage) (受到有害法术，默认仅限伤害)

    PROC_FLAG_DEAL_HARMFUL_PERIODIC         = 0x00040000,   // 18 Successful do periodic (damage / healing, determined by PROC_EX_PERIODIC_POSITIVE or negative if no procEx) (造成周期性伤害或治疗)
    PROC_FLAG_TAKE_HARMFUL_PERIODIC         = 0x00080000,   // 19 Taken spell periodic (damage / healing, determined by PROC_EX_PERIODIC_POSITIVE or negative if no procEx) (受到周期性伤害或治疗)

    PROC_FLAG_TAKE_ANY_DAMAGE               = 0x00100000,   // 20 Taken any damage (受到任意来源伤害)
    PROC_FLAG_ON_TRAP_ACTIVATION            = 0x00200000,   // 21 On trap activation - different from enumerated strings - likely reuse (陷阱激活时触发)

    PROC_FLAG_MAIN_HAND_WEAPON_SWING        = 0x00400000,   // 22 Successful main-hand melee attacks (主手近战攻击命中)
    PROC_FLAG_OFF_HAND_WEAPON_SWING         = 0x00800000,   // 23 Successful off-hand melee attacks (副手近战攻击命中)

    PROC_FLAG_DEATH                         = 0x01000000,   // 24 On death by any means (任意原因死亡时触发)
};

/// 近战与远程武器攻击触发基础掩码组合
#define MELEE_BASED_TRIGGER_MASK (PROC_FLAG_DEAL_MELEE_SWING        | \
                                  PROC_FLAG_TAKE_MELEE_SWING             | \
                                  PROC_FLAG_DEAL_MELEE_ABILITY  | \
                                  PROC_FLAG_TAKE_MELEE_ABILITY       | \
                                  PROC_FLAG_DEAL_RANGED_ATTACK       | \
                                  PROC_FLAG_TAKE_RANGED_ABILITY            | \
                                  PROC_FLAG_DEAL_RANGED_ABILITY | \
                                  PROC_FLAG_TAKE_RANGED_ABILITY)

/// 负面攻击与有害法术触发掩码组合
#define NEGATIVE_TRIGGER_MASK (MELEE_BASED_TRIGGER_MASK                | \
                               PROC_FLAG_DEAL_HARMFUL_ABILITY      | \
                               PROC_FLAG_TAKE_HARMFUL_ABILITY           | \
                               PROC_FLAG_DEAL_HARMFUL_SPELL | \
                               PROC_FLAG_TAKE_HARMFUL_SPELL)

/// 法术触发扩展条件标志位（细化命中结果：普通命中、暴击、未命中、格挡、招架等）
enum ProcFlagsEx
{
    PROC_EX_NONE                = 0x0000000,                // If none can tigger on Hit/Crit only (passive spells MUST defined by SpellFamily flag) (无扩展标志，仅普通命中或暴击触发)
    PROC_EX_NORMAL_HIT          = 0x0000001,                // If set only from normal hit (only damage spells) (仅普通命中触发)
    PROC_EX_CRITICAL_HIT        = 0x0000002,                // 暴击触发
    PROC_EX_MISS                = 0x0000004,                // 未命中触发
    PROC_EX_RESIST              = 0x0000008,                // 抵抗触发
    PROC_EX_DODGE               = 0x0000010,                // 躲闪触发
    PROC_EX_PARRY               = 0x0000020,                // 招架触发
    PROC_EX_BLOCK               = 0x0000040,                // 格挡触发
    PROC_EX_EVADE               = 0x0000080,                // 闪避/脱离触发
    PROC_EX_IMMUNE              = 0x0000100,                // 免疫触发
    PROC_EX_DEFLECT             = 0x0000200,                // 偏斜触发
    PROC_EX_ABSORB              = 0x0000400,                // 吸收触发
    PROC_EX_REFLECT             = 0x0000800,                // 反射触发
    PROC_EX_INTERRUPT           = 0x0001000,                // melee hit result can be Interrupt (not used) (打断触发，未使用)
    PROC_EX_RESERVED1           = 0x0002000,
    PROC_EX_RESERVED2           = 0x0004000,
    PROC_EX_RESERVED3           = 0x0008000,
    PROC_EX_EX_TRIGGER_ON_NO_DAMAGE = 0x0010000,            // if set, hits trigger even if no damage/healing is dealt (即使未造成实际伤害或治疗也触发)
    PROC_EX_EX_ONE_TIME_TRIGGER = 0x0020000,                // if set trigger always but only one time (not used)
    PROC_EX_PERIODIC_POSITIVE   = 0x0040000,                // for periodic heal (周期性治疗跳数触发)
    PROC_EX_CAST_END            = 0x0080000,                // procs on end of cast (施法结束时触发)
    PROC_EX_MAGNET              = 0x0100000,                // for grounding totem hit (根基图腾吸收命中触发)

    // Flags for internal use - do not use these in db!
    PROC_EX_INTERNAL_HOT        = 0x2000000                 // 内部标记：持续治疗 HoT
};

/// 数据库 spell_proc_event 表对应的法术触发事件配置条目
struct SpellProcEventEntry
{
    uint32      schoolMask;                                     // 匹配的法术学派掩码
    uint32      spellFamilyName;                            // if nonzero - for matching proc condition based on candidate spell's SpellFamilyNamer value (非零时匹配候选法术的 SpellFamilyName)
    ClassFamilyMask spellFamilyMask[MAX_EFFECT_INDEX];      // if nonzero - for matching proc condition based on candidate spell's SpellFamilyFlags  (like auras 107 and 108 do) (非零时匹配候选法术家族标志掩码)
    uint32      procFlags;                                  // bitmask for matching proc event (触发事件类型掩码)
    uint32      procEx;                                     // proc Extend info (see ProcFlagsEx) (触发扩展条件掩码)
    float       ppmRate;                                    // for melee (ranged?) damage spells - proc rate per minute. if zero, falls back to flat chance from Spell.dbc (每分钟触发次数 PPM，为 0 时回退至 Spell.dbc 固定几率)
    float       customChance;                               // Owerride chance (in most cases for debug only) (覆盖触发几率，通常用于调试或自定义)
    uint32      cooldown;                                   // hidden cooldown used for some spell proc events, applied to _triggered_spell_ (内置触发冷却时间，毫秒，作用于被触发法术)
};

typedef std::unordered_map<uint32, SpellProcEventEntry> SpellProcEventMap;

/// 药剂与合剂互斥分类掩码定义
#define ELIXIR_BATTLE_MASK    0x01
#define ELIXIR_GUARDIAN_MASK  0x02
#define ELIXIR_FLASK_MASK     (ELIXIR_BATTLE_MASK|ELIXIR_GUARDIAN_MASK)
#define ELIXIR_UNSTABLE_MASK  0x04
#define ELIXIR_SHATTRATH_MASK 0x08
#define ELIXIR_WELL_FED       0x10                          // Some foods have SPELLFAMILY_POTION (进食充分标志，部分食物属于药水家族)

/// 数据库 spell_threat 表对应的法术仇恨修正配置条目
struct SpellThreatEntry
{
    uint16 threat;                                           // 固定仇恨加值
    float multiplier;                                         // 仇恨乘数系数
    float ap_bonus;                                         // 攻击强度(AP)仇恨加成系数
    uint32 inverseEffectMask;                                   // 排除产生仇恨的效果索引掩码

    bool CanCauseThreatOnMask(uint32 mask) const { return ((~inverseEffectMask) & mask) != 0; }
};

typedef std::map<uint32, uint8> SpellElixirMap;
typedef std::map<uint32, float> SpellProcItemEnchantMap;
typedef std::map<uint32, SpellThreatEntry> SpellThreatMap;

// Spell script target related declarations (accessed using SpellMgr functions)
/// 数据库 spell_script_target 表定义的目标实体类型枚举
enum SpellTargetType
{
    SPELL_TARGET_TYPE_GAMEOBJECT    = 0,                   // 游戏对象目标
    SPELL_TARGET_TYPE_CREATURE      = 1,                     // 存活生物目标
    SPELL_TARGET_TYPE_DEAD          = 2,                         // 死亡生物/尸体目标
    SPELL_TARGET_TYPE_CREATURE_GUID = 3, // obsolete - use string id instead (已废弃，请改用 string_id)
    SPELL_TARGET_TYPE_GAMEOBJECT_GUID = 4, // obsolete - use string id instead (已废弃，请改用 string_id)
    SPELL_TARGET_TYPE_STRING_ID     = 5,                    // 字符串标识目标
};

#define MAX_SPELL_TARGET_TYPE 6

// pre-defined targeting for spells
/// 预定义法术脚本目标条目（spell_script_target）
struct SpellTargetEntry
{
    uint32 spellId;                                         // 法术 ID
    uint32 type;                                            // 目标实体类型（SpellTargetType）
    uint32 targetEntry;                                     // 目标模板 Entry 或编号
    uint32 inverseEffectMask;                               // 不允许命中的效果索引反掩码

    bool CanNotHitWithSpellEffect(SpellEffectIndex effect) const { return (inverseEffectMask & (1 << effect)) != 0; }
};

// coordinates for spells (accessed using SpellMgr functions)
/// 法术传送/目标落点坐标配置条目（spell_target_position）
struct SpellTargetPosition
{
    uint32 target_mapId;                                    // 目标地图 ID
    float  target_X;                                         // 目标 X 坐标
    float  target_Y;                                         // 目标 Y 坐标
    float  target_Z;                                         // 目标 Z 坐标
    float  target_Orientation;                               // 目标朝向角（弧度）
};

/// 法术锥形角度配置结构
struct SpellCone
{
    uint32 spellId;                                         // 法术 ID
    int32 coneAngle;                                       // 锥形判定角度
};

typedef std::unordered_map<uint32, SpellTargetPosition> SpellTargetPositionMap;

// Spell pet auras
/// 宠物光环封装类（主人的法术为不同宠物或默认宠物赋予对应被动光环）
class PetAura
{
    public:
        PetAura() :
            removeOnChangePet(false),
            damage(0)
        {
        }

        PetAura(uint32 petEntry, uint32 aura, bool _removeOnChangePet, int _damage) :
            removeOnChangePet(_removeOnChangePet), damage(_damage)
        {
            auras[petEntry] = aura;
        }

        uint32 GetAura(uint32 petEntry) const
        {
            std::map<uint32, uint32>::const_iterator itr = auras.find(petEntry);
            if (itr != auras.end())
                return itr->second;
            std::map<uint32, uint32>::const_iterator itr2 = auras.find(0);
            if (itr2 != auras.end())
                return itr2->second;
            return 0;
        }

        void AddAura(uint32 petEntry, uint32 aura)
        {
            auras[petEntry] = aura;
        }

        bool IsRemovedOnChangePet() const
        {
            return removeOnChangePet;
        }

        int32 GetDamage() const
        {
            return damage;
        }

    private:
        std::map<uint32, uint32> auras;
        bool removeOnChangePet;
        int32 damage;
};
typedef std::map<uint16, PetAura> SpellPetAuraMap;

/// 区域光环/区域限制法术配置条目（spell_area 表结构）
struct SpellArea
{
    uint32 spellId;
    uint32 areaId;                                          // zone/subzone/or 0 is not limited to zone (区域/子区域 ID，0 表示不限区域)
    uint32 questStart;                                      // quest start (quest must be active or rewarded for spell apply) (前置任务：需处于接取或已完成状态)
    uint32 questEnd;                                        // quest end (quest don't must be rewarded for spell apply) (截止任务：该任务不能处于已完成状态)
    uint16 conditionId;                                     // conditionId - will replace questStart, questEnd, raceMask, gender and questStartCanActive (条件 ID，用于替代任务/种族/性别条件)
    int32  auraSpell;                                       // spell aura must be applied for spell apply )if positive) and it don't must be applied in other case (前置光环 ID：正数要求拥有该光环，负数要求不得拥有)
    uint32 raceMask;                                        // can be applied only to races (允许生效的种族掩码)
    Gender gender;                                          // can be applied only to gender (允许生效的性别限制)
    bool questStartCanActive;                               // if true then quest start can be active (not only rewarded) (为 true 时前置任务接取中即可生效，否则需已交任务)
    bool autocast;                                          // if true then auto applied at area enter, in other case just allowed to cast (为 true 时进入区域自动施加，否则仅允许手动施放)

    // helpers
    bool IsFitToRequirements(Player const* player, uint32 newZone, uint32 newArea) const;
    void ApplyOrRemoveSpellIfCan(Player* player, uint32 newZone, uint32 newArea, bool onlyApply) const;
};

typedef std::multimap<uint32 /*applySpellId*/, SpellArea> SpellAreaMap;
typedef std::multimap<uint32 /*auraSpellId*/, SpellArea const*> SpellAreaForAuraMap;
typedef std::multimap<uint32 /*areaOrZoneId*/, SpellArea const*> SpellAreaForAreaMap;
typedef std::pair<SpellAreaMap::const_iterator, SpellAreaMap::const_iterator> SpellAreaMapBounds;
typedef std::pair<SpellAreaForAuraMap::const_iterator, SpellAreaForAuraMap::const_iterator>  SpellAreaForAuraMapBounds;
typedef std::pair<SpellAreaForAreaMap::const_iterator, SpellAreaForAreaMap::const_iterator>  SpellAreaForAreaMapBounds;


// Spell rank chain  (accessed using SpellMgr functions)
/// 法术等级链节点（描述法术的前置等级、首发等级、学习前置与当前阶数）
struct SpellChainNode
{
    uint32 prev;                                           // 上一级法术 ID
    uint32 first;                                          // 等级链第一级法术 ID
    uint32 req;                                            // 学习所需前置法术 ID
    uint8  rank;                                           // 在等级链中的等级序号
};

typedef std::unordered_map<uint32, SpellChainNode> SpellChainMap;
typedef std::multimap<uint32, uint32> SpellChainMapNext;

// Spell learning properties (accessed using SpellMgr functions)
/// 法术学习技能节点（通过施放法术学会专业技能或进阶阶段）
struct SpellLearnSkillNode
{
    uint16 skill;                                          // 学会的技能线 ID
    uint16 step;                                           // 技能阶位（初级/中级/高级/大师等）
    SpellEffects effect;                                   // 触发学习的法术效果类型
};

typedef std::map<uint32, SpellLearnSkillNode> SpellLearnSkillMap;

/// 关联学习法术节点（学习某法术时连带自动学会的衍生法术）
struct SpellLearnSpellNode
{
    uint32 spell;                                          // 被自动学习的子法术 ID
    bool active;                                            // show in spellbook or not (是否在法术书中显示)
    bool autoLearned;                                       // 是否为自动习得（无需训练师）
};

typedef std::multimap<uint32, SpellLearnSpellNode> SpellLearnSpellMap;
typedef std::pair<SpellLearnSpellMap::const_iterator, SpellLearnSpellMap::const_iterator> SpellLearnSpellMapBounds;

typedef std::multimap<uint32, SkillLineAbilityEntry const*> SkillLineAbilityMap;
typedef std::pair<SkillLineAbilityMap::const_iterator, SkillLineAbilityMap::const_iterator> SkillLineAbilityMapBounds;

typedef std::multimap<uint32, SkillRaceClassInfoEntry const*> SkillRaceClassInfoMap;
typedef std::pair<SkillRaceClassInfoMap::const_iterator, SkillRaceClassInfoMap::const_iterator> SkillRaceClassInfoMapBounds;

/// 判断技能 ID 是否为主专业技能
bool IsPrimaryProfessionSkill(uint32 skill);

/// 判断技能 ID 是否为专业技能（主专业或采集/辅助专业）
inline bool IsProfessionSkill(uint32 skill)
{
    return  IsPrimaryProfessionSkill(skill) || skill == SKILL_FISHING || skill == SKILL_COOKING || skill == SKILL_FIRST_AID;
}

/// 判断技能 ID 是否为专业技能或骑术
inline bool IsProfessionOrRidingSkill(uint32 skill)
{
    return  IsProfessionSkill(skill) || skill == SKILL_RIDING;
}

/// 全局法术管理器单例类（负责加载并索引数据库与 DBC 中的法术规则、等级链、仇恨、触发、区域限制等）
class SpellMgr
{
        friend struct DoSpellProcEvent;
        friend struct DoSpellProcItemEnchant;

        // Constructors
    public:
        SpellMgr();
        ~SpellMgr();

        // Accessors (const or static functions)
    public:
        // Spell affects
        /// 获取法术效果影响的目标法术家族掩码（优先读取 spell_affect 表，否则回退 DBC）
        ClassFamilyMask GetSpellAffectMask(uint32 spellId, SpellEffectIndex effectId) const
        {
            SpellAffectMap::const_iterator itr = mSpellAffectMap.find((spellId << 8) + effectId);
            if (itr != mSpellAffectMap.end())
                return ClassFamilyMask(itr->second);
            if (SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(spellId))
                return ClassFamilyMask(spellEntry->EffectItemType[effectId]);
            return ClassFamilyMask();
        }

        /// 获取全部药剂/合剂分类映射表
        SpellElixirMap const& GetSpellElixirMap() const { return mSpellElixirs; }

        /// 获取指定法术的药剂/合剂互斥掩码
        uint32 GetSpellElixirMask(uint32 spellid) const
        {
            SpellElixirMap::const_iterator itr = mSpellElixirs.find(spellid);
            if (itr == mSpellElixirs.end())
                return 0x0;

            return itr->second;
        }

        // Reverse engineered from binary: do not alter
        /// 客户端判定目标类型是否属于有害目标（反编译自客户端逻辑，勿随意修改）
        static inline bool IsSpellTargetHarmfulAtClient(uint32 target)
        {
            switch (target)
            {
                case TARGET_UNIT_ENEMY_NEAR_CASTER:
                case TARGET_UNIT_ENEMY:
                case TARGET_ENUM_UNITS_ENEMY_AOE_AT_SRC_LOC:
                case TARGET_ENUM_UNITS_ENEMY_AOE_AT_DEST_LOC:
                case TARGET_ENUM_UNITS_ENEMY_IN_CONE_24:
                case TARGET_ENUM_UNITS_ENEMY_AOE_AT_DYNOBJ_LOC:
                case TARGET_LOCATION_CASTER_TARGET_POSITION:
                case TARGET_ENUM_UNITS_ENEMY_IN_CONE_54:
                case TARGET_CORPSE_ENEMY_NEAR_CASTER:
                    return true;
                default:
                    return false;
            }
        }

        // Reverse engineered from binary: do not alter
        /// 客户端判定目标类型是否属于有益目标（反编译自客户端逻辑，勿随意修改）
        static inline bool IsSpellTargetHelpfulAtClient(uint32 target)
        {
            switch (target)
            {
                case TARGET_UNIT_CASTER:
                case TARGET_UNIT_FRIEND_NEAR_CASTER:
                case TARGET_UNIT_NEAR_CASTER:
                case TARGET_UNIT_CASTER_PET:
                case TARGET_ENUM_UNITS_PARTY_WITHIN_CASTER_RANGE:
                case TARGET_UNIT_FRIEND:
                case TARGET_UNIT_CASTER_MASTER:
                case TARGET_ENUM_UNITS_FRIEND_AOE_AT_DYNOBJ_LOC:
                case TARGET_ENUM_UNITS_FRIEND_AOE_AT_SRC_LOC:
                case TARGET_ENUM_UNITS_FRIEND_AOE_AT_DEST_LOC:
                case TARGET_ENUM_UNITS_PARTY_AOE_AT_SRC_LOC:
                case TARGET_ENUM_UNITS_PARTY_AOE_AT_DEST_LOC:
                case TARGET_UNIT_PARTY:
                case TARGET_UNIT_FRIEND_CHAIN_HEAL:
                case TARGET_ENUM_UNITS_RAID_WITHIN_CASTER_RANGE:
                case TARGET_UNIT_RAID:
                case TARGET_UNIT_RAID_NEAR_CASTER:
                case TARGET_ENUM_UNITS_FRIEND_IN_CONE:
                case TARGET_UNIT_RAID_AND_CLASS:
                case TARGET_PLAYER_RAID_NYI:
                    return true;
                default:
                    return false;
            }
        }

        // Reverse engineered from binary: do not alter
        /// 客户端判定法术单个效果是否属于有害效果
        static inline bool IsSpellEffectHarmfulAtClient(const SpellEntry &entry, SpellEffectIndex effIndex)
        {
            return (IsSpellTargetHarmfulAtClient(entry.EffectImplicitTargetA[effIndex]) || IsSpellTargetHarmfulAtClient(entry.EffectImplicitTargetB[effIndex]));
        }

        // Reverse engineered from binary: do not alter
        /// 客户端判定法术单个效果是否属于有益效果
        static inline bool IsSpellEffectHelpfulAtClient(const SpellEntry &entry, SpellEffectIndex effIndex)
        {
            if (IsSpellTargetHelpfulAtClient(entry.EffectImplicitTargetA[effIndex]))
            {
                if (entry.EffectImplicitTargetA[effIndex] != TARGET_UNIT_CASTER || entry.EffectApplyAuraName[effIndex] != SPELL_AURA_DUMMY)
                    return true;
            }

            if (IsSpellTargetHelpfulAtClient(entry.EffectImplicitTargetB[effIndex]))
            {
                if (entry.EffectImplicitTargetB[effIndex] != TARGET_UNIT_CASTER || entry.EffectApplyAuraName[effIndex] != SPELL_AURA_DUMMY)
                    return true;
            }

            return false;
        }

        // Reverse engineered from binary: do not alter
        /// 客户端法术阵营倾向分类（中立 / 有益 / 有害）
        enum SpellFaction
        {
            SPELL_NEUTRAL = 0,                                     // 中立倾向
            SPELL_HELPFUL = 1,                                     // 有益倾向
            SPELL_HARMFUL = 2,                                     // 有害倾向
        };

        // Reverse engineered from binary: do not alter
        /// 按客户端规则判定法术在指定效果掩码下的阵营倾向（中立/有益/有害）
        static inline SpellFaction GetSpellFactionAtClient(const SpellEntry &entry, SpellEffectIndexMask mask = EFFECT_MASK_ALL)
        {
            if (entry.Targets & TARGET_FLAG_UNIT_ALLY)
                return SPELL_HELPFUL;

            if (entry.Targets & TARGET_FLAG_UNIT_ENEMY)
                return SPELL_HARMFUL;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                // Customization: skip junk, skip by mask
                if (!entry.Effect[i] || !(mask & (1 << i)))
                    continue;

                if (IsSpellEffectHarmfulAtClient(entry, SpellEffectIndex(i)))
                    return SPELL_HARMFUL;

                if (IsSpellEffectHelpfulAtClient(entry, SpellEffectIndex(i)))
                    return SPELL_HELPFUL;
            }

            return SPELL_NEUTRAL;
        }

        /// 获取法术仇恨修正配置条目
        SpellThreatEntry const* GetSpellThreatEntry(uint32 spellid) const
        {
            SpellThreatMap::const_iterator itr = mSpellThreatMap.find(spellid);
            if (itr != mSpellThreatMap.end())
                return &itr->second;

            return nullptr;
        }

        /// 获取法术仇恨倍率（无特殊配置时返回 1.0）
        float GetSpellThreatMultiplier(SpellEntry const* spellInfo) const
        {
            if (!spellInfo)
                return 1.0f;

            if (SpellThreatEntry const* entry = GetSpellThreatEntry(spellInfo->Id))
                return entry->multiplier;

            return 1.0f;
        }

        // Spell proc events
        /// 获取法术触发事件配置条目
        SpellProcEventEntry const* GetSpellProcEvent(uint32 spellId) const
        {
            SpellProcEventMap::const_iterator itr = mSpellProcEventMap.find(spellId);
            if (itr != mSpellProcEventMap.end())
                return &itr->second;
            return nullptr;
        }

        // Spell procs from item enchants
        /// 获取物品附魔触发的 PPM/概率值
        float GetItemEnchantProcChance(uint32 spellid) const
        {
            SpellProcItemEnchantMap::const_iterator itr = mSpellProcItemEnchantMap.find(spellid);
            if (itr == mSpellProcItemEnchantMap.end())
                return 0.0f;

            return itr->second;
        }

        /// 判断法术触发事件是否可由当前触发标志与候选法术触发
        static bool IsSpellProcEventCanTriggeredBy(SpellProcEventEntry const* spellProcEvent, uint32 EventProcFlag, SpellEntry const* spellInfo, uint32 procFlags, uint32 procExtra);

        // Spell target coordinates
        /// 获取法术预设目标坐标（spell_target_position）
        SpellTargetPosition const* GetSpellTargetPosition(uint32 spell_id) const
        {
            SpellTargetPositionMap::const_iterator itr = mSpellTargetPositions.find(spell_id);
            if (itr != mSpellTargetPositions.end())
                return &itr->second;
            return nullptr;
        }

        // Spell ranks chains
        /// 获取法术在等级链中的节点信息
        SpellChainNode const* GetSpellChainNode(uint32 spell_id) const
        {
            SpellChainMap::const_iterator itr = mSpellChains.find(spell_id);
            if (itr == mSpellChains.end())
                return nullptr;

            return &itr->second;
        }

        /// 获取法术等级链中的第一级（最低等级）法术 ID
        uint32 GetFirstSpellInChain(uint32 spell_id) const
        {
            if (SpellChainNode const* node = GetSpellChainNode(spell_id))
                return node->first;

            return spell_id;
        }

        /// 获取法术等级链中的上一级法术 ID
        uint32 GetPrevSpellInChain(uint32 spell_id) const
        {
            if (SpellChainNode const* node = GetSpellChainNode(spell_id))
                return node->prev;

            return 0;
        }

        /// 获取法术等级链中的下一级法术 ID
        uint32 GetNextSpellInChain(uint32 spell_id) const
        {
            SpellChainMapNext const& nextMap = GetSpellChainNext();

            for (SpellChainMapNext::const_iterator itr = nextMap.lower_bound(spell_id); itr != nextMap.upper_bound(spell_id); ++itr)
            {
                SpellChainNode const* node = GetSpellChainNode(itr->second);

                // If next spell is a requirement for this one then skip it
                if (node->req == spell_id)
                    continue;

                if (node->prev == spell_id)
                    return itr->second;
            }

            return 0;
        }

        /// 获取法术等级后继映射表
        SpellChainMapNext const& GetSpellChainNext() const { return mSpellChainsNext; }

        template<typename Worker>
        /// 对法术等级链中所有更高等级法术依次执行工作回调
        void doForHighRanks(uint32 spellid, Worker& worker)
        {
            SpellChainMapNext const& nextMap = GetSpellChainNext();
            for (SpellChainMapNext::const_iterator itr = nextMap.lower_bound(spellid); itr != nextMap.upper_bound(spellid); ++itr)
            {
                worker(itr->second);
                doForHighRanks(itr->second, worker);
            }
        }

        // Note: not use rank for compare to spell ranks: spell chains isn't linear order
        // Use IsSpellHigherRankOfSpell instead
        /// 获取法术在等级链中的等级序号
        uint8 GetSpellRank(uint32 spell_id) const
        {
            if (SpellChainNode const* node = GetSpellChainNode(spell_id))
                return node->rank;

            return 0;
        }

        /// 判断 spellId1 是否为 spellId2 同等级链中的更高等级法术
        bool IsSpellHigherRankOfSpell(uint32 spellId1, uint32 spellId2) const
        {
            if (spellId1 == spellId2)
                return false;

            SpellChainMap::const_iterator itr = mSpellChains.find(spellId1);

            uint32 rank2 = GetSpellRank(spellId2);

            // not ordered correctly by rank value
            if (itr == mSpellChains.end() || !rank2 || itr->second.rank <= rank2)
                return false;

            // check present in same rank chain
            for (; itr != mSpellChains.end(); itr = mSpellChains.find(itr->second.prev))
                if (itr->second.prev == spellId2)
                    return true;

            return false;
        }

        /// 判断两个法术是否为同一法术等级链中的不同等级
        inline bool IsSpellAnotherRankOfSpell(uint32 spellId1, uint32 spellId2) const
        {
            return (spellId1 != spellId2 && GetFirstSpellInChain(spellId1) == GetFirstSpellInChain(spellId2));
        }

        /// 判断法术是否为单目标限制光环（同一施法者只能对一个目标维持）
        inline bool IsSingleTargetSpell(SpellEntry const* entry) const
        {
            if (entry->HasAttribute(SPELL_ATTR_EX5_LIMIT_N))
                return true;

            // single target triggered spell.
            // Not real client side single target spell, but it' not triggered until prev. aura expired.
            // This is allow store it in single target spells list for caster for spell proc checking
            if (entry->Id == 38324)                             // Regeneration (triggered by 38299 (HoTs on Heals)) (由 38299 触发的再生光环，按单目标限制追踪)
                return true;

            return false;
        }

        /// 判断两个单目标限制法术是否互相排斥（同法术、同等级链或同家族衍生技能）
        bool IsSingleTargetSpells(SpellEntry const* entry1, SpellEntry const* entry2) const
        {
            if (!IsSingleTargetSpell(entry1) || !IsSingleTargetSpell(entry2))
                return false;

            // Early instance of same spell check
            if (entry1 == entry2 || entry1->Id == entry2->Id)
                return true;

            // One spell is a rank of another spell
            if (IsSpellAnotherRankOfSpell(entry1->Id, entry2->Id))
                return true;

            // Experimental: Try to detect spinoffs of specific family spells (e.g. polymorph flavors)
            if (entry1->SpellFamilyName != SPELLFAMILY_GENERIC && entry2->SpellFamilyName != SPELLFAMILY_GENERIC)
            {
                if (!entry1->SpellFamilyFlags.Empty() && !entry2->SpellFamilyFlags.Empty())
                    return entry1->IsFitToFamily(SpellFamily(entry2->SpellFamilyName), entry2->SpellFamilyFlags);
            }

            return false;
        }

        /// 获取法术书中该法术的后继升级法术 ID
        uint32 GetSpellBookSuccessorSpellId(uint32 spellId)
        {
            SkillLineAbilityMapBounds bounds = GetSkillLineAbilityMapBoundsBySpellId(spellId);
            for (SkillLineAbilityMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                if (SkillLineAbilityEntry const* pAbility = itr->second)
                {
                    if (pAbility->forward_spellid)
                        return pAbility->forward_spellid;
                }
            }
            return 0;
        }

        // return true if spell1 can affect spell2
        /// 判断施法/增益法术 1（如天赋或雕文类修正）是否能影响法术 2
        bool IsSpellCanAffectSpell(SpellEntry const* spellInfo_1, SpellEntry const* spellInfo_2) const;

        /// 根据单位当前等级从法术等级链中选择合适的法术等级
        SpellEntry const* SelectAuraRankForLevel(SpellEntry const* spellInfo, uint32 level) const;

        // Spell learning
        /// 获取施放该法术可学会的专业技能信息
        SpellLearnSkillNode const* GetSpellLearnSkill(uint32 spell_id) const
        {
            SpellLearnSkillMap::const_iterator itr = mSpellLearnSkills.find(spell_id);
            if (itr != mSpellLearnSkills.end())
                return &itr->second;
            return nullptr;
        }

        /// 判断该法术是否关联了额外自动学习的其他法术
        bool IsSpellLearnSpell(uint32 spell_id) const
        {
            return mSpellLearnSpells.find(spell_id) != mSpellLearnSpells.end();
        }

        /// 获取该法术关联学习的衍生法术范围迭代器
        SpellLearnSpellMapBounds GetSpellLearnSpellMapBounds(uint32 spell_id) const
        {
            return mSpellLearnSpells.equal_range(spell_id);
        }

        /// 判断 parent 法术是否会连带学会 child 法术
        bool IsSpellLearnToSpell(uint32 parent, uint32 child) const
        {
            SpellLearnSpellMapBounds bounds = GetSpellLearnSpellMapBounds(parent);
            for (SpellLearnSpellMap::const_iterator i = bounds.first; i != bounds.second; ++i)
                if (i->second.spell == child)
                    return true;
            return false;
        }

        /// 判断法术是否为专业技能或骑术学习法术
        static bool IsProfessionOrRidingSpell(uint32 spellId);
        /// 判断法术是否为专业技能学习法术
        static bool IsProfessionSpell(uint32 spellId);
        /// 判断法术是否为主专业技能学习法术
        static bool IsPrimaryProfessionSpell(uint32 spellId);
        /// 判断法术是否为主专业技能的初级（第一阶）学习法术
        bool IsPrimaryProfessionFirstRankSpell(uint32 spellId) const;

        /// 判断法术是否为提供专业技能点数加成的光环法术
        bool IsSkillBonusSpell(uint32 spellId) const;

        // Spell correctness for client using
        /// 校验法术模板数据是否合法完整（用于客户端使用及 GM 命令检查）
        static bool IsSpellValid(SpellEntry const* spellInfo, Player* pl = nullptr, bool msg = true);

        /// 按法术 ID 查询技能线能力映射（SkillLineAbility）
        SkillLineAbilityMapBounds GetSkillLineAbilityMapBoundsBySpellId(uint32 spellId) const
        {
            return mSkillLineAbilityMapBySpellId.equal_range(spellId);
        }

        /// 按技能 ID 查询技能线能力映射（SkillLineAbility）
        SkillLineAbilityMapBounds GetSkillLineAbilityMapBoundsBySkillId(uint32 skillId) const
        {
            return mSkillLineAbilityMapBySkillId.equal_range(skillId);
        }

        /// 按技能 ID 查询种族/职业技能限制信息（SkillRaceClassInfo）
        SkillRaceClassInfoMapBounds GetSkillRaceClassInfoMapBounds(uint32 skill_id) const
        {
            return mSkillRaceClassInfoMap.equal_range(skill_id);
        }

        /// 获取法术对应的宠物光环配置
        PetAura const* GetPetAura(uint32 spell_id)
        {
            SpellPetAuraMap::const_iterator itr = mSpellPetAuraMap.find(spell_id);
            if (itr != mSpellPetAuraMap.end())
                return &itr->second;
            else
                return nullptr;
        }

        /// 检查法术在指定地图/区域/子区域是否允许施放，返回对应错误码
        SpellCastResult GetSpellAllowedInLocationError(SpellEntry const* spellInfo, uint32 map_id, uint32 zone_id, uint32 area_id, Player const* player = nullptr) const;

        /// 按法术 ID 查询区域限制法术配置
        SpellAreaMapBounds GetSpellAreaMapBounds(uint32 spell_id) const
        {
            return mSpellAreaMap.equal_range(spell_id);
        }

        /// 按前置光环 ID 查询关联的区域限制法术配置
        SpellAreaForAuraMapBounds GetSpellAreaForAuraMapBounds(uint32 spell_id) const
        {
            return mSpellAreaForAuraMap.equal_range(spell_id);
        }

        /// 按区域或子区域 ID 查询该区域生效的区域限制法术配置
        SpellAreaForAreaMapBounds GetSpellAreaForAreaMapBounds(uint32 area_id) const
        {
            return mSpellAreaForAreaMap.equal_range(area_id);
        }

        // Modifiers
    public:
        /// 获取全局 SpellMgr 单例引用
        static SpellMgr& Instance();

        /// 检查指定数据库表中引用的法术 ID 是否存在并校验有效性
        void CheckUsedSpells(char const* table) const;

        // Loading data at server startup
        /// 加载法术等级链（spell_chain）
        void LoadSpellChains();
        /// 加载法术学习技能映射（spell_learn_skill）
        void LoadSpellLearnSkills();
        /// 加载法术连带学习映射（spell_learn_spell）
        void LoadSpellLearnSpells();
        /// 加载法术脚本目标映射（spell_script_target）
        void LoadSpellScriptTarget();
        /// 加载法术影响家族掩码（spell_affect）
        void LoadSpellAffects();
        /// 加载药剂/合剂分类表（spell_elixir）
        void LoadSpellElixirs();
        /// 加载法术触发事件配置（spell_proc_event）
        void LoadSpellProcEvents();
        /// 加载物品附魔触发概率表（spell_proc_item_enchant）
        void LoadSpellProcItemEnchant();
        /// 加载法术目标传送坐标（spell_target_position）
        void LoadSpellTargetPositions();
        /// 加载法术仇恨修正表（spell_threat）
        void LoadSpellThreats();
        /// 构建 DBC 技能线能力（SkillLineAbility）双索引映射
        void LoadSkillLineAbilityMaps();
        /// 构建 DBC 技能种族职业信息（SkillRaceClassInfo）索引映射
        void LoadSkillRaceClassInfoMap();
        /// 加载宠物光环映射（spell_pet_auras）
        void LoadSpellPetAuras();
        /// 加载区域限制与自动光环配置（spell_area）
        void LoadSpellAreas();

    private:
        /// 法术 ID -> 等级链节点映射
        SpellChainMap      mSpellChains;
        /// 前置法术 ID -> 后继法术 ID 多重映射
        SpellChainMapNext  mSpellChainsNext;
        /// 法术 ID -> 学习技能节点映射
        SpellLearnSkillMap mSpellLearnSkills;
        /// 法术 ID -> 衍生学习法术多重映射
        SpellLearnSpellMap mSpellLearnSpells;
        /// 法术 ID -> 预设目标位置映射
        SpellTargetPositionMap mSpellTargetPositions;
        /// (法术ID<<8 | 效果索引) -> 家族影响掩码映射
        SpellAffectMap     mSpellAffectMap;
        /// 法术 ID -> 药剂/合剂分类掩码映射
        SpellElixirMap     mSpellElixirs;
        /// 法术 ID -> 仇恨修正配置映射
        SpellThreatMap     mSpellThreatMap;
        /// 法术 ID -> 触发事件配置映射
        SpellProcEventMap  mSpellProcEventMap;
        /// 附魔法术 ID -> 触发概率映射
        SpellProcItemEnchantMap mSpellProcItemEnchantMap;
        /// 法术 ID -> DBC 技能线能力条目多重映射
        SkillLineAbilityMap mSkillLineAbilityMapBySpellId;
        /// 技能 ID -> DBC 技能线能力条目多重映射
        SkillLineAbilityMap mSkillLineAbilityMapBySkillId;
        /// 技能 ID -> DBC 种族职业限制条目多重映射
        SkillRaceClassInfoMap mSkillRaceClassInfoMap;
        /// 法术 ID -> 宠物光环配置映射
        SpellPetAuraMap     mSpellPetAuraMap;
        /// 施加法术 ID -> 区域法术配置多重映射
        SpellAreaMap         mSpellAreaMap;
        /// 依赖光环 ID -> 区域法术配置多重映射
        SpellAreaForAuraMap  mSpellAreaForAuraMap;
        /// 区域/子区域 ID -> 区域法术配置多重映射
        SpellAreaForAreaMap  mSpellAreaForAreaMap;
};

#define sSpellMgr SpellMgr::Instance()            // 全局法术管理器访问宏
#endif
