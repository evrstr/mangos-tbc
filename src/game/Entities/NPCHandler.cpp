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
#include "Tools/Language.h"
#include "Database/DatabaseEnv.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "Server/Opcodes.h"
#include "Log/Log.h"
#include "Globals/ObjectMgr.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Entities/Player.h"
#include "Entities/GossipDef.h"
#include "DBScripts/ScriptMgr.h"
#include "Entities/Creature.h"
#include "Entities/Pet.h"
#include "Guilds/Guild.h"
#include "Guilds/GuildMgr.h"
#include "Chat/Chat.h"

// 兽栏操作结果码枚举
enum StableResultCode
{
    STABLE_ERR_MONEY        = 0x01,                         // "you don't have enough money" (金币不足)
    STABLE_ERR_STABLE       = 0x06,                         // currently used in most fail cases (通用兽栏操作失败)
    STABLE_SUCCESS_STABLE   = 0x08,                         // stable success (寄存宠物成功)
    STABLE_SUCCESS_UNSTABLE = 0x09,                         // unstable/swap success (取出或交换宠物成功)
    STABLE_SUCCESS_BUY_SLOT = 0x0A,                         // buy slot success (购买兽栏栏位成功)
};

// 处理公会徽章设计 NPC 激活请求（MSG_TABARDVENDOR_ACTIVATE）
void WorldSession::HandleTabardVendorActivateOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TABARDDESIGNER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTabardVendorActivateOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    SendTabardVendorActivate(guid);
}

// 向客户端发送打开公会徽章设计界面数据包
void WorldSession::SendTabardVendorActivate(ObjectGuid guid) const
{
    WorldPacket data(MSG_TABARDVENDOR_ACTIVATE, 8);
    data << ObjectGuid(guid);
    SendPacket(data);
}

// 处理银行管理员交互请求（CMSG_BANKER_ACTIVATE）
void WorldSession::HandleBankerActivateOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    DEBUG_LOG("WORLD: Received opcode CMSG_BANKER_ACTIVATE");

    recv_data >> guid;

    if (!CheckBanker(guid))
        return;

    SendShowBank(guid);
}

// 向客户端发送打开银行窗口数据包（SMSG_SHOW_BANK）
void WorldSession::SendShowBank(ObjectGuid guid) const
{
    WorldPacket data(SMSG_SHOW_BANK, 8);
    data << ObjectGuid(guid);
    SendPacket(data);
}

// 处理请求训练师可学法术列表操作码（CMSG_TRAINER_LIST）
void WorldSession::HandleTrainerListOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    SendTrainerList(guid);
}


// 将单条训练师法术条目写入 SMSG_TRAINER_LIST 数据包
static void SendTrainerSpellHelper(WorldPacket& data, TrainerSpell const* tSpell, TrainerSpellState state, float fDiscountMod, bool /*can_learn_primary_prof*/, uint32 reqLevel)
{
    bool primary_prof_first_rank = sSpellMgr.IsPrimaryProfessionFirstRankSpell(tSpell->learnedSpell.size() > 1 ? tSpell->learnedSpell[1] : tSpell->learnedSpell[0]);

    SpellChainNode const* chain_node = sSpellMgr.GetSpellChainNode(tSpell->learnedSpell.size() > 1 ? tSpell->learnedSpell[1] : tSpell->learnedSpell[0]);

    data << uint32(tSpell->spell);                      // learned spell (or cast-spell in profession case) (训练法术 ID，专业技能为触发施放法术)
    data << uint8(state == TRAINER_SPELL_GREEN_DISABLED ? TRAINER_SPELL_GREEN : state);
    data << uint32(floor(tSpell->spellCost * fDiscountMod));

    data << uint32(0); // spells don't cost talent points (法术不消耗天赋点数)
    data << uint32(primary_prof_first_rank ? 1 : 0);    // must be equal prev. field to have learn button in enabled state (与前一字段相等时学习按钮才处于可用状态)
    data << uint8(reqLevel);
    data << uint32(tSpell->reqSkill);
    data << uint32(tSpell->reqSkillValue);
    if (tSpell->reqAbility[0])
        data << *tSpell->reqAbility[0];
    else
        data << uint32(!tSpell->IsCastable() && chain_node ? (chain_node->prev ? chain_node->prev : chain_node->req) : 0);
    if (tSpell->reqAbility[1])
        data << *tSpell->reqAbility[1];
    else
        data << uint32(!tSpell->IsCastable() && chain_node && chain_node->prev ? chain_node->req : 0);
    if (tSpell->reqAbility[2])
        data << *tSpell->reqAbility[2];
    else
        data << uint32(0);
}

