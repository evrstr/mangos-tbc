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

#ifndef MANGOS_GAMEEVENT_MGR_H
#define MANGOS_GAMEEVENT_MGR_H

#include "Common.h"
#include "Globals/SharedDefines.h"
#include "Platform/Define.h"

#define max_ge_check_delay 86400                            // 1 day in seconds (单次事件检查最大延迟：1 天/86400 秒)
#define FAR_FUTURE 4102444800                               // 2100, January 1st (远期占位时间戳：2100 年 1 月 1 日)

class Creature;
class GameObject;
class MapPersistentState;

/// 游戏事件调度类型（决定活动起止时间的计算方式）
enum GameEventScheduleType
{
    GAME_EVENT_SCHEDULE_SERVERSIDE  = 0, // completely handled by the server - start and end (完全由服务端程序或 GM 命令控制启停)
    GAME_EVENT_SCHEDULE_DATE        = 1, // old start and end date behaviour (按固定起止时间与周期重复)
    GAME_EVENT_SCHEDULE_DMF_1       = 2, // first Darkmoon Faire event - first whole week of the month (暗月马戏团第 1 阶段：每月首个完整周)
    GAME_EVENT_SCHEDULE_DMF_2       = 3, // second Darkmoon Faire event (暗月马戏团第 2 阶段)
    GAME_EVENT_SCHEDULE_DMF_3       = 4, // third Darkmoon Faire event (暗月马戏团第 3 阶段)
    GAME_EVENT_SCHEDULE_DMF_BUILDING_STAGE_1_1 = 5, // first Darkmoon Faire building event (暗月马戏团搭建阶段 1)
    GAME_EVENT_SCHEDULE_DMF_BUILDING_STAGE_1_2 = 6,
    GAME_EVENT_SCHEDULE_DMF_BUILDING_STAGE_1_3 = 7,
    GAME_EVENT_SCHEDULE_DMF_BUILDING_STAGE_2_1 = 8,
    GAME_EVENT_SCHEDULE_DMF_BUILDING_STAGE_2_2 = 9,
    GAME_EVENT_SCHEDULE_DMF_BUILDING_STAGE_2_3 = 10,
    GAME_EVENT_SCHEDULE_YEARLY      = 11,                   // 每年固定日期重复事件
    GAME_EVENT_SCHEDULE_LUNAR_NEW_YEAR  = 12,               // 农历新年（春节庆典）动态计算
    GAME_EVENT_SCHEDULE_EASTER          = 13,               // 复活节（贵族花园）动态计算
};

/// 单个游戏事件配置与当前周期起止时间
struct GameEventData
{
    GameEventData() : start(1), end(0), occurence(0), length(0), holiday_id(HOLIDAY_NONE), linkedTo(0), eventGroup(0)
    {
    }

    GameEventScheduleType scheduleType;                     // 调度类型
    time_t start;                                           // 当前周期开始时间戳
    time_t end;                                             // 当前周期结束时间戳
    uint32 occurence;                                       // Delay in minutes between occurences of the event (两次触发间隔分钟数)
    uint32 length;                                          // Length in minutes of the event (事件持续分钟数)
    HolidayIds holiday_id;                                  // 关联节日 ID
    uint32 linkedTo;                                        // 联动前置事件 ID（仅当关联事件激活时才允许开启）
    uint32 eventGroup;                                      // 互斥事件组 ID（同组内同时只激活随机一个事件）
    std::string description;                                // 事件描述名称

    bool isValid() const { return scheduleType == GAME_EVENT_SCHEDULE_SERVERSIDE || length > 0; }
};

/// 事件激活期间对生物模型、装备、商人列表及光环法术的临时替换配置
struct GameEventCreatureData
{
    uint32 entry_id;
    uint32 modelid;
    uint32 equipment_id;
    uint32 vendor_id;
    uint32 spell_id_start;
    uint32 spell_id_end;
};

/// 事件触发时向符合条件玩家发送的节日邮件配置
struct GameEventMail
{
    GameEventMail() : raceMask(0), questId(0), mailTemplateId(0), senderEntry(0) {}
    GameEventMail(uint32 _raceMask, uint32 _quest, uint32 _mailTemplateId, uint32 _senderEntry)
        : raceMask(_raceMask), questId(_quest), mailTemplateId(_mailTemplateId), senderEntry(_senderEntry) {}

    uint32 raceMask;                                        // 接收种族掩码
    uint32 questId;                                         // quest must be rewarded if set (若非 0，要求玩家已完成该任务)
    uint32 mailTemplateId;                                  // 邮件模板 ID
    uint32 senderEntry;                                     // 发件人 NPC 模板 ID
};

typedef std::pair<uint32, GameEventCreatureData> GameEventCreatureDataPair;

