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

#ifndef SQLPREPAREDSTATEMENTS_H
#define SQLPREPAREDSTATEMENTS_H

#include "Common.h"

#include <vector>
#include <stdexcept>

class Database;
class SqlConnection;
class QueryResult;

// 预处理语句参数值的二进制联合体 | Union holding binary value of a prepared statement parameter
union SqlStmtField
{
    bool boolean;
    uint8 ui8;
    int8 i8;
    uint16 ui16;
    int16 i16;
    uint32 ui32;
    int32 i32;
    uint64 ui64;
    int64 i64;
    float f;
    double d;
};

// 预处理语句参数数据类型枚举 | Supported data types for prepared statement parameters
enum SqlStmtFieldType
{
    FIELD_BOOL,
    FIELD_UI8,
    FIELD_UI16,
    FIELD_UI32,
    FIELD_UI64,
    FIELD_I8,
    FIELD_I16,
    FIELD_I32,
    FIELD_I64,
    FIELD_FLOAT,
    FIELD_DOUBLE,
    FIELD_STRING,
    FIELD_NONE
};

// templates might be the best choice here | 此处原本模板可能是更好选择，
// but I didn't have time to play with them | 但作者当时未及深入重构
// 单个预处理参数字段数据包装类（区分基础数值联合体与字符串） | Single prepared statement parameter wrapper (binary union or std::string)
class SqlStmtFieldData
{
    public:
        SqlStmtFieldData() : m_type(FIELD_NONE) { m_binaryData.ui64 = 0; }
        ~SqlStmtFieldData() {}

        template<typename T>
        SqlStmtFieldData(T param) { set(param); }

        template<typename T1>
        void set(T1 param1);

        // getters | 类型安全取值方法（带断言校验）
        bool toBool() const { MANGOS_ASSERT(m_type == FIELD_BOOL); return m_binaryData.ui8 != 0; }
        uint8 toUint8() const { MANGOS_ASSERT(m_type == FIELD_UI8); return m_binaryData.ui8; }
        int8 toInt8() const { MANGOS_ASSERT(m_type == FIELD_I8); return m_binaryData.i8; }
        uint16 toUint16() const { MANGOS_ASSERT(m_type == FIELD_UI16); return m_binaryData.ui16; }
        int16 toInt16() const { MANGOS_ASSERT(m_type == FIELD_I16); return m_binaryData.i16; }
        uint32 toUint32() const { MANGOS_ASSERT(m_type == FIELD_UI32); return m_binaryData.ui32; }
        int32 toInt32() const { MANGOS_ASSERT(m_type == FIELD_I32); return m_binaryData.i32; }
        uint64 toUint64() const { MANGOS_ASSERT(m_type == FIELD_UI64); return m_binaryData.ui64; }
        int64 toInt64() const { MANGOS_ASSERT(m_type == FIELD_I64); return m_binaryData.i64; }
        float toFloat() const { MANGOS_ASSERT(m_type == FIELD_FLOAT); return m_binaryData.f; }
        double toDouble() const { MANGOS_ASSERT(m_type == FIELD_DOUBLE); return m_binaryData.d; }
        const char* toStr() const { MANGOS_ASSERT(m_type == FIELD_STRING); return m_szStringData.c_str(); }

        // get type of data | 获取参数字段类型
        SqlStmtFieldType type() const { return m_type; }
        // get underlying buffer type | 获取底层数据缓冲区指针（字符串或数值联合体）
        void* buff() const { return m_type == FIELD_STRING ? (void*)m_szStringData.c_str() : (void*)&m_binaryData; }

        // get size of data | 获取参数字节长度
        size_t size() const
        {
            switch (m_type)
            {
                case FIELD_NONE:    return 0;
                case FIELD_BOOL:    // return sizeof(bool); | 布尔类型按 uint8 字节大小处理
                case FIELD_UI8:     return sizeof(uint8);
                case FIELD_UI16:    return sizeof(uint16);
                case FIELD_UI32:    return sizeof(uint32);
                case FIELD_UI64:    return sizeof(uint64);
                case FIELD_I8:      return sizeof(int8);
                case FIELD_I16:     return sizeof(int16);
                case FIELD_I32:     return sizeof(int32);
                case FIELD_I64:     return sizeof(int64);
                case FIELD_FLOAT:   return sizeof(float);
                case FIELD_DOUBLE:  return sizeof(double);
                case FIELD_STRING:  return m_szStringData.length();

                default:
                    throw std::runtime_error("unrecognized type of SqlStmtFieldType obtained");
            }
        }

    private:
        SqlStmtFieldType m_type;
        SqlStmtField m_binaryData;
        std::string m_szStringData;
};

