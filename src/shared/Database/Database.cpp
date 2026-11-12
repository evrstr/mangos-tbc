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

#include "DatabaseEnv.h"
#include "Config/Config.h"
#include "Database/SqlOperations.h"

#include <ctime>
#include <iostream>
#include <fstream>
#include <memory>
#include <cstdarg>

#define MIN_CONNECTION_POOL_SIZE 1
#define MAX_CONNECTION_POOL_SIZE 16

//////////////////////////////////////////////////////////////////////////
// 默认创建基于纯文本替换的预处理语句对象 | Create plain-text prepared statement by default
SqlPreparedStatement* SqlConnection::CreateStatement(const std::string& fmt)
{
    return new SqlPlainPreparedStatement(fmt, *this);
}

// 释放连接中缓存的所有预处理语句 | Free all cached prepared statements on this connection
void SqlConnection::FreePreparedStatements()
{
    SqlConnection::Lock guard(this);

    size_t nStmts = m_holder.size();
    for (size_t i = 0; i < nStmts; ++i)
        delete m_holder[i];

    m_holder.clear();
}

// 按索引获取或懒加载并准备预处理语句 | Get or lazily create and prepare statement by index
SqlPreparedStatement* SqlConnection::GetStmt(uint32 nIndex)
{
    // resize stmt container | 按需扩容预处理语句容器
    if (m_holder.size() <= nIndex)
        m_holder.resize(nIndex + 1, nullptr);

    SqlPreparedStatement* pStmt = nullptr;

    // create stmt if needed | 若尚未创建则创建并预处理
    if (m_holder[nIndex] == nullptr)
    {
        // obtain SQL request string | 从数据库注册表获取 SQL 模板
        std::string fmt = m_db.GetStmtString(nIndex);
        MANGOS_ASSERT(fmt.length());
        // allocate SQlPreparedStatement object | 分配预处理语句对象
        pStmt = CreateStatement(fmt);
        // prepare statement | 预处理该语句
        if (!pStmt->prepare())
        {
            MANGOS_ASSERT(false && "Unable to prepare SQL statement");
            return nullptr;
        }

        // save statement in internal registry | 缓存到连接内部的语句表
        m_holder[nIndex] = pStmt;
    }
    else
        pStmt = m_holder[nIndex];

    return pStmt;
}

// 绑定参数并执行指定索引的预处理语句 | Bind parameters and execute prepared statement by index
bool SqlConnection::ExecuteStmt(int nIndex, const SqlStmtParameters& id)
{
    if (nIndex == -1)
        return false;

    // get prepared statement object | 获取预处理语句对象
    SqlPreparedStatement* pStmt = GetStmt(nIndex);
    // bind parameters | 绑定输入参数
    pStmt->bind(id);
    // execute statement | 执行语句
    return pStmt->execute();
}

//////////////////////////////////////////////////////////////////////////
Database::~Database()
{
    StopServer();
}

// 初始化连接池、异步连接、结果队列与延迟工作线程 | Initialize connection pool, async connection, result queue, and delay thread
bool Database::Initialize(const char* infoString, int nConns /*= 1*/)
{
    // Enable logging of SQL commands (usually only GM commands) | 启用 SQL 命令日志记录（通常仅记录 GM 命令）
    // (See method: PExecuteLog) | 详见 PExecuteLog 方法
    m_logSQL = sConfig.GetBoolDefault("LogSQL", false);
    m_logsDir = sConfig.GetStringDefault("LogsDir");
    if (!m_logsDir.empty())
    {
        if ((m_logsDir.at(m_logsDir.length() - 1) != '/') && (m_logsDir.at(m_logsDir.length() - 1) != '\\'))
            m_logsDir.append("/");
    }

    m_pingIntervallms = sConfig.GetIntDefault("MaxPingTime", 30) * (MINUTE * 1000);

    // create DB connections | 创建数据库连接

    // setup connection pool size | 限制同步查询连接池大小在合理范围
    if (nConns < MIN_CONNECTION_POOL_SIZE)
        m_nQueryConnPoolSize = MIN_CONNECTION_POOL_SIZE;
    else if (nConns > MAX_CONNECTION_POOL_SIZE)
        m_nQueryConnPoolSize = MAX_CONNECTION_POOL_SIZE;
    else
        m_nQueryConnPoolSize = nConns;

    // create connection pool for sync requests | 创建同步查询连接池
    for (int i = 0; i < m_nQueryConnPoolSize; ++i)
    {
        SqlConnection* pConn = CreateConnection();
        if (!pConn->Initialize(infoString))
        {
            delete pConn;
            return false;
        }

        m_pQueryConnections.push_back(pConn);
    }

    // create and initialize connection for async requests | 创建并初始化异步操作专用连接
    m_pAsyncConn = CreateConnection();
    if (!m_pAsyncConn->Initialize(infoString))
        return false;

    m_pResultQueue = new SqlResultQueue;

    InitDelayThread();
    return true;
}

