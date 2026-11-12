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

#include "OutdoorPvPTF.h"
#include "Server/WorldPacket.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Entities/Object.h"
#include "Entities/Creature.h"
#include "Entities/GameObject.h"
#include "Entities/Player.h"

OutdoorPvPTF::OutdoorPvPTF() : OutdoorPvP(),
    m_zoneWorldState(WORLD_STATE_TF_TOWERS_CONTROLLED),
    m_zoneOwner(TEAM_NONE),
    //m_zoneUpdateTimer(TIMER_TF_UPDATE_TIME),
    m_zoneLockTimer(0),
    m_towersAlliance(0),
    m_towersHorde(0)
{
    m_towerWorldState[0] = WORLD_STATE_TF_WEST_TOWER_NEUTRAL;
    m_towerWorldState[1] = WORLD_STATE_TF_NORTH_TOWER_NEUTRAL;
    m_towerWorldState[2] = WORLD_STATE_TF_EAST_TOWER_NEUTRAL;
    m_towerWorldState[3] = WORLD_STATE_TF_SOUTH_EAST_TOWER_NEUTRAL;
    m_towerWorldState[4] = WORLD_STATE_TF_SOUTH_TOWER_NEUTRAL;

    for (auto& i : m_towerOwner)
        i = TEAM_NONE;
}

// 填充泰罗卡森林五座灵魂塔及区域锁定倒计时的初始世界状态
void OutdoorPvPTF::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    FillInitialWorldState(data, count, m_zoneWorldState, WORLD_STATE_ADD);
    if (m_zoneWorldState == WORLD_STATE_TF_TOWERS_CONTROLLED)
    {
        FillInitialWorldState(data, count, WORLD_STATE_TF_TOWER_COUNT_H, m_towersHorde);
        FillInitialWorldState(data, count, WORLD_STATE_TF_TOWER_COUNT_A, m_towersAlliance);

        for (unsigned int i : m_towerWorldState)
            FillInitialWorldState(data, count, i, WORLD_STATE_ADD);
    }
    else
    {
        uint32 firstDigit, secondDigit, hoursLeft;
        CalculateTimerWorldStateValues(firstDigit, secondDigit, hoursLeft);
        FillInitialWorldState(data, count, WORLD_STATE_TF_TIME_MIN_FIRST_DIGIT, firstDigit);
        FillInitialWorldState(data, count, WORLD_STATE_TF_TIME_MIN_SECOND_DIGIT, secondDigit);
        FillInitialWorldState(data, count, WORLD_STATE_TF_TIME_HOURS, hoursLeft);
    }
}

// 玩家离开泰罗卡森林时移除灵魂塔相关世界状态 UI
void OutdoorPvPTF::SendRemoveWorldStates(Player* player)
{
    player->SendUpdateWorldState(m_zoneWorldState, WORLD_STATE_REMOVE);

    for (unsigned int i : m_towerWorldState)
        player->SendUpdateWorldState(i, WORLD_STATE_REMOVE);
}

// 玩家进入泰罗卡森林或奥金顿副本时施加“奥金顿的祝福”增益
void OutdoorPvPTF::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    // remove the buff from the player first because there are some issues at relog (先移除旧光环避免重连残留)
    player->RemoveAurasDueToSpell(SPELL_AUCHINDOUN_BLESSING);

    // Handle the buffs (若己方阵营当前控制该区域则施加增益)
    if (player->GetTeam() == m_zoneOwner)
        player->CastSpell(player, SPELL_AUCHINDOUN_BLESSING, TRIGGERED_OLD_TRIGGERED);
}

// 玩家离开区域时移除“奥金顿的祝福”增益
void OutdoorPvPTF::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    // remove the buff from the player (移除奥金顿的祝福光环)
    player->RemoveAurasDueToSpell(SPELL_AUCHINDOUN_BLESSING);

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

// 游戏对象创建时记录五座灵魂塔战旗 GUID 并设置初始 ArtKit
void OutdoorPvPTF::HandleGameObjectCreate(GameObject* go)
{
    OutdoorPvP::HandleGameObjectCreate(go);

    switch (go->GetEntry())
    {
        case GO_TOWER_BANNER_WEST:
            m_towerBanners[0] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[0]));
            break;
        case GO_TOWER_BANNER_NORTH:
            m_towerBanners[1] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[1]));
            break;
        case GO_TOWER_BANNER_EAST:
            m_towerBanners[2] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[2]));
            break;
        case GO_TOWER_BANNER_SOUTH_EAST:
            m_towerBanners[3] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[3]));
            break;
        case GO_TOWER_BANNER_SOUTH:
            m_towerBanners[4] = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_towerOwner[4]));
            break;
    }
}

