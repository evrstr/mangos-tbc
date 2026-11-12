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

#ifndef MANGOS_CREATURE_EAI_H
#define MANGOS_CREATURE_EAI_H

#include "Common.h"
#include "Entities/Creature.h"
#include "AI/BaseAI/CreatureAI.h"
#include "Entities/Unit.h"
#include "AI/ScriptDevAI/base/TimerAI.h"
#include <set>

class Player;
class WorldObject;

#define EVENT_UPDATE_TIME               500 // EventAI 定时事件轮询周期（毫秒）
#define MAX_ACTIONS                     3   // 单个事件最多携带的动作槽位数
#define MAX_PHASE                       32  // EventAI 支持的最大阶段数（使用 32 位阶段掩码）

// EventAI 调试日志输出宏
#define LOG_PROCESS_EVENT                                                                                                       \
    DEBUG_FILTER_LOG(LOG_FILTER_EVENT_AI_DEV, "CreatureEventAI: Event type %u (script %u) triggered for %s (invoked by %s)",    \
                     holder.event.event_type, holder.event.event_id, m_creature->GetGuidStr().c_str(), actionInvoker ? actionInvoker->GetGuidStr().c_str() : "<no invoker>")

// EventAI 事件触发器类型枚举（对应 creature_ai_scripts.event_type）
enum EventAI_Type
{
    EVENT_T_TIMER_IN_COMBAT         = 0,                    // InitialMin, InitialMax, RepeatMin, RepeatMax (战斗内定时触发)
    EVENT_T_TIMER_OOC               = 1,                    // InitialMin, InitialMax, RepeatMin, RepeatMax (脱战状态定时触发)
    EVENT_T_HP                      = 2,                    // HPMax%, HPMin%, RepeatMin, RepeatMax (自身血量百分比区间触发)
    EVENT_T_MANA                    = 3,                    // ManaMax%,ManaMin% RepeatMin, RepeatMax (自身法力百分比区间触发)
    EVENT_T_AGGRO                   = 4,                    // NONE (进入战斗开怪时触发)
    EVENT_T_KILL                    = 5,                    // RepeatMin, RepeatMax, PlayerOnly (1) (击杀目标时触发)
    EVENT_T_DEATH                   = 6,                    // ConditionId (自身死亡时触发)
    EVENT_T_EVADE                   = 7,                    // NONE (脱离战斗时触发)
    EVENT_T_SPELLHIT                = 8,                    // SpellID, School, RepeatMin, RepeatMax (被指定法术或学派命中时触发)
    EVENT_T_RANGE                   = 9,                    // MinDist, MaxDist, RepeatMin, RepeatMax (与当前目标距离落入区间时触发)
    EVENT_T_OOC_LOS                 = 10,                   // NoHostile, MaxRange, RepeatMin, RepeatMax, PlayerOnly, ConditionId (脱战状态视线内出现单位时触发)
    EVENT_T_SPAWNED                 = 11,                   // Condition, CondValue1 (刷新或重生时触发)
    EVENT_T_TARGET_HP               = 12,                   // HPMax%, HPMin%, RepeatMin, RepeatMax (当前仇恨目标血量百分比区间触发)
    EVENT_T_TARGET_CASTING          = 13,                   // RepeatMin, RepeatMax (当前目标正在施法时触发)
    EVENT_T_FRIENDLY_HP             = 14,                   // HPDeficit, Radius, RepeatMin, RepeatMax, IsPercent (附近友军生命缺损达阈值时触发)
    EVENT_T_FRIENDLY_IS_CC          = 15,                   // DispelType, Radius, RepeatMin, RepeatMax (附近友军受控制需驱散时触发)
    EVENT_T_FRIENDLY_MISSING_BUFF   = 16,                   // SpellId, Radius, RepeatMin, RepeatMax, Flags (附近友军缺少指定增益光环时触发)
    EVENT_T_SUMMONED_UNIT           = 17,                   // CreatureId, RepeatMin, RepeatMax (成功召唤指定生物时触发)
    EVENT_T_TARGET_MANA             = 18,                   // ManaMax%, ManaMin%, RepeatMin, RepeatMax (当前目标法力百分比区间触发)
    EVENT_T_QUEST_ACCEPT            = 19,                   // QuestID (接取任务时触发 - 未实现)
    EVENT_T_QUEST_COMPLETE          = 20,                   // (完成任务时触发 - 未实现)
    EVENT_T_REACHED_HOME            = 21,                   // NONE (脱战返回出生点到达时触发)
    EVENT_T_RECEIVE_EMOTE           = 22,                   // EmoteId, ConditionId (收到玩家表情动作时触发)
    EVENT_T_AURA                    = 23,                   // Param1 = SpellID, Param2 = Number of time stacked, Param3/4 Repeat Min/Max (自身存在指定层数光环时触发)
    EVENT_T_TARGET_AURA             = 24,                   // Param1 = SpellID, Param2 = Number of time stacked, Param3/4 Repeat Min/Max (当前目标存在指定层数光环时触发)
    EVENT_T_SUMMONED_JUST_DIED      = 25,                   // CreatureId, RepeatMin, RepeatMax (自身召唤的生物死亡时触发)
    EVENT_T_SUMMONED_JUST_DESPAWN   = 26,                   // CreatureId, RepeatMin, RepeatMax (自身召唤的生物消失时触发)
    EVENT_T_MISSING_AURA            = 27,                   // Param1 = SpellID, Param2 = Number of time stacked expected, Param3/4 Repeat Min/Max (自身缺少指定光环时触发)
    EVENT_T_TARGET_MISSING_AURA     = 28,                   // Param1 = SpellID, Param2 = Number of time stacked expected, Param3/4 Repeat Min/Max (当前目标缺少指定光环时触发)
    EVENT_T_TIMER_GENERIC           = 29,                   // InitialMin, InitialMax, RepeatMin, RepeatMax (通用计时器，战斗内外均递减)
    EVENT_T_RECEIVE_AI_EVENT        = 30,                   // AIEventType, Sender-Entry, unused, unused (收到其他生物发送的 AI 事件时触发)
    EVENT_T_ENERGY                  = 31,                   // EnergyMax%, EnergyMin%, RepeatMin, RepeatMax (自身能量百分比区间触发)
    EVENT_T_SELECT_ATTACKING_TARGET = 32,                   // MinRange, MaxRange, RepeatMin, RepeatMax (从指定距离范围内随机选敌时触发)
    EVENT_T_FACING_TARGET           = 33,                   // Position, unused, RepeatMin, RepeatMax (处于目标正面或背面时触发)
    EVENT_T_SPELLHIT_TARGET         = 34,                   // SpellID, School, RepeatMin, RepeatMax (自身法术命中目标时触发)
    EVENT_T_DEATH_PREVENTED         = 35,                   // (触发免死锁血保护时触发)
    EVENT_T_TARGET_NOT_REACHABLE    = 36,                   // (当前追击目标路径不可达时触发)
    EVENT_T_SPELL_CAST              = 37,                   // SpellId (法术列表成功施放指定法术时触发)
    EVENT_T_BOARD_VEHICLE           = 38,                   // Boarding, SeatID (上载具事件预留)
    EVENT_T_PASSENGER_BOARDED       = 39,                   // Boarding, SeatID (乘客登乘事件预留)
    EVENT_T_VEHICLE_RETURN          = 40,                   // SeatID (载具返回事件预留)
    EVENT_T_PASSENGER_SPAWN         = 41,                   // SeatID (乘客刷新事件预留)
    EVENT_T_PASSENGER_CONTROL_END   = 42,                   // SeatID (乘客控制结束事件预留)
    EVENT_T_ACTION_SET              = 43,                   // SetId, StepIndex, TimeMs (动作集序列按步骤时间触发)
    EVENT_T_MOVEMENT_INFORM         = 44,                   // MovementType, PointId, PathId (到达移动点或路径点时触发)

