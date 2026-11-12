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

#if !defined(QUERYRESULTMYSQL_H)
#define QUERYRESULTMYSQL_H

#include "Common.h"

#ifdef _WIN32
  #include <WinSock2.h>
#endif

#include <mysql.h>

// MySQL 查询结果集封装类，逐行拉取并释放 MYSQL_RES | MySQL query result wrapper fetching rows sequentially from MYSQL_RES
class QueryResultMysql : public QueryResult
{
    public:
        QueryResultMysql(MYSQL_RES* result, MYSQL_FIELD* fields, uint64 rowCount, uint32 fieldCount);

        ~QueryResultMysql();

        // 读取下一行数据到 mCurrentRow，若无更多行则自动释放结果集 | Fetch next row into mCurrentRow, freeing result set at end
        bool NextRow() override;

    private:
        // 将 MySQL 列类型转换为内部 Field::DataTypes | Convert MySQL column type to Field::DataTypes
        enum Field::DataTypes ConvertNativeType(enum_field_types mysqlType) const;
        // 释放当前行数组和 MYSQL_RES 句柄 | Release mCurrentRow and free MYSQL_RES handle
        void EndQuery();

        MYSQL_RES* mResult;
};
#endif
#endif
#endif
