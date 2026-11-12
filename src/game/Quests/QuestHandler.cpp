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
#include "Log/Log.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "Server/Opcodes.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Entities/Player.h"
#include "Entities/GossipDef.h"
#include "Quests/QuestDef.h"
#include "Globals/ObjectAccessor.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Groups/Group.h"
#include "Tools/Formulas.h"

// 处理查询单个任务发布者状态请求（CMSG_QUESTGIVER_STATUS_QUERY）
void WorldSession::HandleQuestgiverStatusQueryOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;
    uint8 dialogStatus = DIALOG_STATUS_NONE;

    Object* questgiver = _player->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
    if (!questgiver)
    {
        DETAIL_LOG("Error in CMSG_QUESTGIVER_STATUS_QUERY, called for not found questgiver %s", guid.GetString().c_str());
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_STATUS_QUERY - for %s to %s", _player->GetGuidStr().c_str(), guid.GetString().c_str());

    switch (questgiver->GetTypeId())
    {
        case TYPEID_UNIT:
        {
            Creature* cr_questgiver = (Creature*)questgiver;

            if (_player->CanInteract(static_cast<Unit*>(questgiver)))       // not show quest status to enemies (不向敌对单位显示任务状态)
            {
                dialogStatus = sScriptDevAIMgr.GetDialogStatus(_player, cr_questgiver);

                if (dialogStatus == DIALOG_STATUS_UNDEFINED)
                    dialogStatus = getDialogStatus(_player, cr_questgiver, DIALOG_STATUS_NONE);
            }
            break;
        }
        case TYPEID_GAMEOBJECT:
        {
            GameObject* go_questgiver = (GameObject*)questgiver;

            if (_player->CanInteract(go_questgiver))
            {
                dialogStatus = sScriptDevAIMgr.GetDialogStatus(_player, go_questgiver);

                if (dialogStatus == DIALOG_STATUS_UNDEFINED)
                    dialogStatus = getDialogStatus(_player, go_questgiver, DIALOG_STATUS_NONE);
            }
            break;
        }
        default:
            sLog.outError("QuestGiver called for unexpected type %u", questgiver->GetTypeId());
            break;
    }

    // inform client about status of quest (向客户端下发任务发布者头顶标记状态)
    _player->GetPlayerMenu()->SendQuestGiverStatus(dialogStatus, guid);
}

// 处理与任务 NPC 对话问候请求（CMSG_QUESTGIVER_HELLO）
void WorldSession::HandleQuestgiverHelloOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_HELLO - for %s to %s", _player->GetGuidStr().c_str(), guid.GetString().c_str());

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleQuestgiverHelloOpcode - for %s to %s not found or you can't interact with him.", _player->GetGuidStr().c_str(), guid.GetString().c_str());
        return;
    }

    // Stop the npc if moving (交互时暂停 NPC 的路径移动)
    if (uint32 pauseTimer = pCreature->GetInteractionPauseTimer())
        pCreature->GetMotionMaster()->PauseWaypoints(pauseTimer);

    if (sScriptDevAIMgr.OnGossipHello(_player, pCreature))
        return;

    _player->PrepareGossipMenu(pCreature, pCreature->GetDefaultGossipMenuId());
    _player->SendPreparedGossip(pCreature);
}

