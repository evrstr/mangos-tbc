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

#include "Entities/Item.h"
#include "Globals/ObjectMgr.h"
#include "Entities/ObjectGuid.h"
#include "Server/WorldPacket.h"
#include "Database/DatabaseEnv.h"
#include "Entities/ItemEnchantmentMgr.h"
#include "Server/SQLStorages.h"
#include "Loot/LootMgr.h"
#include "Spells/SpellTargetDefines.h"

#include <cmath>

// 穿戴属于套装的物品时，递增玩家套装计数并在达到阈值时激活对应套装法术效果
void AddItemsSetItem(Player* player, Item* item)
{
    ItemPrototype const* proto = item->GetProto();
    uint32 setid = proto->ItemSet;

    ItemSetEntry const* set = sItemSetStore.LookupEntry(setid);

    if (!set)
    {
        sLog.outErrorDb("Item set %u for item (id %u) not found, mods not applied.", setid, proto->ItemId);
        return;
    }

    if (set->required_skill_id && player->GetSkillValue(set->required_skill_id) < set->required_skill_value)
        return;

    ItemSetEffect* eff = player->GetItemSetEffect(setid);

    if (!eff)
        eff = player->AddItemSetEffect(setid);

    ++eff->item_count;

    for (uint32 x = 0; x < 8; ++x)
    {
        if (!set->spells[x])
            continue;
        // not enough for  spell (已穿戴件数尚未达到该奖励法术要求)
        if (set->items_to_triggerspell[x] > eff->item_count)
            continue;

        uint32 z = 0;
        for (; z < 8; ++z)
            if (eff->spells[z] && eff->spells[z]->Id == set->spells[x])
                break;

        if (z < 8)
            continue;

        // new spell (在空闲槽位中记录并应用新激活的套装奖励法术)
        for (auto& spell : eff->spells)
        {
            if (!spell)                            // free slot
            {
                SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(set->spells[x]);
                if (!spellInfo)
                {
                    sLog.outError("WORLD: unknown spell id %u in items set %u effects", set->spells[x], setid);
                    break;
                }

                // spell casted only if fit form requirement, in other case will casted at form change
                // 仅在满足变形形态要求时立即施放，否则在形态切换时再触发
                player->ApplyEquipSpell(spellInfo, nullptr, true);
                spell = spellInfo;
                break;
            }
        }
    }
}

// 卸下属于套装的物品时，递减套装计数并移除不再满足件数要求的套装奖励法术
void RemoveItemsSetItem(Player* player, ItemPrototype const* proto)
{
    uint32 setid = proto->ItemSet;

    ItemSetEntry const* set = sItemSetStore.LookupEntry(setid);

    if (!set)
    {
        sLog.outErrorDb("Item set #%u for item #%u not found, mods not removed.", setid, proto->ItemId);
        return;
    }

    ItemSetEffect* eff = player->GetItemSetEffect(setid);

    // can be in case now enough skill requirement for set appling but set has been appliend when skill requirement not enough
    // 可能穿戴时因专业技能不足未生效，故找不到对应的套装效果记录
    if (!eff)
        return;

    --eff->item_count;

    for (uint32 x = 0; x < 8; ++x)
    {
        if (!set->spells[x])
            continue;

        // enough for spell (剩余件数仍满足该套装法术条件则保留)
        if (set->items_to_triggerspell[x] <= eff->item_count)
            continue;

        for (auto& spell : eff->spells)
        {
            if (spell && spell->Id == set->spells[x])
            {
                // spell can be not active if not fit form requirement (即使因形态未激活也执行装备法术移除清理)
                player->ApplyEquipSpell(spell, nullptr, false);
                spell = nullptr;
                break;
            }
        }
    }

    if (!eff->item_count)                                   // all items of a set were removed (套装全部件已卸下，移除条目)
        player->RemoveItemSetEffect(setid);
}

// 检查物品是否允许放入指定容器（校验普通背包及灵魂袋、草药袋、箭袋等专业容器匹配规则）
bool ItemCanGoIntoBag(ItemPrototype const* pProto, ItemPrototype const* pBagProto)
{
    if (!pProto || !pBagProto)
        return false;

    switch (pBagProto->Class)
    {
        case ITEM_CLASS_CONTAINER:
            switch (pBagProto->SubClass)
            {
                case ITEM_SUBCLASS_CONTAINER:
                    return true;
                case ITEM_SUBCLASS_SOUL_CONTAINER:
                    if (!(pProto->BagFamily & BAG_FAMILY_MASK_SOUL_SHARDS))
                        return false;
                    return true;
                case ITEM_SUBCLASS_HERB_CONTAINER:
                    if (!(pProto->BagFamily & BAG_FAMILY_MASK_HERBS))
                        return false;
                    return true;
                case ITEM_SUBCLASS_ENCHANTING_CONTAINER:
                    if (!(pProto->BagFamily & BAG_FAMILY_MASK_ENCHANTING_SUPP))
                        return false;
                    return true;
                case ITEM_SUBCLASS_MINING_CONTAINER:
                    if (!(pProto->BagFamily & BAG_FAMILY_MASK_MINING_SUPP))
                        return false;
                    return true;
                case ITEM_SUBCLASS_ENGINEERING_CONTAINER:
                    if (!(pProto->BagFamily & BAG_FAMILY_MASK_ENGINEERING_SUPP))
                        return false;
                    return true;
                case ITEM_SUBCLASS_GEM_CONTAINER:
                    if (!(pProto->BagFamily & BAG_FAMILY_MASK_GEMS))
                        return false;
                    return true;
                case ITEM_SUBCLASS_LEATHERWORKING_CONTAINER:
                    if (!(pProto->BagFamily & BAG_FAMILY_MASK_LEATHERWORKING_SUPP))
                        return false;
                    return true;
                default:
                    return false;
            }
        case ITEM_CLASS_QUIVER:
            switch (pBagProto->SubClass)
            {
                case ITEM_SUBCLASS_QUIVER:
                    if (!(pProto->BagFamily & BAG_FAMILY_MASK_ARROWS))
                        return false;
                    return true;
                case ITEM_SUBCLASS_AMMO_POUCH:
                    if (!(pProto->BagFamily & BAG_FAMILY_MASK_BULLETS))
                        return false;
                    return true;
                default:
                    return false;
            }
    }
    return false;
}

