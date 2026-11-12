/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "PacketLog.h"
#include "Util/Timer.h"
#include "Server/WorldPacket.h"
#include "Config/Config.h"
#include "Globals/SharedDefines.h"

#pragma pack(push, 1)

// Packet logging structures in PKT 3.1 format | PKT 3.1 格式的数据包日志头结构定义
struct LogHeader
{
    char Signature[3];                                      ///< 文件魔数 "PKT"
    uint16 FormatVersion;                                   ///< 格式版本号（0x0301）
    uint8 SnifferId;                                        ///< 嗅探器标识字符
    uint32 Build;                                           ///< 客户端期望版本号
    char Locale[4];                                         ///< 客户端语言区域（如 enUS）
    uint8 SessionKey[40];                                   ///< 会话密钥占位（全零）
    uint32 SniffStartUnixtime;                              ///< 抓包开始 Unix 时间戳
    uint32 SniffStartTicks;                                 ///< 抓包开始毫秒刻度
    uint32 OptionalDataSize;                                ///< 头部可选附加数据长度
};

/// 单条数据包记录头
struct PacketHeader
{
    // used to uniquely identify a connection | 用于唯一标识连接来源的可选数据
    struct OptionalData
    {
        uint8 SocketIPBytes[16];                            ///< IPv4/IPv6 地址字节
        uint32 SocketPort;                                  ///< 远端端口号
    };

    uint32 Direction;                                       ///< 传输方向标识（CMSG / SMSG）
    uint32 ConnectionId;                                    ///< 连接 ID
    uint32 ArrivalTicks;                                    ///< 到达/发送时的服务器毫秒刻
    uint32 OptionalDataSize;                                ///< OptionalData 字节数
    uint32 Length;                                          ///< 负载长度（含 4 字节 Opcode）
    OptionalData OptionalData;
    uint32 Opcode;                                          ///< 操作码
};

#pragma pack(pop)

PacketLog::PacketLog() : _file(nullptr)
{
    std::call_once(_initializeFlag, &PacketLog::Initialize, this);
}

PacketLog::~PacketLog()
{
    if (_file)
        fclose(_file);

    _file = nullptr;
}

PacketLog* PacketLog::instance()
{
    static PacketLog instance;
    return &instance;
}

void PacketLog::Initialize()
{
    static uint32 buildVersion[] = EXPECTED_MANGOSD_CLIENT_BUILD;
    std::string logsDir = sConfig.GetStringDefault("LogsDir", "");

    if (!logsDir.empty())
        if ((logsDir.at(logsDir.length() - 1) != '/') && (logsDir.at(logsDir.length() - 1) != '\\'))
            logsDir.push_back('/');

    std::string logname = sConfig.GetStringDefault("PacketLogFile", "");
    if (!logname.empty())
    {
        _file = fopen((logsDir + logname).c_str(), "wb");

        LogHeader header;
        header.Signature[0] = 'P'; header.Signature[1] = 'K'; header.Signature[2] = 'T';
        header.FormatVersion = 0x0301;
        header.SnifferId = 'T';
        header.Build = buildVersion[0];
        header.Locale[0] = 'e'; header.Locale[1] = 'n'; header.Locale[2] = 'U'; header.Locale[3] = 'S';
        std::memset(header.SessionKey, 0, sizeof(header.SessionKey));
        header.SniffStartUnixtime = time(nullptr);
        header.SniffStartTicks = WorldTimer::getMSTime();
        header.OptionalDataSize = 0;

        if (CanLogPacket())
            fwrite(&header, sizeof(header), 1, _file);
    }
}

void PacketLog::Reinitialize()
{
    std::lock_guard<std::mutex> lock(_logPacketLock);
    if (CanLogPacket())
    {
        fclose(_file);
        _file = nullptr;
    }
    Initialize();
}

void PacketLog::LogPacket(WorldPacket const& packet, Direction direction, boost::asio::ip::address const& addr, uint16 port)
{
    std::lock_guard<std::mutex> lock(_logPacketLock);

    PacketHeader header;
    header.Direction = direction == CLIENT_TO_SERVER ? 0x47534d43 : 0x47534d53;
    header.ConnectionId = 0;
    header.ArrivalTicks = WorldTimer::getMSTime();

    header.OptionalDataSize = sizeof(header.OptionalData);
    memset(header.OptionalData.SocketIPBytes, 0, sizeof(header.OptionalData.SocketIPBytes));
    if (addr.is_v4())
    {
        auto bytes = addr.to_v4().to_bytes();
        memcpy(header.OptionalData.SocketIPBytes, bytes.data(), bytes.size());
    }
    else if (addr.is_v6())
    {
        auto bytes = addr.to_v6().to_bytes();
        memcpy(header.OptionalData.SocketIPBytes, bytes.data(), bytes.size());
    }

    header.OptionalData.SocketPort = port;
    header.Length = packet.size() + sizeof(header.Opcode);
    header.Opcode = packet.GetOpcode();

    fwrite(&header, sizeof(header), 1, _file);
    if (!packet.empty())
        fwrite(packet.contents(), 1, packet.size(), _file);

    fflush(_file);
}
