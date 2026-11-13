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

#ifndef MANGOS_SPELLAURADEFINES_H
#define MANGOS_SPELLAURADEFINES_H

#define MAX_AURAS 56                                        // 单位最大光环槽位数量
#define MAX_POSITIVE_AURAS 40                               // 单位最大正面光环槽位数量

// 光环标志位（用于下发客户端光环显示与交互控制）
enum AuraFlags
{
    AFLAG_NONE                  = 0x00,                     // 无标志
    AFLAG_EFFECT_0              = 0x01,                     // 启用效果槽 0
    AFLAG_EFFECT_1              = 0x02,                     // 启用效果槽 1
    AFLAG_EFFECT_2              = 0x04,                     // 启用效果槽 2
    AFLAG_NOT_CASTER            = 0x08,     // the target cast it: the client hides it by SPELL_ATTR_EX5_NO_CASTER_AURA_ICON, else by SPELL_ATTR_EX5_NO_TARGET_AURA_ICON (非施法者自身/目标施放：客户端据此结合属性隐藏图标)
    AFLAG_CANCELABLE            = 0x10,                     // 玩家可手动取消的光环

    AFLAG_MASK_ALL              = 0xFF                      // 全部光环标志掩码
};

/**
 * This is what's used in a Modifier by the Aura class
 * to tell what the Aura should modify.
 * 光环类型枚举：供 Aura 类的 Modifier 指定光环要修改的具体属性或行为。
 */
