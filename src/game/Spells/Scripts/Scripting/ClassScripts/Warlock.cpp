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
 
#include "Spells/Scripts/SpellScript.h"
#include "Spells/SpellAuras.h"
#include "Spells/SpellMgr.h"

enum
{
    SPELL_UNSTABLE_AFFLICTION_SILENCE = 31117,              // 痛苦无常被驱散后的沉默与反噬伤害法术
};

// 暗影之拥减益法术列表与可维持暗影之拥的 DoT 法术列表
static uint32 const shadowEmbraceSpellList[5] = { 32386,32388,32389,32390,32391 };
static uint32 const causesShadowEmbraceSpellList[23] = { 172,6222,6223,7648,11671,11672,25311,27216,18265,18879,18880,18881,27264,30911,27243,980,1014,6217,11711,11712,11713,27218 };

// 当施法者最后一个触发“暗影之拥”的 DoT 从目标身上移除时，同步移除“暗影之拥”光环
void RemoveShadowEmbraceIfNecessary(Aura* aura)
{
    bool hasOtherShadowEmbraceCausingAuras = false;
    Unit::AuraList const& auraPeriodicDmg = aura->GetTarget()->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
    for (Unit::AuraList::const_iterator itr = auraPeriodicDmg.begin(); itr != auraPeriodicDmg.end(); ++itr)
    {
        if ((*itr)->GetCasterGuid() == aura->GetCasterGuid() && std::find(std::begin(causesShadowEmbraceSpellList), std::end(causesShadowEmbraceSpellList), (*itr)->GetId()) != std::end(causesShadowEmbraceSpellList))
        {
            hasOtherShadowEmbraceCausingAuras = true;
            break;
        }
    }

    if (!hasOtherShadowEmbraceCausingAuras) // siphon life (检查生命虹吸等周期吸取类 DoT)
    {
        Unit::AuraList const& auraPeriodicDmg = aura->GetTarget()->GetAurasByType(SPELL_AURA_PERIODIC_LEECH);
        for (Unit::AuraList::const_iterator itr = auraPeriodicDmg.begin(); itr != auraPeriodicDmg.end(); ++itr)
        {
            if ((*itr)->GetCasterGuid() == aura->GetCasterGuid() && std::find(std::begin(causesShadowEmbraceSpellList), std::end(causesShadowEmbraceSpellList), (*itr)->GetId()) != std::end(causesShadowEmbraceSpellList))
            {
                hasOtherShadowEmbraceCausingAuras = true;
                break;
            }
        }
    }

    // if this is the final Shadow Embrace causing DoT being removed, also remove Shadow Embrace if found
    // 若目标不再有来自同一施法者的暗影之拥关联 DoT，则清除暗影之拥
    if (!hasOtherShadowEmbraceCausingAuras)
    {
        uint32 shadowEmbraceSpellId = 0;
        Unit::AuraList const& auraPctDmgDone = aura->GetTarget()->GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        for (Unit::AuraList::const_iterator itr = auraPctDmgDone.begin(); itr != auraPctDmgDone.end(); ++itr)
        {
            if ((*itr)->GetCasterGuid() == aura->GetCasterGuid() && std::find(std::begin(shadowEmbraceSpellList), std::end(shadowEmbraceSpellList), (*itr)->GetId()) != std::end(shadowEmbraceSpellList))
            {
                shadowEmbraceSpellId = (*itr)->GetId();
                break;
            }
        }
        if (shadowEmbraceSpellId)
            aura->GetTarget()->RemoveAurasDueToSpell(shadowEmbraceSpellId);
    }
}

// 30108 - Unstable Affliction (痛苦无常：被驱散时对驱散者造成 9 倍单跳基础伤害并沉默)
struct UnstableAffliction : public AuraScript
{
    void OnDispel(SpellAuraHolder* holder, Unit* dispeller, uint32 /*dispellingSpellId*/, uint32 /*originalStacks*/) const override
    {
        // use clean value for initial damage (使用未受加成污染的原始基础值)
        int32 damage = holder->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_0);
        damage *= 9;

        // backfire damage and silence - confirmed to have original caster (保留原施法者 GUID 以正确触发法伤/爆击等归属)
        dispeller->CastCustomSpell(dispeller, SPELL_UNSTABLE_AFFLICTION_SILENCE, &damage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, holder->GetCasterGuid());
    }
};

