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
#include "BattleGroundWS.h"
#include "Entities/GameObject.h"
#include "Globals/ObjectMgr.h"
#include "BattleGroundMgr.h"
#include "Server/WorldPacket.h"

BattleGroundWS::BattleGroundWS() : m_reputationCapture(0), m_honorWinKills(0), m_honorEndKills(0)
{
    // set battleground start message ids (配置战歌峡谷开战前各阶段倒计时公告文本 ID)
    m_startMessageIds[BG_STARTING_EVENT_FIRST]  = 0;
    m_startMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_WS_START_ONE_MINUTE;
    m_startMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_BG_WS_START_HALF_MINUTE;
    m_startMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_WS_HAS_BEGUN;
}

// 每帧更新战歌峡谷旗帜重生计时器、地面掉落归位计时器及僵持易伤叠加计时器
void BattleGroundWS::Update(uint32 diff)
{
    BattleGround::Update(diff);

    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    // compute flag respawn timers (推进双方战旗的基地重刷与掉落归位计时)
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        // respawn flag after team score (交付得分后等待 23 秒在基地重新刷旗)
        if (m_flagOnRespawn[i])
        {
            if (m_flagsTimer[i] <= diff)
            {
                Team playerTeam = GetTeamIdByTeamIndex((PvpTeamIndex)i);
                RespawnFlagAtBase(playerTeam, true);
                m_flagsTimer[i] = 0;
            }
            else
                m_flagsTimer[i] -= diff;
        }
        // respawn flag after dropped (战旗掉落在地面 10 秒无人拾取则自动归位)
        else if (GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[i]) == BG_WS_FLAG_STATE_ON_GROUND && m_flagsDropTimer[i])
        {
            if (m_flagsDropTimer[i] <= diff)
            {
                Team playerTeam = GetTeamIdByTeamIndex((PvpTeamIndex)i);
                m_flagsDropTimer[i] = 0;
                RespawnDroppedFlag(playerTeam);
            }
            else
                m_flagsDropTimer[i] -= diff;
        }
    }
    // as long as both flags are missing from bases, progress the FC debuff timer until 15 minutes have elapsed
    // (当双方旗帜均不在己方基地时，累计僵持计时并在 10/15 分钟为双方旗手施加集中/残暴攻击易伤)
    if (GetBgMap()->GetVariableManager().GetVariable(BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_ALLIANCE) != BG_WS_FLAG_STATE_ON_BASE
        && GetBgMap()->GetVariableManager().GetVariable(BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_HORDE) != BG_WS_FLAG_STATE_ON_BASE)
    {
        if (m_flagCarrierDebuffTimer)
        {
            if (m_flagCarrierDebuffTimer <= BG_WS_FOCUSED_ASSAULT_TIME && !m_focusedAssaultActive)
            {
                for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
                {
                    if (IsFlagPickedUp((PvpTeamIndex)i))
                        if (Player* player = GetBgMap()->GetPlayer(GetFlagCarrierGuid((PvpTeamIndex)i)))
                            player->CastSpell(player, BG_WS_SPELL_FOCUSED_ASSAULT, TRIGGERED_OLD_TRIGGERED);
                }
                m_focusedAssaultActive = true;
            }
            if (m_flagCarrierDebuffTimer <= diff)
            {
                for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
                {
                    if (IsFlagPickedUp((PvpTeamIndex)i))
                    {
                        if (Player* player = GetBgMap()->GetPlayer(GetFlagCarrierGuid((PvpTeamIndex)i)))
                        {
                            player->RemoveAurasDueToSpell(BG_WS_SPELL_FOCUSED_ASSAULT);
                            player->CastSpell(player, BG_WS_SPELL_BRUTAL_ASSAULT, TRIGGERED_OLD_TRIGGERED);
                        }
                    }
                }
                m_focusedAssaultActive = false;
                m_brutalAssaultActive = true;
                m_flagCarrierDebuffTimer = 0; // stop timer here for now - no reason to resume until the current stalemate is over and another begins (capture occurs, or both flags are returned?) (15 分钟已到，暂停计时直至本轮僵持打破)
            }
            else
                m_flagCarrierDebuffTimer -= diff;
        }
    }
}

