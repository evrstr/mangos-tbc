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
SDName: Ruins of Ahn'Qiraj
SD%Complete: 40
SDComment:
SDCategory: Ruins of Ahn'Qiraj
EndScriptData */

/**
 * 安其拉废墟：小怪与相关事件机制脚本
 */

/* ContentData
mob_anubisath_guardian
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"

/*######
## mob_anubisath_guardian
######*/
// 常量与事件枚举
enum
{
    SPELL_METEOR                 = 24340,                   // 技能法术：流星
    SPELL_PLAGUE                 = 22997,                   // 技能法术 ID
    SPELL_SHADOW_STORM           = 26546,                   // 技能法术 ID
    SPELL_THUNDER_CLAP           = 26554,                   // 技能法术 ID
    SPELL_REFLECT_ARFR           = 13022,                   // 技能法术：法术反射
    SPELL_REFLECT_FSSH           = 19595,                   // 技能法术：法术反射
    SPELL_ENRAGE                 = 8599,                    // 技能法术：激怒/狂暴
    SPELL_EXPLODE                = 25698,                   // 技能法术 ID

    SPELL_SUMMON_ANUB_SWARMGUARD = 17430,                   // 技能法术 ID
    SPELL_SUMMON_ANUB_WARRIOR    = 17431,                   // 技能法术 ID

    EMOTE_FRENZY                 = -1000002,                // 剧情台词/喊话文本 ID

    NPC_ANUB_WARRIOR             = 15537,                   // NPC 条目 ID
    NPC_ANUB_SWARM               = 15538
};

// mob_anubisath_guardianAI AI 逻辑实现
struct mob_anubisath_guardianAI : public ScriptedAI
{
    mob_anubisath_guardianAI(Creature* pCreature) : ScriptedAI(pCreature) {Reset();}

    uint32 m_uiSpell1;
    uint32 m_uiSpell2;
    uint32 m_uiSpell3;
    uint32 m_uiSpell4;
    uint32 m_uiSpell5;

    uint32 m_uiSpell1Timer;
    uint32 m_uiSpell2Timer;
    uint32 m_uiSpell5Timer;

    uint8 m_uiSummonCount;

    bool m_bIsEnraged;

    void Reset() override                                   // 重置战斗状态与计时器
    {
        m_uiSpell1 = urand(0, 1) ? SPELL_METEOR : SPELL_PLAGUE;
        m_uiSpell2 = urand(0, 1) ? SPELL_SHADOW_STORM : SPELL_THUNDER_CLAP;
        m_uiSpell3 = urand(0, 1) ? SPELL_REFLECT_ARFR : SPELL_REFLECT_FSSH;
        m_uiSpell4 = urand(0, 1) ? SPELL_ENRAGE : SPELL_EXPLODE;
        m_uiSpell5 = urand(0, 1) ? SPELL_SUMMON_ANUB_SWARMGUARD : SPELL_SUMMON_ANUB_WARRIOR;

        m_uiSpell1Timer = 10000;
        m_uiSpell2Timer = 20000;
        m_uiSpell5Timer = 10000;
        m_uiSummonCount = 0;
        m_bIsEnraged    = false;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        // spell reflection (法术)
        DoCastSpellIfCan(m_creature, m_uiSpell3);
    }

    void JustSummoned(Creature* pSummoned) override         // 召唤仆从成功回调
    {
        pSummoned->AI()->AttackStart(m_creature->GetVictim());
        ++m_uiSummonCount;
    }

    void SummonedCreatureDespawn(Creature* /*pDespawned*/) override
    {
        --m_uiSummonCount;
    }

    void DamageTaken(Unit* /*dealer*/, uint32& /*damage*/, DamageEffectType /*damagetype*/, SpellEntry const* /*spellInfo*/) override
    {
        // when we reach 10% of HP explode or enrage (说明)
        if (!m_bIsEnraged && m_creature->GetHealthPercent() < 10.0f)
        {
            if (m_uiSpell4 == SPELL_ENRAGE)
            {
                DoScriptText(EMOTE_FRENZY, m_creature);
                DoCastSpellIfCan(m_creature, m_uiSpell4);
                m_bIsEnraged = true;
            }
            else
                DoCastSpellIfCan(m_creature->GetVictim(), m_uiSpell4);
        }
    }

    void UpdateAI(const uint32 uiDiff) override             // AI 核心帧更新处理
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // Meteor or Plague (说明)
        if (m_uiSpell1Timer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->GetVictim(), m_uiSpell1);
            m_uiSpell1Timer = 15000;
        }
        else
            m_uiSpell1Timer -= uiDiff;

        // Shadow Storm or Thunder Clap (说明)
        if (m_uiSpell2Timer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->GetVictim(), m_uiSpell2);
            m_uiSpell2Timer = 15000;
        }
        else
            m_uiSpell2Timer -= uiDiff;

        // summon Anubisath Swarmguard or Anubisath Warrior (召唤)
        if (m_uiSpell5Timer < uiDiff)
        {
            // change for summon spell (召唤法术)
            if (m_uiSummonCount < 4)
                DoCastSpellIfCan(m_creature->GetVictim(), m_uiSpell5);

            m_uiSpell5Timer = 15000;
        }
        else
            m_uiSpell5Timer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

UnitAI* GetAI_mob_anubisath_guardian(Creature* pCreature) // 创建并返回 AI 实例
{
    return new mob_anubisath_guardianAI(pCreature);
}

void AddSC_ruins_of_ahnqiraj() // 注册脚本到系统管理器
{
    Script* pNewScript = new Script;
    pNewScript->Name = "mob_anubisath_guardian";
    pNewScript->GetAI = &GetAI_mob_anubisath_guardian;
    pNewScript->RegisterSelf();
}
