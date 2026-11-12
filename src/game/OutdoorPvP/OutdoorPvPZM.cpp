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

#include "OutdoorPvPZM.h"
#include "Server/WorldPacket.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Entities/Object.h"
#include "Entities/Creature.h"
#include "Entities/GameObject.h"
#include "Entities/Player.h"

OutdoorPvPZM::OutdoorPvPZM() : OutdoorPvP(),
    m_graveyardOwner(TEAM_NONE),
    m_graveyardWorldState(WORLD_STATE_ZM_GRAVEYARD_NEUTRAL),
    m_scoutWorldStateAlliance(WORLD_STATE_ZM_FLAG_NOT_READY_ALLIANCE),
    m_scoutWorldStateHorde(WORLD_STATE_ZM_FLAG_NOT_READY_HORDE),
    m_towersAlliance(0),
    m_towersHorde(0)
{
    // init world states (初始化东西信标的 UI 和地图世界状态)
    m_towerWorldState[0] = WORLD_STATE_ZM_BEACON_EAST_UI_NEUTRAL;
    m_towerWorldState[1] = WORLD_STATE_ZM_BEACON_WEST_UI_NEUTRAL;
    m_towerMapState[0] = WORLD_STATE_ZM_BEACON_EAST_NEUTRAL;
    m_towerMapState[1] = WORLD_STATE_ZM_BEACON_WEST_NEUTRAL;

    for (auto& i : m_towerOwner)
        i = TEAM_NONE;

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        m_playerCarryingFlag[i] = false;
        m_flagReady[i] = false;
    }

    // initially set graveyard owner to neither faction (初始将双塔墓地归属设为无阵营控制)
    SetGraveYardLinkTeam(GRAVEYARD_ID_TWIN_SPIRE, GRAVEYARD_ZONE_TWIN_SPIRE, TEAM_INVALID, 530);
}

// 填充赞加沼泽双塔与墓地的初始世界状态
void OutdoorPvPZM::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    FillInitialWorldState(data, count, m_scoutWorldStateAlliance, WORLD_STATE_ADD);
    FillInitialWorldState(data, count, m_scoutWorldStateHorde, WORLD_STATE_ADD);
    FillInitialWorldState(data, count, m_graveyardWorldState, WORLD_STATE_ADD);

    for (uint8 i = 0; i < MAX_ZM_TOWERS; ++i)
    {
        FillInitialWorldState(data, count, m_towerWorldState[i], WORLD_STATE_ADD);
        FillInitialWorldState(data, count, m_towerMapState[i], WORLD_STATE_ADD);
    }
}

// 玩家离开赞加沼泽时移除相关世界状态 UI
void OutdoorPvPZM::SendRemoveWorldStates(Player* player)
{
    player->SendUpdateWorldState(m_scoutWorldStateAlliance, WORLD_STATE_REMOVE);
    player->SendUpdateWorldState(m_scoutWorldStateHorde, WORLD_STATE_REMOVE);
    player->SendUpdateWorldState(m_graveyardWorldState, WORLD_STATE_REMOVE);

    for (uint8 i = 0; i < MAX_ZM_TOWERS; ++i)
    {
        player->SendUpdateWorldState(m_towerWorldState[i], WORLD_STATE_REMOVE);
        player->SendUpdateWorldState(m_towerMapState[i], WORLD_STATE_REMOVE);
    }
}

// 玩家进入赞加沼泽或盘牙水库副本时刷新“双塔祝福”增益
void OutdoorPvPZM::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    // remove the buff from the player first; Sometimes on relog players still have the aura (先移除旧光环避免重连残留)
    player->RemoveAurasDueToSpell(SPELL_TWIN_SPIRE_BLESSING);

    // cast buff the the player which enters the zone (若己方控制中央墓地则施加双塔祝福)
    if (player->GetTeam() == m_graveyardOwner)
        player->CastSpell(player, SPELL_TWIN_SPIRE_BLESSING, TRIGGERED_OLD_TRIGGERED);
}

// 玩家离开区域时移除“双塔祝福”增益
void OutdoorPvPZM::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    // remove the buff from the player (移除双塔祝福光环)
    player->RemoveAurasDueToSpell(SPELL_TWIN_SPIRE_BLESSING);

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

