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

#ifndef __BATTLEGROUNDAV_H
#define __BATTLEGROUNDAV_H

#include "Common.h"
#include "BattleGround.h"

// 奥特兰克山谷各类计时常量
enum AVTimers
{
    BG_AV_MINE_YELL                     = 25 * IN_MILLISECONDS, // 矿洞占领后矿工喊话提示延迟（25 秒）
    BG_AV_MINE_TICK_TIMER               = 45 * IN_MILLISECONDS, // 控制矿洞每 45 秒提供援军点数间隔
    BG_AV_MINE_RECLAIM_TIMER            = 20 * MINUTE * IN_MILLISECONDS,         // TODO: get the right value.. this is currently 20 minutes (矿洞被夺走后自然恢复中立的时间，暂定 20 分钟)

    BG_AV_CAPTIME                       = 4 * MINUTE * IN_MILLISECONDS,          // 4 minutes (墓地与哨塔开旗后正式占领/摧毁所需时间：4 分钟)
    BG_AV_SNOWFALL_FIRSTCAP             = 5 * MINUTE * IN_MILLISECONDS,          // 5 minutes but i also have seen 4:05 (落雪墓地首次占领耗时)
};

// 奥特兰克山谷通用资源分、声望与荣誉常量
enum AVGenericVariables
{
    BG_AV_MAX_MINES                     = 2,
    BG_AV_MAX_GRAVETYPES                = 4,
    BG_AV_MAX_GRAVEYARDS                = 9,
    BG_AV_MAX_STATES                    = 2,
    BG_AV_MAX_NODES                     = 15,
    BG_AV_MAX_TOWERS_PER_TEAM           = 4,
    BG_AV_TEAMS_COUNT                   = PVP_TEAM_COUNT + 1,

    BG_AV_MAX_NODE_DISTANCE             = 25,              // distance in which players are still counted in range of a banner (for alliance towers this is calculated from the center of the tower) (计算据点范围奖励的有效半径：25 码)

    BG_AV_SCORE_INITIAL_POINTS          = 600,             // 双方初始援军兵力点数（600）
    BG_AV_SCORE_NEAR_LOSE               = 120,             // 兵力低于 120 时触发濒败警告

    // description: KILL = bonushonor kill one kill is 21honor worth at 0
    // REP reputation, RES = ressources a team will lose
    // (奖励说明：KILL 为额外荣誉击杀数，REP 为声望，RES 为敌方扣除援军兵力)
    BG_AV_KILL_BOSS                     = 4,
    BG_AV_REP_BOSS                      = 350,
    BG_AV_REP_BOSS_HOLIDAY              = 525,

    BG_AV_KILL_CAPTAIN                  = 3,
    BG_AV_REP_CAPTAIN                   = 125,
    BG_AV_REP_CAPTAIN_HOLIDAY           = 185,
    BG_AV_RES_CAPTAIN                   = 100,

    BG_AV_KILL_TOWER                    = 3,
    BG_AV_REP_TOWER                     = 12,
    BG_AV_REP_TOWER_HOLIDAY             = 18,
    BG_AV_RES_TOWER                     = 75,

    BG_AV_KILL_GET_COMMANDER            = 1,               // for a safely returned wingcommander TODO implement it (安全护送空军指挥官归队奖励，待实现)

    // bonushonor at the end (战场结束时按存活哨塔与中尉发放的结算奖励)
    BG_AV_KILL_SURVIVING_TOWER          = 2,
    BG_AV_REP_SURVIVING_TOWER           = 12,
    BG_AV_REP_SURVIVING_TOWER_HOLIDAY   = 18,

    BG_AV_KILL_SURVIVING_CAPTAIN        = 2,
    BG_AV_REP_SURVIVING_CAPTAIN         = 125,
    BG_AV_REP_SURVIVING_CAPTAIN_HOLIDAY = 175,

    BG_AV_KILL_MAP_COMPLETE             = 0,
    BG_AV_KILL_MAP_COMPLETE_HOLIDAY     = 4,

    BG_AV_REP_OWNED_GRAVE               = 12,
    BG_AV_REP_OWNED_GRAVE_HOLIDAY       = 18,

    BG_AV_REP_OWNED_MINE                = 24,
    BG_AV_REP_OWNED_MINE_HOLIDAY        = 36,

    BG_AV_AREA_ID_FIELD_OF_STRIFE       = 3057,             // area check for achievs (征战平原区域 ID)
};

// 奥特兰克山谷相关法术 ID
enum AVSpells
{
    BG_AV_SPELL_COMPLETE_AV_QUEST       = 23658,            // complete quests 7141 and 7142; cast individually per team, because spell completes both quests at once (完成首领击杀任务法术，须按阵营分别施放)

    BG_AV_SPELL_MARK_HONOR_LOSER        = 24954,
    BG_AV_SPELL_MARK_HONOR_WINNER       = 24955,

    BG_AV_SPELL_MINE_CAPTURE            = 43941,
};

// 奥特兰克山谷出入口区域触发器 ID
enum AVAreaTriggers
{
    BG_AV_AT_ENTRANCE_ALLIANCE1         = 95,
    BG_AV_AT_ENTRANCE_ALLIANCE2         = 2608,
    BG_AV_AT_ENTRANCE_HORDE             = 2606,
};

