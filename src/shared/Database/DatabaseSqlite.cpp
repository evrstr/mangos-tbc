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

#include "QueryResultSqlite.h"
#include <cstdint>
#include <cstring>
#ifdef DO_SQLITE
#include <sqlite3.h>

#include "Util/Util.h"
#include "Policies/Singleton.h"
#include "Platform/Define.h"
#include "Multithreading/Threading.h"
#include "DatabaseEnv.h"
#include "Util/Timer.h"
#include "DatabaseSqlite.h"

SqlConnection* DatabaseSqlite::CreateConnection()
{
    return new SQLiteConnection(*this);
}

// 析构时释放预处理语句、执行 PRAGMA optimize 并关闭句柄 | Free prepared statements, run PRAGMA optimize, and close SQLite connection
SQLiteConnection::~SQLiteConnection()
{
    FreePreparedStatements();
    sqlite3_exec(mSqlite, "PRAGMA optimize;", 0, 0, 0);
    sqlite3_close(mSqlite);
}

// 打开 SQLite 数据库并配置 WAL 模式、忙等待超时与 UTF-8 编码 | Open SQLite database and configure WAL mode, busy timeout, and UTF-8 encoding
bool SQLiteConnection::Initialize(const char* infoString)
{
    if (sqlite3_open(infoString, &mSqlite) != SQLITE_OK)
    {
        sLog.outError("Could not open SQLite database");
        return false;
    }
    // realmd and mangosd share the login database, and each process opens several connections: a statement, the pragmas | realmd 与 mangosd 共享登录库且各开多个连接：
    // below included, waits this long for a lock held by another connection before it fails with SQLITE_BUSY. | 包含下方 PRAGMA 在内，等待其他连接持锁的最长时间（毫秒），超时才返回 SQLITE_BUSY。
    sqlite3_busy_timeout(mSqlite, 5000);
    sqlite3_exec(mSqlite, "PRAGMA journal_mode=WAL;", 0, 0, 0);
    sqlite3_exec(mSqlite, "PRAGMA synchronous=1;", 0, 0, 0);
    sqlite3_exec(mSqlite, "PRAGMA secure_delete=off;", 0, 0, 0);
    sqlite3_exec(mSqlite, "PRAGMA encoding='UTF-8';", 0, 0, 0);

    Tokens tokens = StrSplit(infoString, ";");

    DETAIL_LOG("Connected to SQLite database at %s", infoString);

    return true;
}

// 编译 SQLite 查询语句 | Prepare SQLite query statement
bool SQLiteConnection::_Query(const char* sql, sqlite3_stmt** pStmt)
{
    if (!mSqlite)
        return false;

    uint32 _s = WorldTimer::getMSTime();
    int result;
    result = sqlite3_prepare_v2(mSqlite, sql, -1, pStmt, NULL);
    if (result != SQLITE_OK) {
        sLog.outErrorDb("SQL: %s", sql);
        sLog.outErrorDb("query ERROR: %s", sqlite3_errmsg(mSqlite));
        return false;
    }

    return true;
}

// 执行查询并返回首行就绪的 QueryResultSqlite（空结果返回 nullptr） | Execute query and return QueryResultSqlite at first row, or nullptr if empty
std::unique_ptr<QueryResult> SQLiteConnection::Query(const char* sql)
{
    sqlite3_stmt* stmt = nullptr;
    if (!_Query(sql, &stmt))
        return nullptr;

    auto queryResult = std::make_unique<QueryResultSqlite>(stmt);

    if (queryResult->NextRow())
        return queryResult;
    return nullptr;
}

// 执行查询并返回带列名的 QueryNamedResult | Execute query and return QueryNamedResult with column names
QueryNamedResult* SQLiteConnection::QueryNamed(const char* sql)
{
    uint64 rowCount = 0;
    uint32 fieldCount = 0;

    sqlite3_stmt* stmt = nullptr;

    if (!_Query(sql, &stmt))
        return nullptr;

    fieldCount = sqlite3_column_count(stmt);

    QueryFieldNames names(fieldCount);
    for (uint32 i = 0; i < fieldCount; ++i)
        names[i] = sqlite3_column_name(stmt, i);

    QueryResultSqlite* queryResult = new QueryResultSqlite(stmt);

    queryResult->NextRow();
    return new QueryNamedResult(queryResult, names);
}

