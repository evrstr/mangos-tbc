/*
 * Copyright (C) 2017-2020 namreeb (legal@namreeb.org)
 *
 * This is private software and may not be shared under any circumstances,
 * absent permission of namreeb.
 */

#include "libanticheat.hpp"
#include "config.hpp"
#include "Server/WorldPacket.h"
#include "Log/Log.h"

#include "Util/Util.h"
#include "Database/DatabaseEnv.h"

#include <zlib.h>

#include <memory>

namespace
{
// 校验设备指纹 4 个字节是否合法（不能出现 0x00/0x01/0x02，避免触发客户端插件封禁标志）
bool FingerprintValid(uint32 fingerprint)
{
    // no bytes can equal 0x00, 0x01, or 0x02 (任意字节均不得为 0x00、0x01 或 0x02)
    const uint8 *b = reinterpret_cast<uint8 *>(&fingerprint);
    for (auto i = 0u; i < sizeof(fingerprint); ++i)
        if (b[i] <= 0x02)
            return false;

    return true;
}

// 查询数据库检查该指纹是否已被其他账号使用过
bool FingerprintExists(uint32 fingerprint)
{
    // if it is not valid, it can't exist! (格式非法则必然不存在)
    if (!FingerprintValid(fingerprint))
        return false;

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery("SELECT COUNT(*) FROM system_fingerprint_usage WHERE fingerprint = %u", fingerprint));

    return !!result->Fetch()[0].GetUInt32();
}

// 随机生成全局唯一的 4 字节硬件/客户端指纹（每字节范围 0x03~0xFF）
uint32 GenerateFingerprint()
{
    do
    {
        uint32 fingerprint = 0;

        // generate four random bytes between 0x03 and 0xFF (生成 4 个 0x03~0xFF 之间的随机字节)
        for (auto i = 0u; i < sizeof(uint32); ++i)
            fingerprint |= static_cast<uint8>(urand(0x03, 0xFF)) << 8*i;

        // if the fingerprint already exists, repeat (若数据库已存在相同指纹则重新生成)
        if (FingerprintExists(fingerprint))
            continue;

        // if we reach here, we are done

        return fingerprint;
    } while (true);
}

// 客户端上报的单个插件校验条目
struct AddonInfo
{
    std::string name;
    uint8 flags;
    uint32 moduluscrc;
    uint32 urlcrc;
};

// each addon will give us one byte of the fingerprint.  this byte can not be 0x00, 0x01, or 0x02, as
// the client considers these as valid flags with behaviors we want to avoid (i.e. 0x02 = banned)
// 用于在客户端本地持久化 4 字节指纹的 4 个暴雪内置插件（每插件承载 1 字节，避开 0x00/0x01/0x02 特殊标志位）
static const char *sFingerprintAddons[] =
{
    "Blizzard_BindingUI",
    "Blizzard_InspectUI",
    "Blizzard_MacroUI",
    "Blizzard_RaidUI",
};
}

