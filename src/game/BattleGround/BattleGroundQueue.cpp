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

#include "BattleGround/BattleGroundQueue.h"
#include "Tools/Language.h"
#include "World/World.h"
#include "BattleGround/BattleGroundMgr.h"
#include "Arena/ArenaTeam.h"

 /*********************************************************/
 /***            BATTLEGROUND QUEUE SYSTEM              ***/
 /*********************************************************/

BattleGroundQueueItem::BattleGroundQueueItem()
{
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        for (uint8 j = 0; j < MAX_BATTLEGROUND_BRACKETS; ++j)
        {
            m_sumOfWaitTimes[i][j] = 0;
            m_waitTimeLastPlayer[i][j] = 0;

            for (uint8 k = 0; k < COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME; ++k)
                m_waitTimes[i][j][k] = 0;
        }
    }
}

BattleGroundQueueItem::~BattleGroundQueueItem()
{
    m_queuedPlayers.clear();
    for (auto& m_queuedGroup : m_queuedGroups)
    {
        for (uint8 j = 0; j < BG_QUEUE_GROUP_TYPES_COUNT; ++j)
        {
            for (GroupsQueueType::iterator itr = m_queuedGroup[j].begin(); itr != m_queuedGroup[j].end(); ++itr)
                delete (*itr);

            m_queuedGroup[j].clear();
        }
    }
}

/*********************************************************/
/***      BATTLEGROUND QUEUE SELECTION POOLS           ***/
/*********************************************************/

// selection pool initialization, used to clean up from prev selection (清空上轮筛选池)
void BattleGroundQueueItem::SelectionPool::Init()
{
    selectedGroups.clear();
    playerCount = 0;
}

/**
  Function that removes group infr from pool selection (从筛选池踢出一个队伍以平衡双方人数差)
  - returns true when we need to try to add new group to selection pool (返回 true 表示需要继续补充队伍)
  - returns false when selection pool is ok or when we kicked smaller group than we need to kick (返回 false 表示无需继续补充)
  - sometimes it can be called on empty selection pool

  @param    size
*/
bool BattleGroundQueueItem::SelectionPool::KickGroup(uint32 size)
{
    // find maxgroup or LAST group with size == size and kick it (优先寻找人数最接近目标差值的队伍，否则寻找最大队伍踢出)
    bool found = false;
    GroupsQueueType::iterator groupToKick = selectedGroups.begin();

    for (GroupsQueueType::iterator itr = groupToKick; itr != selectedGroups.end(); ++itr)
    {
        if (abs((int32)((*itr)->players.size() - size)) <= 1)
        {
            groupToKick = itr;
            found = true;
        }
        else if (!found && (*itr)->players.size() >= (*groupToKick)->players.size())
            groupToKick = itr;
    }

    // if pool is empty, do nothing
    if (GetPlayerCount())
    {
        // update player count (更新已选中总人数)
        GroupQueueInfo* queueInfo = (*groupToKick);
        selectedGroups.erase(groupToKick);
        playerCount -= queueInfo->players.size();

        // return false if we kicked smaller group or there are enough players in selection pool
        if (queueInfo->players.size() <= size + 1)
            return false;
    }
    return true;
}

/**
  Function that adds group to selection pool (尝试将队伍加入筛选池；若未受邀且实例匹配且未超上限则加入)
  - returns true if we can invite more players, or when we added group to selection pool
  - returns false when selection pool is full

  @param    group queue info
  @param    desired count
*/
bool BattleGroundQueueItem::SelectionPool::AddGroup(GroupQueueInfo* queueInfo, uint32 desiredCount, uint32 bgInstanceId)
{
    // if group is larger than desired count - don't allow to add it to pool (队伍人数超出剩余空位或指定了其他实例时不加入)
    if (!queueInfo->isInvitedToBgInstanceGuid &&
        (!queueInfo->desiredInstanceId || queueInfo->desiredInstanceId == bgInstanceId) &&
        (desiredCount >= playerCount + queueInfo->players.size()))
    {
        selectedGroups.push_back(queueInfo);
        // increase selected players count
        playerCount += queueInfo->players.size();

        return true;
    }

    return playerCount < desiredCount;
}

/*********************************************************/
/***               BATTLEGROUND QUEUES                 ***/
/*********************************************************/

/**
  Function that adds group or player (grp == nullptr) to battleground queue with the given leader and specifications
  (将单人玩家或整支队伍加入战场/竞技场队列，登记成员信息并发送入队广播)

  @param    leader player
  @param    group
  @param    battleground type id
  @param    bracket entry
  @param    arena type
  @param    isRated
  @param    isPremade
  @param    arena rating
  @param    arena team id
*/
GroupQueueInfo* BattleGroundQueueItem::AddGroup(ObjectGuid leader, AddGroupToQueueInfo const& groupInfo, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracketId, ArenaType arenaType, bool isRated, bool isPremade, uint32 instanceId, uint32 arenaRating, uint32 arenaTeamId)
{
    // create new ginfo (创建队伍排队信息节点)
    GroupQueueInfo* queueInfo = new GroupQueueInfo;
    queueInfo->bgTypeId = bgTypeId;
    queueInfo->bgBracketId = bracketId;
    queueInfo->arenaType = arenaType;
    queueInfo->arenaTeamId = arenaTeamId;
    queueInfo->isRated = isRated;
    queueInfo->isInvitedToBgInstanceGuid = 0;
    queueInfo->mapId = groupInfo.mapId;
    queueInfo->clientInstanceId = groupInfo.clientInstanceId;
    queueInfo->joinTime = WorldTimer::getMSTime();
    queueInfo->removeInviteTime = 0;
    queueInfo->groupTeam = groupInfo.team;
    queueInfo->desiredInstanceId = instanceId;
    queueInfo->arenaTeamRating = arenaRating;
    queueInfo->opponentsTeamRating = 0;

    queueInfo->players.clear();

    // compute index (if group is premade or joined a rated match) to queues
    // (计算目标队列分类下标：预组/排位使用 0..1，散人/练习赛使用 2..3)
    uint32 index = 0;
    if (!isRated && !isPremade)
        index += PVP_TEAM_COUNT;                            // BG_QUEUE_PREMADE_* -> BG_QUEUE_NORMAL_*

    if (queueInfo->groupTeam == HORDE)
        ++index;                                            // BG_QUEUE_*_ALLIANCE -> BG_QUEUE_*_HORDE

    DEBUG_LOG("Adding Group to BattleGroundQueueItem bgTypeId : %u, bracket_id : %u, index : %u", bgTypeId, bracketId, index);

    uint32 lastOnlineTime = WorldTimer::getMSTime();

    // announce world (this don't need mutex) (排位赛报名全服广播)
    if (isRated && sWorld.getConfig(CONFIG_BOOL_ARENA_QUEUE_ANNOUNCER_JOIN))
    {
        sWorld.SendWorldText(LANG_ARENA_QUEUE_ANNOUNCE_WORLD_JOIN, queueInfo->arenaType, queueInfo->arenaType, queueInfo->arenaTeamRating);
    }

    // add players from group to ginfo (为队伍所有成员建立排队索引映射)
    {
        if (!groupInfo.members.empty())
        {
            for (ObjectGuid member : groupInfo.members)
            {
                PlayerQueueInfo& playerInfo = m_queuedPlayers[member];
                playerInfo.lastOnlineTime = lastOnlineTime;
                playerInfo.groupInfo = queueInfo;
                // add the pinfo to ginfo's list
                queueInfo->players[member] = &playerInfo;
            }
        }
        else
        {
            PlayerQueueInfo& playerInfo = m_queuedPlayers[leader];
            playerInfo.lastOnlineTime = lastOnlineTime;
            playerInfo.groupInfo = queueInfo;
            queueInfo->players[leader] = &playerInfo;
        }

        // add GroupInfo to m_QueuedGroups (加入对应等级段与阵营队列)
        m_queuedGroups[bracketId][index].push_back(queueInfo);

        // announce to world, this code needs mutex (普通战场入队人数进度提示或全服通告)
        if (arenaType == ARENA_TYPE_NONE && !isRated && !isPremade && sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN))
        {
            if (BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(queueInfo->bgTypeId))
            {
                char const* bgName = bg->GetName();
                uint32 minPlayers = bg->GetMinPlayersPerTeam();
                uint32 qHorde = 0;
                uint32 qAlliance = 0;
                uint32 q_min_level = sBattleGroundMgr.GetMinLevelForBattleGroundBracketId(bracketId, bgTypeId);
                uint32 qMaxLevel = sBattleGroundMgr.GetMaxLevelForBattleGroundBracketId(bracketId, bgTypeId);
                GroupsQueueType::const_iterator itr;
                for (itr = m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].begin(); itr != m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].end(); ++itr)
                    if (!(*itr)->isInvitedToBgInstanceGuid)
                        qAlliance += (*itr)->players.size();

                for (itr = m_queuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].begin(); itr != m_queuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].end(); ++itr)
                    if (!(*itr)->isInvitedToBgInstanceGuid)
                        qHorde += (*itr)->players.size();

                sWorld.GetMessager().AddMessage([playerGuid = leader, bgName, q_min_level, qMaxLevel, qAlliance, minPlayers, qHorde](World* /*world*/)
                {
                    // Show queue status to player only (when joining queue) (配置=1 仅提示入队玩家本人)
                    if (sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN) == 1)
                    {
                        if (Player* plr = sObjectMgr.GetPlayer(playerGuid))
                            ChatHandler(plr).PSendSysMessage(LANG_BG_QUEUE_ANNOUNCE_SELF, bgName, q_min_level, qMaxLevel,
                                qAlliance, (minPlayers > qAlliance) ? minPlayers - qAlliance : (uint32)0, qHorde, (minPlayers > qHorde) ? minPlayers - qHorde : (uint32)0);
                    }
                    // System message (配置>1 全服广播排队人数)
                    else
                    {
                        sWorld.SendWorldText(LANG_BG_QUEUE_ANNOUNCE_WORLD, bgName, q_min_level, qMaxLevel,
                            qAlliance, (minPlayers > qAlliance) ? minPlayers - qAlliance : (uint32)0, qHorde, (minPlayers > qHorde) ? minPlayers - qHorde : (uint32)0);
                    }
                });
            }
        }
        // release mutex
    }

    return queueInfo;
}

