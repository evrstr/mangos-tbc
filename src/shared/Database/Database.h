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

#ifndef DATABASE_H
#define DATABASE_H

#include "Common.h"
#include "Multithreading/Threading.h"
#include "Database/SqlDelayThread.h"
#include "Policies/ThreadingModel.h"
#include "SqlPreparedStatement.h"
#include "QueryResult.h"

#include <boost/thread/tss.hpp>
#include <atomic>
#include <memory>

class SqlTransaction;
class SqlResultQueue;
class SqlQueryHolder;
class SqlStmtParameters;
class SqlParamBinder;
class Database;

#define MAX_QUERY_LEN   (32*1024)

// 数据库单连接抽象基类，封装同步查询、执行、事务及预处理语句 | Abstract base class for a single database connection supporting queries, execution, transactions, and prepared statements
class SqlConnection
{
    public:
        virtual ~SqlConnection() {}

        // method for initializing DB connection | 初始化数据库连接
        virtual bool Initialize(const char* infoString) = 0;
        // public methods for making queries | 执行查询的公共接口
        virtual std::unique_ptr<QueryResult> Query(const char* sql) = 0;
        virtual QueryNamedResult* QueryNamed(const char* sql) = 0;

        // public methods for making requests | 执行非查询 SQL 请求的公共接口
        virtual bool Execute(const char* sql) = 0;

        // escape string generation | 生成转义后的安全 SQL 字符串
        virtual unsigned long escape_string(char* to, const char* from, unsigned long length) { strncpy(to, from, length); return length; }

        // nothing do if DB not support transactions | 若数据库不支持事务则默认空操作返回 true
        virtual bool BeginTransaction() { return true; }
        virtual bool CommitTransaction() { return true; }
        // can't rollback without transaction support | 无事务支持时无法回滚
        virtual bool RollbackTransaction() { return true; }

        // methods to work with prepared statements | 预处理语句执行接口
        bool ExecuteStmt(int nIndex, const SqlStmtParameters& id);

        // SqlConnection object lock | SqlConnection 互斥锁守卫（RAII）
        class Lock
        {
            public:
                Lock(SqlConnection* conn) : m_pConn(conn) { m_pConn->m_mutex.lock(); }
                ~Lock() { m_pConn->m_mutex.unlock(); }

                SqlConnection* operator->() const { return m_pConn; }

            private:
                SqlConnection* const m_pConn;
        };

        // get DB object | 获取所属 Database 实例
        Database& DB() const { return m_db; }

    protected:
        SqlConnection(Database& db) : m_db(db) {}

        virtual SqlPreparedStatement* CreateStatement(const std::string& fmt);
        // allocate prepared statement and return statement ID | 按索引获取或创建并缓存预处理语句对象
        SqlPreparedStatement* GetStmt(uint32 nIndex);

        Database& m_db;

        // free prepared statements objects | 释放连接持有的全部预处理语句对象
        void FreePreparedStatements();

    private:
        std::recursive_mutex m_mutex;

        typedef std::vector<SqlPreparedStatement* > StmtHolder;
        StmtHolder m_holder;
};

// 数据库核心管理类，提供同步连接池、异步工作线程、事务与预处理语句支持 | Core database manager providing sync connection pool, async worker thread, transactions, and prepared statements
class Database
{
    public:
        virtual ~Database();

        // 初始化数据库连接池与异步连接 | Initialize sync connection pool and async connection
        virtual bool Initialize(const char* infoString, int nConns = 1);
        // start worker thread for async DB request execution | 启动用于异步执行 DB 请求的工作线程
        virtual void InitDelayThread();
        // stop worker thread | 停止异步工作线程并等待队列刷新完成
        virtual void HaltDelayThread();

        /// Synchronous DB queries | 同步数据库查询接口
        inline std::unique_ptr<QueryResult> Query(const char* sql)
        {
            SqlConnection::Lock guard(getQueryConnection());
            return guard->Query(sql);
        }

        inline QueryNamedResult* QueryNamed(const char* sql)
        {
            SqlConnection::Lock guard(getQueryConnection());
            return guard->QueryNamed(sql);
        }

        // 格式化同步查询 | Formatted synchronous query
        std::unique_ptr<QueryResult> PQuery(const char* format, ...) ATTR_PRINTF(2, 3);
        QueryNamedResult* PQueryNamed(const char* format, ...) ATTR_PRINTF(2, 3);

        // 在异步连接上直接同步执行 SQL | Directly execute SQL synchronously on the async connection
        bool DirectExecute(const char* sql) const
        {
            if (!m_pAsyncConn)
                return false;

            SqlConnection::Lock guard(m_pAsyncConn);
            return guard->Execute(sql);
        }

