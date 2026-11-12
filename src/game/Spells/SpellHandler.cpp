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
#include "Server/DBCStores.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "Globals/ObjectMgr.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Log/Log.h"
#include "Server/Opcodes.h"
#include "Spells/Spell.h"
#include "DBScripts/ScriptMgr.h"
#include "Entities/Totem.h"
#include "Spells/SpellAuras.h"
#include "Loot/LootMgr.h"

// 处理客户端使用物品请求（CMSG_USE_ITEM）：校验物品可用性、绑定状态、目标并触发物品法术或脚本
void WorldSession::HandleUseItemOpcode(WorldPacket& recvPacket)
{
    uint8 bagIndex, slot;
    uint8 spell_index;                                      // item spell index which should be used (待使用的物品法术槽位索引)
    uint8 cast_count;                                       // next cast if exists (single or not) (施法序列计数)
    ObjectGuid itemGuid;

    recvPacket >> bagIndex >> slot >> spell_index >> cast_count >> itemGuid;

    // TODO: add targets.read() check
    Player* pUser = _player;

    // ignore for remote control state (处于远程控制他人状态时忽略)
    if (!pUser->IsSelfMover())
    {
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at not read packet tail (跳过未读包尾防止日志刷屏)
        return;
    }

    // 根据背包索引与格位查找物品并比对 GUID
    Item* pItem = pUser->GetItemByPos(bagIndex, slot);
    if (!pItem)
    {
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at not read packet tail
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    if (pItem->GetObjectGuid() != itemGuid)
    {
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at not read packet tail
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    DETAIL_LOG("WORLD: CMSG_USE_ITEM packet, bagIndex: %u, slot: %u, spell_index: %u , Item: %u, data length = %u", bagIndex, slot, spell_index, pItem->GetEntry(), (uint32)recvPacket.size());

    ItemPrototype const* proto = pItem->GetProto();
    if (!proto)
    {
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at not read packet tail
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, nullptr);
        return;
    }

    // some item classes can be used only in equipped state (非消耗类/需装备物品必须在已装备状态下才能使用)
    if (proto->InventoryType != INVTYPE_NON_EQUIP && !pItem->IsEquipped())
    {
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at not read packet tail
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, nullptr);
        return;
    }

    InventoryResult msg = pUser->CanUseItem(pItem);
    if (msg != EQUIP_ERR_OK)
    {
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at not read packet tail
        pUser->SendEquipError(msg, pItem, nullptr);
        return;
    }

    // not allow use item from trade (cheat way only) (禁止使用交易栏中的物品防作弊)
    if (pItem->IsInTrade())
    {
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at not read packet tail
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, nullptr);
        return;
    }

    // only allow conjured consumable, bandage, poisons (all should have the 2^21 item flag set in DB)
    // 竞技场内仅允许使用带 ITEM_FLAG_IGNORE_DEFAULT_ARENA_RESTRICTIONS 标记的消耗品（如造水面包/绷带/毒药）
    if (proto->Class == ITEM_CLASS_CONSUMABLE &&
            !(proto->Flags & ITEM_FLAG_IGNORE_DEFAULT_ARENA_RESTRICTIONS) &&
            pUser->InArena())
    {
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at not read packet tail
        pUser->SendEquipError(EQUIP_ERR_NOT_DURING_ARENA_MATCH, pItem, nullptr);
        return;
    }

    // 战斗中禁止使用带有非战斗法术的物品
    if (pUser->IsInCombat())
    {
        for (const auto& Spell : proto->Spells)
        {
            if (SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(Spell.SpellId))
            {
                if (IsNonCombatSpell(spellInfo))
                {
                    recvPacket.rpos(recvPacket.wpos());     // prevent spam at not read packet tail
                    pUser->SendEquipError(EQUIP_ERR_NOT_IN_COMBAT, pItem, nullptr);
                    return;
                }
            }
        }
    }

    // check also  BIND_WHEN_PICKED_UP and BIND_QUEST_ITEM for .additem or .additemset case by GM (not binded at adding to inventory)
    // 使用时绑定（以及 GM 命令添加但尚未绑定的拾绑/任务物品）在使用时自动灵魂绑定
    if (pItem->GetProto()->Bonding == BIND_WHEN_USE || pItem->GetProto()->Bonding == BIND_WHEN_PICKED_UP || pItem->GetProto()->Bonding == BIND_QUEST_ITEM)
    {
        if (!pItem->IsSoulBound())
        {
            pItem->SetState(ITEM_CHANGED, pUser);
            pItem->SetBinding(true);
        }
    }

    // 读取客户端传入的施法目标信息并更新目标指针
    SpellCastTargets targets;

    recvPacket >> targets.ReadForCaster(pUser);

    targets.Update(pUser);

    if (!pItem->IsTargetValidForItemUse(targets.getUnitTarget()))
    {
        // free gray item after use fail (使用失败时解除客户端物品置灰锁定状态)
        pUser->SendEquipError(EQUIP_ERR_NONE, pItem, nullptr);

        // search spell for spell error (查找对应的物品触发法术以发送正确错误码)
        uint32 spellid = 0;
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
            {
                spellid = proto->Spells[i].SpellId;
                break;
            }
        }

        // send spell error
        if (SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellid))
            Spell::SendCastResult(_player, spellInfo, cast_count, SPELL_FAILED_BAD_TARGETS);
        return;
    }

    // Note: If script stop casting it must send appropriate data to client to prevent stuck item in gray state.
    // 先尝试由 ScriptDev 脚本拦截处理，未拦截则走常规物品施法流程
    if (!sScriptDevAIMgr.OnItemUse(pUser, pItem, targets))
    {
        // no script or script not process request by self
        pUser->CastItemUseSpell(pItem, targets, cast_count, spell_index);
    }
}

