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

#ifndef MANGOS_TOTEMAI_H
#define MANGOS_TOTEMAI_H

#include "CreatureAI.h"
#include "AI/EventAI/CreatureEventAI.h"
#include "Entities/ObjectGuid.h"
#include "Util/Timer.h"

class Creature;
class Totem;

// 图腾 AI：周期性锁定法术射程内的合法敌对目标并施放图腾关联法术（如灼热图腾、熔岩图腾）
class TotemAI : public CreatureEventAI
{
    public:
        explicit TotemAI(Creature* creature);

        void SpellHit(Unit* unit, const SpellEntry* spellInfo) override; // 响应法术命中（继承 EventAI 触发）
        void MoveInLineOfSight(Unit* who) override;                     // 图腾不通过视野主动触发普通生物移动索敌
        void AttackStart(Unit* who) override;                           // 记录攻击目标 GUID（图腾不移动追击）
        void EnterEvadeMode() override;                                 // 空实现（图腾不跑回脱战）
        bool IsVisible(Unit* who) const override;                       // 视野可见性判定

        void UpdateAI(const uint32 diff) override;                      // 刷新目标校验并在就绪时施放图腾法术
        static int Permissible(const Creature* creature);               // 判定生物是否为图腾类型
    protected:
        std::string GetAIName() override { return "TotemAI"; }
        Totem& getTotem() const;                                        // 安全转换为 Totem 引用

    private:
        ObjectGuid i_victimGuid;                                        // 当前锁定的攻击目标 GUID
};
#endif
