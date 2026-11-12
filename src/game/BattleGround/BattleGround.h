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

#ifndef __BATTLEGROUND_H
#define __BATTLEGROUND_H

#include "Common.h"
#include "Globals/SharedDefines.h"
#include "Maps/Map.h"
#include "Util/ByteBuffer.h"
#include "Entities/ObjectGuid.h"
#include "BattleGround/BattleGroundDefines.h"

// magic event-numbers (特殊战场事件编号)
#define BG_EVENT_NONE 255
// those generic events should get a high event id (通用大门开关事件使用高位事件 ID)
#define BG_EVENT_DOOR 254
// only arena event
// cause this buff apears 90sec after start in every bg i implement it here
// (竞技场暗影视界增益刷新事件：开战 90 秒后在所有竞技场生成)
#define ARENA_BUFF_EVENT 253
#define ARENA_TIMELIMIT_POINTS_LOSS -16


class Creature;
class GameObject;
class Group;
class Player;
class WorldPacket;
class BattleGroundMap;

struct WorldSafeLocsEntry;

// 战场对象所属的双层事件索引 (event1, event2)
struct BattleGroundEventIdx
{
    uint8 event1;
    uint8 event2;
};

// 战场音效 ID
enum BattleGroundSounds
{
    SOUND_HORDE_WINS                = 8454,
    SOUND_ALLIANCE_WINS             = 8455,
    SOUND_BG_START                  = 3439
};

// 战场日常/胜利任务奖励触发法术
enum BattleGroundQuests
{
    SPELL_WS_QUEST_REWARD           = 43483,
    SPELL_AB_QUEST_REWARD           = 43484,
    SPELL_AV_QUEST_REWARD           = 43475,
    SPELL_AV_QUEST_KILLED_BOSS      = 23658,
    SPELL_EY_QUEST_REWARD           = 43477,
    SPELL_AB_QUEST_REWARD_4_BASES   = 24061,
    SPELL_AB_QUEST_REWARD_5_BASES   = 24064
};

// 战场荣誉奖章法术与物品 ID
enum BattleGroundMarks
{
    SPELL_WS_MARK_LOSER             = 24950,
    SPELL_WS_MARK_WINNER            = 24951,
    SPELL_AB_MARK_LOSER             = 24952,
    SPELL_AB_MARK_WINNER            = 24953,
    SPELL_AV_MARK_LOSER             = 24954,
    SPELL_AV_MARK_WINNER            = 24955,
    ITEM_EY_MARK_OF_HONOR           = 29024
};

// 战场胜负奖励奖章数量
enum BattleGroundMarksCount
{
    ITEM_WINNER_COUNT               = 3,
    ITEM_LOSER_COUNT                = 1
};

// 战场与竞技场通用法术 ID
enum BattleGroundSpells
{
    SPELL_GRAVEYARD_TELEPORT        = 24237,
    SPELL_ARENA_PREPARATION         = 32727,                // use this one, 32728 not correct (竞技场准备阶段光环，32728 不正确)
    SPELL_ALLIANCE_GOLD_FLAG        = 32724,
    SPELL_ALLIANCE_GREEN_FLAG       = 32725,
    SPELL_HORDE_GOLD_FLAG           = 35774,
    SPELL_HORDE_GREEN_FLAG          = 35775,
    SPELL_PREPARATION               = 44521,                // Preparation (战场准备阶段免费施法光环)
    SPELL_RECENTLY_DROPPED_FLAG     = 42792,                // Recently Dropped Flag (刚丢弃战旗的冷却减益)
    SPELL_AURA_PLAYER_INACTIVE      = 43681,                // Inactive (挂机不活跃标记光环)
};

// 战场各类时间间隔常量
enum BattleGroundTimeIntervals
{
    CHECK_PLAYER_POSITION_INVERVAL  = 1000,                 // ms (检查玩家越界与起始区域位置间隔，毫秒)
    RESURRECTION_INTERVAL           = 30000,                // ms (灵魂医者群体复活周期，30 秒)
    INVITATION_REMIND_TIME          = 60000,                // ms (排进战场邀请二次提醒时间，60 秒)
    INVITE_ACCEPT_WAIT_TIME         = 80000,                // ms (邀请确认超时自动移出队列时间，80 秒)
    TIME_TO_AUTOREMOVE              = 120000,               // ms (战场结束后自动传出延迟，120 秒)
    MAX_OFFLINE_TIME                = 300,                  // secs (掉线保留资格最长时间，300 秒)
    RESPAWN_ONE_DAY                 = 86400,                // secs (单日不自动刷新时间)
    RESPAWN_IMMEDIATELY             = 0,                    // secs (立即刷新)
    BUFF_RESPAWN_TIME               = 180,                  // secs (战场增益符文重生时间，180 秒)
    ARENA_SPAWN_BUFF_OBJECTS        = 90000,                // ms - 90sec after start (竞技场暗影视界刷新时间，开局后 90 秒)
    ARENA_FORCED_DRAW               = 2700000,              // ms - 45min after start (竞技场超时强制平局时限，45 分钟)
};