// 雷矛卫队与霜狼氏族声望阵营 ID
enum AVFactions
{
    // factions ids used only to reward reputation; (仅用于发放声望的阵营 ID)
    BG_AV_FACTION_ID_STORMPIKE          = 730,
    BG_AV_FACTION_ID_FROSTWOLF          = 729,
};

// 奥特兰克山谷音效 ID
enum AVSounds
{
    BG_AV_SOUND_NEAR_LOSE               = 8456,             // not confirmed yet (濒败警报音效)

    BG_AV_SOUND_ALLIANCE_ASSAULTS       = 8212,             // tower,grave + enemy boss if someone tries to attack him (联盟突袭据点或首领)
    BG_AV_SOUND_HORDE_ASSAULTS          = 8174,             // 部落突袭据点或首领
    BG_AV_SOUND_ALLIANCE_GOOD           = 8173,             // if something good happens for the team:  wins(maybe only through killing the boss), captures mine or grave, destroys tower and defends grave (联盟有利事件音效)
    BG_AV_SOUND_HORDE_GOOD              = 8213,             // 部落有利事件音效
    BG_AV_SOUND_BOTH_TOWER_DEFEND       = 8192,

    BG_AV_SOUND_ALLIANCE_CAPTAIN        = 8232,             // gets called when someone attacks them and at the beginning after 3min + rand(x) * 10sec (maybe buff) (巴琳达/石炉中尉喊话音效)
    BG_AV_SOUND_HORDE_CAPTAIN           = 8333,             // 加尔范上尉喊话音效
};

// 奥特兰克山谷游戏对象模板 ID
enum AVGameObjects
{
    // chests (矿洞补给箱与阵营战旗)
    BG_AV_GO_IRONDEEP_SUPPLIES          = 178785,           // object type 3
    BG_AV_GO_COLDTOOTH_SUPPLIES         = 178784,
    BG_AV_GO_FROSTWOLF_BANNER           = 179025,
    BG_AV_GO_STORMPIKE_BANNER           = 179024,

    BG_AV_GO_ALTERAC_GATES              = 180424,

    // tower banners (哨塔/碉堡旗帜)
    BG_AV_GO_BANNER_ALLIANCE            = 178925,           // control flags
    BG_AV_GO_BANNER_HORDE               = 178943,
    BG_AV_GO_BANNER_ALLIANCE_CONT       = 178940,           // contested flags
    BG_AV_GO_BANNER_HORDE_CONT          = 179435,
    // the tower visual banners are triggered automatically
    // object entries: alliance: 178927; horde contested: 179436
    // object entries: horde: 178955; alliance contested: 179446

    // graveyard banners (墓地旗帜)
    BG_AV_GO_GY_BANNER_ALLIANCE         = 178365,           // control flags
    BG_AV_GO_GY_BANNER_HORDE            = 178364,
    BG_AV_GO_GY_BANNER_ALLIANCE_CONT    = 179286,           // contested flags
    BG_AV_GO_GY_BANNER_HORDE_CONT       = 179287,

    // middle graveyard banner; object type 10 (中立落雪墓地旗帜)
    BG_AV_GO_GY_BANNER_SNOWFALL         = 180418,           // triggers spell 24677
    // the middle graveyard banners are triggered automatically
    // object entries: alliance: 179044; alliance contested: 179424
    // object entries: horde: 179064; horde contested: 179425

    BG_AV_GO_ROARING_FLAME              = 179065,           // fire trap which appears when a tower is destroyed (哨塔被烧毁后出现的烈焰陷阱)
};

// 奥特兰克山谷关键生物模板 ID
enum AVCreatures
{
    // alliance bosses (联盟统帅范达尔·雷矛及四名将领、石炉中尉)
    BG_AV_NPC_VANNDAR_STORMPIKE         = 11948,            // main boss and the marshals
    BG_AV_NPC_ICEWING_MARSHAL           = 14764,
    BG_AV_NPC_STONEHEARTH_MARSHAL       = 14765,
    BG_AV_NPC_NORTH_MARSHAL             = 14762,
    BG_AV_NPC_SOUTH_MARSHAL             = 14763,

    BG_AV_NPC_CAPTAIN_STONEHEARTH       = 11949,            // middle alliance boss (巴琳达·斯通赫尔斯)

    // horde bosses (部落大酋长德雷克塔尔、双狼宠物、四名督军及加尔范上尉)
    BG_AV_NPC_DREKTHAR                  = 11946,            // main boss with the 2 wolf pets and the warmasters
    BG_AV_NPC_DRAKAN                    = 12121,
    BG_AV_NPC_DUROS                     = 12122,
    BG_AV_NPC_EAST_WARMASTER            = 14772,
    BG_AV_NPC_ICEBLOOD_WARMASTER        = 14773,
    BG_AV_NPC_TOWERPOINT_WARMASTER      = 14776,
    BG_AV_NPC_WEST_WARMASTER            = 14777,

    BG_AV_NPC_CAPTAIN_GALVANGAR         = 11947,            // middle horde boss (加尔范上尉)

