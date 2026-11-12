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

#ifndef _DATABASEMYSQL_H
#define _DATABASEMYSQL_H

//#include "Common.h"
#include "Database.h"
#include "Policies/Singleton.h"

#include <mysql.h>

// MySQL prepared statement class | MySQL 原生预处理语句封装类
class MySqlPreparedStatement : public SqlPreparedStatement
{
    public:
        MySqlPreparedStatement(const std::string& fmt, SqlConnection& conn, MYSQL* mysql);
        ~MySqlPreparedStatement();

        // prepare statement | 预处理 SQL 语句并解析参数及结果元数据
        virtual bool prepare() override;

        // bind input parameters | 绑定所有输入参数到 MYSQL_BIND 数组
        virtual void bind(const SqlStmtParameters& holder) override;

        // execute DML statement | 执行预处理 DML 语句
        virtual bool execute() override;

    protected:
        // bind parameters | 将单个参数字段写入指定索引的 MYSQL_BIND 结构
        void addParam(unsigned int nIndex, const SqlStmtFieldData& data);

        // 将核心字段类型转换为 MySQL 字段类型及无符号标志 | Convert core field type to MySQL field type and unsigned flag
        static enum_field_types ToMySQLType(const SqlStmtFieldData& data, bool& bUnsigned);

    private:
        // 释放绑定缓冲区与 MySQL 语句句柄 | Free bind arrays, result metadata, and MYSQL_STMT handle
        void RemoveBinds();

        MYSQL* m_pMySQLConn;
        MYSQL_STMT* m_stmt;
        MYSQL_BIND* m_pInputArgs;
        MYSQL_BIND* m_pResult;
        MYSQL_RES* m_pResultMetadata;
};

// MySQL 单连接实现类 | MySQL single connection implementation
class MySQLConnection : public SqlConnection
{
    public:
        MySQLConnection(Database& db) : SqlConnection(db), mMysql(nullptr) {}
        ~MySQLConnection();

        //! Initializes Mysql and connects to a server. | 初始化 MySQL 并连接到数据库服务器
        /*! infoString should be formated like hostname;username;password;database. | infoString 格式应为 hostname;port_or_socket;username;password;database */
        bool Initialize(const char* infoString) override;

        std::unique_ptr<QueryResult> Query(const char* sql) override;
        QueryNamedResult* QueryNamed(const char* sql) override;
        bool Execute(const char* sql) override;

        unsigned long escape_string(char* to, const char* from, unsigned long length) override;

        bool BeginTransaction() override;
        bool CommitTransaction() override;
        bool RollbackTransaction() override;

    protected:
        // 创建 MySQL 原生预处理语句对象 | Create MySQL native prepared statement
        SqlPreparedStatement* CreateStatement(const std::string& fmt) override;

    private:
        // 执行事务控制命令（START/COMMIT/ROLLBACK） | Execute transaction control command
        bool _TransactionCmd(const char* sql);
        // 内部通用查询执行与结果集提取 | Internal query execution and result set extraction
        bool _Query(const char* sql, MYSQL_RES** pResult, MYSQL_FIELD** pFields, uint64* pRowCount, uint32* pFieldCount);

        MYSQL* mMysql;
};

// MySQL 数据库管理器实现类 | MySQL Database manager implementation
class DatabaseMysql : public Database
{
        friend class MaNGOS::OperatorNew<DatabaseMysql>;

    public:
        DatabaseMysql();
        ~DatabaseMysql();

        // must be call before first query in thread | 线程首次执行查询前必须调用（初始化 MySQL 线程局部状态）
        void ThreadStart() override;
        // must be call before finish thread run | 线程结束前必须调用（清理 MySQL 线程局部状态）
        void ThreadEnd() override;

    protected:
        // 创建 MySQL 连接实例 | Create a new MySQLConnection instance
        virtual SqlConnection* CreateConnection() override;

    private:
        static size_t db_count;                             ///< 活跃 DatabaseMysql 实例引用计数（用于库初始化/反初始化）
};

#endif
#endif
#endif
