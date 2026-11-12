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

#include "Common.h"
#include "Globals/SharedDefines.h"
#include "Server/WorldPacket.h"
#include "Server/Opcodes.h"
#include "Log/Log.h"
#include "Entities/Player.h"
#include "Globals/ObjectMgr.h"
#include "Server/WorldSession.h"
#include "Entities/Object.h"
#include "BattleGroundEY.h"
#include "BattleGroundMgr.h"
#include "BattleGroundWS.h"
#include "BattleGround.h"
#include "Arena/ArenaTeam.h"
#include "Tools/Language.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "World/World.h"

// Sent by client when player talk to the battle master (处理玩家与战场军官对话请求 CMSG_BATTLEMASTER_HELLO)
void WorldSession::HandleBattlemasterHelloOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    DEBUG_LOG("WORLD: Received opcode CMSG_BATTLEMASTER_HELLO from %s", guid.GetString().c_str());

    Creature* pCreature = GetPlayer()->GetMap()->GetCreature(guid);

    if (!pCreature)
        return;

    if (!pCreature->isBattleMaster())                       // it's not battlemaster (非战场军官 NPC)
        return;

    // Stop the npc if moving (交互时暂停 NPC 巡逻移动)
    if (uint32 pauseTimer = pCreature->GetInteractionPauseTimer())
        pCreature->GetMotionMaster()->PauseWaypoints(pauseTimer);

    BattleGroundTypeId bgTypeId = GetPlayer()->GetMap()->GetMapDataContainer().GetBattleMasterBG(pCreature->GetEntry());

    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
        return;

    if (!_player->GetBGAccessByLevel(bgTypeId))
    {
        // temp, must be gossip message... (等级不足，返回提示通知)
        SendNotification(LANG_YOUR_BG_LEVEL_REQ_ERROR);
        return;
    }

    SendBattleGroundList(guid, bgTypeId);
}

/**
Send battleground list
(投递消息到战场队列线程构建可用战场列表数据包，再切回世界线程发给客户端)

@param   guid
@param   bgTypeId
*/
void WorldSession::SendBattleGroundList(ObjectGuid guid, BattleGroundTypeId bgTypeId) const
{
    sWorld.GetBGQueue().GetMessager().AddMessage([playerGuid = _player->GetObjectGuid(), masterGuid = guid, playerLevel = _player->GetLevel(), bgTypeId](BattleGroundQueue* queue)
    {
        WorldPacket data;
        queue->BuildBattleGroundListPacket(data, masterGuid, playerLevel, BattleGroundTypeId(bgTypeId));
        sWorld.GetMessager().AddMessage([playerGuid, data](World* /*world*/)
        {
            if (Player* player = sObjectMgr.GetPlayer(playerGuid))
                player->GetSession()->SendPacket(data);
        });
    });
}