// 战场开战倒计时阶段延迟常量
enum BattleGroundStartTimeIntervals
{
    BG_START_DELAY_2M               = 120000,               // ms (2 minutes) (2 分钟)
    BG_START_DELAY_1M               = 60000,                // ms (1 minute) (1 分钟)
    BG_START_DELAY_30S              = 30000,                // ms (30 seconds) (30 秒)
    BG_START_DELAY_15S              = 15000,                // ms (15 seconds) Used only in arena (15 秒，仅用于竞技场)
    BG_START_DELAY_NONE             = 0,                    // ms (无延迟，立即开战)
};

// 战场可拾取增益符文游戏对象模板 ID
enum BattleGroundBuffObjects
{
    BG_OBJECTID_SPEEDBUFF_ENTRY     = 179871,
    BG_OBJECTID_REGENBUFF_ENTRY     = 179904,
    BG_OBJECTID_BERSERKERBUFF_ENTRY = 179905
};

// 战场灵魂向导 NPC 模板 ID
enum BattlegroundSpiritHealers
{
    BG_NPC_SPIRIT_GUIDE_ALLIANCE    = 13116,
    BG_NPC_SPIRIT_GUIDE_HORDE       = 13117,
};

const uint32 Buff_Entries[3] = { BG_OBJECTID_SPEEDBUFF_ENTRY, BG_OBJECTID_REGENBUFF_ENTRY, BG_OBJECTID_BERSERKERBUFF_ENTRY };

// 战场实例运行状态
enum BattleGroundStatus
{
    STATUS_NONE         = 0,                                // first status, should mean bg is not instance (初始/模板状态，非真实战场实例)
    STATUS_WAIT_QUEUE   = 1,                                // means bg is empty and waiting for queue (空闲等待队列分配)
    STATUS_WAIT_JOIN    = 2,                                // this means, that BG has already started and it is waiting for more players (已开启等待玩家进场或开战倒计时中)
    STATUS_IN_PROGRESS  = 3,                                // means bg is running (战斗进行中)
    STATUS_WAIT_LEAVE   = 4                                 // means some faction has won BG and it is ending (已决出胜负，等待传出玩家与销毁)
};

// 战场内玩家状态信息
struct BattleGroundPlayer
{
    time_t  offlineRemoveTime;                              // for tracking and removing offline players from queue after 5 minutes (用于记录掉线超时移除时间戳)
    Team    playerTeam;                                     // Player's team (玩家在战场内的所属阵营)
};

// 战场普通游戏对象（如增益符文）状态信息
struct BattleGroundObjectInfo
{
    BattleGroundObjectInfo() : object(nullptr), timer(0), spellid(0) {}

    GameObject*  object;
    int32       timer;
    uint32      spellid;
};

// handle the queue types and bg types separately to enable joining queue for different sized arenas at the same time
// (将队列类型与战场类型解耦，使玩家可同时排入不同规模的竞技场队列)
enum BattleGroundQueueTypeId
{
    BATTLEGROUND_QUEUE_NONE     = 0,
    BATTLEGROUND_QUEUE_AV       = 1,
    BATTLEGROUND_QUEUE_WS       = 2,
    BATTLEGROUND_QUEUE_AB       = 3,
    BATTLEGROUND_QUEUE_EY       = 4,
    BATTLEGROUND_QUEUE_2v2      = 5,
    BATTLEGROUND_QUEUE_3v3      = 6,
    BATTLEGROUND_QUEUE_5v5      = 7,
};
#define MAX_BATTLEGROUND_QUEUE_TYPES 8

enum BattleGroundBracketId                                  // bracketId for level ranges (战场等级分段索引)
{
    BG_BRACKET_ID_TEMPLATE       = -1,                      // used to mark bg as template (标记战场模板本身)
    BG_BRACKET_ID_FIRST          = 0,                       // brackets start from specific BG min level and each include 10 levels range (从最小准入等级起每 10 级一档)
    BG_BRACKET_ID_LAST           = 6,                       // so for start level 10 will be 10-19, 20-29, ...  all greater max bg level included in last breaket (最高档包含其余所有高等级)
};

#define MAX_BATTLEGROUND_BRACKETS  7

// 战场记分牌统计项类型
enum ScoreType
{
    SCORE_KILLING_BLOWS         = 1,
    SCORE_DEATHS                = 2,
    SCORE_HONORABLE_KILLS       = 3,
    SCORE_BONUS_HONOR           = 4,
    // EY, but in MSG_PVP_LOG_DATA opcode! (风暴之眼及竞技场伤害/治疗量字段)
    SCORE_DAMAGE_DONE           = 5,
    SCORE_HEALING_DONE          = 6,
    // WS (战歌峡谷：夺旗与护旗)
    SCORE_FLAG_CAPTURES         = 7,
    SCORE_FLAG_RETURNS          = 8,
    // AB (阿拉希盆地：攻占与守卫基地)
    SCORE_BASES_ASSAULTED       = 9,
    SCORE_BASES_DEFENDED        = 10,
    // AV (奥特兰克山谷：墓地、哨塔与次要目标)
    SCORE_GRAVEYARDS_ASSAULTED  = 11,
    SCORE_GRAVEYARDS_DEFENDED   = 12,
    SCORE_TOWERS_ASSAULTED      = 13,
    SCORE_TOWERS_DEFENDED       = 14,
    SCORE_SECONDARY_OBJECTIVES  = 15
};