// 处理玩家接取任务请求（CMSG_QUESTGIVER_ACCEPT_QUEST）：校验给予者、前置条件、队友共享确认与触发法术
void WorldSession::HandleQuestgiverAcceptQuestOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 quest;
    recv_data >> guid >> quest;

    if (!CanInteractWithQuestGiver(guid, "CMSG_QUESTGIVER_ACCEPT_QUEST"))
        return;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_ACCEPT_QUEST - for %s to %s, quest = %u", _player->GetGuidStr().c_str(), guid.GetString().c_str(), quest);

    Object* pObject = _player->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_GAMEOBJECT_PLAYER_OR_ITEM);

    // no or incorrect quest giver (校验给予者是否合法且确实提供或可共享该任务)
    if (!pObject
            || (pObject->GetTypeId() != TYPEID_PLAYER && !pObject->HasQuest(quest))
            || (pObject->GetTypeId() == TYPEID_PLAYER && !((Player*)pObject)->CanShareQuest(quest))
       )
    {
        _player->GetPlayerMenu()->CloseGossip();
        _player->ClearDividerGuid();
        return;
    }

    Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest);
    if (qInfo)
    {
        // prevent cheating (防作弊检查：确认玩家满足接取条件)
        if (!GetPlayer()->CanTakeQuest(qInfo, true))
        {
            _player->GetPlayerMenu()->CloseGossip();
            _player->ClearDividerGuid();
            return;
        }

        if (Player* pPlayer = ObjectAccessor::FindPlayer(_player->GetDividerGuid()))
        {
            pPlayer->SendPushToPartyResponse(_player, QUEST_PARTY_MSG_ACCEPT_QUEST);
            _player->ClearDividerGuid();
        }

        if (_player->CanAddQuest(qInfo, true))
        {
            _player->AddQuest(qInfo, pObject);              // pObject (if it item) can be destroyed at call (若由物品触发接取，AddQuest 可能销毁该物品)

            if (qInfo->HasQuestFlag(QUEST_FLAGS_PARTY_ACCEPT))
            {
                if (Group* pGroup = _player->GetGroup())
                {
                    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
                    {
                        Player* pPlayer = itr->getSource();

                        if (!pPlayer || pPlayer == _player || !pPlayer->IsInWorld()) // not self (跳过自己及不在线队员)
                            continue;

                        if (pPlayer->CanTakeQuest(qInfo, true))
                        {
                            pPlayer->SetDividerGuid(_player->GetObjectGuid());

                            // need confirmation that any gossip window will close (关闭队员当前对话窗口后弹出任务确认框)
                            pPlayer->GetPlayerMenu()->CloseGossip();

                            _player->SendQuestConfirmAccept(qInfo, pPlayer);
                        }
                    }
                }
            }

            if (_player->CanCompleteQuest(quest))
                _player->CompleteQuest(quest);

            _player->GetPlayerMenu()->CloseGossip();

            if (qInfo->GetSrcSpell() > 0)
                _player->CastSpell(_player, qInfo->GetSrcSpell(), TRIGGERED_OLD_TRIGGERED);

            return;
        }
    }

    _player->GetPlayerMenu()->CloseGossip();
}

// 处理查询任务详情请求（CMSG_QUESTGIVER_QUERY_QUEST）
void WorldSession::HandleQuestgiverQueryQuestOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 quest;
    recv_data >> guid >> quest;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_QUERY_QUEST - for %s to %s, quest = %u", _player->GetGuidStr().c_str(), guid.GetString().c_str(), quest);

    // Verify that the guid is valid and is a questgiver or involved in the requested quest
    // 校验目标对象存在且与所请求的任务存在发布或交付关系
    Object* pObject = _player->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_GAMEOBJECT_OR_ITEM);
    if (!pObject || (!pObject->HasQuest(quest) && !pObject->HasInvolvedQuest(quest)))
    {
        _player->GetPlayerMenu()->CloseGossip();
        return;
    }

    if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest))
        _player->GetPlayerMenu()->SendQuestGiverQuestDetails(pQuest, pObject->GetObjectGuid(), true);
}

