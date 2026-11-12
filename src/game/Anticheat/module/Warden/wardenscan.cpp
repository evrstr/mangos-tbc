/*
 * Copyright (C) 2017-2020 namreeb (legal@namreeb.org)
 *
 * This is private software and may not be shared under any circumstances,
 * absent permission of namreeb.
 */

#include "WardenScan.hpp"
#include "WardenWin.hpp"
#include "WardenModule.hpp"
#include "Util/ByteBuffer.h"
#include "Util/Util.h"
#include "Auth/CryptoHash.h"
#include "Auth/HMACSHA1.h"

#include <string>
#include <algorithm>
#include <functional>

// 构造模块存在性扫描（按模块名哈希判定目标 DLL 是否加载，返回状态是否与 _wanted 预期一致）
WindowsModuleScan::WindowsModuleScan(const std::string &module, bool wanted, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder (构造模块名 HMAC-SHA1 查询请求)
    [this](const Warden *warden, std::vector<std::string> &, ByteBuffer &scan)
    {
        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);
        auto const seed = static_cast<uint32>(urand());

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[FIND_MODULE_BY_NAME] ^ winWarden->GetXor()) << seed;

        HMACSHA1 hash(sizeof(seed), reinterpret_cast<const uint8*>(&seed));
        hash.UpdateData(this->_module);
        hash.Finalize();

        scan.append(hash.GetDigest(), hash.GetLength());
    },
    // checker (核验模块是否存在，与 _wanted 不符则判定违规)
    [this](const Warden *, ByteBuffer &buff)
    {
        auto const found = buff.read<uint8>() == ModuleFound;
        return found != this->_wanted;
    }, sizeof(uint8) + sizeof(uint32) + Sha1Hash::GetLength(), sizeof(uint8), comment, flags),
    _module(module), _wanted(wanted)
{
    // the game depends on uppercase module names being sent (游戏内模块名比对统一要求大写)
    std::transform(_module.begin(), _module.end(), _module.begin(), ::toupper);
}

// 构造带自定义核验回调的模块扫描
WindowsModuleScan::WindowsModuleScan(const std::string &module, CheckT checker, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &, ByteBuffer &scan)
    {
        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);
        auto const seed = static_cast<uint32>(urand());

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[FIND_MODULE_BY_NAME] ^ winWarden->GetXor()) << seed;

        HMACSHA1 hash(sizeof(seed), reinterpret_cast<const uint8*>(&seed));
        hash.UpdateData(this->_module);
        hash.Finalize();

        scan.append(hash.GetDigest(), hash.GetLength());
    },
    checker, sizeof(uint8) + sizeof(uint32) + Sha1Hash::GetLength(), sizeof(uint8), comment, flags),
    _module(module)
{
    // the game depends on uppercase module names being sent (统一转为大写模块名)
    std::transform(_module.begin(), _module.end(), _module.begin(), ::toupper);
}

// 构造主程序绝对地址内存字节比对扫描
WindowsMemoryScan::WindowsMemoryScan(uint32 offset, const void *expected, size_t length, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder (字符串索引 0 表示直接读取主模块基址偏移)
    [this](const Warden *warden, std::vector<std::string> &, ByteBuffer &scan)
    {
        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[READ_MEMORY] ^ winWarden->GetXor())
             << static_cast<uint8>(0)   // no string associated with this form of the constructor (无关联模块名字符串)
             << this->_offset
             << static_cast<uint8>(this->_expected.size());
    },
    // checker (读取状态非零或内存内容与预期不一致时返回 true)
    [this](const Warden *, ByteBuffer &buff)
    {
        // non-zero value indicates failure (非 0 表示读取失败)
        if (!!buff.read<uint8>())
            return true;

        auto const result = !!memcmp(buff.contents() + buff.rpos(), &this->_expected[0], this->_expected.size());
        buff.rpos(buff.rpos() + this->_expected.size());
        return result;
    }, sizeof(uint8) + sizeof(uint8) + sizeof(uint32) + sizeof(uint8), sizeof(uint8) + length, comment, flags),
    _expected(length), _offset(offset)
{
    // must fit within uint8 (单次读取长度受单字节限制不超过 255)
    MANGOS_ASSERT(_expected.size() <= 0xFF);

    ::memcpy(&_expected[0], expected, _expected.size());
}

