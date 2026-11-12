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

#ifndef MANGOSSERVER_ITEM_H
#define MANGOSSERVER_ITEM_H

#include "Common.h"
#include "Entities/Object.h"
#include "Entities/ItemPrototype.h"

struct SpellEntry;
class Bag;
class Field;
class QueryResult;
class Unit;
class Loot;
struct SpellModifier;

// 套装激活效果信息（记录当前已装备件数与对应触发法术）
struct ItemSetEffect
{
    uint32 item_count = 0;                                  // 已装备同套装件数
    SpellEntry const* spells[8] = {};                       // 达到件数阈值激活的套装奖励法术
};

// 物品装备/移动/使用/拆分操作结果错误码枚举（下发 SMSG_INVENTORY_CHANGE_FAILURE）
enum InventoryResult
{
    EQUIP_ERR_OK                                 = 0,       // 成功
    EQUIP_ERR_CANT_EQUIP_LEVEL_I                 = 1,       // ERR_CANT_EQUIP_LEVEL_I (等级不足)
    EQUIP_ERR_CANT_EQUIP_SKILL                   = 2,       // ERR_CANT_EQUIP_SKILL (技能等级不足)
    EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT             = 3,       // ERR_WRONG_SLOT (该物品无法放入此装备栏)
    EQUIP_ERR_BAG_FULL                           = 4,       // ERR_BAG_FULL (背包已满)
    EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG        = 5,       // ERR_BAG_IN_BAG (非空背包不能替换或放入其他背包)
    EQUIP_ERR_CANT_TRADE_EQUIP_BAGS              = 6,       // ERR_TRADE_EQUIPPED_BAG (无法交易已装备的背包)
    EQUIP_ERR_ONLY_AMMO_CAN_GO_HERE              = 7,       // ERR_AMMO_ONLY (此处只能放置弹药)
    EQUIP_ERR_NO_REQUIRED_PROFICIENCY            = 8,       // ERR_PROFICIENCY_NEEDED (缺少所需护甲/武器熟练度)
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE        = 9,       // ERR_NO_SLOT_AVAILABLE (没有可用的装备栏)
    EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM        = 10,      // ERR_CANT_EQUIP_EVER (种族/职业等原因永久无法使用)
    EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM2       = 11,      // ERR_CANT_EQUIP_EVER
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE2       = 12,      // ERR_NO_SLOT_AVAILABLE
    EQUIP_ERR_CANT_EQUIP_WITH_TWOHANDED          = 13,      // ERR_2HANDED_EQUIPPED (已装备双手武器，无法再装备副手)
    EQUIP_ERR_CANT_DUAL_WIELD                    = 14,      // ERR_2HSKILLNOTFOUND (尚未学会双持技能)
    EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG            = 15,      // ERR_WRONG_BAG_TYPE (物品类型与专业背包不匹配)
    EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2           = 16,      // ERR_WRONG_BAG_TYPE
    EQUIP_ERR_CANT_CARRY_MORE_OF_THIS            = 17,      // ERR_ITEM_MAX_COUNT (超出唯一/最大持有数量上限)
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE3       = 18,      // ERR_NO_SLOT_AVAILABLE
    EQUIP_ERR_ITEM_CANT_STACK                    = 19,      // ERR_CANT_STACK (该物品无法堆叠)
    EQUIP_ERR_ITEM_CANT_BE_EQUIPPED              = 20,      // ERR_NOT_EQUIPPABLE (该物品无法装备)
    EQUIP_ERR_ITEMS_CANT_BE_SWAPPED              = 21,      // ERR_CANT_SWAP (无法交换这两件物品)
    EQUIP_ERR_SLOT_IS_EMPTY                      = 22,      // ERR_SLOT_EMPTY (目标槽位为空)
    EQUIP_ERR_ITEM_NOT_FOUND                     = 23,      // ERR_ITEM_NOT_FOUND (未找到物品)
    EQUIP_ERR_CANT_DROP_SOULBOUND                = 24,      // ERR_DROP_BOUND_ITEM (无法丢弃灵魂绑定物品)
    EQUIP_ERR_OUT_OF_RANGE                       = 25,      // ERR_OUT_OF_RANGE (超出距离)
    EQUIP_ERR_TRIED_TO_SPLIT_MORE_THAN_COUNT     = 26,      // ERR_TOO_FEW_TO_SPLIT (拆分数量大于当前堆叠数)
    EQUIP_ERR_COULDNT_SPLIT_ITEMS                = 27,      // ERR_SPLIT_FAILED (物品拆分失败)
    EQUIP_ERR_MISSING_REAGENT                    = 28,      // ERR_SPELL_FAILED_REAGENTS_GENERIC (缺少施法材料)
    EQUIP_ERR_NOT_ENOUGH_MONEY                   = 29,      // ERR_NOT_ENOUGH_MONEY (金币不足)
    EQUIP_ERR_NOT_A_BAG                          = 30,      // ERR_NOT_A_BAG (目标不是背包)
    EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS        = 31,      // ERR_DESTROY_NONEMPTY_BAG (仅能对空背包执行此操作)
    EQUIP_ERR_DONT_OWN_THAT_ITEM                 = 32,      // ERR_NOT_OWNER (你不拥有该物品)
    EQUIP_ERR_CAN_EQUIP_ONLY1_QUIVER             = 33,      // ERR_ONLY_ONE_QUIVER (只能装备一个箭袋)
    EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT        = 34,      // ERR_NO_BANK_SLOT (必须先购买该银行背包栏位)
    EQUIP_ERR_TOO_FAR_AWAY_FROM_BANK             = 35,      // ERR_NO_BANK_HERE (距离银行过远)
    EQUIP_ERR_ITEM_LOCKED                        = 36,      // ERR_ITEM_LOCKED (物品正被锁定，如交易或拆分中)
    EQUIP_ERR_YOU_ARE_STUNNED                    = 37,      // ERR_GENERIC_STUNNED (昏迷状态下无法操作)
    EQUIP_ERR_YOU_ARE_DEAD                       = 38,      // ERR_PLAYER_DEAD (死亡状态下无法操作)
    EQUIP_ERR_CANT_DO_RIGHT_NOW                  = 39,      // ERR_CLIENT_LOCKED_OUT (当前无法执行该操作)
    EQUIP_ERR_INT_BAG_ERROR                      = 40,      // ERR_INTERNAL_BAG_ERROR (内部背包错误)
    EQUIP_ERR_CAN_EQUIP_ONLY1_BOLT               = 41,      // ERR_ONLY_ONE_BOLT (只能装备一个弩箭袋)
    EQUIP_ERR_CAN_EQUIP_ONLY1_AMMOPOUCH          = 42,      // ERR_ONLY_ONE_AMMO (只能装备一个弹药袋)
    EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED          = 43,      // ERR_CANT_WRAP_STACKABLE (可堆叠物品无法包装)
    EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED           = 44,      // ERR_CANT_WRAP_EQUIPPED (已装备物品无法包装)
    EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED            = 45,      // ERR_CANT_WRAP_WRAPPED (已包装物品无法再次包装)
    EQUIP_ERR_BOUND_CANT_BE_WRAPPED              = 46,      // ERR_CANT_WRAP_BOUND (灵魂绑定物品无法包装)
    EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED             = 47,      // ERR_CANT_WRAP_UNIQUE (唯一物品无法包装)
    EQUIP_ERR_BAGS_CANT_BE_WRAPPED               = 48,      // ERR_CANT_WRAP_BAGS (背包无法包装)
    EQUIP_ERR_ALREADY_LOOTED                     = 49,      // ERR_LOOT_GONE (物品已被拾取)
    EQUIP_ERR_INVENTORY_FULL                     = 50,      // ERR_INV_FULL (物品栏已满)
    EQUIP_ERR_BANK_FULL                          = 51,      // ERR_BAG_FULL (银行已满)
    EQUIP_ERR_ITEM_IS_CURRENTLY_SOLD_OUT         = 52,      // ERR_VENDOR_SOLD_OUT (商人处该物品已售罄)
    EQUIP_ERR_BAG_FULL3                          = 53,      // ERR_BAG_FULL
    EQUIP_ERR_ITEM_NOT_FOUND2                    = 54,      // ERR_ITEM_NOT_FOUND
    EQUIP_ERR_ITEM_CANT_STACK2                   = 55,      // ERR_CANT_STACK
    EQUIP_ERR_BAG_FULL4                          = 56,      // ERR_BAG_FULL
    EQUIP_ERR_ITEM_SOLD_OUT                      = 57,      // ERR_VENDOR_SOLD_OUT
    EQUIP_ERR_OBJECT_IS_BUSY                     = 58,      // ERR_OBJECT_IS_BUSY (对象正忙)
    EQUIP_ERR_NONE                               = 59,      // ERR_CANT_BE_DISENCHANTED (无法被分解)
    EQUIP_ERR_NOT_IN_COMBAT                      = 60,      // ERR_NOT_IN_COMBAT (战斗中无法执行)
    EQUIP_ERR_NOT_WHILE_DISARMED                 = 61,      // ERR_NOT_WHILE_DISARMED (被缴械时无法执行)
    EQUIP_ERR_BAG_FULL6                          = 62,      // ERR_BAG_FULL
    EQUIP_ERR_CANT_EQUIP_RANK                    = 63,      // ERR_CANT_EQUIP_RANK (军衔不足)
    EQUIP_ERR_CANT_EQUIP_REPUTATION              = 64,      // ERR_CANT_EQUIP_REPUTATION (声望等级不足)
    EQUIP_ERR_TOO_MANY_SPECIAL_BAGS              = 65,      // ERR_TOO_MANY_SPECIAL_BAGS (特殊背包数量超出上限)
    EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW            = 66,      // ERR_LOOT_CANT_LOOT_THAT_NOW (当前无法拾取该物品)
    EQUIP_ERR_ITEM_UNIQUE_EQUIPABLE              = 67,      // ERR_ITEM_UNIQUE_EQUIPABLE (同类别唯一装备已穿戴)
    EQUIP_ERR_VENDOR_MISSING_TURNINS             = 68,      // ERR_VENDOR_MISSING_TURNINS (缺少兑换所需的物品/徽章)
    EQUIP_ERR_NOT_ENOUGH_HONOR_POINTS            = 69,      // ERR_NOT_ENOUGH_HONOR_POINTS (荣誉点数不足)
    EQUIP_ERR_NOT_ENOUGH_ARENA_POINTS            = 70,      // ERR_NOT_ENOUGH_ARENA_POINTS (竞技场点数不足)
    EQUIP_ERR_ITEM_MAX_COUNT_SOCKETED            = 71,      // ERR_ITEM_MAX_COUNT_SOCKETED (该唯一宝石持有/镶嵌数已达上限)
    EQUIP_ERR_MAIL_BOUND_ITEM                    = 72,      // ERR_MAIL_BOUND_ITEM (无法邮寄灵魂绑定物品)
    EQUIP_ERR_NO_SPLIT_WHILE_PROSPECTING         = 73,      // ERR_INTERNAL_BAG_ERROR (选矿期间不可拆分)
    EQUIP_ERR_BAG_FULL7                          = 74,      // ERR_BAG_FULL
    EQUIP_ERR_ITEM_MAX_COUNT_EQUIPPED_SOCKETED   = 75,      // ERR_ITEM_MAX_COUNT_EQUIPPED_SOCKETED (已装备同类唯一宝石达到上限)
    EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED    = 76,      // ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED (只能装备一颗该唯一宝石)
    EQUIP_ERR_TOO_MUCH_GOLD                      = 77,      // ERR_TOO_MUCH_GOLD (携带金币达到上限)
    EQUIP_ERR_NOT_DURING_ARENA_MATCH             = 78,      // ERR_NOT_DURING_ARENA_MATCH (竞技场比赛中不可使用)
    EQUIP_ERR_CANNOT_TRADE_THAT                  = 79,      // ERR_TRADE_BOUND_ITEM (无法交易绑定物品)
    EQUIP_ERR_PERSONAL_ARENA_RATING_TOO_LOW      = 80,      // ERR_CANT_EQUIP_RATING (个人竞技场等级不足)
    EQUIP_ERR_EVENT_AUTOEQUIP_BIND_CONFIRM       = 81,            // EQUIP_ERR_OK, EVENT_AUTOEQUIP_BIND_CONFIRM (触发绑定确认提示)
    // probably exist more
};

