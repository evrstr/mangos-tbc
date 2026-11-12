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

#ifndef __BATTLEGROUNDQUEUE_H
#define __BATTLEGROUNDQUEUE_H

#include "Common.h"
#include "BattleGround/BattleGround.h"

struct GroupQueueInfo;                                      // type predefinition (前置声明队伍排队信息)
struct PlayerQueueInfo                                      // stores information for players in queue (队列中单个玩家的排队信息)
{
    uint32  lastOnlineTime;                                 // for tracking and removing offline players from queue after 5 minutes (最后在线时间戳，用于 5 分钟离线移出队列)
    GroupQueueInfo* groupInfo;                              // pointer to the associated groupqueueinfo (所属队伍排队信息指针)
};

typedef std::map<ObjectGuid, PlayerQueueInfo*> GroupQueueInfoPlayers;

// 加入队列请求打包参数
struct AddGroupToQueueInfo
{
    Team team;
    std::vector<ObjectGuid> members; // empty when not in group (单排时为空)
    uint32 clientInstanceId;
    uint32 mapId;
};

struct GroupQueueInfo                                       // stores information about the group in queue (also used when joined as solo!) (队列中队伍或单排玩家的完整匹配信息)
{
    GroupQueueInfoPlayers players;                          // player queue info map (成员排队信息映射)
    Team  groupTeam;                                        // Player team (ALLIANCE/HORDE) (队伍所属阵营)
    BattleGroundTypeId bgTypeId;                            // battleground type id (排队战场类型 ID)
    BattleGroundBracketId bgBracketId;                      // battleground bracked id (等级分段 ID)
    bool    isRated;                                        // rated (是否为排位竞技场)
    uint32  mapId;                                          // invited to mapId (受邀目标地图 ID)
    uint32  clientInstanceId;                               // invited to client instance id (受邀客户端实例编号)
    ArenaType arenaType;                                    // 2v2, 3v3, 5v5 or 0 when BG (竞技场规模，战场为 0)
    uint32  arenaTeamId;                                    // team id if rated match (排位赛对应的竞技场战队 ID)
    uint32  joinTime;                                       // time when group was added (入队时间戳)
    uint32  removeInviteTime;                               // time when we will remove invite for players in group (邀请过期自动移除时间)
    uint32  isInvitedToBgInstanceGuid;                      // was invited to certain BG (受邀进入的战场实例 ID，0 表示尚未分配)
    uint32  desiredInstanceId;                              // queued for this instance specifically (指定排入的特定战场实例 ID)
    uint32  arenaTeamRating;                                // if rated match, inited to the rating of the team (排位赛本方战队匹配等级)
    uint32  opponentsTeamRating;                            // for rated arena matches (排位赛对手战队等级，用于赛后计算扣分/加分)
};

// 队列线程本地维护的可用战场实例快照信息
struct BattleGroundInQueueInfo
{
    BattleGroundTypeId bgTypeId;
    uint32 instanceId;
    bool isBattleGround;
    BattleGroundBracketId bracketId;
    BattleGroundStatus status; // only altered in bg and synched here (仅在战场内部修改后同步至此的运行状态)
    uint32 m_clientInstanceId;
    uint32 mapId;

    ArenaType arenaType;
    bool isRated;

    uint32 playersInside;
    uint32 maxPlayers;
    uint32 m_maxPlayersPerTeam;
    uint32 m_minPlayersPerTeam;

    uint32 GetInstanceId() const { return instanceId; }
    bool IsBattleGround() const { return isBattleGround; }
    bool IsArena() const { return !isBattleGround; }
    BattleGroundTypeId GetTypeId() const { return bgTypeId; }
    BattleGroundBracketId GetBracketId() const { return bracketId; }
    BattleGroundStatus GetStatus() const { return status; }
    uint32 GetClientInstanceId() const { return m_clientInstanceId; }
    uint32 GetMapId() const { return mapId; }

    ArenaType GetArenaType() const { return arenaType; }
    bool IsRated() const { return isRated; }

