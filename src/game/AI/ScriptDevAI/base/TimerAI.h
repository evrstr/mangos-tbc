/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
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

#ifndef TIMER_AI_H
#define TIMER_AI_H

#include "Util/Util.h"
#include "Platform/Define.h"

#include <chrono>
#include <functional>
#include <map>
#include <vector>

using namespace std::chrono_literals;

class ChatHandler;

// 计时器有效战斗环境枚举
enum TimerCombat
{
    TIMER_COMBAT_OOC    = 0, // reset on evade (非战斗状态计时器，脱战闪避时重置)
    TIMER_COMBAT_COMBAT = 1, // reset on evade (战斗状态计时器，脱战闪避时重置)
    TIMER_ALWAYS        = 2, // reset on spawn (始终计时的计时器，仅在生成时重置)
};

/*
Timer data class used for execution of TimerAI events
*/
// 计时器数据结构：存储倒计时状态、初始区间与到期回调函数
struct Timer
{
    Timer(uint32 id, std::function<void()> functor, uint32 timerMin, uint32 timerMax, TimerCombat combatSetting, bool disabled = false);
    uint32 id;
    uint32 timer;
    bool disabled;
    std::function<void()> functor;

    // initial settings
    uint32 initialMin, initialMax;
    bool initialDisabled;
    TimerCombat combatSetting;

    bool UpdateTimer(const uint32 diff, bool combat); // 递减计时并在到期触发时返回 true
    void ResetTimer();                          // 重置计时器至初始随机区间
};

/*
Not an AI in itself
Used for adding unified timer support to any AI
*/
// 统一计时器管理器：为各种 AI 提供可扩展的定时动作与闭包回调支持
class TimerManager
{
    public:
        TimerManager() {}

        // TODO: remove first function
        void AddCustomAction(uint32 id, bool disabled, std::function<void()> functor, TimerCombat timerCombat = TIMER_ALWAYS);
        void AddCustomAction(uint32 id, uint32 timer, std::function<void()> functor, TimerCombat timerCombat = TIMER_ALWAYS);
        void AddCustomAction(uint32 id, std::chrono::milliseconds timer, std::function<void()> functor, TimerCombat timerCombat = TIMER_ALWAYS)
        {
            AddCustomAction(id, uint32(timer.count()), functor, timerCombat);
        }
        void AddCustomAction(uint32 id, uint32 timerMin, uint32 timerMax, std::function<void()> functor, TimerCombat timerCombat = TIMER_ALWAYS);
        void AddCustomAction(uint32 id, std::chrono::milliseconds timerMin, std::chrono::milliseconds timerMax, std::function<void()> functor, TimerCombat timerCombat = TIMER_ALWAYS)
        {
            AddCustomAction(id, timerMin.count(), timerMax.count(), functor, timerCombat);
        }

        virtual void ResetTimer(uint32 index, uint32 timer); // 重置并启动指定计时器
        virtual void ResetTimer(uint32 index, std::chrono::milliseconds timer)
        {
            ResetTimer(index, timer.count());
        }
        virtual void DisableTimer(uint32 index);          // 禁用指定计时器
        virtual void ReduceTimer(uint32 index, uint32 timer);  // 缩短计时器时间至指定上限
        virtual void ReduceTimer(uint32 index, std::chrono::milliseconds timer)
        {
            ReduceTimer(index, timer.count());
        }
        virtual void DelayTimer(uint32 index, uint32 timer);   // 延迟计时器触发时间
        virtual void DelayTimer(uint32 index, std::chrono::milliseconds timer)
        {
            DelayTimer(index, timer.count());
        }
        virtual void ResetIfNotStarted(uint32 index, uint32 timer); // 若计时器尚未启动则进行重置
        virtual void ResetIfNotStarted(uint32 index, std::chrono::milliseconds timer)
        {
            ResetIfNotStarted(index, timer.count());
        }


        virtual void UpdateTimers(const uint32 diff);    // 更新计时器（非战斗）
        virtual void UpdateTimers(const uint32 diff, bool combat); // 更新计时器（指定战斗状态）
        virtual void ResetAllTimers();                  // 重置所有计时器
        virtual void ResetTimersOnEvade();              // 脱战回位时重置计时器