// 停止后台延迟线程并销毁所有连接和结果队列 | Stop delay thread and destroy all connections and result queue
void Database::StopServer()
{
    HaltDelayThread();

    delete m_pResultQueue;
    delete m_pAsyncConn;

    m_pResultQueue = nullptr;
    m_pAsyncConn = nullptr;

    for (auto& m_pQueryConnection : m_pQueryConnections)
        delete m_pQueryConnection;

    m_pQueryConnections.clear();
}

// 创建延迟执行线程体 | Create delay thread runnable object
SqlDelayThread* Database::CreateDelayThread()
{
    assert(m_pAsyncConn);
    return new SqlDelayThread(this, m_pAsyncConn);
}

// 初始化并启动异步 SQL 执行线程 | Initialize and launch async SQL delay thread
void Database::InitDelayThread()
{
    assert(!m_delayThread);

    // New delay thread for delay execute | 创建新的延迟执行线程
    m_threadBody = CreateDelayThread();              // will deleted at m_delayThread delete | 将在删除 m_delayThread 时自动销毁
    m_delayThread = new MaNGOS::Thread(m_threadBody);
}

// 停止并等待延迟线程结束 | Stop delay thread and wait for pending queries to flush
void Database::HaltDelayThread()
{
    if (!m_threadBody || !m_delayThread) return;

    m_threadBody->Stop();                                   // Stop event | 发送停止信号
    m_delayThread->wait();                                  // Wait for flush to DB | 等待队列中 SQL 刷入数据库
    delete m_delayThread;                                   // This also deletes m_threadBody | 删除线程对象并同时销毁 m_threadBody
    m_delayThread = nullptr;
    m_threadBody = nullptr;
}

void Database::ThreadStart()
{
}

void Database::ThreadEnd()
{
}

// 在调用线程上派发已完成的异步查询回调 | Dispatch completed async query callbacks from result queue
void Database::ProcessResultQueue()
{
    if (m_pResultQueue)
        m_pResultQueue->Update();
}

// 原地转义 SQL 字符串 | Escape special SQL characters in-place
void Database::escape_string(std::string& str)
{
    if (str.empty())
        return;

    char* buf = new char[str.size() * 2 + 1];
    // we don't care what connection to use - escape string will be the same | 任意连接上的转义结果一致，直接复用首连接
    m_pQueryConnections[0]->escape_string(buf, str.c_str(), str.size());
    str = buf;
    delete[] buf;
}

// 以原子轮询方式从同步查询连接池获取连接 | Select a sync query connection in round-robin order
SqlConnection* Database::getQueryConnection()
{
    int nCount = 0;

    if (m_nQueryCounter == long(1 << 31))
        m_nQueryCounter = 0;
    else
        nCount = ++m_nQueryCounter;

    return m_pQueryConnections[nCount % m_nQueryConnPoolSize];
}

// 对异步连接及所有同步连接发送心跳查询 | Send keep-alive SELECT 1 ping on async and all query connections
void Database::Ping()
{
    const char* sql = "SELECT 1";

    {
        SqlConnection::Lock guard(m_pAsyncConn);
        guard->Query(sql);
    }

    for (int i = 0; i < m_nQueryConnPoolSize; ++i)
    {
        SqlConnection::Lock guard(m_pQueryConnections[i]);
        guard->Query(sql);
    }
}

