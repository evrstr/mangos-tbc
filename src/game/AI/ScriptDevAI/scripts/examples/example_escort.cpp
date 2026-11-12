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
SDName: Example_Escort
SD%Complete: 100
SDComment: Script used for testing escortAI (用于测试 escortAI 护送基类的示例脚本)
SDCategory: Script Examples
EndScriptData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

enum
{
    NPC_FELBOAR                 = 21878,                    // 邪能野猪 NPC ID

    SPELL_DEATH_COIL            = 33130,                    // 死亡缠绕法术 ID
    SPELL_ELIXIR_OF_FORTITUDE   = 3593,                     // 坚韧药剂法术 ID
    SPELL_BLUE_FIREWORK         = 11540,                    // 蓝色烟花法术 ID

    SAY_AGGRO1                  = -1999910,                 // 进战台词 1 (护送中)
    SAY_AGGRO2                  = -1999911,                 // 进战台词 2 (非护送)
    SAY_WP_1                    = -1999912,                 // 到达路径点 1 台词
    SAY_WP_2                    = -1999913,                 // 到达路径点 3 台词
    SAY_WP_3                    = -1999914,                 // 到达路径点 4 NPC 台词
    SAY_WP_4                    = -1999915,                 // 到达路径点 4 玩家台词
    SAY_DEATH_1                 = -1999916,                 // 死亡台词 1 (自杀时耳语)
    SAY_DEATH_2                 = -1999917,                 // 死亡台词 2 (护送中被击杀)
    SAY_DEATH_3                 = -1999918,                 // 死亡台词 3 (非护送中死亡)
    SAY_SPELL                   = -1999919,                 // 施法台词
    SAY_RAND_1                  = -1999920,                 // 脱战随机台词 1 (放烟花)
    SAY_RAND_2                  = -1999921                  // 脱战随机台词 2 (喝药剂)
};

#define GOSSIP_ITEM_1   "Click to Test Escort(Attack, Run)"
#define GOSSIP_ITEM_2   "Click to Test Escort(NoAttack, Walk)"
#define GOSSIP_ITEM_3   "Click to Test Escort(NoAttack, Run)"

// 护送 AI 示例结构体 (example_escortAI)
struct example_escortAI : public npc_escortAI
{
    // UnitAI functions
    example_escortAI(Creature* pCreature) : npc_escortAI(pCreature) { Reset(); }

    uint32 m_uiDeathCoilTimer;                              // 死亡缠绕施法倒计时
    uint32 m_uiChatTimer;                                   // 脱战闲聊倒计时

    // Is called after each combat, so usally only reset combat-stuff here
    // 每次战斗结束后调用，重置战斗计时器
    void Reset() override
    {
        m_uiDeathCoilTimer = 4000;
        m_uiChatTimer = 4000;
    }

    // 召唤出生物后的回调：令召唤物攻击自身
    void JustSummoned(Creature* pSummoned) override
    {
        pSummoned->AI()->AttackStart(m_creature);
    }

    // Pure Virtual Functions (Have to be implemented)
    // 纯虚函数实现：到达指定路径点时触发事件
    void WaypointReached(uint32 uiWP) override
    {
        switch (uiWP)
        {
            case 1:
                DoScriptText(SAY_WP_1, m_creature);
                break;
            case 3:
                DoScriptText(SAY_WP_2, m_creature);
                m_creature->SummonCreature(NPC_FELBOAR, m_creature->GetPositionX() + 5.0f, m_creature->GetPositionY() + 7.0f, m_creature->GetPositionZ(), 0.0f, TEMPSPAWN_TIMED_OOC_DESPAWN, 3000);
                break;
            case 4:
                if (Player* pTmpPlayer = GetPlayerForEscort())
                {
                    // pTmpPlayer is the target of the text
                    DoScriptText(SAY_WP_3, m_creature, pTmpPlayer);
                    // pTmpPlayer is the source of the text
                    DoScriptText(SAY_WP_4, pTmpPlayer);
                }
                break;
        }
    }

