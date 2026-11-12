/*
 * Copyright (C) 2017-2020 namreeb (legal@namreeb.org)
 *
 * This is private software and may not be shared under any circumstances,
 * absent permission of namreeb.
 */

#include "WardenModule.hpp"
#include "WardenKeyGeneration.h"
#include "../config.hpp"

#include "Common.h"
#include "Entities/Player.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "World/World.h"
#include "Log/Log.h"
#include "Server/Opcodes.h"
#include "Util/ByteBuffer.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "Auth/BigNumber.h"
#include "warden.hpp"
#include "WardenModuleMgr.hpp"
#include "Util/Util.h"
#include "Anticheat/module/libanticheat.hpp"
#include "WardenWin.hpp"
#include "WardenMac.hpp"
#include "WardenScanMgr.hpp"

#include <zlib.h>

#include <algorithm>
#include <memory>

using CheatType = NamreebAnticheat::CheatType;
using SessionAnticheat = NamreebAnticheat::SessionAnticheat;

// 加载 Windows 与 Mac 平台内置的脚本化 Warden 扫描规则
void Warden::LoadScriptedScans()
{
    auto const start = sWardenScanMgr.Count();

    WardenWin::LoadScriptedScans();
    WardenMac::LoadScriptedScans();

    sLog.outBasic(">> %lu scripted Warden scans loaded from anticheat module", uint64(sWardenScanMgr.Count() - start));
}

// 构造 Warden 会话基类：派生初始 RC4 加解密密钥，并在存在模块时向客户端发送 MODULE_USE 握手包
Warden::Warden(WorldSession *session, const WardenModule *module, const BigNumber &K, SessionAnticheatInterface *anticheat) :
    _crk(nullptr), _timeoutClock(0), _scanClock(0), _moduleSendPending(false), _inputCrypto(KeyLength), _outputCrypto(KeyLength),
    _anticheat(reinterpret_cast<SessionAnticheat *>(anticheat)), _session(session), _module(module), _initialized(false)
{
    MANGOS_ASSERT(!!_module || session->GetPlatform() != CLIENT_PLATFORM_X86);
    MANGOS_ASSERT(!!_anticheat);

    auto const kBytes = K.AsByteArray(40);

    SHA1Randx WK(kBytes.data(), kBytes.size());

    uint8 inputKey[KeyLength];
    WK.Generate(inputKey, sizeof(inputKey));

    uint8 outputKey[KeyLength];
    WK.Generate(outputKey, sizeof(outputKey));

    _inputCrypto.Init(inputKey);
    _outputCrypto.Init(outputKey);

    _xor = inputKey[0];

    sLog.outDebug("WARDEN: Initializing for account %u ip %s", _session->GetAccountId(), _session->GetRemoteAddress().c_str());

    if (_module)
    {
        ByteBuffer pkt(1 + _module->hash.size() + _module->key.size() + 4);

        pkt << static_cast<uint8>(WARDEN_SMSG_MODULE_USE);

        pkt.append(&_module->hash[0], _module->hash.size());
        pkt.append(&_module->key[0], _module->key.size());

        pkt << static_cast<uint32>(_module->binary.size());

        SendPacket(pkt);

        BeginTimeoutClock();
    }
}

// 从模块预计算挑战/响应表随机挑选一条种子向客户端发起哈希挑战
void Warden::RequestChallenge()
{
    MANGOS_ASSERT(!!_module && !_module->crk.empty());

    sLog.outDebug("WARDEN: Sending challenge to account %u", _session->GetAccountId());

    StopTimeoutClock();

    // select a random challenge/response/key entry (随机选取一条挑战/响应/密钥记录)
    _crk = &_module->crk[urand(0, _module->crk.size() - 1)];

    ByteBuffer pkt(1 + sizeof(_crk->seed));

    pkt << static_cast<uint8>(WARDEN_SMSG_HASH_REQUEST);
    pkt.append(_crk->seed, sizeof(_crk->seed));

    SendPacket(pkt);

    BeginTimeoutClock();
}

