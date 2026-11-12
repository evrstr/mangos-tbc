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
#ifndef __BATTLEGROUNDAB_H
#define __BATTLEGROUNDAB_H

#include "Common.h"
#include "BattleGround.h"

// 阿拉希盆地世界状态 ID（资源、占领据点数及五个据点图标状态）
enum ABWorldStates
{
    BG_AB_OP_OCCUPIED_BASES_HORDE       = 1778,
    BG_AB_OP_OCCUPIED_BASES_ALLY        = 1779,
    BG_AB_OP_RESOURCES_ALLY             = 1776,
    BG_AB_OP_RESOURCES_HORDE            = 1777,
    BG_AB_OP_RESOURCES_MAX              = 1780,
    BG_AB_OP_RESOURCES_WARNING          = 1955,

    BG_AB_OP_STABLE_ICON                = 1842,     // Stable map icon (NONE) (兽栏中立图标)
    BG_AB_OP_GOLDMINE_ICON              = 1843,     // Gold Mine map icon (NONE) (金矿中立图标)
    BG_AB_OP_LUMBERMILL_ICON            = 1844,     // Lumber Mill map icon (NONE) (伐木场中立图标)
    BG_AB_OP_FARM_ICON                  = 1845,     // Farm map icon (NONE) (农场中立图标)
    BG_AB_OP_BLACKSMITH_ICON            = 1846,     // Blacksmith map icon (NONE) (铁匠铺中立图标)

    BG_AB_OP_STABLE_STATE_A             = 1767,     // Stable map state (ALLIANCE) (兽栏联盟占领)
    BG_AB_OP_STABLE_STATE_H             = 1768,     // Stable map state (HORDE) (兽栏部落占领)
    BG_AB_OP_STABLE_STATE_CON_A         = 1769,     // Stable map state (CON ALLIANCE) (兽栏联盟争夺中)
    BG_AB_OP_STABLE_STATE_CON_H         = 1770,     // Stable map state (CON HORDE) (兽栏部落争夺中)

    BG_AB_OP_FARM_STATE_A               = 1772,     // Farm state (ALLIANCE) (农场联盟占领)
    BG_AB_OP_FARM_STATE_H               = 1773,     // Farm state (HORDE) (农场部落占领)
    BG_AB_OP_FARM_STATE_CON_A           = 1774,     // Farm state (CON ALLIANCE) (农场联盟争夺中)
    BG_AB_OP_FARM_STATE_CON_H           = 1775,     // Farm state (CON HORDE) (农场部落争夺中)

    BG_AB_OP_BLACKSMITH_STATE_A         = 1782,     // Blacksmith map state (ALLIANCE) (铁匠铺联盟占领)
    BG_AB_OP_BLACKSMITH_STATE_H         = 1783,     // Blacksmith map state (HORDE) (铁匠铺部落占领)
    BG_AB_OP_BLACKSMITH_STATE_CON_A     = 1784,     // Blacksmith map state (CON ALLIANCE) (铁匠铺联盟争夺中)
    BG_AB_OP_BLACKSMITH_STATE_CON_H     = 1785,     // Blacksmith map state (CON HORDE) (铁匠铺部落争夺中)

    BG_AB_OP_LUMBERMILL_STATE_A         = 1792,     // Lumber Mill map state (ALLIANCE) (伐木场联盟占领)
    BG_AB_OP_LUMBERMILL_STATE_H         = 1793,     // Lumber Mill map state (HORDE) (伐木场部落占领)
    BG_AB_OP_LUMBERMILL_STATE_CON_A     = 1794,     // Lumber Mill map state (CON ALLIANCE) (伐木场联盟争夺中)
    BG_AB_OP_LUMBERMILL_STATE_CON_H     = 1795,     // Lumber Mill map state (CON HORDE) (伐木场部落争夺中)

