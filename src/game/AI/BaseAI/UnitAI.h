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

#ifndef MANGOS_UNITAI_H
#define MANGOS_UNITAI_H

#include "Platform/Define.h"
#include "Dynamic/FactoryHolder.h"
#include "Globals/SharedDefines.h"
#include "Dynamic/FactoryHolder.h"
#include "Entities/ObjectGuid.h"
#include "AI/BaseAI/AIDefines.h"
#include "AI/ScriptDevAI/base/TimerAI.h"
#include "Entities/EntitiesMgr.h"
#include <functional>

class WorldObject;
class GameObject;
class Unit;
class Creature;
class Player;
struct SpellEntry;
class ChatHandler;
class Spell;
struct CreatureSpellListTargeting;
struct CreatureSpellList;

#define TIME_INTERVAL_LOOK   5000 // 默认视野/巡视时间间隔
#define VISIBILITY_RANGE    10000 // 最大视野检测范围常量
#define DISTANCING_CONSTANT 1.f // 远程走位保持距离判定常量

// UnitAI::CanCastSpell / DoCastSpellIfCan 返回的施法结果枚举
enum CanCastResult
{
    CAST_OK                     = 0,                        // 可以施放 / 施放成功
    CAST_FAIL_IS_CASTING        = 1,                        // 当前正在施法或引导中
    CAST_FAIL_OTHER             = 2,                        // 其他通用失败原因（如缺少目标）
    CAST_FAIL_TOO_FAR           = 3,                        // 目标超出法术最大射程
    CAST_FAIL_TOO_CLOSE         = 4,                        // 目标距离过近（低于最小射程）
    CAST_FAIL_POWER             = 5,                        // 法力/能量/怒气不足
    CAST_FAIL_STATE             = 6,                        // 单位状态不允许施法（眩晕、沉默、已死亡等）
    CAST_FAIL_TARGET_AURA       = 7,                        // 目标已存在冲突光环或缺少前置光环
    CAST_FAIL_NOT_IN_LOS        = 8,                        // 目标不在视野视线 (LoS) 内
    CAST_FAIL_COOLDOWN          = 9,                        // 法术处于冷却中
    CAST_FAIL_EVADE             = 10,                       // 单位处于逃脱/脱战状态
    CAST_FAIL_CAST_PREVENTED    = 11,                       // 脚本或规则阻止本次施法
    CAST_FAIL_MISCELLANEOUS     = 12,                       // 其他杂项限制
};

// DoCastSpellIfCan 使用的施法控制标志位
enum CastFlags
{
    CAST_INTERRUPT_PREVIOUS     = 0x01,                     // Interrupt any spell casting (打断当前正在施放的法术)
    CAST_TRIGGERED              = 0x02,                     // Triggered (this makes spell cost zero mana and have no cast time) (触发施放：无耗蓝且瞬发)
    CAST_FORCE_CAST             = 0x04,                     // Forces cast even if creature is out of mana or out of range (无视蓝量与距离强制施放)
    CAST_NO_MELEE_IF_OOM        = 0x08,                     // Prevents creature from entering melee if out of mana or out of range (缺蓝或超距时不切入近战追击)
    CAST_FORCE_TARGET_SELF      = 0x10,                     // Forces the target to cast this spell on itself (强制让目标对自己施放该法术)
    CAST_AURA_NOT_PRESENT       = 0x20,                     // Only casts the spell if the target does not have an aura from the spell (仅当目标身上尚无该法术光环时施放)
    CAST_IGNORE_UNSELECTABLE_TARGET = 0x40,                 // Can target UNIT_FLAG_UNINTERACTIBLE - Needed in some scripts (允许以不可选中单位为目标)
    CAST_SWITCH_CASTER_TARGET   = 0x80,                     // Switches target and caster for spell cast (交换施法者与目标)
    CAST_MAIN_SPELL             = 0x100,                    // Marks main spell - DEPRECATED - use creature_spell_list instead (标记为主技能，已废弃，请改用 creature_spell_list)
    CAST_PLAYER_ONLY            = 0x200,                    // Selects only player targets - substitution for EAI not having more params (仅选择玩家目标，用于弥补 EAI 参数限制)
    CAST_DISTANCE_YOURSELF      = 0x400,                    // If spell with this cast flag hits main aggro target, caster distances himself - EAI only - DEPRECATED - use creature_spell_list instead (命中主仇恨目标后主动拉开距离，已废弃)
    CAST_TARGET_CASTING         = 0x800,                    // Selects only targets that are casting - EAI only (仅选择正在施法的目标，供 EAI 使用)
    CAST_ONLY_XYZ               = 0x1000,                   // Targets only coords of target and not unit (仅以目标所在坐标而非单位本身作为法术落点)
};

