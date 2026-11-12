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
SDName: Boss_Maleki_the_Pallid
SD%Complete: 100
SDComment:
SDCategory: Stratholme
EndScriptData */

// 苍白的玛勒基（Maleki the Pallid）首领战 AI（冰霜暗影箭雨、冰冻之墓与吸取生命）

#include "AI/ScriptDevAI/include/sc_common.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SPELL_FROST_ARMOR   = 12556,                            // 冰霜armor 法术 ID
    SPELL_FROSTBOLT     = 17503,                            // frostbolt 法术 ID
    SPELL_DRAIN_LIFE    = 17238,                            // 吸取生命 法术 ID
    SPELL_DRAIN_MANA    = 17243,                            // 吸取法力 法术 ID
    SPELL_ICE_TOMB      = 16869                             // ice墓穴 法术 ID
};

// 首领战 AI：thepallid（maleki_the_pallid）
struct boss_maleki_the_pallidAI : public ScriptedAI
{
    boss_maleki_the_pallidAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 m_uiDrainManaTimer;                              // DrainMana 技能计时器
    uint32 m_uiFrostboltTimer;                              // Frostbolt 技能计时器
    uint32 m_uiIceTombTimer;                                // IceTomb 技能计时器
    uint32 m_uiDrainLifeTimer;                              // DrainLife 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiDrainManaTimer  = 30000;
        m_uiFrostboltTimer  = 0;
        m_uiIceTombTimer    = 15000;
        m_uiDrainLifeTimer  = 20000;

        DoCastSpellIfCan(nullptr, SPELL_FROST_ARMOR, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // Frostbolt
        if (m_uiFrostboltTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_FROSTBOLT) == CAST_OK)
                    m_uiFrostboltTimer = urand(3000, 4000);
            }
        }
        else
            m_uiFrostboltTimer -= uiDiff;

        // IceTomb
        if (m_uiIceTombTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_ICE_TOMB) == CAST_OK)
                {
                    m_creature->getThreatManager().modifyThreatPercent(pTarget, -100);
                    m_uiIceTombTimer = urand(15000, 20000);
                }
            }
        }
        else
            m_uiIceTombTimer -= uiDiff;

        // Drain Life (吸取生命)
        if (m_uiDrainLifeTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_DRAIN_LIFE) == CAST_OK)
                    m_uiDrainLifeTimer = urand(15000, 20000);
            }
        }
        else
            m_uiDrainLifeTimer -= uiDiff;

        // Drain mana (吸取法力)
        if (m_uiDrainManaTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_DRAIN_MANA, SELECT_FLAG_POWER_MANA))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_DRAIN_MANA) == CAST_OK)
                    m_uiDrainManaTimer = urand(20000, 30000);
            }
        }
        else
            m_uiDrainManaTimer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_maleki_the_pallid
UnitAI* GetAI_boss_maleki_the_pallid(Creature* pCreature)
{
    return new boss_maleki_the_pallidAI(pCreature);
}

// 注册并载入脚本模块：boss_maleki_the_pallid
void AddSC_boss_maleki_the_pallid()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_maleki_the_pallid";
    pNewScript->GetAI = &GetAI_boss_maleki_the_pallid;
    pNewScript->RegisterSelf();
}
