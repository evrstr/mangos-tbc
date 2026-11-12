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

#ifndef _ITEMPROTOTYPE_H
#define _ITEMPROTOTYPE_H

#include "Common.h"

// 物品属性加成类型枚举（对应 item_template 中的 stat_type1~10）
enum ItemModType
{
    ITEM_MOD_MANA                     = 0,                  // 法力值
    ITEM_MOD_HEALTH                   = 1,                  // 生命值
    ITEM_MOD_AGILITY                  = 3,                  // 敏捷
    ITEM_MOD_STRENGTH                 = 4,                  // 力量
    ITEM_MOD_INTELLECT                = 5,                  // 智力
    ITEM_MOD_SPIRIT                   = 6,                  // 精神
    ITEM_MOD_STAMINA                  = 7,                  // 耐力
    ITEM_MOD_DEFENSE_SKILL_RATING     = 12,                 // 防御等级
    ITEM_MOD_DODGE_RATING             = 13,                 // 躲闪等级
    ITEM_MOD_PARRY_RATING             = 14,                 // 招架等级
    ITEM_MOD_BLOCK_RATING             = 15,                 // 格挡等级
    ITEM_MOD_HIT_MELEE_RATING         = 16,                 // 近战命中等级
    ITEM_MOD_HIT_RANGED_RATING        = 17,                 // 远程命中等级
    ITEM_MOD_HIT_SPELL_RATING         = 18,                 // 法术命中等级
    ITEM_MOD_CRIT_MELEE_RATING        = 19,                 // 近战爆击等级
    ITEM_MOD_CRIT_RANGED_RATING       = 20,                 // 远程爆击等级
    ITEM_MOD_CRIT_SPELL_RATING        = 21,                 // 法术爆击等级
    ITEM_MOD_HIT_TAKEN_MELEE_RATING   = 22,                 // 受到近战命中等级修正
    ITEM_MOD_HIT_TAKEN_RANGED_RATING  = 23,                 // 受到远程命中等级修正
    ITEM_MOD_HIT_TAKEN_SPELL_RATING   = 24,                 // 受到法术命中等级修正
    ITEM_MOD_CRIT_TAKEN_MELEE_RATING  = 25,                 // 受到近战爆击等级修正
    ITEM_MOD_CRIT_TAKEN_RANGED_RATING = 26,                 // 受到远程爆击等级修正
    ITEM_MOD_CRIT_TAKEN_SPELL_RATING  = 27,                 // 受到法术爆击等级修正
    ITEM_MOD_HASTE_MELEE_RATING       = 28,                 // 近战急速等级
    ITEM_MOD_HASTE_RANGED_RATING      = 29,                 // 远程急速等级
    ITEM_MOD_HASTE_SPELL_RATING       = 30,                 // 法术急速等级
    ITEM_MOD_HIT_RATING               = 31,                 // 全命中等级
    ITEM_MOD_CRIT_RATING              = 32,                 // 全爆击等级
    ITEM_MOD_HIT_TAKEN_RATING         = 33,                 // 全受到命中等级修正
    ITEM_MOD_CRIT_TAKEN_RATING        = 34,                 // 全受到爆击等级修正
    ITEM_MOD_RESILIENCE_RATING        = 35,                 // 韧性等级
    ITEM_MOD_HASTE_RATING             = 36,                 // 全急速等级
    ITEM_MOD_EXPERTISE_RATING         = 37                  // 精准等级
};

#define MAX_ITEM_MOD                    38

// 物品附带法术触发时机类型
enum ItemSpelltriggerType
{
    ITEM_SPELLTRIGGER_ON_USE          = 0,                  // use after equip cooldown (使用触发，需等待装备冷却)
    ITEM_SPELLTRIGGER_ON_EQUIP        = 1,                  // 装备时触发
    ITEM_SPELLTRIGGER_CHANCE_ON_HIT   = 2,                  // 击中时有几率触发
    ITEM_SPELLTRIGGER_SOULSTONE       = 4,                  // 灵魂石类触发
    ITEM_SPELLTRIGGER_ON_STORE        = 5,                  // casted at add item to inventory/equip, applied aura removed at remove item, item deleted at aura cancel/expire/etc (入包/装备时自动施放，移除物品时消散光环，光环过期时删除物品)
    ITEM_SPELLTRIGGER_LEARN_SPELL_ID  = 6                   // used in item_template.spell_2 with spell_id with SPELL_GENERIC_LEARN in spell_1 (用于配方物品，spell_1 为通用学习法术，spell_2 为目标法术 ID)
};

#define MAX_ITEM_SPELLTRIGGER           7

// 物品绑定类型枚举
enum ItemBondingType
{
    NO_BIND                                     = 0,        // 不绑定
    BIND_WHEN_PICKED_UP                         = 1,        // 拾取绑定
    BIND_WHEN_EQUIPPED                          = 2,        // 装备后绑定
    BIND_WHEN_USE                               = 3,        // 使用后绑定
    BIND_QUEST_ITEM                             = 4,        // 任务物品
    BIND_QUEST_ITEM1                            = 5         // not used in game (游戏内未使用)
};

