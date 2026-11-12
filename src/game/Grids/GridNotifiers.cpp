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

#include "Grids/GridNotifiers.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "Entities/UpdateData.h"
#include "Maps/MapPersistentStateMgr.h"
#include "Maps/Map.h"
#include "Entities/Transports.h"
#include "Globals/ObjectAccessor.h"
#include "BattleGround/BattleGroundMgr.h"
#include "AI/BaseAI/UnitAI.h"

using namespace MaNGOS;

void VisibleChangesNotifier::Visit(CameraMapType& m)
{
    for (auto& iter : m)
    {
        if (iter.getSource()->IsSendInProgress() || iter.getSource()->GetOwner()->HasAtClient(&i_object))
            continue;
        UpdateData data;
        iter.getSource()->UpdateVisibilityOf(&i_object, data);
        m_unvisitedGuids.erase(iter.getSource()->GetOwner()->GetObjectGuid());
        data.SendData(*iter.getSource()->GetOwner()->GetSession());
    }
}

void VisibleNotifier::Notify()
{
    Player& player = *i_camera.GetOwner();
#ifdef ENABLE_PLAYERBOTS
    if (!player.isRealPlayer())
        return;
#endif

    // at this moment i_clientGUIDs have guids that not iterate at grid level checks
    // but exist one case when this possible and object not out of range: transports
    // 此时 i_clientGUIDs 中剩余的是未在普通网格遍历中访问到的 GUID；但传送载具上的乘客虽未落在同网格却并未超出视野，需单独处理
    if (GenericTransport* transport = player.GetTransport())
    {
        for (auto itr : transport->GetPassengers())
        {
            if (i_clientGUIDs.find(itr->GetObjectGuid()) != i_clientGUIDs.end())
            {
                // ignore far sight case | 忽略千里眼等远视切换情况
                if (itr->IsPlayer())
                {
                    UpdateData data;
                    static_cast<Player*>(itr)->UpdateVisibilityOf(static_cast<Player*>(itr), &player, data);
                    data.SendData(*static_cast<Player*>(itr)->GetSession());
                }
                player.UpdateVisibilityOf(&player, itr, i_data, i_visibleNow);
                i_clientGUIDs.erase(itr->GetObjectGuid());
            }
        }
    }

    // Far objects update on player notify | 玩家视野更新时同步刷新标记为覆盖视野距离的远距对象
    for (GuidSet::iterator itr = i_clientGUIDs.begin(); itr != i_clientGUIDs.end();)
    {
        GuidSet::iterator current = itr++;
        if (WorldObject* obj = player.GetMap()->GetWorldObject(*current))
        {
            if (!obj->GetVisibilityData().IsVisibilityOverridden())
                continue;

            player.UpdateVisibilityOf(&player, obj, i_data);
            i_clientGUIDs.erase(current);
        }
    }

    for (GuidSet::iterator itr = i_clientGUIDs.begin(); itr != i_clientGUIDs.end();)
    {
        if ((*itr).IsMOTransport())
        {
            itr = i_clientGUIDs.erase(itr);
            continue;
        }
        ++itr;
    }

    if (player.IsPendingPhaseChange())
    {
        player.GetMap()->UpdateInfinite(player, i_data, i_clientGUIDs, i_visibleNow);
        player.RemovePendingPhaseChange();
    }

    // generate outOfRange for not iterate objects | 为仍留在 i_clientGUIDs 中的超距对象生成离开视野（OutOfRange）更新包
    i_data.AddOutOfRangeGUID(i_clientGUIDs);
    for (GuidSet::iterator itr = i_clientGUIDs.begin(); itr != i_clientGUIDs.end(); ++itr)
    {
        if (WorldObject* target = player.GetMap()->GetWorldObject(*itr))
        {
            if (target->GetTypeId() == TYPEID_UNIT)
                player.BeforeVisibilityDestroy(static_cast<Creature*>(target));
            player.RemoveAtClient(target);
        }
        else
            sLog.outCustomLog("Object was %s in current map.", player.GetMap()->m_objRemoveList.find(*itr) == player.GetMap()->m_objRemoveList.end() ? "not found" : "found");
        

        DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "%s is out of range (no in active cells set) now for %s",
                         itr->GetString().c_str(), player.GetGuidStr().c_str());
    }

    if (i_data.HasData())
    {
        // send create/outofrange packet to player (except player create updates that already sent using SendUpdateToPlayer)
        // 向玩家发送创建/离开视野数据包（通过 SendUpdateToPlayer 单独发送的自身创建包除外）
        if (i_processSend)
            i_data.SendData(*player.GetSession());
    }

    // Now do operations that required done at object visibility change to visible
    // 以下处理对象刚进入玩家视野时的后续同步操作

    // send data at target visibility change (adding to client) | 对象刚添加到客户端时补发其当前光环状态
    if (i_processSend)
    {
        for (auto vItr : i_visibleNow)
        {
            // target aura duration for caster show only if target exist at caster client
            // 仅当目标已存在于客户端时，光环持续时间才能正确显示
            if (vItr != &player && vItr->isType(TYPEMASK_UNIT))
                player.SendAuraDurationsForTarget((Unit*)vItr);
        }
    }
    else
        for (auto vItr : i_visibleNow)
            if (vItr != &player && vItr->isType(TYPEMASK_UNIT))
            {
                auto auraPackets = Player::BuildAurasForTarget(player, static_cast<Unit const&>(*vItr));
                for (auto& auraPacket : auraPackets)
                {
                    i_data.AddAfterCreatePacket(auraPacket);
                }
            }
}

