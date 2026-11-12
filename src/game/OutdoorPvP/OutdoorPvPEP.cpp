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

#include "OutdoorPvPEP.h"
#include "Server/WorldPacket.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Entities/Object.h"
#include "Entities/Creature.h"
#include "Entities/GameObject.h"
#include "Entities/Player.h"

OutdoorPvPEP::OutdoorPvPEP() : OutdoorPvP(),
    m_towersAlliance(0),
    m_towersHorde(0)
{
    m_towerWorldState[0] = WORLD_STATE_EP_NORTHPASS_NEUTRAL;
    m_towerWorldState[1] = WORLD_STATE_EP_CROWNGUARD_NEUTRAL;
    m_towerWorldState[2] = WORLD_STATE_EP_EASTWALL_NEUTRAL;
    m_towerWorldState[3] = WORLD_STATE_EP_PLAGUEWOOD_NEUTRAL;

    for (auto& i : m_towerOwner)
        i = TEAM_NONE;

    // initially set graveyard owner to neither faction (初始将皇冠哨塔墓地归属设为无阵营控制)
    SetGraveYardLinkTeam(GRAVEYARD_ID_EASTERN_PLAGUE, GRAVEYARD_ZONE_EASTERN_PLAGUE, TEAM_INVALID, 0);
}

// 填充东瘟疫之地四座哨塔的初始世界状态
void OutdoorPvPEP::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    if (sWorld.getConfig(CONFIG_BOOL_OUTDOORPVP_EP_ENABLED))
    {
        FillInitialWorldState(data, count, WORLD_STATE_EP_TOWER_COUNT_ALLIANCE, m_towersAlliance);
        FillInitialWorldState(data, count, WORLD_STATE_EP_TOWER_COUNT_HORDE, m_towersHorde);

        for (unsigned int i : m_towerWorldState)
            FillInitialWorldState(data, count, i, WORLD_STATE_ADD);
    }
}

// 玩家离开东瘟疫之地时移除哨塔 UI 世界状态
void OutdoorPvPEP::SendRemoveWorldStates(Player* player)
{
    for (unsigned int i : m_towerWorldState)
        player->SendUpdateWorldState(i, WORLD_STATE_REMOVE);
}

// 玩家进入区域时刷新洛丹伦的回响增益
void OutdoorPvPEP::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    // remove the buff from the player first; Sometimes on relog players still have the aura (先移除旧增益，防止重新登录时残留)
    for (auto plaguelandsTowerBuff : plaguelandsTowerBuffs)
        player->RemoveAurasDueToSpell(player->GetTeam() == ALLIANCE ? plaguelandsTowerBuff.spellIdAlliance : plaguelandsTowerBuff.spellIdHorde);

    // buff the player (按己方阵营当前控制哨塔数施加对应层级的增益光环)
    switch (player->GetTeam())
    {
        case ALLIANCE:
            if (m_towersAlliance > 0)
                player->CastSpell(player, plaguelandsTowerBuffs[m_towersAlliance - 1].spellIdAlliance, TRIGGERED_OLD_TRIGGERED);
            break;
        case HORDE:
            if (m_towersHorde > 0)
                player->CastSpell(player, plaguelandsTowerBuffs[m_towersHorde - 1].spellIdHorde, TRIGGERED_OLD_TRIGGERED);
            break;
        default:
            break;
    }
}

// 玩家离开区域时移除洛丹伦的回响增益
void OutdoorPvPEP::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    // remove the buff from the player (移除各层级的洛丹伦的回响光环)
    for (auto plaguelandsTowerBuff : plaguelandsTowerBuffs)
        player->RemoveAurasDueToSpell(player->GetTeam() == ALLIANCE ? plaguelandsTowerBuff.spellIdAlliance : plaguelandsTowerBuff.spellIdHorde);

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

// 生物创建时记录幽灵飞行管理员及东墙驻防士兵 GUID
void OutdoorPvPEP::HandleCreatureCreate(Creature* creature)
{
    switch (creature->GetEntry())
    {
        case NPC_SPECTRAL_FLIGHT_MASTER:
            m_flightMaster = creature->GetObjectGuid();
            creature->setFaction(m_towerOwner[TOWER_ID_PLAGUEWOOD] == ALLIANCE ? FACTION_FLIGHT_MASTER_ALLIANCE : FACTION_FLIGHT_MASTER_HORDE);
            creature->CastSpell(creature, m_towerOwner[TOWER_ID_PLAGUEWOOD] == ALLIANCE ? SPELL_SPIRIT_PARTICLES_BLUE : SPELL_SPIRIT_PARTICLES_RED, TRIGGERED_OLD_TRIGGERED);
            break;
        case NPC_LORDAERON_COMMANDER:
        case NPC_LORDAERON_SOLDIER:
        case NPC_LORDAERON_VETERAN:
        case NPC_LORDAERON_FIGHTER:
            m_soldiers.push_back(creature->GetObjectGuid());
            break;
    }
}

