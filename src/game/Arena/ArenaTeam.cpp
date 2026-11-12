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

#include "Server/WorldPacket.h"
#include "Globals/ObjectMgr.h"
#include "Entities/ObjectGuid.h"
#include "Arena/ArenaTeam.h"
#include "World/World.h"
#include "Entities/Player.h"

// 修改战队成员个人等级（下限为 0），并在玩家在线时更新角色字段
void ArenaTeamMember::ModifyPersonalRating(Player* plr, int32 mod, uint32 slot)
{
    if (int32(personal_rating) + mod < 0)
        personal_rating = 0;
    else
        personal_rating += mod;
    if (plr)
        plr->SetArenaTeamInfoField(slot, ARENA_TEAM_PERSONAL_RATING, personal_rating);
}

ArenaTeam::ArenaTeam()
{
    m_TeamId              = 0;
    m_Type                = ARENA_TYPE_NONE;
    m_BackgroundColor     = 0;                              // background (背景色)
    m_EmblemStyle         = 0;                              // icon (队徽图标)
    m_EmblemColor         = 0;                              // icon color (队徽颜色)
    m_BorderStyle         = 0;                              // border (边框样式)
    m_BorderColor         = 0;                              // border color (边框颜色)
    m_stats.games_week    = 0;
    m_stats.games_season  = 0;
    m_stats.rank          = 0;

    int32 conf_value = sWorld.getConfig(CONFIG_INT32_ARENA_STARTRATING);
    if (conf_value < 0)                                     // -1 = select by season id (-1 表示按赛季编号自动选择初始等级)
    {
        if (sWorld.getConfig(CONFIG_UINT32_ARENA_SEASON_ID) >= 6)
            m_stats.rating    = 0;
        else
            m_stats.rating    = 1500;
    }
    else
        m_stats.rating = uint32(conf_value);

    m_stats.wins_week     = 0;
    m_stats.wins_season   = 0;
}

ArenaTeam::~ArenaTeam()
{
}

// 创建新的竞技场战队并写入数据库，同时将队长添加为首位成员
bool ArenaTeam::Create(ObjectGuid captainGuid, ArenaType type, std::string arenaTeamName)
{
    if (!IsArenaTypeValid(type))
        return false;
    if (!sObjectMgr.GetPlayer(captainGuid))                 // player not exist (队长玩家不在线或不存在)
        return false;
    if (sObjectMgr.GetArenaTeamByName(arenaTeamName))       // arena team with this name already exist (同名战队已存在)
        return false;

    DEBUG_LOG("GUILD: creating arena team %s to leader: %s", arenaTeamName.c_str(), captainGuid.GetString().c_str());

    m_CaptainGuid = captainGuid;
    m_Name = arenaTeamName;
    m_Type = type;

    m_TeamId = sObjectMgr.GenerateArenaTeamId();

    // ArenaTeamName already assigned to ArenaTeam::name, use it to encode string for DB (先保存原始队名，再对字符串进行 SQL 转义)
    CharacterDatabase.escape_string(arenaTeamName);

    CharacterDatabase.BeginTransaction();
    // CharacterDatabase.PExecute("DELETE FROM arena_team WHERE arenateamid='%u'", m_TeamId); - MAX(arenateam)+1 not exist
    CharacterDatabase.PExecute("DELETE FROM arena_team_member WHERE arenateamid='%u'", m_TeamId);
    CharacterDatabase.PExecute("INSERT INTO arena_team (arenateamid,name,captainguid,type,BackgroundColor,EmblemStyle,EmblemColor,BorderStyle,BorderColor) "
                               "VALUES('%u','%s','%u','%u','%u','%u','%u','%u','%u')",
                               m_TeamId, arenaTeamName.c_str(), m_CaptainGuid.GetCounter(), m_Type, m_BackgroundColor, m_EmblemStyle, m_EmblemColor, m_BorderStyle, m_BorderColor);
    CharacterDatabase.PExecute("INSERT INTO arena_team_stats (arenateamid, rating, games_week, wins_week, games_season, wins_season, `rank`) VALUES "
                               "('%u', '%u', '%u', '%u', '%u', '%u', '%u')", m_TeamId, m_stats.rating, m_stats.games_week, m_stats.wins_week, m_stats.games_season, m_stats.wins_season, m_stats.rank);

    CharacterDatabase.CommitTransaction();

    AddMember(m_CaptainGuid);
    return true;
}