// 战场大类（普通战场或竞技场）
enum BattleGroundType
{
    TYPE_BATTLEGROUND     = 3,
    TYPE_ARENA            = 4
};

// 开战倒计时已触发阶段的位掩码
enum BattleGroundStartingEvents
{
    BG_STARTING_EVENT_NONE  = 0x00,
    BG_STARTING_EVENT_1     = 0x01,
    BG_STARTING_EVENT_2     = 0x02,
    BG_STARTING_EVENT_3     = 0x04,
    BG_STARTING_EVENT_4     = 0x08
};

// 开战倒计时各阶段下标
enum BattleGroundStartingEventsIds
{
    BG_STARTING_EVENT_FIRST     = 0,
    BG_STARTING_EVENT_SECOND    = 1,
    BG_STARTING_EVENT_THIRD     = 2,
    BG_STARTING_EVENT_FOURTH    = 3
};
#define BG_STARTING_EVENT_COUNT 4

// 团队排战场错误代码
enum BattleGroundJoinError
{
    BG_JOIN_ERR_OK = 0,
    BG_JOIN_ERR_OFFLINE_MEMBER = 1,
    BG_JOIN_ERR_GROUP_TOO_MANY = 2,
    BG_JOIN_ERR_MIXED_FACTION = 3,
    BG_JOIN_ERR_MIXED_LEVELS = 4,
    BG_JOIN_ERR_MIXED_ARENATEAM = 5,
    BG_JOIN_ERR_GROUP_MEMBER_ALREADY_IN_QUEUE = 6,
    BG_JOIN_ERR_GROUP_DESERTER = 7,
    BG_JOIN_ERR_ALL_QUEUES_USED = 8,
    BG_JOIN_ERR_GROUP_NOT_ENOUGH = 9,
    BG_JOIN_ERR_GROUP_IN_ARENA = 10,
};

/*
 This class is used to keep the battleground score for each individual player
 (单名玩家的战场/竞技场战绩数据基类，各具体战场可继承扩展专属扩展列)
*/
class BattleGroundScore
{
    public:
        BattleGroundScore() : killingBlows(0), deaths(0), honorableKills(0),
            bonusHonor(0), damageDone(0), healingDone(0), Team(ALLIANCE)
        {}
        virtual ~BattleGroundScore() {}                     // virtual destructor is used when deleting score from scores map (虚析构函数用于从记分映射中安全释放子类对象)

        uint32 GetKillingBlows() const      { return killingBlows; }
        uint32 GetDeaths() const            { return deaths; }
        uint32 GetHonorableKills() const    { return honorableKills; }
        uint32 GetBonusHonor() const        { return bonusHonor; }
        uint32 GetDamageDone() const        { return damageDone; }
        uint32 GetHealingDone() const       { return healingDone; }

        virtual uint32 GetAttr1() const     { return 0; }
        virtual uint32 GetAttr2() const     { return 0; }
        virtual uint32 GetAttr3() const     { return 0; }
        virtual uint32 GetAttr4() const     { return 0; }
        virtual uint32 GetAttr5() const     { return 0; }

        uint32 killingBlows;
        uint32 deaths;
        uint32 honorableKills;
        uint32 bonusHonor;
        uint32 damageDone;
        uint32 healingDone;

        uint32 Team;
};

/*
This class is used to:
1. Add player to battleground
2. Remove player from battleground
3. some certain cases, same for all battlegrounds
4. It has properties same for all battlegrounds
(战场与竞技场抽象基类：负责实例生命周期、玩家进出、团队编制、事件/生物/大门刷新、世界状态广播与胜负结算)
*/
class BattleGround
{
        friend class BattleGroundMgr;

    public:
        /* Construction */
        BattleGround();
        virtual ~BattleGround();

        // main battleground functions (战场主更新与重置接口；子类覆写 Update 时须在开头调用父类实现)
        virtual void Update(uint32 diff);                   // must be implemented in BG subclass of BG specific update code, but must in begginning call parent version
        virtual void Reset();                               // resets all common properties for battlegrounds, must be implemented and called in BG subclass

        // handle the opening and closing door events (开战前后关闭与开启出生点大门)
        virtual void StartingEventCloseDoors() {}
        virtual void StartingEventOpenDoors() {}