// 灵魂塔占领完成时为范围内的获胜阵营玩家推进日常任务进度
void OutdoorPvPTF::HandleObjectiveComplete(uint32 eventId, const std::list<Player*>& players, Team team)
{
    for (const auto& terokkarTowerEvent : terokkarTowerEvents)
    {
        for (uint8 j = 0; j < 4; ++j)
        {
            if (terokkarTowerEvent[j].eventEntry == eventId)
            {
                for (auto player : players)
                {
                    if (player && player->GetTeam() == team)
                        player->AreaExploredOrEventHappens(team == ALLIANCE ? QUEST_SPIRITS_OF_AUCHINDOUM_ALLIANCE : QUEST_SPIRITS_OF_AUCHINDOUM_HORDE);
                }
                return;
            }
        }
    }
}

// process the capture events (分发五座灵魂塔占领/中立化事件并发送防卫通告)
bool OutdoorPvPTF::HandleEvent(uint32 eventId, Object* source, Object* /*target*/)
{
    if (!source->IsGameObject())
        return false;

    GameObject* go = static_cast<GameObject*>(source);
    for (uint8 i = 0; i < MAX_TF_TOWERS; ++i)
    {
        if (terokkarTowers[i] == go->GetEntry())
        {
            for (uint8 j = 0; j < 4; ++j)
            {
                if (terokkarTowerEvents[i][j].eventEntry == eventId)
                {
                    // prevent processing if the owner did not change (happens if progress event is called after contest event)
                    // (若归属阵营未发生变化则忽略重复事件)
                    if (terokkarTowerEvents[i][j].team != m_towerOwner[i])
                    {
                        sWorld.SendDefenseMessage(ZONE_ID_TEROKKAR_FOREST, terokkarTowerEvents[i][j].defenseMessage);

                        return ProcessCaptureEvent(go, i, terokkarTowerEvents[i][j].team, terokkarTowerEvents[i][j].worldState);
                    }
                    // no need to iterate other events or towers (已匹配哨塔与事件，提前返回)
                    return false;
                }
            }
            // no need to iterate other towers (已匹配哨塔但无匹配事件)
            return false;
        }
    }

    return false;
}

// 处理灵魂塔占领：若某阵营同时控制全部 5 座灵魂塔，则锁定区域 6 小时并激活全区增益
bool OutdoorPvPTF::ProcessCaptureEvent(GameObject* go, uint32 towerId, Team team, uint32 newWorldState)
{
    if (team == ALLIANCE)
    {
        // update banner (将塔旗视觉切换为联盟)
        SetBannerVisual(go, CAPTURE_ARTKIT_ALLIANCE, CAPTURE_ANIM_ALLIANCE);

        // update tower count (联盟塔数 +1)
        ++m_towersAlliance;

        // if all towers are captured then process event (占领全部 5 座塔则触发区域锁定)
        if (m_towersAlliance == MAX_TF_TOWERS)
        {
            LockZone(go, towerId, team, newWorldState);
            return true;
        }

        // update tower count world state (更新联盟塔数世界状态)
        SendUpdateWorldState(WORLD_STATE_TF_TOWER_COUNT_A, m_towersAlliance);
    }
    else if (team == HORDE)
    {
        // update banner (将塔旗视觉切换为部落)
        SetBannerVisual(go, CAPTURE_ARTKIT_HORDE, CAPTURE_ANIM_HORDE);

        // update tower count (部落塔数 +1)
        ++m_towersHorde;

        // if all towers are captured then process event (占领全部 5 座塔则触发区域锁定)
        if (m_towersHorde == MAX_TF_TOWERS)
        {
            LockZone(go, towerId, team, newWorldState);
            return true;
        }

        // update tower count world state (更新部落塔数世界状态)
        SendUpdateWorldState(WORLD_STATE_TF_TOWER_COUNT_H, m_towersHorde);
    }
    else
    {
        // update banner (将塔旗视觉切换为中立)
        SetBannerVisual(go, CAPTURE_ARTKIT_NEUTRAL, CAPTURE_ANIM_NEUTRAL);

        // update tower count (递减原占领阵营控制塔数)
        if (m_towerOwner[towerId] == ALLIANCE)
        {
            --m_towersAlliance;
            SendUpdateWorldState(WORLD_STATE_TF_TOWER_COUNT_A, m_towersAlliance);
        }
        else
        {
            --m_towersHorde;
            SendUpdateWorldState(WORLD_STATE_TF_TOWER_COUNT_H, m_towersHorde);
        }
    }

    // update tower state (切换该塔的 UI 世界状态图标)
    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_REMOVE);
    m_towerWorldState[towerId] = newWorldState;
    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_ADD);

    // update capture point owner (更新该塔归属阵营)
    m_towerOwner[towerId] = team;

    // the are no DB exceptions in this case (无需额外 DB 脚本)
    return true;
}