    // mine creatures north - irondeep (北部深铁矿洞首领)
    BG_AV_NPC_MORLOCH                   = 11657,            // neutral mine leader
    BG_AV_NPC_UMI_THORSON               = 13078,            // alliance mine leader
    BG_AV_NPC_KEETAR                    = 13079,            // horde mine leader

    // mine creatures south - coldtooth (南部冷齿矿洞首领)
    BG_AV_NPC_TASKMASTER_SNIVVLE        = 11677,            // neutral leader
    BG_AV_NPC_AGGI_RUMBLESTOMP          = 13086,            // alliance mine leader
    BG_AV_NPC_MASHA_SWIFTCUT            = 13088,            // horde mine leader

    // creatures used to yell on the map (负责全图喊话播报的传令官 NPC)
    BG_AV_NPC_STORMPIKE_HERALD          = 11997,
    BG_AV_NPC_FROSTWOLF_HERALD          = 11998,
    BG_AV_NPC_HERALD                    = 14848,
};

// 奥特兰克山谷占领任务击杀计数虚拟生物 ID
enum AVKillCredits
{
    BG_AV_NPC_PVP_MINE_CREDIT           = 13796,
    BG_AV_NPC_PVP_GRAVEYARD_CREDIT      = 13756,
    BG_AV_NPC_PVP_TOWER_CREDIT          = 13778,
};

// 奥特兰克山谷 15 个可争夺据点索引（7 座墓地 + 8 座碉堡/哨塔）
enum AVNodeIds
{
    BG_AV_NODES_FIRSTAID_STATION        = 0,                // 雷矛急救站
    BG_AV_NODES_STORMPIKE_GRAVE         = 1,                // 雷矛墓地
    BG_AV_NODES_STONEHEART_GRAVE        = 2,                // 石炉墓地

    BG_AV_NODES_SNOWFALL_GRAVE          = 3,                // 落雪墓地

    BG_AV_NODES_ICEBLOOD_GRAVE          = 4,                // 冰血墓地
    BG_AV_NODES_FROSTWOLF_GRAVE         = 5,                // 霜狼墓地
    BG_AV_NODES_FROSTWOLF_HUT           = 6,                // 霜狼急救站

    BG_AV_NODES_DUNBALDAR_SOUTH         = 7,                // 丹巴达尔南碉堡
    BG_AV_NODES_DUNBALDAR_NORTH         = 8,                // 丹巴达尔北碉堡
    BG_AV_NODES_ICEWING_BUNKER          = 9,                // 冰翼碉堡
    BG_AV_NODES_STONEHEART_BUNKER       = 10,               // 石炉碉堡

    BG_AV_NODES_ICEBLOOD_TOWER          = 11,               // 冰血哨塔
    BG_AV_NODES_TOWER_POINT             = 12,               // 哨塔高地
    BG_AV_NODES_FROSTWOLF_ETOWER        = 13,               // 霜狼东哨塔
    BG_AV_NODES_FROSTWOLF_WTOWER        = 14,               // 霜狼西哨塔
};

// for nodeevents we will use event1=node
// event2 is related to BG_AV_States
// 0 = alliance assaulted
// 1 = alliance control
// 2 = horde assaulted
// 3 = horde control
// 4 = neutral assaulted
// 5 = neutral control
// (据点事件状态值：对应 SpawnEvent 的 event2)
enum AVNodeStatus
{
    BG_AV_NODE_STATUS_ALLY_CONTESTED    = 0,                // values to be used for event2 in SpawnEventfunction
    BG_AV_NODE_STATUS_ALLY_OCCUPIED     = 1,
    BG_AV_NODE_STATUS_HORDE_CONTESTED   = 2,
    BG_AV_NODE_STATUS_HORDE_OCCUPIED    = 3,
    BG_AV_NODE_STATUS_NEUTRAL_CONTESTED = 4,
    BG_AV_NODE_STATUS_NEUTRAL_OCCUPIED  = 5,
};

// virtual ids for the north (0) and south (1) mines (南北两座矿洞的逻辑编号)
enum AVMineIds
{
    BG_AV_IRONDEEP_MINE_ID              = 0,                // 深铁矿洞（北部）
    BG_AV_COLDTOOTH_MINE_ID             = 1,                // 冷齿矿洞（南部）
};

// graves have special creatures - their defenders can be in 4 different states
// through some quests with armor scraps
// so i use event1=BG_AV_NODES_MAX+node (15-21)
// and event2=type
// (奥山各类 NPC 与矿洞/首领/将领刷新的事件 ID)
enum AVEventTypes
{
    BG_AV_MINE_BOSSES                   = 46,               // + mineid will be exact event (矿洞首领基址)
    BG_AV_MINE_BOSSES_NORTH             = 46,
    BG_AV_MINE_BOSSES_SOUTH             = 47,
    BG_AV_CAPTAIN_A                     = 48,
    BG_AV_CAPTAIN_H                     = 49,
    BG_AV_MINE_EVENT                    = 50,               // + mineid will be exact event (矿洞补给/NPC 归属事件基址)
    BG_AV_MINE_EVENT_NORTH              = 50,
    BG_AV_MINE_EVENT_SOUTH              = 51,

