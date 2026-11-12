/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
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

#ifndef __BATTLEGROUNDWS_H
#define __BATTLEGROUNDWS_H

#include "BattleGround.h"
#include "Tools/Language.h"

class BattleGround;

// 战歌峡谷关键计时与得分上限常量
enum WSTimers
{
    BG_WS_MAX_TEAM_SCORE                    = 3,            // 获胜所需夺旗次数（3 次）
    BG_WS_FLAG_RESPAWN_TIME                 = 23 * IN_MILLISECONDS, // 交付战旗后基地重新刷旗时间（23 秒）
    BG_WS_FLAG_DROP_TIME                    = 10 * IN_MILLISECONDS, // 掉落地面战旗无人拾取自动归位时间（10 秒）
    BG_WS_FOCUSED_ASSAULT_TIME              = 10 * MINUTE * IN_MILLISECONDS, // 僵持 10 分钟触发集中攻击易伤
    BG_WS_BRUTAL_ASSAULT_TIME               = 15 * MINUTE * IN_MILLISECONDS, // 僵持 15 分钟升级为残暴攻击易伤
};

// 战歌峡谷声望与荣誉奖励数值（常规/节日周末）
enum WSVariables
{
    BG_WS_NORMAL_FLAG_CAPTURE_REPUTATION    = 35,
    BG_WS_WEEKEND_FLAG_CAPTURE_REPUTATION   = 45,
    BG_WS_NORMAL_WIN_KILLS                  = 1,
    BG_WS_WEEKEND_WIN_KILLS                 = 3,
    BG_WS_NORMAL_MAP_COMPLETE_KILLS         = 2,
    BG_WS_WEEKEND_MAP_COMPLETE_KILLS        = 4
};

// 战歌峡谷音效 ID
enum WSSounds
{
    BG_WS_SOUND_FLAG_CAPTURED_ALLIANCE  = 8173,
    BG_WS_SOUND_FLAG_CAPTURED_HORDE     = 8213,
    BG_WS_SOUND_FLAG_PLACED             = 8232,
    BG_WS_SOUND_FLAG_RETURNED           = 8192,
    BG_WS_SOUND_HORDE_FLAG_PICKED_UP    = 8212,
    BG_WS_SOUND_ALLIANCE_FLAG_PICKED_UP = 8174,
    BG_WS_SOUND_FLAGS_RESPAWNED         = 8232
};

// 战歌峡谷战旗携带、掉落与僵持易伤法术 ID
enum WSSpells
{
    BG_WS_SPELL_WARSONG_FLAG            = 23333,
    BG_WS_SPELL_WARSONG_FLAG_DROPPED    = 23334,
    BG_WS_SPELL_SILVERWING_FLAG         = 23335,
    BG_WS_SPELL_SILVERWING_FLAG_DROPPED = 23336,
    BG_WS_SPELL_FOCUSED_ASSAULT         = 46392,            // 50% increased damage to flag carriers after 10 minute stalemate (僵持 10 分钟后旗手承受伤害提高 50%)
    BG_WS_SPELL_BRUTAL_ASSAULT          = 46393,            // 100% increased damage to flag carriers after 15 minute stalemate (僵持 15 分钟后旗手承受伤害提高 100%)
};

// 战歌峡谷世界状态 ID
enum WSWorldStates
{
    BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_ALLIANCE = 1545,
    BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_HORDE    = 1546,
//  FLAG_UNK                            = 1547,

    BG_WS_STATE_CAPTURES_ALLIANCE       = 1581,             // count the flag captures for each team (联盟已夺旗次数)
    BG_WS_STATE_CAPTURES_HORDE          = 1582,             // 部落已夺旗次数
    BG_WS_STATE_CAPTURES_MAX            = 1601,             // shows the max allowed flags (获胜上限次数 3)

    BG_WS_STATE_FLAG_HORDE              = 2338,             // shows who captured the current flag (部落战旗当前界面图标状态)
    BG_WS_STATE_FLAG_ALLIANCE           = 2339,             // 联盟战旗当前界面图标状态
};