/// 游戏事件（节日/世界事件）管理器：管理事件周期调度、关联刷怪/刷物、任务开关、生物数据覆盖及节日邮件
class GameEventMgr
{
    public:
        GameEventMgr();
        ~GameEventMgr() {};
        typedef std::set<uint16> ActiveEvents;
        typedef std::vector<GameEventData> GameEventDataMap;
        ActiveEvents const& GetActiveEventList() const { return m_activeEvents; }
        GameEventDataMap const& GetEventMap() const { return m_gameEvents; }
        /// 判断指定事件在给定时刻是否处于激活窗口内
        bool CheckOneGameEvent(uint16 entry, time_t currenttime) const;
        /// 计算距指定事件下次状态变化（开始或结束）的秒数
        uint32 NextCheck(uint16 entry) const;
        /// 从数据库加载全部游戏事件及其关联的生物、对象、刷怪池、任务与邮件数据
        void LoadFromDB();
        void Initialize(MapPersistentState* state);         // called at new MapPersistentState object create (新地图持久状态创建时同步事件刷怪)
        /// 周期更新：开启到期事件、停止过期事件，并返回距离下一次检查的毫秒延迟
        uint32 Update(ActiveEvents const* activeAtShutdown = nullptr);
        bool IsValidEvent(uint16 event_id) const { return event_id < m_gameEvents.size() && m_gameEvents[event_id].isValid(); }
        bool IsActiveEvent(uint16 event_id) const { return (m_activeEvents.find(event_id) != m_activeEvents.end()); }
        bool IsActiveHoliday(HolidayIds id) const; // TODO: Make thread safe (检查指定节日当前是否激活)
        uint32 Initialize();
        /// 手动或自动启动指定事件
        void StartEvent(uint16 event_id, bool overwrite = false, bool resume = false);
        /// 手动或自动停止指定事件
        void StopEvent(uint16 event_id, bool overwrite = false);
        template<typename T>
        int16 GetGameEventId(uint32 guid_or_poolid);

        std::unordered_map<uint32, std::vector<uint32>> const& GetEventGroups() const { return m_gameEventGroups; }

        /// 查询生物在当前激活事件下是否有模型/装备/商人覆盖配置
        GameEventCreatureData const* GetCreatureUpdateDataForActiveEvent(uint32 dbGuid) const;

        /// 重新计算周常/周期性事件计时器并通知地图
        void WeeklyEventTimerRecalculation();
    private:
        void ApplyNewEvent(uint16 event_id, bool resume);
        void UnApplyEvent(uint16 event_id);
        void GameEventSpawn(int16 event_id);
        void GameEventUnspawn(int16 event_id);
        void UpdateCreatureData(int16 event_id, bool activate);
        void UpdateEventQuests(uint16 event_id, bool Activate);
        void SendEventMails(int16 event_id);
        void OnEventHappened(uint16 event_id, bool activate, bool resume);
        void ComputeEventStartAndEndTime(GameEventData& data, time_t today);
    protected:
        typedef std::list<uint32> GuidList;
        typedef std::list<uint16> IdList;
        typedef std::vector<GuidList> GameEventGuidMap;
        typedef std::vector<IdList> GameEventIdMap;
        typedef std::list<GameEventCreatureDataPair> GameEventCreatureDataList;
        typedef std::vector<GameEventCreatureDataList> GameEventCreatureDataMap;
        typedef std::multimap<uint32, uint32> GameEventCreatureDataPerGuidMap;
        typedef std::pair<GameEventCreatureDataPerGuidMap::const_iterator, GameEventCreatureDataPerGuidMap::const_iterator> GameEventCreatureDataPerGuidBounds;

        typedef std::list<uint32> QuestList;
        typedef std::vector<QuestList> GameEventQuestMap;

        GameEventQuestMap m_gameEventQuests;                 // events size, only positive event case

        GameEventCreatureDataMap m_gameEventCreatureData;    // events size, only positive event case
        GameEventCreatureDataPerGuidMap m_gameEventCreatureDataPerGuid;

        typedef std::list<GameEventMail> MailList;
        typedef std::vector<MailList> GameEventMailMap;
        GameEventMailMap  m_gameEventMails;                  // events*2-1

        GameEventGuidMap  m_gameEventCreatureGuids;          // events*2-1
        GameEventGuidMap  m_gameEventGameobjectGuids;       // events*2-1
        GameEventIdMap    m_gameEventSpawnPoolIds;          // events size, only positive event case
        GameEventDataMap  m_gameEvents;
        ActiveEvents m_activeEvents;
        bool m_isGameEventsInit;

        std::unordered_map<uint32, std::vector<uint32>> m_gameEventGroups; // events size
};

#define sGameEventMgr MaNGOS::Singleton<GameEventMgr>::Instance()

bool IsHolidayActive(HolidayIds id);

#endif
