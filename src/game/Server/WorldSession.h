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

/// \addtogroup u2w
/// @{
/// \file

#ifndef __WORLDSESSION_H
#define __WORLDSESSION_H

#include "Common.h"
#include "Globals/Locales.h"
#include "Globals/SharedDefines.h"
#include "Entities/ObjectGuid.h"
#include "AuctionHouse/AuctionHouseMgr.h"
#include "Entities/Item.h"
#include "WorldSocket.h"
#include "Multithreading/Messager.h"
#include "LFG/LFGDefines.h"
#include "BattleGround/BattleGroundDefines.h"

#include <atomic>
#include <map>
#include <deque>
#include <mutex>
#include <memory>

#include <boost/circular_buffer.hpp>

struct ItemPrototype;
struct AuctionEntry;
struct AuctionHouseEntry;
struct DeclinedName;
struct TradeStatusInfo;

class ObjectGuid;
class Creature;
class Item;
class Object;
class Player;
class Unit;
class WorldPacket;
class QueryResult;
class LoginQueryHolder;
class CharacterHandler;
class GMTicket;
class MovementInfo;
class WorldSession;
class SessionAnticheatInterface;

struct OpcodeHandler;

/// 客户端账号配置缓存类型（g=账号全局共享，p=单角色独立缓存）
enum AccountDataType
{
    GLOBAL_CONFIG_CACHE             = 0,                    // 0x01 g
    PER_CHARACTER_CONFIG_CACHE      = 1,                    // 0x02 p
    GLOBAL_BINDINGS_CACHE           = 2,                    // 0x04 g
    PER_CHARACTER_BINDINGS_CACHE    = 3,                    // 0x08 p
    GLOBAL_MACROS_CACHE             = 4,                    // 0x10 g
    PER_CHARACTER_MACROS_CACHE      = 5,                    // 0x20 p
    PER_CHARACTER_LAYOUT_CACHE      = 6,                    // 0x40 p
    PER_CHARACTER_CHAT_CACHE        = 7,                    // 0x80 p
    NUM_ACCOUNT_DATA_TYPES          = 8
};

#define GLOBAL_CACHE_MASK           0x15
#define PER_CHARACTER_CACHE_MASK    0xEA

/// 账号/角色界面配置数据缓存项
struct AccountData
{
    AccountData() : Time(0), Data("") {}

    time_t Time;
    std::string Data;
};

/// 客户端操作系统类型
enum ClientOSType
{
    CLIENT_OS_UNKNOWN,
    CLIENT_OS_WIN,
    CLIENT_OS_MAC
};

/// 客户端 CPU 架构类型
enum ClientPlatformType
{
    CLIENT_PLATFORM_UNKNOWN,
    CLIENT_PLATFORM_X86,
    CLIENT_PLATFORM_PPC
};

/// 组队操作类型
enum PartyOperation
{
    PARTY_OP_INVITE = 0,
    PARTY_OP_LEAVE = 2,
};

/// 组队操作结果码
enum PartyResult
{
    ERR_PARTY_RESULT_OK                 = 0,
    ERR_BAD_PLAYER_NAME_S               = 1,
    ERR_TARGET_NOT_IN_GROUP_S           = 2,
    ERR_TARGET_NOT_IN_INSTANCE_S        = 3,
    ERR_GROUP_FULL                      = 4,
    ERR_ALREADY_IN_GROUP_S              = 5,
    ERR_NOT_IN_GROUP                    = 6,
    ERR_NOT_LEADER                      = 7,
    ERR_PLAYER_WRONG_FACTION            = 8,
    ERR_IGNORING_YOU_S                  = 9,
    ERR_LFG_PENDING                     = 12,
    ERR_INVITE_RESTRICTED               = 13,
};

/// 聊天受限提示类型
enum ChatRestrictionType
{
    ERR_CHAT_RESTRICTED = 0,
    ERR_CHAT_THROTTLED  = 1,
    ERR_USER_SQUELCHED  = 2,
};

/// 新手教学标志数据脏状态
enum TutorialDataState
{
    TUTORIALDATA_UNCHANGED = 0,
    TUTORIALDATA_CHANGED   = 1,
    TUTORIALDATA_NEW       = 2
};

/// 世界会话生命周期状态
enum WorldSessionState
{
    WORLD_SESSION_STATE_CREATED        = 0,                 ///< 已建连待初始化角色列表阶段
    WORLD_SESSION_STATE_CHAR_SELECTION = 1,                 ///< 处于选人界面阶段
    WORLD_SESSION_STATE_READY          = 2,                 ///< 已进入游戏世界就绪阶段
    WORLD_SESSION_STATE_OFFLINE        = 3                  ///< 断线保留等待重连阶段
};

#define MAX_DECLINED_NAME_CASES 5

/// 俄语等屈折语角色名的 5 种变格形式
struct DeclinedName
{
    std::string name[MAX_DECLINED_NAME_CASES];
};

/// 角色名称查询响应缓存结构
struct CharacterNameQueryResponse
{
    ObjectGuid          guid;                   // pc's guid | 玩家 GUID
    std::string         name;                   // pc's name | 玩家名称
    std::string         realm;                  // realm name (xrealm battlegrounds) | 跨服战场服务器名
    uint32              race        = 0;        // pc's race | 种族
    uint32              gender      = 0;        // pc's gender | 性别
    uint32              classid     = 0;        // pc's class | 职业
    DeclinedName        declined;               // pc's declined name definitions | 变格名称
};

/// 账号标志位
enum AccountFlags
{
    ACCOUNT_FLAG_SHOW_ANTICHEAT = 0x01,
    ACCOUNT_FLAG_SILENCED       = 0x02,
    ACCOUNT_FLAG_SHOW_ANTISPAM  = 0x04,
    ACCOUNT_FLAG_HIDDEN         = 0x08,
};

// class to deal with packet processing
// allows to determine if next packet is safe to be processed
// 数据包过滤策略基类：用于判断某数据包是否可在当前更新上下文中处理
class PacketFilter
{
    public:
        explicit PacketFilter(WorldSession* pSession) : m_pSession(pSession) {}
        virtual ~PacketFilter() {}

        virtual bool Process(WorldPacket const& /*packet*/) const { return true; }
        virtual bool ProcessLogout() const { return true; }

    protected:
        WorldSession* const m_pSession;
};

// process only thread-safe packets in Map::Update() | 地图线程过滤器：仅允许在 Map::Update() 中安全处理的线程安全数据包
class MapSessionFilter : public PacketFilter
{
    public:
        explicit MapSessionFilter(WorldSession* pSession) : PacketFilter(pSession) {}
        ~MapSessionFilter() {}