// 向战队添加成员（支持在线或离线玩家），并清理其同类型其余请愿签名
bool ArenaTeam::AddMember(ObjectGuid playerGuid)
{
    std::string plName;
    uint8 plClass;

    // arena team is full (can't have more than type * 2 players!) (战队人数已达上限：最多为战队规模的 2 倍)
    if (GetMembersSize() >= GetMaxMembersSize())
        return false;

    Player* pl = sObjectMgr.GetPlayer(playerGuid);
    if (pl)
    {
        if (pl->GetArenaTeamId(GetSlot()))
        {
            sLog.outError("Arena::AddMember() : player already in this sized team");
            return false;
        }

        plClass = pl->getClass();
        plName = pl->GetName();
    }
    else
    {
        //                                                  0     1
        auto queryResult = CharacterDatabase.PQuery("SELECT name, class FROM characters WHERE guid='%u'", playerGuid.GetCounter());
        if (!queryResult)
            return false;

        plName = (*queryResult)[0].GetCppString();
        plClass = (*queryResult)[1].GetUInt8();

        // check if player already in arenateam of that size (检查离线玩家是否已加入同规模战队)
        if (Player::GetArenaTeamIdFromDB(playerGuid, GetType()) != 0)
        {
            sLog.outError("Arena::AddMember() : player %s already in this sized team", playerGuid.GetString().c_str());
            return false;
        }
    }

    // remove all player signs from another petitions
    // this will be prevent attempt joining player to many arenateams and corrupt arena team data integrity
    // (移除该玩家在其他同类型请愿书上的所有签名，防止同时加入多个同规模战队破坏数据一致性)
    Player::RemovePetitionsAndSigns(playerGuid, GetType());

    ArenaTeamMember newmember;
    newmember.name              = plName;
    newmember.guid              = playerGuid;
    newmember.Class             = plClass;
    newmember.games_season      = 0;
    newmember.games_week        = 0;
    newmember.wins_season       = 0;
    newmember.wins_week         = 0;

    int32 conf_value = sWorld.getConfig(CONFIG_INT32_ARENA_STARTPERSONALRATING);
    if (conf_value < 0)                                     // -1 = select by season id (-1 表示按赛季编号决定初始个人等级)
    {
        if (sWorld.getConfig(CONFIG_UINT32_ARENA_SEASON_ID) >= 6)
        {
            if (m_stats.rating < 1000)
                newmember.personal_rating = m_stats.rating;
            else
                newmember.personal_rating = 1000;
        }
        else
        {
            newmember.personal_rating = 1500;
        }
    }
    else
        newmember.personal_rating = uint32(conf_value);

    m_members.push_back(newmember);

    CharacterDatabase.PExecute("INSERT INTO arena_team_member (arenateamid, guid, personal_rating) VALUES ('%u', '%u', '%u')", m_TeamId, newmember.guid.GetCounter(), newmember.personal_rating);

    if (pl)
    {
        pl->SetInArenaTeam(m_TeamId, GetSlot());
        pl->SetArenaTeamIdInvited(0);
        pl->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_PERSONAL_RATING, newmember.personal_rating);

        // hide promote/remove buttons (非队长成员隐藏晋升/踢人按钮)
        if (m_CaptainGuid != playerGuid)
            pl->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_MEMBER, 1);
    }
    return true;
}

// 从数据库结果集中加载战队基础属性、队徽和统计数据
bool ArenaTeam::LoadArenaTeamFromDB(QueryResult* arenaTeamDataResult)
{
    if (!arenaTeamDataResult)
        return false;

    Field* fields = arenaTeamDataResult->Fetch();

    m_TeamId             = fields[0].GetUInt32();
    m_Name               = fields[1].GetCppString();
    m_CaptainGuid        = ObjectGuid(HIGHGUID_PLAYER, fields[2].GetUInt32());
    m_Type               = ArenaType(fields[3].GetUInt32());

    if (!IsArenaTypeValid(m_Type))
        return false;

    m_BackgroundColor    = fields[4].GetUInt32();
    m_EmblemStyle        = fields[5].GetUInt32();
    m_EmblemColor        = fields[6].GetUInt32();
    m_BorderStyle        = fields[7].GetUInt32();
    m_BorderColor        = fields[8].GetUInt32();
    // load team stats (加载战队战绩统计)
    m_stats.rating       = fields[9].GetUInt32();
    m_stats.games_week   = fields[10].GetUInt32();
    m_stats.wins_week    = fields[11].GetUInt32();
    m_stats.games_season = fields[12].GetUInt32();
    m_stats.wins_season  = fields[13].GetUInt32();
    m_stats.rank         = fields[14].GetUInt32();

    return true;
}

