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
SDName: Boss_Hydromancer_Thespia
SD%Complete: 80
SDComment: Timers may need small adjustments; Elementals summon needs further research
SDCategory: Coilfang Resevoir, The Steamvault
EndScriptData */

// 水术师瑟丝比娅（蒸汽地窟一号首领）
/* 遭遇战概述：
 * 水术师瑟丝比娅携两只元素仆从，施放暴风雪、水之结界与肺部充水。
 */

/* ContentData
boss_hydromancer_thespia
mob_coilfang_waterelemental
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "steam_vault.h"

// 相关常量、法术与文本条目枚举
enum
{
    SAY_SUMMON                  = -1545000, // has no bct, but has sound - keeping it as script text
    SAY_CLOUD                   = 19456,
    SAY_AGGRO_1                 = 17696,
    SAY_AGGRO_2                 = 17697,
    SAY_AGGRO_3                 = 17698,
    SAY_SLAY_1                  = 17701,
    SAY_SLAY_2                  = 17702,
    SAY_DEAD                    = 17704,

    SPELL_LIGHTNING_CLOUD       = 25033,
    SPELL_LUNG_BURST            = 31481,
    SPELL_ENVELOPING_WINDS      = 31718,
    SPELL_SUMMON_ELEMENTALS     = 31476,            // not sure where to use this
};

// boss_thespiaAI 逻辑实现
struct boss_thespiaAI : public CombatAI
{
    boss_thespiaAI(Creature* creature) : CombatAI(creature, 0),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_bIsRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddOnKillText(SAY_SLAY_1, SAY_SLAY_2);
    }

    ScriptedInstance* m_instance;
    bool m_bIsRegularMode;

    // 脱战归位回调
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_HYDROMANCER_THESPIA, FAIL);
    }

    // 死亡回调
    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(SAY_DEAD, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_HYDROMANCER_THESPIA, DONE);
    }

    // 进入战斗回调
    void Aggro(Unit* /*who*/) override
    {
        switch (urand(0, 2))
        {
            case 0: DoBroadcastText(SAY_AGGRO_1, m_creature); break;
            case 1: DoBroadcastText(SAY_AGGRO_2, m_creature); break;
            case 2: DoBroadcastText(SAY_AGGRO_3, m_creature); break;
        }

        if (m_instance)
            m_instance->SetData(TYPE_HYDROMANCER_THESPIA, IN_PROGRESS);
    }

    // 施法完成回调
    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        if (spellInfo->Id == SPELL_LIGHTNING_CLOUD)
            if (urand(0, 1))
                DoBroadcastText(SAY_CLOUD, m_creature);
    }
};

// 注册 boss_hydromancer_thespia 脚本
void AddSC_boss_hydromancer_thespia()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_hydromancer_thespia";
    pNewScript->GetAI = &GetNewAIInstance<boss_thespiaAI>;
    pNewScript->RegisterSelf();
}
