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

#include "OutdoorPvPHP.h"
#include "Server/WorldPacket.h"
#include "World/World.h"
#include "Entities/Object.h"
#include "Entities/Creature.h"
#include "Entities/GameObject.h"
#include "Entities/Player.h"

OutdoorPvPHP::OutdoorPvPHP() : OutdoorPvP(),
    m_towersAlliance(0),
    m_towersHorde(0)
{
    m_towerWorldState[0] = WORLD_STATE_HP_OVERLOOK_NEUTRAL;
    m_towerWorldState[1] = WORLD_STATE_HP_STADIUM_NEUTRAL;
    m_towerWorldState[2] = WORLD_STATE_HP_BROKEN_HILL_NEUTRAL;

    for (auto& i : m_towerOwner)
        i = TEAM_NONE;
}

// 填充地狱火半岛三座堡垒的初始世界状态
void OutdoorPvPHP::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    FillInitialWorldState(data, count, WORLD_STATE_HP_TOWER_COUNT_ALLIANCE, m_towersAlliance);
    FillInitialWorldState(data, count, WORLD_STATE_HP_TOWER_COUNT_HORDE, m_towersHorde);
    FillInitialWorldState(data, count, WORLD_STATE_HP_TOWER_DISPLAY_A, WORLD_STATE_ADD);
    FillInitialWorldState(data, count, WORLD_STATE_HP_TOWER_DISPLAY_H, WORLD_STATE_ADD);

    for (unsigned int i : m_towerWorldState)
        FillInitialWorldState(data, count, i, WORLD_STATE_ADD);
}

// 玩家离开地狱火半岛时移除世界状态 UI
void OutdoorPvPHP::SendRemoveWorldStates(Player* player)
{
    player->SendUpdateWorldState(WORLD_STATE_HP_TOWER_DISPLAY_A, WORLD_STATE_REMOVE);
    player->SendUpdateWorldState(WORLD_STATE_HP_TOWER_DISPLAY_H, WORLD_STATE_REMOVE);

    for (unsigned int i : m_towerWorldState)
        player->SendUpdateWorldState(i, WORLD_STATE_REMOVE);
}

// 玩家进入区域或关联副本时检查全控状态并施加地狱火优势增益
void OutdoorPvPHP::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    // remove the buff from the player first; Sometimes on relog players still have the aura (先移除旧光环防止重连残留)
    player->RemoveAurasDueToSpell(player->GetTeam() == ALLIANCE ? SPELL_HELLFIRE_SUPERIORITY_ALLIANCE : SPELL_HELLFIRE_SUPERIORITY_HORDE);

    // buff the player if same team is controlling all capture points (若本方控制全部 3 座堡垒则施加优势光环)
    if (m_towersAlliance == MAX_HP_TOWERS && player->GetTeam() == ALLIANCE)
        player->CastSpell(player, SPELL_HELLFIRE_SUPERIORITY_ALLIANCE, TRIGGERED_OLD_TRIGGERED);
    else if (m_towersHorde == MAX_HP_TOWERS && player->GetTeam() == HORDE)
        player->CastSpell(player, SPELL_HELLFIRE_SUPERIORITY_HORDE, TRIGGERED_OLD_TRIGGERED);
}

// 玩家离开区域时移除地狱火优势光环
void OutdoorPvPHP::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    // remove the buff from the player (移除地狱火优势光环)
    player->RemoveAurasDueToSpell(player->GetTeam() == ALLIANCE ? SPELL_HELLFIRE_SUPERIORITY_ALLIANCE : SPELL_HELLFIRE_SUPERIORITY_HORDE);

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