#define OPEN_CHEST 11437
#define OPEN_SAFE 11535
#define OPEN_CAGE 11792
#define OPEN_BOOTY_CHEST 5107
#define OPEN_STRONGBOX 8517

// 处理客户端打开容器类物品或拆开礼物包装请求（CMSG_OPEN_ITEM）
void WorldSession::HandleOpenItemOpcode(WorldPacket& recvPacket)
{
    DETAIL_LOG("WORLD: CMSG_OPEN_ITEM packet, data length = " SIZEFMTD, recvPacket.size());

    uint8 bagIndex, slot;

    recvPacket >> bagIndex >> slot;

    DETAIL_LOG("bagIndex: %u, slot: %u", bagIndex, slot);

    Player* pUser = _player;

    // ignore for remote control state (远程控制他人时忽略)
    if (!pUser->IsSelfMover())
        return;

    Item* pItem = pUser->GetItemByPos(bagIndex, slot);
    if (!pItem)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    ItemPrototype const* proto = pItem->GetProto();
    if (!proto)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, nullptr);
        return;
    }

    if (proto->RequiredLevel > pUser->GetLevel())
    {
        pUser->SendEquipError(EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW, pItem, nullptr);
        return;
    }

    // locked item (校验带锁物品是否已解锁，若需开锁技能则拒绝直接开启)
    uint32 lockId = proto->LockID;
    if (lockId && !pItem->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED))
    {
        LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);

        if (!lockInfo)
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, pItem, nullptr);
            sLog.outError("WORLD::OpenItem: item [guid = %u] has an unknown lockId: %u!", pItem->GetGUIDLow(), lockId);
            return;
        }

        // required picklocking (仍需撬锁技能)
        if (lockInfo->Skill[1] || lockInfo->Skill[0])
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, pItem, nullptr);
            return;
        }
    }

    if (pItem->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))// wrapped? (若是包装礼盒，则从 character_gifts 恢复内部真实物品)
    {
        auto queryResult = CharacterDatabase.PQuery("SELECT entry, flags FROM character_gifts WHERE item_guid = '%u'", pItem->GetGUIDLow());
        if (queryResult)
        {
            Field* fields = queryResult->Fetch();
            uint32 entry = fields[0].GetUInt32();
            uint32 flags = fields[1].GetUInt32();

            pItem->SetGuidValue(ITEM_FIELD_GIFTCREATOR, ObjectGuid());
            pItem->SetEntry(entry);
            pItem->SetUInt32Value(ITEM_FIELD_FLAGS, flags);
            pItem->SetState(ITEM_CHANGED, pUser);
        }
        else
        {
            sLog.outError("Wrapped item %u don't have record in character_gifts table and will deleted", pItem->GetGUIDLow());
            pUser->DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
            return;
        }

        static SqlStatementID delGifts ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM character_gifts WHERE item_guid = ?");
        stmt.PExecute(pItem->GetGUIDLow());
    }
    else
    {
        // 普通容器物品：生成或展示物品拾取列表
        Loot*& loot = pItem->m_loot;
        if (!loot)
            loot = new Loot(pUser, pItem, LOOT_PICKPOCKETING);

        loot->ShowContentTo(pUser);
    }
}

