/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_WORLD_MAP_SCRIPTS_H
#define DEF_WORLD_MAP_SCRIPTS_H

enum
{
    // Transports (世界交通工具 NPC)
    NPC_NEZRAZ                  = 3149,                     // 内兹拉兹 (格罗姆高飞艇主管)
    NPC_HINDENBURG              = 3150,                     // 兴登堡 (幽暗城飞艇主管)
    NPC_FREZZA                  = 9564,                     // 弗雷扎 (奥格瑞玛飞艇主管)
    NPC_ZAPETTA                 = 9566,                     // 扎佩塔 (幽暗城飞艇主管)
    NPC_SNURK_BUCKSQUICK        = 12136,                    // 斯诺克·巴克斯威克
    NPC_SQUIBBY_OVERSPECK       = 12137,                    // 斯奎比·奥弗斯佩克

    // Elemental invasions (元素入侵世界首领与裂隙 GO)
    NPC_THE_WINDREAVER          = 14454,                    // 烈风掠夺者 (希利苏斯)
    NPC_PRINCESS_TEMPESTRIA     = 14457,                    // 泰比斯蒂亚公主 (冬泉谷)
    NPC_BARON_CHARR             = 14461,                    // 查尔男爵 (安戈洛环形山)
    NPC_AVALANCHION             = 14464,                    // 阿瓦兰奇奥 (艾萨拉)

    GO_EARTH_RIFT               = 179664,                   // 大地裂隙
    GO_WATER_RIFT               = 179665,                   // 水流裂隙
    GO_FIRE_RIFT                = 179666,                   // 火焰裂隙
    GO_AIR_RIFT                 = 179667,                   // 空气裂隙

    // Quest 1126
    NPC_HIVE_ASHI_DRONES        = 13136,                    // 亚什工蜂

    NPC_HIGHLORD_KRUUL          = 18338,                    // 大领主库鲁尔

    // Shade of the Horseman (无头骑士之影活动 NPC)
    NPC_COSTUMED_ORPHAN_MATRON  = 24519,                    // 盛装孤儿监护院院长
    NPC_TOURING_ORPHAN          = 23712,                    // 观光孤儿
    NPC_MASKED_ORPHAN_MATRON    = 23973,                    // 假面孤儿监护院院长
    NPC_TRAVELING_ORPHAN        = 23971,                    // 旅游孤儿
};

// 元素入侵类型索引枚举
enum ElementalInvasionIndexes
{
    ELEMENTAL_EARTH             = 0,                        // 地元素
    ELEMENTAL_AIR               = 1,                        // 气元素
    ELEMENTAL_FIRE              = 2,                        // 火元素
    ELEMENTAL_WATER             = 3,                        // 水元素
    MAX_ELEMENTS
};

// Shade of the Horseman village attack event (无头骑士之影村庄袭击阶段枚举)
enum ShadeOfTheHorsemanPhase
{
    SHADE_PHASE_ALL_CLEAR = 0,                              // 平安状态
    SHADE_PHASE_SPAWNED   = 1,                              // 骑士刷新盘旋
    SHADE_PHASE_LANDED    = 2,                              // 骑士降落作战
    SHADE_PHASE_VICTORY   = 3,                              // 村庄扑灭火焰获胜
};

// 无头骑士之影受袭村庄枚举
enum ShadeOfTheHorsemanVillages
{
    SHADE_VILLAGE_GOLDSHIRE,                                // 闪金镇 (艾尔文森林)
    SHADE_VILLAGE_KHARANOS,                                 // 卡拉诺斯 (丹莫罗)
    SHADE_VILLAGE_AZURE_WATCH,                              // 碧蓝岗哨 (秘蓝岛)
    SHADE_VILLAGE_BRILL,                                    // 布瑞尔 (提瑞斯法林地)
    SHADE_VILLAGE_RAZOR_HILL,                               // 剃刀岭 (杜隆塔尔)
    SHADE_VILLAGE_FALCONWING_SQUARE,                        // 鹰翼广场 (永歌森林)
    MAX_VILLAGES
};

enum
{
    TYPE_SHADE_OF_THE_HORSEMAN_ATTACK_PHASE = 1000, // High number against collisions (袭击阶段数据起始 ID)
    TYPE_SHADE_OF_THE_HORSEMAN_MAX = TYPE_SHADE_OF_THE_HORSEMAN_ATTACK_PHASE + MAX_VILLAGES,
};

// 无头骑士之影世界事件数据管理结构体
struct ShadeOfTheHorsemanData
{
    ShadeOfTheHorsemanData(std::vector<uint32> areaIds) : m_areaIds(areaIds) {}
    uint32 m_shadeOfTheHorsemanAttackPhases[MAX_VILLAGES];  // 各村庄攻击阶段状态数组

    void Reset();                                           // 重置所有村庄状态

    uint32 GetDataAttackPhase(uint32 id) const;             // 获取指定村庄袭击阶段
    void SetDataAttackPhase(uint32 id, uint32 value);       // 设置指定村庄袭击阶段

    bool IsConditionFulfilled(uint32 conditionId, uint32 areaId) const; // 区域条件判定

    uint32 HandleGetData(uint32 id) const;                  // 副本实例数据读取接口
    void HandleSetData(uint32 id, uint32 value);            // 副本实例数据写入接口

    static uint32 GetTypeFromZoneId(uint32 zoneId);         // 根据区域 ID 映射村庄索引

    // TBC only timer section (TBC 计时器逻辑)
    bool Update(const uint32 diff); // returns true when event should be started (每帧更新，返回 true 表示应触发事件)

    uint32 CalculateWorldstateTimerValue() const;           // 计算世界状态倒计时分秒

    std::tm m_horsemanTime;                                 // 上次更新时间戳
    uint32 m_horsemanTimer;                                 // 事件检测倒计时

    std::vector<uint32> m_areaIds;                          // 涉及的区域 ID 列表
};

#endif