// 开战时打开准备室大门、刷出灵魂向导与双方战旗并切换室外墓地链接
void BattleGroundWS::StartingEventOpenDoors()
{
    OpenDoorEvent(BG_EVENT_DOOR);

    // TODO implement timer to despawn doors after a short while (待补充：开战一段时间后清理大门对象)

    SpawnEvent(WS_EVENT_SPIRITGUIDES_SPAWN, 0, true);
    SpawnEvent(WS_EVENT_FLAG_A, 0, true);
    SpawnEvent(WS_EVENT_FLAG_H, 0, true);

    // setup graveyards (开战后禁用旗室墓地，启用双方主墓地)
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_MAIN_ALLIANCE,     BG_WS_ZONE_ID_MAIN, ALLIANCE);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_MAIN_HORDE,        BG_WS_ZONE_ID_MAIN, HORDE);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_FLAGROOM_ALLIANCE, BG_WS_ZONE_ID_MAIN, TEAM_INVALID);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_FLAGROOM_HORDE,    BG_WS_ZONE_ID_MAIN, TEAM_INVALID);
}

// 玩家进场时创建战歌峡谷个人战绩条目
void BattleGroundWS::AddPlayer(Player* player)
{
    BattleGround::AddPlayer(player);

    // create score and add it to map, default values are set in constructor (初始化玩家夺旗/护旗统计)
    BattleGroundWGScore* score = new BattleGroundWGScore;

    m_playerScores[player->GetObjectGuid()] = score;
}

// Respawn the flag at the base (在基地重新刷出战旗；若由交旗得分触发则同时重置双方僵持易伤状态)
void BattleGroundWS::RespawnFlagAtBase(Team team, bool wasCaptured)
{
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);
    GetBgMap()->GetVariableManager().SetVariable(wsFlagPickedUp[teamIdx], BG_WS_FLAG_STATE_ON_BASE);
    SpawnEvent(teamIdx, 0, true);
    m_flagOnRespawn[teamIdx] = false;

    // if flag was captured process the following events (若因成功夺旗得分而重新刷旗，重置易伤计时并重新刷出双旗)
    if (wasCaptured)
    {
        DEBUG_LOG("BattleGroundWS: The main flag for team %u has respawned after enemy score.", team);

        m_flagCarrierDebuffTimer = BG_WS_BRUTAL_ASSAULT_TIME;
        m_brutalAssaultActive = false;
        m_focusedAssaultActive = false;

        // when map_update will be allowed for battlegrounds this code will be useless
        SpawnEvent(WS_EVENT_FLAG_A, 0, true);
        SpawnEvent(WS_EVENT_FLAG_H, 0, true);

        SendMessageToAll(LANG_BG_WS_F_PLACED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
        PlaySoundToAll(BG_WS_SOUND_FLAGS_RESPAWNED);        // flag respawned sound... (播放战旗重新放置音效)
    }
    // if both team flags have been returned, reset FC debuffs like a capture would (TODO: confirm this and remove if required)
    // (若双方战旗均已回到各自底座，同样重置僵持易伤计时)
    else if (GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[otherTeamIdx]) == BG_WS_FLAG_STATE_ON_BASE)
    {
        m_flagCarrierDebuffTimer = BG_WS_BRUTAL_ASSAULT_TIME;
        m_brutalAssaultActive = false;
        m_focusedAssaultActive = false;
    }
}

// Respawn the flag at the base (地面掉落战旗超时后强制消失并归位到基地)
void BattleGroundWS::RespawnDroppedFlag(Team team)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    DEBUG_LOG("BattleGroundWS: The dropped flag for team %u has respawned.", team);

    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);

    // respawn the flag at the base (将战旗重置回基地)
    RespawnFlagAtBase(team, false);

    // inform players (广播战旗归位公告与音效)
    SendMessageToAll(wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_RESPAWN].messageId, wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_RESPAWN].chatType);
    PlaySoundToAll(wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_RESPAWN].soundId);

    // despawn the temp summoned flag (移除地面的临时掉落战旗对象)
    if (GameObject* flag = GetBgMap()->GetGameObject(GetDroppedFlagGuid(team)))
        flag->SetLootState(GO_JUST_DEACTIVATED);

    ClearDroppedFlagGuid(team);
}

