#pragma once
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

#ifndef MANGOS_GUARDIANAI_H
#define MANGOS_GUARDIANAI_H

#include "PetAI.h"
#include "AI/EventAI/CreatureEventAI.h"
#include "Entities/ObjectGuid.h"
#include "Util/Timer.h"

class Creature;
class Spell;

// 守护者（非玩家直接操控的召唤随从/生物宠物）AI：继承 CreatureEventAI 以支持事件脚本与主人协同战斗
class GuardianAI : public CreatureEventAI
{
    public:

        explicit GuardianAI(Creature* creature);
        static int Permissible(const Creature* creature);               // 判定生物是否为守护者类型

        //bool IsControllable() const override { return true; }
        virtual void JustRespawned() override;                          // 召唤/重生时按主人阵营状态及当前目标自动跟进攻击
        virtual void EnterEvadeMode() override;                         // 脱战后恢复跟随主人
        virtual void UpdateAI(const uint32 diff) override;              // 同步主人战斗状态、自动索敌与执行事件/法术动作

        void JustDied(Unit* killer) override;                           // 死亡时清理事件队列与死亡状态

        void OnUnsummon() override;                                     // 取消召唤时清理跟随关系
    protected:
        std::string GetAIName() override { return "GuardianAI"; }
};
#endif