        /* Battleground */
        // Get methods:
        char const* GetName() const         { return m_name; }
        BattleGroundTypeId GetTypeId() const { return m_typeId; }
        BattleGroundBracketId GetBracketId() const { return m_bracketId; }
        // the instanceId check is also used to determine a bg-template
        // that's why the m_map hack is here.. (无关联地图时 instanceId 为 0，用于区分战场模板与真实实例)
        uint32 GetInstanceId() const        { return m_bgMap ? GetBgMap()->GetInstanceId() : 0; }
        BattleGroundStatus GetStatus() const { return m_status; }
        uint32 GetClientInstanceId() const  { return m_clientInstanceId; }
        uint32 GetStartTime() const         { return m_startTime; }
        uint32 GetEndTime() const           { return m_endTime; }
        uint32 GetMaxPlayers() const        { return m_maxPlayers; }
        uint32 GetMinPlayers() const        { return m_minPlayers; }

        uint32 GetMinLevel() const          { return m_levelMin; }
        uint32 GetMaxLevel() const          { return m_levelMax; }

        uint32 GetMaxPlayersPerTeam() const { return m_maxPlayersPerTeam; }
        uint32 GetMinPlayersPerTeam() const { return m_minPlayersPerTeam; }

        int32 GetStartDelayTime() const     { return m_startDelayTime; }
        ArenaType GetArenaType() const      { return m_arenaType; }
        BattleGroundWinner GetWinner() const{ return m_winner; }
        uint32 GetBattlemasterEntry() const;
        uint32 GetBonusHonorFromKill(uint32 kills) const;

        // Set methods:
        void SetName(char const* name)      { m_name = name; }
        void SetTypeId(BattleGroundTypeId typeId) { m_typeId = typeId; }
        void SetBracketId(BattleGroundBracketId id) { m_bracketId = id; }
        void SetStatus(BattleGroundStatus status);
        void SetClientInstanceId(uint32 instanceId) { m_clientInstanceId = instanceId; }
        void SetStartTime(uint32 time)      { m_startTime = time; }
        void SetEndTime(uint32 time)        { m_endTime = time; }
        void SetMaxPlayers(uint32 maxPlayers) { m_maxPlayers = maxPlayers; }
        void SetMinPlayers(uint32 minPlayers) { m_minPlayers = minPlayers; }
        void SetLevelRange(uint32 min, uint32 max) { m_levelMin = min; m_levelMax = max; }
        void SetRated(bool state)           { m_isRated = state; }
        void SetArenaType(ArenaType type)   { m_arenaType = type; }
        void SetArenaorBGType(bool isArena) { m_isArena = isArena; }
        void SetWinner(BattleGroundWinner winner) { m_winner = winner; }

        void ModifyStartDelayTime(int diff) { m_startDelayTime -= diff; }
        void SetStartDelayTime(int time)    { m_startDelayTime = time; }

        void SetMaxPlayersPerTeam(uint32 maxPlayers) { m_maxPlayersPerTeam = maxPlayers; }
        void SetMinPlayersPerTeam(uint32 minPlayers) { m_minPlayersPerTeam = minPlayers; }

        bool AddToBgFreeSlotQueue();                           // this queue will be useful when more battlegrounds instances will be available (将实例登记到空闲槽位队列以便后续补人)
        void RemovedFromBgFreeSlotQueue(bool removeFromQueue); // this method could delete whole BG instance, if another free is available (从空闲槽位队列移除实例)

        // Functions to decrease or increase player count (管理双方已邀请人数与空余名额)
        void SetInvitedCount(Team team, uint32 count);
        uint32 GetInvitedCount(Team team) const
        {
            if (team == ALLIANCE)
                return m_invitedAlliance;
            return m_invitedHorde;
        }
        bool HasFreeSlots() const { return GetPlayersSize() < GetMaxPlayers(); }
        uint32 GetFreeSlotsForTeam(Team team) const;

        // Functions that return if battleground is arena or if it's rated (判断是否为竞技场及是否排位赛)
        bool IsArena() const        { return m_isArena; }
        bool IsBattleGround() const { return !m_isArena; }
        bool IsRated() const        { return m_isRated; }

        // Functions that handle battleground players (获取战场在场玩家映射与人数)
        typedef std::map<ObjectGuid, BattleGroundPlayer> BattleGroundPlayerMap;
        BattleGroundPlayerMap const& GetPlayers() const { return m_players; }
        uint32 GetPlayersSize() const { return m_players.size(); }

        // Functions that handle battleground scores (战场记分表迭代接口)
        typedef std::map<ObjectGuid, BattleGroundScore*> BattleGroundScoreMap;
        BattleGroundScoreMap::const_iterator GetPlayerScoresBegin() const { return m_playerScores.begin(); }
        BattleGroundScoreMap::const_iterator GetPlayerScoresEnd() const { return m_playerScores.end(); }
        uint32 GetPlayerScoresSize() const { return m_playerScores.size(); }

        // Function which starts the battleground (正式开启战场，切换状态为进行中并开启大门)
        void StartBattleGround();