/**
  Method that updates average update wait time (使用循环缓冲区维护最近 N 名受邀玩家的平均排队耗时)

  @param    group queue info
  @param    bracket id
*/
void BattleGroundQueueItem::PlayerInvitedToBgUpdateAverageWaitTime(GroupQueueInfo& queueInfo, BattleGroundBracketId bracketId)
{
    uint32 timeInQueue = WorldTimer::getMSTimeDiff(queueInfo.joinTime, WorldTimer::getMSTime());
    uint8 teamIndex = TEAM_INDEX_ALLIANCE;                     // default set to BG_TEAM_ALLIANCE - or non rated arenas!

    if (queueInfo.arenaType == ARENA_TYPE_NONE)
    {
        if (queueInfo.groupTeam == HORDE)
            teamIndex = TEAM_INDEX_HORDE;
    }
    else
    {
        if (queueInfo.isRated)
            teamIndex = TEAM_INDEX_HORDE;                     // for rated arenas use BG_TEAM_HORDE (排位赛复用部落槽位统计耗时)
    }

    // store pointer to arrayindex of player that was added first
    uint32* lastPlayerAddedPointer = &(m_waitTimeLastPlayer[teamIndex][bracketId]);

    // remove his time from sum (从总和减去被覆盖的最旧样本)
    m_sumOfWaitTimes[teamIndex][bracketId] -= m_waitTimes[teamIndex][bracketId][(*lastPlayerAddedPointer)];

    // set average time to new
    m_waitTimes[teamIndex][bracketId][(*lastPlayerAddedPointer)] = timeInQueue;

    // add new time to sum
    m_sumOfWaitTimes[teamIndex][bracketId] += timeInQueue;

    // set index of last player added to next one
    (*lastPlayerAddedPointer)++;
    (*lastPlayerAddedPointer) %= COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME;
}

/**
  Function that returns averate queue wait time (计算平均等待时间；样本不足时返回 0 表示未知)

  @param    group queue info
  @param    bracket id
*/
uint32 BattleGroundQueueItem::GetAverageQueueWaitTime(GroupQueueInfo* queueInfo, BattleGroundBracketId bracketId)
{
    uint8 teamIndex = TEAM_INDEX_ALLIANCE;                     // default set to BG_TEAM_ALLIANCE - or non rated arenas!
    if (queueInfo->arenaType == ARENA_TYPE_NONE)
    {
        if (queueInfo->groupTeam == HORDE)
            teamIndex = TEAM_INDEX_HORDE;
    }
    else
    {
        if (queueInfo->isRated)
            teamIndex = TEAM_INDEX_HORDE;                     // for rated arenas use BG_TEAM_HORDE
    }

    // check if there is enought values(we always add values > 0) (环形缓冲区填满后才返回均值)
    if (m_waitTimes[teamIndex][bracketId][COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME - 1])
        return (m_sumOfWaitTimes[teamIndex][bracketId] / COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME);

    // if there aren't enough values return 0 - not available
    return 0;
}

/**
  Method that removes player from queue and from group info, if group info is empty then remove it too
  (将玩家从排队记录移除；处理已受邀人数回退、排位赛中途退出扣分及整队联动退队)

  @param    guid
  @param    decrease invite count
*/
void BattleGroundQueueItem::RemovePlayer(BattleGroundQueue& queue, ObjectGuid guid, bool decreaseInvitedCount)
{
    int32 bracketId = -1;                                   // signed for proper for-loop finish

    // remove player from map, if he's there
    QueuedPlayersMap::iterator itr = m_queuedPlayers.find(guid);
    if (itr == m_queuedPlayers.end())
    {
        sLog.outError("BattleGroundQueueItem: couldn't find for remove: %s", guid.GetString().c_str());
        return;
    }

    GroupQueueInfo* group = itr->second.groupInfo;
    GroupsQueueType::iterator group_itr;
    // mostly people with the highest levels are in battlegrounds, thats why
    // we count from MAX_BATTLEGROUND_QUEUES - 1 to 0
    // variable index removes useless searching in other team's queue
    uint32 index = GetTeamIndexByTeamId(group->groupTeam);

    for (int8 bracketIdTmp = MAX_BATTLEGROUND_BRACKETS - 1; bracketIdTmp >= 0 && bracketId == -1; --bracketIdTmp)
    {
        // we must check premade and normal team's queue - because when players from premade are joining bg,
        // they leave groupinfo so we can't use its players size to find out index
        // (同时扫描预组队和散人队列以定位目标 GroupQueueInfo)
        for (uint8 j = index; j < BG_QUEUE_GROUP_TYPES_COUNT; j += BG_QUEUE_NORMAL_ALLIANCE)
        {
            for (GroupsQueueType::iterator group_itr_tmp = m_queuedGroups[bracketIdTmp][j].begin(); group_itr_tmp != m_queuedGroups[bracketIdTmp][j].end(); ++group_itr_tmp)
            {
                if ((*group_itr_tmp) == group)
                {
                    bracketId = bracketIdTmp;
                    group_itr = group_itr_tmp;
                    // we must store index to be able to erase iterator
                    index = j;
                    break;
                }
            }
        }
    }

    // player can't be in queue without group, but just in case
    if (bracketId == -1)
    {
        sLog.outError("BattleGroundQueueItem: ERROR Cannot find groupinfo for %s", guid.GetString().c_str());
        return;
    }
    DEBUG_LOG("BattleGroundQueueItem: Removing %s, from bracket_id %u", guid.GetString().c_str(), (uint32)bracketId);

    // ALL variables are correctly set
    // We can ignore leveling up in queue - it should not cause crash
    // remove player from group
    // if only one player there, remove group

    // remove player queue info from group queue info
    GroupQueueInfoPlayers::iterator pitr = group->players.find(guid);
    if (pitr != group->players.end())
        group->players.erase(pitr);

    // if invited to bg, and should decrease invited count, then do it (若玩家放弃/超时未进入已邀请实例，释放其占用名额)
    if (decreaseInvitedCount && group->isInvitedToBgInstanceGuid)
    {
        if (BattleGroundInQueueInfo* bgInstance = queue.GetFreeSlotInstance(group->bgTypeId, group->isInvitedToBgInstanceGuid))
            bgInstance->DecreaseInvitedCount(group->groupTeam);
    }

    // remove player queue info
    m_queuedPlayers.erase(itr);

    // announce to world if arena team left queue for rated match, show only once
    if (group->arenaType != ARENA_TYPE_NONE && group->isRated && group->players.empty() && sWorld.getConfig(CONFIG_BOOL_ARENA_QUEUE_ANNOUNCER_EXIT))
        sWorld.SendWorldText(LANG_ARENA_QUEUE_ANNOUNCE_WORLD_EXIT, group->arenaType, group->arenaType, group->arenaTeamRating);

    // if player leaves queue and he is invited to rated arena match, then he have to loose (已受邀排位赛但放弃/超时未进场，按战败扣个人分)
    if (group->isInvitedToBgInstanceGuid && group->isRated && decreaseInvitedCount)
    {
        sWorld.GetMessager().AddMessage([arenaTeamId = group->arenaTeamId, guid, opponentRating = group->opponentsTeamRating](World* /*world*/)
        {
            ArenaTeam* at = sObjectMgr.GetArenaTeamById(arenaTeamId); // TODO: One day do all arenateam alterations in bg thread
            if (at)
            {
                DEBUG_LOG("UPDATING memberLost's personal arena rating for %s by opponents rating: %u", guid.GetString().c_str(), opponentRating);
                Player* plr = sObjectMgr.GetPlayer(guid);
                if (plr)
                    at->MemberLost(plr, opponentRating);
                else
                    at->OfflineMemberLost(guid, opponentRating);

                at->SaveToDB();
            }
        });
    }

    // remove group queue info if needed (队伍已无剩余成员时释放 GroupQueueInfo)
    if (group->players.empty())
    {
        m_queuedGroups[bracketId][index].erase(group_itr);
        delete group;
    }
    // if group wasn't empty, so it wasn't deleted, and player have left a rated
    // queue -> everyone from the group should leave too
    // don't remove recursively if already invited to bg!
    // (排位赛尚未受邀前，只要有一名队员退出排队，则整支队伍其余成员同步撤出排队)
    else if (!group->isInvitedToBgInstanceGuid && group->isRated)
    {
        sWorld.GetMessager().AddMessage([playerGuid = group->players.begin()->first, bgTypeId = group->bgTypeId, arenaType = group->arenaType](World* /*world*/)
        {
            // remove next player, this is recursive
            // first send removal information
            if (Player* plr2 = sObjectMgr.GetPlayer(playerGuid))
            {
                BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
                BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BgQueueTypeId(bgTypeId, arenaType);

                uint32 queueSlot = plr2->GetBattleGroundQueueIndex(bgQueueTypeId);
                plr2->RemoveBattleGroundQueueId(bgQueueTypeId); // must be called this way, because if you move this call to

                // queue->removeplayer, it causes bugs
                WorldPacket data;
                sBattleGroundMgr.BuildBattleGroundStatusPacket(data, bg, bg->GetTypeId(), bg->GetClientInstanceId(), bg->IsRated(), bg->GetMapId(), queueSlot, STATUS_NONE, 0, 0, ARENA_TYPE_NONE, TEAM_NONE);
                plr2->GetSession()->SendPacket(data);
            }
        });

        // then actually delete, this may delete the group as well!
        RemovePlayer(queue, group->players.begin()->first, decreaseInvitedCount);
    }
}

