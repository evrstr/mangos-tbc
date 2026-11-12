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

#ifndef SQLSTORAGE_IMPL_H
#define SQLSTORAGE_IMPL_H

#include "Util/ProgressBar.h"
#include "Log/Log.h"
#include "DBCFileLoader.h"

// 通用标量类型转换（S 源类型 -> D 目标类型） | Generic scalar conversion (S source-type -> D destination-type)
template<class DerivedLoader, class StorageClass>
template<class S, class D>                                  // S source-type, D destination-type | S 为源类型，D 为目标类型
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert(uint32 /*field_pos*/, S src, D& dst)
{
    dst = D(src);
}

// 标量转布尔值 | Convert scalar value to bool
template<class DerivedLoader, class StorageClass>
template<class S>                                           // S source-type | S 为源类型
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert_to_bool(uint32 /*field_pos*/, S src, bool& dst)
{
    dst = (src != 0);
}

// 复制 C 字符串到新分配的内存 | Duplicate C-string into newly allocated buffer
template<class DerivedLoader, class StorageClass>

void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert_str_to_str(uint32 /*field_pos*/, char const* src, char*& dst)
{
    if (!src)
    {
        dst = new char[1];
        *dst = 0;
    }
    else
    {
        uint32 l = strlen(src) + 1;
        dst = new char[l];
        memcpy(dst, src, l);
    }
}

// 非字符串类型转空字符串占位 | Convert non-string source value to empty string placeholder
template<class DerivedLoader, class StorageClass>
template<class S>                                           // S source-type | S 为源类型
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert_to_str(uint32 /*field_pos*/, S /*src*/, char*& dst)
{
    dst = new char[1];
    *dst = 0;
}

// 字符串转目标数值类型（默认清零） | Convert string to destination numeric type (default 0)
template<class DerivedLoader, class StorageClass>
template<class D>                                           // D destination-type | D 为目标类型
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert_from_str(uint32 /*field_pos*/, char const* /*src*/, D& dst)
{
    dst = 0;
}

// 字符串转布尔值（默认 false） | Convert string to bool (default false)
template<class DerivedLoader, class StorageClass>
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert_str_to_bool(uint32 /*field_pos*/, char const* /*src*/, bool& dst)
{
    dst = false;
}

// 默认填充目标数值字段 | Default-fill destination numeric field
template<class DerivedLoader, class StorageClass>
template<class S, class D>                                  // S source-type, D destination-type | S 为源类型，D 为目标类型
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::default_fill(uint32 /*field_pos*/, S src, D& dst)
{
    dst = D(src);
}

// 默认填充空字符串字段 | Default-fill destination string field with empty string
template<class DerivedLoader, class StorageClass>
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::default_fill_to_str(uint32 /*field_pos*/, char const* /*src*/, char*& dst)
{
    dst = new char[1];
    *dst = 0;
}

