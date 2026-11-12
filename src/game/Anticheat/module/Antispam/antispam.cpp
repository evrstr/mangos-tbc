/*
 * Copyright (C) 2017-2020 namreeb (legal@namreeb.org)
 *
 * This is private software and may not be shared under any circumstances,
 * absent permission of namreeb.
 */

#include "antispam.hpp"
#include "antispammgr.hpp"
#include "../libanticheat.hpp"
#include "Anticheat/Anticheat.hpp"
#include "../config.hpp"
#include "../dldist.hpp"

#include "Entities/Player.h"
#include "Chat/Chat.h"
#include "World/World.h"
#include "Util/Timer.h"
#include "Database/DatabaseEnv.h"
#include "Tools/Language.h"

#include <string>
#include <unordered_set>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <sstream>

namespace
{
// gets the highest level toon on a given account, for enforcing anti spam max level, or 0 if not available
// 获取指定账号下最高角色等级（用于判定反垃圾检测的豁免等级上限）
uint32 GetAccountLevel(uint32 accountId)
{
    if (auto const session = sWorld.FindSession(accountId))
    {
        auto accountLevel = session->GetAccountMaxLevel();

        // slight adjustment to account level logic to account for increasing character levels in the session
        // 若当前会话角色已升级超过登录时的最高等级记录，则取当前角色等级
        if (auto const player = session->GetPlayer())
            if (player->GetLevel() > accountLevel)
                accountLevel = player->GetLevel();

        return accountLevel;
    }

    return 0;
}
}

namespace NamreebAnticheat
{
// 计算当前会话的每分钟发信速率（在线时长低于宽限期时返回 0）
float Antispam::Rate() const
{
    auto const milliseconds = WorldTimer::getMSTime() - _creationTime;
    auto const seconds = static_cast<float>(milliseconds) / float(IN_MILLISECONDS);

    // if they have not been online long enough, return zero (在线未满宽限期则暂不计算发信频率)
    if (seconds < static_cast<float>(sAnticheatConfig.GetAntispamRateGracePeriod()))
        return 0.f;

    return Total() / (seconds * float(MINUTE));
}

// 向 logs_spamdetect 写库并向在线 GM 发送疑似刷屏警告（受冷却时间限制）
void Antispam::Notify(const char *format, ...)
{
    // if it is too soon, do nothing (冷却未到则跳过通知)
    if ((_lastNotification + sAnticheatConfig.GetAntispamBlacklistNotifyCooldown() * MINUTE * IN_MILLISECONDS) > WorldTimer::getMSTime())
        return;

    _lastNotification = WorldTimer::getMSTime();

    auto const session = sWorld.FindSession(_account);

    // do not log or notify GMs if the account is already silenced (已禁言账号不再重复上报)
    if (!!session && sAntispamMgr.IsSilenced(session))
        return;

    char buffer[1024];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);

    std::stringstream message;

    message << "Possible spammer.  Account: " << _account;

    if (!!session)
        message << " Player: " << session->GetPlayerName();

    message << ".  " << buffer << "\nMost blacklisted messages:";

    for (auto const &msg : _topBlacklistedMessages)
        message << "\n" << msg;

    message << "\nRecent messages:";

    for (auto const &msg : _recentMessages)
        message << "\n" << msg;

    // LOGS DATABASE (写入反垃圾检测日志表)
    static SqlStatementID logSilence;

    LogsDatabase.BeginTransaction();

    auto ins = LogsDatabase.CreateStatement(logSilence,
        "INSERT INTO logs_spamdetect (realm, accountId, fromIP, fromFingerprint, comment) VALUES(?, ?, ?, ?, ?)");

    ins.addUInt32(realmID);
    ins.addUInt32(_account);

    if (!!session)
    {
        ins.addString(session->GetRemoteAddress());

        if (auto const anticheat = dynamic_cast<const SessionAnticheat *>(session->GetAnticheat()))
            ins.addUInt32(anticheat->GetFingerprint());
        else
            ins.addUInt32(0);
    }
    else
    {
        ins.addString("<unknown>");
        ins.addUInt32(0);
    }

