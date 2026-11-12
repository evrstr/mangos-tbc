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

#ifndef _CHANNEL_H
#define _CHANNEL_H

#include "Common.h"
#include "Entities/ObjectGuid.h"
#include "Server/WorldPacket.h"
#include "Entities/Player.h"

#include <map>

/// 聊天频道通知类型（对应 SMSG_CHANNEL_NOTIFY 协议状态码）
enum ChatNotify : uint8
{
    CHAT_JOINED_NOTICE                = 0x00,               //+ "%s joined channel."; | 玩家加入频道
    CHAT_LEFT_NOTICE                  = 0x01,               //+ "%s left channel."; | 玩家离开频道
    // CHAT_SUSPENDED_NOTICE             = 0x01,            // "%s left channel."; | 暂停/离开频道
    CHAT_YOU_JOINED_NOTICE            = 0x02,               //+ "Joined Channel: [%s]"; -- You joined | 你加入了频道
    // CHAT_YOU_CHANGED_NOTICE           = 0x02,            // "Changed Channel: [%s]"; | 你切换了频道
    CHAT_YOU_LEFT_NOTICE              = 0x03,               //+ "Left Channel: [%s]"; -- You left | 你离开了频道
    CHAT_WRONG_PASSWORD_NOTICE        = 0x04,               //+ "Wrong password for %s."; | 频道密码错误
    CHAT_NOT_MEMBER_NOTICE            = 0x05,               //+ "Not on channel %s."; | 不在该频道中
    CHAT_NOT_MODERATOR_NOTICE         = 0x06,               //+ "Not a moderator of %s."; | 非频道管理员
    CHAT_PASSWORD_CHANGED_NOTICE      = 0x07,               //+ "[%s] Password changed by %s."; | 频道密码已修改
    CHAT_OWNER_CHANGED_NOTICE         = 0x08,               //+ "[%s] Owner changed to %s."; | 频道所有者已变更
    CHAT_PLAYER_NOT_FOUND_NOTICE      = 0x09,               //+ "[%s] Player %s was not found."; | 未找到目标玩家
    CHAT_NOT_OWNER_NOTICE             = 0x0A,               //+ "[%s] You are not the channel owner."; | 你不是频道所有者
    CHAT_CHANNEL_OWNER_NOTICE         = 0x0B,               //+ "[%s] Channel owner is %s."; | 当前频道所有者查询结果
    CHAT_MODE_CHANGE_NOTICE           = 0x0C,               //? | 成员权限模式变更
    CHAT_ANNOUNCEMENTS_ON_NOTICE      = 0x0D,               //+ "[%s] Channel announcements enabled by %s."; | 已开启加入/离开通告
    CHAT_ANNOUNCEMENTS_OFF_NOTICE     = 0x0E,               //+ "[%s] Channel announcements disabled by %s."; | 已关闭加入/离开通告
    CHAT_MODERATION_ON_NOTICE         = 0x0F,               //+ "[%s] Channel moderation enabled by %s."; | 已开启频道发言审核（仅管理员/有发言权者可发言）
    CHAT_MODERATION_OFF_NOTICE        = 0x10,               //+ "[%s] Channel moderation disabled by %s."; | 已关闭频道发言审核
    CHAT_MUTED_NOTICE                 = 0x11,               //+ "[%s] You do not have permission to speak."; | 你没有发言权限（被禁言或无发言权）
    CHAT_PLAYER_KICKED_NOTICE         = 0x12,               //? "[%s] Player %s kicked by %s."; | 玩家被踢出频道
    CHAT_BANNED_NOTICE                = 0x13,               //+ "[%s] You are banned from that channel."; | 你已被该频道封禁
    CHAT_PLAYER_BANNED_NOTICE         = 0x14,               //? "[%s] Player %s banned by %s."; | 玩家被封禁
    CHAT_PLAYER_UNBANNED_NOTICE       = 0x15,               //? "[%s] Player %s unbanned by %s."; | 玩家被解除封禁
    CHAT_PLAYER_NOT_BANNED_NOTICE     = 0x16,               //+ "[%s] Player %s is not banned."; | 目标玩家未被封禁
    CHAT_PLAYER_ALREADY_MEMBER_NOTICE = 0x17,               //+ "[%s] Player %s is already on the channel."; | 目标玩家已在频道中
    CHAT_INVITE_NOTICE                = 0x18,               //+ "%2$s has invited you to join the channel '%1$s'."; | 收到加入频道邀请
    CHAT_INVITE_WRONG_FACTION_NOTICE  = 0x19,               //+ "Target is in the wrong alliance for %s."; | 受邀玩家阵营不符
    CHAT_WRONG_FACTION_NOTICE         = 0x1A,               //+ "Wrong alliance for %s."; | 阵营不符无法操作
    CHAT_INVALID_NAME_NOTICE          = 0x1B,               //+ "Invalid channel name"; | 无效频道名称
    CHAT_NOT_MODERATED_NOTICE         = 0x1C,               //+ "%s is not moderated"; | 频道未处于审核模式
    CHAT_PLAYER_INVITED_NOTICE        = 0x1D,               //+ "[%s] You invited %s to join the channel"; | 已向玩家发送频道邀请
    CHAT_PLAYER_INVITE_BANNED_NOTICE  = 0x1E,               //+ "[%s] %s has been banned."; | 目标已被封禁，无法邀请
    CHAT_THROTTLED_NOTICE             = 0x1F,               //+ "[%s] The number of messages that can be sent to this channel is limited, please wait to send another message."; | 发言过于频繁被限流
    CHAT_NOT_IN_AREA_NOTICE           = 0x20,               //+ "[%s] You are not in the correct area for this channel."; -- The user is trying to send a chat to a zone specific channel, and they're not physically in that zone. | 不在对应区域内，无法向该区域频道发言
    CHAT_NOT_IN_LFG_NOTICE            = 0x21,               //+ "[%s] You must be queued in looking for group before joining this channel."; -- The user must be in the looking for group system to join LFG chat channels. | 必须处于寻找组队队列中才能加入 LFG 频道
    CHAT_VOICE_ON_NOTICE              = 0x22,               //+ "[%s] Channel voice enabled by %s."; | 已启用频道语音
    CHAT_VOICE_OFF_NOTICE             = 0x23                //+ "[%s] Channel voice disabled by %s."; | 已禁用频道语音
};

