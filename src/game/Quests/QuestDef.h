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

#ifndef MANGOSSERVER_QUEST_H
#define MANGOSSERVER_QUEST_H

#include "Platform/Define.h"
#include "Database/DatabaseEnv.h"
#include "Util/UniqueTrackablePtr.h"

#include <vector>

class Player;

class ObjectMgr;

#define MAX_QUEST_LOG_SIZE 25

#define QUEST_OBJECTIVES_COUNT 4
#define QUEST_ITEM_OBJECTIVES_COUNT QUEST_OBJECTIVES_COUNT
#define QUEST_SOURCE_ITEM_IDS_COUNT 4
#define QUEST_REWARD_CHOICES_COUNT 6
#define QUEST_REWARDS_COUNT 4
#define QUEST_DEPLINK_COUNT 10
#define QUEST_REPUTATIONS_COUNT 5
#define QUEST_EMOTE_COUNT 4

// 任务不可接取或失败的原因枚举（对应客户端提示文本）
enum QuestFailedReasons
{
    INVALIDREASON_DONT_HAVE_REQ                       = 0,  // this is default case (默认：不满足前置条件)
    INVALIDREASON_QUEST_FAILED_LOW_LEVEL              = 1,  // You are not high enough level for that quest. (等级不足)
    INVALIDREASON_QUEST_FAILED_INVENTORY_FULL         = 4,  // Inventory is full (背包已满)
    INVALIDREASON_QUEST_FAILED_WRONG_RACE             = 6,  // That quest is not available to your race. (种族不符合)
    INVALIDREASON_QUEST_ALREADY_DONE                  = 7,  // You have completed that quest. (已完成该任务)
    INVALIDREASON_QUEST_ONLY_ONE_TIMED                = 12, // You can only be on one timed quest at a time. (同时只能接取一个限时任务)
    INVALIDREASON_QUEST_ALREADY_ON                    = 13, // You are already on that quest. (已接取该任务)
    INVALIDREASON_QUEST_FAILED_EXPANSION              = 16, // This quest requires an expansion enabled account. (需要启用对应资料片权限)
    INVALIDREASON_QUEST_FAILED_DUPLICATE_ITEM         = 17, // Duplicate item found. (发现重复物品)
    INVALIDREASON_QUEST_ALREADY_ON2                   = 18, // You are already on that quest. (已接取该任务)
    INVALIDREASON_QUEST_FAILED_MISSING_ITEMS          = 21, // You don't have the required items with you. Check storage. (缺少所需任务物品)
    INVALIDREASON_QUEST_FAILED_NOT_ENOUGH_MONEY       = 23, // You don't have enough money for that quest. (金币不足)
    INVALIDREASON_DAILY_QUESTS_REMAINING              = 26, // You have already completed 10 daily quests today. (今日已完成 10 个日常任务)
    INVALIDREASON_QUEST_FAILED_CAIS                   = 27  // You cannot complete quests once you have reached tired time. (防沉迷疲劳状态下无法完成任务)
};

// 队伍分享任务结果反馈消息枚举
enum QuestShareMessages
{
    QUEST_PARTY_MSG_SHARING_QUEST           = 0,            // ERR_QUEST_PUSH_SUCCESS_S (正在分享任务)
    QUEST_PARTY_MSG_CANT_TAKE_QUEST         = 1,            // ERR_QUEST_PUSH_INVALID_S (对方无法接取该任务)
    QUEST_PARTY_MSG_ACCEPT_QUEST            = 2,            // ERR_QUEST_PUSH_ACCEPTED_S (对方接受了任务)
    QUEST_PARTY_MSG_DECLINE_QUEST           = 3,            // ERR_QUEST_PUSH_DECLINED_S (对方拒绝了任务)
    QUEST_PARTY_MSG_TOO_FAR                 = 4,            // removed in 3.x (距离过远)
    QUEST_PARTY_MSG_BUSY                    = 5,            // ERR_QUEST_PUSH_BUSY_S (对方正忙)
    QUEST_PARTY_MSG_LOG_FULL                = 6,            // ERR_QUEST_PUSH_LOG_FULL_S (对方任务日志已满)
    QUEST_PARTY_MSG_HAVE_QUEST              = 7,            // ERR_QUEST_PUSH_ONQUEST_S (对方已有该任务)
    QUEST_PARTY_MSG_FINISH_QUEST            = 8,            // ERR_QUEST_PUSH_ALREADY_DONE_S (对方已完成该任务)
};