// template specialization | 各基础类型的 set() 模板特化
template<> inline void SqlStmtFieldData::set(bool val) { m_type = FIELD_BOOL; m_binaryData.ui8 = val; }
template<> inline void SqlStmtFieldData::set(uint8 val) { m_type = FIELD_UI8; m_binaryData.ui8 = val; }
template<> inline void SqlStmtFieldData::set(int8 val) { m_type = FIELD_I8; m_binaryData.i8 = val; }
template<> inline void SqlStmtFieldData::set(uint16 val) { m_type = FIELD_UI16; m_binaryData.ui16 = val; }
template<> inline void SqlStmtFieldData::set(int16 val) { m_type = FIELD_I16; m_binaryData.i16 = val; }
template<> inline void SqlStmtFieldData::set(uint32 val) { m_type = FIELD_UI32; m_binaryData.ui32 = val; }
template<> inline void SqlStmtFieldData::set(int32 val) { m_type = FIELD_I32; m_binaryData.i32 = val; }
template<> inline void SqlStmtFieldData::set(uint64 val) { m_type = FIELD_UI64; m_binaryData.ui64 = val; }
template<> inline void SqlStmtFieldData::set(int64 val) { m_type = FIELD_I64; m_binaryData.i64 = val; }
template<> inline void SqlStmtFieldData::set(float val) { m_type = FIELD_FLOAT; m_binaryData.f = val; }
template<> inline void SqlStmtFieldData::set(double val) { m_type = FIELD_DOUBLE; m_binaryData.d = val; }
template<> inline void SqlStmtFieldData::set(const char* val) { m_type = FIELD_STRING; m_szStringData = val; }

class SqlStatement;
// prepared statement executor | 预处理语句输入参数列表容器
class SqlStmtParameters
{
    public:
        typedef std::vector<SqlStmtFieldData> ParameterContainer;

        // reserve memory to contain all input parameters of stmt | 预留足够容纳全部输入参数的容器容量
        explicit SqlStmtParameters(uint32 nParams);

        // get amount of bound parameters | 获取已绑定的参数个数
        uint32 boundParams() const { return m_params.size(); }
        // add parameter | 追加一个参数
        void addParam(const SqlStmtFieldData& data) { m_params.push_back(data); }
        // empty SQL statement parameters. In case nParams > 1 - reserve memory for parameters | 清空参数并按语句所需参数数重新预留空间，
        // should help to reuse the same object with batched SQL requests | 便于在批量 SQL 请求中复用对象
        void reset(const SqlStatement& stmt);

        // get bound parameters | 获取已绑定的参数只读列表
        const ParameterContainer& params() const { return m_params; }

    private:
        // statement parameter holder | 语句参数容器
        ParameterContainer m_params;
};

// statement ID encapsulation logic | 预处理语句 ID 封装类（缓存注册索引与参数数量）
class SqlStatementID
{
    public:
        SqlStatementID() : m_bInitialized(false) {}

        int ID() const { return m_nIndex; }
        uint32 arguments() const { return m_nArguments; }
        bool initialized() const { return m_bInitialized; }

    private:
        friend class Database;
        void init(int nID, uint32 nArgs) { m_nIndex = nID; m_nArguments = nArgs; m_bInitialized = true; }

        int m_nIndex;
        uint32 m_nArguments;
        bool m_bInitialized;
};

// statement index | 面向调用方的预处理语句句柄，支持链式绑定参数与异步/同步执行
class SqlStatement
{
    public:
        ~SqlStatement() { delete m_pParams; }

        SqlStatement(const SqlStatement& index) : m_index(index.m_index), m_pDB(index.m_pDB), m_pParams(nullptr)
        {
            if (index.m_pParams)
                m_pParams = new SqlStmtParameters(*(index.m_pParams));
        }

        SqlStatement& operator=(const SqlStatement& index);

        int ID() const { return m_index.ID(); }
        uint32 arguments() const { return m_index.arguments(); }

        // 提交预处理语句到异步队列或当前事务执行 | Execute statement asynchronously or within active transaction
        bool Execute();
        // 同步直接执行预处理语句 | Execute statement synchronously on async connection
        bool DirectExecute();

        // templates to simplify 1-4 parameter bindings | 简化 1~4 个参数绑定并执行的模板方法
        template<typename ParamType1>
        bool PExecute(ParamType1 param1)
        {
            arg(param1);
            return Execute();
        }

        template<typename ParamType1, typename ParamType2>
        bool PExecute(ParamType1 param1, ParamType2 param2)
        {
            arg(param1);
            arg(param2);
            return Execute();
        }