// 校验客户端返回的哈希挑战响应，通过后切换为模块派生的新 RC4 会话密钥
void Warden::HandleChallengeResponse(ByteBuffer &buff)
{
    buff.rpos(buff.wpos());

    // shouldn't happen under normal conditions (未处于等待挑战状态却收到响应，视为作弊并踢出)
    if (!_crk)
    {
        _anticheat->RecordCheatInternal(CheatType::CHEAT_TYPE_WARDEN, "Challenge response without request being sent");
        _session->KickPlayer();
        return;
    }

    // Verify key (比对客户端计算结果与预置 reply)
    if (buff.wpos() != 1 + sizeof(_crk->reply) || !!memcmp(buff.contents() + 1, _crk->reply, sizeof(_crk->reply)))
    {
        _anticheat->RecordCheatInternal(CheatType::CHEAT_TYPE_WARDEN, "Challenge response failed");
        _session->KickPlayer();
        return;
    }

    StopTimeoutClock();

    _inputCrypto.Init(_crk->clientKey);
    _outputCrypto.Init(_crk->serverKey);

    _xor = _crk->clientKey[0];

    sLog.outDebug("WARDEN: Challenge response validated.  Warden packet encryption initialized.");

    _crk = nullptr;
}

// 将 Warden 模块二进制按每片最大 500 字节分块下发至客户端缓存
void Warden::SendModuleToClient()
{
    sLog.outDebug("WARDEN: Sending module to account %u ip %s", _session->GetAccountId(), _session->GetRemoteAddress().c_str());

    StopTimeoutClock();

    for (auto pos = 0u; pos < _module->binary.size(); )
    {
        const std::uint16_t sizeLeft = _module->binary.size() - pos;
        const std::uint16_t len = std::min(sizeLeft, static_cast<decltype(sizeLeft)>(500));

        ByteBuffer pkt(1 + 2 + len);

        pkt << static_cast<uint8>(WARDEN_SMSG_MODULE_CACHE) << len;
        pkt.append(&_module->binary[pos], len);

        SendPacket(pkt);

        pos += len;
    }

    BeginTimeoutClock();

    _moduleSendPending = true;

    sLog.outDebug("WARDEN: Module transfer complete");
}

// 按指定标志与会话自身平台/等级标志筛选随机扫描项
std::vector<std::shared_ptr<const Scan>> Warden::SelectScans(ScanFlags flags) const
{
    return sWardenScanMgr.GetRandomScans(static_cast<ScanFlags>(flags | GetScanFlags()));
}

// 将额外扫描项追加到待发送队列末尾
void Warden::EnqueueScans(std::vector<std::shared_ptr<const Scan>> &&scans)
{
    // append the requested scans to the queue (追加至队列尾部)
    _enqueuedScans.insert(_enqueuedScans.end(), scans.cbegin(), scans.cend());
}