// AI 反应姿态枚举
enum ReactStates
{
    REACT_PASSIVE    = 0,                                   // 被动：不主动索敌也不反击
    REACT_DEFENSIVE  = 1,                                   // 防御：受击或主人受击时反击
    REACT_AGGRESSIVE = 2                                    // 主动：主动攻击视野内敌对单位
};

// 当前高优先级 AI 移动/行为指令枚举
enum AIOrders
{
    ORDER_NONE,                                             // 无特殊指令（正常追击/战斗）
    ORDER_DISTANCING,                                       // 远程拉扯保持距离中
    ORDER_FLEEING,                                          // 恐惧/低血量逃跑中
    ORDER_RETREATING,                                       // 撤退向友军求援中
    ORDER_EVADE,                                            // 脱战跑回出生点中
    ORDER_FLEE_FROM_CALL_FOR_HELP,                          // 响应呼救逃离中
    ORDER_CRITTER_FLEE,                                     // 小动物受惊逃跑中
    ORDER_CUSTOM,                                           // 自定义脚本指令
};

// 远程施法 AI 模式分类
enum RangeModeType : uint32 // maybe can be substituted for class checks
{
    TYPE_NONE = 0,                                          // 非远程模式（纯近战）
    TYPE_FULL_CASTER = 1,                                   // 纯施法者：维持远程追击距离，缺蓝或沉默时才切近战
    TYPE_PROXIMITY = 2,                                     // 混合接近模式
    TYPE_NO_MELEE_MODE = 3,                                 // 禁止近战移动模式（保持站桩/远程）
    TYPE_DISTANCER = 4,                                     // 走位拉扯型：与目标过近时主动后撤拉开距离
    TYPE_MAX,
};

// UnitAI 内置通用定时动作 ID
enum GenericAIActions
{
    GENERIC_ACTION_DISTANCE   = 2000,                       // 远程拉开距离后的动作计时器
    GENERIC_ACTION_SPELL_LIST = 2001,                       // 法术列表公共冷却恢复计时器
};

// 单位 AI 抽象基类：定义所有 Unit/Creature/PlayerAI 的统一生命周期、事件回调、施法及远程战斗框架
class UnitAI : public CombatActions
{
    public:
        explicit UnitAI(Unit* unit);
        explicit UnitAI(Unit* unit, uint32 combatActions);

        virtual ~UnitAI();

        ///== Information about AI ========================
        /**
         * This funcion is used to display information about the AI.
         * It is called when the .npc aiinfo command is used.
         * Use this for on-the-fly debugging
         * @param reader is a ChatHandler to send messages to.
         * 输出当前 AI 调试信息（供 .npc aiinfo 命令使用）
         */
        virtual void GetAIInformation(ChatHandler& /*reader*/) {}

        ///== Reactions At =================================

        /**
         * Called if IsVisible(Unit* pWho) is true at each (relative) override pWho move, reaction at visibility zone enter
         * Note: The Unit* pWho can be out of Line of Sight, usually this is only visibiliy (by state) and range dependendend
         * Note: This function is not called for creatures who are in evade mode
         * @param pWho Unit* who moved in the visibility range and is visisble
         * 当可见单位在视野范围内移动或进入视野时触发（脱战跑回期间不调用）
         */
        virtual void MoveInLineOfSight(Unit* /*who*/);

        /**
         * Called for reaction at enter to combat if not in combat yet
         * @param enemy Unit* of whom the Creature enters combat with, can be nullptr
         * 单位首次进入战斗时回调
         */
        virtual void EnterCombat(Unit* /*enemy*/);

        /**
         * Called for reaction at stopping attack at no attackers or targets
         * This is called usually in Unit::SelectHostileTarget, if no more target exists
         * 无可用攻击目标或脱离战斗时调用
         */
        virtual void EnterEvadeMode();