// 980 - Curse of Agony (痛苦诅咒：前 4 跳为 0.5 倍、中间 4 跳为 1 倍、后 4 跳为 1.5 倍递增伤害)
struct CurseOfAgony : public AuraScript
{
    void OnPeriodicCalculateAmount(Aura* aura, uint32& amount) const override
    {
        // 1..4 ticks, 1/2 from normal tick damage (第 1~4 跳为标准每跳伤害的一半)
        if (aura->GetAuraTicks() <= 4)
            amount = amount / 2;
        // 9..12 ticks, 3/2 from normal tick damage (第 9~12 跳为标准每跳伤害的 1.5 倍)
        else if (aura->GetAuraTicks() >= 9)
            amount += (amount + 1) / 2; // +1 prevent 0.5 damage possible lost at 1..4 ticks
        // 5..8 ticks have normal tick damage
    }

    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply)
            RemoveShadowEmbraceIfNecessary(aura);
    }
};

// 1122 - Inferno (Summon) (地狱火召唤：短暂定身、施加奴役恶魔、释放地狱火落地晕与献祭光环)
struct InfernoWarlockSummon : public SpellScript
{
    void OnSummon(Spell* spell, Creature* summon) const override
    {
        // Enslave demon effect, without mana cost and cooldown
        summon->CastSpell(nullptr, 22707, TRIGGERED_OLD_TRIGGERED); // short root spell on infernal from sniffs (召唤短暂定身)
        spell->GetCaster()->CastSpell(summon, 20882, TRIGGERED_OLD_TRIGGERED);
        summon->CastSpell(nullptr, 22703, TRIGGERED_NONE); // Inferno effect - in wotlk cast automatically (地狱火落地伤害与眩晕)
        summon->AI()->DoCastSpellIfCan(nullptr, 19483, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        summon->CastSpell(nullptr, 22764, TRIGGERED_NONE); // aggro spell (落地索敌)
    }
};

// 1454 - Life Tap (生命分流：计算自身法伤加成下的血量消耗，按强化生命分流和法力喂养天赋返还法力)
struct LifeTap : public SpellScript
{
    void OnInit(Spell* spell) const override
    {
        float cost = spell->m_currentBasePoints[EFFECT_INDEX_0];

        Unit* caster = spell->GetCaster();
        if (Player* modOwner = caster->GetSpellModOwner())
            modOwner->ApplySpellMod(spell->m_spellInfo->Id, SPELLMOD_COST, cost);

        int32 dmg = caster->SpellDamageBonusDone(caster, SpellSchoolMask(spell->m_spellInfo->SchoolMask), spell->m_spellInfo, EFFECT_INDEX_0, uint32(cost > 0 ? cost : 0), SPELL_DIRECT_DAMAGE);
        dmg = caster->SpellDamageBonusTaken(caster, SpellSchoolMask(spell->m_spellInfo->SchoolMask), spell->m_spellInfo, EFFECT_INDEX_0, dmg, SPELL_DIRECT_DAMAGE);
        spell->SetScriptValue(dmg);
    }

