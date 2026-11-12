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

#include "Entities/ItemEnchantmentMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log/Log.h"
#include "Globals/ObjectMgr.h"
#include "Util/ProgressBar.h"
#include "Util/Util.h"

#include <list>
#include <vector>

// 随机附魔候选条目（附魔 ID 与出现概率）
struct EnchStoreItem
{
    uint32  ench;                                           // 随机附魔属性/后缀 ID
    float   chance;                                         // 抽取权重百分比

    EnchStoreItem()
        : ench(0), chance(0) {}

    EnchStoreItem(uint32 _ench, float _chance)
        : ench(_ench), chance(_chance) {}
};

typedef std::vector<EnchStoreItem> EnchStoreList;
typedef std::unordered_map<uint32, EnchStoreList> EnchantmentStore;

// 全局随机附魔模板表缓存（按模板组 entry 索引）
static EnchantmentStore RandomItemEnch;

// 从世界数据库加载 item_enchantment_template 表
void LoadRandomEnchantmentsTable()
{
    RandomItemEnch.clear();                                 // for reload case (重载前先清空缓存)

    uint32 count = 0;
    auto queryResult = WorldDatabase.Query("SELECT entry, ench, chance FROM item_enchantment_template");

    if (queryResult)
    {
        BarGoLink bar(queryResult->GetRowCount());

        do
        {
            Field* fields = queryResult->Fetch();
            bar.step();

            uint32 entry = fields[0].GetUInt32();
            uint32 ench = fields[1].GetUInt32();
            float chance = fields[2].GetFloat();

            if (chance > 0.000001f && chance <= 100.0f)
                RandomItemEnch[entry].push_back(EnchStoreItem(ench, chance));

            ++count;
        }
        while (queryResult->NextRow());

        sLog.outString(">> Loaded %u Item Enchantment definitions", count);
    }
    else
        sLog.outErrorDb(">> Loaded 0 Item Enchantment definitions. DB table `item_enchantment_template` is empty.");

    sLog.outString();
}

// 按概率从指定随机附魔组中抽取一条附魔 ID
uint32 GetItemEnchantMod(uint32 entry)
{
    if (!entry) return 0;

    EnchantmentStore::const_iterator tab = RandomItemEnch.find(entry);

    if (tab == RandomItemEnch.end())
    {
        sLog.outErrorDb("Item RandomProperty / RandomSuffix id #%u used in `item_template` but it doesn't have records in `item_enchantment_template` table.", entry);
        return 0;
    }

    double dRoll = rand_chance();
    float fCount = 0;

    const EnchStoreList& enchantList = tab->second;
    for (auto ench_iter : enchantList)
    {
        fCount += ench_iter.chance;

        if (fCount > dRoll) return ench_iter.ench;
    }

    // we could get here only if sum of all enchantment chances is lower than 100%
    // 若各条目概率总和不足 100%，按实际总和重新归一随机一次
    dRoll = (irand(0, (int)floor(fCount * 100) + 1)) / 100;
    fCount = 0;

    for (auto ench_iter : enchantList)
    {
        fCount += ench_iter.chance;

        if (fCount > dRoll) return ench_iter.ench;
    }

    return 0;
}

// 根据物品等级、穿戴槽位与品质从 RandomPropertiesPoints DBC 查询后缀系数
uint32 GenerateEnchSuffixFactor(uint32 item_id)
{
    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(item_id);

    if (!itemProto)
        return 0;
    if (!itemProto->RandomSuffix)
        return 0;

    RandomPropertiesPointsEntry const* randomProperty = sRandomPropertiesPointsStore.LookupEntry(itemProto->ItemLevel);
    if (!randomProperty)
        return 0;

    uint32 suffixFactor;
    switch (itemProto->InventoryType)
    {
        // Items of that type don`t have points (非装备、背包、战袍、弹药等类型无属性点系数)
        case INVTYPE_NON_EQUIP:
        case INVTYPE_BAG:
        case INVTYPE_TABARD:
        case INVTYPE_AMMO:
        case INVTYPE_QUIVER:
        case INVTYPE_RELIC:
            return 0;
        // Select point coefficient (按装备部位选取 0~4 号点位系数)
        case INVTYPE_HEAD:
        case INVTYPE_BODY:
        case INVTYPE_CHEST:
        case INVTYPE_LEGS:
        case INVTYPE_2HWEAPON:
        case INVTYPE_ROBE:
            suffixFactor = 0;
            break;
        case INVTYPE_SHOULDERS:
        case INVTYPE_WAIST:
        case INVTYPE_FEET:
        case INVTYPE_HANDS:
        case INVTYPE_TRINKET:
            suffixFactor = 1;
            break;
        case INVTYPE_NECK:
        case INVTYPE_WRISTS:
        case INVTYPE_FINGER:
        case INVTYPE_SHIELD:
        case INVTYPE_CLOAK:
        case INVTYPE_HOLDABLE:
            suffixFactor = 2;
            break;
        case INVTYPE_WEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
            suffixFactor = 3;
            break;
        case INVTYPE_RANGED:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:
            suffixFactor = 4;
            break;
        default:
            return 0;
    }
    // Select rare/epic modifier (按优秀/精良/史诗品质返回对应点数值)
    switch (itemProto->Quality)
    {
        case ITEM_QUALITY_UNCOMMON:
            return randomProperty->UncommonPropertiesPoints[suffixFactor];
        case ITEM_QUALITY_RARE:
            return randomProperty->RarePropertiesPoints[suffixFactor];
        case ITEM_QUALITY_EPIC:
            return randomProperty->EpicPropertiesPoints[suffixFactor];
        case ITEM_QUALITY_LEGENDARY:
        case ITEM_QUALITY_ARTIFACT:
            return 0;                                       // not have random properties (传说与神器不具有随机属性)
        default:
            break;
    }
    return 0;
}