enum AuraType
{
    SPELL_AURA_NONE = 0,                                    // 无光环效果
    SPELL_AURA_BIND_SIGHT = 1,                              // 绑定视野（心灵视界等）
    SPELL_AURA_MOD_POSSESS = 2,                             // 控制/附身目标
    /**
     * The aura should do periodic damage, the function that handles
     * this is Aura::HandlePeriodicDamage, the amount is usually decided
     * by the Unit::SpellDamageBonusDone or Unit::MeleeDamageBonusDone
     * which increases/decreases the Modifier::m_amount
     * 周期性伤害（DoT），由 Aura::HandlePeriodicDamage 处理，伤害量受法术/近战伤害加成影响
     */
    SPELL_AURA_PERIODIC_DAMAGE = 3,
    /**
     * Used by Aura::HandleAuraDummy
     * 虚拟占位光环，由 Aura::HandleAuraDummy 或脚本处理自定义逻辑
     */
    SPELL_AURA_DUMMY = 4,
    /**
     * Used by Aura::HandleModConfuse, will either confuse or unconfuse
     * the target depending on whether the apply flag is set
     * 迷惑效果，由 Aura::HandleModConfuse 根据 apply 标志施加或解除迷惑
     */
    SPELL_AURA_MOD_CONFUSE = 5,
    SPELL_AURA_MOD_CHARM = 6,                               // 魅惑目标（使其成为友方受控单位）
    SPELL_AURA_MOD_FEAR = 7,                                // 恐惧效果（使目标四处逃窜）
    /**
     * The aura will do periodic heals of a target, handled by
     * Aura::HandlePeriodicHeal, uses Unit::SpellHealingBonusDone
     * to calculate whether to increase or decrease Modifier::m_amount
     * 周期性治疗（HoT），由 Aura::HandlePeriodicHeal 处理并计算治疗加成
     */
    SPELL_AURA_PERIODIC_HEAL = 8,
    /**
     * Changes the attackspeed, the Modifier::m_amount decides
     * how much we change in percent, ie, if the m_amount is
     * 50 the attackspeed will increase by 50%
     * 修改攻击速度百分比（m_amount 为正表示加速，如 50 表示提升 50%）
     */
    SPELL_AURA_MOD_ATTACKSPEED = 9,
    /**
     * Modifies the threat that the Aura does in percent,
     * the Modifier::m_miscvalue decides which of the SpellSchools
     * it should affect threat for.
     * \see SpellSchoolMask
     * 按百分比修改指定法术系别（m_miscvalue 为系别掩码）产生的威胁值
     */
    SPELL_AURA_MOD_THREAT = 10,
    /**
     * Just applies a taunt which will change the threat a mob has
     * Taken care of in Aura::HandleModThreat
     * 嘲讽效果，迫使目标攻击施法者（由 Aura::HandleModThreat 处理）
     */
    SPELL_AURA_MOD_TAUNT = 11,
    /**
     * Stuns targets in different ways, taken care of in
     * Aura::HandleAuraModStun
     * 昏迷效果，使目标无法移动和行动（由 Aura::HandleAuraModStun 处理）
     */
    SPELL_AURA_MOD_STUN = 12,
    /**
     * Changes the damage done by a weapon in any hand, the Modifier::m_miscvalue
     * will tell what school the damage is from, it's used as a bitmask
     * \see SpellSchoolMask
     * 修改指定系别（m_miscvalue 位掩码）的武器/法术造成的固定伤害值
     */
    SPELL_AURA_MOD_DAMAGE_DONE = 13,
    /**
     * Not handled by the Aura class but instead this is implemented in
     * Unit::MeleeDamageBonusTaken and Unit::SpellBaseDamageBonusTaken
     * 修改受到的固定伤害值（在 Unit::MeleeDamageBonusTaken 与 SpellBaseDamageBonusTaken 中实现）
     */
    SPELL_AURA_MOD_DAMAGE_TAKEN = 14,
    /**
     * Not handled by the Aura class, implemented in Unit::DealMeleeDamage
     * 伤害护盾（反伤护盾，如荆棘术、火焰之盾，在 Unit::DealMeleeDamage 中处理）
     */
    SPELL_AURA_DAMAGE_SHIELD = 15,
    /**
     * Taken care of in Aura::HandleModStealth, take note that this
     * is not the same thing as invisibility
     * 潜行效果（与隐身不同，由 Aura::HandleModStealth 处理）
     */
    SPELL_AURA_MOD_STEALTH = 16,
    /**
     * Not handled by the Aura class, implemented in Unit::isVisibleForOrDetect
     * which does a lot of checks to determine whether the person is visible or not,
     * the SPELL_AURA_MOD_STEALTH seems to determine how in/visible ie a rogue is.
     * 侦测潜行等级加成（在 Unit::isVisibleForOrDetect 中判定目标可见性）
     */
    SPELL_AURA_MOD_STEALTH_DETECT = 17,
    /**
     * Handled by Aura::HandleInvisibility, the Modifier::m_miscvalue in the struct
     * seems to decide what kind of invisibility it is with a bitflag. the miscvalue
     * decides which bit is set, ie: 3 would make the 3rd bit be set.
     * 隐身效果，由 Aura::HandleInvisibility 处理，m_miscvalue 决定隐身类型位掩码
     */
    SPELL_AURA_MOD_INVISIBILITY = 18,
    /**
     * Adds one of the kinds of detections to the possible detections.
     * As in SPEALL_AURA_MOD_INVISIBILITY the Modifier::m_miscvalue seems to decide
     * what kind of invisibility the Unit should be able to detect.
     * 侦测隐身效果，m_miscvalue 指定可侦测的隐身类型
     */
    SPELL_AURA_MOD_INVISIBILITY_DETECTION = 19,
    SPELL_AURA_OBS_MOD_HEALTH = 20,                         // 20,21 unofficial (非官方命名：周期性按固定值恢复/调整生命值)
    SPELL_AURA_OBS_MOD_MANA = 21,                           // 非官方命名：周期性按固定值恢复/调整法力值
    /**
     * Handled by Aura::HandleAuraModResistance, changes the resistance for a unit
     * the field Modifier::m_miscvalue decides which kind of resistance that should
     * be changed, for possible values see SpellSchools.
     * \see SpellSchools
     * 修改单位抗性/护甲（由 Aura::HandleAuraModResistance 处理，m_miscvalue 指定系别）
     */
    SPELL_AURA_MOD_RESISTANCE = 22,
    /**
     * Currently just sets Aura::m_isPeriodic to apply and has a special case
     * for Curse of the Plaguebringer.
     * 周期性触发法术（标记为周期性光环并按周期触发指定法术）
     */
    SPELL_AURA_PERIODIC_TRIGGER_SPELL = 23,
    /**
     * Just sets Aura::m_isPeriodic to apply
     * 周期性恢复能量/法力/怒气等资源
     */
    SPELL_AURA_PERIODIC_ENERGIZE = 24,
    /**
     * Changes whether the target is pacified or not depending on the apply flag.
     * Pacify makes the target silenced and have all it's attack skill disabled.
     * See: http://classic.wowhead.com/spell=6462
     * 安抚/平静效果，禁用目标的所有攻击技能
     */
    SPELL_AURA_MOD_PACIFY = 25,
    /**
     * Roots or unroots the target
     * 定身/缠绕效果，禁止目标移动
     */
    SPELL_AURA_MOD_ROOT = 26,
    /**
     * Silences the target and stops and spell casts that should be stopped,
     * they have the flag SpellPreventionType::SPELL_PREVENTION_TYPE_SILENCE
     * 沉默效果，阻止施放受沉默限制的法术并打断当前施法
     */
    SPELL_AURA_MOD_SILENCE = 27,
    SPELL_AURA_REFLECT_SPELLS = 28,                         // 法术反射（全系）
    SPELL_AURA_MOD_STAT = 29,                               // 修改基础属性（力量/敏捷/耐力/智力/精神）
    SPELL_AURA_MOD_SKILL = 30,                              // 修改专业或武器技能等级
    SPELL_AURA_MOD_INCREASE_SPEED = 31,                     // 提高移动速度（可叠加/取最大）
    SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED = 32,             // 提高骑乘移动速度
    SPELL_AURA_MOD_DECREASE_SPEED = 33,                     // 降低移动速度（减速效果）
    SPELL_AURA_MOD_INCREASE_HEALTH = 34,                    // 提高最大生命值（固定值）
    SPELL_AURA_MOD_INCREASE_ENERGY = 35,                    // 提高最大能量/法力/怒气等资源上限
    SPELL_AURA_MOD_SHAPESHIFT = 36,                         // 变形/姿态切换（如熊形态、防御姿态）
    SPELL_AURA_EFFECT_IMMUNITY = 37,                        // 免疫指定法术效果
    SPELL_AURA_STATE_IMMUNITY = 38,                         // 免疫指定光环状态
    SPELL_AURA_SCHOOL_IMMUNITY = 39,                        // 免疫指定法术系别
    SPELL_AURA_DAMAGE_IMMUNITY = 40,                        // 免疫指定类型伤害
    SPELL_AURA_DISPEL_IMMUNITY = 41,                        // 免疫指定驱散类型
    SPELL_AURA_PROC_TRIGGER_SPELL = 42,                     // 事件触发施放法术（Proc）
    SPELL_AURA_PROC_TRIGGER_DAMAGE = 43,                    // 事件触发固定伤害（Proc）
    SPELL_AURA_TRACK_CREATURES = 44,                        // 追踪指定类型生物（小地图显示）
    SPELL_AURA_TRACK_RESOURCES = 45,                        // 追踪采集资源（草药/矿点/宝箱）
    SPELL_AURA_46 = 46,                                     // Ignore all Gear test spells (保留/忽略所有装备测试法术)
    SPELL_AURA_MOD_PARRY_PERCENT = 47,                      // 修改招架几率百分比
    SPELL_AURA_48 = 48,                                     // One periodic spell (未使用的周期性法术占位)
    SPELL_AURA_MOD_DODGE_PERCENT = 49,                      // 修改躲闪几率百分比
    SPELL_AURA_MOD_BLOCK_SKILL = 50,                        // 修改盾牌格挡等级/技能
    SPELL_AURA_MOD_BLOCK_PERCENT = 51,                      // 修改格挡几率百分比
    SPELL_AURA_MOD_CRIT_PERCENT = 52,                       // 修改物理暴击几率百分比
    SPELL_AURA_PERIODIC_LEECH = 53,                         // 周期性生命吸取（造成伤害并治疗施法者）
    SPELL_AURA_MOD_HIT_CHANCE = 54,                         // 修改近战/物理命中几率
    SPELL_AURA_MOD_SPELL_HIT_CHANCE = 55,                   // 修改法术命中几率
    SPELL_AURA_TRANSFORM = 56,                              // 外观变身（修改模型 ID）
    SPELL_AURA_MOD_SPELL_CRIT_CHANCE = 57,                  // 修改法术暴击几率（全系）
    SPELL_AURA_MOD_INCREASE_SWIM_SPEED = 58,                // 提高游泳速度
    SPELL_AURA_MOD_DAMAGE_DONE_CREATURE = 59,               // 对特定生物类型造成的伤害加成
    SPELL_AURA_MOD_PACIFY_SILENCE = 60,                     // 同时安抚与沉默（无法攻击与施法）
    SPELL_AURA_MOD_SCALE = 61,                              // 修改单位模型缩放比例
    SPELL_AURA_PERIODIC_HEALTH_FUNNEL = 62,                 // 周期性生命通道（将自身生命转移给目标）
    SPELL_AURA_PERIODIC_MANA_FUNNEL = 63,                   // 周期性法力通道
    SPELL_AURA_PERIODIC_MANA_LEECH = 64,                    // 周期性法力吸取
    SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK = 65,            // 修改施法速度（不叠加类，如嗜血/英勇）
    SPELL_AURA_FEIGN_DEATH = 66,                            // 假死状态
    SPELL_AURA_MOD_DISARM = 67,                             // 缴械效果（禁用主手武器）
    SPELL_AURA_MOD_STALKED = 68,                            // 被追踪标记（小地图显形，如猎人印记）
    SPELL_AURA_SCHOOL_ABSORB = 69,                          // 吸收指定系别伤害（吸收盾）
    SPELL_AURA_EXTRA_ATTACKS = 70,                          // 赋予额外攻击次数（如风怒/剑精）
    SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL = 71,           // 修改指定法术系别的暴击几率
    SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT = 72,              // 按百分比修改指定系别法术的能量消耗
    SPELL_AURA_MOD_POWER_COST_SCHOOL = 73,                  // 按固定值修改指定系别法术的能量消耗
    SPELL_AURA_REFLECT_SPELLS_SCHOOL = 74,                  // 反射指定系别法术
    SPELL_AURA_MOD_LANGUAGE = 75,                           // 修改/掌握语言
    SPELL_AURA_FAR_SIGHT = 76,                              // 鹰眼/视界术视角切换
    SPELL_AURA_MECHANIC_IMMUNITY = 77,                      // 免疫指定法术机制（如减速、昏迷）
    SPELL_AURA_MOUNTED = 78,                                // 骑乘状态（挂载坐骑模型）
    SPELL_AURA_MOD_DAMAGE_PERCENT_DONE = 79,                // 按百分比修改造成的伤害
    SPELL_AURA_MOD_PERCENT_STAT = 80,                       // 按百分比修改指定基础属性
    SPELL_AURA_SPLIT_DAMAGE_PCT = 81,                       // 按百分比分担伤害（如灵魂链接）
    SPELL_AURA_WATER_BREATHING = 82,                        // 水下呼吸
    SPELL_AURA_MOD_BASE_RESISTANCE = 83,                    // 修改基础护甲/抗性
    SPELL_AURA_MOD_REGEN = 84,                              // 修改脱战生命恢复速度
    SPELL_AURA_MOD_POWER_REGEN = 85,                        // 修改每 5 秒法力/能量恢复（MP5）
    SPELL_AURA_CHANNEL_DEATH_ITEM = 86,                     // 引导目标死亡时产生指定物品（如吸取灵魂）
    SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN = 87,               // 按百分比修改受到的伤害
    SPELL_AURA_MOD_HEALTH_REGEN_PERCENT = 88,               // 按百分比修改生命恢复
    SPELL_AURA_PERIODIC_DAMAGE_PERCENT = 89,                // 周期性按目标最大生命值百分比造成伤害
    SPELL_AURA_MOD_RESIST_CHANCE = 90,                      // 修改抵抗几率
    SPELL_AURA_MOD_DETECT_RANGE = 91,                       // 修改警戒/侦测距离（如安抚心灵）
    SPELL_AURA_PREVENTS_FLEEING = 92,                       // 阻止低血量逃跑
    SPELL_AURA_MOD_UNATTACKABLE = 93,                       // 不可被攻击状态（如放逐术）
    SPELL_AURA_INTERRUPT_REGEN = 94,                        // 中断自然生命/法力恢复
    SPELL_AURA_GHOST = 95,                                  // 灵魂状态
    SPELL_AURA_SPELL_MAGNET = 96,                           // 法术吸引（如根基图腾吸收有害法术）
    SPELL_AURA_MANA_SHIELD = 97,                            // 法力护盾（消耗法力吸收伤害）
    SPELL_AURA_MOD_SKILL_TALENT = 98,                       // 天赋提供的技能等级加成
    SPELL_AURA_MOD_ATTACK_POWER = 99,                       // 修改近战攻击强度
    SPELL_AURA_AURAS_VISIBLE = 100,                         // 侦测目标光环（可查看目标增益/减益）
    SPELL_AURA_MOD_RESISTANCE_PCT = 101,                    // 按百分比修改抗性/护甲
    SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS = 102,         // 对特定生物类型提高近战攻击强度
    SPELL_AURA_MOD_TOTAL_THREAT = 103,                      // 修改总威胁值产生
    SPELL_AURA_WATER_WALK = 104,                            // 水上行走
    SPELL_AURA_FEATHER_FALL = 105,                          // 缓落术（降低坠落速度与免摔伤）
    SPELL_AURA_HOVER = 106,                                 // 漂浮状态（如牧师漂浮术）
    SPELL_AURA_ADD_FLAT_MODIFIER = 107,                     // 为法术添加固定数值修正（天赋/雕文常用）
    SPELL_AURA_ADD_PCT_MODIFIER = 108,                      // 为法术添加百分比修正
    SPELL_AURA_ADD_TARGET_TRIGGER = 109,                    // 命中目标时额外触发法术
    SPELL_AURA_MOD_POWER_REGEN_PERCENT = 110,               // 按百分比修改能量恢复速度
    SPELL_AURA_ADD_CASTER_HIT_TRIGGER = 111,                // 施法者命中时触发法术
    SPELL_AURA_OVERRIDE_CLASS_SCRIPTS = 112,                // 覆盖/启用职业脚本标记
    SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN = 113,               // 修改受到的远程固定伤害
    SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT = 114,           // 按百分比修改受到的远程伤害
    SPELL_AURA_MOD_HEALING = 115,                           // 修改受到的固定治疗量
    SPELL_AURA_MOD_REGEN_DURING_COMBAT = 116,               // 战斗中保持一定比例的法力/生命恢复
    SPELL_AURA_MOD_MECHANIC_RESISTANCE = 117,               // 提高抵抗特定机制法术的几率
    SPELL_AURA_MOD_HEALING_PCT = 118,                       // 按百分比修改受到的治疗效果（如致死打击）
    SPELL_AURA_SHARE_PET_TRACKING = 119,                    // 共享宠物追踪（旧版/未使用）
    SPELL_AURA_UNTRACKABLE = 120,                           // 不可被追踪（屏蔽小地图追踪）
    SPELL_AURA_EMPATHY = 121,                               // 野兽知识（显示野兽属性与驯服信息）
    SPELL_AURA_MOD_OFFHAND_DAMAGE_PCT = 122,                // 按百分比修改副手武器伤害
    SPELL_AURA_MOD_TARGET_RESISTANCE = 123,                 // 忽略目标护甲或抗性（法术穿透/破甲）
    SPELL_AURA_MOD_RANGED_ATTACK_POWER = 124,               // 修改远程攻击强度
    SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN = 125,                // 修改受到的近战固定伤害
    SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT = 126,            // 按百分比修改受到的近战伤害
    SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS = 127,    // 攻击者对该目标的远程攻击强度加成
    SPELL_AURA_MOD_POSSESS_PET = 128,                       // 附身宠物（如野兽之眼）
    SPELL_AURA_MOD_SPEED_ALWAYS = 129,                      // 永久/强制修改移动速度（可与常规加速叠加）
    SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS = 130,              // 强制修改骑乘速度
    SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS = 131,        // 对特定生物类型提高远程攻击强度
    SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT = 132,           // 按百分比提高最大法力/能量上限
    SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT = 133,           // 按百分比提高最大生命值上限
    SPELL_AURA_MOD_MANA_REGEN_INTERRUPT = 134,              // 施法后精神回蓝（五秒规则内保持百分比回蓝）
    SPELL_AURA_MOD_HEALING_DONE = 135,                      // 修改造成的固定治疗量（治疗加成）
    SPELL_AURA_MOD_HEALING_DONE_PERCENT = 136,              // 按百分比修改造成的治疗量
    SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE = 137,             // 按百分比修改全属性（如王者祝福）
    SPELL_AURA_MOD_MELEE_HASTE = 138,                       // 修改近战急速（攻击速度）
    SPELL_AURA_FORCE_REACTION = 139,                        // 强制修改对指定阵营的声望反应态度
    SPELL_AURA_MOD_RANGED_HASTE = 140,                      // 修改远程急速
    SPELL_AURA_MOD_RANGED_AMMO_HASTE = 141,                 // 修改远程弹药攻击急速（如箭袋加速）
    SPELL_AURA_MOD_BASE_RESISTANCE_PCT = 142,               // 按百分比修改基础护甲/抗性
    SPELL_AURA_MOD_RESISTANCE_EXCLUSIVE = 143,              // 互斥型护甲/抗性加成（取最高值，如神圣之灵/光环类）
    SPELL_AURA_SAFE_FALL = 144,                             // 安全坠落（减少坠落伤害）
    SPELL_AURA_CHARISMA = 145,                              // 魅力光环（未使用）
    SPELL_AURA_PERSUADED = 146,                             // 说服光环（未使用）
    SPELL_AURA_MECHANIC_IMMUNITY_MASK = 147,                // 按位掩码免疫多种法术机制
    SPELL_AURA_RETAIN_COMBO_POINTS = 148,                   // 保留连击点数（更换目标不丢失）
    SPELL_AURA_RESIST_PUSHBACK  = 149,                      //    Resist Pushback (抵抗施法被打退)
    SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT = 150,             // 按百分比修改盾牌格挡值
    SPELL_AURA_TRACK_STEALTHED  = 151,                      //    Track Stealthed (追踪潜行单位，如追踪隐藏生物)
    SPELL_AURA_MOD_DETECTED_RANGE = 152,                    //    Mod Detected Range (修改被侦测范围)
    SPELL_AURA_SPLIT_DAMAGE_FLAT = 153,                     //    Split Damage Flat (固定值分担伤害)
    SPELL_AURA_MOD_STEALTH_LEVEL = 154,                     //    Stealth Level Modifier (潜行等级修正)
    SPELL_AURA_MOD_WATER_BREATHING = 155,                   //    Mod Water Breathing (水下呼吸时间修正)
    SPELL_AURA_MOD_REPUTATION_GAIN = 156,                   //    Mod Reputation Gain (声望获取加成)
    SPELL_AURA_PET_DAMAGE_MULTI = 157,                      //    Mod Pet Damage (宠物伤害倍率修正)
    SPELL_AURA_MOD_SHIELD_BLOCKVALUE = 158,                 // 修改盾牌固定格挡值
    SPELL_AURA_NO_PVP_CREDIT = 159,                         // 不提供 PvP 荣誉击杀奖励（荣誉无价值目标）
    SPELL_AURA_MOD_AOE_AVOIDANCE = 160,                     // 降低受到的群体（AoE）伤害/提高躲避
    SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT = 161,            // 战斗中持续生命恢复
    SPELL_AURA_POWER_BURN_MANA = 162,                       // 周期性法力燃烧并造成伤害
    SPELL_AURA_MOD_CRIT_DAMAGE_BONUS = 163,                 // 修改暴击伤害加成百分比
    SPELL_AURA_164 = 164,                                   // 保留光环类型 164
    SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS = 165,     // 攻击者对该目标的近战攻击强度加成
    SPELL_AURA_MOD_ATTACK_POWER_PCT = 166,                  // 按百分比修改近战攻击强度
    SPELL_AURA_MOD_RANGED_ATTACK_POWER_PCT = 167,           // 按百分比修改远程攻击强度
    SPELL_AURA_MOD_DAMAGE_DONE_VERSUS = 168,                // 对特定生物类型造成的伤害百分比加成
    SPELL_AURA_MOD_CRIT_PERCENT_VERSUS = 169,               // 对特定生物类型的暴击几率加成
    SPELL_AURA_DETECT_AMORE = 170,                          // 侦测隐形/爱慕（特殊视觉效果）
    SPELL_AURA_MOD_SPEED_NOT_STACK = 171,                   // 修改移动速度（不与同类叠加）
    SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK = 172,           // 修改骑乘移动速度（不叠加）
    SPELL_AURA_ALLOW_CHAMPION_SPELLS = 173,                 // 允许施放勇士法术（未使用）
    SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT = 174,      // by defeult intelect, dependent from SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT (按属性百分比提高法术伤害)
    SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT = 175,     // 按属性百分比提高治疗效果
    SPELL_AURA_SPIRIT_OF_REDEMPTION = 176,                  // 救赎之魂形态
    SPELL_AURA_AOE_CHARM = 177,                             // 群体魅惑
    SPELL_AURA_MOD_DEBUFF_RESISTANCE = 178,                 // 提高对负面效果的抵抗几率
    SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE = 179,        // 修改攻击者对该目标的法术暴击几率
    SPELL_AURA_MOD_FLAT_SPELL_DAMAGE_VERSUS = 180,          // 对特定生物类型提高固定法术伤害
    SPELL_AURA_MOD_FLAT_SPELL_CRIT_DAMAGE_VERSUS = 181,     // unused - possible flat spell crit damage versus (未使用：对特定生物类型提高固定法术暴击伤害)
    SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT = 182,        // 按属性百分比提高护甲/抗性
    SPELL_AURA_MOD_CRITICAL_THREAT = 183,                   // 修改暴击产生的威胁值比例
    SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE = 184,         // 修改攻击者对该目标的近战命中几率
    SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE = 185,        // 修改攻击者对该目标的远程命中几率
    SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE = 186,         // 修改攻击者对该目标的法术命中几率
    SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE = 187,        // 修改攻击者对该目标的近战暴击几率
    SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE = 188,       // 修改攻击者对该目标的远程暴击几率
    SPELL_AURA_MOD_RATING = 189,                            // 修改战斗等级（暴击/急速/命中/韧性等）
    SPELL_AURA_MOD_FACTION_REPUTATION_GAIN = 190,           // 修改特定阵营的声望获取百分比
    SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED = 191,             // 强制使用正常移动速度（忽略加速/减速）
    SPELL_AURA_MOD_MELEE_RANGED_HASTE = 192,                // 同时修改近战和远程急速
    SPELL_AURA_HASTE_ALL = 193,                             // 全急速加成（近战、远程、施法）
    SPELL_AURA_MOD_DEPRICATED_1  = 194,                     // not used now, old SPELL_AURA_MOD_SPELL_DAMAGE_OF_INTELLECT (已废弃：旧版智力转法伤)
    SPELL_AURA_MOD_DEPRICATED_2  = 195,                     // not used now, old SPELL_AURA_MOD_SPELL_HEALING_OF_INTELLECT (已废弃：旧版智力转治疗)
    SPELL_AURA_MOD_COOLDOWN = 196,                          // only 24818 Noxious Breath (修改法术冷却时间)
    SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE = 197, // 修改攻击者对该目标的法术与武器暴击几率
    SPELL_AURA_MOD_TARGET_RANGED_ATTACK_POWER_BY_ATTACKER = 198, // 攻击者根据受害者身上光环层数获得额外远程攻强 (TBC 2.4.3 猎人印记效果2)
    SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT = 199,        // 提高法术命中几率百分比
    SPELL_AURA_MOD_XP_PCT = 200,                            // 修改经验值获取百分比
    SPELL_AURA_FLY = 201,                                   // 允许飞行状态
    SPELL_AURA_IGNORE_COMBAT_RESULT = 202,                  // 忽略特定战斗结果（如不可躲闪/格挡）
    SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_DAMAGE = 203,        // 修改攻击者对该目标的近战暴击伤害（如韧性减免）
    SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE = 204,       // 修改攻击者对该目标的远程暴击伤害
    SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_DAMAGE = 205,        // 修改攻击者对该目标的法术暴击伤害
    SPELL_AURA_MOD_FLIGHT_SPEED = 206,                      // 修改飞行速度
    SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED = 207,              // 修改骑乘飞行速度
    SPELL_AURA_MOD_FLIGHT_SPEED_STACKING = 208,             // 可叠加飞行速度修正
    SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_STACKING = 209,     // 可叠加骑乘飞行速度修正
    SPELL_AURA_MOD_FLIGHT_SPEED_NOT_STACKING = 210,         // 不叠加飞行速度修正
    SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_NOT_STACKING = 211, // 不叠加骑乘飞行速度修正
    SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT = 212, // 按属性百分比提高远程攻击强度
    SPELL_AURA_MOD_RAGE_FROM_DAMAGE_DEALT = 213,            // 按造成伤害比例获得额外怒气（如狂暴姿态/怒火）
    SPELL_AURA_214 = 214,                                   // 保留光环类型 214
    SPELL_AURA_ARENA_PREPARATION = 215,                     // 竞技场准备阶段（免消耗等）
    SPELL_AURA_HASTE_SPELLS = 216,                          // 修改施法急速（如语言诅咒）
    SPELL_AURA_217 = 217,                                   // 保留光环类型 217
    SPELL_AURA_HASTE_RANGED = 218,                          // 修改远程急速
    SPELL_AURA_MOD_MANA_REGEN_FROM_STAT = 219,              // 按指定属性百分比转化为 MP5 法力恢复
    SPELL_AURA_MOD_RATING_FROM_STAT = 220,                  // 按指定属性百分比转化为战斗等级
    SPELL_AURA_DETAUNT = 221,                               // 降仇恨/消仇恨（如误导/假死类）
    SPELL_AURA_222 = 222,                                   // 保留光环类型 222
    SPELL_AURA_RAID_PROC_FROM_CHARGE = 223,                 // 团队触发消耗充能光环
    SPELL_AURA_224 = 224,                                   // 保留光环类型 224
    SPELL_AURA_RAID_PROC_FROM_CHARGE_WITH_VALUE = 225,      // 带数值的团队触发消耗充能光环（如愈合祷言）
    SPELL_AURA_PERIODIC_DUMMY = 226,                        // 周期性虚拟光环（每跳触发脚本回调）
    SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE = 227,     // 周期性带数值触发法术
    SPELL_AURA_DETECT_STEALTH = 228,                        // 增强潜行侦测范围/等级
    SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE = 229,              // 减少受到的 AoE 伤害百分比（如躲避）
    SPELL_AURA_MOD_INCREASE_MAX_HEALTH = 230,               // 提高最大生命值（命令怒吼类，可共存取大）
    SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE = 231,         // 带预设数值的事件触发法术
    SPELL_AURA_MECHANIC_DURATION_MOD = 232,                 // 按百分比修改特定机制效果的持续时间（可叠加）
    SPELL_AURA_233 = 233,                                   // 保留光环类型 233
    SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK = 234,       // 按百分比修改特定机制效果的持续时间（不叠加）
    SPELL_AURA_MOD_DISPEL_RESIST = 235,                     // 提高光环抵抗驱散的几率
    SPELL_AURA_236 = 236,                                   // 保留光环类型 236
    SPELL_AURA_MOD_SPELL_DAMAGE_OF_ATTACK_POWER = 237,      // 按攻击强度百分比转化为法术伤害加成
    SPELL_AURA_MOD_SPELL_HEALING_OF_ATTACK_POWER = 238,     // 按攻击强度百分比转化为法术治疗加成
    SPELL_AURA_MOD_SCALE_2 = 239,                           // 单位模型缩放修正 2
    SPELL_AURA_MOD_EXPERTISE = 240,                         // 修改精准等级
    SPELL_AURA_FORCE_MOVE_FORWARD = 241,                    // 强制向前移动
    SPELL_AURA_242 = 242,                                   // 保留光环类型 242
    SPELL_AURA_FACTION_OVERRIDE = 243,                      // 临时覆盖单位阵营模板
    SPELL_AURA_COMPREHEND_LANGUAGE = 244,                   // 理解指定语言
    SPELL_AURA_245 = 245,                                   // 保留光环类型 245
    SPELL_AURA_246 = 246,                                   // 保留光环类型 246
    SPELL_AURA_MIRROR_IMAGE = 247,                          // 镜像复制外观
    SPELL_AURA_MOD_COMBAT_RESULT_CHANCE = 248,              // 修改战斗结果几率（如碾压/未命中）
    SPELL_AURA_249 = 249,                                   // 保留光环类型 249
    SPELL_AURA_MOD_INCREASE_HEALTH_2 = 250,                 // 提高最大生命值 2
    SPELL_AURA_MOD_ENEMY_DODGE = 251,                       // 降低敌人躲闪几率
    SPELL_AURA_252 = 252,                                   // 保留光环类型 252
    SPELL_AURA_253 = 253,                                   // 保留光环类型 253
    SPELL_AURA_254 = 254,                                   // 保留光环类型 254
    SPELL_AURA_255 = 255,                                   // 保留光环类型 255
    SPELL_AURA_256 = 256,                                   // 保留光环类型 256
    SPELL_AURA_257 = 257,                                   // 保留光环类型 257
    SPELL_AURA_258 = 258,                                   // 保留光环类型 258
    SPELL_AURA_259 = 259,                                   // 保留光环类型 259
    SPELL_AURA_260 = 260,                                   // 保留光环类型 260
    SPELL_AURA_PHASE = 261,                                 // 位面/相位切换光环
    TOTAL_AURAS = 262                                       // 光环类型总数上限
};

