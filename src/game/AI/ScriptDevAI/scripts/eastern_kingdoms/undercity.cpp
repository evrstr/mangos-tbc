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
SDName: Undercity
SD%Complete: 95
SDComment: Quest support: 9180(post-event).
SDCategory: Undercity
EndScriptData */

// 幽暗城（Undercity）部落主城 NPC 与皇家幽灵药剂师脚本

/* ContentData
npc_lady_sylvanas_windrunner
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/TimerAI.h"

/*######
## npc_lady_sylvanas_windrunner
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    EMOTE_LAMENT_START          = -1000193,                 // lament开始 表情动作 ID
    SAY_LAMENT_END              = -1000196,                 // lamentend 台词广播 ID
    EMOTE_LAMENT_END            = -1000197,                 // lamentend 表情动作 ID
    SAY_AMBASSADOR              = -1001277,                 // ambassador 台词广播 ID

    SPELL_HIGHBORNE_AURA        = 37090,                    // highborne光环 法术 ID
    SPELL_SYLVANAS_CAST         = 36568,                    // sylvanascast 法术 ID
    SPELL_RIBBON_OF_SOULS       = 37099,                    // ribbonofsouls 法术 ID

    NPC_AMBASSADOR              = 16287,                    // ambassador Entry ID
    NPC_HIGHBORNE_LAMENTER      = 21628,                    // highbornelamenter Entry ID
    NPC_HIGHBORNE_BUNNY         = 21641,                    // highbornebunny Entry ID

    QUEST_ID_JOURNEY_UNDERCITY  = 9180,                     // IDjourneyundercity 任务 ID

    MAX_LAMENTERS               = 4,                        // lamenters 标识 ID
};

static const float aHighborneLoc[MAX_LAMENTERS][5] =
{
    {1286.178f, 312.7989f, -61.15427f, 0.594699f, 1.0f},
    {1287.959f, 311.5340f, -61.26538f, 1.140551f, 2.0f},
    {1290.516f, 311.1839f, -61.20983f, 1.521469f, 3.0f},
    {1292.552f, 311.8947f, -61.07092f, 1.953438f, 4.0f},
};

// 枚举定义：法术技能、生物与环境事件标识
enum SylvanasActions
{
    SYLV_AMBASSADOR_WHISPER,
    SYLV_SUMMON_HIGHBORNE,
    SYLV_HIGHBORNE_BUNNY,
    SYLV_LAMENT_END,
    SYLV_KNEEL,
    SYLV_STAND,
};

// 随从/NPC AI：sylvanaswindrunner（lady_sylvanas_windrunner）
struct npc_lady_sylvanas_windrunnerAI : public ScriptedAI
{
    npc_lady_sylvanas_windrunnerAI(Creature* pCreature) : ScriptedAI(pCreature) 
    {
        AddCustomAction(SYLV_AMBASSADOR_WHISPER, true, [&]
        { 
            if (Creature* ambassador = GetClosestCreatureWithEntry(m_creature, NPC_AMBASSADOR, 50.f))
                if (Player* player = m_creature->GetMap()->GetPlayer(m_guidCurrentPlayer))
                    DoScriptText(SAY_AMBASSADOR, ambassador, player);
        });
        AddCustomAction(SYLV_SUMMON_HIGHBORNE, true, [&]
        {
            for (auto& i : aHighborneLoc)
            {
                if (Creature* summon = m_creature->SummonCreature(NPC_HIGHBORNE_LAMENTER, i[0], i[1], i[2], i[3], TEMPSPAWN_TIMED_DESPAWN, 155000, false, false))
                {
                    float fX, fY, fZ;
                    summon->GetContactPoint(m_creature, fX, fY, fZ, 0.01f);
                    summon->GetMotionMaster()->MovePoint(0, fX, fY, fZ + 5.0f);
                }
            }

            ResetTimer(SYLV_HIGHBORNE_BUNNY, 2000);
        });
        AddCustomAction(SYLV_HIGHBORNE_BUNNY, true, [&]
        {
            float fX, fY, fZ;
            m_creature->GetRandomPoint(m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), 20.0f, fX, fY, fZ);
            m_creature->SummonCreature(NPC_HIGHBORNE_BUNNY, fX, fY, fZ + 15.0f, 0, TEMPSPAWN_TIMED_DESPAWN, 3000);

            if (m_bLamentInProgress)
                ResetTimer(SYLV_HIGHBORNE_BUNNY, 2000);
        });
        AddCustomAction(SYLV_LAMENT_END, true, [&]
        {
            ResetTimer(SYLV_KNEEL, 3000);
            DoScriptText(SAY_LAMENT_END, m_creature);
        });
        AddCustomAction(SYLV_KNEEL, true, [&]
        {
            ResetTimer(SYLV_STAND, 4000);
            m_creature->SetStandState(UNIT_STAND_STATE_KNEEL);
            DoScriptText(EMOTE_LAMENT_END, m_creature);
        });
        AddCustomAction(SYLV_STAND, true, [&]
        {
            m_bLamentInProgress = false;
            m_creature->SetStandState(UNIT_STAND_STATE_STAND);
        });
        Reset(); 
    }

    bool m_bLamentInProgress;                               // LamentInProgress 状态标记

    ObjectGuid m_guidCurrentPlayer;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_bLamentInProgress = false;
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() == NPC_HIGHBORNE_BUNNY)
            pSummoned->CastSpell(pSummoned, SPELL_RIBBON_OF_SOULS, TRIGGERED_NONE);
        else if (pSummoned->GetEntry() == NPC_HIGHBORNE_LAMENTER)
        {
            pSummoned->CastSpell(pSummoned, SPELL_HIGHBORNE_AURA, TRIGGERED_NONE);
        }
    }

    void DoStartLamentEvent(ObjectGuid playerGuid)
    {
        if (!m_bLamentInProgress)
        {
            m_bLamentInProgress = true;
            DoScriptText(EMOTE_LAMENT_START, m_creature);
            DoCastSpellIfCan(m_creature, SPELL_SYLVANAS_CAST);
            ResetTimer(SYLV_AMBASSADOR_WHISPER, 3000);
            ResetTimer(SYLV_SUMMON_HIGHBORNE, 10000);
            ResetTimer(SYLV_LAMENT_END, 172500);
            m_guidCurrentPlayer = playerGuid;
        }
    }
};

// 创建并返回 AI 实例工厂函数：npc_lady_sylvanas_windrunner
UnitAI* GetAI_npc_lady_sylvanas_windrunner(Creature* pCreature)
{
    return new npc_lady_sylvanas_windrunnerAI(pCreature);
}

bool QuestRewarded_npc_lady_sylvanas_windrunner(Player* pPlayer, Creature* pCreature, Quest const* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_ID_JOURNEY_UNDERCITY)
    {
        if (npc_lady_sylvanas_windrunnerAI* pSylvanAI = dynamic_cast<npc_lady_sylvanas_windrunnerAI*>(pCreature->AI()))
            pSylvanAI->DoStartLamentEvent(pPlayer->GetObjectGuid());
    }

    return true;
}

// 注册并载入脚本模块：undercity
void AddSC_undercity()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_lady_sylvanas_windrunner";
    pNewScript->GetAI = &GetAI_npc_lady_sylvanas_windrunner;
    pNewScript->pQuestRewardedNPC = &QuestRewarded_npc_lady_sylvanas_windrunner;
    pNewScript->RegisterSelf();
}