// 游戏对象创建时记录三座堡垒旗帜及占领点 GUID 并设置初始美术外观
void OutdoorPvPHP::HandleGameObjectCreate(GameObject* go)
{
    OutdoorPvP::HandleGameObjectCreate(go);

    switch (go->GetEntry())
    {
        case GO_TOWER_BANNER_OVERLOOK:
            m_banners[0] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[0], GO_ARTKIT_OVERLOOK_ALLIANCE, GO_ARTKIT_OVERLOOK_HORDE, GO_ARTKIT_OVERLOOK_NEUTRAL));
            break;
        case GO_TOWER_BANNER_STADIUM:
            m_banners[1] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[1], GO_ARTKIT_STADIUM_ALLIANCE, GO_ARTKIT_STADIUM_HORDE, GO_ARTKIT_STADIUM_NEUTRAL));
            break;
        case GO_TOWER_BANNER_BROKEN_HILL:
            m_banners[2] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[2], GO_ARTKIT_BROKEN_HILL_ALLIANCE, GO_ARTKIT_BROKEN_HILL_HORDE, GO_ARTKIT_BROKEN_HILL_NEUTRAL));
            break;
        case GO_HELLFIRE_BANNER_OVERLOOK:
            m_towers[0] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[0]));
            break;
        case GO_HELLFIRE_BANNER_STADIUM:
            m_towers[1] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[1]));
            break;
        case GO_HELLFIRE_BANNER_BROKEN_HILL:
            m_towers[2] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[2]));
            break;
    }
}

// 占领堡垒完成时为范围内获胜阵营玩家发放任务击杀计数和荣誉奖励
void OutdoorPvPHP::HandleObjectiveComplete(uint32 eventId, const std::list<Player*>& players, Team team)
{
    uint32 credit;
    switch (eventId)
    {
        case EVENT_OVERLOOK_PROGRESS_ALLIANCE:
        case EVENT_OVERLOOK_PROGRESS_HORDE:
            credit = NPC_CAPTURE_CREDIT_OVERLOOK;
            break;
        case EVENT_STADIUM_PROGRESS_ALLIANCE:
        case EVENT_STADIUM_PROGRESS_HORDE:
            credit = NPC_CAPTURE_CREDIT_STADIUM;
            break;
        case EVENT_BROKEN_HILL_PROGRESS_ALLIANCE:
        case EVENT_BROKEN_HILL_PROGRESS_HORDE:
            credit = NPC_CAPTURE_CREDIT_BROKEN_HILL;
            break;
        default:
            return;
    }

    for (auto player : players)
    {
        if (player && player->GetTeam() == team)
        {
            player->KilledMonsterCredit(credit);
            player->RewardHonor(nullptr, 1, HONOR_REWARD_HELLFIRE);
        }
    }
}

// Cast player spell on opponent kill (玩家在任一堡垒占领点半径内击杀敌对玩家时发放阵营印记)
void OutdoorPvPHP::HandlePlayerKillInsideArea(Player* player)
{
    for (auto m_tower : m_towers)
    {
        if (GameObject* capturePoint = player->GetMap()->GetGameObject(m_tower))
        {
            // check capture point range (检查玩家是否处于该占领点半径范围内)
            GameObjectInfo const* info = capturePoint->GetGOInfo();
            if (info && player->IsWithinDistInMap(capturePoint, info->capturePoint.radius))
            {
                player->CastSpell(nullptr, player->GetTeam() == ALLIANCE ? SPELL_HELLFIRE_TOWER_TOKEN_ALLIANCE : SPELL_HELLFIRE_TOWER_TOKEN_HORDE, TRIGGERED_OLD_TRIGGERED);
                return;
            }
        }
    }
}

