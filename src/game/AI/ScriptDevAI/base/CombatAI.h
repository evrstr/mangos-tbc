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

#ifndef COMBAT_AI_H
#define COMBAT_AI_H

#include "Entities/Creature.h"
#include "AI/ScriptDevAI/include/sc_creature.h"

/*
    Small wrapper class that reduces the amount of code needed to use CombatActions and adds proper spell action reset functionality
    战斗 AI 封装基类：简化 CombatActions 的调用代码，提供法术施法朝向保持、目标恢复及击杀玩家反应功能
*/
class CombatAI : public ScriptedAI
{
    public:
        CombatAI(Creature* creature, uint32 combatActions);

        void Reset() override;                              // 重置状态与计时器

        void HandleDelayedInstantAnimation(SpellEntry const* spellInfo) override; // 延迟瞬发法术施法动画处理（暂存当前目标）
        void HandleTargetRestoration();                     // 施法完成后将目标恢复为主仇恨受害者
        bool IsTargetingRestricted() override;              // 检查目标切换是否受到限制
        void StopTargeting(bool state) { m_stopTargeting = state; } // 启用或关闭目标选择
        void OnTaunt() override;                            // 受到嘲讽时立即清理暂存目标

        void AddOnKillText(int32 text);                     // 添加击杀玩家时的广播台词
        template<typename... Targs>
        void AddOnKillText(int32 value, Targs... fargs)
        {
            AddOnKillText(value);
            AddOnKillText(fargs...);
        }
        void AddOnKillSound(int32 soundId);                 // 添加击杀玩家时的音效 ID
        void ClearOnKillReactions();                        // 清空击杀玩家反应列表
        void KilledUnit(Unit* /*victim*/) override;         // 击杀单位回调（击杀玩家时播放台词或音效）

        // virtual void ExecuteAction(uint32 action) {}
    private:
        ObjectGuid m_storedTarget;                          // 施法期间暂存锁定的目标 GUID

        std::vector<std::pair<int32, bool>> m_onDeathReactions; // 击杀反应条目列表 (ID, 是否为声音)
        bool m_onKillCooldown;                              // 击杀反应内置冷却标志

        bool m_stopTargeting;                               // 是否停止目标更新与朝向切换
};

#endif
