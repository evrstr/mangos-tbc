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

#include "Server/WorldSession.h"
#include "Server/WorldPacket.h"
#include "Log/Log.h"
#include "Entities/Player.h"
#include "Globals/ObjectMgr.h"
#include "Arena/ArenaTeam.h"
#include "World/World.h"
#include "Social/SocialMgr.h"

// 处理观察目标玩家竞技场战队信息请求 (MSG_INSPECT_ARENA_TEAMS)
void WorldSession::HandleInspectArenaTeamsOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("MSG_INSPECT_ARENA_TEAMS");

    ObjectGuid guid;
    recv_data >> guid;
    DEBUG_LOG("Inspect Arena stats %s", guid.GetString().c_str());

    Player* player = sObjectMgr.GetPlayer(guid);
    if (!player)
        return;

    if (!_player->IsWithinDistInMap(player, INSPECT_DISTANCE, false))
        return;

    if (_player->CanAttack(player))
        return;

    for (uint8 i = 0; i < MAX_ARENA_SLOT; ++i)
    {
        if (uint32 a_id = player->GetArenaTeamId(i))
        {
            if (ArenaTeam* arenaTeam = sObjectMgr.GetArenaTeamById(a_id))
                arenaTeam->InspectStats(this, player->GetObjectGuid());
        }
    }
}

// 处理竞技场战队基本信息与战绩查询 (CMSG_ARENA_TEAM_QUERY)
void WorldSession::HandleArenaTeamQueryOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ARENA_TEAM_QUERY");

    uint32 ArenaTeamId;
    recv_data >> ArenaTeamId;

    if (ArenaTeam* arenateam = sObjectMgr.GetArenaTeamById(ArenaTeamId))
    {
        arenateam->Query(this);
        arenateam->Stats(this);
    }
}

// 处理竞技场战队名册请求 (CMSG_ARENA_TEAM_ROSTER)
void WorldSession::HandleArenaTeamRosterOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ARENA_TEAM_ROSTER");

    uint32 ArenaTeamId;                                     // arena team id (竞技场战队 ID)
    recv_data >> ArenaTeamId;

    if (ArenaTeam* arenateam = sObjectMgr.GetArenaTeamById(ArenaTeamId))
        arenateam->Roster(this);
}

// 处理邀请玩家加入竞技场战队请求 (CMSG_ARENA_TEAM_INVITE)
void WorldSession::HandleArenaTeamInviteOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_INVITE");

    uint32 ArenaTeamId;                                     // arena team id (竞技场战队 ID)
    std::string Invitedname;

    Player* player = nullptr;

    recv_data >> ArenaTeamId >> Invitedname;

    if (!Invitedname.empty())
    {
        if (!normalizePlayerName(Invitedname))
            return;

        player = ObjectAccessor::FindPlayerByName(Invitedname.c_str());
    }

    if (!player)
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", Invitedname, ERR_ARENA_TEAM_PLAYER_NOT_FOUND_S);
        return;
    }

    if (player->GetLevel() < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", player->GetName(), ERR_ARENA_TEAM_TARGET_TOO_LOW_S);
        return;
    }

    ArenaTeam* arenateam = sObjectMgr.GetArenaTeamById(ArenaTeamId);
    if (!arenateam)
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_PLAYER_NOT_IN_TEAM);
        return;
    }

    // OK result but not send invite (若被对方屏蔽则静默返回，不发送邀请)
    if (player->GetSocial()->HasIgnore(GetPlayer()->GetObjectGuid()))
        return;

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != GetPlayer()->GetTeam())
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", "", ERR_ARENA_TEAM_NOT_ALLIED);
        return;
    }

    if (player->GetArenaTeamId(arenateam->GetSlot()))
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", player->GetName(), ERR_ALREADY_IN_ARENA_TEAM_S);
        return;
    }

    if (player->GetArenaTeamIdInvited())
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", player->GetName(), ERR_ALREADY_INVITED_TO_ARENA_TEAM_S);
        return;
    }

    if (arenateam->GetMembersSize() >= arenateam->GetMaxMembersSize())
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, arenateam->GetName(), "", ERR_ARENA_TEAM_TOO_MANY_MEMBERS_S);
        return;
    }

    DEBUG_LOG("Player %s Invited %s to Join his ArenaTeam", GetPlayer()->GetName(), Invitedname.c_str());

    player->SetArenaTeamIdInvited(arenateam->GetId());

    WorldPacket data(SMSG_ARENA_TEAM_INVITE, (8 + 10));
    data << GetPlayer()->GetName();
    data << arenateam->GetName();
    player->GetSession()->SendPacket(data);

    DEBUG_LOG("WORLD: Sent SMSG_ARENA_TEAM_INVITE");
}

