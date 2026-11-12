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

#include "OutdoorPvPNA.h"
#include "Server/WorldPacket.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Entities/Object.h"
#include "Entities/Creature.h"
#include "Entities/GameObject.h"
#include "Entities/Player.h"
#include "Tools/Language.h"

OutdoorPvPNA::OutdoorPvPNA() : OutdoorPvP(),
    m_zoneOwner(TEAM_NONE),
    m_soldiersRespawnTimer(0),
    m_zoneWorldState(0),
    m_zoneMapState(WORLD_STATE_NA_HALAA_NEUTRAL),
    m_guardsLeft(0),
    m_isUnderSiege(false)
{
    // initially set graveyard owner to neither faction (初始将哈兰墓地归属设为无阵营控制)
    SetGraveYardLinkTeam(GRAVEYARD_ID_HALAA, GRAVEYARD_ZONE_ID_HALAA, TEAM_INVALID, 530);
}

// 填充哈兰城防、双足飞龙轰炸点及剩余守卫数的初始世界状态
void OutdoorPvPNA::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    if (m_zoneOwner != TEAM_NONE)
    {
        FillInitialWorldState(data, count, m_zoneWorldState, WORLD_STATE_ADD);

        // map states (填充四个方向双足飞龙轰炸点的地图状态)
        for (unsigned int i : m_roostWorldState)
            FillInitialWorldState(data, count, i, WORLD_STATE_ADD);
    }

    FillInitialWorldState(data, count, m_zoneMapState, WORLD_STATE_ADD);
    FillInitialWorldState(data, count, WORLD_STATE_NA_GUARDS_MAX, MAX_NA_GUARDS);
    FillInitialWorldState(data, count, WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);
}

// 玩家离开纳格兰时移除哈兰相关世界状态 UI
void OutdoorPvPNA::SendRemoveWorldStates(Player* player)
{
    player->SendUpdateWorldState(m_zoneWorldState, WORLD_STATE_REMOVE);
    player->SendUpdateWorldState(m_zoneMapState, WORLD_STATE_REMOVE);

    for (unsigned int i : m_roostWorldState)
        player->SendUpdateWorldState(i, WORLD_STATE_REMOVE);
}

// 玩家进入纳格兰时若己方控制哈兰则施加“哈兰之力”增益
void OutdoorPvPNA::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    // remove the buff from the player first because there are some issues at relog (先移除旧光环，避免重连时残留)
    player->RemoveAurasDueToSpell(SPELL_STRENGTH_HALAANI);

    // buff the player if same team is controlling the zone (控制方玩家获得哈兰之力增益)
    if (player->GetTeam() == m_zoneOwner)
        player->CastSpell(player, SPELL_STRENGTH_HALAANI, TRIGGERED_OLD_TRIGGERED);
}

// 玩家离开纳格兰时移除“哈兰之力”增益
void OutdoorPvPNA::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    // remove the buff from the player (移除哈兰之力光环)
    player->RemoveAurasDueToSpell(SPELL_STRENGTH_HALAANI);

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

// 占领哈兰战旗完成时为范围内的获胜方玩家发放任务击杀计数
void OutdoorPvPNA::HandleObjectiveComplete(uint32 eventId, const std::list<Player*>& players, Team team)
{
    if (eventId == EVENT_HALAA_BANNER_WIN_ALLIANCE || eventId == EVENT_HALAA_BANNER_WIN_HORDE)
    {
        for (auto player : players)
        {
            if (player && player->GetTeam() == team)
                player->KilledMonsterCredit(NPC_HALAA_COMBATANT);
        }
    }
}

// Cast player spell on opponent kill (玩家在哈兰区域内击杀敌对玩家时奖励阵营研究勋章)
void OutdoorPvPNA::HandlePlayerKillInsideArea(Player* player)
{
    if (player->GetAreaId() == ZONE_HALAA)
        player->CastSpell(nullptr, player->GetTeam() == ALLIANCE ? SPELL_NAGRAND_TOKEN_ALLIANCE : SPELL_NAGRAND_TOKEN_HORDE, TRIGGERED_OLD_TRIGGERED);
}