// 组装并下发训练师可学法术列表（SMSG_TRAINER_LIST）
void WorldSession::SendTrainerList(ObjectGuid guid) const
{
    DEBUG_LOG("WORLD: SendTrainerList");

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: SendTrainerList - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // trainer list loaded at check; (校验职业/专业训练资格并加载训练法术列表)
    if (!unit->IsTrainerOf(_player, true))
        return;

    CreatureInfo const* ci = unit->GetCreatureInfo();
    if (!ci)
        return;

    TrainerSpellData const* cSpells = unit->GetTrainerSpells();
    TrainerSpellData const* tSpells = unit->GetTrainerTemplateSpells();

    if (!cSpells && !tSpells)
    {
        DEBUG_LOG("WORLD: SendTrainerList - Training spells not found for %s", guid.GetString().c_str());
        return;
    }

    uint32 maxcount = (cSpells ? cSpells->spellList.size() : 0) + (tSpells ? tSpells->spellList.size() : 0);
    uint32 trainer_type = cSpells && cSpells->trainerType ? cSpells->trainerType : (tSpells ? tSpells->trainerType : 0);

    // 加载训练师问候语及其本地化文本
    std::string strTitle;
    if (TrainerGreeting const* data = sObjectMgr.GetTrainerGreetingData(guid.GetEntry()))
    {
        strTitle = data->text;
        int loc_idx = GetSessionDbLocaleIndex();
        sObjectMgr.GetTrainerGreetingLocales(guid.GetEntry(), loc_idx, &strTitle);
    }
    else
        strTitle = GetMangosString(LANG_NPC_TAINER_HELLO); // Fallback in case no trainer greeting found (未配置问候语时使用默认语言文本)

    WorldPacket data(SMSG_TRAINER_LIST, 8 + 4 + 4 + maxcount * 38 + strTitle.size() + 1);
    data << ObjectGuid(guid);
    data << uint32(trainer_type);

    size_t count_pos = data.wpos();
    data << uint32(maxcount);

    // reputation discount (计算声望折扣系数)
    float fDiscountMod = _player->GetReputationPriceDiscount(unit);
    bool can_learn_primary_prof = GetPlayer()->GetFreePrimaryProfessionPoints() > 0;

    uint32 count = 0;

    // 写入生物专属训练法术表
    if (cSpells)
    {
        for (const auto& itr : cSpells->spellList)
        {
            TrainerSpell const* tSpell = &itr.second;

            uint32 reqLevel = 0;
            if (!_player->IsSpellFitByClassAndRace(tSpell->spell, &reqLevel))
                continue;

            if (tSpell->conditionId && !sObjectMgr.IsConditionSatisfied(tSpell->conditionId, GetPlayer(), unit->GetMap(), unit, CONDITION_FROM_TRAINER))
                continue;

            reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);

            TrainerSpellState state = _player->GetTrainerSpellState(tSpell, reqLevel);

            SendTrainerSpellHelper(data, tSpell, state, fDiscountMod, can_learn_primary_prof, reqLevel);

            ++count;
        }
    }

    // 写入训练师模板法术表
    if (tSpells)
    {
        for (const auto& itr : tSpells->spellList)
        {
            TrainerSpell const* tSpell = &itr.second;

            uint32 reqLevel = 0;
            if (!_player->IsSpellFitByClassAndRace(tSpell->spell, &reqLevel))
                continue;

            if (tSpell->conditionId && !sObjectMgr.IsConditionSatisfied(tSpell->conditionId, GetPlayer(), unit->GetMap(), unit, CONDITION_FROM_TRAINER))
                continue;

            reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);

            TrainerSpellState state = _player->GetTrainerSpellState(tSpell, reqLevel);

            SendTrainerSpellHelper(data, tSpell, state, fDiscountMod, can_learn_primary_prof, reqLevel);

            ++count;
        }
    }

    data << strTitle;

    data.put<uint32>(count_pos, count);
    SendPacket(data);
}

