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
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Log/Log.h"
#include "Server/Opcodes.h"
#include "Guilds/Guild.h"
#include "Guilds/GuildMgr.h"
#include "Social/SocialMgr.h"

/// 响应客户端查询公会基本信息请求
void WorldSession::HandleGuildQueryOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_QUERY");

    uint32 guildId;
    recvPacket >> guildId;

    if (Guild* guild = sGuildMgr.GetGuildById(guildId))
    {
        guild->Query(this);
        return;
    }

    SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
}

/// 处理创建公会请求
void WorldSession::HandleGuildCreateOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_CREATE");

    std::string gname;
    recvPacket >> gname;

    if (GetPlayer()->GetGuildId())                          // already in guild
        return;

    Guild* guild = new Guild;
    if (!guild->Create(GetPlayer(), gname))
    {
        delete guild;
        return;
    }

    sGuildMgr.AddGuild(guild);
}

/// 处理公会邀请玩家入会请求（校验权限、阵营、屏蔽列表及已有公会状态）
void WorldSession::HandleGuildInviteOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_INVITE");

    std::string Invitedname, plname;
    Player* player = nullptr;

    recvPacket >> Invitedname;

    if (normalizePlayerName(Invitedname))
        player = ObjectAccessor::FindPlayerByName(Invitedname.c_str());

    if (!player)
    {
        SendGuildCommandResult(GUILD_INVITE_S, Invitedname, ERR_GUILD_PLAYER_NOT_FOUND_S);
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    // OK result but not send invite
    if (player->GetSocial()->HasIgnore(GetPlayer()->GetObjectGuid()))
        return;

    // not let enemies sign guild charter
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != GetPlayer()->GetTeam())
    {
        SendGuildCommandResult(GUILD_INVITE_S, Invitedname, ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (player->GetGuildId())
    {
        plname = player->GetName();
        SendGuildCommandResult(GUILD_INVITE_S, plname, ERR_ALREADY_IN_GUILD_S);
        return;
    }

    if (player->GetGuildIdInvited())
    {
        plname = player->GetName();
        SendGuildCommandResult(GUILD_INVITE_S, plname, ERR_ALREADY_INVITED_TO_GUILD_S);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_INVITE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    DEBUG_LOG("Player %s Invited %s to Join his Guild", GetPlayer()->GetName(), Invitedname.c_str());

    player->SetGuildIdInvited(GetPlayer()->GetGuildId());
    // Put record into guildlog
    guild->LogGuildEvent(GUILD_EVENT_LOG_INVITE_PLAYER, GetPlayer()->GetObjectGuid(), player->GetObjectGuid());

    WorldPacket data(SMSG_GUILD_INVITE, (8 + 10));          // guess size
    data << GetPlayer()->GetName();
    data << guild->GetName();
    player->GetSession()->SendPacket(data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_INVITE)");
}

/// 处理将成员踢出公会请求
void WorldSession::HandleGuildRemoveOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_REMOVE");

    std::string plName;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
        return;

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_REMOVE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->RankId == GR_GUILDMASTER)
    {
        SendGuildCommandResult(GUILD_QUIT_S, "", ERR_GUILD_LEADER_LEAVE);
        return;
    }

    // do not allow to kick player with same or higher rights
    if (GetPlayer()->GetRank() >= slot->RankId)
    {
        SendGuildCommandResult(GUILD_QUIT_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_UNINVITE_PLAYER, GetPlayer()->GetObjectGuid(), slot->guid);

    // possible last member removed, do cleanup, and no need events
    if (guild->DelMember(slot->guid))
    {
        guild->Disband();
        return;
    }

    guild->BroadcastEvent(GE_REMOVED, plName.c_str(), _player->GetName());
}

/// 处理玩家接受公会邀请请求
void WorldSession::HandleGuildAcceptOpcode(WorldPacket& /*recvPacket*/)
{
    Player* player = GetPlayer();

    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_ACCEPT");

    Guild* guild = sGuildMgr.GetGuildById(player->GetGuildIdInvited());
    if (!guild || player->GetGuildId())
        return;

    // not let enemies sign guild charter
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(guild->GetLeaderGuid()))
        return;

    if (!guild->AddMember(GetPlayer()->GetObjectGuid(), guild->GetLowestRank()))
        return;
    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_JOIN_GUILD, GetPlayer()->GetObjectGuid());

    guild->BroadcastEvent(GE_JOINED, player->GetObjectGuid(), player->GetName());
}

