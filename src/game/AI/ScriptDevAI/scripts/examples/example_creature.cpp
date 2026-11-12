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
SDName: Example_Creature
SD%Complete: 100
SDComment: Short custom scripting example (生物自定义 AI 脚本基础示例)
SDCategory: Script Examples
EndScriptData */

#include "AI/ScriptDevAI/include/sc_common.h"

// **** This script is designed as an example for others to build on ****
// **** Please modify whatever you'd like to as this script is only for developement ****
// 【示例设计说明】本脚本专为开发者提供扩展与参考范例

// **** Script Info ****
// This script is written in a way that it can be used for both friendly and hostile monsters
// Its primary purpose is to show just how much you can really do with scripts
// I recommend trying it out on both an agressive NPC and on friendly npc
// 【脚本功能】适用于敌对或友方生物，展示阶段转换、计时器管理、表情接收与技能施放等核心机制

// **** Quick Info ****
// Functions with Handled Function marked above them are functions that are called automatically by the core
// Functions that are marked Custom Function are functions I've created to simplify code
// 【快速提示】标记为 Handled Function 的函数由核心引擎自动回调，Custom Function 为自定义辅助逻辑

enum
{
    // List of text id's. The text is stored in database, also in a localized version
    // (if translation not exist for the textId, default english text will be used)
    // 台词文本 ID 列表（配置于数据库中，支持本地化文本）
    SAY_AGGRO       = -1999900,                             // 进战台词
    SAY_RANDOM_0    = -1999901,                             // 随机台词 0
    SAY_RANDOM_1    = -1999902,                             // 随机台词 1
    SAY_RANDOM_2    = -1999903,                             // 随机台词 2
    SAY_RANDOM_3    = -1999904,                             // 随机台词 3
    SAY_RANDOM_4    = -1999905,                             // 随机台词 4
    SAY_BESERK      = -1999906,                             // 狂暴台词
    SAY_PHASE       = -1999907,                             // 阶段转换台词
    SAY_DANCE       = -1999908,                             // 跳舞表情回应台词
    SAY_SALUTE      = -1999909,                             // 敬礼表情回应台词

    // List of used spells
    // 所用法术 ID 列表
    SPELL_BUFF      = 25661,                                // 脱战自身增益
    SPELL_ONE       = 12555,                                // 战斗技能 1 (近距离)
    SPELL_ONE_ALT   = 24099,                                // 战斗技能 1 备选 (远程)
    SPELL_TWO       = 10017,                                // 战斗技能 2 (自身范围 AoE)
    SPELL_THREE     = 26027,                                // 阶段 2 技能 3
    SPELL_ENRAGE    = 23537,                                // 阶段转换激怒法术
    SPELL_BESERK    = 32309,                                // 狂暴致死法术

    // Some other information we need to store
    // 杂项常量信息
    TEXT_ID_GREET   = 907,                                  // 对话问候文本 ID
    FACTION_WORGEN  = 24                                    // 狼人敌对阵营 ID
};

// List of gossip item texts. Items will appear in the gossip window.
// Actually such gossip can already be handled in normal World-Database
// If (and only if) a gossip must be handled within SD2, then it should be moved to SD2-database!
// 对话选项文本宏定义
#define GOSSIP_ITEM     "I'm looking for a fight"

// 自定义生物示例 AI (example_creatureAI)
struct example_creatureAI : public ScriptedAI
{
    // *** HANDLED FUNCTION ***
    // This is the constructor, called only once when the creature is first created
    // 构造函数：生物生成时调用一次，初始化 AI 并重置状态
    example_creatureAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    // *** CUSTOM VARIABLES ****
    // These variables are for use only by this individual script.
    // Nothing else will ever call them but us.
    // 自定义成员变量：仅供本脚本管理内部计时器与战斗阶段

    uint32 m_uiSayTimer;                                    // Timer for random chat (脱战随机说话计时器)
    uint32 m_uiRebuffTimer;                                 // Timer for rebuffing (脱战补 Buff 计时器)
    uint32 m_uiSpellOneTimer;                               // Timer for spell 1 when in combat (战斗技能 1 冷却计时器)
    uint32 m_uiSpellTwoTimer;                               // Timer for spell 1 when in combat (战斗技能 2 冷却计时器)
    uint32 m_uiSpellThreeTimer;                             // Timer for spell 1 when in combat (阶段 2 技能 3 计时器)
    uint32 m_uiBeserkTimer;                                 // Timer until we go into Beserk (enraged) mode (狂暴倒计时)
    uint32 m_uiPhase;                                       // The current battle phase we are in (当前战斗阶段)
    uint32 m_uiPhaseTimer;                                  // Timer until phase transition (阶段转换倒计时)