#define MAX_BIND_TYPE                             6

// Mask for ItemPrototype.Flags field (物品模板 Flags 字段标志位掩码)
enum ItemPrototypeFlags
{
    ITEM_FLAG_NO_PICKUP                         = 0x00000001, // not used
    ITEM_FLAG_CONJURED                          = 0x00000002, // 魔法制造物品（下线一定时间后消失）
    ITEM_FLAG_HAS_LOOT                          = 0x00000004, // affect only non container items that can be "open" for loot. It or lockid set enable for client show "Right click to open". See also ITEM_DYNFLAG_UNLOCKED (可右键开启拾取的非容器物品，如钱袋/宝箱)
    ITEM_FLAG_HEROIC_TOOLTIP                    = 0x00000008, // heroic item version (提示框显示“英雄”标签)
    ITEM_FLAG_DEPRECATED                        = 0x00000010, // can't repeat old note: appears red icon (like when item durability==0) (已废弃物品)
    ITEM_FLAG_NO_USER_DESTROY                   = 0x00000020, // used for totem. Item can not be destroyed, except by using spell (item can be reagent for spell and then allowed) (玩家不可手动摧毁，如萨满图腾)
    ITEM_FLAG_PLAYERCAST                        = 0x00000040, // ? old note: usable
    ITEM_FLAG_NO_EQUIPCOOLDOWN                  = 0x00000080, // 装备后不触发 30 秒切换冷却
    ITEM_FLAG_MULTILOOTQUEST                    = 0x00000100, // saw this on item 47115, 49295...
    ITEM_FLAG_IS_WRAPPER                        = 0x00000200, // used or not used wrapper (包装纸物品)
    ITEM_FLAG_USES_RESOURCES                    = 0x00000400, // ignore bag space at new item creation?
    ITEM_FLAG_MULTI_DROP                        = 0x00000800, // determines if item is party loot or not (小队共享掉落任务物品，队伍每人可各拾取一份)
    ITEM_FLAG_ITEMPURCHASE_RECORD               = 0x00001000, // item cost can be refunded within 2 hours after purchase (购买后 2 小时内可退还)
    ITEM_FLAG_PETITION                          = 0x00002000, // arena/guild charter (公会/竞技场战队登记表)
    ITEM_FLAG_HAS_TEXT                          = 0x00004000, // 包含可读文本内容
    ITEM_FLAG_NO_DISENCHANT                     = 0x00008000, // a lot of items have this (不可被附魔分解)
    ITEM_FLAG_REAL_DURATION                     = 0x00010000, // a lot of items have this (按现实时间计时过期)
    ITEM_FLAG_NO_CREATOR                        = 0x00020000, // 不记录制造者 GUID
    ITEM_FLAG_IS_PROSPECTABLE                   = 0x00040000, // item can have prospecting loot (in fact some items expected have empty loot) (可被珠宝加工选矿)
    ITEM_FLAG_UNIQUE_EQUIPPABLE                 = 0x00080000, // 唯一装备（背包可持有多件，但同时仅能装备一件）
    ITEM_FLAG_IGNORE_FOR_AURAS                  = 0x00100000,
    ITEM_FLAG_IGNORE_DEFAULT_ARENA_RESTRICTIONS = 0x00200000, // 可在竞技场中使用（忽略默认竞技场物品限制）
    ITEM_FLAG_NO_DURABILITY_LOSS                = 0x00400000, // Only items of ITEM_SUBCLASS_WEAPON_THROWN have it but not all, so can't be used as in game check (不损失耐久度)
    ITEM_FLAG_USE_WHEN_SHAPESHIFTED             = 0x00800000, // last used flag in 2.3.0 (变形状态下可使用，2.3.0 引入)
};

// 特殊容器（箭袋、灵魂袋、草药袋等）可容纳的物品族掩码
enum BagFamilyMask
{
    BAG_FAMILY_MASK_NONE                      = 0x00000000, // 通用背包
    BAG_FAMILY_MASK_ARROWS                    = 0x00000001, // 箭矢
    BAG_FAMILY_MASK_BULLETS                   = 0x00000002, // 子弹
    BAG_FAMILY_MASK_SOUL_SHARDS               = 0x00000004, // 灵魂碎片
    BAG_FAMILY_MASK_LEATHERWORKING_SUPP       = 0x00000008, // 制皮材料
    BAG_FAMILY_MASK_INSCRIPTION_SUPP          = 0x00000010, // 铭文材料
    BAG_FAMILY_MASK_HERBS                     = 0x00000020, // 草药
    BAG_FAMILY_MASK_ENCHANTING_SUPP           = 0x00000040, // 附魔材料
    BAG_FAMILY_MASK_ENGINEERING_SUPP          = 0x00000080, // 工程材料
    BAG_FAMILY_MASK_KEYS                      = 0x00000100, // 钥匙
    BAG_FAMILY_MASK_GEMS                      = 0x00000200, // 宝石
    BAG_FAMILY_MASK_MINING_SUPP               = 0x00000400, // 采矿材料
    BAG_FAMILY_MASK_SOULBOUND_EQUIPMENT       = 0x00000800, // 灵魂绑定装备
    BAG_FAMILY_MASK_VANITY_PETS               = 0x00001000, // 观赏宠物
    BAG_FAMILY_MASK_CURRENCY_TOKENS           = 0x00002000, // 货币徽章
    BAG_FAMILY_MASK_QUEST_ITEMS               = 0x00004000  // 任务物品
};

