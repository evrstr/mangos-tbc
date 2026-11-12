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
#ifndef MANGOS_SPELLAURAS_H
#define MANGOS_SPELLAURAS_H

#include "Spells/SpellAuraDefines.h"
#include "Server/DBCEnums.h"
#include "Entities/ObjectGuid.h"
#include "Spells/Scripts/SpellScript.h"
#include "Util/UniqueTrackablePtr.h"

/**
 * Used to modify what an Aura does to a player/npc.
 * Accessible through Aura::m_modifier.
 * 光环效果修饰器结构：定义光环对玩家/NPC 产生何种数值与周期修正（通过 Aura::m_modifier 访问）。
 * \see CreateAura
 * \see Aura
 * \see AreaAura
 * \see AuraType
 */
struct Modifier
{
    /**
     * Decides what the aura does, ie, it may have the
     * value AuraType::SPELL_AURA_MOD_BASE_RESISTANCE_PCT which
     * would change the base armor of a player.
     * 光环类型（决定光环具体作用，如修改基础护甲百分比等）
     */
    AuraType m_auraname;
    /**
     * By how much the aura should change the affected
     * value. Ie, -27 would make the value decided by Modifier::m_miscvalue
     * be reduced by 27% if the earlier mentioned AuraType
     * would have been used. And 27 would increase the value by 27%
     * 效果数值/幅度（正负值决定增减量或百分比）
     */
    int32 m_amount;
    /**
     * A miscvalue that is dependent on what the aura will do, this
     * is usually decided by the AuraType, ie:
     * with AuraType::SPELL_AURA_MOD_BASE_RESISTANCE_PCT this value
     * could be SpellSchoolMask::SPELL_SCHOOL_MASK_NORMAL which would
     * tell the aura that it should change armor.
     * If Modifier::m_auraname would have been AuraType::SPELL_AURA_MOUNTED
     * then m_miscvalue would have decided which model the mount should have
     * 附加杂项参数（含义随 AuraType 变化，例如法术学派掩码或坐骑生物模板 ID）
     */
    int32 m_miscvalue;
    /**
     * Decides how often the aura should be applied, if it is
     * set to 0 it's only applied once and then removed when
     * the Aura is removed
     * 周期触发间隔（毫秒）；为 0 表示非周期光环，仅在施加/移除时生效
     */
    uint32 periodictime;
    /**
    * Tells how much amount increased during recent stacks change
    * 最近一次层数变化导致的数值增量
    */
    int32 m_recentAmount;
    /**
    * Set at the beginning so that stack increases dont need to calculate spell values
    * 初始计算得到的单层基础数值（叠层时无需重新计算公式）
    */
    int32 m_baseAmount;
};

class Unit;
struct SpellEntry;
struct SpellModifier;
struct ProcTriggerSpell;

class Aura;

// internal helper（内部辅助结构，用于重算并重新应用受影响的被动光环）
struct ReapplyAffectedPassiveAurasHelper;

// 光环容器（SpellAuraHolder）生命周期状态
enum SpellAuraHolderState
{
    SPELLAURAHOLDER_STATE_CREATED       = 0,                // just created, initialization steps（刚创建，处于初始化阶段）
    SPELLAURAHOLDER_STATE_READY         = 1,                // all initialization steps are done（初始化完成，处于正常激活状态）
    SPELLAURAHOLDER_STATE_REMOVING      = 2,                // removing steps（正在执行移除清理流程）
    SPELLAURAHOLDER_STATE_DB_LOAD       = 3                 // during db load some events must not be executed（从数据库加载中，禁止执行部分运行时事件）
};

// 法术光环容器类：对应目标身上由单个法术施加的光环实例组（最多包含 3 个效果对应的 Aura 对象），统一管理持续时间、叠层、充能、客户端槽位与脚本钩子
class SpellAuraHolder
{
    public:
        SpellAuraHolder(SpellEntry const* spellproto, Unit* target, WorldObject* caster, Item* castItem, SpellEntry const* triggeredBy);
        ~SpellAuraHolder();
        Aura* m_auras[MAX_EFFECT_INDEX];

