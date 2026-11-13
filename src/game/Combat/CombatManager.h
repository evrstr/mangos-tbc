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

#ifndef _COMBATMANAGER
#define _COMBATMANAGER

#include "Common.h"
#include <functional>
#include <memory>
#include "Entities/Object.h"

// 战斗逃脱/脱战状态枚举
enum EvadeState
{
    EVADE_NONE,                                                         // 未处于逃脱状态
    EVADE_COMBAT, // In a dungeon in combat evades all hits until target becomes reachable (副本战斗中目标不可达时触发，闪避所有攻击直至目标重新可达)
    EVADE_HOME, // When running home evades all hits and disables some AI actions (跑回出生点脱战重置期间，闪避所有攻击并禁用部分 AI 行为)
};

class Unit;

// 单位战斗管理器：管理脱战/逃脱状态、追击超时（Leash）计时器及自定义脱战判定
class CombatManager
{
    public:
        CombatManager(Unit* owner);

        void Update(const uint32 diff);                                 // 每帧更新逃脱计时器、脱战倒计时与牵引距离检测

        // evade handling (逃脱状态处理)
        bool IsInEvadeMode() const { return m_evadeTimer > 0 || m_evadeState; } // 是否处于任意逃脱模式
        bool IsEvadingHome() const { return m_evadeState == EVADE_HOME; } // 是否正在跑回出生点重置
        bool IsEvadeRegen() const { return (m_evadeTimer > 0 && m_evadeTimer <= 5000) || m_evadeState; } // Only regen after 5 seconds, or when in permanent evade (逃脱计时过半或处于永久逃脱状态时才允许回血)
        void StartEvadeTimer() { m_evadeTimer = 10000; } // 10 seconds after which action is taken (启动 10 秒战斗逃脱计时器，到期后触发脱战处理)
        void StopEvade(); // Stops either timer or evade state (停止逃脱计时器或清除逃脱状态)
        EvadeState GetEvadeState() const { return m_evadeState; }
        void SetEvadeState(EvadeState state); // Propagated to pets (设置逃脱状态并同步传播至宠物/随从)
        void OnCombatStart();                                           // 进入战斗时立即触发一次检测刻度

        // combat timer handling (战斗追击计时处理)
        uint32 GetCombatTimer() const { return m_combatTimer; }
        void TriggerCombatTimer(Unit* target);                          // 攻击目标时刷新目标的战斗追击计时器
        void TriggerCombatTimer(bool pvp);                              // 按 PVP/PVE 模式重置战斗计时器并记录当前位置
        void TriggerCombatTimer(uint32 timer);                          // 按指定毫秒数重置战斗计时器并记录位置
        void StopCombatTimer() { m_combatTimer = 0; }                   // 清零战斗计时器
        std::shared_ptr<time_t> const& GetLastLeashExtensionPtr() const; // 获取共享牵引时间指针
        void ShareLeashExtension(std::shared_ptr<time_t> const& ptr);   // 设置/同步共享牵引时间指针（群怪联动）
        void ClearLeashExtension();                                     // 清空共享牵引时间
        time_t GetLastLeashExtension() const;                           // 获取最后一次牵引时间
        void UpdateLeashExtension();                                    // 刷新当前牵引时间戳
        bool IsLeashingDisabled() { return m_leashingDisabled; }        // 是否禁用追击脱战限制
        void SetLeashingDisable(bool apply) { m_leashingDisabled = apply; }
        void SetLeashingCheck(std::function<bool(Unit*, float x, float y, float z)> check) { m_leashingCheck = check; } // if check evals as true - evade (注册自定义脱战位置检查回调，返回 true 时强制脱战)
    private:
        Unit* m_owner;                                                  // 所属单位

        // evade handling (逃脱状态数据)
        uint32 m_evadeTimer; // Used for evade during combat when mob is not running home and target isnt reachable (战斗中未跑回出生点但目标不可达时的逃脱倒计时)
        EvadeState m_evadeState; // Used for evade during running home (跑回出生点期间的逃脱状态)
        uint32 m_combatTick;                                            // 周期性脱战检测累计时间

        // combat timer handling (追击计时与脱战检查数据)
        uint32 m_combatTimer;                                           // 剩余追击战斗时间（毫秒）
        Position m_lastRefreshPos;                                      // 上次刷新战斗计时器时的位置
        mutable std::shared_ptr<time_t> m_lastLeashExtension;           // 共享牵引刷新时间戳（群怪联动刷新）
        bool m_leashingDisabled;                            // disables leashing timer for script mobs (脚本怪物禁用牵引计时器标志)
        std::function<bool(Unit*, float x, float y, float z)> m_leashingCheck; // 自定义脱战坐标判定函数
};

#endif