// Sent by client when player wants to join a battleground (处理玩家单排或组队排入战场队列 CMSG_BATTLEMASTER_JOIN)
void WorldSession::HandleBattlemasterJoinOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 receivedBgTypeId;
    uint32 instanceId;
    uint8 joinAsGroup;
    bool isPremade = false;
    Group* group = nullptr;
    uint32 mapId = 0;

    recv_data >> guid;                                      // battlemaster guid (战场军官 GUID)
    recv_data >> receivedBgTypeId;                          // battleground type id (DBC id) (战场类型 ID)
    recv_data >> instanceId;                                // instance id, 0 if First Available selected (指定实例编号，0 表示首个可用)
    recv_data >> joinAsGroup;                               // join as group (是否作为队伍排队)

    if (!sBattlemasterListStore.LookupEntry(receivedBgTypeId))
    {
        sLog.outError("Battleground: invalid bgtype (%u) received. possible cheater? player guid %u", receivedBgTypeId, _player->GetGUIDLow());
        return;
    }

    BattleGroundTypeId bgTypeId = BattleGroundTypeId(receivedBgTypeId);

    DEBUG_LOG("WORLD: Received opcode CMSG_BATTLEMASTER_JOIN from %s", guid.GetString().c_str());

    // can do this, since it's battleground, not arena (普通战场仅使用 ARENA_TYPE_NONE 映射队列类型)
    BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BgQueueTypeId(bgTypeId, ARENA_TYPE_NONE);

    // ignore if player is already in BG (已在战场中则忽略)
    if (_player->InBattleGround())
        return;

    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
        return;

    if (!unit->isBattleMaster())                            // it's not battlemaster (校验交互对象确为战场军官)
        return;

    // get bg instance or bg template if instance not found (优先查找指定战场实例，否则回退到战场模板)
    BattleGround* bg = nullptr;
    if (instanceId)
        bg = sBattleGroundMgr.GetBattleGroundThroughClientInstance(instanceId, bgTypeId);

    if (!bg)
        bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);

    if (!bg)
    {
        sLog.outError("Battleground: no available bg / template found");
        return;
    }

    mapId = bg->GetMapId();

    BattleGroundBracketId bgBracketId = sBattleGroundMgr.GetBattleGroundBracketIdFromLevel(bgTypeId, _player->GetLevel());

    // check queue conditions (校验逃兵减益、重复排队与队列槽位上限)
    if (!joinAsGroup)
    {
        // check Deserter debuff (检查逃兵状态)
        if (!_player->CanJoinToBattleground())
        {
            WorldPacket data;
            sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(data, bgTypeId, BG_GROUP_JOIN_STATUS_DESERTERS);
            _player->GetSession()->SendPacket(data);
            return;
        }
        // check if already in queue (已在该战场队列中则忽略)
        if (_player->GetBattleGroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
            // player is already in this queue
            return;
        // check if has free queue slots (检查是否还有空闲排队槽位)
        if (!_player->HasFreeBattleGroundQueueId())
        {
            WorldPacket data;
            sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(data, bgTypeId, BG_GROUP_JOIN_STATUS_TOO_MANY_QUEUES);
            _player->GetSession()->SendPacket(data);
            return;
        }
    }
    else
    {
        group = _player->GetGroup();
        // no group found, error (组队排队但当前无队伍)
        if (!group)
            return;
        uint32 err = group->CanJoinBattleGroundQueue(bgTypeId, bgQueueTypeId, 0, bg->GetMaxPlayersPerTeam(), false, 0);
        isPremade = sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH) &&
                    (group->GetMembersCount() >= bg->GetMinPlayersPerTeam());
        if (err != BG_JOIN_ERR_OK)
        {
            SendBattleGroundOrArenaJoinError(err);
            return;
        }
    }
    // if we're here, then the conditions to join a bg are met. We can proceed in joining.
    // (全部前置校验通过，打包成员信息投递至队列线程完成入队与匹配调度)

    // _player->GetGroup() was already checked, group is already initialized
    AddGroupToQueueInfo info;
    info.team = _player->GetTeam();
    info.clientInstanceId = instanceId;
    info.mapId = mapId;
    if (joinAsGroup)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            if (Player* member = itr->getSource())
                info.members.push_back(member->GetObjectGuid());
        DEBUG_LOG("Battleground: the following players are joining as group:");
        sWorld.GetBGQueue().GetMessager().AddMessage([bgQueueTypeId, leaderGuid = group->GetLeaderGuid(), info, bgTypeId, bgBracketId, isPremade, instanceId, mapId](BattleGroundQueue* queue)
        {
            BattleGroundQueueItem& queueItem = queue->GetBattleGroundQueue(bgQueueTypeId);
            GroupQueueInfo* groupInfo = queueItem.AddGroup(leaderGuid, info, bgTypeId, bgBracketId, ARENA_TYPE_NONE, false, isPremade, instanceId, 0);
            uint32 avgTime = queueItem.GetAverageQueueWaitTime(groupInfo, bgBracketId);

            sWorld.GetMessager().AddMessage([leaderGuid, members = info.members, bgQueueTypeId, bgTypeId, bgClientInstanceId = instanceId, avgTime, arenaType = groupInfo->arenaType, isRated = groupInfo->isRated, mapId](World* /*world*/)
            {
                Player* leader = sObjectMgr.GetPlayer(leaderGuid);
                for (ObjectGuid guid : members)
                {
                    Player* member = sObjectMgr.GetPlayer(guid);
                    if (!member)
                        continue;

                    uint32 queueSlot = member->AddBattleGroundQueueId(bgQueueTypeId);           // add to queue (分配队列槽位)

                    // store entry point coords (same as leader entry point) (记录进入前坐标)
                    member->SetBattleGroundEntryPoint(leader);

                    // send status packet (in queue) (发送排队状态及入队成功包)
                    WorldPacket data;
                    sBattleGroundMgr.BuildBattleGroundStatusPacket(data, true, bgTypeId, bgClientInstanceId, isRated, mapId, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, arenaType, TEAM_NONE);
                    member->GetSession()->SendPacket(data);
                    sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(data, bgTypeId, BG_GROUP_JOIN_STATUS_SUCCESS);
                    member->GetSession()->SendPacket(data);
                    DEBUG_LOG("Battleground: player joined queue for bg queue type %u bg type %u: GUID %u, NAME %s", bgQueueTypeId, bgTypeId, member->GetGUIDLow(), member->GetName());
                }

                DEBUG_LOG("Battleground: group end");
            });
            queue->ScheduleQueueUpdate(0, ARENA_TYPE_NONE, bgQueueTypeId, bgTypeId, bgBracketId);
        });
    }
    else
    {
        sWorld.GetBGQueue().GetMessager().AddMessage([bgQueueTypeId, playerGuid = _player->GetObjectGuid(), info, bgTypeId, bgBracketId, isPremade, instanceId, mapId](BattleGroundQueue* queue)
        {
            BattleGroundQueueItem& queueItem = queue->GetBattleGroundQueue(bgQueueTypeId);
            GroupQueueInfo* groupInfo = queueItem.AddGroup(playerGuid, info, bgTypeId, bgBracketId, ARENA_TYPE_NONE, false, isPremade, instanceId, 0);
            uint32 avgTime = queueItem.GetAverageQueueWaitTime(groupInfo, bgBracketId);
            sWorld.GetMessager().AddMessage([playerGuid, bgQueueTypeId, bgTypeId, bgClientInstanceId = instanceId, avgTime, arenaType = groupInfo->arenaType, isRated = groupInfo->isRated, mapId](World* /*world*/)
            {
                if (Player* player = sObjectMgr.GetPlayer(playerGuid))
                {
                    // already checked if queueSlot is valid, now just get it (分配队列槽位)
                    uint32 queueSlot = player->AddBattleGroundQueueId(bgQueueTypeId);
                    // store entry point coords (保存进场前位置以便退出传回)
                    player->SetBattleGroundEntryPoint();

                    WorldPacket data;
                    // send status packet (in queue) (下发等待队列状态包)
                    sBattleGroundMgr.BuildBattleGroundStatusPacket(data, true, bgTypeId, bgClientInstanceId, isRated, mapId, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, arenaType, TEAM_NONE);
                    player->GetSession()->SendPacket(data);
                    DEBUG_LOG("Battleground: player joined queue for bg queue type %u bg type %u: GUID %u, NAME %s", bgQueueTypeId, bgTypeId, player->GetGUIDLow(), player->GetName());
                }
            });
            queue->ScheduleQueueUpdate(0, ARENA_TYPE_NONE, bgQueueTypeId, bgTypeId, bgBracketId);
        });
    }
}