    SpellCastResult OnCheckCast(Spell* spell, bool strict) const override
    {
        if (spell->GetScriptValue() > int32(spell->GetCaster()->GetHealth()))
            return SPELL_FAILED_FIZZLE;

        if (!strict)
        {
            int32 dmg = spell->GetScriptValue();
            if (Aura* aura = spell->GetCaster()->GetAura(28830, EFFECT_INDEX_0))
                dmg += dmg * aura->GetModifier()->m_amount / 100;
            spell->SetPowerCost(dmg);
        }
        return SPELL_CAST_OK;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* caster = spell->GetCaster();
        int32 mana = spell->GetScriptValue();

        Unit::AuraList const& auraDummy = caster->GetAurasByType(SPELL_AURA_DUMMY);
        for (Unit::AuraList::const_iterator itr = auraDummy.begin(); itr != auraDummy.end(); ++itr)
        {
            if ((*itr)->isAffectedOnSpell(spell->m_spellInfo))
            {
                switch ((*itr)->GetSpellProto()->Id)
                {
                    case 28830: // Plagueheart Rainment - reduce hp cost (T3 套装降低生命消耗)
                        break;
                        // Improved Life Tap (强化生命分流提高转化法力值)
                    default: mana = ((*itr)->GetModifier()->m_amount + 100) * mana / 100; break;
                }
            }
        }

        caster->CastCustomSpell(nullptr, 31818, &mana, nullptr, nullptr, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG);

        // Mana Feed (法力喂养天赋：按比例同步为恶魔宠物恢复法力)
        int32 manaFeedVal = caster->CalculateSpellEffectValue(caster, spell->m_spellInfo, EFFECT_INDEX_1);
        manaFeedVal = manaFeedVal * mana / 100;
        if (manaFeedVal > 0 && caster->GetPet())
            caster->CastCustomSpell(nullptr, 32553, &manaFeedVal, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr);            
    }
};

// 5699 - Create Healthstone (制造治疗石：根据“强化治疗石”天赋等级决定生成物品 ID，并检查背包唯一性)
struct CreateHealthStoneWarlock : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        Unit* caster = spell->GetCaster();
        // check if we already have a healthstone
        uint32 itemType = GetUsableHealthStoneItemType(caster, spell->m_spellInfo);
        if (itemType && caster->IsPlayer() && static_cast<Player*>(caster)->GetItemCount(itemType) > 0)
            return SPELL_FAILED_TOO_MANY_OF_ITEM;
        return SPELL_CAST_OK;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        Unit* target = spell->GetUnitTarget();
        if (!target)
            return;

        uint32 itemType = GetUsableHealthStoneItemType(target, spell->m_spellInfo);
        if (itemType)
            spell->DoCreateItem(effIdx, itemType);
    }

    uint32 GetUsableHealthStoneItemType(Unit* target, SpellEntry const* spellInfo) const
    {
        if (!target || !target->IsPlayer())
            return 0;

        uint32 itemtype = 0;
        uint32 rank = 0;
        Unit::AuraList const& mDummyAuras = target->GetAurasByType(SPELL_AURA_DUMMY);
        for (auto mDummyAura : mDummyAuras)
        {
            if (mDummyAura->GetId() == 18692)
            {
                rank = 1;
                break;
            }
            if (mDummyAura->GetId() == 18693)
            {
                rank = 2;
                break;
            }
        }

        // 各等级治疗石对应普通 / 强化 1 级 / 强化 2 级物品 ID 查找表
        static uint32 const itypes[6][3] =
        {
            { 5512, 19004, 19005},              // Minor Healthstone
            { 5511, 19006, 19007},              // Lesser Healthstone
            { 5509, 19008, 19009},              // Healthstone
            { 5510, 19010, 19011},              // Greater Healthstone
            { 9421, 19012, 19013},              // Major Healthstone
            {22103, 22104, 22105}               // Master Healthstone
        };

        switch (spellInfo->Id)
        {
            case  6201:
                itemtype = itypes[0][rank]; break; // Minor Healthstone
            case  6202:
                itemtype = itypes[1][rank]; break; // Lesser Healthstone
            case  5699:
                itemtype = itypes[2][rank]; break; // Healthstone
            case 11729:
                itemtype = itypes[3][rank]; break; // Greater Healthstone
            case 11730:
                itemtype = itypes[4][rank]; break; // Major Healthstone
            case 27230:
                itemtype = itypes[5][rank]; break; // Master Healthstone
        }
        return itemtype;
    }
};

// 35696 - Demonic Knowledge (恶魔知识：根据宠物耐力与智力总和的 4%/8%/12% 提高主人法术伤害)
struct DemonicKnowledge : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        if (!data.aura)
            return value;
        if (Unit* caster = data.caster)
        {
            Unit* target = data.target;
            if (!data.aura->GetScriptValue())
                data.aura->SetScriptValue((target->HasAura(35693) ? 12 : target->HasAura(35692) ? 8 : target->HasAura(35691) ? 4 : 0));
            value = data.aura->GetScriptValue() * (caster->GetStat(STAT_STAMINA) + caster->GetStat(STAT_INTELLECT)) / 100;
        }
        return value;
    }
};