    EVENT_T_END,
};

// EventAI 动作类型枚举（对应 creature_ai_scripts.actionN_type）
enum EventAI_ActionType
{
    ACTION_T_NONE                       = 0,                // No action (无动作)
    ACTION_T_TEXT                       = 1,                // TextId1, optionally TextId2, optionally TextId3(if TextId2 exist). If more than just TextId1 is defined, randomize. Values are broadcast_text Ids. (喊话/文本输出，支持最多 3 个随机广播文本 ID)
    ACTION_T_SET_FACTION                = 2,                // FactionId (or 0 for default) (临时修改或恢复阵营)
    ACTION_T_MORPH_TO_ENTRY_OR_MODEL    = 3,                // Creature_template entry(param1) OR ModelId (param2) (or 0 for both to demorph) (变形为指定生物外观或模型，双 0 则还原)
    ACTION_T_SOUND                      = 4,                // SoundId (播放音效)
    ACTION_T_EMOTE                      = 5,                // EmoteId (播放动作表情)
    ACTION_T_RANDOM_SAY                 = 6,                // UNUSED
    ACTION_T_RANDOM_YELL                = 7,                // UNUSED
    ACTION_T_RANDOM_TEXTEMOTE           = 8,                // UNUSED
    ACTION_T_RANDOM_SOUND               = 9,                // SoundId1, SoundId2, SoundId3 (-1 in any field means no output if randomed that field) (随机播放音效)
    ACTION_T_RANDOM_EMOTE               = 10,               // EmoteId1, EmoteId2, EmoteId3 (-1 in any field means no output if randomed that field) (随机播放表情)
    ACTION_T_CAST                       = 11,               // SpellId, Target - default = 15, CastFlags (施放法术)
    ACTION_T_SPAWN                      = 12,               // CreatureID, Target, Duration in ms (在自身位置召唤临时生物并可选攻击目标)
    ACTION_T_THREAT_SINGLE              = 13,               // Threat, Target, IsDirect (增减单个目标的仇恨值或百分比)
    ACTION_T_THREAT_ALL_PCT             = 14,               // Threat% (按百分比调整仇恨列表所有目标仇恨)
    ACTION_T_QUEST_EVENT                = 15,               // QuestID, Target (向指定玩家或小队完成任务探索事件)
    ACTION_T_CAST_EVENT                 = 16,               // QuestID, SpellId, Target - must be removed as hack? (通知玩家触发法术任务击杀/施放计数)
    ACTION_T_SET_UNIT_FIELD             = 17,               // Field_Number, Value, Target (设置单位对象字段值)
    ACTION_T_SET_UNIT_FLAG              = 18,               // Flags (may be more than one field OR'd together), Target (设置 UNIT_FIELD_FLAGS 标志位)
    ACTION_T_REMOVE_UNIT_FLAG           = 19,               // Flags (may be more than one field OR'd together), Target (移除 UNIT_FIELD_FLAGS 标志位)
    ACTION_T_AUTO_ATTACK                = 20,               // AllowAttackState (0 = stop attack, anything else means continue attacking) (开关普通近战自动攻击)
    ACTION_T_COMBAT_MOVEMENT            = 21,               // AllowCombatMovement (0 = stop combat based movement, anything else continue attacking) (开关战斗追击移动)
    ACTION_T_SET_PHASE                  = 22,               // Phase (设置当前 AI 阶段)
    ACTION_T_INC_PHASE                  = 23,               // Value (may be negative to decrement phase, should not be 0) (按步长增减 AI 阶段)
    ACTION_T_EVADE                      = 24,               // CombatOnly (脱离战斗重置，或仅清战不归位)
    ACTION_T_FLEE_FOR_ASSIST            = 25,               // No Params (逃跑呼救)
    ACTION_T_QUEST_EVENT_ALL            = 26,               // QuestID, UseThreatList (1 = true, 0 = false) (向仇恨列表所有玩家完成任务事件)
    ACTION_T_CAST_EVENT_ALL             = 27,               // CreatureId, SpellId (向仇恨列表所有玩家触发法术任务计数)
    ACTION_T_REMOVEAURASFROMSPELL       = 28,               // Target, Spellid (移除目标身上的指定法术光环)
    ACTION_T_RANGED_MOVEMENT            = 29,               // Distance, Angle (设置远程追击距离与角度)
    ACTION_T_RANDOM_PHASE               = 30,               // PhaseId1, PhaseId2, PhaseId3 (从 3 个候选阶段中随机切换)
    ACTION_T_RANDOM_PHASE_RANGE         = 31,               // PhaseMin, PhaseMax (在阶段区间内随机切换)
    ACTION_T_SUMMON_ID                  = 32,               // CreatureId, Target, SpawnId (按 creature_ai_summons 坐标点召唤生物)
    ACTION_T_KILLED_MONSTER             = 33,               // CreatureId, Target (奖励击杀生物任务计数)
    ACTION_T_SET_INST_DATA              = 34,               // Field, Data (写入副本脚本整数数据)
    ACTION_T_SET_INST_DATA64            = 35,               // Field, Target (写入目标 GUID 到副本脚本 64 位数据)
    ACTION_T_UPDATE_TEMPLATE            = 36,               // Entry (变身/更新生物模板 Entry)
    ACTION_T_DIE                        = 37,               // No Params (立即自杀死亡)
    ACTION_T_ZONE_COMBAT_PULSE          = 38,               // No Params (触发区域战斗脉冲拉所有副本玩家进战)
    ACTION_T_CALL_FOR_HELP              = 39,               // Radius (向指定半径内友军呼救)
    ACTION_T_SET_SHEATH                 = 40,               // Sheath (0-passive,1-melee,2-ranged) (设置武器拔出状态)
    ACTION_T_FORCE_DESPAWN              = 41,               // Delay (0-instant despawn) (强制消失)
    ACTION_T_SET_DEATH_PREVENTION       = 42,               // 0-off/1-on (开关免死锁血状态)
    ACTION_T_MOUNT_TO_ENTRY_OR_MODEL    = 43,               // Creature_template entry(param1) OR ModelId (param2) (or 0 for both to unmount) (上马或下马)
    ACTION_T_CHANCED_TEXT               = 44,               // Chance to display the text, TextId1, optionally TextId2. If more than just TextId1 is defined, randomize. Values are broadcast_text Ids. (带概率触发的随机喊话)
    ACTION_T_THROW_AI_EVENT             = 45,               // EventType, Radius, Target (向周围生物发送 AI 事件)
    ACTION_T_SET_THROW_MASK             = 46,               // EventTypeMask, unused, unused (设置血量阈值/死亡自动发送 AI 事件掩码)
    ACTION_T_SET_STAND_STATE            = 47,               // StandState, unused, unused (设置站立/坐下/躺下姿态)
    ACTION_T_CHANGE_MOVEMENT            = 48,               // MovementType, WanderDistance if Movement Type 1 and PathId if Movement Type 2, asDefault (切换移动生成器类型)
    ACTION_T_REUSE                      = 49,               // REUSE (保留槽位)
    ACTION_T_SET_REACT_STATE            = 50,               // React state, unused, unused (设置反应姿态：被动/防御/主动)
    ACTION_T_PAUSE_WAYPOINTS            = 51,               // DoPause 0: unpause waypoint 1: pause waypoint, unused, unused (暂停或恢复路径点巡逻)
    ACTION_T_INTERRUPT_SPELL            = 52,               // SpellType enum CurrentSpellTypes, unused, unused (打断当前正在施放的指定类型法术)
    ACTION_T_START_RELAY_SCRIPT         = 53,               // Relay script ID, target, unused (启动 dbscript_on_relay 中继脚本)
    ACTION_T_TEXT_NEW                   = 54,               // Text ID, target, template Id (新版文本输出，支持随机文本模板)
    ACTION_T_ATTACK_START               = 55,               // Target, unused, unused (对目标发起攻击)
    ACTION_T_DESPAWN_GUARDIANS          = 56,               // Guardian Entry ID (or 0 to despawn all guardians), unused, unused (解散指定或全部守护者随从)
    ACTION_T_SET_RANGED_MODE            = 57,               // type of ranged mode, distance to chase at (配置远程施法模式与追击距离)
    ACTION_T_SET_WALK                   = 58,               // type of walking, unused, unused (设置脱战/追击时的行走或奔跑状态)
    ACTION_T_SET_FACING                 = 59,               // Target, 0 - set, 1 - reset (面向目标或重置朝向)
    ACTION_T_SET_SPELL_SET              = 60,               // SetId (切换生物法术列表集合 ID)
    ACTION_T_SET_IMMOBILIZED_STATE      = 61,               // state (true - rooted), combatonly (true - autoremoved on combat stop) (设置 AI 定身状态)
    ACTION_T_SET_DESPAWN_AGGREGATION    = 62,               // mask, entry, entry2 (设置召唤物联动销毁聚合掩码与过滤 Entry)
    ACTION_T_SET_IMMUNITY_SET           = 63,               // SetId - creature_immunities (切换生物免疫集合)
    ACTION_T_SET_FOLLOW_MOVEMENT        = 64,               // state - 0 off, 1 on (开关跟随移动)
    ACTION_T_RETREAT                    = 65,               // (触发撤退求援)
    ACTION_T_START_ACTION_SET           = 66,               // SetId, StepIndex, Repeat (0x1), Ooc (0x2) (启动定时动作集序列)

