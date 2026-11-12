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

// 14185 - Preparation (伺机待发：立即重置闪避、疾跑、消失、冷血、暗影步与预谋的冷却时间)
struct Preparation : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        if (spell->GetCaster()->IsPlayer())
        {
            // immediately finishes the cooldown on certain Rogue abilities
            auto cdCheck = [](SpellEntry const & spellEntry) -> bool { return (spellEntry.SpellFamilyName == SPELLFAMILY_ROGUE && (spellEntry.SpellFamilyFlags & uint64(0x0000026000000860))); };
            static_cast<Player*>(spell->GetCaster())->RemoveSomeCooldown(cdCheck);
        }
    }
};

enum
{
    SPELL_DISTRACT      = 1725,                             // 扰乱
    SPELL_EARTHBIND     = 3600,                             // 地缚图腾减速脉冲
    SPELL_MASS_DISPEL   = 39897,                            // 群体驱散
    SPELL_MASS_DISPEL_2 = 32592,                            // 群体驱散（变体）
};

// Warning: Also currently used by Prowl (警告：同时用于德鲁伊潜行 Prowl)
// 1784 - Stealth, 5215 - Prowl (潜行与潜伏：地缚图腾和群体驱散不会打破潜行)
struct Stealth : public AuraScript
{
    bool OnCheckProc(Aura* /*aura*/, ProcExecutionData& data) const override // per 1.12.0 patch notes - no other indication of how it works
    {
        if (data.spell)
        {
            switch (data.spell->m_spellInfo->Id)
            {
                case SPELL_EARTHBIND:
                case SPELL_MASS_DISPEL:
                case SPELL_MASS_DISPEL_2:
                    return false;
            }
        }
        return true;
    }
};

// 施放已学习的最高等级潜行并清除其冷却时间
void CastHighestStealthRank(Unit* caster)
{
    if (!caster->IsPlayer())
        return;

    // get highest rank of the Stealth spell
    uint32 spellId = static_cast<Player*>(caster)->LookupHighestLearnedRank(1784);
    if (!spellId)
        return;
    SpellEntry const* stealthSpellEntry = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    // no Stealth spell found
    if (!stealthSpellEntry)
        return;

    // reset cooldown on it if needed (若潜行在冷却中则先移除冷却)
    if (!caster->IsSpellReady(*stealthSpellEntry))
        caster->RemoveSpellCooldown(*stealthSpellEntry);

    caster->CastSpell(nullptr, stealthSpellEntry, TRIGGERED_OLD_TRIGGERED);
}

// 1856 - Vanish (消失：强制进入最高等级潜行状态，并处理套装移速加成)
struct VanishRogue : public SpellScript
{
    void OnCast(Spell* spell) const override
    {
        CastHighestStealthRank(spell->GetCaster());
        // meant to be hooked like override scripts but we dont have that functionality yet
        if (spell->GetCaster()->HasAura(23582, EFFECT_INDEX_0)) // amount has trigger chance 100 (T3 套装移动速度触发)
            spell->GetCaster()->CastSpell(nullptr, 23583, TRIGGERED_OLD_TRIGGERED);
    }
};

// 6770 - Sap (闷棍：手动在命中时移除目标的潜行状态)
struct SapRogue : public SpellScript
{
    // SPELL_ATTR_EX3_SUPPRESS_TARGET_PROCS prevents sap to proc stealth normally
    void OnHit(Spell* spell, SpellMissInfo missInfo) const override
    {
        if (missInfo == SPELL_MISS_NONE && spell->GetUnitTarget())
            spell->GetUnitTarget()->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
    }
};

// 13983 - Setup (调整天赋：仅当目标当前正在瞄准/选中触发者时允许触发连击点)
struct SetupRogue : public AuraScript
{
    bool OnCheckProc(Aura* /*aura*/, ProcExecutionData& data) const override
    {
        return data.victim->GetTarget() == data.attacker;
    }
};

// 14082 - Dirty Deeds (卑鄙行径：对生命值低于 35% 的目标按天赋效果 0 数值提高伤害)
struct DirtyDeeds : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_1)
            aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_MELEE_DAMAGE_DONE, apply);
        else if (aura->GetEffIndex() == EFFECT_INDEX_2)
            aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_MELEE_DAMAGE_DONE, apply);
    }

    void OnDamageCalculate(Aura* aura, Unit* /*attacker*/, Unit* victim, int32& /*advertisedBenefit*/, float& totalMod) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_0)
            return;

        if (victim->HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT))
        {
            Aura* eff0 = aura->GetHolder()->m_auras[EFFECT_INDEX_0];
            if (!eff0)
            {
                sLog.outError("Spell structure of DD (%u) changed.", aura->GetId());
                return;
            }

            // effect 0 have expected value but in negative state (效果 0 存的是负值能量消耗减免，取反即对应增伤百分比)
            totalMod *= (-eff0->GetModifier()->m_amount + 100.0f) / 100.0f;
        }
    }
};

