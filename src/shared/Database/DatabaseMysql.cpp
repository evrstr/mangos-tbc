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

#ifndef DO_POSTGRESQL
#ifndef DO_SQLITE

#include "Util/Util.h"
#include "Policies/Singleton.h"
#include "Platform/Define.h"
#include "Multithreading/Threading.h"
#include "DatabaseEnv.h"
#include "Util/Timer.h"

size_t DatabaseMysql::db_count = 0;

// 在当前工作线程初始化 MySQL 线程环境 | Initialize MySQL thread-local state for calling thread
void DatabaseMysql::ThreadStart()
{
    mysql_thread_init();
}

// 清理当前工作线程的 MySQL 线程环境 | Clean up MySQL thread-local state for calling thread
void DatabaseMysql::ThreadEnd()
{
    mysql_thread_end();
}

DatabaseMysql::DatabaseMysql()
{
    // before first connection | 首个实例构造时初始化 MySQL 客户端库并检查线程安全性
    if (db_count++ == 0)
    {
        // Mysql Library Init | 初始化 MySQL 库
        mysql_library_init(-1, nullptr, nullptr);

        if (!mysql_thread_safe())
        {
            sLog.outError("FATAL ERROR: Used MySQL library isn't thread-safe.");
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }
    }
}

DatabaseMysql::~DatabaseMysql()
{
    StopServer();

    // Free Mysql library pointers for last ~DB | 最后一个实例析构时释放 MySQL 库全局资源
    if (--db_count == 0)
        mysql_library_end();
}

SqlConnection* DatabaseMysql::CreateConnection()
{
    return new MySQLConnection(*this);
}

MySQLConnection::~MySQLConnection()
{
    FreePreparedStatements();
    mysql_close(mMysql);
}

// 解析连接字符串并建立 MySQL 连接，设置 UTF-8 与自动提交 | Parse infoString, connect to MySQL, configure UTF-8 and autocommit
bool MySQLConnection::Initialize(const char* infoString)
{
    MYSQL* mysqlInit = mysql_init(nullptr);
    if (!mysqlInit)
    {
        sLog.outError("Could not initialize Mysql connection");
        return false;
    }

    Tokens tokens = StrSplit(infoString, ";");

    std::string host, port_or_socket, user, password, database;
    int port;
    char const* unix_socket;

    Tokens::iterator iter = tokens.begin();

    if (iter != tokens.end())
        host = *iter++;
    if (iter != tokens.end())
        port_or_socket = *iter++;
    if (iter != tokens.end())
        user = *iter++;
    if (iter != tokens.end())
        password = *iter++;
    if (iter != tokens.end())
        database = *iter++;

    mysql_options(mysqlInit, MYSQL_SET_CHARSET_NAME, "utf8");
#ifdef _WIN32
    if (host == ".")                                        // named pipe use option (Windows) | Windows 下使用命名管道连接
    {
        unsigned int opt = MYSQL_PROTOCOL_PIPE;
        mysql_options(mysqlInit, MYSQL_OPT_PROTOCOL, (char const*)&opt);
        port = 0;
        unix_socket = nullptr;
    }
    else                                                    // generic case | 常规 TCP 端口连接
    {
        port = atoi(port_or_socket.c_str());
        unix_socket = nullptr;
    }
#else
    if (host == ".")                                        // socket use option (Unix/Linux) | Unix/Linux 下使用本地套接字连接
    {
        unsigned int opt = MYSQL_PROTOCOL_SOCKET;
        mysql_options(mysqlInit, MYSQL_OPT_PROTOCOL, (char const*)&opt);
        host = "localhost";
        port = 0;
        unix_socket = port_or_socket.c_str();
    }
    else                                                    // generic case | 常规 TCP 端口连接
    {
        port = atoi(port_or_socket.c_str());
        unix_socket = nullptr;
    }
#endif

    mMysql = mysql_real_connect(mysqlInit, host.c_str(), user.c_str(),
                                password.c_str(), database.c_str(), port, unix_socket, 0);

    if (!mMysql)
    {
        sLog.outError("Could not connect to MySQL database at %s: %s\n",
                      host.c_str(), mysql_error(mysqlInit));
        mysql_close(mysqlInit);
        return false;
    }

    DETAIL_LOG("Connected to MySQL database %s@%s:%s/%s", user.c_str(), host.c_str(), port_or_socket.c_str(), database.c_str());
    sLog.outString("MySQL client library: %s", mysql_get_client_info());
    sLog.outString("MySQL server ver: %s ", mysql_get_server_info(mMysql));

    /*----------SET AUTOCOMMIT ON---------*/
    // It seems mysql 5.0.x have enabled this feature
    // by default. In crash case you can lose data!!!
    // So better to turn this off
    // ---
    // This is wrong since mangos use transactions,
    // autocommit is turned of during it.
    // Setting it to on makes atomic updates work
    // ---
    // LEAVE 'AUTOCOMMIT' MODE ALWAYS ENABLED!!!
    // W/O IT EVEN 'SELECT' QUERIES WOULD REQUIRE TO BE WRAPPED INTO 'START TRANSACTION'<>'COMMIT' CLAUSES!!!
    // 必须始终保持 AUTOCOMMIT 开启：事务期间会自动关闭它，若全局关闭则连 SELECT 都需要手动包裹在事务中
    if (!mysql_autocommit(mMysql, 1))
        DETAIL_LOG("AUTOCOMMIT SUCCESSFULLY SET TO 1");
    else
        DETAIL_LOG("AUTOCOMMIT NOT SET TO 1");
    /*-------------------------------------*/

    // set connection properties to UTF8 to properly handle locales for different | 将连接字符集设为 UTF8 以匹配核心发送的多语言数据
    // server configs - core sends data in UTF8, so MySQL must expect UTF8 too | 核心以 UTF8 发送数据，MySQL 连接也必须使用 UTF8
    Execute("SET NAMES `utf8`");
    Execute("SET CHARACTER SET `utf8`");

    return true;
}