    ACTION_T_END,
};

// EventAI 动作目标选择类型枚举
enum Target
{
    // Self (m_creature) (自身)
    TARGET_T_SELF                           = 0,            // Self cast (生物自身)

    // Hostile targets (仇恨列表敌对目标)
    TARGET_T_HOSTILE                        = 1,            // Our current target (ie: highest aggro) (当前主仇恨目标)
    TARGET_T_HOSTILE_SECOND_AGGRO           = 2,            // Second highest aggro (generaly used for cleaves and some special attacks) (第二高仇恨目标)
    TARGET_T_HOSTILE_LAST_AGGRO             = 3,            // Dead last on aggro (no idea what this could be used for) (最低仇恨目标)
    TARGET_T_HOSTILE_RANDOM                 = 4,            // Just any random target on our threat list (仇恨列表随机目标)
    TARGET_T_HOSTILE_RANDOM_NOT_TOP         = 5,            // Any random target except top threat (除第一仇恨外的随机目标)

    // Invoker targets (事件触发者相关目标)
    TARGET_T_ACTION_INVOKER                 = 6,            // Unit who caused this Event to occur (only works for EVENT_T_AGGRO, EVENT_T_KILL, EVENT_T_DEATH, EVENT_T_SPELLHIT, EVENT_T_OOC_LOS, EVENT_T_FRIENDLY_HP, EVENT_T_FRIENDLY_IS_CC, EVENT_T_FRIENDLY_MISSING_BUFF, EVENT_T_RECEIVE_EMOTE, EVENT_T_RECEIVE_AI_EVENT) (触发当前事件的单位)
    TARGET_T_ACTION_INVOKER_OWNER           = 7,            // Unit who is responsible for Event to occur (only works for EVENT_T_AGGRO, EVENT_T_KILL, EVENT_T_DEATH, EVENT_T_SPELLHIT, EVENT_T_OOC_LOS, EVENT_T_FRIENDLY_HP, EVENT_T_FRIENDLY_IS_CC, EVENT_T_FRIENDLY_MISSING_BUFF, EVENT_T_RECEIVE_EMOTE, EVENT_T_RECEIVE_AI_EVENT) (事件触发者的控制/受益玩家)
    TARGET_T_EVENT_SENDER                   = 10,           // Unit who sent an AIEvent that was received with EVENT_T_RECEIVE_AI_EVENT (发送 AI 事件的源单位)