        /**
         * Called at reaching home after MoveTargetedHome
         * 脱战跑回出生点到达后调用
         */
        virtual void JustReachedHome() {}

        /**
        * Called at reaching home after MoveTargetedHome
        * 被召唤的生物跑回其默认位置后回调
        */
        virtual void SummonedJustReachedHome(Creature* /*summoned*/) {}

        /**
         * Called at any Heal received from any Unit
         * @param pHealer Unit* which deals the heal
         * @param uiHealedAmount Amount of healing received
         * 受到任意单位治疗时回调（可修改治疗量）
         */
        virtual void HealedBy(Unit* /*healer*/, uint32& /*healedAmount*/) {}

        /**
         * Called at any Damage to any victim (before damage apply)
         * @param pDoneTo Unit* to whom Damage of amount uiDamage will be dealt
         * @param uiDamage Amount of Damage that will be dealt, can be changed here
         * 对目标造成伤害前回调（可修改伤害数值）
         */
        virtual void DamageDeal(Unit* /*doneTo*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellEntry const* /*spellInfo*/) {}

        /**
         * Called at any Damage from any attacker (before damage apply)
         * Note:    Use for recalculation damage or special reaction at damage
         *          for attack reaction use AttackedBy called for not DOT damage in Unit::DealDamage also
         * @param pDealer Unit* who will deal Damage to the creature
         * @param uiDamage Amount of Damage that will be dealt, can be changed here
         * 受到伤害生效前回调（可用于减伤/锁血/反伤）
         */
        virtual void DamageTaken(Unit* /*dealer*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellEntry const* /*spellInfo*/) {}

        /**
         * Called when the creature is killed
         * @param pKiller Unit* who killed the creature
         * 单位死亡时回调
         */
        virtual void JustDied(Unit* /*killer*/);

        /**
         * Called when the corpse of this creature gets removed
         * @param uiRespawnDelay Delay (in seconds). If != 0, then this is the time after which the creature will respawn, if = 0 the default respawn-delay will be used
         * 尸体消失时回调（可调整重生延迟时间）
         */
        virtual void CorpseRemoved(uint32& /*respawnDelay*/) {}

        /**
         * Called when a summoned creature is killed
         * @param pSummoned Summoned Creature* that got killed
         * 本单位召唤出的生物被击杀时回调
         */
        virtual void SummonedCreatureJustDied(Creature* /*summoned*/) {}

        /**
         * Called when the creature kills a unit
         * @param pVictim Victim that got killed
         * 本单位击杀目标时回调
         */
        virtual void KilledUnit(Unit* /*victim*/) {}

        /**
         * Called when owner of m_creature (if m_creature is PROTECTOR_PET) kills a unit
         * @param pVictim Victim that got killed (by owner of creature)
         * 当所属主人击杀目标时回调
         */
        virtual void OwnerKilledUnit(Unit* /*victim*/) {}

        /**
         * Called when the creature summon successfully other creature
         * @param pSummoned Creature that got summoned
         * 成功召唤其他生物时回调
         */
        virtual void JustSummoned(Creature* /*summoned*/) {}

        /**
         * Called when the creature summon successfully a gameobject
         * @param pGo GameObject that was summoned
         * 成功召唤游戏对象时回调
         */
        virtual void JustSummoned(GameObject* /*go*/) {}

        /**
         * Called when a summoned creature gets TemporarySummon::UnSummon ed
         * @param pSummoned Summoned creature that despawned
         * 已召唤的临时生物消失时回调
         */
        virtual void SummonedCreatureDespawn(Creature* /*summoned*/) {}

        /**
         * Called when hit by a spell
         * @param pCaster Caster who casted the spell
         * @param pSpell The spell that hit the creature
         * 被法术命中时回调
         */
        virtual void SpellHit(Unit* /*caster*/, const SpellEntry* /*spellInfo*/) {}

        /**
         * Called when spell hits creature's target
         * @param pTarget Target that we hit with the spell
         * @param pSpell Spell with which we hit pTarget
         * 本单位施放的法术命中目标时回调
         */
        virtual void SpellHitTarget(Unit* target, const SpellEntry* spellInfo, SpellMissInfo /*missInfo*/) { SpellHitTarget(target, spellInfo); }
        virtual void SpellHitTarget(Unit* /*target*/, const SpellEntry* /*spellInfo*/) {}

