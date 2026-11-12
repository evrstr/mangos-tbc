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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef WORLD_PVP_MGR_H
#define WORLD_PVP_MGR_H

#include "Common.h"
#include "Util/Timer.h"

enum
{
    TIMER_OPVP_MGR_UPDATE           = MINUTE * IN_MILLISECONDS // 1 minute is enough for us but this might change with wintergrasp support (1 分钟更新间隔，后续若有冬拥湖等机制可扩展)
};

// 野外 PvP 脚本类型索引
enum OutdoorPvPTypes
{
    OPVP_ID_SI = 0,                                         // 希利苏斯（Silithus）
    OPVP_ID_EP,                                             // 东瘟疫之地（Eastern Plaguelands）
    OPVP_ID_HP,                                             // 地狱火半岛（Hellfire Peninsula）
    OPVP_ID_ZM,                                             // 赞加沼泽（Zangarmarsh）
    OPVP_ID_TF,                                             // 泰罗卡森林（Terokkar Forest）
    OPVP_ID_NA,                                             // 纳格兰哈兰（Nagrand / Halaa）

    MAX_OPVP_ID
};

// 受野外 PvP 增益/状态影响的区域与关联副本 Zone ID
enum OutdoorPvPZones
{
    ZONE_ID_SILITHUS                = 1377,
    ZONE_ID_TEMPLE_OF_AQ            = 3428,
    ZONE_ID_RUINS_OF_AQ             = 3429,
    ZONE_ID_GATES_OF_AQ             = 3478,

    ZONE_ID_EASTERN_PLAGUELANDS     = 139,
    ZONE_ID_STRATHOLME              = 2017,
    ZONE_ID_SCHOLOMANCE             = 2057,

    ZONE_ID_HELLFIRE_PENINSULA      = 3483,
    ZONE_ID_HELLFIRE_RAMPARTS       = 3562,
    ZONE_ID_HELLFIRE_CITADEL        = 3563,
    ZONE_ID_BLOOD_FURNACE           = 3713,
    ZONE_ID_SHATTERED_HALLS         = 3714,
    ZONE_ID_MAGTHERIDON_LAIR        = 3836,

    ZONE_ID_ZANGARMARSH             = 3521,
    ZONE_ID_SERPENTSHRINE_CAVERN    = 3607,
    ZONE_ID_STEAMVAULT              = 3715,
    ZONE_ID_UNDERBOG                = 3716,
    ZONE_ID_SLAVE_PENS              = 3717,

    ZONE_ID_TEROKKAR_FOREST         = 3519,
    ZONE_ID_SHADOW_LABYRINTH        = 3789,
    ZONE_ID_AUCHENAI_CRYPTS         = 3790,
    ZONE_ID_SETHEKK_HALLS           = 3791,
    ZONE_ID_MANA_TOMBS              = 3792,

    ZONE_ID_NAGRAND                 = 3518
};

// 占领点进度滑块状态（进度值与锁定状态）
struct CapturePointSlider
{
    CapturePointSlider() : Value(0.0f), IsLocked(false) {}
    CapturePointSlider(float value, bool isLocked) : Value(value), IsLocked(isLocked) {}

    float Value;
    bool IsLocked;
};

class Player;
class GameObject;
class Creature;
class Object;
class OutdoorPvP;

typedef std::map<uint32 /*capture point entry*/, CapturePointSlider /*slider value and lock state*/> CapturePointSliderMap;

// 野外 PvP 全局管理器单例：负责初始化各战区脚本、分发玩家进出区域事件及定时驱动更新
class OutdoorPvPMgr
{
    public:
        OutdoorPvPMgr();
        ~OutdoorPvPMgr();

        // load all outdoor pvp scripts (初始化并加载所有野外 PvP 战区脚本)
        void InitOutdoorPvP();

        // called when a player enters an outdoor pvp area (玩家进入野外 PvP 区域或关联副本时调用)
        void HandlePlayerEnterZone(Player* player, uint32 zoneId);

        // called when player leaves an outdoor pvp area (玩家离开野外 PvP 区域或关联副本时调用)
        void HandlePlayerLeaveZone(Player* player, uint32 zoneId);

        // return assigned outdoor pvp script (根据主区域 ID 获取对应的野外 PvP 脚本)
        OutdoorPvP* GetScript(uint32 zoneId);

        // 定时驱动所有野外 PvP 脚本更新
        void Update(uint32 diff);

        // Save and load capture point slider (保存与读取占领点滑块进度映射)
        CapturePointSliderMap const* GetCapturePointSliderMap() const { return &m_capturePointSlider; }
        void SetCapturePointSlider(uint32 entry, CapturePointSlider value) { m_capturePointSlider[entry] = value; }

    private:
        // return assigned outdoor pvp script (根据主区域或受影响副本的 Zone ID 查找野外 PvP 脚本)
        OutdoorPvP* GetScriptOfAffectedZone(uint32 zoneId);

        // contains all outdoor pvp scripts (存放所有野外 PvP 战区脚本实例)
        OutdoorPvP* m_scripts[MAX_OPVP_ID];

        CapturePointSliderMap m_capturePointSlider;

        // update interval (更新定时器)
        ShortIntervalTimer m_updateTimer;
};

#define sOutdoorPvPMgr MaNGOS::Singleton<OutdoorPvPMgr>::Instance()

#endif