int32 BattleGroundWS::GetFlagState(Team team)
{
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    return GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[teamIdx]);
}

// Method that process captured flag score (处理玩家携带敌方战旗回到己方旗室成功交旗得分的完整流程)
void BattleGroundWS::ProcessPlayerFlagScoreEvent(Player* player)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    DEBUG_LOG("BattleGroundWS: Team %u has scored.", player->GetTeam());

    player->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_PVP_ACTIVE_CANCELS);

    Team team = player->GetTeam();
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    uint8 otherTeamIdx = GetOtherTeamIndex(teamIdx);

    if (!IsFlagPickedUp(otherTeamIdx))
        return;

    ClearFlagCarrier(otherTeamIdx); // must be before aura remove to prevent 2 events (drop+capture) at the same time (必须先清除持旗者记录再移除光环，避免同时触发掉旗与得分事件)

    // Horde flag in base (but not respawned yet) (标记敌方战旗已回到基地等待 23 秒重刷)
    GetBgMap()->GetVariableManager().SetVariable(wsFlagPickedUp[otherTeamIdx], BG_WS_FLAG_STATE_ON_BASE);
    m_flagOnRespawn[otherTeamIdx] = true;

    // Drop Horde Flag from Player (移除玩家身上的持旗与僵持易伤光环)
    player->RemoveAurasDueToSpell(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_PICKEDUP].spellId);
    player->RemoveAurasDueToSpell(BG_WS_SPELL_FOCUSED_ASSAULT);
    player->RemoveAurasDueToSpell(BG_WS_SPELL_BRUTAL_ASSAULT);

    // play sound and reward reputation (播放夺旗音效并向全队发放声望与荣誉)
    PlaySoundToAll(wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_CAPTURED].soundId);
    RewardReputationToTeam(team == ALLIANCE ? 890 : 889, m_reputationCapture, team);

    // for flag capture is reward 2 honorable kills (每次夺旗全队奖励 2 个荣誉击杀等值荣誉)
    RewardHonorToTeam(GetBonusHonorFromKill(2), team);

    // update score (累加己方夺旗总分并隐藏顶部持旗图标)
    int32 pointsWorldState = team == ALLIANCE ? BG_WS_STATE_CAPTURES_ALLIANCE : BG_WS_STATE_CAPTURES_HORDE;
    GetBgMap()->GetVariableManager().SetVariable(pointsWorldState, GetBgMap()->GetVariableManager().GetVariable(pointsWorldState) + 1);

    GetBgMap()->GetVariableManager().SetVariable(wsFlagPickedUp[teamIdx], BG_WS_FLAG_STATE_ON_BASE);
    GetBgMap()->GetVariableManager().SetVariable(wsFlagHUDPickedUp[teamIdx], BG_WS_FLAG_ICON_INVISIBLE);

    // despawn flags (暂时隐藏双方基地旗帜等待重新放置)
    SpawnEvent(WS_EVENT_FLAG_A, 0, false);
    SpawnEvent(WS_EVENT_FLAG_H, 0, false);

    SendMessageToAll(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_CAPTURED].messageId, wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_CAPTURED].chatType, player);

    // only flag capture should be updated (更新个人夺旗次数统计)
    UpdatePlayerScore(player, SCORE_FLAG_CAPTURES, 1);      // +1 flag captures

    // Process match winner (检查是否已率先夺得 3 次旗帜)
    Team winner = GetBgMap()->GetVariableManager().GetVariable(pointsWorldState) == BG_WS_MAX_TEAM_SCORE ? GetTeamIdByTeamIndex(teamIdx) : TEAM_NONE;

    // end battleground or set respawn timer (达成 3 分立即结束战场，否则启动 23 秒刷旗倒计时)
    if (winner)
    {
        GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_ALLIANCE, 0);
        GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_HORDE, 0);
        GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_FLAG_ALLIANCE, BG_WS_FLAG_ICON_INVISIBLE);
        GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_FLAG_HORDE, BG_WS_FLAG_ICON_INVISIBLE);

        EndBattleGround(winner);
    }
    else
        m_flagsTimer[otherTeamIdx] = BG_WS_FLAG_RESPAWN_TIME;
}

