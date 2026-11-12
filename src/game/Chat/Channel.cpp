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

#include "Chat/Channel.h"
#include "Globals/ObjectMgr.h"
#include "World/World.h"
#include "Social/SocialMgr.h"
#include "Chat/Chat.h"
#include "Anticheat/Anticheat.hpp"

/// 构造频道实例：若匹配 ChatChannels.dbc 则初始化为内置公共频道，否则初始化为玩家自定义频道
Channel::Channel(const std::string& name, uint32 channel_id/* = 0*/)
    : m_name(name)
{
    if (ChatChannelsEntry const* builtin = GetChatChannelsEntryFor(name, channel_id))
    {
        m_entry = builtin;                                              // built-in channel entry | 内置 DBC 频道条目
        m_announcements = false;                                        // no join/leave announcements by default | 默认关闭进出频道通告
        m_flags = CHANNEL_FLAG_GENERAL;                                 // default for all built-in channels | 所有内置频道基础标志

        if (builtin->flags & CHANNEL_DBC_FLAG_TRADE)                    // for trade channel | 交易频道标志
            m_flags |= CHANNEL_FLAG_TRADE;

        if (builtin->flags & CHANNEL_DBC_FLAG_CITY_ONLY2)               // for city only channels | 仅限主城频道标志
            m_flags |= CHANNEL_FLAG_CITY;

        if (builtin->flags & CHANNEL_DBC_FLAG_LFG)                      // for LFG channel | 寻求组队频道标志
            m_flags |= CHANNEL_FLAG_LFG;
        else                                                            // for all other channels | 非寻求组队频道标志
            m_flags |= CHANNEL_FLAG_NOT_LFG;

        // Custom features: | 扩展自定义特性：
        m_realmzone = true;                                             // channel language matches realm zone | 公共频道启用服务器区域语言校验
    }
    else // it's custom channel | 自定义频道分支
    {
        m_announcements = true;                                         // join/leave announcements enabled by default | 默认开启进出通告
        m_flags = CHANNEL_FLAG_CUSTOM;                                  // default for all custom channels | 自定义频道标志

        // Custom features: | 扩展自定义特性：
        m_realmzone = sObjectMgr.CheckPublicMessageLanguage(m_name);    // channel language matches channel name | 根据频道名是否符合区服语言决定是否启用语言限制
    }
}

/// 玩家加入频道：校验重复加入、封禁、密码、LFG 队列限制及自动静态化阈值
void Channel::Join(Player* player, const char* password)
{
    ObjectGuid guid = player->GetObjectGuid();

    WorldPacket data;
    if (IsOn(guid))
    {
        if (!IsConstant())                                  // non send error message for built-in channels | 内置频道不重复发送“已是成员”提示
        {
            MakePlayerAlreadyMember(data, m_name, guid);
            SendToOne(data, guid);
        }
        return;
    }

    if (IsBanned(guid))
    {
        MakeBanned(data, m_name);
        SendToOne(data, guid);
        return;
    }

    bool checkPassword = (m_password.length() > 0);

    if (const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_MODERATION))
        if (player->GetSession()->GetSecurity() >= level)
            checkPassword = false;

    if (checkPassword && strcmp(password, m_password.c_str()))
    {
        MakeWrongPassword(data, m_name);
        SendToOne(data, guid);
        return;
    }

    if (HasFlag(CHANNEL_FLAG_LFG) && sWorld.getConfig(CONFIG_BOOL_CHANNEL_RESTRICTED_LFG))
    {
        if (player->GetSession()->GetSecurity() == SEC_PLAYER && !player->GetSession()->m_lfgInfo.queued)
        {
            MakeNotInLFG(data, m_name);
            SendToOne(data, guid);
            return;
        }
    }

    if (player->GetGuildId() && (GetFlags() == 0x38))
        return;

    // join channel | 正式登记加入频道
    player->JoinedChannel(this);

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_SILENT_JOIN);
    const bool silent = (level && player->GetSession()->GetSecurity() >= level);

    if (m_announcements && !silent)
    {
        MakeJoined(data, m_name, guid);
        SendToAll(data);
    }

    data.clear();

    PlayerInfo& pinfo = m_players[guid];
    pinfo.player = guid;
    pinfo.flags = MEMBER_FLAG_NONE;

    MakeYouJoined(data, m_name, *this);
    SendToOne(data, guid);

    JoinNotify(guid);

    // if no owner first logged will become | 非公共频道若尚无所有者，则由首位加入者担任
    if (!IsPublic() && !m_ownerGuid)
        SetOwner(guid, (m_players.size() > 1));

    // try to auto-convert this channel to static upon reaching treshold | 人数达到配置阈值时尝试自动转为静态公共频道
    SetStatic(true);
}