// 装备宝石插槽颜色位掩码
enum SocketColor
{
    SOCKET_COLOR_META                           = 1,        // 多彩插槽
    SOCKET_COLOR_RED                            = 2,        // 红色插槽
    SOCKET_COLOR_YELLOW                         = 4,        // 黄色插槽
    SOCKET_COLOR_BLUE                           = 8         // 蓝色插槽
};

#define SOCKET_COLOR_ALL (SOCKET_COLOR_META | SOCKET_COLOR_RED | SOCKET_COLOR_YELLOW | SOCKET_COLOR_BLUE)

// 物品穿戴部位类型（对应 InventoryType）
enum InventoryType
{
    INVTYPE_NON_EQUIP                           = 0,        // 不可装备
    INVTYPE_HEAD                                = 1,        // 头部
    INVTYPE_NECK                                = 2,        // 颈部
    INVTYPE_SHOULDERS                           = 3,        // 肩部
    INVTYPE_BODY                                = 4,        // 衬衣
    INVTYPE_CHEST                               = 5,        // 胸部
    INVTYPE_WAIST                               = 6,        // 腰部
    INVTYPE_LEGS                                = 7,        // 腿部
    INVTYPE_FEET                                = 8,        // 脚部
    INVTYPE_WRISTS                              = 9,        // 手腕
    INVTYPE_HANDS                               = 10,       // 手部
    INVTYPE_FINGER                              = 11,       // 手指
    INVTYPE_TRINKET                             = 12,       // 饰品
    INVTYPE_WEAPON                              = 13,       // 单手武器
    INVTYPE_SHIELD                              = 14,       // 盾牌
    INVTYPE_RANGED                              = 15,       // 远程武器（弓/弩）
    INVTYPE_CLOAK                               = 16,       // 背部（披风）
    INVTYPE_2HWEAPON                            = 17,       // 双手武器
    INVTYPE_BAG                                 = 18,       // 容器背包
    INVTYPE_TABARD                              = 19,       // 战袍
    INVTYPE_ROBE                                = 20,       // 长袍（占用胸部槽位）
    INVTYPE_WEAPONMAINHAND                      = 21,       // 主手武器
    INVTYPE_WEAPONOFFHAND                       = 22,       // 副手武器
    INVTYPE_HOLDABLE                            = 23,       // 副手物品（法杖副手/书籍等）
    INVTYPE_AMMO                                = 24,       // 弹药
    INVTYPE_THROWN                              = 25,       // 投掷武器
    INVTYPE_RANGEDRIGHT                         = 26,       // 枪械/魔杖
    INVTYPE_QUIVER                              = 27,       // 箭袋/弹药袋
    INVTYPE_RELIC                               = 28        // 圣物（圣契/神像/图腾）
};

#define MAX_INVTYPE                               29

// 物品主分类枚举（ItemClass.dbc）
enum ItemClass
{
    ITEM_CLASS_CONSUMABLE                       = 0,        // 消耗品
    ITEM_CLASS_CONTAINER                        = 1,        // 容器
    ITEM_CLASS_WEAPON                           = 2,        // 武器
    ITEM_CLASS_GEM                              = 3,        // 宝石
    ITEM_CLASS_ARMOR                            = 4,        // 护甲
    ITEM_CLASS_REAGENT                          = 5,        // 施法材料
    ITEM_CLASS_PROJECTILE                       = 6,        // 弹药
    ITEM_CLASS_TRADE_GOODS                      = 7,        // 商品/专业材料
    ITEM_CLASS_GENERIC                          = 8,        // 通用（未使用）
    ITEM_CLASS_RECIPE                           = 9,        // 配方/图纸
    ITEM_CLASS_MONEY                            = 10,       // 金币（未使用）
    ITEM_CLASS_QUIVER                           = 11,       // 箭袋
    ITEM_CLASS_QUEST                            = 12,       // 任务物品
    ITEM_CLASS_KEY                              = 13,       // 钥匙
    ITEM_CLASS_PERMANENT                        = 14,       // 永久效果（未使用）
    ITEM_CLASS_MISC                             = 15        // 杂项
};