// 处理客户端查询任务模板信息请求（CMSG_QUEST_QUERY），按会话语言返回 SMSG_QUEST_QUERY_RESPONSE
void WorldSession::HandleQuestQueryOpcode(WorldPacket& recv_data)
{
    uint32 quest;
    recv_data >> quest;
    DEBUG_LOG("WORLD: Received opcode CMSG_QUEST_QUERY quest = %u", quest);

    Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest);
    if (pQuest)
    {
        std::string ObjectiveText[QUEST_OBJECTIVES_COUNT];
        std::string Title = pQuest->GetTitle();
        std::string Details = pQuest->GetDetails();
        std::string Objectives = pQuest->GetObjectives();
        std::string EndText = pQuest->GetEndText();

        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            ObjectiveText[i] = pQuest->ObjectiveText[i];

        int loc_idx = GetSessionDbLocaleIndex();
        if (loc_idx >= 0)
        {
            if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
            {
                if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
                    Title = ql->Title[loc_idx];
                if (ql->Details.size() > (size_t)loc_idx && !ql->Details[loc_idx].empty())
                    Details = ql->Details[loc_idx];
                if (ql->Objectives.size() > (size_t)loc_idx && !ql->Objectives[loc_idx].empty())
                    Objectives = ql->Objectives[loc_idx];
                if (ql->EndText.size() > (size_t)loc_idx && !ql->EndText[loc_idx].empty())
                    EndText = ql->EndText[loc_idx];

                for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
                    if (ql->ObjectiveText[i].size() > (size_t)loc_idx && !ql->ObjectiveText[i][loc_idx].empty())
                        ObjectiveText[i] = ql->ObjectiveText[i][loc_idx];
            }
        }

        WorldPacket data(SMSG_QUEST_QUERY_RESPONSE, 100);       // guess size (预估包长)

        data << uint32(pQuest->GetQuestId());                   // quest id (任务 ID)
        data << uint32(pQuest->GetQuestMethod());               // Accepted values: 0, 1 or 2. 0==IsAutoComplete() (skip objectives/details) (接取方式：0 表示自动完成，跳过详情/目标)
        data << int32(pQuest->GetQuestLevel());                 // may be -1, static data, in other cases must be used dynamic level: Player::GetQuestLevelForPlayer (0 is not known, but assuming this is no longer valid for quest intended for client) (静态任务等级，-1 表示随玩家等级动态计算)
        data << uint32(pQuest->GetZoneOrSort());                // zone or sort to display in quest log (任务日志中的区域或分类排序 ID)

        data << uint32(pQuest->GetType());                      // quest type (任务类型)
        data << uint32(pQuest->GetSuggestedPlayers());          // suggested players count (建议人数)

        data << uint32(pQuest->GetRepObjectiveFaction());       // shown in quest log as part of quest objective (任务目标要求声望阵营)
        data << uint32(pQuest->GetRepObjectiveValue());         // shown in quest log as part of quest objective (任务目标要求声望值)

        data << uint32(0);                                      // RequiredOpositeRepFaction (对立阵营声望需求，未使用)
        data << uint32(0);                                      // RequiredOpositeRepValue, required faction value with another (oposite) faction (objective)

        data << uint32(pQuest->GetNextQuestInChain());          // client will request this quest from NPC, if not 0 (任务链中的后续任务 ID)

        if (pQuest->HasQuestFlag(QUEST_FLAGS_HIDDEN_REWARDS))
            data << uint32(0);                                  // Hide money rewarded (隐藏奖励金币)
        else
            data << uint32(pQuest->GetRewOrReqMoney());         // reward money (below max lvl) (奖励或需求金币)

        data << uint32(pQuest->GetRewMoneyMaxLevel());          // used in XP calculation at client (满级折算金币，客户端也据此推算经验)
        data << uint32(pQuest->GetRewSpell());                  // reward spell, this spell will display (icon) (casted if RewSpellCast==0) (展示用奖励法术图标)
        data << uint32(pQuest->GetRewSpellCast());              // casted spell (交任务时实际施放的奖励法术)

        // rewarded honor points (奖励荣誉点数)
        data << uint32(MaNGOS::Honor::hk_honor_at_level(GetCurrentPlayerLevel(), pQuest->GetRewHonorableKills()));
        data << uint32(pQuest->GetSrcItemId());                 // source item id (接取附带物品 ID)
        data << uint32(pQuest->GetQuestFlags());                // quest flags (任务标志位)
        data << uint32(pQuest->GetCharTitleId());               // CharTitleId, new 2.4.0, player gets this title (id from CharTitles) (2.4.0 新增奖励头衔 ID)

        uint32 iI;

        if (pQuest->HasQuestFlag(QUEST_FLAGS_HIDDEN_REWARDS))
        {
            for (iI = 0; iI < QUEST_REWARDS_COUNT; ++iI)
                data << uint32(0) << uint32(0);
            for (iI = 0; iI < QUEST_REWARD_CHOICES_COUNT; ++iI)
                data << uint32(0) << uint32(0);
        }
        else
        {
            for (iI = 0; iI < QUEST_REWARDS_COUNT; ++iI)
            {
                data << uint32(pQuest->RewItemId[iI]);
                data << uint32(pQuest->RewItemCount[iI]);
            }
            for (iI = 0; iI < QUEST_REWARD_CHOICES_COUNT; ++iI)
            {
                data << uint32(pQuest->RewChoiceItemId[iI]);
                data << uint32(pQuest->RewChoiceItemCount[iI]);
            }
        }

        data << pQuest->GetPointMapId();
        data << pQuest->GetPointX();
        data << pQuest->GetPointY();
        data << pQuest->GetPointOpt();

        data << Title;
        data << Objectives;
        data << Details;
        data << EndText;

        for (iI = 0; iI < QUEST_OBJECTIVES_COUNT; ++iI)
        {
            if (pQuest->ReqCreatureOrGOId[iI] < 0)
            {
                // client expected gameobject template id in form (id|0x80000000) (负值表示游戏对象，客户端要求最高位置 1)
                data << uint32((pQuest->ReqCreatureOrGOId[iI] * (-1)) | 0x80000000);
            }
            else
            {
                data << uint32(pQuest->ReqCreatureOrGOId[iI]);
            }
            data << uint32(pQuest->ReqCreatureOrGOCount[iI]);
            data << uint32(pQuest->ReqItemId[iI]);
            data << uint32(pQuest->ReqItemCount[iI]);
        }

        for (iI = 0; iI < QUEST_OBJECTIVES_COUNT; ++iI)
            data << ObjectiveText[iI];

        SendPacket(data);

        DEBUG_LOG("WORLD: Sent SMSG_QUEST_QUERY_RESPONSE questid=%u", pQuest->GetQuestId());
    }
}

