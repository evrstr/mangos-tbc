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

// 27827 - Spirit of Redemption (救赎之魂死亡治疗：治疗量设为施法者最大生命值)
struct SpiritOfRedemptionHeal : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
            spell->SetDamage(spell->GetCaster()->GetMaxHealth());
    }
};

enum
{
    SPELL_PLAYER_CONSUME_MAGIC = 32676,
};

// 32676 - Consume Magic (吞噬魔法：消耗施法者自身一个牧师正面非被动光环以换取法力)
struct ConsumeMagic : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool strict) const override
    {
        if (strict)
        {
            auto holderMap = spell->GetCaster()->GetSpellAuraHolderMap();
            for (auto holderPair : holderMap)
            {
                if (holderPair.second->GetSpellProto())
                {
                    if (holderPair.second->GetSpellProto()->SpellFamilyName == SPELLFAMILY_PRIEST)
                    {
                        if (holderPair.second->IsPositive() && !holderPair.second->IsPassive())
                        {
                            spell->SetScriptValue(holderPair.second->GetId());
                            return SPELL_CAST_OK;
                        }
                    }
                }
            }

            return SPELL_FAILED_NOTHING_TO_DISPEL;
        }
        else
            return SPELL_CAST_OK;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        spell->GetCaster()->RemoveAurasDueToSpell(spell->GetScriptValue());
    }
};

// 10060 - Power Infusion (能量灌注：与奥术强化 12042 互斥，目标已有奥术强化则失败)
struct PowerInfusion : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool/* strict*/) const override
    {
        // Patch 1.10.2 (2006-05-02):
        // Power Infusion: This aura will no longer stack with Arcane Power. If you attempt to cast it on someone with Arcane Power, the spell will fail.
        if (Unit* target = spell->m_targets.getUnitTarget())
            if (target->GetAuraCount(12042))
                return SPELL_FAILED_AURA_BOUNCED;

        return SPELL_CAST_OK;
    }
};

// 32379 - Shadow Word: Death (暗言术：灭：反伤需计入吸收部分但尊重减伤与部分抵抗)
struct ShadowWordDeath : public SpellScript
{
    void OnHit(Spell* spell, SpellMissInfo /*missInfo*/) const override
    {
        // ignores absorb - has to respect stuff like mitigation and partial resist
        int32 swdDamage = spell->GetTotalTargetDamage() + spell->GetTotalTargetAbsorb();
        spell->GetCaster()->CastCustomSpell(nullptr, 32409, &swdDamage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
    }
};

// 15268 - Blackout (昏厥：仅由暗影伤害或周期性伤害触发，治疗及无伤害技能不触发)
struct Blackout : public AuraScript
{
    bool OnCheckProc(Aura* /*aura*/, ProcExecutionData& data) const override
    {
        if (data.isHeal || (!data.damage && data.spellInfo && !IsSpellHaveAura(data.spellInfo, SPELL_AURA_PERIODIC_DAMAGE)))
            return false;
        return true;
    }
};

// 28376 - Shadowguard (暗影守卫：受击时依据自身暗影守卫光环等级对攻击者施放反击暗影伤害)
struct Shadowguard : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        if (!spell->GetTriggeredByAuraSpellInfo())
            return;

        uint32 spellId = 0;
        switch (spell->GetTriggeredByAuraSpellInfo()->Id)
        {
            default:
            case 18137: spellId = 28377; break;   // Rank 1
            case 19308: spellId = 28378; break;   // Rank 2
            case 19309: spellId = 28379; break;   // Rank 3
            case 19310: spellId = 28380; break;   // Rank 4
            case 19311: spellId = 28381; break;   // Rank 5
            case 19312: spellId = 28382; break;   // Rank 6
            case 25477: spellId = 28385; break;   // Rank 7
        }

        if (spellId)
            spell->GetCaster()->CastSpell(spell->GetUnitTarget(), spellId, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
    }
};

enum
{
    MANA_LEECH_PASSIVE = 28305,                             // 暗影魔攻击回蓝被动光环
};

// 34433 - Shadowfiend (暗影魔：召唤后挂上回蓝被动并立即攻击施法指定目标)
struct Shadowfiend : public SpellScript
{
    void OnSummon(Spell* spell, Creature* summon) const override
    {
        summon->CastSpell(summon, MANA_LEECH_PASSIVE, TRIGGERED_OLD_TRIGGERED);
        summon->AI()->AttackStart(spell->m_targets.getUnitTarget());
    }
};

