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

#ifndef __SPELL_DEFINES_H
#define __SPELL_DEFINES_H

#include "Common.h"

// ***********************************
// Spell Attributes definitions (法术属性标志位定义)
// ***********************************

// 法术基础属性标志（对应 DBC Attributes 字段）
enum SpellAttributes
{
    SPELL_ATTR_PROC_FAILURE_BURNS_CHARGE       = 0x00000001,// 0 触发失败时也消耗充能次数
    SPELL_ATTR_USES_RANGED_SLOT                = 0x00000002,// 1 All ranged abilites have this flag (使用远程武器槽位，所有远程技能均有此标志)
    SPELL_ATTR_ON_NEXT_SWING_NO_DAMAGE         = 0x00000004,// 2 on next swing (下一次近战挥击触发且无额外武器伤害)
    SPELL_ATTR_DO_NOT_LOG_IMMUNE_MISSES        = 0x00000008,// 3 not set in 3.0.3 (不记录免疫/未命中日志)
    SPELL_ATTR_IS_ABILITY                      = 0x00000010,// 4 Displays ability instead of spell clientside (客户端显示为“技能”而非“法术”)
    SPELL_ATTR_IS_TRADESKILL                   = 0x00000020,// 5 trade spells, will be added by client to a sublist of profession spell (商业技能/配方，归入专业子列表)
    SPELL_ATTR_PASSIVE                         = 0x00000040,// 6 Passive spell (被动法术)
    SPELL_ATTR_DO_NOT_DISPLAY                  = 0x00000080,// 7 Hidden in Spellbook, Aura Icon, Combat Log (在法术书、光环栏和战斗日志中隐藏)
    SPELL_ATTR_DO_NOT_LOG                      = 0x00000100,// 8 不写入战斗日志
    SPELL_ATTR_HELD_ITEM_ONLY                  = 0x00000200,// 9 Client automatically selects item from mainhand slot as a cast target (客户端自动选择主手武器作为施法目标)
    SPELL_ATTR_ON_NEXT_SWING                   = 0x00000400,// 10 on next swing 2 (下一次近战挥击时附加效果，如英勇打击)
    SPELL_ATTR_WEARER_CASTS_PROC_TRIGGER       = 0x00000800,// 11 由装备佩戴者触发施放
    SPELL_ATTR_DAYTIME_ONLY                    = 0x00001000,// 12 only useable at daytime, not set in 2.4.2 (仅白天可用)
    SPELL_ATTR_NIGHT_ONLY                      = 0x00002000,// 13 only useable at night, not set in 2.4.2 (仅夜间可用)
    SPELL_ATTR_ONLY_INDOORS                    = 0x00004000,// 14 only useable indoors, not set in 2.4.2 (仅室内可用)
    SPELL_ATTR_ONLY_OUTDOORS                   = 0x00008000,// 15 Only useable outdoors. (仅室外可用)
    SPELL_ATTR_NOT_SHAPESHIFT                  = 0x00010000,// 16 Not while shapeshifted (变形状态下不可用)
    SPELL_ATTR_ONLY_STEALTHED                  = 0x00020000,// 17 Must be in stealth (仅潜行状态下可用)
    SPELL_ATTR_DO_NOT_SHEATH                   = 0x00040000,// 18 client won't hide unit weapons in sheath on cast/channel TODO: Implement (施法/引导时不收起武器)
    SPELL_ATTR_SCALES_WITH_CREATURE_LEVEL      = 0x00080000,// 19 spelldamage depends on caster level (法术伤害随施法者等级缩放)
    SPELL_ATTR_CANCELS_AUTO_ATTACK_COMBAT      = 0x00100000,// 20 Stop attack after use this spell (and not begin attack if use) (使用后停止自动攻击)
    SPELL_ATTR_NO_ACTIVE_DEFENSE               = 0x00200000,// 21 Cannot be dodged/parried/blocked (不可被躲闪、招架或格挡)
    SPELL_ATTR_TRACK_TARGET_IN_CAST_PLAYER_ONLY= 0x00400000,// 22 SetTrackingTarget (施法过程中仅对玩家追踪目标朝向)
    SPELL_ATTR_ALLOW_CAST_WHILE_DEAD           = 0x00800000,// 23 castable while dead (死亡状态下可施放)
    SPELL_ATTR_ALLOW_WHILE_MOUNTED             = 0x01000000,// 24 castable while mounted (骑乘状态下可施放)
    SPELL_ATTR_COOLDOWN_ON_EVENT               = 0x02000000,// 25 Activate and start cooldown after aura fade or remove summoned creature or go (光环消失或召唤物移除后才进入冷却)
    SPELL_ATTR_AURA_IS_DEBUFF                  = 0x04000000,// 26 光环强制视为负面减益效果（Debuff）
    SPELL_ATTR_ALLOW_WHILE_SITTING             = 0x08000000,// 27 castable while sitting (坐下状态下可施放，如食物/饮水)
    SPELL_ATTR_NOT_IN_COMBAT_ONLY_PEACEFUL     = 0x10000000,// 28 Cannot be used in combat (战斗中不可使用)
    SPELL_ATTR_NO_IMMUNITIES                   = 0x20000000,// 29 unaffected by invulnerability (无视无敌等免疫效果)
    SPELL_ATTR_HEARTBEAT_RESIST                = 0x40000000,// 30 Chance for spell effects to break early (heartbeat resist) (心跳抵抗：控制效果周期性有几率提前解除)
    SPELL_ATTR_NO_AURA_CANCEL                  = 0x80000000,// 31 positive aura can't be canceled (正面光环不可手动取消)
};

// 法术扩展属性 1（对应 DBC AttributesEx 字段）
enum SpellAttributesEx
{
    SPELL_ATTR_EX_DISMISS_PET_FIRST            = 0x00000001,// 0 施法前先解散当前宠物
    SPELL_ATTR_EX_USE_ALL_MANA                 = 0x00000002,// 1 消耗全部剩余法力/怒气/能量
    SPELL_ATTR_EX_IS_CHANNELED                 = 0x00000004,// 2 引导类法术（需目标）
    SPELL_ATTR_EX_NO_REDIRECTION               = 0x00000008,// 3 不可被重定向/法术吸引（如根基图腾）
    SPELL_ATTR_EX_NO_SKILL_INCREASE            = 0x00000010,// 4 使用时不提升商业或武器技能点
    SPELL_ATTR_EX_ALLOW_WHILE_STEALTHED        = 0x00000020,// 5 允许在潜行状态下施放（不破潜行）
    SPELL_ATTR_EX_IS_SELF_CHANNELED            = 0x00000040,// 6 自我引导类法术（无需目标）
    SPELL_ATTR_EX_NO_REFLECTION                = 0x00000080,// 7 不可被法术反射
    SPELL_ATTR_EX_ONLY_PEACEFUL_TARGETS        = 0x00000100,// 8 Target must not be in combat (目标必须处于非战斗状态)
    SPELL_ATTR_EX_INITIATES_COMBAT_ENABLES_AUTO_ATTACK = 0x00000200,// 9 施放后进入战斗并开启自动攻击
    SPELL_ATTR_EX_NO_THREAT                    = 0x00000400,// 10 不产生任何威胁值
    SPELL_ATTR_EX_AURA_UNIQUE                  = 0x00000800,// 11 光环全局唯一（不可重复叠加）
    SPELL_ATTR_EX_FAILURE_BREAKS_STEALTH       = 0x00001000,// 12 施法失败也会打破潜行
    SPELL_ATTR_EX_TOGGLE_FARSIGHT              = 0x00002000,// 13 切换鹰眼/视界术视角
    SPELL_ATTR_EX_TRACK_TARGET_IN_CHANNEL      = 0x00004000,// 14 引导过程中持续面向目标
    SPELL_ATTR_EX_IMMUNITY_PURGES_EFFECT       = 0x00008000,// 15 auras which give immunity also punch through and purge the given effect (提供免疫的光环会驱散已有同类效果)
    SPELL_ATTR_EX_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS = 0x00010000,// 16 TODO: Investigate use (同时免疫敌对与友方效果)
    SPELL_ATTR_EX_NO_AUTOCAST_AI               = 0x00020000,// 17 宠物/AI 不自动施放
    SPELL_ATTR_EX_PREVENTS_ANIM                = 0x00040000,// 18 阻止播放施法动作动画
    SPELL_ATTR_EX_EXCLUDE_CASTER               = 0x00080000,// 19 spells with area effect or friendly targets that exclude the caster (范围或友方效果排除施法者自身)
    SPELL_ATTR_EX_FINISHING_MOVE_DAMAGE        = 0x00100000,// 20 Uses combo points (伤害随连击点数缩放的终结技)
    SPELL_ATTR_EX_THREAT_ONLY_ON_MISS          = 0x00200000,// 21 仅在未命中时产生威胁值
    SPELL_ATTR_EX_FINISHING_MOVE_DURATION      = 0x00400000,// 22 Uses combo points (持续时间随连击点数缩放的终结技)
    SPELL_ATTR_EX_IGNORE_OWNERS_DEATH          = 0x00800000,// 23 忽略拥有者/施法者死亡
    SPELL_ATTR_EX_SPECIAL_SKILLUP              = 0x01000000,// 24 特殊技能升级规则
    SPELL_ATTR_EX_AURA_STAYS_AFTER_COMBAT      = 0x02000000,// 25 脱离战斗后光环仍保留
    SPELL_ATTR_EX_REQUIRE_ALL_TARGETS          = 0x04000000,// 26 需要所有目标均合法
    SPELL_ATTR_EX_DISCOUNT_POWER_ON_MISS       = 0x08000000,// 27 未命中时返还部分能量消耗
    SPELL_ATTR_EX_NO_AURA_ICON                 = 0x10000000,// 28 不显示光环图标
    SPELL_ATTR_EX_NAME_IN_CHANNEL_BAR          = 0x20000000,// 29 在引导条中显示法术名称
    SPELL_ATTR_EX_COMBO_ON_BLOCK               = 0x40000000,// 30 NYI (格挡时获得连击点，未实现)
    SPELL_ATTR_EX_CAST_WHEN_LEARNED            = 0x80000000,// 31 学会时自动施放一次
};

