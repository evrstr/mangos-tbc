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

#ifndef MANGOS_PETAI_H
#define MANGOS_PETAI_H

#include "CreatureAI.h"
#include "Entities/ObjectGuid.h"
#include "Spells/SpellDefines.h"

class Creature;
class Spell;
class CharmInfo;

// 玩家宠物（受玩家操控栏控制）及常规被魅惑生物 AI：管理主动/防御/被动反应、跟随主人及自动施法队列
class PetAI : public CreatureAI
{
    public:

        explicit PetAI(Creature* creature);
        explicit PetAI(Creature* creature, uint32 combatActions);

        void MoveInLineOfSight(Unit* who) override;                     // 主动模式下在视野内自动寻找可攻击目标
        void AttackStart(Unit* who) override;                           // 发起攻击并通知停止/切换目标
        void EnterEvadeMode() override;                                 // 停止战斗并将控制权交回主人指令状态
        void AttackedBy(Unit* attacker) override;                       // 受击时在防御/主动模式下反击

        void UpdateAI(const uint32 diff) override;                      // 宠物主循环：目标校验、自动平砍、自动施法、脱战回位
        static int Permissible(const Creature* creature);               // 判定生物是否适用 PetAI

        void OnUnsummon() override;                                     // 取消召唤时释放跟随关系
        void JustDied(Unit* killer) override;                           // 宠物死亡时清理状态
        void JustRespawned() override;                                  // 宠物重生/召唤后清除延迟与状态

    protected:
        std::string GetAIName() override { return "PetAI"; }

    private:
        // 为有益法术挑选合适的小队成员（如驱散、BUFF）
        Player* PickGroupMemberForSpell(Player* player, SpellEntry const* spellInfo);
        // 遍历宠物自动施法列表，筛选满足条件且可施放的（法术, 目标, 是否需进入追击）集合
        std::vector<std::tuple<SpellEntry const*, Unit*, bool>> PickSpellWithTarget(Unit* owner, Unit* victim, CharmInfo* charminfo);
        bool Cast(std::tuple<SpellEntry const*, Unit*, bool> spellWithTarget); // 执行单个选中法术的施放或追击
        bool ShouldCast(SpellEntry const* spellInfo, Unit* victim);     // 检查能量消耗与目标距离，决定是否应施法

        bool CanAutoCastAreaAura(SpellEntry const* spellInfo, Unit* target) const; // 检查区域光环类自动施法条件
        bool CanAutoCast(SpellEntry const* spellInfo, Unit* target) const;         // 检查单体自动施法条件
        SpellCastResult CheckPetCast(SpellEntry const* spellInfo, Unit* target) const; // 校验宠物施法合法性

        bool m_inCombat;                                                // 宠物当前是否处于战斗循环标记

        Pet* m_pet;                                                     // 若所属生物为 Pet 实例则缓存其指针，否则为 nullptr
};
#endif