// 从数据库结果集中加载战队成员列表，并校验队长与角色有效性
bool ArenaTeam::LoadMembersFromDB(QueryResult* arenaTeamMembersResult)
{
    if (!arenaTeamMembersResult)
        return false;

    bool captainPresentInTeam = false;

    do
    {
        Field* fields = arenaTeamMembersResult->Fetch();
        // prevent crash if db records are broken, when all members in result are already processed and current team hasn't got any members
        // (防止数据库记录损坏时崩溃：当结果集中成员已全部处理且当前战队无有效成员时跳出)
        if (!fields)
            break;
        uint32 arenaTeamId        = fields[0].GetUInt32();
        if (arenaTeamId < m_TeamId)
        {
            // there is in table arena_team_member record which doesn't have arenateamid in arena_team table, report error
            // (arena_team_member 中存在没有对应 arena_team 主记录的孤立数据，报错并清理)
            sLog.outErrorDb("ArenaTeam %u does not exist but it has record in arena_team_member table, deleting it!", arenaTeamId);
            CharacterDatabase.PExecute("DELETE FROM arena_team_member WHERE arenateamid = '%u'", arenaTeamId);
            continue;
        }

        if (arenaTeamId > m_TeamId)
            // we loaded all members for this arena_team already, break cycle (当前战队的成员已加载完毕，退出循环)
            break;

        ArenaTeamMember newmember;
        newmember.guid            = ObjectGuid(HIGHGUID_PLAYER, fields[1].GetUInt32());
        newmember.games_week      = fields[2].GetUInt32();
        newmember.wins_week       = fields[3].GetUInt32();
        newmember.games_season    = fields[4].GetUInt32();
        newmember.wins_season     = fields[5].GetUInt32();
        newmember.personal_rating = fields[6].GetUInt32();
        newmember.name            = fields[7].GetCppString();
        newmember.Class           = fields[8].GetUInt8();

        // check if member exists in characters table (检查成员在 characters 表中是否存在)
        if (newmember.name.empty())
        {
            sLog.outErrorDb("ArenaTeam %u has member with empty name - probably player %s doesn't exist, deleting him from memberlist!", arenaTeamId, newmember.guid.GetString().c_str());
            DelMember(newmember.guid);
            continue;
        }

        // arena team can't be > 2 * arenatype (2 for 2x2, 3 for 3x3, 5 for 5x5) (战队成员数不能超过 2 * 战队规模)
        if (GetMembersSize() >= GetMaxMembersSize())
            return false;

        if (newmember.guid == GetCaptainGuid())
            captainPresentInTeam = true;

        m_members.push_back(newmember);
    }
    while (arenaTeamMembersResult->NextRow());

    if (Empty() || !captainPresentInTeam)
    {
        // arena team is empty or captain is not in team, delete from db (战队为空或队长不在战队中，从数据库移除并解散)
        sLog.outErrorDb("ArenaTeam %u does not have any members or its captain is not in team, disbanding it...", m_TeamId);
        return false;
    }

    return true;
}

// 移交队长给指定成员，并更新数据库及在线玩家的界面权限字段
void ArenaTeam::SetCaptain(ObjectGuid guid)
{
    // disable remove/promote buttons (禁用原队长的踢人/晋升按钮)
    Player* oldcaptain = sObjectMgr.GetPlayer(GetCaptainGuid());
    if (oldcaptain)
        oldcaptain->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_MEMBER, 1);

    // set new captain (设置新队长 GUID)
    m_CaptainGuid = guid;

    // update database (更新数据库中的队长记录)
    CharacterDatabase.PExecute("UPDATE arena_team SET captainguid = '%u' WHERE arenateamid = '%u'", guid.GetCounter(), m_TeamId);

    // enable remove/promote buttons (启用新队长的踢人/晋升按钮)
    if (Player* newcaptain = sObjectMgr.GetPlayer(guid))
        newcaptain->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_MEMBER, 0);
}

// 从战队中移除成员，清理在线玩家字段并删除数据库记录
void ArenaTeam::DelMember(ObjectGuid guid)
{
    for (MemberList::iterator itr = m_members.begin(); itr != m_members.end(); ++itr)
    {
        if (itr->guid == guid)
        {
            m_members.erase(itr);
            break;
        }
    }

    if (Player* player = sObjectMgr.GetPlayer(guid))
    {
        player->GetSession()->SendArenaTeamCommandResult(ERR_ARENA_TEAM_QUIT_S, GetName(), "", 0);
        // delete all info regarding this team (清空该玩家身上对应槽位的全部战队信息字段)
        for (int i = 0; i < ARENA_TEAM_END; ++i)
            player->SetArenaTeamInfoField(GetSlot(), ArenaTeamInfoType(i), 0);
    }

    CharacterDatabase.PExecute("DELETE FROM arena_team_member WHERE arenateamid = '%u' AND guid = '%u'", GetId(), guid.GetCounter());
}

