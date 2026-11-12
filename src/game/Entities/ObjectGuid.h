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

#ifndef MANGOS_OBJECT_GUID_H
#define MANGOS_OBJECT_GUID_H

#include "Common.h"
#include "Util/ByteBuffer.h"
#include <atomic>

// 对象类型 ID 枚举
enum TypeID
{
    TYPEID_OBJECT        = 0,
    TYPEID_ITEM          = 1,
    TYPEID_CONTAINER     = 2,
    TYPEID_UNIT          = 3,
    TYPEID_PLAYER        = 4,
    TYPEID_GAMEOBJECT    = 5,
    TYPEID_DYNAMICOBJECT = 6,
    TYPEID_CORPSE        = 7
};

// 对象类型 ID 总数
#define MAX_TYPE_ID        8

// 对象类型位掩码枚举
enum TypeMask
{
    TYPEMASK_OBJECT         = 0x0001,
    TYPEMASK_ITEM           = 0x0002,
    TYPEMASK_CONTAINER      = 0x0004,
    TYPEMASK_UNIT           = 0x0008,                       // players also have it (玩家同样包含此标记)
    TYPEMASK_PLAYER         = 0x0010,
    TYPEMASK_GAMEOBJECT     = 0x0020,
    TYPEMASK_DYNAMICOBJECT  = 0x0040,
    TYPEMASK_CORPSE         = 0x0080,

    // used combinations in Player::GetObjectByTypeMask (TYPEMASK_UNIT case ignore players in call)
    // Player::GetObjectByTypeMask 中使用的组合掩码（TYPEMASK_UNIT 在调用中会忽略玩家）
    TYPEMASK_CREATURE_OR_GAMEOBJECT = TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT,
    TYPEMASK_CREATURE_GAMEOBJECT_OR_ITEM = TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT | TYPEMASK_ITEM,
    TYPEMASK_CREATURE_GAMEOBJECT_PLAYER_OR_ITEM = TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT | TYPEMASK_ITEM | TYPEMASK_PLAYER,

    TYPEMASK_WORLDOBJECT = TYPEMASK_UNIT | TYPEMASK_PLAYER | TYPEMASK_GAMEOBJECT | TYPEMASK_DYNAMICOBJECT | TYPEMASK_CORPSE,
};

// 64 位 GUID 高 16 位类型前缀枚举
enum HighGuid
{
    HIGHGUID_ITEM           = 0x4000,                       // blizz 4000 (物品)
    HIGHGUID_CONTAINER      = 0x4000,                       // blizz 4000 (容器/背包)
    HIGHGUID_PLAYER         = 0x0000,                       // blizz 0000 (玩家)
    HIGHGUID_GAMEOBJECT     = 0xF110,                       // blizz F110 (游戏对象)
    HIGHGUID_TRANSPORT      = 0xF120,                       // blizz F120 (for GAMEOBJECT_TYPE_TRANSPORT) (本地传送载具)
    HIGHGUID_UNIT           = 0xF130,                       // blizz F130 (生物/NPC)
    HIGHGUID_PET            = 0xF140,                       // blizz F140 (宠物)
    HIGHGUID_DYNAMICOBJECT  = 0xF100,                       // blizz F100 (动态法术对象)
    HIGHGUID_CORPSE         = 0xF101,                       // blizz F100 (玩家尸体)
    HIGHGUID_MO_TRANSPORT   = 0x1FC0,                       // blizz 1FC0 (for GAMEOBJECT_TYPE_MO_TRANSPORT) (跨地图大型运输工具)
    HIGHGUID_GROUP          = 0x1F50,                       // blizz 1F5x (队伍)
};

class ObjectGuid;
class PackedGuid;

// 用于从 ByteBuffer 读取压缩 GUID 到目标 ObjectGuid 的辅助适配器
struct PackedGuidReader
{
    explicit PackedGuidReader(ObjectGuid& guid) : m_guidPtr(&guid) {}
    ObjectGuid* m_guidPtr;                                  // 目标 ObjectGuid 指针
};

// 游戏对象全局唯一标识符封装类（封装 64 位 GUID：高 16 位类型 + 可选 24 位 Entry + 低位计数器）
class ObjectGuid
{
    public:                                                 // constructors (构造函数)
        ObjectGuid() : m_guid(0) {}
        explicit ObjectGuid(uint64 const& guid) : m_guid(guid) {}
        ObjectGuid(HighGuid hi, uint32 entry, uint32 counter) : m_guid(counter ? uint64(counter) | (uint64(entry) << 24) | (uint64(hi) << 48) : 0) {}
        ObjectGuid(HighGuid hi, uint32 counter) : m_guid(counter ? uint64(counter) | (uint64(hi) << 48) : 0) {}