    uint32 GetPlayersSize() const { return playersInside; }
    uint32 GetMaxPlayers() const { return maxPlayers; }
    bool HasFreeSlots() const { return GetPlayersSize() < GetMaxPlayers(); }
    uint32 GetMaxPlayersPerTeam() const { return m_maxPlayersPerTeam; }
    uint32 GetMinPlayersPerTeam() const { return m_minPlayersPerTeam; }

    void DecreaseInvitedCount(Team team);
    void IncreaseInvitedCount(Team team);
    uint32 GetInvitedCount(Team team) const
    {
        if (team == ALLIANCE)
            return m_invitedAlliance;
        return m_invitedHorde;
    }

    uint32 GetFreeSlotsForTeam(Team team) const
    {
        // return free slot count to MaxPlayerPerTeam (计算指定阵营剩余可邀请空位数量)
        if (GetStatus() == STATUS_WAIT_JOIN || GetStatus() == STATUS_IN_PROGRESS)
            return (GetInvitedCount(team) < GetMaxPlayersPerTeam()) ? GetMaxPlayersPerTeam() - GetInvitedCount(team) : 0;

        return 0;
    }

    void Fill(BattleGround* bg);                            // 从真实战场对象填充队列快照数据

    private:
        uint32 m_invitedAlliance;
        uint32 m_invitedHorde;
};

// this container can't be deque, because deque doesn't like removing the last element - if you remove it, it invalidates next iterator and crash appears
// (使用 std::list 存储有空位的战场实例快照，避免 deque 删除尾元素导致迭代器失效)
typedef std::list<BattleGroundInQueueInfo> BgFreeSlotQueueType;

class BattleGroundQueue;

// 单个战场/竞技场队列项：管理各等级分段的排队队伍、自动匹配与进场邀请池
class BattleGroundQueueItem
{
    public:
        BattleGroundQueueItem();
        ~BattleGroundQueueItem();

        // 调度一次指定战场类型与等级分段的匹配更新
        void Update(BattleGroundQueue& queue, BattleGroundTypeId /*bgTypeId*/, BattleGroundBracketId /*bracketId*/, ArenaType arenaType = ARENA_TYPE_NONE, bool isRated = false, uint32 arenaRating = 0);

        void FillPlayersToBg(BattleGroundInQueueInfo& queueInfo, BattleGroundBracketId /*bracketId*/); // 为已开启但未满员的战场补充排队玩家
        bool CheckPremadeMatch(BattleGroundBracketId /*bracketId*/, uint32 /*minPlayersPerTeam*/, uint32 /*maxPlayersPerTeam*/); // 优先匹配双方整编预组队
        bool CheckNormalMatch(BattleGroundQueue& queue, BattleGround* /*bgTemplate*/, BattleGroundBracketId /*bracketId*/, uint32 /*minPlayers*/, uint32 /*maxPlayers*/); // 匹配常规散人与小队
        bool CheckSkirmishForSameFaction(BattleGroundBracketId /*bracketId*/, uint32 /*minPlayersPerTeam*/); // 竞技场练习赛支持同阵营内战匹配
        GroupQueueInfo* AddGroup(ObjectGuid leader, AddGroupToQueueInfo const& /*groupInfo*/, BattleGroundTypeId /*bgTypeId*/, BattleGroundBracketId /*bracketEntry*/, ArenaType /*arenaType*/, bool /*isRated*/, bool /*isPremade*/, uint32 /*instanceId*/, uint32 /*arenaRating*/, uint32 arenaTeamId = 0); // 将玩家或队伍加入对应排队队列
        void RemovePlayer(BattleGroundQueue& queue, ObjectGuid guid, bool decreaseInvitedCount); // 从队列中移除玩家，并在队伍为空时释放 GroupQueueInfo
        bool IsPlayerInvited(ObjectGuid /*playerGuid*/, const uint32 /*bgInstanceGuid*/, const uint32 /*removeTime*/);
        bool GetPlayerGroupInfoData(ObjectGuid /*guid*/, GroupQueueInfo* /*groupInfo*/);
        void PlayerInvitedToBgUpdateAverageWaitTime(GroupQueueInfo& /*groupInfo*/, BattleGroundBracketId /*bracketId*/); // 记录受邀玩家的等待耗时以更新滚动平均值
        uint32 GetAverageQueueWaitTime(GroupQueueInfo* /*groupInfo*/, BattleGroundBracketId /*bracketId*/); // 获取当前阵营与等级段的平均排队时间

