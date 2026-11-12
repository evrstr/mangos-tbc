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
SDName: Boss_Curator
SD%Complete: 90%
SDComment:
SDCategory: Karazhan
EndScriptData */

// 馆长（The Curator）首领战 AI（召唤奥术畸体与唤醒虚弱易伤阶段）

/* Pre-nerf Changes
Add Astral Armor ability - present whenever Curator is not evocating or enraged.

Patches
Removed sometime prior to 2.1.
*/

#include "AI/ScriptDevAI/include/sc_common.h"
#include "karazhan.h"
#include "Spells/Spell.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO                   = -1532057,                 // 开战 台词广播 ID
    SAY_SUMMON1                 = -1532058,                 // summon1 台词广播 ID
    SAY_SUMMON2                 = -1532059,                 // summon2 台词广播 ID
    SAY_EVOCATE                 = -1532060,                 // evocate 台词广播 ID
    SAY_ENRAGE                  = -1532061,                 // 狂暴 台词广播 ID
    SAY_KILL1                   = 15335,                    // kill1 台词广播 ID
    SAY_KILL2                   = 15336,                    // kill2 台词广播 ID
    SAY_DEATH                   = -1532064,                 // 死亡 台词广播 ID

    // Flare
    NPC_ASTRAL_FLARE_1          = 17096,                    // astralflare1 Entry ID
    NPC_ASTRAL_FLARE_2          = 19781,                    // astralflare2 Entry ID
    NPC_ASTRAL_FLARE_3          = 19782,                    // astralflare3 Entry ID
    NPC_ASTRAL_FLARE_4          = 19783,                    // astralflare4 Entry ID

    SPELL_ASTRAL_FLARE_NE       = 30236,                    // astralflarene 法术 ID
    SPELL_ASTRAL_FLARE_NW       = 30239,                    // astralflarenw 法术 ID
    SPELL_ASTRAL_FLARE_SE       = 30240,                    // astralflarese 法术 ID
    SPELL_ASTRAL_FLARE_SW       = 30241,                    // astralflaresw 法术 ID

    SPELL_ASTRAL_FLARE_PASSIVE  = 30234,                    // astralflarepassive 法术 ID
    SPELL_ASTRAL_FLARE_VISUAL   = 30237,                    // astralflare视觉特效 法术 ID

    // The Curator (The馆长)
    SPELL_HATEFUL_BOLT          = 30383,                    // 仇恨箭 法术 ID
    SPELL_EVOCATION             = 30254,                    // 唤醒 法术 ID
    SPELL_ARCANE_INFUSION       = 30403,                    // 奥术infusion 法术 ID
    SPELL_ARCANE_DECONSTRUCTION = 30407,                    // 奥术deconstruction 法术 ID

    SPELL_ASTRAL_ARMOR          = 29476,                    // astralarmor 法术 ID

    CURATOR_PHASE_1 = 0,                                    // 阶段1 标识 ID
    CURATOR_PHASE_2 = 1,                                    // 阶段2 标识 ID
};

static uint32 astralFlareSpells[]{ SPELL_ASTRAL_FLARE_NE, SPELL_ASTRAL_FLARE_NW, SPELL_ASTRAL_FLARE_SE, SPELL_ASTRAL_FLARE_SW };

// 枚举定义：法术技能、生物与环境事件标识
enum CuratorActions
{
    CURATOR_ACTION_BERSERK,
    CURATOR_ACTION_ARCANE_INFUSION,
    CURATOR_ACTION_FLARE,
    CURATOR_ACTION_HATEFUL_BOLT,
    CURATOR_ACTION_MAX,
};

// 首领战 AI：标识 ID（curator）
struct boss_curatorAI : public CombatAI
{
    boss_curatorAI(Creature* creature) : CombatAI(creature, CURATOR_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddTimerlessCombatAction(CURATOR_ACTION_ARCANE_INFUSION, true);
        AddCombatAction(CURATOR_ACTION_BERSERK, uint32(10 * MINUTE * IN_MILLISECONDS));
        AddCombatAction(CURATOR_ACTION_FLARE, 11000u);
        AddCombatAction(CURATOR_ACTION_HATEFUL_BOLT, 15000u);
        m_creature->GetCombatManager().SetLeashingCheck([&](Unit*, float x, float y, float z)
        {
            return y < -1926.f && z < 164.f && x > -11045.f;
        });
        AddOnKillText(SAY_KILL1, SAY_KILL2);
        Reset();
    }

    ScriptedInstance* m_instance;
    GuidVector m_sparkGuids;
    uint8 m_phase;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();