        void AddAura(Aura* aura, SpellEffectIndex index);
        void RemoveAura(SpellEffectIndex index);
        void ApplyAuraModifiers(bool apply, bool real = false);
        void _AddSpellAuraHolder();
        void _RemoveSpellAuraHolder();
        void HandleSpellSpecificBoosts(bool apply, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        void CleanupTriggeredSpells();

        void setDiminishGroup(DiminishingGroup group) { m_AuraDRGroup = group; }
        DiminishingGroup getDiminishGroup() const { return m_AuraDRGroup; }

        uint32 GetStackAmount() const { return m_stackAmount; }
        void SetStackAmount(uint32 stackAmount, Unit* newCaster);
        bool ModStackAmount(int32 num, Unit* newCaster); // return true if last charge dropped（增减层数，若最后一层耗尽则返回 true）

        Aura* GetAuraByEffectIndex(SpellEffectIndex index) const { return m_auras[index]; }
        SpellEntry const* GetTriggeredBy() const { return m_triggeredBy; }

        uint32 GetId() const { return m_spellProto->Id; }
        SpellEntry const* GetSpellProto() const { return m_spellProto; }
        SpellAuraHolderState GetState() const { return m_spellAuraHolderState; }
        void SetState(SpellAuraHolderState state) { m_spellAuraHolderState = state; }

        ObjectGuid const& GetCasterGuid() const { return m_casterGuid; }
        void SetCasterGuid(ObjectGuid guid) { m_casterGuid = guid; }
        ObjectGuid const& GetCastItemGuid() const { return m_castItemGuid; }
        Unit* GetCaster() const;
        Unit* GetTarget() const { return m_target; }
        void SetTarget(Unit* target) { m_target = target; }

        bool IsPermanent() const { return m_permanent; }
        void SetPermanent(bool permanent) { m_permanent = permanent; }
        bool IsPassive() const { return m_isPassive; }
        bool IsDeathPersistent() const { return m_isDeathPersist; }
        bool IsPersistent() const;
        bool IsPositive() const;
        bool IsAreaAura() const;                            // if one from auras of holder applied as area aura（容器内任意光环为区域光环时返回 true）
        bool IsWeaponBuffCoexistableWith(SpellAuraHolder const* ref) const;
        bool IsNeedVisibleSlot(Unit const* caster) const;
        bool IsRemovedOnShapeLost() const { return m_isRemovedOnShapeLost; }
        bool IsDeleted() const { return m_deleted;}
        bool IsEmptyHolder() const;
        bool IsSaveToDbHolder() const;
        bool IsCharm() const;

        void SetDeleted() { m_deleted = true; m_spellAuraHolderState = SPELLAURAHOLDER_STATE_REMOVING; }

        void UpdateHolder(uint32 diff) { Update(diff); }
        void Update(uint32 diff);
        void RefreshHolder();

        TrackedAuraType GetTrackedAuraType() const { return m_trackedAuraType; }
        void SetTrackedAuraType(TrackedAuraType val) { m_trackedAuraType = val; }
        void UnregisterAndCleanupTrackedAuras();

        int32 GetAuraMaxDuration() const { return m_maxDuration; }
        void SetAuraMaxDuration(int32 duration);
        int32 GetAuraDuration() const { return m_duration; }
        void SetAuraDuration(int32 duration) { m_duration = duration; }

        void SetHeartbeatResist(uint32 chance, int32 originalDuration, uint32 drLevel);

        uint8 GetAuraSlot() const { return m_auraSlot; }
        void SetAuraSlot(uint8 slot) { m_auraSlot = slot; }
        uint8 GetAuraLevel() const { return m_auraLevel; }
        void SetAuraLevel(uint8 level) { m_auraLevel = level; }
        uint32 GetAuraCharges() const { return m_procCharges; }
        void SetAuraCharges(uint32 charges, bool update = true);

        // SpellMods（法术修饰器充能管理）
        bool DropAuraCharge();                               // return true if last charge dropped（消耗一次充能，耗尽最后一次时返回 true）
        void ResetSpellModCharges();
        bool HasModifier(const uint64& modId) const;

        uint32 GetAuraApplyTime() const { return m_applyTime; }
        uint32 GetAuraApplyMSTime() const { return m_applyMSTime; } // milliseconds time（施加时的毫秒时间戳）

        void SetRemoveMode(AuraRemoveMode mode) { m_removeMode = mode; }
        void SetLoadedState(ObjectGuid const& casterGUID, ObjectGuid const& itemGUID, uint32 stackAmount, uint32 charges, int32 maxduration, int32 duration)
        {
            m_casterGuid   = casterGUID;
            m_castItemGuid = itemGUID;
            m_procCharges  = charges;
            m_stackAmount  = stackAmount;
            SetAuraMaxDuration(maxduration);
            SetAuraDuration(duration);
        }

        bool HasMechanic(uint32 mechanic) const;
        bool HasMechanicMask(uint32 mechanicMask) const;
        bool IsDispellableByMask(uint32 dispelMask, Unit const* caster, SpellEntry const* spellInfo) const;

        void UpdateAuraDuration();
        void LoginAuraDuration();
        void ForceUpdateAuraDuration();
        void SendAuraDuration();
        WorldPacket BuildAuraDurationToCaster(uint32 slot = MAX_AURAS);
        void SendAuraDurationToCaster(Player* caster, uint32 slot = MAX_AURAS);
        void SendAuraDurationToCasterNeedUpdate(Player* caster);

        void SetAura(uint32 slot, bool remove) { m_target->SetUInt32Value(UNIT_FIELD_AURA + slot, remove ? 0 : GetId()); }
        void SetAuraFlag(uint32 slot, bool add);
        void SetAuraLevel(uint32 slot, uint32 level);

        bool HasPeriodicAura() const;

        void SetCreationDelayFlag();

        bool HasAuraType(AuraType type);

        bool IsProcReady(TimePoint const& now) const;
        void SetProcCooldown(std::chrono::milliseconds cooldown, TimePoint const& now);

        bool IsReducedProcChancePast60() { return m_reducedProcChancePast60; }
        void SetReducedProcChancePast60() { m_reducedProcChancePast60 = true; }

        // Scripting system（光环脚本系统接口与回调钩子）
        AuraScript* GetAuraScript() const { return m_auraScript; }
        // hooks
        void OnHolderInit(WorldObject* caster);
        void OnDispel(Unit* dispeller, uint32 dispellingSpellId, uint32 originalStacks);
        // helpers
        void PresetAuraStacks(uint32 stacks) { m_stackAmount = stacks; } // use only in OnHolderInit（仅限在 OnHolderInit 中预设初始层数）
    private:
        void UpdateAuraApplication();                       // called at charges or stack changes（在充能或叠层变化时同步更新客户端显示）
        void ClearExtraAuraInfo(Unit* caster);
        void UpdateHeartbeatResist(uint32 diff);

        SpellEntry const* m_spellProto;

        Unit* m_target;
        ObjectGuid m_casterGuid;
        ObjectGuid m_castItemGuid;                          // it is NOT safe to keep a pointer to the item because it may get deleted（不可保存 Item 指针以防物品被销毁产生悬垂指针，仅存 GUID）
        time_t m_applyTime;
        uint32 m_applyMSTime;
        SpellEntry const* m_triggeredBy;                    // Spell responsible for this holder（触发产生此光环容器的父法术原型）
        SpellAuraHolderState m_spellAuraHolderState;        // State used to be sure init part is finished (ex there is still some aura to add or effect to process)（容器生命周期状态，确保初始化完成后再触发回调）

        uint8 m_auraSlot;                                   // Aura slot on unit (for show in client)（单位身上的客户端可见光环槽位索引）
        uint8 m_auraLevel;                                  // Aura level (store caster level for correct show level dep amount)（光环等级，记录施法者等级以正确显示随等级变化的数值）
        uint32 m_procCharges;                               // Aura charges (0 for infinite)（光环剩余触发充能次数，0 表示无限次）
        uint32 m_stackAmount;                               // Aura stack amount（当前叠加层数）
        int32 m_maxDuration;                                // Max aura duration（最大持续时间，毫秒）
        int32 m_duration;                                   // Current time（当前剩余持续时间，毫秒）
        int32 m_timeCla;                                    // Timer for power per sec calculation（引导/持续消耗类光环的每秒扣蓝计时器）

        float m_heartbeatResistChance;                      // Chance to break this spell due to heartbeat resistance（心跳抵抗触发几率）
        int32 m_heartbeatResistInterval;                    // Heartbeat resistance periodic interval（心跳抵抗检测周期）
        int32 m_heartbeatResistTimer;                       // Timer for heartbeat resistance（心跳抵抗倒计时）

        AuraRemoveMode m_removeMode: 8;                     // Store info for know remove aura reason（记录光环移除原因）
        DiminishingGroup m_AuraDRGroup: 8;                  // Diminishing（所属控制技能递减分组）
        TrackedAuraType m_trackedAuraType: 8;               // store if the caster tracks the aura - can change at spell steal for example（施法者对该光环的追踪类型，如法术窃取后会变更）

        bool m_permanent: 1;
        bool m_isPassive: 1;
        bool m_isDeathPersist: 1;
        bool m_isRemovedOnShapeLost: 1;
        bool m_deleted: 1;
        bool m_skipUpdate: 1;

        TimePoint m_procCooldown;

        bool m_reducedProcChancePast60;

        // Scripting System（关联的光环脚本实例）
        AuraScript* m_auraScript;
};

typedef void(Aura::*pAuraHandler)(bool Apply, bool Real);
// Real == true at aura add/remove
// Real == false at aura mod unapply/reapply; when adding/removing dependent aura/item/stat mods
// （Real == true 表示光环真正添加或移除；Real == false 表示因重算属性/装备/依赖光环而临时卸载并重新应用修饰器）
//
// Code in aura handler can be guarded by if(Real) check if it should execution only at real add/remove of aura
// （仅需在光环真正施加或移除时执行的逻辑应放在 if(Real) 保护内）
//
// MAIN RULE: Code MUST NOT be guarded by if(Real) check if it modifies any stats
//      (percent auras, stats mods, etc)
// （主规则：修改任何属性数值或百分比修正的代码严禁放入 if(Real) 判断中，否则属性重算时会丢失）
// Second rule: Code must be guarded by if(Real) check if it modifies object state (start/stop attack, send packets to client, etc)
// （第二规则：修改单位状态、启停攻击或向客户端发包的代码必须放入 if(Real) 判断中）
//
// Other case choice: each code line moved under if(Real) check is mangos speedup,
//      each setting object update field code line moved under if(Real) check is significant mangos speedup, and less server->client data sends
//      each packet sending code moved under if(Real) check is _large_ mangos speedup, and lot less server->client data sends

// 单个光环效果实例类：对应法术的某一个效果索引，负责具体 AuraType 处理器分发、周期跳动及脚本钩子调用
class Aura
{
        friend struct ReapplyAffectedPassiveAurasHelper;
        friend Aura* CreateAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32 const* currentDamage, int32 const* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster, Item* castItem, uint64 scriptValue);

