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

#ifndef SPELLSCRIPT_H
#define SPELLSCRIPT_H

#include "Spells/Spell.h"
#include <map>
#include <memory>
#include <functional>

class DynamicObject;

// 周期性触发法术时传递给脚本回调的上下文参数包
struct PeriodicTriggerData
{
    WorldObject* trueCaster;                                // 真实施法者对象（可能为图腾、物品或单位）
    Unit* caster; Unit* target; WorldObject* targetObject;  // 施法单位、目标单位及目标世界对象
    SpellEntry const* spellInfo;                            // 准备触发的子法术模板
    int32* basePoints;                                      // 可选的自定义基础效果数值数组（3 槽）
    uint32 triggerFlags;                                    // 触发施法标志位
    PeriodicTriggerData(WorldObject* trueCaster, Unit* caster, Unit* target, WorldObject* targetObject, SpellEntry const* spellInfo, int32* basePoints) :
        trueCaster(trueCaster), caster(caster), target(target), targetObject(targetObject), spellInfo(spellInfo), basePoints(basePoints), triggerFlags(TRIGGERED_OLD_TRIGGERED) {}
};

// 法术脚本基类：提供法术生命周期各阶段的虚函数钩子
struct SpellScript
{
    virtual ~SpellScript() = default;

    // called on spell init (法术对象初始化时调用)
    virtual void OnInit(Spell* /*spell*/) const {}
    // called on success during Spell::Prepare (法术准备成功开始读条/施放时调用)
    virtual void OnSuccessfulStart(Spell* /*spell*/) const {}
    // called on success inside Spell::finish - for channels this only happens if whole channel went through (施法正常完成时调用；引导法术仅在完整引导结束时触发)
    virtual void OnSuccessfulFinish(Spell* /*spell*/) const {}
    // called at end of Spell::CheckCast - strict is true in Spell::Prepare (施法合法性检查结尾调用，Prepare 阶段 strict 为 true)
    virtual SpellCastResult OnCheckCast(Spell* /*spell*/, bool /*strict*/) const { return SPELL_CAST_OK; }
    // called on Spell::SendCastResult - for overriding generic errors (发送施法结果前调用，用于覆盖通用失败错误码)
    virtual void OnSpellCastResultOverride(SpellCastResult& /*result*/, uint32& /*param1*/, uint32& /*param2*/) const {}
    // called before effect execution (每个法术效果执行前调用)
    virtual void OnEffectExecute(Spell* /*spell*/, SpellEffectIndex /*effIdx*/) const {}
    // called in targeting to determine radius for spell (计算法术作用半径时调用)
    virtual void OnRadiusCalculate(Spell* /*spell*/, SpellEffectIndex /*effIdx*/, bool /*targetB*/, float& /*radius*/) const {}
    // called on adding dest target (添加目标位置坐标时调用)
    virtual void OnDestTarget(Spell* /*spell*/) const {}
    // called on Unit Spell::CheckTarget (校验游戏对象目标合法性时调用)
    virtual bool OnCheckTarget(const Spell* /*spell*/, GameObject* /*target*/, SpellEffectIndex /*eff*/) const { return true; }
    // called on GameObject target checking (校验单位目标合法性时调用)
    virtual bool OnCheckTarget(const Spell* /*spell*/, Unit* /*target*/, SpellEffectIndex /*eff*/) const { return true; }
    // called in Spell::cast on all successful checks and after taking reagents (施法校验通过且消耗材料后、执行效果前调用)
    virtual void OnCast(Spell* /*spell*/) const {}
    // called on unit target hit before damage deal and proc or after effect execution for targetless, dest, item and go (命中结算伤害与触发之前调用)
    virtual void OnHit(Spell* /*spell*/, SpellMissInfo /*missInfo*/) const {}
    // called on target hit after damage deal and proc (命中结算伤害与触发之后调用)
    virtual void OnAfterHit(Spell* /*spell*/) const {}
    // called after summoning a creature (召唤生物成功后调用)
    virtual void OnSummon(Spell* /*spell*/, Creature* /*summon*/) const {}
    // called after summoning a gameobject (召唤游戏对象成功后调用)
    virtual void OnSummon(Spell* /*spell*/, GameObject* /*summon*/) const {}
};

