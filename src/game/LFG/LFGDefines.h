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

#ifndef _LFG_DEFINES_H
#define _LFG_DEFINES_H

#include "Common.h"

#define MAX_LOOKING_FOR_GROUP_SLOT 3                        // 寻找队伍（LFG）界面最多同时选择的地下城/活动槽位数

/// 寻求组队活动分类类型
enum LfgType : uint32
{
    LFG_TYPE_NONE           = 0,                            // 无
    LFG_TYPE_DUNGEON        = 1,                            // 普通地下城
    LFG_TYPE_RAID           = 2,                            // 团队副本
    LFG_TYPE_QUEST          = 3,                            // 任务（精英/组队任务）
    LFG_TYPE_ZONE           = 4,                            // 区域活动
    LFG_TYPE_HEROIC_DUNGEON = 5                             // 英雄难度地下城
};

/// 集合石/自动组队加入失败原因
enum class MeetingstoneFailedStatus : uint8
{
    MEETINGSTONE_FAIL_NONE                                  = 0, // custom, not to be sent (成功/不发送错误)
    MEETINGSTONE_FAIL_PARTYLEADER                           = 1, // 非队长无权操作
    MEETINGSTONE_FAIL_FULL_GROUP                            = 2, // 队伍已满
    MEETINGSTONE_FAIL_RAID_GROUP                            = 3, // 团队模式下不可使用
};

/// 集合石/队列状态变更枚举
enum class MeetingstoneSetqueue : uint8
{
    LEFT_QUEUE          = 0,                                // 已离开队列
    IN_QUEUE            = 1,                                // 已加入队列
    UNK                 = 2,
    OTHER_MEMBER_LEFT   = 3,                                // 其他成员离开队列
    KICKED_FROM_QUEUE   = 4,                                // 被移出队列
    JOINED_GROUP        = 5,                                // 匹配成功并加入队伍
};

/// 会话层缓存的 LFG/LFM 玩家状态信息（与异步 LFGQueue 线程交互）
struct LfgPlayerInfo
{
    std::string comment;                                    // 寻求组队附带说明文本
    bool autojoin = false;                                  // 是否开启自动加入匹配队伍
    bool autofill = false;                                  // 是否开启自动填充招募队员
    bool queued = false; // cached async information        // 异步缓存的当前排队状态标记
};

#endif