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

#ifndef _LFG_QUEUE_H
#define _LFG_QUEUE_H

#include "LFG/LFGDefines.h"
#include "Entities/ObjectGuid.h"
#include "Globals/ObjectMgr.h"

/// LFG 队伍中除队长外的成员信息快照
struct LFGGroupQueueInfo
{
    ObjectGuid partyMember;                                 // 成员 GUID
    uint32 level;                                           // 成员等级

    LFGGroupQueueInfo(ObjectGuid partyMember, uint32 level) : partyMember(partyMember), level(level) {}
};

/// 玩家或队伍队长在 LFG/LFM 队列中的排队状态条目
struct LFGPlayerQueueInfo
{
    /// 单个 LFG/LFM 目标槽位（目标 ID 与活动类型）
    struct Slot
    {
        bool empty() const { return (!type || !entry); }
        void clear() { entry = 0; }
        bool set(uint16 _entry, uint16 _type) { entry = _entry; type = _type; return !empty(); }
        bool is(uint16 _entry, uint16 _type) const { return entry == _entry && type == _type; }
        /// 仅普通/英雄地下城支持集合石自动匹配加入与自动补员
        bool isAuto() const { return entry && (type == LFG_TYPE_DUNGEON || type == LFG_TYPE_HEROIC_DUNGEON); }

        uint16 entry = 0;                                   // LFGDungeons.dbc 条目或区域/任务 ID
        uint16 type = LFG_TYPE_DUNGEON;                     // LfgType 枚举值
    };

    inline void clear()
    {
        more.clear();
        for (auto& slot : group)
            slot.clear();
    }
    inline bool isAutoFill() const { return more.isAuto(); }
    inline bool isAutoJoin() const
    {
        for (auto& slot : group)
            if (slot.isAuto())
                return true;
        return false;
    }
    inline bool isEmpty() const { return (!isLFM() && !isLFG()); }
    inline bool isLFG() const
    {
        for (auto& slot : group)
            if (!slot.empty())
                return true;
        return false;
    }
    inline bool isLFG(uint32 entry, uint32 type, bool autoOnly) const
    {
        for (auto& slot : group)
            if (slot.is(uint16(entry), uint16(type)) && (!autoOnly || slot.isAuto()))
                return true;
        return false;
    }
    inline bool isLFG(LFGPlayerQueueInfo const& info, bool autoOnly) const { return isLFG(uint16(info.more.entry), uint16(info.more.type), autoOnly); }
    inline bool isLFM() const { return !more.empty(); }
    inline bool isLFM(uint32 entry, uint32 type) const { return more.is(uint16(entry), uint16(type)); }

    // bool queued = false;
    Slot group[MAX_LOOKING_FOR_GROUP_SLOT];                 // 寻求组队（LFG）最多 3 个候选槽位
    Slot more;                                              // 寻求更多队员（LFM）招募目标槽位
    std::string comment;                                    // 招募/求组备注
    Team team = TEAM_NONE;                                  // 所属阵营（联盟/部落）
    bool isLeader = true;                                   // 是否为队长或单人求组者
    bool full = false;                                      // 队伍是否已满
    uint32 level = 0;                                       // 等级
    uint32 zoneId = 0;                                      // 所在区域 ID
    bool status = false;                                    // 当前自动匹配队列激活状态
    ObjectGuid leaderGuid;                                  // 队列条目主体（单人或队长）GUID

    bool autoFill = false;                                  // 是否启用自动补员
    bool autoJoin = false;                                  // 是否启用自动入队

    bool pendingTransfer = false;                           // 是否处于匹配成功待确认加入状态
    ObjectGuid pendingLeaderGuid;                           // 待加入的目标队长 GUID
    uint32 pendingEntry = 0;                                // 匹配成功的副本 ID

    std::vector<LFGGroupQueueInfo> members;                 // 队伍当前其他成员列表
    std::vector<ObjectGuid> pendingMembers;                 // 正在等待确认加入的候选玩家列表
};