// 光环数值计算上下文（供 OnAuraValueCalculate 使用）
struct AuraCalcData
{
    Unit* caster; Unit* target; SpellEntry const* spellProto; SpellEffectIndex effIdx; Item* castItem;
    Aura* aura; // cannot be used in auras that utilize stacking in checkcast - can be nullptr (当前光环指针，预计算阶段可能为 nullptr)
    AuraCalcData(Aura* aura, Unit* caster, Unit* target, SpellEntry const* spellProto, SpellEffectIndex effIdx, Item* castItem) : caster(caster), target(target), spellProto(spellProto), effIdx(effIdx), castItem(castItem), aura(aura) {}
};

// 光环脚本基类：提供光环创建、应用、计算、触发、吸收与周期结算钩子
struct AuraScript
{
    virtual ~AuraScript() = default;

    // called on SpellAuraHolder creation - caster can be nullptr (光环持有容器创建时调用，施法者可能为空)
    virtual void OnHolderInit(SpellAuraHolder* /*holder*/, WorldObject* /*caster*/) const {}
    // called after end of aura object constructor (光环实例构造完成后调用)
    virtual void OnAuraInit(Aura* /*aura*/) const {}
    // called during any event that calculates aura modifier amount - caster can be nullptr (计算光环效果数值时调用)
    virtual int32 OnAuraValueCalculate(AuraCalcData& /*data*/, int32 value) const { return value; }
    // called during done/taken damage calculation (计算造成/受到的伤害或治疗修正时调用)
    virtual void OnDamageCalculate(Aura* /*aura*/, Unit* /*attacker*/, Unit* /*victim*/, int32& /*advertisedBenefit*/, float& /*totalMod*/) const {}
    // called during duration calculation - target can be nullptr for channel duration calculation (计算光环或引导持续时间时调用)
    virtual int32 OnDurationCalculate(WorldObject const* /*caster*/, Unit const* /*target*/, int32 duration) const { return duration; }
    // 计算法术暴击几率加成时调用
    virtual void OnCritChanceCalculate(Aura* /*aura*/, Unit const* /*target*/, float& /*chance*/, SpellEntry const* /*spellInfo*/) const {}
    // the following two hooks are done in an alternative fashion due to how they are usually used
    // if an aura is applied before, its removed after, and if some aura needs to do something after aura effect is applied, need to revert that change before its removed
    // called before aura apply and after aura unapply (光环应用前 / 移除后调用，与 OnAfterApply 成对对称)
    virtual void OnApply(Aura* /*aura*/, bool /*apply*/) const {}
    // the following two hooks are done in an alternative fashion due to how they are usually used
    // if an aura is applied before, its removed after, and if some aura needs to do something after aura effect is applied, need to revert that change before its removed
    // called after aura apply and before aura unapply (光环应用后 / 移除前调用)
    virtual void OnAfterApply(Aura* /*aura*/, bool /*apply*/) const {}
    // called during proc eligibility checking (触发条件校验时调用)
    virtual bool OnCheckProc(Aura* /*aura*/, ProcExecutionData& /*data*/) const { return true; }
    // called before proc handler (执行触发效果前调用)
    virtual SpellAuraProcResult OnProc(Aura* /*aura*/, ProcExecutionData& /*procData*/) const { return SPELL_AURA_PROC_OK; }
    // called on absorb of this aura (护盾光环吸收伤害时调用)
    virtual void OnAbsorb(Aura* /*aura*/, int32& /*currentAbsorb*/, int32& /*remainingDamage*/, uint32& /*reflectedSpellId*/, int32& /*reflectDamage*/, bool& /*preventedDeath*/, bool& /*dropCharge*/, DamageEffectType /*damageType*/) const {}
    // called on mana shield absorb of this aura (法力护盾吸收伤害时调用)
    virtual void OnManaAbsorb(Aura* /*aura*/, int32& /*currentAbsorb*/) const {}
    // called on death prevention (阻止致死伤害生效后调用，如假死/春哥)
    virtual void OnAuraDeathPrevention(Aura* /*aura*/, int32& /*remainingDamage*/) const {}
    // called on aura dispel - warning - auras of holder might be deleted (光环被驱散时调用，注意 holder 下光环可能已被销毁)
    virtual void OnDispel(SpellAuraHolder* /*holder*/, Unit* /*dispeller*/, uint32 /*dispellingSpellId*/, uint32 /*originalStacks*/) const {}
    // called on periodic auras which need amount calculation (damage, heal, burn, drain) (周期性光环每跳计算伤害/治疗数值时调用)
    virtual void OnPeriodicCalculateAmount(Aura* /*aura*/, uint32& /*amount*/) const {}
    // called on periodic spell trigger (周期性光环触发法术时调用)
    virtual void OnPeriodicTrigger(Aura* /*aura*/, PeriodicTriggerData& /*data*/) const {}
    // called on periodic dummy (周期性虚拟光环每跳触发时调用)
    virtual void OnPeriodicDummy(Aura* /*aura*/) const {}
    // called on periodic tick end (周期性光环单跳结算结束后调用)
    virtual void OnPeriodicTickEnd(Aura* /*aura*/) const {}
    // called on persistent area aura dyngo lifetime end (持续地面区域光环动态对象销毁时调用)
    virtual void OnPersistentAreaAuraEnd(DynamicObject* /*dynGo*/) const {}
    // called on AreaAura target selection (持续地面区域光环筛选目标时调用)
    virtual bool OnPersistentAreaAuraCheckTarget(DynamicObject* /*dynGo*/, Unit* /*target*/) const { return true; }
    // called on unit heartbeat (光环心跳检测时调用)
    virtual void OnHeartbeat(Aura* /*aura*/) const {}
    // called on AreaAura target checking (区域光环检查目标合法性时调用)
    virtual bool OnAreaAuraCheckTarget(Aura const* /*aura*/, Unit* /*target*/) const { return true; }
    // called on affect check of aura - spellInfo can be nullptr in case of melee (判定光环是否影响指定法术或近战攻击时调用)
    virtual bool OnAffectCheck(Aura const* /*aura*/, SpellEntry const* /*spellInfo*/) const { return true; }
    // used to override SPELL_AURA_TRANSFORM or SPELL_AURA_MOD_SHAPESHIFT display id - more uses in future (用于覆盖变身或变形模型显示 ID)
    virtual uint32 GetAuraScriptCustomizationValue(Aura* /*aura*/) const { return 0; }
};

