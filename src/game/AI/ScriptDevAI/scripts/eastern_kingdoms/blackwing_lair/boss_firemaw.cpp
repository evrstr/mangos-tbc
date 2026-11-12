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
SDName: Boss_Firemaw
SD%Complete: 100
SDComment:
SDCategory: Blackwing Lair
EndScriptData */

// 费尔默（Firemaw）4号首领战 AI（烈焰打击与火翼打击）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "blackwing_lair.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SPELL_SHADOW_FLAME          = 22539,                    // 暗影烈焰 法术 ID
    SPELL_WING_BUFFET           = 23339,                    // 翼击震击 法术 ID
    SPELL_FLAME_BUFFET          = 23341,                    // 烈焰震击 法术 ID
    SPELL_THRASH                = 3391, // confirmed not proc
};

// 枚举定义：法术技能、生物与环境事件标识
enum FiremawActions
{
    FIREMAW_SHADOW_FLAME,
    FIREMAW_WING_BUFFET,
    FIREMAW_FLAME_BUFFET,
    FIREMAW_THRASH,
    FIREMAW_ACTION_MAX,
};

// 首领战 AI：标识 ID（firemaw）
struct boss_firemawAI : public CombatAI
{
    boss_firemawAI(Creature* creature) : CombatAI(creature, FIREMAW_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddCombatAction(FIREMAW_SHADOW_FLAME, uint32(18 * IN_MILLISECONDS));
        AddCombatAction(FIREMAW_WING_BUFFET, uint32(30 * IN_MILLISECONDS));
        AddCombatAction(FIREMAW_FLAME_BUFFET, 5000u);
        AddCombatAction(FIREMAW_THRASH, uint32(6 * IN_MILLISECONDS));
    }

    ScriptedInstance* m_instance;

    void Aggro(Unit* /*who*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_FIREMAW, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_FIREMAW, DONE);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_FIREMAW, FAIL);
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
            case FIREMAW_SHADOW_FLAME:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_SHADOW_FLAME) == CAST_OK)
                    ResetCombatAction(action, urand(15 * IN_MILLISECONDS, 18 * IN_MILLISECONDS));
                break;
            }
            case FIREMAW_WING_BUFFET:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_WING_BUFFET) == CAST_OK)
                    ResetCombatAction(action, urand(30 * IN_MILLISECONDS, 35 * IN_MILLISECONDS));
                break;
            }
            case FIREMAW_FLAME_BUFFET:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_FLAME_BUFFET) == CAST_OK)
                    ResetCombatAction(action, 5000);
                break;
            }
            case FIREMAW_THRASH:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_THRASH) == CAST_OK)
                    ResetCombatAction(action, urand(2 * IN_MILLISECONDS, 6 * IN_MILLISECONDS));
                break;
            }
        }
    }
};

// 注册并载入脚本模块：boss_firemaw
void AddSC_boss_firemaw()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_firemaw";
    pNewScript->GetAI = &GetNewAIInstance<boss_firemawAI>;
    pNewScript->RegisterSelf();
}
