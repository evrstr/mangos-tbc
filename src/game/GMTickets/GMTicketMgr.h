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

#ifndef _GMTICKETMGR_H
#define _GMTICKETMGR_H

#include "Database/DatabaseEnv.h"
#include "Entities/ObjectGuid.h"
#include "Entities/Player.h"

#include <string>
#include <map>
#include <list>
#include <time.h>

// GM 工单系统启用状态
enum GMTicketSystemStatus : uint8
{
    GMTICKET_QUEUE_STATUS_DISABLED  = 0,                    // 工单队列已禁用
    GMTICKET_QUEUE_STATUS_ENABLED   = 1                     // 工单队列已启用
};

// GM 工单查询返回状态
enum GMTicketStatus
{
    GMTICKET_STATUS_DBERROR = 0x00,                         // 数据库错误
    GMTICKET_STATUS_HASTEXT = 0x06,                         // 当前存在有效工单文本
    GMTICKET_STATUS_DEFAULT = 0x0A                          // 无工单/默认状态
};

// 客户端工单增删改操作响应码
enum GMTicketResponse
{
    GMTICKET_RESPONSE_NOT_EXIST         = 0,                // 工单不存在
    GMTICKET_RESPONSE_ALREADY_EXIST     = 1,                // 已有未关闭工单
    GMTICKET_RESPONSE_CREATE_SUCCESS    = 2,                // 创建工单成功
    GMTICKET_RESPONSE_CREATE_ERROR      = 3,                // 创建工单失败
    GMTICKET_RESPONSE_UPDATE_SUCCESS    = 4,                // 更新工单成功
    GMTICKET_RESPONSE_UPDATE_ERROR      = 5,                // 更新工单失败
    GMTICKET_RESPONSE_TICKET_DELETED    = 9                 // 工单已删除/放弃
};

// 工单状态推送通知类型（SMSG_GM_TICKET_STATUS_UPDATE）
enum GMTicketStateStatus
{
    GMTICKET_STATUS_UPDATED = 1,                            // 工单被 GM 更新/回复
    GMTICKET_STATUS_CLOSED  = 2,                            // 工单已关闭
    GMTICKET_STATUS_SURVEY  = 3                             // 弹出满意度调查问卷
};

// from UI Lua: (源自客户端 Lua UI 定义：工单分配与升级状态)
enum GMTicketEscalationStatus : uint8
{
    GMTICKET_ASSIGNEDTOGM_STATUS_NOT_ASSIGNED   = 0,        // ticket is not currently assigned to a gm (尚未分配给 GM)
    GMTICKET_ASSIGNEDTOGM_STATUS_ASSIGNED       = 1,        // ticket is assigned to a normal gm (已分配给普通 GM)
    GMTICKET_ASSIGNEDTOGM_STATUS_ESCALATED      = 2         // ticket is in the escalation queue (已升级至高级 GM 队列)
};

// from UI Lua: (源自客户端 Lua UI 定义：工单是否已被 GM 查阅)
enum GMTicketOpenedByGMStatus : uint8
{
    GMTICKET_OPENEDBYGM_STATUS_NOT_OPENED   = 0,            // ticket has never been opened by a gm (尚未被 GM 打开过)
    GMTICKET_OPENEDBYGM_STATUS_OPENED       = 1             // ticket has been opened by a gm (已被 GM 打开过)
};

// 工单生命周期状态
enum GMTicketState : uint8
{
    GMTICKET_STATE_OPEN       = 0,                          // 开启中（待处理）
    GMTICKET_STATE_CLOSED     = 1,                          // 已关闭
    GMTICKET_STATE_ABANDONED  = 2                           // 玩家已放弃
};

