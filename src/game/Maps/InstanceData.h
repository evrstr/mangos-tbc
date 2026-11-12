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

#ifndef MANGOS_INSTANCE_DATA_H
#define MANGOS_INSTANCE_DATA_H

#include "Common.h"
#include "Entities/ObjectGuid.h"
#include "Util/ByteBuffer.h"

class Map;
class Unit;
class Player;
class GameObject;
class Creature;
class WorldObject;
class ChatHandler;
class CreatureGroup;

// 用于 CONDITION_INSTANCE_SCRIPT 条件的通用副本条件 ID 枚举
enum InstanceConditionIDs                                   // Suggested values used with CONDITION_INSTANCE_SCRIPT for some generic uses
{
    // for hard-mode loot (0 normal; 1,2... hard,harder... mode)
    // 用于困难模式掉落判定（0 为普通模式，1、2... 依次为更高难度）
    INSTANCE_CONDITION_ID_NORMAL_MODE       = 0,
    INSTANCE_CONDITION_ID_HARD_MODE         = 1,
    INSTANCE_CONDITION_ID_HARD_MODE_2       = 2,
    INSTANCE_CONDITION_ID_HARD_MODE_3       = 3,
    INSTANCE_CONDITION_ID_HARD_MODE_4       = 4,
    INSTANCE_CONDITION_ID_HARD_MODE_5       = 5,
    INSTANCE_CONDITION_ID_HARD_MODE_6       = 6,

    // to check for which team the instance is doing scripts
    // 检查副本脚本当前服务的阵营（部落/联盟）
    INSTANCE_CONDITION_ID_TEAM_HORDE        = 67,
    INSTANCE_CONDITION_ID_TEAM_ALLIANCE     = 469,

    // to check water event in SSC (检查毒蛇神殿水温事件)
    INSTANCE_CONDITION_ID_LURKER            = 21217,
    INSTANCE_CONDITION_ID_SCALDING_WATER    = 37284,

    INSTANCE_CONDITION_ID_SOCRETHAR_GOSSIP  = 8215,

    // Gossip condition for OHF (旧希尔斯布莱德丘陵对话条件)
    INSTANCE_CONDITION_ID_THRALL_STARTABLE = 17876,

    // Gossip conditions for Hyjal Summit (海加尔山之战对话条件)
    INSTANCE_CONDITION_ID_WINTERCHILL_STARTABLE = 17767,
    INSTANCE_CONDITION_ID_ANETHERON_STARTABLE = 17808,
    INSTANCE_CONDITION_ID_ANETHERON_DONE = 17772,
    INSTANCE_CONDITION_ID_KAZROGAL_STARTABLE = 17888,
    INSTANCE_CONDITION_ID_AZGALOR_STARTABLE = 17842,
    INSTANCE_CONDITION_ID_AZGALOR_DONE = 17852,
    INSTANCE_CONDITION_ID_ARCHIMONDE_STARTABLE = 17968,
    INSTANCE_CONDITION_ID_ARCHIMONDE_DONE = 17948,
    INSTANCE_CONDITION_ID_WAVES_INPROGRESS = 17895,

    INSTANCE_CONDITION_ID_SAAT_BEACON               = 20201,

    INSTANCE_CONDITION_ID_BASHIR_FLYING             = 87361,
    INSTANCE_CONDITION_ID_BASHIR_IN_PROGRESS        = 87362,
    
    // Shade of the Horseman village attack event (无头骑士村庄袭击节日事件条件)
    INSTANCE_CONDITION_ID_FIRE_BRIGADE_PRACTICE_GOLDSHIRE   = 18000, // must be first and all subsequent numbers matching ShadeOfTheHorsemanVillages enum (必须排在首位且后续编号需与 ShadeOfTheHorsemanVillages 枚举对应)
    INSTANCE_CONDITION_ID_FIRE_BRIGADE_PRACTICE_KHARANOS    = 18001,
    INSTANCE_CONDITION_ID_FIRE_BRIGADE_PRACTICE_AZURE_WATCH = 18002,
    INSTANCE_CONDITION_ID_FIRE_TRAINING_BRILL               = 18003,
    INSTANCE_CONDITION_ID_FIRE_TRAINING_RAZOR_HILL          = 18004,
    INSTANCE_CONDITION_ID_FIRE_TRAINING_FALCONWING          = 18005,
    INSTANCE_CONDITION_ID_STOP_THE_FIRES_ALLIANCE           = 18006,
    INSTANCE_CONDITION_ID_STOP_THE_FIRES_HORDE              = 18007,
    INSTANCE_CONDITION_ID_THE_HEADLESS_HORSEMAN_ALLIANCE    = 18008,
    INSTANCE_CONDITION_ID_THE_HEADLESS_HORSEMAN_HORDE       = 18009,
    INSTANCE_CONDITION_ID_LET_THE_FIRES_COME_ALLIANCE       = 18010,
    INSTANCE_CONDITION_ID_LET_THE_FIRES_COME_HORDE          = 18011,
};

// 地图/副本脚本数据基类：负责保存副本进度、响应玩家/生物/游戏对象事件及条件校验
class InstanceData
{
    public:

