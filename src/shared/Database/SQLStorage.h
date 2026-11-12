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

#ifndef SQLSTORAGE_H
#define SQLSTORAGE_H

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "DBCFileLoader.h"

// SQL 数据表内存存储基类，管理连续记录缓冲区与源/目标字段格式 | Base class for in-memory SQL table storage managing record buffer and src/dst field formats
class SQLStorageBase
{
        template<class DerivedLoader, class StorageClass> friend class SQLStorageLoaderBase;

    public:
        char const* GetTableName() const { return m_tableName; }
        char const* EntryFieldName() const { return m_entry_field; }

        FieldFormat GetDstFormat(uint32 idx) const { return (FieldFormat)m_dst_format[idx]; };
        const char* GetDstFormat() const { return m_dst_format; };
        FieldFormat GetSrcFormat(uint32 idx) const { return (FieldFormat)m_src_format[idx]; };
        const char* GetSrcFormat() const { return m_src_format; };

        uint32 GetMaxEntry() const { return m_maxEntry; };
        uint32 GetRecordCount() const { return m_recordCount; };

        // 连续内存记录迭代器 | Contiguous record iterator for SQLStorageBase
        template<typename T>
        class SQLSIterator
        {
                friend class SQLStorageBase;

            public:
                T const* getValue() const { return reinterpret_cast<T const*>(pointer); }

                void operator ++() { pointer += recordSize; }
                T const* operator *() const { return getValue(); }
                T const* operator ->() const { return getValue(); }
                bool operator <(const SQLSIterator& r) const { return pointer < r.pointer; }
                void operator =(const SQLSIterator& r) { pointer = r.pointer; recordSize = r.recordSize; }

            private:
                SQLSIterator(char* ptr, uint32 _recordSize) : pointer(ptr), recordSize(_recordSize) {}
                char* pointer;
                uint32 recordSize;
        };

        template<typename T>
        SQLSIterator<T> getDataBegin() const { return SQLSIterator<T>(m_data, m_recordSize); }
        template<typename T>
        SQLSIterator<T> getDataEnd() const { return SQLSIterator<T>(m_data + m_recordCount * m_recordSize, m_recordSize); }

    protected:
        SQLStorageBase();
        virtual ~SQLStorageBase() { Free(); }

        // 初始化表名、主键列名及源/目标字段格式字符串 | Initialize table name, key column name, and source/destination format strings
        void Initialize(const char* tableName, const char* entry_field, const char* src_format, const char* dst_format);

        uint32 GetDstFieldCount() const { return m_dstFieldCount; }
        uint32 GetSrcFieldCount() const { return m_srcFieldCount; }
        uint32 GetRecordSize() const { return m_recordSize; }

        // 加载前预分配记录缓冲区 | Allocate contiguous record buffer before loading
        virtual void prepareToLoad(uint32 maxEntry, uint32 recordCount, uint32 recordSize);
        // 新建记录后由子类注册到各自的索引结构 | Subclass hook to index a newly allocated record
        virtual void JustCreatedRecord(uint32 recordId, char* record) = 0;
        // 释放动态字符串字段与主数据缓冲区 | Free allocated string fields and record buffer
        virtual void Free();

    private:
        // 在缓冲区中分配一条新记录并触发索引回调 | Allocate next record slot and invoke JustCreatedRecord
        char* createRecord(uint32 recordId);

        // Information about the table | 数据表元信息
        const char* m_tableName;
        const char* m_entry_field;
        const char* m_src_format;
        const char* m_dst_format;

        // Information about the records | 记录布局与数量信息
        uint32 m_dstFieldCount;
        uint32 m_srcFieldCount;
        uint32 m_recordCount;
        uint32 m_maxEntry;
        uint32 m_recordSize;

        // Data Storage | 连续记录原始内存块
        char* m_data;
};