// 向商人购买物品的失败原因枚举
enum class BuyResult
{
    CANT_FIND_ITEM                      = 0,                // 未找到商品
    ITEM_ALREADY_SOLD                   = 1,                // 商品已售出
    NOT_ENOUGHT_MONEY                   = 2,                // 金币不足
    SELLER_DONT_LIKE_YOU                = 4,                // 商人对你声望敌对/冷淡
    DISTANCE_TOO_FAR                    = 5,                // 距离商人过远
    SILENT_ERROR                        = 6,                // 静默错误
    ITEM_SOLD_OUT                       = 7,                // 商品已售罄
    CANT_CARRY_MORE                     = 8,                // 无法携带更多
    RANK_REQUIRE                        = 11,               // 军衔不足
    REPUTATION_REQUIRE                  = 12                // 声望不足
};

// 向商人出售物品的失败原因枚举
enum class SellResult
{
    CANT_FIND_ITEM                      = 1,                // 未找到待售物品
    CANT_SELL_ITEM                      = 2,       // merchant doesn't like that item (该物品不可出售)
    CANT_FIND_VENDOR                    = 3,       // merchant doesn't like you (未找到商人)
    YOU_DONT_OWN_THAT_ITEM              = 4,       // you don't own that item (你不拥有该物品)
    NONE                                = 5,       // nothing appears...
    ONLY_EMPTY_BAG                      = 6,       // can only do with empty bags (只能出售空背包)
    VENDOR_DOESNT_BUY_THIS_ITEM         = 7,                // 商人拒收此类物品
};