        virtual bool Process(WorldPacket const& packet) const override;
        // in Map::Update() we do not process player logout! | 禁止在 Map::Update() 中执行玩家登出逻辑
        virtual bool ProcessLogout() const override { return false; }
};

// class used to filer only thread-unsafe packets from queue
// in order to update only be used in World::UpdateSessions()
// 世界主线程过滤器：筛选需在 World::UpdateSessions() 串行处理的非线程安全数据包
class WorldSessionFilter : public PacketFilter
{
    public:
        explicit WorldSessionFilter(WorldSession* pSession) : PacketFilter(pSession) {}
        ~WorldSessionFilter() {}

        virtual bool Process(WorldPacket const& packet) const override;
};

/// Player session in the World | 玩家在世界服中的会话对象（管理连接、队列包分发、登出状态机及各类 CMSG 处理器）
class WorldSession
{
        friend class CharacterHandler;

    public:
        WorldSession(uint32 id, WorldSocket* sock, AccountTypes sec, uint8 expansion, time_t mute_time, LocaleConstant locale, std::string accountName, uint32 accountFlags, uint32 recruitingFriend, bool isARecruiter);
        ~WorldSession();

        // Set this session have no attached socket but keep it alive for short period of time to permit a possible reconnection
        // 将会话置为离线保留态（关闭现有连接但短时间内保留会话以支持重连）
        void SetOffline();
        void SetOnline();
        void SetInCharSelection();

        // Request set offline, close socket and put session offline | 重连时请求接管新 Socket 连接
        bool RequestNewSocket(WorldSocket* socket);
        bool IsOffline() const { return m_sessionState == WORLD_SESSION_STATE_OFFLINE; }
        WorldSessionState GetState() const { return m_sessionState; }

        bool PlayerLoading() const { return m_playerLoading; }
        bool PlayerLogout() const { return m_playerLogout; }
        bool PlayerLogoutWithSave() const { return m_playerLogout && m_playerSave; }

        void SizeError(WorldPacket const& packet, uint32 size) const;

        void SendPacket(WorldPacket const& packet, bool forcedSend = false) const;
        void SendExpectedSpamRecords();
        void SendMotd();
        void SendOfflineNameQueryResponses();
        void SendNotification(const char* format, ...) const ATTR_PRINTF(2, 3);
        void SendNotification(int32 string_id, ...) const;
        void SendPetNameInvalid(uint32 error, const std::string& name, DeclinedName* declinedName) const;
        void SendPartyResult(PartyOperation operation, const std::string& member, PartyResult res) const;
        void SendAreaTriggerMessage(const char* Text, ...) const ATTR_PRINTF(2, 3);
        void SendTransferAborted(uint32 mapid, uint8 reason, uint8 arg = 0) const;
        void SendQueryTimeResponse() const;

        bool IsInitialZoneUpdated() { return m_initialZoneUpdated; }

        AccountTypes GetSecurity() const { return _security; }
        uint32 GetAccountFlags() const { return m_accountFlags; }
        bool HasAccountFlag(uint32 flags) const { return (m_accountFlags & flags) != 0; }
        void AddAccountFlag(uint32 flags) { m_accountFlags |= flags; }
        void RemoveAccountFlag(uint32 flags) { m_accountFlags &= ~flags; }
        uint32 GetAccountId() const { return _accountId; }
        std::string const& GetAccountName() const { return m_accountName; }
        Player* GetPlayer() const { return _player; }
        char const* GetPlayerName() const;
        std::string GetChatType(uint32 type);
        void SetSecurity(AccountTypes security) { _security = security; }
#ifdef ENABLE_PLAYERBOTS
        // Players connected without socket are bot
        const std::string GetRemoteAddress() const { return m_socket ? m_socket->GetRemoteAddress() : "disconnected/bot"; }
#else
        const std::string GetRemoteAddress() const { return m_socket ? m_socket->GetRemoteAddress() : "disconnected"; }
#endif
        const std::string& GetLocalAddress() const { return m_localAddress; }

        void SetPlayer(Player* plr, uint32 playerGuid);
        uint8 GetExpansion() const { return m_expansion; }
        void SetExpansion(uint8 expansion);

        void InitializeAnticheat(const BigNumber& K);
        void AssignAnticheat(std::unique_ptr<SessionAnticheatInterface>&& anticheat);
        SessionAnticheatInterface* GetAnticheat() const { return m_anticheat.get(); }

#ifdef ENABLE_PLAYERBOTS
        void SetNoAnticheat();
#endif

        /// Session in auth.queue currently | 设置会话是否处于登录排队中
        void SetInQueue(bool state) { m_inQueue = state; }

        /// Is the user engaged in a log out process? | 判断当前是否已发起或正在执行登出流程
        bool isLogingOut() const { return _logoutTime || m_playerLogout; }

        /// Engage the logout process for the user | 触发玩家登出请求（记录请求时间及是否保存数据库/踢出连接）
        void LogoutRequest(time_t requestTime, bool saveToDB = true, bool kickSession = false)
        {
            _logoutTime = requestTime;
            m_playerSave = saveToDB;
            m_kickSession = kickSession;
        }

        void AfkStateChange(bool state);

        /// Is logout cooldown expired? | 判断 20 秒延迟登出倒计时是否已到期
        bool ShouldLogOut(time_t currTime) const
        {
            return (_logoutTime > 0 && currTime >= _logoutTime + 20);
        }

        /// 判断离线断线保留宽限期（60 秒）是否已超时
        bool ShouldDisconnect(time_t currTime)
        {
            return (_logoutTime > 0 && currTime >= _logoutTime + 60);
        }

        /// 判断 AFK 挂机超时（15 分钟）是否需要断开连接
        bool ShouldAfkDisconnect(time_t currTime) const
        {
            return (m_afkTime > 0 && currTime >= m_afkTime + 15 * MINUTE);
        }

        /// 执行完整玩家下线清理（保存角色、退队/退频道、移出世界并下发登出完成包）
        void LogoutPlayer();
        void KickPlayer(bool save = false, bool inPlace = false); // inplace variable needed for shutdown | 踢出玩家（inPlace=true 用于关服时原地立即登出）

        /// 将网络层收到的客户端数据包按线程策略入队或立即执行
        void QueuePacket(std::unique_ptr<WorldPacket> new_packet);

        /// 清空待处理的地图线程移动包（防止传送/控制权切换时残留旧包）
        void DeleteMovementPackets();

        /// 世界主线程更新：处理非线程安全队列包、反作弊更新与会话状态机流转
        bool Update(uint32 diff);
        /// 地图线程更新：发送时间同步包并处理地图线程安全队列包（PROCESS_MAP_THREAD）
        void UpdateMap(uint32 diff);

        /// Handle the authentication waiting queue (to be completed) | 发送登录排队位置通知
        void SendAuthWaitQue(uint32 position) const;