// 基于平坦指针数组索引的 SQL 存储类（适合连续且 ID 上限可控的表） | Flat array-indexed SQL storage (best for dense IDs with bounded max entry)
class SQLStorage : public SQLStorageBase
{
        template<class DerivedLoader, class StorageClass> friend class SQLStorageLoaderBase;

    public:
        SQLStorage(const char* fmt, const char* _entry_field, const char* sqlname);

        SQLStorage(const char* src_fmt, const char* dst_fmt, const char* _entry_field, const char* sqlname);

        ~SQLStorage() override { Free(); }

        // 按主键 ID 快速查找记录指针 | Fast O(1) entry lookup by ID
        template<class T>
        T const* LookupEntry(uint32 id) const
        {
            if (id >= GetMaxEntry())
                return nullptr;
            return reinterpret_cast<T const*>(m_Index[id]);
        }

        // 从 WorldDatabase 加载全表数据 | Load all rows from WorldDatabase
        void Load(bool error_at_empty = true);

        void EraseEntry(uint32 id);

    protected:
        void prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize) override;
        void JustCreatedRecord(uint32 recordId, char* record) override
        {
            m_Index[recordId] = record;
        }

        void Free() override;

    private:
        // Lookup access | 按 ID 下标直接寻址的指针数组
        char** m_Index;
};

// 基于哈希表索引的 SQL 存储类（适合 ID 稀疏或较大的表） | Hash-map indexed SQL storage (best for sparse or large IDs)
class SQLHashStorage : public SQLStorageBase
{
        template<class DerivedLoader, class StorageClass> friend class SQLStorageLoaderBase;

    public:
        SQLHashStorage(const char* fmt, const char* _entry_field, const char* sqlname);
        SQLHashStorage(const char* src_fmt, const char* dst_fmt, const char* _entry_field, const char* sqlname);

        ~SQLHashStorage() override { Free(); }

        // 通过哈希表按键查找记录 | Lookup record pointer by ID in hash map
        template<class T>
        T const* LookupEntry(uint32 id) const
        {
            RecordMap::const_iterator find = m_indexMap.find(id);
            if (find != m_indexMap.end())
                return reinterpret_cast<T const*>(find->second);
            return nullptr;
        }

        void Load();

        void EraseEntry(uint32 id);

    protected:
        void prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize) override;
        void JustCreatedRecord(uint32 recordId, char* record) override
        {
            m_indexMap[recordId] = record;
        }

        void Free() override;

    private:
        typedef std::unordered_map<uint32 /*recordId*/, char* /*record*/> RecordMap;
        RecordMap m_indexMap;
};

// 基于 multimap 一对多索引的 SQL 存储类（支持同一键对应多条记录） | Multimap-indexed SQL storage supporting multiple records per key
class SQLMultiStorage : public SQLStorageBase
{
        template<class DerivedLoader, class StorageClass> friend class SQLStorageLoaderBase;
        template<typename T> friend class SQLMultiSIterator;
        template<typename T> friend class SQLMSIteratorBounds;

    private:
        typedef std::multimap<uint32 /*recordId*/, char* /*record*/> RecordMultiMap;

    public:
        SQLMultiStorage(const char* fmt, const char* _entry_field, const char* sqlname);
        SQLMultiStorage(const char* src_fmt, const char* dst_fmt, const char* _entry_field, const char* sqlname);

        ~SQLMultiStorage() override { Free(); }

        // forward declaration | 前向声明范围边界包装类
        template<typename T> class SQLMSIteratorBounds;

        // multimap 记录迭代器 | Iterator over multimap entries
        template<typename T>
        class SQLMultiSIterator
        {
                friend class SQLMultiStorage;
                friend class SQLMSIteratorBounds<T>;

            public:
                T const* getValue() const { return reinterpret_cast<T const*>(citerator->second); }
                uint32 getKey() const { return citerator->first; }

                void operator ++() { ++citerator; }
                T const* operator *() const { return getValue(); }
                T const* operator ->() const { return getValue(); }
                bool operator !=(const SQLMultiSIterator& r) const { return citerator != r.citerator; }
                bool operator ==(const SQLMultiSIterator& r) const { return citerator == r.citerator; }

