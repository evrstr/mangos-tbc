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

#ifndef MANGOSSERVER_LOG_H
#define MANGOSSERVER_LOG_H

#include "Common.h"
#include "Policies/Singleton.h"

#include <mutex>

class Config;
class ByteBuffer;

/// 日志级别枚举
enum LogLevel
{
    LOG_LVL_MINIMAL = 0,                                    // unconditional and errors | 仅无条件输出与错误日志
    LOG_LVL_BASIC   = 1,
    LOG_LVL_DETAIL  = 2,
    LOG_LVL_DEBUG   = 3
};

// bitmask (not forgot update logFilterData content) | 日志过滤掩码位（修改后须同步更新 Log.cpp 中的 logFilterData 数组）
enum LogFilters
{
    LOG_FILTER_TRANSPORT_MOVES    = 0x000001,               //  0 any related to transport moves | 0: 载具/船只移动相关日志
    LOG_FILTER_CREATURE_MOVES     = 0x000002,               //  1 creature move by cells | 1: 生物跨网格移动日志
    LOG_FILTER_VISIBILITY_CHANGES = 0x000004,               //  2 update visibility for diff objects and players | 2: 对象与玩家可见性变更日志
    // reserved for future version | 保留供后续版本使用
    LOG_FILTER_WEATHER            = 0x000010,               //  4 weather changes | 4: 天气变化日志
    LOG_FILTER_PLAYER_STATS       = 0x000020,               //  5 player save data | 5: 玩家属性与存盘日志
    LOG_FILTER_SQL_TEXT           = 0x000040,               //  6 raw SQL text send to DB engine | 6: 发送给数据库引擎的原始 SQL 文本
    LOG_FILTER_PLAYER_MOVES       = 0x000080,               //  7 player moves by grid/cell | 7: 玩家跨网格移动日志
    LOG_FILTER_PERIODIC_AFFECTS   = 0x000100,               //  8 DoT/HoT apply trace | 8: 周期性伤害/治疗效果追踪
    LOG_FILTER_AI_AND_MOVEGENSS   = 0x000200,               //  9 AI/movement generators debug output | 9: AI 与移动生成器调试日志
    LOG_FILTER_DAMAGE             = 0x000400,               // 10 Direct/Area damage trace | 10: 直接/范围伤害计算追踪
    LOG_FILTER_COMBAT             = 0x000800,               // 11 attack states/roll attack results/etc | 11: 攻击状态与战斗命中判定日志
    LOG_FILTER_SPELL_CAST         = 0x001000,               // 12 spell cast/aura apply/spell proc events | 12: 法术施放、光环应用与触发事件
    LOG_FILTER_DB_STRICTED_CHECK  = 0x002000,               // 13 stricted DB data checks output (with possible false reports) for DB devs | 13: 严格数据库校验输出（面向数据库开发，可能含误报）
    // reserved for future version | 保留供后续版本使用
    // reserved for future version | 保留供后续版本使用
    LOG_FILTER_PATHFINDING        = 0x010000,               // 16 Pathfinding | 16: 寻路系统日志
    LOG_FILTER_MAP_LOADING        = 0x020000,               // 17 Map loading/unloading (MAP, VMAPS, MMAP) | 17: 地图/VMAP/MMAP 加载与卸载日志
    LOG_FILTER_EVENT_AI_DEV       = 0x040000,               // 18 Event AI actions | 18: EventAI 事件动作调试日志
    // reserved for future version | 保留供后续版本使用
    LOG_FILTER_DB_SCRIPT          = 0x100000,               // 20 Db scripts | 20: 数据库脚本（DBScript）日志
};

#define LOG_FILTER_COUNT            21

/// 日志过滤器配置描述条目
struct LogFilterData
{
    char const* name;
    char const* configName;
    bool defaultState;
};

extern LogFilterData logFilterData[LOG_FILTER_COUNT];

/// 控制台输出颜色枚举
enum Color
{
    BLACK,
    RED,
    GREEN,
    BROWN,
    BLUE,
    MAGENTA,
    CYAN,
    GREY,
    YELLOW,
    LRED,
    LGREEN,
    LBLUE,
    LMAGENTA,
    LCYAN,
    WHITE
};

const int Color_count = int(WHITE) + 1;