/// 处理玩家拒绝公会邀请请求
void WorldSession::HandleGuildDeclineOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DECLINE");
    if (_player->GetGuildId())
        return;

    if (_player->GetGuildIdInvited() != 0)
    {
        if (Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildIdInvited()))
        {
            ObjectGuid inviterGuid = guild->GetGuildInviter(_player->GetObjectGuid());
            if (!inviterGuid.IsEmpty())
            {
                if (Player const* pInviter = ObjectAccessor::FindPlayer(inviterGuid))
                {
                    WorldPacket data(SMSG_GUILD_DECLINE);
                    data << _player->GetName();
                    pInviter->GetSession()->SendPacket(data);
                }
            }
        }
    }

    GetPlayer()->SetGuildIdInvited(0);
}

/// 响应客户端请求公会创建日期与账号统计信息
void WorldSession::HandleGuildInfoOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_INFO");

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    WorldPacket data(SMSG_GUILD_INFO, (5 * 4 + guild->GetName().size() + 1));
    data << guild->GetName();
    data << uint32(guild->GetCreatedDay());
    data << uint32(guild->GetCreatedMonth());
    data << uint32(guild->GetCreatedYear());
    data << uint32(guild->GetMemberSize());                 // amount of chars
    data << uint32(guild->GetAccountsNumber());             // amount of accounts
    SendPacket(data);
}

/// 响应客户端请求公会花名册列表
void WorldSession::HandleGuildRosterOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_ROSTER");

    if (Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId()))
        guild->Roster(this);
}

/// 处理晋升公会成员职级请求
void WorldSession::HandleGuildPromoteOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_PROMOTE");

    std::string plName;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
        return;

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_PROMOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->guid == GetPlayer()->GetObjectGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_NAME_INVALID);
        return;
    }

    // allow to promote only to lower rank than member's rank
    // guildmaster's rank = 0
    // GetPlayer()->GetRank() + 1 is highest rank that current player can promote to
    if (GetPlayer()->GetRank() + 1 >= slot->RankId)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    uint32 newRankId = slot->RankId - 1;                    // when promoting player, rank is decreased

    slot->ChangeRank(newRankId);
    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_PROMOTE_PLAYER, GetPlayer()->GetObjectGuid(), slot->guid, newRankId);

    guild->BroadcastEvent(GE_PROMOTION, _player->GetName(), plName.c_str(), guild->GetRankName(newRankId).c_str());
}

/// 处理降级公会成员职级请求
void WorldSession::HandleGuildDemoteOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DEMOTE");

    std::string plName;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
        return;

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_DEMOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);

    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->guid == GetPlayer()->GetObjectGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_NAME_INVALID);
        return;
    }

    // do not allow to demote same or higher rank
    if (GetPlayer()->GetRank() >= slot->RankId)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    // do not allow to demote lowest rank
    if (slot->RankId >= guild->GetLowestRank())
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_LOW_S);
        return;
    }

    uint32 newRankId = slot->RankId + 1;                    // when demoting player, rank is increased

    slot->ChangeRank(newRankId);
    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_DEMOTE_PLAYER, GetPlayer()->GetObjectGuid(), slot->guid, newRankId);

    guild->BroadcastEvent(GE_DEMOTION, _player->GetName(), plName.c_str(), guild->GetRankName(slot->RankId).c_str());
}