/**
  Function that returns true when player is in queue and is invited to bgInstanceGuid (校验玩家是否仍处于目标战场实例的当前有效邀请中)

  @param    player guid
  @param    battleground instance guid
  @param    remove time
*/
bool BattleGroundQueueItem::IsPlayerInvited(ObjectGuid playerGuid, const uint32 bgInstanceGuid, const uint32 removeTime)
{
    QueuedPlayersMap::const_iterator qItr = m_queuedPlayers.find(playerGuid);
    return (qItr != m_queuedPlayers.end()
        && qItr->second.groupInfo->isInvitedToBgInstanceGuid == bgInstanceGuid
        && qItr->second.groupInfo->removeInviteTime == removeTime);
}

/**
  Function that returns player group info data (读取玩家所属队伍的排队信息副本)
  - returns true when the player is found in queue

  @param    player guid
  @param    group queue info
*/
bool BattleGroundQueueItem::GetPlayerGroupInfoData(ObjectGuid guid, GroupQueueInfo* queueInfo)
{
    // std::lock_guard<std::recursive_mutex> g(m_Lock);
    QueuedPlayersMap::const_iterator qItr = m_queuedPlayers.find(guid);
    if (qItr == m_queuedPlayers.end())
        return false;

    *queueInfo = *(qItr->second.groupInfo);
    return true;
}

/**
  Function that invites group to battleground (向整支队伍成员发出战场进场邀请，并在世界线程注册 60 秒提醒与 80 秒超时移除事件)

  @param    group queue info
  @param    battleground
  @param    team
*/
bool BattleGroundQueueItem::InviteGroupToBg(GroupQueueInfo& groupInfo, BattleGroundInQueueInfo& queueInfo, Team team)
{
    // set side if needed (同阵营内战时覆盖队伍的进场阵营)
    if (team == ALLIANCE || team == HORDE)
        groupInfo.groupTeam = team;

    if (!groupInfo.isInvitedToBgInstanceGuid)
    {
        // not yet invited (标记受邀实例与超时时间戳)
        // set invitation
        groupInfo.isInvitedToBgInstanceGuid = queueInfo.GetInstanceId();
        groupInfo.mapId = queueInfo.GetMapId();
        groupInfo.clientInstanceId = queueInfo.GetClientInstanceId();
        BattleGroundTypeId bgTypeId = queueInfo.GetTypeId();
        BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BgQueueTypeId(bgTypeId, queueInfo.GetArenaType());
        BattleGroundBracketId bracket_id = queueInfo.GetBracketId();

        groupInfo.removeInviteTime = WorldTimer::getMSTime() + INVITE_ACCEPT_WAIT_TIME;

        // loop through the players (向队伍内每位成员注册提醒/超时事件并下发 STATUS_WAIT_JOIN 数据包)
        for (auto itr = groupInfo.players.begin(); itr != groupInfo.players.end(); ++itr)
        {
            sWorld.GetMessager().AddMessage([playerGuid = itr->first, bgQueueTypeId, bgTypeId, isInvited = groupInfo.isInvitedToBgInstanceGuid, clientInstanceId = queueInfo.GetClientInstanceId(), isRated = queueInfo.IsRated(), mapId = queueInfo.GetMapId(), arenaType = groupInfo.arenaType, removeInviteTime = groupInfo.removeInviteTime, instanceId = queueInfo.GetInstanceId()](World* /*world*/)
            {
                Player* plr = sObjectMgr.GetPlayer(playerGuid);
                // if offline, skip him, can happen due to asynchronicity now
                if (!plr)
                    return;

                plr->SetInviteForBattleGroundQueueType(bgQueueTypeId, isInvited);

                // create remind invite events (60 秒后若未响应则二次提醒)
                BgQueueInviteEvent* inviteEvent = new BgQueueInviteEvent(plr->GetObjectGuid(), isInvited, bgTypeId, arenaType, removeInviteTime);
                plr->m_events.AddEvent(inviteEvent, plr->m_events.CalculateTime(INVITATION_REMIND_TIME));

                // create automatic remove events (80 秒后仍未进场则自动移出队列)
                BgQueueRemoveEvent* removeEvent = new BgQueueRemoveEvent(plr->GetObjectGuid(), isInvited, bgTypeId, bgQueueTypeId, removeInviteTime);
                plr->m_events.AddEvent(removeEvent, plr->m_events.CalculateTime(INVITE_ACCEPT_WAIT_TIME));

                WorldPacket data;

                uint32 queueSlot = plr->GetBattleGroundQueueIndex(bgQueueTypeId);

                DEBUG_LOG("Battleground: invited %s to BG instance %u queueindex %u bgtype %u, I can't help it if they don't press the enter battle button.", plr->GetGuidStr().c_str(), instanceId, queueSlot, bgTypeId);

                // send status packet
                sBattleGroundMgr.BuildBattleGroundStatusPacket(data, true, bgTypeId, clientInstanceId, isRated, mapId, queueSlot, STATUS_WAIT_JOIN, INVITE_ACCEPT_WAIT_TIME, 0, arenaType, TEAM_NONE);

                plr->GetSession()->SendPacket(data);
            });

            PlayerInvitedToBgUpdateAverageWaitTime(groupInfo, bracket_id);

            // set invited player counters (递增战场实例已邀请人数计数)
            queueInfo.IncreaseInvitedCount(groupInfo.groupTeam);
            // if issues arise due to async state, need to add pending and confirmation
        }
        return true;
    }

    return false;
}

/**
  Method that invites players to an already running battleground (为已运行但有空位的战场从散人队列筛选补人组合，并按配置尽量维持双方人数差 <= 1)
  - invitation is based on config file
  - large groups are disadvantageous, because they will be kicked first if invitation type = 1

  @param    battleground
  @param    bracket id
*/
void BattleGroundQueueItem::FillPlayersToBg(BattleGroundInQueueInfo& queueInfo, BattleGroundBracketId bracketId)
{
    int32 hordeFree = queueInfo.GetFreeSlotsForTeam(HORDE);
    int32 aliFree = queueInfo.GetFreeSlotsForTeam(ALLIANCE);

    // iterator for iterating through bg queue (按排队先后顺序尽量填满双方空位)
    GroupsQueueType::const_iterator Ali_itr = m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].begin();
    // count of groups in queue - used to stop cycles
    uint32 aliCount = m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].size();
    // index to queue which group is current
    uint32 aliIndex = 0;
    for (; aliIndex < aliCount && m_selectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*Ali_itr), aliFree, queueInfo.GetClientInstanceId()); ++aliIndex)
        ++Ali_itr;

    // the same thing for horde
    GroupsQueueType::const_iterator Horde_itr = m_queuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].begin();
    uint32 hordeCount = m_queuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].size();
    uint32 hordeIndex = 0;
    for (; hordeIndex < hordeCount && m_selectionPools[TEAM_INDEX_HORDE].AddGroup((*Horde_itr), hordeFree, queueInfo.GetClientInstanceId()); ++hordeIndex)
        ++Horde_itr;

    // if ofc like BG queue invitation is set in config, then we are happy (若配置为不平衡补人模式，则直接按空位全额邀请)
    if (sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE) == 0)
        return;

    /*
    if we reached this code, then we have to solve NP - complete problem called Subset sum problem
    So one solution is to check all possible invitation subgroups, or we can use these conditions:
    1. Last time when BattleGroundQueueItem::Update was executed we invited all possible players - so there is only small possibility
        that we will invite now whole queue, because only 1 change has been made to queues from the last BattleGroundQueueItem::Update call
    2. Other thing we should consider is group order in queue
    */

    // At first we need to compare free space in bg and our selection pool (平衡模式：通过踢出超出差值的小队，保证补人后双方人数差不超过 1)
    int32 diffAli = aliFree - int32(m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount());
    int32 diffHorde = hordeFree - int32(m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount());
    while (abs(diffAli - diffHorde) > 1 && (m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount() > 0 || m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() > 0))
    {
        // each cycle execution we need to kick at least 1 group
        if (diffAli < diffHorde)
        {
            // kick alliance group, add to pool new group if needed
            if (m_selectionPools[TEAM_INDEX_ALLIANCE].KickGroup(diffHorde - diffAli))
            {
                for (; aliIndex < aliCount && m_selectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*Ali_itr), (aliFree >= diffHorde) ? aliFree - diffHorde : 0, queueInfo.GetClientInstanceId()); ++aliIndex)
                    ++Ali_itr;
            }
            // if ali selection is already empty, then kick horde group, but if there are less horde than ali in bg - break;
            if (!m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())
            {
                if (aliFree <= diffHorde + 1)
                    break;
                m_selectionPools[TEAM_INDEX_HORDE].KickGroup(diffHorde - diffAli);
            }
        }
        else
        {
            // kick horde group, add to pool new group if needed
            if (m_selectionPools[TEAM_INDEX_HORDE].KickGroup(diffAli - diffHorde))
            {
                for (; hordeIndex < hordeCount && m_selectionPools[TEAM_INDEX_HORDE].AddGroup((*Horde_itr), (hordeFree >= diffAli) ? hordeFree - diffAli : 0, queueInfo.GetClientInstanceId()); ++hordeIndex)
                    ++Horde_itr;
            }
            if (!m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount())
            {
                if (hordeFree <= diffAli + 1)
                    break;
                m_selectionPools[TEAM_INDEX_ALLIANCE].KickGroup(diffAli - diffHorde);
            }
        }

        // count diffs after small update
        diffAli = aliFree - int32(m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount());
        diffHorde = hordeFree - int32(m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount());
    }
}