// 任务关联的商业/专业技能分类
enum __QuestTradeSkill
{
    QUEST_TRSKILL_NONE           = 0,
    QUEST_TRSKILL_ALCHEMY        = 1,
    QUEST_TRSKILL_BLACKSMITHING  = 2,
    QUEST_TRSKILL_COOKING        = 3,
    QUEST_TRSKILL_ENCHANTING     = 4,
    QUEST_TRSKILL_ENGINEERING    = 5,
    QUEST_TRSKILL_FIRSTAID       = 6,
    QUEST_TRSKILL_HERBALISM      = 7,
    QUEST_TRSKILL_LEATHERWORKING = 8,
    QUEST_TRSKILL_POISONS        = 9,
    QUEST_TRSKILL_TAILORING      = 10,
    QUEST_TRSKILL_MINING         = 11,
    QUEST_TRSKILL_FISHING        = 12,
    QUEST_TRSKILL_SKINNING       = 13,
    QUEST_TRSKILL_JEWELCRAFTING  = 14,
};

// 玩家当前任务进度状态枚举
enum QuestStatus
{
    QUEST_STATUS_NONE           = 0,                        // 未接取
    QUEST_STATUS_COMPLETE       = 1,                        // 目标已达成待交付
    QUEST_STATUS_UNAVAILABLE    = 2,                        // 不可用
    QUEST_STATUS_INCOMPLETE     = 3,                        // 进行中
    QUEST_STATUS_AVAILABLE      = 4,                        // unused in fact (实际未使用)
    QUEST_STATUS_FAILED         = 5,                        // 已失败
    MAX_QUEST_STATUS
};

// NPC/交互对象头顶及小地图的任务对话标记状态
enum __QuestGiverStatus
{
    DIALOG_STATUS_NONE                     = 0,
    DIALOG_STATUS_UNAVAILABLE              = 1,             // Grey Exclamation Mark (灰色感叹号：等级不足等暂不可接)
    DIALOG_STATUS_CHAT                     = 2,             // No marker (无任务标记，仅普通对话)
    DIALOG_STATUS_INCOMPLETE               = 3,             // Grey Question Mark - quest taken (灰色问号：任务进行中)
    DIALOG_STATUS_REWARD_REP               = 4,             // Blue Question Mark - non-daily repeatable available (蓝色问号：非日常可重复任务可交付)
    DIALOG_STATUS_AVAILABLE_REP            = 5,             // Blue Exclamation Mark - daily available (蓝色感叹号：日常/周常可接)
    DIALOG_STATUS_AVAILABLE                = 6,             // Yellow Exclamation Mark - quest available (黄色感叹号：普通任务可接)
    DIALOG_STATUS_REWARD2                  = 7,             // no yellow dot on minimap (可交付但小地图不显示黄点)
    DIALOG_STATUS_REWARD                   = 8,             // yellow dot on minimap (黄色问号：可交付且小地图显示黄点)
    DIALOG_STATUS_UNDEFINED                = 100            // Used as result for unassigned ScriptCall (脚本调用未覆盖时返回的占位值)
};

// values based at QuestInfo.dbc (任务类型，对应 QuestInfo.dbc)
enum QuestTypes
{
    QUEST_TYPE_ELITE               = 1,                     // 精英
    QUEST_TYPE_LIFE                = 21,                    // 生活/职业
    QUEST_TYPE_PVP                 = 41,                    // PvP
    QUEST_TYPE_RAID                = 62,                    // 团队副本
    QUEST_TYPE_DUNGEON             = 81,                    // 地下城
    QUEST_TYPE_WORLD_EVENT         = 82,                    // 世界事件
    QUEST_TYPE_LEGENDARY           = 83,                    // 传说
    QUEST_TYPE_ESCORT              = 84,                    // 护送
    QUEST_TYPE_HEROIC              = 85,                    // 英雄地下城
};

