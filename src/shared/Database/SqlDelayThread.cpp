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

#include "Database/SqlDelayThread.h"
#include "Database/SqlOperations.h"
#include "DatabaseEnv.h"

SqlDelayThread::SqlDelayThread(Database* db, SqlConnection* conn) : m_dbEngine(db), m_dbConnection(conn), m_running(true)
{
}

SqlDelayThread::~SqlDelayThread()
{
    // process all requests which might have been queued while thread was stopping | 析构时处理线程停止期间残留的队列请求，确保数据不丢失
    ProcessRequests();
}

// 后台 SQL 执行线程主循环：周期性消费请求队列并触发数据库心跳 Ping | Worker thread loop: periodically drain SQL queue and send keep-alive pings
void SqlDelayThread::run()
{
#ifndef DO_POSTGRESQL
#ifndef DO_SQLITE
    mysql_thread_init();
#endif
#endif

    const uint32 loopSleepms = 10;

    const uint32 pingEveryLoop = m_dbEngine->GetPingIntervall() / loopSleepms;

    uint32 loopCounter = 0;
    while (m_running)
    {
        // if the running state gets turned off while sleeping | 若休眠期间收到停止信号，
        // empty the queue before exiting | 在退出前清空并执行剩余队列
        MaNGOS::Thread::Sleep(loopSleepms);

        ProcessRequests();

        if ((loopCounter++) >= pingEveryLoop)
        {
            loopCounter = 0;
            m_dbEngine->Ping();
        }
    }

#ifndef DO_POSTGRESQL
#ifndef DO_SQLITE
    mysql_thread_end();
#endif
#endif
}

// 设置停止标志以退出主循环 | Clear running flag to exit worker loop
void SqlDelayThread::Stop()
{
    m_running = false;
}

// 将队列整体移至局部副本后无锁执行，避免持锁执行 SQL 时与主线程产生死锁 | Move queue locally before execution to prevent deadlock with world thread
void SqlDelayThread::ProcessRequests()
{
    std::queue<std::unique_ptr<SqlOperation>> sqlQueue;

    // we need to move the contents of the queue to a local copy because executing these statements with the | 必须先加锁将队列交换到局部变量再解锁执行：
    // lock in place can result in a deadlock with the world thread which calls Database::ProcessResultQueue() | 持锁执行语句可能与调用 Database::ProcessResultQueue() 的主线程发生死锁
    {
        std::lock_guard<std::mutex> guard(m_queueMutex);
        sqlQueue = std::move(m_sqlQueue);
    }

    while (!sqlQueue.empty())
    {
        auto const s = std::move(sqlQueue.front());
        sqlQueue.pop();
        s->Execute(m_dbConnection);
    }
}