// 将非字符串源字段按目标格式转换并写入记录缓冲区（ARM 平台使用临时变量避免未对齐访问） | Store non-string source value into record buffer according to destination format (ARM uses temporaries for alignment safety)
template<class DerivedLoader, class StorageClass>
template<class V>                                           // V value-type | V 为值类型
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::storeValue(V value, StorageClass& store, char* p, uint32 x, uint32& offset)
{
#if defined(__arm__)
    bool tmpbool;
    char tmpchar;
    uint32 tmpint;
    float tmpfloat;
    char* tmpstr = new char[1];
    uint64 tmpbigint;
#endif
    DerivedLoader* subclass = (static_cast<DerivedLoader*>(this));
    switch (store.GetDstFormat(x))
    {
#if defined(__arm__)
        case FT_LOGIC:
            subclass->convert_to_bool(x, value, tmpbool);
            memcpy(&(p[offset]), &tmpbool, sizeof(bool));
            offset += sizeof(bool);
            break;
        case FT_BYTE:
            subclass->convert(x, value, tmpchar);
            memcpy(&(p[offset]), &tmpchar, sizeof(char));
            offset += sizeof(char);
            break;
        case FT_INT:
            subclass->convert(x, value, tmpint);
            memcpy(&(p[offset]), &tmpint, sizeof(uint32));
            offset += sizeof(uint32);
            break;
        case FT_FLOAT:
            subclass->convert(x, value, tmpfloat);
            memcpy(&(p[offset]), &tmpfloat, sizeof(float));
            offset += sizeof(float);
            break;
        case FT_STRING:
            subclass->convert_to_str(x, value, tmpstr);
            memcpy(&(p[offset]), &tmpstr, sizeof(char*));
            offset += sizeof(char*);
            break;
        case FT_NA:
            subclass->default_fill(x, value, tmpint);
            memcpy(&(p[offset]), &tmpint, sizeof(uint32));
            offset += sizeof(uint32);
            break;
        case FT_NA_BYTE:
            subclass->default_fill(x, value, tmpchar);
            memcpy(&(p[offset]), &tmpchar, sizeof(char));
            offset += sizeof(char);
            break;
        case FT_NA_FLOAT:
            subclass->default_fill(x, value, tmpfloat);
            memcpy(&(p[offset]), &tmpfloat, sizeof(float));
            offset += sizeof(float);
            break;
        case FT_64BITINT:
            subclass->default_fill(x, value, tmpbigint);
            memcpy(&(p[offset]), &tmpbigint, sizeof(uint64));
            offset += sizeof(uint64);
            break;
        case FT_IND:
        case FT_SORT:
            assert(false && "SQL storage not have sort field types");
            break;
        default:
            assert(false && "unknown format character");
            break;
#else
        case FT_LOGIC:
            subclass->convert_to_bool(x, value, *((bool*)(&p[offset])));
            offset += sizeof(bool);
            break;
        case FT_BYTE:
            subclass->convert(x, value, *((char*)(&p[offset])));
            offset += sizeof(char);
            break;
        case FT_INT:
            subclass->convert(x, value, *((uint32*)(&p[offset])));
            offset += sizeof(uint32);
            break;
        case FT_FLOAT:
            subclass->convert(x, value, *((float*)(&p[offset])));
            offset += sizeof(float);
            break;
        case FT_STRING:
            subclass->convert_to_str(x, value, *((char**)(&p[offset])));
            offset += sizeof(char*);
            break;
        case FT_NA:
            subclass->default_fill(x, value, *((uint32*)(&p[offset])));
            offset += sizeof(uint32);
            break;
        case FT_NA_BYTE:
            subclass->default_fill(x, value, *((char*)(&p[offset])));
            offset += sizeof(char);
            break;
        case FT_NA_FLOAT:
            subclass->default_fill(x, value, *((float*)(&p[offset])));
            offset += sizeof(float);
            break;
        case FT_64BITINT:
            subclass->default_fill(x, value, *((uint64*)(&p[offset])));
            offset += sizeof(uint64);
            break;
        case FT_IND:
        case FT_SORT:
            assert(false && "SQL storage not have sort field types");
            break;
        default:
            assert(false && "unknown format character");
            break;
#endif
    }
}