void MessageDeliverer::Visit(CameraMapType& m)
{
    for (auto& iter : m)
    {
        Player* owner = iter.getSource()->GetOwner();

        if (i_toSelf || owner != &i_player)
        {
            if (WorldSession* session = owner->GetSession())
                session->SendPacket(i_message);
        }
    }
}

void MessageDelivererExcept::Visit(CameraMapType& m)
{
    for (auto& iter : m)
    {
        Player* owner = iter.getSource()->GetOwner();

        if (owner == i_skipped_receiver)
            continue;

        if (WorldSession* session = owner->GetSession())
            session->SendPacket(i_message);
    }
}

void ObjectMessageDeliverer::Visit(CameraMapType& m)
{
    for (auto& iter : m)
    {
        if (WorldSession* session = iter.getSource()->GetOwner()->GetSession())
            session->SendPacket(i_message);
    }
}

void MessageDistDeliverer::Visit(CameraMapType& m)
{
    for (auto& iter : m)
    {
        Player* owner = iter.getSource()->GetOwner();

        if ((i_toSelf || owner != &i_player) &&
                (!i_ownTeamOnly || owner->GetTeam() == i_player.GetTeam()) &&
                (!i_dist || iter.getSource()->GetBody()->IsWithinDist(&i_player, i_dist)))
        {
            if (WorldSession* session = owner->GetSession())
                session->SendPacket(i_message);
        }
    }
}

void ObjectMessageDistDeliverer::Visit(CameraMapType& m)
{
    for (auto& iter : m)
    {
        if (!i_dist || iter.getSource()->GetBody()->IsWithinDist(&i_object, i_dist))
        {
            if (WorldSession* session = iter.getSource()->GetOwner()->GetSession())
                session->SendPacket(i_message);
        }
    }
}

template<class T>
void ObjectUpdater::Visit(GridRefManager<T>& m)
{
    for (auto& iter : m)
        m_objectToUpdateSet.emplace(iter.getSource());
}

bool CannibalizeObjectCheck::operator()(Corpse* u)
{
    // ignore bones | 忽略已化为白骨的尸体
    if (u->GetType() == CORPSE_BONES)
        return false;

    Player* owner = ObjectAccessor::FindPlayer(u->GetOwnerGuid());

    if (!owner || i_fobj->CanAssist(owner))
        return false;

    return i_fobj->IsWithinDistInMap(u, i_range);
}

bool TauntFlagObjectCheck::operator()(Corpse* u)
{
    // ignore bones | 忽略已化为白骨的尸体
    if (u->GetType() == CORPSE_BONES)
        return false;

    Player* owner = ObjectAccessor::FindPlayer(u->GetOwnerGuid());

    if (!owner || i_fobj->CanAssist(owner))
        return false;

    return i_fobj->IsWithinDistInMap(u, i_range);
}

void MaNGOS::RespawnDo::operator()(Creature* u) const
{
    // prevent respawn creatures for not active BG event | 战场/竞技场中禁止复活未激活事件对应的生物
    Map* map = u->GetMap();
    if (map->IsBattleGroundOrArena())
    {
        BattleGroundEventIdx eventId = map->GetMapDataContainer().GetCreatureEventIndex(u->GetDbGuid());
        if (!((BattleGroundMap*)map)->GetBG()->IsActiveEvent(eventId.event1, eventId.event2))
            return;
    }

    if (u->IsUsingNewSpawningSystem())
    {
        if (u->IsDead() && !u->GetCreatureGroup())
        {
            if (u->GetMap()->GetMapDataContainer().GetSpawnGroupByGuid(u->GetDbGuid(), TYPEID_UNIT))
                u->GetMap()->GetPersistentState()->SaveCreatureRespawnTime(u->GetDbGuid(), time(nullptr));
            else
                u->GetMap()->GetSpawnManager().RespawnCreature(u->GetDbGuid(), 0);
        }
    }
    else
        u->Respawn();
}

void MaNGOS::RespawnDo::operator()(GameObject* u) const
{
    // prevent respawn gameobject for not active BG event | 战场/竞技场中禁止刷新未激活事件对应的游戏对象
    Map* map = u->GetMap();
    if (map->IsBattleGroundOrArena())
    {
        BattleGroundEventIdx eventId = map->GetMapDataContainer().GetGameObjectEventIndex(u->GetDbGuid());
        if (!static_cast<BattleGroundMap*>(map)->GetBG()->IsActiveEvent(eventId.event1, eventId.event2))
            return;
    }

    u->Respawn();
}

void MaNGOS::CallOfHelpCreatureInRangeDo::operator()(Creature* u)
{
    if (u == i_funit)
        return;

    if (!u->CanAssist(i_funit) || !u->CanAttack(i_enemy))
        return;

    // too far | 超出呼救半径
    if (!i_funit->IsWithinDistInMap(u, i_range))
        return;

    // only if see assisted creature | 仅在双方具备视线时响应呼救
    if (!i_funit->IsWithinLOSInMap(u))
        return;

    if (u->AI())
        u->AI()->OnCallForHelp(i_enemy);
}

bool MaNGOS::AnyAssistCreatureInRangeCheck::operator()(Creature* u)
{
    if (u == i_funit)
        return false;

    if (!u->CanAssistInCombatAgainst(i_funit, i_enemy))
        return false;

    // too far | 超出范围
    if (!i_funit->IsWithinDistInMap(u, i_range))
        return false;

    // only if see assisted creature | 仅当可见被协助单位时成立
    if (!i_funit->IsWithinLOSInMap(u))
        return false;

    return true;
}

template void ObjectUpdater::Visit<GameObject>(GameObjectMapType&);
template void ObjectUpdater::Visit<DynamicObject>(DynamicObjectMapType&);
