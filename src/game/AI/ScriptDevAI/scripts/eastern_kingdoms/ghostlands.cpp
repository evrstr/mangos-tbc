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
SDName: Ghostlands
SD%Complete: 100
SDComment: Quest support: 9212.
SDCategory: Ghostlands
EndScriptData */

// 幽魂之地（Ghostlands）血精灵区域任务与达尔坎·德拉希尔脚本

/* ContentData
npc_ranger_lilatha
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

/*######
## npc_ranger_lilatha
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_START                   = 15894,                    // 开始 台词广播 ID
    SAY_PROGRESS1               = 12187,                    // progress1 台词广播 ID
    SAY_PROGRESS2               = 12188,                    // progress2 台词广播 ID
    SAY_PROGRESS3               = 12189,                    // progress3 台词广播 ID
    SAY_END1                    = 12191,                    // end1 台词广播 ID
    SAY_END2                    = 12190,                    // end2 台词广播 ID
    CAPTAIN_ANSWER              = 12193,                    // answer 标识 ID

    WAYPOINT_PATH_ID            = 16295,                    // 路径ID 标识 ID
    QUEST_CATACOMBS             = 9212,                     // catacombs 任务 ID
    GO_CAGE                     = 181152,                   // cage 游戏对象 Entry ID
    NPC_CAPTAIN_HELIOS          = 16220,                    // captainhelios Entry ID
    NPC_MUMMIFIED_HEADHUNTER    = 16342,                    // mummifiedheadhunter Entry ID
    NPC_SHADOWPINE_ORACLE       = 16343,                    // shadowpineoracle Entry ID
};

// 随从/NPC AI：lilatha（ranger_lilatha）
struct npc_ranger_lilathaAI : public npc_escortAI
{
    npc_ranger_lilathaAI(Creature* pCreature) : npc_escortAI(pCreature) { Reset(); }

    ObjectGuid m_goCageGuid;                                // goCage 实体 GUID
    ObjectGuid m_heliosGuid;                                // helios 实体 GUID

    void JustRespawned() override
    {
        npc_escortAI::JustRespawned();
        m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
        m_creature->SetStandState(UNIT_STAND_STATE_KNEEL);
    }

    void MoveInLineOfSight(Unit* pUnit) override
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING))
        {
            if (!m_heliosGuid && pUnit->GetEntry() == NPC_CAPTAIN_HELIOS)
            {
                if (m_creature->IsWithinDistInMap(pUnit, 30.0f))
                    m_heliosGuid = pUnit->GetObjectGuid();
            }
        }

        npc_escortAI::MoveInLineOfSight(pUnit);
    }

    void WaypointReached(uint32 i) override
    {
        Player* pPlayer = GetPlayerForEscort();

        if (!pPlayer)
            return;

        switch (i)
        {
            case 1:
                if (GameObject* pGoTemp = GetClosestGameObjectWithEntry(m_creature, GO_CAGE, 10.0f))
                {
                    m_goCageGuid = pGoTemp->GetObjectGuid();
                    pGoTemp->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case 2:
                if (GameObject* pGo = m_creature->GetMap()->GetGameObject(m_goCageGuid))
                    pGo->SetGoState(GO_STATE_READY);
                break;
            case 6:
                DoBroadcastText(SAY_PROGRESS1, m_creature, pPlayer);
                break;
            case 12:
                DoBroadcastText(SAY_PROGRESS2, m_creature, pPlayer);
                break;
            case 19:
                DoBroadcastText(SAY_PROGRESS3, m_creature, pPlayer);
                if (Creature* pSum1 = m_creature->SummonCreature(NPC_MUMMIFIED_HEADHUNTER, 7625.81103515625f, -7531.75439453125f, 152.361358642578125f, 1.117010712623596191f, TEMPSPAWN_DEAD_DESPAWN, 0))
                    pSum1->AI()->AttackStart(m_creature);
                if (Creature* pSum2 = m_creature->SummonCreature(NPC_SHADOWPINE_ORACLE, 7622.45849609375f, -7529.16943359375f, 152.9869384765625f, 0.907571196556091308f, TEMPSPAWN_DEAD_DESPAWN, 0))
                    pSum2->AI()->AttackStart(pPlayer);
                break;
            case 20:
                SetRun();
                break;
            case 26:
                SetRun(false);
                break;
            case 31:
                pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_CATACOMBS, m_creature);
                break;
            case 33:
                DoBroadcastText(SAY_END1, m_creature, pPlayer);
                break;
            break;
            case 34:
            {
                DoBroadcastText(SAY_END2, m_creature, pPlayer);
                m_creature->ForcedDespawn(10000u);
                m_creature->SetRespawnDelay(1, true);
                if (Creature* pHelios = m_creature->GetMap()->GetCreature(m_heliosGuid))
                    DoBroadcastText(CAPTAIN_ANSWER, pHelios, m_creature);
            }
            break;
        }
    }

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        if (!HasEscortState(STATE_ESCORT_ESCORTING))
        {
            m_goCageGuid.Clear();
            m_heliosGuid.Clear();
        }
    }
};

// 任务接取回调函数：npc_ranger_lilatha
bool QuestAccept_npc_ranger_lilatha(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_CATACOMBS)
    {
        pCreature->SetStandState(UNIT_STAND_STATE_STAND);
        pCreature->SetFactionTemporary(FACTION_ESCORT_N_NEUTRAL_ACTIVE, TEMPFACTION_RESTORE_RESPAWN | TEMPFACTION_TOGGLE_IMMUNE_TO_PLAYER | TEMPFACTION_TOGGLE_IMMUNE_TO_NPC);
        DoBroadcastText(SAY_START, pCreature, pPlayer);

        pCreature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        pCreature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
        if (npc_ranger_lilathaAI* pEscortAI = dynamic_cast<npc_ranger_lilathaAI*>(pCreature->AI()))
            pEscortAI->Start(false, pPlayer, pQuest, false, false, WAYPOINT_PATH_ID);
    }
    return true;
}

// 注册并载入脚本模块：ghostlands
void AddSC_ghostlands()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_ranger_lilatha";
    pNewScript->GetAI = &GetNewAIInstance<npc_ranger_lilathaAI>;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_ranger_lilatha;
    pNewScript->RegisterSelf();
}
