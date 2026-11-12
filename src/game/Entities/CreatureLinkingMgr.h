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

/**
 * @addtogroup npc_linking System to link groups of NPCs together
 * This NPC-linking system in MaNGOS consists of 2 files:
 * - CreatureLinkingMgr.h
 * - CreatureLinkingMgr.cpp
 * as well of
 * - hooks in Creature.cpp, to trigger actions
 * - holder of the linked npcs for every map
 *
 * @{
 *
 * @file CreatureLinkingMgr.h
 * This file contains the the headers needed for MaNGOS to link NPCs together
 *
 */

#ifndef CREATURE_LINKING_MGR_H
#define CREATURE_LINKING_MGR_H

#include "Common.h"
#include "Entities/ObjectGuid.h"

class Unit;
class Creature;
class Map;

// enum on which Events an action for linked NPCs can trigger
// 触发 NPC 联动动作的事件类型枚举
enum CreatureLinkingEvent
{
    LINKING_EVENT_AGGRO         = 0,                        // 进入战斗（产生仇恨）
    LINKING_EVENT_EVADE         = 1,                        // 脱战重置（Evade）
    LINKING_EVENT_DIE           = 2,                        // 死亡
    LINKING_EVENT_RESPAWN       = 3,                        // 重生刷新
    LINKING_EVENT_DESPAWN       = 4,                        // 强制消失（Despawn）
};

// enum describing possible flags action flags for NPCs linked to other NPCs
// These flags are actually put into the database
// FLAG_TO_ means, that in this case the linked NPC will also trigger an action for the NPC it is linked to
// NPC 联动行为标志位（直接存储在数据库中；FLAG_TO_* 表示从属 NPC 也会反向触发主控 NPC 的联动）
enum CreatureLinkingFlags
{
    // Dynamic behaviour, in combat (战斗中的动态联动行为)
    FLAG_AGGRO_ON_AGGRO             = 0x0001,               // 主怪入战时从怪同步入战
    FLAG_TO_AGGRO_ON_AGGRO          = 0x0002,               // 从怪入战时主怪同步入战
    FLAG_RESPAWN_ON_EVADE           = 0x0004,               // 主怪脱战时从怪若死亡则重生
    FLAG_TO_RESPAWN_ON_EVADE        = 0x0008,               // 从怪脱战时主怪若死亡则重生
    FLAG_DESPAWN_ON_EVADE           = 0x1000,               // 主怪脱战时从怪消失
    FLAG_DESPAWN_ON_DEATH           = 0x0010,               // 主怪死亡时从怪消失
    FLAG_SELFKILL_ON_DEATH          = 0x0020,               // 主怪死亡时从怪自杀死亡
    FLAG_RESPAWN_ON_DEATH           = 0x0040,               // 主怪死亡时从怪重生
    FLAG_RESPAWN_ON_RESPAWN         = 0x0080,               // 主怪重生时从怪重生
    FLAG_DESPAWN_ON_RESPAWN         = 0x0100,               // 主怪重生时从怪消失
    FLAG_EVADE_ON_EVADE             = 0x4000,               // 主怪脱战时从怪同步脱战

    // Dynamic behaviour, out of combat (脱战状态下的动态联动行为)
    FLAG_FOLLOW                     = 0x0200,               // 从怪跟随主怪移动（保持出生相对位置）
    FLAG_DESPAWN_ON_DESPAWN         = 0x2000,               // 主怪消失时从怪同步消失

    // Passive behaviour (被动刷新条件控制)
    FLAG_CANT_SPAWN_IF_BOSS_DEAD    = 0x0400,               // 若首领/主怪已死亡则禁止刷新
    FLAG_CANT_SPAWN_IF_BOSS_ALIVE   = 0x0800,               // 若首领/主怪仍存活则禁止刷新

    LINKING_FLAG_INVALID            = 0x8000,               // TODO adjust when other flags are implemented (无效标志上界，用于校验数据库配置)
};

// Structure holding the information for an entry (单条生物联动配置信息)
struct CreatureLinkingInfo
{
    uint32 mapId;                                           // 生效地图 ID（按 GUID 联动时为 INVALID_MAP_ID）
    uint32 masterId;                                        // 主控 NPC 的模板 Entry 或数据库 GUID
    uint32 masterDBGuid;                                    // 唯一主控 NPC 的数据库 GUID（跨全图条件刷新时使用）
    uint16 linkingFlag: 16;                                 // 联动标志位组合
    uint16 searchRange: 16;                                 // 主从匹配搜索半径（0 表示唯一全图匹配）
};

/**
 * A class to represent the static information of linking NPCs together
 * 生物静态联动配置全局管理器（负责从数据库加载并查询主从配置关系）
 */

class CreatureLinkingMgr
{
    public:                                                 // Constructors
        CreatureLinkingMgr() {}

    public:                                                 // Initialisation
        // 从数据库加载 creature_linking_template 与 creature_linking 表
        void LoadFromDB();

    public:                                                 // Accessors
        // This functions checks if the NPC triggers actions for other NPCs (判断 NPC 是否可触发联动事件)
        bool IsLinkedEventTrigger(Creature* pCreature) const;

        // This function checks if the NPC is a master NPC. (判断 NPC 是否为按 Entry 索引的主控怪)
        bool IsLinkedMaster(Creature* pCreature) const;

        // This function checks if the spawning of this NPC is dependend on other NPCs (判断 NPC 刷新是否受主控怪存活状态约束)
        bool IsSpawnedByLinkedMob(Creature* pCreature) const;
        bool IsSpawnedByLinkedMob(CreatureLinkingInfo const* pInfo) const;

