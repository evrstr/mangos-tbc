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

#ifndef WORLD_PVP_NA
#define WORLD_PVP_NA

#include "Common.h"
#include "OutdoorPvP.h"
#include "World/WorldStateDefines.h"

// 纳格兰哈兰（Halaa）野外 PvP 常量定义
enum
{
    MAX_NA_GUARDS                           = 15,
    MAX_NA_ROOSTS                           = 4,            // roosts for each type and team (双足飞龙/轰炸台位置数量，共 4 个方向)
    MAX_FIRE_BOMBS                          = 10,

    // spells (哈兰之力增益与哈兰勋章牌子法术)
    SPELL_STRENGTH_HALAANI                  = 33795,
    SPELL_NAGRAND_TOKEN_ALLIANCE            = 33005,
    SPELL_NAGRAND_TOKEN_HORDE               = 33004,

    // npcs
    // quest credit (任务击杀计数生物)
    NPC_HALAA_COMBATANT                     = 24867,

    GRAVEYARD_ID_HALAA                      = 993,
    GRAVEYARD_ZONE_ID_HALAA                 = 3518,

    ITEM_ID_FIRE_BOMB                       = 24538,

    // gameobjects (哈兰占领点战旗)
    GO_HALAA_BANNER                         = 182210,

    // spawned when horde is in control - alliance is attacking (部落控制哈兰时刷新，供联盟空袭使用)
    GO_WYVERN_ROOST_ALLIANCE_SOUTH          = 182267,
    GO_WYVERN_ROOST_ALLIANCE_WEST           = 182280,
    GO_WYVERN_ROOST_ALLIANCE_NORTH          = 182281,
    GO_WYVERN_ROOST_ALLIANCE_EAST           = 182282,

    GO_BOMB_WAGON_HORDE_SOUTH               = 182222,
    GO_BOMB_WAGON_HORDE_WEST                = 182272,
    GO_BOMB_WAGON_HORDE_NORTH               = 182273,
    GO_BOMB_WAGON_HORDE_EAST                = 182274,

    GO_DESTROYED_ROOST_ALLIANCE_SOUTH       = 182266,
    GO_DESTROYED_ROOST_ALLIANCE_WEST        = 182275,
    GO_DESTROYED_ROOST_ALLIANCE_NORTH       = 182276,
    GO_DESTROYED_ROOST_ALLIANCE_EAST        = 182277,

    // spawned when alliance is in control - horde is attacking (联盟控制哈兰时刷新，供部落空袭使用)
    GO_WYVERN_ROOST_HORDE_SOUTH             = 182301,
    GO_WYVERN_ROOST_HORDE_WEST              = 182302,
    GO_WYVERN_ROOST_HORDE_NORTH             = 182303,
    GO_WYVERN_ROOST_HORDE_EAST              = 182304,

    GO_BOMB_WAGON_ALLIANCE_SOUTH            = 182305,
    GO_BOMB_WAGON_ALLIANCE_WEST             = 182306,
    GO_BOMB_WAGON_ALLIANCE_NORTH            = 182307,
    GO_BOMB_WAGON_ALLIANCE_EAST             = 182308,

    GO_DESTROYED_ROOST_HORDE_SOUTH          = 182297,
    GO_DESTROYED_ROOST_HORDE_WEST           = 182298,
    GO_DESTROYED_ROOST_HORDE_NORTH          = 182299,
    GO_DESTROYED_ROOST_HORDE_EAST           = 182300,

    // npcs
    // alliance (哈兰联盟方商人、军需官与守卫)
    NPC_RESEARCHER_KARTOS                   = 18817,
    NPC_QUARTERMASTER_DAVIAN                = 18822,
    NPC_MERCHANT_ALDRAAN                    = 21485,
    NPC_VENDOR_CENDRII                      = 21487,
    NPC_AMMUNITIONER_BANRO                  = 21488,
    NPC_ALLIANCE_HANAANI_GUARD              = 18256,

