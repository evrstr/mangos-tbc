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
SDName: Boss_Herod
SD%Complete: 95
SDComment: Should in addition spawn Myrmidons in the hallway outside
SDCategory: Scarlet Monastery
EndScriptData */

// 赫洛德（Herod）军械库首领战 AI（狂暴旋风斩与召唤血色狂徒学徒大军）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO              = -1189000,                      // 开战 台词广播 ID
    SAY_WHIRLWIND          = -1189001,                      // 旋风斩 台词广播 ID
    SAY_ENRAGE             = -1189002,                      // 狂暴 台词广播 ID
    SAY_KILL               = -1189003,                      // 击杀 台词广播 ID
    EMOTE_GENERIC_ENRAGED  = -1000003,                      // genericenraged 表情动作 ID
    GENERIC_EMOTE_FLEE     = 1150,                          // 表情flee 标识 ID

    SAY_TRAINEE_SPAWN      = -1189035,                      // trainee生成 台词广播 ID

    SPELL_RUSHINGCHARGE    = 8260,                          // rushingcharge 法术 ID
    SPELL_CLEAVE           = 15496, // 22540 in classic
    SPELL_WHIRLWIND        = 8989,                          // 旋风斩 法术 ID
    SPELL_FRENZY           = 8269,                          // 狂乱 法术 ID

    NPC_SCARLET_TRAINEE    = 6575                           // scarlettrainee Entry ID
};

// 首领战 AI：标识 ID（herod）
struct boss_herodAI : public ScriptedAI
{
    boss_herodAI(Creature* pCreature) : ScriptedAI(pCreature) {Reset();}

    bool m_bEnrage;                                         // Enrage 状态标记
    bool m_bTraineeSay;                                     // TraineeSay 状态标记

    uint32 m_uiCleaveTimer;                                 // Cleave 技能计时器
    uint32 m_uiWhirlwindTimer;                              // Whirlwind 技能计时器
    uint32 m_uiRushingChargeTimer;                          // RushingCharge 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_bTraineeSay = false;
        m_bEnrage     = false;

        m_uiCleaveTimer    = urand(3000, 15000);
        m_uiWhirlwindTimer = urand(10000, 15000);
        m_uiRushingChargeTimer = urand(0, 5000);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
    }

    void SummonedCreature(Creature* pSummoned)
    {
        // make first Scarlet Trainee say text
        if (pSummoned->GetEntry() == NPC_SCARLET_TRAINEE && !m_bTraineeSay)
        {
            DoScriptText(SAY_TRAINEE_SPAWN, pSummoned);
            m_bTraineeSay = true;
        }
    }

    void KilledUnit(Unit* /*pVictim*/) override
    {
        DoScriptText(SAY_KILL, m_creature);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        for (uint8 i = 0; i < 20; ++i)
            m_creature->SummonCreature(NPC_SCARLET_TRAINEE, 1939.18f, -431.58f, 17.09f, 6.22f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 600000);
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // If we are < 50% hp enrage
        if (!m_bEnrage && m_creature->GetHealthPercent() <= 50.0f && !m_creature->IsNonMeleeSpellCasted(false))
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FRENZY) == CAST_OK)
            {
                DoScriptText(EMOTE_GENERIC_ENRAGED, m_creature);
                DoScriptText(SAY_ENRAGE, m_creature);
                m_bEnrage = true;
            }
        }

        if (m_uiCleaveTimer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->GetVictim(), SPELL_CLEAVE);
            m_uiCleaveTimer = urand(8000, 24000);
        }
        else
            m_uiCleaveTimer -= uiDiff;

        if (m_uiWhirlwindTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_WHIRLWIND) == CAST_OK)
            {
                DoScriptText(SAY_WHIRLWIND, m_creature);
                m_uiWhirlwindTimer = urand(15000, 25000);
            }
        }
        else
            m_uiWhirlwindTimer -= uiDiff;

        if (m_uiRushingChargeTimer < uiDiff)
        {
            if (m_creature->IsInRange(m_creature->GetVictim(), 10.0f, 100.0f, true, true))
            {
                DoCastSpellIfCan(nullptr, SPELL_RUSHINGCHARGE);
                m_uiRushingChargeTimer = urand(5000, 10000);
            }
        }
        else
            m_uiRushingChargeTimer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_herod
UnitAI* GetAI_boss_herod(Creature* pCreature)
{
    return new boss_herodAI(pCreature);
}

// 随从/NPC AI：trainee（scarlet_trainee）
struct mob_scarlet_traineeAI : public npc_escortAI
{
    mob_scarlet_traineeAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        m_uiStartTimer = urand(1000, 6000);
        Reset();
    }

    uint32 m_uiStartTimer;                                  // Start 技能计时器
    bool m_hasFled;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_hasFled = false;
    }

    void WaypointReached(uint32 /*uiPointId*/) override {}

    void UpdateEscortAI(const uint32 uiDiff) override
    {
        if (m_uiStartTimer)
        {
            if (m_uiStartTimer <= uiDiff)
            {
                Start(true);
                m_uiStartTimer = 0;
            }
            else
                m_uiStartTimer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // Flee at 15% HP
        if (m_creature->GetHealthPercent() <= 15.0f && !m_hasFled)
        {
            if (m_creature->AI()->DoFlee())
            {
                DoBroadcastText(GENERIC_EMOTE_FLEE, m_creature);
                m_hasFled = true;
            }
        }

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：mob_scarlet_trainee
UnitAI* GetAI_mob_scarlet_trainee(Creature* pCreature)
{
    return new mob_scarlet_traineeAI(pCreature);
}

// 注册并载入脚本模块：boss_herod
void AddSC_boss_herod()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_herod";
    pNewScript->GetAI = &GetAI_boss_herod;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_scarlet_trainee";
    pNewScript->GetAI = &GetAI_mob_scarlet_trainee;
    pNewScript->RegisterSelf();
}
