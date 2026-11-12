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

#ifndef __SPELL_H
#define __SPELL_H

#include "Common.h"
#include "Maps/GridDefines.h"
#include "Globals/SharedDefines.h"
#include "Server/DBCEnums.h"
#include "Entities/ObjectGuid.h"
#include "Entities/Unit.h"
#include "Entities/Player.h"
#include "Server/SQLStorages.h"
#include "Spells/SpellEffectDefines.h"
#include "Util/UniqueTrackablePtr.h"

class WorldSession;
class WorldPacket;
class DynamicObj;
class Item;
class GameObject;
class Group;
class Aura;
struct SpellTargetEntry;
struct SpellScript;
struct AuraScript;
struct SpellTargetingData;

// 法术施放标志位（用于 SMSG_SPELL_START / SMSG_SPELL_GO 数据包控制客户端表现与战斗记录行为）
enum SpellCastFlags
{
    CAST_FLAG_NONE              = 0x00000000,               // 无特殊施法标志
    CAST_FLAG_HIDDEN_COMBATLOG  = 0x00000001,               // hide in combat log?（是否在战斗记录中隐藏）
    CAST_FLAG_UNKNOWN2          = 0x00000002,               // Sent on SMSG_SPELL_START（随 SMSG_SPELL_START 发送）
    CAST_FLAG_UNKNOWN3          = 0x00000004,
    CAST_FLAG_UNKNOWN4          = 0x00000008,
    CAST_FLAG_PERSISTENT_AA     = 0x00000010,               // Spell has Persistent AA effect（法术包含持续性区域光环效果）
    CAST_FLAG_AMMO              = 0x00000020,               // Projectiles visual（显示弹药/投射物视觉效果）
    CAST_FLAG_UNKNOWN7          = 0x00000040,               // Item related?（可能与物品相关）
    CAST_FLAG_UNKNOWN8          = 0x00000080,
    CAST_FLAG_UNKNOWN9          = 0x00000100,               // Sent on SMSG_SPELL_GO（随 SMSG_SPELL_GO 发送）
    CAST_FLAG_UNKNOWN10         = 0x00000200,
    CAST_FLAG_UNKNOWN11         = 0x00000400,
    CAST_FLAG_PREDICTED_POWER   = 0x00000800,               // wotlk, trigger rune cooldown（WLK 特性，触发符文冷却）
    CAST_FLAG_UNKNOWN13         = 0x00001000,
    CAST_FLAG_UNKNOWN14         = 0x00002000,
    CAST_FLAG_UNKNOWN15         = 0x00004000,
    CAST_FLAG_UNKNOWN16         = 0x00008000,               // Something with Portal effects（与传送门类效果相关）
};

// 区域法术目标搜索中心与扩散形状类型
enum SpellNotifyPushType
{
    PUSH_CONE,                                              // 锥形扇面扩散（以施法者为顶点）
    PUSH_SELF_CENTER,                                       // 以施法者自身为圆心
    PUSH_SRC_CENTER,                                        // 以法术源坐标（Source）为圆心
    PUSH_DEST_CENTER,                                       // 以法术目标坐标（Destination）为圆心
    PUSH_TARGET_CENTER                                      // 以主目标单位为圆心
};

// 法术目标检查例外类型
enum class CheckException
{
    EXCEPTION_NONE,                                         // 无例外
    EXCEPTION_MAGNET,                                       // 磁力/避雷针吸收例外（如根基图腾）
};

bool IsQuestTameSpell(uint32 spellId);                      // 判断法术是否为任务驯服类法术

namespace MaNGOS
{
    struct SpellNotifierPlayer;
    struct SpellNotifierCreatureAndPlayer;
}

class SpellCastTargets;

// 法术施放目标反序列化辅助器（绑定目标容器与施法者用于流读取）
struct SpellCastTargetsReader
{
    explicit SpellCastTargetsReader(SpellCastTargets& _targets, Unit* _caster) : targets(_targets), caster(_caster) {}

    SpellCastTargets& targets;
    Unit* caster;
};

// 法术施放目标容器：封装单位、物品、游戏对象、尸体、源/目标坐标及目标掩码
class SpellCastTargets
{
    public:
        SpellCastTargets();
        ~SpellCastTargets();

        void read(ByteBuffer& data, Unit* caster);          // 从客户端数据包反序列化施法目标
        void write(ByteBuffer& data) const;                 // 将施法目标序列化写入数据包

        SpellCastTargetsReader ReadForCaster(Unit* caster) { return SpellCastTargetsReader(*this, caster); }

        SpellCastTargets& operator=(const SpellCastTargets& target)
        {
            m_unitTarget = target.m_unitTarget;
            m_itemTarget = target.m_itemTarget;
            m_GOTarget   = target.m_GOTarget;
            m_CorpseTarget = target.m_CorpseTarget;

            m_unitTargetGUID    = target.m_unitTargetGUID;
            m_GOTargetGUID      = target.m_GOTargetGUID;
            m_CorpseTargetGUID  = target.m_CorpseTargetGUID;
            m_itemTargetGUID    = target.m_itemTargetGUID;

            m_itemTargetEntry  = target.m_itemTargetEntry;

            m_srcPos = target.m_srcPos;
            m_destPos = target.m_destPos;

            m_strTarget = target.m_strTarget;

            m_targetMask = target.m_targetMask;

            m_mapId = target.m_mapId;

            return *this;
        }

        void setUnitTarget(Unit* target);
        ObjectGuid getUnitTargetGuid() const { return m_unitTargetGUID; }
        Unit* getUnitTarget() const { return m_unitTarget; }

        void setDestination(float x, float y, float z);
        void setDestination(Position position);
        void setSource(float x, float y, float z);
        void getDestination(float& x, float& y, float& z) const { x = m_destPos.x; y = m_destPos.y; z = m_destPos.z; }
        Position getDestination() const { return m_destPos; }
        void getDestination(WorldLocation& loc) { loc.coord_x = m_destPos.x; loc.coord_y = m_destPos.y; loc.coord_z = m_destPos.z; }
        void getSource(float& x, float& y, float& z) const { x = m_srcPos.x; y = m_srcPos.y; z = m_srcPos.z; }
        Position getSource() const { return m_srcPos; }

        void setGOTarget(GameObject* target);
        ObjectGuid getGOTargetGuid() const { return m_GOTargetGUID; }
        GameObject* getGOTarget() const { return m_GOTarget; }

        void setCorpseTarget(Corpse* corpse);
        ObjectGuid getCorpseTargetGuid() const { return m_CorpseTargetGUID; }
        Corpse* getCorpseTarget() const { return m_CorpseTarget; }

        void setItemTarget(Item* item);
        void clearItemPointer();
        ObjectGuid getItemTargetGuid() const { return m_itemTargetGUID; }
        Item* getItemTarget() const { return m_itemTarget; }
        uint32 getItemTargetEntry() const { return m_itemTargetEntry; }

        void setTradeItemTarget(Player* caster);

        void updateTradeSlotItem()
        {
            if (m_itemTarget && (m_targetMask & TARGET_FLAG_TRADE_ITEM))
            {
                m_itemTargetGUID = m_itemTarget->GetObjectGuid();
                m_itemTargetEntry = m_itemTarget->GetEntry();
            }
        }

        bool IsEmpty() const { return !m_GOTargetGUID && !m_unitTargetGUID && !m_itemTarget && !m_CorpseTargetGUID; }

        void Update(WorldObject* caster);

        Position m_srcPos;
        Position m_destPos;
        std::string m_strTarget;

        uint32 m_targetMask;

        uint32 m_mapId; // not to be written to packet in vanilla/tbc（经典旧世/TBC 数据包中不写入地图 ID）

    private:
        // objects (can be used at spell creating and after Update at casting)（目标对象指针，在创建法术和 Update 后施法阶段使用）
        Unit* m_unitTarget;
        GameObject* m_GOTarget;
        Item* m_itemTarget;
        Corpse* m_CorpseTarget;

        // object GUID/etc, can be used always（目标对象的 GUID 等持久化标识，随时可用）
        ObjectGuid m_unitTargetGUID;
        ObjectGuid m_GOTargetGUID;
        ObjectGuid m_CorpseTargetGUID;
        ObjectGuid m_itemTargetGUID;
        uint32 m_itemTargetEntry;
};

