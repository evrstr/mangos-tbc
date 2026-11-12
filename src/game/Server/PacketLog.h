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

#ifndef TRINITY_PACKETLOG_H
#define TRINITY_PACKETLOG_H

#include "Common.h"

#include <boost/asio/ip/address.hpp>
#include <mutex>

/// 数据包传输方向
enum Direction
{
    CLIENT_TO_SERVER,                                       ///< 客户端到服务端（上行）
    SERVER_TO_CLIENT                                        ///< 服务端到客户端（下行）
};

class WorldPacket;

/// PKT 3.1 格式网络数据包抓包日志记录器（线程安全单例）
class PacketLog
{
    private:
        PacketLog();
        ~PacketLog();
        std::mutex _logPacketLock;                          ///< 保护日志文件写入的互斥锁
        std::once_flag _initializeFlag;                     ///< 保证单次初始化的标志

    public:
        static PacketLog* instance();

        /// 根据配置打开日志文件并写入 PKT 文件头
        void Initialize();
        /// 关闭现有日志文件并按当前配置重新打开
        void Reinitialize();
        /// 判断抓包日志是否已启用且文件句柄有效
        bool CanLogPacket() const { return (_file != nullptr); }
        /// 写入单条数据包记录（含方向、时间戳、远端 IP/端口、Opcode 与负载）
        void LogPacket(WorldPacket const& packet, Direction direction, boost::asio::ip::address const& addr, uint16 port);

    private:
        FILE* _file;                                        ///< 输出抓包文件指针
};

#define sPacketLog PacketLog::instance()
#endif