// 打包并发送一批不超出请求/响应字节上限及数量上限的扫描检查请求
void Warden::RequestScans(std::vector<std::shared_ptr<const Scan>> &&scans)
{
    // start by appending the requested scans to the queue, which will usually be empty when this is called
    // 先将本次新扫描合并到等待队列
    _enqueuedScans.insert(_enqueuedScans.end(), scans.cbegin(), scans.cend());

    // if for whatever reason there is nothing to do, stop
    if (_enqueuedScans.empty())
        return;

    // if there are already some scans pending, stop (仍有未回复的挂起扫描时暂不发下一批)
    if (!_pendingScans.empty())
        return;

    // begin building the scan request, taking as many scans from the queue as we can

    std::vector<std::string> strings;
    ByteBuffer scan;

    size_t request = 0, reply = 0;

    bool queueUpdated = false;

    for (auto i = _enqueuedScans.begin(); i != _enqueuedScans.end(); ++i)
    {
        // too many or too big? keep the current scan, as well as all following it, in the queue, and halt
        // 超出单包请求/响应缓冲区大小或扫描条数上限时截断，剩余项留待下次发送
        if (request + (*i)->requestSize > MaxRequest || reply + (*i)->replySize > MaxReply ||
            _pendingScans.size() >= sAnticheatConfig.GetWardenScanCount())
        {
            _enqueuedScans = std::vector<std::shared_ptr<const Scan>>(i, _enqueuedScans.end());
            queueUpdated = true;
            break;
        }

        // if we can fit this one, do it

        auto const startSize = scan.wpos();
        auto const startStringSize = strings.size();

        (*i)->Build(this, strings, scan);

        // if the scan did not change the buffer size or the string size, consider
        // it a NOP and don't bother marking it as pending
        // 若 Build 未写入任何数据（如条件跳过），视为空操作不加入 _pendingScans
        if (scan.wpos() != startSize || strings.size() != startStringSize)
        {
            _pendingScans.push_back(*i);

            // account for the space this scan consumes.  without this the checks above only ever
            // compare a single scan against the buffer limits, so a request could be built which
            // overflows the client's buffers, bounded only by Warden.ScanCount.
            // 累计请求与响应字节占用，防止溢出客户端缓冲区
            request += (*i)->requestSize;
            reply += (*i)->replySize;
        }
    }

    // if there are still no pending scans, no request can be built this cycle.  this used to assert
    // (crashing the server), which is reachable if an oversized scan is ever added -- e.g. via the
    // warden_scans table -- so handle each possible cause gracefully instead.
    // 若本轮未能打包任何有效扫描（全为空操作或首条即超限），优雅清理或丢弃超大条目而非断言崩溃
    if (_pendingScans.empty())
    {
        // the loop ran to completion, so every scan we examined was a NOP.  nothing is pending and
        // nothing is worth retrying, which matches what the normal path below does with the queue.
        if (!queueUpdated)
        {
            _enqueuedScans.clear();
            return;
        }

        // otherwise we stopped on the very first scan.  if it can never fit in a single request,
        // drop it so it does not block the queue forever.  if we merely hit the per-request scan
        // count limit (Warden.ScanCount set to zero), leave the queue intact and retry later.
        auto const &next = _enqueuedScans.front();

        if (next->requestSize > MaxRequest || next->replySize > MaxReply)
        {
            sLog.outError("[Warden] Dropping scan too large for a single request (request %u/%u, reply %u/%u bytes)",
                static_cast<uint32>(next->requestSize), static_cast<uint32>(MaxRequest),
                static_cast<uint32>(next->replySize), static_cast<uint32>(MaxReply));

            _enqueuedScans.erase(_enqueuedScans.begin());
        }

        return;
    }

    // if the scan queue has not been updated, its because we were able to fit the entire queue into one request.
    // therefore, the queue can be emptied
    // 队列已全部打包完毕则清空等待队列
    if (!queueUpdated)
        _enqueuedScans.clear();

    // this buffer is where we will combine the different portions of the packet
    ByteBuffer buff;

    // warden opcode
    buff << static_cast<uint8>(WARDEN_SMSG_CHEAT_CHECKS_REQUEST);

    if (_session->GetOS() == CLIENT_OS_WIN)
    {
        // string table for this request (Windows 模块需前置附带 0 结尾的字符串表)
        for (auto const& s : strings)
        {
            buff << static_cast<uint8>(s.length());
            buff.append(s.c_str(), s.length());
        }

        // end of string table
        buff << static_cast<uint8>(0);
    }

    // all scan requests (写入各扫描请求体)
    buff.append(scan);

    if (_session->GetOS() == CLIENT_OS_WIN)
    {
        // indicates to the client that there are no further requests in this packet (写入异或加密后的结束哨兵字节)
        buff << uint8(_module->scanTerminator ^ _xor);
    }

    BeginTimeoutClock();
    SendPacket(buff);
}

// 逐项校验客户端回传的扫描结果；若检查器内部追加了后续扫描则立即发出
void Warden::ReadScanResults(ByteBuffer &buff)
{
    // move the scans to the stack because the check handlers we call may enqueue additional scans
    // 先将待核验列表移至局部栈变量，因 Check() 回调中可能会向队列追加新扫描
    auto const scans = std::move(_pendingScans);

    auto const inWorld = _session->GetPlayer() && _session->GetPlayer()->IsInWorld();

    for (auto const &s : scans)
    {
        // checks return true when they have discovered a hack (Check 返回 true 表示检测到异常/作弊)
        if (s->Check(this, buff))
        {
            // if this scan requires being in the world and they are not in the world (meaning they left
            // the world between the request and response), ignore the results
            // 若该检查要求角色在世界中而收到响应时已离开世界，则忽略避免误报
            if (!!(s->flags & InWorld) && !inWorld)
                continue;

            _anticheat->RecordCheatInternal(CheatType::CHEAT_TYPE_WARDEN, "Check \"%s\" failed", s->comment.c_str());
        }
    }

    // if there are enqueued scans at this point, the check handlers must have requested them.
    // when this happens, request them to be sent immediately.
    // 若校验回调追加了后续关联扫描，立即发送请求
    if (!_enqueuedScans.empty())
        RequestScans({});
}