    // Hostile players (敌对玩家目标)
    TARGET_T_HOSTILE_RANDOM_PLAYER          = 8,            // Just any random player on our threat list (仇恨列表随机玩家)
    TARGET_T_HOSTILE_RANDOM_NOT_TOP_PLAYER  = 9,            // Any random player from threat list except top threat (除最高仇恨外的随机玩家)

    // Summon targeting (召唤者)
    TARGET_T_SPAWNER                        = 11,           // Owner of unit if exists (召唤该生物的源单位)

    // Event specific targeting (事件特定缓存目标)
    TARGET_T_EVENT_SPECIFIC                 = 12,           // Filled by specific event (由特定事件如 FRIENDLY_HP 写入的 eventTarget)

    // Player associations (击杀/拾取归属玩家)
    TARGET_T_PLAYER_INVOKER                 = 13,           // Player who initiated hostile contact with this npc (最先开怪的玩家)
    TARGET_T_PLAYER_TAPPED                  = 14,           // Player who currently holds to score the kill credit from the npc (当前拥有击杀拾取权的玩家)

    // Default Spell Target (默认空目标)
    TARGET_T_NONE                           = 15,           // Default spell target - sets nullptr which should be most common spell fill (传 nullptr 由法术隐式目标自行解析)

    TARGET_T_HOSTILE_RANDOM_MANA            = 16,           // Random target with mana (带法力条的随机敌对目标)
    TARGET_T_NEAREST_AOE_TARGET             = 17,           // Nearest target for aoe (校验 AoE 范围内是否存在最近敌对目标)
    TARGET_T_HOSTILE_FARTHEST_AWAY          = 18,           // Farthest away target, excluding melee range (近战范围之外距离最远的敌对目标)
};

// EventAI 事件标志位掩码
enum EventFlags : uint32
{
    EFLAG_REPEATABLE            = 0x01,                     // Event repeats (事件可重复触发)
    EFLAG_NORMAL                = 0x02,                     // Event only occurs in Normal instance difficulty (仅普通难度副本/野外生效)
    EFLAG_HEROIC                = 0x04,                     // Event only occurs in Heroic instance difficulty (仅英雄难度副本生效)
    EFLAG_RESERVED_3            = 0x08,                     // Used in master for difficulty 2
    EFLAG_RESERVED_4            = 0x10,                     // Used in master for difficulty 3
    EFLAG_RANDOM_ACTION         = 0x20,                     // Event only execute one from existed actions instead each action. (仅从非空动作槽位中随机执行一个)
    EFLAG_RESERVED_6            = 0x40,
    EFLAG_DEBUG_ONLY            = 0x80,                     // Event only occurs in debug build (仅 Debug 构建生效)
    EFLAG_RANGED_MODE_ONLY      = 0x100,                    // Event only occurs in ranged mode (仅远程模式下触发)
    EFLAG_MELEE_MODE_ONLY       = 0x200,                    // Event only occurs in melee mode (仅近战模式下触发)
    EFLAG_COMBAT_ACTION         = 0x400,                    // First action must succeed (要求第一个动作执行成功才继续后续动作并重置计时器)
    // no free bits, uint8 field
    EFLAG_DIFFICULTY_ALL        = (EFLAG_NORMAL | EFLAG_HEROIC)
};

// EVENT_T_SPAWNED 触发条件模式
enum SpawnedEventMode
{
    SPAWNED_EVENT_ALWAY = 0,                                // 每次生成总是触发
    SPAWNED_EVENT_MAP   = 1,                                // 仅在指定 MapId 地图生效
    SPAWNED_EVENT_ZONE  = 2                                 // 仅在指定 Zone/Area 区域生效
};

// ACTION_T_SET_WALK 行走模式参数
enum WalkSetting : uint32
{
    RUN_DEFAULT  = 0, // Default for OOC (脱战默认奔跑)
    WALK_DEFAULT = 1, // 脱战默认行走
    RUN_CHASE    = 2, // Default for combat (战斗追击默认奔跑)
    WALK_CHASE   = 3, // 战斗追击默认行走
};

// ACTION_T_SET_DESPAWN_AGGREGATION 聚合销毁掩码
enum DespawnAggregation : uint32
{
    AGGREGATION_ENABLED = 0x1,                              // 启用召唤物 GUID 聚合收集
    AGGREGATION_EVADE   = 0x2,                              // 脱战或尸体移除时销毁收集的召唤物
    AGGREGATION_DEATH   = 0x4,                              // 死亡时销毁收集的召唤物
};

// ACTION_T_CHANGE_MOVEMENT 移动切换标志
enum ChangeMovementFlags : uint32
{
    CHANGE_MOVEMENT_FLAG_AS_DEFAULT = 0x1,                  // 同步更新默认移动生成器类型
    CHANGE_MOVEMENT_FLAG_WAYPOINT_PATH = 0x2,               // 路径点来源于 waypoint_path 表
    CHANGE_MOVEMENT_MAX = 0x3,
};

