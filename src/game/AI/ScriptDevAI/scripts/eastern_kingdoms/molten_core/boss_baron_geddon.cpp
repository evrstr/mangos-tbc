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
SDName: Boss_Baron_Geddon
SD%Complete: 100
SDComment: Armaggedon is not working properly (core issue)
SDCategory: Molten Core
EndScriptData */

// 迦顿男爵（Baron Geddon）5号首领战 AI（活体炸弹、点燃法力与地狱火脉冲）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "molten_core.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    EMOTE_SERVICE               = -1409000,                 // service 表情动作 ID

    SPELL_INFERNO               = 19695,                    // inferno 法术 ID
    SPELL_IGNITE_MANA           = 19659,                    // ignite法力 法术 ID
    SPELL_LIVING_BOMB           = 20475,                    // living炸弹 法术 ID
    SPELL_ARMAGEDDON            = 20478                     // armageddon 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum GeddonActions
{
    GEDDON_ARMAGEDDON,
    GEDDON_INFERNO,
    GEDDON_IGNITE_MANA,
    GEDDON_LIVING_BOMB,
    GEDDON_ACTION_MAX,
};

// 首领战 AI：迦顿男爵（baron_geddon）
struct boss_baron_geddonAI : public CombatAI
{
    boss_baron_geddonAI(Creature* creature) : CombatAI(creature, GEDDON_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddTimerlessCombatAction(GEDDON_ARMAGEDDON, true);
        AddCombatAction(GEDDON_INFERNO, 45000u);
        AddCombatAction(GEDDON_IGNITE_MANA, 30000u);
        AddCombatAction(GEDDON_LIVING_BOMB, 35000u);
        Reset();
    }

    ScriptedInstance* m_instance;

    void Aggro(Unit* /*who*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_GEDDON, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_GEDDON, DONE);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_GEDDON, NOT_STARTED);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case GEDDON_ARMAGEDDON:
            {
                if (m_creature->GetHealthPercent() <= 2.0f)
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_ARMAGEDDON) == CAST_OK)
                    {
                        DoScriptText(EMOTE_SERVICE, m_creature);
                        SetActionReadyStatus(action, false);
                        return;
                    }
                }
                break;
            }
            case GEDDON_INFERNO:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_INFERNO) == CAST_OK)
                    ResetCombatAction(action, 45000);
                break;
            }
            case GEDDON_IGNITE_MANA:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_IGNITE_MANA) == CAST_OK)
                    ResetCombatAction(action, 30000);
                break;
            }
            case GEDDON_LIVING_BOMB:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_LIVING_BOMB, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, SPELL_LIVING_BOMB) == CAST_OK)
                        ResetCombatAction(action, 35000);
                break;
            }
        }
    }
};

// 注册并载入脚本模块：boss_baron_geddon
void AddSC_boss_baron_geddon()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_baron_geddon";
    pNewScript->GetAI = &GetNewAIInstance<boss_baron_geddonAI>;
    pNewScript->RegisterSelf();
}
