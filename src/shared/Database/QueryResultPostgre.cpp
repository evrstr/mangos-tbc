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

#ifdef DO_POSTGRESQL

#include "DatabaseEnv.h"

// 构造 PostgreSQL 结果集并初始化各列字段类型 | Construct PostgreSQL result wrapper and set column types from PQftype
QueryResultPostgre::QueryResultPostgre(PGresult* result, uint64 rowCount, uint32 fieldCount) :
    QueryResult(rowCount, fieldCount), mResult(result),  mTableIndex(0)
{

    mCurrentRow = new Field[mFieldCount];
    MANGOS_ASSERT(mCurrentRow);

    for (uint32 i = 0; i < mFieldCount; ++i)
        mCurrentRow[i].SetType(ConvertNativeType(PQftype(result, i)));
}

QueryResultPostgre::~QueryResultPostgre()
{
    EndQuery();
}

// 读取下一行元组数据到 mCurrentRow | Fetch next tuple row into mCurrentRow
bool QueryResultPostgre::NextRow()
{
    if (!mResult)
        return false;

    if (mTableIndex >= mRowCount)
    {
        EndQuery();
        return false;
    }

    char* pPQgetvalue;
    for (int j = 0; j < mFieldCount; ++j)
    {
        pPQgetvalue = PQgetvalue(mResult, mTableIndex, j);
        if (pPQgetvalue && !(*pPQgetvalue))
            pPQgetvalue = nullptr;

        mCurrentRow[j].SetValue(pPQgetvalue);
    }
    ++mTableIndex;

    return true;
}

// 释放行缓冲区与 PGresult 句柄 | Release row buffer and clear PGresult
void QueryResultPostgre::EndQuery()
{
    delete[] mCurrentRow;
    mCurrentRow = 0;

    if (mResult)
    {
        PQclear(mResult);
        mResult = 0;
    }
}

// see types in #include <postgre/pg_type.h> | 参见 <postgre/pg_type.h> 中的类型 OID 定义
// 将 PostgreSQL OID 转换为内部通用数据类型 | Map PostgreSQL OID to internal Field::DataTypes
enum Field::DataTypes QueryResultPostgre::ConvertNativeType(Oid  pOid) const
{
    switch (pOid)
    {
        case BPCHAROID:
        case CIDOID:
        case CIDROID:
        case CIRCLEOID:
        case INETOID:
        case NAMEOID:
        case TEXTOID:
        case VARCHAROID:
            return Field::DB_TYPE_STRING;
        case CASHOID:
        case FLOAT4OID:
        case FLOAT8OID:
        case NUMERICOID:
            return Field::DB_TYPE_FLOAT;
        case DATEOID:                                       // Date | 日期类型
        case RELTIMEOID:                                    // Date | 相对时间
        case TIMEOID:                                       // Time | 时间类型
        case TIMETZOID:                                     // Time | 带时区时间
        case ABSTIMEOID:                                    // DateTime | 绝对时间
        case INTERVALOID:                                   // DateTime | 时间间隔
        case TIMESTAMPOID:                                  // DateTime | 时间戳
        case TIMESTAMPTZOID:                                // DateTime | 带时区时间戳
        case INT2OID:                                       // Int | 16位整数
        case INT2VECTOROID:                                 // Int | 16位整数向量
        case INT4OID:                                       // Int | 32位整数
        case OIDOID:                                        // Int | 对象标识符
        case CHAROID:                                       // UInt | 单字符/无符号整型
        case INT8OID:                                       // LongLong | 64位整数
            return Field::DB_TYPE_INTEGER;
        case BOOLOID:
            return Field::DB_TYPE_BOOL;                     // Bool | 布尔类型
        /*
                case BOXOID:    Rect;
                case LINEOID:   Rect;
                case VARBITOID: BitArray;
                case BYTEAOID:  ByteArray;
        */
        case LSEGOID:
        case OIDVECTOROID:
        case PATHOID:
        case POINTOID:
        case POLYGONOID:
        case REGPROCOID:
        case TIDOID:
        case TINTERVALOID:
        case UNKNOWNOID:
        case XIDOID:
        default:
            return Field::DB_TYPE_UNKNOWN;
    }
    return Field::DB_TYPE_UNKNOWN;
}
#endif