// Method that handles the flag drop (处理玩家死亡、离场或手动取消光环导致的丢旗逻辑)
void BattleGroundWS::HandlePlayerDroppedFlag(Player* player)
{
    DEBUG_LOG("BattleGroundWS: Team %u has dropped the flag.", player->GetTeam());

    Team team = player->GetTeam();
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);

    player->RemoveAurasDueToSpell(BG_WS_SPELL_FOCUSED_ASSAULT);
    player->RemoveAurasDueToSpell(BG_WS_SPELL_BRUTAL_ASSAULT);

    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        if (IsFlagPickedUp(teamIdx) && GetFlagCarrierGuid(teamIdx) == player->GetObjectGuid())
        {
            ClearFlagCarrier(teamIdx);
            player->RemoveAurasDueToSpell(wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_PICKEDUP].spellId);
        }
    }
    // Check if the other team has picked the flag and if the flag carrier stored is the player which triggers this function
    // (校验该玩家确为敌方战旗当前持有者后，在地面生成临时战旗并启动 10 秒归位倒计时)
    else if (IsFlagPickedUp(otherTeamIdx) && GetFlagCarrierGuid(otherTeamIdx) == player->GetObjectGuid())
    {
        ClearFlagCarrier(otherTeamIdx);
        player->RemoveAurasDueToSpell(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_PICKEDUP].spellId);

        GetBgMap()->GetVariableManager().SetVariable(wsFlagPickedUp[otherTeamIdx], BG_WS_FLAG_STATE_ON_GROUND);
        player->CastSpell(player, wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_DROPPED].spellId, TRIGGERED_OLD_TRIGGERED);

        player->CastSpell(player, SPELL_RECENTLY_DROPPED_FLAG, TRIGGERED_OLD_TRIGGERED);
        GetBgMap()->GetVariableManager().SetVariable(wsFlagHUDPickedUp[teamIdx], BG_WS_FLAG_ICON_INVISIBLE);

        SendMessageToAll(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_DROPPED].messageId, wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_DROPPED].chatType, player);

        m_flagsDropTimer[otherTeamIdx] = BG_WS_FLAG_DROP_TIME;
    }
}

bool BattleGroundWS::IsFlagHeldFor45Seconds(Team flagHolderTeam)
{
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(flagHolderTeam);
    PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);
    return m_flagPickupFromBaseTime[otherTeamIdx] + std::chrono::seconds(45) < GetBgMap()->GetCurrentClockTime();
}

// Function that handles the flag pick up from the base (处理玩家从敌方基地拔起战旗)
void BattleGroundWS::ProcessFlagPickUpFromBase(Player* player, Team attackerTeam)
{
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(attackerTeam);
    PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);

    if (GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[otherTeamIdx]) != BG_WS_FLAG_STATE_ON_BASE || m_flagOnRespawn[otherTeamIdx])
        return;

    DEBUG_LOG("BattleGroundWS: Team %u has taken the enemy flag.", attackerTeam);

    SpawnEvent(otherTeamIdx, 0, false);
    SetFlagCarrier(otherTeamIdx, player->GetObjectGuid());
    GetBgMap()->GetVariableManager().SetVariable(wsFlagPickedUp[otherTeamIdx], BG_WS_FLAG_STATE_ON_PLAYER);
    GetBgMap()->GetVariableManager().SetVariable(wsFlagHUDPickedUp[teamIdx], BG_WS_FLAG_ICON_VISIBLE);
    m_flagPickupFromBaseTime[otherTeamIdx] = GetBgMap()->GetCurrentClockTime();

    // not meant to be a triggered cast - clears a lot during cast like stealth
    // (若当前已激活僵持易伤，立即为新旗手补上对应等级的易伤光环)
    if (m_brutalAssaultActive)
        player->CastSpell(player, BG_WS_SPELL_BRUTAL_ASSAULT, TRIGGERED_OLD_TRIGGERED);
    else if (m_focusedAssaultActive)
        player->CastSpell(player, BG_WS_SPELL_FOCUSED_ASSAULT, TRIGGERED_OLD_TRIGGERED);

    PlaySoundToAll(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_PICKEDUP].soundId);
    SendMessageToAll(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_PICKEDUP].messageId, wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_PICKEDUP].chatType, player);
    player->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_PVP_ACTIVE_CANCELS);
}