// Sent by client while inside battleground; depends on the battleground type (响应战场内持旗者位置查询 MSG_BATTLEGROUND_PLAYER_POSITIONS)
void WorldSession::HandleBattleGroundPlayerPositionsOpcode(WorldPacket& /*recv_data*/)
{
    // empty opcode (无请求负载)
    DEBUG_LOG("WORLD: Received opcode MSG_BATTLEGROUND_PLAYER_POSITIONS");

    BattleGround* bg = _player->GetBattleGround();
    if (!bg)                                                // can't be received if player not in battleground (玩家不在战场内则忽略)
        return;

    switch (bg->GetTypeId())
    {
        case BATTLEGROUND_WS:
        {
            uint32 flagCarrierCount = 0;

            Player* flagCarrierAlliance = sObjectMgr.GetPlayer(static_cast<BattleGroundWS*>(bg)->GetFlagCarrierGuid(TEAM_INDEX_ALLIANCE));
            if (flagCarrierAlliance)
            {
                if (flagCarrierAlliance->GetTeam() == _player->GetTeam() || static_cast<BattleGroundWS*>(bg)->IsFlagHeldFor45Seconds(flagCarrierAlliance->GetTeam()))
                    ++flagCarrierCount;
            }

            Player* flagCarrierHorde = sObjectMgr.GetPlayer(static_cast<BattleGroundWS*>(bg)->GetFlagCarrierGuid(TEAM_INDEX_HORDE));
            if (flagCarrierHorde)
            {
                if (flagCarrierHorde->GetTeam() == _player->GetTeam() || static_cast<BattleGroundWS*>(bg)->IsFlagHeldFor45Seconds(flagCarrierHorde->GetTeam()))
                    ++flagCarrierCount;
            }

            WorldPacket data(MSG_BATTLEGROUND_PLAYER_POSITIONS, 4 + 4 + 16 * flagCarrierCount);
            data << uint32(0); // own team position XYZ (本方普通队员位置列表数量，此处为 0)
            data << uint32(flagCarrierCount);

            if (flagCarrierAlliance)
            {
                data << flagCarrierAlliance->GetObjectGuid();
                data << float(flagCarrierAlliance->GetPositionX());
                data << float(flagCarrierAlliance->GetPositionY());
            }
            if (flagCarrierHorde)
            {
                data << flagCarrierHorde->GetObjectGuid();
                data << float(flagCarrierHorde->GetPositionX());
                data << float(flagCarrierHorde->GetPositionY());
            }

            SendPacket(data);
            break;
        }
        case BATTLEGROUND_EY:
        {
            uint32 flagCarrierCount = 0;

            Player* flagCarrier = sObjectMgr.GetPlayer(((BattleGroundEY*)bg)->GetFlagCarrierGuid());
            if (flagCarrier)
                flagCarrierCount = 1;

            WorldPacket data(MSG_BATTLEGROUND_PLAYER_POSITIONS, 4 + 4 + 16 * flagCarrierCount);
            data << uint32(0);
            data << uint32(flagCarrierCount);

            if (flagCarrier)
            {
                data << flagCarrier->GetObjectGuid();
                data << float(flagCarrier->GetPositionX());
                data << float(flagCarrier->GetPositionY());
            }

            SendPacket(data);
            break;
        }
        case BATTLEGROUND_AB:
        case BATTLEGROUND_AV:
        {
            // for other BG types - send default (阿拉希与奥山不展示特殊旗手坐标，返回空列表)
            WorldPacket data(MSG_BATTLEGROUND_PLAYER_POSITIONS, 4 + 4);
            data << uint32(0);
            data << uint32(0);
            SendPacket(data);
            break;
        }
        default:
            // maybe it is sent also in arena - do nothing (竞技场忽略)
            break;
    }
}

// Sent by client when requesting the PvP match log (处理客户端打开战场记分牌请求 MSG_PVP_LOG_DATA)
void WorldSession::HandlePVPLogDataOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode MSG_PVP_LOG_DATA");

    BattleGround* bg = _player->GetBattleGround();
    if (!bg)
        return;

    // arena finish version will send in BattleGround::EndBattleGround directly (竞技场记分板仅在比赛结束时主动发送)
    if (bg->IsArena())
        return;

    WorldPacket data;
    sBattleGroundMgr.BuildPvpLogDataPacket(data, bg);
    SendPacket(data);

    DEBUG_LOG("WORLD: Sent MSG_PVP_LOG_DATA Message");
}

// Sent by client when requesting available battleground list (处理客户端请求刷新战场列表 CMSG_BATTLEFIELD_LIST)
void WorldSession::HandleBattlefieldListOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BATTLEFIELD_LIST");

    uint32 bgTypeId;
    recv_data >> bgTypeId;                                  // id from DBC (战场 DBC 类型 ID)

    BattlemasterListEntry const* bl = sBattlemasterListStore.LookupEntry(bgTypeId);
    if (!bl)
    {
        sLog.outError("Battleground: invalid bgtype received.");
        return;
    }
   
    sWorld.GetBGQueue().GetMessager().AddMessage([playerGuid = _player->GetObjectGuid(), masterGuid = _player->GetObjectGuid(), playerLevel = _player->GetLevel(), bgTypeId](BattleGroundQueue* queue)
    {
        WorldPacket data;
        queue->BuildBattleGroundListPacket(data, masterGuid, playerLevel, BattleGroundTypeId(bgTypeId));
        sWorld.GetMessager().AddMessage([playerGuid, data](World* /*world*/)
        {
            if (Player* player = sObjectMgr.GetPlayer(playerGuid))
                player->GetSession()->SendPacket(data);
        });
    });
}