/// 全局日志管理单例类（线程安全，支持控制台彩色输出及多类日志文件分流）
class Log : public MaNGOS::Singleton<Log, MaNGOS::ClassLevelLockable<Log, std::mutex> >
{
        friend class MaNGOS::OperatorNew<Log>;
        Log();

        ~Log()
        {
            if (logfile != nullptr)
                fclose(logfile);
            logfile = nullptr;

            if (gmLogfile != nullptr)
                fclose(gmLogfile);
            gmLogfile = nullptr;

            if (charLogfile != nullptr)
                fclose(charLogfile);
            charLogfile = nullptr;

            if (dberLogfile != nullptr)
                fclose(dberLogfile);
            dberLogfile = nullptr;

            if (eventAiErLogfile != nullptr)
                fclose(eventAiErLogfile);
            eventAiErLogfile = nullptr;

            if (scriptErrLogFile != nullptr)
                fclose(scriptErrLogFile);
            scriptErrLogFile = nullptr;

            if (raLogfile != nullptr)
                fclose(raLogfile);
            raLogfile = nullptr;

            if (worldLogfile != nullptr)
                fclose(worldLogfile);
            worldLogfile = nullptr;

            if (customLogFile != nullptr)
                fclose(customLogFile);
            customLogFile = nullptr;
        }
    public:
        /// 从配置加载日志目录、各级日志文件句柄、颜色与过滤掩码
        void Initialize();
        /// 解析 LogColors 配置字符串并设置 4 种日志类型对应的控制台颜色
        void InitColors(const std::string& str);

        /// 输出 GM 命令审计日志（可按账号分文件存储）
        void outCommand(uint32 account, const char* str, ...) ATTR_PRINTF(3, 4);
        /// 输出空行（任意日志级别）
        void outString();                                   // any log level | 任意日志级别
        // any log level | 任意日志级别
        void outString(const char* str, ...)      ATTR_PRINTF(2, 3);
        // any log level | 任意日志级别错误输出
        void outError(const char* err, ...)       ATTR_PRINTF(2, 3);
        // log level >= 1 | 基础级别日志（级别 >= 1）
        void outBasic(const char* str, ...)       ATTR_PRINTF(2, 3);
        // log level >= 2 | 详细级别日志（级别 >= 2）
        void outDetail(const char* str, ...)      ATTR_PRINTF(2, 3);
        // log level >= 3 | 调试级别日志（级别 >= 3）
        void outDebug(const char* str, ...)       ATTR_PRINTF(2, 3);

        void outErrorDb();                                  // any log level | 任意日志级别（数据库错误空行）
        // any log level | 任意日志级别（数据库错误日志）
        void outErrorDb(const char* err, ...)     ATTR_PRINTF(2, 3);
        // any log level | 任意日志级别（角色行为日志）
        void outChar(const char* str, ...)        ATTR_PRINTF(2, 3);

        void outErrorEventAI();                             // any log level | 任意日志级别（EventAI 错误空行）
        // any log level | 任意日志级别（EventAI 错误日志）
        void outErrorEventAI(const char* err, ...)      ATTR_PRINTF(2, 3);

        void outErrorScriptLib();                           // any log level | 任意日志级别（脚本库错误空行）
        // any log level | 任意日志级别（脚本库错误日志）
        void outErrorScriptLib(const char* err, ...)     ATTR_PRINTF(2, 3);

        /// 将收发的世界数据包以十六进制转储到 WorldLogFile
        void outWorldPacketDump(const char* socket, uint32 opcode, char const* opcodeName, ByteBuffer const& packet, bool incoming);
        // any log level | 任意日志级别（角色数据转储到 CharLogFile）
        void outCharDump(const char* str, uint32 account_id, uint32 guid, const char* name);
        /// 输出远程控制台（RA）操作日志
        void outRALog(const char* str, ...)       ATTR_PRINTF(2, 3);
        /// 输出自定义日志文件（CustomLogFile）
        void outCustomLog(const char* str, ...)       ATTR_PRINTF(2, 3);
        uint32 GetLogLevel() const { return m_logLevel; }
        void SetLogLevel(char* level);
        void SetLogFileLevel(char* level);
        void SetColor(bool stdout_stream, Color color);
        void ResetColor(bool stdout_stream);
        void outTime() const;
        static void outTimestamp(FILE* file);
        static std::string GetTimestampStr();
        bool HasLogFilter(uint32 filter) const { return (m_logFilter & filter) != 0; }
        void SetLogFilter(LogFilters filter, bool on) { if (on) m_logFilter |= filter; else m_logFilter &= ~filter; }
        bool HasLogLevelOrHigher(LogLevel loglvl) const { return m_logLevel >= loglvl || (m_logFileLevel >= loglvl && logfile); }
        bool IsOutCharDump() const { return m_charLog_Dump; }
        bool IsIncludeTime() const { return m_includeTime; }
        /// 捕获当前线程调用栈并格式化为字符串
        std::string GetTraceLog();