/**
  Method that checks if (premade vs premade) battlegrouns is possible (尝试撮合双方预组队对战；若预组队等待超时或人数减少则退回散人队列)
  - then after 30 mins (default) in queue it moves premade group to normal queue
  - it tries to invite as much players as it can - to maxPlayersPerTeam, because premade groups have more than minPlayersPerTeam players

  @param    bracket id
  @param    min players per team
  @param    max players per team
*/
bool BattleGroundQueueItem::CheckPremadeMatch(BattleGroundBracketId bracketId, uint32 minPlayersPerTeam, uint32 maxPlayersPerTeam)
{
    // check match
    if (!m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].empty() && !m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].empty())
    {
        // start premade match
        // if groups aren't invited (分别从双方预组队队列寻找首个尚未受邀的大团)
        GroupsQueueType::const_iterator ali_group, horde_group;
        for (ali_group = m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].begin(); ali_group != m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].end(); ++ali_group)
            if (!(*ali_group)->isInvitedToBgInstanceGuid)
                break;

        for (horde_group = m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].begin(); horde_group != m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].end(); ++horde_group)
            if (!(*horde_group)->isInvitedToBgInstanceGuid)
                break;

        if (ali_group != m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].end() && horde_group != m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].end())
        {
            m_selectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*ali_group), maxPlayersPerTeam, 0);
            m_selectionPools[TEAM_INDEX_HORDE].AddGroup((*horde_group), maxPlayersPerTeam, 0);

            // add groups/players from normal queue to size of bigger group (用散人队列玩家补齐人数较少的阵营以对齐较大预组队规模)
            uint32 maxPlayers = std::max(m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount(), m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount());
            for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
            {
                for (GroupsQueueType::const_iterator itr = m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE + i].begin(); itr != m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE + i].end(); ++itr)
                {
                    // if itr can join BG and player count is less that maxPlayers, then add group to selectionpool
                    if (!(*itr)->isInvitedToBgInstanceGuid && !m_selectionPools[i].AddGroup((*itr), maxPlayers, 0))
                        break;
                }
            }

            // premade selection pools are set
            return true;
        }
    }
    // now check if we can move group from Premade queue to normal queue (timer has expired) or group size lowered!!
    // this could be 2 cycles but i'm checking only first team in queue - it can cause problem -
    // if first is invited to BG and seconds timer expired, but we can ignore it, because players have only 80 seconds to click to enter bg
    // and when they click or after 80 seconds the queue info is removed from queue
    // (若预组队等待超过阈值或中途人员流失低于最低人数，将其移入散人队列队首)
    uint32 time_before = WorldTimer::getMSTime() - sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH);
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        if (!m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE + i].empty())
        {
            GroupsQueueType::iterator itr = m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE + i].begin();
            if (!(*itr)->isInvitedToBgInstanceGuid && ((*itr)->joinTime < time_before || (*itr)->players.size() < minPlayersPerTeam))
            {
                // we must insert group to normal queue and erase pointer from premade queue
                m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE + i].push_front((*itr));
                m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE + i].erase(itr);
            }
        }
    }

    // selection pools are not set
    return false;
}

/**
  Method that tries to create battleground or arena with minPlayersPerTeam against maxPlayersPerTeam
  (检查常规散人队列是否凑够双方最低人数以开启新战场/非排位竞技场)

  @param    battleground
  @param    bracket id
  @param    min players
  @param    max players
*/
bool BattleGroundQueueItem::CheckNormalMatch(BattleGroundQueue& queue, BattleGround* bgTemplate, BattleGroundBracketId bracketId, uint32 minPlayers, uint32 maxPlayers)
{
    GroupsQueueType::const_iterator itr_team[PVP_TEAM_COUNT];
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        itr_team[i] = m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE + i].begin();
        for (; itr_team[i] != m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE + i].end(); ++(itr_team[i]))
        {
            if (!(*(itr_team[i]))->isInvitedToBgInstanceGuid)
            {
                m_selectionPools[i].AddGroup(*(itr_team[i]), maxPlayers, 0);
                if (m_selectionPools[i].GetPlayerCount() >= minPlayers)
                    break;
            }
        }
    }

    // try to invite same number of players - this cycle may cause longer wait time even if there are enough players in queue, but we want ballanced bg
    // (平衡模式开启时，向人数较少一侧继续补选玩家并限制双方人数差不超过 2 人)
    uint32 j = TEAM_INDEX_ALLIANCE;
    if (m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount() < m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())
        j = TEAM_INDEX_HORDE;

    if (sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE) != 0
        && m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount() >= minPlayers && m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() >= minPlayers)
    {
        // we will try to invite more groups to team with less players indexed by j
        ++(itr_team[j]);                                    // this will not cause a crash, because for cycle above reached break;
        for (; itr_team[j] != m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE + j].end(); ++(itr_team[j]))
        {
            if (!(*(itr_team[j]))->isInvitedToBgInstanceGuid)
                if (!m_selectionPools[j].AddGroup(*(itr_team[j]), m_selectionPools[(j + 1) % PVP_TEAM_COUNT].GetPlayerCount(), 0))
                    break;
        }
        // do not allow to start bg with more than 2 players more on 1 faction
        if (abs((int32)(m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount() - m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())) > 2)
            return false;
    }

    // allow 1v0 if debug bg (开启 .debug bg 时允许单人/单边直接开场)
    if (queue.IsTesting() && bgTemplate->IsBattleGround() && (m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() || m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount()))
        return true;

    // return true if there are enough players in selection pools - enable to work .debug bg command correctly
    return m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() >= minPlayers && m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount() >= minPlayers;
}

/**
  Method that will check if we can invite players to same faction skirmish match
  (竞技场练习赛支持从同阵营队列中凑出两支队伍进行内战匹配)

  @param    bracket id
  @param    min players
*/
bool BattleGroundQueueItem::CheckSkirmishForSameFaction(BattleGroundBracketId bracketId, uint32 minPlayersPerTeam)
{
    if (m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() < minPlayersPerTeam && m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount() < minPlayersPerTeam)
        return false;

    PvpTeamIndex teamIdx = TEAM_INDEX_ALLIANCE;
    PvpTeamIndex otherTeamIdx = TEAM_INDEX_HORDE;
    Team otherTeamId = HORDE;

    if (m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount() == minPlayersPerTeam)
    {
        teamIdx = TEAM_INDEX_HORDE;
        otherTeamIdx = TEAM_INDEX_ALLIANCE;
        otherTeamId = ALLIANCE;
    }

    // clear other team's selection
    m_selectionPools[otherTeamIdx].Init();
    // store last ginfo pointer
    GroupQueueInfo* ginfo = m_selectionPools[teamIdx].selectedGroups.back();
    // set itr_team to group that was added to selection pool latest
    GroupsQueueType::iterator itr_team = m_queuedGroups[bracketId][static_cast<uint8>(BG_QUEUE_NORMAL_ALLIANCE) + static_cast<uint8>(teamIdx)].begin();
    for (; itr_team != m_queuedGroups[bracketId][static_cast<uint8>(BG_QUEUE_NORMAL_ALLIANCE) + static_cast<uint8>(teamIdx)].end(); ++itr_team)
        if (ginfo == *itr_team)
            break;

    if (itr_team == m_queuedGroups[bracketId][static_cast<uint8>(BG_QUEUE_NORMAL_ALLIANCE) + static_cast<uint8>(teamIdx)].end())
        return false;

    GroupsQueueType::iterator itr_team2 = itr_team;
    ++itr_team2;
    // invite players to other selection pool
    for (; itr_team2 != m_queuedGroups[bracketId][static_cast<uint8>(BG_QUEUE_NORMAL_ALLIANCE) + static_cast<uint8>(teamIdx)].end(); ++itr_team2)
    {
        // if selection pool is full then break;
        if (!(*itr_team2)->isInvitedToBgInstanceGuid && !m_selectionPools[otherTeamIdx].AddGroup(*itr_team2, minPlayersPerTeam, 0))
            break;
    }

    if (m_selectionPools[otherTeamIdx].GetPlayerCount() != minPlayersPerTeam)
        return false;

    // here we have correct 2 selections and we need to change one teams team and move selection pool teams to other team's queue
    // (将第二支同阵营队伍临时调剂至对立阵营队列以匹配内战)
    for (GroupsQueueType::iterator itr = m_selectionPools[otherTeamIdx].selectedGroups.begin(); itr != m_selectionPools[otherTeamIdx].selectedGroups.end(); ++itr)
    {
        // set correct team
        (*itr)->groupTeam = otherTeamId;

        // add team to other queue
        m_queuedGroups[bracketId][static_cast<uint8>(BG_QUEUE_NORMAL_ALLIANCE) + static_cast<uint8>(otherTeamIdx)].push_front(*itr);

        // remove team from old queue
        GroupsQueueType::iterator itr2 = itr_team;
        ++itr2;
        for (; itr2 != m_queuedGroups[bracketId][static_cast<uint8>(BG_QUEUE_NORMAL_ALLIANCE) + static_cast<uint8>(teamIdx)].end(); ++itr2)
        {
            if (*itr2 == *itr)
            {
                m_queuedGroups[bracketId][static_cast<uint8>(BG_QUEUE_NORMAL_ALLIANCE) + static_cast<uint8>(teamIdx)].erase(itr2);
                break;
            }
        }
    }
    return true;
}