// 生物创建时登记哈兰阵营商人及统计存活守卫数量（首个守卫刷新时锁定战旗）
void OutdoorPvPNA::HandleCreatureCreate(Creature* creature)
{
    switch (creature->GetEntry())
    {
        case NPC_RESEARCHER_KARTOS:
        case NPC_QUARTERMASTER_DAVIAN:
        case NPC_MERCHANT_ALDRAAN:
        case NPC_VENDOR_CENDRII:
        case NPC_AMMUNITIONER_BANRO:
        case NPC_RESEARCHER_AMERELDINE:
        case NPC_QUARTERMASTER_NORELIQE:
        case NPC_MERCHANT_COREIEL:
        case NPC_VENDOR_EMBELAR:
        case NPC_AMMUNITIONER_TASALDAN:
            m_teamVendors.push_back(creature->GetObjectGuid());
            break;
        case NPC_HORDE_HALAANI_GUARD:
        case NPC_ALLIANCE_HANAANI_GUARD:
            // prevent updating guard counter on owner take over (占领切换期间已重置满额时不再重复累加)
            if (m_guardsLeft == MAX_NA_GUARDS)
                return;

            if (m_guardsLeft == 0)
            {
                LockHalaa(creature);

                // update world state (守卫重新进驻，哈兰恢复阵营锁定状态)
                SendUpdateWorldState(m_zoneMapState, WORLD_STATE_REMOVE);
                m_zoneMapState = m_zoneOwner == ALLIANCE ? WORLD_STATE_NA_HALAA_ALLIANCE : WORLD_STATE_NA_HALAA_HORDE;
                SendUpdateWorldState(m_zoneMapState, WORLD_STATE_ADD);
            }

            ++m_guardsLeft;
            SendUpdateWorldState(WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);
            break;
    }
}

// 哈兰守卫死亡时记录重生坐标并递减计数；全部阵亡后进入围城状态并解锁战旗占领
void OutdoorPvPNA::HandleCreatureDeath(Creature* creature)
{
    if (creature->GetEntry() != NPC_HORDE_HALAANI_GUARD && creature->GetEntry() != NPC_ALLIANCE_HANAANI_GUARD)
        return;

    // get the location of the dead guard for future respawn (保存死亡守卫的重生点位用于后续按序补兵)
    float x, y, z, o;
    creature->GetRespawnCoord(x, y, z, &o);
    HalaaSoldiersSpawns location = {x, y, z, o};
    m_deadSoldiers.push(location);

    // set the respawn timer after the last guard died - 5 min for the first time, or 1 hour if the city is under siege
    // (启动守卫复活计时：常态 5 分钟，处于被围攻状态时延长为 1 小时)
    if (!m_soldiersRespawnTimer)
        m_soldiersRespawnTimer = m_isUnderSiege ? HOUR * IN_MILLISECONDS : 5 * MINUTE * IN_MILLISECONDS;

    // decrease the counter (递减剩余守卫数量并更新 UI)
    --m_guardsLeft;
    SendUpdateWorldState(WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);

    if (m_guardsLeft == 0)
    {
        // set the zone under siege and increase the respawn timer (守卫全部阵亡，进入围城状态并将重生计时设为 1 小时)
        m_isUnderSiege = true;
        m_soldiersRespawnTimer = HOUR * IN_MILLISECONDS;

        // make capturable (解锁哈兰战旗允许敌对阵营占领)
        UnlockHalaa(creature);

        // update world state (将地图图标切换为无防御/可争夺状态)
        SendUpdateWorldState(m_zoneMapState, WORLD_STATE_REMOVE);
        m_zoneMapState = m_zoneOwner == ALLIANCE ? WORLD_STATE_NA_HALAA_NEUTRAL_A : WORLD_STATE_NA_HALAA_NEUTRAL_H;
        SendUpdateWorldState(m_zoneMapState, WORLD_STATE_ADD);

        sWorld.SendDefenseMessage(ZONE_ID_NAGRAND, LANG_OPVP_NA_DEFENSELESS);
    }
}