// 构造物品实例并初始化对象类型、更新字段数量及默认状态
Item::Item()
{
    m_objectType |= TYPEMASK_ITEM;
    m_objectTypeId = TYPEID_ITEM;
    // 2.3.2 - 0x18 (2.3.2 起物品更新标志掩码)
    m_updateFlag = (UPDATEFLAG_LOWGUID | UPDATEFLAG_HIGHGUID);

    m_valuesCount = ITEM_END;
    m_slot = 0;
    uState = ITEM_NEW;
    uQueuePos = -1;
    m_container = nullptr;
    mb_in_trade = false;
    m_lootState = ITEM_LOOT_NONE;
    m_enchantmentModifier = 0;

    m_usedInSpell = false;
}

Item::~Item()
{

}

// 使用模板属性初始化新创建的物品实例（耐久度、充能次数、限时时长与所有者）
bool Item::Create(uint32 guidlow, uint32 itemid, Player const* owner)
{
    Object::_Create(guidlow, guidlow, 0, HIGHGUID_ITEM);

    SetEntry(itemid);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(ITEM_FIELD_OWNER, owner ? owner->GetObjectGuid() : ObjectGuid());
    SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());

    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(itemid);
    if (!itemProto)
        return false;

    SetUInt32Value(ITEM_FIELD_STACK_COUNT, 1);
    SetUInt32Value(ITEM_FIELD_MAXDURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_DURABILITY, itemProto->MaxDurability);

    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        SetSpellCharges(i, itemProto->Spells[i].SpellCharges);

    SetUInt32Value(ITEM_FIELD_DURATION, itemProto->Duration);

    return true;
}

// 从世界移除物品前，先将其从所属玩家地图的待下发创建更新管线中撤除
void Item::RemoveFromWorld()
{
    if (Player* owner = GetOwner())
        if (owner->IsInWorld())
            owner->GetMap()->RemoveUpdateCreateObject(this); // in case we are in the pipeline (避免残留于下发更新管线中)

    Object::RemoveFromWorld();
}

// 扣减限时物品剩余存活时间，到期且未正在施法中则销毁该物品
void Item::UpdateDuration(Player* owner, uint32 diff)
{
    if (!GetUInt32Value(ITEM_FIELD_DURATION))
        return;

    // DEBUG_LOG("Item::UpdateDuration Item (Entry: %u Duration %u Diff %u)", GetEntry(), GetUInt32Value(ITEM_FIELD_DURATION), diff);

    if (GetUInt32Value(ITEM_FIELD_DURATION) <= diff && !IsUsedInSpell())
    {
        owner->DestroyItem(GetBagSlot(), GetSlot(), true);
        return;
    }

    uint32 oldDuration = GetUInt32Value(ITEM_FIELD_DURATION);
    SetUInt32Value(ITEM_FIELD_DURATION, oldDuration <= diff ? 1 : oldDuration - diff);
    SetState(ITEM_CHANGED, owner);                          // save new time in database (标记已修改以便存库)
}

// 根据 uState 将物品实例、附魔、充能、礼物关系及内含掉落同步至角色数据库
void Item::SaveToDB()
{
    static const char* UPDATE_ITEM = "UPDATE item_instance SET owner_guid = ?, itemEntry = ?, creatorGuid = ?, giftCreatorGuid = ?, count = ?, duration = ?, charges = ?, flags = ?, enchantments = ?, randomPropertyId = ?, durability = ?, itemTextId = ? WHERE guid = ?";
    static const char* INSERT_ITEM = "REPLACE INTO item_instance (owner_guid, itemEntry, creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, randomPropertyId, durability, itemTextId, guid) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)";
    static const char* UPDATE_GIFT = "UPDATE character_gifts SET guid = ? WHERE item_guid = ?";
    static const char* DELETE_TEXT = "DELETE FROM item_text WHERE id = ?";
    static const char* DELETE_ITEM = "DELETE FROM item_instance WHERE guid = ?";
    static const char* DELETE_GIFT = "DELETE FROM character_gifts WHERE item_guid = ?";
    static const char* DELETE_LOOT = "DELETE FROM item_loot WHERE guid = ?";

    uint32 guid = GetGUIDLow();
    switch (uState)
    {
        case ITEM_NEW:
        case ITEM_CHANGED:
        {
            static SqlStatementID insItem, updItem;
            std::unique_ptr<SqlStatement> stmt;

            //get thread id
            //std::hash<std::thread::id> myHashObject{};
            //uint32 threadID = myHashObject(std::this_thread::get_id());

            if (uState == ITEM_NEW)
            {
                stmt.reset(new SqlStatement(CharacterDatabase.CreateStatement(insItem, INSERT_ITEM)));
                //printf("> Saving new item(%s) for guid[%u]. Thread is [%u]\n", GetProto()->Name1, GetOwnerGuid().GetCounter(), threadID);
            }
            else
            {
                stmt.reset(new SqlStatement(CharacterDatabase.CreateStatement(updItem, UPDATE_ITEM)));
                //printf("> Updating item(%s) for guid[%u]. Thread is [%u]\n", GetProto()->Name1, GetOwnerGuid().GetCounter(), threadID);
            }

            stmt->addUInt32(GetOwnerGuid().GetCounter());
            stmt->addUInt32(GetEntry());
            stmt->addUInt32(GetGuidValue(ITEM_FIELD_CREATOR).GetCounter());
            stmt->addUInt32(GetGuidValue(ITEM_FIELD_GIFTCREATOR).GetCounter());
            stmt->addUInt32(GetCount());
            stmt->addUInt32(GetUInt32Value(ITEM_FIELD_DURATION));

            std::ostringstream ssSpells;
            for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                ssSpells << GetSpellCharges(i) << ' ';
            stmt->addString(ssSpells.str());

            stmt->addUInt32(GetUInt32Value(ITEM_FIELD_FLAGS));

            std::ostringstream ssEnchants;
            for (uint8 i = 0; i < MAX_ENCHANTMENT_SLOT; ++i)
            {
                ssEnchants << GetEnchantmentId(EnchantmentSlot(i)) << ' ';
                ssEnchants << GetEnchantmentDuration(EnchantmentSlot(i)) << ' ';
                ssEnchants << GetEnchantmentCharges(EnchantmentSlot(i)) << ' ';
            }
            stmt->addString(ssEnchants.str());

            stmt->addInt16(GetItemRandomPropertyId());
            stmt->addUInt16(GetUInt32Value(ITEM_FIELD_DURABILITY));
            stmt->addUInt32(GetUInt32Value(ITEM_FIELD_ITEM_TEXT_ID));
            stmt->addUInt32(guid);

            stmt->Execute();

            if (uState == ITEM_CHANGED && HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
            {
                static SqlStatementID updGifts;
                stmt.reset(new SqlStatement(CharacterDatabase.CreateStatement(updGifts, UPDATE_GIFT)));
                stmt->PExecute(GetOwnerGuid().GetCounter(), GetGUIDLow());
            }

            break;
        }
        case ITEM_REMOVED:
        {
            static SqlStatementID delItemText;
            static SqlStatementID delInst;
            static SqlStatementID delGifts;
            static SqlStatementID delLoot;

            if (uint32 item_text_id = GetUInt32Value(ITEM_FIELD_ITEM_TEXT_ID))
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(delItemText, DELETE_TEXT);
                stmt.PExecute(item_text_id);
            }

            SqlStatement stmt = CharacterDatabase.CreateStatement(delInst, DELETE_ITEM);
            stmt.PExecute(guid);

            if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
            {
                stmt = CharacterDatabase.CreateStatement(delGifts, DELETE_GIFT);
                stmt.PExecute(GetGUIDLow());
            }

            if (HasSavedLoot())
            {
                stmt = CharacterDatabase.CreateStatement(delLoot, DELETE_LOOT);
                stmt.PExecute(GetGUIDLow());
            }

            delete this;
            return;
        }
        case ITEM_UNCHANGED:
            return;
    }

    if (m_lootState == ITEM_LOOT_CHANGED || m_lootState == ITEM_LOOT_REMOVED)
    {
        static SqlStatementID delLoot;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delLoot, "DELETE FROM item_loot WHERE guid = ?");
        stmt.PExecute(GetGUIDLow());
    }

    if (m_loot && (m_lootState == ITEM_LOOT_NEW || m_lootState == ITEM_LOOT_CHANGED))
    {
        if (Player* owner = GetOwner())
        {
            static SqlStatementID saveGold;
            static SqlStatementID saveLoot;

            // save money as 0 itemid data
            if (m_loot->GetGoldAmount())
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(saveGold, "INSERT INTO item_loot (guid,owner_guid,itemid,amount,suffix,property) VALUES (?, ?, 0, ?, 0, 0)");
                stmt.PExecute(GetGUIDLow(), owner->GetGUIDLow(), m_loot->GetGoldAmount());
            }

            SqlStatement stmt = CharacterDatabase.CreateStatement(saveLoot, "INSERT INTO item_loot (guid,owner_guid,itemid,amount,suffix,property) VALUES (?, ?, ?, ?, ?, ?)");

            // save items and quest items (at load its all will added as normal, but this not important for item loot case)
            LootItemList lootList;
            m_loot->GetLootItemsListFor(owner, lootList);
            for (LootItemList::const_iterator lootItr = lootList.begin(); lootItr != lootList.end(); ++lootItr)
            {
                LootItem* lootItem = *lootItr;
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(owner->GetGUIDLow());
                stmt.addUInt32(lootItem->itemId);
                stmt.addUInt8(lootItem->count);
                stmt.addUInt32(lootItem->randomSuffix);
                stmt.addInt32(lootItem->randomPropertyId);

                stmt.Execute();
            }
        }
    }

    if (m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_TEMPORARY)
        SetLootState(ITEM_LOOT_UNCHANGED);

    SetState(ITEM_UNCHANGED);
}