// 25228 - Soul Link (灵魂链接：将第 0 效果修正为提高造成的全伤害百分比)
struct SoulLink : public AuraScript
{
    void OnAuraInit(Aura* aura) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_0)
            aura->GetModifier()->m_auraname = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE;
    }
};

// 18265 - Siphon Life (生命虹吸：光环移除时检查是否需同步清除暗影之拥)
struct SiphonLife : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply)
            RemoveShadowEmbraceIfNecessary(aura);
    }
};

// 172 - Corruption (腐蚀术：光环移除时检查是否需同步清除暗影之拥)
struct Corruption : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply)
            RemoveShadowEmbraceIfNecessary(aura);
    }
};

// 126 - Eye of Kilrogg (Summon) (基尔罗格之眼：召唤后施放视觉/隐形关联被动 2585)
struct EyeOfKilrogg : public SpellScript
{
    void OnSummon(Spell* /*spell*/, Creature* summon) const override
    {
        summon->CastSpell(nullptr, 2585, TRIGGERED_OLD_TRIGGERED);
    }
};

// 603 - Curse of Doom (厄运诅咒：禁止对玩家施放；目标死于该效果时有几率召唤末日守卫)
struct CurseOfDoom : public SpellScript, public AuraScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        // not allow cast at player
        Unit* target = spell->m_targets.getUnitTarget();
        if (!target || target->GetTypeId() == TYPEID_PLAYER)
            return SPELL_FAILED_BAD_TARGETS;
        return SPELL_CAST_OK;
    }

    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply && aura->GetRemoveMode() == AURA_REMOVE_BY_DEATH && urand(0, 100) > 95)
            if (Unit* caster = aura->GetCaster())
                caster->CastSpell(aura->GetTarget(), 18662, TRIGGERED_OLD_TRIGGERED);
    }
};

// 18662 - Curse of Doom Effect (厄运诅咒召唤末日守卫效果：召唤后附加控制/束缚法术)
struct CurseOfDoomEffect : public SpellScript
{
    void OnSummon(Spell* /*spell*/, Creature* summon) const override
    {
        summon->CastSpell(nullptr, 42010, TRIGGERED_OLD_TRIGGERED);
    }
};

// 18788 - Demonic Sacrifice (恶魔牺牲：根据被献祭恶魔宠物的生物条目赋予对应强力增益)
struct DemonicSacrifice : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* caster = spell->GetCaster();
        Unit* target = spell->GetUnitTarget();
        uint32 entry = target->GetEntry();
        uint32 spellId;
        switch (entry)
        {
            case 416: spellId = 18789; break;   // imp (小鬼 -> 15% 火伤)
            case 417: spellId = 18792; break;   // fellhunter (地狱猎犬 -> 每 4 秒回蓝)
            case 1860: spellId = 18790; break;  // void (虚空行者 -> 每 4 秒回血)
            case 1863: spellId = 18791; break;  // succubus (魅魔 -> 15% 暗伤)
            case 17252: spellId = 35701; break; // fellguard (恶魔卫士 -> 10% 暗火伤与回蓝)
            default: sLog.outError("EffectInstaKill: Unhandled creature entry (%u) case.", entry); return;
        }

        caster->CastSpell(nullptr, spellId, TRIGGERED_OLD_TRIGGERED);
    }
};

// 19505 - Devour Magic (吞噬魔法：施法前检查目标身上是否存在可驱散的魔法 Buff/Debuff)
struct DevourMagic : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        Unit* target = spell->m_targets.getUnitTarget();
        Unit* caster = spell->GetCaster();
        if (target && caster)
        {
            auto auras = target->GetSpellAuraHolderMap();
            for (auto itr : auras)
            {
                SpellEntry const* spell = itr.second->GetSpellProto();
                if (itr.second->GetTarget()->GetObjectGuid() != caster->GetObjectGuid() && spell->Dispel == DISPEL_MAGIC && (caster->CanAssist(target) ? !IsPositiveSpell(spell) : IsPositiveSpell(spell)) && !IsPassiveSpell(spell))
                    return SPELL_CAST_OK;
            }
        }
        return SPELL_FAILED_NOTHING_TO_DISPEL;
    }
};

