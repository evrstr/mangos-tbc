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
SDName: Boss_Azuregos
SD%Complete: 100
SDComment:
SDCategory: Azshara
EndScriptData
*/

/**
 * 艾萨拉世界首领：艾索雷葛斯 (Azuregos) 战斗 AI
 */

#include "AI/ScriptDevAI/base/CombatAI.h"
#include "AI/ScriptDevAI/include/sc_common.h"

// 艾索雷葛斯法术与台词枚举
enum
{
    SAY_TELEPORT                = -1000100,                 // 传送台词

    SPELL_ARCANE_VACUUM         = 21147,                    // 奥术真空（将玩家拉至身边并清空仇恨）
    SPELL_MARK_OF_FROST_PLAYER  = 23182,                    // 冰霜印记（玩家阵亡时施加）
    SPELL_MARK_OF_FROST_AURA    = 23184,                    // Triggers 23186 on players that have 23182; unfortunatelly 23183 is missing from dbc (冰霜印记触发光环：使复活的带有印记玩家昏迷)
    SPELL_MANA_STORM            = 21097,                    // 法力风暴
    SPELL_CHILL                 = 21098,                    // 冰冻（减速）
    SPELL_FROST_BREATH          = 21099,                    // 冰霜吐息
    SPELL_REFLECT               = 22067,                    // 魔法反射
    SPELL_CLEAVE                = 19983,                    // Was 8255; this one is from wowhead and seems to be the correct one (顺劈斩)
    SPELL_ENRAGE                = 23537,                    // 激怒（生命值低于25%）
};

// 战斗行为枚举
enum AzuregosActions
{
    AZUREGOS_MANASTORM,                                     // 施放法力风暴
    AZUREGOS_CHILL,                                         // 施放冰冻
    AZUREGOS_FROSTBREATH,                                   // 施放冰霜吐息
    AZUREGOS_TELEPORT,                                      // 奥术真空传送
    AZUREGOS_REFLECTMAGIC,                                  // 施放法术反射
    AZUREGOS_CLEAVE,                                        // 顺劈斩
    AZUREGOS_ENRAGE_HP_CHECK,                               // 激怒血量检测
    AZUREGOS_ACTION_MAX,
};

// 艾索雷葛斯首领 AI
struct boss_azuregosAI : public CombatAI
{
    boss_azuregosAI(Creature* creature) : CombatAI(creature, AZUREGOS_ACTION_MAX)
    {
        AddTimerlessCombatAction(AZUREGOS_ENRAGE_HP_CHECK, true);         // Soft enrage at 25% (生命值低于25%时软狂暴)
        AddCombatAction(AZUREGOS_MANASTORM, 5u * IN_MILLISECONDS, 17u * IN_MILLISECONDS);
        AddCombatAction(AZUREGOS_CHILL, 10u * IN_MILLISECONDS, 30u * IN_MILLISECONDS);
        AddCombatAction(AZUREGOS_FROSTBREATH, 2u * IN_MILLISECONDS, 8u * IN_MILLISECONDS);
        AddCombatAction(AZUREGOS_TELEPORT, 30u * IN_MILLISECONDS);
        AddCombatAction(AZUREGOS_REFLECTMAGIC, 15u * IN_MILLISECONDS, 30u * IN_MILLISECONDS);
        AddCombatAction(AZUREGOS_CLEAVE, 7u * IN_MILLISECONDS);
    }

    void Reset() override                                   // 重置战斗状态
    {
        CombatAI::Reset();
    }

    void KilledUnit(Unit* victim) override                  // 击杀单位回调
    {
        // Mark killed players with Mark of Frost (对被杀死的玩家施加冰霜印记)
        if (victim->GetTypeId() == TYPEID_PLAYER)
            victim->CastSpell(victim, SPELL_MARK_OF_FROST_PLAYER, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, m_creature->GetObjectGuid());
    }

    void Aggro(Unit* /*who*/) override                      // 进入战斗回调
    {
        // Boss aura which triggers the stun effect on dead players who resurrect (首领光环：在复活带有印记的玩家时触发昏迷)
        DoCastSpellIfCan(m_creature, SPELL_MARK_OF_FROST_AURA);

        m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
    }

    void EnterEvadeMode () override                         // 脱战回位
    {
        m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

        CombatAI::EnterEvadeMode();
    }

    void ExecuteAction(uint32 action) override              // 执行战斗行为
    {
        switch (action)
        {
            case AZUREGOS_ENRAGE_HP_CHECK:                  // 检测生命值是否低于25%并施放激怒
            {
                if (m_creature->GetHealthPercent() < 25.0f)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_ENRAGE) == CAST_OK)
                        DisableCombatAction(action);
                }
                break;
            }
            case AZUREGOS_MANASTORM:                        // 对最近的目标施放法力风暴
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_NEAREST_BY, 0))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_MANA_STORM) == CAST_OK)
                        ResetCombatAction(action, urand(18, 35) * IN_MILLISECONDS);
                }
                break;
            }
            case AZUREGOS_TELEPORT:                         // 施放奥术真空并喊话
            {
                if (DoCastSpellIfCan(m_creature, SPELL_ARCANE_VACUUM) == CAST_OK)
                {
                    DoScriptText(SAY_TELEPORT, m_creature);
                    ResetCombatAction(action, urand(20, 30) * IN_MILLISECONDS);
                }
                break;
            }
            case AZUREGOS_CHILL:                            // 施放冰冻
            {
                if (DoCastSpellIfCan(m_creature, SPELL_CHILL) == CAST_OK)
                    ResetCombatAction(action, urand(13, 25) * IN_MILLISECONDS);
                break;
            }
            case AZUREGOS_FROSTBREATH:                      // 施放冰霜吐息
            {
                if (DoCastSpellIfCan(m_creature, SPELL_FROST_BREATH) == CAST_OK)
                    ResetCombatAction(action, urand(10, 25) * IN_MILLISECONDS);
                break;
            }
            case AZUREGOS_REFLECTMAGIC:                     // 施放法术反射
            {
                if (DoCastSpellIfCan(m_creature, SPELL_REFLECT) == CAST_OK)
                    ResetCombatAction(action, urand(20, 35) * IN_MILLISECONDS);
                break;
            }
            case AZUREGOS_CLEAVE:                           // 对当前受击目标施放顺劈斩
            {
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_CLEAVE) == CAST_OK)
                    ResetCombatAction(action, 7 * IN_MILLISECONDS);
                break;
            }
            default:
                break;
        }
    }
};

void AddSC_boss_azuregos()                                  // 注册艾索雷葛斯脚本
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_azuregos";
    pNewScript->GetAI = &GetNewAIInstance<boss_azuregosAI>;
    pNewScript->RegisterSelf();
}
