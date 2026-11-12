/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
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

/* ScriptData
SDName: Boss_Thaddius
SD%Complete: 100
SDComment:
SDCategory: Naxxramas
EndScriptData */

// 塔迪乌斯（Thaddius）构造区4号首领战 AI（斯塔拉格与费尔根电极跳台、极性转换正负电荷增伤站位）

/* ContentData
boss_thaddius
boss_stalagg
boss_feugen
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/BossAI.h"
#include "naxxramas.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    // Stalagg (斯塔拉格)
    SAY_STAL_AGGRO                  = 13083,                // stal开战 台词广播 ID
    SAY_STAL_SLAY                   = 13085,                // stal击杀 台词广播 ID
    SAY_STAL_DEATH                  = 13084,                // stal死亡 台词广播 ID

    // Feugen
    SAY_FEUG_AGGRO                  = 13023,                // feug开战 台词广播 ID
    SAY_FEUG_SLAY                   = 13025,                // feug击杀 台词广播 ID
    SAY_FEUG_DEATH                  = 13024,                // feug死亡 台词广播 ID

    // Thaddus
    SAY_AGGRO_1                     = 13086,                // 开战1 台词广播 ID
    SAY_AGGRO_2                     = 13087,                // 开战2 台词广播 ID
    SAY_AGGRO_3                     = 13088,                // 开战3 台词广播 ID
    SAY_SLAY                        = 13096,                // 击杀 台词广播 ID
    SAY_ELECT                       = 13090,                // elect 台词广播 ID
    SAY_DEATH                       = 13089,                // 死亡 台词广播 ID
    EMOTE_TESLA_OVERLOAD            = 12178,                // 特斯拉电极overload 表情动作 ID

    // Thaddius Spells (塔迪乌斯Spells)
    SPELL_THADIUS_SPAWN             = 28160,                // thadius生成 法术 ID
    SPELL_THADIUS_LIGHTNING_VISUAL  = 28136,                // thadius闪电视觉特效 法术 ID
    SPELL_BALL_LIGHTNING            = 28299,                // ball闪电 法术 ID
    SPELL_CHAIN_LIGHTNING           = 28167,                // 连环闪电 法术 ID
    SPELL_POLARITY_SHIFT            = 28089,                // 极性转换 法术 ID
    SPELL_POSITIVE_CHARGE           = 28059,                // 正电荷冲锋 法术 ID
    SPELL_NEGATIVE_CHARGE           = 28084,                // 负电荷冲锋 法术 ID
    SPELL_POSITIVE_CHARGE_BUFF      = 29659,                // 正电荷冲锋增益 法术 ID
    SPELL_NEGATIVE_CHARGE_BUFF      = 29660,                // 负电荷冲锋增益 法术 ID
    SPELL_POSITIVE_CHARGE_DAMAGE    = 28062,                // 正电荷冲锋伤害 法术 ID
    SPELL_NEGATIVE_CHARGE_DAMAGE    = 28085,                // 负电荷冲锋伤害 法术 ID
    SPELL_BESERK                    = 27680,                // beserk 法术 ID

    // Stalagg & Feugen Spells
    SPELL_WARSTOMP                  = 28125,                // warstomp 法术 ID
    SPELL_MAGNETIC_PULL_A           = 28338,                // magneticpulla 法术 ID
    SPELL_MAGNETIC_PULL_B           = 28339,                // magneticpullb 法术 ID
    SPELL_STATIC_FIELD              = 28135,                // staticfield 法术 ID
    SPELL_POWERSURGE                = 28134,                // powersurge 法术 ID
//    SPELL_FEUGEN_INITIALIZE         = 28138,      // usage unknown
//    SPELL_STALAGG_PASSIVE           = 28139,      // usage unknown

    // Tesla Spells (特斯拉电极Spells)
    SPELL_FEUGEN_CHAIN              = 28111,                // feugen连环 法术 ID
    SPELL_STALAGG_CHAIN             = 28096,                // 斯塔拉格连环 法术 ID
    SPELL_FEUGEN_TESLA_PASSIVE      = 28109,                // feugen特斯拉电极passive 法术 ID
    SPELL_STALAGG_TESLA_PASSIVE     = 28097,                // 斯塔拉格特斯拉电极passive 法术 ID
    SPELL_SHOCK_OVERLOAD            = 28159,                // 震击overload 法术 ID
//    SPELL_SHOCK                     = 28099,      // Used in SpellEffects.cpp
    SPELL_TRIGGER_TESLAS            = 28359                 // 触发teslas 法术 ID
};

/************
** boss_thaddius
************/