        // Functions that handle gameobject storage (按条目获取已登记的单例游戏对象或生物)
        typedef std::map<uint32, ObjectGuid> EntryGuidMap;
        GameObject* GetSingleGameObjectFromStorage(uint32 entry) const;
        Creature* GetSingleCreatureFromStorage(uint32 entry, bool skipDebugLog = false) const;

        // Function that set and get battleground map id (设置/获取战场地图 ID)
        void SetMapId(uint32 mapId) { m_mapId = mapId; }
        uint32 GetMapId() const { return m_mapId; }

        // Functions that set and get battleground map (设置/获取战场地图实例指针)
        void SetBgMap(BattleGroundMap* map) { m_bgMap = map; }
        BattleGroundMap* GetBgMap() const
        {
            MANGOS_ASSERT(m_bgMap);
            return m_bgMap;
        }

        // Functions that get and set team start location (设置/获取双方阵营出生传送点坐标与最大合法准备距离)
        void SetTeamStartLoc(Team team, float x, float y, float z, float o);
        void GetTeamStartLoc(Team team, float& x, float& y, float& z, float& o) const
        {
            PvpTeamIndex idx = GetTeamIndexByTeamId(team);
            x = m_teamStartLocX[idx];
            y = m_teamStartLocY[idx];
            z = m_teamStartLocZ[idx];
            o = m_teamStartLocO[idx];
        }

        void SetStartMaxDist(float startMaxDist) { m_startMaxDist = startMaxDist; }
        float GetStartMaxDist() const { return m_startMaxDist; }

        /* Packet Transfer */
        // method that should fill worldpacket with actual world states (not yet implemented for all battlegrounds!) (填充战场初始世界状态包)
        virtual void FillInitialWorldStates(WorldPacket& /*data*/, uint32& /*count*/) {}
        void SendPacketToTeam(Team teamId, WorldPacket const& packet, Player* sender = nullptr, bool self = true) const;
        void SendPacketToAll(WorldPacket const& packet) const;

        template<class Do>
        void BroadcastWorker(Do& _do);

        // Functions that handle sending sounds (向阵营或全体发送音效)
        void PlaySoundToTeam(uint32 /*soundId*/, Team /*teamId*/);
        void PlaySoundToAll(uint32 /*soundId*/);

        // Function that handle casting spells on team (对指定阵营全部玩家施放法术)
        void CastSpellOnTeam(uint32 /*spellId*/, Team /*teamId*/);

        // Function that rewards honor to the team (向指定阵营全部玩家发放荣誉)
        void RewardHonorToTeam(uint32 /*honor*/, Team /*teamId*/);

        // Function that rewards reputation to the team (向指定阵营全部玩家发放声望)
        void RewardReputationToTeam(uint32 /*factionId*/, uint32 /*reputation*/, Team /*teamId*/);

        // Function that rewards PvP marks (发放战场奖章物品或通过邮件补发)
        void RewardMark(Player* /*player*/, uint32 /*count*/);

        // Function that rewards PvP marks by mail (背包满时通过邮件补发战场奖章)
        void SendRewardMarkByMail(Player* /*player*/, uint32 /*mark*/, uint32 /*count*/) const;

        // Function that rewards items (向玩家发放物品奖励)
        void RewardItem(Player* /*player*/, uint32 /*itemId*/, uint32 /*count*/);

        // Function that rewards quest completion (触发战场胜利/日常任务完成法术)
        void RewardQuestComplete(Player* /*player*/);

        // Function that rewards spell id (向玩家施放指定奖励法术)
        void RewardSpellCast(Player* /*player*/, uint32 /*spellId*/) const;

        // Function that updates world states for all players (向全体玩家广播世界状态更新)
        void UpdateWorldState(uint32 /*field*/, uint32 /*value*/);

        // Function that updates world state for a player (向单个玩家发送世界状态更新)
        void UpdateWorldStateForPlayer(uint32 /*field*/, uint32 /*value*/, Player* /*player*/) const;

        // Function that ends battleground (结束战场并结算胜负奖励与竞技场积分)
        virtual void EndBattleGround(Team /*winner*/);

        // Function that blocks movement at the end of the battleground (战场结束时冻结玩家移动)
        static void BlockMovement(Player* /*player*/);

        // Functions that handle battleground announcements; Used usually when players capture battleground objectives
        // (向全场玩家广播战场战报/系统公告)
        void SendMessageToAll(int32 /*entry*/, ChatMsg /*type*/, Player const* source = nullptr);
        void SendMessageToAll(int32 /*entry*/, ChatMsg /*type*/, uint32 /*language*/, ObjectGuid /*guid*/);
        void PSendMessageToAll(int32 /*entry*/, ChatMsg /*type*/, Player const* /*source*/, ...);
        void SendMessage2ToAll(int32 /*entry*/, ChatMsg /*type*/, Player const* /*source*/, int32 arg1 = 0, int32 arg2 = 0);