// 游戏对象创建时初始化哨塔主旗帜、周围装饰旗帜及洛丹伦神龛 GUID
void OutdoorPvPEP::HandleGameObjectCreate(GameObject* go)
{
    OutdoorPvP::HandleGameObjectCreate(go);

    switch (go->GetEntry())
    {
        case GO_TOWER_BANNER_NORTHPASS:
            InitBanner(go, TOWER_ID_NORTHPASS);
            break;
        case GO_TOWER_BANNER_CROWNGUARD:
            InitBanner(go, TOWER_ID_CROWNGUARD);
            break;
        case GO_TOWER_BANNER_EASTWALL:
            InitBanner(go, TOWER_ID_EASTWALL);
            break;
        case GO_TOWER_BANNER_PLAGUEWOOD:
            InitBanner(go, TOWER_ID_PLAGUEWOOD);
            break;
        case GO_TOWER_BANNER:
            // sort banners (根据 50 码距离将通用塔旗归类到对应的四座哨塔)
            if (go->IsWithinDist2d(plaguelandsTowerLocations[TOWER_ID_NORTHPASS][0], plaguelandsTowerLocations[TOWER_ID_NORTHPASS][1], 50.0f))
                InitBanner(go, TOWER_ID_NORTHPASS);
            else if (go->IsWithinDist2d(plaguelandsTowerLocations[TOWER_ID_CROWNGUARD][0], plaguelandsTowerLocations[TOWER_ID_CROWNGUARD][1], 50.0f))
                InitBanner(go, TOWER_ID_CROWNGUARD);
            else if (go->IsWithinDist2d(plaguelandsTowerLocations[TOWER_ID_EASTWALL][0], plaguelandsTowerLocations[TOWER_ID_EASTWALL][1], 50.0f))
                InitBanner(go, TOWER_ID_EASTWALL);
            else if (go->IsWithinDist2d(plaguelandsTowerLocations[TOWER_ID_PLAGUEWOOD][0], plaguelandsTowerLocations[TOWER_ID_PLAGUEWOOD][1], 50.0f))
                InitBanner(go, TOWER_ID_PLAGUEWOOD);
            break;
        case GO_LORDAERON_SHRINE_ALLIANCE:
            m_lordaeronShrineAlliance = go->GetObjectGuid();
            break;
        case GO_LORDAERON_SHRINE_HORDE:
            m_lordaeronShrineHorde = go->GetObjectGuid();
            break;
    }
}

// 哨塔占领完成时为范围内获胜阵营玩家发放任务击杀计数与荣誉奖励
void OutdoorPvPEP::HandleObjectiveComplete(uint32 eventId, const PlayerList& players, Team team)
{
    uint32 credit;
    switch (eventId)
    {
        case EVENT_CROWNGUARD_PROGRESS_ALLIANCE:
        case EVENT_CROWNGUARD_PROGRESS_HORDE:
            credit = NPC_CROWNGUARD_TOWER_QUEST_DOODAD;
            break;
        case EVENT_EASTWALL_PROGRESS_ALLIANCE:
        case EVENT_EASTWALL_PROGRESS_HORDE:
            credit = NPC_EASTWALL_TOWER_QUEST_DOODAD;
            break;
        case EVENT_NORTHPASS_PROGRESS_ALLIANCE:
        case EVENT_NORTHPASS_PROGRESS_HORDE:
            credit = NPC_NORTHPASS_TOWER_QUEST_DOODAD;
            break;
        case EVENT_PLAGUEWOOD_PROGRESS_ALLIANCE:
        case EVENT_PLAGUEWOOD_PROGRESS_HORDE:
            credit = NPC_PLAGUEWOOD_TOWER_QUEST_DOODAD;
            break;
        default:
            return;
    }

    for (auto player : players)
    {
        if (player && player->GetTeam() == team)
        {
            player->KilledMonsterCredit(credit);
            player->RewardHonor(nullptr, 1, HONOR_REWARD_PLAGUELANDS);
        }
    }
}

