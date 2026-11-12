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

#include <zlib.h>

#include "Common.h"
#include "Entities/UpdateData.h"
#include "Util/ByteBuffer.h"
#include "Server/WorldPacket.h"
#include "Log/Log.h"
#include "Server/Opcodes.h"
#include "World/World.h"
#include "Entities/ObjectGuid.h"
#include "Server/WorldSession.h"

// 构造 UpdateData，预分配首个更新缓冲分片
UpdateData::UpdateData() : m_data(1, {ByteBuffer(0), 0, false}), m_currentIndex(0)
{
}

// 批量添加移出视野的对象 GUID 集合
void UpdateData::AddOutOfRangeGUID(GuidSet& guids)
{
    m_outOfRangeGUIDs.insert(guids.begin(), guids.end());
}

// 添加单个移出视野的对象 GUID
void UpdateData::AddOutOfRangeGUID(ObjectGuid const& guid)
{
    m_outOfRangeGUIDs.insert(guid);
}

// 追加一个对象更新块；若超过单包大小上限则自动开辟新的分片缓冲
void UpdateData::AddUpdateBlock(const ByteBuffer& block, bool transport)
{
    const size_t existing = (128 + (9 * m_outOfRangeGUIDs.size()) + m_data[m_currentIndex].m_buffer.size());

    if ((existing + block.size()) < MAX_NETCLIENT_PACKET_SIZE)
    {
        m_data[m_currentIndex].m_buffer.append(block);
        ++m_data[m_currentIndex].m_blockCount;
        if (transport)
            m_data[m_currentIndex].m_hasTransport = true;
    }
    else
    {
        ++m_currentIndex;
        m_data.emplace_back();
        m_data[m_currentIndex].m_buffer.append(block);
        m_data[m_currentIndex].m_blockCount = 1;
        m_data[m_currentIndex].m_hasTransport = transport;
    }
}

// 添加在创建包之后立即发送的附加数据包
void UpdateData::AddAfterCreatePacket(const WorldPacket& data)
{
    m_afterCreatePacket.emplace_back(data);
}

// 使用 zlib 对更新数据包载荷执行 deflate 压缩
void UpdateData::Compress(void* dst, uint32* dst_size, void* src, int src_size)
{
    z_stream c_stream;

    c_stream.zalloc = (alloc_func)nullptr;
    c_stream.zfree = (free_func)nullptr;
    c_stream.opaque = (voidpf)nullptr;

    // default Z_BEST_SPEED (1) (按服务端配置的压缩级别初始化 zlib，默认 Z_BEST_SPEED=1)
    int z_res = deflateInit(&c_stream, sWorld.getConfig(CONFIG_UINT32_COMPRESSION));
    if (z_res != Z_OK)
    {
        sLog.outError("Can't compress update packet (zlib: deflateInit) Error code: %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    c_stream.next_out = (Bytef*)dst;
    c_stream.avail_out = *dst_size;
    c_stream.next_in = (Bytef*)src;
    c_stream.avail_in = (uInt)src_size;

    z_res = deflate(&c_stream, Z_NO_FLUSH);
    if (z_res != Z_OK)
    {
        sLog.outError("Can't compress update packet (zlib: deflate) Error code: %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    if (c_stream.avail_in != 0)
    {
        sLog.outError("Can't compress update packet (zlib: deflate not greedy)");
        *dst_size = 0;
        return;
    }

    z_res = deflate(&c_stream, Z_FINISH);
    if (z_res != Z_STREAM_END)
    {
        sLog.outError("Can't compress update packet (zlib: deflate should report Z_STREAM_END instead %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    z_res = deflateEnd(&c_stream);
    if (z_res != Z_OK)
    {
        sLog.outError("Can't compress update packet (zlib: deflateEnd) Error code: %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    *dst_size = c_stream.total_out;
}

// 组装指定分片索引的 WorldPacket（包含移出视野列表与更新块，超过 100 字节时自动压缩）
WorldPacket UpdateData::BuildPacket(size_t index)
{
    WorldPacket packet;
    MANGOS_ASSERT(packet.empty());                         // shouldn't happen (断言新构造的数据包为空)

    ByteBuffer buf(4 + 1 + (m_outOfRangeGUIDs.empty() ? 0 : 1 + 4 + 9 * m_outOfRangeGUIDs.size()) + m_data[index].m_buffer.wpos());

    buf << (uint32)(!m_outOfRangeGUIDs.empty() ? m_data[index].m_blockCount + 1 : m_data[index].m_blockCount);
    buf << (uint8)(m_data[index].m_hasTransport);

    if (!m_outOfRangeGUIDs.empty())
    {
        buf << (uint8) UPDATETYPE_OUT_OF_RANGE_OBJECTS;
        buf << (uint32) m_outOfRangeGUIDs.size();

        for (auto m_outOfRangeGUID : m_outOfRangeGUIDs)
            buf << m_outOfRangeGUID.WriteAsPacked();
    }

    buf.append(m_data[index].m_buffer);

    size_t pSize = buf.wpos();                              // use real used data size (获取实际写入的数据字节长度)

    if (pSize > 100)                                        // compress large packets (超过 100 字节的大包启用 zlib 压缩发送)
    {
        uint32 destsize = compressBound(pSize);
        packet.resize(destsize + sizeof(uint32));

        packet.put<uint32>(0, pSize);
        Compress(const_cast<uint8*>(packet.contents()) + sizeof(uint32), &destsize, (void*)buf.contents(), pSize);
        if (destsize == 0)
            return packet;

        packet.resize(destsize + sizeof(uint32));
        packet.SetOpcode(SMSG_COMPRESSED_UPDATE_OBJECT);
    }
    else                                                    // send small packets without compression (小包直接明文发送)
    {
        packet.append(buf);
        packet.SetOpcode(SMSG_UPDATE_OBJECT);
    }

    return packet;
}

// 清空已缓存的更新块与移出视野 GUID
void UpdateData::Clear()
{
    m_data.clear();
    m_outOfRangeGUIDs.clear();
}

// 将所有分片更新包及附加包发送给目标玩家会话
void UpdateData::SendData(WorldSession& session, bool forceSend/*= false*/)
{
    if (!HasData())
        return;

    for (size_t i = 0; i < GetPacketCount(); ++i)
    {
        WorldPacket packet = BuildPacket(i);
        session.SendPacket(packet);
    }

    for (auto& packet : m_afterCreatePacket)
        session.SendPacket(packet, forceSend);
}