// 格式化执行 SQL 并按配置写入每日 SQL 日志文件 | Format and execute SQL, optionally appending to daily SQL log file
bool Database::PExecuteLog(const char* format, ...)
{
    if (!format)
        return false;

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

    if (m_logSQL)
    {
        time_t curr;
        time(&curr);                                        // get current time_t value | 获取当前时间戳
        tm local = *(localtime(&curr));                        // dereference and assign | 转换为本地时间结构
        char fName[128];
        sprintf(fName, "%04d-%02d-%02d_logSQL.sql", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);

        std::string logsDir_fname = m_logsDir + fName;
        FILE* log_file = fopen(logsDir_fname.c_str(), "a");
        if (log_file)
        {
            fprintf(log_file, "%s;\n", szQuery);
            fclose(log_file);
        }
        else
        {
            // The file could not be opened | 日志文件打开失败
            sLog.outError("SQL-Logging is disabled - Log file for the SQL commands could not be openend: %s", fName);
        }
    }

    return Execute(szQuery);
}

// 格式化并执行同步查询 | Format SQL and execute synchronous query
std::unique_ptr<QueryResult> Database::PQuery(const char* format, ...)
{
    if (!format)
        return {};

    va_list ap;
    char szQuery [MAX_QUERY_LEN];
    va_start(ap, format);
    int res = vsnprintf(szQuery, MAX_QUERY_LEN, format, ap);
    va_end(ap);

    if (res == -1)
    {
        sLog.outError("SQL Query truncated (and not execute) for format: %s", format);
        return {};
    }

    return Query(szQuery);
}

// 格式化并执行同步命名查询 | Format SQL and execute synchronous named query
QueryNamedResult* Database::PQueryNamed(const char* format, ...)
{
    if (!format) return nullptr;

    va_list ap;
    char szQuery [MAX_QUERY_LEN];
    va_start(ap, format);
    int res = vsnprintf(szQuery, MAX_QUERY_LEN, format, ap);
    va_end(ap);

    if (res == -1)
    {
        sLog.outError("SQL Query truncated (and not execute) for format: %s", format);
        return nullptr;
    }

    return QueryNamed(szQuery);
}

// 根据当前是否处于事务及异步开关，路由执行 SQL | Route SQL execution to active transaction, async thread, or direct execution
bool Database::Execute(const char* sql)
{
    if (!m_pAsyncConn)
        return false;

    auto const pTrans = m_currentTransaction.get();
    if (pTrans)
    {
        // add SQL request to trans queue | 加入当前线程的事务队列
        pTrans->DelayExecute(new SqlPlainRequest(sql));
    }
    else
    {
        // if async execution is not available | 若尚未启用异步执行则同步直连执行
        if (!m_allowAsyncTransactions)
            return DirectExecute(sql);

        // Simple sql statement | 提交普通 SQL 请求到延迟线程
        m_threadBody->Delay(new SqlPlainRequest(sql));
    }

    return true;
}

// 格式化并异步/事务执行 SQL | Format SQL and execute asynchronously or within transaction
bool Database::PExecute(const char* format, ...)
{
    if (!format)
        return false;

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

    return Execute(szQuery);
}

// 格式化并在异步连接上同步直接执行 SQL | Format SQL and execute synchronously on async connection
bool Database::DirectPExecute(const char* format, ...)
{
    if (!format)
        return false;

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

    return DirectExecute(szQuery);
}

// 在当前线程开启一个新事务（不支持嵌套事务） | Begin a new thread-local transaction (nested transactions forbidden)
bool Database::BeginTransaction()
{
    if (!m_pAsyncConn)
        return false;

    MANGOS_ASSERT(!m_currentTransaction.get());   // if we will get a nested transaction request - we MUST fix code!!! | 检测到嵌套事务请求说明调用代码存在逻辑错误，必须修复

    if (!m_currentTransaction.get())
        m_currentTransaction.reset(new SqlTransaction);

    return m_currentTransaction.get() != nullptr;
}