// GM 工单聊天提示事件类型
enum GMTicketMgrChatAlert : uint8
{
    GMTICKETMGR_CHAT_ONLINE     = 0,                        // 提交者上线
    GMTICKETMGR_CHAT_OFFLINE    = 1,                        // 提交者下线
    GMTICKETMGR_CHAT_UPDATED    = 2,                        // 提交者更新工单内容
    GMTICKETMGR_CHAT_ABANDONED  = 3,                        // 提交者放弃工单
    GMTICKETMGR_CHAT_ESCALATED  = 4,                        // 工单被升级
};

/// 单条 GM 工单实体：保存文本、分类、坐标、分配 GM、处理备注及会话统计信息
class GMTicket
{
    public:
        explicit GMTicket(Player* player, std::string message, uint8 category, time_t when = time(nullptr));
        explicit GMTicket(Field* fields) { Load(fields); }

        /// 工单排序比较器（按升级等级与更新时间先后）
        static bool Compare(GMTicket* a, GMTicket* b);

        /// 从数据库查询行加载工单字段
        void Load(Field* fields);
        /// 将工单当前状态持久化到数据库
        inline void Save(SqlStatement& stmt) const;
        /// 复用玩家之前放弃的工单记录以提交新工单（避免数据库垃圾行膨胀）
        void Recycle(Player* player, std::string message, uint8 category, time_t when = time(nullptr));

        inline uint32                   GetId()             const { return m_id; }
        inline static std::string       GetIdTag(uint32 id)       { return ("T#" + std::to_string(id)); }
        inline std::string              GetIdTag()          const { return GetIdTag(m_id); }
        inline uint8                    GetCategory()       const { return m_category; }
        inline GMTicketState            GetState()          const { return GMTicketState(m_state); }
        inline GMTicketEscalationStatus GetStatus()         const { return m_status; }
        inline uint8                    GetLevel()          const { return m_level; }
        inline ObjectGuid const&        GetAuthorGuid()     const { return m_authorGuid; }
        inline const char*              GetAuthorName()     const { return m_authorName.c_str(); }
        inline const char*              GetLocale()         const { return m_locale.c_str(); }
        inline LocaleConstant           GetDbcLocale()      const;
        inline int                      GetDbLocaleIndex()  const;
        inline uint32                   GetMapId()          const { return m_map; }
        inline Position                 GetPosition()       const { return Position(m_x, m_y, m_z, m_o); }
        inline float                    GetPositionX()      const { return m_x; }
        inline float                    GetPositionY()      const { return m_y; }
        inline float                    GetPositionZ()      const { return m_z; }
        inline float                    GetPositionO()      const { return m_o; }
        inline const char*              GetText()           const { return m_text.c_str(); }
        inline time_t                   GetCreatedAt()      const { return m_created; }
        inline time_t                   GetUpdatedAt()      const { return m_updated; }
        inline time_t                   GetSeenAt()         const { return m_seen; }
        inline time_t                   GetAnsweredAt()     const { return m_answered; }
        inline time_t                   GetClosedAt()       const { return m_closed; }
        inline ObjectGuid const&        GetAssigneeGuid()   const { return m_assigneeGuid; }
        inline const char*              GetAssigneeName()   const { return m_assigneeName.c_str(); }
        inline void                     ClearAssignee()
        {
            m_assigneeGuid.Clear();
            m_assigneeName.clear();
            if (m_status == GMTICKET_ASSIGNEDTOGM_STATUS_ASSIGNED)
                m_status = GMTICKET_ASSIGNEDTOGM_STATUS_NOT_ASSIGNED;
        }
        inline std::string const&       GetConclusion()     const { return m_conclusion; }
        inline std::string const&       GetNotes()          const { return m_notes; }

        inline bool IsAssignedTo(ObjectGuid guid) const { return (guid == m_assigneeGuid); }
        inline bool IsAssignedTo(const char* name) const { return (strcmp(m_assigneeName.c_str(), name) == 0); }