// 枚举定义：法术技能、生物与环境事件标识
enum ThaddiusActions
{
    THADDIUS_ACTION_MAX,
};

// 首领战 AI：标识 ID（thaddius）
struct boss_thaddiusAI : public BossAI
{
    boss_thaddiusAI(Creature* creature) : BossAI(creature, THADDIUS_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        SetDataType(TYPE_THADDIUS);
        AddOnAggroText(SAY_AGGRO_1, SAY_AGGRO_2, SAY_AGGRO_3);
        AddOnKillText(SAY_SLAY);
        AddOnDeathText(SAY_DEATH);
    }

    ScriptedInstance* m_instance;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        BossAI::Reset();
        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
        m_creature->SetImmuneToPlayer(true);
        DoCastSpellIfCan(nullptr, SPELL_THADIUS_SPAWN, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    // 进入战斗回调：记录开战状态并启动技能循环
    void Aggro(Unit* who) override
    {
        BossAI::Aggro(who);
        // Make Attackable
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
        m_creature->SetImmuneToPlayer(false);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        BossAI::JustReachedHome();
        DoCastSpellIfCan(nullptr, SPELL_THADIUS_SPAWN, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    // 死亡回调：更新遭遇战状态与开启门禁
    void JustDied(Unit* killer) override
    {
        BossAI::Aggro(killer);
        if (m_instance)
        {
            // Force Despawn of Adds
            Creature* feugen  = m_instance->GetSingleCreatureFromStorage(NPC_FEUGEN);
            Creature* stalagg = m_instance->GetSingleCreatureFromStorage(NPC_STALAGG);

            if (feugen)
                feugen->ForcedDespawn();
            if (stalagg)
                stalagg->ForcedDespawn();
        }
    }
};

/************
** boss_thaddiusAddsAI - Superclass for Feugen & Stalagg
************/

// The superclass and add specific structure will use Feugen NPC as the master NPC that will initialise and synchronise both adds.
// This design is hinted in Classic spells DBCs

enum ThaddiusAddActions
{
    THADDIUS_ADD_ACTIONS_MAX,
    THADDIUS_ADD_REVIVE,
    THADDIUS_ADD_SHOCK_OVERLOAD,
    THADDIUS_ADD_HOLD,
};

// 首领战 AI：标识 ID（thaddiusAdds）
struct boss_thaddiusAddsAI : public BossAI
{
    boss_thaddiusAddsAI(Creature* creature) : BossAI(creature, THADDIUS_ADD_ACTIONS_MAX), m_instance(dynamic_cast<instance_naxxramas*>(creature->GetInstanceData()))
    {
        SetDataType(TYPE_THADDIUS);
        AddCustomAction(THADDIUS_ADD_REVIVE, true, [&]()
        {
            if (!GetOtherAdd())
                return;
            if (boss_thaddiusAddsAI* otherAI = dynamic_cast<boss_thaddiusAddsAI*>(GetOtherAdd()->AI()))
            {
                if (otherAI->IsCountingDead())
                {
                    otherAI->DisableTimer(THADDIUS_ADD_REVIVE);
                    AddCustomAction(THADDIUS_ADD_SHOCK_OVERLOAD, 14s, [&]()
                    {
                        DoCastSpellIfCan(m_creature, SPELL_TRIGGER_TESLAS, TRIGGERED_OLD_TRIGGERED);
                        DisableTimer(THADDIUS_ADD_SHOCK_OVERLOAD);
                    });
                    DisableTimer(THADDIUS_ADD_REVIVE);
                    return;
                }
                Revive();
                m_isFakingDeath = false;
                DisableTimer(THADDIUS_ADD_REVIVE);
            }
        });
    }

    instance_naxxramas* m_instance;

    bool m_isFakingDeath;
    bool m_areBothDead;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        BossAI::Reset();
        m_isFakingDeath = false;
        m_areBothDead = false;

        // We might Reset while faking death, so undo this
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE | UNIT_FLAG_IMMUNE_TO_PLAYER);
        m_creature->SetHealth(m_creature->GetMaxHealth());
        m_creature->SetStandState(UNIT_STAND_STATE_STAND);
        SetDeathPrevention(true);
        SetCombatScriptStatus(false);
        SetCombatMovement(true);
    }

    // For Stalagg returns feugen, for Feugen returns stalagg
    Creature* GetOtherAdd() const
    {
        switch (m_creature->GetEntry())
        {
            case NPC_FEUGEN:  return m_instance->GetSingleCreatureFromStorage(NPC_STALAGG);
            case NPC_STALAGG: return m_instance->GetSingleCreatureFromStorage(NPC_FEUGEN);
            default: return nullptr;
        }
    }

    // 进入战斗回调：记录开战状态并启动技能循环
    void Aggro(Unit* who) override
    {
        BossAI::Aggro(who);
        if (Creature* otherAdd = GetOtherAdd())
        {
            if (!otherAdd->IsInCombat())
                otherAdd->AI()->AttackStart(who);
        }
    }

    void Revive()
    {
        SetCombatScriptStatus(false);
        DoResetThreat();
        PauseCombatMovement();
        Reset();
    }

    bool IsCountingDead() const
    {
        return m_isFakingDeath || m_creature->IsDead();
    }

    void PauseCombatMovement()
    {
        SetAIImmobilizedState(true);
        AddCustomAction(THADDIUS_ADD_HOLD, 1s + 500ms, [&]()
        {
            SetAIImmobilizedState(false);
            m_creature->GetMotionMaster()->MoveChase(m_creature->GetVictim());
            DisableTimer(THADDIUS_ADD_HOLD);
        });
        ResetIfNotStarted(THADDIUS_ADD_HOLD, 1s + 500ms);
    }

    void JustPreventedDeath(Unit* attacker) override
    {
        if (!m_instance)
            return;
        m_isFakingDeath = true;

        m_creature->InterruptNonMeleeSpells(false);
        m_creature->SetHealth(1);
        m_creature->StopMoving();
        m_creature->ClearComboPointHolders();
        m_creature->RemoveAllAurasOnDeath();
        m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);
        m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, false);
        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE | UNIT_FLAG_IMMUNE_TO_PLAYER);
        m_creature->ClearAllReactives();
        m_creature->GetMotionMaster()->Clear();
        m_creature->GetMotionMaster()->MoveIdle();
        m_creature->SetStandState(UNIT_STAND_STATE_DEAD);
        SetCombatScriptStatus(true);

        JustDied(attacker);                                  // Texts
        ResetTimer(THADDIUS_ADD_REVIVE, 10s);
    }
};