// 战旗动作索引
enum WSFlagActions
{
    BG_WS_FLAG_ACTION_NONE              = -1,
    BG_WS_FLAG_ACTION_PICKEDUP          = 0,
    BG_WS_FLAG_ACTION_RETURNED          = 1,
    BG_WS_FLAG_ACTION_DROPPED           = 2,
    BG_WS_FLAG_ACTION_CAPTURED          = 3,
    BG_WS_FLAG_ACTION_RESPAWN           = 4,

    BG_WS_FLAG_ACTIONS_TOTAL            = 5,
};

// 战旗物理所在位置状态
enum WSPickedUpFlagStates
{
    BG_WS_FLAG_STATE_ON_BASE            = 0,                // 在己方基地底座
    BG_WS_FLAG_STATE_ON_PLAYER          = 1,                // 被敌方玩家携带中
    BG_WS_FLAG_STATE_ON_GROUND          = -1                // 掉落在地面上
};

// 客户端顶部 HUD 战旗图标显示状态
enum WSFlagIconStates
{
    BG_WS_FLAG_ICON_INACTIVE            = 0,
    BG_WS_FLAG_ICON_INVISIBLE           = 1,
    BG_WS_FLAG_ICON_VISIBLE             = 2,
};

// 战歌峡谷墓地与区域 ID
enum WSGraveyards
{
    WS_GRAVEYARD_FLAGROOM_ALLIANCE      = 769,
    WS_GRAVEYARD_FLAGROOM_HORDE         = 770,
    WS_GRAVEYARD_MAIN_ALLIANCE          = 771,
    WS_GRAVEYARD_MAIN_HORDE             = 772,

    BG_WS_ZONE_ID_MAIN                  = 3277,
};

// 战旗游戏对象交互触发的事件 ID
enum WSEventIds
{
    WS_EVENT_ALLIANCE_FLAG_PICKUP       = 8504,             // triggered from flag events - source player, target go (从基地拾取联盟战旗)
    WS_EVENT_HORDE_FLAG_PICKUP          = 8505,             // 从基地拾取部落战旗

    WS_EVENT_ALLIANCE_FLAG_DROP         = 8506,             // not used; events used for flag handling; triggered from spell (未使用：丢旗由法术/移除光环直接触发)
    WS_EVENT_HORDE_FLAG_DROP            = 8507,

    WS_EVENT_ALLIANCE_FLAG_DROPPED_PICKUP = 8623,           // 与地面掉落战旗交互事件
    WS_EVENT_HORDE_FLAG_DROPPED_PICKUP  = 8624,
};

// 战歌峡谷旗帜游戏对象模板 ID
enum WSGameObjects
{
    GO_WS_SILVERWING_FLAG               = 179830,
    GO_WS_WARSONG_FLAG                  = 179831,

    GO_WS_SILVERWING_FLAG_DROP          = 179785,           // temp summoned objects when main flag is dropped (战旗掉落时在地面召唤的临时旗帜)
    GO_WS_WARSONG_FLAG_DROP             = 179786,
};

// 双方旗室交旗区域触发器 ID
enum WSAreaTriggers
{
    WS_AT_SILVERWING_ROOM               = 3646,
    WS_AT_WARSONG_ROOM                  = 3647,
};

// 战歌峡谷数据库刷新事件索引
enum WSScriptEvents
{
    WS_EVENT_FLAG_A                     = 0,
    WS_EVENT_FLAG_H                     = 1,
    // spiritguides will spawn (same moment, like WS_EVENT_DOOR_OPEN) (开战同时刷出双方灵魂向导)
    WS_EVENT_SPIRITGUIDES_SPAWN         = 2
};

static const uint32 wsFlagPickedUp[PVP_TEAM_COUNT] = { BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_ALLIANCE, BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_HORDE };
static const uint32 wsFlagHUDPickedUp[PVP_TEAM_COUNT] = { BG_WS_STATE_FLAG_ALLIANCE, BG_WS_STATE_FLAG_HORDE };