// 拷贝缓冲区并执行 RC4 加密后发送 SMSG_WARDEN_DATA 数据包
void Warden::SendPacket(const ByteBuffer &buff)
{
    WorldPacket pkt(SMSG_WARDEN_DATA, buff.wpos());
    pkt.append(buff);

    // we specifically append the packet copy, rather than the input copy, to avoid
    // creating side-effects for this function
    // 对副本数据就地加密，避免破坏调用方传入的原缓冲区
    EncryptData(const_cast<uint8 *>(pkt.contents()), pkt.wpos());

    _session->SendPacket(pkt, true);
}

// 使用输入 RC4 流密码解密客户端上行数据
void Warden::DecryptData(uint8* buffer, size_t size)
{
    _inputCrypto.UpdateData(buffer, size);
}

// 使用输出 RC4 流密码加密服务端下行数据
void Warden::EncryptData(uint8* buffer, size_t size)
{
    _outputCrypto.UpdateData(buffer, size);
}

// 启动响应超时计时器（Debug 构建下禁用超时踢线）
void Warden::BeginTimeoutClock()
{
#ifdef _DEBUG
    _timeoutClock = 0;
#else
    // we will expect a reply eventually
    _timeoutClock = WorldTimer::getMSTime() + 1000 * sAnticheatConfig.GetWardenTimeout();
#endif
}

void Warden::StopTimeoutClock()
{
    _timeoutClock = 0;
}

// 启动下一轮周期性扫描计时器
void Warden::BeginScanClock()
{
    _scanClock = WorldTimer::getMSTime() + 1000 * sAnticheatConfig.GetWardenScanFrequency();
}

void Warden::StopScanClock()
{
    _scanClock = 0;
}

// 计算 Windows Warden 响应包头使用的 SHA-1 折叠 32 位校验和
uint32 Warden::BuildChecksum(const uint8* data, size_t size)
{
    uint8 hash[Sha1Hash::GetLength()];
    SHA1(data, size, hash);

    uint32 checkSum = 0;

    for (auto i = 0u; i < sizeof(hash) / sizeof(uint32); ++i)
        checkSum ^= *reinterpret_cast<uint32 *>(&hash[i * 4]);

    return checkSum;
}