    BG_AV_MARSHAL_A_SOUTH               = 52,
    BG_AV_MARSHAL_A_NORTH               = 53,
    BG_AV_MARSHAL_A_ICE                 = 54,
    BG_AV_MARSHAL_A_STONE               = 55,
    BG_AV_MARSHAL_H_ICE                 = 56,
    BG_AV_MARSHAL_H_TOWER               = 57,
    BG_AV_MARSHAL_H_ETOWER              = 58,
    BG_AV_MARSHAL_H_WTOWER              = 59,

    BG_AV_HERALD                        = 60,
    BG_AV_BOSS_A                        = 61,
    BG_AV_BOSS_H                        = 62,
    BG_AV_NODE_CAPTAIN_DEAD_A           = 63,
    BG_AV_NODE_CAPTAIN_DEAD_H           = 64,
};

// 奥特兰克山谷墓地 ID 与主区域 ID
enum AVGraveyards
{
    BG_AV_GRAVE_STORM_AID               = 751,
    BG_AV_GRAVE_STORM_GRAVE             = 689,
    BG_AV_GRAVE_STONE_GRAVE             = 729,

    BG_AV_GRAVE_SNOWFALL                = 169,

    BG_AV_GRAVE_ICE_GRAVE               = 749,
    BG_AV_GRAVE_FROSTWOLF               = 690,
    BG_AV_GRAVE_FROST_HUT               = 750,

    BG_AV_GRAVE_MAIN_ALLIANCE           = 611,
    BG_AV_GRAVE_MAIN_HORDE              = 610,

    BG_AV_ZONE_MAIN                     = 2597,
};

// 据点内部状态枚举（突袭争夺中 vs 已完全控制）
enum AVStates
{
    POINT_ASSAULTED                     = 0,
    POINT_CONTROLLED                    = 1
};

// 奥特兰克山谷世界状态 ID
enum AVWorldStates
{
    // main world states (双方援军兵力计数及落雪墓地状态)
    BG_AV_STATE_SCORE_A                 = 3127,
    BG_AV_STATE_SCORE_H                 = 3128,
    BG_AV_STATE_SCORE_SHOW_H            = 3133,
    BG_AV_STATE_SCORE_SHOW_A            = 3134,

    BG_AV_STATE_GY_SNOWFALL_N           = 1966,
    BG_AV_STATE_GY_SNOWFALL_A           = 1341,
    BG_AV_STATE_GY_SNOWFALL_A_GREY      = 1343,
    BG_AV_STATE_GY_SNOWFALL_H           = 1342,
    BG_AV_STATE_GY_SNOWFALL_H_GREY      = 1344,

    // mine world states (南北矿洞控制归属状态)
    BG_AV_STATE_IRONDEEP_MINE_A         = 1358,
    BG_AV_STATE_IRONDEEP_MINE_H         = 1359,
    BG_AV_STATE_IRONDEEP_MINE_N         = 1360,

    BG_AV_STATE_COLDTOOTH_MINE_A        = 1355,
    BG_AV_STATE_COLDTOOTH_MINE_H        = 1356,
    BG_AV_STATE_COLDTOOTH_MINE_N        = 1357,

    // graveyard world states (各墓地地图图标状态)
    BG_AV_STATE_GY_DUN_BALDAR_A_GREY    = 1326,
    BG_AV_STATE_GY_DUN_BALDAR_A         = 1325,
    BG_AV_STATE_GY_DUN_BALDAR_H_GREY    = 1328,
    BG_AV_STATE_GY_DUN_BALDAR_H         = 1327,

    BG_AV_STATE_GY_STORMPIKE_A_GREY     = 1335,
    BG_AV_STATE_GY_STORMPIKE_A          = 1333,
    BG_AV_STATE_GY_STORMPIKE_H_GREY     = 1336,
    BG_AV_STATE_GY_STORMPIKE_H          = 1334,

    BG_AV_STATE_GY_STONEHEARTH_A_GREY   = 1304,
    BG_AV_STATE_GY_STONEHEARTH_A        = 1302,
    BG_AV_STATE_GY_STONEHEARTH_H_GREY   = 1303,
    BG_AV_STATE_GY_STONEHEARTH_H        = 1301,

    BG_AV_STATE_GY_ICEBLOOD_A_GREY      = 1348,
    BG_AV_STATE_GY_ICEBLOOD_A           = 1346,
    BG_AV_STATE_GY_ICEBLOOD_H_GREY      = 1349,
    BG_AV_STATE_GY_ICEBLOOD_H           = 1347,

    BG_AV_STATE_GY_FROSTWOLF_A_GREY     = 1339,
    BG_AV_STATE_GY_FROSTWOLF_A          = 1337,
    BG_AV_STATE_GY_FROSTWOLF_H_GREY     = 1340,
    BG_AV_STATE_GY_FROSTWOLF_H          = 1338,

    BG_AV_STATE_GY_FROST_KEEP_A_GREY    = 1331,
    BG_AV_STATE_GY_FROST_KEEP_A         = 1329,
    BG_AV_STATE_GY_FROST_KEEP_H_GREY    = 1332,
    BG_AV_STATE_GY_FROST_KEEP_H         = 1330,