// 处理向训练师购买/学习法术操作码（CMSG_TRAINER_BUY_SPELL）
void WorldSession::HandleTrainerBuySpellOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 spellId = 0;

    recv_data >> guid >> spellId;
    DEBUG_LOG("WORLD: Received opcode CMSG_TRAINER_BUY_SPELL Trainer: %s, learn spell id is: %u", guid.GetString().c_str(), spellId);

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTrainerBuySpellOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    if (!unit->IsTrainerOf(_player, true))
        return;

    // check present spell in trainer spell list (查找法术是否存在于训练师专属表或模板表中)
    TrainerSpellData const* cSpells = unit->GetTrainerSpells();
    TrainerSpellData const* tSpells = unit->GetTrainerTemplateSpells();

    if (!cSpells && !tSpells)
        return;

    // Try find spell in npc_trainer (先在 npc_trainer 查找)
    TrainerSpell const* trainer_spell = cSpells ? cSpells->Find(spellId) : nullptr;

    // Not found, try find in npc_trainer_template (未找到则回退至 npc_trainer_template)
    if (!trainer_spell && tSpells)
        trainer_spell = tSpells->Find(spellId);

    // Not found anywhere, cheating? (未找到则疑似作弊或异常请求)
    if (!trainer_spell)
        return;

    // can't be learn, cheat? Or double learn with lags... (校验职业种族、等级及前置状态是否为绿色可学)
    uint32 reqLevel = 0;
    if (!_player->IsSpellFitByClassAndRace(trainer_spell->spell, &reqLevel))
        return;

    reqLevel = trainer_spell->isProvidedReqLevel ? trainer_spell->reqLevel : std::max(reqLevel, trainer_spell->reqLevel);
    if (_player->GetTrainerSpellState(trainer_spell, reqLevel) != TRAINER_SPELL_GREEN)
        return;

    // apply reputation discount (计算声望折后价格)
    uint32 nSpellCost = uint32(floor(trainer_spell->spellCost * _player->GetReputationPriceDiscount(unit)));

    // check money requirement (校验玩家金币是否足够)
    if (_player->GetMoney() < nSpellCost)
        return;

    _player->ModifyMoney(-int32(nSpellCost));

    SendPlaySpellVisual(guid, 0xB3);                        // visual effect on trainer (播放训练师施法视觉特效)

    WorldPacket data(SMSG_PLAY_SPELL_IMPACT, 8 + 4);        // visual effect on player (播放玩家受法视觉特效)
    data << _player->GetObjectGuid();
    data << uint32(0x016A);                                 // index from SpellVisualKit.dbc (SpellVisualKit.dbc 特效索引)
    SendPacket(data);

    // learn explicitly or cast explicitly (直接学习法术或通过触发施法授予技能/配方)
    // TODO - Are these spells really cast correctly this way?
    if (trainer_spell->IsCastable())
        _player->CastSpell(_player, trainer_spell->spell, TRIGGERED_OLD_TRIGGERED);
    else
        _player->learnSpell(spellId, false);

    data.Initialize(SMSG_TRAINER_BUY_SUCCEEDED, 12);
    data << ObjectGuid(guid);
    data << uint32(spellId);                                // should be same as in packet from client (需与客户端请求中的法术 ID 一致)
    SendPacket(data);
}

// 处理点击 NPC 开启对话请求（CMSG_GOSSIP_HELLO）
void WorldSession::HandleGossipHelloOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GOSSIP_HELLO");

    ObjectGuid guid;
    recv_data >> guid;

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleGossipHelloOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // Stop the npc if moving (交互时暂停 NPC 路径点移动)
    if (uint32 pauseTimer = pCreature->GetInteractionPauseTimer())
        pCreature->GetMotionMaster()->PauseWaypoints(pauseTimer);

    if (pCreature->isSpiritGuide())
        pCreature->SendAreaSpiritHealerQueryOpcode(_player);

    // 优先交由脚本处理，未拦截则准备并发送默认 Gossip 菜单
    if (!sScriptDevAIMgr.OnGossipHello(_player, pCreature))
    {
        _player->PrepareGossipMenu(pCreature, pCreature->GetDefaultGossipMenuId());
        _player->SendPreparedGossip(pCreature);
    }
}

// 处理玩家选择 Gossip 对话选项（CMSG_GOSSIP_SELECT_OPTION）
void WorldSession::HandleGossipSelectOptionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_GOSSIP_SELECT_OPTION");

    uint32 gossipListId;
    uint32 menuId;
    ObjectGuid guid;
    std::string code;

    recv_data >> guid >> menuId >> gossipListId;

    // 若该选项需要用户输入密语代码，则额外读取 code 字符串
    if (_player->GetPlayerMenu()->GossipOptionCoded(gossipListId))
    {
        recv_data >> code;
        DEBUG_LOG("Gossip code: %s", code.c_str());
    }

    uint32 sender = _player->GetPlayerMenu()->GossipOptionSender(gossipListId);
    uint32 action = _player->GetPlayerMenu()->GossipOptionAction(gossipListId);

    // 区分生物与游戏对象两种交互目标并派发至脚本或默认处理逻辑
    if (guid.IsAnyTypeCreature())
    {
        Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);

        if (!pCreature)
        {
            DEBUG_LOG("WORLD: HandleGossipSelectOptionOpcode - %s not found or you can't interact with it.", guid.GetString().c_str());
            return;
        }

        if (!sScriptDevAIMgr.OnGossipSelect(_player, pCreature, sender, action, code.empty() ? nullptr : code.c_str()))
            _player->OnGossipSelect(pCreature, gossipListId, menuId);
    }
    else if (guid.IsGameObject())
    {
        GameObject* pGo = GetPlayer()->GetGameObjectIfCanInteractWith(guid);

        if (!pGo)
        {
            DEBUG_LOG("WORLD: HandleGossipSelectOptionOpcode - %s not found or you can't interact with it.", guid.GetString().c_str());
            return;
        }

        if (!sScriptDevAIMgr.OnGossipSelect(_player, pGo, sender, action, code.empty() ? nullptr : code.c_str()))
            _player->OnGossipSelect(pGo, gossipListId, menuId);
    }
}