// 33076 - Prayer of Mending (愈合祷言：施法前比对已有愈合祷言治疗量防止覆盖更强版本，并应用治疗加成)
struct PrayerOfMending : public SpellScript
{
    // not needed in wotlk
    SpellCastResult OnCheckCast(Spell* spell, bool strict) const override
    {
        Unit* target = spell->m_targets.getUnitTarget();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;
        if (strict)
        {
            if (Aura* aura = target->GetAura(41635, EFFECT_INDEX_0))
            {
                uint32 value = 0;
                value = spell->CalculateSpellEffectValue(EFFECT_INDEX_0, target, true, false);
                value = spell->GetCaster()->SpellHealingBonusDone(target, sSpellTemplate.LookupEntry<SpellEntry>(41635), EFFECT_INDEX_0, value, HEAL);
                if (aura->GetModifier()->m_amount > (int32)value)
                    return SPELL_FAILED_AURA_BOUNCED;
            }
        }
        return SPELL_CAST_OK;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx != EFFECT_INDEX_0)
            return;
        uint32 value = spell->GetDamage();
        value = spell->GetCaster()->SpellHealingBonusDone(spell->GetUnitTarget(), sSpellTemplate.LookupEntry<SpellEntry>(41635), effIdx, value, HEAL);
        spell->SetDamage(value);
    }
};

enum
{
    SPELL_PAIN_SUPPRESSION_THREAT_REDUCTION = 44416,        // 痛苦压制附带的威胁降低光环
};

// 33206 - Pain Suppression (痛苦压制：生效时附加降低 5% 威胁值的子光环)
struct PainSuppression : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (apply)
            aura->GetTarget()->CastSpell(aura->GetTarget(), SPELL_PAIN_SUPPRESSION_THREAT_REDUCTION, TRIGGERED_OLD_TRIGGERED, nullptr, aura, aura->GetCasterGuid());
    }
};

// 17 - Power Word: Shield (真言术：盾：检查反射护盾天赋并在吸收时按比例反射伤害)
struct PowerWordShieldPriest : public AuraScript
{
    void OnAbsorb(Aura* aura, int32& currentAbsorb, int32& remainingDamage, uint32& reflectedSpellId, int32& reflectDamage, bool& /*preventedDeath*/, bool& /*dropCharge*/, DamageEffectType /*damageType*/) const override
    {
        Unit* caster = aura->GetTarget();
        Unit::AuraList const& vOverRideCS = caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
        for (auto k : vOverRideCS) // 33201 - Reflective Shield (反射护盾天赋)
        {
            switch (k->GetModifier()->m_miscvalue)
            {
                case 5065:                      // Rank 1
                case 5064:                      // Rank 2
                case 5063:                      // Rank 3
                case 5062:                      // Rank 4
                case 5061:                      // Rank 5
                {
                    if (remainingDamage >= currentAbsorb)
                        reflectDamage = k->GetModifier()->m_amount * currentAbsorb / 100;
                    else
                        reflectDamage = k->GetModifier()->m_amount * remainingDamage / 100;
                    reflectedSpellId = 33619;
                } break;
                default: break;
            }
        }
    }
};

// 527, 988 - Dispel Magic (驱散魔法：对友方施放时产生的威胁值系数减半)
struct DispelMagicPriest : public SpellScript
{
    void OnInit(Spell* spell) const override
    {
        spell->SetHelpfulThreatCoefficient(0.5f); // halves db defined threat
    }
};

// 注册牧师职业法术脚本
void LoadPriestScripts()
{
    RegisterSpellScript<ConsumeMagic>("spell_consume_magic");
    RegisterSpellScript<PowerInfusion>("spell_power_infusion");
    RegisterSpellScript<ShadowWordDeath>("spell_shadow_word_death");
    RegisterSpellScript<SpiritOfRedemptionHeal>("spell_spirit_of_redemption_heal");
    RegisterSpellScript<Blackout>("spell_blackout");
    RegisterSpellScript<Shadowguard>("spell_shadowguard");
    RegisterSpellScript<PrayerOfMending>("spell_prayer_of_mending");
    RegisterSpellScript<PainSuppression>("spell_pain_suppression");
    RegisterSpellScript<Shadowfiend>("spell_shadowfiend");
    RegisterSpellScript<PowerWordShieldPriest>("spell_power_word_shield_priest");
    RegisterSpellScript<DispelMagicPriest>("spell_dispel_magic_priest");
}