    private:
        typedef std::map<ObjectGuid, PlayerQueueInfo> QueuedPlayersMap;
        QueuedPlayersMap m_queuedPlayers;

        // we need constant add to begin and constant remove / add from the end, therefore deque suits our problem well
        typedef std::list<GroupQueueInfo*> GroupsQueueType;

        /*
        This two dimensional array is used to store All queued groups
        First dimension specifies the bgTypeId
        Second dimension specifies the player's group types -
             BG_QUEUE_PREMADE_ALLIANCE  is used for premade alliance groups and alliance rated arena teams
             BG_QUEUE_PREMADE_HORDE     is used for premade horde groups and horde rated arena teams
             BG_QUEUE_NORMAL_ALLIANCE   is used for normal (or small) alliance groups or non-rated arena matches
             BG_QUEUE_NORMAL_HORDE      is used for normal (or small) horde groups or non-rated arena matches
        (按等级分段和队列分类存储所有排队的队伍)
        */
        GroupsQueueType m_queuedGroups[MAX_BATTLEGROUND_BRACKETS][BG_QUEUE_GROUP_TYPES_COUNT];

        // class to select and invite groups to bg (匹配筛选池：用于从等待队列中挑选凑齐单场人数的队伍组合)
        class SelectionPool
        {
        public:
            SelectionPool() : playerCount(0) {}
            void Init();
            bool AddGroup(GroupQueueInfo* ginfo, uint32 desiredCount, uint32 bgInstanceId);
            bool KickGroup(uint32 size);
            uint32 GetPlayerCount() const { return playerCount; }
            GroupsQueueType selectedGroups;
        private:
            uint32 playerCount;
        };

        // one selection pool for horde, other one for alliance (联盟与部落各自的候选匹配池)
        SelectionPool m_selectionPools[PVP_TEAM_COUNT];

        bool InviteGroupToBg(GroupQueueInfo& groupInfo, BattleGroundInQueueInfo& queueInfo, Team side); // 向选中队伍的所有成员发送进场邀请

        uint32 m_waitTimes[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS][COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME];
        uint32 m_waitTimeLastPlayer[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS];
        uint32 m_sumOfWaitTimes[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS];
};

/*
    This class is used to invite player to BG again, when minute lasts from his first invitation
    it is capable to solve all possibilities
    (战场邀请二次提醒事件：首次发出邀请 60 秒后若玩家仍未进入则再次发送确认包)
*/
class BgQueueInviteEvent : public BasicEvent
{
    public:
        BgQueueInviteEvent(ObjectGuid playerGuid, uint32 bgInstanceGuid, BattleGroundTypeId bgTypeId, ArenaType arenaType, uint32 removeTime) :
            m_playerGuid(playerGuid), m_bgInstanceGuid(bgInstanceGuid), m_bgTypeId(bgTypeId), m_arenaType(arenaType), m_removeTime(removeTime)
        {
        };
        virtual ~BgQueueInviteEvent() {};

        virtual bool Execute(uint64 e_time, uint32 p_time) override;
        virtual void Abort(uint64 e_time) override;

    private:
        ObjectGuid m_playerGuid;
        uint32 m_bgInstanceGuid;
        BattleGroundTypeId m_bgTypeId;
        ArenaType m_arenaType;
        uint32 m_removeTime;
};

/*
    This class is used to remove player from BG queue after 1 minute 20 seconds from first invitation
    We must store removeInvite time in case player left queue and joined and is invited again
    We must store bgQueueTypeId, because battleground can be deleted already, when player entered it
    (战场邀请超时移出事件：首次发出邀请 80 秒后仍未进场则取消邀请并移出队列)
*/
class BgQueueRemoveEvent : public BasicEvent
{
    public:
        BgQueueRemoveEvent(ObjectGuid playerGuid, uint32 bgInstanceGuid, BattleGroundTypeId bgTypeId, BattleGroundQueueTypeId bgQueueTypeId, uint32 removeTime)
            : m_playerGuid(playerGuid), m_bgInstanceGuid(bgInstanceGuid), m_removeTime(removeTime), m_bgTypeId(bgTypeId), m_bgQueueTypeId(bgQueueTypeId)
        {}