        void SendNameQueryResponse(CharacterNameQueryResponse& response) const;
        void SendNameQueryResponseFromDB(ObjectGuid guid) const;
        static void SendNameQueryResponseFromDBCallBack(QueryResult* result, uint32 accountId);

        void SendTrainerList(ObjectGuid guid) const;

        void SendListInventory(ObjectGuid guid) const;
        bool CheckBanker(ObjectGuid guid) const;
        void SendShowBank(ObjectGuid guid) const;
        bool CheckMailBox(ObjectGuid guid) const;
        void SendTabardVendorActivate(ObjectGuid guid) const;
        void SendSpiritResurrect() const;
        void SendBindPoint(Creature* npc) const;

        void SendGMTicketResult(uint32 opcode, uint32 result) const;
        void SendGMTicket(const GMTicket& ticket, time_t now = time(nullptr)) const;

        void SendBattleGroundList(ObjectGuid guid, BattleGroundTypeId bgTypeId) const;

        void SendTradeStatus(const TradeStatusInfo& status) const;
        void SendUpdateTrade(bool trader_state = true) const;
        void SendCancelTrade(TradeStatus status) const;

        void SendPetitionQueryOpcode(ObjectGuid petitionguid) const;

        // pet
        void SendPetNameQuery(ObjectGuid guid, uint32 petnumber) const;
        void SendStablePet(ObjectGuid guid) const;
        void SendStableResult(uint8 res) const;
        bool CheckStableMaster(ObjectGuid guid) const;

        // Account Data
        AccountData* GetAccountData(AccountDataType type) { return &m_accountData[type]; }
        void SetAccountData(AccountDataType type, time_t time_, const std::string& data);
        void SendAccountDataTimes();
        void LoadGlobalAccountData();
        void LoadAccountData(std::unique_ptr<QueryResult> queryResult, uint32 mask);
        void LoadTutorialsData();
        void SendTutorialsData();
        void SaveTutorialsData();
        uint32 GetTutorialInt(uint32 intId)
        {
            return m_Tutorials[intId];
        }

        void SetTutorialInt(uint32 intId, uint32 value)
        {
            if (m_Tutorials[intId] != value)
            {
                m_Tutorials[intId] = value;
                if (m_tutorialState == TUTORIALDATA_UNCHANGED)
                    m_tutorialState = TUTORIALDATA_CHANGED;
            }
        }

        // auction
        void SendAuctionHello(Unit* unit) const;
        void SendAuctionCommandResult(AuctionEntry* auc, AuctionAction Action, AuctionError ErrorCode, InventoryResult invError = EQUIP_ERR_OK, uint32 auctionId = 0) const;
        void SendAuctionBidderNotification(AuctionEntry* auction, bool won) const;
        void SendAuctionOwnerNotification(AuctionEntry* auction, bool sold) const;
        void SendAuctionRemovedNotification(AuctionEntry* auction) const;
        static void SendAuctionOutbiddedMail(AuctionEntry* auction, uint32 oldBidderLowGuid, uint32 oldBid);
        static void SendAuctionCancelledToBidderMail(AuctionEntry* auction);
        void BuildListAuctionItems(std::vector<AuctionEntry*> const& auctions, WorldPacket& data, std::wstring const& searchedname, uint32 listfrom, uint32 levelmin,
                                   uint32 levelmax, uint32 usable, uint32 inventoryType, uint32 itemClass, uint32 itemSubClass, uint32 quality, uint32& count, uint32& totalcount, bool isFull) const;

        AuctionHouseEntry const* GetCheckedAuctionHouseForAuctioneer(ObjectGuid guid) const;

        // Item Enchantment
        void SendItemEnchantTimeUpdate(ObjectGuid playerGuid, ObjectGuid itemGuid, uint32 slot, uint32 duration) const;

        // Taxi
        void SendTaxiStatus(ObjectGuid guid) const;
        void SendTaxiMenu(Creature* unit) const;
        bool SendLearnNewTaxiNode(Creature* unit) const;
        void SendActivateTaxiReply(ActivateTaxiReply reply) const;

        // Guild/Arena Team
        void SendGuildCommandResult(uint32 typecmd, const std::string& str, uint32 cmdresult) const;
        void SendArenaTeamCommandResult(uint32 team_action, const std::string& team, const std::string& player, uint32 error_id) const;
        void SendNotInArenaTeamPacket(uint8 type) const;
        void SendPetitionShowList(ObjectGuid guid) const;
        void SendSaveGuildEmblem(uint32 msg) const;
        void SendBattleGroundOrArenaJoinError(uint8 err) const;

        // Looking For Group
        // TRUE values set by client sending CMSG_LFG_SET_AUTOJOIN and CMSG_LFM_CLEAR_AUTOFILL before player login
        void SendMeetingStoneInProgress();
        void SendMeetingStoneComplete();
        void SendLFGUpdate();
        void SendLFGUpdateLFG();
        void SendLFGUpdateLFM();

        static void BuildPartyMemberStatsChangedPacket(Player* player, WorldPacket& data);

        // Account mute time
        time_t m_muteTime;
        // Ticket squelch timer: prevent ticket spam
        ShortTimeTracker m_ticketSquelchTimer;

        // Locales
        LocaleConstant GetSessionDbcLocale() const { return m_sessionDbcLocale; }
        int GetSessionDbLocaleIndex() const { return m_sessionDbLocaleIndex; }
        const char* GetMangosString(int32 entry) const;

        uint32 GetLatency() const { return m_latency; }
        uint32 GetOneWayDelay() const;
        void SetLatency(uint32 latency) { m_latency = latency; }
        uint32 getDialogStatus(const Player* pPlayer, const Object* questgiver, uint32 defstatus) const;
        ClientOSType GetOS() const { return m_clientOS; }
        void SetOS(ClientOSType os) { m_clientOS = os; }
        ClientPlatformType GetPlatform() const { return m_clientPlatform; }
        void SetPlatform(ClientPlatformType platform) { m_clientPlatform = platform; }
        uint32 GetGameBuild() const { return m_gameBuild; }
        void SetGameBuild(uint32 version) { m_gameBuild = version; }
        uint32 GetAccountMaxLevel() const { return m_accountMaxLevel; }
        void SetAccountMaxLevel(uint32 level) { m_accountMaxLevel = level; }
        uint32 GetOrderCounter() const { return m_orderCounter; }
        void IncrementOrderCounter() { ++m_orderCounter; }

        // Recruit-A-Friend Handling
        uint32 GetRecruitingFriendId() const { return m_recruitingFriendId; }
        bool IsARecruiter() const { return m_isRecruiter; }

        // Time Synchronisation
        void ResetTimeSync();
        void SendTimeSync();