// Handle the zone lock when the timer is activated (全部灵魂塔被单一阵营控制时锁定区域、开启 6 小时倒计时并施加祝福)
void OutdoorPvPTF::LockZone(GameObject* go, uint32 towerId, Team team, uint32 newWorldState)
{
    SendUpdateWorldState(m_zoneWorldState, WORLD_STATE_REMOVE);
    m_zoneWorldState = team == ALLIANCE ? WORLD_STATE_TF_LOCKED_ALLIANCE : WORLD_STATE_TF_LOCKED_HORDE;
    SendUpdateWorldState(m_zoneWorldState, WORLD_STATE_ADD);

    m_zoneLockTimer = TIMER_TF_LOCK_TIME;
    UpdateTimerWorldState();

    m_zoneOwner = team;
    BuffTeam(team, SPELL_AUCHINDOUN_BLESSING);

    // lock the towers (锁定所有灵魂塔占领点使其停止争夺)
    LockTowers(go);

    sWorld.SendDefenseMessage(ZONE_ID_TEROKKAR_FOREST, team == ALLIANCE ? LANG_OPVP_TF_CAPTURE_ALL_TOWERS_A : LANG_OPVP_TF_CAPTURE_ALL_TOWERS_H);

    // remove tower states when zone has been captured and locked (锁定后隐藏单塔世界状态计数)
    for (unsigned int i : m_towerWorldState)
        SendUpdateWorldState(i, WORLD_STATE_REMOVE);

    m_towerWorldState[towerId] = newWorldState;
}

// Handle the zone reset when the timer expires (6 小时锁定期满后重置五座灵魂塔为中立状态并重新开启争夺)
void OutdoorPvPTF::UnlockZone()
{
    // remove buffs (移除占领方的奥金顿祝福)
    BuffTeam(m_zoneOwner, SPELL_AUCHINDOUN_BLESSING, true);

    m_zoneOwner = TEAM_NONE;

    // reset world states and towers (切回非锁定状态 UI)
    SendUpdateWorldState(m_zoneWorldState, WORLD_STATE_REMOVE);
    m_zoneWorldState = WORLD_STATE_TF_TOWERS_CONTROLLED;
    SendUpdateWorldState(m_zoneWorldState, WORLD_STATE_ADD);

    // reset tower states (重置五座塔的世界状态为中立)
    m_towerWorldState[0] = WORLD_STATE_TF_WEST_TOWER_NEUTRAL;
    m_towerWorldState[1] = WORLD_STATE_TF_NORTH_TOWER_NEUTRAL;
    m_towerWorldState[2] = WORLD_STATE_TF_EAST_TOWER_NEUTRAL;
    m_towerWorldState[3] = WORLD_STATE_TF_SOUTH_EAST_TOWER_NEUTRAL;
    m_towerWorldState[4] = WORLD_STATE_TF_SOUTH_TOWER_NEUTRAL;
    for (unsigned int i : m_towerWorldState)
        SendUpdateWorldState(i, WORLD_STATE_ADD);

    // update tower count (双方控制塔数归零)
    m_towersAlliance = 0;
    m_towersHorde = 0;
    SendUpdateWorldState(WORLD_STATE_TF_TOWER_COUNT_A, m_towersAlliance);
    SendUpdateWorldState(WORLD_STATE_TF_TOWER_COUNT_H, m_towersHorde);

    for (GuidZoneMap::const_iterator itr = m_zonePlayers.begin(); itr != m_zonePlayers.end(); ++itr)
    {
        // Find player who is in main zone (Terokkar Forest) to get correct map reference (查找泰罗卡森林主区域内的玩家以获取地图上下文并重置塔滑块)
        if (!itr->second)
            continue;

        if (Player* player = sObjectMgr.GetPlayer(itr->first))
        {
            ResetTowers(player);
            break;
        }
    }
}