#define MAX_ITEM_CLASS                            16

// 消耗品子类
enum ItemSubclassConsumable
{
    ITEM_SUBCLASS_CONSUMABLE                    = 0,
    ITEM_SUBCLASS_POTION                        = 1,        // 药水
    ITEM_SUBCLASS_ELIXIR                        = 2,        // 药剂
    ITEM_SUBCLASS_FLASK                         = 3,        // 合剂
    ITEM_SUBCLASS_SCROLL                        = 4,        // 卷轴
    ITEM_SUBCLASS_FOOD                          = 5,        // 食物与饮料
    ITEM_SUBCLASS_ITEM_ENHANCEMENT              = 6,        // 物品临时附魔（磨刀石/巫师之油）
    ITEM_SUBCLASS_BANDAGE                       = 7,        // 绷带
    ITEM_SUBCLASS_CONSUMABLE_OTHER              = 8
};

#define MAX_ITEM_SUBCLASS_CONSUMABLE              9

// 容器子类
enum ItemSubclassContainer
{
    ITEM_SUBCLASS_CONTAINER                     = 0,        // 普通背包
    ITEM_SUBCLASS_SOUL_CONTAINER                = 1,        // 灵魂袋
    ITEM_SUBCLASS_HERB_CONTAINER                = 2,        // 草药袋
    ITEM_SUBCLASS_ENCHANTING_CONTAINER          = 3,        // 附魔材料袋
    ITEM_SUBCLASS_ENGINEERING_CONTAINER         = 4,        // 工程学材料袋
    ITEM_SUBCLASS_GEM_CONTAINER                 = 5,        // 宝石袋
    ITEM_SUBCLASS_MINING_CONTAINER              = 6,        // 矿石袋
    ITEM_SUBCLASS_LEATHERWORKING_CONTAINER      = 7         // 制皮材料袋
};

#define MAX_ITEM_SUBCLASS_CONTAINER               8

// 武器子类
enum ItemSubclassWeapon
{
    ITEM_SUBCLASS_WEAPON_AXE                    = 0,        // 单手斧
    ITEM_SUBCLASS_WEAPON_AXE2                   = 1,        // 双手斧
    ITEM_SUBCLASS_WEAPON_BOW                    = 2,        // 弓
    ITEM_SUBCLASS_WEAPON_GUN                    = 3,        // 枪械
    ITEM_SUBCLASS_WEAPON_MACE                   = 4,        // 单手锤
    ITEM_SUBCLASS_WEAPON_MACE2                  = 5,        // 双手锤
    ITEM_SUBCLASS_WEAPON_POLEARM                = 6,        // 长柄武器
    ITEM_SUBCLASS_WEAPON_SWORD                  = 7,        // 单手剑
    ITEM_SUBCLASS_WEAPON_SWORD2                 = 8,        // 双手剑
    ITEM_SUBCLASS_WEAPON_obsolete               = 9,        // 已废弃
    ITEM_SUBCLASS_WEAPON_STAFF                  = 10,       // 法杖
    ITEM_SUBCLASS_WEAPON_EXOTIC                 = 11,       // 异域单手
    ITEM_SUBCLASS_WEAPON_EXOTIC2                = 12,       // 异域双手
    ITEM_SUBCLASS_WEAPON_FIST                   = 13,       // 拳套
    ITEM_SUBCLASS_WEAPON_MISC                   = 14,       // 杂项武器（如铁匠锤、鱼竿外杂类）
    ITEM_SUBCLASS_WEAPON_DAGGER                 = 15,       // 匕首
    ITEM_SUBCLASS_WEAPON_THROWN                 = 16,       // 投掷武器
    ITEM_SUBCLASS_WEAPON_SPEAR                  = 17,       // 矛
    ITEM_SUBCLASS_WEAPON_CROSSBOW               = 18,       // 弩
    ITEM_SUBCLASS_WEAPON_WAND                   = 19,       // 魔杖
    ITEM_SUBCLASS_WEAPON_FISHING_POLE           = 20        // 钓鱼竿
};

#define MAX_ITEM_SUBCLASS_WEAPON                  21

// 宝石子类
enum ItemSubclassGem
{
    ITEM_SUBCLASS_GEM_RED                       = 0,        // 红色宝石
    ITEM_SUBCLASS_GEM_BLUE                      = 1,        // 蓝色宝石
    ITEM_SUBCLASS_GEM_YELLOW                    = 2,        // 黄色宝石
    ITEM_SUBCLASS_GEM_PURPLE                    = 3,        // 紫色宝石（红+蓝）
    ITEM_SUBCLASS_GEM_GREEN                     = 4,        // 绿色宝石（黄+蓝）
    ITEM_SUBCLASS_GEM_ORANGE                    = 5,        // 橙色宝石（红+黄）
    ITEM_SUBCLASS_GEM_META                      = 6,        // 多彩宝石
    ITEM_SUBCLASS_GEM_SIMPLE                    = 7,        // 简易宝石
    ITEM_SUBCLASS_GEM_PRISMATIC                 = 8         // 棱彩宝石（匹配任意红黄蓝插槽）
};