    // 进入战斗回调：区分是否处于护送状态播放对应台词
    void Aggro(Unit* /*pWho*/) override
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING))
        {
            if (Player* pTemp = GetPlayerForEscort())
                DoScriptText(SAY_AGGRO1, m_creature, pTemp);
        }
        else
            DoScriptText(SAY_AGGRO2, m_creature);
    }

    // Only overwrite if there is something special
    // 死亡回调：播放遗言并触发护送失败处理
    void JustDied(Unit* pKiller) override
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING))
        {
            if (Player* pTemp = GetPlayerForEscort())
            {
                // not a likely case, code here for the sake of example
                if (pKiller == m_creature)
                {
                    // This is actually a whisper. You control the text type in database
                    DoScriptText(SAY_DEATH_1, m_creature, pTemp);
                }
                else
                    DoScriptText(SAY_DEATH_2, m_creature, pTemp);
            }
        }
        else
            DoScriptText(SAY_DEATH_3, m_creature);

        // Fail quest for group - if you don't implement JustDied in your script, this will automatically work
        // 护送任务对小队标记失败
        npc_escortAI::JustDied(pKiller);
    }

    // 护送 AI 每帧更新
    void UpdateEscortAI(const uint32 uiDiff) override
    {
        // Combat check (战斗状态检查)
        if (m_creature->SelectHostileTarget() && m_creature->GetVictim())
        {
            if (m_uiDeathCoilTimer < uiDiff)
            {
                DoScriptText(SAY_SPELL, m_creature);
                m_creature->CastSpell(m_creature->GetVictim(), SPELL_DEATH_COIL, TRIGGERED_NONE);
                m_uiDeathCoilTimer = 4000;
            }
            else
                m_uiDeathCoilTimer -= uiDiff;

            DoMeleeAttackIfReady();
        }
        else
        {
            // Out of combat but being escorted (脱战但正在被护送中)
            if (HasEscortState(STATE_ESCORT_ESCORTING))
            {
                if (m_uiChatTimer < uiDiff)
                {
                    if (m_creature->HasAura(SPELL_ELIXIR_OF_FORTITUDE, EFFECT_INDEX_0))
                    {
                        DoScriptText(SAY_RAND_1, m_creature);
                        m_creature->CastSpell(m_creature, SPELL_BLUE_FIREWORK, TRIGGERED_NONE);
                    }
                    else
                    {
                        DoScriptText(SAY_RAND_2, m_creature);
                        m_creature->CastSpell(m_creature, SPELL_ELIXIR_OF_FORTITUDE, TRIGGERED_NONE);
                    }

                    m_uiChatTimer = 12000;
                }
                else
                    m_uiChatTimer -= uiDiff;
            }
        }
    }
};

// 获取护送 AI 实例函数
UnitAI* GetAI_example_escort(Creature* pCreature)
{
    return new example_escortAI(pCreature);
}

// 对话菜单问候：展示护送测试选项
bool GossipHello_example_escort(Player* pPlayer, Creature* pCreature)
{
    pPlayer->TalkedToCreature(pCreature->GetEntry(), pCreature->GetObjectGuid());
    pPlayer->PrepareGossipMenu(pCreature, pPlayer->GetDefaultGossipMenuForSource(pCreature));

    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_3, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);

    pPlayer->SendPreparedGossip(pCreature);

    return true;
}

// 选择对话选项：启动不同模式的护送
bool GossipSelect_example_escort(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    example_escortAI* pEscortAI = dynamic_cast<example_escortAI*>(pCreature->AI());

    switch (uiAction)
    {
        case GOSSIP_ACTION_INFO_DEF+1:
            pPlayer->CLOSE_GOSSIP_MENU();

            if (pEscortAI)
                pEscortAI->Start(true, pPlayer);
            break;
        case GOSSIP_ACTION_INFO_DEF+2:
            pPlayer->CLOSE_GOSSIP_MENU();

            if (pEscortAI)
                pEscortAI->Start(false, pPlayer);
            break;
        case GOSSIP_ACTION_INFO_DEF+3:
            pPlayer->CLOSE_GOSSIP_MENU();

            if (pEscortAI)
                pEscortAI->Start(true, pPlayer);
            break;
        default:
            return false;                                   // nothing defined      -> mangos core handling
    }

    return true;                                            // no default handling  -> prevent mangos core handling
}

// 注册护送示例脚本
void AddSC_example_escort()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "example_escort";
    pNewScript->GetAI = &GetAI_example_escort;
    pNewScript->pGossipHello = &GossipHello_example_escort;
    pNewScript->pGossipSelect = &GossipSelect_example_escort;
    pNewScript->RegisterSelf(false);
}
