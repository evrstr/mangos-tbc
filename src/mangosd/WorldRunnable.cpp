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

/** \file
    \ingroup mangosd
*/

#include "Common.h"
#include "World/World.h"
#include "WorldRunnable.h"
#include "Util/Timer.h"
#include "Maps/MapManager.h"

#include "Database/DatabaseEnv.h"

#define WORLD_SLEEP_CONST 50

#ifdef _WIN32
#include "Platform/ServiceWin32.h"
#include "timeapi.h"
extern int m_ServiceStatus;
#endif

/// Windows 系统定时器分辨率 RAII 封装：构造时提升定时器精度，析构时自动恢复
struct TimeBeginRAII
{
    TimeBeginRAII()
    {
#ifdef _WIN32
        TIMECAPS timeResolutionLimits;
        if (timeGetDevCaps(&timeResolutionLimits, sizeof(TIMECAPS)) == TIMERR_NOERROR)
        {
            newTimerResolution = std::min(std::max(timeResolutionLimits.wPeriodMin, 1u), timeResolutionLimits.wPeriodMax);
            timeBeginPeriod(*newTimerResolution);
        }
#endif
    }

    ~TimeBeginRAII()
    {
#ifdef _WIN32
        if (newTimerResolution)
            timeEndPeriod(*newTimerResolution);
#endif
    }

#ifdef _WIN32
    std::optional<UINT> newTimerResolution;
#endif
};

/// Heartbeat for the World | 世界主循环心跳入口
void WorldRunnable::run()
{
    TimeBeginRAII raii;

    ///- Init new SQL thread for the world database | 为世界数据库初始化新的 SQL 线程
    WorldDatabase.ThreadStart();                            // let thread do safe mySQL requests (one connection call enough) | 使当前线程能够安全执行 MySQL 请求（单次连接调用即可）
    sWorld.InitResultQueue();

    uint32 diffTick = WorldTimer::tick(); // initialize world timer vars | 初始化世界计时器变量
    uint32 diffTime = 0; // used to compute real time elapsed in World::Update() | 用于计算 World::Update() 的实际耗时
    uint32 overCounter = 0; // count overtime loops | 统计超时循环次数

    ///- While we have not World::m_stopEvent, update the world | 只要未触发世界停止事件（World::m_stopEvent），就持续更新世界状态
    while (!World::IsStopped())
    {
//        ++World::m_worldLoopCounter;
        World::m_worldLoopCounter = World::m_worldLoopCounter + 1;

        diffTick = WorldTimer::tick();
        sWorld.Update(diffTick);
        diffTime = WorldTimer::getMSTime() - WorldTimer::tickTime();

        // we have to wait WORLD_SLEEP_CONST max between loops | 两次循环之间最多等待 WORLD_SLEEP_CONST 毫秒
        // don't wait if over | 若本次更新耗时已超过该阈值则不再休眠
        if (diffTime < WORLD_SLEEP_CONST)
        {
            MaNGOS::Thread::Sleep(WORLD_SLEEP_CONST - diffTime);
        }
#ifdef MANGOS_DEBUG
        else
        {
            ++overCounter;
            sLog.outString("WorldRunnable:run Long loop #%d : %dms (total : %d loop(s), %.3f%%)", World::m_worldLoopCounter, diffTime, overCounter, (float)(100*overCounter) / (float)World::m_worldLoopCounter);
        }
#endif

#ifdef _WIN32
        if (m_ServiceStatus == 0)
            World::StopNow(SHUTDOWN_EXIT_CODE);

        while (m_ServiceStatus == 2)
            Sleep(1000);
#endif
    }

    sWorld.CleanupsBeforeStop();

    ///- End the database thread | 结束数据库线程
    WorldDatabase.ThreadEnd();                              // free mySQL thread resources | 释放 MySQL 线程资源
}