    // tower world states (各碉堡与哨塔地图图标状态)
    BG_AV_STATE_SOUTH_BUNKER_A_GREY     = 1375,
    BG_AV_STATE_SOUTH_BUNKER_A          = 1361,
    BG_AV_STATE_SOUTH_BUNKER_H_GREY     = 1378,
    BG_AV_STATE_SOUTH_BUNKER_H          = 1370,

    BG_AV_STATE_NORTH_BUNKER_A_GREY     = 1374,
    BG_AV_STATE_NORTH_BUNKER_A          = 1362,
    BG_AV_STATE_NORTH_BUNKER_H_GREY     = 1379,
    BG_AV_STATE_NORTH_BUNKER_H          = 1371,

    BG_AV_STATE_ICEWING_BUNKER_A_GREY   = 1376,
    BG_AV_STATE_ICEWING_BUNKER_A        = 1363,
    BG_AV_STATE_ICEWING_BUNKER_H_GREY   = 1380,
    BG_AV_STATE_ICEWING_BUNKER_H        = 1372,

    BG_AV_STATE_STONE_BUNKER_A_GREY     = 1377,
    BG_AV_STATE_STONE_BUNKER_A          = 1364,
    BG_AV_STATE_STONE_BUNKER_H_GREY     = 1381,
    BG_AV_STATE_STONE_BUNKER_H          = 1373,

    BG_AV_STATE_ICEBLOOD_TOWER_A_GREY   = 1390,
    BG_AV_STATE_ICEBLOOD_TOWER_A        = 1368,
    BG_AV_STATE_ICEBLOOD_TOWER_H_GREY   = 1395,
    BG_AV_STATE_ICEBLOOD_TOWER_H        = 1385,

    BG_AV_STATE_TOWER_POINT_A_GREY      = 1389,
    BG_AV_STATE_TOWER_POINT_A           = 1367,
    BG_AV_STATE_TOWER_POINT_H_GREY      = 1394,
    BG_AV_STATE_TOWER_POINT_H           = 1384,

    BG_AV_STATE_FROSTWOLF_EAST_A_GREY   = 1388,
    BG_AV_STATE_FROSTWOLF_EAST_A        = 1366,
    BG_AV_STATE_FROSTWOLF_EAST_H_GREY   = 1393,
    BG_AV_STATE_FROSTWOLF_EAST_H        = 1383,

    BG_AV_STATE_FROSTWOLF_WEST_A_GREY   = 1387,
    BG_AV_STATE_FROSTWOLF_WEST_A        = 1365,
    BG_AV_STATE_FROSTWOLF_WEST_H_GREY   = 1392,
    BG_AV_STATE_FROSTWOLF_WEST_H        = 1382,
};

// 奥特兰克山谷可重复上交与支援召唤任务 ID
enum AVQuestIds
{
    BG_AV_QUEST_CAPTURE_MINE_A          = 7122,
    BG_AV_QUEST_CAPTURE_MINE_H          = 7124,

    BG_AV_QUEST_ARMOR_SCRAPS_A          = 7223,             // first quest (联盟首次护甲碎片任务)
    BG_AV_QUEST_ARMOR_SCRAPS_A_REPEAT   = 6781,             // repeatable (可重复上交护甲碎片)

    BG_AV_QUEST_ARMOR_SCRAPS_H          = 7224,             // 部落首次护甲碎片任务
    BG_AV_QUEST_ARMOR_SCRAPS_H_REPEAT   = 6741,

    BG_AV_QUEST_SLIDORES_FLEET          = 6942,             // alliance version (联盟空军指挥官物资任务)
    BG_AV_QUEST_VIPORES_FLEET           = 6941,
    BG_AV_QUEST_ICHMANS_FLEET           = 6943,

    BG_AV_QUEST_GUSES_FLEET             = 6825,             // horde version (部落空军指挥官物资任务)
    BG_AV_QUEST_JEZTORS_FLEET           = 6826,
    BG_AV_QUEST_MULVERICKS_FLEET        = 6827,

    BG_AV_QUEST_CRYSTAL_CLUSTER         = 7386,             // alliance; 5 cristal/blood (联盟上交风暴水晶)
    BG_AV_QUEST_GALLON_OF_BLOOD         = 7385,             // horde (部落上交鲜血)

    BG_AV_QUEST_IVUS_FOREST_LORD        = 6881,             // alliance (召唤森林之王伊弗斯)
    BG_AV_QUEST_LOKHOLAR_ICE_LORD       = 6801,             // horde (召唤冰雪之王洛克霍拉)

    BG_AV_QUEST_IRONDEEP_SUPPLIES_A     = 5892,             // the mine near start location of team (己方矿洞补给任务)
    BG_AV_QUEST_COLDTOOTH_SUPPLIES_H    = 5893,

    BG_AV_QUEST_COLDTOOTH_SUPPLIES_A    = 6982,             // opposite faction mine (敌方矿洞补给任务)
    BG_AV_QUEST_IRONDEEP_SUPPLIES_H     = 6985,