struct CreatureEventAI_Action
{
    EventAI_ActionType type: 16;
    union
    {
        // ACTION_T_TEXT                                    = 1
        struct
        {
            int32 TextId[3];
        } text;
        // ACTION_T_SET_FACTION                             = 2
        struct
        {
            uint32 factionId;                               // faction id or 0 to restore default faction
            uint32 factionFlags;                            // flags will restore default faction at evade and/or respawn
        } set_faction;
        // ACTION_T_MORPH_TO_ENTRY_OR_MODEL                 = 3
        struct
        {
            uint32 creatureId;                              // set one from fields (or 0 for both to demorph)
            uint32 modelId;
        } morph;
        // ACTION_T_SOUND                                   = 4
        struct
        {
            uint32 soundId;
        } sound;
        // ACTION_T_EMOTE                                   = 5
        struct
        {
            uint32 emoteId;
        } emote;
        // ACTION_T_RANDOM_SOUND                            = 9
        struct
        {
            int32 soundId1;                                 // (-1 in any field means no output if randomed that field)
            int32 soundId2;
            int32 soundId3;
        } random_sound;
        // ACTION_T_RANDOM_EMOTE                            = 10
        struct
        {
            int32 emoteId1;                                 // (-1 in any field means no output if randomed that field)
            int32 emoteId2;
            int32 emoteId3;
        } random_emote;
        // ACTION_T_CAST                                    = 11
        struct
        {
            uint32 spellId;
            uint32 target;
            uint32 castFlags;
        } cast;
        // ACTION_T_SPAWN                                   = 12
        struct
        {
            uint32 creatureId;
            uint32 target;
            uint32 duration;
        } summon;
        // ACTION_T_THREAT_SINGLE                           = 13
        struct
        {
            int32 value;
            uint32 target;
            uint32 isDirect;
        } threat_single;
        // ACTION_T_THREAT_ALL_PCT                          = 14
        struct
        {
            int32 percent;
        } threat_all_pct;
        // ACTION_T_QUEST_EVENT                             = 15
        struct
        {
            uint32 questId;
            uint32 target;
            uint32 rewardGroup;
        } quest_event;
        // ACTION_T_CAST_EVENT                              = 16
        struct
        {
            uint32 creatureId;
            uint32 spellId;
            uint32 target;
        } cast_event;
        // ACTION_T_SET_UNIT_FIELD                          = 17
        struct
        {
            uint32 field;
            uint32 value;
            uint32 target;
        } set_unit_field;
        // ACTION_T_SET_UNIT_FLAG                           = 18,  // value provided mask bits that will be set
        // ACTION_T_REMOVE_UNIT_FLAG                        = 19,  // value provided mask bits that will be clear
        struct
        {
            uint32 value;
            uint32 target;
        } unit_flag;
        // ACTION_T_AUTO_ATTACK                             = 20
        struct
        {
            uint32 state;                                   // 0 = stop attack, anything else means continue attacking
        } auto_attack;
        // ACTION_T_COMBAT_MOVEMENT                         = 21
        struct
        {
            uint32 state;                                   // 0 = stop combat based movement, anything else continue attacking
            uint32 melee;                                   // if set: at stop send melee combat stop if in combat, use for terminate melee fighting state for switch to ranged
        } combat_movement;
        // ACTION_T_SET_PHASE                               = 22
        struct
        {
            uint32 phase;
        } set_phase;
        // ACTION_T_INC_PHASE                               = 23
        struct
        {
            uint32 combatOnly;
        } evade;
        // ACTION_T_EVADE                                   = 24
        struct
        {
            int32 step;
        } set_inc_phase;
        // ACTION_T_QUEST_EVENT_ALL                         = 26
        struct
        {
            uint32 questId;
            uint32 useThreatList;                           // bool: 1 = true; 0 = false
        } quest_event_all;
        // ACTION_T_CAST_EVENT_ALL                          = 27
        struct
        {
            uint32 creatureId;
            uint32 spellId;
        } cast_event_all;
        // ACTION_T_REMOVEAURASFROMSPELL                    = 28
        struct
        {
            uint32 target;
            uint32 spellId;
        } remove_aura;
        // ACTION_T_RANGED_MOVEMENT                         = 29
        struct
        {
            uint32 distance;
            int32  angle;
        } ranged_movement;
        // ACTION_T_RANDOM_PHASE                            = 30
        struct
        {
            uint32 phase1;
            uint32 phase2;
            uint32 phase3;
        } random_phase;
        // ACTION_T_RANDOM_PHASE_RANGE                      = 31
        struct
        {
            uint32 phaseMin;
            uint32 phaseMax;
        } random_phase_range;
        // ACTION_T_SUMMON_ID                               = 32
        struct
        {
            uint32 creatureId;
            uint32 target;
            uint32 spawnId;
        } summon_id;
        // ACTION_T_KILLED_MONSTER                          = 33
        struct
        {
            uint32 creatureId;
            uint32 target;
        } killed_monster;
        // ACTION_T_SET_INST_DATA                           = 34
        struct
        {
            uint32 field;
            uint32 value;
        } set_inst_data;
        // ACTION_T_SET_INST_DATA64                         = 35
        struct
        {
            uint32 field;
            uint32 target;
        } set_inst_data64;
        // ACTION_T_UPDATE_TEMPLATE                         = 36
        struct
        {
            uint32 creatureId;
        } update_template;
        // ACTION_T_CALL_FOR_HELP                           = 39
        struct
        {
            uint32 radius;
        } call_for_help;
        // ACTION_T_SET_SHEATH                              = 40
        struct
        {
            uint32 sheath;
        } set_sheath;
        // ACTION_T_FORCE_DESPAWN                           = 41
        struct
        {
            uint32 msDelay;
        } forced_despawn;
        // ACTION_T_SET_DEATH_PREVENTION                    = 42
        struct
        {
            uint32 state;
        } deathPrevention;
        // ACTION_T_MOUNT_TO_ENTRY_OR_MODEL                 = 43
        struct
        {
            uint32 creatureId;                              // set one from fields (or 0 for both to dismount)
            uint32 modelId;
        } mount;
        // ACTION_T_CHANCED_TEXT                            = 44
        struct
        {
            uint32 chance;
            int32 TextId[2];
        } chanced_text;
        // ACTION_T_THROW_AI_EVENT                          = 45
        struct
        {
            uint32 eventType;
            uint32 radius;
            uint32 target;
        } throwEvent;
        // ACTION_T_SET_THROW_MASK                          = 46
        struct
        {
            uint32 eventTypeMask;
            uint32 unused1;
            uint32 unused2;
        } setThrowMask;
        // ACTION_T_SET_STAND_STATE                         = 47
        struct
        {
            uint32 standState;
            uint32 unused1;
            uint32 unused2;
        } setStandState;
        // ACTION_T_CHANGE_MOVEMENT                         = 48
        struct
        {
            uint32 movementType;
            uint32 wanderORpathID;
            uint32 flags;
        } changeMovement;
        // ACTION_T_REUSE                                   = 49
        struct
        {
            uint32 unused1;
            uint32 unused2;
            uint32 unused3;
        } reuse;
        // ACTION_T_SET_REACT_STATE                         = 50
        struct
        {
            uint32 reactState;
            uint32 unused1;
            uint32 unused2;
        } setReactState;
        // ACTION_T_PAUSE_WAYPOINTS                         = 51
        struct
        {
            uint32 doPause;                                 // bool: 1 = on; 0 = off
            uint32 unused1;
        } pauseWaypoint;
        // ACTION_T_INTERRUPT_SPELL                         = 52
        struct
        {
            uint32 currentSpellType;                        // enum CurrentSpellTypes
            uint32 unused1;
            uint32 unused2;
        } interruptSpell;
        // ACTION_T_START_RELAY_SCRIPT                      = 53
        struct
        {
            int32 relayId;                                 // dbscript_on_relay id
            uint32 target;                                 // target
            uint32 unused;
        } relayScript;
        // ACTION_T_TEXT_NEW                                = 54
        struct
        {
            int32 textId;                                  // text id
            uint32 target;                                 // target
            uint32 templateId;                             // random template Id
        } textNew;
        // ACTION_T_ATTACK_START                            = 55
        struct
        {
            uint32 target;                                 // target
            uint32 unused;
            uint32 unused2;
        } attackStart;
        // ACTION_T_DESPAWN_GUARDIANS                       = 56
        struct
        {
            uint32 entryId;                                // guardian Entry
            uint32 unused;
            uint32 unused2;
        } despawnGuardians;
        // ACTION_T_SET_RANGED_MODE
        struct
        {
            uint32 type;                                    // enum RangeModeType
            uint32 chaseDistance;                           // distance at which the AI will chase
            uint32 unused;                                  // unused
        } rangedMode;
        // ACTION_T_SET_WALK
        struct
        {
            WalkSetting type;                               // enum RangeModeType
        } walkSetting;
        // ACTION_T_SET_FACING
        struct
        {
            uint32 target;                                  // Target
            uint32 reset;                                   // 0 - set, 1 - reset
        } setFacing;
        // ACTION_T_SET_SPELL_SET
        struct
        {
            uint32 setId;                                   // creature_template_spells setId
        } spellSet;
        // ACTION_T_SET_IMMOBILIZED_STATE
        struct
        {
            uint32 apply;
            uint32 combatOnly;
        } immobilizedState;
        // ACTION_T_SET_DESPAWN_AGGREGATION
        struct
        {
            uint32 mask;
            uint32 entry;
            uint32 entry2;
        } despawnAggregation;
        // ACTION_T_SET_IMMUNITY_SET
        struct
        {
            uint32 setId;
        } immunitySet;
        // ACTION_T_SET_FOLLOW_MOVEMENT
        struct
        {
            uint32 state;
        } followMovement;
        // ACTION_T_RETREAT - no params at this time
        struct
        {
            uint32 setId;
            uint32 stepIndex;
            uint32 repeatAndOocFlags;
        } startActionSet;
        // ACTION_T_START_ACTION_SET
        // RAW
        struct
        {
            uint32 param1;
            uint32 param2;
            uint32 param3;
        } raw;
    };
};