    public:
        // aura handlers（光环类型处理器声明，对应 SpellAuras.cpp 中的 AuraHandler 表）
        void HandleNULL(bool, bool)
        {
            // NOT IMPLEMENTED
        }
        void HandleUnused(bool, bool)
        {
            // NOT USED BY ANY SPELL OR USELESS
        }
        void HandleNoImmediateEffect(bool, bool)
        {
            // aura not have immediate effect at add/remove and handled by ID in other code place
        }
        void HandleBindSight(bool apply, bool Real);
        void HandleModPossess(bool apply, bool Real);
        void HandlePeriodicDamage(bool apply, bool Real);
        void HandleAuraDummy(bool apply, bool Real);
        void HandleAuraPeriodicDummy(bool apply, bool Real);
        void HandleModConfuse(bool apply, bool Real);
        void HandleModCharm(bool apply, bool Real);
        void HandleModFear(bool apply, bool Real);
        void HandlePeriodicHeal(bool apply, bool Real);
        void HandleModAttackSpeed(bool apply, bool Real);
        void HandleModMeleeRangedSpeedPct(bool apply, bool Real);
        void HandleModCombatSpeedPct(bool apply, bool Real);
        void HandleModThreat(bool apply, bool Real);
        void HandleModTaunt(bool Apply, bool Real);
        void HandleFeignDeath(bool apply, bool Real);
        void HandleAuraModDisarm(bool apply, bool Real);
        void HandleAuraModStalked(bool apply, bool Real);
        void HandleAuraWaterWalk(bool apply, bool Real);
        void HandleAuraFeatherFall(bool apply, bool Real);
        void HandleAuraHover(bool apply, bool Real);
        void HandleAddModifier(bool apply, bool Real);
        void HandleAuraModStun(bool apply, bool Real);
        void HandleModDamageDone(bool apply, bool Real);
        void HandleAuraUntrackable(bool apply, bool Real);
        void HandleAuraEmpathy(bool apply, bool Real);
        void HandleModOffhandDamagePercent(bool apply, bool Real);
        void HandleAuraModRangedAttackPower(bool apply, bool Real);
        void HandleAuraModIncreaseEnergyPercent(bool apply, bool Real);
        void HandleAuraModIncreaseHealthPercent(bool apply, bool Real);
        void HandleAuraModRegenInterrupt(bool apply, bool Real);
        void HandleModMeleeSpeedPct(bool apply, bool Real);
        void HandlePeriodicTriggerSpell(bool apply, bool Real);
        void HandlePeriodicTriggerSpellWithValue(bool apply, bool Real);
        void HandlePeriodicEnergize(bool apply, bool Real);
        void HandleAuraModResistanceExclusive(bool apply, bool Real);
        void HandleAuraSafeFall(bool Apply, bool Real);
        void HandleModStealth(bool apply, bool Real);
        void HandleModStealthDetect(bool apply, bool real);
        void HandleInvisibility(bool apply, bool Real);
        void HandleInvisibilityDetect(bool apply, bool Real);
        void HandleAuraModTotalHealthPercentRegen(bool apply, bool Real);
        void HandleAuraModTotalManaPercentRegen(bool apply, bool Real);
        void HandleAuraModResistance(bool apply, bool Real);
        void HandleAuraModRoot(bool apply, bool Real);
        void HandleAuraModSilence(bool apply, bool Real);
        void HandleAuraModStat(bool apply, bool Real);
        void HandleDetectAmore(bool apply, bool Real);
        void HandleAuraModIncreaseSpeed(bool Apply, bool Real);
        void HandleAuraModIncreaseMountedSpeed(bool apply, bool Real);
        void HandleAuraModIncreaseFlightSpeed(bool apply, bool Real);
        void HandleAuraModDecreaseSpeed(bool apply, bool Real);
        void HandleAuraModUseNormalSpeed(bool Apply, bool Real);
        void HandleAuraModIncreaseHealth(bool apply, bool Real);
        void HandleAuraModIncreaseEnergy(bool apply, bool Real);
        void HandleAuraModShapeshift(bool apply, bool Real);
        void HandleAuraModEffectImmunity(bool apply, bool Real);
        void HandleAuraModStateImmunity(bool apply, bool Real);
        void HandleAuraModSchoolImmunity(bool apply, bool Real);
        void HandleAuraModDmgImmunity(bool apply, bool Real);
        void HandleAuraModDispelImmunity(bool apply, bool Real);
        void HandleAuraProcTriggerSpell(bool apply, bool Real);
        void HandleAuraTrackCreatures(bool apply, bool Real);
        void HandleAuraTrackResources(bool apply, bool Real);
        void HandleAuraModParryPercent(bool apply, bool Real);
        void HandleAuraModDodgePercent(bool apply, bool Real);
        void HandleAuraModBlockPercent(bool apply, bool Real);
        void HandleAuraModCritPercent(bool apply, bool Real);
        void HandlePeriodicLeech(bool apply, bool Real);
        void HandleModHitChance(bool apply, bool Real);
        void HandleModSpellHitChance(bool apply, bool Real);
        void HandleAuraModScale(bool apply, bool Real);
        void HandlePeriodicManaLeech(bool apply, bool Real);
        void HandlePeriodicHealthFunnel(bool apply, bool Real);
        void HandleModCastingSpeed(bool apply, bool Real);
        void HandleAuraMounted(bool apply, bool Real);
        void HandleWaterBreathing(bool apply, bool real);
        void HandleModStealthLevel(bool apply, bool real);
        void HandleModWaterBreathing(bool apply, bool Real);
        void HandleModBaseResistance(bool apply, bool Real);
        void HandleModRegen(bool apply, bool Real);
        void HandleModPowerRegen(bool apply, bool Real);
        void HandleModPowerRegenPCT(bool Apply, bool Real);
        void HandleModHealingPCT(bool apply, bool real);
        void HandleChannelDeathItem(bool apply, bool Real);
        void HandlePeriodicDamagePCT(bool apply, bool Real);
        void HandleAuraModAttackPower(bool apply, bool Real);
        void HandleAuraTransform(bool apply, bool Real);
        void HandleModSpellCritChance(bool apply, bool Real);
        void HandleAuraModIncreaseSwimSpeed(bool Apply, bool Real);
        void HandleModPowerCostPCT(bool apply, bool Real);
        void HandleModPowerCost(bool apply, bool Real);
        void HandleFarSight(bool apply, bool Real);
        void HandleModPossessPet(bool apply, bool Real);
        void HandleModMechanicImmunity(bool apply, bool Real);
        void HandleModMechanicImmunityMask(bool apply, bool Real);
        void HandleAuraModSkill(bool apply, bool Real);
        void HandleModDamagePercentDone(bool apply, bool Real);
        void HandleModPercentStat(bool apply, bool Real);
        void HandleModResistancePercent(bool apply, bool Real);
        void HandleAuraModBaseResistancePercent(bool apply, bool Real);
        void HandleAuraTrackStealthed(bool apply, bool Real);
        void HandleForceReaction(bool apply, bool Real);
        void HandleAuraModRangedHaste(bool apply, bool Real);
        void HandleRangedAmmoHaste(bool apply, bool Real);
        void HandleModHealingDone(bool Apply, bool Real);
        void HandleModTotalPercentStat(bool apply, bool Real);
        void HandleAuraModTotalThreat(bool apply, bool Real);
        void HandleModUnattackable(bool Apply, bool Real);
        void HandleAuraModPacify(bool apply, bool Real);
        void HandleAuraGhost(bool apply, bool Real);
        void HandleAuraAllowFlight(bool apply, bool Real);
        void HandleModRating(bool apply, bool Real);
        void HandleModTargetResistance(bool apply, bool Real);
        void HandleAuraModAttackPowerPercent(bool apply, bool Real);
        void HandleAuraModRangedAttackPowerPercent(bool apply, bool Real);
        void HandleAuraModRangedAttackPowerOfStatPercent(bool apply, bool Real);
        void HandleSpiritOfRedemption(bool apply, bool Real);
        void HandleAoECharm(bool apply, bool Real);
        void HandleModManaRegen(bool apply, bool Real);
        void HandleComprehendLanguage(bool apply, bool Real);
        void HandleShieldBlockValue(bool apply, bool Real);
        void HandleModSpellCritChanceShool(bool apply, bool Real);
        void HandleAuraRetainComboPoints(bool apply, bool Real);
        void HandleModSpellDamagePercentFromStat(bool apply, bool Real);
        void HandleModSpellHealingPercentFromStat(bool apply, bool Real);
        void HandleAuraModDispelResist(bool apply, bool Real);
        void HandleModSpellDamagePercentFromAttackPower(bool apply, bool Real);
        void HandleModSpellHealingPercentFromAttackPower(bool apply, bool Real);
        void HandleAuraModPacifyAndSilence(bool apply, bool Real);
        void HandleAuraModIncreaseMaxHealth(bool apply, bool Real);
        void HandleAuraModExpertise(bool apply, bool Real);
        void HandleForceMoveForward(bool apply, bool Real);
        void HandleAuraModResistenceOfStatPercent(bool apply, bool Real);
        void HandleAuraPowerBurn(bool apply, bool Real);
        void HandleSchoolAbsorb(bool apply, bool Real);
        void HandlePreventFleeing(bool apply, bool Real);
        void HandleManaShield(bool apply, bool Real);
        void HandleArenaPreparation(bool apply, bool Real);
        void HandleAuraMirrorImage(bool apply, bool Real);
        void HandleFactionOverride(bool apply, bool Real);
        void HandlePrayerOfMending(bool apply, bool Real);
        void HandleAuraDetaunt(bool Apply, bool Real);
        void HandleOverrideClassScript(bool apply, bool real);
        void HandleAuraPhase(bool apply, bool real);