/// 处理成员主动退出公会请求
void WorldSession::HandleGuildLeaveOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_LEAVE");

    Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (_player->GetObjectGuid() == guild->GetLeaderGuid() && guild->GetMemberSize() > 1)
    {
        SendGuildCommandResult(GUILD_QUIT_S, "", ERR_GUILD_LEADER_LEAVE);
        return;
    }

    if (_player->GetObjectGuid() == guild->GetLeaderGuid())
    {
        guild->Disband();
        return;
    }

    SendGuildCommandResult(GUILD_QUIT_S, guild->GetName(), ERR_PLAYER_NO_MORE_IN_GUILD);

    if (guild->DelMember(_player->GetObjectGuid()))
    {
        guild->Disband();
        return;
    }

    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_LEAVE_GUILD, _player->GetObjectGuid());

    guild->BroadcastEvent(GE_LEFT, _player->GetObjectGuid(), _player->GetName());
}

/// 处理会长解散公会请求
void WorldSession::HandleGuildDisbandOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DISBAND");

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->Disband();

    DEBUG_LOG("WORLD: Guild Successfully Disbanded");
}

/// 处理会长转让会长职位请求
void WorldSession::HandleGuildLeaderOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_LEADER");

    std::string name;
    recvPacket >> name;

    Player* oldLeader = GetPlayer();

    if (!normalizePlayerName(name))
        return;

    Guild* guild = sGuildMgr.GetGuildById(oldLeader->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (oldLeader->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* oldSlot = guild->GetMemberSlot(oldLeader->GetObjectGuid());
    if (!oldSlot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(name);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, name, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    guild->SetLeader(slot->guid);
    oldSlot->ChangeRank(GR_OFFICER);

    guild->BroadcastEvent(GE_LEADER_CHANGED, oldLeader->GetName(), name.c_str());
}

/// 处理修改公会今日信息（MOTD）请求
void WorldSession::HandleGuildMOTDOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_MOTD");

    std::string MOTD;

    if (!recvPacket.empty())
        recvPacket >> MOTD;
    else
        MOTD.clear();

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_SETMOTD))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->SetMOTD(MOTD);

    guild->BroadcastEvent(GE_MOTD, MOTD.c_str());
}

/// 处理修改成员公共备注请求
void WorldSession::HandleGuildSetPublicNoteOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_SET_PUBLIC_NOTE");

    std::string name, PNOTE;
    recvPacket >> name;

    if (!normalizePlayerName(name))
        return;

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_EPNOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(name);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, name, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    recvPacket >> PNOTE;

    slot->SetPNOTE(PNOTE);

    guild->Roster(this);
}

/// 处理修改成员官员备注请求
void WorldSession::HandleGuildSetOfficerNoteOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_SET_OFFICER_NOTE");

    std::string plName, OFFNOTE;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
        return;

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_EOFFNOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    recvPacket >> OFFNOTE;

    slot->SetOFFNOTE(OFFNOTE);

    guild->Roster(this);
}

/// 处理更新公会职级名称、权限与银行限额配置请求
void WorldSession::HandleGuildRankOpcode(WorldPacket& recvPacket)
{
    std::string rankname;
    uint32 rankId;
    uint32 rights, MoneyPerDay;

    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_RANK");

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        recvPacket.rpos(recvPacket.wpos());                 // set to end to avoid warnings spam
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        recvPacket.rpos(recvPacket.wpos());                 // set to end to avoid warnings spam
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    recvPacket >> rankId;
    recvPacket >> rights;
    recvPacket >> rankname;
    recvPacket >> MoneyPerDay;

    for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
    {
        uint32 BankRights;
        uint32 BankSlotPerDay;

        recvPacket >> BankRights;
        recvPacket >> BankSlotPerDay;
        guild->SetBankRightsAndSlots(rankId, uint8(i), uint16(BankRights & 0xFF), uint16(BankSlotPerDay), true);
    }

    DEBUG_LOG("WORLD: Changed RankName to %s , Rights to 0x%.4X", rankname.c_str(), rights);

    guild->SetBankMoneyPerDay(rankId, MoneyPerDay);
    guild->SetRankName(rankId, rankname);

    if (rankId == GR_GUILDMASTER)                           // prevent loss leader rights
        rights = GR_RIGHT_ALL;

    guild->SetRankRights(rankId, rights);

    guild->Query(this);
    guild->Roster();                                        // broadcast for tab rights update
}