// 生物创建时按 Y 坐标区分东塔、中央墓地与西塔的光柱视觉特效 NPC
void OutdoorPvPZM::HandleCreatureCreate(Creature* creature)
{
    switch (creature->GetEntry())
    {
        case NPC_PVP_BEAM_RED:
            if (creature->GetPositionY() < 7000.0f)         // East Beam (东塔红光柱)
                m_beamTowerRed[0] = creature->GetObjectGuid();
            else if (creature->GetPositionY() < 7300.0f)    // Center Beam (中央墓地红光柱)
                m_beamGraveyardRed = creature->GetObjectGuid();
            else                                            // West Beam (西塔红光柱)
                m_beamTowerRed[1] = creature->GetObjectGuid();
            break;
        case NPC_PVP_BEAM_BLUE:
            if (creature->GetPositionY() < 7000.0f)         // East Beam (东塔蓝光柱)
                m_beamTowerBlue[0] = creature->GetObjectGuid();
            else if (creature->GetPositionY() < 7300.0f)    // Center Beam (中央墓地蓝光柱)
                m_beamGraveyardBlue = creature->GetObjectGuid();
            else                                            // West Beam (西塔蓝光柱)
                m_beamTowerBlue[1] = creature->GetObjectGuid();
            break;
    }
}

// 游戏对象创建时记录东西信标与中央墓地三色战旗 GUID
void OutdoorPvPZM::HandleGameObjectCreate(GameObject* go)
{
    OutdoorPvP::HandleGameObjectCreate(go);

    switch (go->GetEntry())
    {
        case GO_ZANGA_BANNER_EAST:
            m_towerBanners[0] = go->GetObjectGuid();
            break;
        case GO_ZANGA_BANNER_WEST:
            m_towerBanners[1] = go->GetObjectGuid();
            break;
        case GO_ZANGA_BANNER_CENTER_ALLIANCE:
            m_graveyardBannerAlliance = go->GetObjectGuid();
            break;
        case GO_ZANGA_BANNER_CENTER_HORDE:
            m_graveyardBannerHorde = go->GetObjectGuid();
            break;
        case GO_ZANGA_BANNER_CENTER_NEUTRAL:
            m_graveyardBannerNeutral = go->GetObjectGuid();
            break;
    }
}

// Cast player spell on opponent kill (玩家在东西信标占领点半径内击杀敌对玩家时发放阵营印记)
void OutdoorPvPZM::HandlePlayerKillInsideArea(Player* player)
{
    for (auto m_towerBanner : m_towerBanners)
    {
        if (GameObject* capturePoint = player->GetMap()->GetGameObject(m_towerBanner))
        {
            // check capture point range (检查玩家是否处于信标占领半径内)
            GameObjectInfo const* info = capturePoint->GetGOInfo();
            if (info && player->IsWithinDistInMap(capturePoint, info->capturePoint.radius))
            {
                player->CastSpell(nullptr, player->GetTeam() == ALLIANCE ? SPELL_ZANGA_TOWER_TOKEN_ALLIANCE : SPELL_ZANGA_TOWER_TOKEN_HORDE, TRIGGERED_OLD_TRIGGERED);
                return;
            }
        }
    }
}

// process the capture events (分发东西信标占领/中立化事件)
bool OutdoorPvPZM::HandleEvent(uint32 eventId, Object* source, Object* /*target*/)
{
    if (!source->IsGameObject())
        return false;

    GameObject* go = static_cast<GameObject*>(source);
    for (uint8 i = 0; i < MAX_ZM_TOWERS; ++i)
    {
        if (zangarmarshTowers[i] == go->GetEntry())
        {
            for (uint8 j = 0; j < 4; ++j)
            {
                if (zangarmarshTowerEvents[i][j].eventEntry == eventId)
                {
                    // prevent processing if the owner did not change (happens if progress event is called after contest event)
                    // (若归属阵营未实际变化则忽略重复事件)
                    if (zangarmarshTowerEvents[i][j].team != m_towerOwner[i])
                    {
                        if (zangarmarshTowerEvents[i][j].defenseMessage)
                            sWorld.SendDefenseMessage(ZONE_ID_ZANGARMARSH, zangarmarshTowerEvents[i][j].defenseMessage);

                        return ProcessCaptureEvent(go, i, zangarmarshTowerEvents[i][j].team, zangarmarshTowerEvents[i][j].worldState, zangarmarshTowerEvents[i][j].mapState);
                    }
                    // no need to iterate other events or towers (已匹配信标与事件，提前返回)
                    return false;
                }
            }
            // no need to iterate other towers (已匹配信标但无匹配事件)
            return false;
        }
    }

    return false;
}