// process the capture events (分发占领点触发的事件并广播防卫/占领公告)
bool OutdoorPvPEP::HandleEvent(uint32 eventId, Object* source, Object* /*target*/)
{
    if (!source->IsGameObject())
        return false;

    GameObject* go = static_cast<GameObject*>(source);
    for (uint8 i = 0; i < MAX_EP_TOWERS; ++i)
    {
        if (plaguelandsBanners[i] == go->GetEntry())
        {
            for (uint8 j = 0; j < 4; ++j)
            {
                if (plaguelandsTowerEvents[i][j].eventEntry == eventId)
                {
                    // prevent processing if the owner did not change (happens if progress event is called after contest event)
                    // (若归属阵营未实际变化则忽略重复事件)
                    if (plaguelandsTowerEvents[i][j].team != m_towerOwner[i])
                    {
                        if (plaguelandsTowerEvents[i][j].defenseMessage)
                            sWorld.SendDefenseMessageBroadcastText(ZONE_ID_EASTERN_PLAGUELANDS, plaguelandsTowerEvents[i][j].defenseMessage);

                        return ProcessCaptureEvent(go, i, plaguelandsTowerEvents[i][j].team, plaguelandsTowerEvents[i][j].worldState);
                    }
                    // no need to iterate other events or towers (已匹配哨塔与事件，无需继续遍历)
                    return false;
                }
            }
            // no need to iterate other towers (已匹配哨塔但无对应事件)
            return false;
        }
    }

    return false;
}

// 执行哨塔占领/中立化状态切换：更新旗帜视觉、阵营塔数、洛丹伦的回响增益与单塔奖励
bool OutdoorPvPEP::ProcessCaptureEvent(GameObject* go, uint32 towerId, Team team, uint32 newWorldState)
{
    // Remove existing buff for both teams before applying new one: this prevent auras stacking
    // (先移除双方现有哨塔增益，避免重新施加时出现多层叠加)
    if (m_towersAlliance != 0)
        BuffTeam(ALLIANCE, plaguelandsTowerBuffs[m_towersAlliance - 1].spellIdAlliance, true);
    if (m_towersHorde != 0)
        BuffTeam(HORDE, plaguelandsTowerBuffs[m_towersHorde - 1].spellIdHorde, true);

    // Update world state and banners for captured tower (更新塔旗视觉外观与双方占领计数)
    if (team == ALLIANCE)
    {
        // update banner (将塔旗切换为联盟外观)
        for (GuidList::const_iterator itr = m_towerBanners[towerId].begin(); itr != m_towerBanners[towerId].end(); ++itr)
            SetBannerVisual(go, (*itr), CAPTURE_ARTKIT_ALLIANCE, CAPTURE_ANIM_ALLIANCE);

        // update counter (联盟控制塔数 +1)
        ++m_towersAlliance;
        SendUpdateWorldState(WORLD_STATE_EP_TOWER_COUNT_ALLIANCE, m_towersAlliance);
    }
    else if (team == HORDE)
    {
        // update banner (将塔旗切换为部落外观)
        for (GuidList::const_iterator itr = m_towerBanners[towerId].begin(); itr != m_towerBanners[towerId].end(); ++itr)
            SetBannerVisual(go, (*itr), CAPTURE_ARTKIT_HORDE, CAPTURE_ANIM_HORDE);

        // update counter (部落控制塔数 +1)
        ++m_towersHorde;
        SendUpdateWorldState(WORLD_STATE_EP_TOWER_COUNT_HORDE, m_towersHorde);
    }
    else
    {
        // update banner (将塔旗切换为中立外观)
        for (GuidList::const_iterator itr = m_towerBanners[towerId].begin(); itr != m_towerBanners[towerId].end(); ++itr)
            SetBannerVisual(go, (*itr), CAPTURE_ARTKIT_NEUTRAL, CAPTURE_ANIM_NEUTRAL);

        if (m_towerOwner[towerId] == ALLIANCE)
        {
            // update counter (联盟控制塔数 -1)
            --m_towersAlliance;
            SendUpdateWorldState(WORLD_STATE_EP_TOWER_COUNT_ALLIANCE, m_towersAlliance);
        }
        else
        {
            // update counter (部落控制塔数 -1)
            --m_towersHorde;
            SendUpdateWorldState(WORLD_STATE_EP_TOWER_COUNT_HORDE, m_towersHorde);
        }
    }

    // Update Echoes of Lordaeron aura for capturing team and opposite team (if needed)
    // Apply auras with updated capture towers values
    // (按更新后的双方控制塔数重新施加洛丹伦的回响光环)
    if (m_towersAlliance != 0)
        BuffTeam(ALLIANCE, plaguelandsTowerBuffs[m_towersAlliance - 1].spellIdAlliance);
    if (m_towersHorde != 0)
        BuffTeam(HORDE, plaguelandsTowerBuffs[m_towersHorde - 1].spellIdHorde);

    // Update reward for captured tower (应用或撤销对应哨塔的专属奖励)
    bool eventHandled = true;

    if (team != TEAM_NONE)
    {
        // update capture point owner before rewards are applied (先记录新占领阵营)
        m_towerOwner[towerId] = team;

        // apply rewards of changed tower (启用占领哨塔的专属奖励)
        switch (towerId)
        {
            case TOWER_ID_NORTHPASS:
                RespawnGO(go, team == ALLIANCE ? m_lordaeronShrineAlliance : m_lordaeronShrineHorde, true);
                break;
            case TOWER_ID_CROWNGUARD:
                SetGraveYardLinkTeam(GRAVEYARD_ID_EASTERN_PLAGUE, GRAVEYARD_ZONE_EASTERN_PLAGUE, team, 0);
                break;
            case TOWER_ID_EASTWALL:
                // Return false - allow the DB to handle summons (返回 false 交由数据库事件脚本召唤巡逻士兵)
                if (m_towerOwner[TOWER_ID_NORTHPASS] != team)
                    eventHandled = false;
                break;
            case TOWER_ID_PLAGUEWOOD:
                // Return false - allow the DB to handle summons (返回 false 交由数据库事件脚本召唤幽灵飞行管理员)
                eventHandled = false;
                break;
        }
    }
    else
    {
        // remove rewards of changed tower (哨塔变为中立时移除专属奖励)
        switch (towerId)
        {
            case TOWER_ID_NORTHPASS:
                RespawnGO(go, m_towerOwner[TOWER_ID_NORTHPASS] == ALLIANCE ? m_lordaeronShrineAlliance : m_lordaeronShrineHorde, false);
                break;
            case TOWER_ID_CROWNGUARD:
                SetGraveYardLinkTeam(GRAVEYARD_ID_EASTERN_PLAGUE, GRAVEYARD_ZONE_EASTERN_PLAGUE, TEAM_INVALID, 0);
                break;
            case TOWER_ID_EASTWALL:
                UnsummonSoldiers(go);
                break;
            case TOWER_ID_PLAGUEWOOD:
                UnsummonFlightMaster(go);
                break;
        }

        // update capture point owner after rewards have been removed (移除奖励后再清空归属阵营)
        m_towerOwner[towerId] = team;
    }

    // update tower state (切换该哨塔的世界状态图标)
    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_REMOVE);
    m_towerWorldState[towerId] = newWorldState;
    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_ADD);

    // there are some events which required further DB script (部分事件需返回 false 以继续执行 DB 脚本召唤)
    return eventHandled;
}