    BG_AB_OP_GOLDMINE_STATE_A           = 1787,     // Gold Mine map state (ALLIANCE) (金矿联盟占领)
    BG_AB_OP_GOLDMINE_STATE_H           = 1788,     // Gold Mine map state (HORDE) (金矿部落占领)
    BG_AB_OP_GOLDMINE_STATE_CON_A       = 1789,     // Gold Mine map state (CON ALLIANCE (金矿联盟争夺中)
    BG_AB_OP_GOLDMINE_STATE_CON_H       = 1790,     // Gold Mine map state (CON HORDE) (金矿部落争夺中)
};

// 阿拉希盆地开旗占领时间及荣誉/声望资源跳分阈值
enum ABTimers
{
    BG_AB_FLAG_CAPTURING_TIME           = 60000,            // 据点开旗到正式占领所需时间（60 秒）

    BG_AB_NORMAL_HONOR_INTERVAL         = 260,
    BG_AB_WEEKEND_HONOR_INTERVAL        = 160,
    BG_AB_NORMAL_REPUTATION_INTERVAL    = 160,
    BG_AB_WEEKEND_REPUTATION_INTERVAL   = 120,
};

// 阿拉希盆地资源胜利线与接近胜利警告线
enum ABScores
{
    BG_AB_WARNING_NEAR_VICTORY_SCORE    = 1800,
    BG_AB_MAX_TEAM_SCORE                = 2000
};

// Battleground Node Events
// node-events are just event1=ABNodes, event2=ABNodeStatus
// so we don't need to define the constants here :)
// do NOT change the order, else wrong behaviour
// (据点事件枚举：event1 对应 ABNodes，event2 对应 ABNodeStatus，切勿改动顺序以免与数据库事件索引错位)
enum ABNodes
{
    BG_AB_NODE_STABLES                  = 0,                // node values used in event1 for SpawnEvent (兽栏)
    BG_AB_NODE_BLACKSMITH               = 1,                // 铁匠铺
    BG_AB_NODE_FARM                     = 2,                // 农场
    BG_AB_NODE_LUMBER_MILL              = 3,                // 伐木场
    BG_AB_NODE_GOLD_MINE                = 4,                // 金矿

    BG_AB_MAX_NODES                     = 5,
};

// 据点状态基础偏移量（用于与队伍索引相加得到具体状态）
enum ABNodeType
{
    BG_AB_NODE_TYPE_NEUTRAL             = 0,
    BG_AB_NODE_TYPE_CONTESTED           = 1,                // logic for contested and occupied is: teamIndex + current value (争夺中基数)
    BG_AB_NODE_TYPE_OCCUPIED            = 3,                // this gives us the event2 value for SpawnEvent (已占领基数，对应 SpawnEvent 的 event2)
};

// 据点完整状态枚举
enum ABNodeStatus
{
    BG_AB_NODE_STATUS_NEUTRAL           = 0,                // values to be used for event2 in SpawnEventfunction (中立)
    BG_AB_NODE_STATUS_ALLY_CONTESTED    = 1,                // 联盟争夺中
    BG_AB_NODE_STATUS_HORDE_CONTESTED   = 2,                // 部落争夺中
    BG_AB_NODE_STATUS_ALLY_OCCUPIED     = 3,                // 联盟已占领
    BG_AB_NODE_STATUS_HORDE_OCCUPIED    = 4                 // 部落已占领
};

// 阿拉希盆地据点旗帜游戏对象模板 ID
enum ABBanners
{
    // neutral banners; all are GO type 10 (中立初始旗帜)
    BG_AB_BANNER_MINE                   = 180091,           // triggers spell 23937 
    BG_AB_BANNER_LUMBER_MILL            = 180090,           // triggers spell 23938
    BG_AB_BANNER_FARM                   = 180089,           // triggers spell 23935
    BG_AB_BANNER_BLACKSMITH             = 180088,           // triggers spell 23936
    BG_AB_BANNER_STABLE                 = 180087,           // triggers spell 23932

