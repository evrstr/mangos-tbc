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

#ifndef MANGOSSERVER_COMMON_H
#define MANGOSSERVER_COMMON_H

#include "Platform/Define.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cmath>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#if defined(__sun__)
#include <ieeefp.h> // finite() on Solaris | Solaris 平台的 finite() 声明
#endif

#include <set>
#include <vector>
#include <list>
#include <string>
#include <map>
#include <queue>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

#include "Platform/Filesystem.h"

#include "Util/Errors.h"
#include "Multithreading/Threading.h"
#include "Log/Log.h"

// included to use sleep_for() | 引入 sleep_for() 线程休眠支持
#include <thread>

/// 系统时钟类型别名
typedef std::chrono::system_clock Clock;
/// 毫秒级系统时间点类型别名
typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> TimePoint;

#if COMPILER == COMPILER_MICROSOFT

#  include <float.h>

#  define I32FMT "%08I32X"
#  define I64FMT "%016I64X"

#  pragma warning ( disable : 4251 )
#else

#  define stricmp strcasecmp
#  define strnicmp strncasecmp

#  define I32FMT "%08X"
#  if ACE_SIZEOF_LONG == 8
#    define I64FMT "%016lX"
#  else
#    define I64FMT "%016llX"
#  endif /* ACE_SIZEOF_LONG == 8 */

#endif

#define __STDC_FORMAT_MACROS
#include <cinttypes>

#define UI64FMTD "%" PRIu64

#define SI64FMTD "%" PRId64

#if COMPILER == COMPILER_MICROSOFT
#  define SIZEFMTD "%Iu"
#else
#  define SIZEFMTD "%zu"
#endif

/// 保证浮点数为有限值，NaN 或 Inf 时回退为 0.0f
inline float finiteAlways(float f) { return std::isfinite(f) ? f : 0.0f; }

#define STRINGIZE(a) #a

// used for creating values for respawn for example | 用于拼接与拆分 64/32 位组合整型值（例如重生标识、GUID 片段等）
#define MAKE_PAIR64(l, h)  uint64( uint32(l) | ( uint64(h) << 32 ) )
#define PAIR64_HIPART(x)   (uint32)((uint64(x) >> 32) & uint64(0x00000000FFFFFFFF))
#define PAIR64_LOPART(x)   (uint32)(uint64(x)         & uint64(0x00000000FFFFFFFF))

#define MAKE_PAIR32(l, h)  uint32( uint16(l) | ( uint32(h) << 16 ) )
#define PAIR32_HIPART(x)   uint16( ((uint32(x) >> 16) & 0x0000FFFF) )
#define PAIR32_LOPART(x)   uint16( (uint32(x)         & 0x0000FFFF) )

#define MAX_NETCLIENT_PACKET_SIZE (32767 - 1)               // Client hardcap: int16 with trailing zero space otherwise crash on memory free | 客户端网络包硬上限：有符号 16 位长度且需预留结尾空字节，否则释放内存时崩溃

/// 常用时间单位换算常量（秒/毫秒）
enum TimeConstants
{
    MINUTE = 60,
    HOUR   = MINUTE * 60,
    DAY    = HOUR * 24,
    WEEK   = DAY * 7,
    MONTH  = DAY * 30,
    YEAR   = MONTH * 12,
    IN_MILLISECONDS = 1000
};

/// 账号权限安全等级枚举
enum AccountTypes : uint32
{
    SEC_PLAYER         = 0,
    SEC_MODERATOR      = 1,
    SEC_GAMEMASTER     = 2,
    SEC_ADMINISTRATOR  = 3,
    SEC_CONSOLE        = 4                                  // must be always last in list, accounts must have less security level always also | 控制台最高权限，必须始终位于末尾，普通账号权限须严格低于此值
};

// Used in mangosd/realmd | 服务器状态标志位（供 mangosd 与 realmd 共用）
enum RealmFlags
{
    REALM_FLAG_NONE         = 0x00,
    REALM_FLAG_INVALID      = 0x01,
    REALM_FLAG_OFFLINE      = 0x02,
    REALM_FLAG_SPECIFYBUILD = 0x04,                         // client will show realm version in RealmList screen in form "RealmName (major.minor.revision.build)" | 客户端在服务器列表显示版本号格式 "RealmName (主.次.修订.构建)"
    REALM_FLAG_UNK1         = 0x08,
    REALM_FLAG_UNK2         = 0x10,
    REALM_FLAG_NEW_PLAYERS  = 0x20,
    REALM_FLAG_RECOMMENDED  = 0x40,
    REALM_FLAG_FULL         = 0x80
};