// 处理信标占领变更：更新塔顶光柱，当同一阵营同时占领双塔且尚未控制墓地时开放斥候领旗
bool OutdoorPvPZM::ProcessCaptureEvent(GameObject* go, uint32 towerId, Team team, uint32 newWorldState, uint32 newMapState)
{
    if (team == ALLIANCE)
    {
        // update counter (开启联盟蓝光柱并累加占塔数)
        SetBeaconArtKit(go, m_beamTowerBlue[towerId], SPELL_BEAM_BLUE);
        ++m_towersAlliance;

        if (m_towersAlliance == MAX_ZM_TOWERS)
        {
            // Send this defense message before updating scout state as this sends another (先广播双塔占领消息，再更新斥候提示)
            sWorld.SendDefenseMessage(ZONE_ID_ZANGARMARSH, LANG_OPVP_ZM_CAPTURE_BOTH_BEACONS_A);

            // only add flag to scouts if team does not have captured graveyard already (仅当未占领中央墓地时才启用联盟斥候战旗)
            if (m_graveyardOwner != ALLIANCE)
                UpdateScoutState(ALLIANCE, true);
        }
    }
    else if (team == HORDE)
    {
        // update counter (开启部落红光柱并累加占塔数)
        SetBeaconArtKit(go, m_beamTowerRed[towerId], SPELL_BEAM_RED);
        ++m_towersHorde;

        if (m_towersHorde == MAX_ZM_TOWERS)
        {
            // Send this defense message before updating scout state as this sends another (先广播双塔占领消息，再更新斥候提示)
            sWorld.SendDefenseMessage(ZONE_ID_ZANGARMARSH, LANG_OPVP_ZM_CAPTURE_BOTH_BEACONS_H);

            // only add flag to scouts if team does not already have captured graveyard (仅当未占领中央墓地时才启用部落斥候战旗)
            if (m_graveyardOwner != HORDE)
                UpdateScoutState(HORDE, true);
        }
    }
    else
    {
        if (m_towerOwner[towerId] == ALLIANCE)
        {
            SetBeaconArtKit(go, m_beamTowerBlue[towerId], 0);

            // only remove flag from scouts if team does not already have captured graveyard (失去双塔控制时关闭斥候战旗领取)
            if (m_towersAlliance == MAX_ZM_TOWERS && m_graveyardOwner != ALLIANCE)
                UpdateScoutState(ALLIANCE, false);

            // update counter (联盟占塔数 -1)
            --m_towersAlliance;
        }
        else
        {
            SetBeaconArtKit(go, m_beamTowerRed[towerId], 0);

            // only remove flag from scouts if team does not already have captured graveyard (失去双塔控制时关闭斥候战旗领取)
            if (m_towersHorde == MAX_ZM_TOWERS && m_graveyardOwner != HORDE)
                UpdateScoutState(HORDE, false);

            // update counter (部落占塔数 -1)
            --m_towersHorde;
        }
    }

    // update tower state (更新信标的 UI 与地图世界状态)
    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_REMOVE);
    m_towerWorldState[towerId] = newWorldState;
    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_ADD);

    SendUpdateWorldState(m_towerMapState[towerId], WORLD_STATE_REMOVE);
    m_towerMapState[towerId] = newMapState;
    SendUpdateWorldState(m_towerMapState[towerId], WORLD_STATE_ADD);

    // update capture point owner (记录信标当前归属阵营)
    m_towerOwner[towerId] = team;

    // the are no DB exceptions in this case (无需额外 DB 脚本)
    return true;
}