    BG_AV_QUEST_RAM_RIDING              = 7026,             // alliance (驯服山羊/战狼坐骑任务)
    BG_AV_QUEST_RAM_HIDE                = 7002,             // horde

    BG_AV_QUEST_EMPTY_STABLES_A         = 7027,
    BG_AV_QUEST_EMPTY_STABLES_H         = 7001,
};

const uint32 avGraveyardEntries[BG_AV_MAX_GRAVEYARDS] =
{
    BG_AV_GRAVE_STORM_AID,
    BG_AV_GRAVE_STORM_GRAVE,
    BG_AV_GRAVE_STONE_GRAVE,
    BG_AV_GRAVE_SNOWFALL,
    BG_AV_GRAVE_ICE_GRAVE,
    BG_AV_GRAVE_FROSTWOLF,
    BG_AV_GRAVE_FROST_HUT,
    BG_AV_GRAVE_MAIN_ALLIANCE,
    BG_AV_GRAVE_MAIN_HORDE
};

// 奥特兰克山谷传令官与矿洞首领喊话文本 ID
enum AVEndYells
{
    YELL_STORMPIKE_DEAD = 7297,
    YELL_FROSTWOLF_DEAD = 7296,
    YELL_ALLIANCE_WINS  = 7335,
    YELL_HORDE_WINS     = 7336,

    YELL_SNIVVLE_MINE   = 10025,
    YELL_MORLOCH_MINE   = 10024,
};

// alliance_control horde_control neutral_control (南北矿洞三态世界状态映射表)
const uint32 avMineWorldStates[BG_AV_MAX_MINES][BG_AV_TEAMS_COUNT] =
{
    {BG_AV_STATE_IRONDEEP_MINE_A, BG_AV_STATE_IRONDEEP_MINE_H, BG_AV_STATE_IRONDEEP_MINE_N},
    {BG_AV_STATE_COLDTOOTH_MINE_A, BG_AV_STATE_COLDTOOTH_MINE_H, BG_AV_STATE_COLDTOOTH_MINE_N}
};

// 单据点四态世界状态配置结构（联盟控制、联盟争夺中、部落控制、部落争夺中）
struct AlteracNodeStates
{
    uint32 worldStateAlly, worldStateAllyGrey, worldStateHorde, worldStateHordeGrey;
};

// alliance; alliance_grey; horde; horde_grey (15 个据点的世界状态配置表)
const AlteracNodeStates avNodeWorldStates[BG_AV_MAX_NODES] =
{
    // Alliance graveyards (联盟侧墓地)
    {BG_AV_STATE_GY_DUN_BALDAR_A,  BG_AV_STATE_GY_DUN_BALDAR_A_GREY,  BG_AV_STATE_GY_DUN_BALDAR_H,  BG_AV_STATE_GY_DUN_BALDAR_H_GREY},
    {BG_AV_STATE_GY_STORMPIKE_A,   BG_AV_STATE_GY_STORMPIKE_A_GREY,   BG_AV_STATE_GY_STORMPIKE_H,   BG_AV_STATE_GY_STORMPIKE_H_GREY},
    {BG_AV_STATE_GY_STONEHEARTH_A, BG_AV_STATE_GY_STONEHEARTH_A_GREY, BG_AV_STATE_GY_STONEHEARTH_H, BG_AV_STATE_GY_STONEHEARTH_H_GREY},

    // Center graveyard (中立落雪墓地)
    {BG_AV_STATE_GY_SNOWFALL_A,    BG_AV_STATE_GY_SNOWFALL_A_GREY,    BG_AV_STATE_GY_SNOWFALL_H,    BG_AV_STATE_GY_SNOWFALL_H_GREY},

    // Horde graveyards (部落侧墓地)
    {BG_AV_STATE_GY_ICEBLOOD_A,    BG_AV_STATE_GY_ICEBLOOD_A_GREY,    BG_AV_STATE_GY_ICEBLOOD_H,    BG_AV_STATE_GY_ICEBLOOD_H_GREY},
    {BG_AV_STATE_GY_FROSTWOLF_A,   BG_AV_STATE_GY_FROSTWOLF_A_GREY,   BG_AV_STATE_GY_FROSTWOLF_H,   BG_AV_STATE_GY_FROSTWOLF_H_GREY},
    {BG_AV_STATE_GY_FROST_KEEP_A,  BG_AV_STATE_GY_FROST_KEEP_A_GREY,  BG_AV_STATE_GY_FROST_KEEP_H,  BG_AV_STATE_GY_FROST_KEEP_H_GREY},

    // Alliance towers (联盟侧碉堡)
    {BG_AV_STATE_SOUTH_BUNKER_A,   BG_AV_STATE_SOUTH_BUNKER_A_GREY,   BG_AV_STATE_SOUTH_BUNKER_H,   BG_AV_STATE_SOUTH_BUNKER_H_GREY},
    {BG_AV_STATE_NORTH_BUNKER_A,   BG_AV_STATE_NORTH_BUNKER_A_GREY,   BG_AV_STATE_NORTH_BUNKER_H,   BG_AV_STATE_NORTH_BUNKER_H_GREY},
    {BG_AV_STATE_ICEWING_BUNKER_A, BG_AV_STATE_ICEWING_BUNKER_A_GREY, BG_AV_STATE_ICEWING_BUNKER_H, BG_AV_STATE_ICEWING_BUNKER_H_GREY},
    {BG_AV_STATE_STONE_BUNKER_A,   BG_AV_STATE_STONE_BUNKER_A_GREY,   BG_AV_STATE_STONE_BUNKER_H,   BG_AV_STATE_STONE_BUNKER_H_GREY},

    // Horde towers (部落侧哨塔)
    {BG_AV_STATE_ICEBLOOD_TOWER_A, BG_AV_STATE_ICEBLOOD_TOWER_A_GREY, BG_AV_STATE_ICEBLOOD_TOWER_H, BG_AV_STATE_ICEBLOOD_TOWER_H_GREY},
    {BG_AV_STATE_TOWER_POINT_A,    BG_AV_STATE_TOWER_POINT_A_GREY,    BG_AV_STATE_TOWER_POINT_H,    BG_AV_STATE_TOWER_POINT_H_GREY},
    {BG_AV_STATE_FROSTWOLF_EAST_A, BG_AV_STATE_FROSTWOLF_EAST_A_GREY, BG_AV_STATE_FROSTWOLF_EAST_H, BG_AV_STATE_FROSTWOLF_EAST_H_GREY},
    {BG_AV_STATE_FROSTWOLF_WEST_A, BG_AV_STATE_FROSTWOLF_WEST_A_GREY, BG_AV_STATE_FROSTWOLF_WEST_H, BG_AV_STATE_FROSTWOLF_WEST_H_GREY},
};