// 解散战队：广播事件、逐个移除成员并删除数据库中的战队相关表数据
void ArenaTeam::Disband(WorldSession* session)
{
    // event (广播战队解散事件)
    if (session)
    {
        // probably only 1 string required... (可能仅需要 1 个字符串参数……)
        BroadcastEvent(ERR_ARENA_TEAM_DISBANDED_S, session->GetPlayerName(), GetName().c_str());
    }

    while (!m_members.empty())
    {
        // Removing from members is done in DelMember. (从成员列表移除的操作在 DelMember 中完成)
        DelMember(m_members.front().guid);
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM arena_team WHERE arenateamid = '%u'", m_TeamId);
    CharacterDatabase.PExecute("DELETE FROM arena_team_member WHERE arenateamid = '%u'", m_TeamId); //< this should be already done by calling DelMember(memberGuids[j]); for each member (正常已由 DelMember 逐个删除)
    CharacterDatabase.PExecute("DELETE FROM arena_team_stats WHERE arenateamid = '%u'", m_TeamId);
    CharacterDatabase.CommitTransaction();
    sObjectMgr.RemoveArenaTeam(m_TeamId);
}

// 向指定会话发送战队成员名册数据包 (SMSG_ARENA_TEAM_ROSTER)
void ArenaTeam::Roster(WorldSession* session)
{
    WorldPacket data(SMSG_ARENA_TEAM_ROSTER, 100);
    data << uint32(GetId());                                // team id (战队 ID)
    data << uint32(GetMembersSize());                       // members count (成员数量)
    data << uint32(GetType());                              // arena team type? (战队类型：2v2/3v3/5v5)

    for (MemberList::const_iterator itr = m_members.begin(); itr != m_members.end(); ++itr)
    {
        Player* pl = sObjectMgr.GetPlayer(itr->guid);

        data << itr->guid;                                  // guid (成员 GUID)
        data << uint8((pl ? 1 : 0));                        // online flag (在线标记)
        data << itr->name;                                  // member name (成员名称)
        data << uint32((itr->guid == GetCaptainGuid() ? 0 : 1));// captain flag 0 captain 1 member (队长标记：0=队长，1=普通成员)
        data << uint8((pl ? pl->GetLevel() : 0));           // unknown, level? (玩家等级)
        data << uint8(itr->Class);                          // class (职业)
        data << uint32(itr->games_week);                    // played this week (本周场次)
        data << uint32(itr->wins_week);                     // wins this week (本周胜场)
        data << uint32(itr->games_season);                  // played this season (本赛季场次)
        data << uint32(itr->wins_season);                   // wins this season (本赛季胜场)
        data << uint32(itr->personal_rating);               // personal rating (个人等级)
    }

    session->SendPacket(data);
    DEBUG_LOG("WORLD: Sent SMSG_ARENA_TEAM_ROSTER");
}

// 响应战队信息查询，发送队名、类型和队徽数据 (SMSG_ARENA_TEAM_QUERY_RESPONSE)
void ArenaTeam::Query(WorldSession* session) const
{
    WorldPacket data(SMSG_ARENA_TEAM_QUERY_RESPONSE, 4 * 7 + GetName().size() + 1);
    data << uint32(GetId());                                // team id (战队 ID)
    data << GetName();                                      // team name (战队名称)
    data << uint32(GetType());                              // arena team type (2=2x2, 3=3x3 or 5=5x5) (战队类型 2/3/5)
    data << uint32(m_BackgroundColor);                      // background color (背景颜色)
    data << uint32(m_EmblemStyle);                          // emblem style (队徽样式)
    data << uint32(m_EmblemColor);                          // emblem color (队徽颜色)
    data << uint32(m_BorderStyle);                          // border style (边框样式)
    data << uint32(m_BorderColor);                          // border color (边框颜色)
    session->SendPacket(data);
    DEBUG_LOG("WORLD: Sent SMSG_ARENA_TEAM_QUERY_RESPONSE");
}

// 发送战队战绩统计数据 (SMSG_ARENA_TEAM_STATS)
void ArenaTeam::Stats(WorldSession* session) const
{
    WorldPacket data(SMSG_ARENA_TEAM_STATS, 4 * 7);
    data << uint32(GetId());                                // team id (战队 ID)
    data << uint32(m_stats.rating);                         // rating (战队等级)
    data << uint32(m_stats.games_week);                     // games this week (本周场次)
    data << uint32(m_stats.wins_week);                      // wins this week (本周胜场)
    data << uint32(m_stats.games_season);                   // played this season (本赛季场次)
    data << uint32(m_stats.wins_season);                    // wins this season (本赛季胜场)
    data << uint32(m_stats.rank);                           // rank (当前排名)
    session->SendPacket(data);
}

// 在排位赛结束后向全体在线成员广播最新战队战绩
void ArenaTeam::NotifyStatsChanged()
{
    // this is called after a rated match ended
    // updates arena team stats for every member of the team (not only the ones who participated!)
    // (排位赛结束后调用，向战队所有成员更新战绩，不仅限于本场出战成员)
    for (MemberList::const_iterator itr = m_members.begin(); itr != m_members.end(); ++itr)
    {
        Player* plr = sObjectMgr.GetPlayer(itr->guid);
        if (plr)
            Stats(plr->GetSession());
    }
}

// 响应观察玩家战队战绩的请求 (MSG_INSPECT_ARENA_TEAMS)
void ArenaTeam::InspectStats(WorldSession* session, ObjectGuid guid)
{
    ArenaTeamMember* member = GetMember(guid);
    if (!member)
        return;

    WorldPacket data(MSG_INSPECT_ARENA_TEAMS, 8 + 1 + 4 * 6);
    data << guid;                                           // player guid (被观察玩家 GUID)
    data << uint8(GetSlot());                               // slot (0...2) (战队槽位 0..2)
    data << uint32(GetId());                                // arena team id (战队 ID)
    data << uint32(m_stats.rating);                         // rating (战队等级)
    data << uint32(m_stats.games_season);                   // season played (战队赛季总场次)
    data << uint32(m_stats.wins_season);                    // season wins (战队赛季胜场)
    data << uint32(member->games_season);                   // played (count of all games, that the inspected member participated...) (该成员赛季参与总场次)
    data << uint32(member->personal_rating);                // personal rating (成员个人等级)
    session->SendPacket(data);
}

// 设置战队队徽外观并持久化到数据库
void ArenaTeam::SetEmblem(uint32 backgroundColor, uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor)
{
    m_BackgroundColor = backgroundColor;
    m_EmblemStyle = emblemStyle;
    m_EmblemColor = emblemColor;
    m_BorderStyle = borderStyle;
    m_BorderColor = borderColor;

    CharacterDatabase.PExecute("UPDATE arena_team SET BackgroundColor='%u', EmblemStyle='%u', EmblemColor='%u', BorderStyle='%u', BorderColor='%u' WHERE arenateamid='%u'", m_BackgroundColor, m_EmblemStyle, m_EmblemColor, m_BorderStyle, m_BorderColor, m_TeamId);
}

// 按统计项类型更新战队战绩并同步数据库
void ArenaTeam::SetStats(uint32 stat_type, uint32 value)
{
    switch (stat_type)
    {
        case STAT_TYPE_RATING:
            m_stats.rating = value;
            CharacterDatabase.PExecute("UPDATE arena_team_stats SET rating = '%u' WHERE arenateamid = '%u'", value, GetId());
            break;
        case STAT_TYPE_GAMES_WEEK:
            m_stats.games_week = value;
            CharacterDatabase.PExecute("UPDATE arena_team_stats SET games_week = '%u' WHERE arenateamid = '%u'", value, GetId());
            break;
        case STAT_TYPE_WINS_WEEK:
            m_stats.wins_week = value;
            CharacterDatabase.PExecute("UPDATE arena_team_stats SET wins_week = '%u' WHERE arenateamid = '%u'", value, GetId());
            break;
        case STAT_TYPE_GAMES_SEASON:
            m_stats.games_season = value;
            CharacterDatabase.PExecute("UPDATE arena_team_stats SET games_season = '%u' WHERE arenateamid = '%u'", value, GetId());
            break;
        case STAT_TYPE_WINS_SEASON:
            m_stats.wins_season = value;
            CharacterDatabase.PExecute("UPDATE arena_team_stats SET wins_season = '%u' WHERE arenateamid = '%u'", value, GetId());
            break;
        case STAT_TYPE_RANK:
            m_stats.rank = value;
            CharacterDatabase.PExecute("UPDATE arena_team_stats SET `rank` = '%u' WHERE arenateamid = '%u'", value, GetId());
            break;
        default:
            DEBUG_LOG("unknown stat type in ArenaTeam::SetStats() %u", stat_type);
            break;
    }
}

// 向战队所有在线成员广播数据包
void ArenaTeam::BroadcastPacket(WorldPacket const& packet) const
{
    for (MemberList::const_iterator itr = m_members.cbegin(); itr != m_members.cend(); ++itr)
    {
        Player* player = sObjectMgr.GetPlayer(itr->guid);
        if (player)
            player->GetSession()->SendPacket(packet);
    }
}

// 向战队成员广播事件消息 (SMSG_ARENA_TEAM_EVENT)
void ArenaTeam::BroadcastEvent(ArenaTeamEvents event, ObjectGuid guid, char const* str1 /*=nullptr*/, char const* str2 /*=nullptr*/, char const* str3 /*=nullptr*/)
{
    uint8 strCount = !str1 ? 0 : (!str2 ? 1 : (!str3 ? 2 : 3));

    WorldPacket data(SMSG_ARENA_TEAM_EVENT, 1 + 1 + 1 * strCount + (!guid ? 0 : 8));
    data << uint8(event);
    data << uint8(strCount);

    if (str3)
    {
        data << str1;
        data << str2;
        data << str3;
    }
    else if (str2)
    {
        data << str1;
        data << str2;
    }
    else if (str1)
        data << str1;

    if (guid)
        data << ObjectGuid(guid);

    BroadcastPacket(data);

    DEBUG_LOG("WORLD: Sent SMSG_ARENA_TEAM_EVENT");
}

// 根据竞技场类型获取对应的角色槽位索引 (0=2v2, 1=3v3, 2=5v5)
uint8 ArenaTeam::GetSlotByType(ArenaType type)
{
    switch (type)
    {
        case ARENA_TYPE_2v2: return 0;
        case ARENA_TYPE_3v3: return 1;
        case ARENA_TYPE_5v5: return 2;
        default:
            break;
    }
    sLog.outError("FATAL: Unknown arena team type %u for some arena team", type);
    return 0xFF;
}

bool ArenaTeam::HaveMember(ObjectGuid guid) const
{
    for (const auto& m_member : m_members)
        if (m_member.guid == guid)
            return true;

    return false;
}

// 计算成员基于战队/个人等级与战队规模可获得的周竞技场点数
uint32 ArenaTeam::GetPoints(uint32 MemberRating)
{
    // returns how many points would be awarded with this team type with this rating
    // (计算该类型战队在当前等级下可获得的竞技场点数)
    float points;

    uint32 rating = MemberRating + 150 < m_stats.rating ? MemberRating : m_stats.rating;

    if (rating <= 1500)
    {
        // As of Season 6 and later, all teams below 1500 rating will earn points as if they were a 1500 rated team
        // (第 6 赛季及之后，1500 分以下的战队按 1500 分计算奖励点数)
        if (sWorld.getConfig(CONFIG_UINT32_ARENA_SEASON_ID) >= 6)
            rating = 1500;

        points = (float)rating * 0.22f + 14.0f;
    }
    else
        points = 1511.26f / (1.0f + 1639.28f * exp(-0.00412f * (float)rating));

    // type penalties for <5v5 teams (少于 5v5 的战队类型适用点数折算系数)
    if (m_Type == ARENA_TYPE_2v2)
        points *= 0.76f;
    else if (m_Type == ARENA_TYPE_3v3)
        points *= 0.88f;

    return (uint32) points;
}

// 使用 ELO 公式计算对阵指定等级敌队时的预期胜率
float ArenaTeam::GetChanceAgainst(uint32 own_rating, uint32 enemy_rating) const
{
    // returns the chance to win against a team with the given rating, used in the rating adjustment calculation
    // ELO system (返回对阵指定等级敌队的预期胜率，用于 ELO 等级增减计算)

    if (sWorld.getConfig(CONFIG_UINT32_ARENA_SEASON_ID) >= 6)
        if (enemy_rating < 1500)
            enemy_rating = 1500;
    return 1.0f / (1.0f + exp(log(10.0f) * (float)((float)enemy_rating - (float)own_rating) / 400.0f));
}

// 结算单场比赛后的战队等级、总场次以及同规模战队实时排名
void ArenaTeam::FinishGame(int32 mod)
{
    if (int32(m_stats.rating) + mod < 0)
        m_stats.rating = 0;
    else
        m_stats.rating += mod;

    m_stats.games_week += 1;
    m_stats.games_season += 1;
    // update team's rank (更新战队在同规模战队中的排名)
    m_stats.rank = 1;
    ObjectMgr::ArenaTeamMap::const_iterator i = sObjectMgr.GetArenaTeamMapBegin();
    for (; i != sObjectMgr.GetArenaTeamMapEnd(); ++i)
    {
        if (i->second->GetType() == this->m_Type && i->second->GetStats().rating > m_stats.rating)
            ++m_stats.rank;
    }
}

// 战队获胜时按 ELO (K=32) 结算战队等级并累加胜场，返回等级变动值
int32 ArenaTeam::WonAgainst(uint32 againstRating)
{
    // called when the team has won
    // 'chance' calculation - to beat the opponent (战队获胜时调用，计算击败对手的预期胜率)
    float chance = GetChanceAgainst(m_stats.rating, againstRating);
    // calculate the rating modification (ELO system with k=32) (按 K=32 的 ELO 系统计算等级增幅)
    int32 mod = (int32)floor(32.0f * (1.0f - chance));
    // modify the team stats accordingly (更新战队等级与场次统计)
    FinishGame(mod);
    m_stats.wins_week += 1;
    m_stats.wins_season += 1;

    // return the rating change, used to display it on the results screen (返回等级变化值供结算界面显示)
    return mod;
}

// 战队战败时按 ELO (K=32) 结算战队等级，返回等级变动值
int32 ArenaTeam::LostAgainst(uint32 againstRating)
{
    // called when the team has lost
    //'chance' calculation - to loose to the opponent (战队战败时调用，计算相对对手的胜率期望)
    float chance = GetChanceAgainst(m_stats.rating, againstRating);
    // calculate the rating modification (ELO system with k=32) (按 K=32 的 ELO 系统计算等级扣分)
    int32 mod = (int32)ceil(32.0f * (0.0f - chance));
    // modify the team stats accordingly (更新战队等级与场次统计)
    FinishGame(mod);

    // return the rating change, used to display it on the results screen (返回等级变化值供结算界面显示)
    return mod;
}

// 战败后更新参战在线成员的个人等级与场次字段
void ArenaTeam::MemberLost(Player* plr, uint32 againstRating)
{
    // called for each participant of a match after losing (战败后为每位参战成员调用)
    for (auto& m_member : m_members)
    {
        if (m_member.guid == plr->GetObjectGuid())
        {
            // update personal rating (更新个人等级)
            float chance = GetChanceAgainst(m_member.personal_rating, againstRating);
            int32 mod = (int32)ceil(32.0f * (0.0f - chance));
            m_member.ModifyPersonalRating(plr, mod, GetSlot());

            // update personal played stats (更新个人本周/本赛季参与场次)
            m_member.games_week += 1;
            m_member.games_season += 1;
            // update the unit fields (同步玩家单位属性字段)
            plr->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_GAMES_WEEK, m_member.games_week);
            plr->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_GAMES_SEASON, m_member.games_season);
            return;
        }
    }
}