// 法术扩展属性 2（对应 DBC AttributesEx2 字段）
enum SpellAttributesEx2
{
    SPELL_ATTR_EX2_ALLOW_DEAD_TARGET           = 0x00000001,// 0 can target dead unit or corpse (允许以死亡单位或尸体为目标)
    SPELL_ATTR_EX2_NO_SHAPESHIFT_UI            = 0x00000002,// 1 不在变形栏显示
    SPELL_ATTR_EX2_IGNORE_LINE_OF_SIGHT        = 0x00000004,// 2 忽略视线（LOS）阻挡检查
    SPELL_ATTR_EX2_ALLOW_LOW_LEVEL_BUFF        = 0x00000008,// 3 允许对低等级目标施放高等级增益（自动降级）
    SPELL_ATTR_EX2_USE_SHAPESHIFT_BAR          = 0x00000010,// 4 client displays icon in stance bar when learned, even if not shapeshift (学会后显示在姿态栏)
    SPELL_ATTR_EX2_AUTO_REPEAT                 = 0x00000020,// 5 自动重复施放（如魔杖射击/自动射击）
    SPELL_ATTR_EX2_CANNOT_CAST_ON_TAPPED       = 0x00000040,// 6 only usable on tapped by yourself (不能对已被他人抢占归属的目标施放)
    SPELL_ATTR_EX2_DO_NOT_REPORT_SPELL_FAILURE = 0x00000080,// 7 不向客户端报告施法失败原因
    SPELL_ATTR_EX2_INCLUDE_IN_ADVANCED_COMBAT_LOG = 0x00000100,// 8 not used anywhere - likely also had different meaning (高级战斗日志标志)
    SPELL_ATTR_EX2_ALWAYS_CAST_AS_UNIT         = 0x00000200,// 9 no idea (始终以单位身份施放)
    SPELL_ATTR_EX2_SPECIAL_TAMING_FLAG         = 0x00000400,// 10 no idea (特殊驯服标志)
    SPELL_ATTR_EX2_NO_TARGET_PER_SECOND_COSTS  = 0x00000800,// 11 无每秒目标维持消耗
    SPELL_ATTR_EX2_CHAIN_FROM_CASTER           = 0x00001000,// 12 链式法术从施法者自身起跳
    SPELL_ATTR_EX2_ENCHANT_OWN_ITEM_ONLY       = 0x00002000,// 13 仅可附魔自己的物品
    SPELL_ATTR_EX2_ALLOW_WHILE_INVISIBLE       = 0x00004000,// 14 允许在隐身状态下施放
    SPELL_ATTR_EX2_UNK15                       = 0x00008000,// 15 未知标志 15
    SPELL_ATTR_EX2_NO_ACTIVE_PETS              = 0x00010000,// 16 需要当前没有激活的宠物
    SPELL_ATTR_EX2_DO_NOT_RESET_COMBAT_TIMERS  = 0x00020000,// 17 suspend weapon timer instead of resetting it, (?Hunters Shot and Stings only have this flag?) (施法不重置普攻计时器，而是暂停)
    SPELL_ATTR_EX2_REQ_DEAD_PET                = 0x00040000,// 18 unconfirmed (需要宠物处于死亡状态)
    SPELL_ATTR_EX2_ALLOW_WHILE_NOT_SHAPESHIFTED= 0x00080000,// 19 仅未变形时可用
    SPELL_ATTR_EX2_INITIATE_COMBAT_POST_CAST   = 0x00100000,// 20 NYI (施法完成后进入战斗)
    SPELL_ATTR_EX2_FAIL_ON_ALL_TARGETS_IMMUNE  = 0x00200000,// 21 所有目标免疫时判定施法失败
    SPELL_ATTR_EX2_NO_INITIAL_THREAT           = 0x00400000,// 22 施放时不产生初始威胁值
    SPELL_ATTR_EX2_PROC_COOLDOWN_ON_FAILURE    = 0x00800000,// 23 施法失败也触发冷却
    SPELL_ATTR_EX2_ITEM_CAST_WITH_OWNER_SKILL  = 0x01000000,// 24 NYI (物品施法使用持有者技能等级)
    SPELL_ATTR_EX2_DONT_BLOCK_MANA_REGEN       = 0x02000000,// 25 maybe different meaning - doesnt hurt anything however (不触发五秒回蓝中断)
    SPELL_ATTR_EX2_NO_SCHOOL_IMMUNITIES        = 0x04000000,// 26 无视系别免疫
    SPELL_ATTR_EX2_IGNORE_WEAPONSKILL          = 0x08000000,// 27 NYI (忽略武器技能等级检查)
    SPELL_ATTR_EX2_NOT_AN_ACTION               = 0x10000000,// 28 非主动动作（不打断部分行为/光环）
    SPELL_ATTR_EX2_CANT_CRIT                   = 0x20000000,// 29 Spell can't crit (法术无法暴击)
    SPELL_ATTR_EX2_ACTIVE_THREAT               = 0x40000000,// 30 NYI - has some sort of active threat component like fire nova totem (主动威胁标志)
    SPELL_ATTR_EX2_RETAIN_ITEM_CAST            = 0x80000000,// 31 Possibly could have stacking implications in aura triggered spells because item guid is retained (保留触发物品 GUID)
};