static const uint32 wsDroppedFlagId[PVP_TEAM_COUNT] = { GO_WS_SILVERWING_FLAG_DROP, GO_WS_WARSONG_FLAG_DROP };

// 战旗动作对应的消息、音效与光环数据表项
struct WarsongData
{
    uint8 flagAction;
    uint32 messageId, soundId, spellId;
    ChatMsg chatType;
};

// *** Battleground flag data *** (战歌峡谷双方战旗各动作提示、音效与法术配置表) //
static const WarsongData wsgFlagData[PVP_TEAM_COUNT][BG_WS_FLAG_ACTIONS_TOTAL] =
{
    {
        {BG_WS_FLAG_ACTION_PICKEDUP, LANG_BG_WS_PICKEDUP_AF,             BG_WS_SOUND_ALLIANCE_FLAG_PICKED_UP, BG_WS_SPELL_SILVERWING_FLAG,         CHAT_MSG_BG_SYSTEM_ALLIANCE},
        {BG_WS_FLAG_ACTION_RETURNED, LANG_BG_WS_RETURNED_AF,             BG_WS_SOUND_FLAG_RETURNED,           BG_WS_SPELL_SILVERWING_FLAG,         CHAT_MSG_BG_SYSTEM_ALLIANCE},
        {BG_WS_FLAG_ACTION_DROPPED,  LANG_BG_WS_DROPPED_AF,              0,                                   BG_WS_SPELL_SILVERWING_FLAG_DROPPED, CHAT_MSG_BG_SYSTEM_ALLIANCE},
        {BG_WS_FLAG_ACTION_CAPTURED, LANG_BG_WS_CAPTURED_AF,             BG_WS_SOUND_FLAG_CAPTURED_ALLIANCE,  0,                                   CHAT_MSG_BG_SYSTEM_ALLIANCE},
        {BG_WS_FLAG_ACTION_RESPAWN,  LANG_BG_WS_ALLIANCE_FLAG_RESPAWNED, BG_WS_SOUND_FLAGS_RESPAWNED,         0,                                   CHAT_MSG_BG_SYSTEM_NEUTRAL},
    },
    {
        {BG_WS_FLAG_ACTION_PICKEDUP, LANG_BG_WS_PICKEDUP_HF,          BG_WS_SOUND_HORDE_FLAG_PICKED_UP, BG_WS_SPELL_WARSONG_FLAG,         CHAT_MSG_BG_SYSTEM_HORDE},
        {BG_WS_FLAG_ACTION_RETURNED, LANG_BG_WS_RETURNED_HF,          BG_WS_SOUND_FLAG_RETURNED,        BG_WS_SPELL_WARSONG_FLAG,         CHAT_MSG_BG_SYSTEM_HORDE},
        {BG_WS_FLAG_ACTION_DROPPED,  LANG_BG_WS_DROPPED_HF,           0,                                BG_WS_SPELL_WARSONG_FLAG_DROPPED, CHAT_MSG_BG_SYSTEM_HORDE},
        {BG_WS_FLAG_ACTION_CAPTURED, LANG_BG_WS_CAPTURED_HF,          BG_WS_SOUND_FLAG_CAPTURED_HORDE,  0,                                CHAT_MSG_BG_SYSTEM_HORDE},
        {BG_WS_FLAG_ACTION_RESPAWN,  LANG_BG_WS_HORDE_FLAG_RESPAWNED, BG_WS_SOUND_FLAGS_RESPAWNED,      0,                                CHAT_MSG_BG_SYSTEM_NEUTRAL},
    }
};

// 战歌峡谷玩家战绩结构（扩展夺旗与护旗次数）
class BattleGroundWGScore : public BattleGroundScore
{
    public:
        BattleGroundWGScore() : flagCaptures(0), flagReturns(0) {};

        uint32 GetAttr1() const override { return flagCaptures; }
        uint32 GetAttr2() const override { return flagReturns; }

