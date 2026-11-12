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
SDName: Boss_Maiden_of_Virtue
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

// 贞节圣女（Maiden of Virtue）首领战 AI（忏悔、神圣之火与神圣愤怒）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "karazhan.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO               = 15079,                        // 开战 台词广播 ID
    SAY_SLAY1               = 15082,                        // slay1 台词广播 ID
    SAY_SLAY2               = 15083,                        // slay2 台词广播 ID
    SAY_SLAY3               = 15084,                        // slay3 台词广播 ID
    SAY_REPENTANCE1         = 15080,                        // repentance1 台词广播 ID
    SAY_REPENTANCE2         = 13396,                        // repentance2 台词广播 ID
    SAY_DEATH               = 15081,                        // 死亡 台词广播 ID

    SPELL_REPENTANCE        = 29511,                        // repentance 法术 ID
    SPELL_HOLYFIRE          = 29522,                        // holyfire 法术 ID
    SPELL_HOLYWRATH         = 32445,                        // holywrath 法术 ID
    SPELL_HOLYGROUND        = 29512                         // holyground 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum MaidenOfVirtueActions
{
    MAIDEN_ACTION_MAX,
};

// 首领战 AI：ofvirtue（maiden_of_virtue）
struct boss_maiden_of_virtueAI : public CombatAI
{
    boss_maiden_of_virtueAI(Creature* creature) : CombatAI(creature, MAIDEN_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        m_creature->GetCombatManager().SetLeashingCheck([&](Unit*, float x, float y, float /*z*/)
        {
            return y > -2071.f && x > -10924.f;
        });

        AddOnKillText(SAY_SLAY1, SAY_SLAY2, SAY_SLAY3);
    }

    ScriptedInstance* m_instance;

    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_MAIDEN, DONE);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoBroadcastText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_MAIDEN, IN_PROGRESS);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MAIDEN, FAIL);
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        switch (spellInfo->Id)
        {
            case SPELL_REPENTANCE:
                DoBroadcastText(urand(0, 1) ? SAY_REPENTANCE1 : SAY_REPENTANCE2, m_creature);
                break;
        }
    }
};

// 注册并载入脚本模块：boss_maiden_of_virtue
void AddSC_boss_maiden_of_virtue()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_maiden_of_virtue";
    pNewScript->GetAI = &GetNewAIInstance<boss_maiden_of_virtueAI>;
    pNewScript->RegisterSelf();
}