inline ByteBuffer& operator<< (ByteBuffer& buf, SpellCastTargets const& targets)
{
    targets.write(buf);
    return buf;
}

inline ByteBuffer& operator>> (ByteBuffer& buf, SpellCastTargetsReader const& targets)
{
    targets.targets.read(buf, targets.caster);
    return buf;
}

// 法术实例生命周期状态枚举
enum SpellState
{
    SPELL_STATE_CREATED    = 0,                             // just created（刚创建）
    SPELL_STATE_TARGETING  = 1,                             // doing initial check（正在执行初始目标与施法条件检查）
    SPELL_STATE_CASTING    = 2,                             // cast time delay period, non channeled spell（非引导法术的读条吟唱阶段）
    SPELL_STATE_DELAYED    = 3,                             // spell is delayed (cast time pushed back) TODO: need to be implemented properly（法术被推迟/打断退条，待完善）
    SPELL_STATE_TRAVELING  = 4,                             // spell casted but need time to hit target(s)（法术已出手，弹道飞行中等待命中）
    SPELL_STATE_LANDING    = 5,                             // processing the effects（正在结算命中目标与法术效果）
    SPELL_STATE_CHANNELING = 6,                             // channeled time period spell casting state（引导类法术的持续引导阶段）
    SPELL_STATE_FINISHED   = 7,                             // cast finished to success or fail（施放成功或失败并已结束）
};

// 区域/链式搜索的目标阵营过滤类型
enum SpellTargets
{
    SPELL_TARGETS_ASSISTABLE,                               // 可协助的友方目标
    SPELL_TARGETS_AOE_ATTACKABLE,                           // 可攻击的 AOE 敌方目标
    SPELL_TARGETS_CHAIN_ATTACKABLE,                         // 可攻击的链式弹跳目标
    SPELL_TARGETS_ALL                                       // 所有符合条件的目标（不限敌友）
};

typedef std::multimap<uint64, uint64> SpellTargetTimeMap;

// SpellLog class to manage spells logs that have to be sent to clients
// 法术战斗记录构建器：汇总各效果命中目标并发送 SMSG_SPELLLOGEXECUTE 到客户端
class SpellLog
{
    public:
        SpellLog(Spell* spell) :
            m_spell(spell), m_spellLogDataEffectsCounterPos(0), m_spellLogDataEffectsCounter(0),
            m_spellLogDataTargetsCounterPos(0), m_spellLogDataTargetsCounter(0), m_currentEffect(MAX_SPELL_EFFECTS) {}
        SpellLog() = delete;
        SpellLog(const SpellLog&) = delete;

        void Initialize();

        // Variadic template to add log data (warnings, devs should respect the correct packet structure)
        // 可变参数模板：追加单条效果日志数据（调用方需严格遵守数据包字段顺序）
        template<typename... Args>
        void AddLog(uint32 spellEffect, Args... args)
        {
            SetCurrentEffect(spellEffect);
            AddLogData(args...);
            ++m_spellLogDataEffectsCounter;
        }

        // Send collected logs（发送已收集的法术效果日志给周围玩家）
        void SendToSet();

    private:
        // Finalize previous log if need by setting total targets amount（如有需要，回填上一条效果的总目标数量）
        void FinalizePrevious();

        // Handle multi targets cases by adjusting targets counter if need（切换当前效果索引并维护多目标计数）
        void SetCurrentEffect(uint32 effect);

        // Variadic template to method to handle multi arguments passed with different types（递归写入可变参数到数据包缓冲区）
        template<typename T>
        void AddLogData(T const& data) { m_spellLogData << data; }

        template<typename T, typename... Args>
        void AddLogData(T const& data, Args const& ... args)
        {
            AddLogData(data);
            AddLogData(args...);
        }

        Spell* m_spell;
        WorldPacket m_spellLogData;
        size_t m_spellLogDataEffectsCounterPos;
        uint32 m_spellLogDataEffectsCounter;
        size_t m_spellLogDataTargetsCounterPos;
        uint32 m_spellLogDataTargetsCounter;
        uint32 m_currentEffect;
};

// 法术异步事件包装器：挂载到施法者的 EventProcessor，用于驱动读条完成、引导跳数及弹道延迟结算
class SpellEvent : public BasicEvent
{
    public:
        SpellEvent(Spell* spell);
        virtual ~SpellEvent();

        virtual bool Execute(uint64 e_time, uint32 p_time) override;
        virtual void Abort(uint64 e_time) override;
        virtual bool IsDeletable() const override;

        Spell* GetSpell() const { return m_Spell.get(); }
        MaNGOS::unique_weak_ptr<Spell> GetSpellWeakPtr() const { return m_Spell; }

    protected:
        MaNGOS::unique_trackable_ptr<Spell> m_Spell;
};

// 法术修饰器 RAII 守卫：在施法前后安全应用、保存或回滚玩家天赋/光环提供的 SpellModifier 充能
class SpellModRAII
{
    public:
        SpellModRAII(Spell* spell, Player* modOwner, bool success = false, bool onlySave = false);
        void SetSuccess() { m_success = true; }
        ~SpellModRAII();
    private:
        Spell* m_spell;
        Player* m_modOwner;
        bool m_success;
        bool m_onlySave; // casting time（仅在读条开始时保存修饰器状态）
};

// 触发/脚本施法可选参数构造器（链式设置目标、脚本值、自定义基础点数、目标地点及物品目标）
class SpellCastArgs
{
    public:
        SpellCastArgs() : m_target(nullptr), m_scriptValue(0), m_scriptValueSet(false), m_destinationSet(false), m_itemSet(false), m_itemTarget(nullptr)
        {
            memset(m_basePoints, 0, sizeof(m_basePoints));
        }

        SpellCastArgs& SetTarget(Unit* target) { m_target = target; return *this; }
        Unit* GetTarget() const { return m_target; }

        SpellCastArgs& SetScriptValue(uint64 scriptValue) { m_scriptValue = scriptValue; m_scriptValueSet = true; return *this; }
        bool IsScriptValueSet() const { return m_scriptValueSet; }
        uint64 GetScriptValue() const { return m_scriptValue; }

        SpellCastArgs& SetBasePoints(int32* basePoints1, int32* basePoints2, int32* basePoints3)
        {
            m_basePoints[0] = basePoints1;
            m_basePoints[1] = basePoints2;
            m_basePoints[2] = basePoints3;
            return *this;
        }
        int32* GetBasePoints(uint32 index) { return m_basePoints[index]; }

        SpellCastArgs& SetDestination(float x, float y, float z)
        {
            m_destinationSet = true;
            m_destination.x = x;
            m_destination.y = y;
            m_destination.z = z;
            return *this;
        }
        SpellCastArgs& SetDestination(Position& position)
        {
            m_destinationSet = true;
            m_destination = position;
            return *this;
        }
        bool IsDestinationSet() const { return m_destinationSet; }
        Position GetDestination() const { return m_destination; }

        SpellCastArgs& SetItemTarget(Item* itemTarget)
        {
            m_itemSet = true;
            m_itemTarget = itemTarget;
            return *this;
        }
        bool IsItemTargetSet() const { return m_itemSet; }
        Item* GetItemTarget() const { return m_itemTarget; }
    private:
        Unit* m_target;
        uint64 m_scriptValue;
        bool m_scriptValueSet;
        int32* m_basePoints[3];
        bool m_destinationSet;
        Position m_destination;
        bool m_itemSet;
        Item* m_itemTarget;
};