// 法术扩展属性 3（对应 DBC AttributesEx3 字段）
enum SpellAttributesEx3
{
    SPELL_ATTR_EX3_PVP_ENABLING                = 0x00000001,// 0 Spell landed counts as hostile action against enemy even if it doesn't trigger combat state, propagates PvP flags (命中视为敌对行为并传播 PvP 标记)
    SPELL_ATTR_EX3_NO_PROC_EQUIP_REQUIREMENT   = 0x00000002,// 1 触发时忽略装备需求
    SPELL_ATTR_EX3_NO_CASTING_BAR_TEXT         = 0x00000004,// 2 施法条不显示文字
    SPELL_ATTR_EX3_COMPLETELY_BLOCKED          = 0x00000008,// 3 可被完全格挡
    SPELL_ATTR_EX3_NO_RES_TIMER                = 0x00000010,// 4 复活后不施加复活虚弱/复活计时器惩罚
    SPELL_ATTR_EX3_NO_DURABILITY_LOSS          = 0x00000020,// 5 不损耗装备耐久度
    SPELL_ATTR_EX3_NO_AVOIDANCE                = 0x00000040,// 6 不可被闪避/躲避
    SPELL_ATTR_EX3_DOT_STACKING_RULE           = 0x00000080,// 7 create a separate (de)buff stack for each caster (不同施法者的 DoT/光环各自独立存在)
    SPELL_ATTR_EX3_ONLY_ON_PLAYER              = 0x00000100,// 8 Can target only player (仅可对玩家目标施放)
    SPELL_ATTR_EX3_NOT_A_PROC                  = 0x00000200,// 9 aura periodic trigger is not evaluated as triggered (official meaning of proc) (周期性触发不被视为 Proc)
    SPELL_ATTR_EX3_REQUIRES_MAIN_HAND_WEAPON   = 0x00000400,// 10 需要装备主手武器
    SPELL_ATTR_EX3_ONLY_BATTLEGROUNDS          = 0x00000800,// 11 Can casted only on battleground (仅在战场中可用)
    SPELL_ATTR_EX3_ONLY_ON_GHOSTS              = 0x00001000,// 12 仅可对灵魂状态单位施放
    SPELL_ATTR_EX3_HIDE_CHANNEL_BAR            = 0x00002000,// 13 隐藏引导施法条
    SPELL_ATTR_EX3_HIDE_IN_RAID_FILTER         = 0x00004000,// 14 "Honorless Target" only these spells have this flag (在团队光环过滤器中隐藏)
    SPELL_ATTR_EX3_NORMAL_RANGED_ATTACK        = 0x00008000,// 15 Spells with this attribute are processed as ranged attacks in client (客户端按普通远程攻击处理)
    SPELL_ATTR_EX3_SUPPRESS_CASTER_PROCS       = 0x00010000,// 16 抑制施法者身上的触发效果（Proc）
    SPELL_ATTR_EX3_SUPPRESS_TARGET_PROCS       = 0x00020000,// 17 抑制目标身上的触发效果（Proc）
    SPELL_ATTR_EX3_ALWAYS_HIT                  = 0x00040000,// 18 Spell should always hit its target (必定命中，跳过命中判定)
    SPELL_ATTR_EX3_INSTANT_TARGET_PROCS        = 0x00080000,// 19 Related to spell batching - not an issue for us (即时目标触发)
    SPELL_ATTR_EX3_ALLOW_AURA_WHILE_DEAD       = 0x00100000,// 20 Death persistent spells (死亡后光环依然保留，如合剂/死亡持久 Buff)
    SPELL_ATTR_EX3_ONLY_PROC_OUTDOORS          = 0x00200000,// 21 仅在室外触发
    SPELL_ATTR_EX3_CASTING_CANCELS_AUTOREPEAT  = 0x00400000,// 22 施法时取消自动重复攻击
    SPELL_ATTR_EX3_NO_DAMAGE_HISTORY           = 0x00800000,// 23 不计入伤害历史/击杀归属
    SPELL_ATTR_EX3_REQUIRES_OFFHAND_WEAPON     = 0x01000000,// 24 Req offhand weapon (需要装备副手武器)
    SPELL_ATTR_EX3_TREAT_AS_PERIODIC           = 0x02000000,// 25 Treated as periodic spell (按周期性法术处理)
    SPELL_ATTR_EX3_CAN_PROC_FROM_PROCS         = 0x04000000,// 26 Auras with this attribute can proc off procced spells (periodic triggers etc) (允许由其他触发法术再次触发)
    SPELL_ATTR_EX3_ONLY_PROC_ON_CASTER         = 0x08000000,// 27 仅对施法者自身触发
    SPELL_ATTR_EX3_IGNORE_CASTER_AND_TARGET_RESTRICTIONS = 0x10000000,// 28 忽略施法者与目标限制条件
    SPELL_ATTR_EX3_IGNORE_CASTER_MODIFIERS     = 0x20000000,// 29 Resistances should still affect damage (忽略施法者伤害加成，但目标抗性仍生效)
    SPELL_ATTR_EX3_DO_NOT_DISPLAY_RANGE        = 0x40000000,// 30 不在客户端技能栏显示射程提示
    SPELL_ATTR_EX3_NOT_ON_AOE_IMMUNE           = 0x80000000,// 31 对 AoE 免疫的目标无效
};

// 法术扩展属性 4（对应 DBC AttributesEx4 字段）
enum SpellAttributesEx4
{
    SPELL_ATTR_EX4_NO_CAST_LOG                 = 0x00000001,// 0 不发送施法日志
    SPELL_ATTR_EX4_CLASS_TRIGGER_ONLY_ON_TARGET= 0x00000002,// 1 仅在目标上触发职业脚本
    SPELL_ATTR_EX4_AURA_EXPIRES_OFFLINE        = 0x00000004,// 2 玩家离线时照常计算光环过期时间
    SPELL_ATTR_EX4_NO_HELPFUL_THREAT           = 0x00000008,// 3 有益效果不产生威胁值
    SPELL_ATTR_EX4_NO_HARMFUL_THREAT           = 0x00000010,// 4 伤害/负面效果不产生威胁值
    SPELL_ATTR_EX4_ALLOW_CLIENT_TARGETING      = 0x00000020,// 5 允许客户端自定义目标选择
    SPELL_ATTR_EX4_CANNOT_BE_STOLEN            = 0x00000040,// 6 although such auras might be dispellable, they cannot be stolen (不可被法术偷取)
    SPELL_ATTR_EX4_ALLOW_CAST_WHILE_CASTING    = 0x00000080,// 7 Can use this spell while another is channeled/cast/autocast (允许在其他法术施放/引导期间同时施放)
    SPELL_ATTR_EX4_IGNORE_DAMAGE_TAKEN_MODIFIERS = 0x00000100,// 8 忽略目标受到伤害的百分比修正
    SPELL_ATTR_EX4_COMBAT_FEEDBACK_WHEN_USABLE = 0x00000200,// 9 initially disabled / trigger activate from event (Execute, Riposte, Deep Freeze end other) (仅在条件触发可用时亮起图标，如斩杀/还击)
    SPELL_ATTR_EX4_WEAPON_SPEED_COST_SCALING   = 0x00000400,// 10 消耗随武器速度缩放
    SPELL_ATTR_EX4_NO_PARTIAL_IMMUNITY         = 0x00000800,// 11 Curse of Tongues - 1.12.0 (无部分免疫)
    SPELL_ATTR_EX4_AURA_IS_BUFF                = 0x00001000,// 12 强制视为正面增益（Buff）
    SPELL_ATTR_EX4_DO_NOT_LOG_CASTER           = 0x00002000,// 13 战斗日志中不记录施法者
    SPELL_ATTR_EX4_REACTIVE_DAMAGE_PROC        = 0x00004000,// 14 反应式伤害触发（如受击反伤）
    SPELL_ATTR_EX4_NOT_IN_SPELLBOOK            = 0x00008000,// 15 不在法术书中显示
    SPELL_ATTR_EX4_NOT_IN_ARENA                = 0x00010000,// 16 竞技场中禁用
    SPELL_ATTR_EX4_IGNORE_DEFAULT_ARENA_RESTRICTIONS = 0x00020000,// 17 忽略竞技场默认限制（允许在竞技场使用）
    SPELL_ATTR_EX4_BOUNCY_CHAIN_MISSILES       = 0x00040000,// 18 弹跳链式弹道
    SPELL_ATTR_EX4_ALLOW_PROC_WHILE_SITTING    = 0x00080000,// 19 坐下状态下允许触发
    SPELL_ATTR_EX4_AURA_NEVER_BOUNCES          = 0x00100000,// 20 do not give "more powerful spell" error message (不提示“已存在更强法术”错误)
    SPELL_ATTR_EX4_ALLOW_ENTERING_ARENA        = 0x00200000,// 21 进入竞技场时保留（不移除）
    SPELL_ATTR_EX4_PROC_SUPPRESS_SWING_ANIM    = 0x00400000,// 22 触发时不播放挥击动画
    SPELL_ATTR_EX4_SUPPRESS_WEAPON_PROCS       = 0x00800000,// 23 抑制武器附魔/特效触发
    SPELL_ATTR_EX4_AUTO_RANGED_COMBAT          = 0x01000000,// 24 自动远程战斗（如自动射击）
    SPELL_ATTR_EX4_OWNER_POWER_SCALING         = 0x02000000,// 25 pet scaling auras (宠物属性随主人缩放光环)
    SPELL_ATTR_EX4_ONLY_FLYING_AREAS           = 0x04000000,// 26 仅在可飞行区域使用
    SPELL_ATTR_EX4_FORCE_DISPLAY_CASTBAR       = 0x08000000,// 27 强制显示施法条
    SPELL_ATTR_EX4_IGNORE_COMBAT_TIMER         = 0x10000000,// 28 NYI (忽略战斗计时器)
    SPELL_ATTR_EX4_UNK29                       = 0x20000000,// 29 different - impact on dispel of effect 0 of something (驱散效果 0 时有特殊处理)
    SPELL_ATTR_EX4_UNK30                       = 0x40000000,// 30 different - impact on dispel of effect 1 of something (驱散效果 1 时有特殊处理)
    SPELL_ATTR_EX4_UNK31                       = 0x80000000,// 31 different - impact on dispel of effect 2 of something (驱散效果 2 时有特殊处理)
};