/**
  Method that is called when group is inserted, or player / group is removed from BG Queue - there is only one player's status changed, so we don't use while(true) cycles to invite whole queue
  - it must be called after fully adding the members of a group to ensure group joining
  - should be called from BattleGround::RemovePlayer function in some cases
  (队列核心匹配调度函数：先向已有空位的战场补人，再依次尝试创建预组对战、常规战场/练习赛及按 ELO 分差匹配的竞技场排位赛)

  @param    bg type id
  @param    bracket id
  @param    arena type
  @param    isRated
  @param    arenaRating
*/
void BattleGroundQueueItem::Update(BattleGroundQueue& queue, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracketId, ArenaType arenaType, bool isRated, uint32 arenaRating)
{
    // if no players in queue - do nothing (各分队队列均为空则直接返回)
    if (m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].empty() &&
        m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].empty() &&
        m_queuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].empty() &&
        m_queuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].empty())
        return;

    // battleground with free slot for player should be always in the beggining of the queue
    // maybe it would be better to create bgfreeslotqueue for each bracket_id
    // (1. 优先遍历存在空位的活跃普通战场，为其补充排队玩家)
    BgFreeSlotQueueType::iterator next;
    auto& queueItems = queue.GetFreeSlotQueueItem(bgTypeId);
    for (BgFreeSlotQueueType::iterator itr = queueItems.begin(); itr != queueItems.end(); itr = next)
    {
        BattleGroundInQueueInfo& queueInfo = *itr;
        next = itr;
        ++next;
        // DO NOT allow queue manager to invite new player to arena (绝不允许向已开启的竞技场中途补人)
        if (queueInfo.IsBattleGround() && queueInfo.GetTypeId() == bgTypeId && queueInfo.GetBracketId() == bracketId &&
            queueInfo.GetStatus() > STATUS_WAIT_QUEUE && queueInfo.GetStatus() < STATUS_WAIT_LEAVE)
        {
            // and iterator is invalid

            // clear selection pools
            m_selectionPools[TEAM_INDEX_ALLIANCE].Init();
            m_selectionPools[TEAM_INDEX_HORDE].Init();

            // call a function that does the job for us
            FillPlayersToBg(queueInfo, bracketId);

            // now everything is set, invite players
            for (GroupsQueueType::const_iterator citr = m_selectionPools[TEAM_INDEX_ALLIANCE].selectedGroups.begin(); citr != m_selectionPools[TEAM_INDEX_ALLIANCE].selectedGroups.end(); ++citr)
                InviteGroupToBg(*(*citr), queueInfo, (*citr)->groupTeam);
            for (GroupsQueueType::const_iterator citr = m_selectionPools[TEAM_INDEX_HORDE].selectedGroups.begin(); citr != m_selectionPools[TEAM_INDEX_HORDE].selectedGroups.end(); ++citr)
                InviteGroupToBg(*(*citr), queueInfo, (*citr)->groupTeam);

            if (!queueInfo.HasFreeSlots())
            {
                // remove BG from BGFreeSlotQueue (已满员，移出空闲槽位队列并同步世界线程标志)
                queueItems.erase(itr);
                sWorld.GetMessager().AddMessage([instanceId = queueInfo.instanceId, typeId = queueInfo.bgTypeId](World* /*world*/)
                {
                    if (BattleGround* bg = sBattleGroundMgr.GetBattleGround(instanceId, typeId))
                    {
                        bg->RemovedFromBgFreeSlotQueue(false);
                    }
                });                
            }
        }
    }

    // finished iterating through the bgs with free slots, maybe we need to create a new bg

    BattleGround* bgTemplate = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    if (!bgTemplate)
    {
        sLog.outError("Battleground: Update: bg template not found for %u", bgTypeId);
        return;
    }

    // get the min. players per team, properly for larger arenas as well. (must have full teams for arena matches!)
    uint32 minPlayersPerTeam = bgTemplate->GetMinPlayersPerTeam();
    uint32 maxPlayersPerTeam = bgTemplate->GetMaxPlayersPerTeam();

    if (queue.IsTesting())
        minPlayersPerTeam = 1;

    if (bgTemplate->IsArena())
    {
        if (queue.IsArenaTesting())
        {
            maxPlayersPerTeam = 1;
            minPlayersPerTeam = 1;
        }
        else
        {
            // this switch can be much shorter
            maxPlayersPerTeam = arenaType;
            minPlayersPerTeam = arenaType;
            /*switch(arenaType)
            {
            case ARENA_TYPE_2v2:
                MaxPlayersPerTeam = 2;
                MinPlayersPerTeam = 2;
                break;
            case ARENA_TYPE_3v3:
                MaxPlayersPerTeam = 3;
                MinPlayersPerTeam = 3;
                break;
            case ARENA_TYPE_5v5:
                MaxPlayersPerTeam = 5;
                MinPlayersPerTeam = 5;
                break;
            }*/
        }
    }

    // 为 BATTLEGROUND_AA 随机抽取纳格兰、刀锋山或洛丹伦废墟竞技场地图
    auto pickRandomArena = [&](BattleGroundTypeId& bgTypeId, BattleGround*& bgTemplate)
    {
        BattleGroundTypeId arenas[] = { BATTLEGROUND_NA, BATTLEGROUND_BE, BATTLEGROUND_RL };
        bgTypeId = arenas[urand(0, countof(arenas) - 1)];
        bgTemplate = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
        if (!bgTemplate)
            sLog.outError("BattleGround: CreateNewBattleGround - bg template not found for %u", bgTypeId);
    };

    m_selectionPools[TEAM_INDEX_ALLIANCE].Init();
    m_selectionPools[TEAM_INDEX_HORDE].Init();

    if (bgTemplate->IsBattleGround())
    {
        // check if there is premade against premade match (2. 优先匹配预组队对预组队)
        if (CheckPremadeMatch(bracketId, minPlayersPerTeam, maxPlayersPerTeam))
        {
            BattleGround* bgTemplate = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
            if (!bgTemplate)
            {
                sLog.outError("BattleGround: CreateNewBattleGround - bg template not found for %u", bgTypeId);
                return;
            }

            BattleGroundInQueueInfo bgInfo;
            bgInfo.Fill(bgTemplate);
            bgInfo.bracketId = bracketId;
            bgInfo.instanceId = sMapMgr.GenerateInstanceId();
            bgInfo.m_clientInstanceId = queue.CreateClientVisibleInstanceId(bgTypeId, bracketId);

            // invite those selection pools
            for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
                for (GroupsQueueType::const_iterator citr = m_selectionPools[TEAM_INDEX_ALLIANCE + i].selectedGroups.begin(); citr != m_selectionPools[TEAM_INDEX_ALLIANCE + i].selectedGroups.end(); ++citr)
                    InviteGroupToBg(*(*citr), bgInfo, (*citr)->groupTeam);

            // clear structures
            m_selectionPools[TEAM_INDEX_ALLIANCE].Init();
            m_selectionPools[TEAM_INDEX_HORDE].Init();

            queue.AddBgToFreeSlots(bgInfo);

            sWorld.GetMessager().AddMessage([instanceId = bgInfo.instanceId, clientInstanceId = bgInfo.m_clientInstanceId, bgTypeId, bracketId, allianceCount = bgInfo.GetInvitedCount(ALLIANCE), hordeCount = bgInfo.GetInvitedCount(HORDE)](World* /*world*/)
            {
                // create new battleground
                BattleGround* bg2 = sBattleGroundMgr.CreateNewBattleGround(bgTypeId, bracketId, ARENA_TYPE_NONE, false, instanceId, clientInstanceId);
                MANGOS_ASSERT(bg2);
                bg2->SetInvitedCount(ALLIANCE, allianceCount);
                bg2->SetInvitedCount(HORDE, hordeCount);
                // start bg
                bg2->StartBattleGround();
            });
        }
    }

    // now check if there are in queues enough players to start new game of (normal battleground, or non-rated arena)
    // (3. 尝试开启新的常规战场或非排位竞技场练习赛)
    if (!isRated)
    {
        // if there are enough players in pools, start new battleground or non rated arena
        if (CheckNormalMatch(queue, bgTemplate, bracketId, minPlayersPerTeam, maxPlayersPerTeam)
            || (bgTemplate->IsArena() && CheckSkirmishForSameFaction(bracketId, minPlayersPerTeam)))
        {
            BattleGround* bgTemplate = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
            if (!bgTemplate)
            {
                sLog.outError("BattleGround: CreateNewBattleGround - bg template not found for %u", bgTypeId);
                return;
            }

            if (bgTypeId == BATTLEGROUND_AA)
            {
                pickRandomArena(bgTypeId, bgTemplate);
                if (bgTemplate == nullptr)
                    return;
            }

            BattleGroundInQueueInfo bgInfo;
            bgInfo.Fill(bgTemplate);
            bgInfo.bracketId = bracketId;
            bgInfo.instanceId = sMapMgr.GenerateInstanceId();
            bgInfo.m_clientInstanceId = queue.CreateClientVisibleInstanceId(bgTypeId, bracketId);

            // invite those selection pools
            for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
                for (GroupsQueueType::const_iterator citr = m_selectionPools[TEAM_INDEX_ALLIANCE + i].selectedGroups.begin(); citr != m_selectionPools[TEAM_INDEX_ALLIANCE + i].selectedGroups.end(); ++citr)
                    InviteGroupToBg(*(*citr), bgInfo, (*citr)->groupTeam);

            queue.AddBgToFreeSlots(bgInfo);

            sWorld.GetMessager().AddMessage([instanceId = bgInfo.instanceId, clientInstanceId = bgInfo.m_clientInstanceId, bgTypeId, bracketId, allianceCount = bgInfo.GetInvitedCount(ALLIANCE), hordeCount = bgInfo.GetInvitedCount(HORDE)](World* /*world*/)
            {
                // create new battleground
                BattleGround* bg2 = sBattleGroundMgr.CreateNewBattleGround(bgTypeId, bracketId, ARENA_TYPE_NONE, false, instanceId, clientInstanceId);
                MANGOS_ASSERT(bg2);
                bg2->SetInvitedCount(ALLIANCE, allianceCount);
                bg2->SetInvitedCount(HORDE, hordeCount);
                // start bg
                bg2->StartBattleGround();
            });
        }
    }
    else if (bgTemplate->IsArena())
    {
        // found out the minimum and maximum ratings the newly added team should battle against
        // arenaRating is the rating of the latest joined team, or 0
        // 0 is on (automatic update call) and we must set it to team's with longest wait time
        // (4. 排位竞技场匹配：若未指定基准分，取队首等待最久战队的等级计算合理分差窗口)
        if (!arenaRating)
        {
            GroupQueueInfo* front1 = nullptr;
            GroupQueueInfo* front2 = nullptr;
            if (!m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].empty())
            {
                front1 = m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].front();
                arenaRating = front1->arenaTeamRating;
            }

            if (!m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].empty())
            {
                front2 = m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].front();
                arenaRating = front2->arenaTeamRating;
            }

            if (front1 && front2)
            {
                if (front1->joinTime < front2->joinTime)
                    arenaRating = front1->arenaTeamRating;
            }
            else if (!front1 && !front2)
                return; // queues are empty
        }

        // set rating range
        uint32 arenaMinRating = (arenaRating <= sBattleGroundMgr.GetMaxRatingDifference()) ? 0 : arenaRating - sBattleGroundMgr.GetMaxRatingDifference();
        uint32 arenaMaxRating = arenaRating + sBattleGroundMgr.GetMaxRatingDifference();
        // if max rating difference is set and the time past since server startup is greater than the rating discard time
        // (after what time the ratings aren't taken into account when making teams) then
        // the discard time is current_time - time_to_discard, teams that joined after that, will have their ratings taken into account
        // else leave the discard time on 0, this way all ratings will be discarded
        // (超过分差放宽等待时限的队伍将无视分差限制，避免高分或低分队伍长期无人可排)
        uint32 discardTime = WorldTimer::getMSTime() - sBattleGroundMgr.GetRatingDiscardTimer();

        // we need to find 2 teams which will play next game

        GroupsQueueType::iterator itr_team[PVP_TEAM_COUNT];

        // optimalization : --- we dont need to use selection_pools - each update we select max 2 groups

        for (uint8 i = BG_QUEUE_PREMADE_ALLIANCE; i < BG_QUEUE_NORMAL_ALLIANCE; ++i)
        {
            // take the group that joined first
            itr_team[i] = m_queuedGroups[bracketId][i].begin();
            for (; itr_team[i] != m_queuedGroups[bracketId][i].end(); ++(itr_team[i]))
            {
                // if group match conditions, then add it to pool
                if (!(*itr_team[i])->isInvitedToBgInstanceGuid
                    && (((*itr_team[i])->arenaTeamRating >= arenaMinRating && (*itr_team[i])->arenaTeamRating <= arenaMaxRating)
                        || (*itr_team[i])->joinTime < discardTime))
                {
                    m_selectionPools[i].AddGroup((*itr_team[i]), maxPlayersPerTeam, 0);
                    // break for cycle to be able to start selecting another group from same faction queue
                    break;
                }
            }
        }
        // now we are done if we have 2 groups - ali vs horde!
        // if we don't have, we must try to continue search in same queue
        // tmp variables are correctly set
        // this code isn't much userfriendly - but it is supposed to continue search for mathing group in HORDE queue
        // (若跨阵营未凑齐两队，则允许同阵营部落队或联盟队内部匹配对战)
        if (m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() == 0 && m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount())
        {
            itr_team[TEAM_INDEX_ALLIANCE] = itr_team[TEAM_INDEX_HORDE];
            ++itr_team[TEAM_INDEX_ALLIANCE];

            for (; itr_team[TEAM_INDEX_ALLIANCE] != m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].end(); ++(itr_team[TEAM_INDEX_ALLIANCE]))
            {
                if (!(*itr_team[TEAM_INDEX_ALLIANCE])->isInvitedToBgInstanceGuid
                    && (((*itr_team[TEAM_INDEX_ALLIANCE])->arenaTeamRating >= arenaMinRating && (*itr_team[TEAM_INDEX_ALLIANCE])->arenaTeamRating <= arenaMaxRating)
                        || (*itr_team[TEAM_INDEX_ALLIANCE])->joinTime < discardTime))
                {
                    m_selectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*itr_team[TEAM_INDEX_ALLIANCE]), maxPlayersPerTeam, 0);
                    break;
                }
            }
        }
        // this code isn't much userfriendly - but it is supposed to continue search for mathing group in ALLIANCE queue
        if (m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount() == 0 && m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())
        {
            itr_team[TEAM_INDEX_HORDE] = itr_team[TEAM_INDEX_ALLIANCE];
            ++itr_team[TEAM_INDEX_HORDE];

            for (; itr_team[TEAM_INDEX_HORDE] != m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].end(); ++(itr_team[TEAM_INDEX_HORDE]))
            {
                if (!(*itr_team[TEAM_INDEX_HORDE])->isInvitedToBgInstanceGuid
                    && (((*itr_team[TEAM_INDEX_HORDE])->arenaTeamRating >= arenaMinRating && (*itr_team[TEAM_INDEX_HORDE])->arenaTeamRating <= arenaMaxRating)
                        || (*itr_team[TEAM_INDEX_HORDE])->joinTime < discardTime))
                {
                    m_selectionPools[TEAM_INDEX_HORDE].AddGroup((*itr_team[TEAM_INDEX_HORDE]), maxPlayersPerTeam, 0);
                    break;
                }
            }
        }

        // if we have 2 teams, then start new arena and invite players!
        if (m_selectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() && m_selectionPools[TEAM_INDEX_HORDE].GetPlayerCount())
        {
            BattleGround* bgTemplate = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
            if (!bgTemplate)
            {
                sLog.outError("BattleGround: CreateNewBattleGround - bg template not found for %u", bgTypeId);
                return;
            }

            if (bgTypeId == BATTLEGROUND_AA)
            {
                pickRandomArena(bgTypeId, bgTemplate);
                if (bgTemplate == nullptr)
                    return;
            }

            BattleGroundInQueueInfo bgInfo;
            bgInfo.Fill(bgTemplate);
            bgInfo.bracketId = bracketId;
            bgInfo.instanceId = sMapMgr.GenerateInstanceId();
            bgInfo.m_clientInstanceId = queue.CreateClientVisibleInstanceId(bgTypeId, bracketId);
            bgInfo.isRated = true;
            bgInfo.arenaType = arenaType;

            GroupQueueInfo* firstGroup = *(itr_team[TEAM_INDEX_ALLIANCE]);
            GroupQueueInfo* secondGroup = *(itr_team[TEAM_INDEX_HORDE]);

            firstGroup->opponentsTeamRating = secondGroup->arenaTeamRating;
            DEBUG_LOG("Setting oposite teamrating for team %u to %u", firstGroup->arenaTeamId, firstGroup->opponentsTeamRating);
            secondGroup->opponentsTeamRating = firstGroup->arenaTeamRating;
            DEBUG_LOG("Setting oposite teamrating for team %u to %u", secondGroup->arenaTeamId, secondGroup->opponentsTeamRating);

            // now we must move team if we changed its faction to another faction queue, because then we will spam log by errors in Queue::RemovePlayer
            // (同阵营匹配时将调整后的队伍移入对应队列槽位，确保后续 RemovePlayer 能正确定位)
            if (firstGroup->groupTeam != ALLIANCE)
            {
                // add to alliance queue
                m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].push_front(firstGroup);
                // erase from horde queue
                m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].erase(itr_team[TEAM_INDEX_ALLIANCE]);
                itr_team[TEAM_INDEX_ALLIANCE] = m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].begin();
            }

            if (secondGroup->groupTeam != HORDE)
            {
                m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].push_front(secondGroup);
                m_queuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE].erase(itr_team[TEAM_INDEX_HORDE]);
                itr_team[TEAM_INDEX_HORDE] = m_queuedGroups[bracketId][BG_QUEUE_PREMADE_HORDE].begin();
            }

            InviteGroupToBg(*firstGroup, bgInfo, ALLIANCE);
            InviteGroupToBg(*secondGroup, bgInfo, HORDE);

            queue.AddBgToFreeSlots(bgInfo);

            DEBUG_LOG("Starting rated arena match!");

            sWorld.GetMessager().AddMessage([instanceId = bgInfo.instanceId, clientInstanceId = bgInfo.m_clientInstanceId, arenaType, bgTypeId, bracketId, allianceCount = bgInfo.GetInvitedCount(ALLIANCE), hordeCount = bgInfo.GetInvitedCount(HORDE), firstTeam = firstGroup->groupTeam, firstTeamId = firstGroup->arenaTeamId, secondTeam = secondGroup->groupTeam, secondTeamId = secondGroup->arenaTeamId](World* /*world*/)
            {
                // create new battleground
                BattleGround* arena = sBattleGroundMgr.CreateNewBattleGround(bgTypeId, bracketId, arenaType, true, instanceId, clientInstanceId);
                MANGOS_ASSERT(arena);
                arena->SetInvitedCount(ALLIANCE, allianceCount);
                arena->SetInvitedCount(HORDE, hordeCount);
                arena->SetArenaTeamIdForTeam(firstTeam, firstTeamId);
                arena->SetArenaTeamIdForTeam(secondTeam, secondTeamId);
                // start bg
                arena->StartBattleGround();
            });
        }
    }
}