// 游戏对象创建时记录哈兰主战旗及四个方向的双足飞龙台、炸弹车、损毁栖木 GUID
void OutdoorPvPNA::HandleGameObjectCreate(GameObject* go)
{
    OutdoorPvP::HandleGameObjectCreate(go);

    switch (go->GetEntry())
    {
        case GO_HALAA_BANNER:
            m_capturePoint = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_zoneOwner));
            break;

        case GO_WYVERN_ROOST_ALLIANCE_SOUTH:
            m_roostsAlliance[0] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_ALLIANCE_NORTH:
            m_roostsAlliance[1] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_ALLIANCE_EAST:
            m_roostsAlliance[2] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_ALLIANCE_WEST:
            m_roostsAlliance[3] = go->GetObjectGuid();
            break;

        case GO_BOMB_WAGON_HORDE_SOUTH:
            m_wagonsHorde[0] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_HORDE_NORTH:
            m_wagonsHorde[1] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_HORDE_EAST:
            m_wagonsHorde[2] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_HORDE_WEST:
            m_wagonsHorde[3] = go->GetObjectGuid();
            break;

        case GO_DESTROYED_ROOST_ALLIANCE_SOUTH:
            m_roostsBrokenAlliance[0] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_ALLIANCE_NORTH:
            m_roostsBrokenAlliance[1] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_ALLIANCE_EAST:
            m_roostsBrokenAlliance[2] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_ALLIANCE_WEST:
            m_roostsBrokenAlliance[3] = go->GetObjectGuid();
            break;

        case GO_WYVERN_ROOST_HORDE_SOUTH:
            m_roostsHorde[0] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_HORDE_NORTH:
            m_roostsHorde[1] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_HORDE_EAST:
            m_roostsHorde[2] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_HORDE_WEST:
            m_roostsHorde[3] = go->GetObjectGuid();
            break;

        case GO_BOMB_WAGON_ALLIANCE_SOUTH:
            m_wagonsAlliance[0] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_ALLIANCE_NORTH:
            m_wagonsAlliance[1] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_ALLIANCE_EAST:
            m_wagonsAlliance[2] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_ALLIANCE_WEST:
            m_wagonsAlliance[3] = go->GetObjectGuid();
            break;

        case GO_DESTROYED_ROOST_HORDE_SOUTH:
            m_roostsBrokenHorde[0] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_HORDE_NORTH:
            m_roostsBrokenHorde[1] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_HORDE_EAST:
            m_roostsBrokenHorde[2] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_HORDE_WEST:
            m_roostsBrokenHorde[3] = go->GetObjectGuid();
            break;
    }
}

void OutdoorPvPNA::UpdateWorldState(uint32 value)
{
    SendUpdateWorldState(m_zoneWorldState, value);
    SendUpdateWorldState(m_zoneMapState, value);

    UpdateWyvernsWorldState(value);
}

void OutdoorPvPNA::UpdateWyvernsWorldState(uint32 value)
{
    for (unsigned int i : m_roostWorldState)
        SendUpdateWorldState(i, value);
}