// 法术扩展属性 5（对应 DBC AttributesEx5 字段）
enum SpellAttributesEx5
{
    SPELL_ATTR_EX5_ALLOW_ACTIONS_DURING_CHANNEL= 0x00000001,// 0 don't interrupt channeling spells when moving/melee attacking/casting (引导期间移动/攻击/施法不会打断引导)
    SPELL_ATTR_EX5_NO_REAGENT_COST_WITH_AURA   = 0x00000002,// 1 not need reagents if UNIT_FLAG_PREPARATION (具备准备状态光环时不消耗材料)
    SPELL_ATTR_EX5_REMOVE_ENTERING_ARENA       = 0x00000004,// 2 removed at enter arena (e.g. 31850 since 3.3.3) (进入竞技场时移除)
    SPELL_ATTR_EX5_ALLOW_WHILE_STUNNED         = 0x00000008,// 3 昏迷状态下仍可施放
    SPELL_ATTR_EX5_TRIGGERS_CHANNELING         = 0x00000010,// 4 触发引导状态
    SPELL_ATTR_EX5_LIMIT_N                     = 0x00000020,// 5 Only N targets can be apply at a time - dont of spells that have N>1 in tbc/wotlk (同时最多作用于 N 个目标，如大地之盾/恐惧)
    SPELL_ATTR_EX5_IGNORE_AREA_EFFECT_PVP_CHECK= 0x00000040,// 6 忽略范围法术的 PvP 合法性检查
    SPELL_ATTR_EX5_NOT_ON_PLAYER               = 0x00000080,// 7 不可对玩家使用
    SPELL_ATTR_EX5_NOT_ON_PLAYER_CONTROLLED_NPC= 0x00000100,// 8 不可对玩家控制的 NPC/宠物使用
    SPELL_ATTR_EX5_EXTRA_INITIAL_PERIOD        = 0x00000200,// 9 begin periodic tick at aura apply (施加光环时立即触发第一跳)
    SPELL_ATTR_EX5_DO_NOT_DISPLAY_DURATION     = 0x00000400,// 10 不显示光环持续时间
    SPELL_ATTR_EX5_IMPLIED_TARGETING           = 0x00000800,// 11 targets target of current hostile target if current hostile target is not eligible (隐含目标：若敌对目标不可协助则转向目标的目标)
    SPELL_ATTR_EX5_MELEE_CHAIN_TARGETING       = 0x00001000,// 12 Cleave Generic chain damage front targets (顺劈类前方链式近战多目标)
    SPELL_ATTR_EX5_SPELL_HASTE_AFFECTS_PERIODIC= 0x00002000,// 13 haste affects duration (法术急速缩短周期性效果总时长与间隔)
    SPELL_ATTR_EX5_NOT_AVAILABLE_WHILE_CHARMED = 0x00004000,// 14 被魅惑时不可用
    SPELL_ATTR_EX5_TREAT_AS_AREA_EFFECT        = 0x00008000,// 15 视为范围效果（AoE）处理
    SPELL_ATTR_EX5_AURA_AFFECTS_NOT_JUST_REQ_EQUIPPED_ITEM  = 0x00010000,// 16 (only affects protpally One-Handed Weapon Spec) (光环不仅影响所要求的已装备武器)
    SPELL_ATTR_EX5_ALLOW_WHILE_FLEEING         = 0x00020000,// 17 恐惧逃跑时仍可施放
    SPELL_ATTR_EX5_ALLOW_WHILE_CONFUSED        = 0x00040000,// 18 迷惑状态下仍可施放
    SPELL_ATTR_EX5_AI_DOESNT_FACE_TARGET       = 0x00080000,// 19 AI 施放时不强制面向目标
    SPELL_ATTR_EX5_DO_NOT_ATTEMPT_A_PET_RESUMMON_WHEN_DISMOUNTING = 0x00100000,// 20 NYI - I think we do it innately for festive auras already (下马时不自动重新召唤宠物)
    SPELL_ATTR_EX5_IGNORE_TARGET_REQUIREMENTS  = 0x00200000,// 21 忽略目标需求检查
    SPELL_ATTR_EX5_NOT_ON_TRIVIAL              = 0x00400000,// 22 不可对灰名低级目标使用
    SPELL_ATTR_EX5_NO_PARTIAL_RESISTS          = 0x00800000,// 23 不会发生部分抵抗
    SPELL_ATTR_EX5_IGNORE_CASTER_REQUIREMENTS  = 0x01000000,// 24 忽略施法者前提条件
    SPELL_ATTR_EX5_ALWAYS_LINE_OF_SIGHT        = 0x02000000,// 25 始终进行视线检查
    SPELL_ATTR_EX5_ALWAYS_AOE_LINE_OF_SIGHT    = 0x04000000,// 26 AoE 目标始终检查视线
    SPELL_ATTR_EX5_NO_CASTER_AURA_ICON         = 0x08000000,// 27 施法者界面隐藏该光环图标
    SPELL_ATTR_EX5_NO_TARGET_AURA_ICON         = 0x10000000,// 28 目标界面隐藏该光环图标
    SPELL_ATTR_EX5_AURA_UNIQUE_PER_CASTER      = 0x20000000,// 29 每个施法者只能维持一个实例
    SPELL_ATTR_EX5_ALWAYS_SHOW_GROUND_TEXTURE  = 0x40000000,// 30 始终显示地面光环贴图
    SPELL_ATTR_EX5_ADD_MELEE_HIT_RATING        = 0x80000000,// 31 Introduced in patch 2.3: Taunt, Growl, etc spells use ability miss calculation (see implementation for details) (使用近战技能命中率计算，如嘲讽)
};

// 法术扩展属性 6（对应 DBC AttributesEx6 字段）
enum SpellAttributesEx6
{
    SPELL_ATTR_EX6_NO_COOLDOWN_ON_TOOLTIP      = 0x00000001,// 0 技能提示不显示冷却时间
    SPELL_ATTR_EX6_DO_NOT_RESET_COOLDOWN_IN_ARENA = 0x00000002, // 1 Unused in tbc (进入竞技场时不重置冷却，TBC 未使用)
    SPELL_ATTR_EX6_NOT_AN_ATTACK               = 0x00000004,// 2 Likely has more meaning than just ignore caster auras (非攻击行为)
    SPELL_ATTR_EX6_CAN_ASSIST_IMMUNE_PC        = 0x00000008,// 3 可对处于免疫状态的玩家提供辅助治疗/增益
    SPELL_ATTR_EX6_IGNORE_FOR_MOD_TIME_RATE    = 0x00000010,// 4 Unused in tbc (TBC 未使用)
    SPELL_ATTR_EX6_DO_NOT_CONSUME_RESOURCES    = 0x00000020,// 5 Requires resources but doesnt consume them - ritual of summoning child spell consumes (需要资源但不实际消耗)
    SPELL_ATTR_EX6_FLOATING_COMBAT_TEXT_ON_CAST= 0x00000040,// 6 Auras with this attribute trigger SPELL_CAST combat log event instead of SPELL_AURA_START (clientside attribute) (施放时显示浮动战斗文字)
    SPELL_ATTR_EX6_AURA_IS_WEAPON_PROC         = 0x00000080,// 7 光环为武器触发效果
    SPELL_ATTR_EX6_DO_NOT_CHAIN_TO_CROWD_CONTROLLED_TARGETS = 0x00000100,// 8 ignores target with cc effects (链式法术不弹射至被群体控制的目标)
    SPELL_ATTR_EX6_ALLOW_ON_CHARMED_TARGETS    = 0x00000200,// 9 Unused in tbc (允许作用于被魅惑目标，TBC 未使用)
    SPELL_ATTR_EX6_NO_AURA_LOG                 = 0x00000400,// 10 Seems different in wotlk (不记录光环日志)
    SPELL_ATTR_EX6_NOT_IN_RAID_INSTANCES       = 0x00000800,// 11 团队副本中不可用
    SPELL_ATTR_EX6_ALLOW_WHILE_RIDING_VEHICLE  = 0x00001000,// 12 Unused in tbc (TBC 未使用)
    SPELL_ATTR_EX6_IGNORE_PHASE_SHIFT          = 0x00002000,// 13 Can target in different phase (可作用于不同位面的目标)
    SPELL_ATTR_EX6_AI_PRIMARY_RANGED_ATTACK    = 0x00004000,// 14 NYI - Likely global flag alternative to flagging spells as main in creature spell list (AI 主远程攻击标识)
    SPELL_ATTR_EX6_NO_PUSHBACK                 = 0x00008000,// 15 Unused in tbc (免疫施法推迟，TBC 未使用)
    SPELL_ATTR_EX6_NO_JUMP_PATHING             = 0x00010000,// 16 Unused in tbc (TBC 未使用)
    SPELL_ATTR_EX6_ALLOW_EQUIP_WHILE_CASTING   = 0x00020000,// 17 允许施法时切换装备
};