        bool DirectPExecute(const char* format, ...) ATTR_PRINTF(2, 3);

        /// Async queries and query holders, implemented in DatabaseImpl.h | 异步查询与查询持有器接口（实现在 DatabaseImpl.h 中）

        // Query | 异步原始 SQL 查询
        /**
         * Callable needs to be a callable object (function object, pointer to function, reference
         * to function, pointer to member function, etc) that takes a std::unique_ptr<QueryResult>
         * or a pointer to QueryResult as an argument and returns void. The caller can use std::bind
         * to create such a callable object and bind any other required arguments for the actual callback.
         * Callable 需为可调用对象（函数对象、函数指针、成员函数绑定等），接收 std::unique_ptr<QueryResult> 或 QueryResult* 参数并返回 void。
         */
        template <typename Callable>
        bool AsyncQuery(Callable callback, const char* sql);

        // PQuery | 异步格式化 SQL 查询
        /**
         * Callable needs to be a callable object (function object, pointer to function, reference
         * to function, pointer to member function, etc) that takes a std::unique_ptr<QueryResult>
         * or a pointer to QueryResult as an argument and returns void. The caller can use std::bind
         * to create such a callable object and bind any other required arguments for the actual callback.
         * Callable 需为可调用对象，接收查询结果并返回 void。
         */
        template <typename Callable>
        bool AsyncPQuery(Callable callback, const char* format, ...) ATTR_PRINTF(3, 4);