// process the capture events (分发占领点触发事件并广播防御/占领提示)
bool OutdoorPvPHP::HandleEvent(uint32 eventId, Object* source, Object* /*target*/)
{
    if (!source->IsGameObject())
        return false;

    GameObject* go = static_cast<GameObject*>(source);
    for (uint8 i = 0; i < MAX_HP_TOWERS; ++i)
    {
        if (hellfireBanners[i] == go->GetEntry())
        {
            for (uint8 j = 0; j < 4; ++j)
            {
                if (hellfireTowerEvents[i][j].eventEntry == eventId)
                {
                    // prevent processing if the owner did not change (happens if progress event is called after contest event)
                    // (若归属阵营未发生变化则忽略重复事件)
                    if (hellfireTowerEvents[i][j].team != m_towerOwner[i])
                    {
                        if (hellfireTowerEvents[i][j].defenseMessage)
                            sWorld.SendDefenseMessage(ZONE_ID_HELLFIRE_PENINSULA, hellfireTowerEvents[i][j].defenseMessage);

                        return ProcessCaptureEvent(go, i, hellfireTowerEvents[i][j].team, hellfireTowerEvents[i][j].worldState, hellfireTowerEvents[i][j].towerArtKit, hellfireTowerEvents[i][j].towerAnim);
                    }
                    // no need to iterate other events or towers (已匹配堡垒与事件，提前返回)
                    return false;
                }
            }
            // no need to iterate other towers (已匹配堡垒但事件无关)
            return false;
        }
    }

    return false;
}

// 处理堡垒占领/中立化逻辑：更新旗帜视觉、阵营占领数及全控增益
bool OutdoorPvPHP::ProcessCaptureEvent(GameObject* go, uint32 towerId, Team team, uint32 newWorldState, uint32 towerArtKit, uint32 towerAnim)
{
    // set artkits and process buffs (设置旗帜视觉外观并增减全控增益)
    if (team == ALLIANCE)
    {
        SetBannerVisual(go, CAPTURE_ARTKIT_ALLIANCE, CAPTURE_ANIM_ALLIANCE);

        // update counter state (联盟占领数 +1，满 3 座则全区施加增益)
        ++m_towersAlliance;
        SendUpdateWorldState(WORLD_STATE_HP_TOWER_COUNT_ALLIANCE, m_towersAlliance);

        if (m_towersAlliance == MAX_HP_TOWERS)
            BuffTeam(ALLIANCE, SPELL_HELLFIRE_SUPERIORITY_ALLIANCE);
    }
    else if (team == HORDE)
    {
        SetBannerVisual(go, CAPTURE_ARTKIT_HORDE, CAPTURE_ANIM_HORDE);

        // update counter (部落占领数 +1，满 3 座则全区施加增益)
        ++m_towersHorde;
        SendUpdateWorldState(WORLD_STATE_HP_TOWER_COUNT_HORDE, m_towersHorde);

        if (m_towersHorde == MAX_HP_TOWERS)
            BuffTeam(HORDE, SPELL_HELLFIRE_SUPERIORITY_HORDE);
    }
    else
    {
        SetBannerVisual(go, CAPTURE_ARTKIT_NEUTRAL, CAPTURE_ANIM_NEUTRAL);

        if (m_towerOwner[towerId] == ALLIANCE)
        {
            if (m_towersAlliance == MAX_HP_TOWERS)
                BuffTeam(ALLIANCE, SPELL_HELLFIRE_SUPERIORITY_ALLIANCE, true);

            // update counter (联盟占领数 -1)
            --m_towersAlliance;
            SendUpdateWorldState(WORLD_STATE_HP_TOWER_COUNT_ALLIANCE, m_towersAlliance);
        }
        else
        {
            if (m_towersHorde == MAX_HP_TOWERS)
                BuffTeam(HORDE, SPELL_HELLFIRE_SUPERIORITY_HORDE, true);

            // update counter (部落占领数 -1)
            --m_towersHorde;
            SendUpdateWorldState(WORLD_STATE_HP_TOWER_COUNT_HORDE, m_towersHorde);
        }
    }

    SetBannerVisual(go, m_banners[towerId], towerArtKit, towerAnim);

    // update tower state (切换堡垒世界状态图标)
    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_REMOVE);
    m_towerWorldState[towerId] = newWorldState;
    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_ADD);

    // update capture point owner (记录最新归属阵营)
    m_towerOwner[towerId] = team;

    // the are no DB exceptions in this case (无需额外 DB 脚本处理)
    return true;
}
