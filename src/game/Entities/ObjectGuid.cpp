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

#include "Entities/ObjectGuid.h"

#include "World/World.h"
#include "Globals/ObjectMgr.h"

#include <sstream>

// 获取指定 HighGuid 类型对应的调试名称字符串
char const* ObjectGuid::GetTypeName(HighGuid high)
{
    switch (high)
    {
        case HIGHGUID_ITEM:         return "Item";
        case HIGHGUID_PLAYER:       return "Player";
        case HIGHGUID_GAMEOBJECT:   return "Gameobject";
        case HIGHGUID_TRANSPORT:    return "Transport";
        case HIGHGUID_UNIT:         return "Creature";
        case HIGHGUID_PET:          return "Pet";
        case HIGHGUID_DYNAMICOBJECT: return "DynObject";
        case HIGHGUID_CORPSE:       return "Corpse";
        case HIGHGUID_MO_TRANSPORT: return "MoTransport";
        case HIGHGUID_GROUP:        return "Group";
        default:
            return "<unknown>";
    }
}

// 格式化输出用于日志与调试的可读 GUID 描述字符串
std::string ObjectGuid::GetString() const
{
    std::ostringstream str;
    str << GetTypeName();

    if (IsPlayer())
    {
        std::string name;
        if (sObjectMgr.GetPlayerNameByGUID(*this, name))
            str << " " << name;
    }

    str << " (";
    if (HasEntry())
        str << (IsPet() ? "Petnumber: " : "Entry: ") << GetEntry() << " ";
    str << "Counter: " << GetCounter() << ")";
    return str.str();
}

// 生成下一个唯一低位 GUID；若接近上限则报错并安全关闭服务器
template<HighGuid high>
uint32 ObjectGuidGenerator<high>::Generate()
{
    if (m_nextGuid >= ObjectGuid::GetMaxCounter(high) - 1000)
    {
        sLog.outError("%s guid overflow very close!! Can't continue, shutting down server. ", ObjectGuid::GetTypeName(high));
        World::StopNow(ERROR_EXIT_CODE);
    }
    return m_nextGuid++;
}

// 将完整 64 位 ObjectGuid 写入字节缓冲区
ByteBuffer& operator<< (ByteBuffer& buf, ObjectGuid const& guid)
{
    buf << uint64(guid.GetRawValue());
    return buf;
}

// 从字节缓冲区读取完整 64 位 ObjectGuid
ByteBuffer& operator>>(ByteBuffer& buf, ObjectGuid& guid)
{
    guid.Set(buf.read<uint64>());
    return buf;
}

// 将压缩后的 PackedGuid 写入字节缓冲区
ByteBuffer& operator<< (ByteBuffer& buf, PackedGuid const& guid)
{
    buf.append(guid.m_packedGuid);
    return buf;
}

// 从字节缓冲区读取压缩 GUID 到目标 ObjectGuid
ByteBuffer& operator>>(ByteBuffer& buf, PackedGuidReader const& guid)
{
    guid.m_guidPtr->Set(buf.readPackGUID());
    return buf;
}

// 显式实例化各常见 HighGuid 类型的 GUID 生成器
template uint32 ObjectGuidGenerator<HIGHGUID_ITEM>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_PLAYER>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_GAMEOBJECT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_TRANSPORT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_UNIT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_PET>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_DYNAMICOBJECT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_CORPSE>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_GROUP>::Generate();