// 构造指定模块相对偏移的内存字节比对扫描
WindowsMemoryScan::WindowsMemoryScan(const std::string &module, uint32 offset, const void *expected, size_t length, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder (将模块名写入字符串表，发送 1-based 字符串索引)
    [this](const Warden *warden, std::vector<std::string> &strings, ByteBuffer &scan)
    {
        MANGOS_ASSERT(strings.size() < 0xFF);

        strings.emplace_back(this->_module);

        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[READ_MEMORY] ^ winWarden->GetXor())
             << static_cast<uint8>(strings.size())
             << this->_offset
             << static_cast<uint8>(this->_expected.size());
    },
    // checker
    [this](const Warden *, ByteBuffer &buff)
    {
        // non-zero value indicates failure
        if (!!buff.read<uint8>())
            return true;

        auto const result = !!memcmp(buff.contents() + buff.rpos(), &this->_expected[0], this->_expected.size());
        buff.rpos(buff.rpos() + this->_expected.size());
        return result;
    }, module.length() + sizeof(uint8) + sizeof(uint8) + sizeof(uint32) + sizeof(uint8), sizeof(uint8) + length, comment, flags),
    _expected(length), _offset(offset), _module(module)
{
    // must fit within uint8
    MANGOS_ASSERT(_expected.size() <= 0xFF);

    ::memcpy(&_expected[0], expected, _expected.size());

    // since this scan uses GetModuleHandle() rather than GetFirstModule()/GetNextModule(), this is case insensitive.
    // but still it seems prudent to be consistent
    // GetModuleHandle 不区分大小写，但仍统一转为大写保持规范
    std::transform(_module.begin(), _module.end(), _module.begin(), ::toupper);
}

// 构造带自定义核验回调的绝对地址内存读取扫描
WindowsMemoryScan::WindowsMemoryScan(uint32 offset, size_t length, CheckT checker, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &, ByteBuffer &scan)
    {
        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[READ_MEMORY] ^ winWarden->GetXor())
             << static_cast<uint8>(0)   // no string associated with this form of the constructor
             << this->_offset
             << static_cast<uint8>(this->_expected.size());
    }, checker, sizeof(uint8) + sizeof(uint8) + sizeof(uint32) + sizeof(uint8), sizeof(uint8) + length, comment, flags),
    _expected(length), _offset(offset)
{
    MANGOS_ASSERT(_expected.size() <= 0xFF);
}

// 构造带自定义核验回调的模块偏移内存读取扫描
WindowsMemoryScan::WindowsMemoryScan(const std::string &module, uint32 offset, size_t length, CheckT checker, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &strings, ByteBuffer &scan)
    {
        MANGOS_ASSERT(strings.size() < 0xFF);

        strings.emplace_back(this->_module);

        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[READ_MEMORY] ^ winWarden->GetXor())
             << static_cast<uint8>(strings.size())
             << this->_offset
             << static_cast<uint8>(this->_expected.size());
    }, checker, module.length() + sizeof(uint8) + sizeof(uint8) + sizeof(uint32) + sizeof(uint8), sizeof(uint8) + length, comment, flags),
    _expected(length), _offset(offset), _module(module)
    {}

// 构造代码特征码哈希扫描（在指定偏移或可执行映像内搜索特定机器码序列的 HMAC-SHA1）
WindowsCodeScan::WindowsCodeScan(uint32 offset, const std::vector<uint8> &pattern, bool memImageOnly, bool wanted, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &, ByteBuffer &scan)
    {
        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);
        auto const seed = static_cast<uint32>(urand());

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[this->_memImageOnly ? FIND_MEM_IMAGE_CODE_BY_HASH : FIND_CODE_BY_HASH] ^ winWarden->GetXor())
             << seed;

        HMACSHA1 hash(sizeof(seed), reinterpret_cast<const uint8*>(&seed));
        hash.UpdateData(&this->_pattern[0], this->_pattern.size());
        hash.Finalize();
        
        scan.append(hash.GetDigest(), hash.GetLength());

        scan << this->_offset << static_cast<uint8>(this->_pattern.size());
    },
    [this](const Warden *, ByteBuffer &buff)
    {
        auto const found = buff.read<uint8>() == PatternFound;
        return found != this->_wanted;
    }, sizeof(uint8) + sizeof(uint32) + Sha1Hash::GetLength() + sizeof(uint32) + sizeof(uint8), sizeof(uint8), comment, flags),
    _offset(offset), _pattern(pattern), _memImageOnly(memImageOnly), _wanted(wanted)
{
    MANGOS_ASSERT(_pattern.size() <= 0xFF);
}