        virtual ~Aura();

        void SetModifier(AuraType type, int32 amount, uint32 periodicTime, int32 miscValue);
        Modifier*       GetModifier()       { return &m_modifier; }
        Modifier const* GetModifier() const { return &m_modifier; }
        int32 GetMiscValue() const { return m_spellAuraHolder->GetSpellProto()->EffectMiscValue[m_effIndex]; }
        int32 GetMiscBValue() const { return m_spellAuraHolder->GetSpellProto()->EffectMiscValueB[m_effIndex]; }

        SpellEntry const* GetSpellProto() const { return GetHolder()->GetSpellProto(); }
        uint32 GetId() const { return GetHolder()->GetSpellProto()->Id; }
        ObjectGuid const& GetCastItemGuid() const { return GetHolder()->GetCastItemGuid(); }
        ObjectGuid const& GetCasterGuid() const { return GetHolder()->GetCasterGuid(); }
        Unit* GetCaster() const { return GetHolder()->GetCaster(); }
        Unit* GetTarget() const { return GetHolder()->GetTarget(); }

        SpellEffectIndex GetEffIndex() const { return m_effIndex; }
        int32 GetBasePoints() const { return m_currentBasePoints; }
        int32 GetAmount() const { return m_modifier.m_amount; }
        void SetAmount(int32 amount) { m_modifier.m_amount = amount; }

