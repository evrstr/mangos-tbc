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
SDName: Tirisfal_Glades
SD%Complete: 100
SDComment: Quest support: 590, 1819
SDCategory: Tirisfal Glades
EndScriptData */

// 提瑞斯法林地（Tirisfal Glades）被遗忘者新手区域任务脚本

/* ContentData
go_mausoleum_door
go_mausoleum_trigger
npc_calvin_montague
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"

/*######
## go_mausoleum_door
## go_mausoleum_trigger
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    QUEST_ULAG      = 1819,                                 // ulag 任务 ID
    NPC_ULAG        = 6390,                                 // ulag Entry ID
    GO_TRIGGER      = 104593,                               // 触发 游戏对象 Entry ID
    GO_DOOR         = 176594                                // 门 游戏对象 Entry ID
};

bool GOUse_go_mausoleum_door(Player* pPlayer, GameObject* /*pGo*/)
{
    if (pPlayer->GetQuestStatus(QUEST_ULAG) != QUEST_STATUS_INCOMPLETE)
        return false;

    if (GameObject* pTrigger = GetClosestGameObjectWithEntry(pPlayer, GO_TRIGGER, 30.0f))
    {
        pTrigger->SetGoState(GO_STATE_READY);
        pPlayer->SummonCreature(NPC_ULAG, 2390.26f, 336.47f, 40.01f, 2.26f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 300000);
        return false;
    }

    return false;
}

// 游戏对象使用交互回调：go_mausoleum_trigger
bool GOUse_go_mausoleum_trigger(Player* pPlayer, GameObject* pGo)
{
    if (pPlayer->GetQuestStatus(QUEST_ULAG) != QUEST_STATUS_INCOMPLETE)
        return false;

    if (GameObject* pDoor = GetClosestGameObjectWithEntry(pPlayer, GO_DOOR, 30.0f))
    {
        pGo->SetGoState(GO_STATE_ACTIVE);
        pDoor->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_INTERACT_COND);
        return true;
    }

    return false;
}

/*######
## npc_calvin_montague
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_COMPLETE        = -1000356,                         // complete 台词广播 ID
    SD_SPELL_DRINK      = 2639,                             // possibly not correct spell (but iconId is correct)
    QUEST_590           = 590,                              // 590 任务 ID
    FACTION_HOSTILE     = 168                               // hostile 阵营 ID
};

// 随从/NPC AI：montague（calvin_montague）
struct npc_calvin_montagueAI : public ScriptedAI
{
    npc_calvin_montagueAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    uint32 m_uiPhase;                                       // 战斗阶段标识
    uint32 m_uiPhaseTimer;                                  // Phase 技能计时器
    ObjectGuid m_playerGuid;                                // player 实体 GUID

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiPhase = 0;
        m_uiPhaseTimer = 5000;
        SetReactState(REACT_AGGRESSIVE);
        m_creature->SetImmuneToPlayer(true);
        m_playerGuid.Clear();
    }

    void DamageTaken(Unit* dealer, uint32& damage, DamageEffectType /*damagetype*/, SpellEntry const* /*spellInfo*/) override
    {
        if (damage > m_creature->GetHealth() || ((m_creature->GetHealth() - damage) * 100 / m_creature->GetMaxHealth() < 15))
        {
            damage = std::min(damage, m_creature->GetHealth() - 1);

            m_creature->CombatStop(true);
            SetReactState(REACT_PASSIVE);

            m_uiPhase = 1;

            if (dealer && dealer->GetTypeId() == TYPEID_PLAYER)
                m_playerGuid = dealer->GetObjectGuid();
        }
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiPhase)
        {
            if (m_uiPhaseTimer < uiDiff)
                m_uiPhaseTimer = 7500;
            else
            {
                m_uiPhaseTimer -= uiDiff;
                return;
            }

            switch (m_uiPhase)
            {
                case 1:
                    DoScriptText(SAY_COMPLETE, m_creature);
                    ++m_uiPhase;
                    break;
                case 2:
                    if (Player* pPlayer = m_creature->GetMap()->GetPlayer(m_playerGuid))
                        pPlayer->AreaExploredOrEventHappens(QUEST_590);

                    m_creature->CastSpell(m_creature, SD_SPELL_DRINK, TRIGGERED_OLD_TRIGGERED);
                    ++m_uiPhase;
                    break;
                case 3:
                    EnterEvadeMode();
                    break;
            }

            return;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：npc_calvin_montague
UnitAI* GetAI_npc_calvin_montague(Creature* pCreature)
{
    return new npc_calvin_montagueAI(pCreature);
}

// 任务接取回调函数：npc_calvin_montague
bool QuestAccept_npc_calvin_montague(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_590)
    {
        pCreature->SetImmuneToPlayer(false);
        pCreature->SetFactionTemporary(FACTION_HOSTILE, TEMPFACTION_RESTORE_COMBAT_STOP | TEMPFACTION_RESTORE_RESPAWN);
        pCreature->AI()->AttackStart(pPlayer);
    }
    return true;
}

// 注册并载入脚本模块：tirisfal_glades
void AddSC_tirisfal_glades()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "go_mausoleum_door";
    pNewScript->pGOUse = &GOUse_go_mausoleum_door;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_mausoleum_trigger";
    pNewScript->pGOUse = &GOUse_go_mausoleum_trigger;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_calvin_montague";
    pNewScript->GetAI = &GetAI_npc_calvin_montague;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_calvin_montague;
    pNewScript->RegisterSelf();
}
