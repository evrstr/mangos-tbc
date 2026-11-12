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

#include "OutdoorPvPSI.h"
#include "Server/WorldPacket.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Entities/Object.h"
#include "Entities/Creature.h"
#include "Entities/GameObject.h"
#include "Entities/Player.h"
#include "Tools/Language.h"

OutdoorPvPSI::OutdoorPvPSI() : OutdoorPvP(),
    m_resourcesAlliance(0),
    m_resourcesHorde(0),
    m_zoneOwner(TEAM_NONE)
{
}

// Send initial world states (发送希利苏斯水晶尘收集进度初始世界状态)
void OutdoorPvPSI::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    if (sWorld.getConfig(CONFIG_BOOL_OUTDOORPVP_SI_ENABLED))
    {
        FillInitialWorldState(data, count, WORLD_STATE_SI_GATHERED_A, m_resourcesAlliance);
        FillInitialWorldState(data, count, WORLD_STATE_SI_GATHERED_H, m_resourcesHorde);
        FillInitialWorldState(data, count, WORLD_STATE_SI_SILITHYST_MAX, MAX_SILITHYST);
    }
}

// Handle buffs when player enters the zone (玩家进入希利苏斯或安其拉副本时刷新塞纳里奥的青睐增益)
void OutdoorPvPSI::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    // remove the buff from the player first; Sometimes on relog players still have the aura (先移除旧光环避免重连残留)
    player->RemoveAurasDueToSpell(SPELL_CENARION_FAVOR);

    // buff the player if same team is controlling the zone (若己方阵营当前持有希利苏斯控制权则施加增益)
    if (player->GetTeam() == m_zoneOwner)
        player->CastSpell(player, SPELL_CENARION_FAVOR, TRIGGERED_OLD_TRIGGERED);
}

// Remove buffs when player leaves zone (玩家离开区域时移除塞纳里奥的青睐增益)
void OutdoorPvPSI::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    // remove the buff from the player (移除塞纳里奥的青睐光环)
    player->RemoveAurasDueToSpell(SPELL_CENARION_FAVOR);

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

// Handle case when player returns a silithyst (处理玩家将采集到的水晶尘带回阵营营地上交区域触发器)
bool OutdoorPvPSI::HandleAreaTrigger(Player* player, uint32 triggerId)
{
    if (player->IsGameMaster() || player->IsDead())
        return false;

    switch (triggerId)
    {
        case AREATRIGGER_SILITHUS_ALLIANCE:
            if (player->GetTeam() != ALLIANCE || !player->HasAura(SPELL_SILITHYST))
                return false;

            // update counter (累加联盟收集计数并同步世界状态)
            ++ m_resourcesAlliance;
            SendUpdateWorldState(WORLD_STATE_SI_GATHERED_A, m_resourcesAlliance);

            // handle the case when the faction has reached maximum resources allowed (集满 200 份时夺取控制权并重置双方计数)
            if (m_resourcesAlliance == MAX_SILITHYST)
            {
                // NOTE: On retail it would not reset until server restart but we do not support weekly restart :)
                // (注：官服会保持至周重启，此处夺冠后重置双方收集计数以开启新一轮争夺)
                m_zoneOwner = ALLIANCE;
                m_resourcesAlliance = 0;
                m_resourcesHorde = 0;

                // also update the horde counter if resources were reset (同步重置部落计数器显示)
                SendUpdateWorldState(WORLD_STATE_SI_GATHERED_H, m_resourcesHorde);

                // apply buff to owner team (为获胜方全体区域内成员施加塞纳里奥的青睐)
                BuffTeam(ALLIANCE, SPELL_CENARION_FAVOR);

                // Send defense message (广播占领通告)
                sWorld.SendDefenseMessage(ZONE_ID_SILITHUS, LANG_OPVP_SI_CAPTURE_A);
            }

            // give quest credit if necessary (若任务未完成则授予收集任务计数)
            if (player->GetQuestStatus(QUEST_SCOURING_DESERT_ALLIANCE) == QUEST_STATUS_INCOMPLETE)
                player->KilledMonsterCredit(NPC_SILITHUS_DUST_QUEST_ALLIANCE);
            break;
        case AREATRIGGER_SILITHUS_HORDE:
            if (player->GetTeam() != HORDE || !player->HasAura(SPELL_SILITHYST))
                return false;

            // update counter (累加部落收集计数并同步世界状态)
            ++ m_resourcesHorde;
            SendUpdateWorldState(WORLD_STATE_SI_GATHERED_H, m_resourcesHorde);

            // handle the case when the faction has reached maximum resources allowed (集满 200 份时夺取控制权并重置双方计数)
            if (m_resourcesHorde == MAX_SILITHYST)
            {
                // NOTE: On retail it would not reset until server restart but we do not support weekly restart :)
                // (注：官服会保持至周重启，此处夺冠后重置双方收集计数以开启新一轮争夺)
                m_zoneOwner = HORDE;
                m_resourcesAlliance = 0;
                m_resourcesHorde = 0;

                // also update the alliance counter if resources were reset (同步重置联盟计数器显示)
                SendUpdateWorldState(WORLD_STATE_SI_GATHERED_A, m_resourcesAlliance);

                // apply buff to owner team (为获胜方全体区域内成员施加塞纳里奥的青睐)
                BuffTeam(HORDE, SPELL_CENARION_FAVOR);

                // Send defense message (广播占领通告)
                sWorld.SendDefenseMessage(ZONE_ID_SILITHUS, LANG_OPVP_SI_CAPTURE_H);
            }

            // give quest credit if necessary (若任务未完成则授予收集任务计数)
            if (player->GetQuestStatus(QUEST_SCOURING_DESERT_HORDE) == QUEST_STATUS_INCOMPLETE)
                player->KilledMonsterCredit(NPC_SILITHUS_DUST_QUEST_HORDE);
            break;
        default:
            return false;
    }

    // remove silithyst aura (移除携带水晶尘光环)
    player->RemoveAurasDueToSpell(SPELL_SILITHYST);

    // reward the player (发放个人增益、荣誉点数与塞纳里奥议会声望奖励)
    player->CastSpell(player, SPELL_TRACES_OF_SILITHYST, TRIGGERED_OLD_TRIGGERED);
    player->RewardHonor(nullptr, 1, HONOR_REWARD_SILITHYST);
    player->GetReputationMgr().ModifyReputation(sFactionStore.LookupEntry<FactionEntry>(FACTION_CENARION_CIRCLE), REPUTATION_REWARD_SILITHYST);

    return true;
}

