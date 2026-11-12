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

/// \addtogroup world
/// @{
/// \file

#ifndef __WEATHER_H
#define __WEATHER_H

#include "Common.h"
#include "Globals/SharedDefines.h"
#include "Util/Timer.h"

class Player;
class Map;

// ---------------------------------------------------------
//            Actual Weather in one zone (单个区域的实时天气)
// ---------------------------------------------------------

// 天气状态枚举（客户端 SMSG_WEATHER 协议使用的状态值）
enum WeatherState
{
    WEATHER_STATE_FINE              = 0,  // 晴朗
    WEATHER_STATE_LIGHT_RAIN        = 3,  // 小雨
    WEATHER_STATE_MEDIUM_RAIN       = 4,  // 中雨
    WEATHER_STATE_HEAVY_RAIN        = 5,  // 大雨
    WEATHER_STATE_LIGHT_SNOW        = 6,  // 小雪
    WEATHER_STATE_MEDIUM_SNOW       = 7,  // 中雪
    WEATHER_STATE_HEAVY_SNOW        = 8,  // 大雪
    WEATHER_STATE_LIGHT_SANDSTORM   = 22, // 轻度沙尘暴
    WEATHER_STATE_MEDIUM_SANDSTORM  = 41, // 中度沙尘暴
    WEATHER_STATE_HEAVY_SANDSTORM   = 42, // 重度沙尘暴
    WEATHER_STATE_THUNDERS          = 86, // 雷暴
    WEATHER_STATE_BLACKRAIN         = 90  // 黑雨
};

struct WeatherZoneChances;

/// Weather for one zone (单个区域的天气实例，负责天气状态生成、更新与向区域玩家广播)
class Weather
{
    public:
        Weather(uint32 zone, WeatherZoneChances const* weatherChances);
        ~Weather() {};

        /// Send Weather to one player (向单个玩家发送当前天气状态数据包)
        void SendWeatherUpdateToPlayer(Player* player);
        /// Set the weather (强制设置天气类型与强度，并可标记为永久天气)
        void SetWeather(WeatherType type, float grade, Map const* _map, bool isPermanent);
        /// Update the weather in this zone, when the timer is expired the weather will be rolled again (更新区域天气计时器，到期后重新随机生成天气)
        bool Update(uint32 diff, Map const* _map);
        /// Check if a type is valid (校验天气类型枚举是否有效)
        static bool IsValidWeatherType(uint32 type)
        {
            switch (type)
            {
                case WEATHER_TYPE_FINE:
                case WEATHER_TYPE_RAIN:
                case WEATHER_TYPE_SNOW:
                case WEATHER_TYPE_STORM:
                case WEATHER_TYPE_THUNDERS:
                case WEATHER_TYPE_BLACKRAIN:
                    return true;
                default:
                    return false;
            }
        }

    private:
        /// Send SMSG_WEATHER to all players in the zone (向该区域所有玩家广播 SMSG_WEATHER 数据包)
        bool SendWeatherForPlayersInZone(Map const* _map);
        /// Calculate new weather (根据季节概率计算新天气，返回天气是否发生变化)
        bool ReGenerate();
        /// Calculate state based on type and grade (根据天气类型和强度计算对应的 WeatherState)
        WeatherState GetWeatherState() const;
        // Helper to get the grade between 0..1 (将天气强度限制在 [0, 1) 范围内)
        void NormalizeGrade();
        // Helper to log recent state (记录当前天气状态变更日志)
        void LogWeatherState(WeatherState state) const;

        uint32 m_zone;                              // 所属区域 ID
        WeatherType m_type;                         // 当前天气类型
        float m_grade;                              // 当前天气强度（0.0 ~ 0.9999）
        ShortIntervalTimer m_timer;                 // 天气变更周期计时器
        WeatherZoneChances const* m_weatherChances; // 区域四季天气概率配置指针
        bool m_isPermanentWeather;                  // 是否为 GM 设置的永久天气
};

// ---------------------------------------------------------
//         Weather information hold on one map (单张地图持有的天气系统)
// ---------------------------------------------------------

/// Weathers for one map (管理单张地图上各个区域的 Weather 实例)
class WeatherSystem
{
    public:
        WeatherSystem(Map const* _map);
        ~WeatherSystem();

        Weather* FindOrCreateWeather(uint32 zoneId); // 查找或按区域 ID 创建天气对象
        void UpdateWeathers(uint32 diff);            // 更新地图内所有活跃区域的天气

    private:
        Map const* const m_map;

        typedef std::unordered_map<uint32 /*zoneId*/, Weather*> WeatherMap;
        WeatherMap m_weathers;
};

// ---------------------------------------------------------
//              Global Weather Information (全局天气配置信息)
// ---------------------------------------------------------

#define WEATHER_SEASONS 4
// 单季度的各类天气触发概率
struct WeatherSeasonChances
{
    uint32 rainChance;  // 下雨概率（0~100）
    uint32 snowChance;  // 下雪概率（0~100）
    uint32 stormChance; // 沙暴概率（0~100）
};

// 单个区域的四季天气概率表
struct WeatherZoneChances
{
    WeatherSeasonChances data[WEATHER_SEASONS];
};

// 全局天气管理器，负责从数据库加载各区域的天气概率配置
class WeatherMgr
{
    public:
        WeatherMgr() {};
        ~WeatherMgr() {};

        void LoadWeatherZoneChances(); // 从 game_weather 表加载所有区域的天气概率

        // 获取指定区域的天气概率配置，未配置则返回 nullptr
        WeatherZoneChances const* GetWeatherChances(uint32 zone_id) const
        {
            WeatherZoneMap::const_iterator itr = mWeatherZoneMap.find(zone_id);
            if (itr != mWeatherZoneMap.end())
                return &itr->second;
            return nullptr;
        }

    private:
        typedef std::unordered_map<uint32 /*zoneId*/, WeatherZoneChances> WeatherZoneMap;
        WeatherZoneMap      mWeatherZoneMap;
};

#define sWeatherMgr MaNGOS::Singleton<WeatherMgr>::Instance()

#endif