// Sent by client when requesting teleport to the battleground location (处理玩家接受进场传送或主动退出排队 CMSG_BATTLEFIELD_PORT)
void WorldSession::HandleBattlefieldPortOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BATTLEFIELD_PORT");

    uint8 type;                                             // arenatype if arena (竞技场规模：2/3/5，普通战场为 0)
    uint8 unk2;                                             // unk, can be 0x0 (may be if was invited?) and 0x1 (未知标记)
    uint32 receivedBgTypeId;                                // type id from dbc (战场类型 DBC ID)
    uint16 unk;                                             // 0x1F90 constant? (未知常量)
    uint8 action;                                           // enter battle 0x1, leave queue 0x0 (动作：1=传送进入战场，0=取消排队)

    recv_data >> type >> unk2 >> receivedBgTypeId >> unk >> action;

    if (action != 0 && action != 1)
    {
        sLog.outError("BattlegroundHandler: invalid action (%u) received.", action);
        return;
    }

    if (!sBattlemasterListStore.LookupEntry(receivedBgTypeId))
    {
        sLog.outError("BattlegroundHandler: invalid bgtype (%u) received.", receivedBgTypeId);
        return;
    }

    if (type && !IsArenaTypeValid(ArenaType(type)))
    {
        sLog.outError("BattlegroundHandler: Invalid CMSG_BATTLEFIELD_PORT received from player (%u), arena type wrong: %u.", _player->GetGUIDLow(), type);
        return;
    }

    if (!_player->InBattleGroundQueue())
    {
        sLog.outError("BattlegroundHandler: Invalid CMSG_BATTLEFIELD_PORT received from player (%u), he is not in bg_queue.", _player->GetGUIDLow());
        return;
    }

    // get GroupQueueInfo from BattleGroundQueue (解析目标队列类型及玩家槽位并投递到队列线程执行)
    BattleGroundTypeId bgTypeId = BattleGroundTypeId(receivedBgTypeId);
    BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BgQueueTypeId(bgTypeId, ArenaType(type));
    bool canJoinToBg = _player->CanJoinToBattleground();
    uint32 queueSlot = _player->GetBattleGroundQueueIndex(bgQueueTypeId);

    if (queueSlot == PLAYER_MAX_BATTLEGROUND_QUEUES) // tried to join a bg not in queue for (未在该队列中)
        return;

    sWorld.GetBGQueue().GetMessager().AddMessage([bgQueueTypeId, playerGuid = _player->GetObjectGuid(), actionTemp = action, canJoinToBg, bgTypeId, playerLevel = _player->GetLevel(), queueSlot](BattleGroundQueue* queue)
    {
        uint8 action = actionTemp;
        BattleGroundQueueItem& queueItem = queue->GetBattleGroundQueue(bgQueueTypeId);
        GroupQueueInfo queueInfo;
        if (!queueItem.GetPlayerGroupInfoData(playerGuid, &queueInfo))
        {
            sLog.outError("BattlegroundHandler: itrplayerstatus not found.");
            return;
        }

        if (!queueInfo.isInvitedToBgInstanceGuid && action == 1)
        {
            sLog.outError("BattlegroundHandler: instance not found.");
            return;
        }

        // some checks if player isn't cheating - it is not exactly cheating, but we cannot allow it
        if (action == 1 && queueInfo.arenaType == ARENA_TYPE_NONE)
        {
            // if player is trying to enter battleground (not arena!) and he has deserter debuff, we must just remove him from queue
            // (若玩家携带逃兵减益尝试进入普通战场，则拒绝传送并将其移出队列)
            if (!canJoinToBg)
            {
                // send bg command result to show nice message
                WorldPacket data2;
                sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(data2, bgTypeId, BG_GROUP_JOIN_STATUS_DESERTERS);
                sWorld.GetMessager().AddMessage([playerGuid, data2](World* /*world*/)
                {
                    if (Player* player = sObjectMgr.GetPlayer(playerGuid))
                    {
                        player->GetSession()->SendPacket(data2);
                    }
                });

                action = 0;
            }
        }

        switch (action)
        {
            case 1:                                         // port to battleground (确认进场：移除队列记录并切回世界线程传送)
            {
                BattleGroundInQueueInfo* bgInQueue = queue->GetFreeSlotInstance(bgTypeId, queueInfo.isInvitedToBgInstanceGuid);
                MANGOS_ASSERT(bgInQueue); // at this point must always exist

                // remove battleground queue status from BGmgr (从排队队列移除该玩家但不减少已邀请计数)
                queueItem.RemovePlayer(*queue, playerGuid, false);

                sWorld.GetMessager().AddMessage([playerGuid, invitedTo = queueInfo.isInvitedToBgInstanceGuid, bgTypeId, bgQueueTypeId, groupTeam = queueInfo.groupTeam, queueSlot, bgClientInstanceId = bgInQueue->GetClientInstanceId(), isRated = bgInQueue->IsRated(), mapId = bgInQueue->GetMapId(), arenaType = bgInQueue->GetArenaType()](World* /*world*/)
                {
                    Player* player = sObjectMgr.GetPlayer(playerGuid);
                    if (!player)
                        return;

                    // resurrect the player (进场前若死亡则自动复活)
                    if (!player->IsAlive())
                    {
                        player->ResurrectPlayer(1.0f);
                        player->SpawnCorpseBones();
                    }

                    player->TaxiFlightInterrupt();

                    uint32 startTime = 0;
                    if (BattleGround* bg = sBattleGroundMgr.GetBattleGround(invitedTo, bgTypeId))
                        startTime = bg->GetStartTime();

                    WorldPacket data;
                    sBattleGroundMgr.BuildBattleGroundStatusPacket(data, true, bgTypeId, bgClientInstanceId, isRated, mapId, queueSlot, STATUS_IN_PROGRESS, 0, startTime, arenaType, player->GetBGTeam());
                    player->GetSession()->SendPacket(data);

                    // this is still needed here if battleground "jumping" shouldn't add deserter debuff
                    // also this is required to prevent stuck at old battleground after SetBattleGroundId set to new
                    // (若玩家正处于另一战场中跳转，须先从原战场干净退出)
                    if (BattleGround* currentBg = player->GetBattleGround())
                        currentBg->RemovePlayerAtLeave(player->GetObjectGuid(), false, true);

                    // set the destination instance id (设置目标战场实例 ID 与战场内所属阵营)
                    player->SetBattleGroundId(invitedTo, bgTypeId);
                    // set the destination team
                    player->SetBGTeam(groupTeam);

                    sBattleGroundMgr.SendToBattleGround(player, invitedTo, bgTypeId);

                    DEBUG_LOG("Battleground: player %s (%u) joined battle for bg %u, bgtype %u, queue type %u.", player->GetName(), player->GetGUIDLow(), invitedTo, bgTypeId, bgQueueTypeId);
                });

                break;
            }
            case 0:                                         // leave queue (取消/退出排队)
                // if player leaves rated arena match before match start, it is counted as he played but he lost
                // (若已受邀进入竞技场排位赛但中途退队，则按战败扣除个人等级)
                if (queueInfo.isRated && queueInfo.isInvitedToBgInstanceGuid)
                {
                    sWorld.GetMessager().AddMessage([arenaTeamId = queueInfo.arenaTeamId, playerGuid, opponentRating = queueInfo.opponentsTeamRating](World* /*world*/)
                    {
                        ArenaTeam* at = sObjectMgr.GetArenaTeamById(arenaTeamId);
                        if (at)
                        {
                            Player* player = sObjectMgr.GetPlayer(playerGuid);
                            if (player)
                                at->MemberLost(player, opponentRating);
                            else
                                at->OfflineMemberLost(playerGuid, opponentRating);
                            at->SaveToDB();
                        }
                    });
                }

                queueItem.RemovePlayer(*queue, playerGuid, true);
                sWorld.GetMessager().AddMessage([playerGuid, bgQueueTypeId, queueSlot, bgTypeId, bgClientInstanceId = queueInfo.clientInstanceId, isRated = queueInfo.isRated, mapId = queueInfo.mapId](World* /*world*/)
                {
                    Player* player = sObjectMgr.GetPlayer(playerGuid);
                    if (!player)
                        return;
                    player->RemoveBattleGroundQueueId(bgQueueTypeId);
                    WorldPacket data;
                    sBattleGroundMgr.BuildBattleGroundStatusPacket(data, true, bgTypeId, bgClientInstanceId, isRated, mapId, queueSlot, STATUS_NONE, 0, 0, ARENA_TYPE_NONE, TEAM_NONE);
                    player->GetSession()->SendPacket(data);
                });

                if (queueInfo.arenaType == ARENA_TYPE_NONE)
                    queue->ScheduleQueueUpdate(queueInfo.arenaTeamRating, queueInfo.arenaType, bgQueueTypeId, bgTypeId, queueInfo.bgBracketId);
                break;
        }
    });
}

