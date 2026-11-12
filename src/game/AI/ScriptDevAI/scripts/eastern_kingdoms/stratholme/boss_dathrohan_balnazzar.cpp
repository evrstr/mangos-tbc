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
SDName: Boss_Dathrohan_Balnazzar
SD%Complete: 95
SDComment: Possibly need to fix/improve summons after death
SDCategory: Stratholme
EndScriptData */

// 达索汉与巴纳扎尔（Dathrohan & Balnazzar）首领战 AI（血色大十字军达索汉战败后恐惧魔王巴纳扎尔现出真身）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "stratholme.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO                       = -1329016,             // 开战 台词广播 ID
    SAY_TRANSFORM                   = -1329017,             // 变形 台词广播 ID
    SAY_DEATH                       = -1329018,             // 死亡 台词广播 ID

    // Dathrohan spells (达索汉spells)
    SPELL_CRUSADERSHAMMER           = 17286,                // AOE stun (AOE击晕)
    SPELL_CRUSADERSTRIKE            = 17281,                // crusaderstrike 法术 ID
    SPELL_HOLYSTRIKE                = 17284,                // weapon dmg +3

    // Transform (变形)
    SPELL_BALNAZZARTRANSFORM        = 17288,                // restore full HP/mana, trigger spell Balnazzar Transform Stun

    // Balnazzar spells (巴纳扎尔spells)
    SPELL_SHADOWSHOCK               = 17399,                // shadowshock 法术 ID
    SPELL_MINDBLAST                 = 17287,                // mindblast 法术 ID
    SPELL_PSYCHICSCREAM             = 13704,                // psychicscream 法术 ID
    SPELL_SLEEP                     = 12098,                // sleep 法术 ID
    SPELL_MINDCONTROL               = 15690,                // mindcontrol 法术 ID

    NPC_DATHROHAN                   = 10812                 // 达索汉 Entry ID
};

// 首领战 AI：巴纳扎尔（dathrohan_balnazzar）
struct boss_dathrohan_balnazzarAI : public ScriptedAI
{
    boss_dathrohan_balnazzarAI(Creature* pCreature) : ScriptedAI(pCreature) {Reset();}

    uint32 m_uiCrusadersHammer_Timer;                       // CrusadersHammer_ 技能计时器
    uint32 m_uiCrusaderStrike_Timer;                        // CrusaderStrike_ 技能计时器
    uint32 m_uiMindBlast_Timer;                             // MindBlast_ 技能计时器
    uint32 m_uiHolyStrike_Timer;                            // HolyStrike_ 技能计时器
    uint32 m_uiShadowShock_Timer;                           // ShadowShock_ 技能计时器
    uint32 m_uiPsychicScream_Timer;                         // PsychicScream_ 技能计时器
    uint32 m_uiDeepSleep_Timer;                             // DeepSleep_ 技能计时器
    uint32 m_uiMindControl_Timer;                           // MindControl_ 技能计时器
    bool m_bTransformed;                                    // Transformed 状态标记

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiCrusadersHammer_Timer = 8000;
        m_uiCrusaderStrike_Timer = 12000;
        m_uiMindBlast_Timer = 6000;
        m_uiHolyStrike_Timer = 18000;
        m_uiShadowShock_Timer = 4000;
        m_uiPsychicScream_Timer = 16000;
        m_uiDeepSleep_Timer = 20000;
        m_uiMindControl_Timer = 10000;
        m_bTransformed = false;

        if (m_creature->GetEntry() == NPC_BALNAZZAR)
            m_creature->UpdateEntry(NPC_DATHROHAN);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
    }

    void JustDied(Unit* /*Victim*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // START NOT TRANSFORMED
        if (!m_bTransformed)
        {
            // MindBlast
            if (m_uiMindBlast_Timer < uiDiff)
            {
                DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MINDBLAST);
                m_uiMindBlast_Timer = urand(15000, 20000);
            }
            else m_uiMindBlast_Timer -= uiDiff;

            // CrusadersHammer
            if (m_uiCrusadersHammer_Timer < uiDiff)
            {
                DoCastSpellIfCan(m_creature->GetVictim(), SPELL_CRUSADERSHAMMER);
                m_uiCrusadersHammer_Timer = 12000;
            }
            else m_uiCrusadersHammer_Timer -= uiDiff;

            // CrusaderStrike
            if (m_uiCrusaderStrike_Timer < uiDiff)
            {
                DoCastSpellIfCan(m_creature->GetVictim(), SPELL_CRUSADERSTRIKE);
                m_uiCrusaderStrike_Timer = 15000;
            }
            else m_uiCrusaderStrike_Timer -= uiDiff;

            // HolyStrike
            if (m_uiHolyStrike_Timer < uiDiff)
            {
                DoCastSpellIfCan(m_creature->GetVictim(), SPELL_HOLYSTRIKE);
                m_uiHolyStrike_Timer = 15000;
            }
            else m_uiHolyStrike_Timer -= uiDiff;

            // BalnazzarTransform
            if (m_creature->GetHealthPercent() < 40.0f)
            {
                // restore hp, mana and stun
                if (DoCastSpellIfCan(m_creature, SPELL_BALNAZZARTRANSFORM) == CAST_OK)
                {
                    m_creature->UpdateEntry(NPC_BALNAZZAR);
                    DoScriptText(SAY_TRANSFORM, m_creature);
                    // Dathrohan wield a weapon but Balnazzar does not need one
                    SetEquipmentSlots(true);
                    m_creature->SetSheath(SHEATH_STATE_UNARMED);
                    m_bTransformed = true;
                }
            }
        }
        else
        {
            // MindBlast
            if (m_uiMindBlast_Timer < uiDiff)
            {
                DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MINDBLAST);
                m_uiMindBlast_Timer = urand(15000, 20000);
            }
            else m_uiMindBlast_Timer -= uiDiff;

            // ShadowShock
            if (m_uiShadowShock_Timer < uiDiff)
            {
                DoCastSpellIfCan(m_creature->GetVictim(), SPELL_SHADOWSHOCK);
                m_uiShadowShock_Timer = 11000;
            }
            else m_uiShadowShock_Timer -= uiDiff;

            // PsychicScream
            if (m_uiPsychicScream_Timer < uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                    DoCastSpellIfCan(pTarget, SPELL_PSYCHICSCREAM);

                m_uiPsychicScream_Timer = 20000;
            }
            else m_uiPsychicScream_Timer -= uiDiff;

            // DeepSleep
            if (m_uiDeepSleep_Timer < uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                    DoCastSpellIfCan(pTarget, SPELL_SLEEP);

                m_uiDeepSleep_Timer = 15000;
            }
            else m_uiDeepSleep_Timer -= uiDiff;

            // MindControl
            if (m_uiMindControl_Timer < uiDiff)
            {
                DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MINDCONTROL);
                m_uiMindControl_Timer = 15000;
            }
            else m_uiMindControl_Timer -= uiDiff;
        }

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_dathrohan_balnazzar
UnitAI* GetAI_boss_dathrohan_balnazzar(Creature* pCreature)
{
    return new boss_dathrohan_balnazzarAI(pCreature);
}

// 注册并载入脚本模块：boss_dathrohan_balnazzar
void AddSC_boss_dathrohan_balnazzar()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_dathrohan_balnazzar";
    pNewScript->GetAI = &GetAI_boss_dathrohan_balnazzar;
    pNewScript->RegisterSelf();
}