/************
** boss_stalagg
************/

// 首领战 AI：标识 ID（stalagg）
struct boss_stalaggAI : public boss_thaddiusAddsAI
{
    boss_stalaggAI(Creature* creature) : boss_thaddiusAddsAI(creature)
    {
        AddOnKillText(SAY_STAL_SLAY);
        AddOnAggroText(SAY_STAL_AGGRO);
        AddOnDeathText(SAY_STAL_DEATH);
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        if (spellInfo->Id == SPELL_MAGNETIC_PULL_A)
        {
            if (Creature* feugen = GetOtherAdd())
                feugen->CastSpell(nullptr, SPELL_MAGNETIC_PULL_B, TRIGGERED_OLD_TRIGGERED); // meant to be cast as non triggered
        }
    }
};

/************
** boss_feugen
************/

// 首领战 AI：标识 ID（feugen）
struct boss_feugenAI : public boss_thaddiusAddsAI
{
    boss_feugenAI(Creature* creature) : boss_thaddiusAddsAI(creature)
    {
        AddOnKillText(SAY_FEUG_SLAY);
        AddOnAggroText(SAY_FEUG_AGGRO);
        AddOnDeathText(SAY_FEUG_DEATH);
    }
};

// 28159 - Shock (28159 - 震击)
struct ShockThaddius : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            Unit* target = spell->GetUnitTarget();
            // Only do something to Thaddius, and on the first hit.
            if (target->GetEntry() != NPC_THADDIUS || !target->HasAura(SPELL_THADIUS_SPAWN))
                return;
            // remove Stun and then Cast
            target->RemoveAurasDueToSpell(SPELL_THADIUS_SPAWN);
            target->CastSpell(nullptr, SPELL_THADIUS_LIGHTNING_VISUAL, TRIGGERED_OLD_TRIGGERED);
            // Remove visual effect on Tesla GOs
            if (instance_naxxramas* instance = dynamic_cast<instance_naxxramas*>(target->GetInstanceData()))
            {
                if (GameObject* stalaggTesla = instance->GetSingleGameObjectFromStorage(GO_CONS_NOX_TESLA_STALAGG))
                    stalaggTesla->SetGoState(GO_STATE_READY);
                if (GameObject* feugenTesla = instance->GetSingleGameObjectFromStorage(GO_CONS_NOX_TESLA_FEUGEN))
                    feugenTesla->SetGoState(GO_STATE_READY);
            }
        }
    }
};

// 28136 - Thadius Lightning Visual (28136 - Thadius 闪电 视觉特效)
struct ThaddiusLightningVisual : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        Unit* target = spell->GetUnitTarget();
        if (effIdx == EFFECT_INDEX_0 && target->IsCreature())
        {
            target->AI()->SetCombatScriptStatus(false);
            static_cast<Creature*>(target)->SetInCombatWithZone(false);
        }
    }
};