// 从 item_instance 字段集合加载物品属性，并自动修正过期的时长、绑定、耐久与包装状态
bool Item::LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid)
{
    // 0          1            2                3      4         5        6      7             8                 9           10
    // itemEntry, creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, randomPropertyId, durability, itemTextId

    // create item before any checks for store correct guid
    // and allow use "FSetState(ITEM_REMOVED); SaveToDB();" for deleting item from DB
    // 先初始化 GUID，以便后续校验失败时可通过 ITEM_REMOVED 安全从数据库删除
    Object::_Create(guidLow, guidLow, 0, HIGHGUID_ITEM);

    // Set entry, MUST be before proto check (必须先设置 Entry 再查询物品模板)
    SetEntry(fields[0].GetUInt32());
    SetObjectScale(1.0f);

    ItemPrototype const* proto = GetProto();
    if (!proto)
        return false;

    // set owner (not if item is only loaded for gbank/auction/mail) (公会银行/拍卖行/邮件加载时不覆盖所有者)
    if (ownerGuid)
        SetOwnerGuid(ownerGuid);

    bool need_save = false;                                 // need explicit save data at load fixes (加载修复后是否需要立即回写数据库)
    SetGuidValue(ITEM_FIELD_CREATOR, ObjectGuid(HIGHGUID_PLAYER, fields[1].GetUInt32()));
    SetGuidValue(ITEM_FIELD_GIFTCREATOR, ObjectGuid(HIGHGUID_PLAYER, fields[2].GetUInt32()));
    SetCount(fields[3].GetUInt32());

    uint32 duration = fields[4].GetUInt32();
    SetUInt32Value(ITEM_FIELD_DURATION, duration);
    // update duration if need, and remove if not need (模板与数据库限时状态不一致时进行修正)
    if ((proto->Duration == 0) != (duration == 0))
    {
        SetUInt32Value(ITEM_FIELD_DURATION, proto->Duration);
        need_save = true;
    }

    Tokens tokens = StrSplit(fields[5].GetCppString(), " ");
    if (tokens.size() == MAX_ITEM_PROTO_SPELLS)
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            SetSpellCharges(i, atoi(tokens[i].c_str()));

    SetUInt32Value(ITEM_FIELD_FLAGS, fields[6].GetUInt32());

    // Remove bind flag for items vs NO_BIND set (若模板改为不绑定则清除旧的绑定标志)
    if (IsSoulBound() && proto->Bonding == NO_BIND)
    {
        ApplyModFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED, false);
        need_save = true;
    }

    _LoadIntoDataField(fields[7].GetString(), ITEM_FIELD_ENCHANTMENT_1_1, MAX_ENCHANTMENT_SLOT * MAX_ENCHANTMENT_OFFSET);
     SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, fields[8].GetInt16());

    // recalculate suffix factor (负值为随机后缀 ID，重新计算其属性点缩放系数)
    if (GetItemRandomPropertyId() < 0)
        UpdateItemSuffixFactor();

    uint32 durability = fields[9].GetUInt16();
    SetUInt32Value(ITEM_FIELD_DURABILITY, durability);
    // update max durability (and durability) if need (更新最大耐久并在非包装状态下钳制当前耐久)
    SetUInt32Value(ITEM_FIELD_MAXDURABILITY, proto->MaxDurability);

    // do not overwrite durability for wrapped items (已包装礼物的耐久字段存储的是原物品信息，不可覆盖)
    if (durability > proto->MaxDurability && !HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
    {
        SetUInt32Value(ITEM_FIELD_DURABILITY, proto->MaxDurability);
        need_save = true;
    }

    SetUInt32Value(ITEM_FIELD_ITEM_TEXT_ID, fields[10].GetUInt32());

    // set correct wrapped state (校验并修复异常的礼物包装标志)
    if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
    {
        // wrapped item must be wrapper (used version that not stackable)
        if (!(proto->Flags & ITEM_FLAG_IS_WRAPPER) || GetMaxStackCount() > 1)
        {
            RemoveFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
            need_save = true;

            static SqlStatementID delGifts;

            // also cleanup for sure gift table (同步清理 character_gifts 残留记录)
            SqlStatement stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM character_gifts WHERE item_guid = ?");
            stmt.PExecute(GetGUIDLow());
        }
    }

    if (need_save)                                          // normal item changed state set not work at loading (加载期直接更新数据库而不走更新队列)
    {
        static SqlStatementID updItem;

        SqlStatement stmt = CharacterDatabase.CreateStatement(updItem, "UPDATE item_instance SET duration = ?, flags = ?, durability = ? WHERE guid = ?");

        stmt.addUInt32(GetUInt32Value(ITEM_FIELD_DURATION));
        stmt.addUInt32(GetUInt32Value(ITEM_FIELD_FLAGS));
        stmt.addUInt32(GetUInt32Value(ITEM_FIELD_DURABILITY));
        stmt.addUInt32(guidLow);

        stmt.Execute();
    }

    return true;
}