        // This gives the information of a linked NPC (describes action when its ActionTrigger triggers)
        // Depends of the map (获取指定生物或模板/GUID/地图对应的联动配置信息)
        CreatureLinkingInfo const* GetLinkedTriggerInformation(Creature* pCreature) const;
        CreatureLinkingInfo const* GetLinkedTriggerInformation(uint32 entry, uint32 lowGuid, uint32 mapId) const;

    private:
        typedef std::multimap < uint32 /*slaveEntry*/, CreatureLinkingInfo > CreatureLinkingMap;
        typedef std::pair<CreatureLinkingMap::const_iterator, CreatureLinkingMap::const_iterator> CreatureLinkingMapBounds;

        // Storage of Data: npc_entry_slave, (map, npc_entry_master, flag, master_db_guid[If Unique], search_range)
        // 按从怪 Entry 存储的联动规则映射
        CreatureLinkingMap m_creatureLinkingMap;
        // Storage of Data: npc_guid_slave, (map, npc_guid_master, flag, master_db_guid, search_range)
        // 按从怪数据库 GUID 存储的联动规则映射
        CreatureLinkingMap m_creatureLinkingGuidMap;

        // Lookup Storage for fast access: (快速索引集合：记录哪些 Entry / DBGuid 作为主控方存在联动)
        std::unordered_set<uint32> m_eventTriggers;              // master by entry
        std::unordered_set<uint32> m_eventGuidTriggers;          // master by guid

        // Check-routine (校验数据库联动条目合法性)
        static bool IsLinkingEntryValid(uint32 slaveEntry, CreatureLinkingInfo* pTmp, bool byEntry);
};

/**
 * A class to represent the dynamic information of linking NPCs together
 *
 * Every map has an instance of this class as member, in which the dynamic information (GUIDs) are stored
 * 生物动态联动运行时持有器（每张地图实例独立持有一份，维护已刷新主从实体的实时 GUID 绑定）
 */

class CreatureLinkingHolder
{
    public:                                                 // Constructors
        CreatureLinkingHolder() {}

    public:                                                 // Accessors
        // Function to add slave-NPCs to the holder (将从怪实例注册到地图联动持有器)
        void AddSlaveToHolder(Creature* pCreature);

        // Function to add master-NPCs to the holder (将主怪实例注册到地图联动持有器)
        void AddMasterToHolder(Creature* pCreature);

        // Function to process actions for linked NPCs (处理并分发指定联动事件)
        void DoCreatureLinkingEvent(CreatureLinkingEvent eventType, Creature* pSource, Unit* pEnemy = nullptr);

        // Function to check if a passive spawning condition is met (检查被动条件刷新的生物当前是否允许刷新)
        bool CanSpawn(Creature* pCreature) const;
        // Helper function for recursive spawning-checks of an additional linked - also used in pooling (按数据库 GUID 与坐标校验刷新条件，亦用于 Pool 系统)
        bool CanSpawn(uint32 lowGuid, Map* _map, CreatureLinkingInfo const*  pInfo, float sx, float sy) const;

        // This function lets a slave refollow his master (尝试让从怪重新开始跟随其主怪)
        bool TryFollowMaster(Creature* pCreature);

    private:
        // Structure associated to a master (entry case) (按 Entry 归组的主从绑定条目与从怪 GUID 列表)
        struct InfoAndGuids
        {
            uint16 linkingFlag: 16;
            uint16 searchRange: 16;
            std::list<std::pair<uint32, ObjectGuid>> linkedGuids;
            bool inUse = false;                             // 防止联动事件递归重入的标记
        };
        // Structure associated to a master (guid case) (单对一 GUID 绑定结构)
        struct InfoAndGuid
        {
            uint16 linkingFlag;
            ObjectGuid linkedGuid;
        };

        typedef std::multimap < uint32 /*masterEntryOrGuid*/, InfoAndGuids > HolderMap;
        typedef std::pair<HolderMap::iterator, HolderMap::iterator> HolderMapBounds;
        typedef std::multimap < uint32 /*Entry*/, ObjectGuid > BossGuidMap;
        typedef std::pair<BossGuidMap::const_iterator, BossGuidMap::const_iterator> BossGuidMapBounds;

        // Helper function, to process a slave list (批量处理从怪列表中的联动响应)
        void ProcessSlaveGuidList(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, uint16 searchRange, std::list<std::pair<uint32, ObjectGuid>>& slaveGuidList, Unit* pEnemy);
        // Helper function, to process a single slave (处理单个从怪对联动事件的具体响应)
        void ProcessSlave(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, Creature* pSlave, Unit* pEnemy);
        // Helper function to set following (计算相对偏移角度与距离并下发跟随移动)
        void SetFollowing(Creature* pWho, Creature* pWhom) const;
        // Helper function to return if a slave is in range of a boss (判断从怪出生点是否在主怪搜索半径内)
        bool IsSlaveInRangeOfMaster(Creature const* pSlave, Creature const* pBoss, uint16 searchRange) const;
        bool IsSlaveInRangeOfMaster(Creature const* pBoss, float sX, float sY, uint16 searchRange) const;
        // Another helper function (检查指定数据库 GUID 生物是否已满足重生时间及联动/池条件)
        bool IsRespawnReady(uint32 dbLowGuid, Map* _map) const;

        // Storage of Data (boss, flag, searchRange, GuidList) for action triggering (按主怪 Entry 索引的从怪列表)
        HolderMap m_holderMap;
        // Storage of Data (boss, flag, slave-guid) (按主怪 DBGuid 索引的从怪列表)
        HolderMap m_holderGuidMap;
        // boss_entry, guid for reverse action triggering and check alive (按主怪 Entry 索引的运行时 ObjectGuid 列表)
        BossGuidMap m_masterGuid;
};

#define sCreatureLinkingMgr MaNGOS::Singleton<CreatureLinkingMgr>::Instance()

#endif
/*! @} */