        m_creature->ApplySpellImmune(nullptr, IMMUNITY_EFFECT, SPELL_EFFECT_POWER_DRAIN, true);
        m_creature->ApplySpellImmune(nullptr, IMMUNITY_EFFECT, SPELL_EFFECT_POWER_BURN, true);
        m_creature->ApplySpellImmune(nullptr, IMMUNITY_STATE, SPELL_AURA_PERIODIC_MANA_LEECH, true);
        m_creature->ApplySpellImmune(nullptr, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_ARCANE, true);

        m_phase = CURATOR_PHASE_1;

        // if (!m_creature->HasAura(SPELL_ASTRAL_ARMOR))
        //     m_creature->CastSpell(m_creature, SPELL_ASTRAL_ARMOR, TRIGGERED_NONE);
    }

    uint32 GetSubsequentActionTimer(uint32 id)
    {
        switch (id)
        {
            case CURATOR_ACTION_FLARE: return 11000;
            case CURATOR_ACTION_HATEFUL_BOLT: return GetActionReadyStatus(CURATOR_ACTION_ARCANE_INFUSION) ? 15000 : 7000;
            default: return 0; // never occurs but for compiler (仅供编译器检查，实际不触发)
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_CURATOR, DONE);

        m_sparkGuids.clear();
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_CURATOR, IN_PROGRESS);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_CURATOR, FAIL);

        for (ObjectGuid guid : m_sparkGuids)
            if (Creature* spark = m_creature->GetMap()->GetCreature(guid))
                spark->ForcedDespawn();

        m_sparkGuids.clear();
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        switch (summoned->GetEntry())
        {
            case NPC_ASTRAL_FLARE_1:
            case NPC_ASTRAL_FLARE_2:
            case NPC_ASTRAL_FLARE_3:
            case NPC_ASTRAL_FLARE_4:
            {
                switch (urand(0, 3))
                {
                    case 0: DoScriptText(SAY_SUMMON1, m_creature); break;
                    case 1: DoScriptText(SAY_SUMMON2, m_creature); break;
                }

                // Flare start with aggro on it's target, should be immune to arcane
                summoned->AI()->DoCastSpellIfCan(summoned, SPELL_ASTRAL_FLARE_PASSIVE, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
                summoned->AI()->DoCastSpellIfCan(summoned, SPELL_ASTRAL_FLARE_VISUAL, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
                summoned->AI()->SetMeleeEnabled(false);

                summoned->SetInCombatWithZone();
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    summoned->AddThreat(pTarget, 1000.f);
                m_sparkGuids.push_back(summoned->GetObjectGuid());
                break;
            }
        }
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case CURATOR_ACTION_BERSERK:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_ARCANE_DECONSTRUCTION) == CAST_OK)
                    DisableCombatAction(action); // once per fight (每场战斗仅触发一次)
                return;
            }
            case CURATOR_ACTION_ARCANE_INFUSION:
            {
                if (m_creature->GetHealthPercent() < 15.0f)
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_ARCANE_INFUSION) == CAST_OK)
                    {
                        DoScriptText(SAY_ENRAGE, m_creature);
                        SetActionReadyStatus(action, false); // once per fight (每场战斗仅触发一次)
                        m_phase = CURATOR_PHASE_2;
                    }
                }
                return;
            }
            case CURATOR_ACTION_FLARE:
            {
                if (m_phase != CURATOR_PHASE_1)
                    return;

                // summon Astral Flare (召唤 Astral Flare)
                uint32 randomSpell = astralFlareSpells[urand(0, 3)];
                DoCastSpellIfCan(nullptr, randomSpell);
                ResetCombatAction(action, GetSubsequentActionTimer(action));
                SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(randomSpell);
                // supposed to happen right away not on next oom
                if (m_creature->GetPower((Powers)spellInfo->powerType) < Spell::CalculatePowerCost(spellInfo, m_creature))
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_EVOCATION) == CAST_OK)
                    {
                        DoScriptText(SAY_EVOCATE, m_creature);
                        // m_creature->RemoveAurasDueToSpell(SPELL_ASTRAL_ARMOR);
                    }
                }
                return;
            }
            case CURATOR_ACTION_HATEFUL_BOLT:
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 1, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(pTarget, SPELL_HATEFUL_BOLT) == CAST_OK)
                        ResetCombatAction(action, GetSubsequentActionTimer(action));
                return;
            }
        }
    }
};

// 注册并载入脚本模块：boss_curator
void AddSC_boss_curator()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_curator";
    pNewScript->GetAI = &GetNewAIInstance<boss_curatorAI>;
    pNewScript->RegisterSelf();
}