// 使用洛丹伦神龛后重置重生时间为 0，防止交互后消失
bool OutdoorPvPEP::HandleGameObjectUse(Player* /*player*/, GameObject* go)
{
    // prevent despawning after go use (防止玩家使用神龛后游戏对象消失)
    if (go->GetEntry() == GO_LORDAERON_SHRINE_ALLIANCE || go->GetEntry() == GO_LORDAERON_SHRINE_HORDE)
        go->SetRespawnTime(0);

    return false;
}

void OutdoorPvPEP::InitBanner(GameObject* go, uint32 towerId)
{
    m_towerBanners[towerId].push_back(go->GetObjectGuid());
    go->SetGoArtKit(GetBannerArtKit(m_towerOwner[towerId]));
}

// Handle the unsummon of the spectral flight master when the Plaguewood tower is lost (病木林哨塔失守时解召唤幽灵飞行管理员)
void OutdoorPvPEP::UnsummonFlightMaster(const WorldObject* objRef)
{
    if (Creature* flightMaster = objRef->GetMap()->GetCreature(m_flightMaster))
        flightMaster->ForcedDespawn();
}

// Handle the unsummon of the soldiers when the Eastwall tower is lost (东墙哨塔失守时解召唤所有驻防士兵)
void OutdoorPvPEP::UnsummonSoldiers(const WorldObject* objRef)
{
    for (GuidList::const_iterator itr = m_soldiers.begin(); itr != m_soldiers.end(); ++itr)
    {
        if (Creature* soldier = objRef->GetMap()->GetCreature(*itr))
            soldier->ForcedDespawn();
    }

    m_soldiers.clear();
}