// Function that handles the click action on the dropped flag (处理玩家点击地面掉落的战旗：己方点旗立即归位，敌方点旗重新拾起)
void BattleGroundWS::ProcessDroppedFlagActions(Player* player, GameObject* target)
{
    Team team = player->GetTeam();
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);

    int32 actionId = BG_WS_FLAG_ACTION_NONE;

    // check if we are returning our flag (点击己方掉落的战旗：立即归还基地并记护旗次数)
    if (wsDroppedFlagId[teamIdx] == target->GetEntry())
    {
        if (GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[teamIdx]) != BG_WS_FLAG_STATE_ON_GROUND)
            return;

        DEBUG_LOG("BattleGroundWS: Team %u has returned the dropped flag %u.", player->GetTeam(), target->GetEntry());

        actionId = BG_WS_FLAG_ACTION_RETURNED;

        GetBgMap()->GetVariableManager().SetVariable(wsFlagPickedUp[teamIdx], BG_WS_FLAG_STATE_ON_BASE);
        GetBgMap()->GetVariableManager().SetVariable(wsFlagHUDPickedUp[teamIdx], BG_WS_FLAG_ICON_INVISIBLE);

        RespawnFlagAtBase(team, false);
        UpdatePlayerScore(player, SCORE_FLAG_RETURNS, 1);

        // send messages and sounds (广播护旗公告并播放归还音效)
        SendMessageToAll(wsgFlagData[teamIdx][actionId].messageId, wsgFlagData[teamIdx][actionId].chatType, player);
        PlaySoundToAll(wsgFlagData[teamIdx][actionId].soundId);
    }
    // check if we are picking up enemy flag (点击敌方掉落的战旗：重新拾起继续运送)
    else if (wsDroppedFlagId[otherTeamIdx] == target->GetEntry())
    {
        if (GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[otherTeamIdx]) != BG_WS_FLAG_STATE_ON_GROUND)
            return;

        DEBUG_LOG("BattleGroundWS: Team %u has recaptured the dropped flag %u.", player->GetTeam(), target->GetEntry());

        actionId = BG_WS_FLAG_ACTION_PICKEDUP;

        SpawnEvent(otherTeamIdx, 0, false);
        SetFlagCarrier(otherTeamIdx, player->GetObjectGuid());

        if (m_brutalAssaultActive)
            player->CastSpell(player, BG_WS_SPELL_BRUTAL_ASSAULT, TRIGGERED_OLD_TRIGGERED);
        else if (m_focusedAssaultActive)
            player->CastSpell(player, BG_WS_SPELL_FOCUSED_ASSAULT, TRIGGERED_OLD_TRIGGERED);

        GetBgMap()->GetVariableManager().SetVariable(wsFlagPickedUp[otherTeamIdx], BG_WS_FLAG_STATE_ON_PLAYER);
        GetBgMap()->GetVariableManager().SetVariable(wsFlagHUDPickedUp[teamIdx], BG_WS_FLAG_ICON_VISIBLE);

        // send messages and sounds (广播重拾敌旗公告与音效)
        SendMessageToAll(wsgFlagData[otherTeamIdx][actionId].messageId, wsgFlagData[teamIdx][actionId].chatType, player);
        PlaySoundToAll(wsgFlagData[otherTeamIdx][actionId].soundId);
    }

    if (actionId != BG_WS_FLAG_ACTION_NONE)
        player->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_PVP_ACTIVE_CANCELS);
}