enum
{
    SPELL_SEED_DAMAGE = 27285,                              // 腐蚀之种爆炸 AoE 伤害法术
};

// 27243 - Seed of Corruption (腐蚀之种：目标死亡或累计受到足够伤害时引爆 AoE 并同步检查暗影之拥)
struct SeedOfCorruption : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (apply)
            return;
        if (aura->GetEffIndex() != EFFECT_INDEX_1)
        {
            RemoveShadowEmbraceIfNecessary(aura);
            return;
        }
        if (aura->GetRemoveMode() == AURA_REMOVE_BY_DEATH)
            if (Unit* caster = aura->GetCaster())
                caster->CastSpell(aura->GetTarget(), SPELL_SEED_DAMAGE, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_IGNORE_CASTER_AURA_STATE | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG);
    }

    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_1)
            return SPELL_AURA_PROC_OK;
        Modifier* mod = procData.triggeredByAura->GetModifier();
        // if damage is more than need (累计伤害达到阈值则移除光环并引爆)
        if (mod->m_amount <= (int32)procData.damage)
        {
            // remember guid before aura delete
            ObjectGuid casterGuid = procData.triggeredByAura->GetCasterGuid();

            // Remove aura (before cast for prevent infinite loop handlers) (先移除光环防止无限递归触发)
            procData.victim->RemoveAurasByCasterSpell(procData.triggeredByAura->GetId(), procData.triggeredByAura->GetCasterGuid());

            // Cast finish spell (triggeredByAura already not exist!)
            if (Unit* caster = procData.triggeredByAura->GetCaster())
                caster->CastSpell(procData.victim, SPELL_SEED_DAMAGE, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_IGNORE_CASTER_AURA_STATE | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG);
            return SPELL_AURA_PROC_OK;              // no hidden cooldown
        }

        // Damage counting (扣减剩余伤害阈值)
        mod->m_amount -= procData.damage;
        return SPELL_AURA_PROC_OK;
    }
};

// 27285 - Seed of Corruption (腐蚀之种引爆伤害目标检查)
struct SeedOfCorruptionDamage : public SpellScript
{
    bool OnCheckTarget(const Spell* spell, Unit* target, SpellEffectIndex /*eff*/) const override
    {
        if (target->GetObjectGuid() == spell->m_targets.getUnitTargetGuid()) // in TBC skip target of initial aura
            return true; // in WotLK Seed of Corruption also damages the initial target
        return true;
    }
};

// 30293 - Soul Leech (灵魂榨取：按触发伤害的百分比触发回血)
struct SoulLeech : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        int32 damage = int32(procData.damage * aura->GetAmount() / 100);
        Unit* target = aura->GetTarget();
        target->CastCustomSpell(nullptr, 30294, &damage, nullptr, nullptr, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG);
        return SPELL_AURA_PROC_OK;
    }
};

// 诅咒类 PvP 持续时间递减（玩家对战时限制为 12 秒）
struct CurseDiminishingDuration : public AuraScript
{
    int32 OnDurationCalculate(WorldObject const* caster, Unit const* target, int32 duration) const override
    {
        if (caster->IsControlledByPlayer() && target && target->IsPlayerControlled())
            return 12000;
        return duration;
    }
};

// 17804 - Soul Siphon (灵魂虹吸：按目标身上术士痛苦系效果数量（含堆叠）提高吸取生命伤害)
struct SoulSiphon : public AuraScript
{
    void OnAuraInit(Aura* aura) const override
    {
        aura->SetAffectOverriden();
    }