/// 处理新增公会职级请求
void WorldSession::HandleGuildAddRankOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_ADD_RANK");

    std::string rankname;
    recvPacket >> rankname;

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (guild->GetRanksSize() >= GUILD_RANKS_MAX_COUNT)     // client not let create more 10 than ranks
        return;

    guild->CreateRank(rankname, GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);

    guild->Query(this);
    guild->Roster();                                        // broadcast for tab rights update
}

/// 处理删除最低公会职级请求
void WorldSession::HandleGuildDelRankOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DEL_RANK");

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->DelRank();

    guild->Query(this);
    guild->Roster();                                        // broadcast for tab rights update
}

/// 发送公会命令执行结果给客户端
void WorldSession::SendGuildCommandResult(uint32 typecmd, const std::string& str, uint32 cmdresult) const
{
    WorldPacket data(SMSG_GUILD_COMMAND_RESULT, (8 + str.size() + 1));
    data << typecmd;
    data << str;
    data << cmdresult;
    SendPacket(data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_COMMAND_RESULT)");
}

/// 处理修改公会简介文本请求
void WorldSession::HandleGuildChangeInfoTextOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_INFO_TEXT");

    std::string GINFO;
    recvPacket >> GINFO;

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_MODIFY_GUILD_INFO))
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->SetGINFO(GINFO);
}

/// 处理保存公会战袍徽章样式并扣除金币请求
void WorldSession::HandleSaveGuildEmblemOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode MSG_SAVE_GUILD_EMBLEM");

    ObjectGuid vendorGuid;
    uint32 EmblemStyle, EmblemColor, BorderStyle, BorderColor, BackgroundColor;

    recvPacket >> vendorGuid;
    recvPacket >> EmblemStyle >> EmblemColor >> BorderStyle >> BorderColor >> BackgroundColor;

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_TABARDDESIGNER);
    if (!pCreature)
    {
        //"That's not an emblem vendor!"
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_INVALIDVENDOR);
        DEBUG_LOG("WORLD: HandleSaveGuildEmblemOpcode - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        //"You are not part of a guild!";
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOGUILD);
        return;
    }

    if (guild->GetLeaderGuid() != GetPlayer()->GetObjectGuid())
    {
        //"Only guild leaders can create emblems."
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTGUILDMASTER);
        return;
    }

    if (GetPlayer()->GetMoney() < 10 * GOLD)
    {
        //"You can't afford to do that."
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTENOUGHMONEY);
        return;
    }

    GetPlayer()->ModifyMoney(-10 * GOLD);
    guild->SetEmblem(EmblemStyle, EmblemColor, BorderStyle, BorderColor, BackgroundColor);

    //"Guild Emblem saved."
    SendSaveGuildEmblem(ERR_GUILDEMBLEM_SUCCESS);

    guild->Query(this);
}

/// 响应查询公会成员变动事件日志请求
void WorldSession::HandleGuildEventLogQueryOpcode(WorldPacket& /* recvPacket */)
{
    // empty
    DEBUG_LOG("WORLD: Received (MSG_GUILD_EVENT_LOG_QUERY)");

    if (uint32 GuildId = GetPlayer()->GetGuildId())
        if (Guild* pGuild = sGuildMgr.GetGuildById(GuildId))
            pGuild->DisplayGuildEventLog(this);
}

/******  GUILD BANK  *******/

/// 响应查询当前玩家当日剩余可提取公会银行金币额度
void WorldSession::HandleGuildBankMoneyWithdrawn(WorldPacket& /* recv_data */)
{
    DEBUG_LOG("WORLD: Received (MSG_GUILD_BANK_MONEY_WITHDRAWN)");

    if (uint32 GuildId = GetPlayer()->GetGuildId())
        if (Guild* pGuild = sGuildMgr.GetGuildById(GuildId))
            pGuild->SendMoneyInfo(this, GetPlayer()->GetGUIDLow());
}