// Handle flag click event (处理点击基地旗帜对象事件)
void BattleGroundWS::HandlePlayerClickedOnFlag(Player* player, GameObject* goTarget)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    // assess the current flag state based on the gameobject entry (根据旗帜对象模板分发基地拔旗逻辑)
    switch (goTarget->GetEntry())
    {
        case GO_WS_SILVERWING_FLAG:
        case GO_WS_WARSONG_FLAG:
            ProcessFlagPickUpFromBase(player, player->GetTeam());
            break;
    }
}

// Handle player exit (玩家离开战歌峡谷或掉线移除时强制掉落或重置其携带的战旗)
void BattleGroundWS::RemovePlayer(Player* player, ObjectGuid guid)
{
    std::vector<std::pair<Team, PvpTeamIndex>> checkIndex;
    if (player)
    {
        Team playerTeam = player->GetTeam();
        PvpTeamIndex playerTeamIndex = GetTeamIndexByTeamId(playerTeam);
        PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(playerTeamIndex);
        checkIndex.emplace_back(playerTeam, otherTeamIdx);
    }
    else // if no player, check both (若玩家已离线不存在对象指针，则双向检查持旗者 GUID)
    {
        checkIndex.push_back({ HORDE, TEAM_INDEX_ALLIANCE });
        checkIndex.push_back({ ALLIANCE, TEAM_INDEX_HORDE });
    }

    for (auto& data : checkIndex)
    {
        // Clear flag carrier and respawn main flag (若离场玩家正携带战旗，立即清理状态并归位或掉落)
        if (IsFlagPickedUp(data.second) && m_flagCarrier[data.second] == guid)
        {
            if (!player) // recheck the validity of this, shouldnt flag be dropped when player is logged out?
            {
                sLog.outError("BattleGroundWS: Removing offline player who unexpectendly carries the flag!");

                ClearFlagCarrier(data.second);
                RespawnFlagAtBase(data.first, false);
            }
            else
                HandlePlayerDroppedFlag(player);
        }
    }
}

// Handle area trigger (处理玩家携旗进入己方旗室交旗点区域触发器：仅当己方战旗在底座时才允许得分)
bool BattleGroundWS::HandleAreaTrigger(Player* player, uint32 trigger)
{
    // this is wrong way to implement these things. On official it done by gameobject spell cast. (待完善：官服通过对象施法判定)
    if (GetStatus() != STATUS_IN_PROGRESS)
        return false;

    switch (trigger)
    {
        case WS_AT_SILVERWING_ROOM:                                     // Alliance Flag spawn (联盟银翼旗室交旗点)
            if (GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[TEAM_INDEX_HORDE])
                && !GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[TEAM_INDEX_ALLIANCE]))
                if (GetFlagCarrierGuid(TEAM_INDEX_HORDE) == player->GetObjectGuid())
                    ProcessPlayerFlagScoreEvent(player);
            break;
        case WS_AT_WARSONG_ROOM:                                        // Horde Flag spawn (部落战歌旗室交旗点)
            if (GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[TEAM_INDEX_ALLIANCE])
                && !GetBgMap()->GetVariableManager().GetVariable(wsFlagPickedUp[TEAM_INDEX_HORDE]))
                if (GetFlagCarrierGuid(TEAM_INDEX_ALLIANCE) == player->GetObjectGuid())
                    ProcessPlayerFlagScoreEvent(player);
            break;
        default:
            return false;
    }

    return true;
}

void BattleGroundWS::HandleGameObjectCreate(GameObject* go)
{
    // set initial data and activate capture points (记录地面召唤的临时掉落战旗 GUID)
    switch (go->GetEntry())
    {
        case GO_WS_SILVERWING_FLAG_DROP:
            m_droppedFlagGuid[TEAM_INDEX_ALLIANCE] = go->GetObjectGuid();
            break;
        case GO_WS_WARSONG_FLAG_DROP:
            m_droppedFlagGuid[TEAM_INDEX_HORDE] = go->GetObjectGuid();
            break;
    }
}