// 排位赛结束时对已离线战败成员结算个人等级与场次
void ArenaTeam::OfflineMemberLost(ObjectGuid guid, uint32 againstRating)
{
    // called for offline player after ending rated arena match! (排位赛结束时为离线参战玩家调用)
    for (auto& m_member : m_members)
    {
        if (m_member.guid == guid)
        {
            // update personal rating (更新个人等级)
            float chance = GetChanceAgainst(m_member.personal_rating, againstRating);
            int32 mod = (int32)ceil(32.0f * (0.0f - chance));
            if (int32(m_member.personal_rating) + mod < 0)
                m_member.personal_rating = 0;
            else
                m_member.personal_rating += mod;
            // update personal played stats (更新个人参与场次)
            m_member.games_week += 1;
            m_member.games_season += 1;
            return;
        }
    }
}

// 获胜后更新参战成员的个人等级、场次与胜场字段
void ArenaTeam::MemberWon(Player* plr, uint32 againstRating)
{
    // called for each participant after winning a match (获胜后为每位参战成员调用)
    for (auto& m_member : m_members)
    {
        if (m_member.guid == plr->GetObjectGuid())
        {
            // update personal rating (更新个人等级)
            float chance = GetChanceAgainst(m_member.personal_rating, againstRating);
            int32 mod = (int32)floor(32.0f * (1.0f - chance));
            m_member.ModifyPersonalRating(plr, mod, GetSlot());
            // update personal stats (更新个人场次与胜场统计)
            m_member.games_week += 1;
            m_member.games_season += 1;
            m_member.wins_season += 1;
            m_member.wins_week += 1;
            // update unit fields (同步玩家单位属性字段)
            plr->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_GAMES_WEEK, m_member.games_week);
            plr->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_GAMES_SEASON, m_member.games_season);
            return;
        }
    }
}