// process the capture events (处理哈兰战旗的占领进度和胜利事件)
bool OutdoorPvPNA::HandleEvent(uint32 eventId, Object* source, Object* /*target*/)
{
    if (!source->IsGameObject())
        return false;

    GameObject* go = static_cast<GameObject*>(source);
    // If we are not using the Halaa banner return (仅处理哈兰主战旗触发的事件)
    if (go->GetEntry() != GO_HALAA_BANNER)
        return false;

    bool eventHandled = true;

    switch (eventId)
    {
        case EVENT_HALAA_BANNER_WIN_ALLIANCE:
            ProcessCaptureEvent(go, ALLIANCE);
            eventHandled = false;
            break;
        case EVENT_HALAA_BANNER_WIN_HORDE:
            ProcessCaptureEvent(go, HORDE);
            eventHandled = false;
            break;
        case EVENT_HALAA_BANNER_PROGRESS_ALLIANCE:
            SetBannerVisual(go, CAPTURE_ARTKIT_ALLIANCE, CAPTURE_ANIM_ALLIANCE);
            sWorld.SendDefenseMessage(ZONE_ID_NAGRAND, LANG_OPVP_NA_PROGRESS_A);
            break;
        case EVENT_HALAA_BANNER_PROGRESS_HORDE:
            SetBannerVisual(go, CAPTURE_ARTKIT_HORDE, CAPTURE_ANIM_HORDE);
            sWorld.SendDefenseMessage(ZONE_ID_NAGRAND, LANG_OPVP_NA_PROGRESS_H);
            break;
    }

    // there are some events which required further DB script (占领胜利事件返回 false 以允许 DB 脚本刷新新守卫与商人)
    return eventHandled;
}

// 切换哈兰控制阵营：更新锁定状态、满额守卫、墓地归属、阵营对象与区域增益
void OutdoorPvPNA::ProcessCaptureEvent(GameObject* go, Team team)
{
    BuffTeam(m_zoneOwner, SPELL_STRENGTH_HALAANI, true);

    // update capture point owner (更新哈兰控制权并锁定战旗)
    m_zoneOwner = team;

    LockHalaa(go);
    m_guardsLeft = MAX_NA_GUARDS;

    m_isUnderSiege = false;
    m_soldiersRespawnTimer = 0;

    UpdateWorldState(WORLD_STATE_REMOVE);
    DespawnVendors(go);
    SetGraveYardLinkTeam(GRAVEYARD_ID_HALAA, GRAVEYARD_ZONE_ID_HALAA, m_zoneOwner, 530);

    if (m_zoneOwner == ALLIANCE)
    {
        m_zoneWorldState = WORLD_STATE_NA_GUARDS_ALLIANCE;
        m_zoneMapState = WORLD_STATE_NA_HALAA_ALLIANCE;
    }
    else
    {
        m_zoneWorldState = WORLD_STATE_NA_GUARDS_HORDE;
        m_zoneMapState = WORLD_STATE_NA_HALAA_HORDE;
    }

    HandleFactionObjects(go);
    UpdateWorldState(WORLD_STATE_ADD);

    SendUpdateWorldState(WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);

    BuffTeam(m_zoneOwner, SPELL_STRENGTH_HALAANI);
    sWorld.SendDefenseMessage(ZONE_ID_NAGRAND, m_zoneOwner == ALLIANCE ? LANG_OPVP_NA_CAPTURE_A : LANG_OPVP_NA_CAPTURE_H);
}

// Handle the gameobjects spawn/despawn depending on the controller faction (根据当前占领阵营切换四个方向的飞龙栖木与炸弹车状态)
void OutdoorPvPNA::HandleFactionObjects(const WorldObject* objRef)
{
    if (m_zoneOwner == ALLIANCE)
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            RespawnGO(objRef, m_wagonsHorde[i], false);
            RespawnGO(objRef, m_roostsBrokenAlliance[i], false);
            RespawnGO(objRef, m_roostsAlliance[i], false);
            RespawnGO(objRef, m_wagonsAlliance[i], false);
            RespawnGO(objRef, m_roostsBrokenHorde[i], true);

            m_roostWorldState[i] = nagrandRoostStatesHordeNeutral[i];
        }
    }
    else
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            RespawnGO(objRef, m_wagonsAlliance[i], false);
            RespawnGO(objRef, m_roostsBrokenHorde[i], false);
            RespawnGO(objRef, m_roostsHorde[i], false);
            RespawnGO(objRef, m_wagonsHorde[i], false);
            RespawnGO(objRef, m_roostsBrokenAlliance[i], true);

            m_roostWorldState[i] = nagrandRoostStatesAllianceNeutral[i];
        }
    }
}

