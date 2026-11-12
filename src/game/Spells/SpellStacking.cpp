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

#include "Spells/SpellStacking.h"
#include "Server/DBCStructure.h"
#include "Entities/Unit.h"
#include "Database/DatabaseEnv.h"
#include "Spells/SpellMgr.h"
#include "Server/SQLStorages.h"

// 获取全局唯一的 SpellStacker 实例
SpellStacker& SpellStacker::Instance()
{
    static SpellStacker spellStacker;
    return spellStacker;
}

// 从世界数据库加载法术叠加组（spell_group 与 spell_group_spell）并构建掩码索引
void SpellStacker::LoadSpellGroups()
{
    m_spellGroups.clear();
    m_spellGroupSpellData.clear();

    uint32 count = 0;

    auto queryResult = WorldDatabase.Query("SELECT Id, Rule, Name FROM spell_group");

    if (queryResult)
    {
        BarGoLink bar(queryResult->GetRowCount());
        do
        {
            Field* fields = queryResult->Fetch();

            bar.step();

            SpellGroup group;
            group.Id = fields[0].GetUInt32();
            group.rule = (SpellGroupRule)fields[1].GetUInt32();
            group.name = fields[2].GetCppString();

            if (SpellGroupRule::MAX <= group.rule)
            {
                sLog.outErrorDb("spell_group (Id: %u) has invalid rule %u.", group.Id, static_cast<uint32>(group.rule));

                continue;
            }

            m_spellGroups.emplace(group.Id, group);
        }
        while (queryResult->NextRow());
    }

    sLog.outString(">> Loaded %u spell groups", count);
    sLog.outString();

    count = 0;

    queryResult = WorldDatabase.Query("SELECT Id, SpellId FROM spell_group_spell");

    if (queryResult)
    {
        BarGoLink bar(queryResult->GetRowCount());
        do
        {
            Field* fields = queryResult->Fetch();

            bar.step();

            uint32 groupId = fields[0].GetUInt32();
            uint32 spellId = fields[1].GetUInt32();

            auto existingItr = m_spellGroups.find(groupId);
            if (existingItr == m_spellGroups.end())
            {
                sLog.outErrorDb("`spell_group_spell` group id %u does not exist", groupId);
                continue;
            }

            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
            if (!spellInfo)
            {
                sLog.outErrorDb("Spell (ID:%u) listed in `spell_group_spell` group id %u does not exist.", spellId, groupId);
                continue;
            }

            existingItr->second.spellIds.push_back(spellId);
        }
        while (queryResult->NextRow());
    }

    // 为每个法术建立所属组的 64 位掩码，便于快速判定组冲突
    for (auto& group : m_spellGroups)
    {
        for (uint32 spellId : group.second.spellIds)
        {
            auto& maskData = m_spellGroupSpellData[spellId];
            maskData.mask |= (uint64(1) << (group.second.Id - 1)); // start from Id 1 (组 ID 从 1 开始编号)
            if (maskData.rule == (SpellGroupRule)0)
                maskData.rule = group.second.rule;
        }
    }

    sLog.outString(">> Loaded %u spell group spells", count);
    sLog.outString();
}