// 处理玩家选择任务奖励并交付任务请求（CMSG_QUESTGIVER_CHOOSE_REWARD）
void WorldSession::HandleQuestgiverChooseRewardOpcode(WorldPacket& recv_data)
{
    uint32 quest, reward;
    ObjectGuid guid;
    recv_data >> guid >> quest >> reward;

    if (reward >= QUEST_REWARD_CHOICES_COUNT)
    {
        sLog.outError("Error in CMSG_QUESTGIVER_CHOOSE_REWARD - %s tried to get invalid reward (%u) (probably packet hacking)", _player->GetGuidStr().c_str(), reward);
        return;
    }

    if (!CanInteractWithQuestGiver(guid, "CMSG_QUESTGIVER_CHOOSE_REWARD"))
        return;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_CHOOSE_REWARD - for %s to %s, quest = %u, reward = %u", _player->GetGuidStr().c_str(), guid.GetString().c_str(), quest, reward);

    Object* pObject = _player->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
    if (!pObject)
        return;

    if (!pObject->HasInvolvedQuest(quest))
        return;

    Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest);
    if (pQuest)
    {
        if (_player->CanRewardQuest(pQuest, reward, true))
        {
            _player->RewardQuest(pQuest, reward, pObject);

            // Send next quest (若存在后续连锁任务，立即推送给客户端)
            if (Quest const* nextquest = _player->GetNextQuest(guid, pQuest))
                _player->GetPlayerMenu()->SendQuestGiverQuestDetails(nextquest, guid, true);
        }
        else
            _player->GetPlayerMenu()->SendQuestGiverOfferReward(pQuest, guid, true);
    }
}

