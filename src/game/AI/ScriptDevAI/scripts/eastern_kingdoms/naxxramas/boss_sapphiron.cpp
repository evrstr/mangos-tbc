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
SDName: Boss_Sapphiron
SD%Complete: 100
SDComment:
SDCategory: Naxxramas
EndScriptData */

// 萨菲隆（Sapphiron）冰霜巨龙首领战 AI（冰霜光环全团减血、冰棺躲避视线与深呼吸冰霜炸弹）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/BossAI.h"
#include "naxxramas.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    EMOTE_BREATH                = 7213,                     // 吐息 表情动作 ID
    EMOTE_GENERIC_ENRAGED       = 2384,                     // genericenraged 表情动作 ID

    // All phases spells
    SPELL_DOUBLE_ATTACK         = 19818,                    // 双重攻击 法术 ID
    SPELL_FROST_AURA            = 28529,            // Periodically triggers 28531
    SPELL_BESERK                = 26662,                    // beserk 法术 ID

    // Ground phase spells
    SPELL_CLEAVE                = 19983,                    // 顺劈斩 法术 ID
    SPELL_TAIL_SWEEP            = 15847,                    // 扫尾横扫 法术 ID
    SPELL_LIFE_DRAIN            = 28542,                    // 生命吸取 法术 ID
    SPELL_SUMMON_BLIZZARD_INIT  = 28560,                    // 召唤暴风雪init 法术 ID
    SPELL_SUMMON_BLIZZARD       = 28561,                    // 召唤暴风雪 法术 ID

    // Air phase spells
    SPELL_DRAGON_HOVER          = 18430,                    // 巨龙hover 法术 ID
    SPELL_ICEBOLT_INIT          = 28526,            // Triggers spell 28522 (Icebolt) (Triggers法术28522寒冰箭)
    SPELL_ICEBOLT               = 28522,                    // 寒冰箭 法术 ID
    SPELL_ICEBOLT_IMMUNITY      = 31800,                    // 寒冰箭immunity 法术 ID
    SPELL_ICEBLOCK_SUMMON       = 28535,                    // iceblock召唤 法术 ID
    SPELL_FROST_BREATH_DUMMY    = 30101,                    // 冰霜吐息dummy 法术 ID
    SPELL_FROST_BREATH          = 28524,            // Triggers spells 29318 (Frost Breath) and 30132 (Despawn Ice Block)
    SPELL_DESPAWN_ICEBLOCK_GO   = 28523,                    // 消除iceblock游戏对象 法术 ID
    SPELL_SUMMON_WING_BUFFET    = 29329,                    // 召唤翼击震击 法术 ID
    SPELL_DESPAWN_WING_BUFFET   = 29330,            // Triggers spell 29336 (Despawn Buffet) (Triggers法术29336消除震击)
    SPELL_DESPAWN_BUFFET_EFFECT = 29336,                    // 消除震击效果 法术 ID

    NPC_BLIZZARD                = 16474,                    // 暴风雪 Entry ID

    SPELLSET_GROUND             = 1598901,                  // ground 标识 ID
    SPELLSET_AIR                = 1598902,                  // air 标识 ID
    SPELLSET_NULL               = 1598903,                  // null 标识 ID
};

static const float aLiftOffPosition[3] = {3522.386f, -5236.784f, 137.709f};

// 枚举定义：法术技能、生物与环境事件标识
enum SapphironPhases
{
    PHASE_GROUND        = 1,                                // ground 阶段 ID
    PHASE_LIFT_OFF      = 2,                                // liftoff 阶段 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum SapphironActions
{
    SAPPHIRON_AIR_PHASE,
    SAPPHIRON_LANDING_PHASE,
    SAPPHIRON_GROUND_PHASE,
    SAPPHIRON_BERSERK,
    SAPPHIRON_ACTION_MAX,
};

// 首领战 AI：标识 ID（sapphiron）
struct boss_sapphironAI : public BossAI
{
    boss_sapphironAI(Creature* creature) : BossAI(creature, SAPPHIRON_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())),
        m_iceboltCount(0), m_phase(PHASE_GROUND)
    {
        SetDataType(TYPE_SAPPHIRON);
        AddCombatAction(SAPPHIRON_BERSERK, 15u * MINUTE * IN_MILLISECONDS);
        AddCombatAction(SAPPHIRON_AIR_PHASE, 46s);
        AddCustomAction(SAPPHIRON_LANDING_PHASE, true, [&]() { HandleLandingPhase(); }, TIMER_COMBAT_COMBAT);
        AddCustomAction(SAPPHIRON_GROUND_PHASE, true, [&]() { HandleGroundPhase(); }, TIMER_COMBAT_COMBAT);
    }

    ScriptedInstance* m_instance;

    uint32 m_iceboltCount;
    SapphironPhases m_phase;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        BossAI:: Reset();