namespace NamreebAnticheat
{
// 解析认证包中的压缩插件列表，提取或生成并回写客户端指纹，同时构造 SMSG_ADDON_INFO 响应
bool SessionAnticheat::ReadAddonInfo(WorldPacket *authSession, WorldPacket &out)
{
    // broken addon packet, can't be received from real client (数据包长度异常，非正常客户端报文)
    if (authSession->rpos() + 4 > authSession->size())
        return false;

    // have we already received this information?  if so, it must be some kind of hacker
    // 若当前会话已记录过指纹又再次收到插件信息，视为异常/攻击行为
    if (!!_fingerprint)
    {
        sLog.outBasic(
            "ADDON: Received addon information when fingerprint is already known (0x%x) account %u ip %s.  This may be an attempt to crash the server",
            _fingerprint, _session->GetAccountId(), _session->GetRemoteAddress().c_str());
        authSession->rpos(authSession->wpos());
        return false;
    }

    uLong addonSize = authSession->read<uint32>();

    // empty addon packet, nothing process, can't be received from real client (插件数据解压后大小为 0，拒绝处理)
    if (!addonSize)
        return false;

    if (addonSize > 0xFFFFF)
    {
        sLog.outBasic("ADDON: Addon info too big, size %lu.  Account %s (id %u) IP %s",
            addonSize, _session->GetAccountName().c_str(), _session->GetAccountId(), _session->GetRemoteAddress().c_str());

        return false;
    }

    ByteBuffer clientAddonData;
    clientAddonData.resize(addonSize);

    if (uncompress(const_cast<Bytef*>(clientAddonData.contents()), &addonSize,
        reinterpret_cast<const Bytef *>(authSession->contents() + authSession->rpos()),
        static_cast<uLongf>(authSession->size() - authSession->rpos())) != Z_OK)
    {
        sLog.outBasic("ADDON: Addon information failed to compress.  Account %s (id %u) IP %s",
            _session->GetAccountName().c_str(), _session->GetAccountId(), _session->GetRemoteAddress().c_str());

        return false;
    }

    static constexpr uint32 correctModulusCRC = 0x4C1C776D; // 客户端内置标准公钥模数 CRC32

    // 标准暴雪插件校验公钥模数（256 字节）
    static constexpr uint8 modulus[] =
    {
        0xC3, 0x5B, 0x50, 0x84, 0xB9, 0x3E, 0x32, 0x42, 0x8C, 0xD0, 0xC7, 0x48, 0xFA, 0x0E, 0x5D, 0x54,
        0x5A, 0xA3, 0x0E, 0x14, 0xBA, 0x9E, 0x0D, 0xB9, 0x5D, 0x8B, 0xEE, 0xB6, 0x84, 0x93, 0x45, 0x75,
        0xFF, 0x31, 0xFE, 0x2F, 0x64, 0x3F, 0x3D, 0x6D, 0x07, 0xD9, 0x44, 0x9B, 0x40, 0x85, 0x59, 0x34,
        0x4E, 0x10, 0xE1, 0xE7, 0x43, 0x69, 0xEF, 0x7C, 0x16, 0xFC, 0xB4, 0xED, 0x1B, 0x95, 0x28, 0xA8,
        0x23, 0x76, 0x51, 0x31, 0x57, 0x30, 0x2B, 0x79, 0x08, 0x50, 0x10, 0x1C, 0x4A, 0x1A, 0x2C, 0xC8,
        0x8B, 0x8F, 0x05, 0x2D, 0x22, 0x3D, 0xDB, 0x5A, 0x24, 0x7A, 0x0F, 0x13, 0x50, 0x37, 0x8F, 0x5A,
        0xCC, 0x9E, 0x04, 0x44, 0x0E, 0x87, 0x01, 0xD4, 0xA3, 0x15, 0x94, 0x16, 0x34, 0xC6, 0xC2, 0xC3,
        0xFB, 0x49, 0xFE, 0xE1, 0xF9, 0xDA, 0x8C, 0x50, 0x3C, 0xBE, 0x2C, 0xBB, 0x57, 0xED, 0x46, 0xB9,
        0xAD, 0x8B, 0xC6, 0xDF, 0x0E, 0xD6, 0x0F, 0xBE, 0x80, 0xB3, 0x8B, 0x1E, 0x77, 0xCF, 0xAD, 0x22,
        0xCF, 0xB7, 0x4B, 0xCF, 0xFB, 0xF0, 0x6B, 0x11, 0x45, 0x2D, 0x7A, 0x81, 0x18, 0xF2, 0x92, 0x7E,
        0x98, 0x56, 0x5D, 0x5E, 0x69, 0x72, 0x0A, 0x0D, 0x03, 0x0A, 0x85, 0xA2, 0x85, 0x9C, 0xCB, 0xFB,
        0x56, 0x6E, 0x8F, 0x44, 0xBB, 0x8F, 0x02, 0x22, 0x68, 0x63, 0x97, 0xBC, 0x85, 0xBA, 0xA8, 0xF7,
        0xB5, 0x40, 0x68, 0x3C, 0x77, 0x86, 0x6F, 0x4B, 0xD7, 0x88, 0xCA, 0x8A, 0xD7, 0xCE, 0x36, 0xF0,
        0x45, 0x6E, 0xD5, 0x64, 0x79, 0x0F, 0x17, 0xFC, 0x64, 0xDD, 0x10, 0x6F, 0xF3, 0xF5, 0xE0, 0xA6,
        0xC3, 0xFB, 0x1B, 0x8C, 0x29, 0xEF, 0x8E, 0xE5, 0x34, 0xCB, 0xD1, 0x2A, 0xCE, 0x79, 0xC3, 0x9A,
        0x0D, 0x36, 0xEA, 0x01, 0xE0, 0xAA, 0x91, 0x20, 0x54, 0xF0, 0x72, 0xD8, 0x1E, 0xC7, 0x89, 0xD2
    };

    static constexpr size_t fingerprintAddonCount = sizeof(sFingerprintAddons) / sizeof(sFingerprintAddons[0]);
    static_assert(fingerprintAddonCount == sizeof(uint32), "Bad size for fingerprint calculations");

    // flag bytes for each addon which contributes to the fingerprint (存储各目标插件对应的指纹字节)
    uint8 fingerprintBytes[fingerprintAddonCount];

    memset(fingerprintBytes, 0, sizeof(fingerprintBytes));

    std::vector<AddonInfo> addonInfo;

    // first, read client addon info, determining if the packet is valid, and if so, whether a fingerprint is already present
    // 读取解压后的客户端插件列表，并从指定暴雪插件的 flags 中提取已有指纹字节
    while (clientAddonData.rpos() < clientAddonData.size())
    {
        AddonInfo info;

        clientAddonData >> info.name >> info.flags >> info.moduluscrc >> info.urlcrc;

        sLog.outBasic("ADDON: %s flags 0x%02x modulus crc 0x%08x url crc 0x%08x",
            info.name.c_str(), info.flags, info.moduluscrc, info.urlcrc);

        for (auto i = 0u; i < fingerprintAddonCount; ++i)
        {
            if (info.name != sFingerprintAddons[i])
                continue;

            fingerprintBytes[i] = info.flags;
        }

        addonInfo.push_back(info);
    }

    bool fingerprintFound = false;
    for (auto i = 0u; i < fingerprintAddonCount; ++i)
    {
        // we should never have zeros together with a fingerprint (指纹字节中不应出现 0)
        if (!fingerprintBytes[i])
        {
            fingerprintFound = false;
            break;
        }

        // there has to be at least one byte which is neither zero nor 0x01 for it to be a fingerprint (至少有一个字节不为 0x01 才说明不是默认标志)
        if (fingerprintBytes[i] != 0x01)
            fingerprintFound = true;
    }

    uint32 fingerprint = *reinterpret_cast<uint32 *>(&fingerprintBytes[0]);

    // if a fingerprint was found and is valid, use it (若提取到的指纹有效则直接复用，否则生成新指纹)
    if (fingerprintFound && FingerprintValid(fingerprint))
        sLog.outBasic("ADDON: Found fingerprint: 0x%08x.  Account %s (id %u) IP %s",
            fingerprint, _session->GetAccountName().c_str(), _session->GetAccountId(), _session->GetRemoteAddress().c_str());
    else
    {
        // if it was found, but we reach here, it must not have been valid?
        if (fingerprintFound)
        {
            sLog.outBasic("ADDON: Found invalid fingerprint 0x%08x.  Account %s (id %u) IP %s.  Generating new...",
                fingerprint, _session->GetAccountName().c_str(), _session->GetAccountId(), _session->GetRemoteAddress().c_str());

            fingerprintFound = false;
        }

        fingerprint = GenerateFingerprint();
        sLog.outBasic("ADDON: Generated new fingerprint: 0x%08x.  Account %s (id %u) IP %s local IP %s",
            fingerprint, _session->GetAccountName().c_str(), _session->GetAccountId(), _session->GetRemoteAddress().c_str(),
            _session->GetLocalAddress().c_str());
    }

    _fingerprint = fingerprint;

    out.Initialize(SMSG_ADDON_INFO);

    for (auto const &addon : addonInfo)
    {
        out << static_cast<uint8>(2);
        
        bool flagsWritten = false;

        // do we need to set a new fingerprint? (若客户端尚无有效指纹，则将新指纹字节随模数写入对应插件配置中)
        if (!fingerprintFound)
            for (auto i = 0u; i < fingerprintAddonCount; ++i)
                // is this addon part of the fingerprint?  if so, write the appropriate portion of the fingerprint
                if (addon.name == sFingerprintAddons[i])
                {
                    // if the fingerprint needs to be written here, we must also send
                    // the modulus data so the fingerprint is saved to the disk
                    // 写入指纹字节后必须同时下发公钥模数数据，客户端才会将其持久化到磁盘
                    out << *(reinterpret_cast<uint8 *>(&fingerprint) + i);

                    // true when modulus data follows (1 表示后续附带模数数据)
                    out << static_cast<uint8>(1);
                    out.append(modulus, sizeof(modulus));

                    // unknown field (未知占位字段)
                    out << static_cast<uint32>(0);

                    flagsWritten = true;
                    break;
                }

        // if we havent written the flags yet, because either we're not setting a
        // fingerprint or this addon isnt involved in it, send normal data...
        // 非指纹插件或已有指纹时，按常规插件协议回传标志与模数校验结果
        if (!flagsWritten)
        {
            out << addon.flags;

            // if there is data, verify it, and update if necessary
            if (!!addon.flags)
            {
                const uint8 sendData = addon.moduluscrc == correctModulusCRC ? 0 : 1;   // standard addon CRC (校验客户端模数 CRC 是否与标准一致)

                out << sendData;

                if (sendData)
                    out.append(modulus, sizeof(modulus));

                if (!!addon.flags)
                    out << static_cast<uint32>(0);
            }
        }

        // never update the url (不下发插件更新 URL)
        out << static_cast<uint8>(0);
    }

    return true;
}
}