// process click on dropped flag events (处理法术/对象事件驱动的拾旗与归还事件)
bool BattleGroundWS::HandleEvent(uint32 eventId, Object* source, Object* target)
{
    switch (eventId)
    {
        case WS_EVENT_ALLIANCE_FLAG_DROPPED_PICKUP:
        case WS_EVENT_HORDE_FLAG_DROPPED_PICKUP:
        {
            if (!source || !source->IsGameObject())
                return true;

            GameObject* go = static_cast<GameObject*>(source);
            if (!target->IsPlayer())
                return true;

            Player* srcPlayer = static_cast<Player*>(target);

            // only handles return, not pickup of dropped (此处仅处理己方归还，不处理敌方拾起)
            Team team = srcPlayer->GetTeam();
            PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
            PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);
            if (wsDroppedFlagId[teamIdx] != go->GetEntry())
                return true;

            ProcessDroppedFlagActions(srcPlayer, go);

            // when clicked the flag despawns (交互后移除地面掉落战旗)
            go->SetLootState(GO_JUST_DEACTIVATED);
            break;
        }
        case WS_EVENT_ALLIANCE_FLAG_PICKUP:
        case WS_EVENT_HORDE_FLAG_PICKUP:
        {
            if (!target || !target->IsGameObject())
                return true;

            GameObject* go = static_cast<GameObject*>(target);
            if (!source->IsPlayer())
                return true;

            Player* srcPlayer = static_cast<Player*>(source);

            if (go->GetGoType() == GAMEOBJECT_TYPE_FLAGDROP)
                ProcessDroppedFlagActions(srcPlayer, go);
            else
                HandlePlayerClickedOnFlag(srcPlayer, go);

            // when clicked the flag despawns (交互后令基地或地面战旗对象消失)
            go->SetLootState(GO_JUST_DEACTIVATED);
            break;
        }
    }

    return false;
}

// 重置战歌峡谷全部世界状态、战旗计时器、易伤标记及墓地链接
void BattleGroundWS::Reset()
{
    // call parent's class reset (调用基类重置)
    BattleGround::Reset();

    GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_CAPTURES_ALLIANCE, 0);
    GetBgMap()->GetVariableManager().SetVariableData(BG_WS_STATE_CAPTURES_ALLIANCE, true, 0, 0);
    GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_CAPTURES_HORDE, 0);
    GetBgMap()->GetVariableManager().SetVariableData(BG_WS_STATE_CAPTURES_HORDE, true, 0, 0);
    GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_CAPTURES_MAX, BG_WS_MAX_TEAM_SCORE);
    GetBgMap()->GetVariableManager().SetVariableData(BG_WS_STATE_CAPTURES_MAX, true, 0, 0);
    GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_FLAG_HORDE, BG_WS_FLAG_ICON_INVISIBLE);
    GetBgMap()->GetVariableManager().SetVariableData(BG_WS_STATE_FLAG_HORDE, true, 0, 0);
    GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_FLAG_ALLIANCE, BG_WS_FLAG_ICON_INVISIBLE);
    GetBgMap()->GetVariableManager().SetVariableData(BG_WS_STATE_FLAG_ALLIANCE, true, 0, 0);
    GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_ALLIANCE, 0);
    GetBgMap()->GetVariableManager().SetVariableData(BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_ALLIANCE, true, 0, 0);
    GetBgMap()->GetVariableManager().SetVariable(BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_HORDE, 0);
    GetBgMap()->GetVariableManager().SetVariableData(BG_WS_STATE_FLAG_PICKED_UP_FLAG_STATE_HORDE, true, 0, 0);

    // spiritguides and flags not spawned at beginning (初始不刷出灵魂向导与战旗，等待开门阶段刷新)
    m_activeEvents[WS_EVENT_SPIRITGUIDES_SPAWN] = BG_EVENT_NONE;
    m_activeEvents[WS_EVENT_FLAG_A] = BG_EVENT_NONE;
    m_activeEvents[WS_EVENT_FLAG_H] = BG_EVENT_NONE;

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        m_droppedFlagGuid[i].Clear();
        m_flagCarrier[i].Clear();
        m_flagOnRespawn[i]  = false;

        m_flagsDropTimer[i] = 0;
        m_flagsTimer[i]     = 0;
    }

    bool isBgWeekend = BattleGroundMgr::IsBgWeekend(GetTypeId());
    m_reputationCapture = (isBgWeekend) ? BG_WS_WEEKEND_FLAG_CAPTURE_REPUTATION : BG_WS_NORMAL_FLAG_CAPTURE_REPUTATION;
    m_honorWinKills = (isBgWeekend) ? BG_WS_WEEKEND_WIN_KILLS : BG_WS_NORMAL_WIN_KILLS;
    m_honorEndKills = (isBgWeekend) ? BG_WS_WEEKEND_MAP_COMPLETE_KILLS : BG_WS_NORMAL_MAP_COMPLETE_KILLS;
    m_flagCarrierDebuffTimer = BG_WS_BRUTAL_ASSAULT_TIME;
    m_brutalAssaultActive = false;
    m_focusedAssaultActive = false;

    // setup graveyards (开战前使用旗室出生墓地)
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_MAIN_ALLIANCE,     BG_WS_ZONE_ID_MAIN, TEAM_INVALID);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_MAIN_HORDE,        BG_WS_ZONE_ID_MAIN, TEAM_INVALID);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_FLAGROOM_ALLIANCE, BG_WS_ZONE_ID_MAIN, ALLIANCE);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_FLAGROOM_HORDE,    BG_WS_ZONE_ID_MAIN, HORDE);
}