        // Functions that handle creature yells in battleground; Used specifically in Alterac Valley
        // (向全场广播 NPC 喊话，主要用于奥特兰克山谷)
        void SendYellToAll(int32 /*entry*/, uint32 /*language*/, Creature const* /*source*/);
        void SendYell2ToAll(int32 /*entry*/, uint32 /*language*/, Creature const* /*source*/, int32 /*arg1*/, int32 /*arg2*/);

        void SendBcdToAll(int32 bcdEntry, ChatMsg msgtype, Creature const* source);
        void SendBcdToTeam(int32 bcdEntry, ChatMsg msgtype, Creature const* source, Team team);

        // Handle raid groups (获取与设置阵营对应的战场团队指针)
        Group* GetBgRaid(Team team) const { return m_bgRaids[GetTeamIndexByTeamId(team)]; }
        void SetBgRaid(Team /*team*/, Group* /*bgRaid*/);

        // Handle player score update (更新玩家单项战场记分数据)
        virtual void UpdatePlayerScore(Player* /*player*/, uint32 /*type*/, uint32 /*value*/);

        uint32 GetPlayersCountByTeam(Team team) const { return m_playersCount[GetTeamIndexByTeamId(team)]; }
        uint32 GetAlivePlayersCountByTeam(Team /*team*/) const;             // used in arenas to correctly handle death in spirit of redemption / last stand etc. (killer = killed) cases (统计指定阵营当前存活玩家数，排除救赎之魂等假死状态)
        void UpdatePlayersCountByTeam(Team team, bool remove)
        {
            if (remove)
                --m_playersCount[GetTeamIndexByTeamId(team)];
            else
                ++m_playersCount[GetTeamIndexByTeamId(team)];
        }

        // Arena specific functions (竞技场战队 ID、分数变化与胜负判定)
        void SetArenaTeamIdForTeam(Team team, uint32 arenaTeamId) { m_arenaTeamIds[GetTeamIndexByTeamId(team)] = arenaTeamId; }
        uint32 GetArenaTeamIdForTeam(Team team) const             { return m_arenaTeamIds[GetTeamIndexByTeamId(team)]; }
        void SetArenaTeamRatingChangeForTeam(Team team, int32 ratingChange) { m_arenaTeamRatingChanges[GetTeamIndexByTeamId(team)] = ratingChange; }
        int32 GetArenaTeamRatingChangeForTeam(Team team) const    { return m_arenaTeamRatingChanges[GetTeamIndexByTeamId(team)]; }
        void CheckArenaWinConditions();

        /* Battleground Events */

        // handle player in area trigger (处理玩家触发区域触发器)
        virtual bool HandleAreaTrigger(Player* /*player*/, uint32 /*triggerId*/) { return false;  }

        // handle player killed (处理玩家击杀事件)
        virtual void HandleKillPlayer(Player* /*player*/, Player* /*killer*/);

        // handle creature killed (处理生物击杀事件，如奥山首领/中尉)
        virtual void HandleKillUnit(Creature* /*unit*/, Player* /*killer*/) {}

        // handle event sent from gameobjects (处理游戏对象触发的事件)
        virtual bool HandleEvent(uint32 /*eventId*/, Object* /*source*/, Object* /*target*/) { return false; }

        // Called when a creature is created (战场生物创建回调)
        virtual void HandleCreatureCreate(Creature* /*creature*/) {}

        // Called when a gameobject is created (战场游戏对象创建回调)
        virtual void HandleGameObjectCreate(GameObject* /*go*/) {}

        // Called when player drops flag (玩家丢弃/掉落战旗回调)
        virtual void HandlePlayerDroppedFlag(Player* /*player*/) {}

        // Called when player clicks on flag (玩家点击战旗回调)
        virtual void HandlePlayerClickedOnFlag(Player* /*player*/, GameObject* /*target_obj*/) {}

        // Called when player logs in / out (玩家在战场内重连登录或离线处理)
        void EventPlayerLoggedIn(Player* /*player*/);
        void EventPlayerLoggedOut(Player* /*player*/);

        // Called when player joins the battleground after the initial load (玩家进入战场并加载完毕后回调，子类须扩展初始记分)
        virtual void AddPlayer(Player* /*player*/);                     // must be implemented in BG subclass

        void AddOrSetPlayerToCorrectBgGroup(Player* /*player*/, ObjectGuid /*playerGuid*/, Team /*team*/);

        // Called when player leaves battleground (玩家离开战场时移除并传送回原位置)
        virtual void RemovePlayerAtLeave(ObjectGuid /*playerGuid*/, bool /*isOnTransport*/, bool /*doSendPacket*/);

        /* Battleground Events */
        // called when a creature gets added to map (NOTE: only triggered if
        // a player activates the cell of the creature)
        // (数据库生物/对象加载到战场地图网格时按事件激活状态决定是否刷新)
        void OnObjectDBLoad(Creature* /*creature*/);
        void OnObjectDBLoad(GameObject* /*obj*/);

