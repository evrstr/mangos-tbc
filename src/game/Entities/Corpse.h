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

#ifndef MANGOSSERVER_CORPSE_H
#define MANGOSSERVER_CORPSE_H

#include "Common.h"
#include "Entities/Object.h"
#include "Database/DatabaseEnv.h"
#include "Maps/GridDefines.h"

// 玩家尸体类型枚举
enum CorpseType
{
    CORPSE_BONES             = 0,                           // 骸骨（不可复活，仅作视觉残留）
    CORPSE_RESURRECTABLE_PVE = 1,                           // PVE 可复活尸体
    CORPSE_RESURRECTABLE_PVP = 2                            // PVP 可复活尸体（战场中可被剥取徽章等）
};
#define MAX_CORPSE_TYPE        3

// Value equal client resurrection dialog show radius. (与客户端弹出复活确认框的距离半径一致)
#define CORPSE_RECLAIM_RADIUS 39

// 尸体状态标志位
enum CorpseFlags
{
    CORPSE_FLAG_NONE        = 0x00,
    CORPSE_FLAG_BONES       = 0x01,                         // 骸骨状态
    CORPSE_FLAG_UNK1        = 0x02,
    CORPSE_FLAG_PVP         = 0x04,                         // PVP 尸体标志
    CORPSE_FLAG_HIDE_HELM   = 0x08,                         // 隐藏头盔
    CORPSE_FLAG_HIDE_CLOAK  = 0x10,                         // 隐藏披风
    CORPSE_FLAG_LOOTABLE    = 0x20                          // 可拾取（战场尸体徽章）
};

// 玩家尸体世界对象类（管理复活尸体与骸骨生命周期、外观字段及数据库持久化）
class Corpse : public WorldObject
{
    public:
        explicit Corpse(CorpseType type = CORPSE_BONES);
        ~Corpse();

        // 注册/注销于全局 ObjectAccessor 并加入/移出世界
        void AddToWorld() override;
        void RemoveFromWorld() override;

        // 创建空尸体或绑定所属玩家的尸体对象
        bool Create(uint32 guidlow);
        bool Create(uint32 guidlow, Player* owner);

        // 尸体记录的数据库保存与加载
        void SaveToDB();
        bool LoadFromDB(uint32 lowguid, Field* fields);

        // 从世界移除骸骨 / 从数据库删除非骸骨尸体
        void DeleteBonesFromWorld();
        void DeleteFromDB() const;

        ObjectGuid const& GetOwnerGuid() const override { return GetGuidValue(CORPSE_FIELD_OWNER); }
        void SetOwnerGuid(ObjectGuid guid) override { SetGuidValue(CORPSE_FIELD_OWNER, guid); }

        uint8 getRace() const { return GetByteValue(CORPSE_FIELD_BYTES_1, 1); }
        uint32 getRaceMask() const { return convertEnumToFlag(getRace()); }
        uint8 getGender() const { return GetByteValue(CORPSE_FIELD_BYTES_1, 2); }

        // faction template id (根据尸体种族获取阵营模板 ID)
        uint32 GetFaction() const override;

        time_t const& GetGhostTime() const { return m_time; }
        void ResetGhostTime() { m_time = time(nullptr); }
        CorpseType GetType() const { return m_type; }

        GridPair const& GetGrid() const { return m_grid; }
        void SetGrid(GridPair const& grid) { m_grid = grid; }

        bool isVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const override;

        Player* lootRecipient;                              // 尸体拾取归属者（战场徽章拾取用）
        bool lootForBody;                                   // 是否允许对尸体进行拾取

        GridReference<Corpse>& GetGridRef() { return m_gridRef; }

        // 判断尸体或骸骨是否已过期
        bool IsExpired(time_t t) const;
        Team GetTeam() const;
    private:
        GridReference<Corpse> m_gridRef;

        CorpseType m_type;                                  // 尸体类型
        time_t m_time;                                      // 创建/转为灵魂状态的时间戳
        GridPair m_grid;                                    // gride for corpse position for fast search (尸体所在网格坐标，用于快速检索)
};
#endif