// 处理灵魂医者复活交互请求（CMSG_SPIRIT_HEALER_ACTIVATE）
void WorldSession::HandleSpiritHealerActivateOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_SPIRIT_HEALER_ACTIVATE");

    ObjectGuid guid;

    recv_data >> guid;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_SPIRITHEALER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleSpiritHealerActivateOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    SendSpiritResurrect();
}

// 执行灵魂医者复活：半血半蓝复活、扣除 25% 全身耐久并视需要传送至尸体最近墓地
void WorldSession::SendSpiritResurrect() const
{
    _player->ResurrectPlayer(0.5f, true);

    _player->DurabilityLossAll(0.25f, true);

    // get corpse nearest graveyard (查找距离尸体最近的墓地)
    WorldSafeLocsEntry const* corpseGrave = nullptr;
    Corpse* corpse = _player->GetCorpse();
    if (corpse)
        corpseGrave = _player->GetMap()->GetGraveyardManager().GetClosestGraveYard(corpse->GetPositionX(), corpse->GetPositionY(), corpse->GetPositionZ(), corpse->GetMapId(), _player->GetTeam());

    // now can spawn bones (将玩家尸体转为白骨)
    _player->SpawnCorpseBones();

    // teleport to nearest from corpse graveyard, if different from nearest to player ghost (若尸体最近墓地与当前灵魂位置墓地不同，则传送过去)
    if (corpseGrave)
    {
        WorldSafeLocsEntry const* ghostGrave = _player->GetMap()->GetGraveyardManager().GetClosestGraveYard(_player->GetPositionX(), _player->GetPositionY(), _player->GetPositionZ(), _player->GetMapId(), _player->GetTeam());

        if (corpseGrave != ghostGrave)
            _player->TeleportTo(corpseGrave->map_id, corpseGrave->x, corpseGrave->y, corpseGrave->z, corpseGrave->o);
        // or update at original position (否则在原地重新同步创建对象)
        else
        {
            _player->GetMap()->AddUpdateCreateObject(_player);
        }
    }
    // or update at original position (无尸体记录时原地刷新视野)
    else
    {
        _player->GetMap()->AddUpdateCreateObject(_player);
    }
}

// 处理旅店老板绑定炉石请求（CMSG_BINDER_ACTIVATE）
void WorldSession::HandleBinderActivateOpcode(WorldPacket& recv_data)
{
    ObjectGuid npcGuid;
    recv_data >> npcGuid;

    if (!GetPlayer()->IsInWorld() || !GetPlayer()->IsAlive())
        return;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_INNKEEPER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleBinderActivateOpcode - %s not found or you can't interact with him.", npcGuid.GetString().c_str());
        return;
    }

    SendBindPoint(unit);
}

// 为玩家绑定炉石点（禁止在副本地图内绑定）
void WorldSession::SendBindPoint(Creature* npc) const
{
    // prevent set homebind to instances in any case (任何情况下都禁止将炉石绑定到副本地图)
    if (GetPlayer()->GetMap()->Instanceable())
        return;

    // send spell for bind 3286 bind magic (施放炉石绑定法术 3286)
    npc->CastSpell(_player, 3286, TRIGGERED_OLD_TRIGGERED);                    // Bind

    WorldPacket data(SMSG_TRAINER_BUY_SUCCEEDED, (8 + 4));
    data << npc->GetObjectGuid();
    data << uint32(3286);                                   // Bind
    SendPacket(data);

    _player->GetPlayerMenu()->CloseGossip();
}

// 处理查询兽栏宠物列表请求（MSG_LIST_STABLED_PETS）
void WorldSession::HandleListStabledPetsOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv MSG_LIST_STABLED_PETS");
    ObjectGuid npcGUID;

    recv_data >> npcGUID;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(npcGUID, UNIT_NPC_FLAG_STABLEMASTER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleListStabledPetsOpcode - %s not found or you can't interact with him.", npcGUID.GetString().c_str());
        return;
    }

    SendStablePet(npcGUID);
}