        // (de-)spawns creatures and gameobjects from an event (按事件编号批量刷新或解召唤对应生物与游戏对象)
        void SpawnEvent(uint8 /*event1*/, uint8 /*event2*/, bool /*spawn*/);
        bool IsActiveEvent(uint8 event1, uint8 event2)
        {
            if (m_activeEvents.find(event1) == m_activeEvents.end())
                return false;

            return m_activeEvents[event1] == event2;
        }

        // Get creature guid from event (获取事件关联的唯一生物 DB GUID)
        uint32 GetSingleCreatureGuid(uint8 /*event1*/, uint8 /*event2*/);

        // Get gameobject guid from event (获取事件关联的唯一游戏对象 DB GUID)
        uint32 GetSingleGameObjectGuid(uint8 /*event1*/, uint8 /*event2*/);

        // Handle door events (开启事件关联的大门并判定是否为大门事件)
        void OpenDoorEvent(uint8 /*event1*/, uint8 event2 = 0);
        bool IsDoorEvent(uint8 /*event1*/, uint8 /*event2*/) const;

        // Handle battleground buffs (traps) that give players specific benefits (处理玩家拾取战场增益符文对象)
        void HandleTriggerBuff(ObjectGuid /*go_guid*/);

        // Handle the respawn or despawn of creatures or gameobjects (通过数据库 GUID 修改战场游戏对象或生物的刷新/消失状态)
        void ChangeBgObjectSpawnState(uint32 dbGuid, uint32 /*respawntime*/);
        void ChangeBgCreatureSpawnState(uint32 dbGuid, uint32 /*respawntime*/);

        // Handle door states (直接开启或关闭指定 DB GUID 的战场大门)
        void DoorOpen(uint32 dbGuid);
        void DoorClose(uint32 dbGuid);

        // Get match premature winner (人数不足提前结束时判定获胜阵营)
        virtual Team GetPrematureWinner();

        // handle players that fall under the map (处理玩家掉出地图下方的复位逻辑)
        virtual bool HandlePlayerUnderMap(Player* /*player*/) { return false; }

        // since arenas can be AvA or Hvh, we have to get the "temporary" team of a player (获取玩家在当前战场/竞技场中的临时归属阵营)
        Team GetPlayerTeam(ObjectGuid /*playerGuid*/);

        // returns the other team (返回对立阵营)
        static Team GetOtherTeam(Team team) { return (team == ALLIANCE || team == HORDE) ? ((team == ALLIANCE) ? HORDE : ALLIANCE) : TEAM_NONE; }

        // returns the other team index (返回对立阵营索引)
        static PvpTeamIndex GetOtherTeamIndex(PvpTeamIndex teamIdx) { return teamIdx == TEAM_INDEX_ALLIANCE ? TEAM_INDEX_HORDE : TEAM_INDEX_ALLIANCE; }

        // check if player is inside battleground (判断玩家是否在当前战场玩家列表中)
        bool IsPlayerInBattleGround(ObjectGuid /*playerGuid*/);

        // Handle script condition fulfillment (检查战场脚本自定义条件是否满足)
        virtual bool IsConditionFulfilled(Player const* /*source*/, uint32 /*conditionId*/, WorldObject const* /*conditionSource*/, uint32 /*conditionSourceType*/) { return false; }

        struct EventObjects
        {
            std::vector<uint32> gameobjects;
            std::vector<uint32> creatures;
        };

        // cause we create it dynamicly i use a map - to avoid resizing when
        // using vector - also it contains 2*events concatenated with PAIR32
        // this is needed to avoid overhead of a 2dimensional std::map
        // (以 PAIR32(event1, event2) 为键存储各事件关联的游戏对象与生物 DB GUID 列表)
        std::map<uint32, EventObjects> m_eventObjects;
        // this must be filled first in BattleGroundXY::Reset().. else
        // creatures will get added wrong
        // door-events are automaticly added - but _ALL_ other must be in this vector
        // (记录当前激活的事件阶段，必须在子类 Reset() 最开始初始化，否则网格加载时刷新状态会出错)
        std::map<uint8, uint8> m_activeEvents;

        uint32 GetPlayerSkinRefLootId() const { return m_playerSkinReflootId; }
        void SetPlayerSkinRefLootId(uint32 reflootId) { m_playerSkinReflootId = reflootId; }

        MaNGOS::unique_weak_ptr<BattleGround> GetWeakPtr() const { return m_weakRef; }
        void SetWeakPtr(MaNGOS::unique_weak_ptr<BattleGround> weakRef) { m_weakRef = std::move(weakRef); }

    protected:
        // this method is called, when BG cannot spawn its own spirit guide, or something is wrong, It correctly ends BattleGround
        // (异常状态下强制结束战场并返还/结算状态)
        void EndNow();
        void PlayerAddedToBgCheckIfBgIsRunning(Player* /*player*/);

        /* Scorekeeping */
        BattleGroundScoreMap m_playerScores;                // Player scores (玩家个人战绩映射表)

        // must be implemented in BG subclass - player can be nullptr (玩家离场时的子类清理钩子，player 可能为空)
        virtual void RemovePlayer(Player* /*player*/, ObjectGuid /*guid*/) {}

