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

#include "DatabaseEnv.h"
#include "Util/Errors.h"

// 构造 MySQL 结果集并初始化每列的数据类型 | Construct MySQL result wrapper and initialize column data types
QueryResultMysql::QueryResultMysql(MYSQL_RES* result, MYSQL_FIELD* fields, uint64 rowCount, uint32 fieldCount) :
    QueryResult(rowCount, fieldCount), mResult(result)
{
    mCurrentRow = new Field[mFieldCount];
    MANGOS_ASSERT(mCurrentRow);

    for (uint32 i = 0; i < mFieldCount; ++i)
        mCurrentRow[i].SetType(ConvertNativeType(fields[i].type));
}

QueryResultMysql::~QueryResultMysql()
{
    EndQuery();
}

// 获取下一行记录并更新当前行各字段指针 | Fetch next row and update field value pointers
bool QueryResultMysql::NextRow()
{
    if (!mResult)
        return false;

    MYSQL_ROW row = mysql_fetch_row(mResult);
    if (!row)
    {
        EndQuery();
        return false;
    }

    for (uint32 i = 0; i < mFieldCount; ++i)
        mCurrentRow[i].SetValue(row[i]);

    return true;
}

// 释放行缓冲区与 MySQL 结果集 | Free row buffer and MYSQL_RES handle
void QueryResultMysql::EndQuery()
{
    delete[] mCurrentRow;
    mCurrentRow = nullptr;

    if (mResult)
    {
        mysql_free_result(mResult);
        mResult = nullptr;
    }
}

// 将 MySQL 原生字段枚举映射到内部通用数据类型 | Map MySQL native field type enum to internal Field::DataTypes
enum Field::DataTypes QueryResultMysql::ConvertNativeType(enum_field_types mysqlType) const
{
    switch (mysqlType)
    {
        case FIELD_TYPE_TIMESTAMP:
        case FIELD_TYPE_DATE:
        case FIELD_TYPE_TIME:
        case FIELD_TYPE_DATETIME:
        case FIELD_TYPE_YEAR:
        case FIELD_TYPE_STRING:
        case FIELD_TYPE_VAR_STRING:
        case FIELD_TYPE_BLOB:
        case FIELD_TYPE_SET:
        case FIELD_TYPE_NULL:
            return Field::DB_TYPE_STRING;
        case FIELD_TYPE_TINY:
        case FIELD_TYPE_SHORT:
        case FIELD_TYPE_LONG:
        case FIELD_TYPE_INT24:
        case FIELD_TYPE_LONGLONG:
        case FIELD_TYPE_ENUM:
            return Field::DB_TYPE_INTEGER;
        case FIELD_TYPE_DECIMAL:
        case FIELD_TYPE_FLOAT:
        case FIELD_TYPE_DOUBLE:
            return Field::DB_TYPE_FLOAT;
        default:
            return Field::DB_TYPE_UNKNOWN;
    }
}
#endif
#endif
