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
#ifndef __BATTLEGROUNDRL_H
#define __BATTLEGROUNDRL_H

class BattleGround;

// 洛丹伦废墟竞技场玩家积分记录类
class BattleGroundRLScore : public BattleGroundScore
{
    public:
        BattleGroundRLScore() {};
        virtual ~BattleGroundRLScore() {};
        // TODO fix me (待补充积分扩展)
};

// 洛丹伦废墟竞技场（Ruins of Lordaeron）逻辑控制类
class BattleGroundRL : public BattleGround
{
        friend class BattleGroundMgr;

    public:
        BattleGroundRL();

        void Reset() override;                              // 重置状态并启用双方存活人数 HUD 变量

        /* inherited from BattlegroundClass (继承自 BattleGround 的虚方法覆写) */
        virtual void AddPlayer(Player* plr) override;
        virtual void StartingEventOpenDoors() override;

        void RemovePlayer(Player* plr, ObjectGuid guid) override;
        void HandleKillPlayer(Player* player, Player* killer) override;
        bool HandlePlayerUnderMap(Player* player) override;
};
#endif
