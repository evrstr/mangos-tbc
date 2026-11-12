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

#ifndef __SQLDELAYTHREAD_H
#define __SQLDELAYTHREAD_H

#include "Multithreading/Threading.h"
#include "SqlOperations.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>

class Database;
class SqlOperation;
class SqlConnection;

// 异步 SQL 延迟执行工作线程类，负责串行消费队列中的写操作、事务及异步查询 | Worker thread serially executing queued SQL writes, transactions, and async queries
class SqlDelayThread : public MaNGOS::Runnable
{
    private:
        std::mutex m_queueMutex;
        std::queue<std::unique_ptr<SqlOperation>> m_sqlQueue;   ///< Queue of SQL statements | 待执行 SQL 操作队列
        Database* m_dbEngine;                                   ///< Pointer to used Database engine | 所属 Database 引擎指针
        SqlConnection* m_dbConnection;                          ///< Pointer to DB connection | 专用的数据库连接指针
        std::atomic<bool> m_running;                            ///< 线程运行标志

        // process all enqueued requests | 取出并执行队列中当前积压的所有 SQL 请求
        void ProcessRequests();

    public:
        SqlDelayThread(Database* db, SqlConnection* conn);
        ~SqlDelayThread();

        ///< Put sql statement to delay queue | 将 SQL 操作放入异步延迟队列
        bool Delay(SqlOperation* sql)
        {
            std::lock_guard<std::mutex> guard(m_queueMutex);
            m_sqlQueue.push(std::unique_ptr<SqlOperation>(sql));
            return true;
        }

        virtual void Stop();                                ///< Stop event | 请求停止工作线程
        virtual void run();                                 ///< Main Thread loop | 工作线程主循环
};
#endif                                                      //__SQLDELAYTHREAD_H