        m_iceboltCount = 0;
        m_phase = PHASE_GROUND;

        SetCombatMovement(true);
        SetDeathPrevention(false);
        SetMeleeEnabled(true);
        m_creature->SetHover(false);

        DoCastSpellIfCan(nullptr, SPELL_DOUBLE_ATTACK, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    uint32 GetSubsequentActionTimer(uint32 action)
    {
        switch (action)
        {
            case SAPPHIRON_BERSERK: return 300u * IN_MILLISECONDS;
            case SAPPHIRON_AIR_PHASE: return 46u * IN_MILLISECONDS;
            default: return 0;
        }
    }

    // 进入战斗回调：记录开战状态并启动技能循环
    void Aggro(Unit* who) override
    {
        BossAI::Aggro(who);
        DoCastSpellIfCan(nullptr, SPELL_FROST_AURA, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        if (summoned->GetEntry() == NPC_BLIZZARD)
        {
            if (Unit* enemy = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                summoned->AI()->AttackStart(enemy);
        }
    }

    void MovementInform(uint32 type, uint32 /*pointId*/) override
    {
        if (type == POINT_MOTION_TYPE && m_phase == PHASE_LIFT_OFF)
        {
            // Summon the Wing Buffet NPC and cast the triggered aura to despawn it (召唤 the Wing Buffet NPC and cast the triggered aura to despawn it)
            if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_WING_BUFFET) == CAST_OK)
                DoCastSpellIfCan(m_creature, SPELL_DESPAWN_WING_BUFFET);

            // Actual take off
            m_creature->HandleEmote(EMOTE_ONESHOT_LIFTOFF);
            m_creature->SetHover(true);
            m_creature->CastSpell(nullptr, SPELL_DRAGON_HOVER, TRIGGERED_OLD_TRIGGERED);
            m_creature->SetSpellList(SPELLSET_AIR);
        }
    }

    void HandleLandingPhase()
    {
        m_creature->HandleEmote(EMOTE_ONESHOT_LAND);
        ResetTimer(SAPPHIRON_GROUND_PHASE, 2u * IN_MILLISECONDS);
    }

    void HandleGroundPhase()
    {
        m_phase = PHASE_GROUND;
        m_creature->SetSpellList(SPELLSET_GROUND);
        SetDeathPrevention(false);
        SetCombatMovement(true);
        SetMeleeEnabled(true);
        StopTargeting(false);
        m_creature->RemoveAurasDueToSpell(SPELL_DRAGON_HOVER);
        m_creature->SetHover(false);
        m_creature->GetMotionMaster()->Clear(false);
        m_creature->GetMotionMaster()->MoveChase(m_creature->GetVictim());

        ResetCombatAction(SAPPHIRON_AIR_PHASE, GetSubsequentActionTimer(SAPPHIRON_AIR_PHASE));
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        if (spellInfo->Id == SPELL_FROST_BREATH)
        {
            DoCastSpellIfCan(nullptr, SPELL_FROST_BREATH_DUMMY, CAST_TRIGGERED);
            DoBroadcastText(EMOTE_BREATH, m_creature);
        }
    }

    void ExecuteAction(uint32 action) override
    {
        switch(action)
        {
            case SAPPHIRON_BERSERK:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_BESERK) == CAST_OK)
                {
                    DoBroadcastText(EMOTE_GENERIC_ENRAGED, m_creature);
                    DisableCombatAction(action);
                }
                return;
            }
            case SAPPHIRON_AIR_PHASE:
            {
                if (m_creature->GetHealthPercent() > 10.f)
                {
                    m_phase = PHASE_LIFT_OFF;
                    SetDeathPrevention(true);
                    m_creature->InterruptNonMeleeSpells(false);
                    SetCombatMovement(false);
                    SetMeleeEnabled(false);
                    StopTargeting(true);
                    m_creature->GetMotionMaster()->MovePoint(1, aLiftOffPosition[0], aLiftOffPosition[1], aLiftOffPosition[2]);
                    DisableCombatAction(action);
                }
                break;
            }
        }
    }
};

bool GOUse_go_sapphiron_birth(Player* /*player*/, GameObject* go)
{
    ScriptedInstance* instance = (ScriptedInstance*)go->GetInstanceData();

    if (!instance)
        return true;

    if (instance->GetData(TYPE_SAPPHIRON) != NOT_STARTED)
        return true;

    // If already summoned return (safety check)
    if (instance->GetSingleCreatureFromStorage(NPC_SAPPHIRON, true))
        return true;

    // Set data to special and allow the Go animation to proceed
    instance->SetData(TYPE_SAPPHIRON, SPECIAL);
    return false;
}