/// 单个聊天频道实体：维护成员列表、封禁列表、密码、权限标志与频道消息广播
class Channel
{
    public:
        /// 客户端频道类型标志位
        enum ChannelFlags
        {
            CHANNEL_FLAG_NONE       = 0x00,
            CHANNEL_FLAG_CUSTOM     = 0x01,
            // 0x02
            CHANNEL_FLAG_TRADE      = 0x04,
            CHANNEL_FLAG_NOT_LFG    = 0x08,
            CHANNEL_FLAG_GENERAL    = 0x10,
            CHANNEL_FLAG_CITY       = 0x20,
            CHANNEL_FLAG_LFG        = 0x40,
            CHANNEL_FLAG_VOICE      = 0x80
                                      // General                  0x18 = 0x10 | 0x08 | 综合频道
                                      // Trade                    0x3C = 0x20 | 0x10 | 0x08 | 0x04 | 交易频道
                                      // LocalDefence             0x18 = 0x10 | 0x08 | 本地防务频道
                                      // GuildRecruitment         0x38 = 0x20 | 0x10 | 0x08 | 公会招募频道
                                      // LookingForGroup          0x50 = 0x40 | 0x10 | 寻求组队频道
        };

        /// ChatChannels.dbc 中定义的频道属性标志
        enum ChannelDBCFlags
        {
            CHANNEL_DBC_FLAG_NONE       = 0x00000,
            CHANNEL_DBC_FLAG_INITIAL    = 0x00001,          // General, Trade, LocalDefense, LFG | 综合、交易、本地防务、寻求组队
            CHANNEL_DBC_FLAG_ZONE_DEP   = 0x00002,          // General, Trade, LocalDefense, GuildRecruitment | 依赖区域（随区域切换）
            CHANNEL_DBC_FLAG_GLOBAL     = 0x00004,          // WorldDefense | 世界防务
            CHANNEL_DBC_FLAG_TRADE      = 0x00008,          // Trade | 交易频道
            CHANNEL_DBC_FLAG_CITY_ONLY  = 0x00010,          // Trade, GuildRecruitment | 仅限主城
            CHANNEL_DBC_FLAG_CITY_ONLY2 = 0x00020,          // Trade, GuildRecruitment | 仅限主城2
            CHANNEL_DBC_FLAG_DEFENSE    = 0x10000,          // LocalDefense, WorldDefense | 防务频道
            CHANNEL_DBC_FLAG_GUILD_REQ  = 0x20000,          // GuildRecruitment | 公会招募
            CHANNEL_DBC_FLAG_LFG        = 0x40000           // LookingForGroup | 寻求组队
        };