    ins.addString(message.str());

    ins.Execute();

    LogsDatabase.CommitTransaction();

    // GM NOTIFICATION (向订阅反垃圾告警的 GM 发送系统消息)
    sWorld.SendGMTextFlags(ACCOUNT_FLAG_SHOW_ANTISPAM, LANG_GM_ANNOUNCE_COLOR, "AntiSpam", message.str().c_str());
    va_end(args);
}

// 判定为恶意广告/刷屏者：按配置执行账号禁言、写入日志库并通知在线 GM
void Antispam::Silence(const char *format, ...)
{
    auto const session = sWorld.FindSession(_account);

    // do not log or notify GMs if the account is already silenced (已禁言账号不再重复处理)
    if (!!session && sAntispamMgr.IsSilenced(session))
        return;

    char buffer[1024];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);

    std::stringstream message;

    message << "Flagrant spammer.  Account: " << _account;

    if (!!session)
        message << " Player: " << session->GetPlayerName();

    if (sAnticheatConfig.EnableAntispamSilence())
    {
        sAntispamMgr.Silence(_account);
        message << " (silenced)";
    }

    message << ".  " << buffer << "\nMost blacklisted messages:";

    for (auto const &msg : _topBlacklistedMessages)
        message << "\n" << msg;

    message << "\nRecent messages:";

    for (auto const &msg : _recentMessages)
        message << "\n" << msg;

    // LOGS DATABASE (写入 logs_spamdetect 表)
    static SqlStatementID logSilence;

    LogsDatabase.BeginTransaction();

    auto ins = LogsDatabase.CreateStatement(logSilence,
        "INSERT INTO logs_spamdetect (realm, accountId, fromIP, fromFingerprint, comment) VALUES(?, ?, ?, ?, ?)");

    ins.addUInt32(realmID);
    ins.addUInt32(_account);

    if (!!session)
    {
        ins.addString(session->GetRemoteAddress());

        if (auto const anticheat = dynamic_cast<const SessionAnticheat *>(session->GetAnticheat()))
            ins.addUInt32(anticheat->GetFingerprint());
        else
            ins.addUInt32(0);
    }
    else
    {
        ins.addString("<unknown>");
        ins.addUInt32(0);
    }

    ins.addString(message.str());

    ins.Execute();

    LogsDatabase.CommitTransaction();
    
    // GM NOTIFICATION (向订阅反垃圾告警的 GM 广播)
    sWorld.SendGMTextFlags(ACCOUNT_FLAG_SHOW_ANTISPAM, LANG_GM_ANNOUNCE_COLOR, "AntiSpam", message.str().c_str());
    va_end(args);
}

Antispam::Antispam(uint32 account) :
    _account(account), _creationTime(WorldTimer::getMSTime()), _whisperCount(0), _sayCount(0), _yellCount(0),
    _channelCount(0), _mailCount(0), _channelInviteCount(0), _partyInviteCount(0), _blacklistCount(0), _lastNotification(0) {}