/****************
** Polarity Shift
****************/

// 28089 - Polarity Shift (28089 - 极性 转换)
struct PolarityShift : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx ) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            if (Unit* unitTarget = spell->GetUnitTarget())
            {
                unitTarget->RemoveAurasDueToSpell(SPELL_POSITIVE_CHARGE);
                unitTarget->RemoveAurasDueToSpell(SPELL_NEGATIVE_CHARGE);

                uint64 scriptValue = spell->GetScriptValue();

                // 28059 : Positive Charge, 28084 : Negative Charge
                switch (scriptValue)
                {
                    case 0: // first target random
                        scriptValue = urand(0, 1) ? SPELL_POSITIVE_CHARGE : SPELL_NEGATIVE_CHARGE;
                        spell->SetScriptValue(scriptValue);
                        unitTarget->CastSpell(unitTarget, scriptValue, TRIGGERED_INSTANT_CAST);
                        break;
                    case SPELL_POSITIVE_CHARGE: // second target the other
                        spell->SetScriptValue(1);
                        unitTarget->CastSpell(unitTarget, SPELL_NEGATIVE_CHARGE, TRIGGERED_INSTANT_CAST);
                        break;
                    case SPELL_NEGATIVE_CHARGE:
                        spell->SetScriptValue(1);
                        unitTarget->CastSpell(unitTarget, SPELL_POSITIVE_CHARGE, TRIGGERED_INSTANT_CAST);
                        break;
                    default: // third and later random
                        unitTarget->CastSpell(unitTarget, urand(0, 1) ? SPELL_POSITIVE_CHARGE : SPELL_NEGATIVE_CHARGE, TRIGGERED_INSTANT_CAST);
                        break;
                }
            }
        }
    }
};

// 28062 - Positive Charge (28062 - 正电荷 冲锋)
// 28085 - Negative Charge (28085 - 负电荷 冲锋)
struct ThaddiusChargeDamage : public SpellScript
{
    bool OnCheckTarget(const Spell* spell, Unit* target, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            switch (spell->m_spellInfo->Id)
            {
                case SPELL_POSITIVE_CHARGE_DAMAGE:                // Positive Charge (正电荷冲锋)
                    if (target->HasAura(SPELL_POSITIVE_CHARGE))   // Only deal damage to targets without Positive Charge
                        return false;
                    break;
                case SPELL_NEGATIVE_CHARGE_DAMAGE:                // Negative Charge (负电荷冲锋)
                    if (target->HasAura(SPELL_NEGATIVE_CHARGE))   // Only deal damage to targets without Negative Charge
                        return false;
                    break;
                default:
                    break;
            }
        }
        return true;
    }
};

// 28059 - Positive Charge (28059 - 正电荷 冲锋)
// 28084 - Negative Charge (28084 - 负电荷 冲锋)
struct ThaddiusCharge : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (apply)
        {
            Unit* target = aura->GetTarget();
            if (!target)
                return;

            // On Polarity Shift, remove the previous damage buffs
            uint32 buffAuraIds[2] = { SPELL_POSITIVE_CHARGE_BUFF , SPELL_NEGATIVE_CHARGE_BUFF };
            for (auto buffAura: buffAuraIds)
                target->RemoveAurasDueToSpell(buffAura);
        }
    }

    void OnPeriodicTrigger(Aura* aura, PeriodicTriggerData& /*data*/) const override
    {
        if (Unit* target = aura->GetTarget())
        {

            uint32 buffAuraId = aura->GetId() == SPELL_POSITIVE_CHARGE ? SPELL_POSITIVE_CHARGE_BUFF : SPELL_NEGATIVE_CHARGE_BUFF;
            float range = 13.f; // Static value from DBC files. As the value is the same for both spells we can hardcode it instead of accessing is through sSpellRadiusStore

            uint32 curCount = 0;
            PlayerList playerList;
            GetPlayerListWithEntryInWorld(playerList, target, range);
            for (Player* player : playerList)
                if (target != player && player->HasAura(aura->GetId()))
                    ++curCount;

            // Remove previous buffs in case we have less targets of the same charge near use than in previous tick
            target->RemoveAurasDueToSpell(buffAuraId);
                for (uint32 i = 0; i < curCount; i++)
                    target->CastSpell(target, buffAuraId, TRIGGERED_OLD_TRIGGERED);
        }
    }
};

