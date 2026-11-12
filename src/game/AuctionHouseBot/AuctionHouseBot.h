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

#ifndef AUCTION_HOUSE_BOT_H
#define AUCTION_HOUSE_BOT_H

#include "AuctionHouse/AuctionHouseMgr.h"
#include "Config/Config.h"
#include "Entities/ItemPrototype.h"
#include "Globals/SharedDefines.h"
#include "Loot/LootMgr.h"
#include "Util/Util.h"

/// 单个物品在拍卖行机器人中的自定义配置数据（覆盖默认值）
struct AuctionHouseBotItemData
{
    uint32 Value = 0;                                       // 单价（0 表示黑名单/禁用）
    uint32 AddChance = 0;                                   // 每次刷新额外上架概率（百分比）
    uint32 MinAmount = 0;                                   // 最小上架数量
    uint32 MaxAmount = 0;                                   // 最大上架数量
};

/// 单个拍卖行内机器人挂单的统计信息（总数与按品质分布）
struct AuctionHouseBotStatusInfoPerType
{
    uint32 ItemsCount;                                      // 机器人拍卖条目总数
    uint32 QualityInfo[MAX_ITEM_QUALITY];                   // 各品质物品数量统计
};

typedef AuctionHouseBotStatusInfoPerType AuctionHouseBotStatusInfo[MAX_AUCTION_HOUSE_TYPE];

/// 拍卖行机器人，按配置自动向拍卖行投放物品并参与竞拍/买断
class AuctionHouseBot
{
    public:
        AuctionHouseBot();
        ~AuctionHouseBot();

        /// 读取配置与数据库并初始化拍卖行机器人
        void Initialize();
        void SetConfigFileName(const std::string& filename) { m_configFileName = filename; }
        /// 周期更新：轮询联盟/部落/中立拍卖行执行上架或购买动作
        void Update();

        // Following methods are mainly used by level3.cpp for ingame/console commands (以下方法主要用于 GM/控制台命令)
        /// 重新加载全部 AHBot 配置
        bool ReloadAllConfig();
        /// 重建机器人拍卖物品（all=true 时连同已有竞价的条目一起过期重刷）
        void Rebuild(bool all);
        /// 统计各拍卖行中机器人当前的挂单数量与品质分布
        void PrepareStatusInfos(AuctionHouseBotStatusInfo& statusInfo) const;
        /// 设置或重置指定物品的自定义价格/数量配置并持久化到数据库
        void SetItemData(uint32 item, AuctionHouseBotItemData& itemData, bool reset = false);
        /// 查询指定物品的 AHBot 配置或默认估算售价
        AuctionHouseBotItemData GetItemData(uint32 item);

    private:
        uint32 GetMinMaxConfig(const char* config, uint32 minValue, uint32 maxValue, uint32 defaultValue);
        void ParseLootConfig(char const* fieldname, std::vector<int32>& lootConfig);
        void FillUintVectorFromQuery(char const* query, std::vector<uint32>& lootTemplates);
        void ParseLevelConstraints();                       // 解析等级与装等上限配置
        void UpdateDynamicMaxLevel();                       // 根据当前在线玩家动态刷新最大需求等级
        void CalculateItemLevelCap();                       // 计算允许上架的物品装等上限
        void ParseItemValueConfig(char const* fieldname, std::vector<uint32>& itemValues);
        void AddLootToItemMap(LootStore* store, std::vector<int32>& lootConfig, std::vector<uint32>& lootTemplates, std::unordered_map<uint32, uint32>& itemMap);
        uint32 CalculateBuyoutPrice(ItemPrototype const* prototype);
        /// 按配置的波动百分比对物品基准价格施加随机浮动
        uint32 ValueWithVariance(uint32 itemValue) { return (uint32) (itemValue + ((int32) urand(0, m_valueVariance * 2 + 1) - (int32) m_valueVariance) * (int32) (itemValue / 100)); };

        std::string m_configFileName;
        Config m_ahBotCfg;

        uint32 m_houseAction;                               // 当前轮询动作索引（0..2 卖，3..5 买）

        uint32 m_chanceSell;                                // 上架触发概率（0-100）
        uint32 m_chanceBuy;                                 // 买断/竞价触发概率（0-100）

        std::vector<int32> m_creatureLootNormalConfig;
        std::vector<int32> m_creatureLootRareConfig;
        std::vector<int32> m_creatureLootEliteConfig;
        std::vector<int32> m_creatureLootRareEliteConfig;
        std::vector<int32> m_creatureLootWorldBossConfig;
        std::vector<int32> m_disenchantLootConfig;
        std::vector<int32> m_fishingLootConfig;
        std::vector<int32> m_gameobjectLootConfig;
        std::vector<int32> m_skinningLootConfig;
        std::vector<int32> m_professionItemsConfig;

        bool m_useDynamicMaxLevel;                          // 是否启用动态最高玩家等级限制
        bool m_ignoreGm;                                    // 计算动态最高等级时是否忽略 GM 账号
        uint32 m_lastLevelUpdateTime = 0;                   // 上次刷新动态等级的时间戳
        uint32 m_levelRefreshInterval = 0;                  // 动态等级刷新间隔（秒）
        uint32 m_maxRequiredLevel;                          // 允许上架的最大物品需求等级
        uint32 m_maxItemLevel;                              // 允许上架的最大物品装等

        std::vector<std::vector<uint32>> m_itemValue = std::vector<std::vector<uint32>>(MAX_ITEM_QUALITY, std::vector<uint32>(MAX_ITEM_CLASS));
        bool m_vendorValue;
        uint32 m_valueVariance;
        uint32 m_auctionBidMin;
        uint32 m_auctionBidMax;
        uint32 m_auctionTimeMin;
        uint32 m_auctionTimeMax;
        uint32 m_buyValue;

        std::vector<uint32> m_creatureLootNormalTemplates;
        std::vector<uint32> m_creatureLootRareTemplates;
        std::vector<uint32> m_creatureLootEliteTemplates;
        std::vector<uint32> m_creatureLootRareEliteTemplates;
        std::vector<uint32> m_creatureLootWorldBossTemplates;
        std::vector<uint32> m_disenchantLootTemplates;
        std::vector<uint32> m_fishingLootTemplates;
        std::vector<uint32> m_gameobjectLootTemplates;
        std::vector<uint32> m_skinningLootTemplates;
        std::vector<uint32> m_professionItems;

        std::unordered_set<uint32> m_vendorItems;           // 商人出售的物品集合（防止低买高卖）

        std::unordered_map<uint32, AuctionHouseBotItemData> m_itemData; // 数据库自定义物品覆盖表
};

#define sAuctionHouseBot MaNGOS::Singleton<AuctionHouseBot>::Instance()

#endif
