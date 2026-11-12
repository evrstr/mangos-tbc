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

#ifndef DBCSTORE_H
#define DBCSTORE_H

#include "DBCFileLoader.h"

// 泛型 DBC 数据存储模板类，提供按 ID 查找、加载与本地化字符串追加 | Generic DBC storage container supporting ID lookup, loading, and localized string merging
template<class T>
class DBCStorage
{
        typedef std::list<char*> StringPoolList;
    public:
        explicit DBCStorage(const char* f) : nCount(0), fieldCount(0), fmt(f), indexTable(nullptr), m_dataTable(nullptr) { }
        ~DBCStorage() { Clear(); }

        // 按 ID 查找对应的 DBC 记录 | Lookup DBC entry by ID
        T const* LookupEntry(uint32 id) const { return (id >= nCount) ? nullptr : indexTable[id]; }
        uint32  GetNumRows() const { return nCount; }
        char const* GetFormat() const { return fmt; }
        uint32 GetFieldCount() const { return fieldCount; }

        // 从主 DBC 文件加载数据与默认字符串 | Load records and default strings from primary DBC file
        bool Load(char const* fn)
        {
            DBCFileLoader dbc;
            // Check if load was sucessful, only then continue | 仅在加载成功后继续解析
            if (!dbc.Load(fn, fmt))
                return false;

            fieldCount = dbc.GetCols();

            // load raw non-string data | 加载非字符串原始字段数据
            m_dataTable = (T*)dbc.AutoProduceData(fmt, nCount, (char**&)indexTable);

            // load strings from dbc data | 从 DBC 数据加载字符串池
            m_stringPoolList.push_back(dbc.AutoProduceStrings(fmt, (char*)m_dataTable));

            // error in dbc file at loading if nullptr | 若索引表为 nullptr 则说明加载失败
            return indexTable != nullptr;
        }

        // 从额外本地化 DBC 文件追加填充字符串 | Load localized strings from an additional locale DBC file
        bool LoadStringsFrom(char const* fn)
        {
            // DBC must be already loaded using Load | 必须先调用 Load 加载主 DBC
            if (!indexTable)
                return false;

            DBCFileLoader dbc;
            // Check if load was successful, only then continue | 仅在加载成功后继续
            if (!dbc.Load(fn, fmt))
                return false;

            // load strings from another locale dbc data | 加载其他语言版本的 DBC 字符串数据
            m_stringPoolList.push_back(dbc.AutoProduceStrings(fmt, (char*)m_dataTable));

            return true;
        }

        // 释放所有已加载的索引表、数据表与字符串池内存 | Free all loaded index table, data table, and string pools
        void Clear()
        {
            if (!indexTable)
                return;

            delete[]((char*)indexTable);
            indexTable = nullptr;
            delete[]((char*)m_dataTable);
            m_dataTable = nullptr;

            while (!m_stringPoolList.empty())
            {
                delete[] m_stringPoolList.front();
                m_stringPoolList.pop_front();
            }
            nCount = 0;
        }

        // 从索引表中移除指定 ID 的记录指针 | Erase entry pointer for given ID from index table
        void EraseEntry(uint32 id) { assert(id < nCount && "To be erased entry must be in bounds!") ; indexTable[id] = nullptr; }
        // 向索引表中插入或替换指定 ID 的记录指针 | Insert or overwrite entry pointer for given ID in index table
        void InsertEntry(T* entry, uint32 id) { assert(id < nCount && "To be inserted entry must be in bounds!"); indexTable[id] = entry; }

    private:
        uint32 nCount;
        uint32 fieldCount;
        char const* fmt;
        T** indexTable;
        T* m_dataTable;
        StringPoolList m_stringPoolList;
};

#endif