/// 玩家离开频道；若为私有频道所有者离开则推选新所有者
void Channel::Leave(Player* player, bool send)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        if (send)
        {
            WorldPacket data;
            MakeNotMember(data, m_name);
            SendToOne(data, guid);
        }
        return;
    }

    // leave channel | 移除玩家频道成员状态并按需通知客户端
    if (send)
    {
        WorldPacket data;
        MakeYouLeft(data, m_name, *this);
        SendToOne(data, guid);
        player->LeftChannel(this);
        data.clear();
    }

    bool changeowner = m_players[guid].IsOwner();

    m_players.erase(guid);

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_SILENT_JOIN);
    const bool silent = (level && player->GetSession()->GetSecurity() >= level);

    if (m_announcements && !silent)
    {
        WorldPacket data;
        MakeLeft(data, m_name, guid);
        SendToAll(data);
    }

    LeaveNotify(guid);

    if (changeowner && !IsPublic())
        SetOwner(SelectNewOwner(), (m_players.size() > 1));
}

/// 踢出或封禁目标玩家（需管理员或配置允许的 GM 权限，不能越权踢所有者）
void Channel::KickOrBan(Player* player, const char* targetName, bool ban)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_MODERATION);
    const bool gm = (level && player->GetSession()->GetSecurity() >= level);

    if (!m_players[guid].IsModerator() && !gm)
    {
        WorldPacket data;
        MakeNotModerator(data, m_name);
        SendToOne(data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (!IsOn(targetGuid))
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    bool changeowner = m_ownerGuid == targetGuid;

    if (!gm && changeowner && guid != m_ownerGuid)
    {
        WorldPacket data;
        MakeNotOwner(data, m_name);
        SendToOne(data, guid);
        return;
    }

    // kick or ban player | 执行踢出或加入封禁列表并广播通知
    WorldPacket data;

    if (ban && !IsBanned(targetGuid))
    {
        m_banned.insert(targetGuid);
        MakePlayerBanned(data, m_name, targetGuid, guid);
    }
    else
        MakePlayerKicked(data, m_name, targetGuid, guid);

    SendToAll(data);
    m_players.erase(targetGuid);
    target->LeftChannel(this);

    if (changeowner && !IsPublic())
        SetOwner(SelectNewOwner(), (m_players.size() > 1));
}

/// 解除目标玩家在本频道的封禁状态
void Channel::UnBan(Player* player, const char* targetName)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_MODERATION);
    const bool gm = (level && player->GetSession()->GetSecurity() >= level);

    if (!m_players[guid].IsModerator() && !gm)
    {
        WorldPacket data;
        MakeNotModerator(data, m_name);
        SendToOne(data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (!IsBanned(targetGuid))
    {
        WorldPacket data;
        MakePlayerNotBanned(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    // unban player | 从封禁集合移除并全频道通告
    m_banned.erase(targetGuid);

    WorldPacket data;
    MakePlayerUnbanned(data, m_name, targetGuid, guid);
    SendToAll(data);
}

/// 修改频道密码并通知全体成员
void Channel::SetPassword(Player* player, const char* password)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_MODERATION);
    const bool gm = (level && player->GetSession()->GetSecurity() >= level);

    if (!m_players[guid].IsModerator() && !gm)
    {
        WorldPacket data;
        MakeNotModerator(data, m_name);
        SendToOne(data, guid);
        return;
    }

    // set channel password | 更新频道密码
    m_password = password;

    WorldPacket data;
    MakePasswordChanged(data, m_name, guid);
    SendToAll(data);
}

/// 设置或取消目标成员的频道权限标志（当前支持管理员与禁言标志）
void Channel::SetModeFlags(Player* player, const char* targetName, ChannelMemberFlags flags, bool set)
{
    // Restrict input flags to currently supported by this method | 仅允许修改管理员与禁言两种成员标志
    flags = ChannelMemberFlags(uint8(flags) & (MEMBER_FLAG_MODERATOR | MEMBER_FLAG_MUTED));

    if (!flags)
        return;

    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_MODERATION);
    const bool gm = (level && player->GetSession()->GetSecurity() >= level);

    if (!m_players[guid].IsModerator() && !gm)
    {
        WorldPacket data;
        MakeNotModerator(data, m_name);
        SendToOne(data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();

    if ((flags & MEMBER_FLAG_MODERATOR) && guid == m_ownerGuid && targetGuid == m_ownerGuid)
        return;

    if (!IsOn(targetGuid))
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    const bool gmTarget = (level && target->GetSession()->GetSecurity() >= level);

    // allow make moderator from another team only if both is GMs | 仅双方均为 GM 时才允许跨阵营授予管理员权限
    // at this moment this only way to show channel post for GM from another team | 当前这是让对立阵营 GM 显示频道发言的唯一方式
    if ((!gm || !gmTarget) && player->GetTeam() != target->GetTeam() && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    if (m_ownerGuid == targetGuid && m_ownerGuid != guid)
    {
        WorldPacket data;
        MakeNotOwner(data, m_name);
        SendToOne(data, guid);
        return;
    }

    SetModeFlags(targetGuid, flags, set);
}

/// 将频道所有权转让给指定玩家
void Channel::SetOwner(Player* player, const char* targetName)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_MODERATION);
    const bool gm = (level && player->GetSession()->GetSecurity() >= level);

    if (!gm && guid != m_ownerGuid)
    {
        WorldPacket data;
        MakeNotOwner(data, m_name);
        SendToOne(data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (!IsOn(targetGuid))
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    if (target->GetTeam() != player->GetTeam() && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    // set channel owner | 变更频道所有者
    SetOwner(targetGuid, (m_players.size() > 1));
}

/// 发送当前频道所有者查询响应（普通玩家需过滤不可见的 GM）
void Channel::SendChannelOwnerResponse(Player* player) const
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    ObjectGuid ownerGuid = m_ownerGuid;

    if (const bool visibilityCheck = (player->GetSession()->GetSecurity() == SEC_PLAYER))
    {
        // PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters | 普通玩家无法看到高权限 GM 角色
        // MODERATOR, GAME MASTER, ADMINISTRATOR can see all | 管理人员可见所有成员
        const AccountTypes visibilityThreshold = AccountTypes(sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST));

        if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
        {
            if (owner->GetSession()->GetSecurity() > visibilityThreshold || !owner->IsVisibleGloballyFor(player))
                ownerGuid = ObjectGuid();
        }
    }

    // send channel owner | 发送频道所有者通知包
    WorldPacket data;
    MakeChannelOwner(data, m_name, guid);
    SendToOne(data, guid);
}

/// 发送频道成员列表（SMSG_CHANNEL_LIST），对普通玩家过滤隐身或高权限 GM
void Channel::SendChannelListResponse(Player* player, bool display/*= false*/)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    // list players in channel | 构造频道成员列表数据包
    WorldPacket data(SMSG_CHANNEL_LIST, 1 + (GetName().size() + 1) + 1 + 4 + m_players.size() * (8 + 1));
    data << uint8(display);                                 // response type: 0 - /chatinfo query, 1 - display list query | 响应类型：0 为 /chatinfo 查询，1 为界面列表展示
    data << GetName();                                      // channel name | 频道名称
    data << uint8(GetFlags());                              // channel flags? | 频道标志位

    size_t countpos = data.wpos();
    data << uint32(0);                                      // size of list, placeholder | 成员数量占位符

    // PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters | 普通玩家看不到高于可见阈值的 GM 成员
    // MODERATOR, GAME MASTER, ADMINISTRATOR can see all | GM 可查看全部成员
    const bool visibilityCheck = (player->GetSession()->GetSecurity() == SEC_PLAYER);
    AccountTypes visibilityThreshold = AccountTypes(sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST));

    uint32 count = 0;
    for (PlayerList::const_iterator i = m_players.begin(); i != m_players.end(); ++i)
    {
        if (Player* member = sObjectMgr.GetPlayer(i->first))
        {
            if (visibilityCheck && (member->GetSession()->GetSecurity() > visibilityThreshold || !member->IsVisibleGloballyFor(player)))
                continue;

            data << ObjectGuid(i->first);
            data << uint8(i->second.flags);                 // flags seems to be changed... | 成员权限标志
            ++count;
        }
    }

    data.put<uint32>(countpos, count);

    SendToOne(data, guid);
}

