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
SDName: scholomance.cpp
SD%Complete: 100
SDComment: Creature support for 10498
SDCategory: Scholomance
EndScriptData */

// 通灵学院（Scholomance）六大讲堂小首领与杂项生物 AI

/* ContentData
npc_spectral_tutor
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"

/*######
## npc_spectral_tutor
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SPELL_IMAGE_PROJECTION          = 17651,                // 影像projection 法术 ID
    SPELL_IMAGE_PROJECTION_HEAL     = 17652,                // 影像projection治疗 法术 ID
    SPELL_IMAGE_PROJECTION_SUMMON   = 17653,                // 影像projection召唤 法术 ID
    SPELL_MANA_BURN                 = 17630,                // 法力burn 法术 ID
    SPELL_SILENCE                   = 12528,                // 沉默 法术 ID
};

// 随从/NPC AI：tutor（spectral_tutor）
struct npc_spectral_tutorAI : public ScriptedAI
{
    npc_spectral_tutorAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 m_uiManaBurnTimer;                               // ManaBurn 技能计时器
    uint32 m_uiSilenceTimer;                                // Silence 技能计时器
    uint32 m_uiProjectionTimer;                             // Projection 技能计时器
    uint32 m_uiProjEndTimer;                                // ProjEnd 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiProjEndTimer    = 0;
        m_uiManaBurnTimer   = urand(4000, 19000);
        m_uiSilenceTimer    = urand(0, 3000);
        m_uiProjectionTimer = urand(12000, 13000);
    }

    // 进入脱战闪避模式
    void EnterEvadeMode() override
    {
        if (m_uiProjEndTimer)
            return;

        ScriptedAI::EnterEvadeMode();
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        if (m_uiProjEndTimer)
        {
            if (m_uiProjEndTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_IMAGE_PROJECTION_HEAL) == CAST_OK)
                    m_uiProjEndTimer = 0;
            }
            else
                m_uiProjEndTimer -= uiDiff;

            // no other actions during Image Projection
            return;
        }

        if (m_uiManaBurnTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MANA_BURN) == CAST_OK)
                m_uiManaBurnTimer = urand(9000, 26000);
        }
        else
            m_uiManaBurnTimer -= uiDiff;

        if (m_uiSilenceTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SILENCE) == CAST_OK)
                m_uiSilenceTimer = urand(12000, 26000);
        }
        else
            m_uiSilenceTimer -= uiDiff;

        if (m_uiProjectionTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_IMAGE_PROJECTION) == CAST_OK)
            {
                DoCastSpellIfCan(m_creature, SPELL_IMAGE_PROJECTION_SUMMON, CAST_TRIGGERED);
                m_uiProjEndTimer = 1000;
                m_uiProjectionTimer = urand(18000, 25000);
            }
        }
        else
            m_uiProjectionTimer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：npc_spectral_tutor
UnitAI* GetAI_npc_spectral_tutor(Creature* pCreature)
{
    return new npc_spectral_tutorAI(pCreature);
}

// 注册并载入脚本模块：scholomance
void AddSC_scholomance()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_spectral_tutor";
    pNewScript->GetAI = &GetAI_npc_spectral_tutor;
    pNewScript->RegisterSelf();
}