// 生成该账号的详细反垃圾统计报告文本（供 GM 命令查询）
std::string Antispam::GetInfo() const
{
    std::stringstream ret;
    std::lock_guard<std::mutex> guard(_mutex);

    auto const age = WorldTimer::getMSTime() -_creationTime;

    bool silenced = false;

    if (auto const session = sWorld.FindSession(_account))
    {
        ret << "Spam information for player " << session->GetPlayerName() << " account " << _account << ":\n"
            << "Online: " << age / IN_MILLISECONDS << " seconds Distance traveled: " << GetDistanceTraveled()
            << " Account level: " << session->GetAccountMaxLevel() << "\n";

        silenced = sAntispamMgr.IsSilenced(session);
    }
    else
    {
        ret << "Spam information for account " << _account << ":\n";

        silenced = sAntispamMgr.IsSilenced(_account);
    }

    if (silenced)
        ret << "Account is silenced\n";

    auto const minutes = static_cast<float>(age) / (IN_MILLISECONDS * MINUTE);

    ret << "\nWhispers: " << _whisperCount << " (" << _whisperCount / minutes << " per minute actual, "
        << Rate() << " for check) unique targets: " << _whisperTargets.size()
        << "\nSays: " << _sayCount << " (" << _sayCount / minutes << " per minute)\n"
        << "Yells: " << _yellCount << " (" << _yellCount / minutes << " per minute)\n"
        << "Channel messages: " << _channelCount << " (" << _channelCount / minutes << " per minute)\n"
        << "Mails: " << _mailCount << " (" << _mailCount / minutes << " per minute) unique targets: " << _mailTargets.size()
        << "Channel invites: " << _channelInviteCount << " (" << _channelInviteCount / minutes << " per minute) unique targets: " << _channelInviteTargets.size()
        << "Party invites: " << _partyInviteCount << " (" << _partyInviteCount / minutes << " per minute) unique targets: " << _partyInviteTargets.size()
        << "\nBlacklist count: " << _blacklistCount
        << "\nTotal: " << Total() << " (" << Total() / minutes << " per minute)\n"
        << "Repetition score: " << RepetitionScore() << " Unique messages:\n";

    for (auto i = 0u; i < _uniqueMessages.size(); ++i)
    {
        auto const &u = _uniqueMessages[i];
        ret << "Repeats: " << u.first << " Message: \"" << u.second << "\"";

        if (i > 0)
            ret << " Distance from previous message: " << nam::damerau_levenshtein_distance(_uniqueMessages[i - 1].second, u.second);

        ret << "\n";
    }

    return ret.str();
}

// 记录自动回复、私聊、说话、大喊、频道、邮件、频道邀请与组队邀请事件并提交分析队列
void Antispam::AutoReply(const std::string &msg)
{
    std::lock_guard<std::mutex> guard(_mutex);

    NewMessage(msg);
}

void Antispam::Whisper(const std::string &msg, const ObjectGuid &to)
{
    std::lock_guard<std::mutex> guard(_mutex);

    ++_whisperCount;
    _whisperTargets.insert(to);
    NewMessage(msg);
}

void Antispam::Say(const std::string &msg)
{
    std::lock_guard<std::mutex> guard(_mutex);

    ++_sayCount;
    NewMessage(msg);
}

void Antispam::Yell(const std::string &msg)
{
    std::lock_guard<std::mutex> guard(_mutex);

    ++_yellCount;
    NewMessage(msg);
}

void Antispam::Channel(const std::string &msg)
{
    std::lock_guard<std::mutex> guard(_mutex);

    ++_channelCount;

    NewMessage(msg);
}

void Antispam::Mail(const std::string &subject, const std::string &body, const ObjectGuid &to)
{
    std::lock_guard<std::mutex> guard(_mutex);

    ++_mailCount;
    _mailTargets.insert(to);

    NewMessage(subject);
    NewMessage(body);
}

void Antispam::ChannelInvite(const std::string &channelName, const ObjectGuid &to)
{
    std::lock_guard<std::mutex> guard(_mutex);

    ++_channelInviteCount;
    _channelInviteTargets.insert(to);

    NewMessage(channelName);
}

void Antispam::PartyInvite(const ObjectGuid& to)
{
    std::lock_guard<std::mutex> guard(_mutex);

    ++_partyInviteCount;
    _partyInviteTargets.insert(to);
}

// 将新消息入队并调度后台分析线程检查
void Antispam::NewMessage(const std::string &msg)
{
    _pendingMessages.push_back(msg);

    sAntispamMgr.ScheduleAnalysis(shared_from_this());
}

// 获取玩家近期有效移动距离（超过静止超时时间则视为 0，用于压低挂机喊话脚本评分豁免）
float Antispam::GetDistanceTraveled() const
{
    auto const session = sWorld.FindSession(_account);

    if (!session)
        return 0.f;

    auto const player = session->GetPlayer();

    if (!player)
        return 0.f;

    auto const movementTimeout = sAnticheatConfig.GetAntispamRepetitionMovementTimeout() * IN_MILLISECONDS;
    auto const lastMovement = WorldTimer::getMSTime() - player->m_movementInfo.ctime;

    if (lastMovement > movementTimeout)
        return 0.f;

    auto const anticheat = dynamic_cast<const NamreebAnticheat::SessionAnticheat *>(session->GetAnticheat());

    if (!anticheat)
        return 0.f;

    return anticheat->GetDistanceTraveled();
}