// -1 from client enchantment slot number (物品附魔槽位索引，比客户端协议槽位少 1 或按更新字段顺排)
enum EnchantmentSlot
{
    PERM_ENCHANTMENT_SLOT           = 0,                    // 永久附魔槽位
    TEMP_ENCHANTMENT_SLOT           = 1,                    // 临时附魔槽位（涂油/磨刀石/毒药等）
    SOCK_ENCHANTMENT_SLOT           = 2,                    // 宝石插槽 1
    SOCK_ENCHANTMENT_SLOT_2         = 3,                    // 宝石插槽 2
    SOCK_ENCHANTMENT_SLOT_3         = 4,                    // 宝石插槽 3
    BONUS_ENCHANTMENT_SLOT          = 5,                    // 插槽颜色全匹配奖励附魔槽位
    MAX_INSPECTED_ENCHANTMENT_SLOT  = 6,                    // 观察界面可见附魔槽位上限

    PROP_ENCHANTMENT_SLOT_0         = 6,                    // used with RandomSuffix (随机后缀槽位 0)
    PROP_ENCHANTMENT_SLOT_1         = 7,                    // used with RandomSuffix (随机后缀槽位 1)
    PROP_ENCHANTMENT_SLOT_2         = 8,                    // used with RandomSuffix and RandomProperty (随机后缀/随机属性槽位 2)
    PROP_ENCHANTMENT_SLOT_3         = 9,                    // used with RandomProperty (随机属性槽位 3)
    PROP_ENCHANTMENT_SLOT_4         = 10,                   // used with RandomProperty (随机属性槽位 4)
    MAX_ENCHANTMENT_SLOT            = 11                    // 附魔槽位总数
};