/// 切换频道加入/离开通告开关
void Channel::ToggleAnnouncements(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_MODERATION);
    const bool gm = (level && player->GetSession()->GetSecurity() >= level);

    if (!m_players[guid].IsModerator() && !gm)
    {
        WorldPacket data;
        MakeNotModerator(data, m_name);
        SendToOne(data, guid);
        return;
    }

    // toggle channel announcement | 翻转通告开关并全频道广播
    m_announcements = !m_announcements;

    WorldPacket data;
    if (m_announcements)
        MakeAnnouncementsOn(data, m_name, guid);
    else
        MakeAnnouncementsOff(data, m_name, guid);

    SendToAll(data);
}

/// 切换频道发言审核模式（开启后仅管理员与 GM 可发言）
void Channel::ToggleModeration(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_MODERATION);
    const bool gm = (level && player->GetSession()->GetSecurity() >= level);

    if (!m_players[guid].IsModerator() && !gm)
    {
        WorldPacket data;
        MakeNotModerator(data, m_name);
        SendToOne(data, guid);
        return;
    }

    // toggle channel moderation | 翻转审核开关并全频道广播
    m_moderation = !m_moderation;

    WorldPacket data;
    if (m_moderation)
        MakeModerationOn(data, m_name, guid);
    else
        MakeModerationOff(data, m_name, guid);

    SendToAll(data);
}