// 将字符串源字段按目标格式转换并写入记录缓冲区 | Store string source value into record buffer according to destination format
template<class DerivedLoader, class StorageClass>
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::storeValue(char const* value, StorageClass& store, char* p, uint32 x, uint32& offset)
{
#if defined(__arm__)
    bool tmpbool;
    char tmpchar;
    uint32 tmpint;
    float tmpfloat;
    char* tmpstr = new char[1];
    uint64 tmpbigint;
#endif
    DerivedLoader* subclass = (static_cast<DerivedLoader*>(this));
    switch (store.GetDstFormat(x))
    {
#if defined(__arm__)
        case FT_LOGIC:
            subclass->convert_str_to_bool(x, value, tmpbool);
            memcpy(&(p[offset]), &tmpbool, sizeof(bool));
            offset += sizeof(bool);
            break;
        case FT_BYTE:
            subclass->convert_from_str(x, value, tmpchar);
            memcpy(&(p[offset]), &tmpchar, sizeof(char));
            offset += sizeof(char);
            break;
        case FT_INT:
            subclass->convert_from_str(x, value, tmpint);
            memcpy(&(p[offset]), &tmpint, sizeof(uint32));
            offset += sizeof(uint32);
            break;
        case FT_FLOAT:
            subclass->convert_from_str(x, value, tmpfloat);
            memcpy(&(p[offset]), &tmpfloat, sizeof(float));
            offset += sizeof(float);
            break;
        case FT_STRING:
            subclass->convert_str_to_str(x, value, tmpstr);
            memcpy(&(p[offset]), &tmpstr, sizeof(char*));
            offset += sizeof(char*);
            break;
        case FT_NA_POINTER:
            subclass->default_fill_to_str(x, value, tmpstr);
            memcpy(&(p[offset]), &tmpstr, sizeof(char*));
            offset += sizeof(char*);
            break;
        case FT_64BITINT:
            subclass->convert_from_str(x, value, tmpbigint);
            memcpy(&(p[offset]), &tmpbigint, sizeof(uint64));
            offset += sizeof(uint64);
            break;
        case FT_IND:
        case FT_SORT:
            assert(false && "SQL storage not have sort field types");
            break;
        default:
            assert(false && "unknown format character");
            break;
#else
        case FT_LOGIC:
            subclass->convert_str_to_bool(x, value, *((bool*)(&p[offset])));
            offset += sizeof(bool);
            break;
        case FT_BYTE:
            subclass->convert_from_str(x, value, *((char*)(&p[offset])));
            offset += sizeof(char);
            break;
        case FT_INT:
            subclass->convert_from_str(x, value, *((uint32*)(&p[offset])));
            offset += sizeof(uint32);
            break;
        case FT_FLOAT:
            subclass->convert_from_str(x, value, *((float*)(&p[offset])));
            offset += sizeof(float);
            break;
        case FT_STRING:
            subclass->convert_str_to_str(x, value, *((char**)(&p[offset])));
            offset += sizeof(char*);
            break;
        case FT_NA_POINTER:
            subclass->default_fill_to_str(x, value, *((char**)(&p[offset])));
            offset += sizeof(char*);
            break;
        case FT_64BITINT:
            subclass->convert_from_str(x, value, *((uint64*)(&p[offset])));
            offset += sizeof(uint64);
            break;
        case FT_IND:
        case FT_SORT:
            assert(false && "SQL storage not have sort field types");
            break;
        default:
            assert(false && "unknown format character");
            break;
#endif
    }
}