// 28338, 28339 - Magnetic Pull
struct MagneticPullThaddius : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* target = spell->GetUnitTarget();
        Unit* caster = spell->GetCaster();
        if (target && caster->GetVictim())
            target->CastSpell(caster->GetVictim(), 28337, TRIGGERED_OLD_TRIGGERED); // target cast actual Magnetic Pull on caster's victim
        // ToDo research if target should also get the threat of the caster for caster's victim.
        // This is the case in WotLK version but we have no proof of this in Classic/TBC
        // and it was common at these times to let players manage threat and tank transitions by themselves
    }
};

// 28096 - Stalagg Chain (28096 - 斯塔拉格 连环)
// 28111 - Feugen Chain (28111 - Feugen 连环)
struct FeugenStalaggChain : public AuraScript
{
    void OnPeriodicTrigger(Aura* /*aura*/, PeriodicTriggerData& /*data*/) const override
    {
        
    }
};

// 28098 - Stalagg Tesla Effect (28098 - 斯塔拉格 特斯拉电极 效果)
// 28110 - Feugen Tesla Effect (28110 - Feugen 特斯拉电极 效果)
struct FeugenStalaggTeslaEffect : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* unitTarget = spell->GetUnitTarget();
        Unit* caster = spell->GetCaster();
        if (unitTarget && unitTarget->GetTypeId() == TYPEID_UNIT && unitTarget->IsAlive())
        {
            if (!caster->hasUnitState(UNIT_STAT_ROOT))    // This state is found in sniffs and is probably caused by another aura like 23973
                caster->addUnitState(UNIT_STAT_ROOT);     // but as we are not sure (the aura does not show up in sniffs), we handle the state here

            // Cast chain (Stalagg Chain or Feugen Chain)
            uint32 chainSpellId = spell->m_spellInfo->Id == 28098 ? 28096 : 28111;
            // Only cast if not already present and in range
            if (!unitTarget->HasAura(chainSpellId) && caster->IsWithinDistInMap(unitTarget, 60.0f))
            {
                if (!caster->IsImmuneToPlayer())
                    caster->SetImmuneToPlayer(true);
                caster->CastSpell(unitTarget, chainSpellId, TRIGGERED_OLD_TRIGGERED);
            }
            // Not in range and fight in progress: remove aura and cast Shock onto players
            else if (!caster->IsWithinDistInMap(unitTarget, 60.0f) && caster)
            {
                unitTarget->RemoveAurasDueToSpell(chainSpellId);
                caster->SetImmuneToPlayer(false);

                if (Unit* target = caster->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                    caster->CastSpell(target, 28099, TRIGGERED_NONE);
            }
            // else: in range and already have aura: do nothing
        }
    }
};

// 28359 - Trigger Teslas (28359 - 触发 Teslas)
struct TriggerTeslas : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* unitTarget = spell->GetUnitTarget();
        if (unitTarget)
        {
            DoBroadcastText(EMOTE_TESLA_OVERLOAD, unitTarget, unitTarget);
            unitTarget->RemoveAllAuras();
            unitTarget->CastSpell(nullptr, SPELL_SHOCK_OVERLOAD, TRIGGERED_NONE); // Shock (震击)
        }
    }
};

// 注册并载入脚本模块：boss_thaddius
void AddSC_boss_thaddius()
{
    Script* newScript = new Script;
    newScript->Name = "boss_thaddius";
    newScript->GetAI = &GetNewAIInstance<boss_thaddiusAI>;
    newScript->RegisterSelf();

    newScript = new Script;
    newScript->Name = "boss_stalagg";
    newScript->GetAI = &GetNewAIInstance<boss_stalaggAI>;
    newScript->RegisterSelf();

    newScript = new Script;
    newScript->Name = "boss_feugen";
    newScript->GetAI = &GetNewAIInstance<boss_feugenAI>;
    newScript->RegisterSelf();

    RegisterSpellScript<ShockThaddius>("spell_shock_thaddius");
    RegisterSpellScript<ThaddiusLightningVisual>("spell_thaddius_lightning_visual");
    RegisterSpellScript<PolarityShift>("spell_thaddius_polarity_shift");
    RegisterSpellScript<ThaddiusChargeDamage>("spell_thaddius_charge_damage");
    RegisterSpellScript<ThaddiusCharge>("spell_thaddius_charge_buff");
    RegisterSpellScript<MagneticPullThaddius>("spell_magnetic_pull_thaddius");
    RegisterSpellScript<FeugenStalaggChain>("spell_feugen_stalagg_chain");
    RegisterSpellScript<FeugenStalaggTeslaEffect>("spell_feugen_stalagg_tesla_effect");
    RegisterSpellScript<TriggerTeslas>("spell_trigger_teslas");
}