/// 在频道内发言：校验成员资格、禁言/审核状态、区服语言限制与反作弊静默状态后广播
void Channel::Say(Player* player, const char* text, uint32 lang)
{
    if (!text)
        return;

    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    if (m_players[guid].IsMuted())
    {
        WorldPacket data;
        MakeMuted(data, m_name);
        SendToOne(data, guid);
        return;
    }

    const bool moderator = m_players[guid].IsModerator();

    const uint32 level = sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHANNEL_MODERATION);
    const bool gm = (level && player->GetSession()->GetSecurity() >= level);

    if (m_moderation && !moderator && !gm)
    {
        WorldPacket data;
        MakeNotModerator(data, m_name);
        SendToOne(data, guid);
        return;
    }

    if (uint32 restriction = sWorld.getConfig(CONFIG_UINT32_CHANNEL_RESTRICTED_LANGUAGE_MODE))
    {
        bool restricted = false;

        switch (restriction)
        {
            case 1: restricted = IsConstant();                  break;
            case 2: restricted = (IsPublic() && m_realmzone);   break;
            case 3: restricted = true;                          break;
        }

        if (restricted && !sObjectMgr.CheckPublicMessageLanguage(text))
        {
            WorldPacket data;
            MakeMuted(data, m_name);
            SendToOne(data, guid);
            return;
        }
    }

    // send channel message | 构造并发送频道消息（若被反作弊静默则仅回显给发送者自身）
    if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
        lang = LANG_UNIVERSAL;

    auto const silenced = player->GetSession()->GetAnticheat()->IsSilenced();

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_CHANNEL, text, Language(lang), player->GetChatTag(), guid, player->GetName(), ObjectGuid(), "", m_name.c_str());
    // if the source is silenced, send only to them | 若发送者被静默惩罚，仅向其自身回显以防察觉
    if (silenced)
        player->GetSession()->SendPacket(data);
    else
        SendMessage(data, (moderator ? ObjectGuid() : guid));
}