        int32 GetAuraMaxDuration() const { return GetHolder()->GetAuraMaxDuration(); }
        int32 GetAuraDuration() const { return GetHolder()->GetAuraDuration(); }
        time_t GetAuraApplyTime() const { return m_applyTime; }
        uint32 GetAuraTicks() const { return m_periodicTick; }
        uint32 GetAuraMaxTicks() const
        {
            int32 maxDuration = GetAuraMaxDuration();
            return maxDuration > 0 && m_modifier.periodictime > 0 ? maxDuration / m_modifier.periodictime : 0;
        }
        uint32 GetStackAmount() const { return GetHolder()->GetStackAmount(); }

        void SetLoadedState(int32 damage, uint32 periodicTime)
        {
            m_modifier.m_amount = damage;
            m_modifier.periodictime = periodicTime;

            if (uint32 maxticks = GetAuraMaxTicks())
                m_periodicTick = maxticks - GetAuraDuration() / m_modifier.periodictime;
        }

        bool IsPositive() const { return m_positive; }
        bool IsPersistent() const { return m_isPersistent; }
        bool IsAreaAura() const { return m_isAreaAura; }
        bool IsPeriodic() const { return m_isPeriodic; }
        bool IsSaveToDbAura() const;

        void ApplyModifier(bool apply, bool Real = false);
        void UpdateAuraScaling();