#define MAX_ITEM_SUBCLASS_GEM                     9

// 护甲子类
enum ItemSubclassArmor
{
    ITEM_SUBCLASS_ARMOR_MISC                    = 0,        // 杂项护甲（饰品/戒指/项链/披风等）
    ITEM_SUBCLASS_ARMOR_CLOTH                   = 1,        // 布甲
    ITEM_SUBCLASS_ARMOR_LEATHER                 = 2,        // 皮甲
    ITEM_SUBCLASS_ARMOR_MAIL                    = 3,        // 锁甲
    ITEM_SUBCLASS_ARMOR_PLATE                   = 4,        // 板甲
    ITEM_SUBCLASS_ARMOR_BUCKLER                 = 5,        // 圆盾（已废弃）
    ITEM_SUBCLASS_ARMOR_SHIELD                  = 6,        // 盾牌
    ITEM_SUBCLASS_ARMOR_LIBRAM                  = 7,        // 圣契
    ITEM_SUBCLASS_ARMOR_IDOL                    = 8,        // 神像
    ITEM_SUBCLASS_ARMOR_TOTEM                   = 9         // 图腾
};

#define MAX_ITEM_SUBCLASS_ARMOR                   10

// 施法材料子类
enum ItemSubclassReagent
{
    ITEM_SUBCLASS_REAGENT                       = 0
};

#define MAX_ITEM_SUBCLASS_REAGENT                 1

// 弹药子类
enum ItemSubclassProjectile
{
    ITEM_SUBCLASS_WAND                          = 0,        // ABS
    ITEM_SUBCLASS_BOLT                          = 1,        // ABS
    ITEM_SUBCLASS_ARROW                         = 2,        // 箭矢
    ITEM_SUBCLASS_BULLET                        = 3,        // 子弹
    ITEM_SUBCLASS_THROWN                        = 4         // ABS
};

#define MAX_ITEM_SUBCLASS_PROJECTILE              5

// 商品/专业材料子类
enum ItemSubclassTradeGoods
{
    ITEM_SUBCLASS_TRADE_GOODS                   = 0,
    ITEM_SUBCLASS_PARTS                         = 1,        // 零件
    ITEM_SUBCLASS_EXPLOSIVES                    = 2,        // 爆炸物
    ITEM_SUBCLASS_DEVICES                       = 3,        // 装置
    ITEM_SUBCLASS_JEWELCRAFTING                 = 4,        // 珠宝加工材料
    ITEM_SUBCLASS_CLOTH                         = 5,        // 布料
    ITEM_SUBCLASS_LEATHER                       = 6,        // 皮革
    ITEM_SUBCLASS_METAL_STONE                   = 7,        // 金属与矿石
    ITEM_SUBCLASS_MEAT                          = 8,        // 肉类
    ITEM_SUBCLASS_HERB                          = 9,        // 草药
    ITEM_SUBCLASS_ELEMENTAL                     = 10,       // 元素微粒/源生材料
    ITEM_SUBCLASS_TRADE_GOODS_OTHER             = 11,       // 其他商品
    ITEM_SUBCLASS_ENCHANTING                    = 12,       // 附魔材料
    ITEM_SUBCLASS_MATERIAL                      = 13        // Added in 2.4.2 (2.4.2 新增材料分类)
};

#define MAX_ITEM_SUBCLASS_TRADE_GOODS             14

enum ItemSubclassGeneric
{
    ITEM_SUBCLASS_GENERIC                       = 0
};

#define MAX_ITEM_SUBCLASS_GENERIC                 1

// 配方图纸子类
enum ItemSubclassRecipe
{
    ITEM_SUBCLASS_BOOK                          = 0,        // 书籍
    ITEM_SUBCLASS_LEATHERWORKING_PATTERN        = 1,        // 制皮图样
    ITEM_SUBCLASS_TAILORING_PATTERN             = 2,        // 裁缝图样
    ITEM_SUBCLASS_ENGINEERING_SCHEMATIC         = 3,        // 工程学结构图
    ITEM_SUBCLASS_BLACKSMITHING                 = 4,        // 锻造设计图
    ITEM_SUBCLASS_COOKING_RECIPE                = 5,        // 烹饪食谱
    ITEM_SUBCLASS_ALCHEMY_RECIPE                = 6,        // 炼金配方
    ITEM_SUBCLASS_FIRST_AID_MANUAL              = 7,        // 急救手册
    ITEM_SUBCLASS_ENCHANTING_FORMULA            = 8,        // 附魔公式
    ITEM_SUBCLASS_FISHING_MANUAL                = 9,        // 钓鱼指南
    ITEM_SUBCLASS_JEWELCRAFTING_RECIPE          = 10        // 珠宝设计图
};

