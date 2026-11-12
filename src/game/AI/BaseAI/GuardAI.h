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

#ifndef MANGOS_GUARDAI_H
#define MANGOS_GUARDAI_H

#include "CreatureAI.h"
#include "Entities/ObjectGuid.h"
#include "Util/Timer.h"

class Creature;

// 城镇/阵营守卫 AI：负责在视野内主动拦截敌对玩家、主动怪或处于战斗状态的敌方单位
class GuardAI : public CreatureAI
{
        enum GuardState
        {
            STATE_NORMAL = 1,                                           // 常规巡逻/警戒状态
            STATE_LOOK_AT_VICTIM = 2                                    // 注视攻击目标状态
        };

    public:
        explicit GuardAI(Creature* creature);

        void MoveInLineOfSight(Unit* who) override;                     // 视野检测：主动攻击合法敌对目标或攻击平民的敌对生物

        static int Permissible(const Creature* creature);               // 判定生物是否适用 GuardAI
    protected:
        std::string GetAIName() override { return "GuardAI"; }
};
#endif
