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
SDName: Boss_General_Angerforge
SD%Complete: 100
SDComment:
SDCategory: Blackrock Depths
EndScriptData */

// 安格弗将军（General Angerforge）首领战 AI

#include "AI/ScriptDevAI/include/sc_common.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    EMOTE_ALARM                 = -1230035,                 // 警报 表情动作 ID

    SPELL_FLURRY                = 15088,                    // 乱击 法术 ID
    SPELL_ENRAGE                = 15097,                    // 狂暴 法术 ID
    SPELL_SUNDER_ARMOR          = 15572,                    // sunderarmor 法术 ID

    NPC_ANVILRAGE_MEDIC         = 8894,                     // anvilrage医师 Entry ID
    NPC_ANVILRAGE_RESERVIST     = 8901,                     // anvilragereservist Entry ID

    NPC_ELITE_AMOUNT            = 2,                        // eliteamount Entry ID
    NPC_NORMAL_AMOUNT           = 8,                        // normalamount Entry ID
};

static const float aAlarmPoint[4] = {717.343f, 22.116f, -45.4321f, 3.1415f};

// 首领战 AI：安格弗（general_angerforge）
struct boss_general_angerforgeAI : public ScriptedAI
{
    boss_general_angerforgeAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 m_uiSunderArmorTimer;                            // SunderArmor 技能计时器
    uint32 m_uiAlarmTimer;                                  // Alarm 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiSunderArmorTimer = urand(5 * IN_MILLISECONDS, 10 * IN_MILLISECONDS);
        m_uiAlarmTimer = 0;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoCastSpellIfCan(m_creature, SPELL_FLURRY, CAST_AURA_NOT_PRESENT | CAST_TRIGGERED);
        DoCastSpellIfCan(m_creature, SPELL_ENRAGE, CAST_AURA_NOT_PRESENT | CAST_TRIGGERED);
    }

    void SummonAdd(uint32 uiEntry)
    {
        float fX, fY, fZ;
        m_creature->GetRandomPoint(aAlarmPoint[0], aAlarmPoint[1], aAlarmPoint[2], 1.0f, fX, fY, fZ);
        m_creature->SummonCreature(uiEntry, fX, fY, fZ, aAlarmPoint[3], TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 30 * IN_MILLISECONDS);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* pSummoned) override
    {
        if (m_creature->GetVictim())
            pSummoned->AI()->AttackStart(m_creature->GetVictim());
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        // Return since we have no target (无有效目标，退出更新)
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // Sunder_Armor-Timer
        if (m_uiSunderArmorTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_SUNDER_ARMOR) == CAST_OK)
                m_uiSunderArmorTimer = urand(5 * IN_MILLISECONDS, 15 * IN_MILLISECONDS);
        }
        else
            m_uiSunderArmorTimer -= uiDiff;

        // Alarm-Timer
        if (m_creature->GetHealthPercent() < 30.0f)
        {
            if (m_uiAlarmTimer < uiDiff)
            {
                DoScriptText(EMOTE_ALARM, m_creature);

                for (uint8 i = 0; i < NPC_NORMAL_AMOUNT; i++)
                    SummonAdd(NPC_ANVILRAGE_RESERVIST);
                for (uint8 i = 0; i < NPC_ELITE_AMOUNT; i++)
                    SummonAdd(NPC_ANVILRAGE_MEDIC);

                m_uiAlarmTimer = 3 * MINUTE * IN_MILLISECONDS;
            }
            else
                m_uiAlarmTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_general_angerforge
UnitAI* GetAI_boss_general_angerforge(Creature* pCreature)
{
    return new boss_general_angerforgeAI(pCreature);
}

// 注册并载入脚本模块：boss_general_angerforge
void AddSC_boss_general_angerforge()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_general_angerforge";
    pNewScript->GetAI = &GetAI_boss_general_angerforge;
    pNewScript->RegisterSelf();
}