#define MAX_ITEM_SUBCLASS_RECIPE                  11

enum ItemSubclassMoney
{
    ITEM_SUBCLASS_MONEY                         = 0
};

#define MAX_ITEM_SUBCLASS_MONEY                   1

// 箭袋/弹药袋子类
enum ItemSubclassQuiver
{
    ITEM_SUBCLASS_QUIVER0                       = 0,        // ABS
    ITEM_SUBCLASS_QUIVER1                       = 1,        // ABS
    ITEM_SUBCLASS_QUIVER                        = 2,        // 箭袋
    ITEM_SUBCLASS_AMMO_POUCH                    = 3         // 弹药袋
};

#define MAX_ITEM_SUBCLASS_QUIVER                  4

enum ItemSubclassQuest
{
    ITEM_SUBCLASS_QUEST                         = 0
};

#define MAX_ITEM_SUBCLASS_QUEST                   1

// 钥匙子类
enum ItemSubclassKey
{
    ITEM_SUBCLASS_KEY                           = 0,        // 钥匙
    ITEM_SUBCLASS_LOCKPICK                      = 1         // 开锁工具
};

#define MAX_ITEM_SUBCLASS_KEY                     2

enum ItemSubclassPermanent
{
    ITEM_SUBCLASS_PERMANENT                     = 0
};

#define MAX_ITEM_SUBCLASS_PERMANENT               1

// 杂项子类
enum ItemSubclassJunk
{
    ITEM_SUBCLASS_JUNK                          = 0,        // 垃圾/杂物
    ITEM_SUBCLASS_JUNK_REAGENT                  = 1,        // 材料杂项
    ITEM_SUBCLASS_JUNK_PET                      = 2,        // 伴生宠物
    ITEM_SUBCLASS_JUNK_HOLIDAY                  = 3,        // 节日物品
    ITEM_SUBCLASS_JUNK_OTHER                    = 4,        // 其他
    ITEM_SUBCLASS_JUNK_MOUNT                    = 5         // 坐骑
};

#define MAX_ITEM_SUBCLASS_JUNK                    6

// 雕文子类（WLK 预留）
enum ItemSubclassGlyph
{
    ITEM_SUBCLASS_GLYPH_WARRIOR                 = 1,
    ITEM_SUBCLASS_GLYPH_PALADIN                 = 2,
    ITEM_SUBCLASS_GLYPH_HUNTER                  = 3,
    ITEM_SUBCLASS_GLYPH_ROGUE                   = 4,
    ITEM_SUBCLASS_GLYPH_PRIEST                  = 5,
    ITEM_SUBCLASS_GLYPH_DEATH_KNIGHT            = 6,
    ITEM_SUBCLASS_GLYPH_SHAMAN                  = 7,
    ITEM_SUBCLASS_GLYPH_MAGE                    = 8,
    ITEM_SUBCLASS_GLYPH_WARLOCK                 = 9,
    ITEM_SUBCLASS_GLYPH_DRUID                   = 11
};

#define MAX_ITEM_SUBCLASS_GLYPH                   12

// 各物品主分类对应的子类数量上限查表数组
const uint32 MaxItemSubclassValues[MAX_ITEM_CLASS] =
{
    MAX_ITEM_SUBCLASS_CONSUMABLE,
    MAX_ITEM_SUBCLASS_CONTAINER,
    MAX_ITEM_SUBCLASS_WEAPON,
    MAX_ITEM_SUBCLASS_GEM,
    MAX_ITEM_SUBCLASS_ARMOR,
    MAX_ITEM_SUBCLASS_REAGENT,
    MAX_ITEM_SUBCLASS_PROJECTILE,
    MAX_ITEM_SUBCLASS_TRADE_GOODS,
    MAX_ITEM_SUBCLASS_GENERIC,
    MAX_ITEM_SUBCLASS_RECIPE,
    MAX_ITEM_SUBCLASS_MONEY,
    MAX_ITEM_SUBCLASS_QUIVER,
    MAX_ITEM_SUBCLASS_QUEST,
    MAX_ITEM_SUBCLASS_KEY,
    MAX_ITEM_SUBCLASS_PERMANENT,
    MAX_ITEM_SUBCLASS_JUNK
};

// 将武器/护甲子类映射为 DurabilityCosts.dbc 中的耐久修理系数索引
inline uint8 ItemSubClassToDurabilityMultiplierId(uint32 ItemClass, uint32 ItemSubClass)
{
    switch (ItemClass)
    {
        case ITEM_CLASS_WEAPON: return ItemSubClass;
        case ITEM_CLASS_ARMOR:  return ItemSubClass + 21;
    }
    return 0;
}