        operator uint64() const { return m_guid; }
    private:
        explicit ObjectGuid(uint32 const&);                 // no implementation, used for catch wrong type assign (未实现，用于编译期捕获错误类型赋值)
        ObjectGuid(HighGuid, uint32, uint64 counter);       // no implementation, used for catch wrong type assign (未实现，用于编译期捕获错误类型赋值)
        ObjectGuid(HighGuid, uint64 counter);               // no implementation, used for catch wrong type assign (未实现，用于编译期捕获错误类型赋值)

    public:                                                 // modifiers (修改器)
        PackedGuidReader ReadAsPacked() { return PackedGuidReader(*this); }

        void Set(uint64 const& guid) { m_guid = guid; }
        void Clear() { m_guid = 0; }

        PackedGuid WriteAsPacked() const;
    public:                                                 // accessors (访问器)
        uint64 const& GetRawValue() const { return m_guid; }
        HighGuid GetHigh() const { return HighGuid((m_guid >> 48) & 0x0000FFFF); }
        uint32   GetEntry() const { return HasEntry() ? uint32((m_guid >> 24) & uint64(0x0000000000FFFFFF)) : 0; }
        uint32   GetCounter()  const
        {
            return HasEntry()
                   ? uint32(m_guid & uint64(0x0000000000FFFFFF))
                   : uint32(m_guid & uint64(0x00000000FFFFFFFF));
        }

        // 获取指定 HighGuid 类型允许的最大低位计数器上限
        static uint32 GetMaxCounter(HighGuid high)
        {
            return HasEntry(high)
                   ? uint32(0x00FFFFFF)
                   : uint32(0xFFFFFFFF);
        }

        uint32 GetMaxCounter() const { return GetMaxCounter(GetHigh()); }

        bool IsEmpty()             const { return m_guid == 0;                                }
        bool IsCreature()          const { return GetHigh() == HIGHGUID_UNIT;                 }
        bool IsPet()               const { return GetHigh() == HIGHGUID_PET;                  }
        bool IsCreatureOrPet()     const { return IsCreature() || IsPet();                    }
        bool IsAnyTypeCreature()   const { return IsCreature() || IsPet();                    } // wrapper to master branch (兼容主分支的生物或宠物判断封装)
        bool IsPlayer()            const { return !IsEmpty() && GetHigh() == HIGHGUID_PLAYER; }
        bool IsUnit()              const { return IsAnyTypeCreature() || IsPlayer();          }
        bool IsItem()              const { return GetHigh() == HIGHGUID_ITEM;                 }
        bool IsGameObject()        const { return GetHigh() == HIGHGUID_GAMEOBJECT;           }
        bool IsDynamicObject()     const { return GetHigh() == HIGHGUID_DYNAMICOBJECT;        }
        bool IsCorpse()            const { return GetHigh() == HIGHGUID_CORPSE;               }
        bool IsTransport()         const { return GetHigh() == HIGHGUID_TRANSPORT;            }
        bool IsMOTransport()       const { return GetHigh() == HIGHGUID_MO_TRANSPORT;         }
        bool IsGroup()             const { return GetHigh() == HIGHGUID_GROUP;                }

        // 根据 HighGuid 映射对应的 TypeID
        static TypeID GetTypeId(HighGuid high)
        {
            switch (high)
            {
                case HIGHGUID_ITEM:         return TYPEID_ITEM;
                // case HIGHGUID_CONTAINER:    return TYPEID_CONTAINER; HIGHGUID_CONTAINER==HIGHGUID_ITEM currently (当前容器与物品共用高 16 位)
                case HIGHGUID_UNIT:         return TYPEID_UNIT;
                case HIGHGUID_PET:          return TYPEID_UNIT;
                case HIGHGUID_PLAYER:       return TYPEID_PLAYER;
                case HIGHGUID_GAMEOBJECT:   return TYPEID_GAMEOBJECT;
                case HIGHGUID_TRANSPORT:    return TYPEID_GAMEOBJECT;
                case HIGHGUID_DYNAMICOBJECT: return TYPEID_DYNAMICOBJECT;
                case HIGHGUID_CORPSE:       return TYPEID_CORPSE;
                case HIGHGUID_MO_TRANSPORT: return TYPEID_GAMEOBJECT;
                // unknown (未知类型)
                case HIGHGUID_GROUP:
                default:                    return TYPEID_OBJECT;
            }
        }

        TypeID GetTypeId() const { return GetTypeId(GetHigh()); }