// 从 item_loot 表恢复可开启物品内部尚未拾取的金币或物品
void Item::LoadLootFromDB(Field* fields)
{
    uint32 item_id     = fields[1].GetUInt32();
    uint32 item_amount = fields[2].GetUInt32();
    uint32 item_suffix = fields[3].GetUInt32();
    int32  item_propid = fields[4].GetInt32();

    // money value special case (item_id == 0 代表内含金币)
    if (item_id == 0)
    {
        m_loot->SetGoldAmount(item_amount);
        SetLootState(ITEM_LOOT_UNCHANGED);
        return;
    }

    // normal item case (普通内含掉落物品)
    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);

    if (!proto)
    {
        CharacterDatabase.PExecute("DELETE FROM item_loot WHERE guid = '%u' AND itemid = '%u'", GetGUIDLow(), item_id);
        sLog.outError("Item::LoadLootFromDB: %s has an unknown item (id: #%u) in item_loot, deleted.", GetOwnerGuid().GetString().c_str(), item_id);
        return;
    }

    m_loot->AddItem(item_id, item_amount, item_suffix, item_propid);

    SetLootState(ITEM_LOOT_UNCHANGED);
}

// 从 item_instance 表删除本物品实例记录
void Item::DeleteFromDB()
{
    static SqlStatementID delItem ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delItem, "DELETE FROM item_instance WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());
}

// 从 character_inventory 表删除本物品所在背包槽位映射
void Item::DeleteFromInventoryDB() const
{
    static SqlStatementID delInv ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delInv, "DELETE FROM character_inventory WHERE item = ?");
    stmt.PExecute(GetGUIDLow());
}

ItemPrototype const* Item::GetProto() const
{
    return ObjectMgr::GetItemPrototype(GetEntry());
}

Player* Item::GetOwner()const
{
    return sObjectMgr.GetPlayer(GetOwnerGuid());
}

// 返回装备该物品所需的技能线 ID（武器技能或护甲熟练度技能）
uint32 Item::GetSkill() const
{
    const static uint32 item_weapon_skills[MAX_ITEM_SUBCLASS_WEAPON] =
    {
        SKILL_AXES,     SKILL_2H_AXES,  SKILL_BOWS,          SKILL_GUNS,      SKILL_MACES,
        SKILL_2H_MACES, SKILL_POLEARMS, SKILL_SWORDS,        SKILL_2H_SWORDS, 0,
        SKILL_STAVES,   0,              0,                   SKILL_UNARMED,   0,
        SKILL_DAGGERS,  SKILL_THROWN,   SKILL_ASSASSINATION, SKILL_CROSSBOWS, SKILL_WANDS,
        SKILL_FISHING
    };

    const static uint32 item_armor_skills[MAX_ITEM_SUBCLASS_ARMOR] =
    {
        0, SKILL_CLOTH, SKILL_LEATHER, SKILL_MAIL, SKILL_PLATE_MAIL, 0, SKILL_SHIELD, 0, 0, 0
    };

    ItemPrototype const* proto = GetProto();

    switch (proto->Class)
    {
        case ITEM_CLASS_WEAPON:
        {
            if (proto->SubClass >= MAX_ITEM_SUBCLASS_WEAPON)
                return 0;
            return item_weapon_skills[proto->SubClass];
        }

        case ITEM_CLASS_ARMOR:
        {
            if (proto->SubClass >= MAX_ITEM_SUBCLASS_ARMOR)
                return 0;
            return item_armor_skills[proto->SubClass];
        }

        default:
            return 0;
    }
}

// 返回装备该类武器或护甲所需的被动掌握法术 ID
uint32 Item::GetSpell() const
{
    ItemPrototype const* proto = GetProto();

    switch (proto->Class)
    {
        case ITEM_CLASS_WEAPON:
            switch (proto->SubClass)
            {
                case ITEM_SUBCLASS_WEAPON_AXE:     return  196;
                case ITEM_SUBCLASS_WEAPON_AXE2:    return  197;
                case ITEM_SUBCLASS_WEAPON_BOW:     return  264;
                case ITEM_SUBCLASS_WEAPON_GUN:     return  266;
                case ITEM_SUBCLASS_WEAPON_MACE:    return  198;
                case ITEM_SUBCLASS_WEAPON_MACE2:   return  199;
                case ITEM_SUBCLASS_WEAPON_POLEARM: return  200;
                case ITEM_SUBCLASS_WEAPON_SWORD:   return  201;
                case ITEM_SUBCLASS_WEAPON_SWORD2:  return  202;
                case ITEM_SUBCLASS_WEAPON_STAFF:   return  227;
                case ITEM_SUBCLASS_WEAPON_DAGGER:  return 1180;
                case ITEM_SUBCLASS_WEAPON_THROWN:  return 2567;
                case ITEM_SUBCLASS_WEAPON_SPEAR:   return 3386;
                case ITEM_SUBCLASS_WEAPON_CROSSBOW: return 5011;
                case ITEM_SUBCLASS_WEAPON_WAND:    return 5009;
                default: return 0;
            }
        case ITEM_CLASS_ARMOR:
            switch (proto->SubClass)
            {
                case ITEM_SUBCLASS_ARMOR_CLOTH:    return 9078;
                case ITEM_SUBCLASS_ARMOR_LEATHER:  return 9077;
                case ITEM_SUBCLASS_ARMOR_MAIL:     return 8737;
                case ITEM_SUBCLASS_ARMOR_PLATE:    return  750;
                case ITEM_SUBCLASS_ARMOR_SHIELD:   return 9116;
                default: return 0;
            }
    }
    return 0;
}