/// 邀请目标玩家加入当前频道（记录反垃圾统计、过滤屏蔽与被静默发送者）
void Channel::Invite(Player* player, const char* targetName)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(data, m_name);
        SendToOne(data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (IsOn(targetGuid))
    {
        WorldPacket data;
        MakePlayerAlreadyMember(data, m_name, targetGuid);
        SendToOne(data, guid);
        return;
    }

    // Record invite to antispam here | 向反作弊/反广告模块记录频道邀请行为
    player->GetSession()->GetAnticheat()->ChannelInvite(m_name, targetGuid);

    if (IsBanned(targetGuid))
    {
        WorldPacket data;
        MakePlayerInviteBanned(data, m_name, targetName);
        SendToOne(data, guid);
        return;
    }

    if (target->GetTeam() != player->GetTeam() && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        WorldPacket data;
        MakeInviteWrongFaction(data, m_name);
        SendToOne(data, guid);
        return;
    }

    // invite player | 若未被对方屏蔽且未处于静默惩罚，则向目标发送邀请包
    WorldPacket data;
    if (!target->GetSocial()->HasIgnore(guid) && !player->GetSession()->GetAnticheat()->IsSilenced())
    {
        MakeInvite(data, m_name, guid);
        SendToOne(data, targetGuid);
        data.clear();
    }

    MakePlayerInvited(data, m_name, targetName);
    SendToOne(data, guid);
}

void Channel::SendToOne(WorldPacket const& data, ObjectGuid receiver) const
{
    if (Player* player = sObjectMgr.GetPlayer(receiver))
        player->GetSession()->SendPacket(data);
}

void Channel::SendToAll(WorldPacket const& data) const
{
    for (PlayerList::const_iterator i = m_players.begin(); i != m_players.end(); ++i)
        SendToOne(data, i->first);
}

void Channel::SendMessage(WorldPacket const& data, ObjectGuid sender) const
{
    for (PlayerList::const_iterator i = m_players.begin(); i != m_players.end(); ++i)
        if (Player* plr = sObjectMgr.GetPlayer(i->first))
            if (!sender || !plr->GetSocial()->HasIgnore(sender))
                plr->GetSession()->SendPacket(data);
}

void Channel::Voice(ObjectGuid /*guid1*/, ObjectGuid /*guid2*/) const
{
}

void Channel::DeVoice(ObjectGuid /*guid1*/, ObjectGuid /*guid2*/) const
{
}

void Channel::JoinNotify(ObjectGuid guid)
{
    WorldPacket data;

    if (IsConstant())
        data.Initialize(SMSG_USERLIST_ADD, 8 + 1 + 1 + 4 + GetName().size() + 1);
    else
        data.Initialize(SMSG_USERLIST_UPDATE, 8 + 1 + 1 + 4 + GetName().size() + 1);

    data << ObjectGuid(guid);
    data << uint8(GetPlayerFlags(guid));
    data << uint8(GetFlags());
    data << uint32(GetNumPlayers());
    data << GetName();
    SendToAll(data);
}