        void UpdateAura(uint32 diff) { Update(diff); }

        void SetRemoveMode(AuraRemoveMode mode) { m_removeMode = mode; }
        AuraRemoveMode GetRemoveMode() { return m_removeMode; }

        virtual Unit* GetTriggerTarget() const { return m_spellAuraHolder->GetTarget(); }

        // add/remove SPELL_AURA_MOD_SHAPESHIFT (36) linked auras（添加/移除变形姿态关联的额外被动光环）
        void HandleShapeshiftBoosts(bool apply);

        void TriggerSpell();
        void TriggerSpellWithValue();

        // more limited that used in future versions (spell_affect table based only), so need be careful with backporting uses（基于 spell_affect 表的法术影响掩码，向后移植时需注意适用范围）
        ClassFamilyMask GetAuraSpellClassMask() const;
        bool isAffectedOnSpell(SpellEntry const* spell) const;
        bool CanProcFrom(SpellEntry const* spell, uint32 EventProcEx, uint32 procEx, bool damaging, bool absorbing, bool useClassMask) const;

        SpellAuraHolder* GetHolder() { return m_spellAuraHolder; }
        SpellAuraHolder const* GetHolder() const { return m_spellAuraHolder; }

        bool IsLastAuraOnHolder();

        bool HasMechanic(uint32 mechanic) const;