// 服务端自定义法术属性标志（数据库 spell_template 扩展字段）
enum SpellAttributesServerside
{
    SPELL_ATTR_SS_PREVENT_INVIS                = 0x00000001, // 阻止在隐身状态生效/打破隐身
    SPELL_ATTR_SS_AOE_CAP                      = 0x00000002, // 启用 AoE 伤害上限分摊
    SPELL_ATTR_SS_IGNORE_EVADE                 = 0x00000004, // 无视目标脱离战斗（Evade）状态
    SPELL_ATTR_SS_FACING_BACK                  = 0x00000008, // 需要面向目标背面
};

// 法术施放结果/失败原因枚举（发送至客户端 SMSG_CAST_FAILED）
enum SpellCastResult : uint32
{
    SPELL_FAILED_AFFECTING_COMBAT               = 0x00,     // 正在战斗中
    SPELL_FAILED_ALREADY_AT_FULL_HEALTH         = 0x01,     // 生命值已满
    SPELL_FAILED_ALREADY_AT_FULL_MANA           = 0x02,     // 法力值已满
    SPELL_FAILED_ALREADY_AT_FULL_POWER          = 0x03,     // 能量/怒气已满
    SPELL_FAILED_ALREADY_BEING_TAMED            = 0x04,     // 正在被驯服
    SPELL_FAILED_ALREADY_HAVE_CHARM             = 0x05,     // 已有魅惑目标
    SPELL_FAILED_ALREADY_HAVE_SUMMON            = 0x06,     // 已有召唤物
    SPELL_FAILED_ALREADY_OPEN                   = 0x07,     // 已经打开
    SPELL_FAILED_AURA_BOUNCED                   = 0x08,     // 已存在更强大的法术效果
    SPELL_FAILED_AUTOTRACK_INTERRUPTED          = 0x09,     // 自动追踪被打断
    SPELL_FAILED_BAD_IMPLICIT_TARGETS           = 0x0A,     // 没有合法的隐式目标
    SPELL_FAILED_BAD_TARGETS                    = 0x0B,     // 无效目标
    SPELL_FAILED_CANT_BE_CHARMED                = 0x0C,     // 无法被魅惑
    SPELL_FAILED_CANT_BE_DISENCHANTED           = 0x0D,     // 无法被分解
    SPELL_FAILED_CANT_BE_DISENCHANTED_SKILL     = 0x0E,     // 附魔技能等级不足，无法分解
    SPELL_FAILED_CANT_BE_PROSPECTED             = 0x0F,     // 无法被选矿
    SPELL_FAILED_CANT_CAST_ON_TAPPED            = 0x10,     // 无法对已被他人攻击的目标施放
    SPELL_FAILED_CANT_DUEL_WHILE_INVISIBLE      = 0x11,     // 隐身状态下无法决斗
    SPELL_FAILED_CANT_DUEL_WHILE_STEALTHED      = 0x12,     // 潜行状态下无法决斗
    SPELL_FAILED_CANT_STEALTH                   = 0x13,     // 无法潜行
    SPELL_FAILED_CASTER_AURASTATE               = 0x14,     // 施法者光环状态不满足条件
    SPELL_FAILED_CASTER_DEAD                    = 0x15,     // 你已经死亡
    SPELL_FAILED_CHARMED                        = 0x16,     // 被魅惑中无法施放
    SPELL_FAILED_CHEST_IN_USE                   = 0x17,     // 宝箱正在被他人使用
    SPELL_FAILED_CONFUSED                       = 0x18,     // 处于迷惑状态
    SPELL_FAILED_DONT_REPORT                    = 0x19,     // 静默失败（不显示错误提示）
    SPELL_FAILED_EQUIPPED_ITEM                  = 0x1A,     // 未装备所需物品
    SPELL_FAILED_EQUIPPED_ITEM_CLASS            = 0x1B,     // 装备物品类型不符
    SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND   = 0x1C,     // 主手装备类型不符
    SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND    = 0x1D,     // 副手装备类型不符
    SPELL_FAILED_ERROR                          = 0x1E,     // 内部错误
    SPELL_FAILED_FIZZLE                         = 0x1F,     // 法术失效
    SPELL_FAILED_FLEEING                        = 0x20,     // 正在逃跑中
    SPELL_FAILED_FOOD_LOWLEVEL                  = 0x21,     // 食物等级过低
    SPELL_FAILED_HIGHLEVEL                      = 0x22,     // 目标等级过高
    SPELL_FAILED_HUNGER_SATIATED                = 0x23,     // 已经吃饱
    SPELL_FAILED_IMMUNE                         = 0x24,     // 目标免疫
    SPELL_FAILED_INTERRUPTED                    = 0x25,     // 施法被打断
    SPELL_FAILED_INTERRUPTED_COMBAT             = 0x26,     // 因进入战斗/受击被打断
    SPELL_FAILED_ITEM_ALREADY_ENCHANTED         = 0x27,     // 物品已有附魔
    SPELL_FAILED_ITEM_GONE                      = 0x28,     // 物品已不存在
    SPELL_FAILED_ITEM_NOT_FOUND                 = 0x29,     // 未找到物品
    SPELL_FAILED_ITEM_NOT_READY                 = 0x2A,     // 物品尚未冷却就绪
    SPELL_FAILED_LEVEL_REQUIREMENT              = 0x2B,     // 等级未达到要求
    SPELL_FAILED_LINE_OF_SIGHT                  = 0x2C,     // 目标不在视野（视线）范围内
    SPELL_FAILED_LOWLEVEL                       = 0x2D,     // 目标等级过低
    SPELL_FAILED_LOW_CASTLEVEL                  = 0x2E,     // 施法等级过低
    SPELL_FAILED_MAINHAND_EMPTY                 = 0x2F,     // 主手未装备武器
    SPELL_FAILED_MOVING                         = 0x30,     // 移动中无法施法
    SPELL_FAILED_NEED_AMMO                      = 0x31,     // 缺少弹药
    SPELL_FAILED_NEED_AMMO_POUCH                = 0x32,     // 需要弹药袋/箭袋
    SPELL_FAILED_NEED_EXOTIC_AMMO               = 0x33,     // 需要特殊弹药
    SPELL_FAILED_NOPATH                         = 0x34,     // 没有可用路径到达目标
    SPELL_FAILED_NOT_BEHIND                     = 0x35,     // 必须位于目标背后
    SPELL_FAILED_NOT_FISHABLE                   = 0x36,     // 该区域无法钓鱼
    SPELL_FAILED_NOT_FLYING                     = 0x37,     // 必须在飞行状态
    SPELL_FAILED_NOT_HERE                       = 0x38,     // 无法在此处使用
    SPELL_FAILED_NOT_INFRONT                    = 0x39,     // 必须面向目标
    SPELL_FAILED_NOT_IN_CONTROL                 = 0x3A,     // 失去角色控制权
    SPELL_FAILED_NOT_KNOWN                      = 0x3B,     // 尚未学会该法术
    SPELL_FAILED_NOT_MOUNTED                    = 0x3C,     // 骑乘状态下无法使用
    SPELL_FAILED_NOT_ON_TAXI                    = 0x3D,     // 飞行航线中无法使用
    SPELL_FAILED_NOT_ON_TRANSPORT               = 0x3E,     // 载具/船只上无法使用
    SPELL_FAILED_NOT_READY                      = 0x3F,     // 法术尚未冷却就绪
    SPELL_FAILED_NOT_SHAPESHIFT                 = 0x40,     // 变形状态下无法使用
    SPELL_FAILED_NOT_STANDING                   = 0x41,     // 必须处于站立状态
    SPELL_FAILED_NOT_TRADEABLE                  = 0x42,     // 不可交易物品
    SPELL_FAILED_NOT_TRADING                    = 0x43,     // 必须在交易中
    SPELL_FAILED_NOT_UNSHEATHED                 = 0x44,     // 必须拔出武器
    SPELL_FAILED_NOT_WHILE_GHOST                = 0x45,     // 灵魂状态下无法使用
    SPELL_FAILED_NO_AMMO                        = 0x46,     // 弹药耗尽
    SPELL_FAILED_NO_CHARGES_REMAIN              = 0x47,     // 充能次数已耗尽
    SPELL_FAILED_NO_CHAMPION                    = 0x48,     // 缺少勇士
    SPELL_FAILED_NO_COMBO_POINTS                = 0x49,     // 需要连击点数
    SPELL_FAILED_NO_DUELING                     = 0x4A,     // 该区域禁止决斗
    SPELL_FAILED_NO_ENDURANCE                   = 0x4B,     // 耐力不足
    SPELL_FAILED_NO_FISH                        = 0x4C,     // 鱼群已消失/没有鱼
    SPELL_FAILED_NO_ITEMS_WHILE_SHAPESHIFTED    = 0x4D,     // 变形状态下无法使用物品
    SPELL_FAILED_NO_MOUNTS_ALLOWED              = 0x4E,     // 此处禁止骑乘
    SPELL_FAILED_NO_PET                         = 0x4F,     // 没有宠物
    SPELL_FAILED_NO_POWER                       = 0x50,     // 法力/能量/怒气不足
    SPELL_FAILED_NOTHING_TO_DISPEL              = 0x51,     // 没有可驱散的效果
    SPELL_FAILED_NOTHING_TO_STEAL               = 0x52,     // 没有可偷取的效果
    SPELL_FAILED_ONLY_ABOVEWATER                = 0x53,     // 仅可在水面之上使用
    SPELL_FAILED_ONLY_DAYTIME                   = 0x54,     // 仅可在白天使用
    SPELL_FAILED_ONLY_INDOORS                   = 0x55,     // 仅可在室内使用
    SPELL_FAILED_ONLY_MOUNTED                   = 0x56,     // 仅可在骑乘状态使用
    SPELL_FAILED_ONLY_NIGHTTIME                 = 0x57,     // 仅可在夜间使用
    SPELL_FAILED_ONLY_OUTDOORS                  = 0x58,     // 仅可在室外使用
    SPELL_FAILED_ONLY_SHAPESHIFT                = 0x59,     // 仅可在指定变形/姿态下使用
    SPELL_FAILED_ONLY_STEALTHED                 = 0x5A,     // 仅可在潜行状态下使用
    SPELL_FAILED_ONLY_UNDERWATER                = 0x5B,     // 仅可在水下使用
    SPELL_FAILED_OUT_OF_RANGE                   = 0x5C,     // 超出法术射程
    SPELL_FAILED_PACIFIED                       = 0x5D,     // 处于安抚状态
    SPELL_FAILED_POSSESSED                      = 0x5E,     // 被控制/附身状态
    SPELL_FAILED_REAGENTS                       = 0x5F,     // 缺少施法材料
    SPELL_FAILED_REQUIRES_AREA                  = 0x60,     // 必须在特定区域使用
    SPELL_FAILED_REQUIRES_SPELL_FOCUS           = 0x61,     // 附近需要法术焦点对象（如熔炉/铁砧）
    SPELL_FAILED_ROOTED                         = 0x62,     // 被定身无法施放
    SPELL_FAILED_SILENCED                       = 0x63,     // 处于沉默状态
    SPELL_FAILED_SPELL_IN_PROGRESS              = 0x64,     // 正在施放另一个法术
    SPELL_FAILED_SPELL_LEARNED                  = 0x65,     // 已经学会该法术
    SPELL_FAILED_SPELL_UNAVAILABLE              = 0x66,     // 法术当前不可用
    SPELL_FAILED_STUNNED                        = 0x67,     // 处于昏迷状态
    SPELL_FAILED_TARGETS_DEAD                   = 0x68,     // 目标已经死亡
    SPELL_FAILED_TARGET_AFFECTING_COMBAT        = 0x69,     // 目标正在战斗中
    SPELL_FAILED_TARGET_AURASTATE               = 0x6A,     // 目标光环状态不符
    SPELL_FAILED_TARGET_DUELING                 = 0x6B,     // 目标正在决斗中
    SPELL_FAILED_TARGET_ENEMY                   = 0x6C,     // 目标是敌方单位
    SPELL_FAILED_TARGET_ENRAGED                 = 0x6D,     // 目标处于激怒状态
    SPELL_FAILED_TARGET_FRIENDLY                = 0x6E,     // 目标是友方单位
    SPELL_FAILED_TARGET_IN_COMBAT               = 0x6F,     // 目标处于战斗状态
    SPELL_FAILED_TARGET_IS_PLAYER               = 0x70,     // 不可对玩家使用
    SPELL_FAILED_TARGET_IS_PLAYER_CONTROLLED    = 0x71,     // 不可对玩家控制单位使用
    SPELL_FAILED_TARGET_NOT_DEAD                = 0x72,     // 目标尚未死亡
    SPELL_FAILED_TARGET_NOT_IN_PARTY            = 0x73,     // 目标不在小队中
    SPELL_FAILED_TARGET_NOT_LOOTED              = 0x74,     // 目标战利品尚未拾取完毕
    SPELL_FAILED_TARGET_NOT_PLAYER              = 0x75,     // 目标必须是玩家
    SPELL_FAILED_TARGET_NO_POCKETS              = 0x76,     // 目标没有可偷窃的物品
    SPELL_FAILED_TARGET_NO_WEAPONS              = 0x77,     // 目标未装备武器
    SPELL_FAILED_TARGET_UNSKINNABLE             = 0x78,     // 目标无法剥皮
    SPELL_FAILED_THIRST_SATIATED                = 0x79,     // 已经喝饱
    SPELL_FAILED_TOO_CLOSE                      = 0x7A,     // 距离目标太近（低于最小射程）
    SPELL_FAILED_TOO_MANY_OF_ITEM               = 0x7B,     // 背包中该物品数量已达上限
    SPELL_FAILED_TOTEM_CATEGORY                 = 0x7C,     // 缺少所需工具/图腾类别
    SPELL_FAILED_TOTEMS                         = 0x7D,     // 缺少所需图腾
    SPELL_FAILED_TRAINING_POINTS                = 0x7E,     // 训练点数不足
    SPELL_FAILED_TRY_AGAIN                      = 0x7F,     // 请重试
    SPELL_FAILED_UNIT_NOT_BEHIND                = 0x80,     // 目标未背对你
    SPELL_FAILED_UNIT_NOT_INFRONT               = 0x81,     // 单位未在前方
    SPELL_FAILED_WRONG_PET_FOOD                 = 0x82,     // 宠物不吃此类食物
    SPELL_FAILED_NOT_WHILE_FATIGUED             = 0x83,     // 疲劳状态下无法使用
    SPELL_FAILED_TARGET_NOT_IN_INSTANCE         = 0x84,     // 目标不在同一副本中
    SPELL_FAILED_NOT_WHILE_TRADING              = 0x85,     // 交易过程中无法使用
    SPELL_FAILED_TARGET_NOT_IN_RAID             = 0x86,     // 目标不在同一团队中
    SPELL_FAILED_DISENCHANT_WHILE_LOOTING       = 0x87, // TODO: add (拾取战利品时无法分解)
    SPELL_FAILED_PROSPECT_WHILE_LOOTING         = 0x88, // TODO: add (拾取战利品时无法选矿)
    SPELL_FAILED_NEED_MORE_ITEMS                = 0x89,     // 所需物品数量不足
    SPELL_FAILED_TARGET_FREEFORALL              = 0x8A,     // 目标处于自由混战（FFA）区域
    SPELL_FAILED_NO_EDIBLE_CORPSES              = 0x8B,     // 附近没有可食用的尸体
    SPELL_FAILED_ONLY_BATTLEGROUNDS             = 0x8C,     // 仅可在战场中使用
    SPELL_FAILED_TARGET_NOT_GHOST               = 0x8D,     // 目标不是灵魂状态
    SPELL_FAILED_TOO_MANY_SKILLS                = 0x8E,     // 已学习过多专业技能
    SPELL_FAILED_TRANSFORM_UNUSABLE             = 0x8F,     // 当前变形不可用
    SPELL_FAILED_WRONG_WEATHER                  = 0x90,     // 天气条件不符
    SPELL_FAILED_DAMAGE_IMMUNE                  = 0x91,     // 目标免疫伤害
    SPELL_FAILED_PREVENTED_BY_MECHANIC          = 0x92,     // 被某种法术机制阻止
    SPELL_FAILED_PLAY_TIME                      = 0x93,     // 游戏时间限制
    SPELL_FAILED_REPUTATION                     = 0x94,     // 声望等级不足
    SPELL_FAILED_MIN_SKILL                      = 0x95,     // 专业技能等级不足
    SPELL_FAILED_NOT_IN_ARENA                   = 0x96,     // 竞技场中无法使用
    SPELL_FAILED_NOT_ON_SHAPESHIFT              = 0x97,     // 无法对变形目标使用
    SPELL_FAILED_NOT_ON_STEALTHED               = 0x98,     // 无法对潜行目标使用
    SPELL_FAILED_NOT_ON_DAMAGE_IMMUNE           = 0x99,     // 无法对伤害免疫目标使用
    SPELL_FAILED_NOT_ON_MOUNTED                 = 0x9A,     // 无法对骑乘目标使用
    SPELL_FAILED_TOO_SHALLOW                    = 0x9B,     // 水太浅
    SPELL_FAILED_TARGET_NOT_IN_SANCTUARY        = 0x9C,     // 目标不在庇护所区域
    SPELL_FAILED_TARGET_IS_TRIVIAL              = 0x9D,     // 目标等级过低（无意义目标）
    SPELL_FAILED_BM_OR_INVISGOD                 = 0x9E,     // 隐身 GM 或特殊模式阻挡
    SPELL_FAILED_EXPERT_RIDING_REQUIREMENT      = 0x9F,     // 需要高级骑术
    SPELL_FAILED_ARTISAN_RIDING_REQUIREMENT     = 0xA0,     // 需要专家级骑术
    SPELL_FAILED_NOT_IDLE                       = 0xA1,     // 角色非空闲状态
    SPELL_FAILED_NOT_INACTIVE                   = 0xA2,     // 角色非离线/非激活状态
    SPELL_FAILED_PARTIAL_PLAYTIME               = 0xA3,     // 沉迷部分收益时间限制
    SPELL_FAILED_NO_PLAYTIME                    = 0xA4,     // 防沉迷零收益时间限制
    SPELL_FAILED_NOT_IN_BATTLEGROUND            = 0xA5,     // 战场中无法使用
    SPELL_FAILED_ONLY_IN_ARENA                  = 0xA6,     // 仅可在竞技场中使用
    SPELL_FAILED_TARGET_LOCKED_TO_RAID_INSTANCE = 0xA7,     // 目标已被锁定至其他团队副本进度
    SPELL_FAILED_UNKNOWN                        = 0xA8,     // 未知原因

