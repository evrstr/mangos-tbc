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

/**
 * \addtogroup game
 * @{
 * \file
 */


#ifndef __UNIT_H
#define __UNIT_H

#include "Common.h"
#include "Entities/Object.h"
#include "Server/Opcodes.h"
#include "Spells/SpellAuraDefines.h"
#include "Entities/UpdateFields.h"
#include "Globals/SharedDefines.h"
#include "Combat/ThreatManager.h"
#include "Combat/HostileRefManager.h"
#include "Combat/CombatManager.h"
#include "Maps/MapManager.h"
#include "MotionGenerators/FollowerReference.h"
#include "MotionGenerators/FollowerRefManager.h"
#include "Utilities/EventProcessor.h"
#include "MotionGenerators/MotionMaster.h"
#include "Server/DBCStructure.h"
#include "Server/WorldPacket.h"
#include "Util/Timer.h"
#include "AI/BaseAI/UnitAI.h"
#include "Spells/SpellDefines.h"
#include "Maps/SpawnGroupDefines.h"

#include <list>
#include <array>

// 法术部分抵抗等级枚举
enum SpellPartialResist
{
    SPELL_PARTIAL_RESIST_NONE = 0,  // 0%, full hit (0% 抵抗，全额命中)
    SPELL_PARTIAL_RESIST_PCT_25,    // 25% (抵抗 25% 伤害)
    SPELL_PARTIAL_RESIST_PCT_50,    // 50% (抵抗 50% 伤害)
    SPELL_PARTIAL_RESIST_PCT_75,    // 75% (抵抗 75% 伤害)
    SPELL_PARTIAL_RESIST_PCT_100,   // 100%, full resist (100% 抵抗，完全抵抗)
};

#define NUM_SPELL_PARTIAL_RESISTS 5

#define BASE_MELEERANGE_OFFSET 1.33f                        // 基础近战攻击距离补偿偏移
#define BASE_MINDAMAGE 1.0f                                 // 默认最小基础伤害
#define BASE_MAXDAMAGE 2.0f                                 // 默认最大基础伤害
#define BASE_ATTACK_TIME 2000                               // 基础攻击间隔（毫秒）

// UNIT_FIELD_BYTES_0 各字节偏移量定义
enum UnitBytes0Offsets : uint8
{
    UNIT_BYTES_0_OFFSET_RACE       = 0,                     // 种族字节偏移
    UNIT_BYTES_0_OFFSET_CLASS      = 1,                     // 职业字节偏移
    UNIT_BYTES_0_OFFSET_GENDER     = 2,                     // 性别字节偏移
    UNIT_BYTES_0_OFFSET_POWER_TYPE = 3,                     // 能量类型字节偏移
};

// UNIT_FIELD_BYTES_1 各字节偏移量定义
enum UnitBytes1Offsets : uint8
{
    UNIT_BYTES_1_OFFSET_STAND_STATE     = 0,                // 站立/坐姿等姿态状态偏移
    UNIT_BYTES_1_OFFSET_PET_LOYALTY     = 1,                // 宠物忠诚度/自由天赋点偏移
    UNIT_BYTES_1_OFFSET_VIS_FLAGS       = 2,                // 可见性标志偏移
    UNIT_BYTES_1_OFFSET_MISC_FLAGS      = 3,                // 杂项状态标志偏移
};

// UNIT_FIELD_BYTES_2 各字节偏移量定义
enum UnitBytes2Offsets : uint8
{
    UNIT_BYTES_2_OFFSET_SHEATH_STATE = 0,                   // 武器收起/拔出状态偏移
    UNIT_BYTES_2_OFFSET_DEBUFF_LIMIT = 1,                   // 减益上限偏移
    UNIT_BYTES_2_OFFSET_PET_FLAGS    = 2,                   // 宠物重命名/遗弃标志偏移
    UNIT_BYTES_2_OFFSET_SHAPESHIFT   = 3,                   // 变形形态偏移
};

// byte value (UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_STAND_STATE) (单位站立/坐姿姿态枚举)
enum UnitStandStateType
{
    UNIT_STAND_STATE_STAND             = 0,                 // 站立
    UNIT_STAND_STATE_SIT               = 1,                 // 席地而坐
    UNIT_STAND_STATE_SIT_CHAIR         = 2,                 // 坐在椅子上
    UNIT_STAND_STATE_SLEEP             = 3,                 // 睡眠/躺卧
    UNIT_STAND_STATE_SIT_LOW_CHAIR     = 4,                 // 坐在矮椅上
    UNIT_STAND_STATE_SIT_MEDIUM_CHAIR  = 5,                 // 坐在中等高度椅子上
    UNIT_STAND_STATE_SIT_HIGH_CHAIR    = 6,                 // 坐在高椅上
    UNIT_STAND_STATE_DEAD              = 7,                 // 死亡倒地
    UNIT_STAND_STATE_KNEEL             = 8,                 // 单膝跪地
    UNIT_STAND_STATE_CUSTOM            = 9                  // Depends on model animation. Submerge, freeze, hide, hibernate, rest (自定义动作状态，取决于模型动画：潜伏、冰冻、隐藏、休眠等)
};

#define MAX_UNIT_STAND_STATE             10

// byte flags value (UNIT_FIELD_BYTES_1,UNIT_BYTES_1_OFFSET_PET_LOYALTY)
// This corresponds to free talent points (pet case) (宠物场景下该字节对应可用训练点数/忠诚度)

// byte flags value (UNIT_FIELD_BYTES_1,2) (单位视觉外观标志位)
enum UnitVisFlags
{
    UNIT_VIS_FLAG_GHOST         = 0x01,                     // 幽灵半透明外观
    UNIT_VIS_FLAG_CREEP         = 0x02,                     // 潜行/匍匐外观
    UNIT_VIS_FLAG_UNTRACKABLE   = 0x04,                     // 无法被小地图追踪
    UNIT_VIS_FLAG_UNK4          = 0x08,
    UNIT_VIS_FLAG_UNK5          = 0x10,
    UNIT_VIS_FLAGS_ALL          = 0xFF
};

// byte flags value (UNIT_FIELD_BYTES_1,3)
// These flags seem to be related to visibility
// In wotlk+ they are moved to UNIT_FIELD_BYTES_1,2 (单位杂项标志位，WLK+ 已移至字节 2)
enum UnitMiscFlags
{
    UNIT_BYTE1_FLAG_ALWAYS_STAND = 0x01,                    // 始终保持站立动画
    UNIT_BYTE1_FLAG_FLY_ANIM     = 0x02,                    // Creature that can fly and are not on the ground appear to have this flag. If they are on the ground, flag is not present. (飞行生物离地时播放飞行动画标志)
    UNIT_BYTE1_FLAG_ALL          = 0xFF
};

// byte value (UNIT_FIELD_BYTES_2,UNIT_BYTES_2_OFFSET_SHEATH_STATE) (武器拔出/收起状态)
enum SheathState
{
    SHEATH_STATE_UNARMED  = 0,                              // non prepared weapon (收起武器/徒手)
    SHEATH_STATE_MELEE    = 1,                              // prepared melee weapon (拔出近战武器)
    SHEATH_STATE_RANGED   = 2                               // prepared ranged weapon (拔出远程武器)
};

#define MAX_SHEATH_STATE    3

// byte flags value (UNIT_FIELD_BYTES_2,1) (单位减益光环槽位上限常量)
enum UnitBytes2_Flags
{
    UNIT_BYTE2_CREATURE_DEBUFF_LIMIT            = 16,       // 生物默认减益上限
    UNIT_BYTE2_PLAYER_CONTROLLED_DEBUFF_LIMIT   = 40,       // 玩家控制单位减益上限
};

// byte flags value (UNIT_FIELD_BYTES_2,2) (宠物可操作标志)
enum UnitRename
{
    UNIT_CAN_BE_RENAMED     = 0x01,                         // 允许重命名宠物
    UNIT_CAN_BE_ABANDONED   = 0x02,                         // 允许遗弃/解散宠物
};

// byte flags value (UNIT_FIELD_BYTES_2,3)                  See enum ShapeshiftForm in SharedDefines.h

#define CREATURE_MAX_SPELLS     10                          // 生物技能栏最大法术数

// 武器挥舞动作类型
enum Swing
{
    NOSWING                    = 0,                         // 无挥舞
    SINGLEHANDEDSWING          = 1,                         // 单手挥舞
    TWOHANDEDSWING             = 2                          // 双手挥舞
};

// 受害者受击状态枚举（用于战斗日志与客户端受击动画）
enum VictimState : uint32
{
    VICTIMSTATE_UNAFFECTED     = 0,                         // seen in relation with HITINFO_MISS (未受影响，通常伴随未命中)
    VICTIMSTATE_NORMAL         = 1,                         // 正常受击
    VICTIMSTATE_DODGE          = 2,                         // 躲闪
    VICTIMSTATE_PARRY          = 3,                         // 招架
    VICTIMSTATE_INTERRUPT      = 4,                         // 施法被打断
    VICTIMSTATE_BLOCKS         = 5,                         // 格挡
    VICTIMSTATE_EVADES         = 6,                         // 闪避/脱战
    VICTIMSTATE_IS_IMMUNE      = 7,                         // 免疫
    VICTIMSTATE_DEFLECTS       = 8                          // 偏斜
};

// 攻击命中信息标志位（对应 SMSG_ATTACKERSTATEUPDATE）
enum HitInfo
{
    HITINFO_NORMALSWING         = 0x00000000,               // 普通主手挥击
    HITINFO_DEBUG               = 0x00000001,               // req correct packet structure (调试标志)
    HITINFO_NORMALSWING2        = 0x00000002,               // 普通近战挥击标志
    HITINFO_LEFTSWING           = 0x00000004,               // 副手/左手挥击
    HITINFO_UNK3                = 0x00000008,
    HITINFO_MISS                = 0x00000010,               // 未命中
    HITINFO_ABSORB              = 0x00000020,               // plays absorb sound (伤害被吸收，播放吸收音效)
    HITINFO_RESIST              = 0x00000040,               // resisted atleast some damage (部分或全部抗性抵抗)
    HITINFO_CRITICALHIT         = 0x00000080,               // 暴击
    HITINFO_UNK8                = 0x00000100,               // wotlk?
    HITINFO_UNK9                = 0x00000200,
    HITINFO_UNK10               = 0x00000400,
    HITINFO_BLOCK               = 0x00000800,               // 格挡
    HITINFO_UNK12               = 0x00001000,
    HITINFO_BLOOD_SPURT         = 0x00002000,               // blood spurt (飙血特效)
    HITINFO_GLANCING            = 0x00004000,               // 偏斜（等级差导致的降伤）
    HITINFO_CRUSHING            = 0x00008000,               // 碾压（高等级怪物攻击低等级目标）
    HITINFO_NOACTION            = 0x00010000,               // 无受击动作
    HITINFO_UNK17               = 0x00020000,
    HITINFO_UNK18               = 0x00040000,
    HITINFO_SWINGNOHITSOUND     = 0x00080000                // 挥击不播放命中音效
};

struct FactionTemplateEntry;
struct Modifier;
struct SpellEntry;
struct SpellEntryExt;

class Aura;
class SpellAuraHolder;
class Creature;
class Spell;
class DynamicObject;
class GameObject;
class Item;
class Pet;
class PetAura;
class Totem;
class SpellCastTargets;
class SpellCastArgs;

// 法术免疫条目：记录免疫类型及施加该免疫的光环来源
struct SpellImmune
{
    uint32 type;
    Aura const* aura;
};

typedef std::list<SpellImmune> SpellImmuneList;

// 单位属性修正阶段类型（总值 = ((BASE_VALUE * BASE_PCT) + TOTAL_VALUE) * TOTAL_PCT）
enum UnitModifierType
{
    BASE_VALUE = 0,                                         // 基础固定值加成
    BASE_PCT = 1,                                           // 基础百分比乘数
    TOTAL_VALUE = 2,                                        // 总值阶段固定加成
    TOTAL_PCT = 3,                                          // 总值阶段百分比乘数
    MODIFIER_TYPE_END = 4
};

// 武器伤害上下限索引
enum WeaponDamageRange
{
    MINDAMAGE,                                              // 最小伤害
    MAXDAMAGE                                               // 最大伤害
};

// 伤害/抗性按学派分类类型
enum DamageTypeToSchool
{
    RESISTANCE,                                             // 抗性
    DAMAGE_DEALT,                                           // 造成伤害
    DAMAGE_TAKEN                                            // 承受伤害
};

// 光环移除原因枚举
enum AuraRemoveMode
{
    AURA_REMOVE_BY_DEFAULT,                                 // 默认移除
    AURA_REMOVE_BY_STACK,                                   // at replace by similar aura (被同类高优先级/新施放光环顶替)
    AURA_REMOVE_BY_CANCEL,                                  // 玩家手动取消
    AURA_REMOVE_BY_DISPEL,                                  // 被驱散
    AURA_REMOVE_BY_DEATH,                                   // 单位死亡移除
    AURA_REMOVE_BY_DELETE,                                  // use for speedup and prevent unexpected effects at player logout/pet unsummon (must be used _only_ after save), delete. (登出或解散时直接清理，不触发常规移除副作用)
    AURA_REMOVE_BY_SHIELD_BREAK,                            // when absorb shield is removed by damage (吸收护盾耗尽破裂)
    AURA_REMOVE_BY_EXPIRE,                                  // at duration end (持续时间自然到期)
    AURA_REMOVE_BY_TRACKING,                                // aura is removed because of a conflicting tracked aura (被互斥追踪类光环替换)
    AURA_REMOVE_BY_GAINED_STACK                             // gained stack (层数叠加触发替换)
};

// 光环脚本回调挂载点枚举
enum AuraScriptLocation : uint32
{
    SCRIPT_LOCATION_MELEE_DAMAGE_DONE,                      // 造成近战伤害时
    SCRIPT_LOCATION_MELEE_DAMAGE_TAKEN,                     // 承受近战伤害时
    SCRIPT_LOCATION_SPELL_DAMAGE_DONE,                      // 造成法术伤害时
    SCRIPT_LOCATION_SPELL_DAMAGE_TAKEN,                     // 承受法术伤害时
    SCRIPT_LOCATION_SPELL_HEALING_DONE,                     // 造成法术治疗时
    SCRIPT_LOCATION_SPELL_HEALING_TAKEN,                    // 承受法术治疗时
    SCRIPT_LOCATION_CRIT_CHANCE,                            // 计算暴击几率时
    SCRIPT_LOCATION_MAX,
};

// 单位属性修正组索引（部分段落与 Stats / Powers / SpellSchools 枚举顺序严格对应）
enum UnitMods
{
    UNIT_MOD_STAT_STRENGTH,                                 // UNIT_MOD_STAT_STRENGTH..UNIT_MOD_STAT_SPIRIT must be in existing order, it's accessed by index values of Stats enum. (力量，与 Stats 枚举顺序一致)
    UNIT_MOD_STAT_AGILITY,                                  // 敏捷
    UNIT_MOD_STAT_STAMINA,                                  // 耐力
    UNIT_MOD_STAT_INTELLECT,                                // 智力
    UNIT_MOD_STAT_SPIRIT,                                   // 精神
    UNIT_MOD_HEALTH,                                        // 生命值上限
    UNIT_MOD_MANA,                                          // UNIT_MOD_MANA..UNIT_MOD_HAPPINESS must be in existing order, it's accessed by index values of Powers enum. (法力值，与 Powers 枚举顺序一致)
    UNIT_MOD_RAGE,                                          // 怒气值
    UNIT_MOD_FOCUS,                                         // 集中值
    UNIT_MOD_ENERGY,                                        // 能量值
    UNIT_MOD_HAPPINESS,                                     // 宠物快乐度
    UNIT_MOD_ARMOR,                                         // UNIT_MOD_ARMOR..UNIT_MOD_RESISTANCE_ARCANE must be in existing order, it's accessed by index values of SpellSchools enum. (护甲，与 SpellSchools 枚举顺序一致)
    UNIT_MOD_RESISTANCE_HOLY,                               // 神圣抗性
    UNIT_MOD_RESISTANCE_FIRE,                               // 火焰抗性
    UNIT_MOD_RESISTANCE_NATURE,                             // 自然抗性
    UNIT_MOD_RESISTANCE_FROST,                              // 冰霜抗性
    UNIT_MOD_RESISTANCE_SHADOW,                             // 暗影抗性
    UNIT_MOD_RESISTANCE_ARCANE,                             // 奥术抗性
    UNIT_MOD_ATTACK_POWER,                                  // 近战攻击强度
    UNIT_MOD_ATTACK_POWER_RANGED,                           // 远程攻击强度
    UNIT_MOD_DAMAGE_MAINHAND,                               // 主手物理伤害
    UNIT_MOD_DAMAGE_OFFHAND,                                // 副手物理伤害
    UNIT_MOD_DAMAGE_RANGED,                                 // 远程物理伤害
    UNIT_MOD_END,
    // synonyms (区间别名)
    UNIT_MOD_STAT_START = UNIT_MOD_STAT_STRENGTH,
    UNIT_MOD_STAT_END = UNIT_MOD_STAT_SPIRIT + 1,
    UNIT_MOD_RESISTANCE_START = UNIT_MOD_ARMOR,
    UNIT_MOD_RESISTANCE_END = UNIT_MOD_RESISTANCE_ARCANE + 1,
    UNIT_MOD_POWER_START = UNIT_MOD_MANA,
    UNIT_MOD_POWER_END = UNIT_MOD_HAPPINESS + 1
};

// 玩家基础战斗修正组（暴击率、格挡值）
enum BaseModGroup
{
    CRIT_PERCENTAGE,                                        // 主手近战暴击率
    RANGED_CRIT_PERCENTAGE,                                 // 远程暴击率
    OFFHAND_CRIT_PERCENTAGE,                                // 副手近战暴击率
    SHIELD_BLOCK_VALUE,                                     // 盾牌格挡值
    BASEMOD_END
};

// 基础修正值类型（固定值 / 百分比）
enum BaseModType
{
    FLAT_MOD,                                               // 固定值修正
    PCT_MOD,                                                // 百分比修正
    MOD_END
};

// 单位生死状态枚举
enum DeathState
{
    ALIVE          = 0,                                     // show as alive (存活)
    JUST_DIED      = 1,                                     // temporary state at die, for creature auto converted to CORPSE, for player at next update call (刚死亡过渡态)
    CORPSE         = 2,                                     // corpse state, for player this also meaning that player not leave corpse (尸体状态，玩家表示未释放灵魂)
    DEAD           = 3,                                     // for creature despawned state (corpse despawned), for player CORPSE/DEAD not clear way switches (FIXME), and use m_deathtimer > 0 check for real corpse state (已死亡/尸体消失状态)
    JUST_ALIVED    = 4,                                     // temporary state at resurrection, for creature auto converted to ALIVE, for player at next update call (刚复活过渡态)
};

// internal state flags for some auras and movement generators, other. (单位内部行为与移动生成器状态位掩码)
enum UnitState
{
    // persistent state (applied by aura/etc until expire) (持续性控制/行为状态，由光环等施加)
    UNIT_STAT_MELEE_ATTACKING = 0x00000001,                 // unit is melee attacking someone Unit::Attack (正在进行近战攻击)
    UNIT_STAT_LOGOUT_TIMER    = 0x00000002,                 // Unit is logging out (正在登出倒计时)
    UNIT_STAT_FEIGN_DEATH     = 0x00000004,                 // Unit::SetFeignDeath - a successful feign death is currently active (假死生效中)
    UNIT_STAT_STUNNED         = 0x00000008,                 // Aura::HandleAuraModStun (处于昏迷状态)
    UNIT_STAT_ROOT            = 0x00000010,                 // Aura::HandleAuraModRoot (处于定身状态)
    UNIT_STAT_ISOLATED        = 0x00000020,                 // area auras do not affect other players, Aura::HandleAuraModSchoolImmunity (被隔离，区域光环不影响他人)
    UNIT_STAT_POSSESSED       = 0x00000040,                 // Aura::HandleAuraModPossess (duplicates UNIT_FLAG_POSSESSED) (被精神控制/附身)

    // movement generators begin: (移动生成器栈状态位)
    UNIT_STAT_TAXI_FLIGHT     = 0x00000080,                 // TaxiMovementGenerator on stack (飞行路线乘骑中)
    UNIT_STAT_PROPELLED       = 0x00000100,                 // EffectMovementGenerator on stack (击退/冲锋等位移效果中)
    UNIT_STAT_PANIC           = 0x00000200,                 // PanicMovementGenerator on stack (恐慌逃跑中)
    UNIT_STAT_RETREATING      = 0x00000400,                 // RetreatMovementGenerator on stack (撤退求援中)
    UNIT_STAT_DISTRACTED      = 0x00000800,                 // DistractedMovementGenerator on stack (被扰乱停步中)
    UNIT_STAT_STAY            = 0x00001000,                 // StayMovementGenerator on stack (原地停留指令中)
    UNIT_STAT_CONFUSED        = 0x00002000,                 // ConfusedMovementGenerator on stack (处于迷惑移动生成器)
    UNIT_STAT_CONFUSED_MOVE   = 0x00004000,                 // ^ - spline dispatched (迷惑移动路径已派发)
    UNIT_STAT_ROAMING         = 0x00008000,                 // Point/Retreat/Stay/Wander/Waypoint/Effect MovementGenerator on stack (巡逻/定点/漫游中)
    UNIT_STAT_ROAMING_MOVE    = 0x00010000,                 // ^ - spline dispatched (漫游移动路径已派发)
    UNIT_STAT_CHASE           = 0x00020000,                 // ChaseMovementGenerator on stack (追击目标中)
    UNIT_STAT_CHASE_MOVE      = 0x00040000,                 // ^ - spline dispatched (追击移动路径已派发)
    UNIT_STAT_FOLLOW          = 0x00080000,                 // FollowMovementGenerator on stack (跟随目标中)
    UNIT_STAT_FOLLOW_MOVE     = 0x00100000,                 // ^ - spline dispatched (跟随移动路径已派发)
    UNIT_STAT_FLEEING         = 0x00200000,                 // FleeingMovementGenerator/PanicMovementGenerator on stack (逃跑生成器栈激活)
    UNIT_STAT_FLEEING_MOVE    = 0x00400000,                 // ^ - spline dispatched (逃跑移动路径已派发)
    // movemement generators end

    UNIT_STAT_CHANNELING      = 0x00800000,                 // 正在引导法术

    // High-Level states (usually only with Creatures) (高级状态位，通常用于生物 AI 控制)
    UNIT_STAT_NO_COMBAT_MOVEMENT    = 0x01000000,           // Combat Movement for MoveChase stopped (禁止战斗追击移动)
    UNIT_STAT_NO_FOLLOW_MOVEMENT    = 0x02000000,           // 禁止跟随移动
    UNIT_STAT_RUNNING               = 0x04000000,           // SetRun for waypoints and such (路径点等移动使用奔跑速度)
    UNIT_STAT_WAYPOINT_PAUSED       = 0x08000000,           // Waypoint-Movement paused genericly (ie by script) (路径点移动被脚本暂停)