void Channel::LeaveNotify(ObjectGuid guid)
{
    WorldPacket data(SMSG_USERLIST_REMOVE, 8 + 1 + 4 + GetName().size() + 1);
    data << ObjectGuid(guid);
    data << uint8(GetFlags());
    data << uint32(GetNumPlayers());
    data << GetName();
    SendToAll(data);
}

void Channel::MakeNotifyPacket(WorldPacket& data, const std::string& channel, ChatNotify type)
{
    data.Initialize(SMSG_CHANNEL_NOTIFY, 1 + channel.size() + 1);
    data << uint8(type);
    data << channel;
}

void Channel::MakeJoined(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_JOINED_NOTICE);
    data << guid;
}

void Channel::MakeLeft(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_LEFT_NOTICE);
    data << guid;
}

void Channel::MakeYouJoined(WorldPacket& data, const std::string& channel, const Channel& which)
{
    MakeNotifyPacket(data, channel, CHAT_YOU_JOINED_NOTICE);
    data << uint8(which.GetFlags());
    data << uint32(which.GetChannelId());
    data << uint32(0);                                      // channel index (when split occurs due to player count) | 频道分线索引（人数过多拆分子频道时使用）
}

void Channel::MakeYouLeft(WorldPacket& data, const std::string& channel, const Channel& which)
{
    MakeNotifyPacket(data, channel, CHAT_YOU_LEFT_NOTICE);
    data << uint32(which.GetChannelId());
    data << uint8(0);                                       // left: 0x00, suspended: 0x01 | 离开类型：0x00 为退出，0x01 为挂起
}

void Channel::MakeWrongPassword(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_WRONG_PASSWORD_NOTICE);
}

void Channel::MakeNotMember(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_NOT_MEMBER_NOTICE);
}

void Channel::MakeNotModerator(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_NOT_MODERATOR_NOTICE);
}

void Channel::MakePasswordChanged(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_PASSWORD_CHANGED_NOTICE);
    data << guid;
}

void Channel::MakeOwnerChanged(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_OWNER_CHANGED_NOTICE);
    data << guid;
}

void Channel::MakePlayerNotFound(WorldPacket& data, const std::string& channel, const std::string& name)
{
    MakeNotifyPacket(data, channel, CHAT_PLAYER_NOT_FOUND_NOTICE);
    data << name;
}

void Channel::MakeNotOwner(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_NOT_OWNER_NOTICE);
}

void Channel::MakeChannelOwner(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    std::string name;

    if (!guid)
        name = "Nobody";
    else if (!sObjectMgr.GetPlayerNameByGUID(guid, name) || name.empty())
        name = "PLAYER_NOT_FOUND";

    MakeNotifyPacket(data, channel, CHAT_CHANNEL_OWNER_NOTICE);
    data << name;
}

void Channel::MakeModeChange(WorldPacket& data, const std::string& channel, const ObjectGuid& guid, uint8 oldflags, uint8 newFlags)
{
    MakeNotifyPacket(data, channel, CHAT_MODE_CHANGE_NOTICE);
    data << guid;
    data << uint8(oldflags);
    data << uint8(newFlags);
}

void Channel::MakeAnnouncementsOn(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_ANNOUNCEMENTS_ON_NOTICE);
    data << guid;
}

void Channel::MakeAnnouncementsOff(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_ANNOUNCEMENTS_OFF_NOTICE);
    data << guid;
}

void Channel::MakeModerationOn(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_MODERATION_ON_NOTICE);
    data << guid;
}

void Channel::MakeModerationOff(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_MODERATION_OFF_NOTICE);
    data << guid;
}

void Channel::MakeMuted(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_MUTED_NOTICE);
}

