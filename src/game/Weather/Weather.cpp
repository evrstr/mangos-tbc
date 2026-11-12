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

/** \file
    \ingroup world
*/

#include "Weather/Weather.h"
#include "Entities/Player.h"
#include "Maps/Map.h"
#include "World/World.h"
#include "Server/WorldPacket.h"
#include "Log/Log.h"
#include "Util/Util.h"
#include "Util/ProgressBar.h"

/// Create the Weather object (创建区域天气对象并初始化变更计时器)
Weather::Weather(uint32 zone, WeatherZoneChances const* weatherChances) :
    m_zone(zone),
    m_type(WEATHER_TYPE_FINE),
    m_grade(0.0f),
    m_weatherChances(weatherChances),
    m_isPermanentWeather(false)
{
    m_timer.SetInterval(sWorld.getConfig(CONFIG_UINT32_INTERVAL_CHANGEWEATHER));
    DETAIL_FILTER_LOG(LOG_FILTER_WEATHER, "WORLD: Starting weather system for zone %u (change every %u minutes).", m_zone, (m_timer.GetInterval() / (MINUTE * IN_MILLISECONDS)));
}

/// Launch a weather update (推进天气计时器并在到期时尝试更新天气)
bool Weather::Update(uint32 diff, Map const* _map)
{
    m_timer.Update(diff);

    ///- If the timer has passed, ReGenerate the weather (若计时器到期，重新生成天气)
    if (m_timer.Passed())
    {
        m_timer.Reset();
        // update only if Regenerate has changed the weather (仅当重新生成改变了天气时才广播更新)
        if (ReGenerate())
        {
            ///- Weather will be removed if not updated (no players in zone anymore) (若区域内已无玩家，发送失败后移除该天气实例)
            if (!SendWeatherForPlayersInZone(_map))
                return false;
        }
    }
    return true;
}