    UNIT_STAT_IGNORE_PATHFINDING    = 0x10000000,           // do not use pathfinding in any MovementGenerator (禁用寻路，走直线)
    UNIT_STAT_AI_ROOT               = 0x20000000,           // root but from AI script (由 AI 脚本施加的定身)

    // masks (only for check) (组合掩码，仅用于状态判定)

    // can't move currently (当前无法移动)
    UNIT_STAT_CAN_NOT_MOVE    = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_LOGOUT_TIMER | UNIT_STAT_FEIGN_DEATH | UNIT_STAT_AI_ROOT,

    // stay by different reasons (因控制/扰乱而停止移动)
    UNIT_STAT_NOT_MOVE        = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_LOGOUT_TIMER | UNIT_STAT_FEIGN_DEATH |
                                UNIT_STAT_DISTRACTED | UNIT_STAT_AI_ROOT,

    // stay or scripted movement for effect( = in player case you can't move by client command) (玩家无法通过客户端自主移动的状态)
    UNIT_STAT_NO_FREE_MOVE    = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_LOGOUT_TIMER | UNIT_STAT_FEIGN_DEATH |
                                UNIT_STAT_TAXI_FLIGHT |
                                UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING | UNIT_STAT_PROPELLED | UNIT_STAT_AI_ROOT,

    // not react at move in sight or other (无法对视野内移动或事件作出反应)
    UNIT_STAT_CAN_NOT_REACT   = UNIT_STAT_STUNNED | UNIT_STAT_LOGOUT_TIMER | UNIT_STAT_FEIGN_DEATH |
                                UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING | UNIT_STAT_RETREATING,

    // AI disabled by some reason (AI 被控制效果接管/禁用)
    UNIT_STAT_LOST_CONTROL    = UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING | UNIT_STAT_POSSESSED,

    // above 2 state cases (无法反应或失去控制)
    UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL  = UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_LOST_CONTROL,

    // masks (for check or reset) (移动中状态组合掩码)

    // for real move using movegen check and stop (except unstoppable flight) (正在通过移动生成器实际移动)
    UNIT_STAT_MOVING          = UNIT_STAT_ROAMING_MOVE | UNIT_STAT_CHASE_MOVE | UNIT_STAT_FOLLOW_MOVE | UNIT_STAT_FLEEING_MOVE,

    UNIT_STAT_RUNNING_STATE   = UNIT_STAT_CHASE_MOVE | UNIT_STAT_FLEEING_MOVE | UNIT_STAT_RUNNING,

    UNIT_STAT_ALL_STATE       = 0xFFFFFFFF,
    UNIT_STAT_ALL_DYN_STATES  = UNIT_STAT_ALL_STATE & ~(UNIT_STAT_NO_COMBAT_MOVEMENT | UNIT_STAT_RUNNING | UNIT_STAT_WAYPOINT_PAUSED | UNIT_STAT_IGNORE_PATHFINDING),
};

#define BASE_CHARGE_SPEED 27.0f                             // 冲锋类技能基础移动速度

// 各移动类型对应的速度同步协议操作码表 [强制变更, 样条设置, 广播更新]
typedef const Opcodes SpeedOpcodePair[3];
SpeedOpcodePair SetSpeed2Opc_table[MAX_MOVE_TYPE] =
{
    {SMSG_FORCE_WALK_SPEED_CHANGE,        SMSG_SPLINE_SET_WALK_SPEED,           MSG_MOVE_SET_WALK_SPEED},
    {SMSG_FORCE_RUN_SPEED_CHANGE,         SMSG_SPLINE_SET_RUN_SPEED,            MSG_MOVE_SET_RUN_SPEED},
    {SMSG_FORCE_RUN_BACK_SPEED_CHANGE,    SMSG_SPLINE_SET_RUN_BACK_SPEED,       MSG_MOVE_SET_RUN_BACK_SPEED},
    {SMSG_FORCE_SWIM_SPEED_CHANGE,        SMSG_SPLINE_SET_SWIM_SPEED,           MSG_MOVE_SET_SWIM_SPEED},
    {SMSG_FORCE_SWIM_BACK_SPEED_CHANGE,   SMSG_SPLINE_SET_SWIM_BACK_SPEED,      MSG_MOVE_SET_SWIM_BACK_SPEED},
    {SMSG_FORCE_TURN_RATE_CHANGE,         SMSG_SPLINE_SET_TURN_RATE,            MSG_MOVE_SET_TURN_RATE},
    {SMSG_FORCE_FLIGHT_SPEED_CHANGE,      SMSG_SPLINE_SET_FLIGHT_SPEED,         MSG_MOVE_SET_FLIGHT_SPEED},
    {SMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE, SMSG_SPLINE_SET_FLIGHT_BACK_SPEED,    MSG_MOVE_SET_FLIGHT_BACK_SPEED},
};

// 战斗等级类型枚举（TBC 等级转换系统）
enum CombatRating
{
    CR_WEAPON_SKILL             = 0,                        // 武器技能等级
    CR_DEFENSE_SKILL            = 1,                        // 防御等级
    CR_DODGE                    = 2,                        // 躲闪等级
    CR_PARRY                    = 3,                        // 招架等级
    CR_BLOCK                    = 4,                        // 格挡等级
    CR_HIT_MELEE                = 5,                        // 近战命中等级
    CR_HIT_RANGED               = 6,                        // 远程命中等级
    CR_HIT_SPELL                = 7,                        // 法术命中等级
    CR_CRIT_MELEE               = 8,                        // 近战暴击等级
    CR_CRIT_RANGED              = 9,                        // 远程暴击等级
    CR_CRIT_SPELL               = 10,                       // 法术暴击等级
    CR_HIT_TAKEN_MELEE          = 11,                       // 近战受击命中减免等级
    CR_HIT_TAKEN_RANGED         = 12,                       // 远程受击命中减免等级
    CR_HIT_TAKEN_SPELL          = 13,                       // 法术受击命中减免等级
    CR_CRIT_TAKEN_MELEE         = 14,                       // 近战韧性/受爆免伤等级
    CR_CRIT_TAKEN_RANGED        = 15,                       // 远程韧性/受爆免伤等级
    CR_CRIT_TAKEN_SPELL         = 16,                       // 法术韧性/受爆免伤等级
    CR_HASTE_MELEE              = 17,                       // 近战急速等级
    CR_HASTE_RANGED             = 18,                       // 远程急速等级
    CR_HASTE_SPELL              = 19,                       // 法术急速等级
    CR_WEAPON_SKILL_MAINHAND    = 20,                       // 主手武器技能等级
    CR_WEAPON_SKILL_OFFHAND     = 21,                       // 副手武器技能等级
    CR_WEAPON_SKILL_RANGED      = 22,                       // 远程武器技能等级
    CR_EXPERTISE                = 23                        // 精准等级
};

#define MAX_COMBAT_RATING         24

/// internal used flags for marking special auras - for example some dummy-auras (内部使用的特殊光环标志)
enum UnitAuraFlags
{
    UNIT_AURAFLAG_ALIVE_INVISIBLE   = 0x1,                  // aura which makes unit invisible for alive (使单位对存活单位不可见的光环标志)
};

// 单位可见性模式枚举
enum UnitVisibility
{
    VISIBILITY_OFF                = 0,                      // absolute, not detectable, GM-like, can see all other (完全隐身，GM 级不可侦测)
    VISIBILITY_ON                 = 1,                      // 正常可见
    VISIBILITY_GROUP_STEALTH      = 2,                      // detect chance, seen and can see group members (潜行：按侦测几率可见，队友可见)
    VISIBILITY_GROUP_INVISIBILITY = 3,                      // invisibility, can see and can be seen only another invisible unit or invisible detection unit, set only if not stealthed, and in checks not used (mask used instead) (隐形：仅同为隐形或具侦测隐形能力者可见)
    VISIBILITY_GROUP_NO_DETECT    = 4,                      // state just at stealth apply for update Grid state. Don't remove, otherwise stealth spells will break (刚进入潜行刷新网格视野用的过渡状态)
    VISIBILITY_REMOVE_CORPSE      = 5                       // special totally not detectable visibility for force delete object while removing a corpse (移除尸体时强制不可见的过渡状态)
};

// Value masks for UNIT_FIELD_FLAGS (单位主标志位掩码)
enum UnitFlags
{
    UNIT_FLAG_UNK_0                 = 0x00000001,           // Movement checks disabled, likely paired with loss of client control packet. We use it to add custom cliffwalking to GM mode until actual usecases will be known. (禁用移动校验，常配合失控包使用)
    UNIT_FLAG_SPAWNING              = 0x00000002,           // Spawning in - other uses are wrong (正在出生/刷出过程中)
    UNIT_FLAG_CLIENT_CONTROL_LOST   = 0x00000004,           // Generic unspecified loss of control initiated by server script, movement checks disabled, paired with loss of client control packet. (服务器脚本导致的通用客户端失控)
    UNIT_FLAG_PLAYER_CONTROLLED     = 0x00000008,           // players, pets, totems, guardians, companions, charms, any units associated with players (受玩家控制的单位：玩家、宠物、图腾、守卫、被魅惑单位等)
    UNIT_FLAG_EVADING_HOME          = 0x00000010,           // TBC+ unsure what version, used in CG_Unit_C::CanSwim (正在逃脱返回出生点)
    UNIT_FLAG_PREPARATION           = 0x00000020,           // don't take reagents for spells with SPELL_ATTR_EX5_NO_REAGENT_WHILE_PREP (战场准备阶段，不消耗特定技能材料)
    UNIT_FLAG_UNK_6                 = 0x00000040,
    UNIT_FLAG_NOT_ATTACKABLE_1      = 0x00000080,           // ?? (UNIT_FLAG_PVP_ATTACKABLE | UNIT_FLAG_NOT_ATTACKABLE_1) is NON_PVP_ATTACKABLE (不可被攻击标志)
    UNIT_FLAG_IMMUNE_TO_PLAYER      = 0x00000100,           // Target is immune to players (对玩家免疫攻击)
    UNIT_FLAG_IMMUNE_TO_NPC         = 0x00000200,           // Target is immune to Non-Player Characters (对 NPC 免疫攻击)
    UNIT_FLAG_LOOTING               = 0x00000400,           // loot animation (拾取动画中)
    UNIT_FLAG_PET_IN_COMBAT         = 0x00000800,           // in combat?, 2.0.8 (宠物处于战斗中)
    UNIT_FLAG_PVP                   = 0x00001000,           // 开启 PvP 标记
    UNIT_FLAG_SILENCED              = 0x00002000,           // silenced, 2.1.1 (被沉默)
    UNIT_FLAG_PERSUADED             = 0x00004000,           // persuaded, 2.0.8 (被说服/临时友善)
    UNIT_FLAG_SWIMMING              = 0x00008000,           // controls water swimming animation - TODO: confirm whether dynamic or static (游泳状态动画)
    UNIT_FLAG_UNTARGETABLE          = 0x00010000,           // is not targetable by attack or spell (不可被攻击或法术选中)
    UNIT_FLAG_PACIFIED              = 0x00020000,           // 被安抚/平静（无法近战攻击）
    UNIT_FLAG_STUNNED               = 0x00040000,           // Unit is a subject to stun, turn and strafe movement disabled (被昏迷，禁止移动与转向)
    UNIT_FLAG_IN_COMBAT             = 0x00080000,           // 处于战斗状态
    UNIT_FLAG_TAXI_FLIGHT           = 0x00100000,           // Unit is on taxi, paired with a duplicate loss of client control packet (likely a legacy serverside hack). Disables any spellcasts not allowed in taxi flight client-side. (处于飞行路线交通工具上)
    UNIT_FLAG_DISARMED              = 0x00200000,           // disable melee spells casting..., "Required melee weapon" added to melee spells tooltip. (被缴械，禁用需要近战武器的技能)
    UNIT_FLAG_CONFUSED              = 0x00400000,           // Unit is a subject to confused movement, movement checks disabled, paired with loss of client control packet. (处于迷惑状态)
    UNIT_FLAG_FLEEING               = 0x00800000,           // Unit is a subject to fleeing movement, movement checks disabled, paired with loss of client control packet. (处于恐惧/逃跑状态)
    UNIT_FLAG_POSSESSED             = 0x01000000,           // Unit is under remote control by another unit, movement checks disabled, paired with loss of client control packet. New master is allowed to use melee attack and can't select this unit via mouse in the world (as if it was own character). (被远程附身控制)
    UNIT_FLAG_UNINTERACTIBLE        = 0x02000000,           // 不可交互（客户端鼠标无法选中）
    UNIT_FLAG_SKINNABLE             = 0x04000000,           // 可剥皮/采矿/草药采集尸体
    UNIT_FLAG_MOUNT                 = 0x08000000,           // 骑乘状态
    UNIT_FLAG_UNK_28                = 0x10000000,
    UNIT_FLAG_PREVENT_ANIM          = 0x20000000,           // used in Feing Death spell (阻止常规动画播放，用于假死)
    UNIT_FLAG_SHEATHE               = 0x40000000,           // 收起武器标志
    UNIT_FLAG_IMMUNE                = 0x80000000            // 全局免疫标志
};

// Value masks for UNIT_FIELD_FLAGS_2 (单位扩展标志位掩码)
enum UnitFlags2
{
    UNIT_FLAG2_FEIGN_DEATH          = 0x00000001,           // 假死标志
    UNIT_FLAG2_HIDE_BODY            = 0x00000002,           // Hides body and body armor. Weapons and shoulder and head armor still visible (隐藏躯干与胸甲，仅显示武器/肩/头)
    UNIT_FLAG2_IGNORE_REPUTATION    = 0x00000004,           // 忽略声望限制
    UNIT_FLAG2_COMPREHEND_LANG      = 0x00000008,           // 通晓所有语言
    UNIT_FLAG2_CLONED               = 0x00000010,           // Used in SPELL_AURA_MIRROR_IMAGE (镜像克隆体标志)
    UNIT_FLAG2_DO_NOT_FADE_IN       = 0x00000020,           // 出现时不淡入
    UNIT_FLAG2_FORCE_MOVE           = 0x00000040,           // 强制移动状态
    // UNIT_FLAG2_DISARM_OFFHAND       = 0x00000080,        // also shield case - added in 3.x, possible all later not used in pre-3.x
    // UNIT_FLAG2_UNK8                 = 0x00000100,
    // UNIT_FLAG2_UNK9                 = 0x00000200,
    // UNIT_FLAG2_DISARM_RANGED        = 0x00000400,        // added in 3.x
    // UNIT_FLAG2_REGENERATE_POWER     = 0x00000800,        // added in 3.x
};

/// Non Player Character flags (NPC 交互功能标志位)
enum NPCFlags
{
    UNIT_NPC_FLAG_NONE                  = 0x00000000,
    UNIT_NPC_FLAG_GOSSIP                = 0x00000001,       // 100% (对话菜单)
    UNIT_NPC_FLAG_QUESTGIVER            = 0x00000002,       // guessed, probably ok (任务发布者)
    UNIT_NPC_FLAG_UNK1                  = 0x00000004,
    UNIT_NPC_FLAG_UNK2                  = 0x00000008,
    UNIT_NPC_FLAG_TRAINER               = 0x00000010,       // 100% (通用训练师)
    UNIT_NPC_FLAG_TRAINER_CLASS         = 0x00000020,       // 100% (职业训练师)
    UNIT_NPC_FLAG_TRAINER_PROFESSION    = 0x00000040,       // 100% (专业技能训练师)
    UNIT_NPC_FLAG_VENDOR                = 0x00000080,       // 100% (商人)
    UNIT_NPC_FLAG_VENDOR_AMMO           = 0x00000100,       // 100%, general goods vendor (弹药/杂货商)
    UNIT_NPC_FLAG_VENDOR_FOOD           = 0x00000200,       // 100% (食物商人)
    UNIT_NPC_FLAG_VENDOR_POISON         = 0x00000400,       // guessed (毒药商人)
    UNIT_NPC_FLAG_VENDOR_REAGENT        = 0x00000800,       // 100% (施法材料商人)
    UNIT_NPC_FLAG_REPAIR                = 0x00001000,       // 100% (装备修理)
    UNIT_NPC_FLAG_FLIGHTMASTER          = 0x00002000,       // 100% (飞行管理员)
    UNIT_NPC_FLAG_SPIRITHEALER          = 0x00004000,       // guessed (灵魂医者)
    UNIT_NPC_FLAG_SPIRITGUIDE           = 0x00008000,       // guessed (战场灵魂向导)
    UNIT_NPC_FLAG_INNKEEPER             = 0x00010000,       // 100% (旅店老板)
    UNIT_NPC_FLAG_BANKER                = 0x00020000,       // 100% (银行职员)
    UNIT_NPC_FLAG_PETITIONER            = 0x00040000,       // 100% 0xC0000 = guild petitions, 0x40000 = arena team petitions (公会/竞技场签名注册员)
    UNIT_NPC_FLAG_TABARDDESIGNER        = 0x00080000,       // 100% (战袍设计者)
    UNIT_NPC_FLAG_BATTLEMASTER          = 0x00100000,       // 100% (战场军官)
    UNIT_NPC_FLAG_AUCTIONEER            = 0x00200000,       // 100% (拍卖师)
    UNIT_NPC_FLAG_STABLEMASTER          = 0x00400000,       // 100% (兽栏管理员)
    UNIT_NPC_FLAG_GUILD_BANKER          = 0x00800000,       // cause client to send 997 opcode (公会银行职员)
    UNIT_NPC_FLAG_SPELLCLICK            = 0x01000000,       // cause client to send 1015 opcode (spell click), dynamic, set at loading and don't must be set in DB (可点击触发法术交互)
};

namespace Movement
{
    class MoveSpline;
}

/**
 * The different available diminishing return levels. (递减收益等级枚举)
 * \see DiminishingReturn
 */
enum DiminishingLevels
{
    DIMINISHING_LEVEL_1             = 0,         //< Won't make a difference to stun duration (第 1 级：完整持续时间，无递减)
    DIMINISHING_LEVEL_2             = 1,         //< Reduces stun time by 50% (第 2 级：持续时间减半 50%)
    DIMINISHING_LEVEL_3             = 2,         //< Reduces stun time by 75% (第 3 级：持续时间减少 75%)
    DIMINISHING_LEVEL_IMMUNE        = 3          //< The target is immune to the DiminishingGrouop (免疫级：目标对该递减组完全免疫)
};

/**
 * Structure to keep track of diminishing returns, for more information
 * about the idea behind diminishing returns, see: http://www.wowwiki.com/Diminishing_returns
 * (递减收益追踪结构体，用于记录某控制递减组的命中次数与时间)
 * \see Unit::GetDiminishing
 * \see Unit::IncrDiminishing
 * \see Unit::ApplyDiminishingToDuration
 * \see Unit::ApplyDiminishingAura
 */
struct DiminishingReturn
{
    DiminishingReturn(DiminishingGroup group, uint32 t, uint32 count)
        : DRGroup(group), stack(0), hitTime(t), hitCount(count)
    {}

    /**
     * Group that this diminishing return will affect (所属递减收益组)
     */
    DiminishingGroup        DRGroup: 16;
    /**
     * Seems to be how many times this has been stacked, modified in
     * Unit::ApplyDiminishingAura (当前生效的光环叠层计数)
     */
    uint16                  stack: 16;
    /**
     * Records at what time the last hit with this DiminishingGroup was done, if it's
     * higher than 15 seconds (ie: 15 000 ms) the DiminishingReturn::hitCount will be reset
     * to DiminishingLevels::DIMINISHING_LEVEL_1, which will do no difference to the duration
     * of the stun etc. (最近一次命中时间，超过 15 秒无新命中则重置递减计数)
     */
    uint32                  hitTime;
    /**
     * Records how many times a spell of this DiminishingGroup has hit, this in turn
     * decides how how long the duration of the stun etc is. (该递减组累计命中次数)
     */
    uint32                  hitCount;
};

// At least some values expected fixed and used in auras field, other custom (近战攻击命中结果枚举)
enum MeleeHitOutcome
{
    MELEE_HIT_EVADE     = 0,                                // 闪避/脱战
    MELEE_HIT_MISS      = 1,                                // 未命中
    MELEE_HIT_DODGE     = 2,                                // used as misc in SPELL_AURA_IGNORE_COMBAT_RESULT (躲闪)
    MELEE_HIT_BLOCK     = 3,                                // used as misc in SPELL_AURA_IGNORE_COMBAT_RESULT (格挡)
    MELEE_HIT_PARRY     = 4,                                // used as misc in SPELL_AURA_IGNORE_COMBAT_RESULT (招架)
    MELEE_HIT_GLANCING  = 5,                                // 偏斜
    MELEE_HIT_CRIT      = 6,                                // 暴击
    MELEE_HIT_CRUSHING  = 7,                                // 碾压
    MELEE_HIT_NORMAL    = 8,                                // 普通命中
    MELEE_HIT_BLOCK_CRIT = 9,                               // 格挡暴击
};

// 战斗骰子圆桌事件分面枚举（用于攻击判定与调试输出）
enum UnitCombatDieSide
{
    UNIT_COMBAT_DIE_MISS,                                   // 未命中
    UNIT_COMBAT_DIE_RESIST,                                 // 抵抗
    UNIT_COMBAT_DIE_DODGE,                                  // 躲闪
    UNIT_COMBAT_DIE_PARRY,                                  // 招架
    UNIT_COMBAT_DIE_DEFLECT,                                // 偏斜/招架远程
    UNIT_COMBAT_DIE_BLOCK,                                  // 格挡
    UNIT_COMBAT_DIE_GLANCE,                                 // 偏斜打击
    UNIT_COMBAT_DIE_CRIT,                                   // 暴击
    UNIT_COMBAT_DIE_CRUSH,                                  // 碾压
    UNIT_COMBAT_DIE_HIT,                                    // 普通命中
};

#define NUM_UNIT_COMBAT_DIE_SIDES (UNIT_COMBAT_DIE_HIT + 1)

// A little helper func for a nice attack table debug output (攻击圆桌各分面枚举转可读文本辅助函数)
inline const char* UnitCombatDieSideText(UnitCombatDieSide side)
{
    switch (side)
    {
        case UNIT_COMBAT_DIE_MISS:    return "MISS";
        case UNIT_COMBAT_DIE_RESIST:  return "RESIST";
        case UNIT_COMBAT_DIE_DODGE:   return "DODGE";
        case UNIT_COMBAT_DIE_PARRY:   return "PARRY";
        case UNIT_COMBAT_DIE_DEFLECT: return "DEFLECT";
        case UNIT_COMBAT_DIE_BLOCK:   return "BLOCK";
        case UNIT_COMBAT_DIE_GLANCE:  return "GLANCE";
        case UNIT_COMBAT_DIE_CRIT:    return "CRIT";
        case UNIT_COMBAT_DIE_CRUSH:   return "CRUSH";
        case UNIT_COMBAT_DIE_HIT:     return "HIT";
    }
    return "INVALID";
}

