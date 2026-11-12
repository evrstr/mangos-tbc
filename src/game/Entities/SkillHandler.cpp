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
#include "Server/Opcodes.h"
#include "Log/Log.h"
#include "Entities/Player.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"

// 处理学习天赋操作码（CMSG_LEARN_TALENT）
void WorldSession::HandleLearnTalentOpcode(WorldPacket& recv_data)
{
    uint32 talent_id, requested_rank;
    recv_data >> talent_id >> requested_rank;

    _player->LearnTalent(talent_id, requested_rank);

    // if player has a pet, update owner talent auras (若玩家当前拥有宠物，则同步更新主人天赋光环加成)
    if (_player->GetPet())
        _player->GetPet()->CastOwnerTalentAuras();
}

// 处理确认洗天赋操作码（MSG_TALENT_WIPE_CONFIRM）
void WorldSession::HandleTalentWipeConfirmOpcode(WorldPacket& recv_data)
{
    DETAIL_LOG("MSG_TALENT_WIPE_CONFIRM");
    ObjectGuid guid;
    recv_data >> guid;

    // 校验可交互的训练师 NPC
    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTalentWipeConfirmOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    if (!unit->CanTrainAndResetTalentsOf(_player))
        return;

    // 执行天赋重置；若玩家未学习任何天赋则回发零值确认包
    if (!(_player->resetTalents()))
    {
        WorldPacket data(MSG_TALENT_WIPE_CONFIRM, 8 + 4);   // you have not any talent (当前没有任何已学天赋)
        data << uint64(0);
        data << uint32(0);
        SendPacket(data);
        return;
    }

    unit->CastSpell(_player, 14867, TRIGGERED_OLD_TRIGGERED);                  // spell: "Untalent Visual Effect" (施放洗天赋视觉特效法术)

    // 同步更新宠物身上的主人天赋光环
    if (_player->GetPet())
        _player->GetPet()->CastOwnerTalentAuras();
}

// 处理遗忘专业技能操作码（CMSG_UNLEARN_SKILL）
void WorldSession::HandleUnlearnSkillOpcode(WorldPacket& recv_data)
{
    uint32 skill_id;
    recv_data >> skill_id;

    Player* player = GetPlayer();

    // Check if unlearnable (校验该技能是否允许被遗忘)
    if (!player->GetSkillInfo(uint16(skill_id), ([] (SkillRaceClassInfoEntry const& entry) { return (entry.flags & SKILL_FLAG_CAN_UNLEARN); })))
        return;

    player->SetSkillStep(uint16(skill_id), 0);
}
