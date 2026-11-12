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
SDName: Boss_jandicebarov
SD%Complete: 90
SDComment: Jandice's Illusions are missing their AoE immunity, they should only be vunerable to direct damage (missing core feature ?)
SDCategory: Scholomance
EndScriptData */

// 詹迪斯·巴罗夫（Jandice Barov）首领战 AI（镜像分身术迷阵与物理近战打击）

#include "AI/ScriptDevAI/include/sc_common.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SPELL_CURSE_OF_BLOOD        = 16098,                    // 诅咒of鲜血 法术 ID
    SPELL_SPREAD_ILLUSIONS      = 17772,                    // spreadillusions 法术 ID
    SPELL_SUMMON_ILLUSIONS      = 17773,                    // 召唤illusions 法术 ID
    SPELL_SPREAD_JANDICE        = 17774,                    // spreadjandice 法术 ID
    SPELL_BANISH                = 8994,                     // banish 法术 ID
};

// 首领战 AI：标识 ID（jandicebarov）
struct boss_jandicebarovAI : public ScriptedAI
{
    boss_jandicebarovAI(Creature* pCreature) : ScriptedAI(pCreature) {Reset();}

    uint32 m_uiCurseOfBloodTimer;                           // CurseOfBlood 技能计时器
    uint32 m_uiIllusionTimer;                               // Illusion 技能计时器
    uint32 m_uiBanishTimer;                                 // Banish 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiCurseOfBloodTimer = 5000;
        m_uiIllusionTimer = 15000;
        m_uiBanishTimer = urand(9000, 13000);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* pSummoned) override
    {
        pSummoned->CastSpell(pSummoned, SPELL_SPREAD_ILLUSIONS, TRIGGERED_OLD_TRIGGERED);
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        // Return since we have no target (无有效目标，退出更新)
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // CurseOfBlood_Timer
        if (m_uiCurseOfBloodTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_CURSE_OF_BLOOD) == CAST_OK)
                m_uiCurseOfBloodTimer = urand(30000, 35000);
        }
        else
            m_uiCurseOfBloodTimer -= uiDiff;

        // Banish
        if (m_uiBanishTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_BANISH) == CAST_OK)
                    m_uiBanishTimer = urand(17000, 21000);
            }
        }
        else
            m_uiBanishTimer -= uiDiff;

        // Illusion_Timer
        if (m_uiIllusionTimer < uiDiff)
        {
            // Summon illusions and becomes invisible for 8 seconds while hiding into the illusions (召唤 illusions and becomes invisible for 8 seconds while hiding into the illusions)
            if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_ILLUSIONS) == CAST_OK)
            {
                m_uiIllusionTimer = 25000;
                DoCastSpellIfCan(m_creature, SPELL_SPREAD_JANDICE);
            }
        }
        else
            m_uiIllusionTimer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_jandicebarov
UnitAI* GetAI_boss_jandicebarov(Creature* pCreature)
{
    return new boss_jandicebarovAI(pCreature);
}

// 注册并载入脚本模块：boss_jandicebarov
void AddSC_boss_jandicebarov()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_jandice_barov";
    pNewScript->GetAI = &GetAI_boss_jandicebarov;
    pNewScript->RegisterSelf();
}
