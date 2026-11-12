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

#include "SqlOperations.h"
#include "SqlDelayThread.h"
#include "DatabaseEnv.h"
#include "DatabaseImpl.h"

#include <cstdarg>

#define LOCK_DB_CONN(conn) SqlConnection::Lock guard(conn)

/// ---- ASYNC STATEMENTS / TRANSACTIONS ---- | 异步普通语句与事务执行实现

bool SqlPlainRequest::Execute(SqlConnection* conn)
{
    /// just do it | 加锁并直接执行 SQL 文本
    LOCK_DB_CONN(conn);
    return conn->Execute(m_sql);
}

SqlTransaction::~SqlTransaction()
{
    while (!m_queue.empty())
    {
        delete m_queue.back();
        m_queue.pop_back();
    }
}

// 在数据库连接上开启事务并顺序执行所有子操作，任一失败即回滚 | Begin transaction, execute queued operations sequentially, rollback on any failure
bool SqlTransaction::Execute(SqlConnection* conn)
{
    if (m_queue.empty())
        return true;

    LOCK_DB_CONN(conn);

    conn->BeginTransaction();

    const int nItems = m_queue.size();
    for (int i = 0; i < nItems; ++i)
    {
        SqlOperation* pStmt = m_queue[i];

        if (!pStmt->Execute(conn))
        {
            conn->RollbackTransaction();
            return false;
        }
    }

    return conn->CommitTransaction();
}

SqlPreparedRequest::SqlPreparedRequest(int nIndex, SqlStmtParameters* arg) : m_nIndex(nIndex), m_param(arg)
{
}

SqlPreparedRequest::~SqlPreparedRequest()
{
    delete m_param;
}

bool SqlPreparedRequest::Execute(SqlConnection* conn)
{
    LOCK_DB_CONN(conn);
    return conn->ExecuteStmt(m_nIndex, *m_param);
}

/// ---- ASYNC QUERIES ---- | 异步查询与结果队列实现

bool SqlQuery::Execute(SqlConnection* conn)
{
    if (!m_callback || !m_queue)
        return false;

    LOCK_DB_CONN(conn);
    /// execute the query and store the result in the callback | 执行查询并将结果存入回调对象
    m_callback->SetResult(conn->Query(&m_sql[0]));
    /// add the callback to the sql result queue of the thread it originated from | 将回调加入发起线程对应的结果同步队列
    m_queue->Add(m_callback);

    return true;
}

void SqlResultQueue::Update()
{
    std::lock_guard<std::mutex> guard(m_mutex);

    /// execute the callbacks waiting in the synchronization queue | 依次执行结果同步队列中等待的回调
    while (!m_queue.empty())
    {
        auto const callback = std::move(m_queue.front());
        m_queue.pop();
        callback->Execute();
    }
}

void SqlResultQueue::Add(MaNGOS::IQueryCallback* callback)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_queue.push(std::unique_ptr<MaNGOS::IQueryCallback>(callback));
}

bool SqlQueryHolder::Execute(MaNGOS::IQueryCallback* callback, SqlDelayThread* thread, SqlResultQueue* queue)
{
    if (!callback || !thread || !queue)
        return false;

    /// delay the execution of the queries, sync them with the delay thread | 将批量查询交由延迟线程执行，
    /// which will in turn resync on execution (via the queue) and call back | 完成后通过结果队列同步回到主线程触发回调
    SqlQueryHolderEx* holderEx = new SqlQueryHolderEx(this, callback, queue);
    thread->Delay(holderEx);
    return true;
}

bool SqlQueryHolder::SetQuery(size_t index, const char* sql)
{
    if (m_queries.size() <= index)
    {
        sLog.outError("Query index (" SIZEFMTD ") out of range (size: " SIZEFMTD ") for query: %s", index, m_queries.size(), sql);
        return false;
    }

    if (m_queries[index].first != nullptr)
    {
        sLog.outError("Attempt assign query to holder index (" SIZEFMTD ") where other query stored (Old: [%s] New: [%s])",
                      index, m_queries[index].first, sql);
        return false;
    }

    /// not executed yet, just stored (it's not called a holder for nothing) | 仅存储待执行 SQL 字符串，暂不执行
    m_queries[index] = SqlResultPair(mangos_strdup(sql), std::unique_ptr<QueryResult>{});
    return true;
}

bool SqlQueryHolder::SetPQuery(size_t index, const char* format, ...)
{
    if (!format)
    {
        sLog.outError("Query (index: " SIZEFMTD ") is empty.", index);
        return false;
    }

    va_list ap;
    char szQuery [MAX_QUERY_LEN];
    va_start(ap, format);
    int res = vsnprintf(szQuery, MAX_QUERY_LEN, format, ap);
    va_end(ap);

    if (res == -1)
    {
        sLog.outError("SQL Query truncated (and not execute) for format: %s", format);
        return false;
    }

    return SetQuery(index, szQuery);
}

std::unique_ptr<QueryResult> SqlQueryHolder::GetResult(size_t index)
{
    if (index < m_queries.size())
    {
        /// the query strings are freed on the first GetResult or in the destructor | 查询字符串在首次 GetResult 或析构时释放
        if (m_queries[index].first != nullptr)
        {
            delete[](const_cast<char*>(m_queries[index].first));
            m_queries[index].first = nullptr;
        }

        return std::move(m_queries[index].second);
    }
    return {};
}

void SqlQueryHolder::SetResult(size_t index, std::unique_ptr<QueryResult> queryResult)
{
    /// store the result in the holder | 将查询结果移交存储到持有器对应槽位
    if (index < m_queries.size())
        m_queries[index].second = std::move(queryResult);
}

SqlQueryHolder::~SqlQueryHolder()
{
    for (auto& m_querie : m_queries)
    {
        /// if the result was never used, free the resources | 若结果未被取出，释放残留的查询字符串内存
        /// results used already (getresult called) are expected to be deleted | 已调用 GetResult 取出的结果由调用方负责销毁
        if (m_querie.first != nullptr)
        {
            delete[](const_cast<char*>(m_querie.first));
        }
    }
}

void SqlQueryHolder::SetSize(size_t size)
{
    /// to optimize push_back, reserve the number of queries about to be executed | 预先调整槽位大小以容纳待执行的查询数量
    m_queries.resize(size);
}

bool SqlQueryHolderEx::Execute(SqlConnection* conn)
{
    if (!m_holder || !m_callback || !m_queue)
        return false;

    LOCK_DB_CONN(conn);
    /// we can do this, we are friends | 通过友元权限直接访问 m_holder 的内部查询列表
    std::vector<SqlQueryHolder::SqlResultPair>& queries = m_holder->m_queries;
    for (size_t i = 0; i < queries.size(); ++i)
    {
        /// execute all queries in the holder and pass the results | 逐条执行持有器中的查询并保存结果
        char const* sql = queries[i].first;
        if (sql) m_holder->SetResult(i, conn->Query(sql));
    }

    /// sync with the caller thread | 投递回调到结果队列以与调用线程同步
    m_queue->Add(m_callback);

    return true;
}