    SPELL_FAILED_CLIENT_MAX                     = 0xA9,     // 客户端可识别的最大失败码边界

    SPELL_RESULT_DONT_SEND_TO_CLIENT            = 0xFD,     // Always has to be first - TODO: is this still needed? not in public (不发送至客户端标记)
    SPELL_FAILED_PVP_CHECK                      = 0xFD,     // custom value for serverside check (服务端自定义：PvP 检查未通过)
    SPELL_NOT_FOUND                             = 0xFE,     // 服务端自定义：未找到法术
    SPELL_CAST_OK                               = 0xFF      // custom value, don't must be send to client (施法检查通过，不可发送至客户端)
};

// 法术施法打断标志位
enum SpellInterruptFlags
{
    SPELL_INTERRUPT_FLAG_MOVEMENT           = 0x01,         // 移动打断施法
    SPELL_INTERRUPT_FLAG_DAMAGE_PUSHBACK    = 0x02, // Player only (受到伤害推迟施法进度，仅玩家)
    SPELL_INTERRUPT_FLAG_STUN               = 0x04, // not implemented until more research - many creature spells miss it (昏迷打断)
    SPELL_INTERRUPT_FLAG_COMBAT             = 0x08,         // 进入战斗打断
    SPELL_INTERRUPT_FLAG_DAMAGE_CANCELS     = 0x10, // Player only (受到伤害直接打断，仅玩家)
};

