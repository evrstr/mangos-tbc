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

#include "OutdoorPvP.h"
#include "Entities/Object.h"
#include "Entities/GameObject.h"
#include "Entities/Player.h"
#include "Globals/ObjectMgr.h"
#include "World/World.h"

/**
   Function that adds a player to the players of the affected outdoor pvp zones
   (设置野外 PvP 关联墓地的占领阵营，同时同步给世界与对应地图线程)

   @param   player to add
   @param   whether zone is main outdoor pvp zone or a affected zone
 */
void OutdoorPvP::SetGraveYardLinkTeam(uint32 id, uint32 locKey, Team team, uint32 mapId)
{
    sWorld.GetMessager().AddMessage([=](World* world) { world->GetGraveyardManager().SetGraveYardLinkTeam(id, locKey, team); });
    sMapMgr.DoForAllMapsWithMapId(mapId, [=](Map* map) { map->GetMessager().AddMessage([=](Map* map) {map->GetGraveyardManager().SetGraveYardLinkTeam(id, locKey, team); }); });
}

// 记录进入野外 PvP 区域或关联副本的玩家
void OutdoorPvP::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    m_zonePlayers[player->GetObjectGuid()] = isMainZone;
}

/**
   Function that removes a player from the players of the affected outdoor pvp zones
   (将离开野外 PvP 区域或关联副本的玩家从列表移除，并在需要时清理世界状态 UI)

   @param   player to remove
   @param   whether zone is main outdoor pvp zone or a affected zone
 */
void OutdoorPvP::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    if (m_zonePlayers.erase(player->GetObjectGuid()))
    {
        // remove the world state information from the player (仅在主区域离开且非登出时移除世界状态 UI)
        if (isMainZone && !player->GetSession()->PlayerLogout())
            SendRemoveWorldStates(player);

        sLog.outDebug("Player %s left an Outdoor PvP zone", player->GetName());
    }
}

/**
   Function that updates the world state for all the players of the outdoor pvp zone
   (向主区域内的所有在线玩家发送世界状态数值更新)

   @param   world state to update
   @param   new world state value
 */
void OutdoorPvP::SendUpdateWorldState(uint32 field, uint32 value)
{
    for (GuidZoneMap::const_iterator itr = m_zonePlayers.begin(); itr != m_zonePlayers.end(); ++itr)
    {
        // only send world state update to main zone (仅向位于主战区的玩家发送状态更新，跳过关联副本中的玩家)
        if (!itr->second)
            continue;

        if (Player* player = sObjectMgr.GetPlayer(itr->first))
            player->SendUpdateWorldState(field, value);
    }
}

// 游戏对象创建时初始化占领点滑块进度与锁定状态
void OutdoorPvP::HandleGameObjectCreate(GameObject* go)
{
    // set initial data and activate capture points (读取持久化滑块状态或初始化为中立中点)
    if (go->GetGOInfo()->type == GAMEOBJECT_TYPE_CAPTURE_POINT)
    {
        CapturePointSliderMap const* capturePoints = sOutdoorPvPMgr.GetCapturePointSliderMap();
        CapturePointSliderMap::const_iterator itr = capturePoints->find(go->GetEntry());
        if (itr != capturePoints->end())
            go->SetCapturePointSlider(itr->second.Value, itr->second.IsLocked);
        else
            go->SetCapturePointSlider(CAPTURE_SLIDER_MIDDLE, false);
    }
}

// 游戏对象移除/网格卸载时保存占领点滑块进度
void OutdoorPvP::HandleGameObjectRemove(GameObject* go)
{
    // save capture point slider value (negative value if locked) (保存占领点滑块进度及锁定状态，便于网格重载后恢复)
    if (go->GetGOInfo()->type == GAMEOBJECT_TYPE_CAPTURE_POINT)
    {
        CapturePointSlider value(go->GetCapturePointSliderValue(), go->GetLootState() != GO_ACTIVATED);
        sOutdoorPvPMgr.SetCapturePointSlider(go->GetEntry(), value);
    }
}