// 处理玩家请求展示任务奖励界面（CMSG_QUESTGIVER_REQUEST_REWARD）
void WorldSession::HandleQuestgiverRequestRewardOpcode(WorldPacket& recv_data)
{
    uint32 quest;
    ObjectGuid guid;
    recv_data >> guid >> quest;

    if (!CanInteractWithQuestGiver(guid, "CMSG_QUESTGIVER_REQUEST_REWARD"))
        return;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_REQUEST_REWARD - for %s to %s, quest = %u", _player->GetGuidStr().c_str(), guid.GetString().c_str(), quest);

    Object* pObject = _player->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
    if (!pObject || !pObject->HasInvolvedQuest(quest))
        return;

    if (_player->CanCompleteQuest(quest))
        _player->CompleteQuest(quest);

    if (_player->GetQuestStatus(quest) != QUEST_STATUS_COMPLETE)
        return;

    if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest))
        _player->GetPlayerMenu()->SendQuestGiverOfferReward(pQuest, guid, true);
}

// 处理取消任务对话界面（CMSG_QUESTGIVER_CANCEL）
void WorldSession::HandleQuestgiverCancel(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_CANCEL");

    _player->GetPlayerMenu()->CloseGossip();
}

// 处理交换任务日志槽位请求（CMSG_QUESTLOG_SWAP_QUEST）
void WorldSession::HandleQuestLogSwapQuest(WorldPacket& recv_data)
{
    uint8 slot1, slot2;
    recv_data >> slot1 >> slot2;

    if (slot1 == slot2 || slot1 >= MAX_QUEST_LOG_SIZE || slot2 >= MAX_QUEST_LOG_SIZE)
        return;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTLOG_SWAP_QUEST slot 1 = %u, slot 2 = %u", slot1, slot2);

    GetPlayer()->SwapQuestSlot(slot1, slot2);
}

// 处理玩家放弃任务请求（CMSG_QUESTLOG_REMOVE_QUEST）：回收任务起始物品、移除限时任务并重置状态
void WorldSession::HandleQuestLogRemoveQuest(WorldPacket& recv_data)
{
    uint8 slot;
    recv_data >> slot;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTLOG_REMOVE_QUEST slot = %u", slot);

    if (slot < MAX_QUEST_LOG_SIZE)
    {
        if (uint32 quest = _player->GetQuestSlotQuestId(slot))
        {
            if (!_player->TakeQuestSourceItem(quest, true))
                return;                                     // can't un-equip some items, reject quest cancel (部分任务物品无法卸下时拒绝放弃)

            if (const Quest* pQuest = sObjectMgr.GetQuestTemplate(quest))
            {
                if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
                    _player->RemoveTimedQuest(quest);

                for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
                {
                    if (pQuest->ReqSourceId[i])
                    {
                        ItemPrototype const* iProto = ObjectMgr::GetItemPrototype(pQuest->ReqSourceId[i]);
                        if (iProto && iProto->Bonding == BIND_QUEST_ITEM)
                            _player->DestroyItemCount(pQuest->ReqSourceId[i], pQuest->ReqSourceCount[i], true, false, true);
                    }
                }
            }

            _player->SetQuestStatus(quest, QUEST_STATUS_NONE);
        }

        _player->SetQuestSlot(slot, 0);
    }
}