// 光环与引导打断标志位
enum SpellAuraInterruptFlags // also used for ChannelInterruptFlags (同时也用于引导法术的 ChannelInterruptFlags)
{
    AURA_INTERRUPT_FLAG_HOSTILE_ACTION              = 0x00000001,   // 0    removed when getting hit by a negative spell (被负面法术击中时移除)
    AURA_INTERRUPT_FLAG_DAMAGE                      = 0x00000002,   // 1    removed by any damage (受到任何伤害时移除)
    AURA_INTERRUPT_FLAG_ACTION                      = 0x00000004,   // 2    removed by any cast (施放任何法术时移除)
    AURA_INTERRUPT_FLAG_MOVING                      = 0x00000008,   // 3    removed by any movement (发生任何移动时移除)
    AURA_INTERRUPT_FLAG_TURNING                     = 0x00000010,   // 4    removed by any turning (发生转向时移除)
    AURA_INTERRUPT_FLAG_ANIM_CANCELS                = 0x00000020,   // 5    removed by anim (播放其他动作动画时移除)
    AURA_INTERRUPT_FLAG_DISMOUNT                    = 0x00000040,   // 6    removed by unmounting (下马时移除)
    AURA_INTERRUPT_FLAG_UNDERWATER_CANCELS          = 0x00000080,   // 7    removed by entering water (入水时移除)
    AURA_INTERRUPT_FLAG_ABOVEWATER_CANCELS          = 0x00000100,   // 8    removed by leaving water (离开水面时移除)
    AURA_INTERRUPT_FLAG_SHEATHING_CANCELS           = 0x00000200,   // 9    removed by unsheathing (拔出武器时移除)
    AURA_INTERRUPT_FLAG_INTERACTING                 = 0x00000400,   // 10   talk to npc / loot? action on creature (与 NPC 对话或交互时移除)
    AURA_INTERRUPT_FLAG_LOOTING                     = 0x00000800,   // 11   mine/use/open action on gameobject (采集/打开游戏对象时移除)
    AURA_INTERRUPT_FLAG_ATTACKING                   = 0x00001000,   // 12   removed by attack (发动近战/远程攻击时移除)
    AURA_INTERRUPT_FLAG_ITEM_USE                    = 0x00002000,   // 13 使用物品时移除
    AURA_INTERRUPT_FLAG_DAMAGE_CHANNEL_DURATION     = 0x00004000,   // 14 受到伤害时缩短引导持续时间（打退引导条）
    AURA_INTERRUPT_FLAG_SHAPESHIFTING               = 0x00008000,   // 15 变形时移除
    AURA_INTERRUPT_FLAG_ACTION_LATE                 = 0x00010000,   // 16 施法成功结算后移除（延迟动作取消）
    AURA_INTERRUPT_FLAG_MOUNTING                    = 0x00020000,   // 17   removed by mounting (上马时移除)
    AURA_INTERRUPT_FLAG_STANDING_CANCELS            = 0x00040000,   // 18   removed by standing up (used by food and drink mostly and sleep/Fake Death like) (站起时移除，如进食/喝水/假死)
    AURA_INTERRUPT_FLAG_LEAVE_WORLD                 = 0x00080000,   // 19   leaving map/getting teleported (离开地图或被传送时移除)
    AURA_INTERRUPT_FLAG_STEALTH_INVIS_CANCELS       = 0x00100000,   // 20 进入潜行或隐身时移除
    AURA_INTERRUPT_FLAG_INVULNERABILITY_BUFF_CANCELS= 0x00200000,   // 21 获得无敌增益时移除
    AURA_INTERRUPT_FLAG_ENTER_WORLD                 = 0x00400000,   // 22 进入世界时移除
    AURA_INTERRUPT_FLAG_PVP_ACTIVE_CANCELS          = 0x00800000,   // 23   removed by entering pvp combat (进入 PvP 战斗时移除)
    AURA_INTERRUPT_FLAG_NON_PERIODIC_DAMAGE         = 0x01000000    // 24   removed by any direct damage (受到任何非周期性直接伤害时移除)
};

// 法术朝向限制标志
enum SpellFacingFlags
{
    SPELL_FACING_FLAG_INFRONT = 0x0001                      // 目标必须位于施法者前方
};