/**
   Function that handles player kills in the main outdoor pvp zones
   (处理主战区内的玩家击杀：若击杀者在队伍中则向有效距离内的队友分发区域击杀判定)

   @param   player who killed another player
   @param   victim who was killed
 */
void OutdoorPvP::HandlePlayerKill(Player* killer, Player* victim)
{
    if (Group* group = killer->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* groupMember = itr->getSource();

            if (!groupMember)
                continue;

            // skip if too far away (超出小队奖励距离则跳过)
            if (!groupMember->IsAtGroupRewardDistance(victim))
                continue;

            if (victim->IsTrivialForTarget(groupMember))
                continue;

            // creature kills must be notified, even if not inside objective / not outdoor pvp active
            // player kills only count if active and inside objective
            // (仅当队员处于激活的野外 PvP 占领目标范围内时计入区域击杀)
            if (groupMember->CanUseCapturePoint())
                HandlePlayerKillInsideArea(groupMember);
        }
    }
    else
    {
        // creature kills must be notified, even if not inside objective / not outdoor pvp active (单人击杀校验目标激活状态与有效等级差)
        if (killer && killer->CanUseCapturePoint() && !victim->IsTrivialForTarget(killer))
            HandlePlayerKillInsideArea(killer);
    }
}

// apply a team buff for the main and affected zones (为主战区及关联副本内指定阵营的所有玩家施加或移除增益光环，投递到对应地图线程执行)
void OutdoorPvP::BuffTeam(Team team, uint32 spellId, bool remove /*= false*/)
{
    for (GuidZoneMap::const_iterator itr = m_zonePlayers.begin(); itr != m_zonePlayers.end(); ++itr)
    {
        Player* player = sObjectMgr.GetPlayer(itr->first);
        if (player && player->GetTeam() == team)
        {
            if (remove)
            {
                ObjectGuid guid = player->GetObjectGuid();
                player->GetMap()->GetMessager().AddMessage([guid, spellId](Map* map) -> void
                {
                    if (Player* player = map->GetPlayer(guid))
                        player->RemoveAurasDueToSpell(spellId);
                });
            }
            else
            {
                ObjectGuid guid = player->GetObjectGuid();
                player->GetMap()->GetMessager().AddMessage([guid, spellId](Map* map) -> void
                {
                    if(Player* player = map->GetPlayer(guid))
                        player->CastSpell(player, spellId, TRIGGERED_OLD_TRIGGERED);
                });
            }
        }
    }
}

// 根据占领阵营返回对应的旗帜 ArtKit
uint32 OutdoorPvP::GetBannerArtKit(Team team, uint32 artKitAlliance /*= CAPTURE_ARTKIT_ALLIANCE*/, uint32 artKitHorde /*= CAPTURE_ARTKIT_HORDE*/, uint32 artKitNeutral /*= CAPTURE_ARTKIT_NEUTRAL*/) const
{
    switch (team)
    {
        case ALLIANCE:
            return artKitAlliance;
        case HORDE:
            return artKitHorde;
        default:
            return artKitNeutral;
    }
}

// 通过参考对象定位目标旗帜并更新其 ArtKit 与动画
void OutdoorPvP::SetBannerVisual(const WorldObject* objRef, ObjectGuid goGuid, uint32 artKit, uint32 animId)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(goGuid))
        SetBannerVisual(go, artKit, animId);
}

// 播放旗帜自定义动画并刷新 ArtKit 外观
void OutdoorPvP::SetBannerVisual(GameObject* go, uint32 artKit, uint32 animId)
{
    go->SendGameObjectCustomAnim(go->GetObjectGuid(), animId);
    go->SetGoArtKit(artKit);
    go->Refresh();
}

// 刷新或强制消失指定的游戏对象
void OutdoorPvP::RespawnGO(const WorldObject* objRef, ObjectGuid goGuid, bool respawn)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(goGuid))
    {
        go->SetRespawnTime(7 * DAY);

        if (respawn)
            go->Refresh();
        else if (go->IsSpawned())
            go->ForcedDespawn();
    }
}