#define MAX_VISIBLE_ITEM_OFFSET       16                    // 16 fields per visible item (creator(2) + enchantments(12) + properties(1) + pad(1)) (玩家可见装备槽每件占 16 个更新字段)

#define MAX_GEM_SOCKETS               MAX_ITEM_PROTO_SOCKETS// (BONUS_ENCHANTMENT_SLOT-SOCK_ENCHANTMENT_SLOT) and item proto size, equal value expected

// 单个附魔槽位内部字段偏移（每槽位占 3 个 uint32）
enum EnchantmentOffset
{
    ENCHANTMENT_ID_OFFSET       = 0,                        // SpellItemEnchantment ID
    ENCHANTMENT_DURATION_OFFSET = 1,                        // 剩余持续时间（秒）
    ENCHANTMENT_CHARGES_OFFSET  = 2                         // 剩余使用次数
};

#define MAX_ENCHANTMENT_OFFSET    3

// 附魔槽位附加行为掩码（记录在 ITEM_FIELD_ENCHANTMENT_... 以外的服务端逻辑标志）
enum EnchantmentSlotMask
{
    ENCHANTMENT_SOULBOUND               = 0x01,             // 附魔后使物品灵魂绑定
    ENCHANTMENT_DO_NOT_LOG              = 0x02,             // 不记入日志
    ENCHANTMENT_MAINHAND_ONLY           = 0x04, // removed when MH is swapped elsewhere (仅限主手有效，主手武器被换下时清除)
    ENCHANTMENT_ALLOW_ENTERING_ARENA    = 0x08              // 附魔允许带入竞技场
};