        /**
         * Called when the creature is target of hostile action: swing, hostile spell landed, fear/etc)
         * @param pAttacker Unit* who attacked the creature
         * 遭到敌对攻击行为（挥击、敌对法术命中等）时回调
         */
        virtual void AttackedBy(Unit* attacker);

        /**
         * Called when creature is respawned (for reseting variables)
         * 重生时回调（用于重置状态变量）
         */
        virtual void JustRespawned() {}

        /**
         * Called at waypoint reached or point movement finished
         * @param uiMovementType Type of the movement (enum MovementGeneratorType)
         * @param uiData Data related to the finished movement (ie point-id)
         * 移动至路径点或目标点完成时回调
         */
        virtual void MovementInform(uint32 /*movementType*/, uint32 /*data*/) {}

        /**
         * Called if a temporary summoned of m_creature reach a move point
         * @param pSummoned Summoned Creature that finished some movement
         * @param uiMotionType Type of the movement (enum MovementGeneratorType)
         * @param uiData Data related to the finished movement (ie point-id)
         * 被召唤生物到达移动目标点时回调
         */
        virtual void SummonedMovementInform(Creature* /*summoned*/, uint32 /*motionType*/, uint32 /*data*/) {}

        /**
         * Called at text emote receive from player
         * @param pPlayer Player* who sent the emote
         * @param uiEmote ID of the emote the player used with the creature as target
         * 收到玩家以本单位为目标的表情动作时回调
         */
        virtual void ReceiveEmote(Player* /*player*/, uint32 /*emote*/) {}

        ///== Data Fetch bind ==============================

        virtual uint32 GetScriptData() { return 0; }

        ///== Triggered Actions Requested ==================

        /**
         * Called when creature attack expected (if creature can and no have current victim)
         * Note: for reaction at hostile action must be called AttackedBy function.
         * Note: Usually called by MoveInLineOfSight, in Unit::SelectHostileTarget or when the AI is forced to attack an enemy
         * @param pWho Unit* who is possible target
         * 尝试开始攻击指定目标时调用
         */
        virtual void AttackStart(Unit* /*who*/);

        /**
        * Called when creature stop attack expected
        * Note: have to be called to reinitialize some states or movement
        * Note: Not needed for creature, they use EnterEvadeMode
        * 停止攻击状态重置时调用
        */
        virtual void CombatStop() {}

        /**
         * Called at World update tick, by default every 100ms
         * This setting is dependend on CONFIG_UINT32_INTERVAL_MAPUPDATE
         * Note: Use this function to handle Timers, Threat-Management and MeleeAttacking
         * @param uiDiff Passed time since last call
         * AI 主更新循环：驱动计时器、仇恨目标刷新、法术列表及近战平砍
         */
        virtual void UpdateAI(const uint32 /*diff*/);

        ///== State checks =================================

        /**
         * Check if unit is visible for MoveInLineOfSight
         * Note: This check is by default only the state-depending (visibility, range), NOT LineOfSight
         * @param pWho Unit* who is checked if it is visisble for the creature
         * 检查单位是否处于视野检测范围内且相互可见
         */
        virtual bool IsVisible(Unit* /*who*/) const;

        /// Check if this AI can be replaced in possess case
        // virtual bool IsControllable() const { return false; }

        ///== Helper functions =============================

        /// This function is used to do the actual melee damage (if possible)
        // 若满足攻击间隔与距离条件则执行一次近战平砍
        bool DoMeleeAttackIfReady() const;

        /// Internal helper function, to check if a spell can be cast
        // 内部辅助函数：校验法术是否满足施放条件
        CanCastResult CanCastSpell(Unit* target, const SpellEntry* spellInfo, bool isTriggered) const;

        /**
         * Function to cast a spell if possible
         * @param pTarget Unit* onto whom the spell should be cast
         * @param uiSpell ID of the spell that the creature will try to cast
         * @param uiCastFlags Some flags to define how to cast, see enum CastFlags
         * @param OriginalCasterGuid the original caster of the spell if required, empty by default
         * 按标志位尝试对目标施放指定法术并返回结果码
         */
        virtual CanCastResult DoCastSpellIfCan(Unit* target, uint32 spellId, uint32 castFlags = 0);

