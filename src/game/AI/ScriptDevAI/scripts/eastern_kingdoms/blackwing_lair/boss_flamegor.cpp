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
SDName: Boss_Flamegor
SD%Complete: 100
SDComment:
SDCategory: Blackwing Lair
EndScriptData */

// 弗莱格尔（Flamegor）6号首领战 AI（激怒狂暴与火翼打击）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "blackwing_lair.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    EMOTE_GENERIC_FRENZY        = -1000002,                 // generic狂乱 表情动作 ID

    SPELL_SHADOW_FLAME          = 22539,                    // 暗影烈焰 法术 ID
    SPELL_WING_BUFFET           = 23339,                    // 翼击震击 法术 ID
    SPELL_FRENZY                = 23342,                    // This spell periodically triggers fire nova
    SPELL_THRASH                = 3391,                     // thrash 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum FlamegorActions
{
    FLAMEGOR_FRENZY,
    FLAMEGOR_SHADOW_FLAME,
    FLAMEGOR_WING_BUFFET,
    FLAMEGOR_THRASH,
    FLAMEGOR_ACTION_MAX,
};

// 首领战 AI：标识 ID（flamegor）
struct boss_flamegorAI : public CombatAI
{
    boss_flamegorAI(Creature* creature) : CombatAI(creature, FLAMEGOR_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddCombatAction(FLAMEGOR_FRENZY, 10000u);
        AddCombatAction(FLAMEGOR_SHADOW_FLAME, uint32(18 * IN_MILLISECONDS));
        AddCombatAction(FLAMEGOR_WING_BUFFET, uint32(30 * IN_MILLISECONDS));
        AddCombatAction(FLAMEGOR_THRASH, uint32(6 * IN_MILLISECONDS));
    }

    ScriptedInstance* m_instance;

    void Aggro(Unit* /*who*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_FLAMEGOR, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_FLAMEGOR, DONE);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_FLAMEGOR, FAIL);
    }

    void SpellHitTarget(Unit* target, const SpellEntry* spellInfo, SpellMissInfo /*missInfo*/) override
    {
        if (spellInfo->Id == SPELL_WING_BUFFET) // reduces threat of everyone hit (降低所有受击目标的仇恨)
            m_creature->getThreatManager().modifyThreatPercent(target, -50);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case FLAMEGOR_FRENZY:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_FRENZY) == CAST_OK)
                {
                    DoScriptText(EMOTE_GENERIC_FRENZY, m_creature);
                    ResetCombatAction(action, urand(10 * IN_MILLISECONDS, 15 * IN_MILLISECONDS));
                }
                break;
            }
            case FLAMEGOR_SHADOW_FLAME:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_SHADOW_FLAME) == CAST_OK)
                    ResetCombatAction(action, urand(15 * IN_MILLISECONDS, 18 * IN_MILLISECONDS));
                break;
            }
            case FLAMEGOR_WING_BUFFET:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_WING_BUFFET) == CAST_OK)
                    ResetCombatAction(action, urand(30 * IN_MILLISECONDS, 35 * IN_MILLISECONDS));
                break;
            }
            case FLAMEGOR_THRASH:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_THRASH) == CAST_OK)
                    ResetCombatAction(action, urand(2 * IN_MILLISECONDS, 6 * IN_MILLISECONDS));
                break;
            }
        }
    }
};

// 注册并载入脚本模块：boss_flamegor
void AddSC_boss_flamegor()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_flamegor";
    pNewScript->GetAI = &GetNewAIInstance<boss_flamegorAI>;
    pNewScript->RegisterSelf();
}