// 周结算辅助函数：统计满足本周至少 10 场且个人出勤率 >= 30% 的成员可得的最高竞技场点数
void ArenaTeam::UpdateArenaPointsHelper(std::map<uint32, uint32>& PlayerPoints)
{
    // called after a match has ended and the stats are already modified
    // helper function for arena point distribution (this way, when distributing, no actual calculation is required, just a few comparisons)
    // 10 played games per week is a minimum (战队本周至少需完成 10 场比赛才有资格分配点数)
    if (m_stats.games_week < 10)
        return;
    // to get points, a player has to participate in at least 30% of the matches (玩家本人须至少参与战队本周 30% 的比赛)
    uint32 min_plays = (uint32) ceil(m_stats.games_week * 0.3);
    for (MemberList::const_iterator itr = m_members.begin(); itr !=  m_members.end(); ++itr)
    {
        // the player participated in enough games, update his points (若玩家出勤达标，计算并取其所有战队中的最高可获点数)
        uint32 points_to_add = 0;
        if (itr->games_week >= min_plays)
            points_to_add = GetPoints(itr->personal_rating);
        // OBSOLETE : CharacterDatabase.PExecute("UPDATE arena_team_member SET points_to_add = '%u' WHERE arenateamid = '%u' AND guid = '%u'", points_to_add, m_TeamId, itr->guid);

        std::map<uint32, uint32>::iterator plr_itr = PlayerPoints.find(itr->guid.GetCounter());
        if (plr_itr != PlayerPoints.end())
        {
            // check if there is already more points (若玩家在另一支战队已有更高点数，则保留较高值)
            if (plr_itr->second < points_to_add)
                PlayerPoints[itr->guid.GetCounter()] = points_to_add;
        }
        else
            PlayerPoints[itr->guid.GetCounter()] = points_to_add;
    }
}

