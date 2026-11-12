/*
 * Copyright (C) 2017-2020 namreeb (legal@namreeb.org)
 *
 * This is private software and may not be shared under any circumstances,
 * absent permission of namreeb.
 */

#include "WardenModule.hpp"

#include "Platform/Define.h"
#include "Auth/CryptoHash.h"

#include <zlib.h>

#include <string>
#include <vector>
#include <fstream>
#include <exception>

// 从磁盘加载单个 Warden 模块三件套（.bin 模块二进制、.key RC4 解密密钥、.cr 挑战响应表与扫描操作码）
WardenModule::WardenModule(std::string const &bin, std::string const &kf, std::string const &cr)
{
    std::ifstream b(bin, std::ios::binary | std::ios::ate);

    if (b.fail())
        throw std::runtime_error("Failed to open Warden module binary");

    if (b.tellg() < SignatureSize + 4)
        throw std::runtime_error("Warden module too small.  Must be at least 264 bytes.");

    binary.resize(static_cast<size_t>(b.tellg()));
    b.seekg(0, std::ios::beg);

    if (!b.read(reinterpret_cast<char *>(&binary[0]), binary.size()))
        throw std::runtime_error("Failed to open: " + bin);

    b.close();

    // compute md5 hash of encrypted/compressed data (计算模块二进制的 MD5 校验值作为客户端握手标识)
    MD5Hash md5;
    md5.UpdateData(&binary[0], binary.size());
    md5.Finalize();
    hash.resize(md5.GetLength());
    std::memcpy(hash.data(), md5.GetDigest(), md5.GetLength());

    std::ifstream k(kf, std::ios::binary | std::ios::ate);

    if (k.fail())
        throw std::runtime_error("Failed to open Warden module key file");

    if (static_cast<size_t>(k.tellg()) != KeySize)
        throw std::runtime_error("Key file " + kf + " is incorrect size!");

    key.resize(KeySize);
    k.seekg(0, std::ios::beg);

    if (!k.read(reinterpret_cast<char *>(&key[0]), key.size()))
        throw std::runtime_error("Failed to open " + kf);

    k.close();

    std::ifstream c(cr, std::ios::binary | std::ios::ate);

    if (c.fail())
        throw std::runtime_error("Failed to open Warden challenge/response file");

    auto const crSize = static_cast<size_t>(c.tellg()) - sizeof(opcodes) - sizeof(memoryRead) - sizeof(pageScanCheck);

    if (!!(crSize % sizeof(ChallengeResponseEntry)))
        throw std::runtime_error("Unexpected data in Warden challenge/response file");

    crk.resize(crSize / sizeof(ChallengeResponseEntry));

    c.seekg(0, std::ios::beg);

    c.read(reinterpret_cast<char *>(&memoryRead), sizeof(memoryRead));
    c.read(reinterpret_cast<char *>(&pageScanCheck), sizeof(pageScanCheck));
    c.read(reinterpret_cast<char *>(&opcodes), sizeof(opcodes));
    c.read(reinterpret_cast<char *>(&crk[0]), crSize);

    c.close();

    // Windows 模块需挑选未与任意扫描操作码冲突的字节作为 scanTerminator 结束哨兵
    if (Windows())
    {
        for (uint8 i = 0; i <= UINT8_MAX; i++)
        {
            if (opcodes[0] != i &&
                opcodes[1] != i &&
                opcodes[2] != i &&
                opcodes[3] != i &&
                opcodes[4] != i &&
                opcodes[5] != i &&
                opcodes[6] != i &&
                opcodes[7] != i &&
                opcodes[8] != i)
            {
                scanTerminator = i;
                break;
            }
        }

        if (!memoryRead)
            throw std::runtime_error("Module data does not include memory read information");

        if (!pageScanCheck)
            throw std::runtime_error("Module data does not include page scan check information");
    }
}

// 通过 opcodes 数组是否非全零判断该模块是否为 Windows 平台模块
bool WardenModule::Windows() const
{
    // these values will be all zero for OSX modules (OSX 模块的操作码表全为 0)

    for (auto i = 0; i < sizeof(opcodes); ++i)
        if (!!opcodes[i])
            return true;

    return false;
}