        /// 频道成员权限与状态标志
        enum ChannelMemberFlags : uint8
        {
            MEMBER_FLAG_NONE        = 0x00,
            MEMBER_FLAG_OWNER       = 0x01,                 /// 频道所有者
            MEMBER_FLAG_MODERATOR   = 0x02,                 /// 频道管理员
            MEMBER_FLAG_VOICED      = 0x04,                 /// 拥有语音/发言权限
            MEMBER_FLAG_MUTED       = 0x08,                 /// 被禁言
            MEMBER_FLAG_CUSTOM      = 0x10,                 /// 自定义标志
            MEMBER_FLAG_MIC_MUTED   = 0x20,                 /// 麦克风静音
            // 0x40
            // 0x80
        };

    private:

        /// 频道内成员信息记录
        struct PlayerInfo
        {
            ObjectGuid player;
            uint8 flags;

            inline bool HasFlag(uint8 flag) const { return (flags & flag) != 0; }
            void SetFlag(uint8 flag, bool state) { if (state) flags |= flag; else flags &= ~flag; }
            inline bool IsOwner() const { return HasFlag(MEMBER_FLAG_OWNER); }
            inline void SetOwner(bool state) { SetFlag(MEMBER_FLAG_OWNER, state); }
            inline bool IsModerator() const { return HasFlag(MEMBER_FLAG_MODERATOR); }
            inline void SetModerator(bool state) { SetFlag(MEMBER_FLAG_MODERATOR, state); }
            inline bool IsMuted() const { return HasFlag(MEMBER_FLAG_MUTED); }
            inline void SetMuted(bool state) { SetFlag(MEMBER_FLAG_MUTED, state); }
        };

        typedef std::map<ObjectGuid, PlayerInfo> PlayerList;

    public:
        Channel(const std::string& name, uint32 channel_id = 0);
        std::string GetName() const { return m_name; }
        uint32 GetChannelId() const { return (m_entry ? m_entry->ChannelID : 0); }
        const ChatChannelsEntry* GetChannelEntry() const { return m_entry; }
        /// 是否为 DBC 内置常驻公共频道
        bool IsConstant() const { return m_entry; }
        /// 是否为自定义静态全局频道
        bool IsStatic() const { return m_static; }
        bool IsLFG() const { return (GetFlags() & CHANNEL_FLAG_LFG) != 0; }
        inline bool IsPublic() const { return (IsConstant() || IsStatic()); }
        std::string GetPassword() const { return m_password; }
        size_t GetNumPlayers() const { return m_players.size(); }
        uint8 GetFlags() const { return m_flags; }
        bool HasFlag(uint8 flag) const { return (m_flags & flag) != 0; }