/*********************************************************/
/***            BATTLEGROUND QUEUE EVENTS              ***/
/*********************************************************/

/**
  Function that executes battleground queue invite event (60 秒二次提醒事件：向仍未进场的受邀玩家再次发送待进场状态包)
*/
bool BgQueueInviteEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    sWorld.GetMessager().AddMessage([event = *this](World* /*world*/)
    {
        Player* plr = sObjectMgr.GetPlayer(event.m_playerGuid);
        // player logged off (we should do nothing, he is correctly removed from queue in another procedure)
        if (!plr)
            return;

        BattleGround* bg = sBattleGroundMgr.GetBattleGround(event.m_bgInstanceGuid, event.m_bgTypeId);
        // if battleground ended and its instance deleted - do nothing
        if (!bg)
            return;

        bool bgExists = bg;
        BattleGroundTypeId bgTypeId = event.m_bgTypeId;
        uint32 bgInstanceId = bg->GetClientInstanceId();
        bool isRated = bg->IsRated();
        uint32 mapId = bg->GetMapId();

        BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BgQueueTypeId(bg->GetTypeId(), bg->GetArenaType());
        uint32 queueSlot = plr->GetBattleGroundQueueIndex(bgQueueTypeId);
        if (queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES)         // player is in queue or in battleground
        {
            sWorld.GetBGQueue().GetMessager().AddMessage([playerGuid = event.m_playerGuid, bgTypeId, bgInstanceId, isRated, mapId, removeTime = event.m_removeTime, bgQueueTypeId, queueSlot, bgExists, arenaType = event.m_arenaType](BattleGroundQueue* queue)
            {
                // check if player is invited to this bg
                BattleGroundQueueItem& bgQueue = queue->m_battleGroundQueues[bgQueueTypeId];
                if (bgQueue.IsPlayerInvited(playerGuid, bgInstanceId, removeTime))
                {
                    WorldPacket data;
                    // we must send remaining time in queue
                    BattleGroundMgr::BuildBattleGroundStatusPacket(data, bgExists, bgTypeId, bgInstanceId, isRated, mapId, queueSlot, STATUS_WAIT_JOIN, INVITE_ACCEPT_WAIT_TIME - INVITATION_REMIND_TIME, 0, arenaType, TEAM_NONE);

                    sWorld.GetMessager().AddMessage([playerGuid, bgQueueTypeId, data](World* /*world*/)
                    {
                        if (Player* plr = sObjectMgr.GetPlayer(playerGuid))
                        {
                            plr->GetSession()->SendPacket(data);
                        }
                    });
                }
            });
        }
    });
    return true;                                            // event will be deleted
}

