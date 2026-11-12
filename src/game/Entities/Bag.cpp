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

#include "Entities/Bag.h"
#include "Globals/ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Entities/UpdateData.h"

// 构造背包对象，设置容器对象掩码并清空槽位指针数组
Bag::Bag(): Item()
{
    m_objectType |= (TYPEMASK_ITEM | TYPEMASK_CONTAINER);
    m_objectTypeId = TYPEID_CONTAINER;

    m_valuesCount = CONTAINER_END;

    memset(m_bagslot, 0, sizeof(Item*) * MAX_BAG_SIZE);
}

// 析构背包时释放仍驻留在槽位中的所有 Item 对象
Bag::~Bag()
{
    for (auto& i : m_bagslot)
        delete i;
}

// 将背包本身及所有有效槽位内的物品加入世界
void Bag::AddToWorld()
{
    Item::AddToWorld();

    for (uint32 i = 0;  i < GetBagSize(); ++i)
        if (m_bagslot[i])
            m_bagslot[i]->AddToWorld();
}

// 先将槽位内所有物品移出世界，再将背包移出世界
void Bag::RemoveFromWorld()
{
    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i])
            m_bagslot[i]->RemoveFromWorld();

    Item::RemoveFromWorld();
}

// 创建背包实体，校验容器格数上限并初始化所有容器字段
bool Bag::Create(uint32 guidlow, uint32 itemid, Player const* owner)
{
    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(itemid);

    if (!itemProto || itemProto->ContainerSlots > MAX_BAG_SIZE)
        return false;

    Object::_Create(guidlow, guidlow, 0, HIGHGUID_CONTAINER);

    SetEntry(itemid);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(ITEM_FIELD_OWNER, owner ? owner->GetObjectGuid() : ObjectGuid());
    SetGuidValue(ITEM_FIELD_CONTAINED, owner ? owner->GetObjectGuid() : ObjectGuid());

    SetUInt32Value(ITEM_FIELD_MAXDURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_DURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_STACK_COUNT, 1);

    // Setting the number of Slots the Container has (设置容器的可用格数)
    SetUInt32Value(CONTAINER_FIELD_NUM_SLOTS, itemProto->ContainerSlots);

    // Cleaning 20 slots (清空所有容器槽位 GUID 与指针)
    for (uint8 i = 0; i < MAX_BAG_SIZE; ++i)
    {
        SetGuidValue(CONTAINER_FIELD_SLOT_1 + (i * 2), ObjectGuid());
        m_bagslot[i] = nullptr;
    }

    return true;
}

// 保存背包实例数据至数据库（复用 Item::SaveToDB）
void Bag::SaveToDB()
{
    Item::SaveToDB();
}

// 从数据库加载背包记录并重置槽位状态（包内物品后续由 character_inventory 填充）
bool Bag::LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid)
{
    if (!Item::LoadFromDB(guidLow, fields, ownerGuid))
        return false;

    ItemPrototype const* itemProto = sObjectMgr.GetItemPrototype(GetEntry());
    if (!itemProto || itemProto->ContainerSlots > MAX_BAG_SIZE)
        return false;

    // Setting the number of Slots the Container has (设置容器格数)
    SetUInt32Value(CONTAINER_FIELD_NUM_SLOTS, itemProto->ContainerSlots);

    // cleanup bag content related item value fields (its will be filled correctly from `character_inventory`)
    // 清理背包槽位相关的更新字段与旧指针（稍后从 character_inventory 表逐格装载）
    for (int i = 0; i < MAX_BAG_SIZE; ++i)
    {
        SetGuidValue(CONTAINER_FIELD_SLOT_1 + (i * 2), ObjectGuid());

        delete m_bagslot[i];
        m_bagslot[i] = nullptr;
    }

    return true;
}

// 从数据库删除背包及其内部所有物品的持久化记录
void Bag::DeleteFromDB()
{
    for (auto& i : m_bagslot)
        if (i)
            i->DeleteFromDB();

    Item::DeleteFromDB();
}

// 统计背包当前剩余空槽位数量
uint32 Bag::GetFreeSlots() const
{
    uint32 slots = 0;
    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (!m_bagslot[i])
            ++slots;

    return slots;
}

// 从指定槽位移除物品引用并清空对应更新字段
void Bag::RemoveItem(uint8 slot)
{
    MANGOS_ASSERT(slot < MAX_BAG_SIZE);

    if (m_bagslot[slot])
        m_bagslot[slot]->SetContainer(nullptr);

    m_bagslot[slot] = nullptr;
    SetGuidValue(CONTAINER_FIELD_SLOT_1 + (slot * 2), ObjectGuid());
}

// 将物品放入指定槽位，同步更新容器/所有者 GUID 与物品所属关系
void Bag::StoreItem(uint8 slot, Item* pItem)
{
    MANGOS_ASSERT(slot < MAX_BAG_SIZE);

    if (pItem)
    {
        m_bagslot[slot] = pItem;
        SetGuidValue(CONTAINER_FIELD_SLOT_1 + (slot * 2), pItem->GetObjectGuid());
        pItem->SetGuidValue(ITEM_FIELD_CONTAINED, GetObjectGuid());
        pItem->SetGuidValue(ITEM_FIELD_OWNER, GetOwnerGuid());
        pItem->SetContainer(this);
        pItem->SetSlot(slot);
    }
}

// 为目标玩家生成背包本体以及其中每个物品的创建更新块
void Bag::BuildCreateUpdateBlockForPlayer(UpdateData& data, Player* target) const
{
    Item::BuildCreateUpdateBlockForPlayer(data, target);

    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i])
            m_bagslot[i]->BuildCreateUpdateBlockForPlayer(data, target);
}

// If the bag is empty returns true (若背包所有槽位均为空则返回 true)
bool Bag::IsEmpty() const
{
    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i])
            return false;

    return true;
}

// 按物品模板 ID 查找背包内首个匹配物品
Item* Bag::GetItemByEntry(uint32 item) const
{
    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i] && m_bagslot[i]->GetEntry() == item)
            return m_bagslot[i];

    return nullptr;
}

// 统计背包及包内物品（含忽略物品宝石唯一性检查时的镶嵌宝石）中指定模板 ID 的总数量
uint32 Bag::GetItemCount(uint32 item, Item* skipItem) const
{
    uint32 count = 0;

    if (this != skipItem && GetEntry() == item)
        ++count;

    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i])
            if (m_bagslot[i] != skipItem && m_bagslot[i]->GetEntry() == item)
                count += m_bagslot[i]->GetCount();

    if (skipItem && skipItem->GetProto()->GemProperties)
        for (uint32 i = 0; i < GetBagSize(); ++i)
            if (m_bagslot[i])
                if (m_bagslot[i] != skipItem && m_bagslot[i]->GetProto()->Socket[0].Color)
                    count += m_bagslot[i]->GetGemCountWithID(item);

    return count;
}

// 根据物品 GUID 查找其在背包中的槽位索引，未找到则返回 NULL_SLOT
uint8 Bag::GetSlotByItemGUID(ObjectGuid guid) const
{
    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i])
            if (m_bagslot[i]->GetObjectGuid() == guid)
                return i;

    return NULL_SLOT;
}

// 获取指定槽位的物品指针（越界返回 nullptr）
Item* Bag::GetItemByPos(uint8 slot) const
{
    if (slot < GetBagSize())
        return m_bagslot[slot];

    return nullptr;
}