// 计算消息重复度得分：结合重复发言次数、移动距离衰减与在线时长加权
uint32 Antispam::RepetitionScore() const
{
    // compute unique messages versus total messages and check for actions.
    // the initial score is how many times the person has repeated themselves
    // 统计总消息数及重复消息累加分值
    auto score = 0u, total = 0u;

    for (auto const &u : _uniqueMessages)
    {
        total += u.first;

        // don't count messages which have only occurred once as repeats (仅出现一次的消息不计入重复分)
        if (u.first > 1)
            score += static_cast<double>(u.first);
    }

    // if we don't have enough data, return zero (样本量不足时不判定重复)
    if (total < sAnticheatConfig.GetAntispamMinRepetitionMessages())
        return 0;

    if (auto const distanceScale = sAnticheatConfig.GetAntispamRepetitionDistanceScale())
    {
        auto const distanceTraveled = GetDistanceTraveled();

        // avoid division by zero, value will never be negative (移动距离越大分母越大，减少正常跑图玩家误报)
        if (distanceTraveled > 0.f)
            score /= distanceTraveled * distanceScale;
    }

    if (auto const timeScale = sAnticheatConfig.GetAntispamRepetitionTimeScale())
    {
        auto const milliseconds = WorldTimer::getMSTime() - _creationTime;
        auto const lifetime = static_cast<float>(milliseconds) / float(IN_MILLISECONDS);

        if (lifetime > 0)
            score *= lifetime * timeScale;
    }

    return score;
}