// 判定 entry 的 effIdx 效果光环是否可与 entry2 叠加
bool SpellStacker::IsStackableAuraEffect(SpellEntry const* entry, SpellEntry const* entry2, SpellEffectIndex effIdx, Unit* target) const
{
    const uint32 aura = entry->EffectApplyAuraName[effIdx];
    // Ignore non-aura effects (忽略非光环效果)
    if (!aura)
        return true;

    // Get first similar - second spell's same aura with the same sign (在第二个法术中查找首个同类型且同正负性的相似光环效果)
    uint32 similar = EFFECT_INDEX_0;
    for (uint32 e = EFFECT_INDEX_0; e < MAX_EFFECT_INDEX; ++e)
    {
        if (IsSimilarAuraEffect(entry, effIdx, entry2, e))
        {
            similar = e;
            break;
        }
        else if (e == (MAX_EFFECT_INDEX - 1))
            return true; // No similarities (无相似光环，可直接叠加)
    }

    // Special case for potions (药水类法术允许叠加/另行处理)
    if (entry->SpellFamilyName == SPELLFAMILY_POTION || entry2->SpellFamilyName == SPELLFAMILY_POTION)
        return true;

    // Special rule for food buffs - TODO: Remove one day (食物“进食充分”特殊规则：仅一方属于 WELL_FED 组时允许叠加)
    auto firstItr = m_spellGroupSpellData.find(entry->Id);
    auto secondItr = m_spellGroupSpellData.find(entry2->Id);
    if (firstItr != m_spellGroupSpellData.end() && secondItr != m_spellGroupSpellData.end())
    {
        auto wellFedFlag = convertEnumToFlag(SpellGroupId::WELL_FED);
        if (firstItr->second.HasFlag(wellFedFlag) && !secondItr->second.HasFlag(wellFedFlag))
            return true;
    }

    // Short alias (法术关联性简写标志)
    const bool positive = (IsPositiveEffect(entry, effIdx));
    const bool related = (entry->SpellFamilyName == entry2->SpellFamilyName);
    const bool siblings = (entry->SpellFamilyFlags == entry2->SpellFamilyFlags);
    const bool player = (entry->SpellFamilyName && !entry->SpellFamilyFlags.Empty());
    const bool multirank = (related && siblings && player);
    const bool instance = (entry->Id == entry2->Id || multirank);

    // HACK:
    // Old, bad practice, but the detection of a few old spells may still depend on it (旧版兼容：部分法术仍依赖图标与视觉 ID 区分)
    const bool icon = (entry->SpellIconID == entry2->SpellIconID);
    const bool visual = entry->SpellVisual == entry2->SpellVisual;

    // If aura makes spell not multi-instanceable (do not stack the same spell id or ranks of this spell)
    // 若设为 true，则禁止同法术 ID 或同技能不同等级（multirank）叠加
    bool nonmui = false;

    // Stack rules (按光环类型分派叠加规则)
    switch (aura)
    {
        // Dummy/Triggers (虚拟/周期触发类光环)
        case SPELL_AURA_DUMMY:
        case SPELL_AURA_PERIODIC_DUMMY:
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
        {
            if (!related)
                break;
            switch (entry->SpellFamilyName)
            {
                case SPELLFAMILY_SHAMAN:
                    if (entry->IsFitToFamilyMask(uint64(0x200)) && multirank)
                        return true; // Shaman Reincarnation (Passive) and Twisting Nether (萨满复生被动与扭曲虚空可共存)
                    break;
                case SPELLFAMILY_DRUID:
                    if (entry->IsFitToFamilyMask(uint64(0x80)) && multirank)
                        return true; // Tranquility (宁静可叠加)
                    if (entry->IsFitToFamilyMask(uint64(0x01000000000)))
                        return true; // Lifebloom (生命绽放允许堆叠/共存)
                    break;
            }
            break;
        }
        // DoT (持续伤害与吸取类)
        case SPELL_AURA_PERIODIC_LEECH:
        case SPELL_AURA_PERIODIC_MANA_LEECH:
            if (target && target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
                return false;
            break;
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
        case SPELL_AURA_POWER_BURN_MANA:
            return true;
            break;
            // HoT (持续治疗与固定周期恢复类)
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_OBS_MOD_HEALTH:
        case SPELL_AURA_OBS_MOD_MANA:
            return true;
            break;
            // Raid debuffs: Hunter's Mark and Expose Weakness stack with each other, but not itself
            // 团队减益：猎人印记与破甲虚弱可互相叠加，但不可与自身叠加
        case SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS:
        case SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS:
            return (!related || !siblings);
            break;
        case SPELL_AURA_MOD_ATTACK_POWER:
            // Attack Power debuffs logic: Do not stack Curse of Weakness, Demoralizing Roars/Shouts (虚弱诅咒、挫志咆哮/怒吼等减攻强效果互斥)
            if (!positive && entry->EffectBasePoints[effIdx] < 1 && entry2->EffectBasePoints[similar] < 1)
                return (!entry->SpellFamilyName && !entry2->SpellFamilyName);
            break;
            // Armor & Resistance buffs and debuffs logic (护甲与抗性增益/减益叠加逻辑)
        case SPELL_AURA_MOD_RESISTANCE:
        {
            if (entry->EffectMiscValue[effIdx] != entry2->EffectMiscValue[similar])
                break;
            if (positive)
            {
                // Used as a non-zero type in this context (在此上下文作为非零类型标志)
                const bool type = (entry->Dispel && entry->Dispel == entry2->Dispel);
                const bool attacktable = (entry->DmgClass && entry->DmgClass == entry2->DmgClass);
                if ((attacktable || type) && !entry->SpellFamilyName && !entry2->SpellFamilyName)
                    return false; // Do not stack scrolls with other srolls and some procs (such as Hyjal ring) (非职业卷轴/触发类护甲增益互斥)
                if (player && related && siblings && entry->HasAttribute(SPELL_ATTR_EX3_DOT_STACKING_RULE))
                    return true;
            }
            else
            {
                if (entry->Id == 16145 || entry2->Id == 16145) // Grom'tor's sunder spell stacks with warrior sunder (特定任务破甲法术与战士破甲可叠加)
                {
                    nonmui = true;
                    break;
                }  
                const bool type = (entry->Dispel == entry2->Dispel);
                const bool prevention = (entry->PreventionType && entry->PreventionType == entry2->PreventionType);
                if (type && prevention)
                    return false;
            }
            break;
        }
        case SPELL_AURA_MOD_RESISTANCE_PCT:
        {
            if (entry->EffectMiscValue[effIdx] != entry2->EffectMiscValue[similar])
                break;
            if (positive && entry->Dispel && entry->Dispel == entry2->Dispel)
                return false; // Inspiration / Ancestral Fortitude (灵感与先祖坚韧互斥)
            nonmui = true;
            break;
        }
        // By default base stats cannot stack if they're similar (默认情况下同类基础属性增益不可叠加)
        case SPELL_AURA_MOD_STAT:
        {
            if (entry->Id == 5320 || entry2->Id == 5320) // Echeyakee's Grace - stacks with everything (任务增益可与所有效果叠加)
                return true;
            if (entry->Id == 15366 || entry2->Id == 15366) // Songflower Serenade - stacks with everything (风歌夜曲可与所有效果叠加)
                return true;
            if (entry->EffectMiscValue[effIdx] != entry2->EffectMiscValue[similar])
                break;
            if (positive)
            {
                // Used as a non-zero type in this context
                const bool type = (entry->Dispel && entry->Dispel == entry2->Dispel);
                const bool family = (entry->SpellFamilyName || entry2->SpellFamilyName);
                const bool uncategorized = (!entry->SpellFamilyName || !entry2->SpellFamilyName);
                if (type && family && uncategorized)
                    return false; // Do not stack player buffs with scrolls (玩家职业 Buff 与卷轴不叠加)
                if (!entry->SpellFamilyName && !entry2->SpellFamilyName)
                {
                    if (type)
                        return false; // Do not stack scrolls and other non-player buffs with each other (卷轴与非玩家增益互相不叠加)
                    if (entry->HasAttribute(SPELL_ATTR_EX2_NOT_AN_ACTION) && entry2->HasAttribute(SPELL_ATTR_EX2_NOT_AN_ACTION))
                        return false; // FIXME: Cozy fire hack (篝火类精神加成不叠加)
                }
            }
            else
            {
                // Debuffs are based on patch notes decription of Shrink effects stacking (减益依据缩小效果叠加规则)
                // Used as a non-zero type in this context
                const bool type = (entry->Dispel && entry->Dispel == entry2->Dispel);
                if (type)
                    return false;
                nonmui = true;
            }
            break;
        }
        case SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE:
        case SPELL_AURA_MOD_PERCENT_STAT:
            nonmui = true;
            break;
        case SPELL_AURA_MOD_RATING: // Rejuvenation also has this (战斗等级类修正)
        {
            if (entry->EffectMiscValue[effIdx] != entry2->EffectMiscValue[similar])
                break;
            if (entry->Dispel && entry->Dispel == entry2->Dispel)
            {
                if (player && related && siblings && entry->HasAttribute(SPELL_ATTR_EX3_DOT_STACKING_RULE))
                    return true;
                return false;
            }
            nonmui = true;
            break;
        }
        case SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT:
            nonmui = true;
            break;
        case SPELL_AURA_MOD_INCREASE_HEALTH:
            break;
        case SPELL_AURA_MOD_HEALING_DONE:
        case SPELL_AURA_MOD_HEALING_PCT:
            // Do not stack similar debuffs: Mortal Strike, Aimed Shot, Hex of Weakness (致死打击、瞄准射击、虚弱妖术等致死减疗效果互斥)
            if (!positive)
                return (entry->EffectMiscValue[effIdx] == entry2->EffectMiscValue[similar]);
            break;
        case SPELL_AURA_MOD_MELEE_HASTE:
        case SPELL_AURA_MOD_RANGED_HASTE:
        case SPELL_AURA_MOD_DAMAGE_DONE:
        case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE: // Ferocious Inspiration, Shadow Embrace (凶猛灵感、暗影之拥等正面增伤可叠，负面互斥)
            if (positive)
                return true;
            nonmui = true;
            break;
        case SPELL_AURA_MOD_SPELL_CRIT_CHANCE:
        case SPELL_AURA_MOD_SPELL_HIT_CHANCE:
            if (player && related && entry->Id == entry2->Id && entry->HasAttribute(SPELL_ATTR_PASSIVE))
                return true; // TBC: Party auras whitelist for Totem of Wrath (天怒图腾等被动小队光环允许叠加)
            nonmui = true;
            break;
        case SPELL_AURA_MOD_DAMAGE_TAKEN:
        case SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN:
        case SPELL_AURA_MOD_DECREASE_SPEED: // Bonus stacking handled by core (移速修正由核心另行处理叠加)
        case SPELL_AURA_MOD_INCREASE_SPEED: // Bonus stacking handled by core
        case SPELL_AURA_MOD_HEALTH_REGEN_PERCENT:
        case SPELL_AURA_PREVENTS_FLEEING:
            nonmui = true;
            break;
        case SPELL_AURA_PROC_TRIGGER_SPELL:
            if (instance && !icon)
                // Exception: Judgement of Light and Judgement of Wisdom have exact same spell family flags
                // Comparing icons is the fastest (but hacky) way to destinguish between two without poking spell chain
                // 例外：光明审判与智慧审判的 SpellFamilyFlags 完全相同，通过图标 ID 区分
                break;
            nonmui = true;
            break;
        case SPELL_AURA_SPELL_MAGNET: // Party auras whitelist for Grounding Totem (根基图腾可共存)
            return true; // Always stacking auras (始终可叠加光环)
            break;
        case SPELL_AURA_MOD_POSSESS: // Mind control derrivatives (控制类)
        case SPELL_AURA_MOD_POSSESS_PET: // Eyes of the beast (野兽之眼)
        case SPELL_AURA_MOD_CHARM: // Temporary Enslave/Tame derrivatives (魅惑/奴役)
        case SPELL_AURA_AOE_CHARM: // Mass charm by Boss/NPC (群体魅惑)
        case SPELL_AURA_DAMAGE_SHIELD: // Damage shields: Fire Shield, Thorns... (反伤护盾：火焰之盾、荆棘术)
        case SPELL_AURA_MOD_SHAPESHIFT: // Forms and stances (变形与姿态)
        case SPELL_AURA_MOUNTED: // Mount (坐骑)
        case SPELL_AURA_EMPATHY: // Beast Lore (野兽知识)
            // TODO: Make these exclusive rather than unstackable physically and move to nonmui:
        case SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK: // Heroism, Bloodlust, Icy Veins, Power Infusion (英勇/嗜血/冰脉/能量灌注互斥)
        case SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK: // Mounted Speed effects (骑乘加速不叠加)
        case SPELL_AURA_HASTE_SPELLS: // Post 2.0 Mind-numbing Poison and Curse of Tongues (麻痹毒药与语言诅咒互斥)
            return false; // Never stacking auras (永不叠加光环)
            break;
        case SPELL_AURA_MOD_DETECT_RANGE: // Never stack (警戒范围修正永不叠加)
            return false;
        case SPELL_AURA_PERIODIC_ENERGIZE:
            break;
    }
    if (nonmui && instance && !IsChanneledSpell(entry) && !IsChanneledSpell(entry2))
        return false; // Forbids multi-ranking and multi-application on rule, exclude channeled spells (like Mind Flay) (禁止同法术多实例/多等级叠加，引导法术如精神鞭笞除外)

    if (multirank && IsPositiveSpell(entry) && IsPositiveSpell(entry2))
        return false; // Forbids multi-ranking for positive spells (禁止同职业正面增益的不同等级同时存在)

    return true;
}

// 逐一校验法术的所有光环效果是否均允许与 entry2 叠加
bool SpellStacker::IsStackableSpell(SpellEntry const* entry, SpellEntry const* entry2, Unit* target) const
{
    for (uint32 i = EFFECT_INDEX_0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (entry->Effect[i] && entry->EffectApplyAuraName[i] && !IsStackableAuraEffect(entry, entry2, SpellEffectIndex(i), target))
            return false;
    }
    return true;
}

// 判断两个法术是否允许叠加（优先检查唯一标记、不可取消标记、主被动差异）
bool SpellStacker::IsSpellStackableWithSpell(const SpellEntry* entry1, const SpellEntry* entry2, Unit* target) const
{
    if (!entry1 || !entry2)
        return true;

    if (entry1 == entry2 && entry1->HasAttribute(SPELL_ATTR_EX_AURA_UNIQUE))
        return false;

    // Uncancellable spells are expected to be persistent at all times (不可取消光环预期持续存在)
    if (entry1->HasAttribute(SPELL_ATTR_NO_AURA_CANCEL) || entry2->HasAttribute(SPELL_ATTR_NO_AURA_CANCEL))
        return true;

    // Allow stacking passive and active spells (允许被动法术与主动法术叠加)
    if (entry1->HasAttribute(SPELL_ATTR_PASSIVE) != entry2->HasAttribute(SPELL_ATTR_PASSIVE))
        return true;

    return IsStackableSpell(entry1, entry2, target);
}

// 判断来自不同施法者的两个法术是否允许叠加
bool SpellStacker::IsSpellStackableWithSpellForDifferentCasters(SpellEntry const* entry1, SpellEntry const* entry2, bool isSameChain, Unit* target) const
{
    if (!entry1 || !entry2)
        return true;

    // If spells are two instances of the same spell, check attribute first, and formal aura holder stacking rules after
    // 若为同一法术，先检查按施法者独立堆叠属性，再回退到常规叠加规则
    if (entry1 == entry2)
        return (entry1->HasAttribute(SPELL_ATTR_EX3_DOT_STACKING_RULE) || entry1->HasAttribute(SPELL_ATTR_EX5_AURA_UNIQUE_PER_CASTER) || IsSpellStackableWithSpell(entry1, entry2, target));

    // If spells are in the same spell chain (若两个法术属于同一技能链的不同等级)
    if (isSameChain)
    {
        // Both ranks have attribute, allow stacking (若双方均标记按施法者独立存在，则允许叠加)
        if (entry1->HasAttribute(SPELL_ATTR_EX3_DOT_STACKING_RULE) && entry2->HasAttribute(SPELL_ATTR_EX3_DOT_STACKING_RULE))
            return true;
    }

    // By default, check formal aura holder stacking rules (默认遵循常规叠加规则)
    return IsSpellStackableWithSpell(entry1, entry2, target);
}

// 查询指定法术所属的法术组掩码数据
SpellGroupSpellData const* SpellStacker::GetSpellGroupDataForSpell(uint32 spellId) const
{
    auto itr = m_spellGroupSpellData.find(spellId);
    if (itr == m_spellGroupSpellData.end())
        return nullptr;
    
    return &(itr->second);
}
