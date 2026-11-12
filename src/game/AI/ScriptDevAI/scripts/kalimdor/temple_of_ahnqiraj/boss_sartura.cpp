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
SDName: Boss_Sartura
SD%Complete: 100
SDComment:
SDCategory: Temple of Ahn'Qiraj
EndScriptData */

/**
 * 安其拉神殿：沙尔图拉 (Battleguard Sartura) 旋风斩与皇家卫兵首领战斗 AI
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "temple_of_ahnqiraj.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "Spells/Scripts/SpellScript.h"

// 常量与事件枚举
enum
{
    SAY_AGGRO                   = -1531008,                 // 剧情台词/喊话文本 ID
    SAY_SLAY                    = 11443,                    // 剧情台词/喊话文本 ID
    SAY_DEATH                   = -1531010,                 // 剧情台词/喊话文本 ID

    EMOTE_FRENZY                = -1000002,                 // 剧情台词/喊话文本 ID
    EMOTE_BERSERK               = -1000004,                 // 剧情台词/喊话文本 ID

    SPELL_ENRAGE                = 8269,                     // 技能法术：激怒/狂暴
    SPELL_SUNDERING_CLEAVE      = 25174,                    // 技能法术：顺劈斩
    SPELL_WHIRLWIND             = 26083,                    // 技能法术：旋风斩
    SPELL_BERSERK               = 27680,                    // 技能法术：狂暴

    SPELL_OTHER_WHIRLWIND_TRIGGER = 26686,                  // 技能法术 ID
};

// SarturaActions 战斗行为动作枚举
enum SarturaActions
{
    SARTURA_ENRAGE,
    SARTURA_WHIRLWIND,
    SARTURA_SUNDERING_CLEAVE,
    SARTURA_BERSERK,
    SARTURA_ACTION_MAX,                                     // 战斗行为动作
};

// boss_sarturaAI 首领战斗 AI
struct boss_sarturaAI : public CombatAI
{
    boss_sarturaAI(Creature* creature) : CombatAI(creature, SARTURA_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddTimerlessCombatAction(SARTURA_ENRAGE, true);
        AddCombatAction(SARTURA_WHIRLWIND, 10000, 20000);
        AddCombatAction(SARTURA_SUNDERING_CLEAVE, 2000, 5000);
        AddCombatAction(SARTURA_BERSERK, uint32(10 * MINUTE * IN_MILLISECONDS));
        AddOnKillText(SAY_SLAY);
        Reset();
    }

    ScriptedInstance* m_instance;

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_SARTURA, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_SARTURA, DONE);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_SARTURA, FAIL);
    }

    void ExecuteAction(uint32 action) override              // 执行战斗行为轮换
    {
        switch (action)
        {
            case SARTURA_ENRAGE:
            {
                if (m_creature->GetHealthPercent() <= 25.0f)
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_ENRAGE) == CAST_OK)
                    {
                        DoScriptText(EMOTE_FRENZY, m_creature);
                        SetActionReadyStatus(action, false);
                    }
                }
                break;
            }
            case SARTURA_WHIRLWIND:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_WHIRLWIND) == CAST_OK)
                    ResetCombatAction(action, urand(20, 25) * IN_MILLISECONDS);
                break;
            }
            case SARTURA_SUNDERING_CLEAVE:
            {
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_SUNDERING_CLEAVE) == CAST_OK)
                    ResetCombatAction(action, urand(2, 5) * IN_MILLISECONDS);
                break;
            }
            case SARTURA_BERSERK:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_BERSERK) == CAST_OK)
                {
                    DoScriptText(EMOTE_BERSERK, m_creature);
                    DisableCombatAction(action);
                }
                break;
            }
        }
    }
};

struct AQWhirlwind : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx != EFFECT_INDEX_1)
            return;

        if (!spell->GetCaster()->CanHaveThreatList() || spell->GetCaster()->getThreatManager().isThreatListEmpty())
            return;

        if (!spell->GetTriggeredByAuraSpellInfo())
            return;

        SpellAuraHolder* holder = spell->GetCaster()->GetSpellAuraHolder(spell->GetTriggeredByAuraSpellInfo()->Id);
        if (holder->m_auras[EFFECT_INDEX_0]->GetAuraTicks() != holder->m_auras[EFFECT_INDEX_0]->GetAuraMaxTicks())
        {
            if (spell->m_spellInfo->Id == SPELL_OTHER_WHIRLWIND_TRIGGER)
                if (urand(0, 2))
                    return;

            spell->GetCaster()->getThreatManager().modifyAllThreatPercent(-100);
            if (Unit* target = static_cast<Creature*>(spell->GetCaster())->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                spell->GetCaster()->getThreatManager().addThreat(target, 100000.f);
        }
        else
            spell->GetCaster()->getThreatManager().modifyAllThreatPercent(-100);
    }
};

void AddSC_boss_sartura() // 注册脚本到系统管理器
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_sartura";
    pNewScript->GetAI = &GetNewAIInstance<boss_sarturaAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<AQWhirlwind>("spell_aq_whirlwind");
}