// Sent by client when leaving the battleground (处理玩家手动离开战场/竞技场 CMSG_LEAVE_BATTLEFIELD)
void WorldSession::HandleLeaveBattlefieldOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LEAVE_BATTLEFIELD");
    uint64 guid;

    // Essentially a BG guid - uint64 (读取战场 GUID)
    recv_data >> guid;

    uint32 bgTypeId = (guid & 0x0000FFFFFFFF0000) >> 16; // TODO: Test

    if (bgTypeId >= MAX_BATTLEGROUND_TYPE_ID)               // cheating - but not important in this case
        return;

    // not allow leave battleground in combat (战斗进行中禁止直接离开战场，结束阶段除外)
    if (_player->IsInCombat())
        if (BattleGround* bg = _player->GetBattleGround())
            if (bg->GetStatus() != STATUS_WAIT_LEAVE)
                return;

    _player->LeaveBattleground();
}

// Sent by client when requesting battleground status (处理客户端查询当前排队及战场状态 CMSG_BATTLEFIELD_STATUS)
void WorldSession::HandleBattlefieldStatusOpcode(WorldPacket& /*recv_data*/)
{
    // empty opcode
    DEBUG_LOG("WORLD: Battleground status");

    WorldPacket data;
    // we must update all queues here (遍历玩家全部队列槽位，同步已进场实例状态并向队列线程查询未进场槽位状态)
    BattleGround* bg;
    std::vector<std::pair<uint32, BattleGroundQueueTypeId>> idsToCheck;
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        BattleGroundQueueTypeId bgQueueTypeId = _player->GetBattleGroundQueueTypeId(i);
        if (!bgQueueTypeId)
            continue;

        BattleGroundTypeId bgTypeId = BattleGroundMgr::BgTemplateId(bgQueueTypeId);
        ArenaType arenaType = BattleGroundMgr::BgArenaType(bgQueueTypeId);

        // bgTypeId is always BATTLEGROUND_AA for arena whereas GetBattleGroundTypeId() is the actual arena map type
        // (竞技场在队列中的模板类型为 BATTLEGROUND_AA，而实际地图实例类型为具体竞技场地图)
        if (bgTypeId == _player->GetBattleGroundTypeId() || (bgTypeId == BATTLEGROUND_AA && sBattleGroundMgr.IsArenaType(_player->GetBattleGroundTypeId())))
        {
            bg = _player->GetBattleGround();
            // i cannot check any variable from player class because player class doesn't know if player is in 2v2 / 3v3 or 5v5 arena
            // so i must use bg pointer to get that information
            if (bg && bg->GetArenaType() == arenaType)
            {
                // this line is checked, i only don't know if GetStartTime is changing itself after bg end!
                // send status in BattleGround (发送进行中战场状态)
                sBattleGroundMgr.BuildBattleGroundStatusPacket(data, bg, bg->GetTypeId(), bg->GetClientInstanceId(), bg->IsRated(), bg->GetMapId(), i, STATUS_IN_PROGRESS, bg->GetEndTime(), bg->GetStartTime(), arenaType, _player->GetBGTeam());
                SendPacket(data);
                continue;
            }
        }

        idsToCheck.push_back({i, bgQueueTypeId});
        // we are sending update to player about queue - he can be invited there!
    }

    sWorld.GetBGQueue().GetMessager().AddMessage([idsToCheck, playerGuid = _player->GetObjectGuid(), playerLevel = _player->GetLevel()](BattleGroundQueue* queue)
    {
        for (auto [queueSlot, bgQueueTypeId] : idsToCheck)
        {
            BattleGroundQueueItem& queueItem = queue->GetBattleGroundQueue(bgQueueTypeId);
            GroupQueueInfo queueInfo;
            if (!queueItem.GetPlayerGroupInfoData(playerGuid, &queueInfo))
                continue;

            WorldPacket data;
            if (queueInfo.isInvitedToBgInstanceGuid)
            {
                uint32 remainingTime = WorldTimer::getMSTimeDiff(WorldTimer::getMSTime(), queueInfo.removeInviteTime);
                // send status invited to BattleGround (发送已受邀待进场状态)
                sBattleGroundMgr.BuildBattleGroundStatusPacket(data, true, queueInfo.bgTypeId, queueInfo.clientInstanceId, queueInfo.isRated, queueInfo.mapId, queueSlot, STATUS_WAIT_JOIN, remainingTime, 0, queueInfo.arenaType, TEAM_NONE);
            }
            else
            {
                uint32 avgTime = queueItem.GetAverageQueueWaitTime(&queueInfo, queueInfo.bgBracketId);
                // send status in BattleGround Queue (发送排队等待状态及预估等待时间)
                sBattleGroundMgr.BuildBattleGroundStatusPacket(data, true, queueInfo.bgTypeId, queueInfo.clientInstanceId, queueInfo.isRated, queueInfo.mapId, queueSlot, STATUS_WAIT_QUEUE, avgTime, WorldTimer::getMSTimeDiff(queueInfo.joinTime, WorldTimer::getMSTime()), queueInfo.arenaType, TEAM_NONE);
            }
            sWorld.GetMessager().AddMessage([playerGuid, data](World* /*world*/)
            {
                if (Player* player = sObjectMgr.GetPlayer(playerGuid))
                {
                    player->GetSession()->SendPacket(data);
                }
            });
        }
    });

}