// 处理接受加入竞技场战队邀请 (CMSG_ARENA_TEAM_ACCEPT)
void WorldSession::HandleArenaTeamAcceptOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_ACCEPT");                    // empty opcode (无负载数据)

    ArenaTeam* at = sObjectMgr.GetArenaTeamById(_player->GetArenaTeamIdInvited());
    if (!at)
        return;

    if (_player->GetArenaTeamId(at->GetSlot()))
    {
        // already in arena team that size (已加入同规模的竞技场战队)
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ALREADY_IN_ARENA_TEAM);
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
            _player->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(at->GetCaptainGuid()))
    {
        // not let enemies sign petition (禁止敌对阵营玩家加入战队)
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_NOT_ALLIED);
        return;
    }

    if (!at->AddMember(_player->GetObjectGuid()))
    {
        // arena team not found (添加成员失败，返回内部错误)
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_INTERNAL);
        return;
    }

    // event (广播成员加入事件)
    at->BroadcastEvent(ERR_ARENA_TEAM_JOIN_SS, _player->GetObjectGuid(), _player->GetName(), at->GetName().c_str());
}

// 处理拒绝竞技场战队邀请 (CMSG_ARENA_TEAM_DECLINE)
void WorldSession::HandleArenaTeamDeclineOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_DECLINE");                   // empty opcode (无负载数据)

    _player->SetArenaTeamIdInvited(0);                      // no more invited (清除待处理邀请状态)
}

// 处理玩家主动退出竞技场战队 (CMSG_ARENA_TEAM_LEAVE)
void WorldSession::HandleArenaTeamLeaveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_LEAVE");

    uint32 ArenaTeamId;                                     // arena team id (竞技场战队 ID)
    recv_data >> ArenaTeamId;

    ArenaTeam* at = sObjectMgr.GetArenaTeamById(ArenaTeamId);
    if (!at)
        return;

    if (_player->InBattleGroundQueue() || _player->InArena())
        return;

    if (_player->GetObjectGuid() == at->GetCaptainGuid() && at->GetMembersSize() > 1)
    {
        // check for correctness (队长在还有其他成员时必须先移交队长才能离队)
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_QUIT_S, "", "", ERR_ARENA_TEAM_LEADER_LEAVE_S);
        return;
    }

    // arena team has only one member (=captain) (若战队仅剩队长一人，离队即解散战队)
    if (_player->GetObjectGuid() == at->GetCaptainGuid())
    {
        at->Disband(this);
        delete at;
        return;
    }

    at->DelMember(_player->GetObjectGuid());

    // event (广播成员离开事件)
    at->BroadcastEvent(ERR_ARENA_TEAM_LEAVE_SS, _player->GetObjectGuid(), _player->GetName(), at->GetName().c_str());

    // send you are no longer member of team (向离队玩家发送退出确认通知)
    SendArenaTeamCommandResult(ERR_ARENA_TEAM_QUIT_S, at->GetName(), "", 0);
}

// 处理队长解散竞技场战队请求 (CMSG_ARENA_TEAM_DISBAND)
void WorldSession::HandleArenaTeamDisbandOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_DISBAND");

    uint32 ArenaTeamId;                                     // arena team id (竞技场战队 ID)
    recv_data >> ArenaTeamId;

    if (ArenaTeam* at = sObjectMgr.GetArenaTeamById(ArenaTeamId))
    {
        if (at->GetCaptainGuid() != _player->GetObjectGuid())
            return;

        if (at->IsFighting())
            return;

        at->Disband(this);
        delete at;
    }
}

// 处理队长踢出战队成员请求 (CMSG_ARENA_TEAM_REMOVE)
void WorldSession::HandleArenaTeamRemoveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_REMOVE");

    uint32 ArenaTeamId;
    std::string name;

    recv_data >> ArenaTeamId;
    recv_data >> name;

    ArenaTeam* at = sObjectMgr.GetArenaTeamById(ArenaTeamId);
    if (!at)                                                // arena team not found (未找到战队)
        return;

    if (at->GetCaptainGuid() != _player->GetObjectGuid())
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_PERMISSIONS);
        return;
    }

    if (!normalizePlayerName(name))
        return;

    ArenaTeamMember* member = at->GetMember(name);
    if (!member)                                            // member not found (未找到目标成员)
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", name, ERR_ARENA_TEAM_PLAYER_NOT_FOUND_S);
        return;
    }

    if (at->GetCaptainGuid() == member->guid)
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_QUIT_S, "", "", ERR_ARENA_TEAM_LEADER_LEAVE_S);
        return;
    }

    Player* player = ObjectAccessor::FindPlayer(member->guid);
    if (player && (player->InBattleGroundQueue() || player->InArena()))
        return;

    at->DelMember(member->guid);

    // event (广播成员被踢出事件)
    at->BroadcastEvent(ERR_ARENA_TEAM_REMOVE_SSS, name.c_str(), at->GetName().c_str(), _player->GetName());
}