// 任务标志位（服务端使用并随任务详情下发给客户端）
enum QuestFlags
{
    // Flags used at server and sent to client
    QUEST_FLAGS_NONE           = 0x00000000,
    QUEST_FLAGS_STAY_ALIVE     = 0x00000001,                // Quest is failed on dying (角色死亡即判定任务失败)
    QUEST_FLAGS_PARTY_ACCEPT   = 0x00000002,                // If player in party, all players that can accept this quest will receive confirmation box to accept quest CMSG_QUEST_CONFIRM_ACCEPT/SMSG_QUEST_CONFIRM_ACCEPT (队员接取时向全队弹出确认接取框)
    QUEST_FLAGS_EXPLORATION    = 0x00000004,                // Not used currently (当前未使用)
    QUEST_FLAGS_SHARABLE       = 0x00000008,                // Can be shared: Player::CanShareQuest() (可共享给队友)
    // QUEST_FLAGS_NONE2        = 0x00000010,               // Not used currently
    QUEST_FLAGS_EPIC           = 0x00000020,                // Not used currently: Unsure of content
    QUEST_FLAGS_RAID           = 0x00000040,                // Not used currently
    QUEST_FLAGS_TBC            = 0x00000080,                // Not used currently: Available if TBC expansion enabled only
    QUEST_FLAGS_NO_MONEY_FROM_XP = 0x00000100,              // Not used currently: _DELIVER_MORE Quest needs more than normal _q-item_ drops from mobs (满级后不再将经验折算为额外金币奖励)
    QUEST_FLAGS_HIDDEN_REWARDS = 0x00000200,                // Items and money rewarded only sent in SMSG_QUESTGIVER_OFFER_REWARD (not in SMSG_QUESTGIVER_QUEST_DETAILS or in client quest log(SMSG_QUEST_QUERY_RESPONSE)) (隐藏奖励物品与金币，仅在交任务界面展示)
    QUEST_FLAGS_AUTO_REWARDED  = 0x00000400,                // These quests are automatically rewarded on quest complete and they will never appear in quest log client side. (完成时自动发奖，不进入客户端任务日志)
    QUEST_FLAGS_TBC_RACES      = 0x00000800,                // Not used currently: Blood elf/Draenei starting zone quests
    QUEST_FLAGS_DAILY          = 0x00001000,                // Daily quest. Can be done once a day. Quests reset at regular intervals for all players. (日常任务，每天可完成一次)
    QUEST_FLAGS_FLAGS_PVP      = 0x00002000,                // activates PvP on accept (接取后开启 PvP 标记)
    QUEST_FLAGS_UNAVAILABLE    = 0x00004000,                // quests not available (任务不可用)
    QUEST_FLAGS_WEEKLY         = 0x00008000,                // Weekly quest. Can be done once a week. Quests reset at regular intervals for all players. (周常任务，每周可完成一次)
};

// 服务端内部特殊任务标志位（部分来自数据库 SpecialFlags，部分在加载时计算）
enum QuestSpecialFlags
{
    // Mangos flags for set SpecialFlags in DB if required but used only at server
    QUEST_SPECIAL_FLAG_REPEATABLE           = 0x001,        // |1 in SpecialFlags from DB (可重复完成)
    QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT = 0x002,        // |2 in SpecialFlags from DB (if required area explore, spell SPELL_EFFECT_QUEST_COMPLETE casting, table `*_script` command SCRIPT_COMMAND_QUEST_EXPLORED use, set from script DLL) (区域探索或脚本/法术触发完成)
    QUEST_SPECIAL_FLAG_MONTHLY              = 0x004,        // |4 in SpecialFlags. Quest reset for player at beginning of month. (月常任务，每月初重置)

    // Mangos flags for internal use only (仅服务端内部计算使用的标志)
    QUEST_SPECIAL_FLAG_DELIVER              = 0x008,        // Internal flag computed only (需要收集物品)
    QUEST_SPECIAL_FLAG_SPEAKTO              = 0x010,        // Internal flag computed only (需要与目标对话)
    QUEST_SPECIAL_FLAG_KILL_OR_CAST         = 0x020,        // Internal flag computed only (需要击杀生物、交互对象或施放法术)
    QUEST_SPECIAL_FLAG_TIMED                = 0x040,        // Internal flag computed only (限时任务)
};

#define QUEST_SPECIAL_FLAG_DB_ALLOWED (QUEST_SPECIAL_FLAG_REPEATABLE | QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT | QUEST_SPECIAL_FLAG_MONTHLY)