// 法术核心执行类：管理法术施放检查、读条/引导/飞行状态机、目标筛选、命中/抗性结算及效果分发
class Spell
{
        friend struct MaNGOS::SpellNotifierPlayer;
        friend struct MaNGOS::SpellNotifierCreatureAndPlayer;
        friend void Unit::SetCurrentCastedSpell(Spell* pSpell);
    public:
        // 法术效果处理器声明（对应 SpellEffects.cpp 中的 SpellEffects 表）
        void EffectEmpty(SpellEffectIndex eff_idx);
        void EffectNULL(SpellEffectIndex eff_idx);
        void EffectUnused(SpellEffectIndex eff_idx);
        void EffectDistract(SpellEffectIndex eff_idx);
        void EffectPull(SpellEffectIndex eff_idx);
        void EffectSchoolDMG(SpellEffectIndex eff_idx);
        void EffectEnvironmentalDMG(SpellEffectIndex eff_idx);
        void EffectInstaKill(SpellEffectIndex eff_idx);
        void EffectDummy(SpellEffectIndex eff_idx);
        void EffectTeleportUnits(SpellEffectIndex eff_idx);
        void EffectApplyAura(SpellEffectIndex eff_idx);
        void EffectSendEvent(SpellEffectIndex eff_idx);
        void EffectPowerBurn(SpellEffectIndex eff_idx);
        void EffectPowerDrain(SpellEffectIndex eff_idx);
        void EffectHeal(SpellEffectIndex eff_idx);
        void EffectBind(SpellEffectIndex eff_idx);
        void EffectHealthLeech(SpellEffectIndex eff_idx);
        void EffectQuestComplete(SpellEffectIndex eff_idx);
        void EffectCreateItem(SpellEffectIndex eff_idx);
        void EffectPersistentAA(SpellEffectIndex eff_idx);
        void EffectEnergize(SpellEffectIndex eff_idx);
        void EffectOpenLock(SpellEffectIndex eff_idx);
        void EffectSummonChangeItem(SpellEffectIndex eff_idx);
        void EffectProficiency(SpellEffectIndex eff_idx);
        void EffectTeleportGraveyard(SpellEffectIndex eff_idx);
        void EffectApplyAreaAura(SpellEffectIndex eff_idx);
        void EffectSummonType(SpellEffectIndex eff_idx);
        void EffectLearnSpell(SpellEffectIndex eff_idx);
        void EffectDispel(SpellEffectIndex eff_idx);
        void EffectDualWield(SpellEffectIndex eff_idx);
        void EffectPickPocket(SpellEffectIndex eff_idx);
        void EffectAddFarsight(SpellEffectIndex eff_idx);
        void EffectHealMechanical(SpellEffectIndex eff_idx);
        void EffectTeleUnitsFaceCaster(SpellEffectIndex eff_idx);
        void EffectLearnSkill(SpellEffectIndex eff_idx);
        void EffectAddHonor(SpellEffectIndex eff_idx);
        void EffectSpawn(SpellEffectIndex eff_idx);
        void EffectTradeSkill(SpellEffectIndex eff_idx);
        void EffectEnchantItemPerm(SpellEffectIndex eff_idx);
        void EffectEnchantItemTmp(SpellEffectIndex eff_idx);
        void EffectTameCreature(SpellEffectIndex eff_idx);
        void EffectSummonPet(SpellEffectIndex eff_idx);
        void EffectLearnPetSpell(SpellEffectIndex eff_idx);
        void EffectWeaponDmg(SpellEffectIndex eff_idx);
        void EffectForceCast(SpellEffectIndex eff_idx);
        void EffectTriggerSpell(SpellEffectIndex eff_idx);
        void EffectTriggerMissileSpell(SpellEffectIndex eff_idx);
        void EffectThreat(SpellEffectIndex eff_idx);
        void EffectHealMaxHealth(SpellEffectIndex eff_idx);
        void EffectInterruptCast(SpellEffectIndex eff_idx);
        void EffectSummonObjectWild(SpellEffectIndex eff_idx);
        void EffectScriptEffect(SpellEffectIndex eff_idx);
        void EffectSanctuary(SpellEffectIndex eff_idx);
        void EffectAddComboPoints(SpellEffectIndex eff_idx);
        void EffectDuel(SpellEffectIndex eff_idx);
        void EffectStuck(SpellEffectIndex eff_idx);
        void EffectSummonPlayer(SpellEffectIndex eff_idx);
        void EffectActivateObject(SpellEffectIndex eff_idx);
        void EffectEnchantHeldItem(SpellEffectIndex eff_idx);
        void EffectSummonObject(SpellEffectIndex eff_idx);
        void EffectResurrect(SpellEffectIndex eff_idx);
        void EffectParry(SpellEffectIndex eff_idx);
        void EffectBlock(SpellEffectIndex eff_idx);
        void EffectLeapForward(SpellEffectIndex eff_idx);
        void EffectLeapBack(SpellEffectIndex eff_idx);
        void EffectTransmitted(SpellEffectIndex eff_idx);
        void EffectDisEnchant(SpellEffectIndex eff_idx);
        void EffectInebriate(SpellEffectIndex eff_idx);
        void EffectFeedPet(SpellEffectIndex eff_idx);
        void EffectDismissPet(SpellEffectIndex eff_idx);
        void EffectReputation(SpellEffectIndex eff_idx);
        void EffectSelfResurrect(SpellEffectIndex eff_idx);
        void EffectSkinning(SpellEffectIndex eff_idx);
        void EffectCharge(SpellEffectIndex eff_idx);
        void EffectChargeDest(SpellEffectIndex eff_idx);
        void EffectProspecting(SpellEffectIndex eff_idx);
        void EffectRedirectThreat(SpellEffectIndex eff_idx);
        void EffectSendTaxi(SpellEffectIndex eff_idx);
        void EffectKnockBack(SpellEffectIndex eff_idx);
        void EffectPullTowards(SpellEffectIndex eff_idx);
        void EffectDispelMechanic(SpellEffectIndex eff_idx);
        void EffectSummonDeadPet(SpellEffectIndex eff_idx);
        void EffectDestroyAllTotems(SpellEffectIndex eff_idx);
        void EffectDurabilityDamage(SpellEffectIndex eff_idx);
        void EffectSkill(SpellEffectIndex eff_idx);
        void EffectTaunt(SpellEffectIndex eff_idx);
        void EffectDurabilityDamagePCT(SpellEffectIndex eff_idx);
        void EffectModifyThreatPercent(SpellEffectIndex eff_idx);
        void EffectAddExtraAttacks(SpellEffectIndex eff_idx);
        void EffectSpiritHeal(SpellEffectIndex eff_idx);
        void EffectSkinPlayerCorpse(SpellEffectIndex eff_idx);
        void EffectStealBeneficialBuff(SpellEffectIndex eff_idx);
        void EffectUnlearnSpecialization(SpellEffectIndex eff_idx);
        void EffectHealPct(SpellEffectIndex eff_idx);
        void EffectEnergisePct(SpellEffectIndex eff_idx);
        void EffectTriggerSpellWithValue(SpellEffectIndex eff_idx);
        void EffectTriggerRitualOfSummoning(SpellEffectIndex eff_idx);
        void EffectKillCreditGroup(SpellEffectIndex eff_idx);
        void EffectQuestFail(SpellEffectIndex eff_idx);
        void EffectPlaySound(SpellEffectIndex eff_idx);
        void EffectPlayMusic(SpellEffectIndex eff_idx);
        void EffectKnockBackFromPosition(SpellEffectIndex eff_idx);
        void EffectSummonRafFriend(SpellEffectIndex eff_idx);
        void EffectCreateTamedPet(SpellEffectIndex eff_ifx);