// 组装并下发当前及已寄存兽栏宠物列表（MSG_LIST_STABLED_PETS）
void WorldSession::SendStablePet(ObjectGuid guid) const
{
    DEBUG_LOG("WORLD: Recv MSG_LIST_STABLED_PETS Send.");

    WorldPacket data(MSG_LIST_STABLED_PETS, 200);           // guess size (预估包长)
    data << guid;

    Pet* pet = _player->GetPet();

    size_t wpos = data.wpos();
    data << uint8(0);                                       // place holder for slot show number (宠物数量占位字节)

    data << uint8(GetPlayer()->m_stableSlots);

    uint8 num = 0;                                          // counter for place holder (实际写入宠物计数)
    PetSaveMode firstSlot = PET_SAVE_FIRST_STABLE_SLOT;     // have to be changed to PET_SAVE_AS_CURRENT if pet is currently temp unsummoned (若当前宠物处于临时收起状态，需从当前槽位开始查库)

    // not let move dead pet in slot (优先写入当前出战或未寄存的猎人宠物)
    if (pet && pet->IsAlive() && pet->getPetType() == HUNTER_PET)
    {
        data << uint32(pet->GetCharmInfo()->GetPetNumber());
        data << uint32(pet->GetEntry());
        data << uint32(pet->GetLevel());
        data << pet->GetName();                             // petname (宠物名称)
        data << uint32(pet->GetLoyaltyLevel());             // loyalty (忠诚度等级)
        data << uint8(0x01);                                // client slot 1 == current pet (0) (客户端槽位 1 表示当前激活宠物)
        ++num;
    }
    else
    {
        if(_player->GetTemporaryUnsummonedPetNumber()) // temporary unsummon - mount (如骑乘导致的临时收起宠物)
            firstSlot = PET_SAVE_AS_CURRENT;
        else
        {
            auto queryResult = CharacterDatabase.PQuery("SELECT owner, id, entry, level, name, loyalty FROM character_pet WHERE owner = '%u' AND slot = '%u' ORDER BY slot",
                _player->GetGUIDLow(), uint32(PET_SAVE_NOT_IN_SLOT));

            if (queryResult) // dismissed pet (已解散但未入兽栏的宠物)
            {
                do
                {
                    Field* fields = queryResult->Fetch();

                    data << uint32(fields[1].GetUInt32());          // petnumber (宠物编号)
                    data << uint32(fields[2].GetUInt32());          // creature entry (生物模板 ID)
                    data << uint32(fields[3].GetUInt32());          // level (等级)
                    data << fields[4].GetString();                  // name (名称)
                    data << uint32(fields[5].GetUInt32());          // loyalty (忠诚度)
                    data << uint8(0x03);                            // slot (不在兽栏的待召回槽位)

                    ++num;
                } while (queryResult->NextRow());
            }
        }
    }

    //                                                  0      1     2   3      4      5        6
    auto queryResult = CharacterDatabase.PQuery("SELECT owner, slot, id, entry, level, loyalty, name FROM character_pet WHERE owner = '%u' AND slot >= '%u' AND slot <= '%u' ORDER BY slot",
                          _player->GetGUIDLow(), uint32(firstSlot), uint32(PET_SAVE_LAST_STABLE_SLOT));

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            data << uint32(fields[2].GetUInt32());          // petnumber (宠物编号)
            data << uint32(fields[3].GetUInt32());          // creature entry (生物模板 ID)
            data << uint32(fields[4].GetUInt32());          // level (等级)
            data << fields[6].GetString();                  // name (名称)
            data << uint32(fields[5].GetUInt32());          // loyalty (忠诚度)
            data << uint8(fields[1].GetUInt32() + 1);       // slot (数据库槽位转客户端 1-based 槽位)

            ++num;
        }
        while (queryResult->NextRow());
    }

    data.put<uint8>(wpos, num);                             // set real data to placeholder (回填实际宠物数量)
    SendPacket(data);
}

// 发送兽栏操作结果通知（SMSG_STABLE_RESULT）
void WorldSession::SendStableResult(uint8 res) const
{
    WorldPacket data(SMSG_STABLE_RESULT, 1);
    data << uint8(res);
    SendPacket(data);
}

// 校验兽栏管理员交互权限（支持自身 GUID 对应 GM 命令或兽栏管理员 NPC）
bool WorldSession::CheckStableMaster(ObjectGuid guid) const
{
    // spell case or GM (法术触发或 GM 命令场景)
    if (guid == GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command (仅当玩家有权执行 stable 命令时放行)
        if (!ChatHandler(GetPlayer()).FindCommand("stable"))
        {
            DEBUG_LOG("%s attempt open stable in cheating way.", guid.GetString().c_str());
            return false;
        }
    }
    // stable master case (常规兽栏管理员 NPC 交互校验)
    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_STABLEMASTER))
        {
            DEBUG_LOG("Stablemaster %s not found or you can't interact with him.", guid.GetString().c_str());
            return false;
        }
    }

    return true;
}