        // Thread safety
        uint32 GetCurrentPlayerLevel() { return m_currentPlayerLevel; }
        void SetCurrentPlayerLevel(uint32 level) { m_currentPlayerLevel = level; }

        // Misc
        void SendKnockBack(Unit* who, float angle, float horizontalSpeed, float verticalSpeed);
        void SendPlaySpellVisual(ObjectGuid guid, uint32 spellArtKit) const;

#ifdef ENABLE_PLAYERBOTS
        void SendTeleportToObservers(float x, float y, float z, float orientation);
#endif

        void SendAuthOk() const;
        void SendAuthQueued() const;
        void SendKickReason(uint8 reason, std::string const& string) const;

        // opcodes handlers | 客户端操作码处理器声明
        void Handle_NULL(WorldPacket& recvPacket);          // not used | 未使用操作码空处理器
        void Handle_EarlyProccess(WorldPacket& recvPacket); // just mark packets processed in WorldSocket::OnRead | 仅标记为网络层早期已处理
        void Handle_ServerSide(WorldPacket& recvPacket);    // sever side only, can't be accepted from client | 仅服务端下行使用，不允许客户端发送
        void Handle_Deprecated(WorldPacket& recvPacket);    // never used anymore by client | 已废弃操作码

        void HandleCharEnumOpcode(WorldPacket& recvPacket);
        void HandleCharDeleteOpcode(WorldPacket& recvPacket);
        void HandleCharCreateOpcode(WorldPacket& recvPacket);
        void HandlePlayerLoginOpcode(WorldPacket& recvPacket);
        void HandleCharEnum(std::unique_ptr<QueryResult> queryResult);
        void HandlePlayerLogin(LoginQueryHolder* holder);
        void HandlePlayerReconnect();

        // played time
        void HandlePlayedTime(WorldPacket& recvPacket);

        // new
        void HandleMoveRootAck(WorldPacket& recvPacket);

        // new inspect
        void HandleInspectOpcode(WorldPacket& recvPacket);

        // new party stats
        void HandleInspectHonorStatsOpcode(WorldPacket& recvPacket);

        void HandleMoveFlagChangeOpcode(WorldPacket& recvPacket);

        void HandleMountSpecialAnimOpcode(WorldPacket& recvdata);

        // character view
        void HandleShowingHelmOpcode(WorldPacket& recv_data);
        void HandleShowingCloakOpcode(WorldPacket& recv_data);

        // repair
        void HandleRepairItemOpcode(WorldPacket& recvPacket);

        // Knockback
        void HandleMoveKnockBackAck(WorldPacket& recvPacket);

        void HandleMoveTeleportAckOpcode(WorldPacket& recvPacket);
        void HandleForceSpeedChangeAckOpcodes(WorldPacket& recv_data);

        void HandleRepopRequestOpcode(WorldPacket& recvPacket);
        void HandleAutostoreLootItemOpcode(WorldPacket& recvPacket);
        void HandleLootMoneyOpcode(WorldPacket& recvPacket);
        void HandleLootOpcode(WorldPacket& recvPacket);
        void HandleLootReleaseOpcode(WorldPacket& recvPacket);
        void HandleLootMasterGiveOpcode(WorldPacket& recvPacket);
        void HandleWhoOpcode(WorldPacket& recvPacket);
        void HandleLogoutRequestOpcode(WorldPacket& recvPacket);
        void HandlePlayerLogoutOpcode(WorldPacket& recvPacket);
        void HandleLogoutCancelOpcode(WorldPacket& recvPacket);

        void HandleGMTicketSystemStatusOpcode(WorldPacket& recvPacket);
        void HandleGMTicketGetTicketOpcode(WorldPacket& recvPacket);
        void HandleGMTicketCreateOpcode(WorldPacket& recvPacket);
        void HandleGMTicketUpdateTextOpcode(WorldPacket& recvPacket);
        void HandleGMTicketDeleteTicketOpcode(WorldPacket& recvPacket);
        void HandleGMSurveySubmitOpcode(WorldPacket& recvPacket);

        void HandleTogglePvP(WorldPacket& recvPacket);

        void HandleZoneUpdateOpcode(WorldPacket& recvPacket);
        void HandleSetTargetOpcode(WorldPacket& recvPacket);
        void HandleSetSelectionOpcode(WorldPacket& recvPacket);
        void HandleStandStateChangeOpcode(WorldPacket& recvPacket);
        void HandleEmoteOpcode(WorldPacket& recvPacket);
        void HandleContactListOpcode(WorldPacket& recvPacket);
        void HandleAddFriendOpcode(WorldPacket& recvPacket);
        static void HandleAddFriendOpcodeCallBack(QueryResult* result, uint32 accountId, std::string friendNote);
        void HandleDelFriendOpcode(WorldPacket& recvPacket);
        void HandleAddIgnoreOpcode(WorldPacket& recvPacket);
        static void HandleAddIgnoreOpcodeCallBack(QueryResult* result, uint32 accountId);
        void HandleDelIgnoreOpcode(WorldPacket& recvPacket);
        void HandleSetContactNotesOpcode(WorldPacket& recvPacket);
        void HandleBugOpcode(WorldPacket& recvPacket);
        void HandleSetAmmoOpcode(WorldPacket& recvPacket);
        void HandleItemNameQueryOpcode(WorldPacket& recvPacket);

        void HandleAreaTriggerOpcode(WorldPacket& recvPacket);

        void HandleSetFactionAtWarOpcode(WorldPacket& recv_data);
        void HandleSetWatchedFactionOpcode(WorldPacket& recv_data);
        void HandleSetFactionInactiveOpcode(WorldPacket& recv_data);

        void HandleUpdateAccountData(WorldPacket& recvPacket);
        void HandleRequestAccountData(WorldPacket& recvPacket);
        void HandleSetActionButtonOpcode(WorldPacket& recvPacket);

        void HandleGameObjectUseOpcode(WorldPacket& recPacket);
        void HandleMeetingStoneInfoOpcode(WorldPacket& recPacket);

        void HandleNameQueryOpcode(WorldPacket& recvPacket);

        void HandleQueryTimeOpcode(WorldPacket& recvPacket);

        void HandleCreatureQueryOpcode(WorldPacket& recvPacket);

        void HandleGameObjectQueryOpcode(WorldPacket& recvPacket);

        void HandleMoveWorldportAckOpcode(WorldPacket& recvPacket);
        void HandleMoveWorldportAckOpcode();                // for server-side calls

        void HandleMovementOpcodes(WorldPacket& recvPacket);
        void HandleSetActiveMoverOpcode(WorldPacket& recv_data);
        void HandleMoveNotActiveMoverOpcode(WorldPacket& recv_data);
        void HandleMoveTimeSkippedOpcode(WorldPacket& recv_data);

