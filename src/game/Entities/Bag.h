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

#ifndef MANGOS_BAG_H
#define MANGOS_BAG_H

#include "Common.h"
#include "Entities/Item.h"

// 空背包与空槽位标记值
enum InventorySlot
{
    NULL_BAG = 0,                                           // 空背包索引（通常指默认行囊/装备栏上下文）
    NULL_SLOT = 255                                         // 无效槽位标记
};

// Maximum 36 Slots ( (CONTAINER_END - CONTAINER_FIELD_SLOT_1)/2 (最大 36 格容量，对应容器槽位字段跨度)
#define MAX_BAG_SIZE 36                                     // 2.0.12 (TBC 2.0.12 起容器最大格数)

// 背包容器类（继承自 Item，管理子物品槽位与容器更新字段）
class Bag : public Item
{
    public:

        Bag();
        ~Bag();

        // 将背包及其内部物品加入世界 / 从世界移除
        void AddToWorld() override;
        void RemoveFromWorld() override;

        // 创建新背包实例并初始化槽位
        bool Create(uint32 guidlow, uint32 itemid, Player const* owner) override;

        // 向指定槽位存入或移除物品
        void StoreItem(uint8 slot, Item* pItem);
        void RemoveItem(uint8 slot);

        // 按槽位、物品模板 ID 或 GUID 查询容器内物品与数量
        Item* GetItemByPos(uint8 slot) const;
        Item* GetItemByEntry(uint32 item) const;
        uint32 GetItemCount(uint32 item, Item* skipItem = nullptr) const;

        uint8 GetSlotByItemGUID(ObjectGuid guid) const;
        bool IsEmpty() const;
        uint32 GetFreeSlots() const;
        uint32 GetBagSize() const { return GetUInt32Value(CONTAINER_FIELD_NUM_SLOTS); }

        // DB operations (数据库持久化操作)
        // overwrite virtual Item::SaveToDB (重写 Item::SaveToDB)
        void SaveToDB() override;
        // overwrite virtual Item::LoadFromDB (重写 Item::LoadFromDB，初始化容器格数并清空槽位指针)
        bool LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid = ObjectGuid()) override;
        // overwrite virtual Item::DeleteFromDB (重写 Item::DeleteFromDB，级联删除包内物品记录)
        void DeleteFromDB() override;

        // 为目标玩家构建背包及包内物品的创建更新块
        void BuildCreateUpdateBlockForPlayer(UpdateData& data, Player* target) const override;

    protected:

        // Bag Storage space (背包物品指针数组)
        Item* m_bagslot[MAX_BAG_SIZE];
};

// 根据物品原型的 InventoryType 创建 Bag 或普通 Item 对象
inline Item* NewItemOrBag(ItemPrototype const* proto)
{
    if (proto->InventoryType == INVTYPE_BAG)
        return new Bag;

    return new Item;
}

#endif
