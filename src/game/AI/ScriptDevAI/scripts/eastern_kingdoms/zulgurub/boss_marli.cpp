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
SDName: Boss_Marli
SD%Complete: 90
SDComment: Enlarge for small spiders NYI
SDCategory: Zul'Gurub
EndScriptData */

// 高阶祭司玛尔里（High Priestess Mar'li）蜘蛛祭司首领战 AI（缠绕蛛网、毒液喷射与孵化剧毒幼蛛）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "zulgurub.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO                   = -1309005,                 // 开战 台词广播 ID
    SAY_TRANSFORM               = -1309006,                 // 变形 台词广播 ID
    SAY_TRANSFORM_BACK          = -1309025,                 // 变形back 台词广播 ID
    SAY_SPIDER_SPAWN            = -1309007,                 // spider生成 台词广播 ID
    SAY_DEATH                   = -1309008,                 // 死亡 台词广播 ID

    // Spider form spells
    SPELL_CORROSIVE_POISON      = 24111,                    // 腐蚀性毒药 法术 ID
    SPELL_CHARGE                = 22911,                    // 冲锋 法术 ID
    SPELL_ENVELOPING_WEBS       = 24110,                    // envelopingwebs 法术 ID
    SPELL_POISON_SHOCK          = 24112,                    // 毒药震击 法术 ID

    // Troll form spells
    SPELL_POISON_VOLLEY         = 24099,                    // 毒药箭雨 法术 ID
    SPELL_DRAIN_LIFE            = 24300,                    // 吸取生命 法术 ID
    SPELL_ENLARGE               = 24109,                    // cast on friendlies
    SPELL_SPIDER_EGG            = 24082,                    // removed from DBC - should trigger 24081 which summons 15041

    // common spells
    SPELL_SPIDER_FORM           = 24084,                    // spiderform 法术 ID
    SPELL_TRANSFORM_BACK        = 24085,                    // 变形back 法术 ID
    SPELL_TRASH                 = 3391,                     // trash 法术 ID
    SPELL_HATCH_EGGS            = 24083,                    // note this should only target 4 eggs!

    SPELL_LIST_PHASE_1          = 1450701,                  // list阶段1 法术 ID
    SPELL_LIST_PHASE_2          = 1450702,                  // list阶段2 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum MarliActions
{
    MARLI_PHASE_CHANGE,
    MARLI_CHARGE,
    MARLI_ACTIONS_MAX,
};

// 首领战 AI：标识 ID（marli）
struct boss_marliAI : public CombatAI
{
    boss_marliAI(Creature* creature) : CombatAI(creature, MARLI_ACTIONS_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddCombatAction(MARLI_PHASE_CHANGE, 60000u);
        AddCombatAction(MARLI_CHARGE, true);
    }

    ScriptedInstance* m_instance;

    bool m_phaseTwo;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();

        m_phaseTwo = false;
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        DoCastSpellIfCan(nullptr, SPELL_HATCH_EGGS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_MARLI, DONE);
    }

    void SpellHitTarget(Unit* /*target*/, const SpellEntry* spellInfo) override
    {
        if (spellInfo->Id == SPELL_ENVELOPING_WEBS)
            ResetCombatAction(MARLI_CHARGE, 1000);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MARLI, FAIL);

        m_creature->SetSpellList(SPELL_LIST_PHASE_1);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case MARLI_PHASE_CHANGE:
                if (!m_phaseTwo)
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_SPIDER_FORM) == CAST_OK)
                    {
                        DoScriptText(SAY_TRANSFORM, m_creature);
                        DoResetThreat();
                        ResetCombatAction(action, 60000);
                        m_creature->SetSpellList(SPELL_LIST_PHASE_2);
                        m_phaseTwo = true;
                    }
                }
                else
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_TRANSFORM_BACK) == CAST_OK)
                    {
                        DoScriptText(SAY_TRANSFORM_BACK, m_creature);
                        m_creature->RemoveAurasDueToSpell(SPELL_SPIDER_FORM);
                        ResetCombatAction(action, 60000);
                        m_creature->SetSpellList(SPELL_LIST_PHASE_1);
                        m_phaseTwo = false;
                    }
                }
                break;
            case MARLI_CHARGE:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_CHARGE, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, SPELL_CHARGE) == CAST_OK)
                        DisableCombatAction(action);
                break;
        }
    }
};

// 注册并载入脚本模块：boss_marli
void AddSC_boss_marli()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_marli";
    pNewScript->GetAI = &GetNewAIInstance<boss_marliAI>;
    pNewScript->RegisterSelf();
}