// 多语言本地化任务文本结构
struct QuestLocale
{
    QuestLocale() { ObjectiveText.resize(QUEST_OBJECTIVES_COUNT); }

    std::vector<std::string> Title;
    std::vector<std::string> Details;
    std::vector<std::string> Objectives;
    std::vector<std::string> OfferRewardText;
    std::vector<std::string> RequestItemsText;
    std::vector<std::string> EndText;
    std::vector< std::vector<std::string> > ObjectiveText;
};

// This Quest class provides a convenient way to access a few pretotaled (cached) quest details,
// all base quest information, and any utility functions such as generating the amount of
// xp to give
// 任务模板定义类：封装数据库任务基础配置、预统计目标数量及经验/金币奖励计算逻辑
class Quest
{
        friend class ObjectMgr;
    public:
        Quest(Field* questRecord);
        uint32 XPValue(Player* pPlayer) const;              // 计算任务对指定玩家提供的经验值（随等级差衰减）

        uint32 GetQuestFlags() const { return m_QuestFlags; }
        bool HasQuestFlag(QuestFlags flag) const { return (m_QuestFlags & flag) != 0; }
        bool HasSpecialFlag(QuestSpecialFlags flag) const { return (m_SpecialFlags & flag) != 0; }
        void SetSpecialFlag(QuestSpecialFlags flag) { m_SpecialFlags |= flag; }

        // table data accessors: (数据库任务模板字段只读访问器)
        uint32 GetQuestId() const { return QuestId; }
        uint32 GetQuestMethod() const { return QuestMethod; }
        int32  GetZoneOrSort() const { return ZoneOrSort; }
        uint32 GetMinLevel() const { return MinLevel; }
        uint32 GetMaxLevel() const { return MaxLevel; }
        int32  GetQuestLevel() const { return QuestLevel; }
        uint32 GetType() const { return Type; }
        uint32 GetRequiredClasses() const { return RequiredClasses; }
        uint32 GetRequiredRaces() const { return RequiredRaces; }
        uint32 GetRequiredSkill() const { return RequiredSkill; }
        uint32 GetRequiredSkillValue() const { return RequiredSkillValue; }
        uint32 GetRequiredCondition() const { return RequiredCondition; }
        uint32 GetRepObjectiveFaction() const { return RepObjectiveFaction; }
        int32  GetRepObjectiveValue() const { return RepObjectiveValue; }
        uint32 GetRequiredMinRepFaction() const { return RequiredMinRepFaction; }
        int32  GetRequiredMinRepValue() const { return RequiredMinRepValue; }
        uint32 GetRequiredMaxRepFaction() const { return RequiredMaxRepFaction; }
        int32  GetRequiredMaxRepValue() const { return RequiredMaxRepValue; }
        uint32 GetSuggestedPlayers() const { return SuggestedPlayers; }
        uint32 GetLimitTime() const { return LimitTime; }
        int32  GetPrevQuestId() const { return PrevQuestId; }
        int32  GetNextQuestId() const { return NextQuestId; }
        int32  GetExclusiveGroup() const { return ExclusiveGroup; }
        uint32 GetBreadcrumbForQuestId() const { return BreadcrumbForQuestId; }
        uint32 GetNextQuestInChain() const { return NextQuestInChain; }
        uint32 GetCharTitleId() const { return CharTitleId; }
        uint32 GetCharTitleBitIndex() const;                // in 2.x in different from 3.x in some quest packets used bit index insed id (2.x 部分数据包使用头衔位索引而非头衔 ID)
        uint32 GetSrcItemId() const { return SrcItemId; }
        uint32 GetSrcItemCount() const { return SrcItemCount; }
        uint32 GetSrcSpell() const { return SrcSpell; }
        std::string GetTitle() const { return Title; }
        std::string GetDetails() const { return Details; }
        std::string GetObjectives() const { return Objectives; }
        std::string GetOfferRewardText() const { return OfferRewardText; }
        std::string GetRequestItemsText() const { return RequestItemsText; }
        std::string GetEndText() const { return EndText; }
        int32  GetRewOrReqMoney() const;
        uint32 GetRewHonorableKills() const { return RewHonorableKills; }
        uint32 GetRewMoneyMaxLevel() const;
        // use in XP calculation at client (客户端亦据此计算任务经验显示)
        uint32 GetRewSpell() const { return RewSpell; }
        uint32 GetRewSpellCast() const { return RewSpellCast; }
        uint32 GetRewMailTemplateId() const { return RewMailTemplateId; }
        uint32 GetRewMailDelaySecs() const { return RewMailDelaySecs; }
        uint32 GetPointMapId() const { return PointMapId; }
        float  GetPointX() const { return PointX; }
        float  GetPointY() const { return PointY; }
        uint32 GetPointOpt() const { return PointOpt; }
        uint32 GetIncompleteEmote() const { return IncompleteEmote; }
        uint32 GetIncompleteEmoteDelay() const { return IncompleteEmoteDelay; }
        uint32 GetCompleteEmote() const { return CompleteEmote; }
        uint32 GetCompleteEmoteDelay() const { return CompleteEmoteDelay; }
        uint32 GetDetailsEmoteCount() const { return m_detailsemotecount; }
        uint32 GetQuestStartScript() const { return QuestStartScript; }
        uint32 GetQuestCompleteScript() const { return QuestCompleteScript; }
        uint32 GetRewFactionFlags() const { return RewFactionFlags; }

