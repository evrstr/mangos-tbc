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
SDName: Boss_Moam
SD%Complete: 100
SDComment:
SDCategory: Ruins of Ahn'Qiraj
EndScriptData */

/**
 * 安其拉废墟：莫阿姆 (Moam) 充能石像首领战斗 AI
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "Spells/Scripts/SpellScript.h"

// 常量与事件枚举
enum
{
    EMOTE_AGGRO              = -1509000,                    // 剧情台词/喊话文本 ID
    EMOTE_MANA_FULL          = -1509001,                    // 剧情台词/喊话文本 ID
    EMOTE_ENERGIZING         = -1509028,                    // 剧情台词/喊话文本 ID

    SPELL_TRAMPLE            = 15550,                       // 技能法术 ID
    SPELL_DOUBLE_ATTACK      = 18941,                       // 技能法术 ID
    SPELL_DRAIN_MANA         = 25676,                       // 技能法术：吸取法力
    SPELL_ARCANE_ERUPTION    = 25672,                       // 技能法术 ID
    SPELL_SUMMON_MANAFIENDS  = 25684,                       // 技能法术 ID
    SPELL_ENERGIZE           = 25685,                       // 技能法术 ID

    SPELL_SUMMON_MANA_FIEND_1 = 25681,                      // 技能法术 ID
    SPELL_SUMMON_MANA_FIEND_2 = 25682,                      // 技能法术 ID
    SPELL_SUMMON_MANA_FIEND_3 = 25683,                      // 技能法术 ID

    NPC_MANA_FIEND           = 15527,                       // NPC 条目 ID
};

// MoamActions 战斗行为动作枚举
enum MoamActions
{
    MOAM_ARCANE_ERUPTION,
    MOAM_MANA_FIENDS,
    MOAM_ENERGIZE,
    MOAM_TRAMPLE,
    MOAM_DOUBLE_ATTACK,
    MOAM_MANA_DRAIN,
    MOAM_ACTION_MAX,                                        // 战斗行为动作
    MOAM_CANCEL_ENERGIZE,
};

// boss_moamAI 首领战斗 AI
struct boss_moamAI : public CombatAI
{
    boss_moamAI(Creature* creature) :
        CombatAI(creature, MOAM_ACTION_MAX),
        m_uiPhase(0)
    {
        AddCombatAction(MOAM_ARCANE_ERUPTION, 0u);
        AddCombatAction(MOAM_TRAMPLE, 9000u);
        AddCombatAction(MOAM_DOUBLE_ATTACK, 8000, 12000);
        AddCombatAction(MOAM_MANA_DRAIN, 6000u);
        AddCombatAction(MOAM_MANA_FIENDS, 90000u);
        AddCombatAction(MOAM_ENERGIZE, 90000u);
        AddCustomAction(MOAM_CANCEL_ENERGIZE, true, [&]()
        {
            m_creature->RemoveAurasDueToSpell(SPELL_ENERGIZE);
        });

        if (m_creature->GetInstanceData())
        {
            m_creature->GetCombatManager().SetLeashingCheck([](Unit* unit, float /*x*/, float /*y*/, float /*z*/)
            {
                return static_cast<ScriptedInstance*>(unit->GetInstanceData())->GetPlayerInMap(true, false) == nullptr;
            });
        }
    }

    uint8 m_uiPhase;
    GuidVector m_summons;

    void Reset() override                                   // 重置战斗状态与计时器
    {
        CombatAI::Reset();
        m_creature->SetPower(POWER_MANA, 0);
        DespawnGuids(m_summons);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(EMOTE_AGGRO, m_creature);
    }

    void JustSummoned(Creature* summoned) override          // 召唤仆从成功回调
    {
        if (summoned->GetEntry() == NPC_MANA_FIEND)
        {
            summoned->SetInCombatWithZone();
            if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
            {
                summoned->AddThreat(target, 100000.f);
                summoned->AI()->AttackStart(target);
            }
            summoned->SetCorpseDelay(2);
        }
        m_summons.push_back(summoned->GetObjectGuid());
    }

    void SummonedCreatureJustDied(Creature* summoned) override
    {
        m_summons.erase(std::remove(m_summons.begin(), m_summons.end(), summoned->GetObjectGuid()), m_summons.end());
        if (m_summons.size() == 0)
            m_creature->RemoveAurasDueToSpell(SPELL_ENERGIZE);
    }

    void ExecuteAction(uint32 action) override              // 执行战斗行为轮换
    {
        switch (action)
        {
            case MOAM_ARCANE_ERUPTION:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_ARCANE_ERUPTION) == CAST_OK)
                {
                    DoScriptText(EMOTE_MANA_FULL, m_creature);
                    ResetCombatAction(action, 5000); // small CD to prevent AI spam (说明)
                }
                break;
            }
            case MOAM_TRAMPLE:
            {
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_TRAMPLE) == CAST_OK)
                    ResetCombatAction(action, 15000);
                break;
            }
            case MOAM_DOUBLE_ATTACK:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_DOUBLE_ATTACK) == CAST_OK)
                    ResetCombatAction(action, urand(8000, 12000));
                break;
            }
            case MOAM_MANA_DRAIN:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_DRAIN_MANA) == CAST_OK)
                    ResetCombatAction(action, 6000);
                break;
            }
            case MOAM_MANA_FIENDS:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_SUMMON_MANAFIENDS) == CAST_OK)
                    ResetCombatAction(action, 90000);
                break;
            }
            case MOAM_ENERGIZE:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_ENERGIZE) == CAST_OK)
                {
                    DoScriptText(EMOTE_ENERGIZING, m_creature);
                    ResetTimer(MOAM_CANCEL_ENERGIZE, 90000);
                    ResetCombatAction(action, 180000);
                }
                break;
            }
        }
    }
};

struct SummonManaFiendsMoam : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        spell->GetCaster()->CastSpell(nullptr, SPELL_SUMMON_MANA_FIEND_1, TRIGGERED_OLD_TRIGGERED);
        spell->GetCaster()->CastSpell(nullptr, SPELL_SUMMON_MANA_FIEND_2, TRIGGERED_OLD_TRIGGERED);
        spell->GetCaster()->CastSpell(nullptr, SPELL_SUMMON_MANA_FIEND_3, TRIGGERED_OLD_TRIGGERED);
    }
};

void AddSC_boss_moam() // 注册脚本到系统管理器
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_moam";
    pNewScript->GetAI = &GetNewAIInstance<boss_moamAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<SummonManaFiendsMoam>("spell_summon_mana_fiends_moam");
}
