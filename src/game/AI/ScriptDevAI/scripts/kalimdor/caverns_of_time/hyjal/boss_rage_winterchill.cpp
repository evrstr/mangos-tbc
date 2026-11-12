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

/**
 * 海加尔峰战役：雷基·冬寒 (Rage Winterchill) 首领战斗 AI
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "hyjal.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 常量与事件枚举
enum
{
    SAY_ENTER       = -1534040,                             // 剧情台词/喊话文本 ID
    SAY_FROST_NOVA1 = -1534041,                             // 剧情台词/喊话文本 ID
    SAY_FROST_NOVA2 = -1534042,                             // 剧情台词/喊话文本 ID
    SAY_DND1        = -1534043,                             // 剧情台词/喊话文本 ID
    SAY_DND2        = -1534044,                             // 剧情台词/喊话文本 ID
    SAY_KILL1       = 18226,                                // 剧情台词/喊话文本 ID
    SAY_KILL2       = 18227,                                // 剧情台词/喊话文本 ID
    SAY_KILL3       = 18228,                                // 剧情台词/喊话文本 ID
    SAY_DEATH       = -1534048,                             // 剧情台词/喊话文本 ID
    SAY_ENRAGE      = -1000004, // Generic Berserker Rage emote (说明)

    // abilities (首领技能列表)
    SPELL_ICEBOLT           = 31249, // TODO: figure out if DR should apply (说明)
    SPELL_FROST_NOVA        = 31250,                        // 技能法术 ID
    SPELL_DEATH_AND_DECAY   = 31258,                        // 技能法术 ID
    SPELL_FROST_ARMOR       = 31256,                        // 技能法术 ID
    SPELL_ENRAGE            = 26662,                        // 技能法术：激怒/狂暴
};

// WinterchillActions 战斗行为动作枚举
enum WinterchillActions
{
    WINTERCHILL_ACTION_FROST_ARMOR,                         // 战斗行为动作
    WINTERCHILL_ACTION_ICEBOLT,                             // 战斗行为动作
    WINTERCHILL_ACTION_FROST_NOVA,                          // 战斗行为动作
    WINTERCHILL_ACTION_DEATH_AND_DECAY,                     // 战斗行为动作
    WINTERCHILL_ACTION_ENRAGE,                              // 战斗行为动作
    WINTERCHILL_ACTION_MAX,                                 // 战斗行为动作
};

// boss_rage_winterchillAI 首领战斗 AI
struct boss_rage_winterchillAI : public CombatAI
{
    boss_rage_winterchillAI(Creature* creature) : CombatAI(creature, WINTERCHILL_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddCombatAction(WINTERCHILL_ACTION_FROST_ARMOR, GetInitialActionTimer(WINTERCHILL_ACTION_FROST_ARMOR));
        AddCombatAction(WINTERCHILL_ACTION_ICEBOLT, GetInitialActionTimer(WINTERCHILL_ACTION_ICEBOLT));
        AddCombatAction(WINTERCHILL_ACTION_FROST_NOVA, GetInitialActionTimer(WINTERCHILL_ACTION_FROST_NOVA));
        AddCombatAction(WINTERCHILL_ACTION_DEATH_AND_DECAY, GetInitialActionTimer(WINTERCHILL_ACTION_DEATH_AND_DECAY));
        AddCombatAction(WINTERCHILL_ACTION_ENRAGE, GetInitialActionTimer(WINTERCHILL_ACTION_ENRAGE));
        AddOnKillText(SAY_KILL1, SAY_KILL2, SAY_KILL3);
        Reset();
    }

    ScriptedInstance* m_instance;

    uint32 GetInitialActionTimer(const uint32 action) const
    {
        switch (action)
        {
            case WINTERCHILL_ACTION_FROST_ARMOR: return urand(18000, 24000);
            case WINTERCHILL_ACTION_ICEBOLT: return urand(5000, 9000);
            case WINTERCHILL_ACTION_FROST_NOVA: return urand(12000, 17000);
            case WINTERCHILL_ACTION_DEATH_AND_DECAY: return urand(21000, 28000);
            case WINTERCHILL_ACTION_ENRAGE: return 600000;
            default: return 0; // never occurs but for compiler (仅为满足编译器要求，实际不会发生)
        }
    }

    uint32 GetSubsequentActionTimer(const uint32 action) const
    {
        switch (action)
        {
            case WINTERCHILL_ACTION_FROST_ARMOR: return urand(30000, 45000);
            case WINTERCHILL_ACTION_ICEBOLT: return urand(9000, 15000);
            case WINTERCHILL_ACTION_FROST_NOVA: return urand(25000, 30000);
            case WINTERCHILL_ACTION_DEATH_AND_DECAY: return 45000;
            case WINTERCHILL_ACTION_ENRAGE: return 300000;
            default: return 0; // never occurs but for compiler (仅为满足编译器要求，实际不会发生)
        }
    }

    void JustRespawned() override
    {
        ScriptedAI::JustRespawned();
        DoScriptText(SAY_ENTER, m_creature);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
        if (m_instance)
            m_instance->SetData(TYPE_WINTERCHILL, DONE);
    }

    void ExecuteAction(uint32 action) override              // 执行战斗行为轮换
    {
        switch (action)
        {
            case WINTERCHILL_ACTION_FROST_ARMOR:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_FROST_ARMOR) == CAST_OK)
                    ResetCombatAction(action, GetSubsequentActionTimer(action));
                break;
            }
            case WINTERCHILL_ACTION_ICEBOLT:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_ICEBOLT, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, SPELL_ICEBOLT) == CAST_OK)
                        ResetCombatAction(action, GetSubsequentActionTimer(action));
                break;
            }
            case WINTERCHILL_ACTION_FROST_NOVA:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_FROST_NOVA, SELECT_FLAG_PLAYER))
                {
                    if (DoCastSpellIfCan(target, SPELL_FROST_NOVA) == CAST_OK)
                    {
                        DoScriptText(urand(0, 1) ? SAY_FROST_NOVA1 : SAY_FROST_NOVA2, m_creature);
                        ResetCombatAction(action, GetSubsequentActionTimer(action));
                    }
                }
                break;
            }
            case WINTERCHILL_ACTION_DEATH_AND_DECAY:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_DEATH_AND_DECAY, SELECT_FLAG_PLAYER))
                {
                    if (DoCastSpellIfCan(target, SPELL_DEATH_AND_DECAY, CAST_ONLY_XYZ) == CAST_OK)
                    {
                        DoScriptText(urand(0, 1) ? SAY_DND1 : SAY_DND2, m_creature);
                        ResetCombatAction(action, GetSubsequentActionTimer(action));
                    }
                }
                break;
            }
            case WINTERCHILL_ACTION_ENRAGE:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_ENRAGE) == CAST_OK)
                {
                    DoScriptText(SAY_ENRAGE, m_creature);
                    ResetCombatAction(action, GetSubsequentActionTimer(action));
                }
                break;
            }
        }
    }
};

struct DeathAndDecay : public AuraScript
{
    void OnPeriodicCalculateAmount(Aura* aura, uint32& amount) const override
    {
        if (aura->GetTarget()->GetEntry() == 17772) // Only Jaina receives less damage (说明)
            amount = uint32(aura->GetTarget()->GetMaxHealth() * 0.5f / 100);
    }
};

void AddSC_boss_rage_winterchill() // 注册脚本到系统管理器
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_rage_winterchill";
    pNewScript->GetAI = &GetNewAIInstance<boss_rage_winterchillAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<DeathAndDecay>("spell_winterchill_death_and_decay");
}