        Spell(WorldObject* caster, SpellEntry const* info, uint32 triggeredFlags, ObjectGuid originalCasterGUID = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        virtual ~Spell();

        SpellCastResult SpellStart(SpellCastTargets const* targets, Aura* triggeredByAura = nullptr);

        void cancel();                                      // 取消当前施法并清理状态与冷却

        void update(uint32 difftime);                       // 推进法术读条、引导或状态机计时
        SpellCastResult cast(bool skipCheck = false);       // 正式施放法术（填充目标、消耗资源并结算即时或弹道效果）
        void finish(bool ok = true);                        // 结束法术施放并执行收尾、触发法术及清理
        void TakePower();                                   // 扣除施法消耗的能量（法力/怒气/能量等）
        void TakeAmmo() const;                              // 消耗远程弹药
        void TakeReagents();                                // 消耗施法材料
        void TakeCastItem();                                // 消耗或扣减施法物品充能

        SpellCastResult CheckCast(bool strict);             // 检查施法前置条件（目标、距离、资源、光环、冷却等）

        // handlers（阶段处理器）
        void handle_immediate();                            // 立即结算目标效果阶段
        uint64 handle_delayed(uint64 t_offset);             // 按弹道飞行时间分批结算延迟命中目标
        // handler helpers（阶段处理辅助函数）
        void _handle_immediate_phase();                     // 立即命中阶段内部处理
        void _handle_finish_phase();                        // 结束阶段内部处理（包含引导终止）

        SpellCastResult CheckItems();                       // 检查施法所需物品、材料、装备及图腾条件
        SpellCastResult CheckRange(bool strict);            // 检查施法者与目标之间的最小/最大距离
        SpellCastResult CheckPower(bool strict);            // 检查施法者资源是否足够
        SpellCastResult CheckCasterAuras(uint32& param1) const; // 检查施法者身上的控制/沉默/定身等光环限制

        bool CheckSpellCancelsAuraEffect(AuraType auraType, uint32& param1) const;
        bool CheckSpellCancelsCharm(uint32& param1) const;
        bool CheckSpellCancelsStun(uint32& param1) const;
        bool CheckSpellCancelsSilence(uint32& param1) const;
        bool CheckSpellCancelsPacify(uint32& param1) const;
        bool CheckSpellCancelsFear(uint32& param1) const;
        bool CheckSpellCancelsConfuse(uint32& param1) const;

        int32 CalculateSpellEffectValue(SpellEffectIndex i, Unit* target, bool maximum = false, bool finalUse = true)
        { return m_trueCaster->CalculateSpellEffectValue(target, m_spellInfo, i, &m_currentBasePoints[i], maximum, finalUse); }
        int32 CalculateSpellEffectDamage(Unit* unitTarget, int32 damage, float damageDoneMod, SpellEffectIndex effectIndex);
        static uint32 CalculatePowerCost(SpellEntry const* spellInfo, Unit* caster, Spell* spell = nullptr, Item* castItem = nullptr, bool finalUse = false);

        bool HaveTargetsForEffect(SpellEffectIndex effect) const;
        void Delayed();                                     // 非引导法术受击退条处理
        void DelayedChannel();                              // 引导法术受击缩短剩余时间处理
        uint32 getState() const { return m_spellState; }
        void setState(uint32 state) { m_spellState = state; }

        // 召唤生物位置与句柄记录结构
        struct CreaturePosition
        {
            CreaturePosition() :
                x(0.0f), y(0.0f), z(0.0f),
                creature(nullptr), processed(false)
            {}

            float x, y, z;
            Creature* creature;
            bool processed;
        };
        typedef std::vector<CreaturePosition> CreatureSummonPositions;

        bool DoCreateItem(SpellEffectIndex eff_idx, uint32 itemtype, bool reportError = true);
        bool DoSummonPet(CreatureSummonPositions& list, SummonPropertiesEntry const* prop, SpellEffectIndex effIdx);
        bool DoSummonTotem(CreatureSummonPositions& list, SpellEffectIndex eff_idx, uint8 slot_dbc = 0);
        bool DoSummonWild(CreatureSummonPositions& list, SummonPropertiesEntry const* prop, SpellEffectIndex effIdx, uint32 level);
        bool DoSummonGuardian(CreatureSummonPositions& list, SummonPropertiesEntry const* prop, SpellEffectIndex effIdx, uint32 level);
        bool DoSummonCritter(CreatureSummonPositions& list, SummonPropertiesEntry const* prop, SpellEffectIndex effIdx, uint32 level);
        bool DoSummonPossessed(CreatureSummonPositions& list, SummonPropertiesEntry const* prop, SpellEffectIndex effIdx, uint32 level);

        void ProcessDispelList(std::list <std::pair<SpellAuraHolder*, uint32> >& dispelList, std::list<std::pair<SpellAuraHolder*, uint32> >& successList, std::list <uint32>& failList);
        void EvaluateResultLists(std::list<std::pair<SpellAuraHolder*, uint32> >& successList, std::list <uint32>& failList);

        void WriteSpellGoTargets(WorldPacket& data);
        void WriteAmmoToPacket(WorldPacket& data) const;

        template<typename T> WorldObject* FindCorpseUsing();

        bool CheckTarget(Unit* target, SpellEffectIndex eff, bool targetB, bool neutralFlagFill, CheckException exception = CheckException::EXCEPTION_NONE) const;

        // 客户端施法状态与结果数据包发送接口
        static void SendCastResult(Player const* caster, SpellEntry const* spellInfo, uint8 cast_count, SpellCastResult result, bool isPetCastResult = false, uint32 param1 = 0, uint32 param2 = 0);
        void SendCastResult(SpellCastResult result) const;
        void SendSpellStart() const;
        void SendSpellGo();
        void SendSpellCooldown();
        void SendInterrupted(SpellCastResult result) const;
        void SendChannelUpdate(uint32 time, uint32 lastTick = 0) const;
        void SendChannelStart(uint32 duration);

        void StopCast(SpellCastResult castResult);

        void ExecuteEffects(Unit* unitTarget, Item* itemTarget, GameObject* GOTarget, uint32 effectMask);
        void HandleEffect(Unit* unitTarget, Item* itemTarget, GameObject* GOTarget, SpellEffectIndex effIdx, float damageMultiplier = 1.0);
        void HandleThreatSpells();
        void ProcessAOECaps();
        // void HandleAddAura(Unit* Target);

        void SetCastItem(Item* item);
        Item* GetCastItem() { return m_CastItem; }

        SpellEntry const* m_spellInfo;
        SpellEntry const* m_triggeredBySpellInfo;
        int32 m_currentBasePoints[MAX_EFFECT_INDEX];        // cache SpellEntry::CalculateSimpleValue and use for set custom base points（缓存基础效果值并支持自定义覆盖）
        uint8 m_cast_count;
        SpellCastTargets m_targets;

        // Trigger flag system（触发标志系统，由 TriggerCastFlags 解包控制检查跳过与行为）
        bool m_IsTriggeredSpell;
        bool m_ignoreHitResult;
        bool m_ignoreCastTime;
        bool m_ignoreUnselectableTarget;
        bool m_ignoreUnattackableTarget;
        bool m_triggerAutorepeat;
        bool m_doNotProc;
        bool m_petCast;
        bool m_notifyAI;
        bool m_ignoreGCD;
        bool m_ignoreCosts;
        bool m_ignoreCooldowns;
        bool m_ignoreConcurrentCasts;
        bool m_ignoreCasterAuraState;
        bool m_hideInCombatLog;
        bool m_resetLeash;
        bool m_channelOnly;
        // Not a trigger flag but same type of information（非触发标志但同属施法上下文的标识：是否源自客户端主动施法）
        bool m_clientCast;

        // scriptable conditionals（可由法术脚本动态修改的条件开关）
        bool m_ignoreRoot; //

        int32 GetCastTime() const { return m_casttime; }
        uint32 GetCastedTime() const { return m_timer; }
        bool IsAutoRepeat() const { return m_autoRepeat; }
        void SetAutoRepeat(bool rep) { m_autoRepeat = rep; }
        void ReSetTimer() { m_timer = m_casttime > 0 ? m_casttime : 0; }
        bool IsRangedSpell() const
        {
            return  m_spellInfo->HasAttribute(SPELL_ATTR_USES_RANGED_SLOT);
        }
        bool IsSpellRequiringAmmo() const
        {
            if (IsRangedSpell())
                return true;

            if (m_spellInfo->speed > 0.0f)
                if (SpellVisualEntry const* spellVisual = sSpellVisualStore.LookupEntry(m_spellInfo->SpellVisual))
                    if (spellVisual->HasMissile)
                        if (spellVisual->MissileModel == -4 || spellVisual->MissileModel == -5)
                            return true;

            return false;
        }
        bool IsChannelActive() const { return m_caster && m_caster->GetUInt32Value(UNIT_CHANNEL_SPELL) != 0; }
        bool IsMeleeAttackResetSpell() const { return !m_IsTriggeredSpell && (m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_COMBAT);  }
        bool IsRangedAttackResetSpell() const { return !m_IsTriggeredSpell && IsRangedSpell() && (m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_COMBAT); }
        bool IsEffectWithImplementedMultiplier(uint32 effectId) const;

        bool IsDeletable() const { return !m_referencedFromCurrentSpell && !m_executedCurrently; }
        void SetReferencedFromCurrent(bool yes) { m_referencedFromCurrentSpell = yes; }
        void SetExecutedCurrently(bool yes) { m_executedCurrently = yes; }
        bool IsExecutedCurrently() const { return m_executedCurrently; }
        uint64 GetDelayStart() const { return m_delayStart; }
        void SetDelayStart(uint64 m_time) { m_delayStart = m_time; }
        uint64 GetDelayMoment() const { return m_delayMoment; }
        void SetSpellStartTravelling(uint32 time) { m_travellingStart = time; }
        uint32 GetSpellStartTravelling() const { return m_travellingStart; }

        bool IsNeedSendToClient() const;                    // use for hide spell cast for client in case when cast not have client side affect (animation or log entries)（若法术在客户端无动画或日志表现则隐藏施法通知）
        bool IsTriggeredSpellWithRedundantCastTime() const; // use for ignore some spell data for triggered spells like cast time, some triggered spells have redundent copy data from main spell for client use purpose（忽略触发法术从主法术复制的冗余施法时间等数据）
        bool IsTriggeredByAura() const { return m_triggeredByAuraSpell != nullptr; }
        SpellEntry const* GetTriggeredByAuraSpellInfo() const { return m_triggeredByAuraSpell; }

        CurrentSpellTypes GetCurrentContainer() const;

        // caster types:（各类施法者对象访问器）
        // formal spell caster, in game source of spell affects cast（表面施法者：游戏内实际发起施法动作的单位）
        Unit* GetCaster() const { return m_caster; }
        // real source of cast affects, explicit caster, or DoT/HoT applier, or GO owner, or wild GO itself. Can be nullptr（实际效果归属者：显式施法者、DoT/HoT 施加者、GO 拥有者或野外 GO 本身）
        WorldObject* GetAffectiveCasterObject() const;
        // limited version returning nullptr in cases wild gameobject caster object, need for Aura（受限版本：当施法者为野外游戏对象时返回 nullptr，供光环系统使用）
        Unit* GetAffectiveCaster() const { return m_originalCasterGUID ? m_originalCaster : m_caster; }
        // m_originalCasterGUID can store GO guid, and in this case this is visual caster（视觉施法对象：m_originalCasterGUID 为游戏对象时返回该游戏对象）
        WorldObject* GetCastingObject() const;
        // channels are generally instant - until more research provided all need to have speed 0（获取弹道飞行速度；引导法术通常视为瞬发速度 0）
        float GetSpellSpeed() const;
        bool IsDelayedSpell() const;

        void UpdatePointers();                              // must be used at call Spell code after time delay (non triggered spell cast/update spell call/etc)（在延迟回调或更新前刷新内部对象指针，防止悬垂指针）

        static bool CheckTargetCreatureType(Unit* target, SpellEntry const* spellInfo);

        void AddTriggeredSpell(SpellEntry const* spellInfo) { m_TriggerSpells.push_back(spellInfo); }
        void AddPrecastSpell(SpellEntry const* spellInfo) { m_preCastSpells.push_back(spellInfo); }
        void AddTriggeredSpell(uint32 spellId);
        void AddPrecastSpell(uint32 spellId);
        void CastPreCastSpells(Unit* target);
        void CastTriggerSpells();

        void CleanupTargetList();
        void ClearCastItem();

        void SetForwardedCastItem(ObjectGuid guid) { m_forwardedCastItemGuid = guid; }

        // spell mods（本次施法消耗过充能的光环修饰器记录）
        std::set<SpellModifierPair> m_usedAuraCharges;

        static void SelectMountByAreaAndSkill(Unit* target, SpellEntry const* parentSpell, uint32 spellId75, uint32 spellId150, uint32 spellId225, uint32 spellId300, uint32 spellIdSpecial);

        bool IsInterruptible() const;
        bool CanBeInterrupted() const { return m_spellState <= SPELL_STATE_DELAYED || m_spellState == SPELL_STATE_CHANNELING; }

        void RegisterAuraProc(Aura* aura);
        bool IsAuraProcced(Aura* aura);
        // setting 0 disables the trigger（设置效果触发概率，设为 0 则禁用触发）
        void SetEffectChance(int32 triggerChance, SpellEffectIndex effIdx) { m_effectTriggerChance[effIdx] = triggerChance; }

        // Spell Target Subsystem - public part（法术目标子系统 - 公共数据结构）
        // Targets store structures and data
        // 单位目标命中与结算信息条目
        struct TargetInfo
        {
            ObjectGuid targetGUID;
            uint64 timeDelay;
            uint64 initialDelay; // Used to store reflect travel time so we can reset it on proc（记录法术反射前的初始飞行延迟，供触发时重置）
            uint32 HitInfo;
            uint32 damage;
            uint32 healing;
            SpellMissInfo missCondition : 8;
            SpellMissInfo reflectResult : 8;
            uint8  effectHitMask : 8; // Used for all effects which actually hit a target（实际命中的效果掩码）
            uint8  effectMask : 8; // Used for all effects a certain target was evaluated for（参与评估的全部效果掩码）
            uint8  effectMaskProcessed : 8; // Used for all effects that were already processed（已执行结算的效果掩码）
            bool   processed : 1;
            bool   magnet : 1;
            bool   procReflect : 1; // Used to tell hit to proc reflect only and return reflect back（仅触发反射并将法术弹回原施法者）
            bool   isCrit : 1;
            bool   executionless : 1;
            uint32 heartbeatResistChance;
            uint32 effectDuration;
            uint32 diminishDuration; // Store duration after diminishing returns are applied（记录递减效应计算后的实际持续时间）
            DiminishingLevels diminishLevel;
            DiminishingGroup diminishGroup;
        };
        uint8 m_needAliveTargetMask;                        // Mask req. alive targets
        void ProcReflectProcs(TargetInfo& targetInfo);

        struct GOTargetInfo
        {
            ObjectGuid targetGUID;
            uint64 timeDelay;
            uint8  effectMask : 8;
            bool   processed : 1;
        };

        struct ItemTargetInfo
        {
            Item* item;
            uint8 effectMask;
        };

        struct CorpseTargetInfo
        {
            ObjectGuid corpseGUID;
            uint8 effectMask;
        };

        struct DestTargetInfo
        {
            uint32 effectMask;
            uint64 timeDelay;
            uint32 effectMaskProcessed;
            DestTargetInfo() : effectMask(0), timeDelay(0), effectMaskProcessed(0) {}
        };

        typedef std::list<TargetInfo>     TargetList;
        typedef std::list<GOTargetInfo>   GOTargetList;
        typedef std::list<ItemTargetInfo> ItemTargetList;
        typedef std::list<CorpseTargetInfo> CorpseTargetList;

        struct TempTargetData
        {
            UnitList tmpUnitList[2];
            GameObjectList tmpGOList[2];
            std::list<Item*> tempItemList;
            CorpseList tempCorpseList;
            bool neutralFlagFill = false;
        };
        struct TempTargetingData
        {
            TempTargetData data[MAX_EFFECT_INDEX];
            uint32 chainTargetCount[MAX_EFFECT_INDEX];
            bool magnet;
        };

        SpellCastResult CheckScriptTargeting(SpellEffectIndex effIndex, uint32 chainTargets, float radius, uint32 targetMode, UnitList& tempUnitList, GameObjectList& tempGOList);

        // Scripting system（法术脚本系统接口与回调钩子）
        SpellScript* GetSpellScript() const { return m_spellScript; }
        // Variable storage（脚本持久化状态变量存取）
        uint64 GetScriptValue() const { return m_scriptValue; }
        void SetScriptValue(uint64 value) { m_scriptValue = value; }
        // hooks（生命周期与效果执行钩子）
        void OnInit();
        void OnSuccessfulStart();
        void OnSuccessfulFinish();
        SpellCastResult OnCheckCast(bool strict);
        void OnEffectExecute(SpellEffectIndex effIndex);
        void OnRadiusCalculate(SpellEffectIndex effIndex, bool targetB, float& radius);
        void OnDestTarget();
        bool OnCheckTarget(GameObject* target, SpellEffectIndex eff) const;
        bool OnCheckTarget(Unit* target, SpellEffectIndex eff) const;
        void OnCast();
        void OnHit(SpellMissInfo missInfo);
        void OnAfterHit();
        void OnSummon(GameObject* summon);
        void OnSummon(Creature* summon);
        // effect execution info access - only to be used in OnEffectExecute OnHit and OnAfterHit（效果执行上下文访问器，仅限上述三个钩子内调用）
        Unit* GetUnitTarget() { return unitTarget; }
        Item* GetItemTarget() { return itemTarget; }
        GameObject* GetGOTarget() { return gameObjTarget; }
        uint32 GetDamage() { return damage; }
        void SetDamage(uint32 newDamage) { damage = newDamage; }
        SpellSchoolMask GetSchoolMask() { return m_spellSchoolMask; }
        void SetGuaranteedCrit() { m_guaranteedCrit = true; }
        // OnInit use only（仅限 OnInit 阶段调用）
        void SetEffectSkipMask(uint32 mask) { m_effectSkipMask = mask; }
        // OnHit use only（仅限 OnHit 阶段调用）
        uint32 GetTotalTargetDamage() const { return m_damage; }
        uint32 GetTotalTargetAbsorb() const { return m_absorb; }
        void SetTotalTargetValueModifier(float modifier);
        int32 GetDamageForEffect(SpellEffectIndex effIdx) const { return m_damagePerEffect[effIdx]; }
        // script initialization hook only setters - use only if dynamic - else use appropriate helper（仅限脚本初始化钩子调用的动态参数设置）
        void SetMaxAffectedTargets(uint32 newValue) { m_affectedTargetCount = newValue; }
        void SetJumpRadius(float newValue) { m_jumpRadius = newValue; }
        // warning - always set scheme for first unique target in a row（注意：同一排首个唯一目标需设置过滤策略）
        void SetFilteringScheme(SpellEffectIndex effIdx, bool targetB, SpellTargetFilterScheme scheme) { m_filteringScheme[effIdx][uint32(targetB)] = scheme; }
        // onsuccessfulspellstart（施法成功开始时读写能量消耗）
        uint32 GetPowerCost() const { return m_powerCost; }
        void SetPowerCost(uint32 powerCost) { m_powerCost = powerCost; }
        // access to targets（访问最终命中目标列表）
        TargetList& GetTargetList() { return m_UniqueTargetInfo; }
        // enables customizing auras after creation - use only in OnEffectExecute and with aura effects（获取当前目标刚创建的光环容器，仅限 OnEffectExecute 中光环类效果使用）
        SpellAuraHolder* GetSpellAuraHolder() { return m_spellAuraHolder; }
        // sets event target object（设置事件目标对象）
        void SetEventTarget(WorldObject* object) { m_eventTarget = object; }

        // GO casting preparations（游戏对象模拟施法准备与真实施法者访问）
        void SetFakeCaster(Unit* caster) { m_caster = caster; } // also used by dyngo caster emulation（也用于动态对象模拟施法者）
        WorldObject* GetTrueCaster() const { return m_trueCaster; }
        Unit* GetAffectiveCasterOrOwner() const;

        // custom Spell Cast Results（自定义施法失败结果的附加参数）
        void SetParam1(uint32 param1) { m_param1 = param1; }
        void SetParam2(uint32 param2) { m_param2 = param2; }
        // overrides（运行时参数覆写接口）
        void SetOverridenSpeed(float newSpeed);
        void SetIgnoreRoot(bool state) { m_ignoreRoot = state; }
        void SetDamageDoneModifier(float mod, SpellEffectIndex effIdx);
        void SetUsableWhileStunned(bool state) { m_usableWhileStunned = state; }
        void SetHelpfulThreatCoefficient(float coeff) { m_helpfulThreatCoeff = coeff; }

        MaNGOS::unique_weak_ptr<Spell> GetWeakPtr() const;

    protected:
        void SendLoot(ObjectGuid guid, LootType loottype, LockType lockType);
        bool IgnoreItemRequirements() const;                // some item use spells have unexpected reagent data（部分物品触发法术带有异常材料数据，需忽略物品前置检查）
        void UpdateOriginalCasterPointer();

        SpellCastResult PreCastCheck(Aura* triggeredByAura = nullptr);
        void Prepare();

        // Spell_C_GetMinMaxRange client equivalent - do not change（与客户端 Spell_C_GetMinMaxRange 等价的距离计算，切勿随意改动）
        std::pair<float, float> GetMinMaxRange(bool strict);

        Unit* m_caster;
        Item* m_CastItem;
        bool m_itemCastSpell;
        ObjectGuid m_forwardedCastItemGuid;

        ObjectGuid m_originalCasterGUID;                    // real source of cast (aura caster/etc), used for spell targets selection（真实施法源 GUID，用于判定敌友与目标选择）
        // e.g. damage around area spell trigered by victim aura and da,age emeies of aura caster
        Unit* m_originalCaster;                             // cached pointer for m_originalCaster, updated at Spell::UpdatePointers()（真实施法者缓存指针，在 UpdatePointers 中刷新）

        // Spell data（法术核心计算属性）
        SpellSchoolMask m_spellSchoolMask;                  // Spell school (can be overwrite for some spells (wand shoot for example)（法术学派掩码，魔杖射击等法术会覆写此值）
        WeaponAttackType m_attackType;                      // For weapon based attack（武器攻击类型：主手/副手/远程）
        uint32 m_powerCost;                                 // Calculated spell cost     initialized only in Spell::prepare（已计算的法术资源消耗，仅在 Prepare 阶段初始化）
        int32 m_casttime;                                   // Calculated spell cast time initialized only in Spell::prepare（已计算的施法时间，仅在 Prepare 阶段初始化）
        int32 m_duration;
        bool m_reflectable;                                  // can reflect this spell?（该法术是否可被法术反射）
        bool m_autoRepeat;
        float m_maxRange;                                   // For channeled spell check（引导法术持续期间的最大距离校验值）

        uint8 m_delayAtDamageCount;
        int32 GetNextDelayAtDamageMsTime() { return m_delayAtDamageCount < 5 ? 1000 - (m_delayAtDamageCount++) * 200 : 200; }

        // Delayed spells system（延迟/弹道法术计时状态）
        uint64 m_delayStart;                                // time of spell delay start, filled by event handler, zero = just started（弹道延迟起始时间戳，0 表示刚起飞）
        uint64 m_delayMoment;                               // moment of next delay call, used internally（下一次延迟结算的目标时刻）
        uint32 m_travellingStart;                           // moment when spell travel starts - used in checks for aura interruption（法术弹道开始飞行的时刻，用于光环打断校验）

        // These vars are used in both delayed spell system and modified immediate spell system（以下标志用于防止法术在执行或引用期间被提前析构）
        bool m_referencedFromCurrentSpell;                  // mark as references to prevent deleted and access by dead pointers（被当前施法槽引用标记）
        bool m_executedCurrently;                           // mark as executed to prevent deleted and access by dead pointers（正在执行中标记）
        bool m_needSpellLog;                                // need to send spell log?（是否需要发送 SMSG_SPELLLOGEXECUTE）
        uint8 m_applyMultiplierMask;                        // by effect: damage multiplier needed?（按效果位标记是否需要应用伤害递减/加成倍率）
        float m_damageMultipliers[3];                       // by effect: damage multiplier（按效果索引存储的弹跳/AOE 伤害倍率）
        float m_damageDoneMultiplier[3];

        // Current targets, to be used in SpellEffects (MUST BE USED ONLY IN SPELL EFFECTS)（当前效果执行上下文目标，仅允许在 SpellEffects 处理器内使用）
        Unit* unitTarget;
        Item* itemTarget;
        GameObject* gameObjTarget;
        Corpse* corpseTarget;
        SpellAuraHolder* m_spellAuraHolder;                 // spell aura holder for current target, created only if spell has aura applying effect（当前目标对应的光环容器，仅当法术含光环效果时创建）
        int32 damage;

        // -------------------------------------------
        WorldObject* m_eventTarget;

        // Damage and healing in effects need just calculate（效果结算期间累计的伤害、吸收、治疗与吸血数值）
        int32 m_damage;                                     // Damage in effects count here（累计对当前目标造成的总伤害）
        int32 m_absorb;                                     // Absorbed amount for scripts to hook into（累计被吸收的伤害，供脚本钩子读取）
        int32 damagePerEffect[MAX_EFFECT_INDEX];            // Workaround for multiple weapon damage effects（多武器伤害效果的中间值记录）
        int32 m_damagePerEffect[MAX_EFFECT_INDEX];
        int32 m_healing;                                    // Healing in effects count here（累计对当前目标造成的总治疗量）
        int32 m_healingPerEffect[MAX_EFFECT_INDEX];
        int32 m_healthLeech;                                // Health leech in effects for all targets count here（跨所有目标累计的生命吸取总量）
        bool m_guaranteedCrit;                              // Used in effect handlers to guarantee crit（在效果处理器中强制本次效果暴击）
        bool m_usableWhileStunned;

        //******************************************
        // Spell trigger system（法术触发系统）
        //******************************************
        bool   m_canTrigger;                                // Can start trigger (m_IsTriggeredSpell can`t use for this)（是否允许触发额外事件/Proc）
        uint8  m_negativeEffectMask;                        // Use for avoid sent negative spell procs for additional positive effects only targets（负面效果掩码，避免对纯友方效果目标触发负面 Proc）
        void prepareDataForTriggerSystem();
        void PrepareMasksForProcSystem(uint8 effectMask, uint32& procAttacker, uint32& procVictim, WorldObject* caster, WorldObject* target);

        //*****************************************
        // Spell target filling（法术目标填充与筛选）
        //*****************************************
        void FillTargetMap();
        void SetTargetMap(SpellEffectIndex effIndex, uint32 targetMode, bool targetB, TempTargetingData& targetingData);
        bool FillUnitTargets(TempTargetingData& targetingData, SpellTargetingData& data, uint32 i);
        bool CheckAndAddMagnetTarget(Unit* unitTarget, SpellEffectIndex effIndex, bool targetB, TempTargetingData& data);
        static void CheckSpellScriptTargets(SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry>& bounds, UnitList& tempTargetUnitMap, UnitList& targetUnitMap, SpellEffectIndex effIndex);
        void FilterTargetMap(UnitList& filterUnitList, SpellTargetFilterScheme scheme, uint32 chainTargetCount);
        void FillFromTargetFlags(TempTargetingData& targetingData, SpellEffectIndex effIndex);

        void FillAreaTargets(UnitList& targetUnitMap, float radius, float cone, SpellNotifyPushType pushType, SpellTargets spellTargets, WorldObject* originalCaster = nullptr);
        void FillRaidOrPartyTargets(UnitList& targetUnitMap, Unit* member, float radius, bool raid, bool withPets, bool withcaster) const;

        // Returns a target that was filled by SPELL_SCRIPT_TARGET (or selected victim) Can return nullptr（返回由 SPELL_SCRIPT_TARGET 或当前目标填充的单位，可能为空）
        Unit* GetPrefilledUnitTargetOrUnitTarget(SpellEffectIndex effIndex) const;
        void GetSpellRangeAndRadius(SpellEffectIndex effIndex, float& radius, bool targetB, uint32& EffectChainTarget);
        float GetCone();

        //*****************************************
        // Spell target subsystem（法术目标结算子系统）
        //*****************************************
        TargetList     m_UniqueTargetInfo;
        GOTargetList   m_UniqueGOTargetInfo;
        ItemTargetList m_UniqueItemInfo;
        uint32         m_targetlessMask;
        DestTargetInfo m_destTargetInfo;
        CorpseTargetList m_uniqueCorpseTargetInfo;
        uint32 m_partialApplicationMask;

        void AddUnitTarget(Unit* target, uint8 effectMask, CheckException exception = CheckException::EXCEPTION_NONE);
        void AddGOTarget(GameObject* target, uint8 effectMask);
        void AddCorpseTarget(Corpse* target, uint8 effectMask);
        void AddItemTarget(Item* item, uint8 effectMask);
        void AddDestExecution(SpellEffectIndex effIndex);
        void DoAllEffectOnTarget(TargetInfo* target);
        void HandleImmediateEffectExecution(TargetInfo* target);
        void InitializeDamageMultipliers();
        void ResetEffectDamageAndHeal();
        void DoSpellHitOnUnit(Unit* unit, uint32 effectMask, TargetInfo* target, bool isReflected = false);
        void DoAllTargetlessEffects(bool dest);
        void DoAllEffectOnTarget(GOTargetInfo* target);
        void DoAllEffectOnTarget(ItemTargetInfo* target);
        void DoAllEffectOnTarget(CorpseTargetInfo* target);
        bool IsAliveUnitPresentInTargetList();
        bool IsValidDeadOrAliveTarget(Unit const* unit) const;
        SpellCastResult CanOpenLock(SpellEffectIndex effIndex, uint32 lockid, SkillType& skillId, int32& reqSkillValue, int32& skillValue);
        void ProcSpellAuraTriggers();
        bool CanExecuteTriggersOnHit(uint8 effMask, SpellEntry const* triggeredByAura, bool auraTarget) const;
        // -------------------------------------------

        // Diminishing returns（控制技能递减判断）
        bool CanSpellDiminish() const;

        // List For Triggered Spells（待联动的预施放法术与收尾触发法术列表）
        typedef std::list<SpellEntry const*> SpellInfoList;
        SpellInfoList m_TriggerSpells;                      // casted by caster to same targets settings in m_targets at success finish of current spell（当前法术成功结束时对同一批目标施放）
        SpellInfoList m_preCastSpells;                      // casted by caster to each target at spell hit before spell effects apply（法术命中各目标但在效果生效前先行施放）

        // Scripting System（脚本状态与每效果触发概率配置）
        uint64 m_scriptValue; // persistent value for spell script state（法术脚本持久化状态值）
        SpellScript* m_spellScript;
        AuraScript* m_auraScript; // needed for some checks for value calculation（部分数值计算所需的关联光环脚本）
        int32 m_effectTriggerChance[MAX_EFFECT_INDEX]; // used by effects to roll if they should go off（各效果独立触发概率）
        uint32 m_effectSkipMask;

        uint32 m_spellState;
        uint32 m_timer;
        uint32 m_creationTime;
        bool m_updated;

        float m_castPositionX;
        float m_castPositionY;
        float m_castPositionZ;
        float m_castOrientation;

        uint32 m_affectedTargetCount;
        float m_jumpRadius;
        SpellTargetFilterScheme m_filteringScheme[MAX_EFFECT_INDEX][2];

        std::set<Aura*> m_procOnceHolder;

        struct EffectSkillInfo
        {
            SkillType skillId = SKILL_NONE;
            int32 reqSkillValue = 0;
            int32 skillValue = 0;
        };

        EffectSkillInfo m_effectSkillInfo[MAX_EFFECT_INDEX];

        // if need this can be replaced by Aura copy
        // we can't store original aura link to prevent access to deleted auras
        // and in same time need aura data and after aura deleting.
        SpellEntry const* m_triggeredByAuraSpell;

        SpellEvent* m_spellEvent;

    private:
        // needed to store all log for this spell
        SpellLog m_spellLog;

        // spell cast results
        uint32 m_param1;
        uint32 m_param2;

        // speed override from parent spell
        bool m_overrideSpeed;
        float m_overridenSpeed;

        // GO casting preparations
        WorldObject* m_trueCaster;

        // Applies coefficient to spell_threat to helpful target
        float m_helpfulThreatCoeff;
};

// 能量/生命补充类型（用于法术日志上报）
enum ReplenishType
{
    REPLENISH_UNDEFINED = 0,
    REPLENISH_HEALTH    = 20,                               // 恢复生命值
    REPLENISH_MANA      = 21,                               // 恢复法力值
    REPLENISH_RAGE      = 22                                // 恢复怒气值
};

namespace MaNGOS
{
    // 网格玩家目标通知器（当前未使用）
    struct SpellNotifierPlayer              // Currently unused. When put to use this one requires handling for source-location (smilar to below)
    {
        UnitList& i_data;
        Spell& i_spell;
        const uint32& i_index;
        float i_radius;
        WorldObject* i_originalCaster;

