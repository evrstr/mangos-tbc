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
SDName: Boss_Temporus
SD%Complete: 90
SDComment:
Patch_2.2.0: Increased recast time for Temporus' Hasten spell on Heroic. GetSubsequentActionTimer(TEMPORUS_ACTION_HASTEN) -> 12-18 from 16-24
SDCategory: Caverns of Time, The Dark Portal
EndScriptData */

/**
 * 开启黑暗之门：坦普卢斯 (Temporus) 首领战斗 AI
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "dark_portal.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 常量与事件枚举
enum
{
    SAY_AGGRO               = 16822,                        // 剧情台词/喊话文本 ID
    SAY_BANISH              = 16824,                        // 剧情台词/喊话文本 ID
    SAY_SLAY1               = 16825,                        // 剧情台词/喊话文本 ID
    SAY_SLAY2               = 16826,                        // 剧情台词/喊话文本 ID
    SAY_DEATH               = 16823,                        // 剧情台词/喊话文本 ID

    SPELL_HASTEN            = 31458,                        // 技能法术 ID
    SPELL_MORTAL_WOUND      = 31464,                        // 技能法术 ID
    SPELL_WING_BUFFET       = 31475,                        // 技能法术：龙翼打击
    SPELL_WING_BUFFET_H     = 38593,                        // 技能法术：龙翼打击
    SPELL_REFLECTION        = 38592
};

enum TemporusActions // order based on priority (说明)
{
    TEMPORUS_ACTION_MAX
};

// boss_temporusAI 首领战斗 AI
struct boss_temporusAI : public CombatAI
{
    boss_temporusAI(Creature* creature) : CombatAI(creature, TEMPORUS_ACTION_MAX),
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

    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        // Despawn Time Keeper (使时间管理者消失)
        if (who->GetTypeId() == TYPEID_UNIT && who->GetEntry() == NPC_TIME_KEEPER)
        {
            if (m_creature->IsWithinDistInMap(who, 20.0f))
            {
                if (DoCastSpellIfCan(who, SPELL_BANISH_HELPER) == CAST_OK)
                    DoBroadcastText(SAY_BANISH, m_creature);
            }
        }

        ScriptedAI::MoveInLineOfSight(who);
    }
};

void AddSC_boss_temporus() // 注册脚本到系统管理器
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_temporus";
    pNewScript->GetAI = &GetNewAIInstance<boss_temporusAI>;
    pNewScript->RegisterSelf();
}
