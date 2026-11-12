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

#include "Common.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "Log/Log.h"
#include "Entities/Player.h"

// 处理客户端接受决斗请求 (CMSG_DUEL_ACCEPTED)
void WorldSession::HandleDuelAcceptedOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    recvPacket >> guid;

    // Check for own duel info first (先校验自身是否存在有效决斗信息)
    Player* self = GetPlayer();
    if (!self || !self->duel)
        return;

    // Check if we are not accepting our own duel request (禁止发起者自己接受自己的决斗请求)
    Player* initiator = self->duel->initiator;
    if (!initiator || self == initiator)
        return;

    // Check for opponent (校验决斗对手是否有效且非本人)
    Player* opponent = self->duel->opponent;
    if (!opponent || self == opponent)
        return;

    // Check if duel is starting (校验决斗倒计时是否已经启动)
    if (self->duel->startTimer != 0 || opponent->duel->startTimer != 0)
        return;

    // Check if duel is in progress (校验决斗是否已经处于进行中)
    if (self->duel->startTime != 0 || opponent->duel->startTime != 0)
        return;

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: received CMSG_DUEL_ACCEPTED");
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "Player 1 is: %u (%s)", self->GetGUIDLow(), self->GetName());
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "Player 2 is: %u (%s)", opponent->GetGUIDLow(), opponent->GetName());

    // 记录倒计时开始时间并向双方发送 3 秒决斗倒计时
    time_t now = time(nullptr);
    self->duel->startTimer = now;
    opponent->duel->startTimer = now;

    self->SendDuelCountdown(3000);
    opponent->SendDuelCountdown(3000);
}

// 处理客户端取消/认输决斗请求 (CMSG_DUEL_CANCELLED)
void WorldSession::HandleDuelCancelledOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    recvPacket >> guid;

    // Check for own duel info first (先校验自身决斗数据)
    Player* self = GetPlayer();
    if (!self || !self->duel)
        return;

    // Check for opponent (校验对手存在)
    Player* opponent = self->duel->opponent;
    if (!opponent)
        return;

    DEBUG_LOG("WORLD: Received opcode CMSG_DUEL_CANCELLED");

    // If duel is in progress, then player surrendered in a duel using /forfeit (决斗已开打时取消即 /forfeit 认输，触发求饶动作并判负)
    if (self->duel->startTime != 0)
    {
        self->CombatStopWithPets(true);
        opponent->CombatStopWithPets(true);
        self->CastSpell(self, 7267, TRIGGERED_OLD_TRIGGERED);    // beg (施放求饶动作法术)
        self->DuelComplete(DUEL_WON);
        return;
    }
    // Player either discarded the duel using the "discard button" or used "/forfeit" before countdown reached 0
    // 倒计时未结束前拒绝或取消决斗，按中断处理
    self->DuelComplete(DUEL_INTERRUPTED);
}