        uint32 flagCaptures;
        uint32 flagReturns;
};

// 战歌峡谷（Warsong Gulch）战场控制类：管理夺旗、掉旗、归还、重刷计时与僵持易伤叠加
class BattleGroundWS : public BattleGround
{
    friend class BattleGroundMgr;

    public:
        BattleGroundWS();
        void Reset() override;
        void Update(uint32 diff) override;

        // Main battleground functions (主流程钩子：进场、离场、开门、结算)
        void AddPlayer(Player* player) override;
        void RemovePlayer(Player* player, ObjectGuid guid) override;
        void StartingEventOpenDoors() override;
        void EndBattleGround(Team winner) override;

        // General functions (通用积分更新与人数不足提前判胜)
        void UpdatePlayerScore(Player* source, uint32 type, uint32 value) override;
        Team GetPrematureWinner() override;

        // Battleground event handlers (战旗事件、交旗区域触发器、击杀与对象创建回调)
        bool HandleEvent(uint32 eventId, Object* source, Object* target) override;
        bool HandleAreaTrigger(Player* source, uint32 trigger) override;
        void HandleGameObjectCreate(GameObject* go) override;
        void HandleKillPlayer(Player* player, Player* killer) override;
        void HandlePlayerClickedOnFlag(Player* source, GameObject* go) override;
        void HandlePlayerDroppedFlag(Player* source) override;

        // Flag handler (获取指定阵营旗帜当前持旗者 GUID)
        ObjectGuid const& GetFlagCarrierGuid(uint8 teamIdx) const { return m_flagCarrier[teamIdx]; }

        // 判断指定阵营旗手是否已持旗满 45 秒（用于特定成就/事件判定）
        bool IsFlagHeldFor45Seconds(Team flagHolderTeam);

    private:

        // Flag Carrier functions (持旗者状态设置与查询)
        void SetFlagCarrier(uint8 teamIdx, ObjectGuid guid) { m_flagCarrier[teamIdx] = guid; }
        void ClearFlagCarrier(uint8 teamIdx) { m_flagCarrier[teamIdx].Clear(); }
        bool IsFlagPickedUp(uint8 teamIdx) const { return !m_flagCarrier[teamIdx].IsEmpty(); }

        // Flag interactions (掉落地面战旗管理、基地与地面战旗刷新)
        void ClearDroppedFlagGuid(Team team)  { m_droppedFlagGuid[GetTeamIndexByTeamId(team)].Clear();}
        ObjectGuid const& GetDroppedFlagGuid(Team team) const { return m_droppedFlagGuid[GetTeamIndexByTeamId(team)];}

        void RespawnFlagAtBase(Team team, bool wasCaptured);
        void RespawnDroppedFlag(Team team);
        int32 GetFlagState(Team team);

        void ProcessFlagPickUpFromBase(Player* player, Team attackerTeam); // 处理从敌方基地拔起战旗
        void ProcessDroppedFlagActions(Player* player, GameObject* target); // 处理点击地面掉落战旗（己方归还/敌方拾取）

        // process score (处理玩家成功交旗得分及双方僵持状态重置)
        void ProcessPlayerFlagScoreEvent(Player* source);

        ObjectGuid m_droppedFlagGuid[PVP_TEAM_COUNT];
        ObjectGuid m_flagCarrier[PVP_TEAM_COUNT];

        bool m_flagOnRespawn[PVP_TEAM_COUNT];
        TimePoint m_flagPickupFromBaseTime[PVP_TEAM_COUNT];
        uint32 m_flagsTimer[PVP_TEAM_COUNT];
        uint32 m_flagsDropTimer[PVP_TEAM_COUNT];
        uint32 m_flagCarrierDebuffTimer;
        bool m_focusedAssaultActive;
        bool m_brutalAssaultActive;

        uint32 m_reputationCapture;
        uint32 m_honorWinKills;
        uint32 m_honorEndKills;
        Team   m_lastCapturedFlagTeam;
};
#endif