// 处理队员确认接取全队共享任务请求（CMSG_QUEST_CONFIRM_ACCEPT）
void WorldSession::HandleQuestConfirmAccept(WorldPacket& recv_data)
{
    uint32 quest;
    recv_data >> quest;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUEST_CONFIRM_ACCEPT quest = %u", quest);

    if (const Quest* pQuest = sObjectMgr.GetQuestTemplate(quest))
    {
        if (!pQuest->HasQuestFlag(QUEST_FLAGS_PARTY_ACCEPT))
            return;

        Player* pOriginalPlayer = ObjectAccessor::FindPlayer(_player->GetDividerGuid());

        if (!pOriginalPlayer)
            return;

        if (pQuest->IsAllowedInRaid())
        {
            if (!_player->IsInGroup(pOriginalPlayer))
                return;
        }
        else
        {
            if (!_player->IsInParty(pOriginalPlayer))
                return;
        }

        if (_player->CanAddQuest(pQuest, true))
            _player->AddQuest(pQuest, nullptr);                // nullptr, this prevent DB script from duplicate running (传入 nullptr 避免重复触发任务接取脚本)

        _player->ClearDividerGuid();
    }
}

// 处理请求交付任务物品界面（CMSG_QUESTGIVER_COMPLETE_QUEST）
void WorldSession::HandleQuestgiverCompleteQuest(WorldPacket& recv_data)
{
    uint32 quest;
    ObjectGuid guid;
    recv_data >> guid >> quest;

    if (!CanInteractWithQuestGiver(guid, "CMSG_QUESTGIVER_COMPLETE_QUEST"))
        return;

    // All ok, continue
    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_COMPLETE_QUEST - for %s to %s, quest = %u", _player->GetGuidStr().c_str(), guid.GetString().c_str(), quest);

    if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest))
    {
        if (_player->GetQuestStatus(quest) != QUEST_STATUS_COMPLETE)
        {
            if (pQuest->IsRepeatable())
                _player->GetPlayerMenu()->SendQuestGiverRequestItems(pQuest, guid, _player->CanCompleteRepeatableQuest(pQuest), false);
            else
                _player->GetPlayerMenu()->SendQuestGiverRequestItems(pQuest, guid, _player->CanRewardQuest(pQuest, false), false);
        }
        else
            _player->GetPlayerMenu()->SendQuestGiverRequestItems(pQuest, guid, _player->CanRewardQuest(pQuest, false), false);
    }
}

// 处理任务自动弹出通知（CMSG_QUESTGIVER_QUEST_AUTOLAUNCH）
void WorldSession::HandleQuestgiverQuestAutoLaunch(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_QUEST_AUTOLAUNCH");
}

// 处理将任务推送分享给小队/团队成员（CMSG_PUSHQUESTTOPARTY）
void WorldSession::HandlePushQuestToParty(WorldPacket& recvPacket)
{
    uint32 questId;
    recvPacket >> questId;

    DEBUG_LOG("WORLD: Received opcode CMSG_PUSHQUESTTOPARTY quest = %u", questId);

    if (!_player->CanShareQuest(questId))
    {
        sLog.outError("Error in CMSG_PUSHQUESTTOPARTY - %s tried to share invalid quest (%u) (probably packet hacking)", _player->GetGuidStr().c_str(), questId);
        return;
    }

    if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(questId))
    {
        if (Group* pGroup = _player->GetGroup())
        {
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* pPlayer = itr->getSource();

                if (!pPlayer || pPlayer == _player || !pPlayer->IsInWorld())         // skip self (跳过自己与未进入世界的队员)
                    continue;

                _player->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_SHARING_QUEST);

                if (_player->GetDistance(pPlayer) > 10)
                {
                    _player->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_TOO_FAR);
                    continue;
                }

                if (!pPlayer->SatisfyQuestStatus(pQuest, false))
                {
                    _player->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_HAVE_QUEST);
                    continue;
                }

                if (pPlayer->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
                {
                    _player->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_FINISH_QUEST);
                    continue;
                }

                if (!pPlayer->CanTakeQuest(pQuest, false))
                {
                    _player->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_CANT_TAKE_QUEST);
                    continue;
                }

                if (!pPlayer->SatisfyQuestLog(false))
                {
                    _player->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_LOG_FULL);
                    continue;
                }

                if (pPlayer->GetDividerGuid())
                {
                    _player->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_BUSY);
                    continue;
                }

                pPlayer->GetPlayerMenu()->SendQuestGiverQuestDetails(pQuest, _player->GetObjectGuid(), true);
                pPlayer->SetDividerGuid(_player->GetObjectGuid());
            }
        }
    }
}