// 构造客户端 MPQ/本地文件存在性及 SHA-1 哈希校验扫描
WindowsFileHashScan::WindowsFileHashScan(const std::string &file, const void *expected, bool wanted, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &strings, ByteBuffer &scan)
    {
        MANGOS_ASSERT(strings.size() < 0xFF);

        strings.emplace_back(this->_file);

        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[HASH_CLIENT_FILE] ^ winWarden->GetXor())
             << static_cast<uint8>(strings.size());
    },
    // checker
    [this](const Warden *, ByteBuffer &buff)
    {
        auto const success = !buff.read<uint8>();

        // either we wanted it but didn't find it, or didn't want it but did find it (存在状态与预期不一致则违规)
        if (this->_wanted != success)
            return true;

        // if we didn't want it and didn't find it, succeed (不期望存在且确实未找到，直接通过)
        if (!this->_wanted && !success)
            return false;

        uint8 hash[Sha1Hash::GetLength()];

        buff.read(hash, sizeof(hash));

        // if a hash was given, check it (some checks may only be interested in existence) (若指定了期望哈希则继续比对文件内容)
        return this->_hashMatch && !!memcmp(hash, this->_expected, sizeof(hash));
    }, sizeof(uint8) + sizeof(uint8) + file.length(), sizeof(uint8) + Sha1Hash::GetLength(), comment, flags),
    _file(file), _hashMatch(!!expected), _wanted(wanted)
{
    if (_hashMatch)
        ::memcpy(_expected, expected, sizeof(_expected));
}

// 构造客户端 Lua 全局变量存在性检查扫描
WindowsLuaScan::WindowsLuaScan(const std::string &lua, bool wanted, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &strings, ByteBuffer &scan)
    {
        MANGOS_ASSERT(strings.size() < 0xFF);

        strings.emplace_back(this->_lua);

        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[GET_LUA_VARIABLE] ^ winWarden->GetXor())
            << static_cast<uint8>(strings.size());
    },
    // checker
    [this](const Warden *, ByteBuffer &buff)
    {
        auto const found = !buff.read<uint8>();

        // if its found we have to 'read' the string, even if we don't care what it is (即使不校验值也必须跳过响应字符串字节，以免污染后续扫描的缓冲区读指针)
        if (found)
        {
            auto const length = buff.read<uint8>();
            buff.rpos(buff.rpos() + length);
        }

        return found != this->_wanted;
    }, sizeof(uint8) + sizeof(uint8) + lua.length(), sizeof(uint8) + 0xFF, comment, flags),
    _lua(lua), _wanted(wanted)
    {}

// 构造客户端 Lua 全局变量值精确匹配扫描
WindowsLuaScan::WindowsLuaScan(const std::string &lua, const std::string &expectedValue, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &strings, ByteBuffer &scan)
    {
        MANGOS_ASSERT(strings.size() < 0xFF);

        strings.emplace_back(this->_lua);

        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[GET_LUA_VARIABLE] ^ winWarden->GetXor())
            << static_cast<uint8>(strings.size());
    },
    // checker
    [this](const Warden *, ByteBuffer &buff)
    {
        auto const result = buff.read<uint8>();

        if (!!result)
            return true;

        const size_t len = buff.read<uint8>();

        const std::string str(reinterpret_cast<const char *>(buff.contents() + buff.rpos()), len);

        buff.rpos(buff.rpos() + len);

        return str == this->_expectedValue;
    }, sizeof(uint8) + sizeof(uint8) + lua.length(), sizeof(uint8) + 0xFF, comment, flags),
    _lua(lua), _expectedValue(expectedValue)
{
    MANGOS_ASSERT(expectedValue.length() <= 0xFF);
}