    // *** HANDLED FUNCTION ***
    // This is called whenever the core decides we need to evade
    // 脱战/重置函数：恢复初始阶段与各项计时器
    void Reset() override
    {
        m_uiPhase = 1;                                      // Start in phase 1 (从第 1 阶段开始)
        m_uiPhaseTimer = 60000;                             // 60 seconds (60秒后转阶段)
        m_uiSpellOneTimer = 5000;                           // 5 seconds (5秒)
        m_uiSpellTwoTimer = 37000;                          // 37 seconds (37秒)
        m_uiSpellThreeTimer = 19000;                        // 19 seconds (19秒)
        m_uiBeserkTimer = 120000;                           // 2 minutes (2分钟后狂暴)
    }

    // *** HANDLED FUNCTION ***
    // Aggro is called when we enter combat, against an enemy, and haven't been in combat before
    // 进入战斗回调：播放进战台词
    void Aggro(Unit* pWho) override
    {
        // Say some stuff (播放开战喊话)
        DoScriptText(SAY_AGGRO, m_creature, pWho);
    }

    // *** HANDLED FUNCTION ***
    // Our Recive emote function
    // 接收玩家表情动作回调：根据表情做出不同回应
    void ReceiveEmote(Player* /*pPlayer*/, uint32 uiTextEmote) override
    {
        m_creature->HandleEmote(uiTextEmote);

        switch (uiTextEmote)
        {
            case TEXTEMOTE_DANCE:
                DoScriptText(SAY_DANCE, m_creature);
                break;
            case TEXTEMOTE_SALUTE:
                DoScriptText(SAY_SALUTE, m_creature);
                break;
        }
    }

