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

#ifndef WORLD_PVP_ZM
#define WORLD_PVP_ZM

#include "Common.h"
#include "OutdoorPvP.h"
#include "Tools/Language.h"
#include "World/WorldStateDefines.h"

// 赞加沼泽（Zangarmarsh）双塔废墟野外 PvP 常量定义
enum
{
    MAX_ZM_TOWERS                           = 2,

    // npcs (战旗提供者斥候 NPC，当前由条件/对话驱动)
    //NPC_ALLIANCE_FIELD_SCOUT              = 18581,
    //NPC_HORDE_FIELD_SCOUT                 = 18564,

    // these 2 npcs act as an artkit (充当塔顶与墓地光柱特效视觉的虚拟 NPC)
    NPC_PVP_BEAM_RED                        = 18757,
    NPC_PVP_BEAM_BLUE                       = 18759,

    // gameobjects (东西双塔占领点及中央墓地阵营战旗)
    GO_ZANGA_BANNER_WEST                    = 182522,
    GO_ZANGA_BANNER_EAST                    = 182523,
    GO_ZANGA_BANNER_CENTER_ALLIANCE         = 182527,           // triggers spell 32433 (联盟占领中央墓地旗帜，触发法术 32433)
    GO_ZANGA_BANNER_CENTER_HORDE            = 182528,           // triggers spell 32433 (部落占领中央墓地旗帜，触发法术 32433)
    GO_ZANGA_BANNER_CENTER_NEUTRAL          = 182529,           // triggers spell 32438 (中立中央墓地旗帜，触发法术 32438)

    // spells (双塔祝福、战旗携带法术、牌子法术及光柱特效法术)
    SPELL_TWIN_SPIRE_BLESSING               = 33779,
    SPELL_BATTLE_STANDARD_ALLIANCE          = 32430,
    SPELL_BATTLE_STANDARD_HORDE             = 32431,

    SPELL_ZANGA_TOWER_TOKEN_ALLIANCE        = 32155,
    SPELL_ZANGA_TOWER_TOKEN_HORDE           = 32158,

    SPELL_BEAM_RED                          = 32839,
    SPELL_BEAM_BLUE                         = 32840,

    // misc (双塔墓地链接 ID)
    GRAVEYARD_ID_TWIN_SPIRE                 = 969,
    GRAVEYARD_ZONE_TWIN_SPIRE               = 3521,

    // events (东西两座信标占领/中立化事件 ID)
    //EVENT_EAST_BEACON_CONTEST_ALLIANCE    = 11816,
    //EVENT_EAST_BEACON_CONTEST_HORDE       = 11817,
    EVENT_EAST_BEACON_PROGRESS_ALLIANCE     = 11807,
    EVENT_EAST_BEACON_PROGRESS_HORDE        = 11806,
    EVENT_EAST_BEACON_NEUTRAL_ALLIANCE      = 11814,
    EVENT_EAST_BEACON_NEUTRAL_HORDE         = 11815,

    //EVENT_WEST_BEACON_CONTEST_ALLIANCE    = 11813,
    //EVENT_WEST_BEACON_CONTEST_HORDE       = 11812,
    EVENT_WEST_BEACON_PROGRESS_ALLIANCE     = 11805,
    EVENT_WEST_BEACON_PROGRESS_HORDE        = 11804,
    EVENT_WEST_BEACON_NEUTRAL_ALLIANCE      = 11808,
    EVENT_WEST_BEACON_NEUTRAL_HORDE         = 11809,

    // condition entries (hardcoded)
    // used to check the gossip options in DB for the flag provider NPC
    // (硬编码脚本条件 ID，用于判断数据库里战场斥候 NPC 的领旗对话选项是否可用)
    OPVP_COND_ZM_ALLY_SCOUT_FLAG_READY      = 0,
    OPVP_COND_ZM_HORDE_SCOUT_FLAG_READY     = 1,
};

// 赞加沼泽信标占领事件描述结构
struct ZangarmarshTowerEvent
{
    uint32  eventEntry;
    Team    team;
    uint32  defenseMessage;
    uint32  worldState;
    uint32  mapState;
};