    // horde (哈兰部落方商人、军需官与守卫)
    NPC_RESEARCHER_AMERELDINE               = 18816,
    NPC_QUARTERMASTER_NORELIQE              = 18821,
    NPC_MERCHANT_COREIEL                    = 21474,
    NPC_VENDOR_EMBELAR                      = 21484,
    NPC_AMMUNITIONER_TASALDAN               = 21483,
    NPC_HORDE_HALAANI_GUARD                 = 18192,

    // events (哈兰占领与争夺事件 ID)
    EVENT_HALAA_BANNER_WIN_ALLIANCE         = 11504,
    EVENT_HALAA_BANNER_WIN_HORDE            = 11503,
    EVENT_HALAA_BANNER_CONTEST_ALLIANCE     = 11559,
    EVENT_HALAA_BANNER_CONTEST_HORDE        = 11558,
    EVENT_HALAA_BANNER_PROGRESS_ALLIANCE    = 11821,
    EVENT_HALAA_BANNER_PROGRESS_HORDE       = 11822,

    ZONE_HALAA                              = 3628,
};

// 哈兰阵亡守卫重生坐标结构
struct HalaaSoldiersSpawns
{
    float x, y, z, o;
};

static const uint32 nagrandRoostsAlliance[MAX_NA_ROOSTS]                = {GO_WYVERN_ROOST_ALLIANCE_SOUTH,          GO_WYVERN_ROOST_ALLIANCE_NORTH,         GO_WYVERN_ROOST_ALLIANCE_EAST,          GO_WYVERN_ROOST_ALLIANCE_WEST};
static const uint32 nagrandRoostsHorde[MAX_NA_ROOSTS]                   = {GO_WYVERN_ROOST_HORDE_SOUTH,             GO_WYVERN_ROOST_HORDE_NORTH,            GO_WYVERN_ROOST_HORDE_EAST,             GO_WYVERN_ROOST_HORDE_WEST};
static const uint32 nagrandRoostsBrokenAlliance[MAX_NA_ROOSTS]          = {GO_DESTROYED_ROOST_ALLIANCE_SOUTH,       GO_DESTROYED_ROOST_ALLIANCE_NORTH,      GO_DESTROYED_ROOST_ALLIANCE_EAST,       GO_DESTROYED_ROOST_ALLIANCE_WEST};
static const uint32 nagrandRoostsBrokenHorde[MAX_NA_ROOSTS]             = {GO_DESTROYED_ROOST_HORDE_SOUTH,          GO_DESTROYED_ROOST_HORDE_NORTH,         GO_DESTROYED_ROOST_HORDE_EAST,          GO_DESTROYED_ROOST_HORDE_WEST};
static const uint32 nagrandWagonsAlliance[MAX_NA_ROOSTS]                = {GO_BOMB_WAGON_ALLIANCE_SOUTH,            GO_BOMB_WAGON_ALLIANCE_NORTH,           GO_BOMB_WAGON_ALLIANCE_EAST,            GO_BOMB_WAGON_ALLIANCE_WEST};
static const uint32 nagrandWagonsHorde[MAX_NA_ROOSTS]                   = {GO_BOMB_WAGON_HORDE_SOUTH,               GO_BOMB_WAGON_HORDE_NORTH,              GO_BOMB_WAGON_HORDE_EAST,               GO_BOMB_WAGON_HORDE_WEST};

