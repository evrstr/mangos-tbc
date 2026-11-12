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

#ifndef _DatabaseSqlite_H
#define _DatabaseSqlite_H

//#include "Common.h"
#include "Database.h"
#include "Policies/Singleton.h"
#include "QueryResultSqlite.h"

#include <sqlite3.h>

// MySQL prepared statement class | SQLite 预处理语句封装类（注：原注释误写为 MySQL）
class SqlitePreparedStatement : public SqlPreparedStatement
{
    public:
        SqlitePreparedStatement(const std::string& fmt, SqlConnection& conn, sqlite3* mysql);
        ~SqlitePreparedStatement();

        // prepare statement | 编译 SQLite 预处理语句
        virtual bool prepare() override;

        // bind input parameters | 绑定所有输入参数
        virtual void bind(const SqlStmtParameters& holder) override;

        // execute DML statement | 执行预处理 DML 语句
        virtual bool execute() override;

    protected:
        // bind parameters | 按索引绑定单个参数值
        void addParam(unsigned int nIndex, const SqlStmtFieldData& data);

    private:
        // 销毁 sqlite3_stmt 并重置状态 | Finalize sqlite3_stmt and reset prepared state
        void RemoveBinds();

        sqlite3* m_pSqliteConn;
        sqlite3_stmt** m_stmt;
        sqlite3_value* m_pResult;
};

// SQLite 单连接实现类 | SQLite single connection implementation
class SQLiteConnection : public SqlConnection
{
    public:
        SQLiteConnection(Database& db) : SqlConnection(db), mSqlite(nullptr) {}
        ~SQLiteConnection();

        //! Initializes Mysql and connects to a server. | 打开 SQLite 数据库文件并配置连接参数
        /*! infoString should be formated like hostname;username;password;database. | infoString 为 SQLite 数据库文件路径 */
        bool Initialize(const char* infoString) override;

        std::unique_ptr<QueryResult> Query(const char* sql) override;
        QueryNamedResult* QueryNamed(const char* sql) override;
        bool Execute(const char* sql) override;

        unsigned long escape_string(char* to, const char* from, unsigned long length) override;

        bool BeginTransaction() override;
        bool CommitTransaction() override;
        bool RollbackTransaction() override;

    protected:
        // 创建 SQLite 预处理语句对象 | Create SqlitePreparedStatement instance
        SqlPreparedStatement* CreateStatement(const std::string& fmt) override;

    private:
        // 执行事务控制语句（BEGIN/COMMIT/ROLLBACK） | Execute transaction control SQL command
        bool _TransactionCmd(const char* sql);
        // 预编译 SQL 查询语句 | Prepare sqlite3_stmt for query execution
        bool _Query(const char* sql, sqlite3_stmt** pStmt);

        sqlite3* mSqlite;
};

// SQLite 数据库管理器实现类 | SQLite Database manager implementation
class DatabaseSqlite : public Database
{
        friend class MaNGOS::OperatorNew<DatabaseSqlite>;

    protected:
        // 创建 SQLite 连接实例 | Create a new SQLiteConnection instance
        virtual SqlConnection* CreateConnection() override;
};

#endif
#endif