struct CreatureEventAI_Event
{
    uint32 event_id;

    uint32 creature_id;
    uint32 creature_guid;

    uint32 event_inverse_phase_mask;

    EventAI_Type event_type : 16;
    uint8 event_chance : 8;
    uint32 event_flags;

    union
    {
        // EVENT_T_TIMER_IN_COMBAT                          = 0
        // EVENT_T_TIMER_OOC                                = 1
        // EVENT_T_TIMER_GENERIC                            = 29
        struct
        {
            uint32 initialMin;
            uint32 initialMax;
            uint32 repeatMin;
            uint32 repeatMax;
        } timer;
        // EVENT_T_HP                                       = 2
        // EVENT_T_MANA                                     = 3
        // EVENT_T_TARGET_HP                                = 12
        // EVENT_T_TARGET_MANA                              = 18
        // EVENT_T_ENERGY                                   = 31
        struct
        {
            uint32 percentMax;
            uint32 percentMin;
            uint32 repeatMin;
            uint32 repeatMax;
            uint32 allowOutOfCombat;
        } percent_range;
        // EVENT_T_KILL                                     = 5
        struct
        {
            uint32 repeatMin;
            uint32 repeatMax;
            uint32 playerOnly;
        } kill;
        // EVENT_T_DEATH                                    = 6
        struct
        {
            uint32 conditionId;
        } death;
        // EVENT_T_SPELLHIT                                 = 8
        struct
        {
            uint32 spellId;
            uint32 schoolMask;                              // -1 (==0xffffffff) is ok value for full mask, or must be more limited mask like (0 < 1) = 1 for normal/physical school
            uint32 repeatMin;
            uint32 repeatMax;
        } spell_hit;
        // EVENT_T_RANGE                                    = 9
        struct
        {
            uint32 minDist;
            uint32 maxDist;
            uint32 repeatMin;
            uint32 repeatMax;
        } range;
        // EVENT_T_OOC_LOS                                  = 10
        struct
        {
            uint32 noHostile;
            uint32 maxRange;
            uint32 repeatMin;
            uint32 repeatMax;
            uint32 playerOnly;
            uint32 conditionId;
        } ooc_los;
        // EVENT_T_SPAWNED                                  = 11
        struct
        {
            uint32 condition;
            uint32 conditionValue1;
        } spawned;
        // EVENT_T_TARGET_CASTING                           = 13
        struct
        {
            uint32 repeatMin;
            uint32 repeatMax;
        } target_casting;
        // EVENT_T_FRIENDLY_HP                              = 14
        struct
        {
            uint32 hpDeficit;
            uint32 radius;
            uint32 repeatMin;
            uint32 repeatMax;
            uint32 isPercent;
        } friendly_hp;
        // EVENT_T_FRIENDLY_IS_CC                           = 15
        struct
        {
            uint32 dispelType;                              // unused ?
            uint32 radius;
            uint32 repeatMin;
            uint32 repeatMax;
        } friendly_is_cc;
        // EVENT_T_FRIENDLY_MISSING_BUFF                    = 16
        struct
        {
            uint32 spellId;
            uint32 radius;
            uint32 repeatMin;
            uint32 repeatMax;
            uint32 flags;
        } friendly_buff;
        // EVENT_T_SUMMONED_UNIT                            = 17
        // EVENT_T_SUMMONED_JUST_DIED                       = 25
        // EVENT_T_SUMMONED_JUST_DESPAWN                    = 26
        struct
        {
            uint32 creatureId;
            uint32 repeatMin;
            uint32 repeatMax;
        } summoned;
        // EVENT_T_QUEST_ACCEPT                             = 19
        // EVENT_T_QUEST_COMPLETE                           = 20
        struct
        {
            uint32 questId;
        } quest;
        // EVENT_T_RECEIVE_EMOTE                            = 22
        struct
        {
            uint32 emoteId;
            uint32 conditionId;
        } receive_emote;
        // EVENT_T_AURA                                     = 23
        // EVENT_T_TARGET_AURA                              = 24
        // EVENT_T_MISSING_AURA                             = 27
        // EVENT_T_TARGET_MISSING_AURA                      = 28
        struct
        {
            uint32 spellId;
            uint32 amount;
            uint32 repeatMin;
            uint32 repeatMax;
        } buffed;
        // EVENT_T_RECEIVE_AI_EVENT                         = 30
        struct
        {
            uint32 eventType;                               // See UnitAI.h enum AIEventType - Receive only events of this type
            uint32 senderEntry;                             // Optional npc from only whom this event can be received
            uint32 unused1;
            uint32 unused2;
        } receiveAIEvent;
        // EVENT_T_SELECT_ATTACKING_TARGET                  = 32
        struct
        {
            uint32 minRange;
            uint32 maxRange;
            uint32 repeatMin;
            uint32 repeatMax;
        } selectTarget;
        // EVENT_T_FACING_TARGET                            = 33
        struct
        {
            uint32 backOrFront;
            uint32 unused;
            uint32 repeatMin;
            uint32 repeatMax;
        } facingTarget;
        // EVENT_T_SPELLHIT_TARGET                          = 34
        struct
        {
            uint32 spellId;
            uint32 schoolMask;                              // -1 (==0xffffffff) is ok value for full mask, or must be more limited mask like (0 < 1) = 1 for normal/physical school
            uint32 repeatMin;
            uint32 repeatMax;
        } spell_hit_target;
        // EVENT_T_DEATH_PREVENTED                          = 35
        struct
        {
            uint32 unused;
        } deathPrevented;
        // EVENT_T_TARGET_NOT_REACHABLE                     = 36
        struct
        {
            uint32 unused;
        } unreachable;
        // EVENT_T_SPELL_CAST                               = 37
        struct
        {
            uint32 spellId;
        } spellCast;
        // EVENT_T_ACTION_SET                               = 38
        struct
        {
            uint32 setId;
            uint32 stepIndex;
            uint32 timeMs;
        } actionSet;
        // EVENT_T_MOVEMENT_INFORM                          = 39
        struct
        {
            uint32 movementType;
            uint32 pointId;
            uint32 pathId;
        } movementInform;
        // RAW
        struct
        {
            uint32 params[6];
        } raw;
    };