        /// 启动出错时根据 WaitAtStartupError 配置暂停等待用户确认或倒计时
        static void WaitBeforeContinueIfNeed();

        // Set filename for scriptlibrary error output | 设置脚本库错误输出文件路径及脚本库标识名
        void setScriptLibraryErrorFile(char const* fname, char const* libName);

        /// 将当前调用栈信息写入 CustomLogFile
        void traceLog();

    private:
        FILE* openLogFile(char const* configFileName, char const* configTimeStampFlag, char const* mode);
        FILE* openGmlogPerAccount(uint32 account);

        FILE* raLogfile;
        FILE* logfile;
        FILE* gmLogfile;
        FILE* charLogfile;
        FILE* dberLogfile;
        FILE* eventAiErLogfile;
        FILE* scriptErrLogFile;
        FILE* worldLogfile;
        FILE* customLogFile;

        std::mutex m_worldLogMtx;
        std::mutex m_traceLogMtx;

        // log/console control | 控制台与文件日志级别、颜色及过滤器状态控制
        LogLevel m_logLevel;
        LogLevel m_logFileLevel;
        bool m_colored;
        bool m_includeTime;
        Color m_colors[4];
        uint32 m_logFilter;

        // cache values for after initilization use (like gm log per account case) | 缓存目录与时间戳后缀，供初始化后动态打开文件（如按账号切分 GM 日志）使用
        std::string m_logsDir;
        std::string m_logsTimestamp;

        // char log control | 角色转储日志开关
        bool m_charLog_Dump;

        // gm log control | GM 日志按账号分文件控制及文件名格式模板
        bool m_gmlog_per_account;
        std::string m_gmlog_filename_format;

        char const* m_scriptLibName;
};

#define sLog MaNGOS::Singleton<Log>::Instance()

#define BASIC_LOG(...)                                  \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_BASIC))    \
            sLog.outBasic(__VA_ARGS__);                 \
    } while(0)

#define BASIC_FILTER_LOG(F,...)                         \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_BASIC) && !sLog.HasLogFilter(F)) \
            sLog.outBasic(__VA_ARGS__);                 \
    } while(0)

#define DETAIL_LOG(...)                                 \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_DETAIL))   \
            sLog.outDetail(__VA_ARGS__);                \
    } while(0)

#define DETAIL_FILTER_LOG(F,...)                        \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_DETAIL) && !sLog.HasLogFilter(F)) \
            sLog.outDetail(__VA_ARGS__);                \
    } while(0)

#define DEBUG_LOG(...)                                  \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))    \
            sLog.outDebug(__VA_ARGS__);                 \
    } while(0)

#define DEBUG_FILTER_LOG(F,...)                         \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG) && !sLog.HasLogFilter(F)) \
            sLog.outDebug(__VA_ARGS__);                 \
    } while(0)

#define ERROR_DB_FILTER_LOG(F,...)                      \
    do {                                                \
        if (!sLog.HasLogFilter(F))                      \
            sLog.outErrorDb(__VA_ARGS__);               \
    } while(0)

#define ERROR_DB_STRICT_LOG(...) \
    ERROR_DB_FILTER_LOG(LOG_FILTER_DB_STRICTED_CHECK, __VA_ARGS__)

// primary for script library | 主要供外部脚本库调用的全局日志便捷函数
void outstring_log();
void outstring_log(const char* str, ...) ATTR_PRINTF(1, 2);
void detail_log(const char* str, ...) ATTR_PRINTF(1, 2);
void debug_log(const char* str, ...) ATTR_PRINTF(1, 2);
void error_log(const char* str, ...) ATTR_PRINTF(1, 2);
void error_db_log(const char* str, ...) ATTR_PRINTF(1, 2);
void setScriptLibraryErrorFile(char const* fname, char const* libName);
void script_error_log(const char* str, ...) ATTR_PRINTF(1, 2);

#endif
