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

#ifndef _BG_DEFINES_H
#define _BG_DEFINES_H

#include "Common.h"

#define BATTLEGROUND_ARENA_POINT_DISTRIBUTION_DAY 86400     // seconds in a day (每日秒数，用于竞技场点数自动发放周期)
#define COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME 10            // 平均等待时间采样人数上限

// 战场排队队伍分类（区分预组队/散人及联盟/部落）
enum BattleGroundQueueGroupTypes
{
    BG_QUEUE_PREMADE_ALLIANCE   = 0,                        // 联盟预组队或排位竞技场队
    BG_QUEUE_PREMADE_HORDE      = 1,                        // 部落预组队或排位竞技场队
    BG_QUEUE_NORMAL_ALLIANCE    = 2,                        // 联盟散人或练习赛队列
    BG_QUEUE_NORMAL_HORDE       = 3                         // 部落散人或练习赛队列
};

#define BG_QUEUE_GROUP_TYPES_COUNT 4

// 团队加入战场队列的结果状态码
enum BattleGroundGroupJoinStatus
{
    BG_GROUP_JOIN_STATUS_TEAM_LEFT_QUEUE        = -7,
    BG_GROUP_JOIN_STATUS_QUEUED_FOR_RATED       = -6,
    BG_GROUP_JOIN_STATUS_CANNOT_QUEUE_FOR_RATED = -5,
    BG_GROUP_JOIN_STATUS_TOO_MANY_QUEUES        = -4,
    BG_GROUP_JOIN_STATUS_NOT_IN_TEAM            = -3,
    BG_GROUP_JOIN_STATUS_DESERTERS              = -2,
    BG_GROUP_JOIN_STATUS_NOT_ELIGIBLE           = -1,
    BG_GROUP_JOIN_STATUS_SUCCESS                = 0,
};

// indexes of BattlemasterList.dbc (对应 BattlemasterList.dbc 的战场类型索引)
enum BattleGroundTypeId
{
    BATTLEGROUND_TYPE_NONE = 0,
    BATTLEGROUND_AV = 1,                                    // 奥特兰克山谷
    BATTLEGROUND_WS = 2,                                    // 战歌峡谷
    BATTLEGROUND_AB = 3,                                    // 阿拉希盆地
    BATTLEGROUND_NA = 4,                                    // 纳格兰竞技场
    BATTLEGROUND_BE = 5,                                    // 刀锋山竞技场
    BATTLEGROUND_AA = 6,                         // all arenas (所有竞技场随机入口)
    BATTLEGROUND_EY = 7,                                    // 风暴之眼
    BATTLEGROUND_RL = 8                                     // 洛丹伦废墟竞技场
};
#define MAX_BATTLEGROUND_TYPE_ID 9

// 竞技场赛制规模枚举
enum ArenaType
{
    ARENA_TYPE_NONE = 0,                            // used for mark non-arenas or problematic cases (非竞技场或无效类型)
    ARENA_TYPE_2v2 = 2,
    ARENA_TYPE_3v3 = 3,
    ARENA_TYPE_5v5 = 5
};

// 校验竞技场规模类型是否合法
inline bool IsArenaTypeValid(ArenaType type) { return type == ARENA_TYPE_2v2 || type == ARENA_TYPE_3v3 || type == ARENA_TYPE_5v5; }

#endif