// 净伤害信息结构（主要用于怒气获取计算）
struct CleanDamage
{
    CleanDamage(uint32 _damage, WeaponAttackType _attackType, MeleeHitOutcome _hitOutCome, bool _takenOrAbsorbedDamage) :
        damage(_damage), attackType(_attackType), hitOutCome(_hitOutCome), takenOrAbsorbedDamage(_takenOrAbsorbedDamage) {}

    uint32 damage; // only used for rage generation (仅用于怒气生成计算的减免后或原始伤害值)
    WeaponAttackType attackType;
    MeleeHitOutcome hitOutCome;
    bool takenOrAbsorbedDamage;
};

// 单系物理/元素子伤害明细（支持附带元素伤害的武器）
struct SubDamageInfo
{
    SpellSchoolMask damageSchoolMask = SPELL_SCHOOL_MASK_NORMAL;
    uint32 damage = 0;
    uint32 absorb = 0;
    int32 resist = 0;
};

// Struct for use in Unit::CalculateMeleeDamage
// Need create structure like in SMSG_ATTACKERSTATEUPDATE opcode (近战伤害计算结构体，对应 SMSG_ATTACKERSTATEUPDATE 封包布局)
struct CalcDamageInfo
{
    Unit*  attacker;             // Attacker (攻击方)
    Unit*  target;               // Target for damage (受击目标)
    uint32 totalDamage;          // 总伤害
    SubDamageInfo subDamage[MAX_ITEM_PROTO_DAMAGES];
    uint32 blockedAmount;        // 被格挡伤害量
    uint32 HitInfo;              // 命中信息位掩码
    VictimState TargetState;     // 目标受击状态
    uint32 meleeSpellId;         // next swing spells (下一次挥击附加近战技能 ID)
    uint32 attackerState;        // no known use in client (客户端未使用的攻击者状态字段)
    // Helper (服务端内部辅助字段)
    WeaponAttackType attackType; // 攻击槽位（主手/副手/远程）
    uint32 procAttacker;         // 攻击方触发标记
    uint32 procVictim;           // 受击方触发标记
    uint32 procEx;               // 扩展触发掩码
    uint32 cleanDamage;          // Used only for rage calculation (用于怒气计算的净伤害)
    uint32 absorb;               // 吸收总量
    MeleeHitOutcome hitOutCome;  // TODO: remove this field (need use TargetState)
};

// Spell damage info structure based on structure sending in SMSG_SPELLNONMELEEDAMAGELOG opcode (非近战法术伤害日志结构，对应 SMSG_SPELLNONMELEEDAMAGELOG)
struct SpellNonMeleeDamage
{
    SpellNonMeleeDamage(WorldObject* _attacker, Unit* _target, uint32 _SpellID, SpellSchoolMask _schoolMask, Spell* spell = nullptr)
        : target(_target), attacker(_attacker), SpellID(_SpellID), damage(0), schoolMask(_schoolMask),
          absorb(0), resist(0), periodicLog(false), unused(false), blocked(0), HitInfo(0), spell(spell)
    {}

    Unit*   target;              // 受击目标
    WorldObject* attacker;       // 施法/攻击方
    uint32 SpellID;              // 法术 ID
    uint32 damage;               // 最终伤害
    SpellSchoolMask schoolMask;  // 伤害学派掩码
    uint32 absorb;               // 吸收量
    int32  resist;               // 抵抗量
    bool   periodicLog;          // 是否作为周期性伤害记录
    bool   unused;
    uint32 blocked;              // 格挡量
    uint32 HitInfo;              // 命中信息位掩码（暴击/未命中等）
    Spell* spell;                // 关联法术实例
};

// 周期性光环效果日志结构
struct SpellPeriodicAuraLogInfo
{
    SpellPeriodicAuraLogInfo(Aura* _aura, uint32 _damage, uint32 _absorb, int32 _resist, float _multiplier)
        : aura(_aura), damage(_damage), absorb(_absorb), resist(_resist), multiplier(_multiplier) {}

    Aura*   aura;                // 触发周期效果的光环
    uint32 damage;               // 跳数伤害或治疗量
    uint32 absorb;               // 被吸收量
    int32 resist;                // 被抵抗量
    float  multiplier;           // 效果倍率
};

uint32 createProcExtendMask(SpellNonMeleeDamage* damageInfo, SpellMissInfo missCondition);

// 单位战斗仇恨数据容器（包含被仇恨与主动敌对引用管理器）
struct CombatData
{
    public:
        CombatData(Unit* owner) : threatManager(ThreatManager(owner)), hostileRefManager(HostileRefManager(owner)) {};

        // Manage all Units threatening us (管理所有对自身存在仇恨的目标列表)
        ThreatManager threatManager;
        // Manage all Units that are threatened by us (管理自身出现在哪些目标的仇恨列表中)
        HostileRefManager hostileRefManager;
};

// 光环触发（Proc）处理器返回状态
enum SpellAuraProcResult
{
    SPELL_AURA_PROC_OK              = 0,                    // proc was processed, will remove charges (触发成功，消耗充能次数)
    SPELL_AURA_PROC_FAILED          = 1,                    // proc failed - if at least one aura failed the proc, charges won't be taken (触发失败，不消耗充能)
    SPELL_AURA_PROC_CANT_TRIGGER    = 2,                    // aura can't trigger - skip charges taking, move to next aura if exists (当前光环不可触发，跳过并继续检查后续光环)
};

// Unit* victim, uint32 procAttacker, uint32 procVictim, uint32 procExtra, uint32 amount, WeaponAttackType attType, SpellEntry const* spellInfo, bool dontTriggerSpecial

// External struct for passing on data (外部传入触发系统的参数包)
struct ProcSystemArguments
{
    Unit* attacker;                                         // 攻击/施法者
    Unit* victim;                                           // 受击/受疗者

    uint32 procFlagsAttacker;                               // 攻击者触发事件标志
    uint32 procFlagsVictim;                                 // 受害者触发事件标志
    uint32 procExtra;                                       // 额外触发细分标志（暴击、躲闪等）

    uint32 damage; // contains full heal or full damage (完整伤害或完整治疗数值)
    uint32 absorb; // absorbed damage (被吸收的伤害量)
    SpellEntry const* spellInfo;                            // 触发源法术原型
    WeaponAttackType attType;                               // 攻击武器槽位

    Spell* spell;                                           // 触发源法术对象

    // Healing specific information (治疗类触发专用信息)
    uint32 healthGain;                                      // 实际有效生命回复量
    bool isHeal;                                            // 是否为治疗事件

    explicit ProcSystemArguments(Unit* attacker, Unit* victim, uint32 procFlagsAttacker, uint32 procFlagsVictim, uint32 procExtra, uint32 amount, uint32 absorb, WeaponAttackType attType = BASE_ATTACK,
        SpellEntry const* spellInfo = nullptr, Spell* spell = nullptr, uint32 healthGain = 0, bool isHeal = false) : attacker(attacker), victim(victim), procFlagsAttacker(procFlagsAttacker), procFlagsVictim(procFlagsVictim), procExtra(procExtra), damage(amount),
        absorb(absorb), spellInfo(spellInfo), attType(attType), spell(spell), healthGain(healthGain), isHeal(isHeal)
    {
    }
};

// Internal struct for passing data to execution (触发处理器内部执行的上下文数据)
struct ProcExecutionData
{
    // these are same for proc on attackers auras and victims auras (无论检查攻击方还是受击方光环均保持不变)
    Unit* attacker;
    Unit* victim;

    // these change based on isVictim (随 isVictim 切换源与目标视角)
    bool isVictim;
    Unit* source;
    Unit* target;

    uint32 procFlags;
    uint32 procExtra;

    WeaponAttackType attType;
    uint32 damage; // contains full heal or full damage (完整伤害或治疗量)
    uint32 absorb; // absorbed damage (被吸收伤害)
    SpellEntry const* spellInfo; // filled always even on aura tick (始终填充，包括周期跳数)

    Spell* spell; // only filled on direct spell execution (仅直接法术施放时填充)

    // Healing specific information (治疗专用字段)
    uint32 healthGain;
    bool isHeal;

    Aura* triggeredByAura;                                  // 正在判定触发的宿主光环
    uint32 cooldown;                                        // 触发内置冷却时间

    // Scripting data (脚本覆盖数据)
    uint32 triggeredSpellId; // used to designate if a proc was overriden if > 0 (大于 0 时表示由脚本覆盖的触发法术 ID)
    std::array<int32, MAX_EFFECT_INDEX> basepoints = { 0, 0, 0 };
    bool procOnce;                                          // 同一法术仅触发一次标志
    Unit* triggerTarget;                                    // 脚本指定的触发目标
    ObjectGuid triggerOriginalCaster; // not filled by default (脚本指定的原始施法者 GUID)

    ProcExecutionData(ProcSystemArguments& data, bool isVictim);
};
typedef SpellAuraProcResult(Unit::*pAuraProcHandler)(ProcExecutionData& data);
extern pAuraProcHandler AuraProcHandler[TOTAL_AURAS];

// 当前施法槽位类型
enum CurrentSpellTypes
{
    CURRENT_MELEE_SPELL             = 0,                    // 近战附魔/下一次挥击技能（如英勇打击）
    CURRENT_GENERIC_SPELL           = 1,                    // 常规读条/瞬发法术
    CURRENT_AUTOREPEAT_SPELL        = 2,                    // 自动重复攻击（魔杖/自动射击）
    CURRENT_CHANNELED_SPELL         = 3,                    // 引导类法术
    CURRENT_MAX_SPELL
};

#define CURRENT_FIRST_NON_MELEE_SPELL 1

// 宠物动作栏技能激活状态枚举
enum ActiveStates
{
    ACT_PASSIVE  = 0x01,                                    // 0x01 - passive (被动)
    ACT_DISABLED = 0x81,                                    // 0x80 - castable (可施放但关闭自动施法)
    ACT_ENABLED  = 0xC1,                                    // 0x40 | 0x80 - auto cast + castable (可施放且开启自动施法)
    ACT_COMMAND  = 0x07,                                    // 0x01 | 0x02 | 0x04 (命令按钮)
    ACT_REACTION = 0x06,                                    // 0x02 | 0x04 (姿态反应按钮)
    ACT_DECIDE   = 0x00                                     // custom (由服务端自行决定默认状态)
};

// 宠物控制命令状态
enum CommandStates
{
    COMMAND_STAY    = 0,                                    // 原地停留
    COMMAND_FOLLOW  = 1,                                    // 跟随主人
    COMMAND_ATTACK  = 2,                                    // 攻击指定目标
    COMMAND_DISMISS = 3                                     // 解散
};

#define UNIT_ACTION_BUTTON_ACTION(X) (uint32(X) & 0x00FFFFFF)
#define UNIT_ACTION_BUTTON_TYPE(X)   ((uint32(X) & 0xFF000000) >> 24)
#define MAX_UNIT_ACTION_BUTTON_ACTION_VALUE (0x00FFFFFF+1)
#define MAKE_UNIT_ACTION_BUTTON(A,T) (uint32(A) | (uint32(T) << 24))

// 宠物/被控制单位动作栏槽位条目（高 8 位存类型，低 24 位存法术或动作 ID）
struct UnitActionBarEntry
{
    UnitActionBarEntry() : packedData(uint32(ACT_DISABLED) << 24) {}

    uint32 packedData;

    // helper (解包与封装辅助方法)
    ActiveStates GetType() const { return ActiveStates(UNIT_ACTION_BUTTON_TYPE(packedData)); }
    uint32 GetAction() const { return UNIT_ACTION_BUTTON_ACTION(packedData); }
    bool IsActionBarForSpell() const
    {
        ActiveStates Type = GetType();
        return Type == ACT_DISABLED || Type == ACT_ENABLED || Type == ACT_PASSIVE;
    }

    void SetActionAndType(uint32 action, ActiveStates type)
    {
        packedData = MAKE_UNIT_ACTION_BUTTON(action, type);
    }

    void SetType(ActiveStates type)
    {
        packedData = MAKE_UNIT_ACTION_BUTTON(UNIT_ACTION_BUTTON_ACTION(packedData), type);
    }

    void SetAction(uint32 action)
    {
        packedData = (packedData & 0xFF000000) | UNIT_ACTION_BUTTON_ACTION(action);
    }
};

typedef UnitActionBarEntry CharmSpellEntry;

// 宠物动作栏槽位区间定义
enum ActionBarIndex
{
    ACTION_BAR_INDEX_START = 0,
    ACTION_BAR_INDEX_PET_SPELL_START = 3,
    ACTION_BAR_INDEX_PET_SPELL_END = 7,
    ACTION_BAR_INDEX_END = 10,
};

#define MAX_UNIT_ACTION_BAR_INDEX (ACTION_BAR_INDEX_END-ACTION_BAR_INDEX_START)

// 单位被魅惑/控制及宠物控制状态信息类
struct CharmInfo
{
    public:
        explicit CharmInfo(Unit* unit);
        ~CharmInfo();

        void SetCharmState(std::string const& ainame, bool withNewThreatList = true);
        void SetCharmState(UnitAI* ai, bool withNewThreatList = true);
        void ResetCharmState();
        uint32 GetPetNumber() const { return m_petnumber; }
        void SetPetNumber(uint32 petnumber, bool statwindow);
        uint32 GetPetLastAttackCommandTime() const { return m_petLastAttackCommandTime; }
        void SetPetLastAttackCommandTime(uint32 time) { m_petLastAttackCommandTime = time; }

        void SetCommandState(CommandStates st);
        CommandStates GetCommandState() { return m_CommandState; }
        bool HasCommandState(CommandStates state) { return (m_CommandState == state); }

        void InitPossessCreateSpells();
        void InitCharmCreateSpells();
        void InitPetActionBar();
        void InitEmptyActionBar();
        void ProcessUnattackableTargets(CombatData* combatData);

        // return true if successful (向宠物动作栏添加法术，成功返回 true)
        bool AddSpellToActionBar(uint32 spellId, ActiveStates newstate = ACT_DECIDE, uint8 forceSlot = 255);
        bool RemoveSpellFromActionBar(uint32 spellId);
        void LoadPetActionBar(const std::string& data);
        void BuildActionBar(WorldPacket& data) const;
        void SetSpellAutocast(uint32 spell_id, bool state);
        void SetActionBar(uint8 index, uint32 spellOrAction, ActiveStates type)
        {
            PetActionBar[index].SetActionAndType(spellOrAction, type);
        }
        UnitActionBarEntry const* GetActionBarEntry(uint8 index) const { return &(PetActionBar[index]); }

        void ToggleCreatureAutocast(uint32 spellid, bool apply);

        CharmSpellEntry* GetCharmSpell(uint8 index) { return &(m_charmspells[index]); }

        void SetIsRetreating(bool retreating = false) { m_retreating = retreating; }
        bool GetIsRetreating() const { return m_retreating; }

        void ResetStayPosition();
        void SetStayPosition();
        bool UpdateStayPosition();

        float GetStayPosX() const { return m_stayPosX; }
        float GetStayPosY() const { return m_stayPosY; }
        float GetStayPosZ() const { return m_stayPosZ; }
        float GetStayPosO() const { return m_stayPosO; }

        uint32 GetSpellOpener() const { return m_opener; }
        uint32 GetSpellOpenerMinRange() const { return m_openerMinRange; }
        uint32 GetSpellOpenerMaxRange() const { return m_openerMaxRange; }

        void SetSpellOpener(uint32 spellId = 0, uint32 minRange = 0, uint32 maxRange = 0)
        {
            m_opener = spellId;
            m_openerMinRange = minRange;
            m_openerMaxRange = maxRange;
        }

        UnitAI* GetAI() const { return m_ai; }
        CombatData* GetCombatData() const { return m_combatData; };

        void SetCharmStartPosition(Position const& position) { m_charmStartPosition = position; }
        Position const& GetCharmStartPosition() { return m_charmStartPosition; }

        void SetWalk(bool walk) { m_walk = walk; }
        bool GetWalk() const { return m_walk; }

        Unit* GetUnit() { return m_unit; }

    private:
        Unit*               m_unit;                         // 所属受控单位
        UnitAI*             m_ai;                           // 受控期间使用的 AI
        CombatData*         m_combatData;                   // 受控期间独立的战斗仇恨数据
        UnitActionBarEntry  PetActionBar[MAX_UNIT_ACTION_BAR_INDEX];
        CharmSpellEntry     m_charmspells[CREATURE_MAX_SPELLS];
        CommandStates       m_CommandState;                 // 当前命令状态（停留/跟随/攻击）
        uint32              m_petnumber;                    // 宠物唯一编号
        uint32              m_petLastAttackCommandTime;     // 上次收到攻击指令时间
        uint32              m_opener;                       // 起手技能 ID
        uint32              m_openerMinRange;               // 起手技能最小射程
        uint32              m_openerMaxRange;               // 起手技能最大射程
        uint32              m_unitFieldFlags;               // 被控前保存的原始单位标志
        uint8               m_unitFieldBytes2_1;            // 被控前保存的减益槽上限
        bool                m_retreating;                   // 是否正在撤退回归
        bool                m_stayPosSet;                   // 是否已设定停留坐标
        float               m_stayPosX;
        float               m_stayPosY;
        float               m_stayPosZ;
        float               m_stayPosO;

        Position            m_charmStartPosition;           // 魅惑开始时的位置
        bool                m_walk;                         // 是否强制步行

        bool                m_deleted;                      // 防重入析构标记
};

// used in CallForAllControlledUnits/CheckAllControlledUnits (受控单位遍历掩码)
enum ControlledUnitMask
{
    CONTROLLED_PET       = 0x01,                            // 主宠物
    CONTROLLED_MINIPET   = 0x02,                            // 非战斗小宠物
    CONTROLLED_GUARDIANS = 0x04,                            // including PROTECTOR_PET (守护者，含护卫宠物)
    CONTROLLED_CHARM     = 0x08,                            // 被魅惑单位
    CONTROLLED_TOTEMS    = 0x10,                            // 图腾
};

// for clearing special attacks (触发性反击技能过期计时器，4 秒)
#define REACTIVE_TIMER_START 4000

// 触发性反击技能类型（如复仇、压制、猫鼬撕咬等）
enum ReactiveType
{
    REACTIVE_DEFENSE      = 1,
    REACTIVE_HUNTER_PARRY = 2,
    REACTIVE_CRIT         = 3,
    REACTIVE_HUNTER_CRIT  = 4,
    REACTIVE_OVERPOWER    = 5
};

#define MAX_REACTIVE 6

// delay time next attack to prevent client attack animation problems (下次攻击最小延迟，防止客户端动画异常)
#define ATTACK_DISPLAY_DELAY 200
#define MAX_PLAYER_STEALTH_DETECT_RANGE 45.0f               // max distance for detection targets by player (玩家侦测潜行的最大距离)
#define MAX_CREATURE_ATTACK_RADIUS 45.0f                    // max distance for creature aggro (use with CONFIG_FLOAT_RATE_CREATURE_AGGRO) (生物仇恨索敌最大半径)

// Regeneration defines (生命与能量周期回复计算时间基数)
#define REGEN_TIME_FULL         2000                            // For this time difference is computed regen value (玩家回复基准周期 2 秒)
#define REGEN_TIME_FULL_UNIT    5000                            // For npcs (NPC 回复基准周期 5 秒)

// Power type values defines (各能量类型默认上限常量)
enum PowerDefaults
{
    POWER_RAGE_DEFAULT              = 1000,                 // 怒气默认上限（100.0 * 10）
    POWER_FOCUS_DEFAULT             = 100,                  // 集中值默认上限
    POWER_ENERGY_DEFAULT            = 100,                  // 能量默认上限
    POWER_HAPPINESS_DEFAULT         = 1000000,              // 快乐度默认上限
};

// Selection method used by SelectAttackingTarget (SelectAttackingTarget 使用的目标选择策略)
enum AttackingTarget
{
    ATTACKING_TARGET_RANDOM = 0,                            // Just selects a random target (随机目标)
    ATTACKING_TARGET_TOPAGGRO,                              // Selects targes from top aggro to bottom (按仇恨从高到低选择)
    ATTACKING_TARGET_BOTTOMAGGRO,                           // Selects targets from bottom aggro to top (按仇恨从低到高选择)
    ATTACKING_TARGET_NEAREST_BY,                            // Selects the nearest by target (按距离从近到远选择)
    ATTACKING_TARGET_FARTHEST_AWAY,                         // Selects the farthest away target (按距离从远到近选择)
    ATTACKING_TARGET_ALL_SUITABLE,                          // 返回所有符合条件的目标
};

// 目标筛选过滤条件标志位
enum SelectFlags
{
    SELECT_FLAG_IN_LOS              = 0x0001,               // Default Selection Requirement for Spell-targets (必须在视线内)
    SELECT_FLAG_PLAYER              = 0x0002,               // 仅限玩家目标
    SELECT_FLAG_POWER_MANA          = 0x0004,               // For Energy based spells, like manaburn (能量类型为法力)
    SELECT_FLAG_POWER_RAGE          = 0x0008,               // 能量类型为怒气
    SELECT_FLAG_POWER_ENERGY        = 0x0010,               // 能量类型为能量
    SELECT_FLAG_IN_MELEE_RANGE      = 0x0040,               // 必须在近战攻击距离内
    SELECT_FLAG_NOT_IN_MELEE_RANGE  = 0x0080,               // 必须在近战攻击距离外
    SELECT_FLAG_HAS_AURA            = 0x0100,               // 目标需带有指定光环
    SELECT_FLAG_NOT_AURA            = 0x0200,               // 目标不得带有指定光环
    SELECT_FLAG_RANGE_RANGE         = 0x0400,               // For direct targeted abilities like charge or frostbolt but need custom data (按自定义最小/最大射程筛选)
    SELECT_FLAG_RANGE_AOE_RANGE     = 0x0800,               // For AOE targeted abilities like frost nova but need custom data (按自定义 AOE 范围筛选)
    SELECT_FLAG_POWER_NOT_MANA      = 0x1000,               // Used in some dungeon encounters (非使用法力值的目标)
    SELECT_FLAG_USE_EFFECT_RADIUS   = 0x2000,               // For AOE targeted abilities which have correct data in effect index 0 (使用法术效果 0 半径筛选)
    SELECT_FLAG_SKIP_TANK           = 0x4000,               // Not GetVictim - tank is not always top threat (跳过当前主坦克/受害者)
    SELECT_FLAG_CASTING             = 0x8000,               // Selects only targets that are casting (仅选择正在施法的目标)
    SELECT_FLAG_SKIP_CUSTOM         =0x10000,               // skips custom target (跳过指定 GUID 目标)
    SELECT_FLAG_NOT_IMMUNE          =0x20000,               // 排除对该法术免疫的目标
    SELECT_FLAG_USE_EFFECT_RADIUS_OF_TRIGGERED_SPELL = 0x40000 // For AOE targeted abilities which have correct data in triggered spell effect index 0 (使用触发法术效果 0 半径筛选)
};