// 28522 - Icebolt (28522 - 寒冰箭)
struct IceBolt : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /* effIdx */) const override
    {
        if (Unit* caster = spell->GetCaster())
        {
            if (Unit* target = caster->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_ICEBOLT, SELECT_FLAG_PLAYER | SELECT_FLAG_NOT_AURA))
                caster->CastSpell(target, SPELL_ICEBOLT, TRIGGERED_NONE); // Icebolt (寒冰箭)

            boss_sapphironAI* boss_ai = dynamic_cast<boss_sapphironAI*>(caster->AI());
            if (!boss_ai)
                return;

            if (boss_ai->m_iceboltCount >= 5u)
            {
                caster->CastSpell(nullptr, SPELL_FROST_BREATH_DUMMY, TRIGGERED_IGNORE_COOLDOWNS | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_IGNORE_GCD);
                caster->CastSpell(nullptr, SPELL_FROST_BREATH, TRIGGERED_IGNORE_COOLDOWNS | TRIGGERED_IGNORE_GCD);
                DoBroadcastText(EMOTE_BREATH, caster);
                static_cast<Creature*>(caster)->SetSpellList(SPELLSET_NULL);
                boss_ai->ResetCombatAction(SAPPHIRON_LANDING_PHASE, 10s);
            }

            boss_ai->m_iceboltCount += 1;
        }
    }
};

// 28526 - Icebolt (28526 - 寒冰箭)
struct PeriodicIceBolt : public AuraScript
{
    void OnPeriodicTrigger(Aura* aura, PeriodicTriggerData& data) const override
    {
        if (Unit* target =  aura->GetTarget())
        {
            if (target->IsAlive() && !target->HasAura(SPELL_ICEBOLT_IMMUNITY))
            {
                target->CastSpell(target, SPELL_ICEBOLT_IMMUNITY, TRIGGERED_OLD_TRIGGERED);     // Icebolt which causes immunity to frost dmg
                data.spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(SPELL_ICEBLOCK_SUMMON); // Summon Ice Block (召唤 Ice Block)
            }
        }
    }
};

// 28560 - Summon Blizzard (28560 - 召唤 暴风雪)
struct SummonBlizzard : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /* effIdx */) const override
    {
        if (Unit* unitTarget = spell->GetUnitTarget())
            unitTarget->CastSpell(unitTarget, SPELL_SUMMON_BLIZZARD, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, spell->GetCaster()->GetObjectGuid());
    }
};

// 30132 - Despawn Ice Block (30132 - 消除 Ice Block)
struct DespawnIceBlock : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /* effIdx */) const override
    {
        if (Unit* unitTarget = spell->GetUnitTarget())
        {
            if (unitTarget->GetTypeId() == TYPEID_PLAYER)
            {
                unitTarget->RemoveAurasDueToSpell(SPELL_ICEBOLT_IMMUNITY);                          // Icebolt immunity spell (寒冰箭immunity法术)
                unitTarget->RemoveAurasDueToSpell(SPELL_ICEBOLT);                                   // Icebolt stun/damage spell (寒冰箭stun/damage法术)
                unitTarget->CastSpell(nullptr, SPELL_DESPAWN_ICEBLOCK_GO, TRIGGERED_OLD_TRIGGERED); // Despawn Ice Block (targets Ice Block GOs)
            }
        }
    }
};

// 29330 - Sapphiron's Wing Buffet Despawn (29330 - 萨菲隆's 翼击 震击 消除)
struct SapphironsWingBuffetDespawn : public AuraScript
{
    void OnPeriodicTrigger(Aura* aura, PeriodicTriggerData& data) const override
    {
        if (Unit* target =  aura->GetTarget())
            data.spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(SPELL_DESPAWN_BUFFET_EFFECT); // Despawn Ice Block
    }
};

// 29336 - Despawn Buffet (29336 - 消除 震击)
struct DespawnBuffet : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* target = spell->GetUnitTarget();
        if (target && target->IsCreature())
            static_cast<Creature*>(target)->ForcedDespawn();
    }
};

// 注册并载入脚本模块：boss_sapphiron
void AddSC_boss_sapphiron()
{
    Script* newScript = new Script;
    newScript->Name = "boss_sapphiron";
    newScript->GetAI = &GetNewAIInstance<boss_sapphironAI>;
    newScript->RegisterSelf();

    newScript = new Script;
    newScript->Name = "go_sapphiron_birth";
    newScript->pGOUse = &GOUse_go_sapphiron_birth;
    newScript->RegisterSelf();

    RegisterSpellScript<PeriodicIceBolt>("spell_sapphiron_icebolt_aura");
    RegisterSpellScript<IceBolt>("spell_sapphiron_icebolt");
    RegisterSpellScript<SummonBlizzard>("spell_sapphiron_blizzard");
    RegisterSpellScript<DespawnIceBlock>("spell_sapphiron_iceblock");
    RegisterSpellScript<SapphironsWingBuffetDespawn>("spell_sapphiron_despawn_buffet");
    RegisterSpellScript<DespawnBuffet>("spell_despawn_buffet");
}