            private:
                SQLMultiSIterator(RecordMultiMap::const_iterator _itr) : citerator(_itr) {}
                RecordMultiMap::const_iterator citerator;
        };

        // 指定键对应记录区间的起止迭代器对 | Begin/end iterator pair for a key's equal_range
        template<typename T>
        class SQLMSIteratorBounds
        {
                friend class SQLMultiStorage;

            public:
                const SQLMultiSIterator<T> first;
                const SQLMultiSIterator<T> second;

            private:
                SQLMSIteratorBounds(std::pair<RecordMultiMap::const_iterator, RecordMultiMap::const_iterator> pair) : first(pair.first), second(pair.second) {}
        };

        // 获取指定键对应的所有记录范围 | Get iterator bounds for all records matching key
        template<typename T>
        SQLMSIteratorBounds<T> getBounds(uint32 key) const { return SQLMSIteratorBounds<T>(m_indexMultiMap.equal_range(key)); }

        void Load();

        void EraseEntry(uint32 id);

    protected:
        void prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize) override;
        void JustCreatedRecord(uint32 recordId, char* record) override
        {
            m_indexMultiMap.insert(RecordMultiMap::value_type(recordId, record));
        }

        void Free() override;

    private:
        RecordMultiMap m_indexMultiMap;
};

// CRTP SQL 存储加载器基类，负责从数据库读取行并按源/目标格式转换写入结构体内存 | CRTP loader base converting DB rows into target struct memory according to src/dst formats
template <class DerivedLoader, class StorageClass>
class SQLStorageLoaderBase
{
    public:
        // 执行查询并将整张表加载到 store 中 | Query table and populate storage container
        void Load(StorageClass& store, bool error_at_empty = true);

        // 各类型字段转换与默认填充辅助方法 | Field conversion and default-fill helpers
        template<class S, class D>
        void convert(uint32 field_pos, S src, D& dst);
        template<class S>
        void convert_to_bool(uint32 field_pos, S src, bool& dst);
        template<class S>
        void convert_to_str(uint32 field_pos, S src, char*& dst);
        template<class D>
        void convert_from_str(uint32 field_pos, char const* src, D& dst);
        void convert_str_to_bool(uint32 field_pos, char const* src, bool& dst);
        void convert_str_to_str(uint32 field_pos, char const* src, char*& dst);
        template<class S, class D>
        void default_fill(uint32 field_pos, S src, D& dst);
        void default_fill_to_str(uint32 field_pos, char const* src, char*& dst);

        // trap, no body | 禁用非常量 char* 重载的编译期陷阱（无实现体）
        template<class D>
        void convert_from_str(uint32 field_pos, char* src, D& dst);
        void convert_str_to_str(uint32 field_pos, char* src, char*& dst);

    private:
        // 将源值按目标字段格式转换并写入记录偏移位置 | Convert source value to destination format and write at record offset
        template<class V>
        void storeValue(V value, StorageClass& store, char* p, uint32 x, uint32& offset);
        void storeValue(char const* value, StorageClass& store, char* p, uint32 x, uint32& offset);

        // trap, no body | 禁用非常量 char* 重载的编译期陷阱（无实现体）
        void storeValue(char* value, StorageClass& store, char* record, uint32 field_pos, uint32& offset);
};

// 分别对应三种存储容器的具体加载器 | Concrete loaders for each storage container type
class SQLStorageLoader : public SQLStorageLoaderBase<SQLStorageLoader, SQLStorage>
{
};

class SQLHashStorageLoader : public SQLStorageLoaderBase<SQLHashStorageLoader, SQLHashStorage>
{
};

class SQLMultiStorageLoader : public SQLStorageLoaderBase<SQLMultiStorageLoader, SQLMultiStorage>
{
};

#include "SQLStorageImpl.h"

#endif