// 执行 5 步反垃圾综合分析：速率检查 -> 独立目标占比 -> 单条黑名单 -> 拼行黑名单 -> 编辑距离重复度评分
void Antispam::Analyze()
{
    // are they too high level to care about them? (账号最高等级高于配置阈值则跳过检测)
    if (GetAccountLevel(_account) > sAnticheatConfig.GetAntispamMaxLevel())
        return;

    std::lock_guard<std::mutex> guard(_mutex);

    // empty pending queue so we can halt at anytime without having to clean it up (一次性取出待分析消息队列)
    std::vector<std::string> messages = std::move(_pendingMessages);

    // update recent messages (更新近期消息环形历史)
    for (auto const &m : messages)
        _recentMessages.push_back(m);

    // step 1: check for messages being sent out too fast (第 1 步：检查发信频率是否过高)
    if (auto const maxRate = sAnticheatConfig.GetAntispamMaxRate())
    {
        if (auto const rate = Rate())
        {
            if (rate > maxRate)
            {
                Silence("Messaging rate too high.  %f > %u (messages per minute)", rate, maxRate);
                return;
            }
        }
    }

    // step 2: see if too many recipients are unique (第 2 步：检查私聊/邮件/邀请的独立目标比例是否异常偏高)
    if (auto const maxUniquePercentage = sAnticheatConfig.GetAntispamMaxUniquePercentage())
    {
        // whispers, mails and channel invites are the only message types we track which have specific targets
        auto const total = _whisperCount + _mailCount + _channelInviteCount + _partyInviteCount;

        // have they sent enough messages to be eligible for this step?
        if (total > sAnticheatConfig.GetAntispamMinUniqueMessages())
        {
            auto const unique = _whisperTargets.size() + _mailTargets.size() + _channelInviteTargets.size() + _partyInviteTargets.size();
            auto const percentage = (100.f * unique) / static_cast<float>(total);

            if (percentage >= maxUniquePercentage)
            {
                Silence("Too high unique target percentage.  %u of %u (%f >= %u)", unique, total, percentage, maxUniquePercentage);
                return;
            }
        }
    }

    // step 3: check for blacklist occurances.  if we reach the configured threshold, stop counting
    // 第 3 步：逐条匹配反垃圾黑名单关键词
    auto const oldBlacklistCount = _blacklistCount;
    for (auto const &msg : messages)
    {
        std::string log;
        auto const score = sAntispamMgr.CheckBlacklist(msg, log);

        // if blacklisted entries were found, record them
        if (!!score)
        {
            // this insertion will automatically do nothing if this message does not have one of the highest count of blacklist offenses
            _topBlacklistedMessages.insert(score, log);
            _blacklistCount += score;
        }
    }

    // step 4: if the blacklist count remains unchanged from step 3, re-analyze the messages with them all combined.
    // this is aimed at detecting spam spread across multiple lines.
    // 第 4 步：若逐条未命中，将本批多条消息拼接后再检测，识别跨行拆分的广告刷屏
    if (oldBlacklistCount == _blacklistCount)
    {
        std::stringstream str;
        for (auto const &msg : messages)
            str << msg;

        std::string log;
        auto const score = sAntispamMgr.CheckBlacklist(str.str(), log);

        if (!!score)
        {
            // this insertion will automatically do nothing if this message does not have one of the highest count of blacklist offenses
            _topBlacklistedMessages.insert(score, log);
            _blacklistCount += score;
        }
    }

    // if there have been blacklist score changes in steps 3 or 4... (若黑名单命中次数增长，按严重程度触发禁言或通知)
    if (oldBlacklistCount != _blacklistCount)
    {
        // if this is severe enough to silence them, do so
        if (_blacklistCount > sAnticheatConfig.GetAntispamBlacklistSilence() && sAnticheatConfig.GetAntispamBlacklistSilence() > 0)
        {
            auto const milliseconds = WorldTimer::getMSTime() - _creationTime;
            auto const minutes = static_cast<float>(milliseconds) / (IN_MILLISECONDS * MINUTE);

            Silence("Encountered too many blacklisted entries.  %u in %u messages in %f minutes", _blacklistCount, Total(), minutes);
            return;
        }
        // otherwise, if it is severe enough to inform GMs, do so
        else if (_blacklistCount > sAnticheatConfig.GetAntispamBlacklistNotify() && sAnticheatConfig.GetAntispamBlacklistNotify() > 0)
        {
            auto const milliseconds = WorldTimer::getMSTime() - _creationTime;
            auto const minutes = static_cast<float>(milliseconds) / (IN_MILLISECONDS * MINUTE);

            Notify("Encountered too many blacklisted entries.  %u in %u messages in %f minutes", _blacklistCount, Total(), minutes);
        }
    }

    // step 5: see if they are repeating their messages too often
    // 第 5 步：使用 Damerau-Levenshtein 编辑距离聚类相似发言，并依据重复度得分决定通知或禁言
    for (auto const &msg : messages)
    {
        // first see if the message is similar to previously observed unique messages
        bool found = false;
        for (auto i = 0u; i < _uniqueMessages.size(); ++i)
        {
            auto &u = _uniqueMessages[i];

            auto const distance = static_cast<uint32>(nam::damerau_levenshtein_distance(msg, u.second));

            // if these two messages are the same, increase the count (编辑距离小于阈值则归为同一类重复消息)
            if (distance < sAnticheatConfig.GetAntispamUniquenessThreshold())
            {
                ++u.first;
                found = true;
                break;
            }
        }

        if (found)
            continue;

        // otherwise, insert the message to track it for repetition
        _uniqueMessages.emplace_back(1, msg);
    }

    auto const score = RepetitionScore();

    // if this is severe enough to silence them, do so
    if (score >= sAnticheatConfig.GetAntispamRepetitionSilence() && sAnticheatConfig.GetAntispamRepetitionSilence() > 0)
    {
        Silence("Messages are too repetitive.  Score: %u", score);
        return;
    }
    // otherwise, if it is severe enough to inform GMs, do so
    else if (score >= sAnticheatConfig.GetAntispamRepetitionNotify() && sAnticheatConfig.GetAntispamRepetitionNotify() > 0)
    {
        Notify("Messages are too repetitive.  Score: %u", score);
    }

}
}
