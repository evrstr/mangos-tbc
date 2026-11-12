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

#ifndef MANGOS_CREATUREAI_H
#define MANGOS_CREATUREAI_H

#include "AI/BaseAI/UnitAI.h"

class Creature;

// 生物 AI 基类：在 UnitAI 基础上扩展生物特有的进战/脱战/重置、假死保护、求援、撤退及阵型跟随能力
class CreatureAI : public UnitAI
{
    public:
        explicit CreatureAI(Creature* creature);
        explicit CreatureAI(Creature* creature, uint32 combatActions);

        virtual void Reset();                                           // 重置生物 AI 状态与计时器

        virtual void OnRemoveFromWorld() {}                             // 生物从世界移除时回调

        virtual void EnterCombat(Unit* enemy) override;                 // 进入战斗：记录开战位置并触发仇恨、下马与求援
        virtual void EnterEvadeMode() override;                         // 脱离战斗并跑回出生点重置
        virtual void AttackStart(Unit* who) override;                   // 对目标发起攻击与追击
        virtual void DamageTaken(Unit* dealer, uint32& damage, DamageEffectType damageType, SpellEntry const* spellInfo) override; // 受伤回调（处理防死亡拦截）
        virtual void JustPreventedDeath(Unit* /*attacker*/) {}          // 触发防死亡保护时回调
        virtual void JustReachedHome() override;                        // 脱战跑回出生点到达时回调
        virtual void JustRespawned() override;                          // 重生时回调

        void DoFakeDeath(uint32 spellId = 0);                           // 触发假死状态并可选施放假死法术
        void SetDeathPrevention(bool state);                            // 开启或关闭防死亡拦截（血量锁 1）
        void ResetDeathPrevented() { m_deathPrevented = false; }        // 重置已触发的防死亡标记

        bool DoRetreat() override;                                      // 触发撤退并向最近友军求援
        void DoCallForHelp(float radius) override;                      // 向指定半径内的同阵营友军呼救

        void RetreatingArrived() override;                              // 撤退到达友军位置时触发呼救与恢复
        void RetreatingEnded() override;                                // 撤退动作结束时恢复追击或脱战

        void HandleAssistanceCall(Unit* sender, Unit* invoker) override; // 响应友军发出的支援呼叫

        void AddUnreachabilityCheck(); // use in constructor (在构造函数中注册目标不可达检测，用于传送或脱战)

        CreatureSpellList const& GetSpellList() const override;         // 获取生物当前使用的法术列表

        void TimedFleeingEnded() override;                              // 定时逃跑结束时重置状态并恢复追击

        void OnCallForHelp(Unit* enemy) override;                       // 响应求援并进入战斗

        // Starts follow and in case of player engages player based formation (发起跟随；若目标为玩家则尝试加入玩家阵型)
        virtual void RequestFollow(Unit* followee) override;
        virtual void RelinquishFollow(ObjectGuid follower) override;    // 解除指定跟随者的跟随/阵型关系
    protected:

        Creature* m_creature;                                           // 所属生物指针
        bool m_deathPrevented;                                          // 本次生命周期是否已触发过防死亡拦截

        float m_followAngle;                                            // 跟随偏移角度
        float m_followDist;                                             // 跟随距离
        ObjectGuid m_requestedFollower;                                 // 当前请求跟随的对象的 GUID
};

#endif