// 处理将当前猎人宠物寄存到兽栏请求（CMSG_STABLE_PET）
void WorldSession::HandleStablePet(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_STABLE_PET");
    ObjectGuid npcGUID;

    recv_data >> npcGUID;

    if (!GetPlayer()->IsAlive())
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (!CheckStableMaster(npcGUID))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    Pet* pet = _player->GetPet();

    // can't place in stable dead pet (校验存活及是否为猎人宠物)
    if (pet)
    {
        bool stop = false;
        if (!pet->IsAlive())
        {
            _player->SendPetTameFailure(PETTAME_DEAD);
            stop = true;
        }

        if (!stop && pet->getPetType() != HUNTER_PET)
        {
            _player->SendPetTameFailure(PETTAME_INVALIDCREATURE);
            stop = true;
        }

        if (stop)
        {
            SendStableResult(STABLE_ERR_STABLE);
            return;
        }
    }
    else
    {
        SpellCastResult loadResult = Pet::TryLoadFromDB(_player, 0, 0, _player->GetTemporaryUnsummonedPetNumber() != 0, HUNTER_PET);
        if (loadResult != SPELL_CAST_OK)
        {
            if (loadResult == SPELL_FAILED_TARGETS_DEAD)
                _player->SendPetTameFailure(PETTAME_DEAD);

            if (loadResult == SPELL_FAILED_BAD_TARGETS)
                _player->SendPetTameFailure(PETTAME_INVALIDCREATURE);

            SendStableResult(STABLE_ERR_STABLE);
            return;
        }
    }

    uint32 free_slot = 1;

    // 按槽位升序查找第一个空闲的兽栏栏位
    auto queryResult = CharacterDatabase.PQuery("SELECT owner,slot,id FROM character_pet WHERE owner = '%u'  AND slot >= '%u' AND slot <= '%u' ORDER BY slot ",
                          _player->GetGUIDLow(), PET_SAVE_FIRST_STABLE_SLOT, PET_SAVE_LAST_STABLE_SLOT);
    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            uint32 slot = fields[1].GetUInt32();

            // slots ordered in query, and if not equal then free (有序槽位出现跳号即表示该槽位空闲)
            if (slot != free_slot)
                break;

            // this slot not free, skip (当前槽位已占用，继续向后查找)
            ++free_slot;
        }
        while (queryResult->NextRow());
    }

    if (free_slot > 0 && free_slot <= GetPlayer()->m_stableSlots)
    {
        if (pet)
            pet->Unsummon(PetSaveMode(free_slot), _player);
        else
        {
            // change pet slot directly in database (宠物当前处于临时收起/解散状态时直接更新数据库槽位)
            CharacterDatabase.BeginTransaction();
            static SqlStatementID ChangePetSlot_ID;
            SqlStatement ChangePetSlot = CharacterDatabase.CreateStatement(ChangePetSlot_ID, "UPDATE character_pet SET slot = ? WHERE owner = ? AND slot = ? ");
            ChangePetSlot.PExecute(free_slot, _player->GetObjectGuid().GetCounter(), uint32(_player->GetTemporaryUnsummonedPetNumber() ? PET_SAVE_AS_CURRENT : PET_SAVE_NOT_IN_SLOT));
            CharacterDatabase.CommitTransaction();
        }
        SendStableResult(STABLE_SUCCESS_STABLE);
        _player->SetTemporaryUnsummonedPetNumber(0);
    }
    else
        SendStableResult(STABLE_ERR_STABLE);
}

