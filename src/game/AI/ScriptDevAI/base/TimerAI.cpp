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

#include "TimerAI.h"
#include "Chat/Chat.h"
#include "Log/Log.h"
#include <string>

Timer::Timer(uint32 id, std::function<void()> functor, uint32 timerMin, uint32 timerMax, TimerCombat combatSetting, bool disabled)
    : id(id), timer(urand(timerMin, timerMax)), disabled(disabled), functor(functor), initialMin(timerMin), initialMax(timerMax), initialDisabled(disabled), combatSetting(combatSetting)
    {}

// 递减计时器并判定是否触发到期回调
bool Timer::UpdateTimer(const uint32 diff, bool combat)
{
    if (disabled)
        return false;

    if (combatSetting != TIMER_ALWAYS && bool(combatSetting) != combat)
        return false;

    if (timer <= diff)
    {
        timer = 0;
        disabled = true;
        return true;
    }
    else timer -= diff;

    return false;
}

// 重置计时器至初始随机区间
void Timer::ResetTimer()
{
    timer = urand(initialMin, initialMax);
    disabled = initialDisabled;
}

// 添加计时器实例
void TimerManager::AddTimer(uint32 id, Timer&& timer)
{
    m_timers.emplace(id, timer);
}

void TimerManager::AddCustomAction(uint32 id, bool disabled, std::function<void()> functor, TimerCombat timerCombat)
{
    m_timers.emplace(id, Timer(id, functor, 0, 0, timerCombat, disabled));  
}

void TimerManager::AddCustomAction(uint32 id, uint32 timer, std::function<void()> functor, TimerCombat timerCombat)
{
    m_timers.emplace(id, Timer(id, functor, timer, timer, timerCombat, false));
}

void TimerManager::AddCustomAction(uint32 id, uint32 timerMin, uint32 timerMax, std::function<void()> functor, TimerCombat timerCombat)
{
    m_timers.emplace(id, Timer(id, functor, timerMin, timerMax, timerCombat, false));
}

// 重置指定索引的计时器时间并取消禁用
void TimerManager::ResetTimer(uint32 index, uint32 timer)
{
    auto data = m_timers.find(index);
    if (data == m_timers.end())
    {
        sLog.outError("Timer index %u does not exist.", index);
        return;
    }
    (*data).second.timer = timer; (*data).second.disabled = false;
}

// 禁用指定索引的计时器
void TimerManager::DisableTimer(uint32 index)
{
    auto data = m_timers.find(index);
    if (data == m_timers.end())
    {
        sLog.outError("Timer index %u does not exist.", index);
        return;
    }
    (*data).second.timer = 0; (*data).second.disabled = true;
}

// 缩减计时器时间（取较小值）
void TimerManager::ReduceTimer(uint32 index, uint32 timer)
{
    auto data = m_timers.find(index);
    if (data == m_timers.end())
    {
        sLog.outError("Timer index %u does not exist.", index);
        return;
    }
    (*data).second.timer = std::min((*data).second.timer, timer);
}

// 延迟计时器时间（取较大值）
void TimerManager::DelayTimer(uint32 index, uint32 timer)
{
    auto data = m_timers.find(index);
    if (data == m_timers.end())
    {
        sLog.outError("Timer index %u does not exist.", index);
        return;
    }
    if (!(*data).second.disabled)
        (*data).second.timer = (*data).second.timer > timer ? (*data).second.timer : timer;
}

// 若未启动则重置并启动计时器
void TimerManager::ResetIfNotStarted(uint32 index, uint32 timer)
{
    auto data = m_timers.find(index);
    if (data == m_timers.end())
    {
        sLog.outError("Timer index %u does not exist.", index);
        return;
    }
    if ((*data).second.disabled)
    {
        (*data).second.timer = timer;
        (*data).second.disabled = false;
    }
}

void TimerManager::UpdateTimers(const uint32 diff)
{
    UpdateTimers(diff, false);
}

// 循环更新所有计时器并执行到期动作回调
void TimerManager::UpdateTimers(const uint32 diff, bool combat)
{
    for (auto& data : m_timers)
    {
        Timer& timer = data.second;
        if (timer.UpdateTimer(diff, combat))
            timer.functor();
    }
}

// 重置管理器中所有计时器
void TimerManager::ResetAllTimers()
{
    for (auto& data : m_timers)
        data.second.ResetTimer();
}

// 闪避脱战时重置非永久计时器
void TimerManager::ResetTimersOnEvade()
{
    for (auto& data : m_timers)
        if (data.second.combatSetting != TIMER_ALWAYS)
            data.second.ResetTimer();
}

void TimerManager::GetAIInformation(ChatHandler& reader)
{
    reader.PSendSysMessage("TimerAI: Timers:");
    std::string output = "";
    for (auto itr = m_timers.begin(); itr != m_timers.end(); ++itr)
    {
        Timer& timer = (*itr).second;
        output += "Timer ID: " + std::to_string(timer.id) + " Timer: " + std::to_string(timer.timer) +" Disabled: " + std::to_string(timer.disabled) + "\n";
    }
    reader.PSendSysMessage("%s", output.data());
}