    // team banners; all are GO type 1; can be clicked by the opposite faction only (阵营占有与争夺中旗帜)
    BG_AB_BANNER_ALLIANCE               = 180058,
    BG_AB_BANNER_CONTESTED_A            = 180059,
    BG_AB_BANNER_HORDE                  = 180060,
    BG_AB_BANNER_CONTESTED_H            = 180061,
};

// 阿拉希盆地控制据点任务奖励法术
enum ABSpells
{
    BG_AB_SPELL_QUEST_REWARD_4_BASES    = 24061,                // quests 8114 and 8121
    BG_AB_SPELL_QUEST_REWARD_5_BASES    = 24064,                // quests 8115 and 8122
};

// 阿拉希盆地音效 ID
enum ABSounds
{
    BG_AB_SOUND_NODE_CLAIMED            = 8192,
    BG_AB_SOUND_NODE_CAPTURED_ALLIANCE  = 8173,
    BG_AB_SOUND_NODE_CAPTURED_HORDE     = 8213,
    BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE = 8212,
    BG_AB_SOUND_NODE_ASSAULTED_HORDE    = 8174,
    BG_AB_SOUND_NEAR_VICTORY_HORDE      = 8456,
    BG_AB_SOUND_NEAR_VICTORY_ALLIANCE   = 8457
};

// 阿拉希盆地墓地与区域 ID
enum ABGraveyards
{
    AB_GRAVEYARD_STABLES                = 895,
    AB_GRAVEYARD_BLACKSMITH             = 894,
    AB_GRAVEYARD_FARM                   = 893,
    AB_GRAVEYARD_LUMBER_MILL            = 897,
    AB_GRAVEYARD_GOLD_MINE              = 896,

    AB_GRAVEYARD_ALLIANCE               = 898,
    AB_GRAVEYARD_HORDE                  = 899,
    AB_GRAVEYARD_ALLIANCE_BASE = 890,
    AB_GRAVEYARD_HORDE_BASE = 889,

    BG_AB_ZONE_MAIN                     = 3358,
};

// 阿拉希盆地传送离开区域触发器 ID
enum ABAreaTriggers
{
    AB_AT_ALLIANCE_EXIT                 = 3948,
    AB_AT_HORDE_EXIT                    = 3949,
};

// default neutral world states (五个据点的默认中立世界状态 ID)
const uint32 abDefaultStates[BG_AB_MAX_NODES] = { BG_AB_OP_STABLE_ICON, BG_AB_OP_BLACKSMITH_ICON, BG_AB_OP_FARM_ICON, BG_AB_OP_LUMBERMILL_ICON, BG_AB_OP_GOLDMINE_ICON };

// occupied world states (双阵营占领各据点对应的世界状态 ID)
const uint32 abOccupiedStates[PVP_TEAM_COUNT][BG_AB_MAX_NODES] =
{
    {BG_AB_OP_STABLE_STATE_A, BG_AB_OP_BLACKSMITH_STATE_A, BG_AB_OP_FARM_STATE_A, BG_AB_OP_LUMBERMILL_STATE_A, BG_AB_OP_GOLDMINE_STATE_A },
    {BG_AB_OP_STABLE_STATE_H, BG_AB_OP_BLACKSMITH_STATE_H, BG_AB_OP_FARM_STATE_H, BG_AB_OP_LUMBERMILL_STATE_H, BG_AB_OP_GOLDMINE_STATE_H }
};

// contested world states (双阵营争夺各据点对应的世界状态 ID)
const uint32 abContestedStates[PVP_TEAM_COUNT][BG_AB_MAX_NODES] =
{
    {BG_AB_OP_STABLE_STATE_CON_A, BG_AB_OP_BLACKSMITH_STATE_CON_A, BG_AB_OP_FARM_STATE_CON_A, BG_AB_OP_LUMBERMILL_STATE_CON_A, BG_AB_OP_GOLDMINE_STATE_CON_A },
    {BG_AB_OP_STABLE_STATE_CON_H, BG_AB_OP_BLACKSMITH_STATE_CON_H, BG_AB_OP_FARM_STATE_CON_H, BG_AB_OP_LUMBERMILL_STATE_CON_H, BG_AB_OP_GOLDMINE_STATE_CON_H }
};

