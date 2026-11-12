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
SDName: Boss_Emperor_Dagran_Thaurissan
SD%Complete: 90
SDComment: With script for Moria
SDCategory: Blackrock Depths
EndScriptData */

// 达格兰·索瑞森大帝与铁炉堡公主麦拉·铜须首领战 AI

#include "AI/ScriptDevAI/include/sc_common.h"
#include "blackrock_depths.h"

// 枚举定义：法术技能、生物与环境事件标识
enum eEmperor
{
    FACTION_NEUTRAL             = 734,                      // neutral 阵营 ID
    YELL_AGGRO_1                = -1230001,                 // 开战1 大喊台词广播 ID
    YELL_AGGRO_2                = -1230064,                 // 开战2 大喊台词广播 ID
    YELL_AGGRO_3                = -1230065,                 // 开战3 大喊台词广播 ID
    YELL_SLAY                   = -1230002,                 // 击杀 大喊台词广播 ID

    SPELL_HANDOFTHAURISSAN      = 17492,                    // handofthaurissan 法术 ID
    SPELL_AVATAROFFLAME         = 15636                     // avatarofflame 法术 ID
};

// 首领战 AI：dagran索瑞森（emperor_dagran_thaurissan）
struct boss_emperor_dagran_thaurissanAI : public ScriptedAI
{
    boss_emperor_dagran_thaurissanAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;                          // 副本数据实例指针

    uint32 m_uiHandOfThaurissanTimer;                       // HandOfThaurissan 技能计时器
    uint32 m_uiAvatarOfFlameTimer;                          // AvatarOfFlame 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiHandOfThaurissanTimer = 4000;
        m_uiAvatarOfFlameTimer    = 25000;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        switch (urand(0, 2))
        {
            case 0: DoScriptText(YELL_AGGRO_1, m_creature); break;
            case 1: DoScriptText(YELL_AGGRO_2, m_creature); break;
            case 2: DoScriptText(YELL_AGGRO_3, m_creature); break;
        }
        m_creature->CallForHelp(VISIBLE_RANGE);
    }

    void JustDied(Unit* /*pVictim*/) override
    {
        if (!m_pInstance)
            return;

        if (Creature* pPrincess = m_pInstance->GetSingleCreatureFromStorage(NPC_PRINCESS))
        {
            // check if we didn't update the entry
            if (pPrincess->GetEntry() != NPC_PRINCESS)
                return;

            if (pPrincess->IsAlive())
            {
                pPrincess->SetFactionTemporary(FACTION_NEUTRAL, TEMPFACTION_NONE);
                pPrincess->AI()->EnterEvadeMode();
            }
        }
    }

    void KilledUnit(Unit* /*pVictim*/) override
    {
        DoScriptText(YELL_SLAY, m_creature);
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        if (m_uiHandOfThaurissanTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_HANDOFTHAURISSAN) == CAST_OK)
                    m_uiHandOfThaurissanTimer = urand(5, 10) * 1000;
            }
        }
        else
            m_uiHandOfThaurissanTimer -= uiDiff;

        // AvatarOfFlame_Timer
        if (m_uiAvatarOfFlameTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_AVATAROFFLAME) == CAST_OK)
                m_uiAvatarOfFlameTimer = 18000;
        }
        else
            m_uiAvatarOfFlameTimer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_emperor_dagran_thaurissan
UnitAI* GetAI_boss_emperor_dagran_thaurissan(Creature* pCreature)
{
    return new boss_emperor_dagran_thaurissanAI(pCreature);
}

/*######
## boss_moira_bronzebeard
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum ePrincess
{
    SPELL_HEAL                  = 15586,                    // 治疗 法术 ID
    SPELL_RENEW                 = 10929,                    // renew 法术 ID
    SPELL_SHIELD                = 10901,                    // 护盾 法术 ID
    SPELL_MINDBLAST             = 15587,                    // mindblast 法术 ID
    SPELL_SHADOWWORDPAIN        = 15654,                    // shadowwordpain 法术 ID
    SPELL_SMITE                 = 10934,                    // 重拳先生 法术 ID
    SPELL_SHADOW_BOLT           = 15537,                    // 暗影箭 法术 ID
    SPELL_OPEN_PORTAL           = 13912                     // open传送门 法术 ID
};

// 首领战 AI：bronzebeard（moira_bronzebeard）
struct boss_moira_bronzebeardAI : public ScriptedAI
{
    boss_moira_bronzebeardAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;                          // 副本数据实例指针

    uint32 m_uiHealTimer;                                   // Heal 技能计时器
    uint32 m_uiMindBlastTimer;                              // MindBlast 技能计时器
    uint32 m_uiShadowWordPainTimer;                         // ShadowWordPain 技能计时器
    uint32 m_uiSmiteTimer;                                  // Smite 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiHealTimer           = 12000;                             // These times are probably wrong (此时间参数可能需进一步校对)
        m_uiMindBlastTimer      = 16000;
        m_uiShadowWordPainTimer = 2000;
        m_uiSmiteTimer          = 8000;

        m_attackDistance = 25.0f;
        m_meleeEnabled = false;
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_pInstance)
        {
            if (Creature* pEmperor = m_pInstance->GetSingleCreatureFromStorage(NPC_EMPEROR))
            {
                // if evade, then check if he is alive. If not, start make portal
                if (!pEmperor->IsAlive())
                    DoCastSpellIfCan(m_creature, SPELL_OPEN_PORTAL);
            }
        }
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        // Return since we have no target (无有效目标，退出更新)
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // MindBlast_Timer
        if (m_uiMindBlastTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MINDBLAST) == CAST_OK)
                m_uiMindBlastTimer = 14000;
        }
        else
            m_uiMindBlastTimer -= uiDiff;

        // ShadowWordPain_Timer (暗言术痛计时器)
        if (m_uiShadowWordPainTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_SHADOWWORDPAIN) == CAST_OK)
                m_uiShadowWordPainTimer = 18000;
        }
        else
            m_uiShadowWordPainTimer -= uiDiff;

        // Smite_Timer
        if (m_uiSmiteTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_SMITE) == CAST_OK)
                m_uiSmiteTimer = 10000;
        }
        else
            m_uiSmiteTimer -= uiDiff;

        // Heal_Timer
        if (m_uiHealTimer < uiDiff)
        {
            if (Creature* pEmperor = m_pInstance->GetSingleCreatureFromStorage(NPC_EMPEROR))
            {
                if (pEmperor->IsAlive() && pEmperor->GetHealthPercent() != 100.0f)
                {
                    if (DoCastSpellIfCan(pEmperor, SPELL_HEAL) == CAST_OK)
                        m_uiHealTimer = 10000;
                }
            }
        }
        else
            m_uiHealTimer -= uiDiff;

        // No meele?
    }
};

// 创建并返回 AI 实例工厂函数：boss_moira_bronzebeard
UnitAI* GetAI_boss_moira_bronzebeard(Creature* pCreature)
{
    return new boss_moira_bronzebeardAI(pCreature);
}

// 注册并载入脚本模块：boss_draganthaurissan
void AddSC_boss_draganthaurissan()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_emperor_dagran_thaurissan";
    pNewScript->GetAI = &GetAI_boss_emperor_dagran_thaurissan;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_moira_bronzebeard";
    pNewScript->GetAI = &GetAI_boss_moira_bronzebeard;
    pNewScript->RegisterSelf();
}