// 处理移交战队队长请求 (CMSG_ARENA_TEAM_LEADER)
void WorldSession::HandleArenaTeamLeaderOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_LEADER");

    uint32 ArenaTeamId;
    std::string name;

    recv_data >> ArenaTeamId;
    recv_data >> name;

    ArenaTeam* at = sObjectMgr.GetArenaTeamById(ArenaTeamId);
    if (!at)                                                // arena team not found (未找到战队)
        return;

    if (at->GetCaptainGuid() != _player->GetObjectGuid())
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_PERMISSIONS);
        return;
    }

    if (!normalizePlayerName(name))
        return;

    ArenaTeamMember* member = at->GetMember(name);
    if (!member)                                            // member not found (未找到目标成员)
    {
        SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", name, ERR_ARENA_TEAM_PLAYER_NOT_FOUND_S);
        return;
    }

    if (at->GetCaptainGuid() == member->guid)               // target player already captain (目标玩家已是队长)
        return;

    at->SetCaptain(member->guid);

    // event (广播队长变更事件)
    at->BroadcastEvent(ERR_ARENA_TEAM_LEADER_CHANGED_SSS, _player->GetName(), name.c_str(), at->GetName().c_str());
}

// 发送竞技场战队操作命令结果数据包
void WorldSession::SendArenaTeamCommandResult(uint32 team_action, const std::string& team, const std::string& player, uint32 error_id) const
{
    WorldPacket data(SMSG_ARENA_TEAM_COMMAND_RESULT, 4 + team.length() + 1 + player.length() + 1 + 4);
    data << uint32(team_action);
    data << team;
    data << player;
    data << uint32(error_id);
    SendPacket(data);
}

// 发送玩家未加入对应规模竞技场战队的错误数据包
void WorldSession::SendNotInArenaTeamPacket(uint8 type) const
{
    WorldPacket data(SMSG_ARENA_ERROR, 4 + 1);              // 886 - You are not in a %uv%u arena team (错误提示：你未加入 %uv%u 竞技场战队)
    uint32 unk = 0;
    data << uint32(unk);                                    // unk(0) (未知字段，固定为 0)
    if (!unk)
        data << uint8(type);                                // team type (2=2v2,3=3v3,5=5v5), can be used for custom types... (战队规模类型 2/3/5，也可用于自定义类型)
    SendPacket(data);
}

/*
+ERR_ARENA_NO_TEAM_II "You are not in a %dv%d arena team"

+ERR_ARENA_TEAM_CREATE_S "%s created.  To disband, use /teamdisband [2v2, 3v3, 5v5]."
+ERR_ARENA_TEAM_INVITE_SS "You have invited %s to join %s"
+ERR_ARENA_TEAM_QUIT_S "You are no longer a member of %s"
ERR_ARENA_TEAM_FOUNDER_S "Congratulations, you are a founding member of %s!  To leave, use /teamquit [2v2, 3v3, 5v5]."

+ERR_ARENA_TEAM_INTERNAL "Internal arena team error"
+ERR_ALREADY_IN_ARENA_TEAM "You are already in an arena team of that size"
+ERR_ALREADY_IN_ARENA_TEAM_S "%s is already in an arena team of that size"
+ERR_INVITED_TO_ARENA_TEAM "You have already been invited into an arena team"
+ERR_ALREADY_INVITED_TO_ARENA_TEAM_S "%s has already been invited to an arena team"
+ERR_ARENA_TEAM_NAME_INVALID "That name contains invalid characters, please enter a new name"
+ERR_ARENA_TEAM_NAME_EXISTS_S "There is already an arena team named \"%s\""
+ERR_ARENA_TEAM_LEADER_LEAVE_S "You must promote a new team captain using /teamcaptain before leaving the team"
+ERR_ARENA_TEAM_PERMISSIONS "You don't have permission to do that"
+ERR_ARENA_TEAM_PLAYER_NOT_IN_TEAM "You are not in an arena team of that size"
+ERR_ARENA_TEAM_PLAYER_NOT_IN_TEAM_SS "%s is not in %s"
+ERR_ARENA_TEAM_PLAYER_NOT_FOUND_S "\"%s\" not found"
+ERR_ARENA_TEAM_NOT_ALLIED "You cannot invite players from the opposing alliance"

+ERR_ARENA_TEAM_JOIN_SS "%s has joined %s"
+ERR_ARENA_TEAM_YOU_JOIN_S "You have joined %s.  To leave, use /teamquit [2v2, 3v3, 5v5]."

+ERR_ARENA_TEAM_LEAVE_SS "%s has left %s"

+ERR_ARENA_TEAM_LEADER_IS_SS "%s is the captain of %s"
+ERR_ARENA_TEAM_LEADER_CHANGED_SSS "%s has made %s the new captain of %s"

+ERR_ARENA_TEAM_REMOVE_SSS "%s has been kicked out of %s by %s"

+ERR_ARENA_TEAM_DISBANDED_S "%s has disbanded %s"

ERR_ARENA_TEAM_TARGET_TOO_LOW_S "%s is not high enough level to join your team"

ERR_ARENA_TEAM_TOO_MANY_MEMBERS_S "%s is full"

ERR_ARENA_TEAM_LEVEL_TOO_LOW_I "You must be level %d to form an arena team"
*/
