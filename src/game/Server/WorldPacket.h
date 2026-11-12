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

#ifndef MANGOSSERVER_WORLDPACKET_H
#define MANGOSSERVER_WORLDPACKET_H

#include "Common.h"
#include "Util/ByteBuffer.h"
#include "Server/Opcodes.h"
#include <chrono>

// Note: m_opcode and size stored in platfom dependent format
// ignore endianess until send, and converted at receive
// 注意：m_opcode 与数据包长度在内存中按主机字节序保存，仅在发送前与接收时进行字节序转换
/// 世界服网络数据包封装（继承自 ByteBuffer，附带操作码与接收时间戳）
class WorldPacket : public ByteBuffer
{
    public:
        // just container for later use | 默认构造：创建空数据包容器（Opcode 为 MSG_NULL_ACTION）
        WorldPacket() : ByteBuffer(0), m_opcode(MSG_NULL_ACTION)
        {
        }
        explicit WorldPacket(Opcodes opcode, size_t reservedSize = 200) : ByteBuffer(reservedSize), m_opcode(opcode) {}

        /// 重置并重新初始化缓冲区容量与操作码
        void Initialize(Opcodes opcode, size_t reservedSize = 200)
        {
            clear();
            reserve(reservedSize);
            m_opcode = opcode;
        }

        Opcodes GetOpcode() const { return m_opcode; }
        void SetOpcode(Opcodes opcode) { m_opcode = opcode; }
        /// 获取操作码对应的可读名称字符串
        inline const char* GetOpcodeName() const { return LookupOpcodeName(m_opcode); }

        std::chrono::steady_clock::time_point GetReceivedTime() const { return m_receivedTime; }
        void SetReceivedTime(std::chrono::steady_clock::time_point receivedTime) { m_receivedTime = receivedTime; }

    private:
        Opcodes m_opcode;                                   ///< 数据包操作码
        std::chrono::steady_clock::time_point m_receivedTime; // only set for a specific set of opcodes, for performance reasons. | 数据包接收时间戳（出于性能考虑仅对特定操作码设置）
};
#endif