    // 仅作用于吸取生命（Drain Life）法术 (Issue #3649)
    bool OnAffectCheck(Aura const* /*aura*/, SpellEntry const* spellInfo) const override
    {
        if (!spellInfo)
            return false;

        if (spellInfo->SpellFamilyName != SPELLFAMILY_WARLOCK)
            return false;

        // Drain Life has SpellFamilyFlags 0x8 in SPELLFAMILY_WARLOCK
        return (spellInfo->SpellFamilyFlags & uint64(0x8)) != 0;
    }

    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_1)
            aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_SPELL_DAMAGE_DONE, apply);
    }

    void OnDamageCalculate(Aura* aura, Unit* attacker, Unit* victim, int32& /*advertisedBenefit*/, float& totalMod) const override
    {
        // effect 1 m_amount (最大增伤百分比上限)
        int32 maxPercent = aura->GetModifier()->m_amount;
        // effect 0 m_amount (每个痛苦效果提供的增伤步长)
        int32 stepPercent = attacker->CalculateSpellEffectValue(attacker, aura->GetSpellProto(), EFFECT_INDEX_0);
        // count affliction effects and calc additional damage in percentage (统计目标身上的术士痛苦类减益总层数)
        int32 modPercent = 0;
        Unit::SpellAuraHolderMap const& victimAuras = victim->GetSpellAuraHolderMap();
        for (const auto& victimAura : victimAuras)
        {
            SpellEntry const* m_spell = victimAura.second->GetSpellProto();
            if (m_spell->SpellFamilyName != SPELLFAMILY_WARLOCK || !(m_spell->SpellFamilyFlags & uint64(0x0000871B804CC41A)))
                continue;
            modPercent += stepPercent * victimAura.second->GetStackAmount();
            if (modPercent >= maxPercent)
            {
                modPercent = maxPercent;
                break;
            }
        }
        totalMod *= (modPercent + 100.0f) / 100.0f;
    }
};

// 37384 - Improved Corruption and Immolate (T5 套装：腐蚀术/献祭触发对应增伤减益)
struct ImprovedCorruptionAndImmolate : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* /*aura*/, ProcExecutionData& procData) const override
    {
        if (procData.spell)
        {
            procData.triggerTarget = procData.victim;
            if (procData.spellInfo->SchoolMask & SPELL_SCHOOL_MASK_SHADOW)
                procData.triggeredSpellId = 37401; // corruption
            else
                procData.triggeredSpellId = 37402; // immolate
        }
        return SPELL_AURA_PROC_OK;
    }
};

// 37401, 37402 - Improved Corruption and Immolate (目标受到指定法术易伤加成光环)
struct IncreasedSpellDamageTakenWarlock : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_SPELL_DAMAGE_TAKEN, apply);
    }

    void OnDamageCalculate(Aura* aura, Unit* /*attacker*/, Unit* /*victim*/, int32& /*advertisedBenefit*/, float& totalMod) const override
    {
        totalMod *= (100.0f + aura->GetModifier()->m_amount) / 100.0f;
    }
};

// 注册术士职业法术脚本
void LoadWarlockScripts()
{
    RegisterSpellScript<UnstableAffliction>("spell_unstable_affliction");
    RegisterSpellScript<CurseOfAgony>("spell_curse_of_agony");
    RegisterSpellScript<InfernoWarlockSummon>("spell_inferno_warlock_summon");
    RegisterSpellScript<LifeTap>("spell_life_tap");
    RegisterSpellScript<CreateHealthStoneWarlock>("spell_create_health_stone_warlock");
    RegisterSpellScript<DemonicKnowledge>("spell_demonic_knowledge");
    RegisterSpellScript<SoulLink>("spell_soul_link");
    RegisterSpellScript<SeedOfCorruption>("spell_seed_of_corruption");
    RegisterSpellScript<Corruption>("spell_corruption");
    RegisterSpellScript<SiphonLife>("spell_siphon_life");
    RegisterSpellScript<SoulLeech>("spell_soul_leech");
    RegisterSpellScript<EyeOfKilrogg>("spell_eye_of_kilrogg");
    RegisterSpellScript<DevourMagic>("spell_devour_magic");
    RegisterSpellScript<SeedOfCorruptionDamage>("spell_seed_of_corruption_damage");
    RegisterSpellScript<CurseOfDoom>("spell_curse_of_doom");
    RegisterSpellScript<CurseOfDoomEffect>("spell_curse_of_doom_effect");
    RegisterSpellScript<DemonicSacrifice>("spell_demonic_sacrifice");
    RegisterSpellScript<CurseDiminishingDuration>("spell_curse_diminishing_duration");
    RegisterSpellScript<SoulSiphon>("spell_soul_siphon");
    RegisterSpellScript<ImprovedCorruptionAndImmolate>("spell_improved_corruption_and_immolate");
    RegisterSpellScript<IncreasedSpellDamageTakenWarlock>("spell_increased_spell_damage_taken_dummy");
}
