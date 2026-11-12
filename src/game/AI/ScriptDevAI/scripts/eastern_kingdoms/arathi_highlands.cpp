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
SDName: Arathi Highlands
SD%Complete: 100
SDComment: Quest support: 660, 665
SDCategory: Arathi Highlands
EndScriptData */

// 阿拉希高地（Arathi Highlands）区域杂项与 NPC 脚本

/* ContentData
npc_professor_phizzlethorpe
npc_kinelory
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

/*######
## npc_professor_phizzlethorpe
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_PROGRESS_1          = -1000264,                     // progress1 台词广播 ID
    SAY_PROGRESS_2          = -1000265,                     // progress2 台词广播 ID
    SAY_PROGRESS_3          = -1000266,                     // progress3 台词广播 ID
    EMOTE_PROGRESS_4        = -1000267,                     // progress4 表情动作 ID
    SAY_AGGRO               = -1000268,                     // 开战 台词广播 ID
    SAY_PROGRESS_5          = -1000269,                     // progress5 台词广播 ID
    SAY_PROGRESS_6          = -1000270,                     // progress6 台词广播 ID
    SAY_PROGRESS_7          = -1000271,                     // progress7 台词广播 ID
    EMOTE_PROGRESS_8        = -1000272,                     // progress8 表情动作 ID
    SAY_PROGRESS_9          = -1000273,                     // progress9 台词广播 ID

    FACTION_BOOTY_BAY       = 120,                          // bootybay 阵营 ID
    QUEST_SUNKEN_TREASURE   = 665,                          // sunkentreasure 任务 ID
    ENTRY_VENGEFUL_SURGE    = 2776                          // vengefulsurge Entry ID
};

// 随从/NPC AI：phizzlethorpe（professor_phizzlethorpe）
struct npc_professor_phizzlethorpeAI : public npc_escortAI
{
    npc_professor_phizzlethorpeAI(Creature* pCreature) : npc_escortAI(pCreature) { Reset(); }

    // 重置战斗状态与技能计时器
    void Reset() override { }

    void WaypointReached(uint32 uiPointId) override
    {
        Player* pPlayer = GetPlayerForEscort();

        if (!pPlayer)
            return;

        switch (uiPointId)
        {
            case 5: DoScriptText(SAY_PROGRESS_2, m_creature, pPlayer); break;
            case 6: DoScriptText(SAY_PROGRESS_3, m_creature, pPlayer); break;
            case 9: DoScriptText(EMOTE_PROGRESS_4, m_creature); break;
            case 10:
                m_creature->SummonCreature(ENTRY_VENGEFUL_SURGE, -2056.41f, -2144.01f, 20.59f, 5.70f, TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN, 600000);
                m_creature->SummonCreature(ENTRY_VENGEFUL_SURGE, -2050.17f, -2140.02f, 19.54f, 5.17f, TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN, 600000);
                break;
            case 11: DoScriptText(SAY_PROGRESS_5, m_creature, pPlayer); break;
            case 12:
                DoScriptText(SAY_PROGRESS_6, m_creature, pPlayer);
                SetRun();
                break;
            case 20: DoScriptText(SAY_PROGRESS_7, m_creature, pPlayer); break;
            case 21:
                DoScriptText(EMOTE_PROGRESS_8, m_creature);
                DoScriptText(SAY_PROGRESS_9, m_creature, pPlayer);
                pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_SUNKEN_TREASURE, m_creature);
                break;
        }
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* pSummoned) override
    {
        pSummoned->AI()->AttackStart(m_creature);
    }
};

// 任务接取回调函数：npc_professor_phizzlethorpe
bool QuestAccept_npc_professor_phizzlethorpe(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_SUNKEN_TREASURE)
    {
        pCreature->SetFactionTemporary(FACTION_BOOTY_BAY, TEMPFACTION_RESTORE_RESPAWN);
        DoScriptText(SAY_PROGRESS_1, pCreature, pPlayer);

        if (npc_professor_phizzlethorpeAI* pEscortAI = dynamic_cast<npc_professor_phizzlethorpeAI*>(pCreature->AI()))
            pEscortAI->Start(false, pPlayer, pQuest, true);
    }
    return true;
}

// 创建并返回 AI 实例工厂函数：npc_professor_phizzlethorpe
UnitAI* GetAI_npc_professor_phizzlethorpe(Creature* pCreature)
{
    return new npc_professor_phizzlethorpeAI(pCreature);
}

/*######
## npc_kinelory
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_START               = -1000948,                     // 开始 台词广播 ID
    SAY_REACH_BOTTOM        = -1000949,                     // reachbottom 台词广播 ID
    SAY_AGGRO_KINELORY      = -1000950,                     // 开战kinelory 台词广播 ID
    SAY_AGGRO_JORELL        = -1000951,                     // 开战jorell 台词广播 ID
    SAY_WATCH_BACK          = -1000952,                     // watchback 台词广播 ID
    EMOTE_BELONGINGS        = -1000953,                     // belongings 表情动作 ID
    SAY_DATA_FOUND          = -1000954,                     // 数据found 台词广播 ID
    SAY_ESCAPE              = -1000955,                     // escape 台词广播 ID
    SAY_FINISH              = -1000956,                     // finish 台词广播 ID
    EMOTE_HAND_PACK         = -1000957,                     // handpack 表情动作 ID

    SPELL_REJUVENATION      = 3627,                         // rejuvenation 法术 ID
    SPELL_BEAR_FORM         = 4948,                         // bearform 法术 ID

    NPC_JORELL              = 2733,                         // jorell Entry ID
    NPC_QUAE                = 2712,                         // quae Entry ID

    QUEST_HINTS_NEW_PLAGUE  = 660                           // hintsnewplague 任务 ID
};

// 随从/NPC AI：标识 ID（kinelory）
struct npc_kineloryAI : public npc_escortAI
{
    npc_kineloryAI(Creature* pCreature) : npc_escortAI(pCreature) { Reset(); }

    uint32 m_uiBearFormTimer;                               // BearForm 技能计时器
    uint32 m_uiHealTimer;                                   // Heal 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiBearFormTimer = urand(5000, 7000);
        m_uiHealTimer     = urand(2000, 5000);
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 10:
                DoScriptText(SAY_REACH_BOTTOM, m_creature);
                break;
            case 17:
                DoScriptText(SAY_WATCH_BACK, m_creature);
                DoScriptText(EMOTE_BELONGINGS, m_creature);
                break;
            case 18:
                DoScriptText(SAY_DATA_FOUND, m_creature);
                break;
            case 19:
                DoScriptText(SAY_ESCAPE, m_creature);
                if (Player* pPlayer = GetPlayerForEscort())
                    m_creature->SetFacingToObject(pPlayer);
                SetRun();
                break;
            case 34:
                DoScriptText(SAY_FINISH, m_creature);
                m_creature->SetImmuneToNPC(true);
                if (Creature* pQuae = GetClosestCreatureWithEntry(m_creature, NPC_QUAE, 10.0f))
                {
                    DoScriptText(EMOTE_HAND_PACK, m_creature, pQuae);
                    m_creature->SetFacingToObject(pQuae);
                }
                break;
            case 35:
                if (Player* pPlayer = GetPlayerForEscort())
                    pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_HINTS_NEW_PLAGUE, m_creature);
                break;
        }
    }

    // 进入战斗回调：记录开战状态并启动技能循环
    void Aggro(Unit* pWho) override
    {
        if (pWho->GetEntry() == NPC_JORELL)
            DoScriptText(SAY_AGGRO_JORELL, pWho, m_creature);
        else if (roll_chance_i(10))
            DoScriptText(SAY_AGGRO_KINELORY, m_creature);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*pSender*/, Unit* pInvoker, uint32 uiMiscValue) override
    {
        if (eventType == AI_EVENT_START_ESCORT && pInvoker->GetTypeId() == TYPEID_PLAYER)
        {   
            m_creature->SetImmuneToNPC(false);
            DoScriptText(SAY_START, m_creature);
            Start(false, (Player*)pInvoker, GetQuestTemplateStore(uiMiscValue), true);
            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC);
        }
    }

    void UpdateEscortAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        if (m_uiBearFormTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_BEAR_FORM) == CAST_OK)
                m_uiBearFormTimer = urand(25000, 30000);
        }
        else
            m_uiBearFormTimer -= uiDiff;

        if (m_uiHealTimer < uiDiff)
        {
            if (Unit* pTarget = DoSelectLowestHpFriendly(40.0f))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_REJUVENATION) == CAST_OK)
                    m_uiHealTimer = urand(15000, 25000);
            }
        }
        else
            m_uiHealTimer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：npc_kinelory
UnitAI* GetAI_npc_kinelory(Creature* pCreature)
{
    return new npc_kineloryAI(pCreature);
}

// 任务接取回调函数：npc_kinelory
bool QuestAccept_npc_kinelory(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_HINTS_NEW_PLAGUE)
        pCreature->AI()->SendAIEvent(AI_EVENT_START_ESCORT, pPlayer, pCreature, pQuest->GetQuestId());

    return true;
}

// 注册并载入脚本模块：arathi_highlands
void AddSC_arathi_highlands()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_professor_phizzlethorpe";
    pNewScript->GetAI = &GetAI_npc_professor_phizzlethorpe;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_professor_phizzlethorpe;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_kinelory";
    pNewScript->GetAI = &GetAI_npc_kinelory;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_kinelory;
    pNewScript->RegisterSelf();
}