static const ZangarmarshTowerEvent zangarmarshTowerEvents[MAX_ZM_TOWERS][4] =
{
    {
        {EVENT_EAST_BEACON_PROGRESS_ALLIANCE,   ALLIANCE,   LANG_OPVP_ZM_CAPTURE_EAST_BEACON_A, WORLD_STATE_ZM_BEACON_EAST_UI_ALLIANCE, WORLD_STATE_ZM_BEACON_EAST_ALLIANCE},
        {EVENT_EAST_BEACON_PROGRESS_HORDE,      HORDE,      LANG_OPVP_ZM_CAPTURE_EAST_BEACON_H, WORLD_STATE_ZM_BEACON_EAST_UI_HORDE,    WORLD_STATE_ZM_BEACON_EAST_HORDE},
        {EVENT_EAST_BEACON_NEUTRAL_HORDE,       TEAM_NONE,  0,                                  WORLD_STATE_ZM_BEACON_EAST_UI_NEUTRAL,  WORLD_STATE_ZM_BEACON_EAST_NEUTRAL},
        {EVENT_EAST_BEACON_NEUTRAL_ALLIANCE,    TEAM_NONE,  0,                                  WORLD_STATE_ZM_BEACON_EAST_UI_NEUTRAL,  WORLD_STATE_ZM_BEACON_EAST_NEUTRAL},
    },
    {
        {EVENT_WEST_BEACON_PROGRESS_ALLIANCE,   ALLIANCE,   LANG_OPVP_ZM_CAPTURE_WEST_BEACON_A, WORLD_STATE_ZM_BEACON_WEST_UI_ALLIANCE, WORLD_STATE_ZM_BEACON_WEST_ALLIANCE},
        {EVENT_WEST_BEACON_PROGRESS_HORDE,      HORDE,      LANG_OPVP_ZM_CAPTURE_WEST_BEACON_H, WORLD_STATE_ZM_BEACON_WEST_UI_HORDE,    WORLD_STATE_ZM_BEACON_WEST_HORDE},
        {EVENT_WEST_BEACON_NEUTRAL_HORDE,       TEAM_NONE,  0,                                  WORLD_STATE_ZM_BEACON_WEST_UI_NEUTRAL,  WORLD_STATE_ZM_BEACON_WEST_NEUTRAL},
        {EVENT_WEST_BEACON_NEUTRAL_ALLIANCE,    TEAM_NONE,  0,                                  WORLD_STATE_ZM_BEACON_WEST_UI_NEUTRAL,  WORLD_STATE_ZM_BEACON_WEST_NEUTRAL},
    },
};

static const uint32 zangarmarshTowers[MAX_ZM_TOWERS] = {GO_ZANGA_BANNER_EAST, GO_ZANGA_BANNER_WEST};

// 赞加沼泽双塔废墟野外 PvP 控制器：管理东西信标占领、斥候领旗状态、中央墓地插旗与双塔祝福增益
class OutdoorPvPZM : public OutdoorPvP
{
    public:
        OutdoorPvPZM();

        void HandlePlayerEnterZone(Player* player, bool isMainZone) override;
        void HandlePlayerLeaveZone(Player* player, bool isMainZone) override;
        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;
        void SendRemoveWorldStates(Player* player) override;

        bool HandleEvent(uint32 eventId, Object* source, Object* target) override;

        void HandleCreatureCreate(Creature* creature) override;
        void HandleGameObjectCreate(GameObject* go) override;

        void HandlePlayerKillInsideArea(Player* player) override;
        bool HandleGameObjectUse(Player* player, GameObject* go) override;

        bool IsConditionFulfilled(Player const* source, uint32 conditionId, WorldObject const* conditionSource, uint32 conditionSourceType) override;
        void HandleConditionStateChange(uint32 conditionId, bool state) override;

    private:
        // process capture events (处理东西信标占领状态变更)
        bool ProcessCaptureEvent(GameObject* go, uint32 towerId, Team team, uint32 newWorldState, uint32 newMapState);

        // handles scout world states (更新斥候可领旗状态及地图提示世界状态)
        void UpdateScoutState(Team team, bool spawned);

        // respawn npcs which act as an artkit visual (切换作为光柱视觉特效的 NPC 附带光环)
        void SetBeaconArtKit(const WorldObject* objRef, ObjectGuid creatureGuid, uint32 auraId);

        uint32 m_towerWorldState[MAX_ZM_TOWERS];
        uint32 m_towerMapState[MAX_ZM_TOWERS];

        Team m_towerOwner[MAX_ZM_TOWERS];
        Team m_graveyardOwner;

        uint32 m_graveyardWorldState;
        uint32 m_scoutWorldStateAlliance;
        uint32 m_scoutWorldStateHorde;
        uint8 m_towersAlliance;
        uint8 m_towersHorde;

        bool m_flagReady[PVP_TEAM_COUNT];
        bool m_playerCarryingFlag[PVP_TEAM_COUNT];

        ObjectGuid m_towerBanners[MAX_ZM_TOWERS];
        ObjectGuid m_graveyardBannerAlliance;
        ObjectGuid m_graveyardBannerHorde;
        ObjectGuid m_graveyardBannerNeutral;

        ObjectGuid m_beamTowerBlue[MAX_ZM_TOWERS];
        ObjectGuid m_beamTowerRed[MAX_ZM_TOWERS];
        ObjectGuid m_beamGraveyardBlue;
        ObjectGuid m_beamGraveyardRed;
};

#endif
