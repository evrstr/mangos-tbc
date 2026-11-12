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
SDName: Boss_Chrono_Lord_Deja
SD%Complete: 90
SDComment: Small adjustments; Timers
SDCategory: Caverns of Time, The Dark Portal
EndScriptData */

/**
 * 开启黑暗之门：编年史领主德亚 (Chrono Lord Deja) 首领战斗 AI
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "dark_portal.h"

// 常量与事件枚举
enum
{
    SAY_AGGRO                   = 16815,                    // 剧情台词/喊话文本 ID
    SAY_BANISH                  = 16817,                    // 剧情台词/喊话文本 ID
    SAY_SLAY1                   = 16818,                    // 剧情台词/喊话文本 ID
    SAY_SLAY2                   = 16819,                    // 剧情台词/喊话文本 ID
    SAY_DEATH                   = 16816,                    // 剧情台词/喊话文本 ID

    SPELL_ARCANE_BLAST          = 31457,                    // 技能法术 ID
    SPELL_ARCANE_BLAST_H        = 38538,                    // 技能法术 ID
    SPELL_ARCANE_DISCHARGE      = 31472,                    // 技能法术 ID
    SPELL_ARCANE_DISCHARGE_H    = 38539,                    // 技能法术 ID
    SPELL_TIME_LAPSE            = 31467,                    // 技能法术 ID
    SPELL_ATTRACTION            = 38540
};

// boss_chrono_lord_dejaAI 首领战斗 AI
struct boss_chrono_lord_dejaAI : public CombatAI
{
    boss_chrono_lord_dejaAI(Creature* creature) : CombatAI(creature, 0),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddOnKillText(SAY_SLAY1, SAY_SLAY2);
    }

    ScriptedInstance* m_instance;
    bool m_isRegularMode;

    void Aggro(Unit* /*who*/) override
    {
        DoBroadcastText(SAY_AGGRO, m_creature);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        // Despawn Time Keeper (使时间管理者消失)
        if (who->IsCreature() && who->GetEntry() == NPC_TIME_KEEPER)
        {
            if (m_creature->IsWithinDistInMap(who, 20.0f))
            {
                if (DoCastSpellIfCan(who, SPELL_BANISH_HELPER) == CAST_OK)
                    DoBroadcastText(SAY_BANISH, m_creature);
            }
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void JustDied(Unit* /*victim*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);
    }
};

void AddSC_boss_chrono_lord_deja() // 注册脚本到系统管理器
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_chrono_lord_deja";
    pNewScript->GetAI = &GetNewAIInstance<boss_chrono_lord_dejaAI>;
    pNewScript->RegisterSelf();
}