// SelectAttackingTarget 附加筛选参数联合体
struct SelectAttackingTargetParams
{
    union
    {
        struct
        {
            uint32 minRange;
            uint32 maxRange;
        } range;
        struct
        {
            uint64 guid;
        } skip;
        struct
        {
            uint32 params[2];
        } raw;
    };
};

struct SpellProcEventEntry;                                 // used only privately

// 近战挥击错误状态枚举
enum SwingErrors
{
    SWING_ERROR_NONE,                                       // 无错误，可攻击
    SWING_ERROR_NOT_IN_RANGE,                               // 超出近战攻击距离
    SWING_ERROR_TARGET_NOT_ALIVE,                           // 目标已死亡
    SWING_ERROR_BAD_FACING,                                 // 未面向目标
    SWING_ERROR_CANT_ATTACK_TARGET,                         // 目标不可攻击
};

// 游戏单位基类（Player 与 Creature 的共同基类，封装属性、战斗、施法、光环、仇恨与移动）
class Unit : public WorldObject
{
    public:
        typedef std::set<Unit*> AttackerSet;
        typedef std::multimap<uint32 /*spellId*/, SpellAuraHolder*> SpellAuraHolderMap;
        typedef std::pair<SpellAuraHolderMap::iterator, SpellAuraHolderMap::iterator> SpellAuraHolderBounds;
        typedef std::pair<SpellAuraHolderMap::const_iterator, SpellAuraHolderMap::const_iterator> SpellAuraHolderConstBounds;
        typedef std::list<SpellAuraHolder*> SpellAuraHolderList;
        typedef std::list<Aura*> AuraList;
        typedef std::list<DiminishingReturn> Diminishing;
        typedef std::set<uint32 /*playerGuidLow*/> ComboPointHolderSet;
        typedef std::map<SpellEntry const*, ObjectGuid /*targetGuid*/> TrackedAuraTargetMap;

        virtual ~Unit();

        void AddToWorld() override;                         // 加入世界并注册相关引用
        void RemoveFromWorld() override;                    // 移出世界并清理状态

        void CleanupsBeforeDelete() override;               // used in ~Creature/~Player (or before mass creature delete to remove cross-references to already deleted units) (析构前清理交叉引用、光环与战斗关系)

        float GetCollisionHeight() const override;
        float GetCollisionWidth() const override;
        float GetObjectBoundingRadius() const override { return m_floatValues[UNIT_FIELD_BOUNDINGRADIUS]; } // overwrite WorldObject version (碰撞包围盒半径)
        float GetCombatReach() const override { return m_floatValues[UNIT_FIELD_COMBATREACH]; } // overwrite WorldObject version (近战攻击触及距离)

        /**
         * Gets the current DiminishingLevels for the given group (获取指定递减组的当前递减等级)
         * @param group The group that you would like to know the current diminishing return level for
         * @return The current diminishing level, up to DIMINISHING_LEVEL_IMMUNE
         */
        DiminishingLevels GetDiminishing(DiminishingGroup  group);
        /**
         * Increases the level of the DiminishingGroup by one level up until
         * DIMINISHING_LEVEL_IMMUNE where the target becomes immune to spells of
         * that DiminishingGroup (提升指定递减组的递减计数等级，直至免疫)
         * @param group The group to increase the level for by one
         */
        void IncrDiminishing(DiminishingGroup group, bool pvp);
        /**
         * Calculates how long the duration of a spell should be considering
         * diminishing returns, ie, if the Level passed in is DIMINISHING_LEVEL_IMMUNE
         * then the duration will be zeroed out. If it is DIMINISHING_LEVEL_1 then a full
         * duration will be used (按递减等级折算控制法术实际持续时间)
         * @param group The group to affect
         * @param duration The duration to be changed, will be updated with the new duration
         * @param caster Who's casting the spell, used to decide whether anything should be calculated
         * @param Level The current level of diminishing returns for the group, decides the new duration
         * @param isReflected Whether the spell was reflected or not, used to determine if we should do any calculations at all.
         */
        void ApplyDiminishingToDuration(DiminishingGroup group, int32& duration, Unit* caster, DiminishingLevels Level, bool isReflected, SpellEntry const* spellInfo, bool hasAuraScript);
        /**
         * Applies a diminishing return to the given group if apply is true,
         * otherwise lowers the level by one (?) (施加或解除光环时更新对应递减组的叠加计数)
         * @param group The group to affect
         * @param apply whether this aura is being added/removed
         */
        void ApplyDiminishingAura(DiminishingGroup  group, bool apply);
        /**
         * Clears all the current diminishing returns for this Unit. (清空当前单位所有递减收益记录)
         */
        void ClearDiminishings() { m_Diminishing.clear(); }

        void Update(const uint32 diff) override;            // 每帧主更新：事件、施法、光环、回复、移动与脱战
        void Heartbeat() override;                          // 心跳周期回调：触发周期性检测