void Channel::MakePlayerKicked(WorldPacket& data, const std::string& channel, const ObjectGuid& target, const ObjectGuid& source)
{
    MakeNotifyPacket(data, channel, CHAT_PLAYER_KICKED_NOTICE);
    data << target;
    data << source;
}

void Channel::MakeBanned(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_BANNED_NOTICE);
}

void Channel::MakePlayerBanned(WorldPacket& data, const std::string& channel, const ObjectGuid& target, const ObjectGuid& source)
{
    MakeNotifyPacket(data, channel, CHAT_PLAYER_BANNED_NOTICE);
    data << target;
    data << source;
}

void Channel::MakePlayerUnbanned(WorldPacket& data, const std::string& channel, const ObjectGuid& target, const ObjectGuid& source)
{
    MakeNotifyPacket(data, channel, CHAT_PLAYER_UNBANNED_NOTICE);
    data << target;
    data << source;
}

void Channel::MakePlayerNotBanned(WorldPacket& data, const std::string& channel, const std::string& name)
{
    MakeNotifyPacket(data, channel, CHAT_PLAYER_NOT_BANNED_NOTICE);
    data << name;
}

void Channel::MakePlayerAlreadyMember(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_PLAYER_ALREADY_MEMBER_NOTICE);
    data << guid;
}

void Channel::MakeInvite(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_INVITE_NOTICE);
    data << guid;
}

void Channel::MakeInviteWrongFaction(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_INVITE_WRONG_FACTION_NOTICE);
}

void Channel::MakeWrongFaction(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_WRONG_FACTION_NOTICE);
}

void Channel::MakeInvalidName(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_INVALID_NAME_NOTICE);
}

void Channel::MakeNotModerated(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_NOT_MODERATED_NOTICE);
}

void Channel::MakePlayerInvited(WorldPacket& data, const std::string& channel, const std::string& name)
{
    MakeNotifyPacket(data, channel, CHAT_PLAYER_INVITED_NOTICE);
    data << name;
}

void Channel::MakePlayerInviteBanned(WorldPacket& data, const std::string& channel, const std::string& name)
{
    MakeNotifyPacket(data, channel, CHAT_PLAYER_INVITE_BANNED_NOTICE);
    data << name;
}

void Channel::MakeThrottled(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_THROTTLED_NOTICE);
}

void Channel::MakeNotInArea(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_NOT_IN_AREA_NOTICE);
}

void Channel::MakeNotInLFG(WorldPacket& data, const std::string& channel)
{
    MakeNotifyPacket(data, channel, CHAT_NOT_IN_LFG_NOTICE);
}

void Channel::MakeVoiceOn(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_VOICE_ON_NOTICE);
    data << guid;
}

void Channel::MakeVoiceOff(WorldPacket& data, const std::string& channel, const ObjectGuid& guid)
{
    MakeNotifyPacket(data, channel, CHAT_VOICE_OFF_NOTICE);
    data << guid;
}

/// 推选新的频道所有者：优先从现有管理员中选取，否则取成员列表首位
ObjectGuid Channel::SelectNewOwner() const
{
    // Prioritise moderators for owner appointment | 优先选择现有频道管理员继任所有者
    for (auto itr = m_players.begin(); itr != m_players.end(); ++itr)
    {
        if ((*itr).second.IsModerator())
            return (*itr).second.player;
    }

    return (m_players.empty() ? ObjectGuid() : m_players.begin()->second.player);
}

/// 内部方法：直接更新成员权限标志位并全频道广播变更
void Channel::SetModeFlags(ObjectGuid guid, ChannelMemberFlags flags, bool set)
{
    // Restrict input flags to currently supported by this method | 限制为本方法当前支持的标志位
    flags = ChannelMemberFlags(uint8(flags) & (MEMBER_FLAG_MODERATOR | MEMBER_FLAG_MUTED));

    if (flags && m_players[guid].HasFlag(flags) != set)
    {
        uint8 oldFlag = GetPlayerFlags(guid);
        m_players[guid].SetFlag(flags, set);

        WorldPacket data;
        MakeModeChange(data, m_name, guid, oldFlag, GetPlayerFlags(guid));
        SendToAll(data);
    }
}

