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

#ifndef MANGOSSERVER_ARENATEAM_H
#define MANGOSSERVER_ARENATEAM_H

#include "Common.h"
#include "Entities/ObjectGuid.h"
#include "Globals/SharedDefines.h"

class QueryResult;
class WorldPacket;
class WorldSession;
class Player;

// 竞技场战队命令类型枚举
enum ArenaTeamCommandTypes
{
    ERR_ARENA_TEAM_CREATE_S                 = 0x00,
    ERR_ARENA_TEAM_INVITE_SS                = 0x01,
    ERR_ARENA_TEAM_QUIT_S                   = 0x03,
    ERR_ARENA_TEAM_FOUNDER_S                = 0x0E
};

// 竞技场战队命令错误码枚举
enum ArenaTeamCommandErrors
{
    ERR_ARENA_TEAM_INTERNAL                 = 0x01,
    ERR_ALREADY_IN_ARENA_TEAM               = 0x02,
    ERR_ALREADY_IN_ARENA_TEAM_S             = 0x03,
    ERR_INVITED_TO_ARENA_TEAM               = 0x04,
    ERR_ALREADY_INVITED_TO_ARENA_TEAM_S     = 0x05,
    ERR_ARENA_TEAM_NAME_INVALID             = 0x06,
    ERR_ARENA_TEAM_NAME_EXISTS_S            = 0x07,
    ERR_ARENA_TEAM_LEADER_LEAVE_S           = 0x08,
    ERR_ARENA_TEAM_PERMISSIONS              = 0x08,
    ERR_ARENA_TEAM_PLAYER_NOT_IN_TEAM       = 0x09,
    ERR_ARENA_TEAM_PLAYER_NOT_IN_TEAM_SS    = 0x0A,
    ERR_ARENA_TEAM_PLAYER_NOT_FOUND_S       = 0x0B,
    ERR_ARENA_TEAM_NOT_ALLIED               = 0x0C,
    ERR_ARENA_TEAM_IGNORING_YOU_S           = 0x13,
    ERR_ARENA_TEAM_TARGET_TOO_LOW_S         = 0x15,
    ERR_ARENA_TEAM_TOO_MANY_MEMBERS_S       = 0x16,
};

// 竞技场战队事件通知类型
enum ArenaTeamEvents
{
    ERR_ARENA_TEAM_JOIN_SS                  = 3,            // player name + arena team name (玩家名 + 战队名)
    ERR_ARENA_TEAM_LEAVE_SS                 = 4,            // player name + arena team name (玩家名 + 战队名)
    ERR_ARENA_TEAM_REMOVE_SSS               = 5,            // player name + arena team name + captain name (玩家名 + 战队名 + 队长名)
    ERR_ARENA_TEAM_LEADER_IS_SS             = 6,            // player name + arena team name (玩家名 + 战队名)
    ERR_ARENA_TEAM_LEADER_CHANGED_SSS       = 7,            // old captain + new captain + arena team name (原队长 + 新队长 + 战队名)
    ERR_ARENA_TEAM_DISBANDED_S              = 8             // captain name + arena team name (队长名 + 战队名)
};

/*
need info how to send these ones:
ERR_ARENA_TEAM_YOU_JOIN_S - client show it automatically when accept invite
ERR_ARENA_TEAM_TARGET_TOO_LOW_S
ERR_ARENA_TEAM_TOO_MANY_MEMBERS_S
ERR_ARENA_TEAM_LEVEL_TOO_LOW_I
(待确认以下事件的发送方式：
ERR_ARENA_TEAM_YOU_JOIN_S - 客户端接受邀请时自动显示
ERR_ARENA_TEAM_TARGET_TOO_LOW_S
ERR_ARENA_TEAM_TOO_MANY_MEMBERS_S
ERR_ARENA_TEAM_LEVEL_TOO_LOW_I)
*/

// 竞技场战队统计字段类型
enum ArenaTeamStatTypes
{
    STAT_TYPE_RATING        = 0,
    STAT_TYPE_GAMES_WEEK    = 1,
    STAT_TYPE_WINS_WEEK     = 2,
    STAT_TYPE_GAMES_SEASON  = 3,
    STAT_TYPE_WINS_SEASON   = 4,
    STAT_TYPE_RANK          = 5
};

// 竞技场战队成员信息结构
struct ArenaTeamMember
{
    ObjectGuid guid;
    std::string name;
    uint8 Class;
    uint32 games_week;
    uint32 wins_week;
    uint32 games_season;
    uint32 wins_season;
    uint32 personal_rating;

    void ModifyPersonalRating(Player* plr, int32 mod, uint32 slot); // 修改成员个人竞技场等级并同步至玩家字段
};

// 竞技场战队整体战绩统计
struct ArenaTeamStats
{
    uint32 rating;
    uint32 games_week;
    uint32 wins_week;
    uint32 games_season;
    uint32 wins_season;
    uint32 rank;
};

#define MAX_ARENA_SLOT 3                                    // 0..2 slots (0..2 号战队槽位，对应 2v2/3v3/5v5)

// 竞技场战队类：管理战队创建、解散、成员增减、队徽外观、积分结算与数据库持久化
class ArenaTeam
{
    public:
        ArenaTeam();
        ~ArenaTeam();

