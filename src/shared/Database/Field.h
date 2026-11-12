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

#ifndef FIELD_H
#define FIELD_H

#include "Common.h"

// 数据库查询结果中的单个字段封装类 | Wrapper representing a single field in a database query result
class Field
{
    public:

        // 数据库字段通用数据类型 | Generic database field data types
        enum DataTypes
        {
            DB_TYPE_UNKNOWN = 0x00,
            DB_TYPE_STRING  = 0x01,
            DB_TYPE_INTEGER = 0x02,
            DB_TYPE_FLOAT   = 0x03,
            DB_TYPE_BOOL    = 0x04
        };

        Field() : mValue(nullptr), mType(DB_TYPE_UNKNOWN) {}
        Field(const char* value, enum DataTypes type) : mValue(value), mType(type) {}

        ~Field() {}

        enum DataTypes GetType() const { return mType; }
        bool IsNULL() const { return mValue == nullptr; }

        // 获取 C 风格字符串，NULL 时返回空串 | Get C-string value, returns empty string if NULL
        const char* GetString() const
        {
            return mValue ? mValue : ""; // We need this null check as we do not always null check what we get back from the database everywhere | 必须判空，因为调用处并非每次都会检查返回值是否为 NULL
        }
        // 获取 std::string 字符串 | Get value as std::string
        std::string GetCppString() const
        {
            return mValue ? mValue : "";                    // std::string s = 0 have undefine result in C++ | 在 C++ 中用空指针构造 std::string 属于未定义行为
        }
        float GetFloat() const { return mValue ? static_cast<float>(atof(mValue)) : 0.0f; }
        bool GetBool() const { return mValue ? atoi(mValue) > 0 : false; }
        int32 GetInt32() const { return mValue ? static_cast<int32>(atol(mValue)) : int32(0); }
        uint8 GetUInt8() const { return mValue ? static_cast<uint8>(atol(mValue)) : uint8(0); }
        uint16 GetUInt16() const { return mValue ? static_cast<uint16>(atol(mValue)) : uint16(0); }
        int16 GetInt16() const { return mValue ? static_cast<int16>(atol(mValue)) : int16(0); }
        uint32 GetUInt32() const { return mValue ? static_cast<uint32>(atoll(mValue)) : uint32(0); }
        // 解析 64 位无符号整数 | Parse field string as uint64
        uint64 GetUInt64() const
        {
            uint64 value = 0;
            if (!mValue || sscanf(mValue, UI64FMTD, &value) == -1)
                return 0;

            return value;
        }
        // 将日期时间字符串解析为 time_t | Parse datetime string into time_t
        time_t GetTime() const;

        void SetType(enum DataTypes type) { mType = type; }
        // no need for memory allocations to store resultset field strings | 无需额外分配内存来保存结果集字段字符串
        // all we need is to cache pointers returned by different DBMS APIs | 只需缓存各数据库 API 返回的原始字符串指针即可
        void SetValue(const char* value) { mValue = value; }

    private:
        Field(Field const&);
        Field& operator=(Field const&);

        const char* mValue;
        enum DataTypes mType;
};
#endif