static const uint32 nagrandRoostStatesAlliance[MAX_NA_ROOSTS]           = {WORLD_STATE_NA_WYVERN_SOUTH_A,           WORLD_STATE_NA_WYVERN_NORTH_A,          WORLD_STATE_NA_WYVERN_EAST_A,           WORLD_STATE_NA_WYVERN_WEST_A};
static const uint32 nagrandRoostStatesHorde[MAX_NA_ROOSTS]              = {WORLD_STATE_NA_WYVERN_SOUTH_H,           WORLD_STATE_NA_WYVERN_NORTH_H,          WORLD_STATE_NA_WYVERN_EAST_H,           WORLD_STATE_NA_WYVERN_WEST_H};
static const uint32 nagrandRoostStatesAllianceNeutral[MAX_NA_ROOSTS]    = {WORLD_STATE_NA_WYVERN_SOUTH_NEUTRAL_A,   WORLD_STATE_NA_WYVERN_NORTH_NEUTRAL_A,  WORLD_STATE_NA_WYVERN_EAST_NEUTRAL_A,   WORLD_STATE_NA_WYVERN_WEST_NEUTRAL_A};
static const uint32 nagrandRoostStatesHordeNeutral[MAX_NA_ROOSTS]       = {WORLD_STATE_NA_WYVERN_SOUTH_NEUTRAL_H,   WORLD_STATE_NA_WYVERN_NORTH_NEUTRAL_H,  WORLD_STATE_NA_WYVERN_EAST_NEUTRAL_H,   WORLD_STATE_NA_WYVERN_WEST_NEUTRAL_H};

// 纳格兰哈兰野外 PvP 控制器：管理哈兰占领点解锁/锁定、守卫数量与复活、双足飞龙轰炸点及阵营商人切换
class OutdoorPvPNA : public OutdoorPvP
{
    public:
        OutdoorPvPNA();

        void HandlePlayerEnterZone(Player* player, bool isMainZone) override;
        void HandlePlayerLeaveZone(Player* player, bool isMainZone) override;
        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;
        void SendRemoveWorldStates(Player* player) override;

        bool HandleEvent(uint32 eventId, Object* source, Object* target) override;
        void HandleObjectiveComplete(uint32 eventId, const std::list<Player*>& players, Team team) override;

        void HandleCreatureCreate(Creature* creature) override;
        void HandleGameObjectCreate(GameObject* go) override;
        void HandleCreatureDeath(Creature* creature) override;

        void HandlePlayerKillInsideArea(Player* player) override;
        bool HandleGameObjectUse(Player* player, GameObject* go) override;
        void Update(uint32 diff) override;

    private:
        // world states handling (同步更新区域与双足飞龙点世界状态)
        void UpdateWorldState(uint32 value);
        void UpdateWyvernsWorldState(uint32 value);

        // process capture events (处理哈兰控制权占领或争夺事件)
        void ProcessCaptureEvent(GameObject* go, Team team);

        // set specific team vendors and objects after capture (占领变更后刷新/清理阵营商人与可交互对象)
        void DespawnVendors(const WorldObject* objRef);
        void HandleFactionObjects(const WorldObject* objRef);

        // handle a specific game objects (锁定或解锁哈兰战旗占领点)
        void LockHalaa(const WorldObject* objRef);
        void UnlockHalaa(const WorldObject* objRef);

        // handle soldier respawn on timer (定时复活阵亡的哈兰守卫)
        void RespawnSoldier();

        Team m_zoneOwner;
        uint32 m_soldiersRespawnTimer;
        uint32 m_zoneWorldState;
        uint32 m_zoneMapState;
        uint32 m_roostWorldState[MAX_NA_ROOSTS];
        uint8 m_guardsLeft;

        bool m_isUnderSiege;

        ObjectGuid m_capturePoint;
        ObjectGuid m_roostsAlliance[MAX_NA_ROOSTS];
        ObjectGuid m_roostsHorde[MAX_NA_ROOSTS];
        ObjectGuid m_roostsBrokenAlliance[MAX_NA_ROOSTS];
        ObjectGuid m_roostsBrokenHorde[MAX_NA_ROOSTS];
        ObjectGuid m_wagonsAlliance[MAX_NA_ROOSTS];
        ObjectGuid m_wagonsHorde[MAX_NA_ROOSTS];

        GuidList m_teamVendors;

        std::queue<HalaaSoldiersSpawns> m_deadSoldiers;
};

#endif
