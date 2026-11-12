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

// 1130 - Hunter's Mark (猎人印记：强化印记将部分远程攻强转为近战攻强，并在每次受击时叠加印记层数上限至 4 倍)
struct HuntersMark : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        if (data.effIdx == EFFECT_INDEX_2)
        {
            int32 auraValue = 0;
            if (data.aura)
            {
                if (Aura* otherAura = data.aura->GetHolder()->m_auras[EFFECT_INDEX_1])
                    auraValue = otherAura->GetAmount(); // fetch ranged aura AP (读取效果 1 的远程攻强加成)
            }
            else if (data.caster) // or newly calculate it (或实时重算效果 1 数值)
                auraValue = data.caster->CalculateSpellEffectValue(data.target, data.spellProto, EFFECT_INDEX_1, nullptr, true, false);

            if (Unit* caster = data.caster)
            {
                Unit::AuraList const& classScriptAuras = caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                for (auto& aura : classScriptAuras)
                {
                    switch (aura->GetModifier()->m_miscvalue)
                    {
                        case 5236:
                        case 5237:
                        case 5238:
                        case 5239:
                        case 5240:
                            value = aura->GetModifier()->m_amount * auraValue / 100;
                            break;
                    }
                }
            }
        }
        return value;
    }

    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& /*procData*/) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_0) // increases debuff strength on every hit up to 4th (每次命中递增印记攻强，最高叠至基础值的 4 倍)
        {
            int32 basevalue = aura->GetBasePoints();
            aura->GetModifier()->m_amount += basevalue / 10;
            if (aura->GetModifier()->m_amount > basevalue * 4)
                aura->GetModifier()->m_amount = basevalue * 4;
        }
        return SPELL_AURA_PROC_OK;
    }
};

// 1515 - Tame Beast (驯服野兽：目标等级不可高于猎人，施加时附加 1000 初始威胁值)
struct TameBeast : public SpellScript, public AuraScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        Unit* target = spell->m_targets.getUnitTarget();
        if (!target)
            return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
        if (target->GetLevel() > spell->GetCaster()->GetLevel())
            return SPELL_FAILED_HIGHLEVEL;
        return SPELL_CAST_OK;
    }

    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_0 && apply)
            if (Unit* caster = aura->GetCaster()) // Wotlk - sniff - adds 1000 threat (施加时附加 1000 仇恨)
                aura->GetTarget()->AddThreat(caster, 1000.0f, false, GetSpellSchoolMask(aura->GetSpellProto()), aura->GetSpellProto());
    }
};

// 34026 - Kill Command (杀戮命令：清除猎人暴击状态并让宠物对当前目标发动撕咬伤害，T5 2件套触发弱点利用)
struct KillCommand : public SpellScript
{
    void OnHit(Spell* spell, SpellMissInfo /*missInfo*/) const override
    {
        if (spell->GetCaster()->HasAura(37483)) // Improved Kill Command - Item set bonus (套装强化杀戮命令)
            spell->GetCaster()->CastSpell(nullptr, 37482, TRIGGERED_OLD_TRIGGERED);// Exploited Weakness (弱点利用)
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx != EFFECT_INDEX_1 || !spell->GetUnitTarget() || spell->GetCaster()->getClass() != CLASS_HUNTER)
            return;

        // clear hunter crit aura state (消耗猎人暴击光环状态)
        spell->GetCaster()->ModifyAuraState(AURA_STATE_HUNTER_CRIT_STRIKE, false);

        // additional damage from pet to pet target (由宠物对当前攻击目标施放杀戮命令伤害技能)
        Pet* pet = static_cast<Pet*>(spell->GetUnitTarget()); // guaranteed by spell targeting
        if (!pet->GetVictim())
            return;

        uint32 spell_id = 0;
        switch (spell->m_spellInfo->Id)
        {
            case 34026: spell_id = 34027; break;    // rank 1
            default: sLog.outError("KillCommand: Spell %u not handled", spell->m_spellInfo->Id); return;
        }

        pet->CastSpell(pet->GetVictim(), spell_id, TRIGGERED_OLD_TRIGGERED);
        return;
    }
};

// 34477 - Misdirection (误导：目标已存在误导触发光环或处于骑乘状态时禁止施放)
struct Misdirection : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool/* strict*/) const override
    {
        // Patch 2.3.0 (2007-11-13):
        // Misdirection: If a Hunter attempts to use this ability on a target which already has an active Misdirection, the spell will fail to apply due to a more powerful spell already being in effect.
        if (Unit* target = spell->m_targets.getUnitTarget())
        {
            if (target->HasAura(35079))
                return SPELL_FAILED_AURA_BOUNCED;
            if (target->IsMounted())
                return SPELL_FAILED_NOT_ON_MOUNTED;
        }

        return SPELL_CAST_OK;
    }
};

// 34501 - Expose Weakness (破甲虚弱：根据施法者敏捷属性百分比提高攻强加成)
struct ExposeWeakness : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        if (data.caster)
            value = (data.caster->GetStat(STAT_AGILITY) * value) / 100;

        return value;
    }
};

// 34701 - Random Aggro (毒蛇陷阱毒蛇随机索敌脚本)
struct RandomAggroSnakeTrap : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx != EFFECT_INDEX_0)
            return;

        Unit* target = spell->GetUnitTarget();
        Unit* snake = spell->GetCaster();

        if (!target || !snake)
            return;

        if (snake->CanAttack(target) && snake->IsVisibleForOrDetect(target, target, true))
        {
            if (snake->AI())
                snake->AI()->AttackStart(target);
        }
    }
};

// 19572, 19573 - Improved Mend Pet (强化治疗宠物：按天赋几率触发驱散宠物身上的减益效果)
struct ImprovedMendPet : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        if (!roll_chance_i(aura->GetModifier()->m_amount))
            return SPELL_AURA_PROC_FAILED;

        procData.triggeredSpellId = 24406;
        procData.triggerTarget = procData.target;
        return SPELL_AURA_PROC_OK;
    }
};

// TODO: some evidence tbc pet growl scales with hunter AP

// 19678 - Tame Adult Plainstrider (驯兽棒任务光环：应用时跟随玩家，过期未完成则强制消失)
struct TamingPetRodAura : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        Unit* caster = aura->GetCaster();
        Unit* target = aura->GetTarget();

        if (!target->IsCreature())
            return;

        Creature* creature = static_cast<Creature*>(target);

        if (apply)
        {
            if (caster && caster->IsPlayer())
                creature->GetMotionMaster()->MoveFollow(caster, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);   
        }
        else
        {
            if (aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
                creature->ForcedDespawn();     
        }
    }
};

// 注册猎人职业法术脚本
void LoadHunterScripts()
{
    RegisterSpellScript<HuntersMark>("spell_hunters_mark");
    RegisterSpellScript<TameBeast>("spell_tame_beast");
    RegisterSpellScript<KillCommand>("spell_kill_command");
    RegisterSpellScript<Misdirection>("spell_misdirection");
    RegisterSpellScript<ExposeWeakness>("spell_expose_weakness");
    RegisterSpellScript<RandomAggroSnakeTrap>("spell_random_aggro_snake_trap");
    RegisterSpellScript<ImprovedMendPet>("spell_improved_mend_pet");
    RegisterSpellScript<TamingPetRodAura>("spell_taming_pet_rod");
}