// 将战队战绩及所有成员个人统计持久化到数据库
void ArenaTeam::SaveToDB()
{
    // save team and member stats to db
    // called after a match has ended, or when calculating arena_points (比赛结束或计算竞技场点数时保存到数据库)
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("UPDATE arena_team_stats SET rating = '%u',games_week = '%u',games_season = '%u',`rank` = '%u',wins_week = '%u',wins_season = '%u' WHERE arenateamid = '%u'", m_stats.rating, m_stats.games_week, m_stats.games_season, m_stats.rank, m_stats.wins_week, m_stats.wins_season, GetId());
    for (MemberList::const_iterator itr = m_members.begin(); itr !=  m_members.end(); ++itr)
    {
        CharacterDatabase.PExecute("UPDATE arena_team_member SET played_week = '%u', wons_week = '%u', played_season = '%u', wons_season = '%u', personal_rating = '%u' WHERE arenateamid = '%u' AND guid = '%u'", itr->games_week, itr->wins_week, itr->games_season, itr->wins_season, itr->personal_rating, m_TeamId, itr->guid.GetCounter());
    }
    CharacterDatabase.CommitTransaction();
}

// 结束本周统计，重置战队与成员的本周场次和胜场
void ArenaTeam::FinishWeek()
{
    m_stats.games_week = 0;                                 // played this week (清零本周场次)
    m_stats.wins_week = 0;                                  // wins this week (清零本周胜场)
    for (auto& m_member : m_members)
    {
        m_member.games_week = 0;
        m_member.wins_week = 0;
    }
}

// 判断战队当前是否有任何在线成员正在竞技场地图中战斗
bool ArenaTeam::IsFighting() const
{
    for (const auto& m_member : m_members)
    {
        if (Player* p = sObjectMgr.GetPlayer(m_member.guid))
        {
            if (p->GetMap()->IsBattleArena())
                return true;
        }
    }
    return false;
}

// 结束当前赛季，重置战队及成员全部战绩并将等级恢复为 1500
void ArenaTeam::FinishSeason()
{
    m_stats.rank = 0;
    m_stats.wins_season = 0;
    m_stats.games_season = 0;
    m_stats.rating = 1500;
    m_stats.games_week = 0;
    m_stats.wins_week = 0;
    for (MemberList::iterator itr = m_members.begin(); itr != m_members.end(); ++itr)
    {
        itr->games_week = 0;
        itr->wins_week = 0;
        itr->games_season = 0;
        itr->wins_season = 0;
        itr->personal_rating = 1500;
    }
}

// 将战队及所有成员个人等级统一设置为指定值
void ArenaTeam::SetRatingForAll(uint32 rating)
{
    m_stats.rating = rating;
    for (auto& memberData : m_members)
        memberData.personal_rating = rating;
}