// 推进区域锁定倒计时并在到期时解除锁定
void OutdoorPvPTF::Update(uint32 diff)
{
    if (m_zoneLockTimer)
    {
        if (m_zoneLockTimer < diff)
        {
            UnlockZone();
            m_zoneLockTimer = 0;
        }
        else
        {
            // update timer - if OutdoorPvPMgr update timer interval needs to be lowered replace this line with the commented-out ones below
            // (刷新客户端倒计时世界状态；若 OutdoorPvPMgr 更新频率调高可启用下方分频计时器)
            UpdateTimerWorldState();

            /*if (m_zoneUpdateTimer < diff)
            {
                // update timer
                UpdateTimerWorldState();
                m_zoneUpdateTimer = TIMER_TF_UPDATE_TIME;
            }
            else
                m_zoneUpdateTimer -= diff;*/

            m_zoneLockTimer -= diff;
        }
    }
}

void OutdoorPvPTF::UpdateTimerWorldState()
{
    // Calculate time (计算倒计时各位数字并下发世界状态)
    uint32 firstDigit, secondDigit, hoursLeft;
    CalculateTimerWorldStateValues(firstDigit, secondDigit, hoursLeft);

    SendUpdateWorldState(WORLD_STATE_TF_TIME_MIN_FIRST_DIGIT, firstDigit);
    SendUpdateWorldState(WORLD_STATE_TF_TIME_MIN_SECOND_DIGIT, secondDigit);
    SendUpdateWorldState(WORLD_STATE_TF_TIME_HOURS, hoursLeft);
}

void OutdoorPvPTF::CalculateTimerWorldStateValues(uint32& firstDigit, uint32& secondDigit, uint32& hoursLeft)
{
    uint32 minutesLeft = m_zoneLockTimer / 60000;
    hoursLeft = minutesLeft / 60;
    minutesLeft -= hoursLeft * 60;
    firstDigit = minutesLeft / 10;
    secondDigit = minutesLeft - firstDigit * 10;
}

// Handle the Terokkar towers lock during the update timer (锁定五座灵魂塔占领点；若网格未加载则写入滑块持久化状态)
void OutdoorPvPTF::LockTowers(const WorldObject* objRef)
{
    for (uint8 i = 0; i < MAX_TF_TOWERS; ++i)
    {
        if (GameObject* go = objRef->GetMap()->GetGameObject(m_towerBanners[i]))
            go->SetLootState(GO_JUST_DEACTIVATED);
        else
        {
            // if grid is unloaded, changing the saved slider value is enough (网格未加载时直接保存锁定滑块值)
            CapturePointSlider value(m_zoneOwner == ALLIANCE ? CAPTURE_SLIDER_ALLIANCE : CAPTURE_SLIDER_HORDE, true);
            sOutdoorPvPMgr.SetCapturePointSlider(terokkarTowers[i], value);
        }
    }
}

// Handle towers reset when the timer expires (将五座灵魂塔重置为中立并恢复可占领状态)
void OutdoorPvPTF::ResetTowers(const WorldObject* objRef)
{
    for (uint8 i = 0; i < MAX_TF_TOWERS; ++i)
    {
        if (GameObject* go = objRef->GetMap()->GetGameObject(m_towerBanners[i]))
        {
            go->SetCapturePointSlider(CAPTURE_SLIDER_MIDDLE, false);
            // visual update needed because banner still has artkit from previous owner (重置滑块后需手动刷新中立旗帜视觉)
            SetBannerVisual(go, CAPTURE_ARTKIT_NEUTRAL, CAPTURE_ANIM_NEUTRAL);
        }
        else
        {
            // if grid is unloaded, changing the saved slider value is enough (网格未加载时直接更新保存的滑块值)
            CapturePointSlider value(CAPTURE_SLIDER_MIDDLE, false);
            sOutdoorPvPMgr.SetCapturePointSlider(terokkarTowers[i], value);
        }
    }
}
