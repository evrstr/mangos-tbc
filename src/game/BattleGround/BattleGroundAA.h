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
#ifndef __BATTLEGROUNDAA_H
#define __BATTLEGROUNDAA_H

class BattleGround;

// 全竞技场统一模板积分记录类
class BattleGroundAAScore : public BattleGroundScore
{
    public:
        BattleGroundAAScore() {};
        virtual ~BattleGroundAAScore() {};
        // TODO fix me (待补充通用竞技场积分扩展)
};

// 全竞技场统一模板类（用于排队界面汇总所有竞技场入口）
class BattleGroundAA : public BattleGround
{
        friend class BattleGroundMgr;

    public:
        BattleGroundAA();

        /* inherited from BattlegroundClass (继承自 BattleGround 的玩家加入接口) */
        virtual void AddPlayer(Player* plr) override;
};
#endif