// 解密并分发客户端上行 Warden 数据包（模块请求/确认、挑战响应、扫描结果、加载失败）
void Warden::HandlePacket(WorldPacket& recvData)
{
    // initialize decrypt packet (先解密整个数据包载荷)
    DecryptData(const_cast<uint8*>(recvData.contents()), recvData.size());

    uint8 opcode;
    recvData >> opcode;

    // when there is a challenge/response pending, the only packet we expect is the hash result
    // 等待挑战响应期间只允许接收 WARDEN_CMSG_HASH_RESULT
    if (!!_crk && opcode != WARDEN_CMSG_HASH_RESULT)
    {
        _anticheat->RecordCheatInternal(CheatType::CHEAT_TYPE_WARDEN, "Unexpected opcode (%u) received while awaiting hash challenge response", opcode);
        recvData.rpos(recvData.wpos());
        _session->KickPlayer();
        return;
    }

    switch (opcode)
    {
        case WARDEN_CMSG_MODULE_MISSING:
        {
            if (!_module)
            {
                sLog.outBasic("WARDEN: Account %u IP %s requested module when none was offered.", _session->GetAccountId(), _session->GetRemoteAddress().c_str());
                _session->KickPlayer();
                break;
            }

            if (_moduleSendPending)
            {
                sLog.outBasic("WARDEN: Account %u IP %s failed to load module.  Kicking.", _session->GetAccountId(), _session->GetRemoteAddress().c_str());
                _session->KickPlayer();
                _moduleSendPending = false;
                break;
            }

            sLog.outBasic("WARDEN: Account - %u get opcode 00 - Load module failed or module is missing...", _session->GetAccountId());
            SendModuleToClient();
            break;
        }

        case WARDEN_CMSG_MODULE_OK:
        {
            if (!_module)
            {
                sLog.outBasic("WARDEN: Account %u IP %s loaded module without server request.", _session->GetAccountId(), _session->GetRemoteAddress().c_str());
                _session->KickPlayer();
                break;
            }

            _moduleSendPending = false;
            sLog.outBasic("WARDEN: Account - %u get opcode 01 - Module has loaded. Recv answer....", _session->GetAccountId());
            RequestChallenge();
            break;
        }

        case WARDEN_CMSG_CHEAT_CHECKS_RESULT:
        {
            if (_session->GetOS() == CLIENT_OS_WIN)
            {
                // verify checksum integrity (Windows 客户端结果包需校验长度与 SHA-1 折叠校验和)
                uint16 length;
                uint32 checksum;
                recvData >> length >> checksum;

                if (length > (recvData.size() - recvData.rpos()))
                {
                    recvData.rpos(recvData.wpos());
                    _anticheat->RecordCheatInternal(CheatType::CHEAT_TYPE_WARDEN, "Packet checksum length fail");
                    _session->KickPlayer();
                    return;
                }

                if (BuildChecksum(recvData.contents() + recvData.rpos(), length) != checksum)
                {
                    recvData.rpos(recvData.wpos());
                    _anticheat->RecordCheatInternal(CheatType::CHEAT_TYPE_WARDEN, "Packet checksum fail");
                    _session->KickPlayer();
                    return;
                }
            }

            // this function will also act on the results (解析并核验所有扫描结果)
            ReadScanResults(recvData);

            StopTimeoutClock();
            BeginScanClock();

            break;
        }

        // FIXME: Find when/why/how this actually happens and how to handle it
        case WARDEN_CMSG_MEM_CHECKS_RESULT:
        {
            sLog.outDebug("WARDEN: Account - %u received opcode 03", _session->GetAccountId());
            break;
        }

        case WARDEN_CMSG_HASH_RESULT:
        {
            sLog.outDebug("WARDEN: Account - %u received opcode 04", _session->GetAccountId());

            HandleChallengeResponse(recvData);

            // if the challenge failed, this will still be set.  by now, the session will be closed.  do nothing further.
            if (!!_crk)
                return;

            // at this point the client has our module loaded.  send whatever packets are necessary to initialize Warden
            // 挑战通过后初始化客户端偏移量/模块环境，并立即下发首轮登录扫描
            InitializeClient();

            // send any initial hack scans that the scan manager may have for us
            RequestScans(SelectScans(ScanFlags::InitialLogin));

            // begin the scan clock (note that even if the clock expires before any initial scans are answered, no new
            // checks will be requested until the reply is received).
            BeginScanClock();

            break;
        }

        case WARDEN_CMSG_MODULE_FAILED:
        {
            sLog.outBasic("WARDEN: Account - %u received opcode 05 - Module load failed!", _session->GetAccountId());
            _session->KickPlayer();
            break;
        }

        default:
        {
            _anticheat->RecordCheatInternal(CheatType::CHEAT_TYPE_WARDEN, "Received unknown opcode %02X of size %u.", opcode, recvData.size() - 1);
            _session->KickPlayer();
            break;
        }
    }
}

// 定时检查 Warden 超时并调度待发或周期性随机扫描
void Warden::Update(uint32 /*diff*/)
{
    if (!!_timeoutClock && WorldTimer::getMSTime() > _timeoutClock)
    {
        sLog.outBasic("WARDEN: Account %u ip %s timeout", _session->GetAccountId(), _session->GetRemoteAddress().c_str());
        _session->KickPlayer();
        return;
    }

    if (_pendingScans.empty())
    {
        // if there are enqueued scans which may now be requested, do so immediately (with no additional scans)
        if (!_enqueuedScans.empty())
            RequestScans({});
        // otherwise, if the scan clock is running and has expired, request randomly selected scans
        else if (!!_scanClock && WorldTimer::getMSTime() > _scanClock)
        {
            auto const inWorld = _session->GetPlayer() ? _session->GetPlayer()->IsInWorld() : false;

            RequestScans(SelectScans(inWorld ? InWorld : None));

            // if no scans were found, just begin the clock again, in case things change
            if (_pendingScans.empty())
                BeginScanClock();
            else
            {
                // let us keep things simple and not request checks while others are pending, but only if scans were found and requested
                StopScanClock();
                BeginTimeoutClock();
            }
        }
    }
}