        /**
         * Updates the attack time for the given WeaponAttackType (设置指定武器槽位剩余攻击冷却时间)
         * @param type The type of weapon that we want to update the time for
         * @param time the remaining time until we can attack with the WeaponAttackType again
         */
        void setAttackTimer(WeaponAttackType type, uint32 time) { m_attackTimer[type] = time; }
        /**
         * Resets the attack timer to the base value decided by Unit::m_modAttackSpeedPct and
         * Unit::GetAttackTime (重置指定武器槽位攻击计时器为当前实际攻速)
         * @param type The weapon attack type to reset the attack timer for.
         */
        void resetAttackTimer(WeaponAttackType type = BASE_ATTACK);
        /**
         * Get's the remaining time until we can do an attack (获取距离下次攻击的剩余毫秒数)
         * @param type The weapon type to check the remaining time for
         * @return The remaining time until we can attack with this weapon type.
         */
        uint32 getAttackTimer(WeaponAttackType type) const { return m_attackTimer[type]; }
        /**
         * Checks whether the unit can do an attack. Does this by checking the attacktimer for the
         * WeaponAttackType, can probably be thought of as a cooldown for each swing/shot (判断指定武器槽位是否已完成冷却可攻击)
         * @param type What weapon should we check for
         * @return true if the Unit::m_attackTimer is zero for the given WeaponAttackType
         */
        bool isAttackReady(WeaponAttackType type = BASE_ATTACK) const { return m_attackTimer[type] == 0; }
        /**
         * Checks if the current Unit has a weapon equipped at the moment (判断是否装备了指定槽位武器)
         * @return True if there is a weapon.
         */
        virtual bool hasWeapon(WeaponAttackType type) const = 0;
        inline bool hasMainhandWeapon() const { return hasWeapon(BASE_ATTACK); }
        inline bool hasOffhandWeapon() const { return hasWeapon(OFF_ATTACK); }
        inline bool hasRangedWeapon() const { return hasWeapon(RANGED_ATTACK); }
        /**
         * Checks if the current Unit has a usable weapon at the moment (判断指定槽位武器是否可用于攻击)
         * @return True if there is a weapon.
         */
        virtual bool hasWeaponForAttack(WeaponAttackType type) const { return CanUseEquippedWeapon(type); }
        inline bool hasMainhandWeaponForAttack() const { return hasWeaponForAttack(BASE_ATTACK); }
        inline bool hasOffhandWeaponForAttack() const { return hasWeaponForAttack(OFF_ATTACK); }
        inline bool hasRangedWeaponForAttack() const { return hasWeaponForAttack(RANGED_ATTACK); }
         /**
         * Does an attack if any of the timers allow it and resets them, if the user
         * isn't in range or behind the target an error is sent to the client.
         * Also makes sure to not make and offhand and mainhand attack at the same
         * time. Only handles non-spells ie melee attacks. (驱动近战平砍主循环：检查主副手计时器、距离与朝向并执行攻击)
         * @return True if an attack was made and no error happened, false otherwise
         */
        bool UpdateMeleeAttackingState();
        /**
         * Check is a given equipped weapon can be used, ie the mainhand, offhand etc. (检查已装备武器在当前变形/缴械状态下是否可用)
         * @param attackType The attack type to check, ie: main/offhand/ranged
         * @return True if the weapon can be used, true except for shapeshifts and if disarmed.
         */
        bool CanUseEquippedWeapon(WeaponAttackType attackType) const
        {
            if (IsAttackSpeedOverridenShapeShift())
                return false;

            switch (attackType)
            {
                default:
                case BASE_ATTACK:
                    return !HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISARMED);
                case OFF_ATTACK:
                case RANGED_ATTACK:
                    return true;
            }
        }
        /** Returns if the Unit can reach a victim with Melee Attack (判断是否在近战攻击触及范围内)
         *
         * @param pVictim Who we want to reach with a melee attack
         * @param flat_mod The same as sent to Unit::GetCombatReach
         * @return true if we can reach pVictim with a melee attack
         */
        bool CanReachWithMeleeAttack(Unit const* pVictim, float flat_mod = 0.0f) const;
        uint32 m_extraAttacks;                              // 待触发的额外连续攻击次数（如剑专精/风怒）
        ObjectGuid m_extraAttackGuid;                       // 额外攻击锁定的目标 GUID
        void DoExtraAttacks(Unit* victim);                  // 执行排队中的额外近战攻击

        bool IsAttackedBy(Unit* attacker) const
        {
            if (m_attackers.find(attacker) != m_attackers.end())
                return true;

            return false;
        }

        bool _addAttacker(Unit* pAttacker)                  //< (Internal Use) must be called only from Unit::Attack(Unit*) (内部使用：登记正在攻击自身的单位)
        {
            AttackerSet::const_iterator itr = m_attackers.find(pAttacker);
            if (itr == m_attackers.end())
            {
                m_attackers.insert(pAttacker);
                return true;
            }
            return false;
        }
        void _removeAttacker(Unit* pAttacker)               //< (Internal Use) must be called only from Unit::AttackStop() (内部使用：移除攻击者记录)
        {
            m_attackers.erase(pAttacker);
        }
        Unit* getAttackerForHelper()                        //< Return a possible enemy from this unit to help in combat (获取一个当前敌对目标供友方呼救援助)
        {
            if (GetVictim() != nullptr)
                return GetVictim();

            if (!m_attackers.empty())
                return *(m_attackers.begin());

            return nullptr;
        }
        /**
         * Tries to attack a Unit/Player, also makes sure to stop attacking the current target
         * if we're already attacking someone. (发起对目标的近战或法术攻击，必要时先停止攻击原目标)
         * @param victim The Unit to attack
         * @param meleeAttack Whether we should attack with melee or ranged/magic
         * @return True if an attack was initiated, false otherwise
         */
        bool Attack(Unit* victim, bool meleeAttack);
        /**
         * Called when we are attacked by someone in someway, might be when a fear runs out and
         * we want to notify AI to attack again or when a spell hits. (受到某单位攻击时的通知回调)
         * @param attacker Who's attacking us
         */
        void AttackedBy(Unit* attacker);
        /**
         * Stop all spells from casting except the one give by except_spellid (停止除指定法术外的所有正在施放法术)
         * @param except_spellid This spell id will not be stopped from casting, defaults to 0
         * \see Unit::InterruptSpell
         */
        void CastStop(uint32 except_spellid = 0);
        /**
         * Stops attacking whatever we are attacking at the moment and tells the Unit we are attacking
         * that we are not doing that anymore. (停止攻击当前目标并解除双方攻击引用)
         * @param targetSwitch if we are switching targets or not, defaults to false
         * @return false if we weren't attacking already, true otherwise
         * \see Unit::m_attacking
         */
        bool AttackStop(bool targetSwitch = false, bool includingCast = false, bool includingCombo = false, bool clientInitiated = false);
        /**
         * Removes all attackers from the Unit::m_attackers set and logs it if someone that
         * wasn't attacking it was in the list. Does this check by checking if Unit::AttackStop()
         * returned false. (强制让所有正在攻击自己的单位停止攻击)
         * \see Unit::AttackStop
         */
        void RemoveAllAttackers();

        void MeleeAttackStart(Unit* victim);                // 开始近战自动攻击并广播包
        void MeleeAttackStop(Unit* victim);                 // 停止近战自动攻击并广播包

        /// Returns the Unit::m_attackers, that stores the units that are attacking you (获取正在攻击当前单位的攻击者集合)
        AttackerSet const& getAttackers() const { return m_attackers; }

        Unit* GetVictim() const { return m_attacking; }     //< Returns the victim that this unit is currently attacking (获取当前正在攻击的目标)
        void CombatStop(bool includingCast = false, bool includingCombo = true);        //< Stop this unit from combat, if includingCast==true, also interrupt casting (退出战斗，清除攻击者、仇恨与战斗状态)
        void CombatStopWithPets(bool includingCast = false, bool includingCombo = true);
        void StopAttackFaction(uint32 faction_id);          // 停止攻击指定阵营的所有目标
        Unit* SelectRandomUnfriendlyTarget(Unit* except = nullptr, float radius = ATTACK_DISTANCE) const;
        Unit* SelectRandomFriendlyTarget(Unit* except = nullptr, float radius = ATTACK_DISTANCE) const;
        bool HasDamageInterruptibleStunAura() const;
        bool HasBreakableByDamageCrowdControlAura(Unit* excludeCasterChannel = nullptr, uint32 excludeAuraApplyMSTimeCutoff = 0) const;
        bool HasBreakableByDamageAuraType(AuraType type, uint32 excludeAura, uint32 excludeAuraApplyMSTimeCutoff) const;
        bool HasAuraPetShouldAvoidBreaking(Unit* excludeCasterChannel = nullptr, uint32 excludeAuraApplyMSTimeCutoff = 0) const;
        void SendMeleeAttackStop(Unit const* victim) const;
        void SendMeleeAttackStart(const Unit& victim) const;

        // 内部 UnitState 状态位操作
        void addUnitState(uint32 f) { m_state |= f; }
        bool hasUnitState(uint32 f) const { return (m_state & f) != 0; }
        void clearUnitState(uint32 f) { m_state &= ~f; }
        bool CanFreeMove() const { return !hasUnitState(UNIT_STAT_NO_FREE_MOVE) && !GetOwnerGuid(); }

        virtual uint32 GetLevelForTarget(Unit const* /*target*/) const { return GetLevel(); }
        bool IsTrivialForTarget(Unit const* pov) const;     // 判断对观察者而言是否为灰色无经验/无荣誉目标

        void SetLevel(uint32 lvl);

        uint32 GetLevel() const override { return GetUInt32Value(UNIT_FIELD_LEVEL); }
        uint8 getRace() const { return GetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_RACE); }
        uint32 getRaceMask() const { return convertEnumToFlag(getRace()); }
        uint8 getClass() const { return GetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_CLASS); }
        uint32 getClassMask() const { return convertEnumToFlag(getClass()); }
        uint8 getGender() const { return GetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_GENDER); }

        float GetStat(Stats stat) const { return float(GetUInt32Value(static_cast<uint16>(UNIT_FIELD_STAT0) + static_cast<uint16>(stat))); }
        void SetStat(Stats stat, int32 val) { SetStatInt32Value(static_cast<uint16>(UNIT_FIELD_STAT0) + static_cast<uint16>(stat), val); }

        inline int32 GetArmor() const { return GetResistance(SPELL_SCHOOL_NORMAL) ; }
        inline void SetArmor(int32 val) { SetStatInt32Value(static_cast<uint16>(UNIT_FIELD_RESISTANCES), val); }
        inline int32 GetResistance(SpellSchools school) const { return GetInt32Value(static_cast<uint16>(UNIT_FIELD_RESISTANCES) + static_cast<uint16>(school)); }
        inline void SetResistance(SpellSchools school, int32 val) { SetInt32Value(static_cast<uint16>(UNIT_FIELD_RESISTANCES) + static_cast<uint16>(school), val); }

        uint32 GetHealth()    const { return GetUInt32Value(UNIT_FIELD_HEALTH); }
        uint32 GetMaxHealth() const { return GetUInt32Value(UNIT_FIELD_MAXHEALTH); }
        float GetHealthPercent() const { return (GetHealth() * 100.0f) / GetMaxHealth(); }
        void SetHealth(uint32 val);
        void SetMaxHealth(uint32 val);
        void SetHealthPercent(float percent);
        int32 ModifyHealth(int32 dVal);
        float OCTRegenHPPerSpirit() const;                  // 脱战后精神转换每 2 秒生命回复系数
        float OCTRegenMPPerSpirit() const;                  // 精神转换基础法力回复系数

        Powers GetPowerType() const { return Powers(GetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_POWER_TYPE)); }
        void SetPowerType(Powers new_powertype);
        uint32 GetPower(Powers power) const { return GetUInt32Value(static_cast<uint16>(UNIT_FIELD_POWER1) + static_cast<uint16>(power)); }
        uint32 GetMaxPower(Powers power) const { return GetUInt32Value(static_cast<uint16>(UNIT_FIELD_MAXPOWER1) + static_cast<uint16>(power)); }
        float GetPowerPercent() const { return (GetMaxPower(GetPowerType()) == 0) ? 0.0f : (GetPower(GetPowerType()) * 100.0f) / GetMaxPower(GetPowerType()); }
        float GetPowerPercent(Powers power) const { return (GetMaxPower(power) == 0) ? 0.0f : (GetPower(power) * 100.0f) / GetMaxPower(power); }
        void SetPower(Powers power, uint32 val);
        void SetMaxPower(Powers power, uint32 val);
        int32 ModifyPower(Powers power, int32 dVal);
        [[deprecated("Use ModifyPower()")]]
        void ApplyPowerMod(Powers power, uint32 val, bool apply);
        void ApplyMaxPowerMod(Powers power, uint32 val, bool apply);
        bool HasMana() { return GetPowerType() == POWER_MANA; }

        uint32 GetAttackTime(WeaponAttackType att) const { return (uint32)(GetFloatValue(static_cast<uint16>(UNIT_FIELD_BASEATTACKTIME) + static_cast<uint16>(att)) / m_modAttackSpeedPct[att]); }
        void SetAttackTime(WeaponAttackType att, uint32 val) { SetFloatValue(static_cast<uint16>(UNIT_FIELD_BASEATTACKTIME) + static_cast<uint16>(att), val * m_modAttackSpeedPct[att]); }
        void ApplyAttackTimePercentMod(WeaponAttackType att, float val, bool apply);
        void ApplyCastTimePercentMod(float val, bool apply);

        SheathState GetSheath() const { return SheathState(GetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_SHEATH_STATE)); }
        virtual void SetSheath(SheathState sheathed) { SetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_SHEATH_STATE, sheathed); }

        // faction template id (阵营模板 ID 与关系判定)
        uint32 GetFaction() const override { return GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE); }
        void setFaction(uint32 faction) { SetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE, faction); }
        FactionTemplateEntry const* GetFactionTemplateEntry() const;
        void RestoreOriginalFaction();
        bool IsNeutralToAll() const;
        bool IsContestedGuard() const
        {
            if (FactionTemplateEntry const* entry = GetFactionTemplateEntry())
                return entry->IsContestedGuardFaction();

            return false;
        }

        Player const* GetControllingPlayer(bool ignoreCharms = false) const;

        ReputationRank GetReactionTo(Unit const* unit) const override;
        ReputationRank GetReactionTo(Corpse const* corpse) const override;

        bool IsEnemy(Unit const* unit) const override;
        bool IsFriend(Unit const* unit) const override;

        bool CanAssist(Unit const* unit, bool ignoreFlags = false) const;
        bool CanAssist(Corpse const* corpse) const;

        bool CanAttack(Unit const* unit) const;
        bool CanAttackNow(Unit const* unit) const;

        bool CanCooperate(Unit const* unit) const;

        bool CanInteract(GameObject const* object) const;
        bool CanInteract(Unit const* unit) const;
        bool CanInteractNow(Unit const* unit) const;

        bool IsCivilianForTarget(Unit const* pov) const;

        // Serverside fog of war settings (服务端战争迷雾可见性检查)
        bool IsFogOfWarVisibleStealth(Unit const* other) const;
        bool IsFogOfWarVisibleHealth(Unit const* other) const;
        bool IsFogOfWarVisibleStats(Unit const* other) const;

        virtual bool IsInGroup(Unit const* other, bool party = false, bool ignoreCharms = false) const;
        inline bool IsInParty(Unit const* other, bool ignoreCharms = false) const { return IsInGroup(other, true, ignoreCharms); }
        bool IsInGuild(Unit const* other, bool ignoreCharms = false) const;
        bool IsInTeam(Unit const* other, bool ignoreCharms = true) const;

        // extensions of CanAttack and CanAssist API needed serverside (服务端法术攻击/协助校验接口)
        virtual bool CanAttackSpell(Unit const* target, SpellEntry const* spellInfo = nullptr, bool isAOE = false) const override;
        virtual bool CanAssistSpell(Unit const* target, SpellEntry const* spellInfo = nullptr) const override;

        bool CanAttackOnSight(Unit const* target) const; // Used in MoveInLineOfSight checks (视野索敌检查：能否在视线内主动攻击目标)
        bool CanAttackInCombat(Unit const* target, bool ignoreFlagsSource, bool ignoreFlagsTarget) const;
        bool CanAttackServerside(Unit const* unit, bool ignoreFlagsSource, bool ignoreFlagsTarget) const;
        bool CanAssistInCombatAgainst(Unit const* who, Unit const* enemy) const;
        bool CanJoinInAttacking(Unit const* enemy) const;

        bool IsImmuneToNPC() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC); }
        void SetImmuneToNPC(bool state);
        bool IsImmuneToPlayer() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PLAYER); }
        void SetImmuneToPlayer(bool state);
        bool IsPlayerControlled() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED); }
        uint32 GetCreatedBySpellId() const { return GetUInt32Value(UNIT_CREATED_BY_SPELL); }

        // PvP 状态标记查询与设置
        bool IsPvP() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP); }
        void SetPvP(bool state);
        bool IsPvPFreeForAll() const;
        void SetPvPFreeForAll(bool state);
        bool IsPvPContested() const;
        void SetPvPContested(bool state);
        bool IsPvPSanctuary() const;
        void SetPvPSanctuary(bool state);
        uint32 GetCreatureType() const;
        uint32 GetCreatureTypeMask() const
        {
            uint32 creatureType = GetCreatureType();
            return (creatureType >= 1) ? convertEnumToFlag(creatureType) : 0;
        }

        // 站立/坐姿及视觉标志操作
        uint8 getStandState() const { return GetByteValue(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_STAND_STATE); }
        bool IsSitState() const;
        bool IsStandState() const;
        bool IsSeatedState() const;
        void SetStandState(uint8 state, bool acknowledge = false);

        void SetVisFlags(uint8 flags) { SetByteFlag(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_VIS_FLAGS, flags); }
        void RemoveVisFlags(uint8 flags) { RemoveByteFlag(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_VIS_FLAGS, flags); }
        bool HasVisFlags(uint8 flags) { return GetByteValue(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_VIS_FLAGS) & flags; }

        // 坐骑状态操作
        bool IsMounted() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_MOUNT); } // not used with creature non-aura mounts
        uint32 GetMountID() const { return GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID); }
        bool MountEntry(uint32 templateEntry, const Aura* aura = nullptr);
        bool UnmountEntry(const Aura* aura = nullptr);
        virtual bool Mount(uint32 displayid, const Aura* aura = nullptr);
        virtual bool Unmount(const Aura* aura = nullptr);

        uint16 GetSkillMaxForLevel(Unit const* target = nullptr) const { return (target ? GetLevelForTarget(target) : GetLevel()) * 5; }

        // 伤害结算、治疗、击杀与触发系统入口
        void Suicide();
        void FallSuicide();
        static void DealDamageMods(Unit* dealer, Unit* victim, uint32& damage, uint32* absorb, DamageEffectType damagetype, SpellEntry const* spellProto = nullptr);
        static uint32 DealDamage(Unit* dealer, Unit* victim, uint32 damage, CleanDamage const* cleanDamage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask, SpellEntry const* spellProto, bool durabilityLoss, Spell* spell = nullptr);
        int32 DealHeal(Unit* pVictim, uint32 addhealth, SpellEntry const* spellProto, bool critical = false);
        static void Kill(Unit* killer, Unit* victim, DamageEffectType damagetype, SpellEntry const* spellProto, bool durabilityLoss, bool duel_hasEnded);
        static void HandleDamageDealt(Unit* dealer, Unit* victim, uint32& damage, CleanDamage const* cleanDamage, DamageEffectType damagetype, SpellSchoolMask damageSchoolMask, SpellEntry const* spellProto, bool duel_hasEnded);
        void InterruptOrDelaySpell(Unit* pVictim, DamageEffectType damagetype, SpellEntry const* spellProto);

        void PetOwnerKilledUnit(Unit* pVictim);

        static void ProcDamageAndSpell(ProcSystemArguments&& data);
        void ProcDamageAndSpellFor(ProcSystemArguments& data, bool isVictim);
        void ProcSkillsAndReactives(bool isVictim, Unit* target, uint32 procFlags, uint32 procEx, WeaponAttackType attType);

        void HandleEmote(uint32 emote_id);                  // auto-select command/state (自动区分一次性动作或持续表情状态)
        void HandleEmoteCommand(uint32 emote_id);
        void HandleEmoteState(uint32 emote_id);
        void AttackerStateUpdate(Unit* pVictim, WeaponAttackType attType = BASE_ATTACK, bool extra = false);

        void CalculateMeleeDamage(Unit* pVictim, CalcDamageInfo* calcDamageInfo, WeaponAttackType attackType = BASE_ATTACK);
        void DealMeleeDamage(CalcDamageInfo* calcDamageInfo, bool durabilityLoss);

        static bool IsAllowedDamageInArea(Unit* attacker, Unit* victim);

        void CalculateSpellDamage(SpellNonMeleeDamage* spellDamageInfo, int32 damage, SpellEntry const* spellInfo, SpellEffectIndex effectIndex, WeaponAttackType attackType = BASE_ATTACK);
        static void DealSpellDamage(Unit* affectiveCaster, SpellNonMeleeDamage* spellDamageInfo, bool durabilityLoss, bool resetLeash);

        uint32 GetResilienceRatingDamageReduction(uint32 damage, SpellDmgClass dmgClass, bool periodic = false, Powers pwrType = POWER_HEALTH) const;

        SpellMissInfo MeleeSpellHitResult(Unit* pVictim, SpellEntry const* spell, uint32* heartbeatResistChance = nullptr);
        SpellMissInfo MagicSpellHitResult(Unit* pVictim, SpellEntry const* spell, SpellSchoolMask schoolMask, uint32* heartbeatResistChance = nullptr);
        static SpellMissInfo SpellHitResult(WorldObject* caster, Unit* victim, SpellEntry const* spell, uint8 effectMask, bool reflectable = false, bool reflected = false, uint32* heartbeatResistChance = nullptr);

        bool CanDualWield() const { return m_canDualWield; }
        virtual void SetCanDualWield(bool value) { m_canDualWield = value; }

        // Unit Combat reactions API: Dodge/Parry/Block (防御反应能力接口：躲闪/招架/格挡)
        bool CanDodge() const { return m_canDodge; }
        bool CanParry() const { return m_canParry; }
        bool CanBlock() const { return m_canBlock; }
        // Unit Melee events API: Crush/Glance/Daze (近战攻击特殊事件接口：碾压/偏斜/眩晕)
        bool CanCrush() const;
        bool CanGlance() const;
        virtual bool CanDaze() const { return false; };

        void SetCanDodge(const bool flag);
        void SetCanParry(const bool flag);
        void SetCanBlock(const bool flag);

        bool CanReactInCombat() const { return (IsAlive() && !IsCrowdControlled() && !GetCombatManager().IsEvadingHome()); }
        bool CanCastSpellInCombat() const { return (IsAlive() && !GetCombatManager().IsEvadingHome()); } // spells are stopped by spell system (战斗中是否允许施法，具体打断由法术系统控制)
        bool CanDodgeInCombat() const;
        bool CanDodgeInCombat(const Unit* attacker) const;
        bool CanParryInCombat() const;
        bool CanParryInCombat(const Unit* attacker) const;
        bool CanBlockInCombat(SpellSchoolMask weaponSchoolMask = SPELL_SCHOOL_MASK_NORMAL) const;
        bool CanBlockInCombat(const Unit* attacker, SpellSchoolMask weaponSchoolMask = SPELL_SCHOOL_MASK_NORMAL) const;
        bool CanCrushInCombat() const;
        bool CanCrushInCombat(const Unit* victim) const;
        bool CanGlanceInCombat() const { return CanGlance(); }
        bool CanGlanceInCombat(const Unit* victim) const;
        bool CanDazeInCombat() const { return CanDaze(); }
        bool CanDazeInCombat(const Unit* victim) const;

        // Unit Combat Reactions API for abilities (melee and ranged spells): Dodge/Parry/Block/Deflect (技能攻击防御反应判定)
        bool CanReactOnAbility(const SpellEntry* ability) const;
        bool CanDodgeAbility(const Unit* attacker, const SpellEntry* ability) const;
        bool CanParryAbility(const Unit* attacker, const SpellEntry* ability) const;
        bool CanBlockAbility(const Unit* attacker, const SpellEntry* ability, bool miss = false) const;
        bool CanDeflectAbility(const Unit* attacker, const SpellEntry* ability) const;

        float CalculateEffectiveDodgeChance(const Unit* attacker, WeaponAttackType attType, const SpellEntry* ability = nullptr) const;
        float CalculateEffectiveParryChance(const Unit* attacker, WeaponAttackType attType, const SpellEntry* ability = nullptr) const;
        float CalculateEffectiveBlockChance(const Unit* attacker, WeaponAttackType attType, const SpellEntry* ability = nullptr) const;
        float CalculateEffectiveCrushChance(const Unit* victim, WeaponAttackType attType) const;
        float CalculateEffectiveGlanceChance(const Unit* victim, WeaponAttackType attType) const;
        float CalculateEffectiveDazeChance(const Unit* victim, WeaponAttackType attType) const;
        uint32 CalculateGlanceAmount(CalcDamageInfo* meleeInfo) const;

        float CalculateAbilityDeflectChance(const Unit* attacker, const SpellEntry* ability) const;

        bool RollAbilityPartialBlockOutcome(const Unit* attacker, WeaponAttackType attType, const SpellEntry* ability) const;

        float GetDodgeChance() const;
        float GetParryChance() const;
        float GetBlockChance() const;

        float GetReflectChance(SpellSchoolMask schoolMask) const;

        virtual bool CanCrit(WeaponAttackType attType) const { return (GetCritChance(attType) >= 0.005f); }
        virtual bool CanCrit(SpellSchoolMask schoolMask) const { return (GetCritChance(schoolMask) >= 0.005f); }
        virtual bool CanCrit(const SpellEntry* entry, SpellSchoolMask schoolMask, WeaponAttackType attType) const;

        virtual float GetCritChance(WeaponAttackType attType) const;
        virtual float GetCritChance(SpellSchoolMask schoolMask) const;
        virtual float GetCritChance(const SpellEntry* entry, SpellSchoolMask schoolMask) const;

        virtual float GetCritTakenChance(SpellSchoolMask dmgSchoolMask, SpellDmgClass dmgClass, bool heal = false) const;

        virtual float GetCritMultiplier(SpellSchoolMask dmgSchoolMask, uint32 creatureTypeMask, const SpellEntry* spell = nullptr, bool heal = false) const;
        virtual float GetCritTakenMultiplier(SpellSchoolMask dmgSchoolMask, SpellDmgClass dmgClass, float ignorePct = 0.0f, bool heal = false) const;
        uint32 CalculateCritAmount(const Unit* victim, uint32 amount, const SpellEntry* entry, bool heal = false) const;
        uint32 CalculateCritAmount(CalcDamageInfo* meleeInfo) const;

        virtual float GetHitChance(WeaponAttackType attackType) const;
        virtual float GetHitChance(SpellSchoolMask schoolMask) const;
        virtual float GetHitChance(const SpellEntry* entry, SpellSchoolMask schoolMask) const;

        virtual float GetMissChance(WeaponAttackType attackType) const;
        virtual float GetMissChance(SpellSchoolMask schoolMask) const;
        virtual float GetMissChance(const SpellEntry* entry, SpellSchoolMask schoolMask) const;

        float CalculateEffectiveCritChance(const Unit* victim, WeaponAttackType attType, const SpellEntry* ability = nullptr) const;
        float CalculateEffectiveMissChance(const Unit* victim, WeaponAttackType attType, const SpellEntry* ability = nullptr) const;

        float CalculateSpellCritChance(const Unit* victim, SpellSchoolMask schoolMask, const SpellEntry* spell) const;
        float CalculateSpellMissChance(const Unit* victim, SpellSchoolMask schoolMask, const SpellEntry* spell) const;

        /*Hack to support always hitting creatures. TODO: investigate Serpentshrine Parasite*/
        void SetAlwaysHit(bool value) { m_alwaysHit = value; }

        static bool RollSpellCritOutcome(Unit* caster, const Unit* victim, SpellSchoolMask schoolMask, const SpellEntry* spell);

        float GetExpertisePercent(WeaponAttackType attType) const;

        virtual int32 GetResistancePenetration(SpellSchools school) const;

        float CalculateEffectiveMagicResistancePercent(const Unit* attacker, SpellSchoolMask schoolMask, bool binary = false) const;
        static float RollMagicResistanceMultiplierOutcomeAgainst(const Unit* attacker, const Unit* victim, SpellSchoolMask schoolMask, DamageEffectType dmgType, bool binary = false);

        float CalculateSpellResistChance(const Unit* victim, SpellSchoolMask schoolMask, const SpellEntry* spell) const;

        virtual uint32 GetShieldBlockValue() const = 0;
        uint32 GetUnitMeleeSkill(Unit const* target = nullptr) const { return (target ? GetLevelForTarget(target) : GetLevel()) * 5; }
        uint32 GetDefenseSkillValue(Unit const* target = nullptr) const;
        uint32 GetWeaponSkillValue(WeaponAttackType attType, Unit const* target = nullptr) const;
        float GetWeaponProcChance() const;
        float GetPPMProcChance(uint32 WeaponSpeed, float PPM) const;

        MeleeHitOutcome RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType, SpellSchoolMask schoolMask) const;

        // NPC 服务类型快捷判断接口
        bool isVendor()       const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR); }
        bool isTrainer()      const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER); }
        bool isQuestGiver()   const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER); }
        bool isGossip()       const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP); }
        bool isTaxi()         const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_FLIGHTMASTER); }
        bool isGuildMaster()  const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PETITIONER); }
        bool isBattleMaster() const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BATTLEMASTER); }
        bool isBanker()       const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER); }
        bool isInnkeeper()    const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_INNKEEPER); }
        bool isSpiritHealer() const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER); }
        bool isSpiritGuide()  const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITGUIDE); }
        bool isTabardDesigner()const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TABARDDESIGNER); }
        bool isAuctioner()    const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_AUCTIONEER); }
        bool isArmorer()      const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_REPAIR); }
        bool isServiceProvider() const
        {
            return HasFlag(UNIT_NPC_FLAGS,
                           UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_FLIGHTMASTER |
                           UNIT_NPC_FLAG_PETITIONER | UNIT_NPC_FLAG_BATTLEMASTER | UNIT_NPC_FLAG_BANKER |
                           UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_SPIRITHEALER |
                           UNIT_NPC_FLAG_SPIRITGUIDE | UNIT_NPC_FLAG_TABARDDESIGNER | UNIT_NPC_FLAG_AUCTIONEER);
        }
        bool isSpiritService() const { return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER | UNIT_NPC_FLAG_SPIRITGUIDE); }

        bool IsTaxiFlying()  const { return hasUnitState(UNIT_STAT_TAXI_FLIGHT); }

        bool IsInCombat() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT); }
        // unidirectional API - only use when absolutely necessary (单向进战状态接口，仅在必要时使用)
        void SetInCombatState(bool PvP, Unit* enemy = nullptr);
        void SetInCombatWith(Unit* enemy);
        void SetInCombatWithAggressor(Unit* aggressor, bool touchOnly = false);
        inline void SetOutOfCombatWithAggressor(Unit* aggressor) { SetInCombatWithAggressor(aggressor, true); }
        void SetInCombatWithAssisted(Unit* assisted, bool touchOnly = false);
        inline void SetOutOfCombatWithAssisted(Unit* assisted) { SetInCombatWithAssisted(assisted, true); }
        void SetInCombatWithVictim(Unit* victim, bool touchOnly = false);
        inline void SetOutOfCombatWithVictim(Unit* victim) { SetInCombatWithVictim(victim, true); }
        // bidirectional api - we need info about who is the attacker - handles leashing (双向入战接口：同时建立双方仇恨并刷新追击脱战计时)
        void EngageInCombatWith(Unit* enemy);
        void EngageInCombatWithAggressor(Unit* aggressor);
        void ClearInCombat();
        void HandleExitCombat(bool customLeash, bool pvpCombat = false);
        virtual uint32 GetPursuit() const { return 15000; }

        // 光环查询与移除接口
        SpellAuraHolderBounds GetSpellAuraHolderBounds(uint32 spell_id)
        {
            return m_spellAuraHolders.equal_range(spell_id);
        }
        SpellAuraHolderConstBounds GetSpellAuraHolderBounds(uint32 spell_id) const
        {
            return m_spellAuraHolders.equal_range(spell_id);
        }

        uint32 GetAuraCount(uint32 spellId) const;

        bool HasAuraType(AuraType auraType) const;
        bool HasAffectedAura(AuraType auraType, SpellEntry const* spellProto) const;
        bool HasAura(uint32 spellId, SpellEffectIndex effIndex) const;
        bool HasAura(uint32 spellId) const
        {
            return m_spellAuraHolders.find(spellId) != m_spellAuraHolders.end();
        }
        bool HasAuraTypeWithCaster(AuraType auratype, ObjectGuid caster) const;
        bool HasPeriodicAura() const;
        bool HasMechanicMaskOrDispelMaskAura(uint32 dispelMask, uint32 mechanicMask, Unit const* caster) const;
        bool HasNegativeAuraWithInterruptFlag(SpellAuraInterruptFlags flag) const;
        template<typename Func>
        bool HasAuraHolder(Func func) const
        {
            auto const& Auras = GetSpellAuraHolderMap();
            for (auto itr = Auras.begin(); itr != Auras.end(); ++itr)
            {
                SpellAuraHolder* holder = itr->second;
                if (func(holder))
                    return true;
            }
            return false;
        }

        template <typename Func>
        bool HasAuraHolder(uint32 spellId, Func func) const
        {
            SpellAuraHolderConstBounds spair = GetSpellAuraHolderBounds(spellId);
            for (SpellAuraHolderMap::const_iterator i_holder = spair.first; i_holder != spair.second; ++i_holder)
                if (func(i_holder->second))
                    return true;

            return false;
        }

        template<typename Func>
        bool HasAura(Func func, AuraType type) const
        {
            auto const& Auras = GetAurasByType(type);
            for (auto itr = Auras.begin(); itr != Auras.end(); ++itr)
            {
                if (func(*itr))
                    return true;
            }
            return false;
        }

        virtual bool HasSpell(uint32 /*spellID*/) const { return false; }

        bool HasStealthAura()      const { return HasAuraType(SPELL_AURA_MOD_STEALTH); }
        bool HasInvisibilityAura() const { return HasAuraType(SPELL_AURA_MOD_INVISIBILITY); }
        bool isFeared()  const { return HasAuraType(SPELL_AURA_MOD_FEAR); }
        bool isInRoots() const { return HasAuraType(SPELL_AURA_MOD_ROOT); }
        bool IsPolymorphed() const;

        bool isFrozen() const;

        virtual bool IsInWater() const;
        bool IsInSwimmableWater() const;
        virtual bool IsUnderwater() const;
        bool IsAboveGround(float diff = 0.5f) const;
        bool isInAccessablePlaceFor(Unit const* unit) const;

        // 法术施放与能量回复接口
        void EnergizeBySpell(Unit* victim, SpellEntry const* spellInfo, uint32 damage, Powers powerType, bool sendLog = true);
        uint32 SpellNonMeleeDamageLog(Unit* pVictim, uint32 spellID, uint32 damage);
        SpellCastResult CastSpell(Unit* Victim, uint32 spellId, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        SpellCastResult CastSpell(Unit* Victim, SpellEntry const* spellInfo, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        SpellCastResult CastCustomSpell(Unit* Victim, uint32 spellId, int32 const* bp0, int32 const* bp1, int32 const* bp2, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        SpellCastResult CastCustomSpell(Unit* Victim, SpellEntry const* spellInfo, int32 const* bp0, int32 const* bp1, int32 const* bp2, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        SpellCastResult CastSpell(float x, float y, float z, uint32 spellId, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        SpellCastResult CastSpell(float x, float y, float z, SpellEntry const* spellInfo, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        SpellCastResult CastSpell(SpellCastTargets& targets, SpellEntry const* spellInfo, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        SpellCastResult CastCustomSpell(SpellCastTargets& targets, SpellEntry const* spellInfo, int32 const* bp0, int32 const* bp1, int32 const* bp2, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        SpellCastResult CastSpell(SpellCastArgs& args, SpellEntry const* spellInfo, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);
        SpellCastResult CastSpell(SpellCastArgs& args, uint32 spellId, uint32 triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);

        // Single flag overload uint32 (单触发标志枚举重载，防参数隐式转换错误)
        SpellCastResult CastSpell(Unit* Victim, uint32 spellId, TriggerCastFlags triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr)
        { return CastSpell(Victim, spellId, uint32(triggeredFlags), castItem, triggeredByAura, originalCaster, triggeredBy); }
        SpellCastResult CastCustomSpell(Unit* Victim, uint32 spellId, int32 const* bp0, int32 const* bp1, int32 const* bp2, TriggerCastFlags triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr)
        {
            return CastCustomSpell(Victim, spellId, bp0, bp1, bp2, uint32(triggeredFlags), castItem, triggeredByAura, originalCaster, triggeredBy);
        }
        SpellCastResult CastSpell(float x, float y, float z, uint32 spellId, TriggerCastFlags triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr)
        {
            return CastSpell(x, y, z, spellId, uint32(triggeredFlags), castItem, triggeredByAura, originalCaster, triggeredBy);
        }
        SpellCastResult CastSpell(SpellCastArgs& args, uint32 spellId, TriggerCastFlags triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);

        // Single flag overload SpellEntry (使用 SpellEntry 的单触发标志重载)
        SpellCastResult CastSpell(Unit* Victim, SpellEntry const* spellInfo, TriggerCastFlags triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr)
        { return CastSpell(Victim, spellInfo, uint32(triggeredFlags), castItem, triggeredByAura, originalCaster, triggeredBy); }
        SpellCastResult CastCustomSpell(Unit* Victim, SpellEntry const* spellInfo, int32 const* bp0, int32 const* bp1, int32 const* bp2, TriggerCastFlags triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr)
        {
            return CastCustomSpell(Victim, spellInfo, bp0, bp1, bp2, uint32(triggeredFlags), castItem, triggeredByAura, originalCaster, triggeredBy);
        }
        SpellCastResult CastSpell(float x, float y, float z, SpellEntry const* spellInfo, TriggerCastFlags triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr)
        {
            return CastSpell(x, y, z, spellInfo, uint32(triggeredFlags), castItem, triggeredByAura, originalCaster, triggeredBy);
        }
        SpellCastResult CastSpell(SpellCastArgs& args, SpellEntry const* spellInfo, TriggerCastFlags triggeredFlags, Item* castItem = nullptr, Aura* triggeredByAura = nullptr, ObjectGuid originalCaster = ObjectGuid(), SpellEntry const* triggeredBy = nullptr);

        void DeMorph();                                     // 恢复原始模型外观（移除变形）

        void SendAIReaction(AiReaction reactionType);       // 发送 AI 反应动作提示包

        // 战斗日志与法术效果同步封包发送接口
        void SendAttackStateUpdate(CalcDamageInfo const& calcDamageInfo) const;
        void SendAttackStateUpdate(uint32 HitInfo, Unit* target, SpellSchoolMask damageSchoolMask, uint32 Damage, uint32 AbsorbDamage, int32 Resist, VictimState TargetState, uint32 BlockedAmount);
        void SendEnergizeSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype) const;
        void SendEnvironmentalDamageLog(uint8 type, uint32 damage, uint32 absorb, int32 resist) const;
        void SendHealSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, bool critical = false);
        static void SendSpellNonMeleeDamageLog(SpellNonMeleeDamage* log);
        static void SendSpellNonMeleeDamageLog(WorldObject* attacker, Unit* target, uint32 spellID, uint32 damage, SpellSchoolMask damageSchoolMask, uint32 absorbedDamage, int32 resist, bool isPeriodic, uint32 blocked, bool criticalHit = false, bool split = false);
        void SendPeriodicAuraLog(SpellPeriodicAuraLogInfo* pInfo) const;
        static void SendSpellMiss(WorldObject* caster, Unit* target, uint32 spellID, SpellMissInfo missInfo);
        void SendSpellDamageResist(Unit* target, uint32 spellId) const;
        static void SendSpellOrDamageImmune(ObjectGuid casterGuid, Unit* target, uint32 spellId);

        void SendEnchantmentLog(ObjectGuid targetGuid, uint32 itemEntry, uint32 enchantId) const;

        static void CasterHitTargetWithSpell(Unit* realCaster, Unit* target, SpellEntry const* spellInfo, bool triggered, bool success = true);
        virtual bool CanInitiateAttack() const;

        // 仇恨目标与条件目标筛选接口
        Unit* SelectAttackingTarget(AttackingTarget target, uint32 position, uint32 spellId, uint32 selectFlags = 0, SelectAttackingTargetParams params = SelectAttackingTargetParams(), int32 unitConditionId = 0) const;
        Unit* SelectAttackingTarget(AttackingTarget target, uint32 position, SpellEntry const* spellInfo = nullptr, uint32 selectFlags = 0, SelectAttackingTargetParams params = SelectAttackingTargetParams(), int32 unitConditionId = 0) const;
        void SelectAttackingTargets(std::vector<Unit*>& selectedTargets, AttackingTarget target, uint32 position, uint32 spellId, uint32 selectFlags = 0, SelectAttackingTargetParams params = SelectAttackingTargetParams(), int32 unitConditionId = 0) const;
        void SelectAttackingTargets(std::vector<Unit*>& selectedTargets, AttackingTarget target, uint32 position, SpellEntry const* spellInfo = nullptr, uint32 selectFlags = 0, SelectAttackingTargetParams params = SelectAttackingTargetParams(), int32 unitConditionId = 0) const;

        // grid searcher include hiding (判定是否满足 UnitCondition 表条件)
        bool IsUnitConditionSatisfied(int32 unitConditionId, Unit const* otherUnit) const;

        void NearTeleportTo(float x, float y, float z, float orientation, bool casting = false, bool transportLeave = false);
        // do not use - kept only for cinematics (仅保留给过场动画使用的直线移动，常规请使用 MotionMaster)
        void MonsterMoveWithSpeed(float x, float y, float z, float speed, bool generatePath = false, bool forceDestination = false);

        // recommend use MonsterMove/MonsterMoveWithSpeed for most case that correctly work with movegens
        // if used additional args in ... part then floats must explicitly casted to double
        void SendTeleportPacket(float x, float y, float z, float ori, GenericTransport* transport);
        void SendHeartBeat();

        void SendMoveRoot(bool state, bool broadcastOnly = false);

        // 移动状态掩码快速查询
        bool IsMoving() const { return m_movementInfo.HasMovementFlag(movementFlagsMask); }
        bool IsMovingIgnoreFlying() const { return m_movementInfo.HasMovementFlag(movementFlagsIgnoreFlyingMask); }
        bool IsMovingForward() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_MASK_MOVING_FORWARD); }
        bool IsLevitating() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_LEVITATING); }
        bool IsHovering() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_HOVER); }
        bool IsWalking() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE); }
        bool IsRooted() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_ROOT); }
        bool IsFalling() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_FALLING); }

        // 寻路网格 (MMAP) 强制/默认/忽略状态
        enum class MmapForcingStatus
        {
            FORCED,
            DEFAULT,
            IGNORED
        };

        virtual MmapForcingStatus IsIgnoringMMAP() const;

        bool IsDebuggingMovement() const { return m_debuggingMovement; }
        void SetDebuggingMovement(bool state) { m_debuggingMovement = state; }

        void SetLevitate(bool enabled);
        void SetSwim(bool enabled);
        void SetCanFly(bool enabled);
        void SetFeatherFall(bool enabled);
        void SetHover(bool enabled);
        void SetWaterWalk(bool enabled);

        void SetInFront(Unit const* target);
        void SetFacingTo(float ori);
        void SetFacingToObject(WorldObject* pObject);

        bool IsAlive() const { return (m_deathState == ALIVE); }
        bool IsDead() const { return (m_deathState == DEAD || m_deathState == CORPSE); }
        DeathState GetDeathState() const { return m_deathState; }
        virtual void SetDeathState(DeathState s);           // overwritten in Creature/Player/Pet (切换生死状态，子类可覆写)

        bool IsTargetUnderControl(Unit const& target) const;

        // Convenience checkers/getters/setters counterparts for some of the protected Unit guid fields
        // See the comments next to protected methods for meanings (受保护 GUID 字段的便捷查询与读写封装)
        bool HasCharm(ObjectGuid const& exactGuid = ObjectGuid()) const { return (exactGuid.IsEmpty() ? GetCharmGuid().IsUnit() : (GetCharmGuid() == exactGuid)); }
        bool HasCharmer(ObjectGuid const& exactGuid) const { return exactGuid.IsUnit() && GetCharmerGuid().IsUnit() && GetCharmerGuid() == exactGuid; }
        bool HasCharmer() const { return GetCharmerGuid().IsUnit(); }
        bool HasMaster() const;
        bool HasTarget(ObjectGuid const& exactGuid = ObjectGuid()) const { return (exactGuid.IsEmpty() ? GetTargetGuid().IsUnit() : (GetTargetGuid() == exactGuid)); }
        bool HasChannelObject(ObjectGuid const& exactGuid = ObjectGuid()) const { return (exactGuid.IsEmpty() ? !(GetChannelObjectGuid().IsEmpty()) : (GetChannelObjectGuid() == exactGuid)); }

        Unit* GetCharm(WorldObject const* pov = nullptr) const;
        Unit* GetCharmer(WorldObject const* pov = nullptr) const;
        Unit* GetCreator(WorldObject const* pov = nullptr) const;
        Unit* GetTarget(WorldObject const* pov = nullptr) const;
        WorldObject* GetChannelObject(WorldObject const* pov = nullptr) const;

        void SetCharm(Unit* charmed) { SetCharmGuid(charmed ? charmed->GetObjectGuid() : ObjectGuid()); }
        void SetCharmer(Unit* charmer) { SetCharmerGuid(charmer ? charmer->GetObjectGuid() : ObjectGuid()); }
        void SetTarget(WorldObject* target) { SetTargetGuid(target ? target->GetObjectGuid() : ObjectGuid()); }
        void SetChannelObject(WorldObject* object) { SetChannelObjectGuid(object ? object->GetObjectGuid() : ObjectGuid()); }

        // Purely logical guid constructs getters/setters (逻辑归属 GUID 封装：Owner/Pet/Selection/Master)
        // Owner: automatically resolves to creator guid and additionally to summoner guid (拥有者自动映射到创建者/召唤者)
        ObjectGuid const& GetOwnerGuid() const override { return GetCreatorGuid(); }
        void SetOwnerGuid(ObjectGuid owner) override { SetCreatorGuid(owner); }
        // Pet: automatically resolves to summon guid (permanent pet) (主宠物映射到常驻召唤 GUID)
        ObjectGuid const& GetPetGuid() const { return GetSummonGuid(); }
        void SetPetGuid(ObjectGuid pet) { SetSummonGuid(pet); }
        // Selection: by default resolves to target, overriden in Player (当前选中目标)
        virtual ObjectGuid const& GetSelectionGuid() const { return GetTargetGuid(); }
        virtual void SetSelectionGuid(ObjectGuid guid) { SetTargetGuid(guid); }
        // Master: automatically resolves to charmer or owner guid (控制者优先解析为魅惑者，否则为拥有者)
        ObjectGuid const& GetMasterGuid() const { ObjectGuid const& guid = GetCharmerGuid(); return (guid ? guid : GetOwnerGuid()); }

        // Convenience unit getters for some of the logical guid constructs above
        Unit* GetOwner(WorldObject const* pov = nullptr, bool recursive = false) const;
        Unit* GetMaster(WorldObject const* pov = nullptr) const;
        Unit* GetSpawner(WorldObject const* pov = nullptr) const;

        // Additional related server-side and client-side ownership-related methods
        // Beneficiary: owner of the xp/loot/etc credit, master or self (server-side) (击杀经验与拾取归属收益者)
        Unit* GetBeneficiary() const;
        Player* GetBeneficiaryPlayer() const;

        // Client controlled: check if unit currently is under client control (has active "mover"), optionally check for specific client (server-side) (检查当前是否受客户端直接操控)
        bool IsClientControlled(Player const* exactClient = nullptr) const;
        // Controlling client: server PoV on which client (player) controls movement of the unit at the moment, obtain "mover" (server-side) (获取当前控制该单位移动的客户端玩家)
        Player const* GetClientControlling() const;

        Pet* GetPet() const;
        virtual void SetPet(Unit* pet) { SetPetGuid(pet ? pet->GetObjectGuid() : ObjectGuid()); }

        Pet* GetMiniPet() const;
        void SetMiniPet(Unit* pet) { SetCritterGuid(pet ? pet->GetObjectGuid() : ObjectGuid()); }
        void RemoveMiniPet();

        void AddGuardian(Pet* pet);
        void RemoveGuardian(Pet* pet);
        void RemoveGuardians(bool force = true); // do not remove guardians in combat unless forced (移除所有守卫宠物，战斗中除非强制否则保留)
        Pet* FindGuardianWithEntry(uint32 entry);
        uint32 CountGuardiansWithEntry(uint32 entry);

        bool HasAnyPet() const;

        CharmInfo* GetCharmInfo() const { return m_charmInfo; }
        virtual CharmInfo* InitCharmInfo(Unit* charm);
        virtual void DeleteCharmInfo() { delete m_charmInfo; m_charmInfo = nullptr; }

        ObjectGuid const& GetTotemGuid(TotemSlot slot) const { return m_TotemSlot[slot]; }
        Totem* GetTotem(TotemSlot slot) const;
        bool IsAllTotemSlotsUsed() const;

        void _AddTotem(TotemSlot slot, Totem* totem);       // only for call from Totem summon code (仅供图腾召唤逻辑注册槽位)
        void _RemoveTotem(Totem* totem);                    // only for call from Totem class (仅供 Totem 类注销槽位)

        template<typename Func>
        void CallForAllControlledUnits(Func const& func, uint32 controlledMask);
        template<typename Func>
        bool CheckAllControlledUnits(Func const& func, uint32 controlledMask) const;

        bool AddSpellAuraHolder(SpellAuraHolder* holder);
        void AddAuraToModList(Aura* aura);

        // removing specific aura stack (移除指定光环/Holder 实例)
        void RemoveAura(Aura* Aur, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        void RemoveSpellAuraHolder(SpellAuraHolder* holder, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        void RemoveSingleAuraFromSpellAuraHolder(SpellAuraHolder* holder, SpellEffectIndex index, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        void RemoveSingleAuraFromSpellAuraHolder(uint32 spellId, SpellEffectIndex effindex, ObjectGuid casterGuid, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);

        // removing specific aura stacks by diff reasons and selections (按法术 ID、施法者、窃取、取消等条件移除光环)
        void RemoveAurasDueToSpell(uint32 spellId, SpellAuraHolder* except = nullptr, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        void RemoveAurasDueToItemSpell(Item* castItem, uint32 spellId);
        void RemoveAurasByCasterSpell(uint32 spellId, ObjectGuid casterGuid, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        void RemoveAurasDueToSpellBySteal(SpellAuraHolder* holder, Unit* stealer);
        void RemoveAurasDueToSpellByCancel(uint32 spellId);
        void RemoveAurasTriggeredBySpell(uint32 spellId, ObjectGuid casterGuid = ObjectGuid());
        void RemoveAuraStack(uint32 spellId, int32 modifier = -1);
        void RemoveAuraCharge(uint32 spellId);

        // removing unknown aura stacks by diff reasons and selections (按机制、驱散类型、打断标志等批量移除光环)
        void RemoveNotOwnTrackedTargetAuras();
        void RemoveAurasAtMechanicImmunity(uint32 mechMask, uint32 exceptSpellId, bool non_positive = false);
        void RemoveSpellsCausingAura(AuraType auraType);
        void RemoveSpellsCausingAura(AuraType auraType, SpellAuraHolder* except);
        void RemoveSpellsCausingAura(AuraType auraType, SpellAuraHolder* except, bool onlyMechanic);
        void RemoveSpellsCausingAura(AuraType auraType, ObjectGuid casterGuid);
        void RemoveRankAurasDueToSpell(uint32 spellId);
        bool RemoveNoStackAurasDueToAuraHolder(SpellAuraHolder* holder);
        void RemoveAurasWithInterruptFlags(uint32 flags);
        void RemoveAurasWithInterruptFlags(uint32 flags, SpellAuraHolder* except);
        void RemoveAurasWithAttribute(uint32 flags);
        void RemoveAurasWithDispelType(DispelType type, ObjectGuid casterGuid = ObjectGuid());
        void RemoveAllAuras(AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        void RemoveArenaAuras(bool onleave = false);
        void RemoveAllAurasOnDeath();
        void RemoveAllAurasOnEvade();
        void RemoveAllGroupBuffsFromCaster(ObjectGuid casterGuid);

        // remove specific aura on cast (施放法术时移除带有对应打断标志的光环)
        void RemoveAurasOnCast(uint32 flag, SpellEntry const* castedSpellEntry);

        // removing specific aura FROM stack by diff reasons and selections (扣减光环堆叠层数或因驱散减少层数)
        void RemoveAuraHolderFromStack(uint32 spellId, uint32 stackAmount = 1, ObjectGuid casterGuid = ObjectGuid(), AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
        void RemoveAuraHolderDueToSpellByDispel(uint32 spellId, uint32 dispellingSpellId, uint32 stackAmount, ObjectGuid casterGuid, Unit* dispeller);

        void DelaySpellAuraHolder(uint32 spellId, int32 delaytime, ObjectGuid casterGuid);

        // 抗性与基础属性增益修正字段读写
        float GetResistanceBuffMods(SpellSchools school, bool positive) const { return GetFloatValue(positive ? static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE) + static_cast<uint16>(school) : static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE) + static_cast<uint16>(school)); }
        void SetResistanceBuffMods(SpellSchools school, bool positive, float val) { SetFloatValue(positive ? static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE) + static_cast<uint16>(school) : static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE) + static_cast<uint16>(school), val); }
        void ApplyResistanceBuffModsMod(SpellSchools school, bool positive, float val, bool apply) { ApplyModSignedFloatValue(positive ? static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE) + static_cast<uint16>(school) : static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE) + static_cast<uint16>(school), val, apply); }
        void ApplyResistanceBuffModsPercentMod(SpellSchools school, bool positive, float val, bool apply) { ApplyPercentModFloatValue(positive ? static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE) + static_cast<uint16>(school) : static_cast<uint16>(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE) + static_cast<uint16>(school), val, apply); }
        void InitStatBuffMods()
        {
            for (int i = STAT_STRENGTH; i < MAX_STATS; ++i) SetFloatValue(static_cast<uint16>(UNIT_FIELD_POSSTAT0) + i, 0);
            for (int i = STAT_STRENGTH; i < MAX_STATS; ++i) SetFloatValue(static_cast<uint16>(UNIT_FIELD_NEGSTAT0) + i, 0);
        }
        void ApplyStatBuffMod(Stats stat, float val, bool apply) { ApplyModSignedFloatValue((val > 0 ? static_cast<uint16>(UNIT_FIELD_POSSTAT0) + stat : static_cast<uint16>(UNIT_FIELD_NEGSTAT0) + stat), val, apply); }
        void ApplyStatPercentBuffMod(Stats stat, float val, bool apply)
        {
            ApplyPercentModFloatValue(static_cast<uint16>(UNIT_FIELD_POSSTAT0) + stat, val, apply);
            ApplyPercentModFloatValue(static_cast<uint16>(UNIT_FIELD_NEGSTAT0) + stat, val, apply);
        }
        void SetCreateStat(Stats stat, float val) { m_createStats[stat] = val; }
        void SetCreateHealth(uint32 val) { SetUInt32Value(UNIT_FIELD_BASE_HEALTH, val); }
        uint32 GetCreateHealth() const { return GetUInt32Value(UNIT_FIELD_BASE_HEALTH); }
        void SetCreateMana(uint32 val) { SetUInt32Value(UNIT_FIELD_BASE_MANA, val); }
        uint32 GetCreateMana() const { return GetUInt32Value(UNIT_FIELD_BASE_MANA); }
        uint32 GetCreatePowers(Powers power) const;
        float GetPosStat(Stats stat) const { return GetFloatValue(static_cast<uint16>(UNIT_FIELD_POSSTAT0) + static_cast<uint16>(stat)); }
        float GetNegStat(Stats stat) const { return GetFloatValue(static_cast<uint16>(UNIT_FIELD_NEGSTAT0) + static_cast<uint16>(stat)); }
        float GetCreateStat(Stats stat) const { return m_createStats[stat]; }
        void SetCreateResistance(SpellSchools school, int32 val) { m_createResistances[school] = val; }
        int32 GetCreateResistance(SpellSchools school) const { return m_createResistances[school]; }

        void SetCurrentCastedSpell(Spell* pSpell);
        void InterruptSpell(CurrentSpellTypes spellType, bool withDelayed = true);
        void FinishSpell(CurrentSpellTypes spellType, bool ok = true);

        // set withDelayed to true to account delayed spells as casted
        // delayed+channeled spells are always accounted as casted
        // we can skip channeled or delayed checks using flags (检查当前是否正在施放非近战法术)
        bool IsNonMeleeSpellCasted(bool withDelayed, bool skipChanneled = false, bool skipAutorepeat = false, bool forMovement = false, bool forAutoIgnore = false, bool forEquip = false) const;

        // set withDelayed to true to interrupt delayed spells too
        // delayed+channeled spells are always interrupted (打断非近战法术)
        void InterruptNonMeleeSpells(bool withDelayed, uint32 spell_id = 0);
        void InterruptSpellsWithChannelFlags(uint32 flags);
        void InterruptSpellsAndAurasWithInterruptFlags(uint32 flags);
        bool IsInterruptible() const;

        Spell* GetCurrentSpell(CurrentSpellTypes spellType) const { return m_currentSpells[spellType]; }
        Spell* FindCurrentSpellBySpellId(uint32 spell_id) const;

        ObjectGuid m_ObjectSlotGuid[4];

        // 变形形态相关查询与设置
        ShapeshiftForm GetShapeshiftForm() const { return ShapeshiftForm(GetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_SHAPESHIFT)); }
        void  SetShapeshiftForm(ShapeshiftForm form) { SetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_SHAPESHIFT, form); }

        bool IsShapeShifted() const;
        bool IsNoWeaponShapeShift() const;
        bool IsAttackSpeedOverridenShapeShift() const;

        bool IsInDisallowedMountForm() const
        {
            ShapeshiftForm form = GetShapeshiftForm();
            return form != FORM_NONE && form != FORM_BATTLESTANCE && form != FORM_BERSERKERSTANCE && form != FORM_DEFENSIVESTANCE &&
                   form != FORM_SHADOW && form != FORM_STEALTH;
        }

        // 命中、暴击、三围、仇恨倍率与攻速修正缓存
        float m_modWeaponHitChance[MAX_ATTACK];
        float m_modSpellHitChance;
        float m_modSpellCritChance[MAX_SPELL_SCHOOL];

        float m_modCritChance[MAX_ATTACK];
        float m_modDodgeChance;
        float m_modParryChance;
        float m_modBlockChance;

        float m_threatModifier[MAX_SPELL_SCHOOL];
        float m_modAttackSpeedPct[3];
        float m_modAttackBaseDPSPct[MAX_ATTACK] = { 1.0f, 1.0f, 1.0f };

        // stat system (属性修正系统接口)
        bool HandleStatModifier(UnitMods unitMod, UnitModifierType modifierType, float amount, bool apply);
        void SetModifierValue(UnitMods unitMod, UnitModifierType modifierType, float value) { m_auraModifiersGroup[unitMod][modifierType] = value; }
        float GetModifierValue(UnitMods unitMod, UnitModifierType modifierType) const;
        float GetTotalStatValue(Stats stat) const;
        float GetTotalResistanceValue(SpellSchools school) const;
        float GetTotalAuraModValue(UnitMods unitMod) const;
        SpellSchools GetSpellSchoolByAuraGroup(UnitMods unitMod) const;
        Stats GetStatByAuraGroup(UnitMods unitMod) const;
        Powers GetPowerTypeByAuraGroup(UnitMods unitMod) const;
        bool CanModifyStats() const { return m_canModifyStats; }
        void SetCanModifyStats(bool modifyStats) { m_canModifyStats = modifyStats; }

        static float GetHealthBonusFromStamina(float stamina);
        virtual float GetHealthBonusFromStamina() const;
        static float GetManaBonusFromIntellect(float intellect);
        float GetManaBonusFromIntellect() const;

        virtual bool UpdateStats(Stats stat) = 0;
        virtual bool UpdateAllStats() = 0;
        virtual void UpdateResistances(uint32 school) = 0;
        virtual void UpdateArmor() = 0;
        virtual void UpdateMaxHealth();
        virtual void UpdateMaxPower(Powers power);
        virtual void UpdateAttackPowerAndDamage(bool ranged = false) = 0;
        virtual void UpdateDamagePhysical(WeaponAttackType attType) = 0;
        float GetTotalAttackPowerValue(WeaponAttackType attType) const;

        float GetBaseWeaponDamage(WeaponAttackType attType, WeaponDamageRange damageRange, uint8 index = 0) const;
        void SetBaseWeaponDamage(WeaponAttackType attType, WeaponDamageRange damageRange, float value, uint8 index = 0) { m_weaponDamageInfo.weapon[attType].damage[index].value[damageRange] = value; }

        SpellSchools GetWeaponDamageSchool(WeaponAttackType attType, uint8 index = 0) const { return m_weaponDamageInfo.weapon[attType].damage[index].school; }
        void SetWeaponDamageSchool(WeaponAttackType attType, SpellSchools school, uint8 index = 0) { m_weaponDamageInfo.weapon[attType].damage[index].school = school; }

        SpellSchoolMask GetAttackDamageSchoolMask(WeaponAttackType attType, bool first = false) const;
        void SetAttackDamageSchool(WeaponAttackType attType, SpellSchools school);

        inline SpellSchoolMask GetRangedDamageSchoolMask(bool first = false) const { return GetAttackDamageSchoolMask(RANGED_ATTACK, first); }
        inline void SetRangedDamageSchool(SpellSchools school) { SetAttackDamageSchool(RANGED_ATTACK, school); }

        inline SpellSchoolMask GetMeleeDamageSchoolMask(bool main, bool first = false) const { return GetAttackDamageSchoolMask((main ? BASE_ATTACK : OFF_ATTACK), first); }
        inline SpellSchoolMask GetMeleeDamageSchoolMask() const { return SpellSchoolMask(GetMeleeDamageSchoolMask(true, true) | GetMeleeDamageSchoolMask(false, true)); }
        inline void SetMeleeDamageSchool(bool main, SpellSchools school) { SetAttackDamageSchool((main ? BASE_ATTACK : OFF_ATTACK), school); }
        inline void SetMeleeDamageSchool(SpellSchools school) { SetMeleeDamageSchool(true, school); SetMeleeDamageSchool(false, school); }

        SpellSchoolMask GetMainAttackSchoolMask();

        // Visibility system (可见性与潜行/隐身侦测系统)
        UnitVisibility GetVisibility() const { return m_Visibility; }
        void SetVisibility(UnitVisibility x);
        void SetVisibilityWithoutUpdate(UnitVisibility x) { m_Visibility = x; }
        void UpdateVisibilityAndView() override;            // overwrite WorldObject::UpdateVisibilityAndView()

        // common function for visibility checks for player/creatures with detection code (玩家/生物通用可见性与反潜行侦测判定)
        bool IsVisibleForOrDetect(Unit const* u, WorldObject const* viewPoint, bool detect, bool inVisibleList = false, bool is3dDistance = true, bool spell = false, bool ignorePhase = false) const;

        // virtual functions for all world objects types
        bool isVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const override;
        // function for low level grid visibility checks in player/creature cases
        virtual bool IsVisibleInGridForPlayer(Player* pl) const = 0;
        bool isInvisibleForAlive() const;

        TrackedAuraTargetMap&       GetTrackedAuraTargets(TrackedAuraType type)       { return m_trackedAuraTargets[type]; }
        TrackedAuraTargetMap const& GetTrackedAuraTargets(TrackedAuraType type) const { return m_trackedAuraTargets[type]; }
        SpellImmuneList m_spellImmune[MAX_SPELL_IMMUNITY];
        bool IsAOEImmune() const { return m_aoeImmune; }
        void SetAOEImmune(bool state) { m_aoeImmune = state; }
        bool IsChainImmune() const { return m_chainImmune; }
        void SetChainImmune(bool state) { m_chainImmune = state; }

        // Threat related methods (仇恨管理与脱战管理器相关方法)
        bool CanHaveThreatList(bool ignoreAliveState = false) const;
        void AddThreat(Unit* pVictim, float threat = 0.0f, bool crit = false, SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NONE, SpellEntry const* threatSpell = nullptr);
        float ApplyTotalThreatModifier(float threat, SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NORMAL);
        void DeleteThreatList();
        bool SelectHostileTarget();
        bool IsSuppressedTarget(Unit* target) const;
        bool IsOfflineTarget(Unit* victim) const;
        void TauntUpdate();
        void FixateTarget(Unit* taunter);
        ThreatManager& getThreatManager() { return GetCombatData()->threatManager; }
        ThreatManager const& getThreatManager() const { return GetCombatData()->threatManager; }
        void addHatedBy(HostileReference* pHostileReference) { GetCombatData()->hostileRefManager.insertFirst(pHostileReference); };
        void removeHatedBy(HostileReference* /*pHostileReference*/) { /* nothing to do yet */ }
        HostileRefManager& getHostileRefManager() { return GetCombatData()->hostileRefManager; }
        HostileRefManager const& getHostileRefManager() const { return GetCombatData()->hostileRefManager; }
        void SetNoThreatState(bool state) { m_noThreat = state; }
        bool GetNoThreatState() { return m_noThreat; }

        CombatManager& GetCombatManager() { return m_combatManager; }
        CombatManager const& GetCombatManager() const { return m_combatManager; }
        void TriggerEvadeEvents();
        void TriggerHomeEvents();
        void EvadeTimerExpired();

        Aura* GetAura(uint32 spellId, SpellEffectIndex effindex);
        Aura const* GetAura(uint32 spellId, SpellEffectIndex effindex) const;
        Aura* GetAura(AuraType type, SpellFamily family, uint64 familyFlag, ObjectGuid casterGuid = ObjectGuid());
        SpellAuraHolder* GetSpellAuraHolder(uint32 spellid) const;
        SpellAuraHolder* GetSpellAuraHolder(uint32 spellid, ObjectGuid casterGuid) const;

        SpellAuraHolderMap&       GetSpellAuraHolderMap()       { return m_spellAuraHolders; }
        SpellAuraHolderMap const& GetSpellAuraHolderMap() const { return m_spellAuraHolders; }
        AuraList const& GetAurasByType(AuraType type) const { return m_modAuras[type]; }
        void ApplyAuraProcTriggerDamage(Aura* aura, bool apply);

        // 光环数值聚合查询（总加成/总乘数/最大正负值）
        int32 GetTotalAuraModifier(AuraType auratype) const;
        float GetTotalAuraMultiplier(AuraType auratype) const;
        int32 GetMaxPositiveAuraModifier(AuraType auratype) const;
        int32 GetMaxNegativeAuraModifier(AuraType auratype) const;

        int32 GetTotalAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const;
        float GetTotalAuraMultiplierByMiscMask(AuraType auratype, uint32 misc_mask) const;
        int32 GetMaxPositiveAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const;
        int32 GetMaxNegativeAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const;

        int32 GetTotalAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const;
        float GetTotalAuraMultiplierByMiscValue(AuraType auratype, int32 misc_value) const;
        int32 GetMaxPositiveAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const;
        int32 GetMaxNegativeAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const;

        int32 GetMaxPositiveAuraModifierByItemClass(AuraType auratype, Item* weapon) const;

        Aura* GetDummyAura(uint32 spell_id) const;

        uint32 m_AuraFlags;

        // 模型外观 ID 读写与碰撞数据刷新
        uint32 GetDisplayId() const { return GetUInt32Value(UNIT_FIELD_DISPLAYID); }
        void SetDisplayId(uint32 modelId);
        uint32 GetNativeDisplayId() const { return GetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID); }
        void SetNativeDisplayId(uint32 modelId) { SetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID, modelId); }
        void RestoreDisplayId();

        // at any changes to scale and/or displayId (缩放或模型变化时更新碰撞高度与半径)
        void UpdateModelData();

        // 单位归属的动态物体 (DynamicObject) / 游戏物体 (GameObject) / 召唤生物管理
        DynamicObject* GetDynObject(uint32 spellId, SpellEffectIndex effIndex, Unit* target = nullptr);
        DynamicObject* GetDynObject(uint32 spellId);
        void AddDynObject(DynamicObject* dynObj);
        void RemoveDynObject(uint32 spellid);
        void RemoveDynObjectWithGUID(ObjectGuid guid) { m_dynObjGUIDs.remove(guid); }
        void RemoveAllDynObjects();

        GameObject* GetGameObject(uint32 spellId) const;
        void AddGameObject(GameObject* gameObj);
        void AddWildGameObject(GameObject* gameObj);
        void RemoveGameObject(GameObject* gameObj, bool del, bool removeAura = true);
        void RemoveGameObject(uint32 spellid, bool del);
        void RemoveAllGameObjects();

        void AddCreature(uint32 spellId, Creature* creature);
        void RemoveCreature(uint32 spellId, bool del);

        float CalculateDamage(WeaponAttackType attType, bool normalized, uint8 index = 0);
        float GetAPMultiplier(WeaponAttackType attType, bool normalized);
        void ModifyAuraState(AuraState flag, bool apply);
        bool HasAuraState(AuraState flag) const { return HasFlag(UNIT_FIELD_AURASTATE, convertEnumToFlag(flag)); }
        bool HasAuraStateForCaster(AuraState flag, ObjectGuid casterGuid) const;
        void UnsummonAllTotems() const;
        Unit* SelectMagnetTarget(Unit* victim, Spell* spell = nullptr);

        // 法术伤害、治疗及近战伤害加成结算接口
        int32 SpellBonusWithCoeffs(SpellEntry const* spellProto, SpellEffectIndex effectIndex, int32 total, int32 benefit, int32 ap_benefit, bool donePart);
        int32 SpellBaseDamageBonusDone(SpellSchoolMask schoolMask);
        int32 SpellBaseDamageBonusTaken(SpellSchoolMask schoolMask) const;
        uint32 SpellDamageBonusDone(Unit* victim, SpellSchoolMask schoolMask, SpellEntry const* spellProto, SpellEffectIndex effectIndex, uint32 pdamage, DamageEffectType damagetype, uint32 stack = 1);
        uint32 SpellDamageBonusTaken(Unit* caster, SpellSchoolMask schoolMask, SpellEntry const* spellProto, SpellEffectIndex effectIndex, uint32 pdamage, DamageEffectType damagetype, uint32 stack = 1);
        int32 SpellBaseHealingBonusDone(SpellSchoolMask schoolMask);
        int32 SpellBaseHealingBonusTaken(SpellSchoolMask schoolMask) const;
        uint32 SpellHealingBonusDone(Unit* victim, SpellEntry const* spellProto, SpellEffectIndex effectIndex, int32 healamount, DamageEffectType damagetype, uint32 stack = 1);
        uint32 SpellHealingBonusTaken(Unit* caster, SpellEntry const* spellProto, SpellEffectIndex effectIndex, int32 healamount, DamageEffectType damagetype, uint32 stack = 1);
        uint32 MeleeDamageBonusDone(Unit* victim, uint32 damage, WeaponAttackType attType, SpellSchoolMask schoolMask, SpellEntry const* spellProto = nullptr, SpellEffectIndex effectIndex = EFFECT_INDEX_0, DamageEffectType damagetype = DIRECT_DAMAGE, uint32 stack = 1, bool flat = true);
        uint32 MeleeDamageBonusTaken(Unit* caster, uint32 pdamage, WeaponAttackType attType, SpellSchoolMask schoolMask, SpellEntry const* spellProto = nullptr, SpellEffectIndex effectIndex = EFFECT_INDEX_0, DamageEffectType damagetype = DIRECT_DAMAGE, uint32 stack = 1, bool flat = true);

        // 法术触发 (Proc) 事件检测与处理函数族
        enum class SpellProcEventTriggerCheck
        {
            SPELL_PROC_TRIGGER_FAILED = 0,
            SPELL_PROC_TRIGGER_ROLL_FAILED = 1,
            SPELL_PROC_TRIGGER_OK = 2,
        };

        SpellProcEventTriggerCheck IsTriggeredAtSpellProcEvent(ProcExecutionData& data, SpellAuraHolder* holder, SpellProcEventEntry const*& spellProcEvent, bool (&canProc)[MAX_EFFECT_INDEX]);
        // only to be used in proc handlers - basepoints is expected to be a MAX_EFFECT_INDEX sized array (仅供触发处理器内部使用)
        SpellAuraProcResult TriggerProccedSpell(Unit* target, std::array<int32, MAX_EFFECT_INDEX>& basepoints, uint32 triggeredSpellId, Item* castItem, Aura* triggeredByAura, uint32 cooldown, ObjectGuid originalCaster);
        SpellAuraProcResult TriggerProccedSpell(Unit* target, std::array<int32, MAX_EFFECT_INDEX>& basepoints, SpellEntry const* spellInfo, Item* castItem, Aura* triggeredByAura, uint32 cooldown, ObjectGuid originalCaster);
        // Aura proc handlers (各类光环触发处理器)
        SpellAuraProcResult HandleDummyAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleHasteAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleProcTriggerSpellAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleProcTriggerDamageAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleOverrideClassScriptAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleRaidProcFromChargeAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleRaidProcFromChargeWithValueAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleModCastingSpeedNotStackAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleReflectSpellsSchoolAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleModPowerCostSchoolAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleMagnetAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleMechanicImmuneResistanceAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleManaShieldAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleAttackPowerAttackerBonusAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleModResistanceAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleRemoveByDamageChanceProc(ProcExecutionData& data);
        SpellAuraProcResult HandleInvisibilityAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandlePeriodicAuraProc(ProcExecutionData& data);
        SpellAuraProcResult HandleSpellModProc(ProcExecutionData& data);
        SpellAuraProcResult HandleNULLProc(ProcExecutionData& /*data*/)
        {
            // no proc handler for this aura type (该光环类型无需特殊触发处理)
            return SPELL_AURA_PROC_OK;
        }
        SpellAuraProcResult HandleCantTrigger(ProcExecutionData& /*data*/)
        {
            // this aura type can't proc (该光环类型不可被触发)
            return SPELL_AURA_PROC_CANT_TRIGGER;
        }

        // 五秒回蓝规则计时器
        void SetLastManaUse() { m_lastManaUseTimer = 5000; }
        bool IsUnderLastManaUseEffect() const { return m_lastManaUseTimer != 0 && m_lastManaUseTimer != 5000; }

        void SetContestedPvP(Player* attackedPlayer = nullptr);

        // 免疫与伤害吸收/抵抗计算
        void ApplySpellImmune(Aura const* aura, uint32 op, uint32 type, bool apply);
        void ApplySpellDispelImmunity(Aura const* aura, DispelType type, bool apply);
        virtual bool IsImmuneToSpell(SpellEntry const* spellInfo, bool castOnSelf, uint8 effectMask, WorldObject const* caster);
        virtual bool IsImmuneToDamage(SpellSchoolMask meleeSchoolMask);
        virtual bool IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const;
        bool IsImmuneToSchool(SpellEntry const* spellInfo, uint8 effectMask) const;

        static float CalcArmorReducedDamage(WorldObject* attacker, Unit* victim, const float damage);
        void CalculateDamageAbsorbAndResist(Unit* caster, SpellSchoolMask schoolMask, DamageEffectType damagetype, const uint32 damage, uint32* absorb, int32* resist, bool canReflect = false, bool canResist = true, bool binary = false);
        void CalculateAbsorbResistBlock(Unit* caster, SpellNonMeleeDamage* spellDamageInfo, SpellEntry const* spellProto, WeaponAttackType attType = BASE_ATTACK);

        // 移动速度计算与击退接口
        void  UpdateSpeed(UnitMoveType mtype, bool forced, float ratio = 1.0f);
        float GetSpeedInMotion() const;
        float GetSpeed(UnitMoveType mtype) const;
        float GetXYFlagBasedSpeed() const;
        float GetXYFlagBasedSpeed(uint32 moveFlags) const;
        float GetSpeedRateInMotion() const;
        float GetSpeedRate(UnitMoveType mtype) const { return m_speed_rate[mtype]; }
        void SetSpeedRate(UnitMoveType mtype, float rate, bool forced = false);

        void KnockBackFrom(Unit* target, float horizontalSpeed, float verticalSpeed);
        void KnockBackWithAngle(float angle, float horizontalSpeed, float verticalSpeed);

        bool HasHoverAura() const { return HasAuraType(SPELL_AURA_HOVER); }
        template<typename Func>
        bool HasAuraWithCondition(Func const& func) const
        {
            Unit::SpellAuraHolderMap const& holders = GetSpellAuraHolderMap();
            for (const auto& holder : holders)
            {
                if (func(holder.second))
                {
                    return true;
                }
            }

            return false;
        }

        void _RemoveAllAuraMods();
        void _ApplyAllAuraMods();

        int32 CalculateAuraDuration(SpellEntry const* spellProto, uint32 effectMask, int32 duration, Unit const* caster);

        float CalculateLevelPenalty(SpellEntry const* spellProto) const;

        void addFollower(FollowerReference* pRef) { m_FollowingRefManager.insertFirst(pRef); }
        void removeFollower(FollowerReference* /*pRef*/) { /* nothing to do yet */ }

        MotionMaster* GetMotionMaster() { return &i_motionMaster; }
        MotionMaster const* GetMotionMaster() const { return &i_motionMaster; }

        bool IsStopped() const { return !(hasUnitState(UNIT_STAT_MOVING)); }
        void StopMoving(bool forceSendStop = false);
        void InterruptMoving(bool forceSendStop = false);

        ///----------Various crowd control methods----------------- (群体控制状态设置接口：迷惑/恐惧/昏迷/定身)
        inline bool IsCrowdControlled() const { return HasFlag(UNIT_FIELD_FLAGS, (UNIT_FLAG_CONFUSED | UNIT_FLAG_FLEEING | UNIT_FLAG_STUNNED)); }

        inline bool IsConfused() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CONFUSED); }
        bool SetConfused(bool apply, ObjectGuid casterGuid = ObjectGuid(), uint32 spellID = 0);

        inline bool IsFleeing() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_FLEEING); }
        bool SetFleeing(bool apply, ObjectGuid casterGuid = ObjectGuid(), uint32 spellID = 0, uint32 duration = 0);

        inline bool IsStunned() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED); }
        bool SetStunned(bool apply, ObjectGuid casterGuid = ObjectGuid(), uint32 spellID = 0, bool logout = false);

        inline bool IsStunnedByLogout() const { return hasUnitState(UNIT_STAT_LOGOUT_TIMER); }

        // Panic: AI reaction script, NPC flees (e.g. at low health) (恐慌逃跑，通常由低血量 AI 触发)
        inline bool IsInPanic() const { return hasUnitState(UNIT_STAT_PANIC); }
        inline bool SetInPanic(uint32 duration) { return SetFleeing(true, GetObjectGuid(), 0, duration); }

        inline bool IsImmobilizedState() const { return hasUnitState(UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_LOGOUT_TIMER | UNIT_STAT_AI_ROOT); }
        void SetImmobilizedState(bool apply, bool stun = false, bool logout = false);
        ///----------End of crowd control methods----------

        bool IsFeigningDeath() const { return HasFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH); }
        bool IsFeigningDeathSuccessfully() const { return hasUnitState(UNIT_STAT_FEIGN_DEATH); }
        void SetFeignDeath(bool apply, ObjectGuid casterGuid = ObjectGuid(), uint32 spellID = 0, bool dynamic = true, bool success = true);
        virtual bool IsIgnoringFeignDeath() const { return false; }
        virtual bool IsIgnoringSanctuary() const { return false; }

        virtual bool IsSlowedInCombat() const { return false; }

        void InterruptSpellsCastedOnMe(bool killDelayed = false);

        void AddComboPointHolder(uint32 lowguid) { m_ComboPointHolders.insert(lowguid); }
        void RemoveComboPointHolder(uint32 lowguid) { m_ComboPointHolders.erase(lowguid); }
        void ClearComboPointHolders();

        ///----------Pet responses methods----------------- (宠物客户端反馈消息发送)
        void SendPetActionFeedback(uint8 msg) const;
        void SendPetTalk(uint32 pettalk) const;
        void SendPetAIReaction() const;
        void SendPetDismiss(uint32 soundId) const;
        ///----------End of Pet responses methods----------

        void propagateSpeedChange() { GetMotionMaster()->propagateSpeedChange(); }

        // reactive attacks (反击型技能状态刷新)
        void ClearAllReactives();
        void StartReactiveTimer(ReactiveType reactive) { m_reactiveTimer[reactive] = REACTIVE_TIMER_START;}
        void UpdateReactives(uint32 p_time);

        // group updates (通知同小队成员更新光环图标)
        void UpdateAuraForGroup(uint8 slot);

        // pet auras (作用于宠物的光环集合管理)
        typedef std::set<PetAura const*> PetAuraSet;
        PetAuraSet m_petAuras;
        void AddPetAura(PetAura const* petSpell);
        void RemovePetAura(PetAura const* petSpell);

        float GetTransOffsetX() const { return m_movementInfo.GetTransportPos().x; }
        float GetTransOffsetY() const { return m_movementInfo.GetTransportPos().y; }
        float GetTransOffsetZ() const { return m_movementInfo.GetTransportPos().z; }
        float GetTransOffsetO() const { return m_movementInfo.GetTransportPos().o; }
        uint32 GetTransTime() const { return m_movementInfo.GetTransportTime(); }

        // Movement info (样条路径移动处理器)
        Movement::MoveSpline* movespline;

        void ScheduleAINotify(uint32 delay, bool forced = false);
        bool IsAINotifyScheduled() const { return m_AINotifyEvent != nullptr;}
        void FinalizeAINotifyEvent() { m_AINotifyEvent = nullptr; }
        void AbortAINotifyEvent();
        void OnRelocated();

        bool IsLinkingEventTrigger() const { return m_isCreatureLinkingTrigger; }
        void TriggerAggroLinkingEvent(Unit* enemy);

        virtual bool CanSwim() const = 0;
        virtual bool CanFly() const = 0;
        virtual bool CanWalk() const = 0;
        virtual bool IsFlying() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_FLYING); }

        void SendMessageToAllWhoSeeMeMove(WorldPacket const& data, ObjectGuid mover) const;

        // Take possession of an unit (pet, creature, ...) (附身控制目标单位)
        bool TakePossessOf(Unit* possessed);

        // Take possession of a new spawned unit (生成并直接附身控制新单位)
        Unit* TakePossessOf(SpellEntry const* spellEntry, SummonPropertiesEntry const* summonProp, uint32 effIdx, float x, float y, float z, float ang);

        // Take charm of an unit (魅惑控制目标单位)
        bool TakeCharmOf(Unit* charmed, uint32 spellId = 0, bool advertised = true);

        // Break own charm effects on a specific charmed unit (解除对指定目标的控制效果)
        void BreakCharmOutgoing(Unit* charmed);

        // Break own charm effects on all charmed units or advertised charm field only (解除自身施加的所有控制效果)
        void BreakCharmOutgoing(bool advertisedOnly = false);

        // Break charm effects from current charmer (解除来自当前控制者的魅惑效果)
        void BreakCharmIncoming();

        // Uncharm (physically revert the charm effect) the unit and reset player control if required (还原魅惑状态并重置 AI/客户端控制权)
        void Uncharm(Unit* charmed, uint32 spellId = 0);

        void RemoveUnattackableTargets(Unit* charmer = nullptr);

        // Combat prevention (战斗开关、仇恨传递与警戒距离设置)
        bool CanEnterCombat() const { return m_canEnterCombat && !GetCombatManager().IsEvadingHome(); }
        void SetCanEnterCombat(bool can) { m_canEnterCombat = can; }

        void SetIgnoreRangedTargets(bool state) { m_ignoreRangedTargets = state; }
        bool IsIgnoringRangedTargets() { return m_ignoreRangedTargets; }

        void SetSupportThreatOnly(bool state) { m_supportThreatOnly = state; }
        bool IsSupportThreatOnly() { return m_supportThreatOnly; }

        void DisableThreatPropagationToOwner() { m_ownerThreatPropagation = false; }
        bool IsPropagatingThreatToOwner() { return m_ownerThreatPropagation; } // TBC+ - Eye of Kilrogg (是否将仇恨传递至主人，基尔罗格之眼不传递)

        float GetAttackDistance(Unit const* target) const;
        virtual uint32 GetDetectionRange() const { return 18.f; }

        virtual UnitAI* AI() { return nullptr; }
        virtual CombatData* GetCombatData() { return m_combatData; }
        virtual CombatData const* GetCombatData() const { return m_combatData; }

        virtual void SetBaseWalkSpeed(float speed) { m_baseSpeedWalk = speed; }
        virtual void SetBaseRunSpeed(float speed, bool /*force*/ = true) { m_baseSpeedRun = speed; }
        float GetBaseRunSpeed() { return m_baseSpeedRun; }

        bool IsSpellProccingHappening() const { return m_spellProcsHappening; }
        void AddDelayedHolderDueToProc(SpellAuraHolder* holder) { m_delayedSpellAuraHolders.push_back(holder); }

        void ResetAutoRepeatSpells() { m_AutoRepeatFirstCast = true; }
        uint32 GetAutoRepeatRemainingMs() const;

        const uint64& GetAuraUpdateMask() const { return m_auraUpdateMask; }
        void SetAuraUpdateMask(uint8 slot) { m_auraUpdateMask |= (uint64(1) << slot); }
        void ResetAuraUpdateMask() { m_auraUpdateMask = 0; }

        // WorldObject overrides (地形/碰撞 Z 轴修正覆写)
        void UpdateAllowedPositionZ(float x, float y, float& z, Map* atMap = nullptr) const override;
        void AdjustZForCollision(float x, float y, float& z, float halfHeight) const override;

        virtual uint32 GetSpellRank(SpellEntry const* spellInfo) const;

        Player* GetNextRandomRaidMember(float radius, AuraType noAuraType);

        // 脚本光环与缩放光环注册
        bool HasOverrideScript(uint32 id) const;
        Aura* GetOverrideScript(uint32 id) const;
        void RegisterOverrideScriptAura(Aura* aura, uint32 id, bool apply);
        void RegisterScriptedLocationAura(Aura* aura, AuraScriptLocation location, bool apply); // Spell scripting - requires correctly set spell_affect (注册触发位置脚本光环)
        std::vector<Aura*> const& GetScriptedLocationAuras(AuraScriptLocation location) const;

        uint8 GetComboPoints() const { return m_comboPoints; }
        ObjectGuid const& GetComboTargetGuid() const { return m_comboTargetGuid; }

        void AddComboPoints(Unit* target, int8 count);
        void ClearComboPoints();

        void RegisterScalingAura(Aura* aura, bool apply);
        void UpdateScalingAuras();

        uint32 GetDamageDoneByOthers() { return m_damageByOthers; }
        uint32 GetModifierXpBasedOnDamageReceived(uint32 xp);

        void UpdateNextUpdateTime() override;
        uint32 ShouldPerformObjectUpdate(uint32 const diff) override;

        void OverrideMountDisplayId(uint32 newDisplayId);

        void UpdateSplinePosition(bool relocateOnly = false);
        void SendFlightSplineSyncIfNeeded();

        virtual bool CanCallForAssistance() const { return true; }
        virtual bool CanCheckForHelp() const { return true; }

        virtual std::vector<uint32> GetCharmSpells() const { return {}; }
        void CharmCooldownInitialize(WorldPacket& data) const;

        FormationSlotDataSPtr GetFormationSlot() { return m_formationSlot; }
        void SetFormationSlot(FormationSlotDataSPtr fSlot) { m_formationSlot = fSlot; }

        virtual bool IsNoWeaponSkillGain() const { return false; }
        virtual bool IsPreventingDeath() const { return false; }
        virtual bool IsIgnoringMisdirection() const { return false; }

        virtual CreatureInfo const* GetMountInfo() const { return nullptr; } // TODO: Meant to be used by players during taxi
        virtual void SetMountInfo(CreatureInfo const* /*info*/) {} // does nothing for base unit
        virtual void SetModelRunSpeed(float /*runSpeed*/) {} // does nothing for base unit

        virtual bool IsThreatUpdateSent() const { return true; }
        virtual bool IgnoreLosWhenCastingOnMe() const { return false; }
        virtual bool IsDealTripleDamageToPets() const { return false; }
        virtual bool IsEnemyCheckIgnoresLos() const { return false; }

    protected:
        bool MeetsSelectAttackingRequirement(Unit* target, SpellEntry const* spellInfo, uint32 selectFlags, SelectAttackingTargetParams params, int32 unitConditionId) const;

        // 单位各攻击槽位基础武器伤害与学派信息
        struct WeaponDamageInfo
        {
            struct Weapon
            {
                struct Damage
                {
                    SpellSchools school = SPELL_SCHOOL_NORMAL;
                    float value[2] = { BASE_MINDAMAGE, BASE_MAXDAMAGE };
                };

                uint32 lines = 1;
                Damage damage[MAX_ITEM_PROTO_DAMAGES];
            };

            Weapon weapon[MAX_ATTACK];
        };

        explicit Unit();

        void _UpdateSpells(uint32 time);
        void _UpdateAutoRepeatSpell();
        bool m_AutoRepeatFirstCast;                         // 自动射击/魔杖首次施放标记

        uint32 m_attackTimer[MAX_ATTACK];                   // 主手/副手/远程攻击冷却计时器

        float m_createStats[MAX_STATS];                     // 创建时基础属性值（无装备/光环加成）
        int32 m_createResistances[MAX_SPELL_SCHOOL];        // 创建时基础抗性

        Unit* m_attacking;                                  // 当前攻击目标指针

        DeathState m_deathState;                            // 当前生死状态

        SpellAuraHolderMap m_spellAuraHolders;              // 身上所有光环持有器映射表 (spellId -> holder)
        SpellAuraHolderMap::iterator m_spellAuraHoldersUpdateIterator; // != end() in Unit::m_spellAuraHolders update and point to next element (遍历更新时防迭代器失效的游标)
        AuraList m_deletedAuras;                            // auras removed while in ApplyModifier and waiting deleted (延迟销毁的光环列表)
        SpellAuraHolderList m_deletedHolders;               // 延迟销毁的光环持有器列表
        std::map<uint32, Aura*> m_classScripts;             // 按脚本 ID 索引的职业/覆盖光环
        std::vector<Aura*> m_scriptedLocations[SCRIPT_LOCATION_MAX];
        std::vector<Aura*> m_scalingAuras;                  // 随属性动态缩放的光环列表

        // Store Auras for which the target must be tracked (需要追踪唯一目标的光环映射表)
        TrackedAuraTargetMap m_trackedAuraTargets[MAX_TRACKED_AURA_TYPES];

        GuidList m_dynObjGUIDs;                             // 所属动态物体 GUID 列表

        GameObjectList m_gameObj;                           // 所属游戏物体列表（如光明之泉等）
        typedef std::map<uint32, ObjectGuid> WildGameObjectMap;
        WildGameObjectMap m_wildGameObjs;                   // 野外关联游戏物体映射
        bool m_isSorted;
        uint32 m_transform;

        std::map<uint32, Creature*> m_creatures;            // 关联召唤生物映射

        AuraList m_modAuras[TOTAL_AURAS];                   // 按 AuraType 分类索引的光环列表
        float m_auraModifiersGroup[UNIT_MOD_END][MODIFIER_TYPE_END]; // 单位属性四阶段修正数组

        enum class AttackPowerMod
        {
            MELEE_ATTACK_POWER  = 0,
            RANGED_ATTACK_POWER = 1,
            ATTACK_POWER_MOD_MAX
        };

        enum class AttackPowerModSign
        {
            MOD_SIGN_POS,
            MOD_SIGN_NEG,
            MOD_SIGN_MAX
        };
        float m_attackPowerMod[size_t(AttackPowerMod::ATTACK_POWER_MOD_MAX)][size_t(AttackPowerModSign::MOD_SIGN_MAX)]; // 攻击强度正负固定修正

        WeaponDamageInfo m_weaponDamageInfo;                // 武器基础伤害与学派缓存

        bool m_canModifyStats;                              // 是否允许立即重算衍生属性（批量加载时暂闭）
        // std::list< spellEffectPair > AuraSpells[TOTAL_AURAS];  // TODO: use this if ok for mem

        float m_speed_rate[MAX_MOVE_TYPE];                  // 各移动类型当前速度倍率

        CharmInfo* m_charmInfo;                             // 魅惑/宠物控制状态信息

        MotionMaster i_motionMaster;                        // 移动生成器控制器

        uint32 m_reactiveTimer[MAX_REACTIVE];               // 反击技能有效倒计时
        uint32 m_regenTimer;                                // 生命/能量回复累计计时器
        uint32 m_lastManaUseTimer;                          // 施法消耗法力后 5 秒规则计时器

        bool m_canDodge;                                    // 是否具备躲闪能力
        bool m_canParry;                                    // 是否具备招架能力
        bool m_canBlock;                                    // 是否具备格挡能力

        bool m_canDualWield = false;                        // 是否具备双持能力

        void DisableSpline();
        void EndSpline();
        bool m_isCreatureLinkingTrigger;                    // 生物联动触发标记
        bool m_isSpawningLinked;                            // 联动刷出状态标记

        CombatData* m_combatData;                           // 战斗仇恨数据（被魅惑时可能与 CharmInfo 切换）
        CombatManager m_combatManager;                      // 脱战与追击计时管理器

        // base speeds set by model/template (模板或模型决定的基础行走/奔跑速度)
        float m_baseSpeedWalk;
        float m_baseSpeedRun;

        // Protected unit guid fields getters/setters (受保护的更新字段 GUID 读写器)
        // Charm: temporary pet unit guid (临时魅惑控制的目标 GUID)
        ObjectGuid const& GetCharmGuid() const { return GetGuidValue(UNIT_FIELD_CHARM); }
        void SetCharmGuid(ObjectGuid const& charm) { SetGuidValue(UNIT_FIELD_CHARM, charm); }
        // Summon: permanent pet unit guid (do not use: managed by SetPetGuid/GetPetGuid) (永久召唤宠物 GUID)
        ObjectGuid const& GetSummonGuid() const { return GetGuidValue(UNIT_FIELD_SUMMON); }
        void SetSummonGuid(ObjectGuid const& summon) { SetGuidValue(UNIT_FIELD_SUMMON, summon); }
        // Charmer: temporary owner unit guid [nameplate] (当前临时控制者 GUID)
        ObjectGuid const& GetCharmerGuid() const { return GetGuidValue(UNIT_FIELD_CHARMEDBY); }
        void SetCharmerGuid(ObjectGuid const& owner) { SetGuidValue(UNIT_FIELD_CHARMEDBY, owner); }
        // Summoner: permanent owner unit guid for player pets [nameplate] (do not use: managed by SetOwnerGuid/GetOwnerGuid) (玩家宠物的永久召唤者 GUID)
        ObjectGuid const& GetSummonerGuid() const { return GetGuidValue(UNIT_FIELD_SUMMONEDBY); }
        void SetSummonerGuid(ObjectGuid const& owner) { SetGuidValue(UNIT_FIELD_SUMMONEDBY, owner); }
        // Creator: permanent owner unit guid for npc pets or non-pet units [nameplate] (do not use: managed by SetOwnerGuid/GetOwnerGuid) (创建者 GUID)
        ObjectGuid const& GetCreatorGuid() const { return GetGuidValue(UNIT_FIELD_CREATEDBY); }
        void SetCreatorGuid(ObjectGuid const& creator) { SetGuidValue(UNIT_FIELD_CREATEDBY, creator); }
        // Target: current target guid as advertised on unit frames (also known as selection) (当前面向/锁定的目标 GUID)
        ObjectGuid const& GetTargetGuid() const { return GetGuidValue(UNIT_FIELD_TARGET); }
        void SetTargetGuid(ObjectGuid const& targetGuid) { SetGuidValue(UNIT_FIELD_TARGET, targetGuid); }
        // Persuation target: temporarily increased reputation unit guid (pre-WotLK relations) (说服目标 GUID)
        ObjectGuid const& GetPersuadedGuid() const { return GetGuidValue(UNIT_FIELD_PERSUADED); }
        void SetPersuadedGuid(ObjectGuid const& persuaded) { SetGuidValue(UNIT_FIELD_PERSUADED, persuaded); }
        // Channel target: current channeling spell's target worldobject guid (当前引导法术的目标 GUID)
        ObjectGuid const& GetChannelObjectGuid() const { return GetGuidValue(UNIT_FIELD_CHANNEL_OBJECT); }
        void SetChannelObjectGuid(ObjectGuid const& targetGuid) { SetGuidValue(UNIT_FIELD_CHANNEL_OBJECT, targetGuid); }

        // Additional server-side guid fields getters/setters
        // Critter: permanent mini-pet unit guid (pre-WotLK compatibility replacement for critter guid field) (非战斗跟随小宠物 GUID)
        ObjectGuid const& GetCritterGuid() const { return m_critterGuid; }
        void SetCritterGuid(ObjectGuid critterGuid) { m_critterGuid = critterGuid; }

        FormationSlotDataSPtr m_formationSlot;              // 阵型槽位数据

    private:
        void CleanupDeletedAuras();
        void UpdateSplineMovement(uint32 t_diff);

        float GetCombatRatingReduction(CombatRating cr) const;
        uint32 GetCombatRatingDamageReduction(CombatRating cr, float rate, float cap, uint32 damage) const;

        Unit* _GetTotem(TotemSlot slot) const;              // for templated function without include need
        Pet* _GetPet(ObjectGuid guid) const;                // for templated function without include need
        Unit* _GetUnit(ObjectGuid guid) const;              // for templated function without include need

        // Wrapper called by DealDamage when a creature is killed (生物击杀后分派拾取、经验与任务结算包装器)
        static void JustKilledCreature(Unit* killer, Creature* victim, Player* responsiblePlayer);

        uint32 m_state;                                     // Even derived shouldn't modify (内部 UnitState 位掩码，派生类亦不得直接修改)

        AttackerSet m_attackers;                            // Used to help know who is currently attacking this unit (正在攻击自身的单位集合)
        Spell* m_currentSpells[CURRENT_MAX_SPELL];          // 当前各施法槽位实例指针

        UnitVisibility m_Visibility;                        // 当前可见性状态
        Position m_last_notified_position;                  // 上次触发视野通知的位置
        BasicEvent* m_AINotifyEvent;                        // 延迟视野通知事件指针
        ShortTimeTracker m_movesplineTimer;                 // 样条移动同步计时器
        bool m_hasPeriodicAura;                             // 是否存在周期性光环缓存标志

        Diminishing m_Diminishing;                          // 递减收益记录列表

        FollowerRefManager m_FollowingRefManager;           // 跟随者引用管理器

        ComboPointHolderSet m_ComboPointHolders;            // 对自身持有连击点数的玩家低位 GUID 集合

        ObjectGuid m_critterGuid;                           // pre-WotLK critter compatibility field (小宠物 GUID 兼容字段)

        GuidSet m_guardianPets;                             // 守卫宠物 GUID 集合
        GuidSet::iterator m_guardianPetsIterator;

        GuidSet m_charmedUnitsPrivate;                      // stores non-advertised active charmed unit guids (e.g. aoe charms) (未公开到客户端字段的群控单位 GUID 集合)

        ObjectGuid m_TotemSlot[MAX_TOTEM_SLOT];             // 四系图腾槽位 GUID

        bool m_canEnterCombat;                              // 是否允许进入战斗

        // Need to safeguard aura application in Unit::Update (防止 Update 迭代期间修改容器的保护标志)
        bool m_spellUpdateHappening;
        // Need to safeguard aura proccing in Unit::ProcDamageAndSpell (防止触发循环期间重入修改的保护标志)
        bool m_spellProcsHappening;
        std::vector<SpellAuraHolder*> m_delayedSpellAuraHolders;
        uint32 m_hasHeartbeatProcCounter;                   // 存在心跳触发光环计数

        bool m_alwaysHit;                                   // 强制必中标志
        bool m_noThreat;                                    // 不产生仇恨标志
        bool m_supportThreatOnly;                           // 仅对友方提供辅助仇恨标志
        bool m_ownerThreatPropagation;                      // 是否将自身仇恨传播给主人
        bool m_ignoreRangedTargets;                         // Ignores ranged targets when picking someone to attack (索敌时忽略远程目标)
        bool m_debuggingMovement;                           // 移动调试日志开关

        // guard to prevent chaining extra attacks (防止额外攻击无限连锁的重入保护)
        bool m_extraAttacksExecuting;

        uint64 m_auraUpdateMask;                            // 队伍光环槽位脏掩码

        ObjectGuid m_comboTargetGuid;                       // 连击点目标 GUID
        int8 m_comboPoints;                                 // 当前连击点数

        uint32 m_damageByOthers;                            // 由非玩家及其宠物（如守卫 NPC）造成的伤害累计，用于经验衰减

        bool m_isMountOverriden;                            // 是否覆盖飞行坐骑模型
        uint32 m_overridenMountId;                          // 覆盖的坐骑显示 ID

        bool m_aoeImmune;                                   // 是否免疫范围 (AOE) 伤害
        bool m_chainImmune;                                 // 是否免疫链式弹射法术

        TimePoint m_lastMoveTime; // used for resetting combat timer on melee (上次移动时间戳，用于重置近战脱战计时)

    private:                                                // Error traps for some wrong args using (编译期错误陷阱：防止省略可选参数导致布尔/枚举参数错位)
        // this will catch and prevent build for any cases when all optional args skipped and instead triggered used non boolean type
        // no bodies expected for this declarations
        template <typename TR>
        void CastSpell(Unit* Victim, uint32 spell, TR triggered);
        template <typename TR>
        void CastSpell(Unit* Victim, SpellEntry const* spell, TR triggered);
        template <typename TR>
        void CastCustomSpell(Unit* Victim, uint32 spell, int32 const* bp0, int32 const* bp1, int32 const* bp2, TR triggeredFlags);
        template <typename SP, typename TR>
        void CastCustomSpell(Unit* Victim, SpellEntry const* spell, int32 const* bp0, int32 const* bp1, int32 const* bp2, TR triggeredFlags);
        template <typename TR>
        void CastSpell(float x, float y, float z, uint32 spell, TR triggered);
        template <typename TR>
        void CastSpell(float x, float y, float z, SpellEntry const* spell, TR triggered);
};