/// 异步寻求组队（LFG/LFM）匹配队列管理器：运行在独立工作线程，通过 Messager 与主世界线程通信
class LFGQueue
{
    public:
        /// 队列工作循环：周期性消费 Messager 消息并处理匹配
        void Update();

        void SetComment(ObjectGuid playerGuid, std::string const& comment);
        void SetAutoFill(ObjectGuid playerGuid, bool state);
        void SetAutoJoin(ObjectGuid playerGuid, bool state);

        /// 开始/停止寻找更多队员（LFM）
        void StartLookingForMore(ObjectGuid playerGuid, LFGPlayerQueueInfo info);
        void StopLookingForMore(ObjectGuid playerGuid);

        /// 开始/停止寻求组队（LFG）及修改指定槽位数据
        void StartLookingForGroup(LFGPlayerQueueInfo info, ObjectGuid invokerPlayer);
        void StopLookingForGroup(ObjectGuid leaderGuid, ObjectGuid playerGuid);
        void SetLfgSlot(ObjectGuid leaderGuid, uint32 slot, uint16 entry, uint16 type);
        void SetLfmData(ObjectGuid leaderGuid, uint16 entry, uint16 type);

        /// 尝试为启用自动加入的玩家匹配 LFM 队伍，或为启用自动补员的队伍匹配 LFG 玩家
        void TryJoin(ObjectGuid playerGuid, bool initial);
        void TryFill(ObjectGuid leaderGuid, bool initial);

        /// 发起待确认组队邀请并通知双方客户端
        bool AddMember(LFGPlayerQueueInfo& info, LFGPlayerQueueInfo& playerInfo, uint32 entry);

        /// 处理玩家确认/拒绝匹配入队，在世界线程创建或加入 Group 并同步队列状态
        void HandlePendingJoin(ObjectGuid playerGuid);
        void RemovePendingJoin(ObjectGuid leaderGuid, ObjectGuid playerGuid);
        void PendingJoinSuccess(ObjectGuid leaderGuid, ObjectGuid playerGuid, bool full);

        /// 异步回传 LFG/LFM 状态更新与查询列表数据包给世界线程发送
        void SendLFGUpdate(ObjectGuid leaderGuid, ObjectGuid playerGuid) const;
        void SendLFGUpdateLFG(ObjectGuid playerGuid) const;
        void SendLFGUpdateLFM(ObjectGuid playerGuid) const;
        void SendLFGListQueryResponse(ObjectGuid playerGuid, Team playerTeam, LfgType type, uint32 entry) const;

        void GroupUpdate(ObjectGuid playerGuid, ObjectGuid leaderGuid, bool completed = false);
        bool GroupUpdateQueueStatus(ObjectGuid playerGuid, ObjectGuid leaderGuid);
        void GroupUpdateUI(ObjectGuid leaderGuid, bool completed = false);
        static void GroupMakeMeetingStoneQueueJoinedFor(WorldPacket& message, uint16 entry);
        static void GroupMakeMeetingStoneQueueLeftFor(WorldPacket& message, uint16 entry);
        static void GroupMakeMeetingStoneQueueMatchedFor(WorldPacket& message, uint32 entry, bool asLeader = false);
        static void GroupMakeMeetingStoneMemberAdded(WorldPacket& data, ObjectGuid guid);
        static void PendingInvite(WorldPacket& data, uint32 entry);
        static void PendingMatch(WorldPacket& data, uint32 entry);
        static void PendingMatchDone(WorldPacket& data);

        Messager<LFGQueue>& GetMessager() { return m_messager; }

    private:
        typedef std::map<ObjectGuid, LFGPlayerQueueInfo> QueuedPlayersMap;
        QueuedPlayersMap m_queuedPlayers;

        Messager<LFGQueue> m_messager;
};

#endif