        bool ProcessMovementInfo(MovementInfo& movementInfo, Unit* mover, Player* plMover, WorldPacket& recv_data);

        void HandleRequestRaidInfoOpcode(WorldPacket& recv_data);

        void HandleGroupInviteOpcode(WorldPacket& recvPacket);
        void HandleGroupAcceptOpcode(WorldPacket& recvPacket);
        void HandleGroupDeclineOpcode(WorldPacket& recvPacket);
        void HandleGroupUninviteOpcode(WorldPacket& recvPacket);
        void HandleGroupUninviteGuidOpcode(WorldPacket& recvPacket);
        void HandleGroupSetLeaderOpcode(WorldPacket& recvPacket);
        void HandleGroupDisbandOpcode(WorldPacket& recvPacket);
        void HandleOptOutOfLootOpcode(WorldPacket& recv_data);
        void HandleLootMethodOpcode(WorldPacket& recvPacket);
        void HandleLootRoll(WorldPacket& recv_data);
        void HandleRequestPartyMemberStatsOpcode(WorldPacket& recv_data);
        void HandleRaidTargetUpdateOpcode(WorldPacket& recv_data);
        void HandleRaidReadyCheckOpcode(WorldPacket& recv_data);
        void HandleRaidReadyCheckFinishedOpcode(WorldPacket& recv_data);
        void HandleGroupRaidConvertOpcode(WorldPacket& recv_data);
        void HandleGroupChangeSubGroupOpcode(WorldPacket& recv_data);
        void HandleGroupSwapSubGroupOpcode(WorldPacket& recv_data);
        void HandleGroupAssistantLeaderOpcode(WorldPacket& recv_data);
        void HandlePartyAssignmentOpcode(WorldPacket& recv_data);

        void HandlePetitionBuyOpcode(WorldPacket& recv_data);
        void HandlePetitionShowSignOpcode(WorldPacket& recv_data);
        void HandlePetitionQueryOpcode(WorldPacket& recv_data);
        void HandlePetitionRenameOpcode(WorldPacket& recv_data);
        void HandlePetitionSignOpcode(WorldPacket& recv_data);
        void HandlePetitionDeclineOpcode(WorldPacket& recv_data);
        void HandleOfferPetitionOpcode(WorldPacket& recv_data);
        void HandleTurnInPetitionOpcode(WorldPacket& recv_data);

        void HandleGuildQueryOpcode(WorldPacket& recvPacket);
        void HandleGuildCreateOpcode(WorldPacket& recvPacket);
        void HandleGuildInviteOpcode(WorldPacket& recvPacket);
        void HandleGuildRemoveOpcode(WorldPacket& recvPacket);
        void HandleGuildAcceptOpcode(WorldPacket& recvPacket);
        void HandleGuildDeclineOpcode(WorldPacket& recvPacket);
        void HandleGuildInfoOpcode(WorldPacket& recvPacket);
        void HandleGuildEventLogQueryOpcode(WorldPacket& recvPacket);
        void HandleGuildRosterOpcode(WorldPacket& recvPacket);
        void HandleGuildPromoteOpcode(WorldPacket& recvPacket);
        void HandleGuildDemoteOpcode(WorldPacket& recvPacket);
        void HandleGuildLeaveOpcode(WorldPacket& recvPacket);
        void HandleGuildDisbandOpcode(WorldPacket& recvPacket);
        void HandleGuildLeaderOpcode(WorldPacket& recvPacket);
        void HandleGuildMOTDOpcode(WorldPacket& recvPacket);
        void HandleGuildSetPublicNoteOpcode(WorldPacket& recvPacket);
        void HandleGuildSetOfficerNoteOpcode(WorldPacket& recvPacket);
        void HandleGuildRankOpcode(WorldPacket& recvPacket);
        void HandleGuildAddRankOpcode(WorldPacket& recvPacket);
        void HandleGuildDelRankOpcode(WorldPacket& recvPacket);
        void HandleGuildChangeInfoTextOpcode(WorldPacket& recvPacket);
        void HandleSaveGuildEmblemOpcode(WorldPacket& recvPacket);

        void HandleTaxiNodeStatusQueryOpcode(WorldPacket& recvPacket);
        void HandleTaxiQueryAvailableNodes(WorldPacket& recvPacket);
        void HandleActivateTaxiOpcode(WorldPacket& recvPacket);
        void HandleActivateTaxiExpressOpcode(WorldPacket& recvPacket);
        void HandleMoveSplineDoneOpcode(WorldPacket& recvPacket);

        void HandleTabardVendorActivateOpcode(WorldPacket& recvPacket);
        void HandleBankerActivateOpcode(WorldPacket& recvPacket);
        void HandleBuyBankSlotOpcode(WorldPacket& recvPacket);
        void HandleTrainerListOpcode(WorldPacket& recvPacket);
        void HandleTrainerBuySpellOpcode(WorldPacket& recvPacket);

        void HandlePetitionShowListOpcode(WorldPacket& recvPacket);
        void HandleGossipHelloOpcode(WorldPacket& recvPacket);
        void HandleGossipSelectOptionOpcode(WorldPacket& recvPacket);
        void HandleSpiritHealerActivateOpcode(WorldPacket& recvPacket);
        void HandleNpcTextQueryOpcode(WorldPacket& recvPacket);
        void HandleBinderActivateOpcode(WorldPacket& recvPacket);
        void HandleListStabledPetsOpcode(WorldPacket& recvPacket);
        void HandleStablePet(WorldPacket& recvPacket);
        void HandleUnstablePet(WorldPacket& recvPacket);
        void HandleBuyStableSlot(WorldPacket& recvPacket);
        void HandleStableRevivePet(WorldPacket& recvPacket);
        void HandleStableSwapPet(WorldPacket& recvPacket);

        void HandleDuelAcceptedOpcode(WorldPacket& recvPacket);
        void HandleDuelCancelledOpcode(WorldPacket& recvPacket);

        void HandleAcceptTradeOpcode(WorldPacket& recvPacket);
        void HandleBeginTradeOpcode(WorldPacket& recvPacket);
        void HandleBusyTradeOpcode(WorldPacket& recvPacket);
        void HandleCancelTradeOpcode(WorldPacket& recvPacket);
        void HandleClearTradeItemOpcode(WorldPacket& recvPacket);
        void HandleIgnoreTradeOpcode(WorldPacket& recvPacket);
        void HandleInitiateTradeOpcode(WorldPacket& recvPacket);
        void HandleSetTradeGoldOpcode(WorldPacket& recvPacket);
        void HandleSetTradeItemOpcode(WorldPacket& recvPacket);
        void HandleUnacceptTradeOpcode(WorldPacket& recvPacket);