// 编译、单步执行并销毁非查询 SQL 语句 | Prepare, step, and finalize non-query SQL statement
bool SQLiteConnection::Execute(const char* sql)
{
    if (!mSqlite)
        return false;

    {
        uint32 _s = WorldTimer::getMSTime();
        sqlite3_stmt* stmt = nullptr;

        int result;
        result = sqlite3_prepare_v2(mSqlite, sql, -1, &stmt, nullptr);
        if (result != SQLITE_OK)
        {
            sLog.outErrorDb("SQL: %s", sql);
            sLog.outErrorDb("SQL ERROR: %s", sqlite3_errmsg(mSqlite));
            return false;
        }
        result = sqlite3_step(stmt);
        if (result != SQLITE_DONE && result != SQLITE_OK)
        {
            sLog.outErrorDb("SQL: %s", sql);
            sLog.outErrorDb("SQL ERROR: %s", sqlite3_errmsg(mSqlite));
            // A statement that is not finalized keeps the implicit transaction of the connection open: the writes that | 未 finalize 的语句会使连接隐式事务保持开启：
            // follow on this connection would never be committed and would keep the database locked. | 导致该连接后续写入无法提交并持续锁死数据库。
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        DEBUG_FILTER_LOG(LOG_FILTER_SQL_TEXT, "[%u ms] SQL: %s", WorldTimer::getMSTimeDiff(_s, WorldTimer::getMSTime()), sql);
        // end guarded block | 计时作用域结束
    }

    return true;
}

// 执行 SQLite 事务控制命令 | Execute SQLite transaction control command
bool SQLiteConnection::_TransactionCmd(const char* sql)
{
    sqlite3_stmt* stmt = nullptr;

    int result;
    result = sqlite3_prepare_v2(mSqlite, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK)
    {
        sLog.outErrorDb("SQL: %s", sql);
        sLog.outErrorDb("SQL ERROR: %s", sqlite3_errmsg(mSqlite));
        return false;
    }
    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE && result != SQLITE_OK)
    {
        sLog.outErrorDb("SQL: %s", sql);
        sLog.outErrorDb("SQL ERROR: %s", sqlite3_errmsg(mSqlite));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    DEBUG_FILTER_LOG(LOG_FILTER_SQL_TEXT, "SQL: %s", sql);
    return true;
}

bool SQLiteConnection::BeginTransaction()
{
    return _TransactionCmd("BEGIN");
}

bool SQLiteConnection::CommitTransaction()
{
    return _TransactionCmd("COMMIT");
}

bool SQLiteConnection::RollbackTransaction()
{
    return _TransactionCmd("ROLLBACK");
}

// 转义 SQLite 字符串中的单引号 | Escape single quotes in SQLite SQL string
unsigned long SQLiteConnection::escape_string(char* to, const char* from, unsigned long length)
{
    if (!mSqlite || !to || !from || !length)
        return 0;
    std::string newFrom(from);
    std::string newTo;

    for (const char& c : newFrom)
    {
        switch (c)
        {
            [[unlikely]] case '\'': newTo += "''"; break;
            [[unlikely]] case '\\': newTo += "\\"; break;
            [[likely]]   default:   newTo += c;    break;
        }
    }
    strcpy(to, newTo.c_str());

    return newTo.length();
}

//////////////////////////////////////////////////////////////////////////
SqlPreparedStatement* SQLiteConnection::CreateStatement(const std::string& fmt)
{
    return new SqlitePreparedStatement(fmt, *this, mSqlite);
}

//////////////////////////////////////////////////////////////////////////
SqlitePreparedStatement::SqlitePreparedStatement(const std::string& fmt, SqlConnection& conn, sqlite3* mysql) : SqlPreparedStatement(fmt, conn),
    m_pSqliteConn(mysql), m_stmt(nullptr), m_pResult(nullptr)
{
}

SqlitePreparedStatement::~SqlitePreparedStatement()
{
    RemoveBinds();
}

// 编译 SQLite 预处理语句并统计参数和结果列数 | Prepare SQLite statement and inspect parameter and column counts
bool SqlitePreparedStatement::prepare()
{
    if (isPrepared())
        return true;

    // remove old binds | 清理旧语句句柄
    RemoveBinds();

    if (!m_stmt)
        m_stmt = new(sqlite3_stmt*);

    // Create statement object | 编译预处理语句对象
    int result = sqlite3_prepare_v2(m_pSqliteConn, m_szFmt.c_str(), m_szFmt.length(), m_stmt, nullptr);
    if (result != SQLITE_OK) {
        sLog.outError("SQL: sqlite3_prepare_v2() failed: %s", sqlite3_errmsg(m_pSqliteConn));
        return false;
    }
    // Get the parameter count from the statement | 获取占位符参数个数
    m_nParams = sqlite3_bind_parameter_count(*m_stmt);

    // Check if we have a statement which returns result sets | 检查是否返回结果集
    if (sqlite3_stmt_readonly(*m_stmt) == 0) {
        // Our statement is a query | 标记为查询并记录列数
        m_bIsQuery = true;
        m_nColumns = sqlite3_column_count(*m_stmt);  // Get the number of columns in the result set | 获取结果集列数
    }

    m_bPrepared = true;
    return true;
}

// 绑定参数容器中的所有参数到 sqlite3_stmt | Bind all parameters from SqlStmtParameters to sqlite3_stmt
void SqlitePreparedStatement::bind(const SqlStmtParameters& holder)
{
    if (!isPrepared())
    {
        MANGOS_ASSERT(!isPrepared());
        return;
    }

    // verify if we bound all needed input parameters | 校验参数个数是否匹配
    if (m_nParams != holder.boundParams())
    {
        MANGOS_ASSERT(m_nParams != holder.boundParams());
        return;
    }

    unsigned int nIndex = 0; // SQLite uses 1-based index for parameter binding | SQLite 参数绑定下标从 1 开始（addParam 内部加 1）
    SqlStmtParameters::ParameterContainer const& _args = holder.params();

    for (const auto& param : _args)
    {
        // Bind parameter | 逐个绑定参数
        addParam(nIndex, param);
        nIndex++;
    }
}

// 按 0-based 索引将单个 SqlStmtFieldData 绑定到 SQLite（1-based） | Bind single SqlStmtFieldData at 0-based index (1-based in SQLite)
void SqlitePreparedStatement::addParam(unsigned int nIndex, const SqlStmtFieldData& data)
{
    MANGOS_ASSERT(nIndex < m_nParams);

    // SQLite uses different types and structures for binding parameters | 根据字段类型调用对应的 sqlite3_bind_* 接口
    int result = SQLITE_OK;

    switch (data.type()) {
        case FIELD_BOOL:
            result = sqlite3_bind_int(*m_stmt, nIndex + 1, data.toBool());
            break;
        case FIELD_UI8:
            result = sqlite3_bind_int(*m_stmt, nIndex + 1, data.toUint8());
            break;
        case FIELD_UI16:
            result = sqlite3_bind_int(*m_stmt, nIndex + 1, data.toUint16());
            break;
        case FIELD_UI32:
            result = sqlite3_bind_int(*m_stmt, nIndex + 1, data.toUint32());
            break;
        case FIELD_UI64:
            result = sqlite3_bind_int64(*m_stmt, nIndex + 1, data.toUint64());
            break;
        case FIELD_I8:
            result = sqlite3_bind_int(*m_stmt, nIndex + 1, data.toInt8());
            break;
        case FIELD_I16:
            result = sqlite3_bind_int(*m_stmt, nIndex + 1, data.toInt16());
            break;
        case FIELD_I32:
            result = sqlite3_bind_int(*m_stmt, nIndex + 1, data.toInt32());
            break;
        case FIELD_I64:
            result = sqlite3_bind_int64(*m_stmt, nIndex + 1, data.toInt64());
            break;
        case FIELD_FLOAT:
            result = sqlite3_bind_double(*m_stmt, nIndex + 1, data.toFloat());
            break;
        case FIELD_DOUBLE:
            result = sqlite3_bind_double(*m_stmt, nIndex + 1, data.toDouble());
            break;
        case FIELD_STRING:
            result = sqlite3_bind_text(*m_stmt, nIndex + 1, data.toStr(), -1, SQLITE_STATIC);
            break;
        case FIELD_NONE:
            result = sqlite3_bind_null(*m_stmt, nIndex + 1);
            break;
        // Handle other data types as needed | 必要时可扩展支持其他数据类型

        default:
            MANGOS_ASSERT(false && "Unsupported parameter type");
    }

    if (result != SQLITE_OK) {
        // Handle the error, e.g., print an error message | 绑定失败时输出错误日志
        fprintf(stderr, "Error binding parameter at index %d: %s\n", nIndex, sqlite3_errmsg(m_pSqliteConn));
    }
}

// 释放 sqlite3_stmt 句柄并重置预处理标志 | Finalize sqlite3_stmt handle and clear prepared flag
void SqlitePreparedStatement::RemoveBinds()
{
    if (!m_stmt)
        return;

    // Finalize the prepared statement | 销毁已编译的预处理语句
    sqlite3_finalize(*m_stmt);
    *m_stmt = nullptr;

    m_bPrepared = false;
}

// 执行已绑定的预处理语句并在结束后重置绑定以便复用 | Execute bound statement and reset/clear bindings for reuse
bool SqlitePreparedStatement::execute()
{
    if (!isPrepared())
        return false;

    int result = sqlite3_step(*m_stmt);

    if (result != SQLITE_DONE)
    {
        sLog.outErrorDb("SQL: cannot execute '%s'", m_szFmt.c_str());
        sLog.outErrorDb("SQL ERROR: %s", sqlite3_errmsg(m_pSqliteConn));
        sqlite3_reset(*m_stmt);
        sqlite3_clear_bindings(*m_stmt);
        return false;
    }

    // Reset the prepared statement to be executed again if needed | 重置语句状态与参数绑定以供下次执行
    sqlite3_reset(*m_stmt);
    sqlite3_clear_bindings(*m_stmt);

    return true;
}
#endif
