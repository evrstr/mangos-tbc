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

#ifndef MANGOS_PLAYER_AI_H
#define MANGOS_PLAYER_AI_H

#include "AI/BaseAI/UnitAI.h"
#include "AI/ScriptDevAI/base/TimerAI.h"
#include "Entities/CreatureSpellList.h"
#include <functional>
#include <vector>

class Player;

// for future use - in case PlayerAI needs a different purpose from control during charms split them
// 预留扩展：若后续 PlayerAI 需用于非魅惑控制以外的场景可拆分之；当前用于玩家被魅惑期间的 AI 控制
class PlayerAI : public UnitAI
{
    public:
        PlayerAI(Player* player);

        void JustGotCharmed(Unit* charmer) override;                    // 刚被魅惑时加入阵型或跟随魅惑者并攻击最近敌人
        void EnterEvadeMode() override;                                 // 脱战重置并恢复跟随魅惑者
        void AttackClosestEnemy() override;                             // 优先选择敌对玩家或最近敌方目标发起攻击

        void RequestFollow(Unit* followee) override;                    // 请求跟随指定目标

        CreatureSpellList const& GetSpellList() const override { return m_spellList; }

        void ExecuteActions() override;                                 // 执行 AI 行为（施法与切换目标）
    protected:
        // 注册玩家可用的法术动作及其目标选择回调
        void AddPlayerSpellAction(uint32 spellId, std::function<Unit*()> selector = nullptr);
        Player* m_player;
        
        CreatureSpellList m_spellList;
    private:
        // 单个玩家法术动作条目：法术 ID、无目标时是否跳过、目标选择器
        struct SpellData
        {
            uint32 spellId;
            bool failureOnNoTarget;
            std::function<Unit*()> targetFinder;
            SpellData(uint32 spellId, bool failureOnNoTarget, std::function<Unit*()> targetFinder) : spellId(spellId), failureOnNoTarget(failureOnNoTarget), targetFinder(targetFinder) {}
        };

        void ExecuteSpells();                                           // 随机打乱并尝试施放已注册法术，同时处理随机切换目标
        std::vector<SpellData> m_playerSpellActions;
        bool m_spellsDisabled;                                          // 施法冷却标记（防止连续高频施法）
        bool m_executeTargetChange;                                     // 是否触发一次随机目标切换
};

// 根据玩家职业创建对应的魅惑专用 PlayerAI 子类实例
UnitAI* GetClassAI(Classes playerClass, Player* player);

#endif