        /// Set combat movement (on/off), also sets UNIT_STAT_NO_COMBAT_MOVEMENT
        // 开关战斗追击移动能力
        void SetCombatMovement(bool enable, bool stopOrStartMovement = false);
        bool IsCombatMovement() const;
        void SetFollowMovement(bool enable);

        ///== Event Handling ===============================

        /**
         * Send an AI Event to nearby Creatures around
         * @param uiType number to specify the event, default cases listed in enum AIEventType
         * @param pInvoker Unit that triggered this event (like an attacker)
         * @param uiDelay  delay time until the Event will be triggered
         * @param fRadius  range in which for receiver is searched
         * 向周围指定半径内的友方生物广播 AI 事件
         */
        void SendAIEventAround(AIEventType eventType, Unit* invoker, uint32 delay, float radius, uint32 miscValue = 0) const;

        /**
         * Send an AI Event to a Creature
         * @param eventType to specify the event, default cases listed in enum AIEventType
         * @param pInvoker Unit that triggered this event (like an attacker)
         * @param pReceiver Creature to receive this event
         * 向单个目标生物发送 AI 事件
         */
        void SendAIEvent(AIEventType eventType, Unit* invoker, Unit* receiver, uint32 miscValue = 0) const;

        /**
         * Called when an AI Event is received
         * @param eventType to specify the event, default cases listed in enum AIEventType
         * @param pSender Creature that sent this event
         * @param pInvoker Unit that triggered this event (like an attacker)
         * 接收到来自其他生物发送的 AI 事件时回调
         */
        virtual void ReceiveAIEvent(AIEventType /*eventType*/, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) {}

        /**
        * Called when a Game Event starts or ends
        * @param eventId to specify id of event from database
        * @param activate to specify if it started or stopped
        * @param resume to specify whether it launched normally or was resumed after a restart
        * 当游戏活动 (GameEvent) 开启或结束时回调
        */
        virtual void OnEventHappened(uint16 /*eventId*/, bool /*activate*/, bool /*resume*/) {}

        /*
         * Evaluates conditions and returns true if it is going to assist player
         * 判断是否协助附近玩家战斗
         */
        virtual bool AssistPlayerInCombat(Unit* /*who*/) { return false; }

        /*
         * Notifies AI on cast state change
         * 施法状态变化通知（如开始施法或结束施法，控制近战暂停与远程模式切换）
         */
        virtual void OnSpellCastStateChange(Spell const* spell, bool state, WorldObject* target = nullptr);

        /*
         * Notifies AI on channel state update
         * 引导法术状态变化通知
         */
        virtual void OnChannelStateChange(Spell const* spell, bool state, WorldObject* target = nullptr);

        /*
         * Notifies AI on successful spelllist spell cast
         * 法术列表法术成功施放时通知
         */
        virtual void OnSpellCast(SpellEntry const* /*spellInfo*/, Unit* /*target*/) {}

        /*
         * Notifies AI on stealth alert for player nearby
         * 潜行玩家靠近触发警觉时通知
         */
        virtual void OnStealthAlert(Unit* /*who*/) {}

        /*
         * Notifies AI on evade from combat due to leash
         * 因超出牵引范围脱离战斗时通知
         */
        virtual void OnLeash() {}

        /*
         * Notifies AI on object heartbeat
         * 对象心跳触发通知
         */
        virtual void OnHeartbeat() {}

        /*
         * Notifies AI on being called for help
         * 响应呼救进入战斗时通知
         */
        virtual void OnCallForHelp(Unit* /*enemy*/) {}

        /*
         * Notifies AI on pet/totem unsummon - warning: works only for pets/totems
         * 宠物或图腾取消召唤时通知
         */
        virtual void OnUnsummon() {}

        void CheckForHelp(Unit* /*who*/, Unit* /*me*/, float /*dist*/);
        void DetectOrAttack(Unit* who);                                 // 在视野检测中处理潜行警觉或主动开怪
        bool CanTriggerStealthAlert(Unit* who, float attackRadius) const;

        virtual void HandleMovementOnAttackStart(Unit* victim, bool targetChange) const; // 开始攻击或切换目标时更新追击移动


        // TODO: Implement proper casterAI in EAI and remove this from Leotheras script
        uint32 GetAttackDistance() { return m_attackDistance; }
        void SetMoveChaseParams(float dist, float angle, bool moveFurther) { m_attackDistance = dist; m_attackAngle = angle; m_moveFurther = moveFurther; }