void BgQueueInviteEvent::Abort(uint64 /*e_time*/)
{
    // do nothing
}

/**
 Function that executes battleground queue remove event (80 秒邀请超时移除事件：仅当玩家仍处于同一次未响应邀请时才将其踢出队列)
    this event has many possibilities when it is executed:
    1. player is in battleground ( he clicked enter on invitation window )
    2. player left battleground queue and he isn't there any more
    3. player left battleground queue and he joined it again and IsInvitedToBGInstanceGUID = 0
    4. player left queue and he joined again and he has been invited to same battleground again -> we should not remove him from queue yet
    5. player is invited to bg and he didn't choose what to do and timer expired - only in this condition we should call queue::RemovePlayer
    we must remove player in the 5. case even if battleground object doesn't exist!
*/
bool BgQueueRemoveEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    sWorld.GetMessager().AddMessage([event = *this](World* /*world*/)
    {
        Player* plr = sObjectMgr.GetPlayer(event.m_playerGuid);
        if (!plr)
            // player logged off (we should do nothing, he is correctly removed from queue in another procedure)
            return;

        BattleGround* bg = sBattleGroundMgr.GetBattleGround(event.m_bgInstanceGuid, event.m_bgTypeId);
        // battleground can be deleted already when we are removing queue info
        // bg pointer can be nullptr! so use it carefully!

        bool isBattleGround = bg && bg->IsBattleGround();
        bool bgExists = bg;
        BattleGroundTypeId bgTypeId = event.m_bgTypeId;
        uint32 bgInstanceId = bg ? bg->GetClientInstanceId() : 0;
        bool isRated = bg ? bg->IsRated() : false;
        uint32 mapId = bg ? bg->GetMapId() : 0;
        BattleGroundStatus bgStatus = bg ? bg->GetStatus() : STATUS_NONE;
        BattleGroundBracketId bracketId = bg ? bg->GetBracketId() : BG_BRACKET_ID_TEMPLATE;

        uint32 queueSlot = plr->GetBattleGroundQueueIndex(event.m_bgQueueTypeId);
        if (queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES)         // player is in queue, or in Battleground
        {
            sWorld.GetBGQueue().GetMessager().AddMessage([bgQueueTypeId = event.m_bgQueueTypeId, bgTypeId, bgStatus, playerGuid = event.m_playerGuid, instanceGuid = event.m_bgInstanceGuid, removeTime = event.m_removeTime, isBattleGround, queueSlot, bracketId, bgExists, bgInstanceId, isRated, mapId](BattleGroundQueue* queue)
            {
                // check if player is in queue for this BG and if we are removing his invite event
                BattleGroundQueueItem& bgQueue = queue->m_battleGroundQueues[bgQueueTypeId];
                if (bgQueue.IsPlayerInvited(playerGuid, instanceGuid, removeTime))
                {
                    DEBUG_LOG("Battleground: removing player %u from bg queue for instance %u because of not pressing enter battle in time.", playerGuid.GetCounter(), instanceGuid);

                    bgQueue.RemovePlayer(*queue, playerGuid, true);

                    // update queues if battleground isn't ended
                    if (isBattleGround && bgStatus != STATUS_WAIT_LEAVE)
                        queue->ScheduleQueueUpdate(0, ARENA_TYPE_NONE, bgQueueTypeId, bgTypeId, bracketId);

                    WorldPacket data;
                    BattleGroundMgr::BuildBattleGroundStatusPacket(data, bgExists, bgTypeId, bgInstanceId, isRated, mapId, queueSlot, STATUS_NONE, 0, 0, ARENA_TYPE_NONE, TEAM_NONE);

                    sWorld.GetMessager().AddMessage([playerGuid, bgQueueTypeId, data](World* /*world*/)
                    {
                        if (Player* plr = sObjectMgr.GetPlayer(playerGuid))
                        {
                            plr->RemoveBattleGroundQueueId(bgQueueTypeId);
                            plr->GetSession()->SendPacket(data);
                        }
                    });
                }
            });
        }
    });

    // event will be deleted
    return true;
}

void BgQueueRemoveEvent::Abort(uint64 /*e_time*/)
{
    // do nothing
}

BattleGroundQueue::BattleGroundQueue() : m_testing(false), m_arenaTesting(false)
{

}

// 战场队列独立线程主循环：处理跨线程消息、执行去重调度匹配、周期放宽竞技场分差及自动发放每周点数
void BattleGroundQueue::Update()
{
    TimePoint previously = sWorld.GetCurrentClockTime();
    InitAutomaticArenaPointDistribution();
    while (!World::IsStopped())
    {
        TimePoint now = std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::now());
        GetMessager().Execute(this);

        // update scheduled queues (执行待调度的匹配任务)
        if (!m_queueUpdateScheduler.empty())
        {
            std::vector<uint64> scheduled;
            {
                // copy vector and clear the other
                scheduled = std::vector<uint64>(m_queueUpdateScheduler);
                m_queueUpdateScheduler.clear();
            }

            for (unsigned long long i : scheduled)
            {
                uint32 arenaRating = i >> 32;
                ArenaType arenaType = ArenaType(i >> 24 & 255);
                BattleGroundQueueTypeId bgQueueTypeId = BattleGroundQueueTypeId(i >> 16 & 255);
                BattleGroundTypeId bgTypeId = BattleGroundTypeId((i >> 8) & 255);
                BattleGroundBracketId bracket_id = BattleGroundBracketId(i & 255);

                m_battleGroundQueues[bgQueueTypeId].Update(*this, bgTypeId, bracket_id, arenaType, arenaRating > 0, arenaRating);
            }
        }

        // if rating difference counts, maybe force-update queues (定时对 2v2/3v3/5v5 排位队列执行无基准分全量匹配以接纳超时队伍)
        if (sWorld.getConfig(CONFIG_UINT32_ARENA_MAX_RATING_DIFFERENCE) && sWorld.getConfig(CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER))
        {
            // it's time to force update
            if (m_nextRatingDiscardUpdate <= now)
            {
                // forced update for level 70 rated arenas
                DEBUG_LOG("BattleGroundMgr: UPDATING ARENA QUEUES");
                m_battleGroundQueues[BATTLEGROUND_QUEUE_2v2].Update(*this, BATTLEGROUND_AA, BG_BRACKET_ID_FIRST, ARENA_TYPE_2v2, true, 0);
                m_battleGroundQueues[BATTLEGROUND_QUEUE_3v3].Update(*this, BATTLEGROUND_AA, BG_BRACKET_ID_FIRST, ARENA_TYPE_3v3, true, 0);
                m_battleGroundQueues[BATTLEGROUND_QUEUE_5v5].Update(*this, BATTLEGROUND_AA, BG_BRACKET_ID_FIRST, ARENA_TYPE_5v5, true, 0);

                m_nextRatingDiscardUpdate = now + std::chrono::milliseconds(sWorld.getConfig(CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER));
            }
        }

        if (sWorld.getConfig(CONFIG_BOOL_ARENA_AUTO_DISTRIBUTE_POINTS))
        {
            if (m_nextAutoDistributionTime <= now)
            {
                sWorld.GetMessager().AddMessage([](World* /*world*/)
                {
                    sBattleGroundMgr.DistributeArenaPoints(); // TODO: Is meant to be done as battlegroup, not world (通过世界线程执行全服竞技场点数发放)
                });

                m_nextAutoDistributionTime = now + std::chrono::seconds(BATTLEGROUND_ARENA_POINT_DISTRIBUTION_DAY * sWorld.getConfig(CONFIG_UINT32_ARENA_AUTO_DISTRIBUTE_INTERVAL_DAYS));
                CharacterDatabase.PExecute("UPDATE saved_variables SET NextArenaPointDistributionTime = '" UI64FMTD "'", uint64(std::chrono::time_point_cast<std::chrono::seconds>(m_nextAutoDistributionTime).time_since_epoch().count()));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    };
}

// 从数据库加载或按配置首次初始化下次自动发放每周竞技场点数的时间
void BattleGroundQueue::InitAutomaticArenaPointDistribution()
{
    if (sWorld.getConfig(CONFIG_BOOL_ARENA_AUTO_DISTRIBUTE_POINTS))
    {
        auto queryResult = CharacterDatabase.Query("SELECT NextArenaPointDistributionTime FROM saved_variables");
        bool save = false;
        bool insert = false;
        if (!queryResult) // if not set generate time for next wednesday
            insert = true;
        else
        {
            uint64 dbTimePoint = (*queryResult)[0].GetUInt64();
            m_nextAutoDistributionTime = TimePoint(std::chrono::seconds(dbTimePoint));;
            if (dbTimePoint == 0) // uninitialized
                save = true;
            else // if time already exists - check for config changes (若修改了每日重置小时配置则同步调整时间)
            {
                time_t distrTime = std::chrono::system_clock::to_time_t(m_nextAutoDistributionTime);
                tm distribTime = *localtime(&distrTime);
                if (distribTime.tm_hour != sWorld.getConfig(CONFIG_UINT32_QUEST_DAILY_RESET_HOUR))
                {
                    TimePoint now = std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::now());
                    if (now >= m_nextAutoDistributionTime) // if it already expired, do not save and only adjust hour
                    {
                        distribTime.tm_hour = sWorld.getConfig(CONFIG_UINT32_QUEST_DAILY_RESET_HOUR);
                        m_nextAutoDistributionTime = TimePoint(std::chrono::seconds(mktime(&distribTime)));
                    }
                    else
                        save = true;
                }
            }
        }

        if (save || insert)
        {
            // generate time by config on first server launch (首次开服按配置周几与重置小时推算下次发分时间)
            time_t curTime = time(nullptr);
            tm localTm = *localtime(&curTime);
            localTm.tm_hour = sWorld.getConfig(CONFIG_UINT32_QUEST_DAILY_RESET_HOUR);
            localTm.tm_min = 0;
            localTm.tm_sec = 0;
            localTm.tm_mday += ((7 - localTm.tm_wday + sWorld.getConfig(CONFIG_UINT32_ARENA_FIRST_RESET_DAY)) % 7);
            localTm.tm_isdst = -1;
            m_nextAutoDistributionTime = TimePoint(std::chrono::seconds(mktime(&localTm)));

            uint64 distributionTime = uint64(std::chrono::time_point_cast<std::chrono::seconds>(m_nextAutoDistributionTime).time_since_epoch().count());
            if (insert)
                CharacterDatabase.PExecute("INSERT INTO saved_variables (NextArenaPointDistributionTime) VALUES ('" UI64FMTD "')", distributionTime);
            if (save)
                CharacterDatabase.PExecute("UPDATE saved_variables SET NextArenaPointDistributionTime = '" UI64FMTD "'", distributionTime);
        }
    }
}