// 法术修正操作类型（供天赋/光环 SPELL_AURA_ADD_FLAT_MODIFIER / ADD_PCT_MODIFIER 使用）
enum SpellModOp
{
    SPELLMOD_DAMAGE                     = 0,                // 修改伤害/治疗基础数值
    SPELLMOD_DURATION                   = 1,                // 修改持续时间
    SPELLMOD_THREAT                     = 2,                // 修改威胁值
    SPELLMOD_EFFECT1                    = 3,                // 修改第 1 个效果值
    SPELLMOD_CHARGES                    = 4,                // 修改充能层数
    SPELLMOD_RANGE                      = 5,                // 修改施法距离
    SPELLMOD_RADIUS                     = 6,                // 修改作用半径
    SPELLMOD_CRITICAL_CHANCE            = 7,                // 修改暴击几率
    SPELLMOD_ALL_EFFECTS                = 8,                // 修改所有效果值
    SPELLMOD_NOT_LOSE_CASTING_TIME      = 9,                // 减少受击施法打退（防打退）
    SPELLMOD_CASTING_TIME               = 10,               // 修改施法时间
    SPELLMOD_COOLDOWN                   = 11,               // 修改冷却时间
    SPELLMOD_EFFECT2                    = 12,               // 修改第 2 个效果值
    SPELLMOD_UNK1                       = 13, // unused (未使用)
    SPELLMOD_COST                       = 14,               // 修改法力/怒气/能量消耗
    SPELLMOD_CRIT_DAMAGE_BONUS          = 15,               // 修改暴击伤害加成
    SPELLMOD_RESIST_MISS_CHANCE         = 16,               // 降低被抵抗/未命中几率（命中修正）
    SPELLMOD_JUMP_TARGETS               = 17,               // 修改链式跳跃最大目标数
    SPELLMOD_CHANCE_OF_SUCCESS          = 18, // Only used with SPELL_AURA_ADD_FLAT_MODIFIER and affects proc spells (修改触发/成功几率)
    SPELLMOD_ACTIVATION_TIME            = 19,               // 修改触发/周期间隔时间
    SPELLMOD_EFFECT_PAST_FIRST          = 20,               // 修改链式衰减系数（后续跳跃倍率）
    SPELLMOD_GLOBAL_COOLDOWN            = 21,               // 修改公共冷却时间（GCD）
    SPELLMOD_DOT                        = 22,               // 修改周期性伤害/治疗（DoT/HoT）
    SPELLMOD_EFFECT3                    = 23,               // 修改第 3 个效果值
    SPELLMOD_SPELL_BONUS_DAMAGE         = 24,               // 修改法术伤害/治疗加成系数
    SPELLMOD_UNK2                       = 25, // unused (未使用)
    // SPELLMOD_FREQUENCY_OF_SUCCESS    = 26,                // not used in 2.4.3 (2.4.3 未使用)
    SPELLMOD_MULTIPLE_VALUE             = 27,               // 修改多重数值系数
    SPELLMOD_RESIST_DISPEL_CHANCE       = 28,               // 提高抵抗驱散几率
    MAX_SPELLMOD                        = 32,               // 法术修正槽位上限
};

// Spell triggering settings for CastSpell that enable us to skip some checks so that we can investigate spell specific settings
// CastSpell 触发施法标志位：用于跳过特定校验或控制脚本触发行为
enum TriggerCastFlags : uint32
{
    TRIGGERED_NONE                              = 0x00000000,   // Not Triggered (正常主动施法，非触发)
    TRIGGERED_OLD_TRIGGERED                     = 0x00000001,   // Legacy bool support TODO: Restrict usage as much as possible. (旧版 bool triggered=true 兼容标志)
    TRIGGERED_IGNORE_HIT_CALCULATION            = 0x00000002,   // Will ignore calculating hit in SpellHitResult (跳过命中计算，必定命中)
    TRIGGERED_IGNORE_UNSELECTABLE_FLAG          = 0x00000004,   // Ignores UNIT_FLAG_UNINTERACTIBLE in CheckTarget (忽略目标不可交互标志)
    TRIGGERED_INSTANT_CAST                      = 0x00000008,   // Will ignore any cast time set in spell entry (忽略施法时间，强制瞬发)
    TRIGGERED_AUTOREPEAT                        = 0x00000010,   // Will signal spell system that this is internal autorepeat call (内部自动重复攻击调用)
    TRIGGERED_IGNORE_UNATTACKABLE_FLAG          = 0x00000020,   // Ignores UNIT_FLAG_NOT_ATTACKABLE in CheckTarget (忽略目标不可攻击标志)
    TRIGGERED_DO_NOT_PROC                       = 0x00000040,   // Spells from scripts should not proc - DBScripts for example (禁止本次法术触发其他 Proc)
    TRIGGERED_PET_CAST                          = 0x00000080,   // Spell that should report error through pet opcode (宠物施法，通过宠物错误包反馈结果)
    TRIGGERED_NORMAL_COMBAT_CAST                = 0x00000100,   // AI needs to be notified about change of target TODO: change into TRIGGERED_NONE (普通战斗施法，通知 AI 目标变化)
    TRIGGERED_IGNORE_GCD                        = 0x00000200,   // Ignores GCD - to be used in spell scripts (忽略公共冷却 GCD)
    TRIGGERED_IGNORE_COSTS                      = 0x00000400,   // Ignores spell costs (忽略能量/材料消耗)
    TRIGGERED_IGNORE_COOLDOWNS                  = 0x00000800,   // Ignores cooldowns (忽略法术冷却时间)
    TRIGGERED_IGNORE_CURRENT_CASTED_SPELL       = 0x00001000,   // Ignores concurrent casts and is not set as currently executed (忽略并存施法限制且不占用当前施法槽)
    TRIGGERED_HIDE_CAST_IN_COMBAT_LOG           = 0x00002000,   // Sends cast flag for ignoring combat log display - used for many procs - default behaviour for triggered by aura (在战斗日志中隐藏施法事件)
    TRIGGERED_DO_NOT_RESET_LEASH                = 0x00004000,   // Does not reset leash on cast (施法不重置怪物脱战牵引计时)
    TRIGGERED_CHANNEL_ONLY                      = 0x00008000,   // Only starts channel and no effects - used for summoning portal GO anims (仅开启引导动画不结算效果)
    TRIGGERED_IGNORE_CASTER_AURA_STATE          = 0x00010000,   // Ignores the Aurastate of the caster (忽略施法者光环状态检查)
    TRIGGERED_FORCE_COSTS                       = 0x00020000,   // Forces costs for triggered spells (强制触发法术也扣除资源消耗)
    TRIGGERED_FULL_MASK                         = 0xFFFFFFFF    // 全标志掩码
};

// high byte (3 from 0..3) of UNIT_FIELD_BYTES_2 (对应 UNIT_FIELD_BYTES_2 第 3 字节的变形/姿态枚举)
enum ShapeshiftForm
{
    FORM_NONE               = 0x00,                         // 无变形/常态
    FORM_CAT                = 0x01,                         // 猎豹形态
    FORM_TREE               = 0x02,                         // 生命之树形态
    FORM_TRAVEL             = 0x03,                         // 旅行形态
    FORM_AQUA               = 0x04,                         // 水栖形态
    FORM_BEAR               = 0x05,                         // 熊形态
    FORM_AMBIENT            = 0x06,                         // 环境生物形态
    FORM_GHOUL              = 0x07,                         // 食尸鬼形态
    FORM_DIREBEAR           = 0x08,                         // 巨熊形态
    FORM_CREATUREBEAR       = 0x0E,                         // 生物熊形态
    FORM_CREATURECAT        = 0x0F,                         // 生物豹形态
    FORM_GHOSTWOLF          = 0x10,                         // 幽魂之狼
    FORM_BATTLESTANCE       = 0x11,                         // 战斗姿态
    FORM_DEFENSIVESTANCE    = 0x12,                         // 防御姿态
    FORM_BERSERKERSTANCE    = 0x13,                         // 狂暴姿态
    FORM_TEST               = 0x14,                         // 测试形态
    FORM_ZOMBIE             = 0x15,                         // 僵尸形态
    FORM_FLIGHT_EPIC        = 0x1B,                         // 迅捷飞行形态
    FORM_SHADOW             = 0x1C,                         // 暗影形态
    FORM_FLIGHT             = 0x1D,                         // 飞行形态
    FORM_STEALTH            = 0x1E,                         // 潜行形态
    FORM_MOONKIN            = 0x1F,                         // 枭兽形态
    FORM_SPIRITOFREDEMPTION = 0x20                          // 救赎之魂形态
};

// 法术类别冷却标志（用于 SpellCategory.dbc）
enum class SpellCategoryFlags
{
    CooldownModifiesItem        = 0x1,    // Cooldown modifies item - unused (冷却修改物品，未使用)
    CooldownIsGlobal            = 0x2,    // Cooldown is global (冷却属于全局冷却)
    CooldownEventOnLeaveCombat  = 0x4,    // Cooldown event on leave combat - used in wotlk (脱离战斗时触发冷却事件，WLK 使用)
};

// 将 SpellCastResult 枚举值转换为可读调试名称字符串
const char* GetSpellCastResultString(SpellCastResult result);

#endif