        /// 玩家加入频道（校验密码、封禁、LFG/阵营等条件）
        void Join(Player* player, const char* password);
        /// 玩家离开频道（所有者离开时自动转移所有权）
        void Leave(Player* player, bool send = true);
        /// 踢出或封禁目标玩家
        void KickOrBan(Player* player, const char* targetName, bool ban);
        void Kick(Player* player, const char* targetName) { KickOrBan(player, targetName, false); }
        void Ban(Player* player, const char* targetName) { KickOrBan(player, targetName, true); }
        /// 解除目标玩家的频道封禁
        void UnBan(Player* player, const char* targetName);
        /// 设置频道密码
        void SetPassword(Player* player, const char* password);
        /// 设置或撤销成员的权限/禁言标志
        void SetModeFlags(Player* player, const char* targetName, ChannelMemberFlags flags, bool set);
        inline void SetModerator(Player* player, const char* targetName, bool set) { SetModeFlags(player, targetName, MEMBER_FLAG_MODERATOR, set); }
        inline void SetMute(Player* player, const char* targetName, bool set) { SetModeFlags(player, targetName, MEMBER_FLAG_MUTED, set); }
        /// 转让频道所有权给指定玩家
        void SetOwner(Player* player, const char* targetName);
        /// 向客户端发送当前频道所有者信息
        void SendChannelOwnerResponse(Player* player) const;
        /// 向客户端发送频道成员列表
        void SendChannelListResponse(Player* player, bool display = false);
        /// 切换频道进出通告开关
        void ToggleAnnouncements(Player* player);
        /// 切换频道发言审核模式开关
        void ToggleModeration(Player* player);
        /// 在频道内发言并广播给频道成员
        void Say(Player* player, const char* text, uint32 lang);
        /// 邀请目标玩家加入频道
        void Invite(Player* player, const char* targetName);
        /// 启用/取消某玩家在审核频道中的语音/发言权
        void Voice(ObjectGuid guid1, ObjectGuid guid2) const;
        void DeVoice(ObjectGuid guid1, ObjectGuid guid2) const;
        void JoinNotify(ObjectGuid guid);                   // invisible notify | 静默加入通知（预留）
        void LeaveNotify(ObjectGuid guid);                  // invisible notify | 静默离开通知（预留）