// 构造带自定义核验回调的 Lua 变量扫描
WindowsLuaScan::WindowsLuaScan(const std::string &lua, CheckT checker, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &strings, ByteBuffer &scan)
    {
        MANGOS_ASSERT(strings.size() < 0xFF);

        strings.emplace_back(this->_lua);

        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[GET_LUA_VARIABLE] ^ winWarden->GetXor())
            << static_cast<uint8>(strings.size());
    }, checker, sizeof(uint8) + sizeof(uint8) + lua.length(), sizeof(uint8) + 0xFF, comment, flags),
    _lua(lua)
{
    MANGOS_ASSERT(checker);
}

// 构造系统 API Hook 校验扫描（检查指定 DLL 导出函数前若干字节是否被 Detour 篡改）
WindowsHookScan::WindowsHookScan(const std::string &module, const std::string &proc, const void *hash,
    uint32 offset, size_t length, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &strings, ByteBuffer &scan)
    {
        MANGOS_ASSERT(strings.size() < 0xFE);

        strings.emplace_back(this->_module);
        strings.emplace_back(this->_proc);

        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);
        auto const seed = static_cast<uint32>(urand());

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[API_CHECK] ^ winWarden->GetXor()) << seed;

        scan.append(this->_hash, sizeof(this->_hash));
        scan << static_cast<uint8>(strings.size() - 1)
             << static_cast<uint8>(strings.size())
             << this->_offset
             << static_cast<uint8>(this->_length);
    },
    // checker
    [](const Warden *, ByteBuffer &buff)
    {
        return buff.read<uint8>() == Detoured;
    },
    sizeof(uint8) + sizeof(uint32) + Sha1Hash::GetLength() + sizeof(uint8) + sizeof(uint8) + sizeof(uint32) + sizeof(uint8), sizeof(uint8), comment, flags),
    _module(module), _proc(proc), _offset(offset), _length(length)
{
    MANGOS_ASSERT(length <= 0xFF);
    ::memcpy(_hash, hash, sizeof(_hash));
}

// 构造 Windows 内核驱动存在性扫描
WindowsDriverScan::WindowsDriverScan(const std::string &name, const std::string &targetPath, bool wanted, const std::string &comment, uint32 flags)
    : WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &strings, ByteBuffer &scan)
    {
        MANGOS_ASSERT(strings.size() < 0xFF);

        strings.emplace_back(this->_name);

        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);
        auto const seed = static_cast<uint32>(urand());

        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[FIND_DRIVER_BY_NAME] ^ winWarden->GetXor()) << seed;

        HMACSHA1 hash(sizeof(seed), reinterpret_cast<const uint8*>(&seed));
        hash.UpdateData(this->_targetPath);
        hash.Finalize();

        scan.append(hash.GetDigest(), hash.GetLength());
        scan << static_cast<uint8>(strings.size());
    },
    // checker
    [this](const Warden *, ByteBuffer &buff)
    {
        auto const found = buff.read<uint8>() == Found;

        return found != this->_wanted;
    }, sizeof(uint8) + sizeof(uint32) + Sha1Hash::GetLength() + sizeof(uint8) + name.length(), sizeof(uint8), comment, flags),
    _name(name), _targetPath(targetPath), _wanted(wanted)
    {}

// 构造客户端高精度计时器检查扫描
WindowsTimeScan::WindowsTimeScan(CheckT checker, const std::string &comment, uint32 flags) :
    WindowsScan(
    // builder
    [this](const Warden *warden, std::vector<std::string> &, ByteBuffer &scan)
    {
        auto const winWarden = reinterpret_cast<const WardenWin *>(warden);
        scan << static_cast<uint8>(winWarden->GetModule()->opcodes[CHECK_TIMING_VALUES] ^ winWarden->GetXor());
    }, checker, sizeof(uint8), sizeof(uint8) + sizeof(uint32), comment, flags)
{
    MANGOS_ASSERT(!!checker);
}