// Tick intervals and given points: case 0,1,2,3,4,5 captured nodes (按控制据点数 0..5 对应的资源产出间隔与每次点数)
const uint32 abTickIntervals[6] = {0, 12000, 9000, 6000, 3000, 1000};
const uint32 abTickPoints[6] = {0, 10, 10, 10, 10, 30};

// WorldSafeLocs ids (五个据点关联的墓地 ID)
const uint32 abGraveyardIds[BG_AB_MAX_NODES] = { AB_GRAVEYARD_STABLES, AB_GRAVEYARD_BLACKSMITH, AB_GRAVEYARD_FARM, AB_GRAVEYARD_LUMBER_MILL, AB_GRAVEYARD_GOLD_MINE };

// 据点旗帜模型切换延迟计时器（3 秒后切换为新状态旗帜）
struct ArathiBannerTimer
{
    uint32          timer;
    ABNodeStatus    status;
};

// 阿拉希盆地玩家战绩结构（扩展突袭与守卫基地次数）
class BattleGroundABScore : public BattleGroundScore
{
    public:
        BattleGroundABScore(): basesAssaulted(0), basesDefended(0) {};

        uint32 GetAttr1() const override { return basesAssaulted; }
        uint32 GetAttr2() const override { return basesDefended; }

        uint32 basesAssaulted;
        uint32 basesDefended;
};

// 阿拉希盆地（Arathi Basin）战场控制类：管理 5 座资源据点争夺、资源跳分、荣誉/声望结算及墓地切换
class BattleGroundAB : public BattleGround
{
        friend class BattleGroundMgr;

    public:
        BattleGroundAB();
        void Reset() override;
        void Update(uint32 diff) override;

        // Main battleground functions (主流程接口)
        void AddPlayer(Player* player) override;
        void StartingEventOpenDoors() override;
        void EndBattleGround(Team winner) override;

        // General functions (积分更新与人数不足提前判胜)
        void UpdatePlayerScore(Player* source, uint32 type, uint32 value) override;
        Team GetPrematureWinner() override;

        // Battleground event handlers (区域触发器与开旗点击事件)
        bool HandleAreaTrigger(Player* source, uint32 trigger) override;
        void HandlePlayerClickedOnFlag(Player* source, GameObject* target_obj) override;

    private:
        // Banner update functions (更新据点旗帜刷新状态与客户端世界状态图标)
        void DoUpdateBanner(uint8 node, ABNodeStatus nodeStatus, bool delay);
        void DoUpdateNodeWorldstate(uint8 node, uint32 newState);

        // Process node capture (1 分钟争夺结束正式完成占领并切换墓地归属)
        void ProcessNodeCapture(uint8 node, PvpTeamIndex teamIdx);

        // Get text id for node (获取据点名称语言文本 ID)
        int32 GetNodeMessageId(uint8 node) const;

        ABNodeStatus m_nodeStatus[BG_AB_MAX_NODES];         // store the current node status (各据点当前状态)
        ABNodeStatus m_prevNodeStatus[BG_AB_MAX_NODES];     // store the previous node status (各据点前一状态，用于重夺防守判断)
        ArathiBannerTimer m_bannerTimers[BG_AB_MAX_NODES];

        uint32 m_nodeVisualState[BG_AB_MAX_NODES];
        uint32 m_nodeTimers[BG_AB_MAX_NODES];
        uint32 m_lastTick[PVP_TEAM_COUNT];                  // timer that handles the points update (双方资源跳分累计计时器)
        uint32 m_honorScoreTicks[PVP_TEAM_COUNT];
        uint32 m_reputationScoreTics[PVP_TEAM_COUNT];

        bool   m_isInformedNearVictory;
        uint32 m_honorTicks;
        uint32 m_reputationTics;
};
#endif