// Handle vendors despawn when the city is captured by the other faction (哈兰易主时强制消失原控制方商人)
void OutdoorPvPNA::DespawnVendors(const WorldObject* objRef)
{
    // despawn all team vendors (遍历并解召唤所有已记录的阵营商人)
    for (GuidList::const_iterator itr = m_teamVendors.begin(); itr != m_teamVendors.end(); ++itr)
    {
        if (Creature* soldier = objRef->GetMap()->GetCreature(*itr))
            soldier->ForcedDespawn();
    }
    m_teamVendors.clear();
}

// 处理玩家对四个方向飞龙栖木、损毁栖木及炸弹车的使用交互
bool OutdoorPvPNA::HandleGameObjectUse(Player* player, GameObject* go)
{
    if (player->GetTeam() == ALLIANCE)
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            if (go->GetEntry() == nagrandWagonsAlliance[i])
            {
                // update roost states (联盟引爆炸弹车摧毁部落飞龙栖木)
                UpdateWyvernsWorldState(WORLD_STATE_REMOVE);
                m_roostWorldState[i] = nagrandRoostStatesHordeNeutral[i];
                UpdateWyvernsWorldState(WORLD_STATE_ADD);

                // spawn the broken roost and despawn the other one (隐藏完好栖木并刷新损毁状态栖木)
                RespawnGO(go, m_roostsHorde[i], false);
                RespawnGO(go, m_roostsBrokenHorde[i], true);

                // no need to iterate the other roosts (已匹配目标对象，提前返回)
                return false;
            }
            if (go->GetEntry() == nagrandRoostsBrokenAlliance[i])
            {
                // update roost states (联盟修复损毁的己方飞龙栖木)
                UpdateWyvernsWorldState(WORLD_STATE_REMOVE);
                m_roostWorldState[i] = nagrandRoostStatesAlliance[i];
                UpdateWyvernsWorldState(WORLD_STATE_ADD);

                // spawn the repaired one along with the explosive wagon - the broken one despawns by self
                // (刷新己方完好栖木及可供部落破坏的炸弹车)
                RespawnGO(go, m_wagonsHorde[i], true);
                RespawnGO(go, m_roostsAlliance[i], true);

                // no need to iterate the other roosts (已匹配目标对象，提前返回)
                return false;
            }
            if (go->GetEntry() == nagrandRoostsAlliance[i])
            {
                // mark player as pvp (骑乘空袭飞龙时标记玩家开启 PvP 状态)
                player->UpdatePvP(true);

                // prevent despawning after go use (防止飞龙栖木被使用后消失)
                go->SetRespawnTime(0);

                // no need to iterate the other roosts (已匹配目标对象，提前返回)
                return false;
            }
        }
    }
    else if (player->GetTeam() == HORDE)
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            if (go->GetEntry() == nagrandWagonsHorde[i])
            {
                // update roost states (部落引爆炸弹车摧毁联盟飞龙栖木)
                UpdateWyvernsWorldState(WORLD_STATE_REMOVE);
                m_roostWorldState[i] = nagrandRoostStatesAllianceNeutral[i];
                UpdateWyvernsWorldState(WORLD_STATE_ADD);

                // spawn the broken roost and despawn the other one (隐藏完好栖木并刷新损毁状态栖木)
                RespawnGO(go, m_roostsAlliance[i], false);
                RespawnGO(go, m_roostsBrokenAlliance[i], true);

                // no need to iterate the other roosts (已匹配目标对象，提前返回)
                return false;
            }
            if (go->GetEntry() == nagrandRoostsBrokenHorde[i])
            {
                // update roost states (部落修复损毁的己方飞龙栖木)
                UpdateWyvernsWorldState(WORLD_STATE_REMOVE);
                m_roostWorldState[i] = nagrandRoostStatesHorde[i];
                UpdateWyvernsWorldState(WORLD_STATE_ADD);

                // spawn the repaired one along with the explosive wagon - the broken one despawns by self
                // (刷新己方完好栖木及可供联盟破坏的炸弹车)
                RespawnGO(go, m_wagonsAlliance[i], true);
                RespawnGO(go, m_roostsHorde[i], true);

                // no need to iterate the other roosts (已匹配目标对象，提前返回)
                return false;
            }
            if (go->GetEntry() == nagrandRoostsHorde[i])
            {
                // mark player as pvp (骑乘空袭飞龙时标记玩家开启 PvP 状态)
                player->UpdatePvP(true);

                // prevent despawning after go use (防止飞龙栖木被使用后消失)
                go->SetRespawnTime(0);

                // no need to iterate the other roosts (已匹配目标对象，提前返回)
                return false;
            }
        }
    }

    return false;
}