// 物品数据库持久化更新状态
enum ItemUpdateState
{
    ITEM_UNCHANGED                               = 0,       // 未变更
    ITEM_CHANGED                                 = 1,       // 已修改（待 UPDATE）
    ITEM_NEW                                     = 2,       // 新建（待 INSERT）
    ITEM_REMOVED                                 = 3        // 已删除（待 DELETE）
};

// 可开启物品（如箱子/钱袋）内部掉落持久化状态
enum ItemLootUpdateState
{
    ITEM_LOOT_NONE                                = 0,      // loot not generated (尚未生成掉落)
    ITEM_LOOT_TEMPORARY                           = 1,      // generated loot is temporary (will deleted at loot window close) (临时掉落，关闭拾取窗即清除)
    ITEM_LOOT_UNCHANGED                           = 2,      // 数据库已同步且未改变
    ITEM_LOOT_CHANGED                             = 3,      // 掉落内容有变动
    ITEM_LOOT_NEW                                 = 4,      // 新生成的待持久化掉落
    ITEM_LOOT_REMOVED                             = 5       // 掉落已清空待删库
};

// masks for ITEM_FIELD_FLAGS field (物品实例动态标志位掩码)
enum ItemDynFlags
{
    ITEM_DYNFLAG_BINDED                       = 0x00000001, // set in game at binding (已灵魂绑定)
    ITEM_DYNFLAG_TRANSLATED                   = 0x00000002, // translated to lang id in query (已按语言翻译)
    ITEM_DYNFLAG_UNLOCKED                     = 0x00000004, // have meaning only for item with proto->LockId, if not set show as "Locked, req. lockpicking N" (已解锁，配合 LockID 使用)
    ITEM_DYNFLAG_WRAPPED                      = 0x00000008, // mark item as wrapped into wrapper container (已被礼物纸包装)
    ITEM_DYNFLAG_UNK4                         = 0x00000010, // can't repeat old note: appears red icon (like when item durability==0)
    ITEM_DYNFLAG_UNK5                         = 0x00000020,
    ITEM_DYNFLAG_UNK6                         = 0x00000040, // ? old note: usable
    ITEM_DYNFLAG_UNK7                         = 0x00000080,
    ITEM_DYNFLAG_UNK8                         = 0x00000100,
    ITEM_DYNFLAG_READABLE                     = 0x00000200, // can be open for read, it or item proto pagetText make show "Right click to read" (可右键阅读)
    ITEM_DYNFLAG_UNK10                        = 0x00000400,
    ITEM_DYNFLAG_UNK11                        = 0x00000800,
    ITEM_DYNFLAG_UNK12                        = 0x00001000,
    ITEM_DYNFLAG_UNK13                        = 0x00002000,
    ITEM_DYNFLAG_UNK14                        = 0x00004000,
    ITEM_DYNFLAG_UNK15                        = 0x00008000,
    ITEM_DYNFLAG_UNK16                        = 0x00010000,
    ITEM_DYNFLAG_UNK17                        = 0x00020000,
    ITEM_DYNFLAG_UNK18                        = 0x00040000,
    ITEM_DYNFLAG_UNK19                        = 0x00080000,
    ITEM_DYNFLAG_UNK20                        = 0x00100000,
    ITEM_DYNFLAG_UNK21                        = 0x00200000,
    ITEM_DYNFLAG_UNK22                        = 0x00400000,
    ITEM_DYNFLAG_UNK23                        = 0x00800000,
    ITEM_DYNFLAG_UNK24                        = 0x01000000,
    ITEM_DYNFLAG_UNK25                        = 0x02000000,
    ITEM_DYNFLAG_UNK26                        = 0x04000000,
    ITEM_DYNFLAG_UNK27                        = 0x08000000,
};

// 物品使用所需目标类型（对应 item_required_target 表）
enum ItemRequiredTargetType
{
    ITEM_TARGET_TYPE_CREATURE   = 1,                        // 需指定生物 Entry（存活）
    ITEM_TARGET_TYPE_DEAD       = 2                         // 需指定生物 Entry（尸体）
};

#define MAX_ITEM_REQ_TARGET_TYPE 2

// 物品使用目标限制条件结构体
struct ItemRequiredTarget
{
    ItemRequiredTarget(ItemRequiredTargetType uiType, uint32 uiTargetEntry) : m_uiType(uiType), m_uiTargetEntry(uiTargetEntry) {}
    ItemRequiredTargetType m_uiType;                        // 目标类型要求
    uint32 m_uiTargetEntry;                                 // 目标生物模板 ID