        void HandleAuctionHelloOpcode(WorldPacket& recvPacket);
        void HandleAuctionListItems(WorldPacket& recv_data);
        void HandleAuctionListBidderItems(WorldPacket& recv_data);
        void HandleAuctionSellItem(WorldPacket& recv_data);

        void HandleAuctionRemoveItem(WorldPacket& recv_data);
        void HandleAuctionListOwnerItems(WorldPacket& recv_data);
        void HandleAuctionPlaceBid(WorldPacket& recv_data);

        void HandleGetMailList(WorldPacket& recv_data);
        void HandleSendMail(WorldPacket& recv_data);
        void HandleMailTakeMoney(WorldPacket& recv_data);
        void HandleMailTakeItem(WorldPacket& recv_data);
        void HandleMailMarkAsRead(WorldPacket& recv_data);
        void HandleMailReturnToSender(WorldPacket& recv_data);
        void HandleMailDelete(WorldPacket& recv_data);
        void HandleItemTextQuery(WorldPacket& recv_data);
        void HandleMailCreateTextItem(WorldPacket& recv_data);
        void HandleQueryNextMailTime(WorldPacket& recv_data);
        void HandleCancelChanneling(WorldPacket& recv_data);

        void HandleSplitItemOpcode(WorldPacket& recvPacket);
        void HandleSwapInvItemOpcode(WorldPacket& recvPacket);
        void HandleDestroyItemOpcode(WorldPacket& recvPacket);
        void HandleAutoEquipItemOpcode(WorldPacket& recvPacket);
        void HandleItemQuerySingleOpcode(WorldPacket& recvPacket);
        void HandleSellItemOpcode(WorldPacket& recvPacket);
        void HandleBuyItemInSlotOpcode(WorldPacket& recvPacket);
        void HandleBuyItemOpcode(WorldPacket& recvPacket);
        void HandleListInventoryOpcode(WorldPacket& recvPacket);
        void HandleAutoStoreBagItemOpcode(WorldPacket& recvPacket);
        void HandleReadItemOpcode(WorldPacket& recvPacket);
        void HandleAutoEquipItemSlotOpcode(WorldPacket& recvPacket);
        void HandleSwapItem(WorldPacket& recvPacket);
        void HandleBuybackItem(WorldPacket& recvPacket);
        void HandleAutoBankItemOpcode(WorldPacket& recvPacket);
        void HandleAutoStoreBankItemOpcode(WorldPacket& recvPacket);
        void HandleWrapItemOpcode(WorldPacket& recvPacket);

        void HandleAttackSwingOpcode(WorldPacket& recvPacket);
        void HandleAttackStopOpcode(WorldPacket& recvPacket);
        void HandleSetSheathedOpcode(WorldPacket& recvPacket);

        void HandleUseItemOpcode(WorldPacket& recvPacket);
        void HandleOpenItemOpcode(WorldPacket& recvPacket);
        void HandleCastSpellOpcode(WorldPacket& recvPacket);
        void HandleCancelCastOpcode(WorldPacket& recvPacket);
        void HandleCancelAuraOpcode(WorldPacket& recvPacket);
        void HandleCancelGrowthAuraOpcode(WorldPacket& recvPacket);
        void HandleCancelAutoRepeatSpellOpcode(WorldPacket& recvPacket);

        void HandleLearnTalentOpcode(WorldPacket& recvPacket);
        void HandleTalentWipeConfirmOpcode(WorldPacket& recvPacket);
        void HandleUnlearnSkillOpcode(WorldPacket& recvPacket);

        void HandleQuestgiverStatusQueryOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverStatusMultipleQuery(WorldPacket& recvPacket);
        void HandleQuestgiverHelloOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverAcceptQuestOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverQueryQuestOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverChooseRewardOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverRequestRewardOpcode(WorldPacket& recvPacket);
        void HandleQuestQueryOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverCancel(WorldPacket& recv_data);
        void HandleQuestLogSwapQuest(WorldPacket& recv_data);
        void HandleQuestLogRemoveQuest(WorldPacket& recv_data);
        void HandleQuestConfirmAccept(WorldPacket& recv_data);
        void HandleQuestgiverCompleteQuest(WorldPacket& recv_data);
        bool CanInteractWithQuestGiver(ObjectGuid guid, char const* descr) const;

        void HandleQuestgiverQuestAutoLaunch(WorldPacket& recvPacket);
        void HandlePushQuestToParty(WorldPacket& recvPacket);
        void HandleQuestPushResult(WorldPacket& recvPacket);

        bool CheckChatMessage(std::string&, bool addon = false);
        void SendPlayerNotFoundNotice(const std::string& name) const;
        void SendWrongFactionNotice() const;
        void SendChatRestrictedNotice(ChatRestrictionType restriction) const;
        void HandleMessagechatOpcode(WorldPacket& recvPacket);
        void HandleTextEmoteOpcode(WorldPacket& recvPacket);
        void HandleChatIgnoredOpcode(WorldPacket& recvPacket);

        void HandleReclaimCorpseOpcode(WorldPacket& recvPacket);
        void HandleCorpseQueryOpcode(WorldPacket& recvPacket);
        void HandleResurrectResponseOpcode(WorldPacket& recvPacket);
        void HandleSummonResponseOpcode(WorldPacket& recv_data);

        bool CheckChatChannelNameAndPassword(std::string& name, std::string& pass);
        void HandleJoinChannelOpcode(WorldPacket& recvPacket);
        void HandleLeaveChannelOpcode(WorldPacket& recvPacket);
        void HandleChannelListOpcode(WorldPacket& recvPacket);
        void HandleChannelPasswordOpcode(WorldPacket& recvPacket);
        void HandleChannelSetOwnerOpcode(WorldPacket& recvPacket);
        void HandleChannelOwnerOpcode(WorldPacket& recvPacket);
        void HandleChannelModeratorOpcode(WorldPacket& recvPacket);
        void HandleChannelUnmoderatorOpcode(WorldPacket& recvPacket);
        void HandleChannelMuteOpcode(WorldPacket& recvPacket);
        void HandleChannelUnmuteOpcode(WorldPacket& recvPacket);
        void HandleChannelInviteOpcode(WorldPacket& recvPacket);
        void HandleChannelKickOpcode(WorldPacket& recvPacket);
        void HandleChannelBanOpcode(WorldPacket& recvPacket);
        void HandleChannelUnbanOpcode(WorldPacket& recvPacket);
        void HandleChannelAnnouncementsOpcode(WorldPacket& recvPacket);
        void HandleChannelModerateOpcode(WorldPacket& recvPacket);
        void HandleChannelDisplayListQueryOpcode(WorldPacket& recvPacket);
        void HandleGetChannelMemberCountOpcode(WorldPacket& recvPacket);
        void HandleSetChannelWatchOpcode(WorldPacket& recvPacket);

