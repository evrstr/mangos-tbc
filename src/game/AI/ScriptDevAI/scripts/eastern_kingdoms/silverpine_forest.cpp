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
SDName: Silverpine_Forest
SD%Complete: 100
SDComment: Quest support: 435, 452
SDCategory: Silverpine Forest
EndScriptData */

// 银松森林（Silverpine Forest）阿鲁高与被遗忘者区域脚本

/* ContentData
npc_deathstalker_erland
npc_deathstalker_faerleia
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

/*#####
## npc_deathstalker_erland
#####*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_START_1         = -1000306,                         // 开始1 台词广播 ID
    SAY_START_2         = -1000307,                         // 开始2 台词广播 ID
    SAY_AGGRO_1         = -1000308,                         // 开战1 台词广播 ID
    SAY_AGGRO_2         = -1000309,                         // 开战2 台词广播 ID
    SAY_AGGRO_3         = -1000310,                         // 开战3 台词广播 ID
    SAY_PROGRESS        = -1000311,                         // progress 台词广播 ID
    SAY_END             = -1000312,                         // end 台词广播 ID
    SAY_RANE            = -1000313,                         // rane 台词广播 ID
    SAY_RANE_REPLY      = -1000314,                         // ranereply 台词广播 ID
    SAY_CHECK_NEXT      = -1000315,                         // checknext 台词广播 ID
    SAY_QUINN           = -1000316,                         // quinn 台词广播 ID
    SAY_QUINN_REPLY     = -1000317,                         // quinnreply 台词广播 ID
    SAY_BYE             = -1000318,                         // bye 台词广播 ID

    QUEST_ERLAND        = 435,                              // erland 任务 ID
    NPC_RANE            = 1950,                             // rane Entry ID
    NPC_QUINN           = 1951                              // quinn Entry ID
};

// 随从/NPC AI：erland（deathstalker_erland）
struct npc_deathstalker_erlandAI : public npc_escortAI
{
    npc_deathstalker_erlandAI(Creature* pCreature) : npc_escortAI(pCreature) { Reset(); }

    void WaypointReached(uint32 i) override
    {
        Player* pPlayer = GetPlayerForEscort();

        if (!pPlayer)
            return;

        switch (i)
        {
            case 1:
                DoScriptText(SAY_START_2, m_creature, pPlayer);
                break;
            case 14:
                DoScriptText(SAY_END, m_creature, pPlayer);
                pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_ERLAND, m_creature);
                break;
            case 15:
                if (Creature* pRane = GetClosestCreatureWithEntry(m_creature, NPC_RANE, 45.0f))
                    DoScriptText(SAY_RANE, pRane, m_creature);
                break;
            case 16:
                DoScriptText(SAY_RANE_REPLY, m_creature);
                break;
            case 17:
                DoScriptText(SAY_CHECK_NEXT, m_creature);
                break;
            case 25:
                DoScriptText(SAY_QUINN, m_creature);
                break;
            case 26:
                if (Creature* pQuinn = GetClosestCreatureWithEntry(m_creature, NPC_QUINN, 45.0f))
                    DoScriptText(SAY_QUINN_REPLY, pQuinn, m_creature);
                break;
            case 27:
                DoScriptText(SAY_BYE, m_creature);
                break;
        }
    }

    // 重置战斗状态与技能计时器
    void Reset() override {}

    // 进入战斗回调：记录开战状态并启动技能循环
    void Aggro(Unit* who) override
    {
        switch (urand(0, 2))
        {
            case 0: DoScriptText(SAY_AGGRO_1, m_creature, who); break;
            case 1: DoScriptText(SAY_AGGRO_2, m_creature, who); break;
            case 2: DoScriptText(SAY_AGGRO_3, m_creature, who); break;
        }
    }
};

// 任务接取回调函数：npc_deathstalker_erland
bool QuestAccept_npc_deathstalker_erland(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_ERLAND)
    {
        DoScriptText(SAY_START_1, pCreature);

        if (npc_deathstalker_erlandAI* pEscortAI = dynamic_cast<npc_deathstalker_erlandAI*>(pCreature->AI()))
            pEscortAI->Start(false, pPlayer, pQuest);
    }
    return true;
}

// 创建并返回 AI 实例工厂函数：npc_deathstalker_erland
UnitAI* GetAI_npc_deathstalker_erland(Creature* pCreature)
{
    return new npc_deathstalker_erlandAI(pCreature);
}

/*#####
## npc_deathstalker_faerleia
#####*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    QUEST_PYREWOOD_AMBUSH    = 452,                         // pyrewoodambush 任务 ID

    // cast it after every wave
    SPELL_DRINK_POTION       = 3359,                        // drinkpotion 法术 ID

    SAY_START                = -1000553,                    // 开始 台词广播 ID
    SAY_COMPLETED            = -1000554,                    // completed 台词广播 ID

    // 1st wave
    NPC_COUNCILMAN_SMITHERS  = 2060,                        // councilmansmithers Entry ID
    // 2nd wave
    NPC_COUNCILMAN_THATHER   = 2061,                        // councilmanthather Entry ID
    NPC_COUNCILMAN_HENDRICKS = 2062,                        // councilmanhendricks Entry ID
    // 3rd wave
    NPC_COUNCILMAN_WILHELM   = 2063,                        // councilmanwilhelm Entry ID
    NPC_COUNCILMAN_HARTIN    = 2064,                        // councilmanhartin Entry ID
    NPC_COUNCILMAN_HIGARTH   = 2066,                        // councilmanhigarth Entry ID
    // final wave
    NPC_COUNCILMAN_COOPER    = 2065,                        // councilmancooper Entry ID
    NPC_COUNCILMAN_BRUNSWICK = 2067,                        // councilmanbrunswick Entry ID
    NPC_LORD_MAYOR_MORRISON  = 2068                         // lordmayormorrison Entry ID
};

struct SpawnPoint
{
    float fX;
    float fY;
    float fZ;
    float fO;
};

SpawnPoint SpawnPoints[] =
{
    { -397.45f, 1509.56f, 18.87f, 4.73f},
    { -398.35f, 1510.75f, 18.87f, 4.76f},
    { -396.41f, 1511.06f, 18.87f, 4.74f}
};

static float m_afMoveCoords[] = { -410.69f, 1498.04f, 19.77f};

// 随从/NPC AI：faerleia（deathstalker_faerleia）
struct npc_deathstalker_faerleiaAI : public ScriptedAI
{
    npc_deathstalker_faerleiaAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    // 重置战斗状态与技能计时器
    void Reset() override {}

    ObjectGuid m_playerGuid;                                // player 实体 GUID
    uint32 m_uiWaveTimer;                                   // Wave 技能计时器
    uint32 m_uiSummonCount;
    uint8  m_uiWaveCount;
    bool   m_bEventStarted;                                 // EventStarted 状态标记

    void StartEvent(Player* pPlayer)
    {
        m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);

        m_playerGuid  = pPlayer->GetObjectGuid();
        m_bEventStarted = true;
        m_uiWaveTimer   = 10000;
        m_uiSummonCount = 0;
        m_uiWaveCount   = 0;
    }

    void FinishEvent()
    {
        m_playerGuid.Clear();
        m_bEventStarted = false;
        m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (Player* pPlayer = m_creature->GetMap()->GetPlayer(m_playerGuid))
            pPlayer->SendQuestFailed(QUEST_PYREWOOD_AMBUSH);

        FinishEvent();
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* pSummoned) override
    {
        ++m_uiSummonCount;

        // put them on correct waypoints later on
        float fX, fY, fZ;
        pSummoned->GetRandomPoint(m_afMoveCoords[0], m_afMoveCoords[1], m_afMoveCoords[2], 10.0f, fX, fY, fZ);
        pSummoned->GetMotionMaster()->MovePoint(0, fX, fY, fZ);
    }

    void SummonedCreatureJustDied(Creature* /*pKilled*/) override
    {
        --m_uiSummonCount;

        if (!m_uiSummonCount)
        {
            DoCastSpellIfCan(m_creature, SPELL_DRINK_POTION);

            // final wave
            if (m_uiWaveCount == 4)
            {
                DoScriptText(SAY_COMPLETED, m_creature);

                if (Player* pPlayer = m_creature->GetMap()->GetPlayer(m_playerGuid))
                    pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_PYREWOOD_AMBUSH, m_creature);

                FinishEvent();
            }
        }
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_bEventStarted && !m_uiSummonCount)
        {
            if (m_uiWaveTimer < uiDiff)
            {
                switch (m_uiWaveCount)
                {
                    case 0:
                        m_creature->SummonCreature(NPC_COUNCILMAN_SMITHERS,  SpawnPoints[1].fX, SpawnPoints[1].fY, SpawnPoints[1].fZ, SpawnPoints[1].fO, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiWaveTimer = 10000;
                        break;
                    case 1:
                        m_creature->SummonCreature(NPC_COUNCILMAN_THATHER,   SpawnPoints[2].fX, SpawnPoints[2].fY, SpawnPoints[2].fZ, SpawnPoints[2].fO, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_COUNCILMAN_HENDRICKS, SpawnPoints[1].fX, SpawnPoints[1].fY, SpawnPoints[1].fZ, SpawnPoints[1].fO, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiWaveTimer = 10000;
                        break;
                    case 2:
                        m_creature->SummonCreature(NPC_COUNCILMAN_WILHELM,   SpawnPoints[1].fX, SpawnPoints[1].fY, SpawnPoints[1].fZ, SpawnPoints[1].fO, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_COUNCILMAN_HARTIN,    SpawnPoints[0].fX, SpawnPoints[0].fY, SpawnPoints[0].fZ, SpawnPoints[0].fO, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_COUNCILMAN_HIGARTH,   SpawnPoints[2].fX, SpawnPoints[2].fY, SpawnPoints[2].fZ, SpawnPoints[2].fO, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiWaveTimer  = 8000;
                        break;
                    case 3:
                        m_creature->SummonCreature(NPC_COUNCILMAN_COOPER,    SpawnPoints[1].fX, SpawnPoints[1].fY, SpawnPoints[1].fZ, SpawnPoints[1].fO, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_COUNCILMAN_BRUNSWICK, SpawnPoints[2].fX, SpawnPoints[2].fY, SpawnPoints[2].fZ, SpawnPoints[2].fO, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_LORD_MAYOR_MORRISON,  SpawnPoints[0].fX, SpawnPoints[0].fY, SpawnPoints[0].fZ, SpawnPoints[0].fO, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 20000);
                        break;
                }

                ++m_uiWaveCount;
            }
            else
                m_uiWaveTimer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 任务接取回调函数：npc_deathstalker_faerleia
bool QuestAccept_npc_deathstalker_faerleia(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_PYREWOOD_AMBUSH)
    {
        DoScriptText(SAY_START, pCreature, pPlayer);

        if (npc_deathstalker_faerleiaAI* pFaerleiaAI = dynamic_cast<npc_deathstalker_faerleiaAI*>(pCreature->AI()))
        {
            pFaerleiaAI->StartEvent(pPlayer);
            pCreature->SetFactionTemporary(FACTION_ESCORT_H_NEUTRAL_ACTIVE, TEMPFACTION_RESTORE_RESPAWN); // faction guessed
        }
    }
    return true;
}

// 创建并返回 AI 实例工厂函数：npc_deathstalker_faerleia
UnitAI* GetAI_npc_deathstalker_faerleia(Creature* pCreature)
{
    return new npc_deathstalker_faerleiaAI(pCreature);
}

// 注册并载入脚本模块：silverpine_forest
void AddSC_silverpine_forest()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_deathstalker_erland";
    pNewScript->GetAI = &GetAI_npc_deathstalker_erland;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_deathstalker_erland;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_deathstalker_faerleia";
    pNewScript->GetAI = &GetAI_npc_deathstalker_faerleia;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_deathstalker_faerleia;
    pNewScript->RegisterSelf();
}