/// Calculate the new weather, returns true if and only if the weather changed (计算新天气，仅在天气类型或强度变化时返回 true)
bool Weather::ReGenerate()
{
    if (m_isPermanentWeather)
        return false;

    // remember old values (记录旧的天气类型与强度用于对比)
    WeatherType old_type = m_type;
    float old_grade = m_grade;

    if (!m_weatherChances)
    {
        m_type = WEATHER_TYPE_FINE;
        m_grade = 0.0f;
        // No chanced calculation for this zone (该区域未配置天气概率，默认设为晴朗)
        return old_type != m_type || old_grade != m_grade;
    }

    /// Weather statistics: (天气变化概率分布：)
    ///- 30% - no change (30% - 保持不变)
    ///- 30% - weather gets better (if not fine) or change weather type (30% - 转好或切换天气类型)
    ///- 30% - weather worsens (if not fine) (30% - 恶化，若当前非晴天)
    ///- 10% - radical change (if not fine) (10% - 剧变，若当前非晴天)
    uint32 u = urand(0, 99);

    if (u < 30)
        return false;

    // 78 days between January 1st and March 20nd; 365/4=91 days by season (1月1日至3月20日约78天；每季约91天，据此换算当前游戏时间所属季节)
    // season source http://aa.usno.navy.mil/data/docs/EarthSeasons.html
    time_t gtime = sWorld.GetGameTime();
    struct tm* ltime = localtime(&gtime);
    uint32 season = ((ltime->tm_yday - 78 + 365) / 91) % 4;

    static char const* seasonName[WEATHER_SEASONS] = { "spring", "summer", "fall", "winter" };

    DEBUG_FILTER_LOG(LOG_FILTER_WEATHER, "Generating a change in %s weather for zone %u.", seasonName[season], m_zone);

    if ((u < 60) && (m_grade < 0.33333334f))                // Get fair (强度足够低时直接转晴)
    {
        m_type = WEATHER_TYPE_FINE;
        m_grade = 0.0f;
    }

    if ((u < 60) && (m_type != WEATHER_TYPE_FINE))          // Get better (天气转好，降低强度)
    {
        m_grade -= 0.33333334f;
        return true;
    }

    if ((u < 90) && (m_type != WEATHER_TYPE_FINE))          // Get worse (天气恶化，提高强度)
    {
        m_grade += 0.33333334f;
        return true;
    }

    if (m_type != WEATHER_TYPE_FINE)
    {
        /// Radical change: (剧烈变化规则：)
        ///- if light -> heavy (轻度 -> 重度)
        ///- if medium -> change weather type (中度 -> 切换天气类型)
        ///- if heavy -> 50% light, 50% change weather type (重度 -> 50% 转为轻度，50% 切换天气类型)

        if (m_grade < 0.33333334f)
        {
            m_grade = 0.9999f;                              // go nuts (直接升至最大强度)
            return true;
        }
        if (m_grade > 0.6666667f)
        {
            // Severe change, but how severe? (大幅转变：50% 概率降为轻度天气)
            uint32 rnd = urand(0, 99);
            if (rnd < 50)
            {
                m_grade -= 0.6666667f;
                return true;
            }
        }
        m_type = WEATHER_TYPE_FINE;                     // clear up (先清零为晴天，随后按季节概率重新抽取)
        m_grade = 0;
    }

    // At this point, only weather that isn't doing anything remains but that have weather data (此时仅剩配置了概率表的晴天状态，按季节概率抽取新天气)
    uint32 chance1 =          m_weatherChances->data[season].rainChance;
    uint32 chance2 = chance1 + m_weatherChances->data[season].snowChance;
    uint32 chance3 = chance2 + m_weatherChances->data[season].stormChance;

    uint32 rnd = urand(1, 100);
    if (rnd <= chance1)
        m_type = WEATHER_TYPE_RAIN;
    else if (rnd <= chance2)
        m_type = WEATHER_TYPE_SNOW;
    else if (rnd <= chance3)
        m_type = WEATHER_TYPE_STORM;
    else
        m_type = WEATHER_TYPE_FINE;

    /// New weather statistics (if not fine): (新天气初始强度分布：)
    ///- 85% light (85% 轻度)
    ///- 7% medium (7% 中度)
    ///- 7% heavy (7% 重度)
    /// If fine 100% sun (no fog) (若为晴天则强度为 0)

    if (m_type == WEATHER_TYPE_FINE)
    {
        m_grade = 0.0f;
    }
    else if (u < 90)
    {
        m_grade = rand_norm_f() * 0.3333f;
    }
    else
    {
        // Severe change, but how severe? (剧烈天气生成：随机分配为中度或重度)
        rnd = urand(0, 99);
        if (rnd < 50)
            m_grade = rand_norm_f() * 0.3333f + 0.3334f;
        else
            m_grade = rand_norm_f() * 0.3333f + 0.6667f;
    }

    NormalizeGrade();

    // return true only in case weather changes (仅当天气类型或强度发生变化时返回 true)
    return m_type != old_type || m_grade != old_grade;
}

// 向指定玩家发送当前区域天气数据包
void Weather::SendWeatherUpdateToPlayer(Player* player)
{
    NormalizeGrade();

    WorldPacket data(SMSG_WEATHER, 4 + 4 + 1);
    data << uint32(GetWeatherState());
    data << float(m_grade);
    data << uint8(0);       // 1 = instant change, 0 = smooth change (1 = 瞬时切换，0 = 平滑过渡)

    player->GetSession()->SendPacket(data);
}

// Send the new weather to all players in the zone (向区域内的所有玩家广播新天气)
bool Weather::SendWeatherForPlayersInZone(Map const* _map)
{
    NormalizeGrade();

    WeatherState state = GetWeatherState();

    WorldPacket data(SMSG_WEATHER, 4 + 4 + 1);
    data << uint32(state);
    data << float(m_grade);
    data << uint8(0);       // 1 = instant change, 0 = smooth change (1 = 瞬时切换，0 = 平滑过渡)

    ///- Send the weather packet to all players in this zone (向区域内所有玩家发送天气数据包)
    if (!_map->SendToPlayersInZone(data, m_zone))
        return false;

    ///- Log the event (记录天气变更日志)
    LogWeatherState(state);
    return true;
}

