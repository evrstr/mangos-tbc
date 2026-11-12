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

#ifndef BOSS_AI_H
#define BOSS_AI_H

#include "AI/ScriptDevAI/include/sc_instance.h"
#include "Entities/Creature.h"
#include "AI/ScriptDevAI/include/sc_creature.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 副本事件动作枚举
enum InstanceActions
{
    INSTANCE_CLOSE_ENTRANCE_DOOR = 250,                     // 开战后关闭入口大门动作 ID
};

// 首领战 AI 基类：封装副本门禁开关、遭遇战进度状态更新、开战/死亡随机台词及战斗计时
class BossAI : public CombatAI
{
    public:
        BossAI(Creature* creature, uint32 combatActions) : CombatAI(creature, combatActions)
        {
            AddCustomAction(INSTANCE_CLOSE_ENTRANCE_DOOR, true, [&]()
            {
                OpenEntrances(false);                       // 关门动作：关闭入口大门
            });
        }

        void OpenEntrances(bool open = true)                // 控制入口门开启或关闭
        {
            ScriptedInstance* instance = dynamic_cast<ScriptedInstance*>(m_creature->GetInstanceData());
            if (!instance)
                return;

            for (auto& id : m_entranceObjects)
                instance->DoUseOpenableObject(id, open);
        }
        void OpenExits()                                    // 开启所有出口门
        {
            ScriptedInstance* instance = dynamic_cast<ScriptedInstance*>(m_creature->GetInstanceData());
            if (!instance)
                return;

            for (auto& id : m_exitObjects)
                instance->DoUseOpenableObject(id, true);
        }
        /**
        * Adds one or more Broadcast Texts to possibly emit when Unit dies (添加单位死亡时随机播放的广播文本)
        * This function is not called if JustDied is overridden. Add CombatAI::JustDied(); to your overriding function.
        * @param text Broadcast Text ID
        */
        void AddOnDeathText(uint32 text);
        template<typename... Targs>
        void AddOnDeathText(uint32 value, Targs... fargs)
        {
            AddOnDeathText(value);
            AddOnDeathText(fargs...);
        }
        /**
        * Adds one or more Broadcast Texts to possibly emit when Unit enters combat (添加单位进入战斗时随机播放的广播文本)
        * This function is not called if Aggro is overridden. Add CombatAI::Aggro(); to your overriding function.
        * @param text Broadcast Text ID
        */
        void AddOnAggroText(uint32 text);
        template<typename... Targs>
        void AddOnAggroText(uint32 value, Targs... fargs)
        {
            AddOnAggroText(value);
            AddOnAggroText(fargs...);
        }

        void SetDataType(uint32 type) { m_instanceDataType = type; } // 设置副本进度数据标识 (如 TYPE_BOSS_X)

        void JustDied(Unit* killer = nullptr) override;     // 死亡回调：播放遗言、置副本状态为 DONE、开启进出口大门
        void JustReachedHome() override;                    // 脱战归位回调：置副本状态为 FAIL、重置入口门与自身状态
        void Aggro(Unit* who = nullptr) override;           // 进入战斗回调：记录开战时间、置副本状态为 IN_PROGRESS、启动关门倒计时
        void AddEntranceObject(uint32 value);               // 添加入口门游戏对象数据 ID
        template <typename... Targs>
        void AddEntranceObject(uint32 value, Targs... fargs)
        {
            AddEntranceObject(value);
            AddEntranceObject(fargs...);
        }
        void AddExitObject(uint32 value);                   // 添加出口门游戏对象数据 ID
        template <typename... Targs>
        void AddExitObject(uint32 value, Targs... fargs)
        {
            AddExitObject(value);
            AddExitObject(fargs...);
        }
        void SetGateDelay(std::chrono::milliseconds delay) { m_gateDelay = delay; } // 设置开战后门禁关闭延迟

        std::chrono::seconds TimeSinceEncounterStart()       // 获取自开战以来的持续秒数
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_combatStartTimestamp);
        }

        static std::chrono::milliseconds RandomTimer(std::chrono::milliseconds min, std::chrono::milliseconds max) // 生成随机时间间隔
        {
            return std::chrono::milliseconds(urand(min.count(), max.count()));
        }
    private:
        std::vector<uint32> m_onKilledTexts;                // 死亡时备选广播台词 ID 列表
        std::vector<uint32> m_onAggroTexts;                 // 开战时备选广播台词 ID 列表

        std::vector<uint32> m_entranceObjects;              // 入口门对象 ID 列表
        std::vector<uint32> m_exitObjects;                  // 出口门对象 ID 列表
        std::chrono::milliseconds m_gateDelay = 3s;         // 关门延迟时间（默认 3 秒）

        uint32 m_instanceDataType = -1;                     // 对应的副本首领状态标识类型

        std::chrono::steady_clock::time_point m_combatStartTimestamp; // 战斗遭遇开始时间戳
};

#endif