    // *** HANDLED FUNCTION ***
    // Update AI is called Every single map update (roughly once every 100ms if a player is within the grid)
    // AI 核心更新循环：脱战闲聊/补增益，战斗时技能施放、阶段推进与狂暴
    void UpdateAI(const uint32 uiDiff) override
    {
        // Out of combat timers (脱战状态逻辑)
        if (!m_creature->GetVictim())
        {
            // Random Say timer (脱战随机台词计时)
            if (m_uiSayTimer < uiDiff)
            {
                // Random switch between 5 outcomes (随机在 5 句台词中切换)
                switch (urand(0, 4))
                {
                    case 0: DoScriptText(SAY_RANDOM_0, m_creature); break;
                    case 1: DoScriptText(SAY_RANDOM_1, m_creature); break;
                    case 2: DoScriptText(SAY_RANDOM_2, m_creature); break;
                    case 3: DoScriptText(SAY_RANDOM_3, m_creature); break;
                    case 4: DoScriptText(SAY_RANDOM_4, m_creature); break;
                }

                m_uiSayTimer = 45 * IN_MILLISECONDS;        // Say something agian in 45 seconds (45 秒后再次说话)
            }
            else
                m_uiSayTimer -= uiDiff;

            // Rebuff timer (脱战补充增益计时)
            if (m_uiRebuffTimer < uiDiff)
            {
                DoCastSpellIfCan(m_creature, SPELL_BUFF);
                // Rebuff agian in 15 minutes (15 分钟后再次施加增益)
                m_uiRebuffTimer = 15 * MINUTE * IN_MILLISECONDS;
            }
            else
                m_uiRebuffTimer -= uiDiff;
        }

        // Return since we have no target (无敌对目标则返回)
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // Abilities of all phases (全阶段通用技能)
        // Spell One timer (技能 1 计时)
        if (m_uiSpellOneTimer < uiDiff)
        {
            // Cast spell one on our current target. (对当前目标施放技能 1)
            if (rand() % 50 > 10)
                DoCastSpellIfCan(m_creature->GetVictim(), SPELL_ONE_ALT);
            else if (m_creature->IsWithinDist(m_creature->GetVictim(), 25.0f))
                DoCastSpellIfCan(m_creature->GetVictim(), SPELL_ONE);

            m_uiSpellOneTimer = 5000;
        }
        else
            m_uiSpellOneTimer -= uiDiff;

        // Spell Two timer (技能 2 计时)
        if (m_uiSpellTwoTimer < uiDiff)
        {
            // Cast spell two on self (AoE spell with only self-target) if we can (施放技能 2 自身 AoE)
            if (DoCastSpellIfCan(m_creature, SPELL_TWO) == CAST_OK)
                m_uiSpellTwoTimer = 37 * IN_MILLISECONDS;   // Only Update Timer, if we could start casting
        }
        else
            m_uiSpellTwoTimer -= uiDiff;

        // End of abliities of all phases

        // Phase 1 abilities (第 1 阶段特有逻辑)
        if (m_uiPhase == 1)
        {
            // Phase timer (阶段转换倒计时)
            if (m_uiPhaseTimer < uiDiff)
            {
                // Only switch phase and display phase-switich text, if out cast was started sucessfull (成功施放激怒后转入第 2 阶段)
                if (DoCastSpellIfCan(m_creature, SPELL_ENRAGE) == CAST_OK)
                {
                    // Go to next phase
                    ++m_uiPhase;
                    DoScriptText(SAY_PHASE, m_creature);
                }
            }
            else
                m_uiPhaseTimer -= uiDiff;
        }
        // Phase 2 abilities (第 2 阶段特有逻辑)
        else if (m_uiPhase > 1)
        {
            // Spell Three timer (技能 3 计时)
            if (m_uiSpellThreeTimer < uiDiff)
            {
                // Cast spell three on self (AoE spell with only self-target) (施放技能 3)
                if (DoCastSpellIfCan(m_creature, SPELL_THREE) == CAST_OK)
                    m_uiSpellThreeTimer = 19000;
            }
            else
                m_uiSpellThreeTimer -= uiDiff;

            // Beserk timer (狂暴计时)
            if (m_uiBeserkTimer < uiDiff)
            {
                // Cast uber death spell if possible (施放狂暴致死法术)
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_BESERK) == CAST_OK)
                {
                    // Say our line if we cast (狂暴喊话)
                    DoScriptText(SAY_BESERK, m_creature, m_creature->GetVictim());

                    // Cast our beserk spell agian in 12 seconds (if we didn't kill everyone) (12 秒后再次狂暴施法)
                    m_uiBeserkTimer = 12000;
                }
            }
            else
                m_uiBeserkTimer -= uiDiff;
        }

        // Normal behaviour: if possible mobs do attack with melee (普通近战攻击)
        DoMeleeAttackIfReady();
    }
};

// This is the GetAI method used by all scripts that involve AI
// It is called every time a new creature using this script is created
// 创建并获取 AI 实例工厂函数
UnitAI* GetAI_example_creature(Creature* pCreature)
{
    return new example_creatureAI(pCreature);
}

// This function is called when the player opens the gossip menu
// In this case as there is nothing special about this gossip dialogue, it should be moved to world-DB
// 玩家打开对话菜单回调
bool GossipHello_example_creature(Player* pPlayer, Creature* pCreature)
{
    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
    pPlayer->SEND_GOSSIP_MENU(TEXT_ID_GREET, pCreature->GetObjectGuid());

    return true;
}

// This function is called when the player clicks an option on the gossip menu
// In this case here the faction change could be handled by world-DB gossip, hence it should be handled there!
// 玩家选择对话菜单选项回调：切换为敌对阵营并主动攻击玩家
bool GossipSelect_example_creature(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 1)
    {
        pPlayer->CLOSE_GOSSIP_MENU();
        // Set our faction to hostile towards all (临时更改阵营为敌对并开战)
        pCreature->SetFactionTemporary(FACTION_WORGEN, TEMPFACTION_RESTORE_RESPAWN);
        pCreature->AI()->AttackStart(pPlayer);
    }

    return true;
}

// This is the actual function called only once durring InitScripts()
// It must define all handled functions that are to be run in this script
// 注册示例生物脚本
void AddSC_example_creature()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "example_creature";
    pNewScript->GetAI = &GetAI_example_creature;
    pNewScript->pGossipHello = &GossipHello_example_creature;
    pNewScript->pGossipSelect = &GossipSelect_example_creature;
    pNewScript->RegisterSelf(false);
}