// 更新战斗动作计时器并在就绪时标记状态
void CombatActions::UpdateTimers(const uint32 diff, bool combat)
{
    TimerManager::UpdateTimers(diff, combat);
    for (auto& data : m_combatActions)
    {
        Timer& timer = data.second;
        if (timer.UpdateTimer(diff, combat))
            timer.functor();
    }
}

// 重置所有战斗动作计时器与就绪状态
void CombatActions::ResetAllTimers()
{
    for (uint32 i = 0; i < m_actionReadyStatus.size(); ++i)
    {
        auto itr = m_timerlessActionSettings.find(i);
        if (itr == m_timerlessActionSettings.end())
            m_actionReadyStatus[i] = false;
        else
            m_actionReadyStatus[i] = (*itr).second;
    }
    for (auto& data : m_combatActions)
        data.second.ResetTimer();
    TimerManager::ResetAllTimers();
}

// 闪避脱战时重置所有战斗动作计时器
void CombatActions::ResetTimersOnEvade()
{
    for (uint32 i = 0; i < m_actionReadyStatus.size(); ++i)
    {
        auto itr = m_timerlessActionSettings.find(i);
        if (itr == m_timerlessActionSettings.end())
            m_actionReadyStatus[i] = false;
        else
            m_actionReadyStatus[i] = (*itr).second;
    }
    for (auto& data : m_combatActions)
        data.second.ResetTimer();
    TimerManager::ResetTimersOnEvade();
}

// 添加初始禁用的战斗动作
void CombatActions::AddCombatAction(uint32 id, bool disabled)
{
    m_combatActions.emplace(id, Timer(id, [&, id] { m_actionReadyStatus[id] = true; }, 0, 0, TIMER_COMBAT_COMBAT, disabled));
    m_actionReadyStatus[id] = !disabled;
}

// 添加固定间隔的战斗动作
void CombatActions::AddCombatAction(uint32 id, uint32 timer)
{
    m_combatActions.emplace(id, Timer(id, [&, id] { m_actionReadyStatus[id] = true; }, timer, timer, TIMER_COMBAT_COMBAT, false));
    m_actionReadyStatus[id] = false;
}

// 添加随机区间间隔的战斗动作
void CombatActions::AddCombatAction(uint32 id, uint32 timerMin, uint32 timerMax)
{
    m_combatActions.emplace(id, Timer(id, [&, id] { m_actionReadyStatus[id] = true; }, timerMin, timerMax, TIMER_COMBAT_COMBAT, false));
    m_actionReadyStatus[id] = false;
}

// 添加无计时器的战斗动作（如血量阶段触发）
void CombatActions::AddTimerlessCombatAction(uint32 id, bool byDefault)
{
    m_timerlessActionSettings[id] = byDefault;
    m_actionReadyStatus[id] = byDefault;
}

void CombatActions::ResetTimer(uint32 index, uint32 timer)
{
    auto data = m_combatActions.find(index);
    if (data == m_combatActions.end())
        TimerManager::ResetTimer(index, timer);
    else
    {
        (*data).second.timer = timer;
        (*data).second.disabled = false;
    }
}

void CombatActions::DisableTimer(uint32 index)
{
    auto data = m_combatActions.find(index);
    if (data == m_combatActions.end())
        TimerManager::DisableTimer(index);
    else
    {
        (*data).second.timer = 0;
        (*data).second.disabled = true;
    }
}

void CombatActions::ReduceTimer(uint32 index, uint32 timer)
{
    auto data = m_combatActions.find(index);
    if (data == m_combatActions.end())
        TimerManager::ReduceTimer(index, timer);
    else
        (*data).second.timer = std::min((*data).second.timer, timer);
}

void CombatActions::DelayTimer(uint32 index, uint32 timer)
{
    auto data = m_combatActions.find(index);
    if (data == m_combatActions.end())
        TimerManager::DelayTimer(index, timer);
    else if (!(*data).second.disabled)
        (*data).second.timer = (*data).second.timer > timer ? (*data).second.timer : timer;
}

void CombatActions::ResetIfNotStarted(uint32 index, uint32 timer)
{
    auto data = m_combatActions.find(index);
    if (data == m_combatActions.end())
        TimerManager::ResetIfNotStarted(index, timer);
    else if ((*data).second.disabled)
    {
        (*data).second.timer = timer;
        (*data).second.disabled = false;
    }
}

// 禁用指定战斗动作
void CombatActions::DisableCombatAction(uint32 index)
{
    if (m_timerlessActionSettings.find(index) == m_timerlessActionSettings.end())
        DisableTimer(index);
    SetActionReadyStatus(index, false);
}

void CombatActions::GetAIInformation(ChatHandler& reader)
{
    reader.PSendSysMessage("Combat Timers:");
    std::string output = "";
    for (auto itr = m_combatActions.begin(); itr != m_combatActions.end(); ++itr)
    {
        Timer& timer = (*itr).second;
        output += "Timer ID: " + std::to_string(timer.id) + " Timer: " + std::to_string(timer.timer) +" Disabled: " + std::to_string(timer.disabled) + "\n";
    }
    reader.PSendSysMessage("%s", output.data());
}