// 处理客户端与游戏对象交互请求（CMSG_GAMEOBJ_USE）：校验距离、锁定、刷新与可交互标志
void WorldSession::HandleGameObjectUseOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    DEBUG_LOG("WORLD: Received opcode CMSG_GAMEOBJ_USE guid: %s", guid.GetString().c_str());

    // ignore for remote control state (远程控制状态或传送中不响应)
    if (!_player->IsSelfMover())
        return;

    if (_player->IsBeingTeleported())
        return;

    GameObject* obj = _player->GetMap()->GetGameObject(guid);
    if (!obj)
        return;

    if (!obj->IsAtInteractDistance(_player))
        return;

    if (obj->GetSpellForLock(_player))
    {
        sLog.outError("HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for spell locked object (Entry %u), didn't expect this to happen.", obj->GetEntry());
        return;
    }

    // Additional check preventing exploits (ie loot despawned chests) (防止利用已消失宝箱等漏洞)
    if (!obj->IsSpawned())
    {
        sLog.outError("HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for despawned GameObject (Entry %u), didn't expect this to happen.", obj->GetEntry());
        return;
    }

    // Never expect this opcode for some type GO's (通用型 GO 不应触发此协议)
    if (obj->GetGoType() == GAMEOBJECT_TYPE_GENERIC)
    {
        sLog.outError("HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for not allowed GameObject type %u (Entry %u), didn't expect this to happen.", obj->GetGoType(), obj->GetEntry());
        return;
    }

    if (obj->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE))
        return;

    // Never expect this opcode for non intractable GO's (不可交互 GO 不应触发此协议)
    if (obj->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT))
    {
        sLog.outError("HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for GameObject (Entry %u) with non intractable flag (Flags %u), didn't expect this to happen.", obj->GetEntry(), obj->GetUInt32Value(GAMEOBJECT_FLAGS));
        return;
    }

    if (!obj->CanUseNow(_player))
        return;

    obj->Use(_player);
}