        bool operator!() const { return IsEmpty(); }
        bool operator== (ObjectGuid const& guid) const { return GetRawValue() == guid.GetRawValue(); }
        bool operator!= (ObjectGuid const& guid) const { return GetRawValue() != guid.GetRawValue(); }
        bool operator< (ObjectGuid const& guid) const { return GetRawValue() < guid.GetRawValue(); }

    public:                                                 // accessors - for debug (调试用访问器)
        static char const* GetTypeName(HighGuid high);
        char const* GetTypeName() const { return !IsEmpty() ? GetTypeName(GetHigh()) : "None"; }
        std::string GetString() const;

    private:                                                // internal functions (内部辅助函数)
        // 判断该 HighGuid 类型是否在中间 24 位编码 Entry
        static bool HasEntry(HighGuid high)
        {
            switch (high)
            {
                case HIGHGUID_ITEM:
                case HIGHGUID_PLAYER:
                case HIGHGUID_DYNAMICOBJECT:
                case HIGHGUID_CORPSE:
                case HIGHGUID_MO_TRANSPORT:
                case HIGHGUID_GROUP:
                    return false;
                case HIGHGUID_GAMEOBJECT:
                case HIGHGUID_TRANSPORT:
                case HIGHGUID_UNIT:
                case HIGHGUID_PET:
                default:
                    return true;
            }
        }

        bool HasEntry() const { return HasEntry(GetHigh()); }

    private:                                                // fields (成员变量)
        uint64 m_guid;                                      // 原始 64 位 GUID 数值
};

// Some Shared defines (常用 GUID 容器类型别名)
typedef std::set<ObjectGuid> GuidSet;
typedef std::list<ObjectGuid> GuidList;
typedef std::vector<ObjectGuid> GuidVector;

// minimum buffer size for packed guid is 9 bytes (压缩 GUID 缓冲区最小长度为 9 字节：1 字节位掩码 + 最多 8 字节非零数据)
#define PACKED_GUID_MIN_BUFFER_SIZE 9

// 压缩格式的 GUID 封装类（用于网络包节省带宽）
class PackedGuid
{
        friend ByteBuffer& operator<< (ByteBuffer& buf, PackedGuid const& guid);

    public:                                                 // constructors (构造函数)
        explicit PackedGuid() : m_packedGuid(PACKED_GUID_MIN_BUFFER_SIZE) { m_packedGuid.appendPackGUID(0); }
        explicit PackedGuid(uint64 const& guid) : m_packedGuid(PACKED_GUID_MIN_BUFFER_SIZE) { m_packedGuid.appendPackGUID(guid); }
        explicit PackedGuid(ObjectGuid const& guid) : m_packedGuid(PACKED_GUID_MIN_BUFFER_SIZE) { m_packedGuid.appendPackGUID(guid.GetRawValue()); }

    public:                                                 // modifiers (修改器)
        void Set(uint64 const& guid) { m_packedGuid.wpos(0); m_packedGuid.appendPackGUID(guid); }
        void Set(ObjectGuid const& guid) { m_packedGuid.wpos(0); m_packedGuid.appendPackGUID(guid.GetRawValue()); }

    public:                                                 // accessors (访问器)
        size_t size() const { return m_packedGuid.size(); }

    private:                                                // fields (成员变量)
        ByteBuffer m_packedGuid;                            // 序列化后的压缩 GUID 字节缓冲
};

// 线程安全的对象 GUID 低位计数器生成器模板
template<HighGuid high>
class ObjectGuidGenerator
{
    public:                                                 // constructors (构造函数)
        explicit ObjectGuidGenerator(uint32 start = 1) : m_nextGuid(start) {}

    public:                                                 // modifiers (修改器)
        void Set(uint32 val) { m_nextGuid = val; }
        uint32 Generate();

    public:                                                 // accessors (访问器)
        uint32 GetNextAfterMaxUsed() const { return m_nextGuid; }

    private:                                                // fields (成员变量)
        std::atomic<uint32> m_nextGuid;                     // 原子递增的下一个可用低位 GUID
};

ByteBuffer& operator<< (ByteBuffer& buf, ObjectGuid const& guid);
ByteBuffer& operator>> (ByteBuffer& buf, ObjectGuid&       guid);

ByteBuffer& operator<< (ByteBuffer& buf, PackedGuid const& guid);
ByteBuffer& operator>> (ByteBuffer& buf, PackedGuidReader const& guid);

inline PackedGuid ObjectGuid::WriteAsPacked() const { return PackedGuid(*this); }

namespace std
{
    template<>
    class hash<ObjectGuid>
    {
        public:

            size_t operator()(ObjectGuid const& key) const
            {
                return hash<uint64>()(key.GetRawValue());
            }
    };
}

#endif