    // helpers (校验目标单位是否满足要求)
    bool IsFitToRequirements(Unit* pUnitTarget) const;
};

// 检查物品原型是否可以放入指定容器背包（考虑特殊背包族与不可放背包嵌套限制）
bool ItemCanGoIntoBag(ItemPrototype const* pProto, ItemPrototype const* pBagProto);

// 物品实例类（管理运行时物品状态、附魔、宝石、随机属性、耐久、掉落及数据库同步）
class Item : public Object
{
    public:
        // 静态工厂方法：创建并初始化指定数量和随机属性的物品（或 Bag）
        static Item* CreateItem(uint32 item, uint32 count, Player const* player = nullptr, uint32 randomPropertyId = 0);
        // 克隆当前物品实例（复制随机属性、绑定状态、附魔等，并指定新堆叠数量）
        Item* CloneItem(uint32 count, Player const* player = nullptr) const;

        Item();
        virtual ~Item();

        virtual bool Create(uint32 guidlow, uint32 itemid, Player const* owner);

        void RemoveFromWorld() override;

        ItemPrototype const* GetProto() const;

        ObjectGuid const& GetOwnerGuid() const { return GetGuidValue(ITEM_FIELD_OWNER); }
        void SetOwnerGuid(ObjectGuid guid) { SetGuidValue(ITEM_FIELD_OWNER, guid); }
        Player* GetOwner()const;