// 处理客户端主动施法请求（CMSG_CAST_SPELL）：验证法术拥有权、读取目标、处理施法队列与 GCD 容差
void WorldSession::HandleCastSpellOpcode(WorldPacket& recvPacket)
{
    uint32 spellId;
    uint8  cast_count;
    recvPacket >> spellId;
    recvPacket >> cast_count;

    // ignore for remote control state (for player case) (被控制或控制其他玩家时忽略)
    Unit* mover = _player->GetMover();
    if (mover != _player && mover->GetTypeId() == TYPEID_PLAYER)
    {
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at ignore packet
        return;
    }

    DEBUG_LOG("WORLD: CMSG_CAST_SPELL, spellId - %u, cast_count: %u data length = " SIZEFMTD,
              spellId, cast_count, recvPacket.size());

    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown spell id %u", spellId);
        recvPacket.rpos(recvPacket.wpos());                 // prevent spam at ignore packet
        return;
    }

    // 校验施法者是否拥有该主动法术（防止伪造客户端包）
    Unit* caster = mover;
    if (mover->GetTypeId() == TYPEID_PLAYER)
    {
        // not have spell in spellbook or spell passive and not casted by client
        if (!((Player*)mover)->HasActiveSpell(spellId) || IsPassiveSpell(spellInfo))
        {
            sLog.outError("World: Player %u casts spell %u which he shouldn't have", mover->GetGUIDLow(), spellId);
            // cheater? kick? ban?
            recvPacket.rpos(recvPacket.wpos());             // prevent spam at ignore packet
            return;
        }
    }
    else
    {
        bool isPassive = IsPassiveSpell(spellInfo);
        // not have spell in spellbook or spell passive and not casted by client
        if (!mover->HasSpell(spellId) || isPassive)
        {
            if (!_player->HasSpell(spellId) || isPassive)
            {
                // cheater? kick? ban?
                recvPacket.rpos(recvPacket.wpos());             // prevent spam at ignore packet
                return;
            }
            else
                caster = _player;
        }
    }

    // client provided targets (解析客户端提供的目标信息)
    SpellCastTargets targets;

    recvPacket >> targets.ReadForCaster(_player);

    // auto-selection buff level base at target level (in spellInfo) (根据友方目标等级自动降级选择适配的 Buff 法术等级)
    if (Unit* target = targets.getUnitTarget())
    {
        // if rank not found then function return nullptr but in explicit cast case original spell can be casted and later failed with appropriate error message
        if (SpellEntry const* actualSpellInfo = sSpellMgr.SelectAuraRankForLevel(spellInfo, target->GetLevel()))
            spellInfo = actualSpellInfo;
    }

    // 客户端未显式传目标时补全当前选中目标
    if (HasMissingTargetFromClient(spellInfo))
        targets.setUnitTarget(mover->GetTarget());

    if (_player->HasQueuedSpell())
        return;

    // 正在自动重复施放相同法术时忽略重复请求
    if (IsAutoRepeatRangedSpell(spellInfo))
        if (const Spell* repeatSpell = caster->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
            if (spellId == repeatSpell->m_spellInfo->Id)
                return;

    bool handled = false;
    Spell* spell = new Spell(caster, spellInfo, TRIGGERED_NONE);
    spell->m_cast_count = cast_count;                       // set count of casts (记录客户端施法序号)
    spell->m_clientCast = true;
    // 若 GCD 或冷却尚在容差窗口内，则加入施法队列并延迟至冷却结束执行
    if (caster->HasGCD(spellInfo) || !caster->IsSpellReady(*spellInfo))
    {
        if (caster->HasGCDOrCooldownWithinMargin(*spellInfo))
        {
            handled = true;
            _player->SetQueuedSpell(spell);
            GetMessager().AddMessage([guid = caster->GetObjectGuid(), isPlayer = caster != mover, targets = targets](WorldSession* session) mutable
            {
                if (session->GetPlayer()) // in case of logout (防止异步执行期间玩家已下线)
                {
                    // in case of mind control end (确认心灵控制状态未发生变化)
                    if ((isPlayer && session->GetPlayer()->GetObjectGuid() == guid) || (!isPlayer && session->GetPlayer()->GetMover()->GetObjectGuid() == guid))
                        session->GetPlayer()->CastQueuedSpell(targets);
                    else
                        session->GetPlayer()->ClearQueuedSpell();
                }
            });
        }
    }

    if (!handled)
        spell->SpellStart(&targets);
}

// 处理客户端取消当前施法请求（CMSG_CANCEL_CAST）
void WorldSession::HandleCancelCastOpcode(WorldPacket& recvPacket)
{
    uint32 spellId;

    recvPacket >> spellId;

    // ignore for remote control state (for player case)
    Unit* mover = _player->GetMover();
    if (mover != _player && mover->GetTypeId() == TYPEID_PLAYER)
        return;

    if (!_player->IsClientControlled(_player))
        return;

    if (_player->IsNonMeleeSpellCasted(false))
        _player->InterruptNonMeleeSpells(false, spellId);
}