// 处理任务分享结果回执并转发给发起分享的玩家（MSG_QUEST_PUSH_RESULT）
void WorldSession::HandleQuestPushResult(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint8 msg;
    recvPacket >> guid >> msg;

    DEBUG_LOG("WORLD: Received opcode MSG_QUEST_PUSH_RESULT");

    if (Player* pPlayer = ObjectAccessor::FindPlayer(_player->GetDividerGuid()))
    {
        WorldPacket data(MSG_QUEST_PUSH_RESULT, (8 + 1));
        data << ObjectGuid(guid);
        data << uint8(msg);                             // valid values: 0-8 (有效枚举值 0~8)
        pPlayer->GetSession()->SendPacket(data);
        _player->ClearDividerGuid();
    }
}

/**
 * What - if any - kind of explanation mark or question-mark should a quest-giver display for a player
 * 计算任务发布者（NPC 或游戏对象）应对指定玩家显示的感叹号/问号标记状态
 * @param pPlayer - for whom (目标玩家)
 * @param questgiver - from whom (任务发布者)
 * @param defstatus - initial set status (usually it will be called with DIALOG_STATUS_NONE) - must not be DIALOG_STATUS_UNDEFINED (初始默认状态，不能为 DIALOG_STATUS_UNDEFINED)
 */