    CreatureEventAI_Action action[MAX_ACTIONS];
};

struct CreatureEventAI_EventComputedData
{
    union
    {
        // EVENT_T_FRIENDLY_HP
        struct
        {
            bool targetSelf;
        } friendlyHp;
    };
};

#define AIEVENT_DEFAULT_THROW_RADIUS    30.0f

// Event_Map
typedef std::vector<CreatureEventAI_Event> CreatureEventAI_Event_Vec;
typedef std::unordered_map<uint32, CreatureEventAI_Event_Vec> CreatureEventAI_Event_Map;
typedef std::unordered_map<uint32, CreatureEventAI_EventComputedData> CreatureEventAI_EventComputedData_Map;

struct CreatureEventAI_Summon
{
    uint32 id;

    float position_x;
    float position_y;
    float position_z;
    float orientation;
    uint32 SpawnTimeSecs;
};

// EventSummon_Map
typedef std::unordered_map<uint32, CreatureEventAI_Summon> CreatureEventAI_Summon_Map;

// 单个事件运行期实例持有器：记录已激活状态、倒计时及事件筛选出的专属目标
struct CreatureEventAIHolder
{
    CreatureEventAIHolder(CreatureEventAI_Event p) : event(p), timer(0), enabled(true), inProgress(false), eventTarget(nullptr) {}

    CreatureEventAI_Event event;                            // 静态事件定义拷贝
    uint32 timer;                                           // 剩余触发倒计时（毫秒）
    bool enabled;                                           // 事件是否启用
    bool inProgress;                                        // 防重入标记：事件当前是否正处于待执行队列中

    Unit* eventTarget; // Target filled on specific event to be used in action (特定事件填充的上下文目标，供 TARGET_T_EVENT_SPECIFIC 使用)

    // helper (在 repeatMin ~ repeatMax 间随机刷新重复计时器)
    bool UpdateRepeatTimer(Creature* creature, uint32 repeatMin, uint32 repeatMax);
};

// 数据库驱动的事件型生物 AI：支持基于阶段、计时器、血量/法力阈值、视野、光环及 AI 事件的 3 动作脚本调度
class CreatureEventAI : public CreatureAI
{
    public:
        explicit CreatureEventAI(Creature* creature);
        ~CreatureEventAI() override
        {
            m_CreatureEventAIList.clear();
        }
        void InitAI();                                                  // 从全局容器拷贝匹配当前生物 Entry/GUID 及地图难度的 EventAI 事件列表

        void GetAIInformation(ChatHandler& reader) override;            // 输出当前阶段、移动/近战状态与全部事件调试信息

        void JustRespawned() override;                                  // 重生时重新初始化事件并触发 EVENT_T_SPAWNED
        void Reset() override;                                          // 重置非战斗状态变量与非战斗计时器
        void JustReachedHome() override;                                // 返回出生点到达时触发 EVENT_T_REACHED_HOME
        void EnterCombat(Unit* enemy) override;                         // 进入战斗触发 EVENT_T_AGGRO 并重置战斗计时器
        void EnterEvadeMode() override;                                 // 脱离战斗触发 EVENT_T_EVADE 并清理聚合召唤物
        void JustDied(Unit* killer) override;                           // 死亡触发 EVENT_T_DEATH 并可选广播 AI_EVENT_JUST_DIED
        void KilledUnit(Unit* victim) override;                         // 击杀目标时触发 EVENT_T_KILL
        void JustSummoned(Creature* summoned) override;                 // 召唤生物时触发 EVENT_T_SUMMONED_UNIT 并登记聚合销毁列表
        // void AttackStart(Unit* who) override;
        void MoveInLineOfSight(Unit* who) override;                     // 处理脱战视线触发事件 (EVENT_T_OOC_LOS) 后转交基类索敌
        void SpellHit(Unit* unit, const SpellEntry* spellInfo) override; // 被法术命中时触发 EVENT_T_SPELLHIT
        void SpellHitTarget(Unit* target, const SpellEntry* spell) override; // 法术命中目标时触发 EVENT_T_SPELLHIT_TARGET
        void DamageTaken(Unit* dealer, uint32& damage, DamageEffectType damagetype, SpellEntry const* spellInfo) override; // 按 90%/50%/10% 血量阶梯自动抛出 AI 事件
        void JustPreventedDeath(Unit* attacker) override;               // 触发免死锁血时触发 EVENT_T_DEATH_PREVENTED
        void HealedBy(Unit* healer, uint32& healedAmount) override;     // 回满血时发送 AI_EVENT_GOT_FULL_HEALTH 事件
        void ReceiveEmote(Player* player, uint32 textEmote) override;   // 收到表情动作时触发 EVENT_T_RECEIVE_EMOTE
        void SummonedCreatureJustDied(Creature* summoned) override;     // 召唤物死亡时触发 EVENT_T_SUMMONED_JUST_DIED
        void SummonedCreatureDespawn(Creature* summoned) override;      // 召唤物消失时触发 EVENT_T_SUMMONED_JUST_DESPAWN
        void ReceiveAIEvent(AIEventType eventType, Unit* sender, Unit* invoker, uint32 miscValue) override; // 收到 AI 事件时触发 EVENT_T_RECEIVE_AI_EVENT
        void CorpseRemoved(uint32& respawnDelay) override;              // 尸体移除时清理聚合召唤物
        void OnSpellCast(SpellEntry const* spellInfo, Unit* target) override; // 施放法术时触发 EVENT_T_SPELL_CAST
        // bool IsControllable() const override { return true; }