/**
  Method that schedules queue update (将匹配参数打包为 64 位整数并去重入队等待下轮循环处理)

  @param    arena rating
  @param    arena type
  @param    battleground queue type id
  @param    battleground type id
  @param    bracket id
*/
void BattleGroundQueue::ScheduleQueueUpdate(uint32 arenaRating, ArenaType arenaType, BattleGroundQueueTypeId bgQueueTypeId, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracketId)
{
    // we will use only 1 number created of bgTypeId and bracket_id
    uint64 schedule_id = (uint64(arenaRating) << 32) | (uint64(arenaType) << 24) | (uint64(bgQueueTypeId) << 16) | (uint64(bgTypeId) << 8) | bracketId;
    bool found = false;
    for (unsigned long long i : m_queueUpdateScheduler)
    {
        if (i == schedule_id)
        {
            found = true;
            break;
        }
    }
    if (!found)
        m_queueUpdateScheduler.push_back(schedule_id);
}

void BattleGroundQueue::AddBgToFreeSlots(BattleGroundInQueueInfo const& info)
{
    auto& typeIdQueue = m_bgFreeSlotQueue[info.GetTypeId()];
    typeIdQueue.emplace_front(info);
}

void BattleGroundQueue::RemoveBgFromFreeSlots(BattleGroundTypeId typeId, uint32 instanceId)
{
    auto& typeIdQueue = m_bgFreeSlotQueue[typeId];
    for (auto itr = typeIdQueue.begin(); itr != typeIdQueue.end(); ++itr)
    {
        if (itr->GetInstanceId() == instanceId)
        {
            typeIdQueue.erase(itr);
            return;
        }
    }
}

BgFreeSlotQueueType& BattleGroundQueue::GetFreeSlotQueueItem(BattleGroundTypeId bgTypeId)
{
    return m_bgFreeSlotQueue[bgTypeId];
}

BattleGroundInQueueInfo* BattleGroundQueue::GetFreeSlotInstance(BattleGroundTypeId bgTypeId, uint32 instanceId)
{
    auto& queueItem = GetFreeSlotQueueItem(bgTypeId);
    auto itr = std::find_if(queueItem.begin(), queueItem.end(), [instanceId](BattleGroundInQueueInfo const& bgInQueue)
    {
        return bgInQueue.instanceId == instanceId;
    });
    if (itr == queueItem.end())
        return nullptr;
    return &(*itr);
}

BattleGroundQueueItem& BattleGroundQueue::GetBattleGroundQueue(BattleGroundQueueTypeId bgQueueTypeId)
{
    return m_battleGroundQueues[bgQueueTypeId];
}

void BattleGroundQueue::SetNextRatingDiscardUpdate(std::chrono::milliseconds timePoint)
{
    TimePoint now = std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::now());;
    m_nextRatingDiscardUpdate = now + timePoint;
}

/**
  Method that builds battleground list data (构建战场军官界面展示的可用战场实例列表数据包 SMSG_BATTLEFIELD_LIST)

  @param    packet
  @param    battlemaster guid
  @param    player
  @param    battleground type id
*/
void BattleGroundQueue::BuildBattleGroundListPacket(WorldPacket& data, ObjectGuid guid, uint32 playerLevel, BattleGroundTypeId bgTypeId) const
{
    data.Initialize(SMSG_BATTLEFIELD_LIST);
    data << guid;                                          // battlemaster guid
    data << uint32(bgTypeId);                              // battleground id
    if (bgTypeId == BATTLEGROUND_AA)                        // arena
    {
        data << uint8(5);                                  // unk
        data << uint32(0);                                 // unk
    }
    else                                                    // battleground
    {
        data << uint8(0x00);                               // unk

        size_t count_pos = data.wpos();
        uint32 count = 0;
        data << uint32(0);                                 // number of bg instances

        uint32 bracket_id = sBattleGroundMgr.GetBattleGroundBracketIdFromLevel(bgTypeId, playerLevel);
        ClientBattleGroundIdSet const& ids = m_clientBattleGroundIds[bgTypeId][bracket_id];
        for (std::set<uint32>::const_iterator itr = ids.begin(); itr != ids.end(); ++itr)
        {
            data << uint32(*itr);
            ++count;
        }
        data.put<uint32>(count_pos, count);
    }
}

/**
  Function that returns client instance id from battleground type id and bracket id
  (为普通战场分配从 1 开始的最小未占用客户端可见编号，竞技场固定返回 0)

  @param    battleground type id
  @param    bracket id
*/
uint32 BattleGroundQueue::CreateClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracketId)
{
    if (BattleGroundMgr::IsArenaType(bgTypeId))
        return 0;                                           // arenas don't have client-instanceids

    // we create here an instanceid, which is just for
    // displaying this to the client and without any other use..
    // the client-instanceIds are unique for each battleground-type
    // the instance-id just needs to be as low as possible, beginning with 1
    // the following works, because std::set is default ordered with "<"
    // the optimalization would be to use as bitmask std::vector<uint32> - but that would only make code unreadable

    uint32 lastId = 0;
    ClientBattleGroundIdSet& ids = m_clientBattleGroundIds[bgTypeId][bracketId];
    for (ClientBattleGroundIdSet::const_iterator itr = ids.begin(); itr != ids.end();)
    {
        if ((++lastId) != *itr)                             // if there is a gap between the ids, we will break..
            break;
        lastId = *itr;
    }
    ids.insert(lastId + 1);

    return lastId + 1;
}

void BattleGroundQueue::RemovePlayer(BattleGroundQueueTypeId bgQueueTypeId, ObjectGuid player, bool decreaseInvitedCount)
{
    m_battleGroundQueues[bgQueueTypeId].RemovePlayer(*this, player, decreaseInvitedCount);
}

// 递减已邀请计数并异步同步回世界线程中的 BattleGround 实例
void BattleGroundInQueueInfo::DecreaseInvitedCount(Team team)
{
    uint32 count = (team == ALLIANCE) ? --m_invitedAlliance : --m_invitedHorde;
    sWorld.GetMessager().AddMessage([bgTypeId = GetTypeId(), instanceId = GetInstanceId(), team, count](World* /*world*/)
    {
        if (BattleGround* bg = sBattleGroundMgr.GetBattleGround(instanceId, bgTypeId))
            bg->SetInvitedCount(team, count);
    });
}

// 递增已邀请计数并异步同步回世界线程中的 BattleGround 实例
void BattleGroundInQueueInfo::IncreaseInvitedCount(Team team)
{
    uint32 count = (team == ALLIANCE) ? ++m_invitedAlliance : ++m_invitedHorde;
    sWorld.GetMessager().AddMessage([bgTypeId = GetTypeId(), instanceId = GetInstanceId(), team, count](World* /*world*/)
    {
        if (BattleGround* bg = sBattleGroundMgr.GetBattleGround(instanceId, bgTypeId))
            bg->SetInvitedCount(team, count);
    });
}

// 从 BattleGround 实例拷贝运行时状态到队列线程快照结构
void BattleGroundInQueueInfo::Fill(BattleGround* bg)
{
    bgTypeId = bg->GetTypeId();
    instanceId = bg->GetInstanceId();
    isBattleGround = bg->IsBattleGround();
    bracketId = bg->GetBracketId();
    status = bg->GetStatus();
    m_clientInstanceId = bg->GetClientInstanceId();
    mapId = bg->GetMapId();
    arenaType = bg->GetArenaType();
    isRated = bg->IsRated();
    playersInside = bg->GetPlayersSize();
    maxPlayers = bg->GetMaxPlayers();
    m_invitedAlliance = bg->GetInvitedCount(ALLIANCE);
    m_invitedHorde = bg->GetInvitedCount(HORDE);
    m_maxPlayersPerTeam = bg->GetMaxPlayersPerTeam();
    m_minPlayersPerTeam = bg->GetMinPlayersPerTeam();
}