/// 下发玩家所在职级的公会及公会银行权限详情
void WorldSession::HandleGuildPermissions(WorldPacket& /* recv_data */)
{
    DEBUG_LOG("WORLD: Received (MSG_GUILD_PERMISSIONS)");

    if (uint32 GuildId = GetPlayer()->GetGuildId())
    {
        if (Guild* pGuild = sGuildMgr.GetGuildById(GuildId))
        {
            uint32 rankId = GetPlayer()->GetRank();

            WorldPacket data(MSG_GUILD_PERMISSIONS, 4 * 15 + 1);
            data << uint32(rankId);                         // guild rank id
            data << uint32(pGuild->GetRankRights(rankId));  // rank rights
            // money per day left
            data << uint32(pGuild->GetMemberMoneyWithdrawRem(GetPlayer()->GetGUIDLow()));
            data << uint8(pGuild->GetPurchasedTabs());      // tabs count
            // why sending all info when not all tabs are purchased???
            for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
            {
                data << uint32(pGuild->GetBankRights(rankId, uint8(i)));
                data << uint32(pGuild->GetMemberSlotWithdrawRem(GetPlayer()->GetGUIDLow(), uint8(i)));
            }
            SendPacket(data);
            DEBUG_LOG("WORLD: Sent (MSG_GUILD_PERMISSIONS)");
        }
    }
}

/* Called when clicking on Guild bank gameobject (点击公会银行对象时发送已购标签页信息) */
void WorldSession::HandleGuildBankerActivate(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANKER_ACTIVATE)");

    ObjectGuid goGuid;
    uint8  unk;
    recv_data >> goGuid >> unk;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    if (uint32 GuildId = GetPlayer()->GetGuildId())
    {
        if (Guild* pGuild = sGuildMgr.GetGuildById(GuildId))
        {
            pGuild->DisplayGuildBankTabsInfo(this);         // this also will load guild bank if not yet
            return;
        }
    }

    SendGuildCommandResult(GUILD_UNK1, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
}

/* Called when opening guild bank tab only (first one) (查询并发送指定公会银行页内的物品列表) */
void WorldSession::HandleGuildBankQueryTab(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_QUERY_TAB)");

    ObjectGuid goGuid;
    uint8 TabId, unk1;
    recv_data >> goGuid >> TabId >> unk1;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
        return;

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
        return;

    if (TabId >= pGuild->GetPurchasedTabs())
        return;

    // Let's update the amount of gold the player can withdraw before displaying the content
    // This is useful if money withdraw right has changed
    pGuild->SendMoneyInfo(this, GetPlayer()->GetGUIDLow());
    pGuild->DisplayGuildBankContent(this, TabId);
}

/// 处理向公会银行存入金币请求
void WorldSession::HandleGuildBankDepositMoney(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_DEPOSIT_MONEY)");

    ObjectGuid goGuid;
    uint32 money;
    recv_data >> goGuid >> money;

    if (!money)
        return;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    if (GetPlayer()->GetMoney() < money)
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
        return;

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
        return;

    if (!pGuild->GetPurchasedTabs())
        return;

    CharacterDatabase.BeginTransaction();

    pGuild->SetBankMoney(pGuild->GetGuildBankMoney() + money);
    GetPlayer()->ModifyMoney(-int(money));
    GetPlayer()->SaveGoldToDB();

    CharacterDatabase.CommitTransaction();

    // logging money
    if (_player->GetSession()->GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        sLog.outCommand(_player->GetSession()->GetAccountId(), "GM %s (Account: %u) deposit money (Amount: %u) to guild bank (Guild ID %u)",
                        _player->GetName(), _player->GetSession()->GetAccountId(), money, GuildId);
    }

    // log
    pGuild->LogBankEvent(GUILD_BANK_LOG_DEPOSIT_MONEY, uint8(0), GetPlayer()->GetGUIDLow(), money);

    pGuild->DisplayGuildBankTabsInfo(this);
    pGuild->DisplayGuildBankContent(this, 0);
    pGuild->DisplayGuildBankMoneyUpdate(this);
}

