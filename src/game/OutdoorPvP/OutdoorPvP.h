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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef OUTDOOR_PVP_H
#define OUTDOOR_PVP_H

#include "Common.h"
#include "Entities/EntitiesMgr.h"
#include "Entities/ObjectGuid.h"
#include "Globals/SharedDefines.h"
#include "OutdoorPvPMgr.h"

class WorldPacket;

// 占领点旗帜美术套件（ArtKit）ID
enum CapturePointArtKits
{
    CAPTURE_ARTKIT_ALLIANCE = 2,
    CAPTURE_ARTKIT_HORDE    = 1,
    CAPTURE_ARTKIT_NEUTRAL  = 21
};

// 占领点旗帜动画状态 ID
enum CapturePointAnimations
{
    CAPTURE_ANIM_ALLIANCE   = 1,
    CAPTURE_ANIM_HORDE      = 0,
    CAPTURE_ANIM_NEUTRAL    = 2
};

typedef std::map<ObjectGuid /*playerGuid*/, bool /*isMainZone*/> GuidZoneMap;

// 野外 PvP（OutdoorPvP）基类：各野外战区脚本继承此类以处理占领点事件、世界状态、区域增益与击杀奖励
class OutdoorPvP
{
        friend class OutdoorPvPMgr;

    public:
        OutdoorPvP() {}
        virtual ~OutdoorPvP() {}

        // called when the zone is initialized (区域初始化时填充初始世界状态)
        virtual void FillInitialWorldStates(WorldPacket& /*data*/, uint32& /*count*/) {}

        // Process Capture event (处理占领点进度/控制权变更事件)
        virtual bool HandleEvent(uint32 /*eventId*/, Object* /*source*/, Object* /*target*/) { return false; }

        // handle capture objective complete (处理占领目标完成时的玩家奖励)
        virtual void HandleObjectiveComplete(uint32 /*eventId*/, const PlayerList& /*players*/, Team /*team*/) {}

        // Called when a creature is created (生物创建时回调，用于记录哨塔守卫、NPC 等 GUID)
        virtual void HandleCreatureCreate(Creature* /*creature*/) {}

        // Called when a gameobject is created or removed (游戏对象创建或移除时回调，用于注册/注销占领点与旗帜)
        virtual void HandleGameObjectCreate(GameObject* /*go*/);
        virtual void HandleGameObjectRemove(GameObject* /*go*/);

        // Called on creature death (生物死亡时回调)
        virtual void HandleCreatureDeath(Creature* /*creature*/) {}

        // called when a player uses a gameobject related to outdoor pvp events (玩家交互野外 PvP 相关游戏对象时回调)
        virtual bool HandleGameObjectUse(Player* /*player*/, GameObject* /*go*/) { return false; }

        // called when a player triggers an areatrigger (玩家触发区域触发器时回调，如交旗点)
        virtual bool HandleAreaTrigger(Player* /*player*/, uint32 /*triggerId*/) { return false; }

        // called when a player drops a flag (玩家掉落/丢弃战旗时回调)
        virtual bool HandleDropFlag(Player* /*player*/, uint32 /*spellId*/) { return false; }

        // update - called by the OutdoorPvPMgr (定时更新回调，由 OutdoorPvPMgr 驱动)
        virtual void Update(uint32 /*diff*/) {}

        // Handle player kill (处理野外 PvP 区域内的玩家击杀事件)
        void HandlePlayerKill(Player* killer, Player* victim);

        // Handle script condition fulfillment (判断脚本自定义条件是否满足，如对话选项可见性)
        virtual bool IsConditionFulfilled(Player const* /*source*/, uint32 /*conditionId*/, WorldObject const* /*conditionSource*/, uint32 /*conditionSourceType*/) { return false; }

        // Handle script condition state change by an external factor (响应外部因素引起的条件状态变化)
        virtual void HandleConditionStateChange(uint32 /*conditionId*/, bool /*state*/) {}

        // 设置墓地关联的占领阵营
        void SetGraveYardLinkTeam(uint32 id, uint32 locKey, Team team, uint32 mapId);

    protected:

        // Player related stuff (玩家进入/离开野外 PvP 区域时的处理)
        virtual void HandlePlayerEnterZone(Player* /*player*/, bool /*isMainZone*/);
        virtual void HandlePlayerLeaveZone(Player* /*player*/, bool /*isMainZone*/);

        // remove world states (玩家离开区域时清除对应的世界状态 UI)
        virtual void SendRemoveWorldStates(Player* /*player*/) {}

        // handle npc/player kill (区域内有效击杀后的具体奖励/计数处理)
        virtual void HandlePlayerKillInsideArea(Player* /*killer*/) {}

        // send world state update to all players present (向区域内所有在场玩家广播世界状态更新)
        void SendUpdateWorldState(uint32 field, uint32 value);

        // applies buff to a team inside the specific zone (对区域内指定阵营的玩家施加或移除增益光环)
        void BuffTeam(Team team, uint32 spellId, bool remove = false);

        // get banner artkit based on controlling team (根据控制阵营获取对应的旗帜 ArtKit)
        uint32 GetBannerArtKit(Team team, uint32 artKitAlliance = CAPTURE_ARTKIT_ALLIANCE, uint32 artKitHorde = CAPTURE_ARTKIT_HORDE, uint32 artKitNeutral = CAPTURE_ARTKIT_NEUTRAL) const;

        // set banner visual (设置旗帜游戏对象的美术外观与动画状态)
        void SetBannerVisual(const WorldObject* objRef, ObjectGuid goGuid, uint32 artKit, uint32 animId);
        void SetBannerVisual(GameObject* go, uint32 artKit, uint32 animId);

        // Handle gameobject spawn / despawn (控制指定游戏对象的刷新或消失)
        void RespawnGO(const WorldObject* objRef, ObjectGuid goGuid, bool respawn);

        // store the players inside the area (记录当前位于该野外 PvP 区域及关联副本中的玩家)
        GuidZoneMap m_zonePlayers;
};

#endif