// Set the weather (手动设置天气类型、强度与永久标志并立即广播)
void Weather::SetWeather(WeatherType type, float grade, Map const* _map, bool isPermanent)
{
    m_isPermanentWeather = isPermanent;

    if (m_type == type && m_grade == grade)
        return;

    m_type = type;
    m_grade = grade;
    SendWeatherForPlayersInZone(_map);
}

// Get the sound number associated with the current weather (根据天气类型与强度映射为客户端 WeatherState 枚举)
WeatherState Weather::GetWeatherState() const
{
    if (m_grade < 0.27f)
        return WEATHER_STATE_FINE;

    switch (m_type)
    {
        case WEATHER_TYPE_RAIN:
        {
            if (m_grade < 0.40f)
                return WEATHER_STATE_LIGHT_RAIN;
            if (m_grade < 0.70f)
                return WEATHER_STATE_MEDIUM_RAIN;
            return WEATHER_STATE_HEAVY_RAIN;
        }
        case WEATHER_TYPE_SNOW:
        {
            if (m_grade < 0.40f)
                return WEATHER_STATE_LIGHT_SNOW;
            if (m_grade < 0.70f)
                return WEATHER_STATE_MEDIUM_SNOW;
            return WEATHER_STATE_HEAVY_SNOW;
        }
        case WEATHER_TYPE_STORM:
        {
            if (m_grade < 0.40f)
                return WEATHER_STATE_LIGHT_SANDSTORM;
            if (m_grade < 0.70f)
                return WEATHER_STATE_MEDIUM_SANDSTORM;
            return WEATHER_STATE_HEAVY_SANDSTORM;
        }
        case WEATHER_TYPE_BLACKRAIN:
            return WEATHER_STATE_BLACKRAIN;
        case WEATHER_TYPE_THUNDERS:
            return WEATHER_STATE_THUNDERS;
        case WEATHER_TYPE_FINE:
        default:
            return WEATHER_STATE_FINE;
    }
}

// 归一化天气强度至 [0.0001, 0.9999]
void Weather::NormalizeGrade()
{
    if (m_grade >= 1)
        m_grade = 0.9999f;
    else if (m_grade < 0)
        m_grade = 0.0001f;
}

// Helper to log recent state (记录当前天气状态日志)
void Weather::LogWeatherState(WeatherState state) const
{
    char const* wthstr;
    switch (state)
    {
        case WEATHER_STATE_LIGHT_RAIN:
            wthstr = "light rain";
            break;
        case WEATHER_STATE_MEDIUM_RAIN:
            wthstr = "medium rain";
            break;
        case WEATHER_STATE_HEAVY_RAIN:
            wthstr = "heavy rain";
            break;
        case WEATHER_STATE_LIGHT_SNOW:
            wthstr = "light snow";
            break;
        case WEATHER_STATE_MEDIUM_SNOW:
            wthstr = "medium snow";
            break;
        case WEATHER_STATE_HEAVY_SNOW:
            wthstr = "heavy snow";
            break;
        case WEATHER_STATE_LIGHT_SANDSTORM:
            wthstr = "light sandstorm";
            break;
        case WEATHER_STATE_MEDIUM_SANDSTORM:
            wthstr = "medium sandstorm";
            break;
        case WEATHER_STATE_HEAVY_SANDSTORM:
            wthstr = "heavy sandstorm";
            break;
        case WEATHER_STATE_THUNDERS:
            wthstr = "thunders";
            break;
        case WEATHER_STATE_BLACKRAIN:
            wthstr = "black rain";
            break;
        case WEATHER_STATE_FINE:
        default:
            wthstr = "fine";
            break;
    }

    DETAIL_FILTER_LOG(LOG_FILTER_WEATHER, "Change the weather of zone %u (type %u, grade %f) to state %s.", m_zone, m_type, m_grade, wthstr);
}