// *** Battleground factions *** (双方声望阵营 ID) //
const uint32 avTeamFactions[PVP_TEAM_COUNT] = { BG_AV_FACTION_ID_STORMPIKE, BG_AV_FACTION_ID_FROSTWOLF };

// *** Battleground factions *** (双方中尉生物条目 ID) //
const uint32 avTeamCaptains[PVP_TEAM_COUNT] = { BG_AV_NPC_CAPTAIN_STONEHEARTH, BG_AV_NPC_CAPTAIN_GALVANGAR };

// 据点初始默认值结构
struct AlteracNodeData
{
    uint32 defaultWorldState;
    PvpTeamIndex initialOwner;
    uint32 graveyardId;
};

const AlteracNodeData avNodeDefaults[BG_AV_MAX_NODES] =
{
    {BG_AV_STATE_GY_DUN_BALDAR_A,  TEAM_INDEX_ALLIANCE, BG_AV_GRAVE_STORM_AID},
    {BG_AV_STATE_GY_STORMPIKE_A,   TEAM_INDEX_ALLIANCE, BG_AV_GRAVE_STORM_GRAVE},
    {BG_AV_STATE_GY_STONEHEARTH_A, TEAM_INDEX_ALLIANCE, BG_AV_GRAVE_STONE_GRAVE},

    {BG_AV_STATE_GY_SNOWFALL_N,    TEAM_INDEX_NEUTRAL, BG_AV_GRAVE_SNOWFALL},

    {BG_AV_STATE_GY_ICEBLOOD_H,    TEAM_INDEX_HORDE, BG_AV_GRAVE_ICE_GRAVE},
    {BG_AV_STATE_GY_FROSTWOLF_H,   TEAM_INDEX_HORDE, BG_AV_GRAVE_FROSTWOLF},
    {BG_AV_STATE_GY_FROST_KEEP_H,  TEAM_INDEX_HORDE, BG_AV_GRAVE_FROST_HUT},

    {BG_AV_STATE_SOUTH_BUNKER_A,   TEAM_INDEX_ALLIANCE, 0},
    {BG_AV_STATE_NORTH_BUNKER_A,   TEAM_INDEX_ALLIANCE, 0},
    {BG_AV_STATE_ICEWING_BUNKER_A, TEAM_INDEX_ALLIANCE, 0},
    {BG_AV_STATE_STONE_BUNKER_A,   TEAM_INDEX_ALLIANCE, 0},

    {BG_AV_STATE_ICEBLOOD_TOWER_H, TEAM_INDEX_HORDE, 0},
    {BG_AV_STATE_TOWER_POINT_H,    TEAM_INDEX_HORDE, 0},
    {BG_AV_STATE_FROSTWOLF_EAST_H, TEAM_INDEX_HORDE, 0},
    {BG_AV_STATE_FROSTWOLF_WEST_H, TEAM_INDEX_HORDE, 0},
};

// 运行时单个据点状态数据
struct AVNodeInfo
{
    PvpTeamIndex totalOwner;
    PvpTeamIndex owner;
    PvpTeamIndex prevOwner;

    AVStates state;
    AVStates prevState;

    uint32 worldState;
    uint32 timer;
    uint32 graveyardId;
};

// 奥特兰克山谷玩家战绩结构（扩展墓地/哨塔突袭守卫及次要目标贡献）
class BattleGroundAVScore : public BattleGroundScore
{
    public:
        BattleGroundAVScore() : graveyardsAssaulted(0), graveyardsDefended(0), towersAssaulted(0), towersDefended(0), secondaryObjectives(0) {};