        inline bool IsAssigned()    const { return (m_status == GMTICKET_ASSIGNEDTOGM_STATUS_ASSIGNED); }
        inline bool IsEscalated()   const { return (m_status == GMTICKET_ASSIGNEDTOGM_STATUS_ESCALATED); }
        inline bool IsOpen()        const { return (m_state == GMTICKET_STATE_OPEN); }
        inline bool IsSeen()        const { return (m_seen > m_updated); }
        inline bool IsSurveyed()    const { return (m_state == GMTICKET_STATE_CLOSED && (m_statsEscalations || (m_statsChatIn >= 3 || m_statsChatOut >= 3))); }

        void AddNote(const std::string& line) { m_notes += ("\n" + line); }

        void SetState(GMTicketState state, time_t when = time(nullptr));
        void SetCategory(uint8 id) { m_category = id; }

        void SetAnswered(time_t when) { m_answered = when; }
        void SetSeen(time_t when) { m_statsLastSeen = m_seen; m_seen = when; }
        void SetUpdated(const std::string &text, time_t when = time(nullptr));

        void SetAssignee(uint8 level, WorldSession* session = nullptr);
        void SetConclusion(const std::string& response) { m_conclusion = response; }

        void UpdateStatsChat(bool incoming);

        inline time_t                   GetLastSeenAt()     const { return m_statsLastSeen; }

    private:
        uint32                      m_id;           // Unique id of the ticket
        uint8                       m_category;     // Category id of the ticket
        GMTicketState               m_state;        // State: open, closed or abandoned
        GMTicketEscalationStatus    m_status;       // Current status: not assigned, assigned, escalated
        uint8                       m_level;        // Support level (Security level of the last account processing the ticket)
        ObjectGuid                  m_authorGuid;   // Character's guid
        std::string                 m_authorName;   // Character's name
        std::string                 m_locale;       // Character's client locale a the time of creation
        uint32                      m_map;          // Character's position at the time of creation (map id)
        float                       m_x;            // Character's position at the time of creation (x axis)
        float                       m_y;            // Character's position at the time of creation (y axis)
        float                       m_z;            // Character's position at the time of creation (z axis)
        float                       m_o;            // Character's position at the time of creation (orientation)
        std::string                 m_text;         // Ticket's text
        time_t                      m_created;      // When ticket was initially created by the author
        time_t                      m_updated;      // When ticket was last updated by the author
        time_t                      m_seen;         // When ticket was last seen by any GM
        time_t                      m_answered;     // When ticket was last answered by an assignee
        time_t                      m_closed;       // When ticket was closed by the last assignee
        ObjectGuid                  m_assigneeGuid; // Guid of the last GM character processing the ticket
        std::string                 m_assigneeName; // Name of the last GM character processing the ticket
        std::string                 m_conclusion;   // Final closing conclusion
        std::string                 m_notes;        // Additional GM notes

    private:
        uint32 m_statsChatIn;                       // Chat stats: times player participated in a conversation
        uint32 m_statsChatOut;                      // Chat stats: times gm participated in a conversation
        uint32 m_statsEscalations;                  // Escalation stats: times ticket was escalated
        time_t m_statsLastSeen;                     // Previous time ticket was senn by a GM
};

typedef std::map<uint32, GMTicket*> GMTicketMap;
typedef std::list<GMTicket*> GMTicketList;

/// 玩家提交的 GM 满意度调查结果
class GMSurveyResult
{
    public:
        explicit GMSurveyResult(const GMSurveyEntry &entry, uint32 ticketId, const std::map<uint32, uint8> &answers, const std::string &feedback);

        inline bool HasComment() const { return !m_comment.empty(); }

        inline size_t GetAnswersCount() const { return m_answers.size(); }
        inline uint32 GetTicketId() const { return m_ticketId; }
        inline uint32 GetSurveyId() const { return m_surveyId; }
        inline std::vector<uint8> const& GetAnswers() const { return m_answers; }
        inline std::string const& GetComment() const { return m_comment; }