// 执行 MySQL 查询并存储结果集与列元数据 | Execute MySQL query, store result set, and retrieve field metadata
bool MySQLConnection::_Query(const char* sql, MYSQL_RES** pResult, MYSQL_FIELD** pFields, uint64* pRowCount, uint32* pFieldCount)
{
    if (!mMysql)
        return false;

    uint32 _s = WorldTimer::getMSTime();

    if (mysql_query(mMysql, sql))
    {
        sLog.outErrorDb("SQL: %s", sql);
        sLog.outErrorDb("query ERROR: %s", mysql_error(mMysql));
        return false;
    }
    DEBUG_FILTER_LOG(LOG_FILTER_SQL_TEXT, "[%u ms] SQL: %s", WorldTimer::getMSTimeDiff(_s, WorldTimer::getMSTime()), sql);

    *pResult = mysql_store_result(mMysql);
    *pRowCount = mysql_affected_rows(mMysql);
    *pFieldCount = mysql_field_count(mMysql);

    if (!*pResult)
        return false;

    if (!*pRowCount)
    {
        mysql_free_result(*pResult);
        return false;
    }

    *pFields = mysql_fetch_fields(*pResult);
    return true;
}

// 执行查询并返回封装后的 QueryResultMysql（自动预取首行） | Execute query and return QueryResultMysql positioned at first row
std::unique_ptr<QueryResult> MySQLConnection::Query(const char* sql)
{
    MYSQL_RES* result = nullptr;
    MYSQL_FIELD* fields = nullptr;
    uint64 rowCount = 0;
    uint32 fieldCount = 0;

    if (!_Query(sql, &result, &fields, &rowCount, &fieldCount))
        return nullptr;

    auto queryResult = std::make_unique<QueryResultMysql>(result, fields, rowCount, fieldCount);

    queryResult->NextRow();
    return queryResult;
}

// 执行查询并返回支持列名索引的 QueryNamedResult | Execute query and return QueryNamedResult with column name mapping
QueryNamedResult* MySQLConnection::QueryNamed(const char* sql)
{
    MYSQL_RES* result = nullptr;
    MYSQL_FIELD* fields = nullptr;
    uint64 rowCount = 0;
    uint32 fieldCount = 0;

    if (!_Query(sql, &result, &fields, &rowCount, &fieldCount))
        return nullptr;

    QueryFieldNames names(fieldCount);
    for (uint32 i = 0; i < fieldCount; ++i)
        names[i] = fields[i].name;

    QueryResultMysql* queryResult = new QueryResultMysql(result, fields, rowCount, fieldCount);

    queryResult->NextRow();
    return new QueryNamedResult(queryResult, names);
}

// 执行无结果集的 SQL 语句 | Execute non-query SQL statement
bool MySQLConnection::Execute(const char* sql)
{
    if (!mMysql)
        return false;

    {
        uint32 _s = WorldTimer::getMSTime();

        if (mysql_query(mMysql, sql))
        {
            sLog.outErrorDb("SQL: %s", sql);
            sLog.outErrorDb("SQL ERROR: %s", mysql_error(mMysql));
            return false;
        }
        DEBUG_FILTER_LOG(LOG_FILTER_SQL_TEXT, "[%u ms] SQL: %s", WorldTimer::getMSTimeDiff(_s, WorldTimer::getMSTime()), sql);
        // end guarded block | 计时与执行作用域结束
    }

    return true;
}