// Sent by client when requesting the spirit healer (查询战场灵魂向导复活倒计时 CMSG_AREA_SPIRIT_HEALER_QUERY)
void WorldSession::HandleAreaSpiritHealerQueryOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_AREA_SPIRIT_HEALER_QUERY");

    BattleGround* bg = _player->GetBattleGround();
    if (!bg)
        return;

    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
        return;

    if (!unit->isSpiritService())                           // it's not spirit service (目标不是灵魂医者服务 NPC)
        return;

    unit->SendAreaSpiritHealerQueryOpcode(GetPlayer());
}

// Sent by client when requesting spirit healer queue (登记加入灵魂向导复活队列 CMSG_AREA_SPIRIT_HEALER_QUEUE)
void WorldSession::HandleAreaSpiritHealerQueueOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_AREA_SPIRIT_HEALER_QUEUE");

    BattleGround* bg = _player->GetBattleGround();
    if (!bg)
        return;

    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
        return;

    if (!unit->isSpiritService())                           // it's not spirit service
        return;

    sScriptDevAIMgr.OnGossipHello(GetPlayer(), unit);
}

// Sent by client when requesting arena join (处理玩家报名竞技场练习赛或排位赛 CMSG_BATTLEMASTER_JOIN_ARENA)
void WorldSession::HandleBattlemasterJoinArena(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_BATTLEMASTER_JOIN_ARENA");
    // recv_data.hexlike();

    ObjectGuid guid;                                        // arena Battlemaster guid (竞技场军官 GUID)
    uint8 arenaslot;                                        // 2v2, 3v3 or 5v5 (战队槽位 0/1/2)
    uint8 asGroup;                                          // asGroup (是否组队报名)
    uint8 isRated;                                          // isRated (是否排位赛)

    recv_data >> guid >> arenaslot >> asGroup >> isRated;

    // ignore if we already in BG or BG queue (已在战场中则忽略)
    if (_player->InBattleGround())
        return;

    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
        return;

    if (!unit->isBattleMaster())                            // it's not battle master (校验 NPC 为战场军官)
        return;

    ArenaType arenatype;
    uint32 arenaRating = 0;

    switch (arenaslot)
    {
        case 0:
            arenatype = ARENA_TYPE_2v2;
            break;
        case 1:
            arenatype = ARENA_TYPE_3v3;
            break;
        case 2:
            arenatype = ARENA_TYPE_5v5;
            break;
        default:
            sLog.outError("Unknown arena slot %u at HandleBattlemasterJoinArena()", arenaslot);
            return;
    }

    // check existence (使用 BATTLEGROUND_AA 全竞技场模板)
    BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(BATTLEGROUND_AA);
    if (!bg)
    {
        sLog.outError("Battleground: template bg (all arenas) not found");
        return;
    }

    BattleGroundTypeId bgTypeId = bg->GetTypeId();
    BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BgQueueTypeId(bgTypeId, arenatype);
    BattleGroundBracketId bgBracketId = sBattleGroundMgr.GetBattleGroundBracketIdFromLevel(bgTypeId, _player->GetLevel());

    Group* group = nullptr;

    // check queue conditions (单人仅限练习赛，排位赛必须组队报名)
    if (!asGroup)
    {
        // you can't join in this way by client
        if (isRated)
            return;

        // check if already in queue
        if (_player->GetBattleGroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
            // player is already in this queue
            return;
        // check if has free queue slots
        if (!_player->HasFreeBattleGroundQueueId())
        {
            WorldPacket data;
            sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(data, bgTypeId, BG_GROUP_JOIN_STATUS_TOO_MANY_QUEUES);
            _player->GetSession()->SendPacket(data);
            return;
        }
    }
    else
    {
        group = _player->GetGroup();
        // no group found, error
        if (!group)
            return;
        uint32 err = group->CanJoinBattleGroundQueue(bgTypeId, bgQueueTypeId, arenatype, arenatype, isRated != 0, arenaslot);
        if (err != BG_JOIN_ERR_OK)
        {
            if (err == BG_JOIN_ERR_MIXED_ARENATEAM)
            {
                WorldPacket data;
                sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(data, bgTypeId, BG_GROUP_JOIN_STATUS_NOT_IN_TEAM);
                _player->GetSession()->SendPacket(data);
            }
            else if (err == BG_JOIN_ERR_ALL_QUEUES_USED)
            {
                WorldPacket data;
                sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(data, bgTypeId, BG_GROUP_JOIN_STATUS_TOO_MANY_QUEUES);
                _player->GetSession()->SendPacket(data);
            }
            else if (err == BG_JOIN_ERR_GROUP_MEMBER_ALREADY_IN_QUEUE)
            {
                WorldPacket data;
                sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(data, bgTypeId, BG_GROUP_JOIN_STATUS_CANNOT_QUEUE_FOR_RATED);
                _player->GetSession()->SendPacket(data);
            }
            else
                SendBattleGroundOrArenaJoinError(err);

            return;
        }
    }

    uint32 ateamId = 0;

    if (isRated)
    {
        ateamId = _player->GetArenaTeamId(arenaslot);
        // check real arena team existence only here (if it was moved to group->CanJoin .. () then we would have to get it twice)
        ArenaTeam* at = sObjectMgr.GetArenaTeamById(ateamId);
        if (!at)
        {
            _player->GetSession()->SendNotInArenaTeamPacket(arenatype);
            return;
        }
        // get the team rating for queue (获取战队当前匹配等级)
        arenaRating = at->GetRating();
        // the arena team id must match for everyone in the group
        // get the personal ratings for queue (计算全队成员个人等级均值)
        uint32 avg_pers_rating = 0;

        for (const auto& citr : group->GetMemberSlots())
        {
            ArenaTeamMember const* at_member = at->GetMember(citr.guid);
            if (!at_member)                                 // group member joining to arena must be in leader arena team
                return;

            // calc avg personal rating
            avg_pers_rating += at_member->personal_rating;
        }

        avg_pers_rating /= group->GetMembersCount();

        // if avg personal rating is more than 150 points below the teams rating, the team will be queued against an opponent matching or similar to the average personal rating
        // (若队员平均个人等级低于战队等级 150 分以上，则改按平均个人等级进行匹配)
        if (avg_pers_rating + 150 < arenaRating)
            arenaRating = avg_pers_rating;
    }

    AddGroupToQueueInfo info;
    info.team = _player->GetTeam();
    info.clientInstanceId = 0;
    info.mapId = bg->GetMapId();
    if (asGroup)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            if (Player* member = itr->getSource())
                info.members.push_back(member->GetObjectGuid());
    }

    sWorld.GetBGQueue().GetMessager().AddMessage([playerGuid = _player->GetObjectGuid(), bgQueueTypeId, bgTypeId, bgBracketId, ateamId, asGroup, isRated, arenaTeamId = _player->GetArenaTeamId(arenaslot), playerName = _player->GetName(), info, arenaRating, arenatype](BattleGroundQueue* queue)
    {
        BattleGroundQueueItem& bgQueue = queue->GetBattleGroundQueue(bgQueueTypeId);
        if (asGroup)
        {
            DEBUG_LOG("Battleground: arena join as group start");
            if (isRated)
                DEBUG_LOG("Battleground: arena team id %u, leader %s queued with rating %u for type %u", arenaTeamId, playerName, arenaRating, arenatype);

            GroupQueueInfo* groupInfo = bgQueue.AddGroup(playerGuid, info, bgTypeId, bgBracketId, arenatype, isRated != 0, false, 0, arenaRating, ateamId);
            uint32 avgTime = bgQueue.GetAverageQueueWaitTime(groupInfo, groupInfo->bgBracketId);
            sWorld.GetMessager().AddMessage([playerGuid, members = info.members, bgQueueTypeId, bgTypeId, bgClientInstanceId = groupInfo->clientInstanceId, mapId = groupInfo->mapId, avgTime, arenaType = groupInfo->arenaType, isRated = groupInfo->isRated](World* /*world*/)
            {
                Player* leader = sObjectMgr.GetPlayer(playerGuid);
                for (ObjectGuid guid : members)
                {
                    Player* member = sObjectMgr.GetPlayer(guid);
                    if (!member)
                        continue;

                    // add to queue
                    uint32 queueSlot = member->AddBattleGroundQueueId(bgQueueTypeId);

                    // store entry point coords (same as leader entry point)
                    member->SetBattleGroundEntryPoint(leader);

                    WorldPacket data;
                    // send status packet (in queue)
                    sBattleGroundMgr.BuildBattleGroundStatusPacket(data, true, bgTypeId, bgClientInstanceId, isRated, mapId, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, arenaType, TEAM_NONE);
                    member->GetSession()->SendPacket(data);
                    sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(data, bgTypeId, BG_GROUP_JOIN_STATUS_SUCCESS);
                    member->GetSession()->SendPacket(data);
                    DEBUG_LOG("Battleground: player joined queue for arena as group bg queue type %u bg type %u: GUID %u, NAME %s", bgQueueTypeId, bgTypeId, member->GetGUIDLow(), member->GetName());
                }

                DEBUG_LOG("Battleground: arena join as group end");
            });
        }
        else
        {
            GroupQueueInfo* groupInfo = bgQueue.AddGroup(playerGuid, info, bgTypeId, bgBracketId, arenatype, isRated != 0, false, 0, arenaRating, ateamId);
            uint32 avgTime = bgQueue.GetAverageQueueWaitTime(groupInfo, bgBracketId);

            sWorld.GetMessager().AddMessage([playerGuid, bgQueueTypeId, bgTypeId, bgClientInstanceId = groupInfo->clientInstanceId, avgTime, arenaType = groupInfo->arenaType, isRated = groupInfo->isRated, mapId = groupInfo->mapId](World* /*world*/)
            {
                if (Player* player = sObjectMgr.GetPlayer(playerGuid))
                {
                    uint32 queueSlot = player->AddBattleGroundQueueId(bgQueueTypeId);

                    // store entry point coords
                    player->SetBattleGroundEntryPoint();

                    WorldPacket data;
                    // send status packet (in queue)
                    sBattleGroundMgr.BuildBattleGroundStatusPacket(data, true, bgTypeId, bgClientInstanceId, isRated, mapId, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, arenaType, TEAM_NONE);
                    player->GetSession()->SendPacket(data);
                    DEBUG_LOG("Battleground: player joined queue for arena, skirmish, bg queue type %u bg type %u: GUID %u, NAME %s", bgQueueTypeId, bgTypeId, playerGuid.GetCounter(), player->GetName());
                }
            });
        }
        queue->ScheduleQueueUpdate(arenaRating, arenatype, bgQueueTypeId, bgTypeId, bgBracketId);
    });    
}