        // Returns friendly unit with the most amount of hp missing from max hp - ignoreSelf - some spells cant target self
        // 在范围内查找损失生命值最多（按绝对值或百分比）的友方单位
        Unit* DoSelectLowestHpFriendly(float range, float minMissing = 1.f, bool percent = false, bool targetSelf = true) const;
        // 查找满足指定数据库 UnitCondition 的友方单位
        Unit* DoSelectConditionalFriendly(float range, int32 unitConditionId) const;
        float CalculateSpellRange(SpellEntry const* spellInfo) const;
        // 查找范围内带有指定驱散类型或机制的可驱散友方生物列表
        CreatureList DoFindFriendlyEligibleDispel(uint32 spellId, bool self = true) const;
        CreatureList DoFindFriendlyEligibleDispel(SpellEntry const* spellInfo, bool self = true) const;
        CreatureList DoFindFriendlyEligibleDispel(float range, uint32 dispelMask = 0, uint32 mechanicMask = 0, bool self = true) const;
        // 查找范围内缺少指定增益光环的友方生物列表
        CreatureList DoFindFriendlyMissingBuff(float range, uint32 spellId, bool inCombat, bool self = true, uint32 spawnGroupId = 0) const;
        CreatureList DoFindFriendlyMissingBuff(SpellEntry const* spellInfo, bool inCombat, bool self = true, uint32 spawnGroupId = 0) const;

        // Start movement toward victim (启动朝向目标的追击移动)
        void DoStartMovement(Unit* victim);

        void SetReactState(ReactStates st) { m_reactState = st; }
        ReactStates GetReactState() const { return m_reactState; }
        bool HasReactState(ReactStates state) const { return (m_reactState == state); }

        virtual bool CanExecuteCombatAction();                          // 校验当前是否允许执行战斗脚本/技能动作
        bool CanCastSpell();                                            // 检查是否处于未在施法且非特殊 AI 指令状态
        void SetCombatScriptStatus(bool state) { m_combatScriptHappening = state; };
        bool GetCombatScriptStatus() const { return m_combatScriptHappening; }

        void SetAIOrder(AIOrders order) { m_currentAIOrder = order; }
        AIOrders GetAIOrder() const { return m_currentAIOrder; }

        bool DoFlee(uint32 duration = 0);                               // 触发定时逃跑移动
        virtual bool DoRetreat() { return false; } // implemented for creatures (撤退求援，由 CreatureAI 实现)
        void DoDistance(); // TODO
        virtual void DoCallForHelp(float /*radius*/) {} // implemented for creatures (向周围呼救，由 CreatureAI 实现)

        // Drops all threat to 0%. Does not remove enemies from the threat list (将仇恨列表内所有敌人的仇恨清零而不移除引用)
        void DoResetThreat();

        void SetMeleeEnabled(bool state);                               // 启用/禁用普通近战自动攻击

        // Rough prototype, we might not need such fidelity
        virtual void JustRootedTarget(SpellEntry const* spellInfo, Unit* victim) { JustStoppedMovementOfTarget(spellInfo, victim); }
        virtual void JustStunnedTarget(SpellEntry const* spellInfo, Unit* victim) { JustStoppedMovementOfTarget(spellInfo, victim); }

        // AI selection - works in connection with IsPossessCharmType (是否支持在被魅惑期间自行接管 AI)
        virtual bool CanHandleCharm() const { return false; }
        virtual void JustGotCharmed(Unit* /*charmer*/) {}

        // Movement generator responses (移动生成器完成回调)
        virtual void TimedFleeingEnded();
        virtual void RetreatingArrived() {}
        virtual void RetreatingEnded() {}

        void AttackSpecificEnemy(std::function<void(Unit*,Unit*&)> check); // 根据自定义筛选器在邻近敌对目标中选敌开战
        virtual void AttackClosestEnemy();                              // 攻击距离最近的可见敌对单位

        void SetAIImmobilizedState(bool apply, bool combatOnly = false); // must call parent JustDied if this is used (AI 主动定身控制，combatOnly 为 true 时仅限战斗期间生效)
        void ClearCombatOnlyRoot();

        virtual void HandleDelayedInstantAnimation(SpellEntry const* /*spellInfo*/); // 延迟瞬发技能动作播放钩子
        virtual bool IsTargetingRestricted() { return GetCombatScriptStatus(); }