// 为物品随机生成随机属性 ID（正值）或随机后缀 ID（负值）
int32 Item::GenerateItemRandomPropertyId(uint32 item_id)
{
    ItemPrototype const* itemProto = sItemStorage.LookupEntry<ItemPrototype>(item_id);

    if (!itemProto)
        return 0;

    // item must have one from this field values not null if it can have random enchantments
    if ((!itemProto->RandomProperty) && (!itemProto->RandomSuffix))
        return 0;

    // Random Property case
    if (itemProto->RandomProperty)
    {
        uint32 randomPropId = GetItemEnchantMod(itemProto->RandomProperty);
        ItemRandomPropertiesEntry const* random_id = sItemRandomPropertiesStore.LookupEntry(randomPropId);
        if (!random_id)
        {
            sLog.outErrorDb("Enchantment id #%u used but it doesn't have records in 'ItemRandomProperties.dbc'", randomPropId);
            return 0;
        }

        return random_id->ID;
    }
    // Random Suffix case
    uint32 randomPropId = GetItemEnchantMod(itemProto->RandomSuffix);
    ItemRandomSuffixEntry const* random_id = sItemRandomSuffixStore.LookupEntry(randomPropId);
    if (!random_id)
    {
        sLog.outErrorDb("Enchantment id #%u used but it doesn't have records in sItemRandomSuffixStore.", randomPropId);
        return 0;
    }

    return -int32(random_id->ID);
}

// 根据 randomPropId（正数随机属性 / 负数随机后缀）写入随机附魔槽位并更新后缀系数
void Item::SetItemRandomProperties(int32 randomPropId)
{
    if (!randomPropId)
        return;

    if (randomPropId > 0)
    {
        ItemRandomPropertiesEntry const* item_rand = sItemRandomPropertiesStore.LookupEntry(randomPropId);
        if (item_rand)
        {
            if (GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID) != int32(item_rand->ID))
            {
                SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, item_rand->ID);
                SetState(ITEM_CHANGED);
            }
            for (uint32 i = PROP_ENCHANTMENT_SLOT_2; i < PROP_ENCHANTMENT_SLOT_2 + 3; ++i)
                SetEnchantment(EnchantmentSlot(i), item_rand->enchant_id[i - PROP_ENCHANTMENT_SLOT_2], 0, 0);
        }
    }
    else
    {
        ItemRandomSuffixEntry const* item_rand = sItemRandomSuffixStore.LookupEntry(-randomPropId);
        if (item_rand)
        {
            if (GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID) != -int32(item_rand->ID) ||
                    !GetItemSuffixFactor())
            {
                SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, -int32(item_rand->ID));
                UpdateItemSuffixFactor();
                SetState(ITEM_CHANGED);
            }

            for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < PROP_ENCHANTMENT_SLOT_0 + 3; ++i)
                SetEnchantment(EnchantmentSlot(i), item_rand->enchant_id[i - PROP_ENCHANTMENT_SLOT_0], 0, 0);
        }
    }
}

// 重新计算并刷新随机后缀属性缩放系数 (ITEM_FIELD_PROPERTY_SEED)
void Item::UpdateItemSuffixFactor()
{
    uint32 suffixFactor = GenerateEnchSuffixFactor(GetEntry());
    if (GetItemSuffixFactor() == suffixFactor)
        return;

    SetUInt32Value(ITEM_FIELD_PROPERTY_SEED, suffixFactor);
    return;
}

// 设置物品数据库同步状态，并维护其在玩家更新队列中的位置
void Item::SetState(ItemUpdateState state, Player* forplayer)
{
    if (uState == ITEM_NEW && state == ITEM_REMOVED)
    {
        // pretend the item never existed (新建尚未入库的物品直接删除，视作从未存在)
        if (forplayer || GetOwnerGuid())
            RemoveFromUpdateQueueOf(forplayer);
        delete this;
        return;
    }

    if (state != ITEM_UNCHANGED)
    {
        // new items must stay in new state until saved (新建物品在真正存库前必须保持 ITEM_NEW 状态)
        if (uState != ITEM_NEW) uState = state;

        if (forplayer || GetOwnerGuid())
            AddToUpdateQueueOf(forplayer);
    }
    else
    {
        // unset in queue
        // the item must be removed from the queue manually
        // 状态清为未修改时仅重置队列下标，实际队列元素需外部手动摘除
        uQueuePos = -1;
        uState = ITEM_UNCHANGED;
    }
}

// 将物品加入所属玩家的待存库更新队列
void Item::AddToUpdateQueueOf(Player* player)
{
    if (IsInUpdateQueue())
        return;

    if (!player)
    {
        player = GetOwner();
        if (!player)
        {
            sLog.outError("Item::AddToUpdateQueueOf - %s current owner (%s) not in world!",
                          GetGuidStr().c_str(), GetOwnerGuid().GetString().c_str());
            return;
        }
    }

    if (player->GetObjectGuid() != GetOwnerGuid())
    {
        sLog.outError("Item::AddToUpdateQueueOf - %s current owner (%s) and inventory owner (%s) don't match!",
                      GetGuidStr().c_str(), GetOwnerGuid().GetString().c_str(), player->GetGuidStr().c_str());
        return;
    }

    if (player->m_itemUpdateQueueBlocked)
        return;

    player->m_itemUpdateQueue.push_back(this);
    uQueuePos = player->m_itemUpdateQueue.size() - 1;
}