// 遍历当前单位所控制的指定类型附属单位并执行回调
template<typename Func>
void Unit::CallForAllControlledUnits(Func const& func, uint32 controlledMask)
{
    if (controlledMask & CONTROLLED_PET)
        if (Pet* pet = GetPet())
            func(pet);

    if (controlledMask & CONTROLLED_MINIPET)
        if (Pet* mini = GetMiniPet())
            func(mini);

    if (controlledMask & CONTROLLED_GUARDIANS)
    {
        for (m_guardianPetsIterator = m_guardianPets.begin(); m_guardianPetsIterator != m_guardianPets.end();)
            if (Pet* guardian = GetMap()->GetPet(*(m_guardianPetsIterator++)))
                func(guardian);
    }

    if (controlledMask & CONTROLLED_TOTEMS)
    {
        for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
            if (Unit* totem = _GetTotem(TotemSlot(i)))
                func(totem);
    }

    if (controlledMask & CONTROLLED_CHARM)
    {
        if (Unit* charm = GetCharm())
            func(charm);

        for (GuidSet::const_iterator itr = m_charmedUnitsPrivate.begin(); itr != m_charmedUnitsPrivate.end();)
            if (Unit* charmed = _GetUnit(*(itr++)))
                func(charmed);
    }
}


// 检查是否存在满足条件的受控附属单位
template<typename Func>
bool Unit::CheckAllControlledUnits(Func const& func, uint32 controlledMask) const
{
    if (controlledMask & CONTROLLED_PET)
        if (Pet const* pet = GetPet())
            if (func(pet))
                return true;

    if (controlledMask & CONTROLLED_MINIPET)
        if (Pet* mini = GetMiniPet())
            if (func(mini))
                return true;

    if (controlledMask & CONTROLLED_GUARDIANS)
    {
        for (auto m_guardianPet : m_guardianPets)
            if (Pet* guardian = GetMap()->GetPet(m_guardianPet))
                if (func(guardian))
                    return true;
    }

    if (controlledMask & CONTROLLED_TOTEMS)
    {
        for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
            if (Unit const* totem = _GetTotem(TotemSlot(i)))
                if (func(totem))
                    return true;
    }

    if (controlledMask & CONTROLLED_CHARM)
    {
        if (Unit const* charm = GetCharm())
            if (func(charm))
                return true;

        for (GuidSet::const_iterator itr = m_charmedUnitsPrivate.begin(); itr != m_charmedUnitsPrivate.end();)
            if (Unit const* charmed = _GetUnit(*(itr++)))
                if (func(charmed))
                    return true;
    }

    return false;
}