        /* Player lists, those need to be accessible by inherited classes */
        BattleGroundPlayerMap  m_players;

        /*
        these are important variables used for starting messages
        (开战倒计时各阶段剩余延迟与对应广播文本 ID)
        */
        uint8 m_events;
        BattleGroundStartTimeIntervals  m_startDelayTimes[BG_STARTING_EVENT_COUNT];
        // this must be filled in constructors! (必须在子类构造函数中初始化)
        uint32 m_startMessageIds[BG_STARTING_EVENT_COUNT];

        bool   m_buffChange;

        /* Storage lists */
        EntryGuidMap m_goEntryGuidStore;                   // Store unique GO-Guids by entry (按模板条目缓存唯一游戏对象 GUID)
        EntryGuidMap m_npcEntryGuidStore;                  // Store unique NPC-Guids by entry (按模板条目缓存唯一生物 GUID)

    private:
        /* Battleground */
        BattleGroundTypeId m_typeId;
        BattleGroundStatus m_status;
        BattleGroundBracketId m_bracketId;
        ArenaType  m_arenaType;                             // 2=2v2, 3=3v3, 5=5v5 (竞技场赛制规模)
        BattleGroundWinner m_winner;

        uint32 m_clientInstanceId;                          // the instance-id which is sent to the client and without any other internal use (仅用于客户端显示编号的战场实例 ID)
        uint32 m_startTime;
        uint32 m_validStartPositionTimer;
        uint32 m_prematureCountDownTimer;
        int32 m_endTime;                                    // it is set to 120000 when bg is ending and it decreases itself (结束阶段自动传出倒计时，毫秒)
        int32  m_startDelayTime;

        bool m_arenaBuffSpawned;                            // to cache if arenabuff event is started (cause bool is faster than checking IsActiveEvent) (缓存竞技场暗影视界增益是否已刷出)
        bool m_hasBgFreeSlotQueue;                          // used to make sure that BG is only once inserted into the BattleGroundMgr.BGFreeSlotQueue[bgTypeId] deque (确保当前战场仅入队一次空闲槽位队列)
        bool m_isRated;                                     // is this battle rated? (是否为竞技场排位赛)
        bool m_prematureCountDown;
        bool m_isArena;

        char const* m_name;

        /* Player lists */
        typedef std::deque<ObjectGuid> OfflineQueue;
        OfflineQueue m_offlineQueue;                        // Player GUID (战场内掉线玩家 GUID 队列)

        /* Invited counters are useful for player invitation to BG - do not allow, if BG is started to one faction to have 2 more players than another faction */
        /* Invited counters will be changed only when removing already invited player from queue, removing player from battleground and inviting player to BG */
        /* Invited players counters (双方已受邀人数计数，用于防止已开战战场出现人数差过大) */
        uint32 m_invitedAlliance;
        uint32 m_invitedHorde;

        /* Raid Group */
        Group* m_bgRaids[PVP_TEAM_COUNT];                   // 0 - alliance, 1 - horde (战场内自动编成的团队，0=联盟，1=部落)

        /* Players count by team */
        uint32 m_playersCount[PVP_TEAM_COUNT];

        /* Arena team ids by team */
        uint32 m_arenaTeamIds[PVP_TEAM_COUNT];

        int32 m_arenaTeamRatingChanges[PVP_TEAM_COUNT];

        /* Limits */
        uint32 m_levelMin;
        uint32 m_levelMax;
        uint32 m_maxPlayersPerTeam;
        uint32 m_maxPlayers;
        uint32 m_minPlayersPerTeam;
        uint32 m_minPlayers;

        /* Start location */
        uint32 m_mapId;
        BattleGroundMap* m_bgMap;
        float m_teamStartLocX[PVP_TEAM_COUNT];
        float m_teamStartLocY[PVP_TEAM_COUNT];
        float m_teamStartLocZ[PVP_TEAM_COUNT];
        float m_teamStartLocO[PVP_TEAM_COUNT];
        float m_startMaxDist;

        uint32 m_playerSkinReflootId;

        MaNGOS::unique_weak_ptr<BattleGround> m_weakRef;
};

// helper functions for world state list fill (向字节缓冲区写入初始世界状态条目的辅助内联函数)
inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, uint32 value)
{
    data << uint32(state);
    data << uint32(value);
    ++count;
}

inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, int32 value)
{
    data << uint32(state);
    data << int32(value);
    ++count;
}

inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, bool value)
{
    data << uint32(state);
    data << uint32(value ? 1 : 0);
    ++count;
}

struct WorldStatePair
{
    uint32 state;
    uint32 value;
};

inline void FillInitialWorldState(ByteBuffer& data, uint32& count, WorldStatePair const* array)
{
    for (WorldStatePair const* itr = array; itr->state; ++itr)
    {
        data << uint32(itr->state);
        data << uint32(itr->value);
        ++count;
    }
}

#endif