// 从所属玩家的待存库更新队列中移除（置空槽位以免破坏其他元素下标）
void Item::RemoveFromUpdateQueueOf(Player* player)
{
    if (!IsInUpdateQueue())
        return;

    if (!player)
    {
        player = GetOwner();
        if (!player)
        {
            sLog.outError("Item::RemoveFromUpdateQueueOf - %s current owner (%s) not in world!",
                          GetGuidStr().c_str(), GetOwnerGuid().GetString().c_str());
            return;
        }
    }

    if (player->GetObjectGuid() != GetOwnerGuid())
    {
        sLog.outError("Item::RemoveFromUpdateQueueOf - %s current owner (%s) and inventory owner (%s) don't match!",
                      GetGuidStr().c_str(), GetOwnerGuid().GetString().c_str(), player->GetGuidStr().c_str());
        return;
    }

    if (player->m_itemUpdateQueueBlocked)
        return;

    player->m_itemUpdateQueue[uQueuePos] = nullptr;
    uQueuePos = -1;
}

uint8 Item::GetBagSlot() const
{
    return m_container ? m_container->GetSlot() : uint8(INVENTORY_SLOT_BAG_0);
}

bool Item::IsEquipped() const
{
    return !IsInBag() && m_slot < EQUIPMENT_SLOT_END;
}

// 检查物品当前是否允许被玩家交易（排除已绑定、非空背包、含未拾取物品及绑定附魔）
bool Item::CanBeTraded() const
{
    if (IsSoulBound())
        return false;
    if (IsBag() && (Player::IsBagPos(GetPos()) || !((Bag const*)this)->IsEmpty()))
        return false;

    if (Player* owner = GetOwner())
    {
        if (owner->CanUnequipItem(GetPos(), false) !=  EQUIP_ERR_OK)
            return false;
        if (owner->GetLootGuid() == GetObjectGuid())
            return false;
    }

    if (HasGeneratedLoot())
        return false;

    if (IsBoundByEnchant())
        return false;

    return true;
}

// 检查物品上是否存在带有 ENCHANTMENT_SOULBOUND 标志的附魔/宝石
bool Item::IsBoundByEnchant() const
{
    // Check all enchants for soulbound (遍历所有附魔槽位判断是否触发灵魂绑定)
    for (uint32 enchant_slot = PERM_ENCHANTMENT_SLOT; enchant_slot < MAX_ENCHANTMENT_SLOT; ++enchant_slot)
    {
        uint32 enchant_id = GetEnchantmentId(EnchantmentSlot(enchant_slot));
        if (!enchant_id)
            continue;

        SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!enchantEntry)
            continue;

        if (enchantEntry->flags & ENCHANTMENT_SOULBOUND)
            return true;
    }
    return false;
}

bool Item::IsMainHandOnlyEnchant(EnchantmentSlot slot) const
{
    SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(GetEnchantmentId(slot));
    if (enchantEntry && enchantEntry->flags & ENCHANTMENT_MAINHAND_ONLY)
        return true;

    return false;
}

bool Item::CanEnterArenaEnchant(EnchantmentSlot slot) const
{
    SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(GetEnchantmentId(slot));
    if (enchantEntry && enchantEntry->flags & ENCHANTMENT_ALLOW_ENTERING_ARENA)
        return true;

    return false;
}

// 检查物品分类、子类及穿戴部位是否符合指定法术的装备/目标条件
bool Item::IsFitToSpellRequirements(SpellEntry const* spellInfo) const
{
    ItemPrototype const* proto = GetProto();

    if (spellInfo->EquippedItemClass != -1)                 // -1 == any item class (-1 表示不限物品大类)
    {
        if (spellInfo->EquippedItemClass != int32(proto->Class))
            return false;                                   //  wrong item class

        if (spellInfo->EquippedItemSubClassMask != 0)       // 0 == any subclass (0 表示不限子类)
        {
            if ((spellInfo->EquippedItemSubClassMask & (1 << proto->SubClass)) == 0)
                return false;                               // subclass not present in mask
        }
    }

    // Only check for item enchantments (TARGET_FLAG_ITEM), all other spells are either NPC spells
    // or spells where slot requirements are already handled with AttributesEx3 fields
    // and special code (Titan's Grip, Windfury Attack). Check clearly not applicable for Lava Lash.
    // 仅对物品目标型法术校验 InventoryTypeMask，其余技能由槽位属性或特殊逻辑控制
    if (spellInfo->EquippedItemInventoryTypeMask != 0 && (spellInfo->Targets & TARGET_FLAG_ITEM))    // 0 == any inventory type
    {
        if ((spellInfo->EquippedItemInventoryTypeMask  & (1 << proto->InventoryType)) == 0)
            return false;                                   // inventory type not present in mask
    }

    return true;
}

// 校验使用该物品时当前选中的目标单位是否满足 item_required_target 要求
bool Item::IsTargetValidForItemUse(Unit* pUnitTarget) const
{
    ItemRequiredTargetMapBounds bounds = sObjectMgr.GetItemRequiredTargetMapBounds(GetProto()->ItemId);

    if (bounds.first == bounds.second)
        return true;

    if (!pUnitTarget)
        return false;

    for (ItemRequiredTargetMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
        if (itr->second.IsFitToRequirements(pUnitTarget))
            return true;

    return false;
}

// 写入物品指定槽位的附魔 ID、持续时间与次数，并向客户端发送附魔记录日志
void Item::SetEnchantment(EnchantmentSlot slot, uint32 id, uint32 duration, uint32 charges, ObjectGuid caster /*= ObjectGuid()*/)
{
    // Better lost small time at check in comparison lost time at item save to DB. (若数据未变化则跳过，避免无谓写库)
    if ((GetEnchantmentId(slot) == id) && (GetEnchantmentDuration(slot) == duration) && (GetEnchantmentCharges(slot) == charges))
        return;

    Player* owner = GetOwner();
    if (slot < MAX_INSPECTED_ENCHANTMENT_SLOT)
    {
        if (uint32 oldEnchant = GetEnchantmentId(slot))
            owner->SendEnchantmentLog(ObjectGuid(), GetEntry(), oldEnchant);

        if (id)
            owner->SendEnchantmentLog(caster, GetEntry(), id);
    }

    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_ID_OFFSET, id);
    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_DURATION_OFFSET, duration);
    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_CHARGES_OFFSET, charges);
    SetState(ITEM_CHANGED);
}

void Item::SetEnchantmentDuration(EnchantmentSlot slot, uint32 duration)
{
    if (GetEnchantmentDuration(slot) == duration)
        return;

    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_DURATION_OFFSET, duration);
    SetState(ITEM_CHANGED);
}