        void HandleCompleteCinematic(WorldPacket& recvPacket);
        void HandleNextCinematicCamera(WorldPacket& recvPacket);

        void HandlePageQuerySkippedOpcode(WorldPacket& recvPacket);
        void HandlePageTextQueryOpcode(WorldPacket& recvPacket);

        void HandleTutorialFlagOpcode(WorldPacket& recv_data);
        void HandleTutorialClearOpcode(WorldPacket& recv_data);
        void HandleTutorialResetOpcode(WorldPacket& recv_data);

        // Pet
        void HandlePetAction(WorldPacket& recv_data);
        void HandlePetStopAttack(WorldPacket& recv_data);
        void HandlePetNameQueryOpcode(WorldPacket& recv_data);
        void HandlePetSetAction(WorldPacket& recv_data);
        void HandlePetAbandon(WorldPacket& recv_data);
        void HandlePetRename(WorldPacket& recv_data);
        void HandlePetCancelAuraOpcode(WorldPacket& recvPacket);
        void HandlePetUnlearnOpcode(WorldPacket& recvPacket);
        void HandlePetSpellAutocastOpcode(WorldPacket& recvPacket);
        void HandlePetCastSpellOpcode(WorldPacket& recvPacket);

        void HandleSetActionBarTogglesOpcode(WorldPacket& recv_data);

        void HandleCharRenameOpcode(WorldPacket& recv_data);
        static void HandleChangePlayerNameOpcodeCallBack(QueryResult* result, uint32 accountId, std::string newname);
        void HandleSetPlayerDeclinedNamesOpcode(WorldPacket& recv_data);

        void HandleTotemDestroyed(WorldPacket& recv_data);

        // BattleGround
        void HandleBattlemasterHelloOpcode(WorldPacket& recv_data);
        void HandleBattlemasterJoinOpcode(WorldPacket& recv_data);
        void HandleBattleGroundPlayerPositionsOpcode(WorldPacket& recv_data);
        void HandlePVPLogDataOpcode(WorldPacket& recv_data);
        void HandleBattlefieldStatusOpcode(WorldPacket& recv_data);
        void HandleBattlefieldPortOpcode(WorldPacket& recv_data);
        void HandleBattlefieldListOpcode(WorldPacket& recv_data);
        void HandleLeaveBattlefieldOpcode(WorldPacket& recv_data);
        void HandleBattlemasterJoinArena(WorldPacket& recv_data);
        void HandleReportPvPAFK(WorldPacket& recv_data);

        void HandleWorldTeleportOpcode(WorldPacket& recv_data);
        void HandleMinimapPingOpcode(WorldPacket& recv_data);
        void HandleRandomRollOpcode(WorldPacket& recv_data);
        void HandleFarSightOpcode(WorldPacket& recv_data);
        void HandleSetDungeonDifficultyOpcode(WorldPacket& recv_data);
        void HandleLfgAcceptLfgMatch(WorldPacket& recv_data);
        void HandleLfgDeclineLfgMatch(WorldPacket& recv_data);
        void HandleLfgCancelPendingLfg(WorldPacket& recv_data);
        void HandleLfgSetAutoJoinOpcode(WorldPacket& recv_data);
        void HandleLfgClearAutoJoinOpcode(WorldPacket& recv_data);
        void HandleLfmSetAutoFillOpcode(WorldPacket& recv_data);
        void HandleLfmClearAutoFillOpcode(WorldPacket& recv_data);
        void HandleLfgClearOpcode(WorldPacket& recv_data);
        void HandleSetLfgOpcode(WorldPacket& recv_data);
        void HandleLfmClearOpcode(WorldPacket& recv_data);
        void HandleSetLfmOpcode(WorldPacket& recv_data);
        void HandleSetLfgCommentOpcode(WorldPacket& recv_data);
        void HandleLFGListQuery(WorldPacket& recvPacket);
        void HandleSetTitleOpcode(WorldPacket& recv_data);
        void HandleRealmSplitOpcode(WorldPacket& recv_data);
        void HandleTimeSyncResp(WorldPacket& recv_data);
        void HandleWhoisOpcode(WorldPacket& recv_data);
        void HandleResetInstancesOpcode(WorldPacket& recv_data);

        // Arena Team
        void HandleInspectArenaTeamsOpcode(WorldPacket& recv_data);
        void HandleArenaTeamQueryOpcode(WorldPacket& recv_data);
        void HandleArenaTeamRosterOpcode(WorldPacket& recv_data);
        void HandleArenaTeamInviteOpcode(WorldPacket& recv_data);
        void HandleArenaTeamAcceptOpcode(WorldPacket& recv_data);
        void HandleArenaTeamDeclineOpcode(WorldPacket& recv_data);
        void HandleArenaTeamLeaveOpcode(WorldPacket& recv_data);
        void HandleArenaTeamRemoveOpcode(WorldPacket& recv_data);
        void HandleArenaTeamDisbandOpcode(WorldPacket& recv_data);
        void HandleArenaTeamLeaderOpcode(WorldPacket& recv_data);

        void HandleAreaSpiritHealerQueryOpcode(WorldPacket& recv_data);
        void HandleAreaSpiritHealerQueueOpcode(WorldPacket& recv_data);
        void HandleCancelMountAuraOpcode(WorldPacket& recv_data);
        void HandleSelfResOpcode(WorldPacket& recv_data);
        void HandleComplainOpcode(WorldPacket& recv_data);
        void HandleRequestPetInfoOpcode(WorldPacket& recv_data);

        // Socket gem
        void HandleSocketOpcode(WorldPacket& recv_data);

        void HandleCancelTempEnchantmentOpcode(WorldPacket& recv_data);

        void HandleChannelVoiceOnOpcode(WorldPacket& recv_data);
        void HandleVoiceSessionEnableOpcode(WorldPacket& recv_data);
        void HandleSetActiveVoiceChannel(WorldPacket& recv_data);
        void HandleSetTaxiBenchmarkOpcode(WorldPacket& recv_data);

        void HandleCommentatorModeOpcode(WorldPacket& recv_data);

        // Guild Bank
        void HandleGuildPermissions(WorldPacket& recv_data);
        void HandleGuildBankMoneyWithdrawn(WorldPacket& recv_data);
        void HandleGuildBankerActivate(WorldPacket& recv_data);
        void HandleGuildBankQueryTab(WorldPacket& recv_data);
        void HandleGuildBankLogQuery(WorldPacket& recv_data);
        void HandleGuildBankDepositMoney(WorldPacket& recv_data);
        void HandleGuildBankWithdrawMoney(WorldPacket& recv_data);
        void HandleGuildBankSwapItems(WorldPacket& recv_data);
        void HandleGuildBankUpdateTab(WorldPacket& recv_data);
        void HandleGuildBankBuyTab(WorldPacket& recv_data);
        void HandleQueryGuildBankTabText(WorldPacket& recv_data);
        void HandleSetGuildBankTabText(WorldPacket& recv_data);

