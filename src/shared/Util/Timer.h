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

#ifndef MANGOS_TIMER_H
#define MANGOS_TIMER_H

#include "Common.h"
#include <chrono>

/// 获取进程启动时的单调时钟基准时间点（首次调用时初始化）
inline std::chrono::steady_clock::time_point GetApplicationStartTime()
{
    using namespace std::chrono;

    static const steady_clock::time_point ApplicationStartTime = steady_clock::now();

    return ApplicationStartTime;
}

/// 世界服务器毫秒级全局计时器：维护服务器启动以来的单调毫秒时间与每帧 Tick 差值
class WorldTimer
{
    public:

        // get current server time | 获取当前服务器运行毫秒数
        static uint32 getMSTime();

        /// 计算旧毫秒时间戳与目标 steady_clock 时间点之间的差值（处理 32 位回绕）
        static inline uint32 getMSTimeDiff(const uint32& oldMSTime, std::chrono::steady_clock::time_point newTime)
        {
            using namespace std::chrono;

            uint32 newMSTime = uint32(duration_cast<milliseconds>(newTime - GetApplicationStartTime()).count());
            return getMSTimeDiff(oldMSTime, newMSTime);
        }

        // get time difference between two timestamps | 计算两个 32 位毫秒时间戳的差值（兼容 uint32 溢出回绕）
        static inline uint32 getMSTimeDiff(const uint32& oldMSTime, const uint32& newMSTime)
        {
            if (oldMSTime > newMSTime)
            {
                const uint32 diff_1 = (uint32(0xFFFFFFFF) - oldMSTime) + newMSTime;
                const uint32 diff_2 = oldMSTime - newMSTime;

                return std::min(diff_1, diff_2);
            }

            return newMSTime - oldMSTime;
        }

        // get last world tick time | 获取最新一次世界主循环 Tick 时间戳
        static uint32 tickTime();

        // get previous world tick time | 获取上一次世界主循环 Tick 时间戳
        static uint32 tickPrevTime();

        // tick world timer | 推进一次世界 Tick 并返回与上次 Tick 的毫秒差值
        static uint32 tick();

     private:
        static uint32 m_iTime;
        static uint32 m_iPrevTime;
};

/// 周期性间隔计时器（time_t 精度）：累计耗时达到 _interval 后触发，Reset 时扣除一个周期
class IntervalTimer
{
    public:
        IntervalTimer() : _interval(0), _current(0) {}

        void Update(time_t diff)
        {
            _current += diff;
            if (_current < 0)
                _current = 0;
        }
        bool Passed() const { return _current >= _interval; }
        void Reset()
        {
            if (_current >= _interval)
                _current -= _interval;
        }

        void SetCurrent(time_t current) { _current = current; }
        void SetInterval(time_t interval) { _interval = interval; }
        time_t GetInterval() const { return _interval; }
        time_t GetCurrent() const { return _current; }

    private:
        time_t _interval;
        time_t _current;
};

/// 短周期无符号毫秒间隔计时器（uint32 精度）
class ShortIntervalTimer
{
    public:
        ShortIntervalTimer() : _interval(0), _current(0) {}

        void Update(uint32 diff)
        {
            _current += diff;
        }

        bool Passed() const { return _current >= _interval; }
        void Reset()
        {
            if (_current >= _interval)
                _current -= _interval;
        }

        void SetCurrent(uint32 current) { _current = current; }
        void SetInterval(uint32 interval) { _interval = interval; }
        uint32 GetInterval() const { return _interval; }
        uint32 GetCurrent() const { return _current; }

    private:
        uint32 _interval;
        uint32 _current;
};

/// 倒计时追踪器（time_t 精度）：从 expiry 倒减至 <= 0 时视为到期
struct TimeTracker
{
    public:
        TimeTracker(time_t expiry) : i_expiryTime(expiry) {}
        void Update(time_t diff) { i_expiryTime -= diff; }
        bool Passed() const { return (i_expiryTime <= 0); }
        void Reset(time_t interval) { i_expiryTime = interval; }
        time_t GetExpiry() const { return i_expiryTime; }

    private:
        time_t i_expiryTime;
};

/// 短周期倒计时追踪器（int32 毫秒精度）
struct ShortTimeTracker
{
    public:
        ShortTimeTracker(uint32 expiry = 0) : i_expiryTime(expiry) {}
        void Update(uint32 diff)
        {
            i_expiryTime -= diff;
        }
        bool Passed() const { return (i_expiryTime <= 0); }
        void Reset(int32 interval) { i_expiryTime = interval; }
        int32 GetExpiry() const { return i_expiryTime; }

    private:
        int32 i_expiryTime;
};

#endif