        bool   IsRepeatable() const { return (m_SpecialFlags & QUEST_SPECIAL_FLAG_REPEATABLE) != 0; }
        bool   IsAutoComplete() const { return !QuestMethod; }
        bool   IsDaily() const { return (m_QuestFlags & QUEST_FLAGS_DAILY) != 0; }
        bool   IsWeekly() const { return (m_QuestFlags & QUEST_FLAGS_WEEKLY) != 0; }
        bool   IsMonthly() const { return (m_SpecialFlags & QUEST_SPECIAL_FLAG_MONTHLY) != 0; }
        bool   IsDailyOrWeekly() const { return (m_QuestFlags & (QUEST_FLAGS_DAILY | QUEST_FLAGS_WEEKLY)) != 0; }
        bool   IsAllowedInRaid() const;

        // quest can be fully deactivated and will not be available for any player (可动态停用任务使其对所有玩家不可接取)
        void SetQuestActiveState(bool state) { m_isActive = state; }
        bool IsActive() const { return m_isActive; }

        // multiple values (任务多槽位目标、源物品、奖励与表情数据)
        std::string ObjectiveText[QUEST_OBJECTIVES_COUNT];
        uint32 ReqItemId[QUEST_ITEM_OBJECTIVES_COUNT];
        uint32 ReqItemCount[QUEST_ITEM_OBJECTIVES_COUNT];
        uint32 ReqSourceId[QUEST_SOURCE_ITEM_IDS_COUNT];
        uint32 ReqSourceCount[QUEST_SOURCE_ITEM_IDS_COUNT];
        int32  ReqCreatureOrGOId[QUEST_OBJECTIVES_COUNT];   // >0 Creature <0 Gameobject (正值为生物模板 ID，负值为游戏对象模板 ID 的相反数)
        uint32 ReqCreatureOrGOCount[QUEST_OBJECTIVES_COUNT];
        uint32 ReqSpell[QUEST_OBJECTIVES_COUNT];
        uint32 RewChoiceItemId[QUEST_REWARD_CHOICES_COUNT];
        uint32 RewChoiceItemCount[QUEST_REWARD_CHOICES_COUNT];
        uint32 RewItemId[QUEST_REWARDS_COUNT];
        uint32 RewItemCount[QUEST_REWARDS_COUNT];
        uint32 RewRepFaction[QUEST_REPUTATIONS_COUNT];
        int32  RewRepValue[QUEST_REPUTATIONS_COUNT];
        int32  RewMaxRepValue[QUEST_REPUTATIONS_COUNT];
        uint32 DetailsEmote[QUEST_EMOTE_COUNT];
        uint32 DetailsEmoteDelay[QUEST_EMOTE_COUNT];
        uint32 OfferRewardEmote[QUEST_EMOTE_COUNT];
        uint32 OfferRewardEmoteDelay[QUEST_EMOTE_COUNT];

        uint32 GetReqItemsCount() const { return m_reqitemscount; }
        uint32 GetReqCreatureOrGOcount() const { return m_reqCreatureOrGOcount; }
        uint32 GetRewChoiceItemsCount() const { return m_rewchoiceitemscount; }
        uint32 GetRewItemsCount() const { return m_rewitemscount; }

