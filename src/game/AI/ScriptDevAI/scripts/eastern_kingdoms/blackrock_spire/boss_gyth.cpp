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
SDName: Boss_Gyth
SD%Complete: 100
SDComment: Timers may need adjustments
SDCategory: Blackrock Spire
EndScriptData */

// 盖斯与雷德·黑手（Gyth & Rend Blackhand）首领战 AI

#include "AI/ScriptDevAI/include/sc_common.h"
#include "blackrock_spire.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_NEFARIUS_BUFF_GYTH  = -1229017,                     // 奈法利奥斯增益盖斯 台词广播 ID
    EMOTE_KNOCKED_OFF       = -1229019,                     // knockedoff 表情动作 ID

    SPELL_CHROMATIC_CHAOS   = 16337,                // casted by Nefarius at 50%
    SPELL_REND_MOUNTS       = 16167,                        // 雷德mounts 法术 ID
    SPELL_SUMMON_REND       = 16328,                        // 召唤雷德 法术 ID
    SPELL_CORROSIVE_ACID    = 16359,                        // 腐蚀性酸液 法术 ID
    SPELL_FREEZE            = 16350,                        // freeze 法术 ID
    SPELL_FLAME_BREATH      = 16390,                        // 烈焰吐息 法术 ID
    SPELL_KNOCK_AWAY        = 10101,                        // knockaway 法术 ID
};

// 首领战 AI：标识 ID（gyth）
struct boss_gythAI : public ScriptedAI
{
    boss_gythAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_blackrock_spire*) pCreature->GetInstanceData();
        Reset();
    }

    instance_blackrock_spire* m_pInstance;

    uint32 uiCorrosiveAcidTimer;
    uint32 uiFreezeTimer;
    uint32 uiFlamebreathTimer;
    uint32 uiKnockAwayTimer;

    bool m_bSummonedRend;                                   // SummonedRend 状态标记
    bool m_bHasChromaticChaos;                              // HasChromaticChaos 状态标记

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        uiCorrosiveAcidTimer = 8000;
        uiFreezeTimer        = 11000;
        uiFlamebreathTimer   = 4000;
        uiKnockAwayTimer     = 23000;
        m_bSummonedRend      = false;
        m_bHasChromaticChaos = false;
        SetDeathPrevention(true);
        DoCastSpellIfCan(m_creature, SPELL_REND_MOUNTS);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* pSummoned) override
    {
        DoScriptText(EMOTE_KNOCKED_OFF, pSummoned);
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        // Return since we have no target (无有效目标，退出更新)
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // Chromatic Chaos at 50%
        if (!m_bHasChromaticChaos && m_creature->GetHealthPercent() < 50.0f)
        {
            if (m_pInstance)
            {
                if (Creature* pNefarius = m_pInstance->GetSingleCreatureFromStorage(NPC_LORD_VICTOR_NEFARIUS))
                {
                    pNefarius->CastSpell(m_creature, SPELL_CHROMATIC_CHAOS, TRIGGERED_OLD_TRIGGERED);
                    DoScriptText(SAY_NEFARIUS_BUFF_GYTH, pNefarius);
                    m_bHasChromaticChaos = true;
                }
            }
        }

        // CorrosiveAcid_Timer
        if (uiCorrosiveAcidTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_CORROSIVE_ACID) == CAST_OK)
                uiCorrosiveAcidTimer = 7000;
        }
        else
            uiCorrosiveAcidTimer -= uiDiff;

        // Freeze_Timer
        if (uiFreezeTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FREEZE) == CAST_OK)
                uiFreezeTimer = 16000;
        }
        else
            uiFreezeTimer -= uiDiff;

        // Flamebreath_Timer
        if (uiFlamebreathTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FLAME_BREATH) == CAST_OK)
                uiFlamebreathTimer = 10500;
        }
        else
            uiFlamebreathTimer -= uiDiff;

        if (uiKnockAwayTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_KNOCK_AWAY) == CAST_OK)
                uiKnockAwayTimer = 23000;
        }
        else
            uiKnockAwayTimer -= uiDiff;

        // Summon Rend (召唤 Rend)
        if (!m_bSummonedRend && m_creature->GetHealthPercent() < 11.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_REND) == CAST_OK)
            {
                SetDeathPrevention(false);
                m_creature->RemoveAurasDueToSpell(SPELL_REND_MOUNTS);
                m_bSummonedRend = true;
            }
        }

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_gyth
UnitAI* GetAI_boss_gyth(Creature* pCreature)
{
    return new boss_gythAI(pCreature);
}

// 注册并载入脚本模块：boss_gyth
void AddSC_boss_gyth()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_gyth";
    pNewScript->GetAI = &GetAI_boss_gyth;
    pNewScript->RegisterSelf();
}