// 定时驱动阵亡哈兰守卫的重生逻辑
void OutdoorPvPNA::Update(uint32 diff)
{
    if (m_soldiersRespawnTimer)
    {
        if (m_soldiersRespawnTimer < diff)
        {
            RespawnSoldier();

            // if all the guards are respawned, stop the timer, else resume the timer depending on the siege state
            // (若守卫已满员则停止计时，否则按当前围城状态续设下次重生间隔)
            if (m_guardsLeft == MAX_NA_GUARDS)
                m_soldiersRespawnTimer = 0;
            else
                m_soldiersRespawnTimer = m_isUnderSiege ? HOUR * IN_MILLISECONDS : 5 * MINUTE * IN_MILLISECONDS;
        }
        else
            m_soldiersRespawnTimer -= diff;
    }
}

// Handle soldiers respawn on timer - this will summon a replacement for the dead soldier (按先死先复的队列顺序重新召唤一名阵亡守卫)
void OutdoorPvPNA::RespawnSoldier()
{
    for (GuidZoneMap::const_iterator itr = m_zonePlayers.begin(); itr != m_zonePlayers.end(); ++itr)
    {
        // Find player who is in main zone (Nagrand) to get correct map reference (查找位于纳格兰主区域的玩家作为地图上下文参考来召唤生物)
        if (!itr->second)
            continue;

        if (Player* player = sObjectMgr.GetPlayer(itr->first))
        {
            // summon a soldier replacement in the order they were set in the deque. delete the element after summon
            // (从队列取出最早阵亡守卫的重生坐标进行召唤并出队)
            const HalaaSoldiersSpawns& location = m_deadSoldiers.front();
            player->SummonCreature(m_zoneOwner == ALLIANCE ? NPC_ALLIANCE_HANAANI_GUARD : NPC_HORDE_HALAANI_GUARD, location.x, location.y, location.z, location.o, TEMPSPAWN_DEAD_DESPAWN, 0, true);
            m_deadSoldiers.pop();
            break;
        }
    }
}

// Lock Halaa when captured (锁定哈兰占领点，守卫清零前不可争夺)
void OutdoorPvPNA::LockHalaa(const WorldObject* objRef)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(m_capturePoint))
        go->SetLootState(GO_JUST_DEACTIVATED);
    else
    {
        // if grid is unloaded, changing the saved slider value is enough (若网格未加载，则直接更新管理器保存的滑块锁定状态)
        CapturePointSlider value(m_zoneOwner == ALLIANCE ? CAPTURE_SLIDER_ALLIANCE : CAPTURE_SLIDER_HORDE, true);
        sOutdoorPvPMgr.SetCapturePointSlider(GO_HALAA_BANNER, value);
    }
}

// Unlock Halaa when all the soldiers are killed (全部守卫阵亡后解锁哈兰占领点允许双方争夺)
void OutdoorPvPNA::UnlockHalaa(const WorldObject* objRef)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(m_capturePoint))
        go->SetLootState(GO_ACTIVATED);
    else
    {
        // if grid is unloaded, changing the saved slider value is enough (若网格未加载，则直接更新管理器保存的滑块解锁状态)
        CapturePointSlider value(m_zoneOwner == ALLIANCE ? CAPTURE_SLIDER_ALLIANCE : CAPTURE_SLIDER_HORDE, false);
        sOutdoorPvPMgr.SetCapturePointSlider(GO_HALAA_BANNER, value);
    }
}
