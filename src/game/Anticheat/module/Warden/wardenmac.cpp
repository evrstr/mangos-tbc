/*
 * Copyright (C) 2017-2020 namreeb (legal@namreeb.org)
 *
 * This is private software and may not be shared under any circumstances,
 * absent permission of namreeb.
 */

#include "WardenMac.hpp"
#include "WardenModuleMgr.hpp"
#include "WardenScanMgr.hpp"
#include "Server/WorldSession.h"
#include "Auth/BigNumber.h"
#include "Auth/CryptoHash.h"
#include "World/World.h"
#include "../config.hpp"
#include "Log/Log.h"

#include <memory>
#include <vector>
#include <string>
#include <sstream>

// 加载 Mac 客户端专用的脚本化 Warden 扫描规则（通过模块对账号特定字符串求 SHA1/MD5 校验模块完整性）
void WardenMac::LoadScriptedScans()
{
    sWardenScanMgr.AddMacScan(std::make_shared<MacScan>(
    // builder (构造请求：下发账号专属校验字符串)
    [](const Warden *warden, std::vector<std::string> &, ByteBuffer &scan)
    {
        auto const macWarden = reinterpret_cast<const WardenMac *>(warden);

        MANGOS_ASSERT(macWarden->_hashString.size() <= 0xFF);

        scan << static_cast<uint8>(macWarden->_hashString.size());

        // skip null terminator this way (跳过末尾空字符写入原始字符串字节)
        scan.append(macWarden->_hashString.c_str(), macWarden->_hashString.size());
    },
    // checker (核验 Mac 客户端回传的 SHA-1 与 MD5 摘要是否与预期一致)
    [](const Warden *warden, ByteBuffer &buff)
    {
        auto const macWarden = reinterpret_cast<const WardenMac *>(warden);

        uint8 sha[Sha1Hash::GetLength()];
        uint8 md5[MD5Hash::GetLength()];

        buff.read(sha, sizeof(sha));
        buff.read(md5, sizeof(md5));

        return !!memcmp(sha, macWarden->_hashSHA, sizeof(sha)) || !!memcmp(md5, macWarden->_hashMD5, sizeof(md5));
    }, 128, sizeof(uint8) + Sha1Hash::GetLength() + MD5Hash::GetLength(), "Mac string hash check"));
}

// 构造 Mac Warden 实例：仅 x86 Mac 使用模块，PPC Mac 直接构造预期哈希并启动扫描
WardenMac::WardenMac(WorldSession *session, const BigNumber &K, SessionAnticheatInterface *anticheat)
    : Warden(session, session->GetPlatform() == CLIENT_PLATFORM_X86 ? sWardenModuleMgr.GetMacModule() : nullptr, K, anticheat), _fingerprintSaved(false)
{
    std::stringstream hash;

    // the only known capability of the Mac Warden module is hashing a string and sending back the hashed value
    // so at least we can make the string that we ask it to hash change by account, I guess...
    // 已知 Mac Warden 模块仅支持对指定字符串计算哈希，因此按账号名动态生成待校验字符串
    hash << "namreeb was here.  please be good.  your username is " << _session->GetAccountName();

    _hashString = hash.str();

    static constexpr uint32 magic = 0xFEEDFACE;

    Sha1Hash sha1;
    sha1.UpdateData(_hashString);
    if (_module) // this constant is only used if there is a module (仅加载了 x86 Mac 模块时才追加魔数)
        sha1.UpdateData(reinterpret_cast<const uint8*>(&magic), sizeof(magic));
    sha1.Finalize();

    memcpy(_hashSHA, sha1.GetDigest(), sizeof(_hashSHA));

    MD5Hash md5;
    md5.UpdateData(_hashString);
    md5.Finalize();
    std::memcpy(_hashMD5, md5.GetDigest(), md5.GetLength());

    // PPC no module, begin string hashing requests directly (PPC 架构无模块，直接初始化并下发首轮扫描)
    if (!_module)
    {
        // at this point the client has our module loaded.  send whatever packets are necessary to initialize Warden
        InitializeClient();

        // send any initial hack scans that the scan manager may have for us
        RequestScans(SelectScans(ScanFlags::InitialLogin));

        // begin the scan clock (note that even if the clock expires before any initial scans are answered, no new
        // checks will be requested until the reply is received).
        BeginScanClock();
    }
}

// 帧更新：初始化完成后补发被延迟的角色枚举包（SMSG_CHAR_ENUM）
void WardenMac::Update(uint32 diff)
{
    Warden::Update(diff);

    if (!_initialized)
        return;

    if (!_fingerprintSaved)
    {
        // XXX FINGERPRINT LOGGING
        //LoginDatabase.BeginTransaction();

        //static SqlStatementID fingerprintUpdate;

        //auto stmt = LoginDatabase.CreateStatement(fingerprintUpdate,
        //    "INSERT INTO system_fingerprint_usage (fingerprint, account, ip, realm) VALUES(?, ?, ?, ?)");

        //stmt.addUInt32(m_anticheat->GetFingerprint());
        //stmt.addUInt32(_session->GetAccountId());
        //stmt.addString(_session->GetRemoteAddress());
        //stmt.addUInt32(realmID);
        //stmt.Execute();

        //LoginDatabase.CommitTransaction();

        _fingerprintSaved = true;

        // at this point if we have the character enum packet, it is okay to send (初始化完成后再发送缓存的角色列表包)
        if (!_charEnum.empty())
        {
            _session->SendPacket(_charEnum, true);
            _charEnum.clear();
        }
    }
}

// 缓存或立即发送角色列表包，确保 Warden 初始化完成前不放行角色选择
void WardenMac::SetCharEnumPacket(WorldPacket &&packet)
{
    // if we have already recorded system information, send the packet immediately.  otherwise delay
    if (_initialized)
        _session->SendPacket(packet, true);
    else
        _charEnum = std::move(packet);
}

// 返回 Mac 平台适用的扫描标志集合
uint32 WardenMac::GetScanFlags() const
{
    uint32 ret = ScanFlags::MacAllBuild;

    if (_session->GetAccountMaxLevel() >= sAnticheatConfig.GetWardenMinimumAdvancedLevel())
        ret |= ScanFlags::AdvancedScan;

    return ret;
}

// 标记 Mac 客户端 Warden 初始化完成
void WardenMac::InitializeClient()
{
    _initialized = true;
}
