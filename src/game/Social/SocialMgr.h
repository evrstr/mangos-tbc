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

#ifndef __MANGOS_SOCIALMGR_H
#define __MANGOS_SOCIALMGR_H

#include "Database/DatabaseEnv.h"
#include "Entities/ObjectGuid.h"
#include "Globals/EnumFlag.h"

class SocialMgr;
class PlayerSocial;
class Player;
class WorldPacket;

// 好友在线状态枚举
enum FriendStatus
{
    FRIEND_STATUS_OFFLINE   = 0,                            // 离线
    FRIEND_STATUS_ONLINE    = 1,                            // 在线
    FRIEND_STATUS_AFK       = 2,                            // 暂离（AFK）
    FRIEND_STATUS_UNK3      = 3,                            // 未知状态 3
    FRIEND_STATUS_DND       = 4                             // 请勿打扰（DND）
};

// 社交关系标志位（可按位叠加）
enum SocialFlag
{
    SOCIAL_FLAG_FRIEND      = 0x01,                         // 好友
    SOCIAL_FLAG_IGNORED     = 0x02,                         // 屏蔽（黑名单）
    SOCIAL_FLAG_MUTED       = 0x04,                         // 语音/文字静音
};

// 单个社交联系人详情（状态、标志、所在区域、等级、职业及备注）
struct FriendInfo
{
    FriendStatus Status;
    uint32 Flags;
    uint32 Area;
    uint32 Level;
    uint32 Class;
    std::string Note;

    FriendInfo() :
        Status(FRIEND_STATUS_OFFLINE),
        Flags(0),
        Area(0),
        Level(0),
        Class(0)
    {}

    FriendInfo(uint32 flags, const std::string& note) :
        Status(FRIEND_STATUS_OFFLINE),
        Flags(flags),
        Area(0),
        Level(0),
        Class(0),
        Note(note)
    {}
};

typedef std::map<uint32, FriendInfo> PlayerSocialMap;
typedef std::map<uint32, PlayerSocial> SocialMap;

/// Results of friend related commands (好友/屏蔽/静音操作结果代码，对应 SMSG_FRIEND_STATUS)
enum FriendsResult
{
    FRIEND_DB_ERROR         = 0x00,                         // ERR_FRIEND_NOT_FOUND (数据库错误)
    FRIEND_LIST_FULL        = 0x01,                         // 好友列表已满
    FRIEND_ONLINE           = 0x02,                         // 好友上线
    FRIEND_OFFLINE          = 0x03,                         // 好友下线
    FRIEND_NOT_FOUND        = 0x04,                         // ERR_FRIEND_NOT_FOUND (未找到玩家)
    FRIEND_REMOVED          = 0x05,                         // 已移除好友
    FRIEND_ADDED_ONLINE     = 0x06,                         // ERR_FRIEND_ADDED_S (添加好友成功-在线)
    FRIEND_ADDED_OFFLINE    = 0x07,                         // 添加好友成功-离线
    FRIEND_ALREADY          = 0x08,                         // 已在好友列表中
    FRIEND_SELF             = 0x09,                         // 不能添加自己为好友
    FRIEND_ENEMY            = 0x0A,                         // 不能添加敌对阵营玩家
    FRIEND_IGNORE_FULL      = 0x0B,                         // 屏蔽列表已满
    FRIEND_IGNORE_SELF      = 0x0C,                         // 不能屏蔽自己
    FRIEND_IGNORE_NOT_FOUND = 0x0D,                         // 未找到要屏蔽的玩家
    FRIEND_IGNORE_ALREADY   = 0x0E,                         // 已在屏蔽列表中
    FRIEND_IGNORE_ADDED     = 0x0F,                         // 已加入屏蔽列表
    FRIEND_IGNORE_REMOVED   = 0x10,                         // 已移出屏蔽列表
    FRIEND_IGNORE_AMBIGUOUS = 0x11,                         // That name is ambiguous, type more of the player's server name (名称存在歧义)
    FRIEND_MUTE_FULL        = 0x12,                         // 静音列表已满
    FRIEND_MUTE_SELF        = 0x13,                         // 不能静音自己
    FRIEND_MUTE_NOT_FOUND   = 0x14,                         // 未找到静音目标
    FRIEND_MUTE_ALREADY     = 0x15,                         // 已在静音列表中
    FRIEND_MUTE_ADDED       = 0x16,                         // 已加入静音列表
    FRIEND_MUTE_REMOVED     = 0x17,                         // 已移出静音列表
    FRIEND_MUTE_AMBIGUOUS   = 0x18,                         // ERR_VOICE_IGNORE_AMBIGUOUS (静音目标名称有歧义)
    FRIEND_UNK7             = 0x19,                         // ERR_MAX_VALUE (nothing is showed) (无提示信息)
    FRIEND_UNKNOWN          = 0x1A                          // Unknown friend response from server (未知错误)
};

#define SOCIALMGR_FRIEND_LIMIT  50                          // 好友上限 50 人
#define SOCIALMGR_IGNORE_LIMIT  25                          // checked max for 2.4.3, list tail not show if more (2.4.3 客户端屏蔽列表上限 25 人)

// 联系人列表数据包含的子列表掩码
enum class SocialListFlags : uint32
{
    FRIEND_LIST = 0x1,
    IGNORE_LIST = 0x2,
    MUTE_LIST   = 0x4,
};

DEFINE_ENUM_FLAG(SocialListFlags);

// 单个玩家的社交关系数据容器：维护好友/屏蔽/静音映射及数据库同步
class PlayerSocial
{
        friend class SocialMgr;
    public:
        PlayerSocial();
        ~PlayerSocial();
        // adding/removing (增删社交联系人与备注)
        bool AddToSocialList(ObjectGuid friend_guid, bool ignore);
        void RemoveFromSocialList(ObjectGuid friend_guid, bool ignore);
        void SetFriendNote(ObjectGuid friend_guid, std::string note);
        // Packet send's (下发完整联系人列表)
        void SendSocialList();
        // Misc (辅助查询接口)
        bool HasFriend(ObjectGuid friend_guid);
        bool HasIgnore(ObjectGuid ignore_guid);
        void SetPlayerGuid(ObjectGuid guid) { m_playerLowGuid = guid.GetCounter(); }
        uint32 GetNumberOfSocialsWithFlag(SocialFlag flag);
    private:
        PlayerSocialMap m_playerSocialMap;
        uint32 m_playerLowGuid;
};

// 全局社交管理器：维护全服玩家社交表并负责好友上下线状态广播
class SocialMgr
{
    public:
        SocialMgr();
        ~SocialMgr();
        // Misc
        void RemovePlayerSocial(uint32 guid) { m_socialMap.erase(guid); }

        void GetFriendInfo(Player* player, uint32 friend_lowguid, FriendInfo& friendInfo) const; // 查询好友实时在线状态与地区等级职业信息
        // Packet management (好友状态数据包构造与广播)
        static void MakeFriendStatusPacket(FriendsResult result, uint32 guid, WorldPacket& data);
        void SendFriendStatus(Player* player, FriendsResult result, ObjectGuid friend_guid, bool broadcast);
        void BroadcastToFriendListers(Player* player, WorldPacket const& packet) const;
        // Loading (从数据库查询结果加载玩家社交记录)
        PlayerSocial* LoadFromDB(std::unique_ptr<QueryResult> queryResult, ObjectGuid guid);
    private:
        SocialMap m_socialMap;
};

#define sSocialMgr MaNGOS::Singleton<SocialMgr>::Instance()
#endif
