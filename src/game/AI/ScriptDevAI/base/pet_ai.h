/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_PET_H
#define SC_PET_H

// Using UnitAI for now. Might change later and use PetAI (need to export for dll first)
// 脚本化宠物/仆从/守护者 AI 基类：处理主人跟随、协助战斗与脱战状态管理
class ScriptedPetAI : public CreatureAI
{
    public:
        explicit ScriptedPetAI(Creature* creature);
        ~ScriptedPetAI() {}

        void AttackStart(Unit* /*who*/) override;           // 开始攻击并追击目标

        void AttackedBy(Unit* /*attacker*/) override;         // 受到攻击反击判定

        void KilledUnit(Unit* /*victim*/) override {}         // 击杀单位回调

        void OwnerKilledUnit(Unit* /*victim*/) override {}    // 主人击杀单位回调

        void UpdateAI(const uint32 diff) override;          // 核心更新主循环

        void CombatStop() override;                         // 停止战斗并重置宠物状态

        virtual void Reset() override {}                    // 重置 AI 虚函数

        virtual void UpdatePetAI(const uint32 diff);      // while in combat (战斗状态下的更新逻辑)

        virtual void UpdatePetOOCAI(const uint32 /*diff*/) {} // when not in combat (脱战脱离战斗状态下的更新逻辑)

    protected:
        void ResetPetCombat();                              // 重置宠物战斗状态并恢复跟随主人
};

#endif