        void HandleGetMirrorimageData(WorldPacket& recv_data);

        // Raf
        void HandleGrantLevel(WorldPacket& recv_data);
        void HandleReferAFriend(WorldPacket& recv_data);
        void HandleAcceptLevelGrant(WorldPacket& recv_data);

        // Warden
        void HandleWardenDataOpcode(WorldPacket& recv_data);

        // Movement
        void SynchronizeMovement(MovementInfo &movementInfo);

        std::deque<uint32> GetOutOpcodeHistory();
        std::deque<uint32> GetIncOpcodeHistory();

#ifdef ENABLE_PLAYERBOTS
        void HandleBotPackets();
#endif

        Messager<WorldSession>& GetMessager() { return m_messager; }

        void SetPacketLogging(bool state);

        LfgPlayerInfo m_lfgInfo;

    private:
        // Additional private opcode handlers
        void HandleComplainMail(WorldPacket& recv_data);
        void HandleComplainChat(WorldPacket& recv_data);

        // private trade methods
        void moveItems(Item* myItems[], Item* hisItems[]);
        bool VerifyMovementInfo(MovementInfo const& movementInfo, Unit* mover, bool unroot) const;
        void HandleMoverRelocation(MovementInfo& movementInfo);

        void ExecuteOpcode(OpcodeHandler const& opHandle, WorldPacket& packet);

        // logging helper
        void LogUnexpectedOpcode(WorldPacket const& packet, const char* reason) const;
        void LogUnprocessedTail(WorldPacket const& packet) const;

        void ProcessByteBufferException(WorldPacket const& packet);

        uint32 m_GUIDLow;                                   // set logged or recently logout player (while m_playerRecentlyLogout set) | 当前登录或刚登出玩家的低位 GUID
        Player* _player;                                    ///< 当前在线玩家角色实体指针
        std::shared_ptr<WorldSocket> m_socket;              // socket pointer is owned by the network thread which created it | 关联的网络 Socket（由网络线程创建并共享所有权）
        std::shared_ptr<WorldSocket> m_requestSocket;       // a new socket for this session is requested (double connection) | 重连请求接管的新 Socket 指针
        std::string m_localAddress;
        WorldSessionState m_sessionState;                   // this session state | 当前会话状态机阶段

        AccountTypes _security;                             ///< 账号安全/GM 权限等级
        uint32 _accountId;                                  ///< 账号数据库 ID
        uint8 m_expansion;                                  ///< 账号资料片权限等级（0=经典旧世，1=TBC）
        std::string m_accountName;                          ///< 账号登录名
        uint32 m_accountFlags;                              ///< 账号特殊标志掩码

        // anticheat | 反作弊与客户端环境信息
        ClientOSType m_clientOS;
        ClientPlatformType m_clientPlatform;
        uint32 m_gameBuild;
        uint32 m_accountMaxLevel;
        uint32 m_orderCounter;
        uint32 m_lastAnticheatUpdate;
        std::unique_ptr<SessionAnticheatInterface> m_anticheat;

        time_t _logoutTime;                                 // when logout will be processed after a logout request | 发起登出请求的时间戳
        time_t m_kickTime;                                  ///< 选人界面超时踢出时间戳
        time_t m_afkTime;                                   ///< 进入 AFK 挂机状态的时间戳
        bool m_playerSave;                                  // should we have to save the player after logout request | 登出时是否保存角色数据到数据库
        bool m_inQueue;                                     // session wait in auth.queue | 是否处于世界登录排队中
        bool m_playerLoading;                               // code processed in LoginPlayer | 是否正在异步加载角色登录数据
        bool m_kickSession;                                 ///< 登出后是否强制关闭底层网络连接

        // True when the player is in the process of logging out (WorldSession::LogoutPlayer is currently executing)
        // 是否正在执行 LogoutPlayer() 清理流程中
        bool m_playerLogout;
        bool m_playerRecentlyLogout;                        ///< 角色是否刚完成登出（用于宽限迟到的客户端数据包）
        LocaleConstant m_sessionDbcLocale;
        int m_sessionDbLocaleIndex;
        std::atomic<uint32> m_latency;                      ///< 最近一次客户端上报的网络延迟（ms）
        AccountData m_accountData[NUM_ACCOUNT_DATA_TYPES];
        uint32 m_Tutorials[8];
        TutorialDataState m_tutorialState;

        boost::circular_buffer<std::pair<int64, uint32>> m_timeSyncClockDeltaQueue; // first member: clockDelta. Second member: latency of the packet exchange that was used to compute that clockDelta. | 时钟同步采样环形缓冲：<客户端时钟偏移, 往返延迟>
        int64 m_timeSyncClockDelta;                         ///< 计算得到的客户端与服务端时钟差值
        std::atomic<uint32> m_timeSyncRoundTrip;           // median round trip of the clock synchronisation | 时钟同步中位数往返耗时
        void ComputeNewClockDelta();

        std::map<uint32, uint32> m_pendingTimeSyncRequests; // key: counter. value: server time when packet with that counter was sent. | 待回应的时间同步请求映射：<序号, 发送时刻>
        uint32 m_timeSyncNextCounter;
        uint32 m_timeSyncTimer;

        std::set<ObjectGuid> m_offlineNameQueries; // for name queries made when not logged in (character selection screen) | 选人界面发出的异步名称查询 GUID 集合
        std::deque<CharacterNameQueryResponse> m_offlineNameResponses; // for responses to name queries made when not logged in | 离线阶段待发送的名称查询响应队列

        bool m_initialZoneUpdated = false;

        // Recruit-A-Friend | 战友招募（RAF）绑定信息
        uint32 m_recruitingFriendId;
        bool m_isRecruiter;

        // Thread safety mechanisms | 跨线程数据包队列及会话消息投递器
        std::mutex m_recvQueueLock;                         ///< 保护世界主线程收包队列的互斥锁
        std::mutex m_recvQueueMapLock;                      ///< 保护地图线程收包队列的互斥锁
        std::deque<std::unique_ptr<WorldPacket>> m_recvQueue;
        std::deque<std::unique_ptr<WorldPacket>> m_recvQueueMap;

        Messager<WorldSession> m_messager;                  ///< 跨线程异步回调投递队列

        std::atomic<uint32> m_currentPlayerLevel;           ///< 原子缓存当前玩家等级（供多线程安全读取）
};
#endif
/// @}