        SpellModifier* GetSpellModifier() { return m_spellmod; }

        void UseMagnet() { m_magnetUsed = true; }
        bool IsMagnetUsed() const { return m_magnetUsed; }

        static uint32 CalculateAuraEffectValue(Unit* caster, Unit* target, SpellEntry const* spellProto, SpellEffectIndex effIdx, uint32 value);

        // Scripting system（光环脚本系统接口与事件钩子）
        AuraScript* GetAuraScript() const { return GetHolder()->GetAuraScript(); }
        // Variable storage（脚本状态与自定义存储指针）
        void SetScriptValue(uint64 value) { m_scriptValue = value; }
        uint64 GetScriptValue() { return m_scriptValue; }
        void SetScriptStorage(ScriptStorage* storage) { m_storage = storage; } // do not set more than once（仅允许设置一次）
        ScriptStorage* GetScriptStorage() { return m_storage; }
        // hooks（光环初始化、数值计算、应用/移除、触发、吸收及周期跳动钩子）
        void OnAuraInit();
        int32 OnAuraValueCalculate(Unit* caster, int32 currentValue, Item* castItem);
        void OnDamageCalculate(Unit* attacker, Unit* victim, int32& advertisedBenefit, float& totalMod);
        void OnCritChanceCalculate(Unit const* victim, float& chance, SpellEntry const* spellInfo);
        void OnApply(bool apply);
        void OnAfterApply(bool apply);
        bool OnCheckProc(ProcExecutionData& data);
        SpellAuraProcResult OnProc(ProcExecutionData& data);
        void OnAbsorb(int32& currentAbsorb, int32& remainingDamage, uint32& reflectedSpellId, int32& reflectDamage, bool& preventedDeath, bool& dropCharge, DamageEffectType damageType);
        void OnManaAbsorb(int32& currentAbsorb);
        void OnAuraDeathPrevention(int32& remainingDamage);
        void OnPeriodicTrigger(PeriodicTriggerData& data);
        void OnPeriodicDummy();
        void OnPeriodicTickEnd();
        void OnPeriodicCalculateAmount(uint32& amount);
        void OnHeartbeat();
        bool OnAffectCheck(SpellEntry const* spellInfo) const;
        uint32 GetAuraScriptCustomizationValue();
        // Hook Requirements（供脚本覆写周期或影响条件）
        void ForcePeriodicity(uint32 periodicTime);
        void SetAffectOverriden() { m_affectOverriden = true; } // spell script must implement condition（启用后脚本必须实现 OnAffectCheck）

