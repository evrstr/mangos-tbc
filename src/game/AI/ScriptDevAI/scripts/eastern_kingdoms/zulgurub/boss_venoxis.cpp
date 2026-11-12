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
SDName: Boss_Venoxis
SD%Complete: 100
SDComment:
SDCategory: Zul'Gurub
EndScriptData */

// 高阶祭司温诺希斯（High Priest Venoxis）毒蛇祭司首领战 AI（闪电链、神圣之火与巨毒毒云转阶段）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "zulgurub.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_TRANSFORM           = -1309000,                     // 变形 台词广播 ID
    SAY_DEATH               = -1309001,                     // 死亡 台词广播 ID

    // troll spells
    SPELL_HOLY_FIRE         = 23860,                        // 神圣火焰 法术 ID
    SPELL_HOLY_WRATH        = 23979,                        // 神圣wrath 法术 ID
    SPELL_HOLY_NOVA         = 23858,                        // 神圣新星 法术 ID
    SPELL_DISPELL           = 23859,                        // dispell 法术 ID
    SPELL_RENEW             = 23895,                        // renew 法术 ID

    // serpent spells
    SPELL_VENOMSPIT         = 23862,                        // venomspit 法术 ID
    SPELL_POISON_CLOUD      = 23861,                        // 毒药cloud 法术 ID
    SPELL_PARASITIC_SERPENT = 23867,                        // parasiticserpent 法术 ID

    // common spells
    SPELL_SNAKE_FORM            = 23849,                    // snakeform 法术 ID
    SPELL_FRENZY                = 23537,                    // 狂乱 法术 ID
    SPELL_VIRULENT_POISON_PROC  = 22413,                    // virulent毒药proc 法术 ID
    SPELL_TRASH                 = 3391,                     // trash 法术 ID

    SPELL_LIST_PHASE_1 = 1450701,                           // list阶段1 法术 ID
    SPELL_LIST_PHASE_2 = 1450702,                           // list阶段2 法术 ID
    SPELL_LIST_PHASE_3 = 1450703,                           // list阶段3 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum VenoxisActions
{
    VENOXIS_PHASE_2,
    VENOXIS_PHASE_3,
    VENOXIS_FRENZY,
    VENOXIS_ACTION_MAX,
};

// 首领战 AI：标识 ID（venoxis）
struct boss_venoxisAI : public CombatAI
{
    boss_venoxisAI(Creature* creature) : CombatAI(creature, VENOXIS_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddTimerlessCombatAction(VENOXIS_PHASE_2, true);
        AddTimerlessCombatAction(VENOXIS_PHASE_3, true);
        AddTimerlessCombatAction(VENOXIS_FRENZY, true);
    }

    ScriptedInstance* m_instance;

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_VENOXIS, FAIL);

        m_creature->SetSpellList(SPELL_LIST_PHASE_1);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_VENOXIS, DONE);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case VENOXIS_PHASE_2:
                if (m_creature->GetHealthPercent() > 50.f)
                    break;

                if (DoCastSpellIfCan(nullptr, SPELL_SNAKE_FORM) == CAST_OK)
                {
                    DoScriptText(SAY_TRANSFORM, m_creature);
                    DoCastSpellIfCan(nullptr, SPELL_POISON_CLOUD); // an instant cloud on change
                    DoCastSpellIfCan(nullptr, SPELL_VIRULENT_POISON_PROC, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
                    DoResetThreat();
                    m_creature->SetSpellList(SPELL_LIST_PHASE_2);
                    DisableCombatAction(action);
                }
                break;
            case VENOXIS_PHASE_3:
                if (m_creature->GetHealthPercent() < 25.f)
                {
                    m_creature->SetSpellList(SPELL_LIST_PHASE_3);
                    DisableCombatAction(action);
                }
                break;
            case VENOXIS_FRENZY:
                if (m_creature->GetHealthPercent() < 20.f) // maybe even 25
                    if (DoCastSpellIfCan(nullptr, SPELL_FRENZY) == CAST_OK)
                        DisableCombatAction(action);
                break;
        }
    }
};

// 注册并载入脚本模块：boss_venoxis
void AddSC_boss_venoxis()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_venoxis";
    pNewScript->GetAI = &GetNewAIInstance<boss_venoxisAI>;
    pNewScript->RegisterSelf();
}