// 结束战歌峡谷并向双方发放胜场与完赛荣誉奖励
void BattleGroundWS::EndBattleGround(Team winner)
{
    // win reward (胜方额外荣誉)
    RewardHonorToTeam(GetBonusHonorFromKill(m_honorWinKills), winner);

    // complete map_end rewards (even if no team wins) (双方完赛基础荣誉)
    RewardHonorToTeam(GetBonusHonorFromKill(m_honorEndKills), ALLIANCE);
    RewardHonorToTeam(GetBonusHonorFromKill(m_honorEndKills), HORDE);

    BattleGround::EndBattleGround(winner);
}

void BattleGroundWS::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    HandlePlayerDroppedFlag(player);

    BattleGround::HandleKillPlayer(player, killer);
}

void BattleGroundWS::UpdatePlayerScore(Player* player, uint32 type, uint32 value)
{
    BattleGroundScoreMap::iterator itr = m_playerScores.find(player->GetObjectGuid());
    if (itr == m_playerScores.end())                        // player not found (未找到玩家记分记录)
        return;

    switch (type)
    {
        case SCORE_FLAG_CAPTURES:                           // flags captured (夺旗得分次数)
            ((BattleGroundWGScore*)itr->second)->flagCaptures += value;
            break;
        case SCORE_FLAG_RETURNS:                            // flags returned (护旗归还次数)
            ((BattleGroundWGScore*)itr->second)->flagReturns += value;
            break;
        default:
            BattleGround::UpdatePlayerScore(player, type, value);
            break;
    }
}

// 人数不足提前结束时按当前夺旗得分判定胜者，平分则回退基类按人数判定
Team BattleGroundWS::GetPrematureWinner()
{
    int32 hordeScore = GetBgMap()->GetVariableManager().GetVariable(BG_WS_STATE_CAPTURES_HORDE);
    int32 allianceScore = GetBgMap()->GetVariableManager().GetVariable(BG_WS_STATE_CAPTURES_ALLIANCE);

    if (hordeScore > allianceScore)
        return HORDE;
    if (allianceScore > hordeScore)
        return ALLIANCE;

    // If the values are equal, fall back to number of players on each team (夺旗数相同时按双方在场人数判定)
    return BattleGround::GetPrematureWinner();
}
