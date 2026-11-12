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
SDName: Searing_Gorge
SD%Complete: 80
SDComment: Quest support: 3367.
SDCategory: Searing Gorge
EndScriptData */

// 灼热峡谷（Searing Gorge）黑铁矮人与索瑞森相关任务脚本

/* ContentData
npc_dorius_stonetender
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

/*######
## npc_dorius_stonetender
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_DORIUS_AGGRO_1              = 4351,                 // dorius开战1 台词广播 ID
    SAY_DORIUS_AGGRO_2              = 4352,                 // dorius开战2 台词广播 ID
    SAY_DORIUS_AGGRO_3              = 4353,                 // dorius开战3 台词广播 ID

    SAY_DORIUS_TO_PLAYER            = 4354,                 // doriustoplayer 台词广播 ID
    SAY_DORIUS_FINISH               = 4359,                 // doriusfinish 台词广播 ID

    NPC_DARK_IRON_STEELSHIFTER      = 8337,                 // dark黑铁steelshifter Entry ID

    SPELL_MARKSMAN_HIT              = 12198,                // marksmanhit 法术 ID
    
    RELAY_SCRIPT_FINISHED           = 8284,                 // scriptfinished 标识 ID

    PATH_ID                         = 8284,        // Sniffed Waypoints for Escort Event
    QUEST_ID_SUNTARA_STONES         = 3367,                 // IDsuntarastones 任务 ID
};

// 随从/NPC AI：stonetender（dorius_stonetender）
struct npc_dorius_stonetenderAI : public npc_escortAI
{
    npc_dorius_stonetenderAI(Creature* pCreature) : npc_escortAI(pCreature) 
    { 
        Reset(); 
    }

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        if (!HasEscortState(STATE_ESCORT_ESCORTING))
            m_creature->SetStandState(UNIT_STAND_STATE_DEAD, true);
    }

    // 进入战斗回调：记录开战状态并启动技能循环
    void Aggro(Unit* who) override
    {
        switch (urand(0, 2))
        {
            case 0: DoBroadcastText(SAY_DORIUS_AGGRO_1, m_creature, who); break;
            case 1: DoBroadcastText(SAY_DORIUS_AGGRO_2, m_creature, who); break;
            case 2: DoBroadcastText(SAY_DORIUS_AGGRO_3, m_creature, who); break;
        }
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*pSender*/, Unit* pInvoker, uint32 uiMiscValue) override
    {
        if (eventType == AI_EVENT_START_ESCORT && pInvoker->GetTypeId() == TYPEID_PLAYER)
        {
            m_creature->SetStandState(UNIT_STAND_STATE_STAND);
            m_creature->SetFactionTemporary(FACTION_ESCORT_A_NEUTRAL_ACTIVE, TEMPFACTION_RESTORE_RESPAWN | TEMPFACTION_TOGGLE_IMMUNE_TO_NPC);
            Start(false, (Player*)pInvoker, GetQuestTemplateStore(uiMiscValue), true, false, PATH_ID);
        }
    }

    // 受到法术命中回调
    void SpellHit(Unit* pCaster, const SpellEntry* pSpell) override
    {        
        if (pSpell->Id == SPELL_MARKSMAN_HIT)
        { 
            if (Player* pPlayer = GetPlayerForEscort())
                pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_ID_SUNTARA_STONES, m_creature);
            // all rp things handled via relayscript
            m_creature->GetMap()->ScriptsStart(SCRIPT_TYPE_RELAY, RELAY_SCRIPT_FINISHED, m_creature, pCaster);
            End();
        }            
    }

    void SummonedMovementInform(Creature* pSummoned, uint32 /*moveType*/, uint32 uiPointId) override
    {
        if (pSummoned->GetEntry() == NPC_DARK_IRON_STEELSHIFTER && uiPointId == 2)
            pSummoned->AI()->AttackStart(m_creature);
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {         
            case 37:
                // Have to do it via core because we need Player as target
                if (Player* pPlayer = GetPlayerForEscort())
                {
                    DoBroadcastText(SAY_DORIUS_TO_PLAYER, m_creature, pPlayer);
                    m_creature->HandleEmote(EMOTE_ONESHOT_POINT);
                }
                break;
            case 53:
                if (Player* pPlayer = GetPlayerForEscort())                
                    m_creature->SetFacingToObject(pPlayer);
                break;
        }
    }

    void UpdateEscortAI(const uint32 /*uiDiff*/) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：npc_dorius_stonetender
UnitAI* GetAI_npc_dorius_stonetender(Creature* pCreature)
{
    return new npc_dorius_stonetenderAI(pCreature);
}

// 任务接取回调函数：npc_dorius_stonetender
bool QuestAccept_npc_dorius_stonetender(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_ID_SUNTARA_STONES)
    {
        pCreature->AI()->SendAIEvent(AI_EVENT_START_ESCORT, pPlayer, pCreature, pQuest->GetQuestId());
        return true;
    }

    return false;
}

// 注册并载入脚本模块：searing_gorge
void AddSC_searing_gorge()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_dorius_stonetender";
    pNewScript->GetAI = &GetAI_npc_dorius_stonetender;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_dorius_stonetender;
    pNewScript->RegisterSelf();
}
