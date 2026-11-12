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
SDName: Boss_Arcanist_Doan
SD%Complete: 100
SDComment:
SDCategory: Scarlet Monastery
EndScriptData */

// 奥法师杜安（Arcanist Doan）图书馆首领战 AI（奥术爆炸、魔法护盾与大火球）

#include "AI/ScriptDevAI/include/sc_common.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO                   = -1189019,                 // 开战 台词广播 ID
    SAY_SPECIALAE               = -1189020,                 // specialae 台词广播 ID

    SPELL_POLYMORPH             = 13323,                    // polymorph 法术 ID
    SPELL_SILENCE               = 8988,                     // 沉默 法术 ID
    SPELL_ARCANE_EXPLOSION      = 9433,                     // 奥术爆炸 法术 ID
    SPELL_DETONATION            = 9435,                     // detonation 法术 ID
    SPELL_ARCANE_BUBBLE         = 9438,                     // 奥术bubble 法术 ID
};

// 首领战 AI：杜安（arcanist_doan）
struct boss_arcanist_doanAI : public ScriptedAI
{
    boss_arcanist_doanAI(Creature* pCreature) : ScriptedAI(pCreature) {Reset();}

    uint32 m_uiPolymorphTimer;                              // Polymorph 技能计时器
    uint32 m_uiSilenceTimer;                                // Silence 技能计时器
    uint32 m_uiArcaneExplosionTimer;                        // ArcaneExplosion 技能计时器
    uint32 m_uiDetonationTimer;                             // Detonation 技能计时器
    bool bShielded;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiPolymorphTimer       = 15000;
        m_uiSilenceTimer         = 7500;
        m_uiArcaneExplosionTimer = urand(1000, 3000);
        m_uiDetonationTimer      = 0;
        bShielded                = false;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        if (m_uiDetonationTimer)
        {
            if (m_uiDetonationTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_DETONATION) == CAST_OK)
                {
                    DoScriptText(SAY_SPECIALAE, m_creature);
                    m_uiDetonationTimer = 0;
                }
            }
            else
                m_uiDetonationTimer -= uiDiff;
        }

        // Do not attack while having the bubble active
        if (m_creature->HasAura(SPELL_ARCANE_BUBBLE))
            return;

        // If we are <50% hp cast Arcane Bubble
        if (!bShielded && m_creature->GetHealthPercent() <= 50.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ARCANE_BUBBLE) == CAST_OK)
            {
                m_uiDetonationTimer = 1000;
                bShielded = true;
            }
        }

        if (m_uiPolymorphTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_POLYMORPH) == CAST_OK)
                    m_uiPolymorphTimer = 20000;
            }
        }
        else
            m_uiPolymorphTimer -= uiDiff;

        // Silence_Timer
        if (m_uiSilenceTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SILENCE) == CAST_OK)
                m_uiSilenceTimer = urand(15000, 22000);
        }
        else
            m_uiSilenceTimer -= uiDiff;

        // ArcaneExplosion_Timer
        if (m_uiArcaneExplosionTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ARCANE_EXPLOSION) == CAST_OK)
                m_uiArcaneExplosionTimer = urand(2500, 8500);
        }
        else
            m_uiArcaneExplosionTimer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_arcanist_doan
UnitAI* GetAI_boss_arcanist_doan(Creature* pCreature)
{
    return new boss_arcanist_doanAI(pCreature);
}

// 注册并载入脚本模块：boss_arcanist_doan
void AddSC_boss_arcanist_doan()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_arcanist_doan";
    pNewScript->GetAI = &GetAI_boss_arcanist_doan;
    pNewScript->RegisterSelf();
}