// 从 WorldDatabase 查询整张表并按源/目标格式填充到存储容器 | Query entire table from WorldDatabase and populate storage container per src/dst formats
template<class DerivedLoader, class StorageClass>
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::Load(StorageClass& store, bool error_at_empty /*= true*/)
{
    Field* fields = nullptr;
    auto queryResult = WorldDatabase.PQuery("SELECT MAX(%s) FROM %s", store.EntryFieldName(), store.GetTableName());
    if (!queryResult)
    {
        sLog.outError("Error loading %s table (not exist?)\n", store.GetTableName());
        Log::WaitBeforeContinueIfNeed();
        exit(1);                                            // Stop server at loading non exited table or not accessable table | 表不存在或无法访问时终止服务器启动
    }

    uint32 maxRecordId = (*queryResult)[0].GetUInt32() + 1;
    uint32 recordCount = 0;
    uint32 recordsize = 0;

    queryResult = WorldDatabase.PQuery("SELECT COUNT(*) FROM %s", store.GetTableName());
    if (queryResult)
    {
        fields = queryResult->Fetch();
        recordCount = fields[0].GetUInt32();
    }

    queryResult = WorldDatabase.PQuery("SELECT * FROM %s", store.GetTableName());

    if (!queryResult)
    {
        if (error_at_empty)
            sLog.outError("%s table is empty!\n", store.GetTableName());
        else
            sLog.outString("%s table is empty!\n", store.GetTableName());

        recordCount = 0;
        return;
    }

    if (store.GetSrcFieldCount() != queryResult->GetFieldCount())
    {
        recordCount = 0;
        sLog.outError("Error in %s table, probably sql file format was updated (there should be %d fields in sql).\n", store.GetTableName(), store.GetSrcFieldCount());
        Log::WaitBeforeContinueIfNeed();
        exit(1);                                            // Stop server at loading broken or non-compatible table. | 表结构字段数不匹配时终止服务器
    }

    // get struct size | 根据目标格式计算单条记录结构体大小
    uint32 offset = 0;
    for (uint32 x = 0; x < store.GetDstFieldCount(); ++x)
    {
        switch (store.GetDstFormat(x))
        {
            case FT_LOGIC:
                recordsize += sizeof(bool);   break;
            case FT_BYTE:
                recordsize += sizeof(char);   break;
            case FT_INT:
                recordsize += sizeof(uint32); break;
            case FT_FLOAT:
                recordsize += sizeof(float);  break;
            case FT_STRING:
                recordsize += sizeof(char*);  break;
            case FT_NA:
                recordsize += sizeof(uint32); break;
            case FT_NA_BYTE:
                recordsize += sizeof(char);   break;
            case FT_NA_FLOAT:
                recordsize += sizeof(float);  break;
            case FT_NA_POINTER:
                recordsize += sizeof(char*);  break;
            case FT_64BITINT:
                recordsize += sizeof(uint64);  break;
            case FT_IND:
            case FT_SORT:
                assert(false && "SQL storage not have sort field types");
                break;
            default:
                assert(false && "unknown format character");
                break;
        }
    }

    // Prepare data storage and lookup storage | 准备数据缓冲区与索引容器
    store.prepareToLoad(maxRecordId, recordCount, recordsize);

    BarGoLink bar(recordCount);
    do
    {
        fields = queryResult->Fetch();
        bar.step();

        char* record = store.createRecord(fields[0].GetUInt32());
        offset = 0;

        // dependend on dest-size | 双指针遍历目标格式 x 与源列 y（根据目标大小推进）
        // iterate two indexes: x over dest, y over source | x 遍历目标字段，y 遍历源列
        //                      y++ If and only If x != FT_NA* | 仅当目标字段不是 FT_NA* 默认填充时才递增 y
        //                      x++ If and only If a value is stored | 仅当向目标写入了值时才递增 x
        for (uint32 x = 0, y = 0; x < store.GetDstFieldCount();)
        {
            switch (store.GetDstFormat(x))
            {
                // For default fill continue and do not increase y | 目标为默认值填充占位符时写入默认值且不递增 y
                case FT_NA:         storeValue((uint32)0, store, record, x, offset);         ++x; continue;
                case FT_NA_BYTE:    storeValue((char)0, store, record, x, offset);           ++x; continue;
                case FT_NA_FLOAT:   storeValue((float)0.0f, store, record, x, offset);       ++x; continue;
                case FT_NA_POINTER: storeValue((char const*)nullptr, store, record, x, offset); ++x; continue;
                default:
                    break;
            }

            // It is required that the input has at least as many columns set as the output requires | 源列数必须满足目标所需列数
            if (y >= store.GetSrcFieldCount())
                assert(false && "SQL storage has too few columns!");

            switch (store.GetSrcFormat(y))
            {
                case FT_LOGIC:  storeValue((bool)(fields[y].GetUInt32() > 0), store, record, x, offset);  ++x; break;
                case FT_BYTE:   storeValue((char)fields[y].GetUInt8(), store, record, x, offset);         ++x; break;
                case FT_INT:    storeValue((uint32)fields[y].GetUInt32(), store, record, x, offset);      ++x; break;
                case FT_FLOAT:  storeValue((float)fields[y].GetFloat(), store, record, x, offset);        ++x; break;
                case FT_STRING: storeValue((char const*)fields[y].GetString(), store, record, x, offset); ++x; break;
                case FT_64BITINT: storeValue((uint64)fields[y].GetUInt64(), store, record, x, offset);            ++x; break;
                case FT_NA:
                case FT_NA_BYTE:
                case FT_NA_FLOAT:
                    // Do Not increase x | 源列被忽略时不递增 x
                    break;
                case FT_IND:
                case FT_SORT:
                case FT_NA_POINTER:
                    assert(false && "SQL storage not have sort or pointer field types");
                    break;
                default:
                    assert(false && "unknown format character");
            }
            ++y;
        }
    }
    while (queryResult->NextRow());
}

#endif