// Sent by client when reporting AFK (处理玩家在战场内举报挂机玩家 CMSG_REPORT_PVP_PLAYER_AFK)
void WorldSession::HandleReportPvPAFK(WorldPacket& recv_data)
{
    ObjectGuid playerGuid;
    recv_data >> playerGuid;
    Player* reportedPlayer = sObjectMgr.GetPlayer(playerGuid);

    if (!reportedPlayer)
    {
        DEBUG_LOG("WorldSession::HandleReportPvPAFK: player not found");
        return;
    }

    DEBUG_LOG("WorldSession::HandleReportPvPAFK: %s reported %s", _player->GetName(), reportedPlayer->GetName());

    reportedPlayer->ReportedAfkBy(_player);
}

// 向队长或队员发送组队排战场/竞技场失败的本地化系统提示
void WorldSession::SendBattleGroundOrArenaJoinError(uint8 err) const
{
    WorldPacket data;
    int32 msg;
    switch (err)
    {
        case BG_JOIN_ERR_OFFLINE_MEMBER:
            msg = LANG_BG_GROUP_OFFLINE_MEMBER;
            break;
        case BG_JOIN_ERR_GROUP_TOO_MANY:
            msg = LANG_BG_GROUP_TOO_LARGE;
            break;
        case BG_JOIN_ERR_MIXED_FACTION:
            msg = LANG_BG_GROUP_MIXED_FACTION;
            break;
        case BG_JOIN_ERR_MIXED_LEVELS:
            msg = LANG_BG_GROUP_MIXED_LEVELS;
            break;
        case BG_JOIN_ERR_GROUP_MEMBER_ALREADY_IN_QUEUE:
            msg = LANG_BG_GROUP_MEMBER_ALREADY_IN_QUEUE;
            break;
        case BG_JOIN_ERR_GROUP_DESERTER:
            msg = LANG_BG_GROUP_MEMBER_DESERTER;
            break;
        case BG_JOIN_ERR_ALL_QUEUES_USED:
            msg = LANG_BG_GROUP_MEMBER_NO_FREE_QUEUE_SLOTS;
            break;
        case BG_JOIN_ERR_GROUP_IN_ARENA:
            msg = LANG_BG_GROUP_MEMBER_IN_ARENA;
            break;
        case BG_JOIN_ERR_GROUP_NOT_ENOUGH:
        case BG_JOIN_ERR_MIXED_ARENATEAM:
        default:
            return;
    }
    ChatHandler::BuildChatPacket(data, CHAT_MSG_BG_SYSTEM_NEUTRAL, GetMangosString(msg), LANG_UNIVERSAL);
    SendPacket(data);
}