        MaNGOS::unique_weak_ptr<Quest> GetWeakPtr() const { return m_weakRef; }

        typedef std::vector<int32> PrevQuests;
        PrevQuests prevQuests;                              // 前置任务列表（正数必须完成，负数表示互斥/前置链）
        typedef std::vector<uint32> PrevChainQuests;
        PrevChainQuests prevChainQuests;                    // 同链前置任务列表
        std::vector<uint32> DependentBreadcrumbQuests;      // 依赖此任务的引导（面包屑）任务列表

        // cached data (构造时预统计的缓存计数)
    private:
        uint32 m_reqitemscount;
        uint32 m_reqCreatureOrGOcount;
        uint32 m_rewchoiceitemscount;
        uint32 m_rewitemscount;
        uint32 m_detailsemotecount;                         // actual allowed value 0..4 (详情动作表情有效个数，范围 0~4)

        bool m_isActive;

        // table data (数据库表原始字段)
    protected:
        uint32 QuestId;
        uint32 QuestMethod;
        int32  ZoneOrSort;
        uint32 MinLevel;
        uint32 MaxLevel;
        int32  QuestLevel;
        uint32 Type;
        uint32 RequiredClasses;
        uint32 RequiredRaces;
        uint32 RequiredSkill;
        uint32 RequiredSkillValue;
        uint32 RequiredCondition;
        uint32 RepObjectiveFaction;
        int32  RepObjectiveValue;
        uint32 RequiredMinRepFaction;
        int32  RequiredMinRepValue;
        uint32 RequiredMaxRepFaction;
        int32  RequiredMaxRepValue;
        uint32 SuggestedPlayers;
        uint32 LimitTime;
        uint32 m_QuestFlags;
        uint32 m_SpecialFlags;
        uint32 CharTitleId;
        int32  PrevQuestId;
        int32  NextQuestId;
        int32  ExclusiveGroup;
        uint32 BreadcrumbForQuestId;
        uint32 NextQuestInChain;
        uint32 SrcItemId;
        uint32 SrcItemCount;
        uint32 SrcSpell;
        std::string Title;
        std::string Details;
        std::string Objectives;
        std::string OfferRewardText;
        std::string RequestItemsText;
        std::string EndText;
        uint32 RewHonorableKills;
        int32  RewOrReqMoney;
        uint32 RewMoneyMaxLevel;
        uint32 RewSpell;
        uint32 RewSpellCast;
        uint32 RewMailTemplateId;
        uint32 RewMailDelaySecs;
        uint32 PointMapId;
        float  PointX;
        float  PointY;
        uint32 PointOpt;
        uint32 IncompleteEmote;
        uint32 IncompleteEmoteDelay;
        uint32 CompleteEmote;
        uint32 CompleteEmoteDelay;
        uint32 QuestStartScript;
        uint32 QuestCompleteScript;
        uint32 RewFactionFlags;

        MaNGOS::unique_weak_ptr<Quest> m_weakRef;
};

// 玩家任务进度数据的数据库脏标记状态
enum QuestUpdateState
{
    QUEST_UNCHANGED = 0,
    QUEST_CHANGED = 1,
    QUEST_NEW = 2
};

// 玩家单个任务的运行时进度与持久化状态数据
struct QuestStatusData
{
    QuestStatusData()
        : m_status(QUEST_STATUS_NONE), m_rewarded(false),
          m_explored(false), m_timer(0), uState(QUEST_NEW)
    {
        memset(m_itemcount, 0, QUEST_ITEM_OBJECTIVES_COUNT * sizeof(uint32));
        memset(m_creatureOrGOcount, 0, QUEST_OBJECTIVES_COUNT * sizeof(uint32));
    }

    QuestStatus m_status;                                   // 当前任务状态
    bool m_rewarded;                                        // 是否已领取奖励
    bool m_explored;                                        // 区域探索/事件目标是否已完成
    uint32 m_timer;                                         // 限时任务剩余时间（毫秒或秒级计时）
    QuestUpdateState uState;                                // 数据库同步状态

    uint32 m_itemcount[ QUEST_ITEM_OBJECTIVES_COUNT ];      // 各收集物品目标当前数量
    uint32 m_creatureOrGOcount[ QUEST_OBJECTIVES_COUNT ];   // 各击杀/交互目标当前完成数量
};
#endif
