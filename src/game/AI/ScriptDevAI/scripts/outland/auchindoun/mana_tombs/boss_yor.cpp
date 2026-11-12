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
SDName: Boss_NexusPrince_Shaffar
SD%Complete: 100
SDComment: ToDo: move the Ethereal Beacon script to eventAI
SDCategory: Auchindoun, Mana Tombs
EndScriptData */

/* ContentData
boss_nexusprince_shaffar
mob_ethereal_beacon
EndContentData */

// 首领：尤尔（法力陵墓英雄难度隐藏首领）与被囚禁的虚灵
/* 遭遇战概述：
 * 尤尔是被关押在法力陵墓静滞容器中的虚空畸变怪，仅在英雄难度下由持有钥匙的玩家开启。
 * 开启容器后，尤尔破除放逐状态进入战斗，具有强力的践踏与双重吐息技能。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 尤尔与被囚禁者相关法术、阵营与 NPC 条目枚举
enum
{
    SPELL_C_C_D               = 35465,                  // 静滞定身与无敌视觉
    SPELL_PURPLE_BANISH_STATE = 32566,                  // 紫色放逐状态视觉

    FACTION_HOSTILE = 14,                               // 敌对阵营 ID

    NPC_YOR = 22930,                                    // 首领：尤尔（英雄难度隐藏首领）

    SPELL_STOMP = 36405,                                // 践踏
    SPELL_DOUBLE_BREATH = 38361,                        // 双重吐息

    NPC_STASIS_PRISONER_DUNGEON = 22927,                // 地下城静滞囚徒
};

// 尤尔战斗行为动作枚举
enum YorActions
{
    YOR_COMBAT_ACTION_MAX,
    YOR_ATTACK,                                         // 破除静滞后发起攻击
};

// 囚徒交互事件动作枚举
enum PrisonerActions
{
    PRISONER_ATTACK,                                    // 囚徒攻击
    PRISONER_TALK,                                      // 囚徒对话
    PRISONER_CAST,                                      // 囚徒施法
    PRISONER_CREDIT,                                    // 任务进度结算
};

// 尤尔 AI
struct boss_yorAI : public CombatAI
{
    boss_yorAI(Creature* creature) : CombatAI(creature, YOR_COMBAT_ACTION_MAX)
    {
        AddCustomAction(YOR_ATTACK, true, [&]
        {
            m_creature->SetImmuneToNPC(false);
            m_creature->SetImmuneToPlayer(false);
            m_creature->setFaction(FACTION_HOSTILE);
            if (Player* player = m_creature->GetMap()->GetPlayer(m_playerGuid))
                AttackStart(player);
        });
        JustRespawned();
    }

    void JustRespawned() override                       // 重生回调：施加静滞放逐光环并重置容器门
    {
        DoCastSpellIfCan(nullptr, SPELL_C_C_D, (CAST_AURA_NOT_PRESENT | CAST_TRIGGERED));
        DoCastSpellIfCan(nullptr, SPELL_PURPLE_BANISH_STATE, (CAST_AURA_NOT_PRESENT | CAST_TRIGGERED));
        if (m_stasisGuid)
            if (GameObject* stasis = m_creature->GetMap()->GetGameObject(m_stasisGuid))
                stasis->ResetDoorOrButton();
    }

    ObjectGuid m_playerGuid;                            // 触发事件的玩家 GUID
    ObjectGuid m_stasisGuid;                            // 静滞容器游戏对象 GUID

    void StartEvent(Player* player, GameObject* go)     // 开启破除静滞事件
    {
        m_playerGuid = player->GetObjectGuid();
        if (go)
            m_stasisGuid = go->GetObjectGuid();
        m_creature->RemoveAurasDueToSpell(SPELL_C_C_D);
        m_creature->RemoveAurasDueToSpell(SPELL_PURPLE_BANISH_STATE);
        m_creature->UpdateEntry(NPC_YOR);
        ResetTimer(YOR_ATTACK, 2000);
    }
};

// 交互开启沙法尔静滞室
bool GOUse_go_stasis_chamber_shaffar(Player* player, GameObject* go)
{
    if (Creature* prisoner = GetClosestCreatureWithEntry(go, NPC_STASIS_PRISONER_DUNGEON, 2.f))
    {
        boss_yorAI* ai = static_cast<boss_yorAI*>(prisoner->AI());
        ai->StartEvent(player, go);
    }

    return false;
}

// 蓝色放逐状态与帕克西维大使枚举
enum
{
    SPELL_BLUE_BANISH_STATE = 39650,                    // 蓝色放逐状态

    SAY_RELEASED_1 = 20633,                             // 获释台词 1
    SAY_RELEASED_2 = 20634,                             // 获释台词 2

    NPC_AMBASSADOR_PAXIVI = 22928,                      // 帕克西维大使

    SPELL_QID_10977 = 39660,                            // 任务 10977 完成奖励法术
};

// 虚灵囚徒 AI（获释事件与任务结算）
struct npc_ethereum_prisoner_dungeonAI : public ScriptedAI
{
    npc_ethereum_prisoner_dungeonAI(Creature* creature) : ScriptedAI(creature, 0)
    {
        AddCustomAction(PRISONER_CREDIT, true, [&]
        {
            if (Player* player = m_creature->GetMap()->GetPlayer(m_playerGuid))
                player->CastSpell(nullptr, SPELL_QID_10977, TRIGGERED_NONE);
        });
        AddCustomAction(PRISONER_TALK, true, [&]
        {
            if (Player* player = m_creature->GetMap()->GetPlayer(m_playerGuid))
                DoBroadcastText(SAY_RELEASED_2, m_creature, player);
            m_creature->ForcedDespawn(600000);
        });
        JustRespawned();
    }

    void JustRespawned() override                       // 重生回调：施加静滞放逐光环并重置容器门
    {
        DoCastSpellIfCan(nullptr, SPELL_C_C_D, (CAST_AURA_NOT_PRESENT | CAST_TRIGGERED));
        DoCastSpellIfCan(nullptr, SPELL_BLUE_BANISH_STATE, (CAST_AURA_NOT_PRESENT | CAST_TRIGGERED));
        if (m_stasisGuid)
            if (GameObject* stasis = m_creature->GetMap()->GetGameObject(m_stasisGuid))
                stasis->ResetDoorOrButton();
    }

    ObjectGuid m_playerGuid;                            // 触发交互的玩家 GUID
    ObjectGuid m_stasisGuid;                            // 静滞容器游戏对象 GUID

    void StartEvent(Player* player, GameObject* go)     // 开启囚徒获释事件
    {
        m_playerGuid = player->GetObjectGuid();
        if (go)
            m_stasisGuid = go->GetObjectGuid();
        m_creature->RemoveAurasDueToSpell(SPELL_C_C_D);
        m_creature->RemoveAurasDueToSpell(SPELL_BLUE_BANISH_STATE);
        m_creature->UpdateEntry(NPC_AMBASSADOR_PAXIVI);
        float angle = m_unit->GetAngle(player);
        m_creature->SetFacingTo(angle);
        m_creature->SetOrientation(angle);
        DoBroadcastText(SAY_RELEASED_1, m_creature, player);
        ResetTimer(PRISONER_CREDIT, 500);
        ResetTimer(PRISONER_TALK, 8500);
    }

    void Reset() override                               // 重置状态
    {

    }

    void ExecuteActions() override {}

    void UpdateAI(const uint32 diff) override           // 更新计时器
    {
        UpdateTimers(diff, m_creature->IsInCombat());
    }
};

// guid scripting, if there ever was any (根据坐标位置分配囚徒 AI 或尤尔 AI)
UnitAI* GetAInpc_ethereum_prisoner_dungeon(Creature* creature)
{
    if (creature->GetPositionX() > 0)
        return new npc_ethereum_prisoner_dungeonAI(creature);
    else
        return new boss_yorAI(creature);
}

// 交互开启法力陵墓静滞室
bool GOUse_go_stasis_chamber_mana_tombs(Player* player, GameObject* go)
{
    if (Creature* prisoner = GetClosestCreatureWithEntry(go, NPC_STASIS_PRISONER_DUNGEON, 2.f))
    {
        npc_ethereum_prisoner_dungeonAI* ai = static_cast<npc_ethereum_prisoner_dungeonAI*>(prisoner->AI());
        ai->StartEvent(player, go);
    }

    return false;
}

// 注册尤尔与静滞室相关脚本
void AddSC_boss_yor()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "go_stasis_chamber_mana_tombs";
    pNewScript->pGOUse = &GOUse_go_stasis_chamber_mana_tombs;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_ethereum_prisoner_dungeon";
    pNewScript->GetAI = &GetAInpc_ethereum_prisoner_dungeon;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_stasis_chamber_shaffar";
    pNewScript->pGOUse = &GOUse_go_stasis_chamber_shaffar;
    pNewScript->RegisterSelf();
}
