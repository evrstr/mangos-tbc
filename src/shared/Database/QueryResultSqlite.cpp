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

#ifdef DO_SQLITE

#include "DatabaseEnv.h"
#include "Util/Errors.h"
#include "sqlite3.h"
#include "QueryResultSqlite.h"

// 一次性读取所有结果行到内存缓冲区并 finalize 语句，防止长期持有 SQLite 读事务锁 | Buffer all result rows in memory and finalize statement immediately to avoid holding SQLite read lock
QueryResultSqlite::QueryResultSqlite(sqlite3_stmt* stmt) :
    QueryResult(0, 0), mNextRow(0)
{
    if (!stmt)
        return;

    mFieldCount = sqlite3_column_count(stmt);

    // The whole result is read before it is used: a statement still running keeps a read transaction open on its | 使用前先读完全部结果：未结束的语句会在连接上保持读事务开启，
    // connection, and a write on that connection then fails at once when another connection has written meanwhile. | 若期间其他连接执行了写入，该连接随后的写操作会立刻失败。
    int result;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        for (int i = 0; i < int(mFieldCount); ++i)
        {
            // the type is read first, reading the text can convert the value | 先读取原始列类型，因为调用 column_text 可能改变底层类型标记
            mValueTypes.push_back(uint8(ConvertNativeType(sqlite3_column_type(stmt, i))));

            const unsigned char* value = sqlite3_column_text(stmt, i);
            if (!value)
            {
                mValueOffsets.push_back(NULL_VALUE);
                continue;
            }

            mValueOffsets.push_back(mValues.size());
            mValues.append(reinterpret_cast<const char*>(value), size_t(sqlite3_column_bytes(stmt, i)));
            mValues.push_back('\0');
        }
        ++mRowCount;
    }

    if (result != SQLITE_DONE)
    {
        sLog.outErrorDb("SQL: %s", sqlite3_sql(stmt));
        sLog.outErrorDb("query ERROR: %s", sqlite3_errmsg(sqlite3_db_handle(stmt)));
        mRowCount = 0;
        mValues.clear();
        mValueOffsets.clear();
        mValueTypes.clear();
    }

    sqlite3_finalize(stmt);

    mCurrentRow = new Field[mFieldCount];
}

QueryResultSqlite::~QueryResultSqlite()
{
    EndQuery();
}

// 从缓存中加载下一行字段值与类型到 mCurrentRow | Populate mCurrentRow with cached values and types for the next row
bool QueryResultSqlite::NextRow()
{
    if (!mCurrentRow)
        return false;

    if (mNextRow >= mRowCount)
    {
        EndQuery();
        return false;
    }

    size_t const firstValue = size_t(mNextRow) * mFieldCount;
    for (uint32 i = 0; i < mFieldCount; ++i)
    {
        size_t const offset = mValueOffsets[firstValue + i];
        mCurrentRow[i].SetValue(offset == NULL_VALUE ? nullptr : mValues.c_str() + offset);
        mCurrentRow[i].SetType(Field::DataTypes(mValueTypes[firstValue + i]));
    }

    ++mNextRow;
    return true;
}

// 释放行对象数组 | Free current row Field array
void QueryResultSqlite::EndQuery()
{
    delete[] mCurrentRow;
    mCurrentRow = nullptr;
}

// 将 SQLite 动态类型常量转换为内部 Field::DataTypes | Map SQLite column type constant to internal Field::DataTypes
enum Field::DataTypes QueryResultSqlite::ConvertNativeType(int sqliteType) const
{
    switch (sqliteType)
    {
        case SQLITE_INTEGER:
            return Field::DB_TYPE_INTEGER;
        case SQLITE_FLOAT:
            return Field::DB_TYPE_FLOAT;
        case SQLITE_TEXT:
        case SQLITE_BLOB:
        case SQLITE_NULL:
            return Field::DB_TYPE_STRING;
        default:
            return Field::DB_TYPE_UNKNOWN;
    }
}
#endif
