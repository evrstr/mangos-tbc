/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_GUARDAI_H
#define SC_GUARDAI_H

#include "AI/ScriptDevAI/base/CombatAI.h"

// 守卫通用常量定义（地精城镇打手、塞纳里奥要塞守卫台词与法术）
enum GuardAI
{
    GENERIC_CREATURE_COOLDOWN       = 5000,

    SAY_GUARD_SIL_AGGRO1            = -1000198,
    SAY_GUARD_SIL_AGGRO2            = -1000199,
    SAY_GUARD_SIL_AGGRO3            = -1000200,

    // Goblin Cities & Events
    RATCHET_BRUISER                 = 3502,
    BOOTY_BAY_BRUISER               = 4624,
    GADGETZAN_BRUISER               = 9460,
    EVERLOOK_BRUISER                = 11190,
    STEAMWHEEDLE_BRUISER            = 16096,
    AREA_52_BIG_BRUISER             = 20484,
    AREA_52_BRUISER                 = 20485,
    COSMOWRENCH_BRUISER             = 22494,
    MUDSPROCKET_BRUISER             = 23636,
    CONCERT_BRUISER                 = 23721,

    SPELL_SHOOT                     = 23337, // not all shoot, currently unused
    SPELL_NET                       = 12024,

    // Cenarion Hold
    CENARION_HOLD_INFANTRY          = 15184,
    SPELL_INCAPACITATING_SHOUT      = 18328,
};

// 沙塔斯城守卫专用法术（放逐、驱逐）
enum eShattrathGuard
{
    SPELL_BANISHED_SHATTRATH_A      = 36642,
    SPELL_BANISHED_SHATTRATH_S      = 36671,
    SPELL_BANISH_TELEPORT           = 36643,
    SPELL_EXILE                     = 39533
};

// 万圣节节日救火守卫事件相关法术、文本与 NPC
enum eHallowsEndGuard
{
    SPELL_START_FIRE_TARGET_TEST_GUARD  = 43711,
    SPELL_FIRE                          = 42074,

    TEXT_ID_AZUREMYST_PEACEKEEPER       = 12119,
    TEXT_ID_RAZOR_HILL_GRUNT            = 60004,
    TEXT_ID_DEATHGUARD_FLORENCE         = 60005,
    TEXT_ID_SILVERMOON_GUARDIAN         = 60006,
    TEXT_ID_IRONFORGE_MOUNTAINEER       = 60007,
    TEXT_ID_STORMWIND_GUARD             = 60008,
    SAY_GUARD_RELIGHT_FIRE              = -1015137,
    
    NPC_HEADLESS_HORSEMAN_FIRE          = 23537,
    NPC_DEATHGUARD_FLORENCE             = 24491,
    NPC_RAZOR_HILL_GRUNT                = 5953,
    NPC_IRONFORGE_MOUNTAINEER           = 727,
    NPC_STORMWIND_GUARD                 = 1423,
    NPC_SILVERMOON_GUARDIAN             = 16221,
    NPC_AZUREMYST_PEACEKEEPER           = 18038,
};

// 阵营/城镇通用守卫 AI 基类：处理防务警报、技能循环与玩家表情互动
struct guardAI : public CombatAI
{
    public:
        explicit guardAI(Creature* creature);
        ~guardAI() {}

        uint32 m_globalCooldown;                          // 模拟公共冷却时间 (GCD)
                          // This variable acts like the global cooldown that players have (1.5 seconds)
        uint32 m_buffTimer;                               // 增益状态维护计时器
                               // This variable keeps track of buffs

        uint32 m_IncapacitatingShoutTimer;               // 瘫痪怒吼施法计时器

        uint32 m_NetTimer;                               // 投掷捕网施法计时器


        void Reset() override;                              // 重置状态与施法计时器

        void Aggro(Unit* who) override;                           // 进入战斗处理（播放开战台词）

        void JustDied(Unit* /*killer*/) override;               // 守卫阵亡回调（触发区域防务报警广播）
        void JustRespawned() override;                          // 重新刷新回调（自动检测远程攻击配置）

        // Commonly used for guards in main cities
        void DoReplyToTextEmote(uint32 textEmote);             // 回应玩家文本表情动作（敬礼、招手等）
};

// 奥格瑞玛守卫专用 AI：仅响应部落玩家表情
struct guardAI_orgrimmar : public guardAI
{
    guardAI_orgrimmar(Creature* creature) : guardAI(creature) {}

    void ReceiveEmote(Player* player, uint32 textEmote) override; // 接收玩家表情输入
};

#endif