// 处理客户端右键取消自身光环请求（CMSG_CANCEL_AURA）
void WorldSession::HandleCancelAuraOpcode(WorldPacket& recvPacket)
{
    uint32 spellId;
    recvPacket >> spellId;

    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellInfo)
        return;

    // 不可取消、无图标（追踪资源除外）或被动光环禁止手动取消
    if (spellInfo->HasAttribute(SPELL_ATTR_NO_AURA_CANCEL))
        return;

    if (spellInfo->HasAttribute(SPELL_ATTR_EX_NO_AURA_ICON) && !IsSpellHaveAura(spellInfo, SPELL_AURA_TRACK_RESOURCES))
        return;

    if (IsPassiveSpell(spellInfo))
        return;

    SpellAuraHolder* holder = _player->GetSpellAuraHolder(spellId);

    if (!holder)
        return;

    // cant remove any auras while possessed (被附身或被魅惑时无法取消光环)
    if (_player->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_POSSESSED) || _player->HasCharmer())
        return;

    if (!holder->IsPositive())
    {
        // ignore for remote control state (仅在控制他人时允许取消特定控制类负面光环)
        if (!_player->IsSelfMover())
        {
            // except own aura spells
            bool allow = false;
            for (unsigned int k : spellInfo->EffectApplyAuraName)
            {
                if (k == SPELL_AURA_MOD_POSSESS ||
                    k == SPELL_AURA_MOD_POSSESS_PET)
                {
                    allow = true;
                    break;
                }
            }

            // this also include case when aura not found
            if (!allow)
                return;
        }
        else
            return;
    }

    // channeled spell case (it currently casted then) (若为引导中产生的光环，则直接打断引导)
    if (IsChanneledSpell(spellInfo))
    {
        if (Spell* curSpell = _player->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            if (curSpell->m_spellInfo->Id == spellId)
                _player->InterruptSpell(CURRENT_CHANNELED_SPELL);
        return;
    }

    // not own area auras can't be cancelled (note: maybe need to check for aura on holder and not general on spell)
    // 他人提供的区域光环（如光环/图腾）不能单方面右键取消
    if (holder && holder->GetCasterGuid() != _player->GetObjectGuid() && HasAreaAuraEffect(holder->GetSpellProto()))
        return;

    // non channeled case (常规非引导增益光环：移除光环)
    _player->RemoveAurasDueToSpellByCancel(spellId);
}

// 处理客户端取消宠物正面光环请求（CMSG_PET_CANCEL_AURA）
void WorldSession::HandlePetCancelAuraOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint32 spellId;

    recvPacket >> guid;
    recvPacket >> spellId;

    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown PET spell id %u", spellId);
        return;
    }

    Creature* pet = GetPlayer()->GetMap()->GetAnyTypeCreature(guid);

    if (!pet)
    {
        sLog.outError("HandlePetCancelAuraOpcode - %s not exist.", guid.GetString().c_str());
        return;
    }

    // 校验目标是否为玩家当前宠物或魅惑随从
    if (guid != GetPlayer()->GetPetGuid() && !GetPlayer()->HasCharm(guid))
    {
        sLog.outError("HandlePetCancelAura. %s isn't pet of %s", guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
        return;
    }

    if (!pet->IsAlive())
    {
        pet->SendPetActionFeedback(FEEDBACK_PET_DEAD);
        return;
    }

    if (spellInfo->HasAttribute(SPELL_ATTR_NO_AURA_CANCEL))
        return;

    if (spellInfo->HasAttribute(SPELL_ATTR_EX_NO_AURA_ICON) && !IsSpellHaveAura(spellInfo, SPELL_AURA_TRACK_RESOURCES))
        return;

    if (IsPassiveSpell(spellInfo))
        return;

    SpellAuraHolder* holder = pet->GetSpellAuraHolder(spellId);
    if (!holder || !holder->IsPositive())
        return;

    pet->RemoveAurasDueToSpell(spellId);
}

// 处理取消体型增长光环请求（预留空实现）
void WorldSession::HandleCancelGrowthAuraOpcode(WorldPacket& /*recvPacket*/)
{
    // nothing do
}

// 处理客户端取消自动重复施法请求（如取消自动射击/魔杖攻击）
void WorldSession::HandleCancelAutoRepeatSpellOpcode(WorldPacket& /*recvPacket*/)
{
    // cancel and prepare for deleting
    // do not send SMSG_CANCEL_AUTO_REPEAT! client will send this Opcode again (loop) (切勿回发 SMSG_CANCEL_AUTO_REPEAT，否则客户端会循环重发)
    _player->GetMover()->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
}

// 处理客户端手动打断引导法术请求（CMSG_CANCEL_CHANNELLING）
void WorldSession::HandleCancelChanneling(WorldPacket& recv_data)
{
    recv_data.read_skip<uint32>();                          // spellid, not used (跳过未使用的法术 ID)

    // ignore for remote control state (for player case)
    Unit* mover = _player->GetMover();
    if (mover != _player && mover->GetTypeId() == TYPEID_PLAYER)
        return;

    _player->InterruptSpell(CURRENT_CHANNELED_SPELL);
}