// Used in mangosd/realmd | 服务器时区/语言区枚举（供 mangosd 与 realmd 共用）
enum RealmZone : uint8
{
    REALM_ZONE_UNKNOWN       = 0,                           // any language | 任意语言
    REALM_ZONE_DEVELOPMENT   = 1,                           // any language | 开发区（任意语言）
    REALM_ZONE_UNITED_STATES = 2,                           // extended-Latin | 扩展拉丁语（美区）
    REALM_ZONE_OCEANIC       = 3,                           // extended-Latin | 扩展拉丁语（大洋洲）
    REALM_ZONE_LATIN_AMERICA = 4,                           // extended-Latin | 扩展拉丁语（拉丁美洲）
    REALM_ZONE_TOURNAMENT_5  = 5,                           // basic-Latin at create, any at login | 锦标赛区（创建限基础拉丁字符，登录不限）
    REALM_ZONE_KOREA         = 6,                           // East-Asian | 东亚语区（韩国）
    REALM_ZONE_TOURNAMENT_7  = 7,                           // basic-Latin at create, any at login | 锦标赛区
    REALM_ZONE_ENGLISH       = 8,                           // extended-Latin | 扩展拉丁语（英语区）
    REALM_ZONE_GERMAN        = 9,                           // extended-Latin | 扩展拉丁语（德语区）
    REALM_ZONE_FRENCH        = 10,                          // extended-Latin | 扩展拉丁语（法语区）
    REALM_ZONE_SPANISH       = 11,                          // extended-Latin | 扩展拉丁语（西班牙语区）
    REALM_ZONE_RUSSIAN       = 12,                          // Cyrillic | 西里尔字母（俄语区）
    REALM_ZONE_TOURNAMENT_13 = 13,                          // basic-Latin at create, any at login | 锦标赛区
    REALM_ZONE_TAIWAN        = 14,                          // East-Asian | 东亚语区（中国台湾）
    REALM_ZONE_TOURNAMENT_15 = 15,                          // basic-Latin at create, any at login | 锦标赛区
    REALM_ZONE_CHINA         = 16,                          // East-Asian | 东亚语区（中国大陆）
    REALM_ZONE_CN1           = 17,                          // basic-Latin at create, any at login | 中国 1 区
    REALM_ZONE_CN2           = 18,                          // basic-Latin at create, any at login | 中国 2 区
    REALM_ZONE_CN3           = 19,                          // basic-Latin at create, any at login | 中国 3 区
    REALM_ZONE_CN4           = 20,                          // basic-Latin at create, any at login | 中国 4 区
    REALM_ZONE_CN5           = 21,                          // basic-Latin at create, any at login | 中国 5 区
    REALM_ZONE_CN6           = 22,                          // basic-Latin at create, any at login | 中国 6 区
    REALM_ZONE_CN7           = 23,                          // basic-Latin at create, any at login | 中国 7 区
    REALM_ZONE_CN8           = 24,                          // basic-Latin at create, any at login | 中国 8 区
    REALM_ZONE_TOURNAMENT_25 = 25,                          // basic-Latin at create, any at login | 锦标赛区
    REALM_ZONE_TEST_SERVER   = 26,                          // any language | 测试服务器（任意语言）
    REALM_ZONE_TOURNAMENT_27 = 27,                          // basic-Latin at create, any at login | 锦标赛区
    REALM_ZONE_QA_SERVER     = 28,                          // any language | QA 服务器（任意语言）
    REALM_ZONE_CN9           = 29,                          // basic-Latin at create, any at login | 中国 9 区
    REALM_ZONE_TEST_SERVER_2 = 30,                          // any language | 测试服务器 2（任意语言）
    // in 3.x | 3.x 新增分区
    REALM_ZONE_CN10          = 31,                          // basic-Latin at create, any at login | 中国 10 区
    REALM_ZONE_CTC           = 32,
    REALM_ZONE_CNC           = 33,
    REALM_ZONE_CN1_4         = 34,                          // basic-Latin at create, any at login | 中国 1-4 区
    REALM_ZONE_CN2_6_9       = 35,                          // basic-Latin at create, any at login | 中国 2/6/9 区
    REALM_ZONE_CN3_7         = 36,                          // basic-Latin at create, any at login | 中国 3/7 区
    REALM_ZONE_CN5_8         = 37                           // basic-Latin at create, any at login | 中国 5/8 区
};

#define MAX_REALM_ZONES 38

// operator new[] based version of strdup() function! Release memory by using operator delete[] !
// | 基于 operator new[] 实现的 strdup() 版本，调用方须使用 operator delete[] 释放内存！
inline char* mangos_strdup(const char* source)
{
    char* dest = new char[strlen(source) + 1];
    strcpy(dest, source);
    return dest;
}

// we always use stdlibc++ std::max/std::min, undefine some not C++ standard defines (Win API and some pother platforms)
// | 统一使用 C++ 标准库 std::max/std::min，取消 WinAPI 等平台非标准宏定义以免冲突
#ifdef max
#  undef max
#endif

#ifdef min
#  undef min
#endif

#ifndef M_PI
#  define M_PI          3.14159265358979323846
#endif

#ifndef M_PI_F
#  define M_PI_F        float(M_PI)
#endif

#ifndef countof
#define countof(array) (sizeof(array) / sizeof((array)[0]))
#endif



/// 开发/GM 调试标志位枚举
enum CMDebugFlags
{
    CMDEBUGFLAG_NONE                        = 0x00000000,
    CMDEBUGFLAG_WP_PATH                     = 0x00000001, // show intermediates point in gm mode (waypoints) | 在 GM 模式下向客户端显示路径点中间节点



    CMDEBUGFLAG_DEV_USE1                    = 0x80000000, // can be used for various reason during development | 开发调试保留标志位 1
    CMDEBUGFLAG_DEV_USE2                    = 0x80000000  // can be used for various reason during development | 开发调试保留标志位 2
};

/// 调试命令行表项结构
struct CMDebugCommandTableStruct
{
    CMDebugCommandTableStruct(std::string const& cmd, std::string const& desc, CMDebugFlags f) :
        command(cmd), description(desc), flag(f) {}

    std::string command;
    std::string description;
    CMDebugFlags flag;
};

/// 支持的调试标志开关命令映射表
static const std::vector<CMDebugCommandTableStruct> CMDebugCommandTable =
{
    { "clearall"                , "reset all flags"                         , CMDEBUGFLAG_NONE                  },
    { "setall"                  , "set all flags"                           , CMDEBUGFLAG_NONE                  },

    { "wppath"                  , "show waypoint path send to client"       , CMDEBUGFLAG_WP_PATH               },

    { "dev1"                    , "for general use during development"      , CMDEBUGFLAG_DEV_USE1              },
    { "dev2"                    , "for general use during development"      , CMDEBUGFLAG_DEV_USE2              }
};

#endif