// Handle scout activation, when both beacons are captured (更新阵营战场斥候的战旗可领状态及 UI 世界状态)
void OutdoorPvPZM::UpdateScoutState(Team team, bool spawned)
{
    if (team == ALLIANCE)
    {
        SendUpdateWorldState(m_scoutWorldStateAlliance, WORLD_STATE_REMOVE);
        m_scoutWorldStateAlliance = spawned ? WORLD_STATE_ZM_FLAG_READY_ALLIANCE : WORLD_STATE_ZM_FLAG_NOT_READY_ALLIANCE;
        SendUpdateWorldState(m_scoutWorldStateAlliance, WORLD_STATE_ADD);

        m_flagReady[TEAM_INDEX_ALLIANCE] = spawned;

        if (spawned)
            sWorld.SendDefenseMessage(ZONE_ID_ZANGARMARSH, LANG_OPVP_ZM_SPAWN_FIELD_SCOUT_A);
    }
    else
    {
        SendUpdateWorldState(m_scoutWorldStateHorde, WORLD_STATE_REMOVE);
        m_scoutWorldStateHorde = spawned ? WORLD_STATE_ZM_FLAG_READY_HORDE : WORLD_STATE_ZM_FLAG_NOT_READY_HORDE;
        SendUpdateWorldState(m_scoutWorldStateHorde, WORLD_STATE_ADD);

        m_flagReady[TEAM_INDEX_HORDE] = spawned;

        if (spawned)
            sWorld.SendDefenseMessage(ZONE_ID_ZANGARMARSH, LANG_OPVP_ZM_SPAWN_FIELD_SCOUT_H);
    }
}

// Handle the graveyard banner use (处理玩家携带军旗点击中央墓地旗帜以夺取双塔废墟墓地与副本增益)
bool OutdoorPvPZM::HandleGameObjectUse(Player* player, GameObject* go)
{
    Team team = player->GetTeam();

    switch (go->GetEntry())
    {
        case GO_ZANGA_BANNER_CENTER_NEUTRAL:
            break;
        case GO_ZANGA_BANNER_CENTER_ALLIANCE:
            if (team == ALLIANCE || !player->HasAura(SPELL_BATTLE_STANDARD_HORDE))
                return false;
            break;
        case GO_ZANGA_BANNER_CENTER_HORDE:
            if (team == HORDE || !player->HasAura(SPELL_BATTLE_STANDARD_ALLIANCE))
                return false;
            break;
        default:
            return false;
    }

    // disable old banners - note the alliance and horde banners can despawn by self (隐藏旧墓地旗帜及光柱特效)
    if (m_graveyardOwner == ALLIANCE)
    {
        RespawnGO(go, m_graveyardBannerAlliance, false);
        SetBeaconArtKit(go, m_beamGraveyardBlue, 0);
    }
    else if (m_graveyardOwner == HORDE)
    {
        RespawnGO(go, m_graveyardBannerHorde, false);
        SetBeaconArtKit(go, m_beamGraveyardRed, 0);
    }
    else
        RespawnGO(go, m_graveyardBannerNeutral, false);

    if (team == ALLIANCE)
    {
        // change banners (切换为联盟墓地旗帜与蓝光柱)
        RespawnGO(go, m_graveyardBannerAlliance, true);
        SetBeaconArtKit(go, m_beamGraveyardBlue, SPELL_BEAM_BLUE);

        // update world state (更新墓地世界状态图标)
        SendUpdateWorldState(m_graveyardWorldState, WORLD_STATE_REMOVE);
        m_graveyardWorldState = WORLD_STATE_ZM_GRAVEYARD_ALLIANCE;
        SendUpdateWorldState(m_graveyardWorldState, WORLD_STATE_ADD);

        // remove player flag aura (移除玩家身上的战旗光环)
        player->RemoveAurasDueToSpell(SPELL_BATTLE_STANDARD_ALLIANCE);

        // send defense message (广播墓地占领消息)
        sWorld.SendDefenseMessage(ZONE_ID_ZANGARMARSH, LANG_OPVP_ZM_CAPTURE_GRAVEYARD_A);
    }
    else
    {
        // change banners (切换为部落墓地旗帜与红光柱)
        RespawnGO(go, m_graveyardBannerHorde, true);
        SetBeaconArtKit(go, m_beamGraveyardRed, SPELL_BEAM_RED);

        // update world state (更新墓地世界状态图标)
        SendUpdateWorldState(m_graveyardWorldState, WORLD_STATE_REMOVE);
        m_graveyardWorldState = WORLD_STATE_ZM_GRAVEYARD_HORDE;
        SendUpdateWorldState(m_graveyardWorldState, WORLD_STATE_ADD);

        // remove player flag aura (移除玩家身上的战旗光环)
        player->RemoveAurasDueToSpell(SPELL_BATTLE_STANDARD_HORDE);

        // send defense message (广播墓地占领消息)
        sWorld.SendDefenseMessage(ZONE_ID_ZANGARMARSH, LANG_OPVP_ZM_CAPTURE_GRAVEYARD_H);
    }

    // change the graveyard link (将赞加沼泽及盘牙水库全系列副本墓地链接到占领方)
    SetGraveYardLinkTeam(GRAVEYARD_ID_TWIN_SPIRE, GRAVEYARD_ZONE_TWIN_SPIRE, team, 530);
    SetGraveYardLinkTeam(GRAVEYARD_ID_TWIN_SPIRE, ZONE_ID_STEAMVAULT, team, 545);
    SetGraveYardLinkTeam(GRAVEYARD_ID_TWIN_SPIRE, ZONE_ID_UNDERBOG, team, 546);
    SetGraveYardLinkTeam(GRAVEYARD_ID_TWIN_SPIRE, ZONE_ID_SLAVE_PENS, team, 547);
    SetGraveYardLinkTeam(GRAVEYARD_ID_TWIN_SPIRE, ZONE_ID_SERPENTSHRINE_CAVERN, team, 548);

    // apply zone buff (撤销旧控制方并向新控制方施加双塔祝福增益)
    if (m_graveyardOwner != TEAM_NONE)
        BuffTeam(m_graveyardOwner, SPELL_TWIN_SPIRE_BLESSING, true);
    BuffTeam(team, SPELL_TWIN_SPIRE_BLESSING);

    // reset scout so that team cannot take flag (占领墓地后关闭己方斥候的继续领旗权限)
    UpdateScoutState(team, false);

    // update graveyard owner (记录中央墓地新控制方)
    m_graveyardOwner = team;

    // return true when completed successfully
    // this is required for the proper despawn of the flag; it won't stop further DB scripts from being executed
    // (返回 true 以确保战旗对象正常消失，同时不影响后续 DB 脚本执行)
    return true;
}