        // initial packet data (notify type and channel name) | 构造通知包头部（通知类型与频道名称）
        static void MakeNotifyPacket(WorldPacket& data, const std::string& channel, ChatNotify type);
        // type specific packet data | 按通知类型填充附加数据包内容
        static void MakeJoined(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                                      //+ 0x00
        static void MakeLeft(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                                        //+ 0x01
        static void MakeYouJoined(WorldPacket& data, const std::string& channel, const Channel& which);                                     //+ 0x02
        static void MakeYouLeft(WorldPacket& data, const std::string& channel, const Channel& which);                                       //+ 0x03
        static void MakeWrongPassword(WorldPacket& data, const std::string& channel);                                                       //? 0x04
        static void MakeNotMember(WorldPacket& data, const std::string& channel);                                                           //? 0x05
        static void MakeNotModerator(WorldPacket& data, const std::string& channel);                                                        //? 0x06
        static void MakePasswordChanged(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                             //+ 0x07
        static void MakeOwnerChanged(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                                //? 0x08
        static void MakePlayerNotFound(WorldPacket& data, const std::string& channel, const std::string& name);                             //+ 0x09
        static void MakeNotOwner(WorldPacket& data, const std::string& channel);                                                            //? 0x0A
        static void MakeChannelOwner(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                                //? 0x0B
        static void MakeModeChange(WorldPacket& data, const std::string& channel, const ObjectGuid& guid, uint8 oldflags, uint8 newFlags);  //+ 0x0C
        static void MakeAnnouncementsOn(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                             //+ 0x0D
        static void MakeAnnouncementsOff(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                            //+ 0x0E
        static void MakeModerationOn(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                                //+ 0x0F
        static void MakeModerationOff(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                               //+ 0x10
        static void MakeMuted(WorldPacket& data, const std::string& channel);                                                               //? 0x11
        static void MakePlayerKicked(WorldPacket& data, const std::string& channel, const ObjectGuid& target, const ObjectGuid& source);    //? 0x12
        static void MakeBanned(WorldPacket& data, const std::string& channel);                                                              //? 0x13
        static void MakePlayerBanned(WorldPacket& data, const std::string& channel, const ObjectGuid& target, const ObjectGuid& source);    //? 0x14
        static void MakePlayerUnbanned(WorldPacket& data, const std::string& channel, const ObjectGuid& target, const ObjectGuid& source);  //? 0x15
        static void MakePlayerNotBanned(WorldPacket& data, const std::string& channel, const std::string& name);                            //? 0x16
        static void MakePlayerAlreadyMember(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                         //+ 0x17
        static void MakeInvite(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                                      //? 0x18
        static void MakeInviteWrongFaction(WorldPacket& data, const std::string& channel);                                                  //? 0x19
        static void MakeWrongFaction(WorldPacket& data, const std::string& channel);                                                        //? 0x1A
        static void MakeInvalidName(WorldPacket& data, const std::string& channel);                                                         //? 0x1B
        static void MakeNotModerated(WorldPacket& data, const std::string& channel);                                                        //? 0x1C
        static void MakePlayerInvited(WorldPacket& data, const std::string& channel, const std::string& name);                              //+ 0x1D
        static void MakePlayerInviteBanned(WorldPacket& data, const std::string& channel, const std::string& name);                         //? 0x1E
        static void MakeThrottled(WorldPacket& data, const std::string& channel);                                                           //? 0x1F
        static void MakeNotInArea(WorldPacket& data, const std::string& channel);                                                           //? 0x20
        static void MakeNotInLFG(WorldPacket& data, const std::string& channel);                                                            //? 0x21
        static void MakeVoiceOn(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                                     //+ 0x22
        static void MakeVoiceOff(WorldPacket& data, const std::string& channel, const ObjectGuid& guid);                                    //+ 0x23

        // Make a custom channel acquire global-like properties | 将自定义频道设为类似全局公共频道的属性（无所有者/持久/无进出通告）
        bool SetStatic(bool state, bool command = false);

    private:
        void SendToOne(WorldPacket const& data, ObjectGuid receiver) const;
        void SendToAll(WorldPacket const& data) const;
        void SendMessage(WorldPacket const& data, ObjectGuid sender) const;

        bool IsOn(ObjectGuid who) const { return m_players.find(who) != m_players.end(); }
        bool IsBanned(ObjectGuid guid) const { return m_banned.find(guid) != m_banned.end(); }

        uint8 GetPlayerFlags(ObjectGuid guid) const
        {
            PlayerList::const_iterator p_itr = m_players.find(guid);
            if (p_itr == m_players.end())
                return 0;

            return p_itr->second.flags;
        }

        ObjectGuid SelectNewOwner() const;

        void SetModeFlags(ObjectGuid guid, ChannelMemberFlags flags, bool set);
        void SetOwner(ObjectGuid guid, bool exclaim = true);

    private:
        std::string                 m_name;
        std::string                 m_password;
        ObjectGuid                  m_ownerGuid;
        PlayerList                  m_players;
        GuidSet                     m_banned;
        const ChatChannelsEntry*    m_entry = nullptr;
        bool                        m_announcements = false;
        bool                        m_moderation = false;
        uint8                       m_flags = CHANNEL_FLAG_NONE;
        // Custom features: | 扩展自定义特性：
        bool                        m_static = false;       /// 是否为静态自定义频道
        bool                        m_realmzone = false;    /// 是否启用服务器区域公共频道特性
};
#endif