// 提交当前线程事务（异步或直接提交） | Commit current thread-local transaction (async or direct)
bool Database::CommitTransaction()
{
    if (!m_pAsyncConn || !m_currentTransaction.get())
        return false;

    // if async execution is not available | 若异步执行未开启则直接同步提交
    if (!m_allowAsyncTransactions)
        return CommitTransactionDirect();

    // add SqlTransaction to the async queue | 将事务对象移交异步队列执行
    m_threadBody->Delay(m_currentTransaction.release());
    return true;
}

// 同步直接执行并提交当前线程事务 | Synchronously execute and commit current thread-local transaction
bool Database::CommitTransactionDirect()
{
    if (!m_pAsyncConn)
        return false;

    // check if we have pending transaction | 检查是否存在待提交事务
    if (!m_currentTransaction.get())
        return false;

    // directly execute SqlTransaction | 直接在异步连接上同步执行事务并释放
    auto const pTrans = m_currentTransaction.release();
    pTrans->Execute(m_pAsyncConn);
    delete pTrans;

    return true;
}

// 丢弃当前线程尚未提交的延迟事务 | Discard uncommitted thread-local transaction
bool Database::RollbackTransaction()
{
    if (!m_pAsyncConn)
        return false;

    if (!m_currentTransaction.get())
        return false;

    // remove scheduled transaction | 释放并清除已排队的事务对象
    m_currentTransaction.reset();

    return true;
}

// 校验指定版本表是否包含所需字段，失败时输出详细的 SQL 更新提示 | Verify required version column in table and log update instructions on failure
bool Database::CheckRequiredField(char const* table_name, char const* required_name)
{
    // check required field | 查询所需的版本列名
    auto queryResult = PQuery("SELECT %s FROM %s LIMIT 1", required_name, table_name);
    if (queryResult)
    {
        return true;
    }

    // check fail, prepare readable error message | 检查失败，构造可读的数据库版本差异提示

    // search current required_* field in DB | 识别所属数据库名称并查找当前库中的 required_* 列
    const char* db_name;
    if (!strcmp(table_name, "db_version"))
        db_name = "WORLD";
    else if (!strcmp(table_name, "character_db_version"))
        db_name = "CHARACTER";
    else if (!strcmp(table_name, "realmd_db_version"))
        db_name = "REALMD";
    else
        db_name = "UNKNOWN";

    char const* req_sql_update_name = required_name + strlen("required_");

    QueryNamedResult* result2 = PQueryNamed("SELECT * FROM %s LIMIT 1", table_name);
    if (result2)
    {
        QueryFieldNames const& namesMap = result2->GetFieldNames();
        std::string reqName;
        for (const auto& itr : namesMap)
        {
            if (itr.substr(0, 9) == "required_")
            {
                reqName = itr;
                break;
            }
        }

        delete result2;

        std::string cur_sql_update_name = reqName.substr(strlen("required_"), std::string::npos);

        if (!reqName.empty())
        {
            sLog.outErrorDb("The table `%s` in your [%s] database indicates that this database is out of date!", table_name, db_name);
            sLog.outErrorDb();
            sLog.outErrorDb("  [A] You have: --> `%s.sql`", cur_sql_update_name.c_str());
            sLog.outErrorDb();
            sLog.outErrorDb("  [B] You need: --> `%s.sql`", req_sql_update_name);
            sLog.outErrorDb();
            sLog.outErrorDb("You must apply all updates after [A] to [B] to use mangos with this database.");
            sLog.outErrorDb("These updates are included in the sql/updates folder.");
            sLog.outErrorDb("Please read the included [README] in sql/updates for instructions on updating.");
        }
        else
        {
            sLog.outErrorDb("The table `%s` in your [%s] database is missing its version info.", table_name, db_name);
            sLog.outErrorDb("MaNGOS cannot find the version info needed to check that the db is up to date.");
            sLog.outErrorDb();
            sLog.outErrorDb("This revision of MaNGOS requires a database updated to:");
            sLog.outErrorDb("`%s.sql`", req_sql_update_name);
            sLog.outErrorDb();

            if (!strcmp(db_name, "WORLD"))
                sLog.outErrorDb("Post this error to your database provider forum or find a solution there.");
            else
                sLog.outErrorDb("Reinstall your [%s] database with the included sql file in the sql folder.", db_name);
        }
    }
    else
    {
        sLog.outErrorDb("The table `%s` in your [%s] database is missing or corrupt.", table_name, db_name);
        sLog.outErrorDb("MaNGOS cannot find the version info needed to check that the db is up to date.");
        sLog.outErrorDb();
        sLog.outErrorDb("This revision of mangos requires a database updated to:");
        sLog.outErrorDb("`%s.sql`", req_sql_update_name);
        sLog.outErrorDb();

        if (!strcmp(db_name, "WORLD"))
            sLog.outErrorDb("Post this error to your database provider forum or find a solution there.");
        else
            sLog.outErrorDb("Reinstall your [%s] database with the included sql file in the sql folder.", db_name);
    }

    return false;
}