        template<typename ParamType1, typename ParamType2, typename ParamType3>
        bool PExecute(ParamType1 param1, ParamType2 param2, ParamType3 param3)
        {
            arg(param1);
            arg(param2);
            arg(param3);
            return Execute();
        }

        template<typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
        bool PExecute(ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
        {
            arg(param1);
            arg(param2);
            arg(param3);
            arg(param4);
            return Execute();
        }

        // bind parameters with specified type | 按明确类型绑定单个输入参数
        void addBool(bool var) { arg(var); }
        void addUInt8(uint8 var) { arg(var); }
        void addInt8(int8 var) { arg(var); }
        void addUInt16(uint16 var) { arg(var); }
        void addInt16(int16 var) { arg(var); }
        void addUInt32(uint32 var) { arg(var); }
        void addInt32(int32 var) { arg(var); }
        void addUInt64(uint64 var) { arg(var); }
        void addInt64(int64 var) { arg(var); }
        void addFloat(float var) { arg(var); }
        void addDouble(double var) { arg(var); }
        void addString(const char* var) { arg(var); }
        void addString(const std::string& var) { arg(var.c_str()); }
        void addString(std::ostringstream& ss) { arg(ss.str().c_str()); ss.str(std::string()); }

    protected:
        // don't allow anyone except Database class to create static SqlStatement objects | 仅允许 Database 类创建 SqlStatement 对象
        friend class Database;
        SqlStatement(const SqlStatementID& index, Database& db) : m_index(index), m_pDB(&db), m_pParams(nullptr) {}

    private:

        // 懒加载获取参数容器 | Lazily allocate parameter container
        SqlStmtParameters* get()
        {
            if (!m_pParams)
                m_pParams = new SqlStmtParameters(arguments());

            return m_pParams;
        }

        // 分离并移交参数容器所有权（供执行后异步释放） | Detach and transfer ownership of parameter container
        SqlStmtParameters* detach()
        {
            SqlStmtParameters* p = m_pParams ? m_pParams : new SqlStmtParameters(0);
            m_pParams = nullptr;
            return p;
        }

        // helper function | 内部参数绑定辅助函数
        // use appropriate add* functions to bind specific data type | 请外部调用对应的 add* 方法绑定指定数据类型
        template<typename ParamType>
        void arg(ParamType val)
        {
            SqlStmtParameters* p = get();
            p->addParam(SqlStmtFieldData(val));
        }

        SqlStatementID m_index;
        Database* m_pDB;
        SqlStmtParameters* m_pParams;
};

// base prepared statement class | 底层数据库连接级预处理语句抽象基类
class SqlPreparedStatement
{
    public:
        virtual ~SqlPreparedStatement() {}

        bool isPrepared() const { return m_bPrepared; }
        bool isQuery() const { return m_bIsQuery; }

        uint32 params() const { return m_nParams; }
        uint32 columns() const { return isQuery() ? m_nColumns : 0; }

        // initialize internal structures of prepared statement | 初始化预处理语句内部结构，
        // upon success m_bPrepared should be true | 成功后应将 m_bPrepared 置为 true
        virtual bool prepare() = 0;
        // bind parameters for prepared statement from parameter placeholder | 从参数容器绑定输入参数
        virtual void bind(const SqlStmtParameters& holder) = 0;

        // execute statement w/o result set | 执行无结果集的预处理语句
        virtual bool execute() = 0;

    protected:
        SqlPreparedStatement(const std::string& fmt, SqlConnection& conn) :
            m_nParams(0), m_nColumns(0), m_bIsQuery(false),
            m_bPrepared(false), m_szFmt(fmt), m_pConn(conn)
        {}

        uint32 m_nParams;
        uint32 m_nColumns;
        bool m_bIsQuery;
        bool m_bPrepared;
        std::string m_szFmt;
        SqlConnection& m_pConn;
};

// prepared statements via plain SQL string requests | 基于纯文本占位符替换的通用预处理语句回退实现
class SqlPlainPreparedStatement : public SqlPreparedStatement
{
    public:
        SqlPlainPreparedStatement(const std::string& fmt, SqlConnection& conn);
        ~SqlPlainPreparedStatement() {}

        // this statement is always prepared | 纯文本替换实现始终处于已准备状态
        virtual bool prepare() override { return true; }

        // we should replace all '?' symbols with substrings with proper format | 将模板中的 '?' 逐一替换为安全转义后的参数文本
        virtual void bind(const SqlStmtParameters& holder) override;

        virtual bool execute() override;

    protected:
        // 将单个参数字段格式化为带引号并转义的 SQL 字符串 | Format a single parameter field into an escaped SQL literal
        void DataToString(const SqlStmtFieldData& data, std::ostringstream& fmt) const;

        std::string m_szPlainRequest;
};

#endif