        static int Permissible(const Creature* creature);               // 判定生物是否适用 EventAI

        virtual void UpdateEventTimers(const uint32 diff) override;     // 按 500ms 步长递减计时器并检查定时类事件与动作集序列
        void MovementInform(uint32 motionType, uint32 pointId) override; // 到达移动点时触发 EVENT_T_MOVEMENT_INFORM
        void ProcessEvents(Unit* actionInvoker = nullptr, Unit* AIEventSender = nullptr); // 批量执行当前递归深度收集到的就绪事件
        bool CheckEvent(CreatureEventAIHolder& holder, Unit* actionInvoker = nullptr, Unit* AIEventSender = nullptr); // 校验事件阶段掩码、近远程模式及触发条件
        void ResetEvent(CreatureEventAIHolder& holder);                 // 刷新重复事件计时器或禁用一次性事件
        bool CheckAndReadyEventForExecution(CreatureEventAIHolder& holder, Unit* actionInvoker = nullptr, Unit* AIEventSender = nullptr); // 检查并压入待执行队列
        void IncreaseDepthIfNecessary() { if (m_depth >= m_creatureEventAITempList.size()) m_creatureEventAITempList.resize(m_depth + 1); } // 按递归深度扩容事件暂存队列
        virtual bool ProcessEvent(CreatureEventAIHolder& holder, Unit* actionInvoker = nullptr, Unit* AIEventSender = nullptr); // 按概率掷骰并执行事件的 3 个动作（或随机 1 个）
        virtual bool ProcessAction(CreatureEventAI_Action const& action, uint32 rnd, uint32 eventId, Unit* actionInvoker, Unit* AIEventSender, Unit* eventTarget); // 执行单个 EventAI 动作指令
        inline uint32 GetRandActionParam(uint32 rnd, uint32 param1, uint32 param2, uint32 param3) const;
        inline int32 GetRandActionParam(uint32 rnd, int32 param1, int32 param2, int32 param3) const;
        /// If the bool& param is true, an error should be reported (按 Target 枚举解析动作目标，找不到合法目标时置 isError=true)
        inline Unit* GetTargetByType(uint32 target, Unit* actionInvoker, Unit* AIEventSender, Unit* eventTarget, bool& isError, uint32 forSpellId = 0, uint32 selectFlags = 0) const;

        bool SpawnedEventConditionsCheck(CreatureEventAI_Event const& event) const; // 校验 EVENT_T_SPAWNED 的地图/区域条件

        MovementGeneratorType GetDefaultMovement() { return m_defaultMovement; }
    protected:
        std::string GetAIName() override { return "EventAI"; }
        // Event rules specifiers (事件分类判定辅助函数)
        bool IsTimerExecutedEvent(EventAI_Type type) const;             // 是否在 UpdateEventTimers 中周期轮询触发
        bool IsRepeatableEvent(EventAI_Type type) const;                // 是否允许按 EFLAG_REPEATABLE 控制重复性
        bool IsTimerBasedEvent(EventAI_Type type) const;                // 是否拥有 repeatMin/repeatMax 冷却计时器

        uint32 m_EventUpdateTime;                           // Time between event updates (距离下次轮询剩余时间)
        uint32 m_EventDiff;                                 // Time between the last event call (上次轮询以来累计经过时间)
        bool   m_bEmptyList;

        // Variables used by Events themselves (事件列表与重入栈队列)
        typedef std::vector<CreatureEventAIHolder> CreatureEventAIList;
        CreatureEventAIList m_CreatureEventAIList;          // Holder for events (stores enabled, time, and eventid) (当前生物实例的事件列表)
        std::vector<std::vector<std::reference_wrapper<CreatureEventAIHolder>>> m_creatureEventAITempList; // Holder for events that are ready to go off (按调用深度分层暂存待执行事件，防止事件触发事件时迭代器失效)
        uint32 m_depth;                                     // 当前事件处理递归深度

        uint8  m_Phase;                                     // Current phase, max 32 phases (当前 AI 阶段编号 0..31)
        bool   m_HasOOCLoSEvent;                            // Cache if a OOC-LoS Event exists (是否包含脱战视线事件缓存)
        uint32 m_InvinceabilityHpLevel;                     // Minimal health level allowed at damage apply (免死保留最小生命值)

        uint32 m_throwAIEventMask;                          // Automatically throw AIEvents that are encoded into this mask (自动抛出 AI 事件的掩码)
        // Note that Step 100 means that AI_EVENT_GOT_FULL_HEALTH was sent
        // Steps 0..2 correspond to AI_EVENT_LOST_SOME_HEALTH(90%), AI_EVENT_LOST_HEALTH(50%), AI_EVENT_CRITICAL_HEALTH(10%)
        uint32 m_throwAIEventStep;                          // Used for damage taken/ received heal (血量阶梯事件当前进度)
        float m_LastSpellMaxRange;                          // Maximum spell range that was cast during dynamic movement (动态移动期间施放法术的最大射程)

        uint32 m_despawnAggregationMask;                    // 召唤物聚合销毁掩码
        std::set<uint32> m_entriesForDespawn;               // 需聚合销毁的召唤生物 Entry 过滤集合
        GuidVector m_despawnGuids;                          // 已登记待销毁的召唤生物 GUID 列表

        uint32 m_timer; // EVENT_T_ACTION_SET (动作集当前累计计时)
        int32 m_setId;                                      // 当前激活的动作集 ID（-1 表示未激活）
        uint32 m_maxIndex;                                  // 当前动作集最大步骤索引
        uint32 m_index;                                     // 当前动作集执行步骤索引
        bool m_repeat;                                      // 动作集是否循环执行
        bool m_setOocOnly;                                  // 动作集是否仅在脱战时推进计时

        MovementGeneratorType m_defaultMovement; // TODO: Extend to all of AI (默认移动生成器类型)
};

#endif