        virtual void GetAIInformation(ChatHandler& reader); // 调试输出计时器信息

    protected:
        void AddTimer(uint32 id, Timer&& timer);
    private:
        std::map<uint32, Timer> m_timers; // yes, we are slicing here
};

// 战斗动作编排管理器：维护战斗技能/事件就绪标志与定时器执行
class CombatActions : public TimerManager
{
    public:
        CombatActions(uint32 maxCombatActions) : m_actionReadyStatus(maxCombatActions) {}

        // Family of methods which add combat action with reset settings saved
        // Adds a combat action which is always disabled at start
        void AddCombatAction(uint32 id, bool disabled);
        // Adds a combat action which is always reset to static timer value
        void AddCombatAction(uint32 id, uint32 timer);
        void AddCombatAction(uint32 id, std::chrono::milliseconds timer)
        {
            AddCombatAction(id, uint32(timer.count()));
        }
        // Adds a combat action which is reset to a random number between min and max (inclusive)
        void AddCombatAction(uint32 id, uint32 timerMin, uint32 timerMax);
        void AddCombatAction(uint32 id, std::chrono::milliseconds timerMin, std::chrono::milliseconds timerMax)
        {
            AddCombatAction(id, timerMin.count(), timerMax.count());
        }
        // Adds a combat action which has no timer. It is reset to default value at start. Useful for one-off actions like phase transition at HP level.
        void AddTimerlessCombatAction(uint32 id, bool byDefault);

        virtual void ResetTimer(uint32 index, uint32 timer) override;
        virtual void ResetTimer(uint32 index, std::chrono::milliseconds timer) override
        {
            ResetTimer(index, timer.count());
        }
        virtual void DisableTimer(uint32 index) override;
        virtual void ReduceTimer(uint32 index, uint32 timer) override;
        virtual void ReduceTimer(uint32 index, std::chrono::milliseconds timer) override
        {
            ReduceTimer(index, timer.count());
        }
        virtual void DelayTimer(uint32 index, uint32 timer) override;
        virtual void DelayTimer(uint32 index, std::chrono::milliseconds timer) override
        {
            DelayTimer(index, timer.count());
        }
        virtual void ResetIfNotStarted(uint32 index, uint32 timer) override;
        virtual void ResetIfNotStarted(uint32 index, std::chrono::milliseconds timer) override
        {
            ResetIfNotStarted(index, timer.count());
        }

        void DisableCombatAction(uint32 index);            // 禁用战斗动作并清除就绪标志

        void ResetCombatAction(uint32 index, uint32 timer)
        {
            ResetTimer(index, timer);
            SetActionReadyStatus(index, false);
        }
        void ResetCombatAction(uint32 index, std::chrono::milliseconds timer)
        {
            ResetCombatAction(index, timer.count());
        }

        void DelayCombatAction(uint32 index, uint32 timer)
        {
            if (GetActionReadyStatus(index))
            {
                SetActionReadyStatus(index, false);
                ResetTimer(index, timer);
            }
            else
                DelayTimer(index, timer);
        }
        void DelayCombatAction(uint32 index, std::chrono::milliseconds timer)
        {
            DelayCombatAction(index, timer.count());
        }

        inline void SetActionReadyStatus(uint32 index, bool state) { m_actionReadyStatus[index] = state; }
        inline bool GetActionReadyStatus(uint32 index) { return m_actionReadyStatus[index]; }

        virtual void UpdateTimers(const uint32 diff, bool combat) override;
        virtual void ExecuteActions() = 0;              // 纯虚函数：执行所有就绪的战斗动作
        virtual void ResetAllTimers() override;
        virtual void ResetTimersOnEvade() override;

        virtual void GetAIInformation(ChatHandler& reader) override;

        size_t GetCombatActionCount() { return m_actionReadyStatus.size(); }

    private:
        std::map<uint32, Timer> m_combatActions;
        std::vector<bool> m_actionReadyStatus;
        std::map<uint32, bool> m_timerlessActionSettings;
        std::map<uint32, uint32> m_spellAction;
};

#endif