        // 创建新战队并写入数据库
        bool Create(ObjectGuid captainGuid, ArenaType type, std::string arenaTeamName);
        // 解散战队并通知成员、清理数据库记录
        void Disband(WorldSession* session);

        typedef std::list<ArenaTeamMember> MemberList;

        uint32 GetId() const              { return m_TeamId; }
        ArenaType GetType() const         { return m_Type; }
        uint8  GetSlot() const            { return GetSlotByType(GetType()); }
        static uint8 GetSlotByType(ArenaType type);
        ObjectGuid GetCaptainGuid() const { return m_CaptainGuid; }
        std::string GetName() const       { return m_Name; }
        const ArenaTeamStats& GetStats() const { return m_stats; }
        void SetStats(uint32 stat_type, uint32 value);
        uint32 GetRating() const          { return m_stats.rating; }

        uint32 GetEmblemStyle() const     { return m_EmblemStyle; }
        uint32 GetEmblemColor() const     { return m_EmblemColor; }
        uint32 GetBorderStyle() const     { return m_BorderStyle; }
        uint32 GetBorderColor() const     { return m_BorderColor; }
        uint32 GetBackgroundColor() const { return m_BackgroundColor; }

        void SetCaptain(ObjectGuid guid);                   // 移交队长权限
        bool AddMember(ObjectGuid playerGuid);              // 添加战队成员
        void DelMember(ObjectGuid guid);                    // 移除战队成员

        void SetEmblem(uint32 backgroundColor, uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor); // 设置队徽样式与颜色

        size_t GetMembersSize() const         { return m_members.size(); }
        size_t GetMaxMembersSize() const      { return size_t(GetType() * 2); }
        bool   Empty() const                  { return m_members.empty(); }
        MemberList& GetMembers()              { return m_members; }
        bool HaveMember(ObjectGuid guid) const;

        ArenaTeamMember* GetMember(ObjectGuid guid)
        {
            for (auto& m_member : m_members)
                if (m_member.guid == guid)
                    return &m_member;

            return nullptr;
        }

        ArenaTeamMember* GetMember(const std::string& name)
        {
            for (auto& m_member : m_members)
                if (m_member.name == name)
                    return &m_member;

            return nullptr;
        }

        bool IsFighting() const;                            // 检查战队当前是否有成员在竞技场战斗中

        bool LoadArenaTeamFromDB(QueryResult* arenaTeamDataResult);    // 从数据库查询结果加载战队基本信息
        bool LoadMembersFromDB(QueryResult* arenaTeamMembersResult);   // 从数据库查询结果加载成员列表

        void SaveToDB();                                    // 保存战队及成员战绩到数据库

        void BroadcastPacket(WorldPacket const& packet) const; // 向在线队员广播数据包

        void BroadcastEvent(ArenaTeamEvents event, ObjectGuid guid, char const* str1 = nullptr, char const* str2 = nullptr, char const* str3 = nullptr);
        void BroadcastEvent(ArenaTeamEvents event, char const* str1 = nullptr, char const* str2 = nullptr, char const* str3 = nullptr)
        {
            BroadcastEvent(event, ObjectGuid(), str1, str2, str3);
        }

        void Roster(WorldSession* session);                 // 发送战队名册信息
        void Query(WorldSession* session) const;            // 响应战队基本信息查询
        void Stats(WorldSession* session) const;            // 发送战队战绩统计
        void InspectStats(WorldSession* session, ObjectGuid guid); // 发送观察目标玩家的战队战绩

        uint32 GetPoints(uint32 MemberRating);              // 根据等级计算可获得的竞技场点数
        float GetChanceAgainst(uint32 own_rating, uint32 enemy_rating) const; // 计算 ELO 胜率期望
        int32 WonAgainst(uint32 againstRating);             // 战队获胜时更新等级并返回变动值
        void MemberWon(Player* plr, uint32 againstRating);  // 获胜成员个人战绩与等级更新
        int32 LostAgainst(uint32 againstRating);            // 战队战败时更新等级并返回变动值
        void MemberLost(Player* plr, uint32 againstRating); // 战败成员个人战绩与等级更新
        void OfflineMemberLost(ObjectGuid guid, uint32 againstRating); // 离线战败成员个人等级与场次更新

        void UpdateArenaPointsHelper(std::map<uint32, uint32>& PlayerPoints); // 周结算时汇总成员应得竞技场点数

        void NotifyStatsChanged();                          // 通知所有在线成员战队战绩已更新

        void FinishWeek();                                  // 重置每周战绩计数
        void FinishGame(int32 mod);                         // 结算单场比赛后的战队等级、场次与排名

        void FinishSeason();                                // 赛季结束重置战绩与等级

        void SetRatingForAll(uint32 rating);                // 统一设置战队及所有成员等级

    protected:

        uint32 m_TeamId;
        ArenaType m_Type;
        std::string m_Name;
        ObjectGuid m_CaptainGuid;

        uint32 m_BackgroundColor; // ARGB format (ARGB 格式背景色)
        uint32 m_EmblemStyle;     // icon id (队徽图标 ID)
        uint32 m_EmblemColor;     // ARGB format (ARGB 格式队徽颜色)
        uint32 m_BorderStyle;     // border image id (边框样式 ID)
        uint32 m_BorderColor;     // ARGB format (ARGB 格式边框颜色)

        MemberList m_members;
        ArenaTeamStats m_stats;
};
#endif