// Helper for targets nearest to the spell target
// The spell target is always first unless there is a target at _completely_ the same position (unbelievable case) (目标距离升序排序仿函数：最近优先)
struct TargetDistanceOrderNear
{
    WorldObject const* m_mainTarget;
    DistanceCalculation m_distcalc;

    TargetDistanceOrderNear(WorldObject const* target, DistanceCalculation distcalc = DIST_CALC_NONE) : m_mainTarget(target), m_distcalc(distcalc) {}
    // functor for operator ">"
    bool operator()(WorldObject const* _Left, WorldObject const* _Right) const
    {
        return m_mainTarget->GetDistanceOrder(_Left, _Right, true, m_distcalc);
    }
};

// Helper for targets furthest away to the spell target
// The spell target is always first unless there is a target at _completely_ the same position (unbelievable case) (目标距离降序排序仿函数：最远优先)
struct TargetDistanceOrderFarAway
{
    WorldObject const* m_mainTarget;
    DistanceCalculation m_distcalc;
    TargetDistanceOrderFarAway(WorldObject const* target, DistanceCalculation distcalc = DIST_CALC_NONE) : m_mainTarget(target), m_distcalc(distcalc) {}
    // functor for operator "<"
    bool operator()(WorldObject const* _Left, WorldObject const* _Right) const
    {
        return !m_mainTarget->GetDistanceOrder(_Left, _Right, true, m_distcalc);
    }
};

// 按生命百分比升序排列（相同血量时距离近者优先）
struct LowestHPNearestOrder
{
    WorldObject const* m_mainTarget;
    DistanceCalculation m_distcalc;

    LowestHPNearestOrder(WorldObject const* target, DistanceCalculation distcalc = DIST_CALC_NONE) : m_mainTarget(target), m_distcalc(distcalc) {}
    // functor for operator ">"
    bool operator()(Unit const* _Left, Unit const* _Right) const
    {
        if (_Left->GetHealthPercent() == _Right->GetHealthPercent())
            return m_mainTarget->GetDistanceOrder(_Left, _Right, true, m_distcalc);
        return _Left->GetHealthPercent() < _Right->GetHealthPercent();
    }
};

// 绑定到 Unit 的 Lambda 延迟事件包装类
class UnitLambdaEvent : public BasicEvent
{
    public:
    UnitLambdaEvent(Unit& owner, std::function<void(Unit&)> const& func) : m_owner(owner), m_func(func) {}

    virtual bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
    {
        m_func(m_owner);
        return true;
    }

    Unit& m_owner;
    std::function<void(Unit&)> m_func;
};

/** @} */

#endif