/// 处理从公会银行提取金币请求（校验每日提取限额）
void WorldSession::HandleGuildBankWithdrawMoney(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_WITHDRAW_MONEY)");

    ObjectGuid goGuid;
    uint32 money;
    recv_data >> goGuid >> money;

    if (!money)
        return;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
        return;

    if (!pGuild->GetPurchasedTabs())
        return;

    if (pGuild->GetGuildBankMoney() < money)                // not enough money in bank
        return;

    if (!pGuild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_WITHDRAW_GOLD))
        return;

    CharacterDatabase.BeginTransaction();

    if (!pGuild->MemberMoneyWithdraw(money, GetPlayer()->GetGUIDLow()))
    {
        CharacterDatabase.RollbackTransaction();
        return;
    }

    GetPlayer()->ModifyMoney(money);
    GetPlayer()->SaveGoldToDB();

    CharacterDatabase.CommitTransaction();

    // Log
    pGuild->LogBankEvent(GUILD_BANK_LOG_WITHDRAW_MONEY, uint8(0), GetPlayer()->GetGUIDLow(), money);

    pGuild->SendMoneyInfo(this, GetPlayer()->GetGUIDLow());
    pGuild->DisplayGuildBankTabsInfo(this);
    pGuild->DisplayGuildBankContent(this, 0);
    pGuild->DisplayGuildBankMoneyUpdate(this);
}

/// 处理公会银行与玩家背包之间或银行页之间的物品存取、拆分与交换
void WorldSession::HandleGuildBankSwapItems(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_SWAP_ITEMS)");

    ObjectGuid goGuid;
    uint8 BankToBank;

    uint8 BankTab, BankTabSlot, AutoStore;
    uint8 PlayerSlot = NULL_SLOT;
    uint8 PlayerBag = NULL_BAG;
    uint8 BankTabDst = 0;
    uint8 BankTabSlotDst = 0;
    uint8 ToChar = 1;
    uint32 ItemEntry;
    uint8 AutoStoreCount = 0;
    uint8 SplitedAmount = 0;

    recv_data >> goGuid >> BankToBank;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
    {
        recv_data.rpos(recv_data.wpos());                   // prevent additional spam at rejected packet
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        recv_data.rpos(recv_data.wpos());                   // prevent additional spam at rejected packet
        return;
    }

    if (BankToBank)
    {
        recv_data >> BankTabDst;
        recv_data >> BankTabSlotDst;
        recv_data.read_skip<uint32>();                      // always 0
        recv_data >> BankTab;
        recv_data >> BankTabSlot;
        recv_data >> ItemEntry;
        recv_data.read_skip<uint8>();                       // always 0
        recv_data >> SplitedAmount;

        if (BankTabSlotDst >= GUILD_BANK_MAX_SLOTS ||
                (BankTabDst == BankTab && BankTabSlotDst == BankTabSlot) ||
                BankTab >= pGuild->GetPurchasedTabs() ||
                BankTabDst >= pGuild->GetPurchasedTabs())
        {
            recv_data.rpos(recv_data.wpos());               // prevent additional spam at rejected packet
            return;
        }
    }
    else
    {
        recv_data >> BankTab;
        recv_data >> BankTabSlot;
        recv_data >> ItemEntry;
        recv_data >> AutoStore;
        if (AutoStore)
        {
            recv_data >> AutoStoreCount;
            recv_data.read_skip<uint8>();                   // ToChar (?), always and expected to be 1 (autostore only triggered in guild->ToChar)
            recv_data.read_skip<uint8>();                   // unknown, always 0
        }
        else
        {
            recv_data >> PlayerBag;
            recv_data >> PlayerSlot;
            recv_data >> ToChar;
            recv_data >> SplitedAmount;
        }

        if ((BankTabSlot >= GUILD_BANK_MAX_SLOTS && BankTabSlot != 0xFF) ||
                BankTab >= pGuild->GetPurchasedTabs())
        {
            recv_data.rpos(recv_data.wpos());               // prevent additional spam at rejected packet
            return;
        }
    }

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    // Bank <-> Bank
    if (BankToBank)
    {
        pGuild->SwapItems(_player, BankTab, BankTabSlot, BankTabDst, BankTabSlotDst, SplitedAmount);
        return;
    }

    // Player <-> Bank

    // allow work with inventory only
    if (!Player::IsInventoryPos(PlayerBag, PlayerSlot) && !(PlayerBag == NULL_BAG && PlayerSlot == NULL_SLOT))
    {
        _player->SendEquipError(EQUIP_ERR_NONE, nullptr, nullptr);
        return;
    }

    // BankToChar swap or char to bank remaining
    if (ToChar)                                             // Bank -> Char cases
        pGuild->MoveFromBankToChar(_player, BankTab, BankTabSlot, PlayerBag, PlayerSlot, SplitedAmount);
    else                                                    // Char -> Bank cases
        pGuild->MoveFromCharToBank(_player, PlayerBag, PlayerSlot, BankTab, BankTabSlot, SplitedAmount);
}

