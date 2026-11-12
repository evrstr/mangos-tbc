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

#if !defined(QUERYRESULTSQLITE_H)
#define QUERYRESULTSQLITE_H

#include "Common.h"
#include "QueryResult.h"

#ifdef _WIN32
  #include <WinSock2.h>
#endif

#include <sqlite3.h>

// SQLite 查询结果集封装类，构造时预读取全部行以尽快释放读锁 | SQLite query result wrapper that buffers all rows upfront to release read lock immediately
class QueryResultSqlite : public QueryResult
{
    public:
        // Reads all the rows of the statement, then finalizes it | 读取语句的全部结果行后立即销毁 sqlite3_stmt
        QueryResultSqlite(sqlite3_stmt* stmt);

        ~QueryResultSqlite();

        // 推进到下一行并更新 mCurrentRow | Advance to next cached row and update mCurrentRow
        bool NextRow() override;

        operator bool() const
        {
          return (mRowCount * mFieldCount) > 0;
        }

    private:
        // 将 SQLite 列类型转换为内部 Field::DataTypes | Convert SQLite column type to Field::DataTypes
        enum Field::DataTypes ConvertNativeType(int sqliteType) const;
        //enum Field::DataTypes ConvertNativeType(enum_field_types mysqlType) const;
        // 释放当前行数组 | Free current row Field array
        void EndQuery();

        static constexpr size_t NULL_VALUE = static_cast<size_t>(-1);

        std::string mValues;                                // text of all the values of the result, each one ended by '\0' | 拼接存储所有字段文本（以 '\0' 分隔）
        std::vector<size_t> mValueOffsets;                  // offset of each value in mValues, NULL_VALUE for NULL | 每个字段在 mValues 中的起始偏移，NULL_VALUE 表示空值
        std::vector<uint8> mValueTypes;                     // Field::DataTypes of each value | 每个字段对应的 Field::DataTypes 类型
        uint64 mNextRow;                                    // 下一待读取行索引 | Next row index to read
};
#endif
#endif