        // PQuery / member | 异步格式化查询（成员函数回调重载）
        template<class Class>
        bool AsyncPQuery(Class* object, void (Class::*method)(QueryResult*), const char* format, ...) ATTR_PRINTF(4, 5);
        template<class Class, typename ParamType1>
        bool AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1), ParamType1 param1, const char* format, ...) ATTR_PRINTF(5, 6);
        template<class Class, typename ParamType1, typename ParamType2>
        bool AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* format, ...) ATTR_PRINTF(6, 7);
        template<class Class, typename ParamType1, typename ParamType2, typename ParamType3>
        bool AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* format, ...) ATTR_PRINTF(7, 8);
        // PQuery / static | 异步格式化查询（静态/自由函数回调重载）
        template<typename ParamType1>
        bool AsyncPQuery(void (*method)(QueryResult*, ParamType1), ParamType1 param1, const char* format, ...) ATTR_PRINTF(4, 5);
        template<typename ParamType1, typename ParamType2>
        bool AsyncPQuery(void (*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* format, ...) ATTR_PRINTF(5, 6);
        template<typename ParamType1, typename ParamType2, typename ParamType3>
        bool AsyncPQuery(void (*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* format, ...) ATTR_PRINTF(6, 7);
        template<class Class>
        // QueryHolder | 异步批量查询持有器延迟执行
        bool DelayQueryHolder(Class* object, void (Class::*method)(QueryResult*, SqlQueryHolder*), SqlQueryHolder* holder);
        template<class Class, typename ParamType1>
        bool DelayQueryHolder(Class* object, void (Class::*method)(QueryResult*, SqlQueryHolder*, ParamType1), SqlQueryHolder* holder, ParamType1 param1);

        // 异步或事务内排队执行 SQL 语句 | Execute SQL asynchronously or queue into active thread transaction
        bool Execute(const char* sql);
        bool PExecute(const char* format, ...) ATTR_PRINTF(2, 3);

        // Writes SQL commands to a LOG file (see mangosd.conf "LogSQL") | 将 SQL 命令记录到日志文件并执行（见配置项 LogSQL）
        bool PExecuteLog(const char* format, ...) ATTR_PRINTF(2, 3);

        // 开启、提交与回滚线程局部事务 | Begin, commit, and rollback thread-local transaction
        bool BeginTransaction();
        bool CommitTransaction();
        bool RollbackTransaction();
        // for sync transaction execution | 同步直接执行并提交当前线程事务
        bool CommitTransactionDirect();

        // PREPARED STATEMENT API | 预处理语句 API

        // allocate index for prepared statement with SQL request 'fmt' | 为 SQL 模板 'fmt' 分配/获取预处理语句索引
        SqlStatement CreateStatement(SqlStatementID& index, const char* fmt);
        // get prepared statement format string | 根据语句 ID 获取对应的 SQL 模板字符串
        std::string GetStmtString(const int stmtId) const;

        operator bool () const { return !m_pQueryConnections.empty() && m_pAsyncConn; }

        // escape string generation | 原地转义字符串中的 SQL 特殊字符
        void escape_string(std::string& str);

        // must be called before first query in thread (one time for thread using one from existing Database objects) | 线程首次执行查询前必须调用一次（每个使用 DB 的线程）
        virtual void ThreadStart();
        // must be called before finish thread run (one time for thread using one from existing Database objects) | 线程结束运行前必须调用一次以清理线程资源
        virtual void ThreadEnd();

        // set database-wide result queue. also we should use object-bases and not thread-based result queues | 处理数据库异步结果回调队列（主线程轮询调用）
        void ProcessResultQueue();

        // 检查数据库表版本字段是否匹配核心要求 | Verify required DB version field exists in version table
        bool CheckRequiredField(char const* table_name, char const* required_name);
        uint32 GetPingIntervall() const { return m_pingIntervallms; }

        // function to ping database connections | 向所有数据库连接发送心跳检测（SELECT 1）
        void Ping();

        // set this to allow async transactions | 启用异步事务执行（应在服务器启动完成后显式调用）
        // you should call it explicitly after your server successfully started up | 应在服务成功启动后再开启
        // NO ASYNC TRANSACTIONS DURING SERVER STARTUP - ONLY DURING RUNTIME!!! | 严禁在服务器启动阶段启用异步事务，仅限运行时使用！
        void AllowAsyncTransactions() { m_allowAsyncTransactions = true; }

    protected:
        Database() :
            m_nQueryConnPoolSize(1), m_pAsyncConn(nullptr), m_pResultQueue(nullptr),
            m_threadBody(nullptr), m_delayThread(nullptr), m_allowAsyncTransactions(false),
            m_iStmtIndex(-1), m_logSQL(false), m_pingIntervallms(0)
        {
            m_nQueryCounter = -1;
        }

        // 停止延迟线程并释放所有连接资源 | Halt delay thread and release all connection resources
        void StopServer();

        // factory method to create SqlConnection objects | 创建具体数据库连接的工厂方法
        virtual SqlConnection* CreateConnection() = 0;
        // factory method to create SqlDelayThread objects | 创建延迟执行线程对象的工厂方法
        virtual SqlDelayThread* CreateDelayThread();

        // per-thread based storage for SqlTransaction object initialization - no locking is required | 线程局部存储的当前事务对象（无需加锁）
        boost::thread_specific_ptr<SqlTransaction> m_currentTransaction;

        ///< DB connections | 数据库连接相关成员

        // round-robin connection selection | 轮询方式从同步查询连接池选取连接
        SqlConnection* getQueryConnection();
        // for now return one single connection for async requests | 返回专用于异步请求的单一连接
        SqlConnection* getAsyncConnection() const { return m_pAsyncConn; }

        friend class SqlStatement;
        // PREPARED STATEMENT API | 预处理语句内部执行接口
        // query function for prepared statements | 异步/事务或直接执行预处理语句
        bool ExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params);
        bool DirectExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params);

        // connection helper counters | 连接池计数与轮询计数器
        int m_nQueryConnPoolSize;                           // current size of query connection pool | 当前同步查询连接池大小
        std::atomic_long m_nQueryCounter;  // counter for connection selection | 连接轮询选择原子计数器

        // lets use pool of connections for sync queries | 同步查询连接池容器
        typedef std::vector< SqlConnection* > SqlConnectionContainer;
        SqlConnectionContainer m_pQueryConnections;

        // only one single DB connection for transactions | 专用于事务与异步操作的单一数据库连接
        SqlConnection* m_pAsyncConn;

        SqlResultQueue*     m_pResultQueue;                 ///< Transaction queues from diff. threads | 来自各线程的异步查询结果回调队列
        SqlDelayThread*     m_threadBody;                   ///< Pointer to delay sql executer (owned by m_delayThread) | 延迟 SQL 执行器对象指针（由 m_delayThread 持有）
        MaNGOS::Thread*     m_delayThread;                  ///< Pointer to executer thread | 异步执行线程对象指针

        std::atomic<bool> m_allowAsyncTransactions;         ///< flag which specifies if async transactions are enabled | 标记是否允许异步执行事务/写操作

        // PREPARED STATEMENT REGISTRY | 预处理语句注册表
        typedef std::mutex LOCK_TYPE;
        typedef std::lock_guard<LOCK_TYPE> LOCK_GUARD;

        mutable LOCK_TYPE m_stmtGuard;

        typedef std::unordered_map<std::string, int> PreparedStmtRegistry;
        PreparedStmtRegistry m_stmtRegistry;                ///< SQL 模板字符串到语句 ID 的映射表

        int m_iStmtIndex;

    private:

        bool m_logSQL;                                      ///< 是否记录 SQL 日志
        std::string m_logsDir;                              ///< SQL 日志目录路径
        uint32 m_pingIntervallms;                           ///< 数据库连接保活 Ping 间隔（毫秒）
};
#endif