// Handle case when player drops flag (处理玩家丢弃或死亡掉落水晶尘；在交旗点附近不生成地面土堆)
// TODO - fix this workaround!
struct SilithusSpawnLocation
{
    float x, y, z;
};
// Area trigger location - workaround to check the flag drop handling (双方交旗点坐标，用于快速判定是否在交旗区附近)
static SilithusSpawnLocation silithusFlagDropLocations[2] =
{
    { -7142.04f, 1397.92f, 4.327f},     // alliance (联盟营地上交点)
    { -7588.48f, 756.806f, -16.425f}    // horde (部落营地上交点)
};

bool OutdoorPvPSI::HandleDropFlag(Player* player, uint32 spellId)
{
    if (spellId != SPELL_SILITHYST)
        return false;

    // don't drop flag at area trigger
    // we are checking distance from the AT hard-coded coordinates because it's much faster than checking the area trigger store
    // (若玩家位于己方交旗触发点 5 码内，则不触发地面掉落)
    switch (player->GetTeam())
    {
        case ALLIANCE:
            if (player->IsWithinDist3d(silithusFlagDropLocations[0].x, silithusFlagDropLocations[0].y, silithusFlagDropLocations[0].z, 5.0f))
                return false;
            break;
        case HORDE:
            if (player->IsWithinDist3d(silithusFlagDropLocations[1].x, silithusFlagDropLocations[1].y, silithusFlagDropLocations[1].z, 5.0f))
                return false;
            break;
        default:
            break;
    }

    // drop the flag in other case (在野外其他位置丢弃则召唤可拾取的水晶尘土堆)
    player->CastSpell(player, SPELL_SILITHYST_FLAG_DROP, TRIGGERED_OLD_TRIGGERED);
    return true;
}

// Handle the case when player picks a silithyst mound or geyser
// This needs to be done because the spells used by these objects are missing
// (处理玩家采集水晶尘土堆或喷泉：因对象关联法术缺失，此处直接施加携带光环并强制对象消失)
bool OutdoorPvPSI::HandleGameObjectUse(Player* player, GameObject* go)
{
    if (go->GetEntry() == GO_SILITHYST_MOUND || go->GetEntry() == GO_SILITHYST_GEYSER)
    {
        // Also mark player with pvp on (施加携带水晶尘光环并开启玩家 PvP 标记)
        player->CastSpell(player, SPELL_SILITHYST, TRIGGERED_OLD_TRIGGERED);
        player->UpdatePvP(true);
        // Despawn the gameobject (workaround) (采完后将对象设为已交互消失状态)
        go->SetLootState(GO_JUST_DEACTIVATED);
        return true;
    }

    return false;
}