void Item::SetEnchantmentCharges(EnchantmentSlot slot, uint32 charges)
{
    if (GetEnchantmentCharges(slot) == charges)
        return;

    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_CHARGES_OFFSET, charges);
    SetState(ITEM_CHANGED);
}

// 清除指定附魔槽位的数据并下发移除日志
void Item::ClearEnchantment(EnchantmentSlot slot)
{
    if (!GetEnchantmentId(slot))
        return;

    if (slot < MAX_INSPECTED_ENCHANTMENT_SLOT)
        if (uint32 oldEnchant = GetEnchantmentId(slot))
            if (Player* owner = GetOwner())
                owner->SendEnchantmentLog(ObjectGuid(), GetEntry(), oldEnchant);

    for (uint8 x = 0; x < 3; ++x)
        SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + x, 0);
    SetState(ITEM_CHANGED);
}

// 检查装备上所有宝石插槽是否均已镶嵌且宝石颜色完全匹配插槽颜色（用于激活插槽奖励）
bool Item::GemsFitSockets() const
{
    bool fits = true;
    for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++enchant_slot)
    {
        uint8 SocketColor = GetProto()->Socket[enchant_slot - SOCK_ENCHANTMENT_SLOT].Color;

        uint32 enchant_id = GetEnchantmentId(EnchantmentSlot(enchant_slot));
        if (!enchant_id)
        {
            if (SocketColor) fits &= false;
            continue;
        }

        SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!enchantEntry)
        {
            if (SocketColor) fits &= false;
            continue;
        }

        uint8 GemColor = 0;

        uint32 gemid = enchantEntry->GemID;
        if (gemid)
        {
            ItemPrototype const* gemProto = sItemStorage.LookupEntry<ItemPrototype>(gemid);
            if (gemProto)
            {
                GemPropertiesEntry const* gemProperty = sGemPropertiesStore.LookupEntry(gemProto->GemProperties);
                if (gemProperty)
                    GemColor = gemProperty->color;
            }
        }

        fits &= (GemColor & SocketColor) != 0;
    }
    return fits;
}

// 统计本装备插槽中镶嵌指定宝石模板 ID 的数量
uint8 Item::GetGemCountWithID(uint32 GemID) const
{
    uint8 count = 0;
    for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++enchant_slot)
    {
        uint32 enchant_id = GetEnchantmentId(EnchantmentSlot(enchant_slot));
        if (!enchant_id)
            continue;

        SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!enchantEntry)
            continue;

        if (GemID == enchantEntry->GemID)
            ++count;
    }
    return count;
}

bool Item::IsLimitedToAnotherMapOrZone(uint32 cur_mapId, uint32 cur_zoneId) const
{
    ItemPrototype const* proto = GetProto();
    return proto && ((proto->Map && proto->Map != cur_mapId) || (proto->Area && proto->Area != cur_zoneId));
}

// Though the client has the information in the item's data field,
// we have to send SMSG_ITEM_TIME_UPDATE to display the remaining
// time.
// 尽管更新字段已有剩余时间，仍需发送 SMSG_ITEM_TIME_UPDATE 以促使客户端提示框倒计时显示
void Item::SendTimeUpdate(Player* owner) const
{
    uint32 duration = GetUInt32Value(ITEM_FIELD_DURATION);
    if (!duration)
        return;

    WorldPacket data(SMSG_ITEM_TIME_UPDATE, (8 + 4));
    data << ObjectGuid(GetObjectGuid());
    data << uint32(duration);
    owner->GetSession()->SendPacket(data);
}

// 静态工厂函数：创建并初始化指定数量与随机属性的 Item 或 Bag 实例
Item* Item::CreateItem(uint32 item, uint32 count, Player const* player, uint32 randomPropertyId)
{
    if (count < 1)
        return nullptr;                                        // don't create item at zero count (数量小于 1 不创建)

    if (ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item))
    {
        if (count > pProto->GetMaxStackSize())
            count = pProto->GetMaxStackSize();

        MANGOS_ASSERT(count != 0 && "pProto->Stackable == 0 but checked at loading already");

        Item* pItem = NewItemOrBag(pProto);
        if (pItem->Create(sObjectMgr.GenerateItemLowGuid(), item, player))
        {
            pItem->SetCount(count);
            if (uint32 randId = randomPropertyId ? randomPropertyId : Item::GenerateItemRandomPropertyId(item))
                pItem->SetItemRandomProperties(randId);

            return pItem;
        }
        delete pItem;
    }
    return nullptr;
}

// 克隆物品（常用于拆分堆叠时生成新物品实例，保留制造者、随机属性、标志与剩余时间）
Item* Item::CloneItem(uint32 count, Player const* player) const
{
    Item* newItem = CreateItem(GetEntry(), count, player, GetItemRandomPropertyId());
    if (!newItem)
        return nullptr;

    newItem->SetGuidValue(ITEM_FIELD_CREATOR,     GetGuidValue(ITEM_FIELD_CREATOR));
    newItem->SetGuidValue(ITEM_FIELD_GIFTCREATOR, GetGuidValue(ITEM_FIELD_GIFTCREATOR));
    newItem->SetUInt32Value(ITEM_FIELD_FLAGS,     GetUInt32Value(ITEM_FIELD_FLAGS));
    newItem->SetUInt32Value(ITEM_FIELD_DURATION,  GetUInt32Value(ITEM_FIELD_DURATION));
    return newItem;
}

// 判断物品是否已绑定且不属于指定玩家（或包含独立拾取数据）
bool Item::IsBindedNotWith(Player const* player) const
{
    // own item
    if (GetOwnerGuid() == player->GetObjectGuid())
        return false;

    // has loot with diff owner
    if (HasGeneratedLoot())
        return true;

    // not binded item
    if (!IsSoulBound())
        return false;

    return true;
}

void Item::AddToClientUpdateList()
{
    if (Player* pl = GetOwner())
        pl->GetMap()->AddUpdateObject(this);
}

void Item::RemoveFromClientUpdateList()
{
    if (Player* pl = GetOwner())
        pl->GetMap()->RemoveUpdateObject(this);
}

void Item::BuildUpdateData(UpdateDataMapType& update_players)
{
    if (Player* pl = GetOwner())
        BuildUpdateDataForPlayer(pl, update_players);

    ClearUpdateMask(false);
}

void Item::UpdateVisibility(UpdateDataMapType& /*update_players*/)
{
    if (Player* pl = GetOwner())
        pl->GetMap()->AddCreateAtClientObject(pl, this);

    if (ItsNewObject())
        SetItsNewObject(false);
}

