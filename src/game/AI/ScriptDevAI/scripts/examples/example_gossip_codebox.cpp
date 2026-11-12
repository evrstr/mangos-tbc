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
SDName: Example_Gossip_Codebox
SD%Complete: 100
SDComment: Show a codebox in gossip option (在对话选项中弹出输入框示例)
SDCategory: Script Examples
EndScriptData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include <cstring>

enum
{
    SPELL_POLYMORPH         = 12826,                        // 变形术 (回答错误惩罚)
    SPELL_MARK_OF_THE_WILD  = 26990,                        // 野性印记 (回答正确奖励)

    SAY_NOT_INTERESTED      = -1999922,                     // 台词：不感兴趣
    SAY_WRONG               = -1999923,                     // 台词：回答错误
    SAY_CORRECT             = -1999924                      // 台词：回答正确
};

// Should actually be handled in SD2-database!
// 对话选项文本定义（常规情况应在数据库中配置）
#define GOSSIP_ITEM_1       "A quiz: what's your name?"
#define GOSSIP_ITEM_2       "I'm not interested"

// This function is called when the player opens the gossip menubool
// 玩家打开对话菜单时的回调函数
bool GossipHello_example_gossip_codebox(Player* pPlayer, Creature* pCreature)
{
    pPlayer->ADD_GOSSIP_ITEM_EXTENDED(0, GOSSIP_ITEM_1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1, "", 0, true);
    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);

    pPlayer->GetPlayerMenu()->SendGossipMenu(907, pCreature->GetObjectGuid());

    return true;
}

// This function is called when the player clicks an option on the gossip menubool
// 玩家点击普通对话选项时的回调函数
bool GossipSelect_example_gossip_codebox(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 2)
    {
        DoScriptText(SAY_NOT_INTERESTED, pCreature);
        pPlayer->CLOSE_GOSSIP_MENU();
    }

    return true;
}

// 玩家在对话框输入文本并提交时的回调函数
bool GossipSelectWithCode_example_gossip_codebox(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction, const char* sCode)
{
    if (uiSender == GOSSIP_SENDER_MAIN)
    {
        switch (uiAction)
        {
            case GOSSIP_ACTION_INFO_DEF+1:
                if (std::strcmp(sCode, pPlayer->GetName()) != 0)
                {
                    DoScriptText(SAY_WRONG, pCreature);
                    pCreature->CastSpell(pPlayer, SPELL_POLYMORPH, TRIGGERED_OLD_TRIGGERED);
                }
                else
                {
                    DoScriptText(SAY_CORRECT, pCreature);
                    pCreature->CastSpell(pPlayer, SPELL_MARK_OF_THE_WILD, TRIGGERED_OLD_TRIGGERED);
                }
                pPlayer->CLOSE_GOSSIP_MENU();

                return true;
        }
    }

    return false;
}

// 注册对话框输入示例脚本
void AddSC_example_gossip_codebox()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "example_gossip_codebox";
    pNewScript->pGossipHello = &GossipHello_example_gossip_codebox;
    pNewScript->pGossipSelect = &GossipSelect_example_gossip_codebox;
    pNewScript->pGossipSelectWithCode = &GossipSelectWithCode_example_gossip_codebox;
    pNewScript->RegisterSelf(false);
}