        explicit InstanceData(Map* map) : instance(map) {}
        virtual ~InstanceData() {}

        Map* instance;

        // On creation, NOT load. (首次创建时调用，而非从数据库加载时)
        virtual void Initialize() {}

        // On load (从持久化存档字符串加载状态时调用)
        virtual void Load(const char* /*data*/) {}

        // When save is needed, this function generates the data (需要保存进度时生成序列化数据)
        virtual const char* Save() const { return ""; }

        void SaveToDB() const; // 将当前副本数据持久化写入数据库

        // Called every map update (每次地图更新时调用)
        virtual void Update(const uint32 /*diff*/) {}

        // This is to prevent players from entering during boss encounters. (用于阻止玩家在首领战斗进行中进入副本)
        virtual bool IsEncounterInProgress() const { return false; };

        // Called when a player successfully enters the instance (after really added to map) (玩家成功进入并添加到地图后调用)
        virtual void OnPlayerEnter(Player*) {}

        // Called when a player dies inside instance (玩家在副本内死亡时调用)
        virtual void OnPlayerDeath(Player*) {}

        // Called when a player rezzurects inside instance (玩家在副本内复活时调用)
        virtual void OnPlayerResurrect(Player*) {}

        // Called when a player leaves the instance (before really removed from map (or possibly world)) (玩家离开副本并从地图移除前调用)
        virtual void OnPlayerLeave(Player*) {}

        // Called when a gameobject is created (游戏对象创建时调用)
        virtual void OnObjectCreate(GameObject*) {}

        // Called when a gameobject is spawned into map (游戏对象刷入地图时调用)
        virtual void OnObjectSpawn(GameObject*) {}

        // Called when a gameobject is despawned from map (游戏对象从地图消失时调用)
        virtual void OnObjectDespawn(GameObject*) {}

        // called on creature creation (生物创建时调用)
        virtual void OnCreatureCreate(Creature* /*creature*/) {}

        // called on creature spawn/respawn (生物刷出或重生时调用)
        virtual void OnCreatureRespawn(Creature* /*creature*/) {}

        // called on creature enter combat (生物进入战斗时调用)
        virtual void OnCreatureEnterCombat(Creature* /*creature*/) {}

        // called on creature evade (生物脱离战斗重置时调用)
        virtual void OnCreatureEvade(Creature* /*creature*/) {}

        // called on creature death (生物死亡时调用)
        virtual void OnCreatureDeath(Creature* /*creature*/) {}

        // called on creature despawn (生物消失时调用)
        virtual void OnCreatureDespawn(Creature* /*creature*/) {}

        // called on creature group last member despawn or death - whichever occurs first and supplies last one to die
        // 生物组最后一个成员消失或死亡时调用（以先发生者为准），并传入最后死亡/消失的生物
        virtual void OnCreatureGroupDespawn(CreatureGroup* /*creatureGroup*/, Creature* /*creature*/) {}

        // called on game event (游戏事件触发/结束时调用)
        virtual void OnEventHappened(uint16 /*event_id*/, bool /*activate*/, bool /*resume*/) {}

        // All-purpose data storage 64 bit (通用 64 位数据存取接口)
        virtual uint64 GetData64(uint32 /*Data*/) const { return 0; }
        virtual void SetData64(uint32 /*Data*/, uint64 /*Value*/) { }

        // Guid data storage (wrapper for set/get from uint64 storage (GUID 数据存取封装，底层复用 64 位接口)
        ObjectGuid GetGuid(uint32 dataIdx) const { return ObjectGuid(GetData64(dataIdx)); }
        void SetGuid(uint32 dataIdx, ObjectGuid value) { SetData64(dataIdx, value.GetRawValue()); }

        // All-purpose data storage 32 bit (通用 32 位数据存取接口)
        virtual uint32 GetData(uint32 /*Type*/) const { return 0; }
        virtual void SetData(uint32 /*Type*/, uint32 /*Data*/) {}

        // Condition criteria additional requirements check
        // This is used for such things are heroic loot
        // See ObjectMgr.h enum ConditionSource for possible values of conditionSourceType
        // 附加条件校验（如英雄模式掉落等），conditionSourceType 参见 ObjectMgr.h 的 ConditionSource 枚举
        virtual bool CheckConditionCriteriaMeet(Player const* source, uint32 instance_condition_id, WorldObject const* conditionSource, uint32 conditionSourceType) const;

        virtual void FillInitialWorldStates(ByteBuffer& /*data*/, uint32& /*count*/, uint32 /*zoneId*/, uint32 /*areaId*/) {}

        // helper functions for world state list fill (填充初始世界状态列表的辅助方法)
        inline void FillInitialWorldStateData(ByteBuffer& data, uint32& count, uint32 state, uint32 value)
        {
            data << uint32(state);
            data << uint32(value);
            ++count;
        }

        inline void FillInitialWorldStateData(ByteBuffer& data, uint32& count, uint32 state, int32 value)
        {
            data << uint32(state);
            data << int32(value);
            ++count;
        }

        virtual void ShowChatCommands(ChatHandler* /*handler*/) {}
        virtual void ExecuteChatCommand(ChatHandler* /*handler*/, char* /*args*/) {}
};

#endif