// 服务端扩展物品标志（对应 item_template.ExtraFlags）
enum ItemExtraFlags
{
    ITEM_EXTRA_REAL_TIME_DURATION  = 0x01, // if set and have Duration time, then offline time included in counting, if not set then counted only in game time (离线也继续扣除物品限时时长)
    ITEM_EXTRA_IGNORE_QUEST_STATUS = 0x02, // if set, queststarter item will drop for player regardless of the related quest's status (无视任务状态强制掉落任务触发物品)
    ITEM_EXTRA_ALL                 = 0x03  // all used flags, used for check DB data (mask all above flags) (所有有效标志掩码，用于数据库校验)
};

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

// 物品武器伤害区间与伤害系别
struct _Damage
{
    float   DamageMin;                                      // 最小伤害
    float   DamageMax;                                      // 最大伤害
    uint32  DamageType;                                     // id from Resistances.dbc (伤害属性类型，见 Resistances.dbc)
};

// 物品属性条目
struct _ItemStat
{
    uint32  ItemStatType;                                   // 属性类型（ItemModType）
    int32   ItemStatValue;                                  // 属性数值
};

// 物品附加法术条目
struct _Spell
{
    uint32 SpellId;                                         // id from Spell.dbc (法术 ID)
    uint32 SpellTrigger;                                    // 触发方式（ItemSpelltriggerType）
    int32  SpellCharges;                                    // 可用充能次数（负值表示耗尽后销毁物品）
    float  SpellPPMRate;                                    // 每分钟触发次数 (PPM)
    int32  SpellCooldown;                                   // 单法术冷却（毫秒）
    uint32 SpellCategory;                                   // id from SpellCategory.dbc (冷却分类 ID)
    int32  SpellCategoryCooldown;                           // 分类冷却（毫秒）
};

// 物品宝石插槽定义
struct _Socket
{
    uint32 Color;                                           // 插槽颜色掩码（SocketColor）
    uint32 Content;                                         // 默认镶嵌内容（通常为 0）
};

#define MAX_ITEM_PROTO_DAMAGES 5
#define MAX_ITEM_PROTO_SOCKETS 3
#define MAX_ITEM_PROTO_SPELLS  5
#define MAX_ITEM_PROTO_STATS  10

