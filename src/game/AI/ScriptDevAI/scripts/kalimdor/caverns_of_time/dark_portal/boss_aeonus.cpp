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
SDName: Boss_Aeonus
SD%Complete: 90
SDComment: Small adjustments; Timers
SDCategory: Caverns of Time, The Dark Portal
EndScriptData */

/**
 * 开启黑暗之门：埃欧努斯 (Aeonus) 首领战斗 AI
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "dark_portal.h"

// 常量与事件枚举
enum
{
    SAY_AGGRO               = 16828,                        // 剧情台词/喊话文本 ID
    SAY_BANISH              = 16829,                        // 剧情台词/喊话文本 ID
    SAY_SLAY1               = 16831,                        // 剧情台词/喊话文本 ID
    SAY_SLAY2               = 16832,                        // 剧情台词/喊话文本 ID
    SAY_DEATH               = 16830,                        // 剧情台词/喊话文本 ID
    EMOTE_GENERIC_FRENZY    = 1191,                         // 剧情台词/喊话文本 ID

    SPELL_THRASH            = 8876,                         // 技能法术 ID
    SPELL_DOUBLE_ATTACK     = 19818,                        // 技能法术 ID

    SPELL_CLEAVE            = 40504,                        // 技能法术：顺劈斩
    SPELL_TIME_STOP         = 31422,                        // 技能法术 ID
    SPELL_ENRAGE            = 37605,                        // 技能法术：激怒/狂暴
    SPELL_SAND_BREATH       = 31473,                        // 技能法术 ID
    SPELL_SAND_BREATH_H     = 39049
};

// boss_aeonusAI 首领战斗 AI
struct boss_aeonusAI : public CombatAI
{
    boss_aeonusAI(Creature* creature) : CombatAI(creature, 0),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddOnKillText(SAY_SLAY1, SAY_SLAY2);
    }

    ScriptedInstance* m_instance;
    bool m_isRegularMode;

    void Reset() override                                   // 重置战斗状态与计时器
    {
        CombatAI::Reset();
        DoCastSpellIfCan(nullptr, m_isRegularMode ? SPELL_THRASH : SPELL_DOUBLE_ATTACK, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoBroadcastText(SAY_AGGRO, m_creature);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        // Despawn Time Keeper (使时间管理者消失)
        if (who->GetTypeId() == TYPEID_UNIT && who->GetEntry() == NPC_TIME_KEEPER)
        {
            if (m_creature->IsWithinDistInMap(who, 20.0f))
            {
                if (DoCastSpellIfCan(who, SPELL_BANISH_HELPER) == CAST_OK)
                    DoScriptText(SAY_BANISH, m_creature);
            }
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        if (spellInfo->Id == SPELL_ENRAGE)
            DoBroadcastText(EMOTE_GENERIC_FRENZY, m_creature);
    }
};

void AddSC_boss_aeonus() // 注册脚本到系统管理器
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_aeonus";
    pNewScript->GetAI = &GetNewAIInstance<boss_aeonusAI>;
    pNewScript->RegisterSelf();
}