// 区域光环作用目标类型
enum AreaAuraType
{
    AREA_AURA_PARTY,                                        // 仅作用于小队成员
    AREA_AURA_FRIEND,                                       // 作用于范围内友方单位
    AREA_AURA_ENEMY,                                        // 作用于范围内敌方单位
    AREA_AURA_PET,                                          // 作用于宠物/随从
    AREA_AURA_OWNER                                         // 作用于召唤者/主人
};

// Spell aura states (法术光环状态掩码，用于判定技能施放前提条件)
enum AuraState
{
    // (C) used in caster aura state     (T) used in target aura state
    // (c) used in caster aura state-not (t) used in target aura state-not
    // (C) 用于施法者光环状态检测       (T) 用于目标光环状态检测
    // (c) 用于施法者非某光环状态检测   (t) 用于目标非某光环状态检测
    AURA_STATE_DEFENSE                      = 1,            // C   | 防御反击状态（如复仇）
    AURA_STATE_HEALTHLESS_20_PERCENT        = 2,            // CcT | 生命值低于 20%（如斩杀）
    AURA_STATE_BERSERKING                   = 3,            // C T | 狂暴状态
    AURA_STATE_FROZEN                       = 4,            //  c t| frozen target (冻结状态，如冰枪术/碎冰)
    AURA_STATE_JUDGEMENT                    = 5,            // C   | 审判可用状态
    // AURA_STATE_UNKNOWN6                   = 6,           //     | not used (未使用)
    AURA_STATE_HUNTER_PARRY                 = 7,            // C   | 猎人招架反击状态（猫鼬撕咬）
    AURA_STATE_ROGUE_ATTACK_FROM_STEALTH    = 7,            // C   | FIX ME: not implemented yet! (潜行攻击状态，尚未实现)
    // AURA_STATE_UNKNOWN7c                  = 7,           //  c  | random/focused bursts spells (?) (未知状态)
    // AURA_STATE_UNKNOWN8                   = 8,           //     | not used (未使用)
    // AURA_STATE_UNKNOWN9                   = 9,           //     | not used (未使用)
    AURA_STATE_WARRIOR_VICTORY_RUSH         = 10,           // C   | warrior victory rush (战士乘胜追击状态)
    AURA_STATE_HUNTER_CRIT_STRIKE           = 10,           // C   | hunter crit strike (猎人暴击状态/杀戮命令)
    AURA_STATE_CRIT                         = 11,           // C   | 暴击触发状态
    AURA_STATE_FAERIE_FIRE                  = 12,           //  c t| 精灵之火状态
    AURA_STATE_HEALTHLESS_35_PERCENT        = 13,           // C T | 生命值低于 35%
    AURA_STATE_CONFLAGRATE                  = 14,           //   T | per-caster (点燃可用状态，按施法者区分)
    AURA_STATE_SWIFTMEND                    = 15,           //   T | 迅捷治愈可用状态（目标身上有回春/愈合）
    AURA_STATE_DEADLY_POISON                = 16,           //   T | 致命药膏状态（毒伤可用）
    AURA_STATE_FORBEARANCE                  = 17,           //  c t| 自律状态（圣盾/保护祝福互斥）
    AURA_STATE_WEAKENED_SOUL                = 18,           //    t| 虚弱灵魂状态（真言术：盾冷却）
    AURA_STATE_HYPOTHERMIA                  = 19            //  c  | 低温状态（寒冰屏障冷却）
};

/** @} */
#endif