// 发送事务控制语句（START TRANSACTION / COMMIT / ROLLBACK） | Execute transaction control SQL command
bool MySQLConnection::_TransactionCmd(const char* sql)
{
    if (mysql_query(mMysql, sql))
    {
        sLog.outError("SQL: %s", sql);
        sLog.outError("SQL ERROR: %s", mysql_error(mMysql));
        return false;
    }
    DEBUG_FILTER_LOG(LOG_FILTER_SQL_TEXT, "SQL: %s", sql);
    return true;
}

bool MySQLConnection::BeginTransaction()
{
    return _TransactionCmd("START TRANSACTION");
}

bool MySQLConnection::CommitTransaction()
{
    return _TransactionCmd("COMMIT");
}

bool MySQLConnection::RollbackTransaction()
{
    return _TransactionCmd("ROLLBACK");
}

unsigned long MySQLConnection::escape_string(char* to, const char* from, unsigned long length)
{
    if (!mMysql || !to || !from || !length)
        return 0;

    return (mysql_real_escape_string(mMysql, to, from, length));
}

//////////////////////////////////////////////////////////////////////////
SqlPreparedStatement* MySQLConnection::CreateStatement(const std::string& fmt)
{
    return new MySqlPreparedStatement(fmt, *this, mMysql);
}

//////////////////////////////////////////////////////////////////////////
MySqlPreparedStatement::MySqlPreparedStatement(const std::string& fmt, SqlConnection& conn, MYSQL* mysql) : SqlPreparedStatement(fmt, conn),
    m_pMySQLConn(mysql), m_stmt(nullptr), m_pInputArgs(nullptr), m_pResult(nullptr), m_pResultMetadata(nullptr)
{
}

MySqlPreparedStatement::~MySqlPreparedStatement()
{
    RemoveBinds();
}

// 初始化并预处理 MySQL 原生语句句柄与输入参数绑定数组 | Initialize and prepare MySQL native statement handle and input bind buffers
bool MySqlPreparedStatement::prepare()
{
    if (isPrepared())
        return true;

    // remove old binds | 清除旧绑定资源
    RemoveBinds();

    // create statement object | 分配 MYSQL_STMT 句柄
    m_stmt = mysql_stmt_init(m_pMySQLConn);
    if (!m_stmt)
    {
        sLog.outError("SQL: mysql_stmt_init() failed ");
        return false;
    }

    // prepare statement | 服务端预处理 SQL 模板
    if (mysql_stmt_prepare(m_stmt, m_szFmt.c_str(), m_szFmt.length()))
    {
        sLog.outError("SQL: mysql_stmt_prepare() failed for '%s'", m_szFmt.c_str());
        sLog.outError("SQL ERROR: %s", mysql_stmt_error(m_stmt));
        return false;
    }

    /* Get the parameter count from the statement | 获取占位符参数数量 */
    m_nParams = mysql_stmt_param_count(m_stmt);

    /* Fetch result set meta information | 获取结果集元数据（若为查询语句） */
    m_pResultMetadata = mysql_stmt_result_metadata(m_stmt);
    // if we do not have result metadata | 若 SELECT 语句缺少元数据则报错
    if (!m_pResultMetadata && strnicmp(m_szFmt.c_str(), "select", 6) == 0)
    {
        sLog.outError("SQL: no meta information for '%s'", m_szFmt.c_str());
        sLog.outError("SQL ERROR: %s", mysql_stmt_error(m_stmt));
        return false;
    }

    // bind input buffers | 为输入参数分配 MYSQL_BIND 数组
    if (m_nParams)
    {
        m_pInputArgs = new MYSQL_BIND[m_nParams];
        memset(m_pInputArgs, 0, sizeof(MYSQL_BIND) * m_nParams);
    }

    // check if we have a statement which returns result sets | 判断该语句是否返回结果集
    if (m_pResultMetadata)
    {
        // our statement is query | 标记为查询语句并记录结果列数
        m_bIsQuery = true;
        /* Get total columns in the query | 获取查询结果总列数 */
        m_nColumns = mysql_num_fields(m_pResultMetadata);

        // bind output buffers | 输出缓冲区绑定（暂未启用）
    }

    m_bPrepared = true;
    return true;
}

