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

#ifndef __SQLOPERATIONS_H
#define __SQLOPERATIONS_H

#include "Common.h"
#include "Utilities/Callback.h"

#include <queue>
#include <vector>
#include <mutex>
#include <memory>

/// ---- BASE --- | 基础抽象操作类

class Database;
class SqlConnection;
class SqlDelayThread;
class SqlStmtParameters;

// 异步 SQL 操作抽象基类 | Abstract base class for queued async SQL operations
class SqlOperation
{
    public:
        virtual void OnRemove() { delete this; }
        // 在指定数据库连接上执行该操作 | Execute this operation on the given SqlConnection
        virtual bool Execute(SqlConnection* conn) = 0;
        virtual ~SqlOperation() {}
};

/// ---- ASYNC STATEMENTS / TRANSACTIONS ---- | 异步语句与事务操作

// 异步普通文本 SQL 请求 | Plain SQL string execution operation
class SqlPlainRequest : public SqlOperation
{
    private:
        const char* m_sql;
    public:
        SqlPlainRequest(const char* sql) : m_sql(mangos_strdup(sql)) {}
        ~SqlPlainRequest() { char* tofree = const_cast<char*>(m_sql); delete[] tofree; }
        bool Execute(SqlConnection* conn) override;
};

// 异步事务操作，按序在同一事务中执行多条子操作，失败则回滚 | Transaction operation executing queued statements atomically with rollback on failure
class SqlTransaction : public SqlOperation
{
    private:
        std::vector<SqlOperation* > m_queue;

    public:
        SqlTransaction() {}
        ~SqlTransaction();

        // 向事务队列追加一条子 SQL 操作 | Append a child SQL operation to this transaction
        void DelayExecute(SqlOperation* sql) { m_queue.push_back(sql); }

        bool Execute(SqlConnection* conn) override;
};

// 异步预处理语句执行请求 | Prepared statement execution operation
class SqlPreparedRequest : public SqlOperation
{
    public:
        SqlPreparedRequest(int nIndex, SqlStmtParameters* arg);
        ~SqlPreparedRequest();

        bool Execute(SqlConnection* conn) override;

    private:
        const int m_nIndex;
        SqlStmtParameters* m_param;
};

/// ---- ASYNC QUERIES ---- | 异步查询与回调机制

class SqlQuery;                                             /// contains a single async query | 单条异步查询操作
class QueryResult;                                          /// the result of one | 单条查询结果
class SqlQueryHolder;                                       /// groups several async quries | 批量异步查询持有器
class SqlQueryHolderEx;                                     /// points to a holder, added to the delay thread | 关联持有器并提交至延迟线程的执行器

// 线程安全的异步查询结果回调队列，由主线程定期排空执行 | Thread-safe queue of completed query callbacks processed on the main thread
class SqlResultQueue
{
    private:
        std::mutex m_mutex;
        std::queue<std::unique_ptr<MaNGOS::IQueryCallback>> m_queue;

    public:
        // 执行队列中所有待处理的查询回调 | Invoke all pending query callbacks in queue
        void Update();
        // 将已填入结果的回调对象加入队列 | Enqueue a completed query callback
        void Add(MaNGOS::IQueryCallback*);
};

// 单条异步查询操作，执行后将结果写入回调并推入结果队列 | Single async query operation that populates callback and pushes it to SqlResultQueue
class SqlQuery : public SqlOperation
{
    private:
        std::vector<char> m_sql;
        MaNGOS::IQueryCallback* const m_callback;
        SqlResultQueue* const m_queue;

    public:
        SqlQuery(const char* sql, MaNGOS::IQueryCallback* callback, SqlResultQueue* queue)
            : m_sql(strlen(sql) + 1), m_callback(callback), m_queue(queue)
        {
            memcpy(&m_sql[0], sql, m_sql.size());
        }

        bool Execute(SqlConnection* conn) override;
};

// 批量异步查询持有器，用于在单次异步任务中打包多条查询并在回调中统一读取结果 | Container grouping multiple queries for atomic async batch execution and callback retrieval
class SqlQueryHolder
{
        friend class SqlQueryHolderEx;
    private:
        typedef std::pair<const char*, std::unique_ptr<QueryResult>> SqlResultPair;
        std::vector<SqlResultPair> m_queries;
    public:
        SqlQueryHolder() {}
        virtual ~SqlQueryHolder();
        bool SetQuery(size_t index, const char* sql);
        bool SetPQuery(size_t index, const char* format, ...) ATTR_PRINTF(3, 4);
        void SetSize(size_t size);
        std::unique_ptr<QueryResult> GetResult(size_t index);
        void SetResult(size_t index, std::unique_ptr<QueryResult> queryResult);
        bool Execute(MaNGOS::IQueryCallback* callback, SqlDelayThread* thread, SqlResultQueue* queue);
};

// 负责在延迟线程中依次执行 SqlQueryHolder 内所有查询并同步回调 | Delay-thread operation executing all queries in a SqlQueryHolder and notifying callback
class SqlQueryHolderEx : public SqlOperation
{
    private:
        SqlQueryHolder* m_holder;
        MaNGOS::IQueryCallback* m_callback;
        SqlResultQueue* m_queue;
    public:
        SqlQueryHolderEx(SqlQueryHolder* holder, MaNGOS::IQueryCallback* callback, SqlResultQueue* queue)
            : m_holder(holder), m_callback(callback), m_queue(queue) {}
        bool Execute(SqlConnection* conn) override;
};
#endif                                                      //__SQLOPERATIONS_H