        SpellNotifierPlayer(Spell& spell, UnitList& data, const uint32& i, float radius)
            : i_data(data), i_spell(spell), i_index(i), i_radius(radius)
        {
            i_originalCaster = i_spell.GetCastingObject();
        }

        void Visit(PlayerMapType& m)
        {
            if (!i_originalCaster)
                return;

            for (auto& itr : m)
            {
                Player* pPlayer = itr.getSource();
                if (!pPlayer->IsAlive() || pPlayer->IsTaxiFlying())
                    continue;

                if (!i_originalCaster->CanAttackSpell(pPlayer, i_spell.m_spellInfo))
                    continue;

                if (pPlayer->IsWithinDist3d(i_spell.m_targets.m_destPos.x, i_spell.m_targets.m_destPos.y, i_spell.m_targets.m_destPos.z, i_radius))
                    i_data.push_back(pPlayer);
            }
        }
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    // 网格生物与玩家目标搜索器：按圆形半径或锥形扇面收集满足敌友与免疫条件的单位
    struct SpellNotifierCreatureAndPlayer
    {
        UnitList& i_data;
        Spell& i_spell;
        SpellNotifyPushType i_push_type;
        float i_radius;
        float i_cone;
        SpellTargets i_TargetType;
        WorldObject* i_originalCaster;
        WorldObject* i_castingObject;
        bool i_playerControlled;
        float i_centerX;
        float i_centerY;
        float i_centerZ;