/// 处理购买新的公会银行标签页请求
void WorldSession::HandleGuildBankBuyTab(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_BUY_TAB)");

    ObjectGuid goGuid;
    uint8 TabId;

    recv_data >> goGuid;
    recv_data >> TabId;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
        return;

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
        return;

    // m_PurchasedTabs = 0 when buying Tab 0, that is why this check can be made
    if (TabId != pGuild->GetPurchasedTabs())
        return;

    uint32 TabCost = GetGuildBankTabPrice(TabId) * GOLD;
    if (!TabCost)
        return;

    if (GetPlayer()->GetMoney() < TabCost)                  // Should not happen, this is checked by client
        return;

    // Go on with creating tab
    pGuild->CreateNewBankTab();
    GetPlayer()->ModifyMoney(-int(TabCost));
    pGuild->SetBankRightsAndSlots(GetPlayer()->GetRank(), TabId, GUILD_BANK_RIGHT_FULL, WITHDRAW_SLOT_UNLIMITED, true);
    pGuild->Roster();                                       // broadcast for tab rights update
    pGuild->DisplayGuildBankTabsInfo(this);
}

/// 处理修改公会银行标签页名称与图标请求
void WorldSession::HandleGuildBankUpdateTab(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_UPDATE_TAB)");

    ObjectGuid goGuid;
    uint8 TabId;
    std::string Name;
    std::string IconIndex;

    recv_data >> goGuid;
    recv_data >> TabId;
    recv_data >> Name;
    recv_data >> IconIndex;

    if (Name.empty())
        return;

    if (IconIndex.empty())
        return;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
        return;

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
        return;

    if (TabId >= pGuild->GetPurchasedTabs())
        return;

    pGuild->SetGuildBankTabInfo(TabId, Name, IconIndex);
    pGuild->DisplayGuildBankTabsInfo(this);
    pGuild->DisplayGuildBankContent(this, TabId);
}

/// 响应查询公会银行物品/金币操作日志请求
void WorldSession::HandleGuildBankLogQuery(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (MSG_GUILD_BANK_LOG_QUERY)");

    uint8 TabId;
    recv_data >> TabId;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
        return;

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
        return;

    // GUILD_BANK_MAX_TABS send by client for money log
    if (TabId >= pGuild->GetPurchasedTabs() && TabId != GUILD_BANK_MAX_TABS)
        return;

    pGuild->DisplayGuildBankLogs(this, TabId);
}

/// 响应查询公会银行标签页公告文本请求
void WorldSession::HandleQueryGuildBankTabText(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode MSG_QUERY_GUILD_BANK_TEXT");

    uint8 TabId;
    recv_data >> TabId;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
        return;

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
        return;

    if (TabId >= pGuild->GetPurchasedTabs())
        return;

    pGuild->SendGuildBankTabText(this, TabId);
}

/// 处理更新公会银行标签页公告文本请求
void WorldSession::HandleSetGuildBankTabText(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_GUILD_BANK_TEXT");

    uint8 TabId;
    std::string Text;
    recv_data >> TabId;
    recv_data >> Text;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
        return;

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
        return;

    if (TabId >= pGuild->GetPurchasedTabs())
        return;

    pGuild->SetGuildBankTabText(TabId, Text);
}

/// 发送保存公会战袍徽章的操作结果码
void WorldSession::SendSaveGuildEmblem(uint32 msg) const
{
    WorldPacket data(MSG_SAVE_GUILD_EMBLEM, 4);
    data << uint32(msg);                                    // not part of guild
    SendPacket(data);
}
