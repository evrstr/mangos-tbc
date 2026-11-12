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

#ifndef __UPDATEDATA_H
#define __UPDATEDATA_H

#include "Util/ByteBuffer.h"
#include "Entities/ObjectGuid.h"

class WorldPacket;
class WorldSession;

// 对象更新块类型枚举（对应客户端 SMSG_UPDATE_OBJECT 协议块类型）
enum ObjectUpdateType
{
    UPDATETYPE_VALUES               = 0,                    // 仅更新对象字段值
    UPDATETYPE_MOVEMENT             = 1,                    // 仅更新对象移动状态
    UPDATETYPE_CREATE_OBJECT        = 2,                    // 创建对象（已有视野对象初始化）
    UPDATETYPE_CREATE_OBJECT2       = 3,                    // 创建新生成对象（播放生成特效等）
    UPDATETYPE_OUT_OF_RANGE_OBJECTS = 4,                    // 移出视野的对象列表
    UPDATETYPE_NEAR_OBJECTS         = 5                     // 附近对象列表
};

// 对象更新标志位枚举
enum ObjectUpdateFlags
{
    UPDATEFLAG_NONE                 = 0x0000,               // 无附加标志
    UPDATEFLAG_SELF                 = 0x0001,               // 更新对象为客户端玩家自身
    UPDATEFLAG_TRANSPORT            = 0x0002,               // 包含运输载具移动/进度信息
    UPDATEFLAG_HAS_ATTACKING_TARGET = 0x0004,               // 包含当前攻击目标 GUID
    UPDATEFLAG_LOWGUID              = 0x0008,               // 包含低 32 位 GUID
    UPDATEFLAG_HIGHGUID             = 0x0010,               // 包含高 32 位 GUID
    UPDATEFLAG_LIVING               = 0x0020,               // 活体单位（包含完整移动包与速度表）
    UPDATEFLAG_HAS_POSITION         = 0x0040,               // 静态位置对象（包含坐标与朝向）
};

// 单个待发送更新数据包对应的缓冲与元信息
struct BufferPair
{
    ByteBuffer m_buffer;                                    // 序列化的更新块字节缓冲
    uint32 m_blockCount;                                    // 本缓冲内包含的更新块数量
    bool m_hasTransport;                                    // 是否包含运输载具相关更新块
};

// 对象状态更新数据聚合器（负责分包、zlib 压缩、视野移除列表及创建后附加包发送）
class UpdateData
{
    public:
        UpdateData();

        void AddOutOfRangeGUID(GuidSet& guids);
        void AddOutOfRangeGUID(ObjectGuid const& guid);
        void AddUpdateBlock(const ByteBuffer& block, bool transport);
        void AddAfterCreatePacket(const WorldPacket& data);
        WorldPacket BuildPacket(size_t index); // Copy Elision is a thing (依赖返回值拷贝消除)
        bool HasData() const { return m_data[0].m_buffer.size() > 0 || !m_outOfRangeGUIDs.empty(); }
        size_t GetPacketCount() const { return m_data.size(); }
        void Clear();

        GuidSet const& GetOutOfRangeGUIDs() const { return m_outOfRangeGUIDs; }

        void SendData(WorldSession& session, bool forceSend = false);

    protected:
        GuidSet m_outOfRangeGUIDs;                          // 本次同步移出视野的对象 GUID 集合
        std::vector<BufferPair> m_data;                     // 分片存储的更新数据包缓冲列表
        uint32 m_currentIndex;                              // 当前正在写入的分片索引

        std::vector<WorldPacket> m_afterCreatePacket;       // 在对象创建包之后紧接着发送的附加数据包列表

        static void Compress(void* dst, uint32* dst_size, void* src, int src_size);
};
#endif