uint32 WorldSession::getDialogStatus(const Player* pPlayer, const Object* questgiver, uint32 defstatus) const
{
    MANGOS_ASSERT(defstatus != DIALOG_STATUS_UNDEFINED);

    uint32 dialogStatus = defstatus;

    QuestRelationsMapBounds rbounds;                        // QuestRelations (quest-giver) (任务发布关系)
    QuestRelationsMapBounds irbounds;                       // InvolvedRelations (quest-finisher) (任务交付关系)

    switch (questgiver->GetTypeId())
    {
        case TYPEID_UNIT:
        {
            rbounds = sObjectMgr.GetCreatureQuestRelationsMapBounds(questgiver->GetEntry());
            irbounds = sObjectMgr.GetCreatureQuestInvolvedRelationsMapBounds(questgiver->GetEntry());
            break;
        }
        case TYPEID_GAMEOBJECT:
        {
            rbounds = sObjectMgr.GetGOQuestRelationsMapBounds(questgiver->GetEntry());
            irbounds = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(questgiver->GetEntry());
            break;
        }
        default:
            // it's impossible, but check ^) (理论上不会出现的对象类型，防御性日志)
            sLog.outError("Warning: GetDialogStatus called for unexpected type %u", questgiver->GetTypeId());
            return DIALOG_STATUS_NONE;
    }

    // Check markings for quest-finisher (先检查作为交付者的可交/进行中任务标记)
    for (QuestRelationsMap::const_iterator itr = irbounds.first; itr != irbounds.second; ++itr)
    {
        uint32 dialogStatusNew = DIALOG_STATUS_NONE;
        uint32 quest_id = itr->second;
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
            continue;

        QuestStatus status = pPlayer->GetQuestStatus(quest_id);

        if (status == QUEST_STATUS_COMPLETE && !pPlayer->GetQuestRewardStatus(quest_id))
            dialogStatusNew = pQuest->IsRepeatable() && !pQuest->IsDailyOrWeekly() ? DIALOG_STATUS_REWARD_REP : DIALOG_STATUS_REWARD;
        else if (pQuest->IsAutoComplete() && pPlayer->CanTakeQuest(pQuest, false))
            dialogStatusNew = pQuest->IsRepeatable() ? pQuest->IsDailyOrWeekly() ? DIALOG_STATUS_AVAILABLE_REP : DIALOG_STATUS_REWARD_REP : DIALOG_STATUS_AVAILABLE;
        else if (status == QUEST_STATUS_INCOMPLETE)
            dialogStatusNew = DIALOG_STATUS_INCOMPLETE;

        if (dialogStatusNew > dialogStatus)
            dialogStatus = dialogStatusNew;
    }

    // check markings for quest-giver (再检查作为发布者的可接/不可接任务标记，取最高优先级)
    for (QuestRelationsMap::const_iterator itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        uint32 dialogStatusNew = DIALOG_STATUS_NONE;
        uint32 quest_id = itr->second;
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
            continue;

        QuestStatus status = pPlayer->GetQuestStatus(quest_id);

        // A failed quest is offered again. For all other cases the mark is handled either at some place else, or with involved-relations already
        // 已失败任务允许重新接取；其余状态已在交付关系或其它分支处理
        if (status == QUEST_STATUS_NONE || status == QUEST_STATUS_FAILED)
        {
            if (pPlayer->CanSeeStartQuest(pQuest))
            {
                if (pPlayer->SatisfyQuestLevel(pQuest, false))
                {
                    int32 lowLevelDiff = sWorld.getConfig(CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF);

                    if (pQuest->IsAutoComplete())
                    {
                        dialogStatusNew = DIALOG_STATUS_REWARD_REP;
                    }
                    else if (lowLevelDiff < 0 || pPlayer->GetLevel() <= pPlayer->GetQuestLevelForPlayer(pQuest) + uint32(lowLevelDiff))
                    {
                        if (pQuest->HasQuestFlag(QUEST_FLAGS_DAILY) || pQuest->HasQuestFlag(QUEST_FLAGS_WEEKLY))
                            dialogStatusNew = DIALOG_STATUS_AVAILABLE_REP;
                        else
                            dialogStatusNew = DIALOG_STATUS_AVAILABLE;
                    }
                    else
                        dialogStatusNew = DIALOG_STATUS_CHAT;
                }
                else
                    dialogStatusNew = DIALOG_STATUS_UNAVAILABLE;
            }
        }

        if (dialogStatusNew > dialogStatus)
            dialogStatus = dialogStatusNew;
    }

    return dialogStatus;
}

// 处理批量查询附近 NPC 任务状态请求（CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY）
void WorldSession::HandleQuestgiverStatusMultipleQuery(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY");

    // TODO: Add parsing
    _player->SendQuestGiverStatusMultiple();
}

// 校验玩家当前是否可与指定任务发布者（NPC、游戏对象或任务物品）安全交互
bool WorldSession::CanInteractWithQuestGiver(ObjectGuid guid, char const* descr) const
{
    if (guid.IsCreature())
    {
        Creature* pCreature = _player->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_QUESTGIVER);
        if (!pCreature)
        {
            DEBUG_LOG("WORLD: %s - %s cannot interact with %s.", descr, _player->GetGuidStr().c_str(), guid.GetString().c_str());
            return false;
        }
    }
    else if (guid.IsGameObject())
    {
        GameObject* pGo = _player->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_QUESTGIVER);
        if (!pGo)
        {
            DEBUG_LOG("WORLD: %s - %s cannot interact with %s.", descr, _player->GetGuidStr().c_str(), guid.GetString().c_str());
            return false;
        }
    }
    else if (!_player->IsAlive())
    {
        DEBUG_LOG("WORLD: %s - %s is dead, requested guid was %s", descr, _player->GetGuidStr().c_str(), guid.GetString().c_str());
        return false;
    }

    return true;
}