    private:
        uint32      m_ticketId;                     // Unique id of the ticket surveyed (关联的工单 ID)
        uint32      m_surveyId;                     // Survey DBC entry id (问卷 DBC 条目 ID)
        std::vector<uint8> m_answers;               // Answers in order matching questions in survey entry (按问卷题目顺序排列的评分答案)
        std::string m_comment;                      // Player's feedback (玩家附加文字评价)
};

/// 全局 GM 工单管理器：负责工单加载、队列排序、GM 密语/处理、邮件结案及问卷统计
class GMTicketMgr
{
    public:
        // 工单管理命令执行结果枚举
        enum CommandResult : uint8
        {
            COMMAND_RESULT_SUCCESS             = 0,         // 成功
            COMMAND_RESULT_TICKET_NOT_FOUND    = 1,         // 未找到工单
            COMMAND_RESULT_PLAYER_OFFLINE      = 2,         // 玩家不在线
            COMMAND_RESULT_ACCESS_DENIED       = 3,         // 权限不足或已被他人认领
            COMMAND_RESULT_TICKET_NOT_ASSIGNED = 4,         // 工单尚未分配
            COMMAND_RESULT_TICKET_UNSEEN       = 5,         // 工单尚未被 GM 查阅
        };

    public:
        GMTicketMgr();
        ~GMTicketMgr() = default;

        /// 启动时从数据库加载未完成及待问卷工单
        void LoadGMTickets();

        static void Save(const GMTicket* ticket);
        static void Save(const GMSurveyResult* survey);

        /// 向指定会话打印工单详细信息
        static void Print(GMTicket const& ticket, WorldSession* session, time_t now = time(nullptr));
        static inline const std::string PrintMailResponse(GMTicket const& ticket, bool resolved);
        static inline const char* PrintTicketCategory(GMTicket const& ticket, LocaleConstant locale = DEFAULT_LOCALE);
        static inline const char* PrintTicketChatAlert(GMTicketMgrChatAlert alert, LocaleConstant locale = DEFAULT_LOCALE);
        static inline const char* PrintTicketStatus(GMTicket const& ticket, LocaleConstant locale = DEFAULT_LOCALE);
        static inline const char* PrintTicketStatusColorSequence(GMTicket const& ticket);
        static inline const std::string PrintTicketSummaryLine(GMTicket const& ticket, LocaleConstant locale = DEFAULT_LOCALE);

        /// 打印符合过滤条件的工单摘要列表
        void PrintTicketList(WorldSession* session, std::ostringstream& output, size_t max, const GMTicketCategoryEntry* category = nullptr, bool online = false) const;

        /// 玩家向正在处理其工单的 GM 发送/接收工单通道聊天消息及系统状态提醒
        static bool TicketChatIncoming(GMTicket* ticket, Player* player, WorldSession* gm, const std::string& message, bool addon = false);
        static bool TicketChatOutgoing(GMTicket* ticket, WorldSession* gm, Player* player, const std::string& message, bool addon = false);
        static bool TicketChatAlert(const GMTicket* ticket, GMTicketMgrChatAlert alert);

        /// 玩家上下线时通知关联 GM 并刷新工单状态
        void OnPlayerOnlineState(Player &player, bool online) const;
        /// GM 登录时展示当前工单队列概览公告
        void ShowMOTD(Player &gm) const;

        /// 拦截玩家与 GM 间的密语及 GM 查询，将其绑定至对应工单对话
        std::pair<bool, bool> HookGMTicketWhisper(Player* sender, const std::string& recepient, Player* character, const std::string &msg, bool addon);
        bool HookGMTicketWhoQuery(const std::string& query, Player* gm) const;

        GMTicket* GetTicketById(uint32 id, GMTicketState state = GMTICKET_STATE_OPEN) const;
        GMTicket* GetTicketByPlayer(ObjectGuid playerGuid, GMTicketState state = GMTICKET_STATE_OPEN, ObjectGuid assigneeGuid = ObjectGuid()) const;

