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

#ifndef _DatabasePostgre_H
#define _DatabasePostgre_H

#include "Common.h"
#include "Database.h"
#include "Policies/Singleton.h"
#include <stdarg.h>

#ifdef _WIN32
#define FD_SETSIZE 1024
#include <winsock2.h>
#include <postgre/libpq-fe.h>
#else
#include <postgresql/libpq-fe.h>
#endif

// PostgreSQL 单连接实现类 | PostgreSQL single connection implementation
class PostgreSQLConnection : public SqlConnection
{
    public:
        PostgreSQLConnection(Database& db) : SqlConnection(db), mPGconn(nullptr) {}
        ~PostgreSQLConnection();

        // 连接并初始化 PostgreSQL 会话 | Connect and initialize PostgreSQL session
        bool Initialize(const char* infoString) override;

        std::unique_ptr<QueryResult> Query(const char* sql) override;
        QueryNamedResult* QueryNamed(const char* sql) override;
        bool Execute(const char* sql) override;

        unsigned long escape_string(char* to, const char* from, unsigned long length);

        bool BeginTransaction() override;
        bool CommitTransaction() override;
        bool RollbackTransaction() override;

    private:
        // 执行事务控制命令（START/COMMIT/ROLLBACK） | Execute transaction control command
        bool _TransactionCmd(const char* sql);
        // 内部执行查询并返回 PGresult 与行列统计 | Internal query execution returning PGresult and row/field counts
        bool _Query(const char* sql, PGresult** pResult, uint64* pRowCount, uint32* pFieldCount);

        PGconn* mPGconn;
};

// PostgreSQL 数据库管理器实现类 | PostgreSQL Database manager implementation
class DatabasePostgre : public Database
{
        friend class MaNGOS::OperatorNew<DatabasePostgre>;

    public:
        DatabasePostgre();
        ~DatabasePostgre();

        //! Initializes Postgres and connects to a server. | 初始化 Postgres 并连接到服务器
        /*! infoString should be formated like hostname;username;password;database. | infoString 格式应为 hostname;port_or_socket_dir;username;password;database */

    protected:
        // 创建 PostgreSQL 连接实例 | Create a new PostgreSQLConnection instance
        virtual SqlConnection* CreateConnection() override;

    private:
        static size_t db_count;                             ///< 活跃 DatabasePostgre 实例计数
};
#endif
