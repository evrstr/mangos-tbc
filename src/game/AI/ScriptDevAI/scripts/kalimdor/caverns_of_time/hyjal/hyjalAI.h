/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_HYJALAI_H
#define SC_HYJALAI_H

/**
 * 海加尔峰战役：通用战斗 AI 基类定义
 */

#include "hyjal.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// Misc 枚举定义
enum Misc
{
    MAX_SPELL               = 3,

    ITEM_TEAR_OF_GODDESS    = 24494,                        // 物品条目 ID

    SAY_CALL_FOR_HELP_EMOTE = -1000008,                     // 剧情台词/喊话文本 ID
};

// HyjalSpells 枚举定义
enum HyjalSpells
{
    SPELL_MASS_TELEPORT     = 16807,                        // 技能法术 ID

    // Spells for Jaina (法术)
    SPELL_BRILLIANCE_AURA   = 31260,                        // 技能法术 ID
    SPELL_BLIZZARD          = 31266,                        // 技能法术：暴风雪
    SPELL_PYROBLAST         = 31263,                        // 技能法术：炎爆术
    SPELL_SUMMON_ELEMENTALS = 31264,                        // 技能法术 ID
    NPC_WATER_ELEMENTAL     = 18001,                        // NPC 条目 ID

    // Thrall spells (法术)
    SPELL_CHAIN_LIGHTNING   = 31330,                        // 技能法术：闪电链
    SPELL_FERAL_SPIRIT      = 31331,                        // 技能法术 ID
    NPC_FERAL_SPIRIT        = 17854,                        // NPC 条目 ID

    // Tyrande spells (法术)
    SPELL_STARFALL          = 20687,                        // 技能法术 ID
    SPELL_TRUESHOT_AURA     = 31519,                        // 技能法术 ID
};

enum TargetType                                             // Used in the spell cast system for the AI (说明)
{
    TARGETTYPE_SELF     = 0,
    TARGETTYPE_RANDOM   = 1,
    TARGETTYPE_VICTIM   = 2,
};

// YellType 枚举定义
enum YellType
{
    ATTACKED     = 0,                                       // Used when attacked and below 90% HP (说明)
    INCOMING     = 1,                                       // Used to warn the raid that another wave phase is coming (说明)
    RALLY        = 2,                                       // Used to rally the raid randomly while a wave event is in progress (说明)
    RETREAT      = 3,                                       // Used when the scourge mob count reaches 30 (which fails the event) (说明)
    WIN          = 4,                                       // Used when both bosses are done and a player asks the NPC to teleport away (说明)
    DEATH        = 5,                                       // Used on death (说明)
};

// HyjalAIActions 战斗行为动作枚举
enum HyjalAIActions
{
    HYJAL_AI_90,
    HYJAL_AI_ATTACKED_TIMER,
    HYJAL_AI_ACTION_MAX,                                    // 战斗行为动作
    HYJAL_AI_RALLY_TIMER = 100, 
};

// hyjalAI AI 逻辑实现
struct hyjalAI : public CombatAI
{
        hyjalAI(Creature* creature, uint32 maxActions);

        // Generically used to reset our variables. Do *not* call in EnterEvadeMode as this may make problems if the raid is still in combat (说明)
        void Reset() override;                              // 重置战斗状态与计时器

        // Send creature back to spawn location and evade. (说明)
        void EnterEvadeMode() override;                     // 脱战重置并返回原位

        // Called when creature reached home location after evade. (说明)
        void JustReachedHome() override;

        void JustRespawned() override;

        void JustDied(Unit* /*killer*/) override;

        void ReceiveAIEvent(AIEventType /*eventType*/, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override;

        void ExecuteAction(uint32 action) override;         // 执行战斗行为轮换

        // Instance response handlers (说明)
        void EventStarted();
        void Retreat();
        void Win();

        bool IsEventStarted() { return m_started; }

        // Searches for the appropriate yell and sound and uses it to inform the raid of various things (说明)
        void DoTalk(YellType yellType);

        instance_mount_hyjal* m_instance;

        bool m_started;
};

#endif