        MaNGOS::unique_weak_ptr<Aura> GetWeakPtr() const { return m_scriptRef; }
        void InvalidateScriptRef() { m_scriptRef = nullptr; }

    protected:
        Aura(SpellEntry const* spellproto, SpellEffectIndex eff, int32 const* currentDamage, int32 const* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster = nullptr, Item* castItem = nullptr);

        // must be called only from Aura::UpdateAura（仅限由 Aura::UpdateAura 调用）
        virtual void Update(uint32 diff);

        // must be called only from Aura*::Update（仅限由 Aura 及其子类的 Update 调用）
        void PeriodicTick();
        void PeriodicDummyTick();

        void ReapplyAffectedPassiveAuras();

        void PickTargetsForSpellTrigger(Unit *& triggerCaster, Unit *& triggerTarget, WorldObject *& triggerTargetObject, SpellEntry const* spellInfo);
        void CastTriggeredSpell(PeriodicTriggerData& data);

        Modifier m_modifier;
        SpellModifier* m_spellmod;

        time_t m_applyTime;

        int32 m_currentBasePoints;                          // cache SpellEntry::CalculateSimpleValue and use for set custom base points（缓存基础点数，支持自定义覆盖）
        int32 m_periodicTimer;                              // Timer for periodic auras（周期光环下一次触发倒计时）
        uint32 m_periodicTick;                              // Tick count pass (including current if use in tick code) from aura apply, used for some tick count dependent aura effects（已执行的周期跳数计数）

        AuraRemoveMode m_removeMode: 8;                     // Store info for know remove aura reason（记录移除原因）

        SpellEffectIndex m_effIndex : 8;                    // Aura effect index in spell（所属法术的效果索引 0..2）

        bool m_positive: 1;
        bool m_isPeriodic: 1;
        bool m_isAreaAura: 1;
        bool m_isPersistent: 1;
        bool m_magnetUsed: 1;

        SpellAuraHolder* const m_spellAuraHolder;

        // Scripting system（脚本状态存储与弱引用追踪指针）
        uint64 m_scriptValue; // persistent value for spell script state
        ScriptStorage* m_storage;
        bool m_affectOverriden;

        struct NoopAuraDeleter { void operator()(Aura*) const { /*noop - not managed*/ } };
        MaNGOS::unique_trackable_ptr<Aura> m_scriptRef;
    private:
        void ReapplyAffectedPassiveAuras(Unit* target, bool owner_mode);
};

// 区域光环类：由施法者身上持续向周围队伍/团员/友方/敌方或图腾半径内目标扩散的光环（如圣骑士光环、图腾光环）
class AreaAura : public Aura
{
    public:
        AreaAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32 const* currentDamage, int32 const* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster = nullptr, Item* castItem = nullptr, uint32 originalRankSpellId = 0);
        virtual ~AreaAura() override;

        bool OnAreaAuraCheckTarget(Unit* target) const;
    protected:
        void Update(uint32 diff) override;
    private:
        float m_radius;
        AreaAuraType m_areaAuraType;
        uint32       m_originalRankSpellId;
};

// 持久区域光环类：挂载在 DynamicObject（地面法阵/暴风雪/奉献等）上周期作用的光环
class PersistentAreaAura : public Aura
{
    public:
        PersistentAreaAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32 const* currentDamage, int32 const* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster = nullptr, Item* castItem = nullptr);
        virtual ~PersistentAreaAura() override;
    protected:
        void Update(uint32 diff) override;
};

// Used for GO Area Auras（游戏对象区域光环类：由 GameObject 持续向范围内单位施加光环）
class GameObjectAura : public Aura
{
    public:
        GameObjectAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32 const* currentDamage, int32 const* currentBasePoints, SpellAuraHolder* holder, Unit* target, GameObject* caster);
        virtual ~GameObjectAura() override;

    protected:
        void Update(uint32 diff) override;
};

// 工厂函数：按法术与效果索引创建对应的 Aura/AreaAura/PersistentAreaAura 实例
Aura* CreateAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32 const* currentDamage, int32 const* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster = nullptr, Item* castItem = nullptr, uint64 scriptValue = 0);
// 工厂函数：创建并初始化 SpellAuraHolder 光环容器
SpellAuraHolder* CreateSpellAuraHolder(SpellEntry const* spellproto, Unit* target, WorldObject* caster, Item* castItem = nullptr, SpellEntry const* triggeredBy = nullptr);
#endif
