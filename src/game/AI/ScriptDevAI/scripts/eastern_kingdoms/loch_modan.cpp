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
SDName: Loch_Modan
SD%Complete: 100
SDComment: Quest support: 3181 (only to argue with pebblebitty to get to searing gorge, before quest rewarded), 309
SDCategory: Loch Modan
EndScriptData */

// 洛克莫丹（Loch Modan）矮人区域任务脚本

/* ContentData
npc_miran
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

/*######
## npc_miran
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    QUEST_PROTECTING_THE_SHIPMENT = 309,                    // protectingtheshipment 任务 ID

    SAY_MIRAN_1           = 510,                            // miran1 台词广播 ID
    SAY_MIRAN_2           = 511,                            // miran2 台词广播 ID
    SAY_MIRAN_3           = 498,                            // miran3 台词广播 ID

    SAY_DARK_IRON_RAIDER  = 512,                            // dark黑铁raider 台词广播 ID
    NPC_DARK_IRON_RAIDER  = 2149,                           // dark黑铁raider Entry ID

    MIRAN_ESCORT_PATH     = 1379                            // escort路径 标识 ID
};

static const Position m_afAmbushSpawn[] =
{
    { -5705.012f, -3736.6575f, 318.56738f, 0.57595f},
    { -5696.1943f, -3736.78f, 318.58145f, 2.40855f}
};

// 随从/NPC AI：标识 ID（miran）
struct npc_miranAI: public npc_escortAI
{
    npc_miranAI(Creature* creature): npc_escortAI(creature)
    {
        Reset();
    }

    uint8 m_uiDwarves;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        if (!HasEscortState(STATE_ESCORT_ESCORTING))
            m_uiDwarves = 0;
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 16:
                DoBroadcastText(SAY_MIRAN_1, m_creature);
                break;
            case 17:
                SetEscortPaused(true);
                m_creature->SummonCreature(NPC_DARK_IRON_RAIDER, m_afAmbushSpawn[0].x, m_afAmbushSpawn[0].y, m_afAmbushSpawn[0].z, m_afAmbushSpawn[0].o, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 25000);
                m_creature->SummonCreature(NPC_DARK_IRON_RAIDER, m_afAmbushSpawn[1].x, m_afAmbushSpawn[1].y, m_afAmbushSpawn[1].z, m_afAmbushSpawn[1].o, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 25000);
                break;
            case 21:
                DoBroadcastText(SAY_MIRAN_3, m_creature);
                if (Player* player = GetPlayerForEscort())
                    player->RewardPlayerAndGroupAtEventExplored(QUEST_PROTECTING_THE_SHIPMENT, m_creature);
                SetEscortPaused(true); 
                m_creature->ForcedDespawn(15000);
                break;
        }
    }

    // 召唤生物死亡回调
    void SummonedCreatureJustDied(Creature* summoned) override
    {
        if (summoned->GetEntry() == NPC_DARK_IRON_RAIDER)
        {
            --m_uiDwarves;
            if (!m_uiDwarves)
            {
                DoBroadcastText(SAY_MIRAN_2, m_creature);
                SetEscortPaused(false);
            }
        }
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        if (summoned->GetEntry() == NPC_DARK_IRON_RAIDER)
        {
            ++m_uiDwarves;
            summoned->AI()->AttackStart(m_creature);
            DoBroadcastText(SAY_DARK_IRON_RAIDER, summoned);
        }
    }
};

// 任务接取回调函数：npc_miran
bool QuestAccept_npc_miran(Player* player, Creature* creature, const Quest* quest)
{
    if (quest->GetQuestId() == QUEST_PROTECTING_THE_SHIPMENT)
    {
        if (npc_miranAI* pEscortAI = dynamic_cast<npc_miranAI*>(creature->AI()))
            pEscortAI->Start(false, player, quest, false, false, MIRAN_ESCORT_PATH);
    }
    return true;
}

// 创建并返回 AI 实例工厂函数：npc_miran
UnitAI* GetAI_npc_miran(Creature* creature)
{
    return new npc_miranAI(creature);
}

// 注册并载入脚本模块：loch_modan
void AddSC_loch_modan()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_miran";
    pNewScript->GetAI = &GetAI_npc_miran;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_miran;
    pNewScript->RegisterSelf();
}