        // 设置/检查灵魂绑定状态与绑定归属
        void SetBinding(bool val) { ApplyModFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED, val); }
        bool IsSoulBound() const { return HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED); }
        bool IsBindedNotWith(Player const* player) const;
        bool IsBoundByEnchant() const;
        bool IsMainHandOnlyEnchant(EnchantmentSlot slot) const;
        bool CanEnterArenaEnchant(EnchantmentSlot slot) const;

        // 数据库持久化操作（保存、加载、删除、清理物品内掉落记录）
        virtual void SaveToDB();
        virtual bool LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid = ObjectGuid());
        virtual void DeleteFromDB();
        void DeleteFromInventoryDB() const;
        void LoadLootFromDB(Field* fields);

        bool IsBag() const { return GetProto()->InventoryType == INVTYPE_BAG; }
        bool IsBroken() const { return GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 && GetUInt32Value(ITEM_FIELD_DURABILITY) == 0; }
        bool CanBeTraded() const;
        void SetInTrade(bool b = true) { mb_in_trade = b; }
        bool IsInTrade() const { return mb_in_trade; }

        // 物品法术/目标/区域/插槽宝石匹配校验
        bool IsFitToSpellRequirements(SpellEntry const* spellInfo) const;
        bool IsTargetValidForItemUse(Unit* pUnitTarget) const;
        bool IsLimitedToAnotherMapOrZone(uint32 cur_mapId, uint32 cur_zoneId) const;
        bool GemsFitSockets() const;

        uint32 GetCount() const { return GetUInt32Value(ITEM_FIELD_STACK_COUNT); }
        void SetCount(uint32 value) { SetUInt32Value(ITEM_FIELD_STACK_COUNT, value); }
        uint32 GetMaxStackCount() const { return GetProto()->GetMaxStackSize(); }
        uint8 GetGemCountWithID(uint32 GemID) const;
        InventoryResult CanBeMergedPartlyWith(ItemPrototype const* proto) const;

        // 物品所在容器与槽位访问器
        uint8 GetSlot() const {return m_slot;}
        Bag* GetContainer() const { return m_container; }
        uint8 GetBagSlot() const;
        void SetSlot(uint8 slot) {m_slot = slot;}
        uint16 GetPos() const { return uint16(GetBagSlot()) << 8 | GetSlot(); }
        void SetContainer(Bag* container) { m_container = container; }

        bool IsInBag() const { return m_container != nullptr; }
        bool IsEquipped() const;

        uint32 GetSkill() const;
        uint32 GetSpell() const;

        // RandomPropertyId (signed but stored as unsigned) (随机属性/后缀 ID 及系数操作，正值为随机属性，负值为随机后缀)
        int32 GetItemRandomPropertyId() const { return GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID); }
        uint32 GetItemSuffixFactor() const { return GetUInt32Value(ITEM_FIELD_PROPERTY_SEED); }
        void SetItemRandomProperties(int32 randomPropId);
        void UpdateItemSuffixFactor();
        static int32 GenerateItemRandomPropertyId(uint32 item_id);

        // 附魔槽位读写与清除
        void SetEnchantment(EnchantmentSlot slot, uint32 id, uint32 duration, uint32 charges, ObjectGuid caster = ObjectGuid());
        void SetEnchantmentDuration(EnchantmentSlot slot, uint32 duration);
        void SetEnchantmentCharges(EnchantmentSlot slot, uint32 charges);
        void ClearEnchantment(EnchantmentSlot slot);
        uint32 GetEnchantmentId(EnchantmentSlot slot)       const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_ID_OFFSET);}
        uint32 GetEnchantmentDuration(EnchantmentSlot slot) const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_DURATION_OFFSET);}
        uint32 GetEnchantmentCharges(EnchantmentSlot slot)  const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_CHARGES_OFFSET);}

        uint32 GetEnchantmentModifier() { return m_enchantmentModifier; }
        void SetEnchantmentModifier(uint32 value) { m_enchantmentModifier = value; }

        // 下发限时物品剩余时间更新并逐帧扣减时长
        void SendTimeUpdate(Player* owner) const;
        void UpdateDuration(Player* owner, uint32 diff);

        // spell charges (negative means that once charges are consumed the item should be deleted)
        // 物品使用充能次数读写（负值表示充能耗尽后自动销毁物品）
        int32 GetSpellCharges(uint8 index/*0..5*/ = 0) const { return GetInt32Value(ITEM_FIELD_SPELL_CHARGES + index); }
        void SetSpellCharges(uint8 index/*0..5*/, int32 value) { SetInt32Value(ITEM_FIELD_SPELL_CHARGES + index, value); }

        // 修理费用与出售价格计算
        uint32 GetBaseRepairCost() const;
        uint32 CalculateDurabilityRepairCost(float discountMod) const;
        uint32 GetSellPrice() const;

        // 物品内含掉落（宝箱/包裹类物品）持久化状态控制
        void SetLootState(ItemLootUpdateState state);
        bool HasGeneratedLoot() const { return m_loot && m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_REMOVED; }
        bool HasTemporaryLoot() const { return m_loot && m_lootState == ITEM_LOOT_TEMPORARY; }

        bool HasSavedLoot() const { return m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_NEW && m_lootState != ITEM_LOOT_TEMPORARY; }

        // Update States (物品更新队列及数据库脏状态维护)
        ItemUpdateState GetState() const { return uState; }
        void SetState(ItemUpdateState state, Player* forplayer = nullptr);
        void AddToUpdateQueueOf(Player* player);
        void RemoveFromUpdateQueueOf(Player* player);
        bool IsInUpdateQueue() const { return uQueuePos != -1; }
        uint16 GetQueuePos() const { return uQueuePos; }
        void FSetState(ItemUpdateState state)               // forced (强制设置状态而不加入更新队列)
        {
            uState = state;
        }

        bool HasQuest(uint32 quest_id) const override { return GetProto()->StartQuest == quest_id; }
        bool HasInvolvedQuest(uint32 /*quest_id*/) const override { return false; }
        bool IsPotion() const { return GetProto()->IsPotion(); }
        bool IsConjuredConsumable() const { return GetProto()->IsConjuredConsumable(); }

        // 客户端对象更新列表注册与增量数据包构建
        void AddToClientUpdateList() override;
        void RemoveFromClientUpdateList() override;
        void BuildUpdateData(UpdateDataMapType& update_players) override;
        void UpdateVisibility(UpdateDataMapType& update_players) override;

        bool IsUsedInSpell() const { return m_usedInSpell; }
        void SetUsedInSpell(bool state) { m_usedInSpell = state; }
    private:
        uint8 m_slot;                                       // 在父容器或角色栏中的槽位索引
        Bag* m_container;                                   // 所属背包指针（nullptr 表示在主行囊/装备栏/银行）
        ItemUpdateState uState;                             // 数据库更新状态
        int16 uQueuePos;                                    // 在玩家 m_itemUpdateQueue 中的下标（-1 表示未入队）
        bool mb_in_trade;                                   // true if item is currently in trade-window (是否正处于交易窗口中)
        ItemLootUpdateState m_lootState;                    // 物品内置掉落状态
        bool m_usedInSpell;                                 // 是否正在被施法过程消耗/引用
        uint32 m_enchantmentModifier; // used by one script and removed in wotlk (脚本专用附魔数值修正)
};

#endif