// 脚本自定义存储基类（供各脚本附加状态数据）
class ScriptStorage
{
    public:
        virtual ~ScriptStorage() {}
};

// 法术与光环脚本注册管理器
class SpellScriptMgr
{
    public:
        // 根据法术 ID 获取对应的法术脚本或光环脚本
        static SpellScript* GetSpellScript(uint32 spellId);
        static AuraScript* GetAuraScript(uint32 spellId);

        // 按脚本名注册法术/光环脚本实例
        static void SetSpellScript(std::string scriptName, SpellScript* script);
        static void SetAuraScript(std::string scriptName, AuraScript* script);

        // 加载所有职业脚本并从数据库 spell_scripts 表绑定法术 ID
        static void LoadScripts();
    private:
        static SpellScript* GetSpellScript(std::string scriptName);
        static AuraScript* GetAuraScript(std::string scriptName);

        static void SetSpellScript(uint32 spellId, SpellScript* script);
        static void SetAuraScript(uint32 spellId, AuraScript* script);

        static std::map<uint32, SpellScript*> m_spellScriptMap;             // 法术 ID -> SpellScript 映射
        static std::map<uint32, AuraScript*> m_auraScriptMap;               // 法术 ID -> AuraScript 映射
        static std::map<std::string, std::unique_ptr<SpellScript>> m_spellScriptStringMap; // 脚本名 -> SpellScript 所有权表
        static std::map<std::string, std::unique_ptr<AuraScript>> m_auraScriptStringMap;   // 脚本名 -> AuraScript 所有权表
};

// note - linux name mangling bugs out if two script templates have same class name - avoid it
// 注意：Linux 符号修饰要求不同脚本类不要重名。此模板根据基类类型自动注册 SpellScript 和/或 AuraScript

template <class T>
void RegisterSpellScript(std::string stringName)
{
    if constexpr (std::is_base_of<SpellScript, T>::value)
        SpellScriptMgr::SetSpellScript(stringName, new T());
    if constexpr (std::is_base_of<AuraScript, T>::value)
        SpellScriptMgr::SetAuraScript(stringName, new T());
}

#endif // SPELLSCRIPT_H
