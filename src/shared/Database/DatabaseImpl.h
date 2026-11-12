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

#include "Database/Database.h"
#include "Database/SqlOperations.h"

#include <cstdarg>
#include <functional>

/// Function body definitions for the template function members of the Database class | Database 类异步查询模板成员函数的实现体

// 检查 QueryHolder 与结果队列是否有效 | Validate QueryHolder pointer and result queue
#define ASYNC_DELAYHOLDER_BODY(holder) if (!holder || !m_pResultQueue) return false;

// 格式化异步查询字符串辅助宏 | Helper macro to format async SQL query string
#define ASYNC_PQUERY_BODY(format, szQuery) \
    if(!format) return false; \
    \
    char szQuery [MAX_QUERY_LEN]; \
    \
    { \
        va_list ap; \
        \
        va_start(ap, format); \
        int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap ); \
        va_end(ap); \
        \
        if(res==-1) \
        { \
            sLog.outError("SQL Query truncated (and not execute) for format: %s",format); \
            return false; \
        } \
    }

// -- Query -- | 异步原始 SQL 查询

template <typename Callable>
bool
Database::AsyncQuery(Callable callback, const char* sql)
{
    if (!sql || !m_pResultQueue)
    {
        return false;
    }
    return m_threadBody->Delay(new SqlQuery(sql, new MaNGOS::QueryCallback(std::move(callback)), m_pResultQueue));
}

// -- PQuery -- | 异步格式化 SQL 查询

template <typename Callable>
bool
Database::AsyncPQuery(Callable callback, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    return AsyncQuery(std::move(callback), szQuery);
}

// -- PQuery / member -- | 异步格式化查询（绑定对象成员函数回调）

template<class Class>
bool
Database::AsyncPQuery(Class* object, void (Class::*method)(QueryResult*), const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    auto callback = std::bind(method, object);
    return AsyncQuery(std::move(callback), szQuery);
}

template<class Class, typename ParamType1>
bool
Database::AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1), ParamType1 param1, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    auto callback = std::bind(method, object, std::placeholders::_1, param1);
    return AsyncQuery(std::move(callback), szQuery);
}

template<class Class, typename ParamType1, typename ParamType2>
bool
Database::AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    auto callback = std::bind(method, object, std::placeholders::_1, param1, param2);
    return AsyncQuery(std::move(callback), szQuery);
}

template<class Class, typename ParamType1, typename ParamType2, typename ParamType3>
bool
Database::AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    auto callback = std::bind(method, object, std::placeholders::_1, param1, param2, param3);
    return AsyncQuery(std::move(callback), szQuery);
}

// -- PQuery / static -- | 异步格式化查询（绑定静态或全局函数回调）

template<typename ParamType1>
bool
Database::AsyncPQuery(void (*method)(QueryResult*, ParamType1), ParamType1 param1, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    auto callback = std::bind(method, std::placeholders::_1, param1);
    return AsyncQuery(std::move(callback), szQuery);
}

template<typename ParamType1, typename ParamType2>
bool
Database::AsyncPQuery(void (*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    auto callback = std::bind(method, std::placeholders::_1, param1, param2);
    return AsyncQuery(std::move(callback), szQuery);
}

template<typename ParamType1, typename ParamType2, typename ParamType3>
bool
Database::AsyncPQuery(void (*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    auto callback = std::bind(method, std::placeholders::_1, param1, param2, param3);
    return AsyncQuery(std::move(callback), szQuery);
}

// -- QueryHolder -- | 异步查询持有器（批量查询）延迟执行

template<class Class>
bool
Database::DelayQueryHolder(Class* object, void (Class::*method)(QueryResult*, SqlQueryHolder*), SqlQueryHolder* holder)
{
    ASYNC_DELAYHOLDER_BODY(holder)
    auto callback = std::bind(method, object, std::placeholders::_1, holder);
    return holder->Execute(new MaNGOS::QueryCallback(std::move(callback)), m_threadBody, m_pResultQueue);
}

template<class Class, typename ParamType1>
bool
Database::DelayQueryHolder(Class* object, void (Class::*method)(QueryResult*, SqlQueryHolder*, ParamType1), SqlQueryHolder* holder, ParamType1 param1)
{
    ASYNC_DELAYHOLDER_BODY(holder)
    auto callback = std::bind(method, object, std::placeholders::_1, holder, param1);
    return holder->Execute(new MaNGOS::QueryCallback(std::move(callback)), m_threadBody, m_pResultQueue);
}

#undef ASYNC_PQUERY_BODY
#undef ASYNC_DELAYHOLDER_BODY
