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

#ifndef DBC_FILE_LOADER_H
#define DBC_FILE_LOADER_H
#include "Platform/Define.h"
#include "Util/ByteConverter.h"
#include <cassert>

// DBC 文件字段格式枚举 | DBC file field format types
enum FieldFormat
{
    FT_NA = 'x',                                            // ignore/ default, 4 byte size, in Source String means field is ignored, in Dest String means field is filled with default value | 忽略或使用默认值（4字节），在源格式中表示忽略该字段，在目标格式中表示填充默认值
    FT_NA_BYTE = 'X',                                       // ignore/ default, 1 byte size, see above | 忽略或使用默认值（1字节），同上
    FT_NA_FLOAT = 'F',                                      // ignore/ default,  float size, see above | 忽略或使用默认值（浮点型大小），同上
    FT_NA_POINTER = 'p',                                    // fill default value into dest, pointer size, Use this only with static data (otherwise mem-leak) | 向目标填充指针大小的默认值，仅限静态数据使用（否则会内存泄漏）
    FT_STRING = 's',                                        // char* | 字符串指针
    FT_FLOAT = 'f',                                         // float | 单精度浮点数
    FT_INT = 'i',                                           // uint32 | 32位无符号整数
    FT_BYTE = 'b',                                          // uint8 | 8位无符号整数
    FT_SORT = 'd',                                          // sorted by this field, field is not included | 按此字段排序（数据中不包含该字段）
    FT_IND = 'n',                                           // the same,but parsed to data | 同上，但会解析并写入数据
    FT_LOGIC = 'l',                                         // Logical (boolean) | 逻辑型（布尔值）
    FT_64BITINT = 'L'                                       // uint64 | 64位无符号整数
};

// DBC 客户端数据文件加载器 | Loader for client DBC (DataBaseClient) files
class DBCFileLoader
{
    public:
        DBCFileLoader();
        ~DBCFileLoader();

        // 加载并解析 DBC 文件 | Load and parse a DBC file with given format string
        bool Load(const char* filename, const char* fmt);

        // 单条 DBC 记录访问器 | Accessor for a single record in the DBC file
        class Record
        {
            public:
                // 读取指定字段的浮点数值（自动转换字节序） | Read float field with endian conversion
                float getFloat(size_t field) const
                {
                    assert(field < file.fieldCount);
                    float val = *reinterpret_cast<float*>(offset + file.GetOffset(field));
                    EndianConvert(val);
                    return val;
                }
                // 读取指定字段的 32 位无符号整数（自动转换字节序） | Read uint32 field with endian conversion
                uint32 getUInt(size_t field) const
                {
                    assert(field < file.fieldCount);
                    uint32 val = *reinterpret_cast<uint32*>(offset + file.GetOffset(field));
                    EndianConvert(val);
                    return val;
                }
                // 读取指定字段的 8 位无符号整数 | Read uint8 field
                uint8 getUInt8(size_t field) const
                {
                    assert(field < file.fieldCount);
                    return *reinterpret_cast<uint8*>(offset + file.GetOffset(field));
                }

                // 根据字段中的偏移量从字符串表读取字符串 | Read string from string table using offset in field
                const char* getString(size_t field) const
                {
                    assert(field < file.fieldCount);
                    size_t stringOffset = getUInt(field);
                    assert(stringOffset < file.stringSize);
                    return reinterpret_cast<char*>(file.stringTable + stringOffset);
                }

            private:
                Record(DBCFileLoader& file_, unsigned char* offset_): offset(offset_), file(file_) {}
                unsigned char* offset;
                DBCFileLoader& file;

                friend class DBCFileLoader;
        };

        // Get record by id | 按行索引获取记录
        Record getRecord(size_t id);
        /// Get begin iterator over records | 获取记录起始迭代器（注：仅保留声明注释）

        uint32 GetNumRows() const { return recordCount;}
        uint32 GetCols() const { return fieldCount; }
        uint32 GetOffset(size_t id) const { return (fieldsOffset != nullptr && id < fieldCount) ? fieldsOffset[id] : 0; }
        bool IsLoaded() const { return data != nullptr; }
        // 按格式字符串自动生成记录数据表和索引表 | Auto-produce structured data table and index table from format
        char* AutoProduceData(const char* format, uint32& records, char**& indexTable);
        // 按格式字符串自动填充字符串表指针到数据表中 | Auto-populate string pointers into produced data table
        char* AutoProduceStrings(const char* format, char* dataTable);
        // 计算指定格式字符串对应的每条记录字节大小及索引列位置 | Calculate record byte size and index column position for format
        static uint32 GetFormatRecordSize(const char* format, int32* index_pos = nullptr);
    private:

        uint32 recordSize;
        uint32 recordCount;
        uint32 fieldCount;
        uint32 stringSize;
        uint32* fieldsOffset;
        unsigned char* data;
        unsigned char* stringTable;
};
#endif
