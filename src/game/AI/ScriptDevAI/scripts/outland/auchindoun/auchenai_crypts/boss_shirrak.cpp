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

/* ScriptData
SDName: boss_shirrak
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Auchenai Crypts
EndScriptData */

// 死亡观察者希尔拉克（奥金尼地穴一号首领）
/* 遭遇战概述：
 * 希尔拉克拥有强力的法术抑制光环（距离越近施法延迟惩罚越高），
 * 会向随机玩家发射聚焦之火（召唤缓慢移动的火球），并释放肉食撕咬与吸引魔法。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "Spells/SpellAuras.h"
#include "Spells/Scripts/SpellScript.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 法术、文本与随从枚举
enum
{
    EMOTE_FOCUS                     = 19321,                    // 聚焦之火注视提示

    SPELL_CARNIVOROUS_BITE          = 36383,                    // 肉食撕咬（普通）
    SPELL_CARNIVOROUS_BITE_H        = 39382,                    // 肉食撕咬（英雄）
    SPELL_INHIBIT_MAGIC             = 32264,                    // 抑制魔法（减缓施法速度）
    SPELL_INHIBIT_MAGIC_TRIGGER     = 33460, // periodically triggers 32264 (周期性触发抑制魔法光环)
    SPELL_ATTRACT_MAGIC             = 32265,                    // 吸引魔法

    SPELL_FOCUS_TARGET_VISUAL       = 32286,                    // 聚焦目标视觉指示
    SPELL_BIRTH                     = 26262,                    // 出生视觉
    SPELL_FOCUS_FIRE_SUMMON         = 32283,                    // 召唤聚焦之火
    SPELL_FOCUS_FIRE_CALLBACK       = 32300,                    // 聚焦之火命中回调
    SPELL_FOCUS_FIRE_AURA           = 32291,                    // 聚焦之火光环
    NPC_FOCUS_FIRE                  = 18374  // summoned by 32283 (被 32283 召唤的聚焦之火 NPC)
};

// 希尔拉克战斗行为枚举
enum ShirrakActions
{
    SHIRRAK_ACTION_MAX,
};

// 死亡观察者希尔拉克 AI
struct boss_shirrakAI : public CombatAI
{
    boss_shirrakAI(Creature* creature) : CombatAI(creature, SHIRRAK_ACTION_MAX), m_bIsRegularMode(creature->GetMap()->IsRegularDifficulty())
    {}

    bool m_bIsRegularMode;                                      // 是否为普通难度模式

    void Reset() override                                       // 遭遇战重置回调
    {
        CombatAI::Reset();

        DoCastSpellIfCan(nullptr, SPELL_INHIBIT_MAGIC_TRIGGER, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    void JustDied(Unit* /*killer*/) override                    // 首领死亡回调
    {
        m_creature->RemoveAurasDueToSpell(SPELL_INHIBIT_MAGIC_TRIGGER); // TODO: Investigate passive spell removal on death (死亡时移除被动光环)
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* target) override // 施法成功回调
    {
        if (spellInfo->Id == SPELL_FOCUS_FIRE_AURA)
            DoBroadcastText(EMOTE_FOCUS, m_creature, target);
    }
};

// 抑制魔法光环脚本：依据玩家与施法者距离叠加光环层数
struct InhibitMagic : public AuraScript
{
    void OnHolderInit(SpellAuraHolder* holder, WorldObject* caster) const override
    {
        uint32 stacks = 1;
        if (caster) // Incorrect impl, meant to be chain cast 4 times with different radius and stacks reset on each first cast (计算距离并设定抑制魔法层数)
        {
            float dist = holder->GetTarget()->GetDistance(caster, true, DIST_CALC_COMBAT_REACH);
            if (dist > 45.f)
                stacks = 1;
            else if (dist > 30.f)
                stacks = 2;
            else if (dist > 15.f)
                stacks = 3;
            else
                stacks = 4;
        }
        holder->PresetAuraStacks(stacks);
    }
};

// 聚焦目标地面范围光环结束脚本
struct FocusTargetVisual : public AuraScript
{
    void OnPersistentAreaAuraEnd(DynamicObject* dynGo) const override
    {
        if (Unit* owner = dynGo->GetCaster())
            owner->CastSpell(nullptr, 32301, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, dynGo->GetObjectGuid());
    }
};

// 聚焦之火触发向希尔拉克施法的法术脚本
struct PingShirrak : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* target = spell->GetUnitTarget();
        if (!target)
            return;

        // Cast Focus fire on caster (向施法者引导聚焦之火)
        target->CastSpell(spell->GetCaster(), SPELL_FOCUS_FIRE_CALLBACK, TRIGGERED_OLD_TRIGGERED);
    }
};

// 注册死亡观察者希尔拉克脚本
void AddSC_boss_shirrak()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_shirrak";
    pNewScript->GetAI = &GetNewAIInstance<boss_shirrakAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<InhibitMagic>("spell_shirrak_inhibit_magic");
    RegisterSpellScript<FocusTargetVisual>("spell_focus_target_visual");
    RegisterSpellScript<PingShirrak>("spell_ping_shirrak");
}