// 物品静态模板结构体（紧凑 1 字节对齐，直接映射 item_template 数据库记录并用于客户端查询响应）
struct ItemPrototype
{
    uint32 ItemId;                                          // 物品模板 ID
    uint32 Class;                                           // id from ItemClass.dbc (物品主类)
    uint32 SubClass;                                        // id from ItemSubClass.dbc (物品子类)
    uint32 Unk0;
    char*  Name1;                                           // 物品名称
    uint32 DisplayInfoID;                                   // id from ItemDisplayInfo.dbc (外观模型贴图 ID)
    uint32 Quality;                                         // 物品品质（灰/白/绿/蓝/紫/橙等）
    uint32 Flags;                                           // 物品标志（ItemPrototypeFlags）
    uint32 BuyCount;                                        // 商人单次购买堆叠数
    uint32 BuyPrice;                                        // 购买价格（铜币）
    uint32 SellPrice;                                       // 出售给商人的价格（铜币）
    uint32 InventoryType;                                   // 穿戴部位（InventoryType）
    uint32 AllowableClass;                                  // 允许职业位掩码
    uint32 AllowableRace;                                   // 允许种族位掩码
    uint32 ItemLevel;                                       // 物品等级 (iLvl)
    uint32 RequiredLevel;                                   // 需求角色等级
    uint32 RequiredSkill;                                   // id from SkillLine.dbc (需求专业/武器技能 ID)
    uint32 RequiredSkillRank;                               // 需求技能点数
    uint32 RequiredSpell;                                   // id from Spell.dbc (需求已学法术)
    uint32 RequiredHonorRank;                               // 需求荣誉军衔
    uint32 RequiredCityRank;                                // 需求主城声望军衔
    uint32 RequiredReputationFaction;                       // id from Faction.dbc (需求声望阵营 ID)
    uint32 RequiredReputationRank;                          // 需求声望等级
    uint32 MaxCount;                                        // 玩家背包/银行最大持有数量（0=不限，1=唯一）
    uint32 Stackable;                                       // 单格最大堆叠数
    uint32 ContainerSlots;                                  // 若为背包则表示容器格数
    _ItemStat ItemStat[MAX_ITEM_PROTO_STATS];               // 最多 10 项绿字/白字属性加成
    _Damage Damage[MAX_ITEM_PROTO_DAMAGES];                 // 最多 5 组武器伤害区间
    uint32 Armor;                                           // 护甲值
    uint32 HolyRes;                                         // 神圣抗性
    uint32 FireRes;                                         // 火焰抗性
    uint32 NatureRes;                                       // 自然抗性
    uint32 FrostRes;                                        // 冰霜抗性
    uint32 ShadowRes;                                       // 暗影抗性
    uint32 ArcaneRes;                                       // 奥术抗性
    uint32 Delay;                                           // 武器攻击间隔（毫秒）
    uint32 AmmoType;                                        // 弹药类型（2=箭，3=子弹）
    float  RangedModRange;                                  // 远程武器射程修正
    _Spell Spells[MAX_ITEM_PROTO_SPELLS];                   // 最多 5 个附加法术效果
    uint32 Bonding;                                         // 绑定规则（ItemBondingType）
    char*  Description;                                     // 物品底部描述（黄字说明）
    uint32 PageText;                                        // 关联书籍页面 ID
    uint32 LanguageID;                                      // 阅读所需语言 ID
    uint32 PageMaterial;                                    // 书籍纸张材质
    uint32 StartQuest;                                      // id from QuestCache.wdb (触发接取的任务 ID)
    uint32 LockID;                                          // 锁具 ID（对应 Lock.dbc）
    uint32 Material;                                        // id from Material.dbc (材质音效/物理分类)
    uint32 Sheath;                                          // 武器收纳位置样式
    uint32 RandomProperty;                                  // id from ItemRandomProperties.dbc (随机属性组)
    uint32 RandomSuffix;                                    // id from ItemRandomSuffix.dbc (随机后缀组)
    uint32 Block;                                           // 盾牌格挡值
    uint32 ItemSet;                                         // id from ItemSet.dbc (所属套装 ID)
    uint32 MaxDurability;                                   // 最大耐久度
    uint32 Area;                                            // id from AreaTable.dbc (限定使用区域)
    uint32 Map;                                             // id from Map.dbc (限定使用地图)
    uint32 BagFamily;                                       // bit mask (1 << id from ItemBagFamily.dbc) (所属特殊背包物品族掩码)
    uint32 TotemCategory;                                   // id from TotemCategory.dbc (图腾/附魔棒等工具类别)
    _Socket Socket[MAX_ITEM_PROTO_SOCKETS];                 // 最多 3 个宝石插槽定义
    uint32 socketBonus;                                     // id from SpellItemEnchantment.dbc (插槽颜色全匹配奖励附魔 ID)
    uint32 GemProperties;                                   // id from GemProperties.dbc (若为宝石则指向宝石属性)
    int32 RequiredDisenchantSkill;                          // 分解所需附魔技能等级（-1 表示不可分解）
    float  ArmorDamageModifier;                             // 护甲额外伤害修正
    uint32 ScriptId;                                        // 关联脚本 ID
    uint32 DisenchantID;                                    // 分解掉落表 ID
    uint32 FoodType;                                        // 猎人宠物喂食食物类型
    uint32 MinMoneyLoot;                                    // 开启可拾取金钱最小值（铜币）
    uint32 MaxMoneyLoot;                                    // 开启可拾取金钱最大值（铜币）
    uint32 Duration;                                        // 物品有效时长（秒）
    uint32 ExtraFlags;                                      // see ItemExtraFlags (服务端扩展标志)

    // helpers (辅助判定方法)
    // 判断战斗中是否允许更换该装备（圣物、盾牌、副手物品、武器和弹药允许战斗中切换）
    bool CanChangeEquipStateInCombat() const
    {
        switch (InventoryType)
        {
            case INVTYPE_RELIC:
            case INVTYPE_SHIELD:
            case INVTYPE_HOLDABLE:
                return true;
        }

        switch (Class)
        {
            case ITEM_CLASS_WEAPON:
            case ITEM_CLASS_PROJECTILE:
                return true;
        }

        return false;
    }

    // 判断该物品实例是否应记录制造者 GUID（不可堆叠、非消耗品/任务物品、非炉石且未禁用制造者记录）
    constexpr bool HasCreator() const
    {
        constexpr uint32 ITEM_HEARTHSTONE_ID = 6948;
        return Stackable <= 1 && Class != ITEM_CLASS_CONSUMABLE && Class != ITEM_CLASS_QUEST && !(Flags & ITEM_FLAG_NO_CREATOR) && ItemId != ITEM_HEARTHSTONE_ID;
    }

    uint32 GetMaxStackSize() const { return Stackable; }

    bool IsPotion() const { return Class == ITEM_CLASS_CONSUMABLE && SubClass == ITEM_SUBCLASS_POTION; }
    bool IsConjuredConsumable() const { return Class == ITEM_CLASS_CONSUMABLE && (Flags & ITEM_FLAG_CONJURED); }
    bool IsWeapon() const { return Class == ITEM_CLASS_WEAPON; }
    bool IsRangedWeapon() const { return IsWeapon() && (InventoryType == INVTYPE_RANGED || InventoryType == INVTYPE_THROWN || InventoryType == INVTYPE_RANGEDRIGHT); }
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

// 物品本地化名称与描述结构体
struct ItemLocale
{
    std::vector<std::string> Name;                          // 多语言物品名称列表
    std::vector<std::string> Description;                   // 多语言物品描述列表
};

#endif
