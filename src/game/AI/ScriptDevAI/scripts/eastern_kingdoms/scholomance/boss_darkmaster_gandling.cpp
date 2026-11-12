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
SDName: Boss_Darkmaster_Gandling
SD%Complete: 100
SDComment:
SDCategory: Scholomance
EndScriptData */

// 院长加丁（Darkmaster Gandling）最终首领战 AI（传送玩家进独立牢房打小怪、奥术飞弹与暗影诅咒）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "scholomance.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SPELL_ARCANE_MISSILES          = 15790,                 // 奥术missiles 法术 ID
    SPELL_SHADOW_SHIELD            = 12040,                 // 暗影护盾 法术 ID
    SPELL_CURSE_SD                 = 18702,                 // 诅咒sd 法术 ID
    SPELL_SHADOW_PORTAL            = 17950                  // 暗影传送门 法术 ID
};

// 首领战 AI：加丁（darkmaster_gandling）
struct boss_darkmaster_gandlingAI : public ScriptedAI
{
    boss_darkmaster_gandlingAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;                          // 副本数据实例指针

    uint32 m_uiArcaneMissilesTimer;                         // ArcaneMissiles 技能计时器
    uint32 m_uiShadowShieldTimer;                           // ShadowShield 技能计时器
    uint32 m_uiCurseTimer;                                  // Curse 技能计时器
    uint32 m_uiTeleportTimer;                               // Teleport 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiArcaneMissilesTimer = 4500;
        m_uiShadowShieldTimer = 12000;
        m_uiCurseTimer = 2000;
        m_uiTeleportTimer = 16000;
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // Arcane Missiles Timer (奥术Missiles计时器)
        if (m_uiArcaneMissilesTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_ARCANE_MISSILES) == CAST_OK)
                m_uiArcaneMissilesTimer = 8000;
        }
        else
            m_uiArcaneMissilesTimer -= uiDiff;

        // Shadow Shield Timer (暗影护盾计时器)
        if (m_uiShadowShieldTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SHADOW_SHIELD) == CAST_OK)
                m_uiShadowShieldTimer = urand(14000, 28000);
        }
        else
            m_uiShadowShieldTimer -= uiDiff;

        // Curse Timer (诅咒计时器)
        if (m_uiCurseTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_CURSE_SD) == CAST_OK)
                m_uiCurseTimer = urand(15000, 27000);
        }
        else
            m_uiCurseTimer -= uiDiff;

        // Teleporting Random Target to one of the six pre boss rooms and spawn 3-4 skeletons near the gamer.
        // We will only telport if gandling has more than 3% of hp so teleported gamers can always loot.
        if (m_creature->GetHealthPercent() > 3.0f)
        {
            if (m_uiTeleportTimer < uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_SHADOW_PORTAL, SELECT_FLAG_PLAYER))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_SHADOW_PORTAL) == CAST_OK)
                    {
                        // remove threat
                        if (m_creature->getThreatManager().getThreat(pTarget))
                            m_creature->getThreatManager().modifyThreatPercent(pTarget, -100);

                        m_uiTeleportTimer = urand(20000, 35000);
                    }
                }
            }
            else
                m_uiTeleportTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_darkmaster_gandling
UnitAI* GetAI_boss_darkmaster_gandling(Creature* pCreature)
{
    return new boss_darkmaster_gandlingAI(pCreature);
}

// 注册并载入脚本模块：boss_darkmaster_gandling
void AddSC_boss_darkmaster_gandling()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_darkmaster_gandling";
    pNewScript->GetAI = &GetAI_boss_darkmaster_gandling;
    pNewScript->RegisterSelf();
}
