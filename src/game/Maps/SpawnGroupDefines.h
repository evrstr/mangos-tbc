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

#ifndef MANGOS_SPAWN_GROUP_DEFINE_H
#define MANGOS_SPAWN_GROUP_DEFINE_H

#include "Platform/Define.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

struct FormationEntry;
class FormationData;
class FormationSlotData;

typedef std::shared_ptr<FormationSlotData> FormationSlotDataSPtr;
typedef std::shared_ptr<FormationData> FormationDataSPtr;
typedef std::shared_ptr<FormationEntry> FormationEntrySPtr;
typedef std::map<uint32, FormationSlotDataSPtr> FormationSlotMap;

// 刷怪组随机条目配置，定义某生物/物体 Entry 的最小/最大同时存活数量与权重概率
struct SpawnGroupRandomEntry
{
    uint32 Id;
    uint32 Entry;
    uint32 MinCount; // Minimum of given entry before randomization is used (进入随机池前必须满足的该 Entry 最小刷新数量)
    uint32 MaxCount; // Maximum of given entry alive spawned in world, 0 is infinite (世界内同时存活的最大数量，0 表示无限制)
    uint32 Chance;
};

// 刷怪组绑定的数据库 GUID 记录及其编队槽位与随机属性
struct SpawnGroupDbGuids
{
    uint32 Id;
    uint32 DbGuid;
    int32 SlotId;
    uint32 Chance;
    uint32 OwnEntry;
    bool RandomEntry;
};

// 刷怪组小队配置（同一刷怪组内可按 SquadId 互斥选择整队成员）
struct SpawnGroupSquad
{
    uint32 SquadId;
    std::map<uint32, uint32> GuidToEntry;
};

// 刷怪组对象类型（生物或游戏对象）
enum SpawnGroupType
{
    SPAWN_GROUP_CREATURE = 0,
    SPAWN_GROUP_GAMEOBJECT = 1,
};

enum SpawnGroupFlags // flags that are common between both (生物与游戏对象刷怪组通用标志位)
{
    SPAWN_GROUP_DESPAWN_ON_COND_FAIL = 0x08,
};

// 生物编队/链接行为标志位
enum CreatureGroupFlags
{
    CREATURE_GROUP_AGGRO_TOGETHER   = 0x01,
    CREATURE_GROUP_RESPAWN_TOGETHER = 0x02,
    CREATURE_GROUP_EVADE_TOGETHER   = 0x04,
    // SPAWN_GROUP_DESPAWN_ON_COND_FAIL
    CREATURE_GROUP_FORMATION_MIRRORING = 0x10, // mirrors position during linear path movement (线性路径移动返程时镜像翻转编队站位)
};

// 生物组内部联动事件类型
enum CreatureGroupEvent : uint32
{
    CREATURE_GROUP_EVENT_AGGRO,
    CREATURE_GROUP_EVENT_EVADE,
    CREATURE_GROUP_EVENT_HOME,
    CREATURE_GROUP_EVENT_RESPAWN,
    CREATURE_GROUP_EVENT_MEMBER_DIED,
};

// 刷怪组静态定义条目（加载自数据库 spawn_group 表）
struct SpawnGroupEntry
{
    uint32 Id;
    std::string Name;
    SpawnGroupType Type;
    uint32 MaxCount; // Maximum active alive entities spawned in world (世界内同时存活的最大活跃实体数)
    int32 WorldStateCondition; // Worldstate value when set to 1 enables spawning of given group and 0 disables spawning (世界状态条件：为 1 时启用刷怪，为 0 时禁用)
    int32 WorldStateExpression; // Exclusive with condition (世界状态表达式，与 WorldStateCondition 互斥)
    uint32 Flags;
    uint32 StringId;
    std::optional<uint32> RespawnOverrideMin;
    std::optional<uint32> RespawnOverrideMax;
    bool Active;
    bool Large;
    bool EnabledByDefault;
    bool HasChancedSpawns;
    std::vector<SpawnGroupDbGuids> DbGuids;
    std::vector<SpawnGroupRandomEntry> RandomEntries;
    std::vector<SpawnGroupRandomEntry*> EquallyChanced;
    std::vector<SpawnGroupRandomEntry*> ExplicitlyChanced;
    std::vector<uint32> LinkedGroups;

    std::vector<SpawnGroupSquad> Squads;

    // may be nullptr (可为空指针，表示无编队配置)
    std::unique_ptr<FormationEntry> Formation;

    int32 GetFormationSlotId(uint32 dbGuid) const
    {
        auto const& itr = std::find_if(DbGuids.begin(), DbGuids.end(), [dbGuid](SpawnGroupDbGuids const& x) { return x.DbGuid == dbGuid; });
        return itr != DbGuids.end() ? (*itr).SlotId : -1;
    }

    bool IsMirroring() const { return Flags & CREATURE_GROUP_FORMATION_MIRRORING; }
};

// Formation defines (编队形状与槽位定义)
enum SpawnGroupFormationType : uint32
{
    SPAWN_GROUP_FORMATION_TYPE_RANDOM              = 0,
    SPAWN_GROUP_FORMATION_TYPE_SINGLE_FILE         = 1,
    SPAWN_GROUP_FORMATION_TYPE_SIDE_BY_SIDE        = 2,
    SPAWN_GROUP_FORMATION_TYPE_LIKE_GEESE          = 3,
    SPAWN_GROUP_FORMATION_TYPE_FANNED_OUT_BEHIND   = 4,
    SPAWN_GROUP_FORMATION_TYPE_FANNED_OUT_IN_FRONT = 5,
    SPAWN_GROUP_FORMATION_TYPE_CIRCLE_THE_LEADER   = 6,

    SPAWN_GROUP_FORMATION_TYPE_COUNT               = 7
};

// 编队槽位来源类型（静态配置、脚本添加或玩家加入）
enum SpawnGroupFormationSlotType : uint32
{
    SPAWN_GROUP_FORMATION_SLOT_TYPE_STATIC = 0,
    SPAWN_GROUP_FORMATION_SLOT_TYPE_SCRIPT = 1,
    SPAWN_GROUP_FORMATION_SLOT_TYPE_PLAYER = 2,
};

// 编队行为选项标志
enum SpawGroupFormationOptions : uint32
{
    SPAWN_GROUP_FORMATION_OPTION_NONE                                       = 0x00,
    SPAWN_GROUP_FORMATION_OPTION_FOLLOWERS_WILL_NOT_PATHFIND_TO_LOCATION    = 0x01, // NYI - need examples where used vs normal (尚未实现：跟随者不使用寻路直接前往槽位)
    SPAWN_GROUP_FORMATION_OPTION_KEEP_CONPACT                               = 0x02,
};

// 编队配置条目，描述队形、移动类型、间距与选项
struct FormationEntry
{
    uint32 GroupId                  = 0;
    SpawnGroupFormationType Type    = SPAWN_GROUP_FORMATION_TYPE_RANDOM;
    uint32 MovementIdOrWander       = 0;      
    uint32 MovementType             = 0;
    float Spread                    = 3.0f;
    uint32 Options                  = 0;
    std::string Comment             = "No comment";
    bool IsDynamic                  = false;
};

// 刷怪组条目全局容器，支持按组 ID 或 (DbGuid, Type) 查找
struct SpawnGroupEntryContainer
{
    std::map<uint32, SpawnGroupEntry> spawnGroupMap;
    std::map<std::pair<uint32, uint32>, SpawnGroupEntry*> spawnGroupByGuidMap;
};

#endif