// 处理客户端右键销毁图腾请求（CMSG_TOTEM_DESTROYED）
void WorldSession::HandleTotemDestroyed(WorldPacket& recvPacket)
{
    uint8 slotId;

    recvPacket >> slotId;

    // ignore for remote control state
    if (!_player->IsSelfMover())
        return;

    if (int(slotId) >= MAX_TOTEM_SLOT)
        return;

    if (Totem* totem = GetPlayer()->GetTotem(TotemSlot(slotId)))
        totem->UnSummon();
}

// 处理客户端确认自我复活请求（如萨满复生或灵魂石复活）
void WorldSession::HandleSelfResOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: CMSG_SELF_RES");                  // empty opcode

    if (_player->GetUInt32Value(PLAYER_SELF_RES_SPELL))
    {
        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(_player->GetUInt32Value(PLAYER_SELF_RES_SPELL));
        if (spellInfo)
            _player->CastSpell(_player, spellInfo, TRIGGERED_NONE);

        _player->SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);
    }
}

// 处理客户端请求镜像（Mirror Image）外观与装备数据（CMSG_GET_MIRRORIMAGE_DATA）
void WorldSession::HandleGetMirrorimageData(WorldPacket& recv_data)
{
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: CMSG_GET_MIRRORIMAGE_DATA");

    ObjectGuid guid;
    recv_data >> guid;

    Creature* pCreature = _player->GetMap()->GetAnyTypeCreature(guid);

    if (!pCreature)
        return;

    Unit::AuraList const& images = pCreature->GetAurasByType(SPELL_AURA_MIRROR_IMAGE);

    if (images.empty())
        return;

    Unit* pCaster = images.front()->GetCaster();


    // FIXME: need proper packet structure for 2.x, currently no items and no proper hair colors show.

    WorldPacket data(SMSG_MIRRORIMAGE_DATA, 68);

    data << guid;
    data << (uint32)pCreature->GetDisplayId();

    data << (uint8)pCreature->getRace();
    data << (uint8)pCreature->getGender();
    // data << (uint8)pCreature->getClass();                // added in 3.x

    if (pCaster && pCaster->GetTypeId() == TYPEID_PLAYER)
    {
        Player* pPlayer = (Player*)pCaster;

        // skin, face, hair, haircolor (肤色、脸型、发型、发色)
        data << (uint8)pPlayer->GetByteValue(PLAYER_BYTES, 0);
        data << (uint8)pPlayer->GetByteValue(PLAYER_BYTES, 1);
        data << (uint8)pPlayer->GetByteValue(PLAYER_BYTES, 2);
        data << (uint8)pPlayer->GetByteValue(PLAYER_BYTES, 3);

        // facial hair (面部特征/胡须)
        data << (uint8)pPlayer->GetByteValue(PLAYER_BYTES_2, 0);

        // guild id (公会 ID)
        data << (uint32)pPlayer->GetGuildId();

        // 各装备槽位显示模型 ID（考虑隐藏头盔与隐藏披风设置）
        if (pPlayer->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM))
            data << (uint32)0;
        else
            data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_HEAD);

        data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_SHOULDERS);
        data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_BODY);
        data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_CHEST);
        data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_WAIST);
        data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_LEGS);
        data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_FEET);
        data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_WRISTS);
        data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_HANDS);

        if (pPlayer->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK))
            data << (uint32)0;
        else
            data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_BACK);

        data << (uint32)pPlayer->GetItemDisplayIdInSlot(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_TABARD);
    }
    else
    {
        // pCaster may have been nullptr (usually not expected, but may happen at disconnect, etc)
        // OR
        // pCaster is not player, data is taken from CreatureDisplayInfoExtraEntry by model already
        // 施法者离线或非玩家单位：填充零值外观与装备槽数据
        data << (uint8)0;
        data << (uint8)0;
        data << (uint8)0;
        data << (uint8)0;

        data << (uint8)0;

        data << (uint32)0;

        for (int i = 0; i < 11; ++i)
            data << (uint32)0;
    }

    SendPacket(data);
}