        bool Add(GMTicket* ticket, bool loading = false);
        bool Recycle(GMTicket* ticket);

        /// 处理玩家提交的工单满意度调查问卷
        void Survey(GMTicket* ticket, const GMSurveyResult* result = nullptr);

        // The tickets written by this character are removed, the ones it handled as a GM go back to unassigned
        // 删除角色时移除其提交的工单，并将其作为 GM 认领的工单重置为未分配状态
        void RemovePlayerReferences(ObjectGuid playerGuid);

        CommandResult Abandon(GMTicket* ticket);
        inline CommandResult Discard(GMTicket* ticket, const std::string& reason, WorldSession* session = nullptr) { return Close(ticket, reason, false, session); }
        CommandResult Escalate(GMTicket* ticket, WorldSession* session, uint8 level);
        CommandResult Comment(GMTicket* ticket, WorldSession* session, const std::string& text);
        CommandResult Read(GMTicket* ticket);
        inline CommandResult Resolve(GMTicket* ticket, const std::string& commentary, WorldSession* session = nullptr) { return Close(ticket, commentary, true, session); }
        CommandResult Sort(GMTicket* ticket, const GMTicketCategoryEntry& category, WorldSession* session = nullptr);
        CommandResult Update(GMTicket* ticket, const std::string& text);
        CommandResult Whisper(GMTicket* ticket, const std::string& message, WorldSession* session = nullptr);

        static inline float GetDaysPassed(time_t now, time_t since) { return (now && since ? std::stof(std::to_string((difftime(now, since) / float(DAY)))) : 0); }

        static inline float GetTicketAgeDays(time_t now, time_t created) { return GetDaysPassed(now, created); }
        inline float GetOldestTicketAgeDays() const { return m_statsOldestTicketAgeDays; }
        float GetOldestTicketAgeLastUpdateDays(time_t now) const;

        inline bool GetSystemStatus() const { return m_status; }
        bool SetSystemStatus(bool status);

        inline uint32 GenerateTicketId() { return ++m_lastTicketId; }

    private:
        static inline CommandResult CanComment(const GMTicket* ticket, WorldSession* who);
        static inline CommandResult CanClose(const GMTicket* ticket, bool resolve, WorldSession* who);
        static inline CommandResult CanSort(const GMTicket* ticket, WorldSession* who);
        static inline CommandResult CanWhisper(const GMTicket* ticket, WorldSession* who);

        std::pair<GMTicket*, std::string> GetTicketByIdTag(const std::string& tag, GMTicketState state = GMTICKET_STATE_OPEN, ObjectGuid assigneeGuid = ObjectGuid()) const;

        bool Remove(GMTicket& ticket);
        void RemoveAll();

        CommandResult Close(GMTicket* ticket, const std::string& commentary, bool resolved, WorldSession* session = nullptr);

        void UpdateTicketQueueStats(GMTicket& closed, time_t when);
        inline void UpdateTicketQueueTimers(GMTicket* except = nullptr) const;

        inline float GetAverageResolutionDays() const { return m_statsAverageResolutionDays; }
        inline size_t GetTicketCount() const { return m_tickets.size(); }

    private:
        GMTicketSystemStatus m_status;

        GMTicketMap m_tickets;
        GMTicketList m_list;

        uint32 m_lastTicketId                   = 0;
        uint32 m_currentTicketCountOpen         = 0;
        uint32 m_currentTicketCountClosed       = 0;

    private:
        float m_statsAverageResolutionDays      = 0;
        float m_statsCurrentTotalResolutionDays = 0;

        float m_statsOldestTicketAgeDays        = -1;
        time_t m_statsOldestTicketAgeLastUpdate = 0;
};

#define sTicketMgr MaNGOS::Singleton<GMTicketMgr>::Instance()
#endif