// 处理从兽栏取出指定宠物请求（CMSG_UNSTABLE_PET）
void WorldSession::HandleUnstablePet(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_UNSTABLE_PET.");
    ObjectGuid npcGUID;
    uint32 petnumber;

    recv_data >> npcGUID >> petnumber;

    if (!CheckStableMaster(npcGUID))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    uint32 creature_id = 0;
    uint32 slot = 0;

    auto queryResult = CharacterDatabase.PQuery("SELECT entry, slot FROM character_pet WHERE owner = '%u' AND id = '%u' AND slot >='%u' AND slot <= '%u'",
                          _player->GetGUIDLow(), petnumber, PET_SAVE_FIRST_STABLE_SLOT, PET_SAVE_LAST_STABLE_SLOT);
    if (queryResult)
    {
        Field* fields = queryResult->Fetch();
        creature_id   = fields[0].GetUInt32();
        slot          = fields[1].GetUInt32();
    }

    if (!creature_id)
    {
        sLog.outError("HandleUnstablePet> %s trying to unstable a pet with invalid creature id!", _player->GetGuidStr().c_str());
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (slot == uint32(PET_SAVE_AS_CURRENT))
    {
        sLog.outError("HandleUnstablePet> %s trying to unstable current pet!", _player->GetGuidStr().c_str());
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(creature_id);
    if (!creatureInfo || !creatureInfo->isTameable())
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // found if a pet is actually summoned (若当前已有出战宠物，先将其存入目标兽栏槽位)
    Pet* pet = _player->GetPet();
    if (pet)
    {
        bool stop = false;
        if (!pet->IsAlive())
        {
            _player->SendPetTameFailure(PETTAME_DEAD);
            stop = true;
        }

        if (!stop && pet->getPetType() != HUNTER_PET)
        {
            _player->SendPetTameFailure(PETTAME_ANOTHERSUMMONACTIVE);
            stop = true;
        }

        if (stop)
        {
            SendStableResult(STABLE_ERR_STABLE);
            return;
        }

        pet->Unsummon(PetSaveMode(slot), _player);
    }
    else
    {
        // try to find if pet is actually temporary unsummoned (检查是否存在临时收起或已解散宠物需先换入槽位)
        SpellCastResult loadResult = Pet::TryLoadFromDB(_player, 0, 0, _player->GetTemporaryUnsummonedPetNumber() != 0, HUNTER_PET);
        if (loadResult != SPELL_CAST_OK)
        {
            if (loadResult == SPELL_FAILED_TARGETS_DEAD)
            {
                _player->SendPetTameFailure(PETTAME_DEAD);

                sLog.outError("UnStablePet> trying to unstable a dead pet. Should not be possible!");
                return;
            }

            if (loadResult == SPELL_FAILED_BAD_TARGETS)
            {
                _player->SendPetTameFailure(PETTAME_ANOTHERSUMMONACTIVE);
                SendStableResult(STABLE_ERR_STABLE);
                return;
            }
        }
        else
        {
            // change pet slot directly in database (直接将数据库中的原宠物槽位修改为腾出的目标槽位)
            CharacterDatabase.BeginTransaction();
            static SqlStatementID ChangePetSlot_ID;
            SqlStatement ChangePetSlot = CharacterDatabase.CreateStatement(ChangePetSlot_ID, "UPDATE character_pet SET slot = ? WHERE owner = ? AND slot = ? ");
            ChangePetSlot.PExecute(slot, _player->GetObjectGuid().GetCounter(), uint32(_player->GetTemporaryUnsummonedPetNumber() ? PET_SAVE_AS_CURRENT : PET_SAVE_NOT_IN_SLOT));
            CharacterDatabase.CommitTransaction();
            _player->SetTemporaryUnsummonedPetNumber(0);
        }
    }

    Pet* newpet = new Pet(HUNTER_PET);
    if (!newpet->LoadPetFromDB(_player, newpet->GetPetSpawnPosition(_player), creature_id, petnumber))
    {
        delete newpet;
        newpet = nullptr;

        // load failed but it may be normal if the pet is set as temporary unsummoned by any reason (若因骑乘等原因导致宠物仍处于临时收起状态则属正常)
        if (!_player->GetTemporaryUnsummonedPetNumber())
        {
            // load really failed (确实加载失败则通知客户端错误)
            SendStableResult(STABLE_ERR_STABLE);
            return;
        }
    }

    SendStableResult(STABLE_SUCCESS_UNSTABLE);
}

// 处理购买额外兽栏栏位请求（CMSG_BUY_STABLE_SLOT）
void WorldSession::HandleBuyStableSlot(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_BUY_STABLE_SLOT.");
    ObjectGuid npcGUID;

    recv_data >> npcGUID;

    if (!CheckStableMaster(npcGUID))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (GetPlayer()->m_stableSlots < MAX_PET_STABLES)
    {
        StableSlotPricesEntry const* SlotPrice = sStableSlotPricesStore.LookupEntry(GetPlayer()->m_stableSlots + 1);
        if (_player->GetMoney() >= SlotPrice->Price)
        {
            ++GetPlayer()->m_stableSlots;
            _player->ModifyMoney(-int32(SlotPrice->Price));
            SendStableResult(STABLE_SUCCESS_BUY_SLOT);
        }
        else
            SendStableResult(STABLE_ERR_MONEY);
    }
    else
        SendStableResult(STABLE_ERR_STABLE);
}

// 处理兽栏复活宠物请求（未实现占位）
void WorldSession::HandleStableRevivePet(WorldPacket& /* recv_data */)
{
    DEBUG_LOG("HandleStableRevivePet: Not implemented");
}

// 处理交换当前宠物与兽栏中宠物请求（CMSG_STABLE_SWAP_PET）
void WorldSession::HandleStableSwapPet(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_STABLE_SWAP_PET.");
    ObjectGuid npcGUID;
    uint32 pet_number;

    recv_data >> npcGUID >> pet_number;

    if (!CheckStableMaster(npcGUID))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    Pet* pet = _player->GetPet();
    if (pet)
    {
        bool stop = false;
        if (!pet->IsAlive())
        {
            _player->SendPetTameFailure(PETTAME_DEAD);
            stop = true;
        }

        if (!stop && pet->getPetType() != HUNTER_PET)
        {
            _player->SendPetTameFailure(PETTAME_INVALIDCREATURE);
            stop = true;
        }

        if (stop)
        {
            SendStableResult(STABLE_ERR_STABLE);
            return;
        }
    }
    else
    {
        SpellCastResult loadResult = Pet::TryLoadFromDB(_player, 0, 0, _player->GetTemporaryUnsummonedPetNumber() != 0, HUNTER_PET);
        if (loadResult != SPELL_CAST_OK)
        {
            if (loadResult == SPELL_FAILED_TARGETS_DEAD)
                _player->SendPetTameFailure(PETTAME_DEAD);

            if (loadResult == SPELL_FAILED_BAD_TARGETS)
                _player->SendPetTameFailure(PETTAME_INVALIDCREATURE);

            SendStableResult(STABLE_ERR_STABLE);
            return;
        }
    }

    // find swapped pet slot in stable (查询待交换宠物所在的兽栏槽位与生物模板 ID)
    auto queryResult = CharacterDatabase.PQuery("SELECT slot,entry FROM character_pet WHERE owner = '%u' AND id = '%u'",
                          _player->GetGUIDLow(), pet_number);
    if (!queryResult)
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    Field* fields = queryResult->Fetch();

    uint32 slot        = fields[0].GetUInt32();
    uint32 creature_id = fields[1].GetUInt32();

    if (!creature_id)
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(creature_id);
    if (!creatureInfo || !creatureInfo->isTameable())
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // move alive pet to slot or delete dead pet (将当前激活宠物移入目标兽栏槽位)
    if (pet)
        pet->Unsummon(PetSaveMode(slot), _player);
    else
    {
        // change pet slot directly in memory (当前未召唤出实体时直接修改数据库槽位)
        CharacterDatabase.BeginTransaction();
        static SqlStatementID ChangePetSlot_ID;
        SqlStatement ChangePetSlot = CharacterDatabase.CreateStatement(ChangePetSlot_ID, "UPDATE character_pet SET slot = ? WHERE owner = ? AND slot = ? ");
        ChangePetSlot.PExecute(slot, _player->GetObjectGuid().GetCounter(), uint32(_player->GetTemporaryUnsummonedPetNumber() ? PET_SAVE_AS_CURRENT : PET_SAVE_NOT_IN_SLOT));
        CharacterDatabase.CommitTransaction();
    }

    // summon unstabled pet (从数据库加载并召唤被换出的兽栏宠物)
    Pet* newpet = new Pet;
    if (!newpet->LoadPetFromDB(_player, newpet->GetPetSpawnPosition(_player), creature_id, pet_number))
    {
        delete newpet;

        // load failed but it may be normal if the pet is set as temporary unsummoned by any reason (若宠物因骑乘等保持临时收起状态则属正常)
        if (!_player->GetTemporaryUnsummonedPetNumber())
        {
            // load really failed
            SendStableResult(STABLE_ERR_STABLE);
            return;
        }
    }

    SendStableResult(STABLE_SUCCESS_UNSTABLE);
}

// 处理 NPC 修理单件或全部装备请求（CMSG_REPAIR_ITEM），支持公会银行资金修理
void WorldSession::HandleRepairItemOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_REPAIR_ITEM");

    ObjectGuid npcGuid;
    ObjectGuid itemGuid;
    uint8 guildBank;                                        // new in 2.3.2, bool that means from guild bank money (2.3.2 新增：是否使用公会银行资金修理)

    recv_data >> npcGuid >> itemGuid >> guildBank;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_REPAIR);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleRepairItemOpcode - %s not found or you can't interact with him.", npcGuid.GetString().c_str());
        return;
    }

    // reputation discount (计算声望修理折扣)
    float discountMod = _player->GetRepairPriceMultiplier(unit);

    uint32 TotalCost = 0;
    if (itemGuid)
    {
        DEBUG_LOG("ITEM: %s repair of %s", npcGuid.GetString().c_str(), itemGuid.GetString().c_str());

        Item* item = _player->GetItemByGuid(itemGuid);

        if (item)
            TotalCost = _player->DurabilityRepair(item->GetPos(), true, discountMod, (guildBank > 0));
    }
    else
    {
        DEBUG_LOG("ITEM: %s repair all items", npcGuid.GetString().c_str());

        TotalCost = _player->DurabilityRepairAll(true, discountMod, (guildBank > 0));
    }
    // 若使用公会银行修理，则记录公会银行日志并同步资金信息
    if (guildBank)
    {
        uint32 GuildId = _player->GetGuildId();
        if (!GuildId)
            return;
        Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
        if (!pGuild)
            return;
        pGuild->LogBankEvent(GUILD_BANK_LOG_REPAIR_MONEY, 0, _player->GetGUIDLow(), TotalCost);
        pGuild->SendMoneyInfo(this, _player->GetGUIDLow());
    }
}