// 将参数容器中的所有输入参数绑定到 MySQL 语句 | Bind all parameters from SqlStmtParameters holder to MySQL statement
void MySqlPreparedStatement::bind(const SqlStmtParameters& holder)
{
    if (!isPrepared())
    {
        MANGOS_ASSERT(false);
        return;
    }

    // finalize adding params | 若无需参数则直接返回
    if (!m_pInputArgs)
        return;

    // verify if we bound all needed input parameters | 校验参数个数是否与预处理模板一致
    if (m_nParams != holder.boundParams())
    {
        MANGOS_ASSERT(false);
        return;
    }

    unsigned int nIndex = 0;
    SqlStmtParameters::ParameterContainer const& _args = holder.params();

    SqlStmtParameters::ParameterContainer::const_iterator iter_last = _args.end();
    for (SqlStmtParameters::ParameterContainer::const_iterator iter = _args.begin(); iter != iter_last; ++iter)
    {
        // bind parameter | 逐个填充 MYSQL_BIND 结构
        addParam(nIndex++, (*iter));
    }

    // bind input arguments | 调用 C API 绑定输入参数
    if (mysql_stmt_bind_param(m_stmt, m_pInputArgs))
    {
        sLog.outError("SQL ERROR: mysql_stmt_bind_param() failed\n");
        sLog.outError("SQL ERROR: %s", mysql_stmt_error(m_stmt));
    }
}

// 填充单个输入参数的 MYSQL_BIND 结构 | Populate MYSQL_BIND struct for a single input parameter
void MySqlPreparedStatement::addParam(unsigned int nIndex, const SqlStmtFieldData& data)
{
    MANGOS_ASSERT(m_pInputArgs);
    MANGOS_ASSERT(nIndex < m_nParams);

    MYSQL_BIND& pData = m_pInputArgs[nIndex];

    bool bUnsigned = 0;
    enum_field_types dataType = ToMySQLType(data, bUnsigned);

    // setup MYSQL_BIND structure | 配置类型、无符号标识、数据指针与字符串长度
    pData.buffer_type = dataType;
    pData.is_unsigned = bUnsigned;
    pData.buffer = data.buff();
    pData.length = nullptr;
    pData.buffer_length = data.type() == FIELD_STRING ? data.size() : 0;
}

// 释放所有绑定数组、元数据及预处理语句句柄 | Free bind buffers, result metadata, and close MYSQL_STMT
void MySqlPreparedStatement::RemoveBinds()
{
    if (!m_stmt)
        return;

    delete[] m_pInputArgs;
    delete[] m_pResult;

    mysql_free_result(m_pResultMetadata);
    mysql_stmt_close(m_stmt);

    m_stmt = nullptr;
    m_pResultMetadata = nullptr;
    m_pResult = nullptr;
    m_pInputArgs = nullptr;

    m_bPrepared = false;
}

bool MySqlPreparedStatement::execute()
{
    if (!isPrepared())
        return false;

    if (mysql_stmt_execute(m_stmt))
    {
        sLog.outError("SQL: cannot execute '%s'", m_szFmt.c_str());
        sLog.outError("SQL ERROR: %s", mysql_stmt_error(m_stmt));
        return false;
    }

    return true;
}

// 将通用预处理字段类型映射为 MySQL C API 枚举类型 | Map generic prepared field type to MySQL C API field type
enum_field_types MySqlPreparedStatement::ToMySQLType(const SqlStmtFieldData& data, bool& bUnsigned)
{
    bUnsigned = 0;
    enum_field_types dataType = MYSQL_TYPE_NULL;

    switch (data.type())
    {
        case FIELD_NONE:    dataType = MYSQL_TYPE_NULL;                     break;
        // MySQL does not support MYSQL_TYPE_BIT as input type | MySQL 不支持将 MYSQL_TYPE_BIT 作为输入参数类型，改用 TINYINT
        case FIELD_BOOL:    // dataType = MYSQL_TYPE_BIT;      bUnsigned = 1;  break;
        case FIELD_UI8:     dataType = MYSQL_TYPE_TINY;     bUnsigned = 1;  break;
        case FIELD_I8:      dataType = MYSQL_TYPE_TINY;                     break;
        case FIELD_I16:     dataType = MYSQL_TYPE_SHORT;                    break;
        case FIELD_UI16:    dataType = MYSQL_TYPE_SHORT;    bUnsigned = 1;  break;
        case FIELD_I32:     dataType = MYSQL_TYPE_LONG;                     break;
        case FIELD_UI32:    dataType = MYSQL_TYPE_LONG;     bUnsigned = 1;  break;
        case FIELD_I64:     dataType = MYSQL_TYPE_LONGLONG;                 break;
        case FIELD_UI64:    dataType = MYSQL_TYPE_LONGLONG; bUnsigned = 1;  break;
        case FIELD_FLOAT:   dataType = MYSQL_TYPE_FLOAT;                    break;
        case FIELD_DOUBLE:  dataType = MYSQL_TYPE_DOUBLE;                   break;
        case FIELD_STRING:  dataType = MYSQL_TYPE_STRING;                   break;
    }

    return dataType;
}
#endif
#endif