        virtual ~BgQueueRemoveEvent() {}

        virtual bool Execute(uint64 e_time, uint32 p_time) override;
        virtual void Abort(uint64 e_time) override;

    private:
        ObjectGuid m_playerGuid;
        uint32 m_bgInstanceGuid;
        uint32 m_removeTime;
        BattleGroundTypeId m_bgTypeId;
        BattleGroundQueueTypeId m_bgQueueTypeId;
};

// 全局战场排队总控系统：管理全部队列类型、可用战场槽位、竞技场等级差放宽及自动周结算定时
class BattleGroundQueue
{
    friend class BgQueueInviteEvent;
    friend class BgQueueRemoveEvent;
    public:
        BattleGroundQueue();

        void Update();                                      // 执行排队的异步消息处理、匹配调度、等级放宽与每周竞技场点数发放

        Messager<BattleGroundQueue>& GetMessager() { return m_messager; }

        void InitAutomaticArenaPointDistribution();         // 初始化下次自动发放竞技场点数的时间点

        void ScheduleQueueUpdate(uint32 /*arenaRating*/, ArenaType /*arenaType*/, BattleGroundQueueTypeId /*bgQueueTypeId*/, BattleGroundTypeId /*bgTypeId*/, BattleGroundBracketId /*bracketId*/); // 注册去重后的队列匹配更新任务

        void AddBgToFreeSlots(BattleGroundInQueueInfo const& info); // 向空闲槽位队列添加或刷新战场实例信息
        void RemoveBgFromFreeSlots(BattleGroundTypeId typeId, uint32 instanceId); // 从空闲槽位队列移除已满或关闭的战场

        void RemovePlayer(BattleGroundQueueTypeId bgQueueTypeId, ObjectGuid player, bool decreaseInvitedCount); // 从指定队列移除玩家

        BgFreeSlotQueueType& GetFreeSlotQueueItem(BattleGroundTypeId bgTypeId);
        BattleGroundInQueueInfo* GetFreeSlotInstance(BattleGroundTypeId bgTypeId, uint32 instanceId);
        BattleGroundQueueItem& GetBattleGroundQueue(BattleGroundQueueTypeId bgQueueTypeId);

        void SetNextRatingDiscardUpdate(std::chrono::milliseconds timePoint);

        bool IsTesting() const { return m_testing; }
        void SetTesting(bool state) { m_testing = state; }
        bool IsArenaTesting() const { return m_arenaTesting; }
        void SetArenaTesting(bool state) { m_arenaTesting = state; }

        uint32 CreateClientVisibleInstanceId(BattleGroundTypeId /*bgTypeId*/, BattleGroundBracketId /*bracketId*/); // 为指定战场类型与等级段分配不冲突的客户端可见实例编号
        void DeleteClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracketId, uint32 clientInstanceId)
        {
            m_clientBattleGroundIds[bgTypeId][bracketId].erase(clientInstanceId);
        }

        void BuildBattleGroundListPacket(WorldPacket& data, ObjectGuid guid, uint32 playerLevel, BattleGroundTypeId bgTypeId) const; // 构建战场军官对话时的战场实例列表数据包
    private:
        BattleGroundQueueItem m_battleGroundQueues[MAX_BATTLEGROUND_QUEUE_TYPES];

        BgFreeSlotQueueType m_bgFreeSlotQueue[MAX_BATTLEGROUND_TYPE_ID];

        std::vector<uint64> m_queueUpdateScheduler;
        TimePoint m_nextRatingDiscardUpdate;
        TimePoint m_nextAutoDistributionTime;
        TimePoint m_autoDistributionTimeChecker;

        Messager<BattleGroundQueue> m_messager;

        bool m_testing;
        bool m_arenaTesting;

        typedef std::set<uint32> ClientBattleGroundIdSet;
        ClientBattleGroundIdSet m_clientBattleGroundIds[MAX_BATTLEGROUND_TYPE_ID][MAX_BATTLEGROUND_BRACKETS]; // the instanceids just visible for the client (当前占用中的客户端可见战场实例编号集合)
};

#endif