        uint32 GetAttr1() const override { return graveyardsAssaulted; }
        uint32 GetAttr2() const override { return graveyardsDefended; }
        uint32 GetAttr3() const override { return towersAssaulted; }
        uint32 GetAttr4() const override { return towersDefended; }
        uint32 GetAttr5() const override { return secondaryObjectives; }

        uint32 graveyardsAssaulted;
        uint32 graveyardsDefended;
        uint32 towersAssaulted;
        uint32 towersDefended;
        uint32 secondaryObjectives;
};

// 奥特兰克山谷（Alterac Valley）战场控制类：管理援军兵力系统、7 墓地 8 哨塔争夺、双矿洞控制、首领/将领联动及战场升级任务
class BattleGroundAV : public BattleGround
{
        friend class BattleGroundMgr;

    public:
        BattleGroundAV();
        void Reset() override;
        void Update(uint32 diff) override;

        // Main battleground functions (主流程接口)
        void AddPlayer(Player* player) override;
        void StartingEventOpenDoors() override;
        void EndBattleGround(Team winner) override;

        // General functions (积分更新与人数不足提前判胜)
        void UpdatePlayerScore(Player* source, uint32 type, uint32 value) override;
        Team GetPrematureWinner() override;

        // Battleground event handlers (生物/对象创建、区域触发器、开旗点击、玩家与 NPC 击杀)
        void HandleCreatureCreate(Creature* /*creature*/) override;
        void HandleGameObjectCreate(GameObject* /*go*/) override;
        bool HandleAreaTrigger(Player* source, uint32 trigger) override;
        void HandlePlayerClickedOnFlag(Player* source, GameObject* target_obj) override;
        void HandleKillPlayer(Player* player, Player* killer) override;
        void HandleKillUnit(Creature* creature, Player* killer) override;

        // Conditions (校验矿洞补给任务或部队升级条件是否满足)
        bool IsConditionFulfilled(Player const* source, uint32 conditionId, WorldObject const* conditionSource, uint32 conditionSourceType) override;

    private:
        // Score and quest functions (兵力点数扣除、任务进度上交与矿洞任务判定)
        void UpdateScore(PvpTeamIndex teamIdx, int32 points);
        void HandleQuestComplete(uint32 questid, Player* player);
        bool CanPlayerDoMineQuest(uint32 gameobjectEntry, Team team);

        // Battleground node functions (据点突袭、防守、占领与烧毁摧毁流程)
        void ProcessPlayerAssaultsPoint(Player* player, AVNodeIds node);
        void ProcessPlayerDefendsPoint(Player* player, AVNodeIds node);
        void ProcessPlayerDestroyedPoint(AVNodeIds node);

        void InitializeNode(AVNodeIds node);
        void AssaultNode(AVNodeIds node, PvpTeamIndex teamIdx);
        void DestroyNode(AVNodeIds node);
        void DefendNode(AVNodeIds node, PvpTeamIndex teamIdx);

        void PopulateNode(AVNodeIds node); // 根据据点当前状态刷出对应生物与旗帜事件
        int32 GetNodeMessageId(AVNodeIds node) const;

        // Mine related functions (变更南北矿洞控制阵营)
        void ChangeMineOwner(AVMineIds mineId, PvpTeamIndex newOwnerTeamIdx);

        // World state helpers (计算阵营相关世界状态索引并更新据点状态)
        uint8 GetWorldStateType(uint8 state, PvpTeamIndex teamIdx) const { return static_cast<uint8>(teamIdx) * static_cast<uint8>(BG_AV_MAX_STATES) + state; }
        void UpdateNodeWorldState(AVNodeIds node, uint32 newState);

        // Herald announcements (由传令官 NPC 向全场播报据点或矿洞战报)
        void DoSendYellToTeam(PvpTeamIndex teamIdx, int32 textId, uint8 nodeId, bool useNode = true, int32 mineLangId = 0);

        uint32 m_teamQuestStatus[PVP_TEAM_COUNT][9];       // [x][y] x=team y=questcounter (双方部队升级/召唤任务累计上交进度)

        AVNodeInfo m_nodes[BG_AV_MAX_NODES];

        // mine variables (双矿洞归属及资源周期/归位计时器)
        PvpTeamIndex m_mineOwner[BG_AV_MAX_MINES];
        uint32 m_mineTimer[BG_AV_MAX_MINES];
        uint32 m_mineReclaimTimer[BG_AV_MAX_MINES];
        uint32 m_mineYellTimer;

        uint8 m_homeTowersControlled[PVP_TEAM_COUNT];
        uint8 m_enemyTowersDestroyed[PVP_TEAM_COUNT];

        bool m_wasInformedNearLose[PVP_TEAM_COUNT];

        uint32 m_honorMapComplete;
        uint32 m_repTowerDestruction;
        uint32 m_repCaptain;
        uint32 m_repBoss;
        uint32 m_repOwnedGrave;
        uint32 m_repOwnedMine;
        uint32 m_repSurviveCaptain;
        uint32 m_repSurviveTower;
};

#endif
