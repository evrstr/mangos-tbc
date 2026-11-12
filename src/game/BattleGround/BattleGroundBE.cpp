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

#include "Entities/Object.h"
#include "Entities/Player.h"
#include "BattleGround.h"
#include "BattleGroundBE.h"
#include "Server/WorldPacket.h"
#include "Tools/Language.h"
#include "World/WorldStateDefines.h"
#include "World/WorldStateVariableManager.h"

BattleGroundBE::BattleGroundBE()
{
    m_startDelayTimes[BG_STARTING_EVENT_FIRST]  = BG_START_DELAY_1M;
    m_startDelayTimes[BG_STARTING_EVENT_SECOND] = BG_START_DELAY_30S;
    m_startDelayTimes[BG_STARTING_EVENT_THIRD]  = BG_START_DELAY_15S;
    m_startDelayTimes[BG_STARTING_EVENT_FOURTH] = BG_START_DELAY_NONE;

    // we must set messageIds (设置开战倒计时各阶段公告文本 ID)
    m_startMessageIds[BG_STARTING_EVENT_FIRST]  = LANG_ARENA_ONE_MINUTE;
    m_startMessageIds[BG_STARTING_EVENT_SECOND] = LANG_ARENA_THIRTY_SECONDS;
    m_startMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_ARENA_FIFTEEN_SECONDS;
    m_startMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_ARENA_HAS_BEGUN;
}

// 重置刀锋山竞技场状态并注册存活人数 HUD 世界状态变量
void BattleGroundBE::Reset()
{
    // call parent's class reset (调用基类重置)
    BattleGround::Reset();

    GetBgMap()->GetVariableManager().SetVariable(WORLD_STATE_ARENA_BE_HUD_ENABLED, 1);
    GetBgMap()->GetVariableManager().SetVariableData(WORLD_STATE_ARENA_BE_HUD_ENABLED, true, 0, 0);
    GetBgMap()->GetVariableManager().SetVariableData(WORLD_STATE_ARENA_BE_ALLIANCE_ALIVE, true, 0, 0);
    GetBgMap()->GetVariableManager().SetVariableData(WORLD_STATE_ARENA_BE_HORDE_ALIVE, true, 0, 0);
}

// 开战时打开双方准备室大门
void BattleGroundBE::StartingEventOpenDoors()
{
    OpenDoorEvent(BG_EVENT_DOOR);
}

// 玩家进场：登记积分项并更新界面双方存活人数
void BattleGroundBE::AddPlayer(Player* plr)
{
    BattleGround::AddPlayer(plr);
    // create score and add it to map, default values are set in constructor (创建积分对象并加入记分表)
    BattleGroundBEScore* sc = new BattleGroundBEScore;

    // Needed for scoreboard if player leaves. (记录竞技场所属阵营以便离场后记分牌仍能正确归属)
    sc->Team = plr->GetBGTeam();

    m_playerScores[plr->GetObjectGuid()] = sc;

    GetBgMap()->GetVariableManager().SetVariable(WORLD_STATE_ARENA_BE_ALLIANCE_ALIVE, GetAlivePlayersCountByTeam(ALLIANCE));
    GetBgMap()->GetVariableManager().SetVariable(WORLD_STATE_ARENA_BE_HORDE_ALIVE, GetAlivePlayersCountByTeam(HORDE));
}

// 玩家离场时刷新双方存活人数并判定胜负条件
void BattleGroundBE::RemovePlayer(Player* /*plr*/, ObjectGuid /*guid*/)
{
    if (GetStatus() == STATUS_WAIT_LEAVE)
        return;

    GetBgMap()->GetVariableManager().SetVariable(WORLD_STATE_ARENA_BE_ALLIANCE_ALIVE, GetAlivePlayersCountByTeam(ALLIANCE));
    GetBgMap()->GetVariableManager().SetVariable(WORLD_STATE_ARENA_BE_HORDE_ALIVE, GetAlivePlayersCountByTeam(HORDE));

    CheckArenaWinConditions();
}

// 玩家被击杀时更新存活人数世界状态并检测竞技场胜负
void BattleGroundBE::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    if (!killer)
    {
        sLog.outError("Killer player not found");
        return;
    }

    BattleGround::HandleKillPlayer(player, killer);

    GetBgMap()->GetVariableManager().SetVariable(WORLD_STATE_ARENA_BE_ALLIANCE_ALIVE, GetAlivePlayersCountByTeam(ALLIANCE));
    GetBgMap()->GetVariableManager().SetVariable(WORLD_STATE_ARENA_BE_HORDE_ALIVE, GetAlivePlayersCountByTeam(HORDE));

    CheckArenaWinConditions();
}

// 玩家掉出刀锋山竞技场地图下方时传回场地中央安全坐标
bool BattleGroundBE::HandlePlayerUnderMap(Player* player)
{
    player->TeleportTo(GetMapId(), 6238.930176f, 262.963470f, 0.889519f, player->GetOrientation());
    return true;
}

void BattleGroundBE::UpdatePlayerScore(Player* source, uint32 type, uint32 value)
{
    BattleGroundScoreMap::iterator itr = m_playerScores.find(source->GetObjectGuid());
    if (itr == m_playerScores.end())                        // player not found... (未找到玩家记分记录)
        return;

    // there is nothing special in this score (直接委托基类更新通用统计字段)
    BattleGround::UpdatePlayerScore(source, type, value);
}