// ---------------------------------------------------------
//                  Weather System (地图天气系统)
// ---------------------------------------------------------

WeatherSystem::WeatherSystem(Map const* _map) : m_map(_map)
{}

WeatherSystem::~WeatherSystem()
{
    ///- Empty the WeatherMap (释放地图内所有区域天气实例)
    for (WeatherMap::const_iterator itr = m_weathers.begin(); itr != m_weathers.end(); ++itr)
        delete itr->second;

    m_weathers.clear();
}

/// Find or Create a Weather object by the given zoneid (按区域 ID 查找或新建天气对象)
Weather* WeatherSystem::FindOrCreateWeather(uint32 zoneId)
{
    WeatherMap::const_iterator itr = m_weathers.find(zoneId);
    // Return if found (已存在则直接返回)
    if (itr != m_weathers.end())
        return itr->second;
    // Create (不存在则创建并注册到映射表)
    Weather* w = new Weather(zoneId, sWeatherMgr.GetWeatherChances(zoneId));
    m_weathers[zoneId] = w;
    return w;
}

/// Update Weathers for the different zones (推进地图内所有区域的天气更新)
void WeatherSystem::UpdateWeathers(uint32 diff)
{
    ///- Send an update signal to Weather objects (向各个区域天气对象发送时间步进信号)
    for (WeatherMap::iterator itr = m_weathers.begin(); itr != m_weathers.end();)
    {
        ///- and remove Weather objects for zones with no player (若区域内已无玩家，移除对应天气对象)
        // As interval > WorldTick (因天气更新间隔远大于世界 Tick，可安全清理空区域)
        if (!itr->second->Update(diff, m_map))
        {
            delete itr->second;
            m_weathers.erase(itr++);
        }
        else
            ++itr;
    }
}

/// Load Weather chanced from table game_weather (从数据库 game_weather 表加载各区域四季天气概率)
void WeatherMgr::LoadWeatherZoneChances()
{
    uint32 count = 0;

    //                                             0     1                   2                   3                    4                   5                   6                    7                 8                 9                  10                  11                  12
    auto queryResult = WorldDatabase.Query("SELECT zone, spring_rain_chance, spring_snow_chance, spring_storm_chance, summer_rain_chance, summer_snow_chance, summer_storm_chance, fall_rain_chance, fall_snow_chance, fall_storm_chance, winter_rain_chance, winter_snow_chance, winter_storm_chance FROM game_weather");

    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 weather definitions. DB table `game_weather` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        Field* fields = queryResult->Fetch();
        bar.step();

        uint32 zone_id = fields[0].GetUInt32();

        WeatherZoneChances& wzc = mWeatherZoneMap[zone_id];

        for (int season = 0; season < WEATHER_SEASONS; ++season)
        {
            wzc.data[season].rainChance  = fields[season * (MAX_WEATHER_TYPE - 1) + 1].GetUInt32();
            wzc.data[season].snowChance  = fields[season * (MAX_WEATHER_TYPE - 1) + 2].GetUInt32();
            wzc.data[season].stormChance = fields[season * (MAX_WEATHER_TYPE - 1) + 3].GetUInt32();

            if (wzc.data[season].rainChance > 100)
            {
                wzc.data[season].rainChance = 25;
                sLog.outErrorDb("Weather for zone %u season %u has wrong rain chance > 100%%", zone_id, season);
            }

            if (wzc.data[season].snowChance > 100)
            {
                wzc.data[season].snowChance = 25;
                sLog.outErrorDb("Weather for zone %u season %u has wrong snow chance > 100%%", zone_id, season);
            }

            if (wzc.data[season].stormChance > 100)
            {
                wzc.data[season].stormChance = 25;
                sLog.outErrorDb("Weather for zone %u season %u has wrong storm chance > 100%%", zone_id, season);
            }
        }

        ++count;
    }
    while (queryResult->NextRow());

    sLog.outString(">> Loaded %u weather definitions", count);
    sLog.outString();
}