// 将预处理语句执行请求入队到事务或异步线程 | Enqueue prepared statement request to active transaction, async thread, or direct execution
bool Database::ExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params)
{
    if (!m_pAsyncConn)
        return false;

    auto const pTrans = m_currentTransaction.get();
    if (pTrans)
    {
        // add SQL request to trans queue | 加入事务队列
        pTrans->DelayExecute(new SqlPreparedRequest(id.ID(), params));
    }
    else
    {
        // if async execution is not available | 未启用异步时直接同步执行
        if (!m_allowAsyncTransactions)
            return DirectExecuteStmt(id, params);

        // Simple sql statement | 提交预处理请求到异步延迟线程
        m_threadBody->Delay(new SqlPreparedRequest(id.ID(), params));
    }

    return true;
}

// 同步直接执行预处理语句并自动接管释放参数对象 | Directly execute prepared statement synchronously and own parameter cleanup
bool Database::DirectExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params)
{
    MANGOS_ASSERT(params);
    std::unique_ptr<SqlStmtParameters> p(params);
    // execute statement | 加锁执行预处理语句
    SqlConnection::Lock _guard(getAsyncConnection());
    return _guard->ExecuteStmt(id.ID(), *params);
}

// 注册或查找 SQL 模板对应的语句 ID，返回可绑定参数的 SqlStatement 句柄 | Register or look up prepared statement ID for SQL template and return SqlStatement handle
SqlStatement Database::CreateStatement(SqlStatementID& index, const char* fmt)
{
    int nId = -1;
    // check if statement ID is initialized | 检查语句 ID 是否已初始化
    if (!index.initialized())
    {
        // convert to lower register | 构造 SQL 模板字符串
        std::string szFmt(fmt);
        // count input parameters | 统计占位符 '?' 数量
        int nParams = std::count(szFmt.begin(), szFmt.end(), '?');
        // find existing or add a new record in registry | 线程安全地查找或注册新语句 ID
        LOCK_GUARD _guard(m_stmtGuard);
        PreparedStmtRegistry::const_iterator iter = m_stmtRegistry.find(szFmt);
        if (iter == m_stmtRegistry.end())
        {
            nId = ++m_iStmtIndex;
            m_stmtRegistry[szFmt] = nId;
        }
        else
            nId = iter->second;

        // save initialized statement index info | 保存已初始化的语句索引与参数计数
        index.init(nId, nParams);
    }

    return SqlStatement(index, *this);
}

// 根据预处理语句 ID 反向查找其 SQL 模板字符串 | Look up SQL format string by prepared statement ID
std::string Database::GetStmtString(const int stmtId) const
{
    LOCK_GUARD _guard(m_stmtGuard);

    if (stmtId == -1 || stmtId > m_iStmtIndex)
        return std::string();

    PreparedStmtRegistry::const_iterator iter_last = m_stmtRegistry.end();
    for (PreparedStmtRegistry::const_iterator iter = m_stmtRegistry.begin(); iter != iter_last; ++iter)
    {
        if (iter->second == stmtId)
            return iter->first;
    }

    return std::string();
}