// Handle the ZM beacons - this is done by npcs which have certain auras (通过为特效 NPC 施放/清除光环控制信标与墓地光柱显示)
void OutdoorPvPZM::SetBeaconArtKit(const WorldObject* objRef, ObjectGuid creatureGuid, uint32 auraId)
{
    if (Creature* beam = objRef->GetMap()->GetCreature(creatureGuid))
    {
        if (auraId)
            beam->CastSpell(beam, auraId, TRIGGERED_OLD_TRIGGERED);
        else
            beam->RemoveAllAuras();
    }
}

// Check condition for ZM flag NPCs (判断斥候 NPC 对话中是否允许领取军旗)
bool OutdoorPvPZM::IsConditionFulfilled(Player const* /*source*/, uint32 conditionId, WorldObject const* /*conditionSource*/, uint32 /*conditionSourceType*/)
{
    switch (conditionId)
    {
        case OPVP_COND_ZM_ALLY_SCOUT_FLAG_READY:
            return m_flagReady[TEAM_INDEX_ALLIANCE] && !m_playerCarryingFlag[TEAM_INDEX_ALLIANCE];
        case OPVP_COND_ZM_HORDE_SCOUT_FLAG_READY:
            return m_flagReady[TEAM_INDEX_HORDE] && !m_playerCarryingFlag[TEAM_INDEX_HORDE];
    }

    return false;
}

// Handle condition state change by checking the aura existance on players (响应战旗携带光环状态变化，更新是否已有玩家持旗)
void OutdoorPvPZM::HandleConditionStateChange(uint32 conditionId, bool state)
{
    // state = false -> flag dropped (false 表示战旗已掉落或消耗)
    // state = true -> flag picked up (true 表示玩家已领取战旗)
    switch (conditionId)
    {
        case OPVP_COND_ZM_ALLY_SCOUT_FLAG_READY:
            m_playerCarryingFlag[TEAM_INDEX_ALLIANCE] = state;
            break;
        case OPVP_COND_ZM_HORDE_SCOUT_FLAG_READY:
            m_playerCarryingFlag[TEAM_INDEX_HORDE] = state;
            break;
    }
}