        virtual void OnTaunt() {}

        virtual void HandleAssistanceCall(Unit* /*sender*/, Unit* /*invoker*/) {} // implemented for creatures (处理友军支援请求，由 CreatureAI 实现)

        virtual bool IsPreventingDeath() const { return false; }        // 是否开启免死保护

        bool IsMeleeEnabled() const { return m_meleeEnabled; }

        // EAI compatibility layer (EAI 事件计时器兼容更新接口)
        virtual void UpdateEventTimers(const uint32 /*diff*/) {}

        // Combat AI components (战斗动作队列执行入口)
        virtual void ExecuteActions();
        virtual void ExecuteAction(uint32 /*action*/) {}

        // Caster AI components (远程施法者 AI 控制接口)
        void AddMainSpell(uint32 spellId);
        void AddDistanceSpell(uint32 spellId) { m_distanceSpells.insert(spellId); }

        void SetRangedMode(bool state, float distance, RangeModeType type); // 配置远程施法模式及期望保持的追击距离
        void SetCurrentRangedMode(bool state);                              // 实时切换当前是否处于远程追击模式

        bool GetCurrentRangedMode() { return m_currentRangedMode; }

        virtual void JustStoppedMovementOfTarget(SpellEntry const* spell, Unit* victim); // 成功定身/昏迷目标后触发远程走位拉扯

        /*
         * Called when a spell is interrupted
         * @param spellInfo to specify which spell was interrupted
         * 法术被打断时回调（例如主法术系被反制时切换近战模式）
         */
        virtual void OnSpellInterrupt(SpellEntry const* spellInfo);

        /*
         * Notifies AI on successful spell execution
         * 法术进入冷却时回调（若主法术进入冷却且需近战切换则调整模式）
         */
        virtual void OnSpellCooldownAdded(SpellEntry const* spellInfo);

        virtual void DistancingStarted();                               // 开始远离目标拉扯
        virtual void DistancingEnded();                                 // 远离拉扯结束

        void DistanceYourself();                                        // 远离当前目标至主法术射程位置
        CanCastResult HandleSpellCastResult(CanCastResult result, SpellEntry const* spellInfo); // 处理施法失败结果（缺蓝/超距切换追击等）

        /*
         * All units which attack at ranged need to return true. It is used for 3 purposes:
         * Determining if a creature should ignore ranged targets during root
         * Attacking enemies flying overhead at aggro range
         * Checking main spell school instead of melee school for immunity suppress - TODO
         * 所有远程攻击单位应返回 true，用于定身跳过远程、攻击空中目标及免疫压制学派判断
         */
        bool IsRangedUnit() { return m_currentRangedMode; }
        SpellSchoolMask GetMainAttackSchoolMask() const;

        bool IsMainSpellPrevented(SpellEntry const* spellInfo) const;   // 检查主法术是否因冷却/沉默/缺蓝不可用
        bool IsEligibleForDistancing() const;                           // 检查是否满足执行后撤拉扯的条件

        // Spell Lists (生物法术列表驱动系统)
        // called when owner spell list changes - need to recalculate main spell and a few other things
        void SpellListChanged();                                        // 法术列表变更时重新提取主法术与初始信息
        void UpdateSpellLists();                                        // 每帧按优先级轮询并施放法术列表中的可用技能
        std::pair<bool, Unit*> ChooseTarget(CreatureSpellListTargeting* targetData, uint32 spellId) const; // 根据法术列表目标配置选择施法对象
        virtual CreatureSpellList const& GetSpellList() const = 0;
        void AddInitialCooldowns();                                     // 为法术列表内技能初始化开场冷却

        // compatibility layer for removing script_waypoint from escort ai using waypoint_path
        virtual uint32 GetCurrentWaypointPath() const { return 0; }

        //////////////////////////////////////////////////////////////////////////
        // Some group Event/Action, not sure best place is here
        //////////////////////////////////////////////////////////////////////////
        // member of the group got killed (生物小队成员死亡回调)
        virtual void CreatureGroupMemberDied(Unit* /*killed*/) {}

        virtual void RequestFollow(Unit* /*followee*/) {}
        virtual void RelinquishFollow(ObjectGuid /*follower*/) {}