// 31228 - Cheat Death (装死天赋：未在内置冷却中时按几率免疫致死，将血量保留至 10% 并触发 31231)
struct CheatDeathRogue : public AuraScript
{
    void OnAbsorb(Aura* aura, int32& currentAbsorb, int32& remainingDamage, uint32& /*reflectedSpellId*/, int32& /*reflectDamage*/, bool& preventedDeath, bool& dropCharge, DamageEffectType /*damageType*/) const override
    {
        if (!preventedDeath && aura->GetTarget()->IsPlayer() &&
            aura->GetHolder()->IsProcReady(aura->GetTarget()->GetMap()->GetCurrentClockTime()) &&
            // Only if no cooldown (冷却就绪且随机判定成功)
            roll_chance_i(aura->GetModifier()->m_amount))
            // Only if roll
        {
            preventedDeath = true;
        }
        // always skip this spell in charge dropping, absorb amount calculation since it has chance as m_amount and doesn't need to absorb any damage
        // 不消耗常规吸收量，仅作致死保护标志判定
        dropCharge = false;
        currentAbsorb = 0; // absorb is only done in death prevention case
        remainingDamage += currentAbsorb;
    }

    void OnAuraDeathPrevention(Aura* aura, int32& remainingDamage) const override
    {
        SpellEntry const* cheatDeath = sSpellTemplate.LookupEntry<SpellEntry>(31231);
        aura->GetTarget()->CastSpell(nullptr, cheatDeath, TRIGGERED_OLD_TRIGGERED);
        aura->GetHolder()->SetProcCooldown(std::chrono::seconds(60), aura->GetTarget()->GetMap()->GetCurrentClockTime());
        // with health > 10% lost health until health==10%, in other case no losses (将剩余伤害截断至刚好保留 10% 最大生命值)
        uint32 health10 = aura->GetTarget()->GetMaxHealth() / 10;
        remainingDamage = aura->GetTarget()->GetHealth() > health10 ? aura->GetTarget()->GetHealth() - health10 : 0;
    }
};

// 45182 - Cheating Death (装死减伤光环：在 3 秒内根据韧性提供最高 90% 全伤害减免)
struct CheatingDeath : public AuraScript
{
    void OnAuraInit(Aura* aura) const override
    {
        aura->SetAffectOverriden();
    }

    void OnApply(Aura* aura, bool apply) const override
    {
        aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_MELEE_DAMAGE_TAKEN, apply);
        aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_SPELL_DAMAGE_TAKEN, apply);
    }

    bool OnAffectCheck(Aura const* aura, SpellEntry const* spellInfo) const override
    {
        if (spellInfo == nullptr)
            return false;

        SpellSchoolMask mask = SPELL_SCHOOL_MASK_NORMAL;
        if (spellInfo)
            mask = GetSpellSchoolMask(spellInfo);
        if (aura->GetModifier()->m_miscvalue & mask)
            return true;

        return false;
    }

    void OnDamageCalculate(Aura* aura, Unit* /*attacker*/, Unit* /*victim*/, int32& /*advertisedBenefit*/, float& totalMod) const override
    {
        if (!aura->GetTarget()->IsPlayer())
            return;

        // 减伤比例受韧性（暴击减免等级）乘以 8 倍影响，封顶 -90%
        float mod = static_cast<Player*>(aura->GetTarget())->GetRatingBonusValue(CR_CRIT_TAKEN_MELEE) * (-8.0f);
        if (mod < float(aura->GetModifier()->m_amount))
            mod = float(aura->GetModifier()->m_amount);

        totalMod *= (mod + 100.0f) / 100.0f;
    }
};

// 16511 - Hemorrhage (出血：命中后额外触发流血/连击关联子法术 34071)
struct Hemorrhage : public SpellScript
{
    void OnAfterHit(Spell* spell) const override
    {
        spell->GetCaster()->CastSpell(spell->GetUnitTarget(), 34071, TRIGGERED_OLD_TRIGGERED);
    }
};

// 注册盗贼职业法术脚本
void LoadRogueScripts()
{
    RegisterSpellScript<Preparation>("spell_preparation");
    RegisterSpellScript<Stealth>("spell_stealth");
    RegisterSpellScript<VanishRogue>("spell_vanish");
    RegisterSpellScript<SapRogue>("spell_sap");
    RegisterSpellScript<SetupRogue>("spell_setup_rogue");
    RegisterSpellScript<DirtyDeeds>("spell_dirty_deeds");
    RegisterSpellScript<CheatDeathRogue>("spell_cheat_death_rogue");
    RegisterSpellScript<CheatingDeath>("spell_cheating_death");
    RegisterSpellScript<Hemorrhage>("spell_hemorrhage");
}