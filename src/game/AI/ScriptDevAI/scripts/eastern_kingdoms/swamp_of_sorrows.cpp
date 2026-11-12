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
SDName: Swamp_of_Sorrows
SD%Complete: 100
SDComment: Quest support: 1393
SDCategory: Swamp of Sorrows
EndScriptData */

// 悲伤沼泽（Swamp of Sorrows）区域杂项与失落者任务脚本

/* ContentData
npc_galen_goodward
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

/*######
## npc_galen_goodward
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum Galen
{
    QUEST_GALENS_ESCAPE     = 1393,                         // galensescape 任务 ID

    GO_GALENS_CAGE          = 37118,                        // galenscage 游戏对象 Entry ID

    SAY_PERIODIC            = -1000582,                     // periodic 台词广播 ID
    SAY_QUEST_ACCEPTED      = -1000583,                     // 任务accepted 台词广播 ID
    SAY_ATTACKED_1          = -1000584,                     // attacked1 台词广播 ID
    SAY_ATTACKED_2          = -1000585,                     // attacked2 台词广播 ID
    SAY_QUEST_COMPLETE      = -1000586,                     // 任务complete 台词广播 ID
    EMOTE_WHISPER           = -1000587,                     // whisper 表情动作 ID
    EMOTE_DISAPPEAR         = -1000588                      // disappear 表情动作 ID
};

// 随从/NPC AI：goodward（galen_goodward）
struct npc_galen_goodwardAI : public npc_escortAI
{
    npc_galen_goodwardAI(Creature* creature) : npc_escortAI(creature) { Reset(); }

    ObjectGuid m_galensCageGuid;                            // galensCage 实体 GUID
    uint32 m_periodicSay;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_periodicSay = 6000;
    }

    // 进入战斗回调：记录开战状态并启动技能循环
    void Aggro(Unit* who) override
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING))
            DoScriptText(urand(0, 1) ? SAY_ATTACKED_1 : SAY_ATTACKED_2, m_creature, who);
    }

    void WaypointStart(uint32 pointId) override
    {
        if (pointId == 1)
        {
            GameObject* cage;
            if (m_galensCageGuid)
                cage = m_creature->GetMap()->GetGameObject(m_galensCageGuid);
            else
                cage = GetClosestGameObjectWithEntry(m_creature, GO_GALENS_CAGE, INTERACTION_DISTANCE);

            if (cage)
            {
                cage->UseDoorOrButton();
                m_galensCageGuid = cage->GetObjectGuid();
            }
        }
    }

    void WaypointReached(uint32 pointId) override
    {
        switch (pointId)
        {
            case 1:
                if (GameObject* cage = m_creature->GetMap()->GetGameObject(m_galensCageGuid))
                    cage->ResetDoorOrButton();
                break;
            case 20:
                SetRun(true);
                break;
            case 21:

                if (Player* player = GetPlayerForEscort())
                {
                    DoScriptText(SAY_QUEST_COMPLETE, m_creature, player);
                    DoScriptText(EMOTE_WHISPER, m_creature, player);
                    player->RewardPlayerAndGroupAtEventExplored(QUEST_GALENS_ESCAPE, m_creature);
                }
                break;
            case 22:
                DoScriptText(EMOTE_DISAPPEAR, m_creature);
                break;
            case 23:
                SetEscortPaused(true);
                m_creature->ForcedDespawn(1000);
                break;
            default:
                break;
        }
    }

    void UpdateEscortAI(const uint32 diff) override
    {

        if (m_periodicSay < diff)
        {
            if (HasEscortState(STATE_ESCORT_NONE))
                DoScriptText(SAY_PERIODIC, m_creature);
            m_periodicSay = 6000;
        }
        else
            m_periodicSay -= diff;

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 任务接取回调函数：npc_galen_goodward
bool QuestAccept_npc_galen_goodward(Player* player, Creature* creature, const Quest* quest)
{
    if (quest->GetQuestId() == QUEST_GALENS_ESCAPE)
    {

        if (auto* escortAI = dynamic_cast<npc_galen_goodwardAI*>(creature->AI()))
        {
            escortAI->Start(false, player, quest);
            creature->SetFactionTemporary(FACTION_ESCORT_N_NEUTRAL_ACTIVE, TEMPFACTION_RESTORE_RESPAWN | TEMPFACTION_TOGGLE_IMMUNE_TO_NPC);
            DoScriptText(SAY_QUEST_ACCEPTED, creature);
        }
    }
    return true;
}

// 创建并返回 AI 实例工厂函数：npc_galen_goodward
UnitAI* GetAI_npc_galen_goodward(Creature* creature)
{
    return new npc_galen_goodwardAI(creature);
}

// 注册并载入脚本模块：swamp_of_sorrows
void AddSC_swamp_of_sorrows()
{
    Script* newScript = new Script;
    newScript->Name = "npc_galen_goodward";
    newScript->GetAI = &GetAI_npc_galen_goodward;
    newScript->pQuestAcceptNPC = &QuestAccept_npc_galen_goodward;
    newScript->RegisterSelf();
}
