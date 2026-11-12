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
SDName: Teldrassil
SD%Complete: 100
SDComment: Quest support: 938
SDCategory: Teldrassil
EndScriptData */

/**
 * 泰达希尔 (Teldrassil) 暗夜精灵起始区任务脚本
 */

/* ContentData
npc_mist
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/follower_ai.h"

/*####
# npc_mist
####*/

// 常量与事件枚举
enum
{
    SAY_AT_HOME             = -1000323,                     // 剧情台词/喊话文本 ID
    EMOTE_AT_HOME           = -1000324,                     // 剧情台词/喊话文本 ID
    QUEST_MIST              = 938,                          // 任务 ID
    NPC_ARYNIA              = 3519,                         // NPC 条目 ID
    FACTION_DARNASSUS       = 79
};

// npc_mistAI NPC 脚本 AI
struct npc_mistAI : public FollowerAI
{
    npc_mistAI(Creature* pCreature) : FollowerAI(pCreature) { Reset(); }

    void Reset() override { }                               // 重置战斗状态与计时器

    void MoveInLineOfSight(Unit* pWho) override
    {
        FollowerAI::MoveInLineOfSight(pWho);

        if (!m_creature->GetVictim() && !HasFollowState(STATE_FOLLOW_COMPLETE) && pWho->GetEntry() == NPC_ARYNIA)
        {
            if (m_creature->IsWithinDistInMap(pWho, 10.0f))
            {
                DoScriptText(SAY_AT_HOME, pWho);
                DoComplete();
            }
        }
    }

    void DoComplete()
    {
        DoScriptText(EMOTE_AT_HOME, m_creature);

        if (Player* pPlayer = GetLeaderForFollower())
        {
            if (pPlayer->GetQuestStatus(QUEST_MIST) == QUEST_STATUS_INCOMPLETE)
                pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_MIST, m_creature);
        }

        // The follow is over (and for later development, run off to the woods before really end) (说明)
        SetFollowComplete();
    }

    // call not needed here, no known abilities (说明)
    /*void UpdateFollowerAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        DoMeleeAttackIfReady();
    }*/
};

UnitAI* GetAI_npc_mist(Creature* pCreature) // 创建并返回 AI 实例
{
    return new npc_mistAI(pCreature);
}

bool QuestAccept_npc_mist(Player* pPlayer, Creature* pCreature, const Quest* pQuest) // 接受任务处理
{
    if (pQuest->GetQuestId() == QUEST_MIST)
    {
        if (npc_mistAI* pMistAI = dynamic_cast<npc_mistAI*>(pCreature->AI()))
            pMistAI->StartFollow(pPlayer, FACTION_DARNASSUS, pQuest);
    }

    return true;
}

void AddSC_teldrassil() // 注册脚本到系统管理器
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_mist";
    pNewScript->GetAI = &GetAI_npc_mist;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_mist;
    pNewScript->RegisterSelf();
}