// 判断物品是否允许与目标模板部分堆叠合并
InventoryResult Item::CanBeMergedPartlyWith(ItemPrototype const* proto) const
{
    // check item type (校验模板 ID 一致)
    if (GetEntry() != proto->ItemId)
        return EQUIP_ERR_ITEM_CANT_STACK;

    // check free space (full stacks can't be target of merge (已满堆叠不可作为合并目标)
    if (GetCount() >= proto->GetMaxStackSize())
        return EQUIP_ERR_ITEM_CANT_STACK;

    // not allow merge looting currently items (禁止合并尚未拾取完毕的容器类物品)
    if (HasGeneratedLoot())
        return EQUIP_ERR_ALREADY_LOOTED;

    return EQUIP_ERR_OK;
}

bool ItemRequiredTarget::IsFitToRequirements(Unit* pUnitTarget) const
{
    if (pUnitTarget->GetTypeId() != TYPEID_UNIT)
        return false;

    if (pUnitTarget->GetEntry() != m_uiTargetEntry)
        return false;

    switch (m_uiType)
    {
        case ITEM_TARGET_TYPE_CREATURE:
            return pUnitTarget->IsAlive();
        case ITEM_TARGET_TYPE_DEAD:
            return !pUnitTarget->IsAlive();
        default:
            return false;
    }
}

// 计算物品当前磨损耐久对应的基础修理费用（铜币，严格保持客户端同步舍入规则）
uint32 Item::GetBaseRepairCost() const
{
    if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
        return 0;

    uint32 maxDurability = GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    uint32 curDurability = GetUInt32Value(ITEM_FIELD_DURABILITY);
    if (!maxDurability || curDurability >= maxDurability)
        return 0;

    ItemPrototype const* proto = GetProto();

    // only weapons, and armor up to the last armor column, have a repair cost (仅武器与前 7 类护甲计算修理费)
    if (!(proto->Class == ITEM_CLASS_WEAPON && proto->SubClass <= 21) && !(proto->Class == ITEM_CLASS_ARMOR && proto->SubClass <= 7))
        return 0;

    DurabilityCostsEntry const* dcost = sDurabilityCostsStore.LookupEntry(proto->ItemLevel);
    if (!dcost)
        return 0;

    DurabilityQualityEntry const* dQualitymodEntry = sDurabilityQualityStore.LookupEntry((proto->Quality + 1) * 2);
    if (!dQualitymodEntry)
        return 0;

    double lost = double(maxDurability - curDurability);
    double qualityMod = double(dQualitymodEntry->quality_mod);
    double multiplier = double(dcost->multiplier[ItemSubClassToDurabilityMultiplierId(proto->Class, proto->SubClass)]);

    // keep these operations, their order and this rounding (half away from zero, at least 1): the prices shown to the player depend on them
    // 严格保持运算顺序与四舍五入方式（最少 1 铜），确保与客户端界面显示的修理价格完全一致
    double cost = float(qualityMod * multiplier * lost);
    uint32 costs = uint32(cost + 0.5);
    return costs ? costs : 1;
}

// 根据声望折扣系数计算实际耐久修理费（向偶数舍入）
uint32 Item::CalculateDurabilityRepairCost(float discountMod) const
{
    // the discounted price is rounded to float, then to the nearest integer, ties to even
    float costs = float(GetBaseRepairCost()) * discountMod;
    return uint32(std::lrintf(costs));
}

// 计算物品实际售店价格（按剩余使用次数折算并扣除耐久修理费，最低为 1 铜）
uint32 Item::GetSellPrice() const
{
    ItemPrototype const* proto = GetProto();
    int32 price = int32(proto->SellPrice);
    if (!price)
        return 0;

    // the value of an item destroyed when its charges are consumed follows the remaining charges
    // 耗尽充能即销毁的物品，售价按剩余充能比例折算
    uint8 index = 0;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellId && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
        {
            index = i;
            break;
        }
    }

    int32 baseCharges = proto->Spells[index].SpellCharges;
    if (baseCharges < -1)
        price = GetSpellCharges(index) * price / baseCharges;

    int32 repairCost = int32(GetBaseRepairCost());
    if (repairCost >= price)
        return 1;

    return uint32(price - repairCost);
}

// 推进物品内含掉落状态机，并在需要存库时同步标记物品本身为 ITEM_CHANGED
void Item::SetLootState(ItemLootUpdateState state)
{
    // ITEM_LOOT_NONE -> ITEM_LOOT_TEMPORARY -> ITEM_LOOT_NONE
    // ITEM_LOOT_NONE -> ITEM_LOOT_NEW -> ITEM_LOOT_NONE
    // ITEM_LOOT_NONE -> ITEM_LOOT_NEW -> ITEM_LOOT_UNCHANGED [<-> ITEM_LOOT_CHANGED] -> ITEM_LOOT_REMOVED -> ITEM_LOOT_NONE
    switch (state)
    {
        case ITEM_LOOT_NONE:
        case ITEM_LOOT_NEW:
            assert(false);                                 // not used in state change calls
            return;
        case ITEM_LOOT_TEMPORARY:
            assert(m_lootState == ITEM_LOOT_NONE);          // called only for not generated yet loot case
            m_lootState = ITEM_LOOT_TEMPORARY;
            break;
        case ITEM_LOOT_CHANGED:
            // new loot must stay in new state until saved, temporary must stay until remove
            if (m_lootState != ITEM_LOOT_NEW && m_lootState != ITEM_LOOT_TEMPORARY)
                m_lootState = m_lootState == ITEM_LOOT_NONE ? ITEM_LOOT_NEW : state;
            break;
        case ITEM_LOOT_UNCHANGED:
            // expected that called after DB update or load
            if (m_lootState == ITEM_LOOT_REMOVED)
                m_lootState = ITEM_LOOT_NONE;
            // temporary must stay until remove (ignore any changes)
            else if (m_lootState != ITEM_LOOT_TEMPORARY)
                m_lootState = ITEM_LOOT_UNCHANGED;
            break;
        case ITEM_LOOT_REMOVED:
            // if loot not saved then it existence in past can be just ignored
            if (m_lootState == ITEM_LOOT_NEW || m_lootState == ITEM_LOOT_TEMPORARY)
            {
                m_lootState = ITEM_LOOT_NONE;
                return;
            }

            m_lootState = ITEM_LOOT_REMOVED;
            break;
    }

    if (m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_UNCHANGED && m_lootState != ITEM_LOOT_TEMPORARY)
        SetState(ITEM_CHANGED);
}