        float GetCenterX() const { return i_centerX; }
        float GetCenterY() const { return i_centerY; }

        SpellNotifierCreatureAndPlayer(Spell& spell, UnitList& data, float radius, float cone, SpellNotifyPushType type,
                                       SpellTargets TargetType = SPELL_TARGETS_AOE_ATTACKABLE, WorldObject* originalCaster = nullptr)
            : i_data(data), i_spell(spell), i_push_type(type), i_radius(radius), i_cone(cone), i_TargetType(TargetType),
              i_originalCaster(originalCaster), i_castingObject(i_spell.GetCastingObject())
        {
            if (!i_originalCaster)
                i_originalCaster = i_spell.GetAffectiveCasterObject();
            i_playerControlled = i_originalCaster  ? i_originalCaster->IsControlledByPlayer() : false;

            switch (i_push_type)
            {
                case PUSH_CONE:
                case PUSH_SELF_CENTER:
                    if (i_castingObject)
                    {
                        i_centerX = i_castingObject->GetPositionX();
                        i_centerY = i_castingObject->GetPositionY();
                        i_centerZ = i_castingObject->GetPositionZ();
                    }
                    break;
                case PUSH_SRC_CENTER:
                    if (i_spell.m_targets.m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
                        i_spell.m_targets.getSource(i_centerX, i_centerY, i_centerZ);
                    break;
                case PUSH_DEST_CENTER:
                    if (i_spell.m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
                        i_spell.m_targets.getDestination(i_centerX, i_centerY, i_centerZ);
                    break;
                case PUSH_TARGET_CENTER:
                    if (Unit* target = i_spell.m_targets.getUnitTarget())
                    {
                        i_centerX = target->GetPositionX();
                        i_centerY = target->GetPositionY();
                        i_centerZ = target->GetPositionZ();
                    }
                    break;
                default:
                    sLog.outError("SpellNotifierCreatureAndPlayer: unsupported PUSH_* case %u.", i_push_type);
            }
        }

        template<class T> inline void Visit(GridRefManager<T>& m)
        {
            if (!i_originalCaster || !i_castingObject)
                return;

            for (typename GridRefManager<T>::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                // there are still more spells which can be casted on dead, but
                // they are no AOE and don't have such a nice SPELL_ATTR flag
                // mostly phase check
                if (i_spell.m_spellInfo->HasAttribute(SPELL_ATTR_EX6_IGNORE_PHASE_SHIFT))
                {
                    if (!itr->getSource()->IsInMapIgnorePhase(i_originalCaster))
                        continue;
                }
                else if (!itr->getSource()->IsInMap(i_originalCaster))
                    continue;

                if (itr->getSource()->IsTaxiFlying())
                    continue;

                switch (i_TargetType)
                {
                    case SPELL_TARGETS_CHAIN_ATTACKABLE:
                        if (itr->getSource()->IsChainImmune())
                            continue;
                        break;
                    case SPELL_TARGETS_AOE_ATTACKABLE:
                        if (itr->getSource()->IsAOEImmune())
                            continue;
                        break;
                    default: break;
                }

                switch (i_TargetType)
                {
                    case SPELL_TARGETS_ASSISTABLE:
                        if (!i_originalCaster->CanAssistSpell(itr->getSource(), i_spell.m_spellInfo))
                            continue;
                        break;
                    case SPELL_TARGETS_CHAIN_ATTACKABLE:
                    case SPELL_TARGETS_AOE_ATTACKABLE:
                    {
                        if (!i_originalCaster->CanAttackSpell(itr->getSource(), i_spell.m_spellInfo, !i_spell.m_spellInfo->HasAttribute(SPELL_ATTR_EX5_IGNORE_AREA_EFFECT_PVP_CHECK)))
                            continue;
                    }
                    break;
                    case SPELL_TARGETS_ALL:
                        break;
                    default: continue;
                }

                // we don't need to check InMap here, it's already done some lines above
                switch (i_push_type)
                {
                    case PUSH_CONE:
                    {
                        float heightDifference = std::abs(itr->getSource()->GetPositionZ() - i_centerZ);
                        float maxHeight = i_radius / 2;
                        float distance = std::min(sqrtf(itr->getSource()->GetDistance2d(i_centerX, i_centerY, DIST_CALC_NONE)), i_radius);
                        float ratio = distance / i_radius;
                        float conalMaxHeight = maxHeight * ratio; // pvp combat uses true cone from roughly model
                        if (!i_originalCaster->IsControlledByPlayer() && itr->getSource()->IsControlledByPlayer())
                            conalMaxHeight = maxHeight; // npcs just do a conal max Z aoe
                        if (i_cone >= 0.f)
                        {
                            if (i_castingObject->isInFront(itr->getSource(), i_radius, i_cone) &&
                                std::abs(itr->getSource()->GetPositionZ() - i_centerZ) - itr->getSource()->GetCombatReach() <= conalMaxHeight)
                                i_data.push_back(itr->getSource());
                        }
                        else
                        {
                            if (i_castingObject->isInBack(itr->getSource(), i_radius, -i_cone) &&
                                std::abs(itr->getSource()->GetPositionZ() - i_centerZ) - itr->getSource()->GetCombatReach() <= conalMaxHeight)
                                i_data.push_back(itr->getSource());
                        }
                        break;
                    }
                    case PUSH_SELF_CENTER:
                    case PUSH_SRC_CENTER:
                    case PUSH_DEST_CENTER:
                    case PUSH_TARGET_CENTER:
                        float radius = i_radius;
                        if (i_originalCaster->IsControlledByPlayer() && !itr->getSource()->IsControlledByPlayer())
                            radius += itr->getSource()->GetCombatReach();
                        if (itr->getSource()->GetDistance(i_centerX, i_centerY, i_centerZ, DIST_CALC_NONE) <= radius * radius)
                            i_data.push_back(itr->getSource());
                        break;
                }
            }
        }

#ifdef _MSC_VER
        template<> inline void Visit(CorpseMapType&) {}
        template<> inline void Visit(GameObjectMapType&) {}
        template<> inline void Visit(DynamicObjectMapType&) {}
        template<> inline void Visit(CameraMapType&) {}
#endif
    };

#ifndef _MSC_VER
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(CorpseMapType&) {}
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(GameObjectMapType&) {}
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(DynamicObjectMapType&) {}
    template<> inline void SpellNotifierCreatureAndPlayer::Visit(CameraMapType&) {}
#endif
}

typedef void(Spell::*pEffect)(SpellEffectIndex eff_idx);

#endif