/// 内部方法：切换频道所有者并同步更新新旧所有者的管理员与所有者标志
void Channel::SetOwner(ObjectGuid guid, bool exclaim)
{
    if (m_ownerGuid)
    {
        // [] will re-add player after it possible removed | 使用 find 避免 map::operator[] 将已移除的玩家重新插入
        PlayerList::iterator p_itr = m_players.find(m_ownerGuid);
        if (p_itr != m_players.end())
        {
            // old owner retains own moderator powers on transfer to another player only | 仅在转让给其他玩家时保留原所有者的管理员权限
            p_itr->second.SetModerator(bool(guid));

            uint8 oldFlag = p_itr->second.flags;
            p_itr->second.SetOwner(false);

            WorldPacket data;
            MakeModeChange(data, m_name, guid, oldFlag, GetPlayerFlags(guid));
            SendToAll(data);
        }
    }

    m_ownerGuid = guid;

    if (m_ownerGuid)
    {
        // new owner receives moderator powers as well | 新所有者同时自动获得管理员权限
        m_players[m_ownerGuid].SetModerator(true);

        uint8 oldFlag = GetPlayerFlags(m_ownerGuid);
        m_players[m_ownerGuid].SetOwner(true);

        WorldPacket data;
        MakeModeChange(data, m_name, guid, oldFlag, GetPlayerFlags(guid));
        SendToAll(data);
    }

    if (exclaim)
    {
        WorldPacket data;
        MakeOwnerChanged(data, m_name, guid);
        SendToAll(data);
    }
}

/// 设置或取消自定义频道的静态公共频道属性（剥夺所有者/管理员并关闭审核与进出通告）
bool Channel::SetStatic(bool state, bool command/* = false*/)
{
    // Only custom channels can be set to static | 仅自定义频道允许转换为静态频道
    if (IsConstant() || !HasFlag(CHANNEL_FLAG_CUSTOM) || m_static == state)
        return false;

    // This conversion cannot be performed if password is set - it has to be removed first | 设有密码的频道不可转为静态，须先清除密码
    if (state && !m_password.empty())
        return false;

    // Treshold for auto-conversion | 自动转换的人数阈值配置
    const uint32 treshold = sWorld.getConfig(CONFIG_UINT32_CHANNEL_STATIC_AUTO_TRESHOLD);

    // Detect auto-converion and reaching treshold | 非命令手动触发时需检查人数是否达到自动转换阈值
    if (!command && (!treshold || state != (GetNumPlayers() >= treshold)))
        return false;

    // Strip moderator privileges | 转为静态时清除所有成员的管理员权限
    if (state)
    {
        for (PlayerList::const_iterator i = m_players.begin(); i != m_players.end(); ++i)
        {
            if (i->second.IsModerator())
                SetModeFlags(i->second.player, MEMBER_FLAG_MODERATOR, false);
        }
    }

    // Unset/Set channel owner | 静态频道清除所有者；取消静态时重新指派所有者
    if (state == bool(m_ownerGuid))
        SetOwner(state ? ObjectGuid() : SelectNewOwner());

    // Disable premoderation mode on conversion to static | 转为静态时关闭发言审核模式
    if (state && m_moderation)
    {
        m_moderation = false;

        WorldPacket data;
        MakeModerationOff(data, m_name, ObjectGuid());
        SendToAll(data);
    }

    // Disable announcements on conversion to static | 转为静态时关闭进出频道通告
    if (state && m_announcements)
    {
        m_announcements = false;

        WorldPacket data;
        MakeAnnouncementsOff(data, m_name, ObjectGuid());
        SendToAll(data);
    }

    m_static = state;

    return true;
}