    protected:
        virtual std::string GetAIName() { return "UnitAI"; }
        void DespawnGuids(GuidVector& spawns); // despawns all creature guids and clears contents (销毁列表中所有临时召唤生物并清空数组)

        ///== Fields =======================================

        /// Pointer to the Creature controlled by this AI (受本 AI 控制的单位指针)
        Unit* m_unit;

        /// How should an enemy be chased (近战/远程追击距离、角度、是否继续贴近与奔跑标志)
        float m_attackDistance;
        float m_attackAngle;
        bool m_moveFurther;
        bool m_chaseRun;

        // How far a creature can detect in MoveInLineOfSight (视野索敌最大检测距离)
        float m_visibilityDistance;

        bool m_combatMovementStarted;                                   // 是否已启动战斗追击移动

        bool m_dismountOnAggro;                                         // 进入战斗时是否自动下马

        bool m_meleeEnabled;                              // If we allow melee auto attack (是否允许近战自动攻击)
        bool m_combatOnlyRoot;                                          // 是否为仅战斗期间生效的 AI 定身

        ReactStates m_reactState;                                       // 当前 AI 反应姿态

        bool m_combatScriptHappening;                    // disables normal combat functions without leaving combat (脚本动作占用标志：暂停常规战斗操作但不脱战)
        AIOrders m_currentAIOrder;                                      // 当前最高优先级 AI 移动指令

        Spell const* m_currentSpell;                                    // 当前正在施放/引导的法术对象

        bool m_teleportUnreachable;                                     // 目标不可达时是否允许传送目标/自身

        // Caster AI components (施法者 AI 运行时状态)
        bool m_rangedMode;                                              // 是否启用远程模式逻辑
        RangeModeType m_rangedModeSetting;                              // 远程模式配置类型
        float m_chaseDistance;                                          // 远程模式维持的追击距离
        bool m_currentRangedMode;                                       // 当前是否正处于远程站位状态
        std::unordered_set<uint32> m_mainSpells;                        // 主输出法术 ID 集合
        std::unordered_set<uint32> m_distanceSpells;                    // 触发走位拉扯的法术 ID 集合
        uint32 m_mainSpellId;                                           // 当前主法术 ID
        uint32 m_mainSpellCost;                                         // 主法术能量消耗
        SpellEntry const* m_mainSpellInfo;                              // 主法术 DBC 条目
        float m_mainSpellMinRange;                                      // 主法术最小射程
        SpellSchoolMask m_mainAttackMask;                               // 主攻击法术学派掩码
        bool m_distancingCooldown;                                      // 走位拉扯冷却标志

        // Spell Lists (法术列表冷却控制)
        bool m_spellListCooldown;                                       // 法术列表公共施法间隔冷却标志
};

// 可参与 FactorySelector::selectAI 评分选择的 AI 工厂基类
struct SelectableAI : public FactoryHolder<UnitAI>, public Permissible<Creature>
{
    SelectableAI(const char* id) : FactoryHolder<UnitAI>(id) {}
};

// 具体生物 AI 工厂模板
template<class REAL_AI>
struct CreatureAIFactory : public SelectableAI
{
    CreatureAIFactory(const char* name) : SelectableAI(name) {}

    UnitAI* Create(void*) const override;

    int Permit(const Creature* c) const override { return REAL_AI::Permissible(c); }
};

// AI 自动选择优先级许可度枚举（数值越大优先级越高）
enum Permitions
{
    PERMIT_BASE_NO                 = -1,                    // 不允许选择
    PERMIT_BASE_IDLE               = 1,                     // 空闲兜底级别（如 NullCreatureAI）
    PERMIT_BASE_REACTIVE           = 100,                   // 被动/反应级别
    PERMIT_BASE_PROACTIVE          = 200,                   // 常规主动生物级别（如 EventAI）
    PERMIT_BASE_FACTION_SPECIFIC   = 400,                   // 特定阵营/职业角色级别（如 GuardAI）
    PERMIT_BASE_SPECIAL            = 800                    // 特殊专属类型（如 PetAI/TotemAI/GuardianAI）
};

typedef FactoryHolder<UnitAI> CreatureAICreator;
typedef FactoryHolder<UnitAI>::FactoryHolderRegistry CreatureAIRegistry;
typedef FactoryHolder<UnitAI>::FactoryHolderRepository CreatureAIRepository;

#endif
