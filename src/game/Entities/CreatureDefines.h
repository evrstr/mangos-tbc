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

#ifndef MANGOSSERVER_CREATURE_DEFINES_H
#define MANGOSSERVER_CREATURE_DEFINES_H

#include "Common.h"
#include "Globals/EnumFlag.h"

// keeping all naming since its known and does not hurt (保留原始命名以保持与暴雪服务端及数据库定义一致)

// 生物静态标志位 1（对应 creature_template 中的 StaticFlags1）
enum class CreatureStaticFlags : uint32
{
    MOUNTABLE                   = 0x00000001, // 可骑乘
    NO_XP                       = 0x00000002, // 击杀不提供经验值
    NO_LOOT                     = 0x00000004, // 无掉落
    UNKILLABLE                  = 0x00000008, // 不可被击杀（生命不会归零死亡）
    TAMEABLE                    = 0x00000010, // CREATURE_TYPE_FLAG_TAMEABLE (可被猎人驯服)
    IMMUNE_TO_PC                = 0x00000020, // UNIT_FLAG_IMMUNE_TO_PC (对玩家角色免疫)
    IMMUNE_TO_NPC               = 0x00000040, // UNIT_FLAG_IMMUNE_TO_NPC (对 NPC 免疫)
    CAN_WIELD_LOOT              = 0x00000080, // 可装备掉落武器
    SESSILE                     = 0x00000100, // creature_template_movement.Rooted = 1 (定身/不可移动)
    UNINTERACTIBLE              = 0x00000200, // UNIT_FLAG_UNINTERACTIBLE (不可交互)
    NO_AUTOMATIC_REGEN          = 0x00000400, // Creatures with that flag uses no UNIT_FLAG2_REGENERATE_POWER (不自动回复生命/能量)
    DESPAWN_INSTANTLY           = 0x00000800, // Creature instantly disappear when killed (死亡时立即消失)
    CORPSE_RAID                 = 0x00001000, // 团队副本尸体规则
    CREATOR_LOOT                = 0x00002000, // Lootable only by creator(engineering dummies) (仅召唤/制造者可拾取，如工程假人)
    NO_DEFENSE                  = 0x00004000, // 无防御/躲闪招架属性
    NO_SPELL_DEFENSE            = 0x00008000, // 无抗性防御
    BOSS_MOB                    = 0x00010000, // CREATURE_TYPE_FLAG_BOSS_MOB, original description: Raid Boss Mob (团队首领级怪物)
    COMBAT_PING                 = 0x00020000, // 战斗时发送小地图提示脉冲
    AQUATIC                     = 0x00040000, // aka Water Only, creature_template_movement.Ground = 0 (仅限水中活动)
    AMPHIBIOUS                  = 0x00080000, // creature_template_movement.Swim = 1 (水陆两栖可游泳)
    NO_MELEE_FLEE               = 0x00100000, // Prevents melee and makes npc flee if it enters combat - also misused to just prevent melee on npcs that cant move (禁止近战并在入战时逃跑，也常用于禁止不能移动的 NPC 近战)
    VISIBLE_TO_GHOSTS           = 0x00200000, // CREATURE_TYPE_FLAG_VISIBLE_TO_GHOSTS (对灵魂状态玩家可见)
    PVP_ENABLING                = 0x00400000, // Old UNIT_FLAG_PVP_ENABLING, now UNIT_BYTES_2_OFFSET_PVP_FLAG from UNIT_FIELD_BYTES_2 (开启 PvP 标记)
    DO_NOT_PLAY_WOUND_ANIM      = 0x00800000, // CREATURE_TYPE_FLAG_DO_NOT_PLAY_WOUND_ANIM (不播放受击动画)
    NO_FACTION_TOOLTIP          = 0x01000000, // CREATURE_TYPE_FLAG_NO_FACTION_TOOLTIP (鼠标提示不显示阵营)
    IGNORE_COMBAT               = 0x02000000, // Actually only changes react state to passive (忽略战斗，实际将反应状态设为被动)
    ONLY_ATTACK_PVP_ENABLING    = 0x04000000, // "Only attack targets that are PvP enabling" (仅攻击开启 PvP 的目标)
    CALLS_GUARDS                = 0x08000000, // Creature will summon a guard if player is within its aggro range (even if creature doesn't attack per se) (玩家进入仇恨范围时召唤卫兵)
    CAN_SWIM                    = 0x10000000, // UnitFlags 0x8000 UNIT_FLAG_CAN_SWIM (允许游泳)
    FLOATING                    = 0x20000000, // creature_template_movement.Flight = 1 (悬浮/飞行移动)
    MORE_AUDIBLE                = 0x40000000, // CREATURE_TYPE_FLAG_MORE_AUDIBLE (音效衰减距离更大)
    LARGE_AOI                   = 0x80000000  // UnitFlags2 0x200000 (大视野范围 AOI)
};

DEFINE_ENUM_FLAG(CreatureStaticFlags);

// 生物静态标志位 2（对应 StaticFlags2）
enum class CreatureStaticFlags2 : uint32
{
    NO_PET_SCALING                      = 0x00000001, // 宠物属性不随主人缩放
    FORCE_PARTY_MEMBERS_INTO_COMBAT     = 0x00000002, // Original description: Force Raid Combat (强制小队/团队成员全部入战)
    LOCK_TAPPERS_TO_RAID_ON_DEATH       = 0x00000004, // "Lock Tappers To Raid On Death", toggleable by 'Set "RAID_LOCK_ON_DEATH" flag for unit(s)' action, CREATURE_FLAG_EXTRA_INSTANCE_BIND (死亡时锁定首杀者副本进度)
    SPELL_ATTACKABLE                    = 0x00000008, // CREATURE_TYPE_FLAG_SPELL_ATTACKABLE, original description(not valid anymore?): No Harmful Vertex Coloring (可被法术攻击)
    NO_CRUSHING_BLOWS                   = 0x00000010, // 不产生碾压打击
    NO_OWNER_THREAT                     = 0x00000020, // 不向召唤者/主人传递仇恨
    NO_WOUNDED_SLOWDOWN                 = 0x00000040, // 血量较低时不减速
    USE_CREATOR_BONUSES                 = 0x00000080, // 继承创建者/召唤者属性加成
    IGNORE_FEIGN_DEATH                  = 0x00000100, // 无视假死技能
    IGNORE_SANCTUARY                    = 0x00000200, // 无视圣域安全区规则
    ACTION_TRIGGERS_WHILE_CHARMED       = 0x00000400, // 被魅惑时仍触发动作脚本
    INTERACT_WHILE_DEAD                 = 0x00000800, // CREATURE_TYPE_FLAG_INTERACT_WHILE_DEAD (死亡状态下仍可交互)
    NO_INTERRUPT_SCHOOL_COOLDOWN        = 0x00001000, // 被打断后不触发同系法术锁定冷却
    RETURN_SOUL_SHARD_TO_MASTER_OF_PET  = 0x00002000, // 宠物消失时向主人返还灵魂碎片
    SKIN_WITH_HERBALISM                 = 0x00004000, // CREATURE_TYPE_FLAG_SKIN_WITH_HERBALISM (死亡后可用草药学采集)
    SKIN_WITH_MINING                    = 0x00008000, // CREATURE_TYPE_FLAG_SKIN_WITH_MINING (死亡后可用采矿采集)
    ALERT_CONTENT_TEAM_ON_DEATH         = 0x00010000,
    ALERT_CONTENT_TEAM_AT_90_PCT_HP     = 0x00020000,
    ALLOW_MOUNTED_COMBAT                = 0x00040000, // CREATURE_TYPE_FLAG_ALLOW_MOUNTED_COMBAT (允许骑乘战斗)
    PVP_ENABLING_OOC                    = 0x00080000, // 脱战时即开启 PvP
    NO_DEATH_MESSAGE                    = 0x00100000, // CREATURE_TYPE_FLAG_NO_DEATH_MESSAGE (死亡时不发送死亡消息)
    IGNORE_PATHING_FAILURE              = 0x00200000, // 忽略寻路失败
    FULL_SPELL_LIST                     = 0x00400000, // 使用完整法术列表
    DOES_NOT_REDUCE_REPUTATION_FOR_RAIDS = 0x00800000, // 团队模式下不降低声望获取
    IGNORE_MISDIRECTION                 = 0x01000000, // 忽略误导效果
    HIDE_BODY                           = 0x02000000, // UNIT_FLAG2_HIDE_BODY (隐藏模型躯体)
    SPAWN_DEFENSIVE                     = 0x04000000, // 刷新时处于防御姿态
    SERVER_ONLY                         = 0x08000000, // 仅服务端逻辑对象，不下发客户端
    CAN_SAFE_FALL                       = 0x10000000, // Original description: No Collision (免疫坠落伤害)
    CAN_ASSIST                          = 0x20000000, // CREATURE_TYPE_FLAG_CAN_ASSIST, original description: Player Can Heal/Buff (玩家可对其治疗/增益，常见于护送 NPC)
    NO_SKILL_GAINS                      = 0x40000000, // 攻击该生物不提升武器/防御技能熟练度
    NO_PET_BAR                          = 0x80000000  // CREATURE_TYPE_FLAG_NO_PET_BAR (不显示宠物动作条)
};

DEFINE_ENUM_FLAG(CreatureStaticFlags2);

// 生物静态标志位 3（对应 StaticFlags3）
enum class CreatureStaticFlags3 : uint32
{
    NO_DAMAGE_HISTORY              = 0x00000001, // 不记录伤害历史
    DONT_PVP_ENABLE_OWNER          = 0x00000002, // 不连带开启主人 PvP 标记
    DO_NOT_FADE_IN                 = 0x00000004, // UNIT_FLAG2_DO_NOT_FADE_IN (刷新时不淡入)
    MASK_UID                       = 0x00000008, // CREATURE_TYPE_FLAG_MASK_UID, original description: Non-Unique In Combat Log (战斗日志中隐藏唯一低位 GUID)
    SKIN_WITH_ENGINEERING          = 0x00000010, // CREATURE_TYPE_FLAG_SKIN_WITH_ENGINEERING (死亡后可用工程学拆解)
    NO_AGGRO_ON_LEASH              = 0x00000020, // 脱战拉回（Leash）时不产生仇恨
    NO_FRIENDLY_AREA_AURAS         = 0x00000040, // 不受友方区域光环影响
    EXTENDED_CORPSE_DURATION       = 0x00000080, // 延长尸体保留时间
    CANNOT_SWIM                    = 0x00000100, // UNIT_FLAG_CANNOT_SWIM (禁止游泳)
    TAMEABLE_EXOTIC                = 0x00000200, // CREATURE_TYPE_FLAG_TAMEABLE_EXOTIC (奇珍异兽，需特殊天赋驯服)
    GIGANTIC_AOI                   = 0x00000400, // Since MoP, creatures with that flag have UnitFlags2 0x400000 (超大视野范围 AOI)
    INFINITE_AOI                   = 0x00000800, // Since MoP, creatures with that flag have UnitFlags2 0x40000000 (全地图无限视野范围 AOI)
    CANNOT_PENETRATE_WATER         = 0x00001000, // Waterwalking (水上行走)
    NO_NAME_PLATE                  = 0x00002000, // CREATURE_TYPE_FLAG_NO_NAME_PLATE (不显示姓名板)
    CHECKS_LIQUIDS                 = 0x00004000, // 检测液体地形
    NO_THREAT_FEEDBACK             = 0x00008000, // 不发送仇恨反馈
    USE_MODEL_COLLISION_SIZE       = 0x00010000, // CREATURE_TYPE_FLAG_USE_MODEL_COLLISION_SIZE (使用模型碰撞体积)
    ATTACKER_IGNORES_FACING        = 0x00020000, // In 3.3.5 used only by Rocket Propelled Warhead (攻击者忽略朝向限制)
    ALLOW_INTERACTION_WHILE_IN_COMBAT = 0x00040000, // CREATURE_TYPE_FLAG_ALLOW_INTERACTION_WHILE_IN_COMBAT (战斗中仍可交互)
    SPELL_CLICK_FOR_PARTY_ONLY     = 0x00080000, // 仅同小队成员可触发 SpellClick
    FACTION_LEADER                 = 0x00100000, // 阵营领袖
    IMMUNE_TO_PLAYER_BUFFS         = 0x00200000, // 免疫玩家增益法术
    COLLIDE_WITH_MISSILES          = 0x00400000, // CREATURE_TYPE_FLAG_COLLIDE_WITH_MISSILES (与投射物发生碰撞)
    CAN_BE_MULTITAPPED             = 0x00800000, // Original description: Do Not Tap (Credit to threat list) (支持多阵营/多玩家共享击杀归属)
    DO_NOT_PLAY_MOUNTED_ANIMATIONS = 0x01000000, // CREATURE_TYPE_FLAG_DO_NOT_PLAY_MOUNTED_ANIMATIONS, original description: Disable Dodge, Parry and Block Animations (禁用骑乘或格挡闪避动画)
    CANNOT_TURN                    = 0x02000000, // UNIT_FLAG2_CANNOT_TURN (禁止转向)
    ENEMY_CHECK_IGNORES_LOS        = 0x04000000, // 索敌检查忽略视线遮挡 (LoS)
    FOREVER_CORPSE_DURATION        = 0x08000000, // 7 days (尸体保留极长时间，约 7 天)
    PETS_ATTACK_WITH_3D_PATHING    = 0x10000000, // "Pets attack with 3d pathing (Kologarn)" (宠物使用 3D 寻路攻击)
    LINK_ALL                       = 0x20000000, // CREATURE_TYPE_FLAG_LINK_ALL
    AI_CAN_AUTO_TAKEOFF_IN_COMBAT  = 0x40000000, // AI 在战斗中可自动起飞
    AI_CAN_AUTO_LAND_IN_COMBAT     = 0x80000000  // AI 在战斗中可自动降落
};

DEFINE_ENUM_FLAG(CreatureStaticFlags3);

// 生物静态标志位 4（对应 StaticFlags4）
enum class CreatureStaticFlags4 : uint32
{
    NO_BIRTH_ANIM                            = 0x00000001, // SMSG_UPDATE_OBJECT's "NoBirthAnim" (无出生刷新动画)
    TREAT_AS_PLAYER_FOR_DIMINISHING_RETURNS  = 0x00000002, // Primarily used by ToC champions (按玩家规则计算控制递减 DR)
    TREAT_AS_PLAYER_FOR_PVP_DEBUFF_DURATION  = 0x00000004, // Primarily used by ToC champions (按 PvP 规则计算减益持续时间)
    INTERACT_ONLY_WITH_CREATOR               = 0x00000008, // CREATURE_TYPE_FLAG_INTERACT_ONLY_WITH_CREATOR, original description: Only Display Gossip for Summoner (仅召唤者可交互对话)
    DO_NOT_PLAY_UNIT_EVENT_SOUNDS            = 0x00000010, // CREATURE_TYPE_FLAG_DO_NOT_PLAY_UNIT_EVENT_SOUNDS, original description: No Death Scream (不播放单位事件音效)
    HAS_NO_SHADOW_BLOB                       = 0x00000020, // CREATURE_TYPE_FLAG_HAS_NO_SHADOW_BLOB, original description(wrongly linked type flag or behavior was changed?): Can be Healed by Enemies (不渲染地面阴影圆斑)
    DEALS_TRIPLE_DAMAGE_TO_PC_CONTROLLED_PETS = 0x00000040, // 对玩家控制的宠物造成三倍伤害
    NO_NPC_DAMAGE_BELOW_85PTC                = 0x00000080, // 其他 NPC 无法将其血量打到 85% 以下
    OBEYS_TAUNT_DIMINISHING_RETURNS          = 0x00000100, // CREATURE_FLAG_EXTRA_OBEYS_TAUNT_DIMINISHING_RETURNS (受嘲讽递减影响)
    NO_MELEE_APPROACH                        = 0x00000200, // 禁止主动近战贴近目标
    UPDATE_CREATURE_RECORD_WHEN_INSTANCE_CHANGES_DIFFICULTY = 0x00000400, // Used only by Snobold Vassal (副本切换难度时更新生物记录)
    CANNOT_DAZE                              = 0x00000800, // "Cannot Daze (Combat Stun)" (无法对目标造成眩晕)
    FLAT_HONOR_AWARD                         = 0x00001000, // 固定荣誉奖励
    IGNORE_LOS_WHEN_CASTING_ON_ME            = 0x00002000, // "Other objects can ignore line of sight requirements when casting spells on me", used only by Ice Tomb in 3.3.5 (其他对象对其施法时忽略视线检查)
    GIVE_QUEST_KILL_CREDIT_WHILE_OFFLINE     = 0x00004000, // 离线玩家也可获得任务击杀计数
    TREAT_AS_RAID_UNIT_FOR_HELPFUL_SPELLS    = 0x00008000, // CREATURE_TYPE_FLAG_TREAT_AS_RAID_UNIT, "Treat as Raid Unit For Helpful Spells (Instances ONLY)", used by Valithria Dreamwalker (有益法术视为团队成员)
    DONT_REPOSITION_IF_MELEE_TARGET_IS_TOO_CLOSE = 0x00010000, // "Don't reposition because melee target is too close" (近战目标过近时不重定位)
    PET_OR_GUARDIAN_AI_DONT_GO_BEHIND_TARGET = 0x00020000, // 宠物/守卫 AI 不绕到目标背后
    MINUTE_5_LOOT_ROLL_TIMER                 = 0x00040000, // Used by Lich King (5 分钟拾取 Roll 点计时)
    FORCE_GOSSIP                             = 0x00080000, // CREATURE_TYPE_FLAG_FORCE_GOSSIP (强制显示对话窗口)
    DONT_REPOSITION_WITH_FRIENDS_IN_COMBAT   = 0x00100000, // 战斗中不因友军拥挤而重定位
    DO_NOT_SHEATHE                           = 0x00200000, // CREATURE_TYPE_FLAG_DO_NOT_SHEATHE, original description: Manual Sheathing control (不自动收起武器)
    IGNORE_SPELL_MIN_RANGE_RESTRICTIONS      = 0x00400000, // UnitFlags2 0x8000000, original description: Attacker Ignores Minimum Ranges (忽略法术最小射程限制)
    SUPPRESS_INSTANCE_WIDE_RELEASE_IN_COMBAT = 0x00800000, // 战斗中禁止全副本范围释放灵魂
    PREVENT_SWIM                             = 0x01000000, // UnitFlags2 0x1000000, original description: AI will only swim if target swims (禁止主动下水游泳)
    HIDE_IN_COMBAT_LOG                       = 0x02000000, // UnitFlags2 0x2000000, original description: Don't generate combat log when engaged with NPC's (不在战斗日志中显示)
    ALLOW_NPC_COMBAT_WHILE_UNINTERACTIBLE    = 0x04000000, // 不可交互状态下仍允许与 NPC 战斗
    PREFER_NPCS_WHEN_SEARCHING_FOR_ENEMIES   = 0x08000000, // 索敌时优先选择 NPC
    ONLY_GENERATE_INITIAL_THREAT             = 0x10000000, // 仅产生初始仇恨
    DO_NOT_TARGET_ON_INTERACTION             = 0x20000000, // CREATURE_TYPE_FLAG_DO_NOT_TARGET_ON_INTERACTION, original description: Doesn't change target on right click (右键交互时不切换当前目标)
    DO_NOT_RENDER_OBJECT_NAME                = 0x40000000, // CREATURE_TYPE_FLAG_DO_NOT_RENDER_OBJECT_NAME, original description: Hide name in world frame (不在场景中渲染对象名称)
    QUEST_BOSS                               = 0x80000000  // CREATURE_TYPE_FLAG_QUEST_BOSS (任务首领标记)
};

DEFINE_ENUM_FLAG(CreatureStaticFlags4);

// 生物静态标志位 5（对应 StaticFlags5）
enum class CreatureStaticFlags5 : uint32
{
    UNTARGETABLE_BY_CLIENT              = 0x00000001, // UnitFlags2 0x4000000 UNIT_FLAG2_UNTARGETABLE_BY_CLIENT (客户端不可选中为目标)
    FORCE_SELF_MOUNTING                 = 0x00000002,
    UNINTERACTIBLE_IF_HOSTILE           = 0x00000004, // UnitFlags2 0x10000000 (敌对时不可交互)
    DISABLES_XP_AWARD                   = 0x00000008, // 禁用经验值奖励
    DISABLE_AI_PREDICTION               = 0x00000010,
    NO_LEAVECOMBAT_STATE_RESTORE        = 0x00000020, // 脱战时不恢复初始状态
    BYPASS_INTERACT_INTERRUPTS          = 0x00000040,
    DEGREE_BACK_ARC_240                 = 0x00000080,
    INTERACT_WHILE_HOSTILE              = 0x00000100, // UnitFlags2 0x4000 UNIT_FLAG2_INTERACT_WHILE_HOSTILE (敌对状态下仍可交互)
    DONT_DISMISS_ON_FLYING_MOUNT        = 0x00000200, // 乘坐飞行坐骑时不解散
    PREDICTIVE_POWER_REGEN              = 0x00000400, // CREATURE_TYPEFLAGS_2_UNK1
    HIDE_LEVEL_INFO_IN_TOOLTIP          = 0x00000800, // CREATURE_TYPEFLAGS_2_UNK2 (鼠标提示隐藏等级)
    HIDE_HEALTH_BAR_UNDER_TOOLTIP       = 0x00001000, // CREATURE_TYPEFLAGS_2_UNK3 (鼠标提示下方隐藏血条)
    SUPPRESS_HIGHLIGHT_WHEN_TARGETED_OR_MOUSED_OVER = 0x00002000, // UnitFlags2 0x80000 (选中或悬停时不显示高亮)
    AI_PREFER_PATHABLE_TARGETS          = 0x00004000, // AI 优先选择可寻路到达的目标
    FREQUENT_AREA_TRIGGER_CHECKS        = 0x00008000, // 提高区域触发器检测频率
    ASSIGN_KILL_CREDIT_TO_ENCOUNTER_LIST= 0x00010000,
    NEVER_EVADE                         = 0x00020000, // 永不脱战重置（Evade）
    AI_CANT_PATH_ON_STEEP_SLOPES        = 0x00040000, // AI 无法在陡坡上寻路
    AI_IGNORE_LOS_TO_MELEE_TARGET       = 0x00080000, // AI 对近战目标忽略视线检查
    NO_TEXT_IN_CHAT_BUBBLE              = 0x00100000, // "Never display emote or chat text in a chat bubble", CREATURE_TYPEFLAGS_2_UNK4 (不在头顶聊天气泡显示台词)
    CLOSE_IN_ON_UNPATHABLE_TARGET       = 0x00200000, // "AI Pets close in on unpathable target" (宠物 AI 尽量靠近不可寻路目标)
    DONT_GO_BEHIND_ME                   = 0x00400000, // "Pet/Guardian AI Don't Go Behind Me (use on target)" (宠物/守卫不绕背)
    NO_DEATH_THUD                       = 0x00800000, // CREATURE_TYPEFLAGS_2_UNK5 (无死亡倒地声效)
    CLIENT_LOCAL_CREATURE               = 0x01000000, // 客户端本地生物
    CAN_DROP_LOOT_WHILE_IN_A_CHALLENGE_MODE_INSTANCE = 0x02000000,
    HAS_SAFE_LOCATION                   = 0x04000000,
    NO_HEALTH_REGEN                     = 0x08000000, // 禁止生命值回复
    NO_POWER_REGEN                      = 0x10000000, // 禁止能量值回复
    NO_PET_UNIT_FRAME                   = 0x20000000, // 不显示宠物单位头像框体
    NO_INTERACT_ON_LEFT_CLICK           = 0x40000000, // CREATURE_TYPEFLAGS_2_UNK6 (左键点击不触发交互)
    GIVE_CRITERIA_KILL_CREDIT_WHEN_CHARMED = 0x80000000 // 被魅惑时仍授予成就/条件击杀计数
};

DEFINE_ENUM_FLAG(CreatureStaticFlags5);

// 生物静态标志位 6（对应 StaticFlags6）
enum class CreatureStaticFlags6 : uint32
{
    DO_NOT_AUTO_RESUMMON                = 0x00000001, // "Do not auto-resummon this companion creature" (不自动重新召唤该伴生生物)
    REPLACE_VISIBLE_UNIT_IF_AVAILABLE   = 0x00000002, // "Smooth Phasing: Replace visible unit if available" (平滑相位切换时替换可见单位)
    IGNORE_REALM_COALESCING_HIDING_CODE = 0x00000004, // "Ignore the realm coalescing hiding code (always show)"
    TAPS_TO_FACTION                     = 0x00000008, // 归属权按阵营共享
    ONLY_QUESTGIVER_FOR_SUMMONER        = 0x00000010, // 仅对召唤者提供任务交互
    AI_COMBAT_RETURN_PRECISE            = 0x00000020, // 脱战返位精确回到原点
    HOME_REALM_ONLY_LOOT                = 0x00000040,
    NO_INTERACT_RESPONSE                = 0x00000080, // TFLAG2_UNK7 (交互无响应动作)
    NO_INITIAL_POWER                    = 0x00000100, // 初始能量为 0
    DONT_CANCEL_CHANNEL_ON_MASTER_MOUNTING = 0x00000200, // 主人上马时不打断引导法术
    CAN_TOGGLE_BETWEEN_DEATH_AND_PERSONAL_LOOT = 0x00000400,
    ALWAYS_STAND_ON_TOP_OF_TARGET       = 0x00000800, // "Always, ALWAYS tries to stand right on top of his move to target. ALWAYS!!", toggleable by 'Set "Always Stand on Target" flag for unit(s)' or not same? (总是直接站到移动目标正上方)
    UNCONSCIOUS_ON_DEATH                = 0x00001000, // 死亡时表现为昏迷状态
    DONT_REPORT_TO_LOCAL_DEFENSE_CHANNEL_ON_DEATH = 0x00002000, // 死亡时不向本地防务频道报警
    PREFER_UNENGAGED_MONSTERS           = 0x00004000, // "Prefer unengaged monsters when picking a target" (索敌时优先选择未入战怪物)
    USE_PVP_POWER_AND_RESILIENCE        = 0x00008000, // "Use PVP power and resilience when players attack this creature" (承受玩家攻击时应用 PvP 强度与韧性)
    DONT_CLEAR_DEBUFFS_ON_LEAVE_COMBAT  = 0x00010000, // 脱战时不清除减益效果
    PERSONAL_LOOT_HAS_FULL_SECURITY     = 0x00020000, // "Personal loot has full security (guaranteed push/mail delivery)"
    TRIPLE_SPELL_VISUALS                = 0x00040000,
    USE_GARRISON_OWNER_LEVEL            = 0x00080000,
    IMMEDIATE_AOI_UPDATE_ON_SPAWN       = 0x00100000, // 刷新时立即触发 AOI 视野更新
    UI_CAN_GET_POSITION                 = 0x00200000,
    SEAMLESS_TRANSFER_PROHIBITED        = 0x00400000,
    ALWAYS_USE_GROUP_LOOT_METHOD        = 0x00800000,
    NO_BOSS_KILL_BANNER                 = 0x01000000,
    FORCE_TRIGGERING_PLAYER_LOOT_ONLY   = 0x02000000,
    SHOW_BOSS_FRAME_WHILE_UNINTERACTABLE= 0x04000000,
    SCALES_TO_PLAYER_LEVEL              = 0x08000000, // 动态缩放至玩家等级
    AI_DONT_LEAVE_MELEE_FOR_RANGED_WHEN_TARGET_GETS_ROOTED = 0x10000000, // 目标被定身时不从近战切为远程攻击
    DONT_USE_COMBAT_REACH_FOR_CHAINING  = 0x20000000, // 牵引联动计算不使用战斗攻击距离
    DO_NOT_PLAY_PROCEDURAL_WOUND_ANIM   = 0x40000000,
    APPLY_PROCEDURAL_WOUND_ANIM_TO_BASE = 0x80000000  // TFLAG2_UNK14
};

DEFINE_ENUM_FLAG(CreatureStaticFlags6);

// 生物静态标志位 7（对应 StaticFlags7）
enum class CreatureStaticFlags7 : uint32
{
    IMPORTANT_NPC                            = 0x00000001, // 重要 NPC
    IMPORTANT_QUEST_NPC                      = 0x00000002, // 重要任务 NPC
    LARGE_NAMEPLATE                          = 0x00000004, // 大号姓名板
    TRIVIAL_PET                              = 0x00000008, // 琐碎宠物（无战斗价值）
    AI_ENEMIES_DONT_BACKUP_WHEN_I_GET_ROOTED = 0x00000010, // 自身被定身时敌方 AI 不后退
    NO_AUTOMATIC_COMBAT_ANCHOR               = 0x00000020,
    ONLY_TARGETABLE_BY_CREATOR               = 0x00000040, // 仅创建者可选中为目标
    TREAT_AS_PLAYER_FOR_ISPLAYERCONTROLLED   = 0x00000080, // IsPlayerControlled 判断视为玩家
    GENERATE_NO_THREAT_OR_DAMAGE             = 0x00000100, // 不产生任何仇恨或伤害
    INTERACT_ONLY_ON_QUEST                   = 0x00000200, // 仅在相关任务进行中可交互
    DISABLE_KILL_CREDIT_FOR_OFFLINE_PLAYERS  = 0x00000400, // 禁止离线玩家获得击杀计数
    AI_ADDITIONAL_PATHING                    = 0x00080000,
};

DEFINE_ENUM_FLAG(CreatureStaticFlags7);

// 生物静态标志位 8（对应 StaticFlags8）
enum class CreatureStaticFlags8 : uint32
{
    FORCE_CLOSE_IN_ON_PATH_FAIL_BEHAVIOR     = 0x00000002, // 寻路失败时强制执行贴近行为
    USE_2D_CHASING_CALCULATION               = 0x00000020, // 使用 2D 平面追击计算
    USE_FAST_CLASSIC_HEARTBEAT               = 0x00000040, // 使用快速经典心跳更新频率
};

DEFINE_ENUM_FLAG(CreatureStaticFlags8);

// 客户端生物类型标志位（CreatureDisplay/CreatureTemplate 下发给客户端）
enum class CreatureTypeFlags : uint32
{
    TAMEABLE                                = 0x00000001,       // Tameable by any hunter (可被猎人驯服)
    VISIBLE_TO_GHOSTS                       = 0x00000002,       // Creatures which can _also_ be seen when player is a ghost, used in CanInteract function by client, can't be attacked (灵魂状态可见，如灵魂医者)
    BOSS_MOB                                = 0x00000004,       // "BOSS" flag for tooltips (提示框显示“首领”等级)
    DO_NOT_PLAY_WOUND_ANIM                  = 0x00000008,       // 不播放受击动作
    NO_FACTION_TOOLTIP                      = 0x00000010,       // controls something in client tooltip related to creature faction (提示框隐藏阵营名)
    MORE_AUDIBLE                            = 0x00000020,       // may be sound related (增大音效传播范围)
    SPELL_ATTACKABLE                        = 0x00000040,       // may be related to attackable / not attackable creatures with spells, used together with lua_IsHelpfulSpell/lua_IsHarmfulSpell (可被法术攻击)
    INTERACT_WHILE_DEAD                     = 0x00000080,       // Creature can be interacted with even if it's dead (死亡时仍可交互)
    SKIN_WITH_HERBALISM                     = 0x00000100,       // Can be looted by herbalist (可用草药学采集)
    SKIN_WITH_MINING                        = 0x00000200,       // Can be looted by miner (可用采矿采集)
    NO_DEATH_MESSAGE                        = 0x00000400,       // no idea, but it used by client (不显示死亡通知消息)
    ALLOW_MOUNTED_COMBAT                    = 0x00000800,       // possibility to attack and cast spells while mounted (允许骑乘战斗与施法)
    CAN_ASSIST                              = 0x00001000,       // Can aid any player (and group) in combat. Typically seen for escorting NPC's (玩家可协助治疗/增益，常用于护送 NPC)
    NO_PET_BAR                              = 0x00002000,       // checked from calls in Lua_PetHasActionBar (无宠物动作条)
    MASK_UID                                = 0x00004000,       // Lua_UnitGUID, client does guid_low &= 0xFF000000 if this flag is set (屏蔽低位 GUID 唯一标识)
    SKIN_WITH_ENGINEERING                   = 0x00008000,       // Can be looted by engineer (可用工程学拆解)
    TAMEABLE_EXOTIC                         = 0x00010000,       // Can be tamed by hunter as exotic pet (可作为奇珍野兽驯服)
    USE_MODEL_COLLISION_SIZE                = 0x00020000,       // related to CreatureDisplayInfo and scaling in some way (使用模型碰撞尺寸)
    ALLOW_INTERACTION_WHILE_IN_COMBAT       = 0x00040000,       // Related to vehicle/siege weapons (战斗中允许交互，如载具/攻城器械)
    COLLIDE_WITH_MISSILES                   = 0x00080000,       // may be has something to do with missiles (与投射物碰撞)
    NO_NAME_PLATE                           = 0x00100000,       // no idea, but it used by client, may be related to rendering (隐藏姓名板)
    DO_NOT_PLAY_MOUNTED_ANIMATIONS          = 0x00200000,       // may be has something to do with animation (disable animation?) (禁用骑乘/防御动作)
    LINK_ALL                                = 0x00400000,       // this one probably controls some creature visual (视觉联动标志)
    INTERACT_ONLY_WITH_CREATOR              = 0x00800000,       // First seen in 3.2.2. Related to banner/backpack of creature/companion, used in CanInteract function by client (仅召唤者可交互)
    DO_NOT_PLAY_UNIT_EVENT_SOUNDS           = 0x01000000,       // pet sounds related? (不播放单位事件音效)
    HAS_NO_SHADOW_BLOB                      = 0x02000000,       // this one probably controls some creature visual (无地面阴影)
    TREAT_AS_RAID_UNIT_FOR_HELPFUL_SPELLS   = 0x04000000,       // creature has no type, or forces creature to be considered as in party, may be related to creature assistance (有益法术视为团队成员)
    FORCE_GOSSIP                            = 0x08000000,       // used in Lua_ForceGossip (强制开启对话菜单)
    DO_NOT_SHEATHE                          = 0x10000000,       // no idea, but it used by client (不收起武器)
    DO_NOT_TARGET_ON_INTERACTION            = 0x20000000,       // 交互时不选中目标
    DO_NOT_RENDER_OBJECT_NAME               = 0x40000000,       // 不渲染实体名称
    QUEST_BOSS                              = 0x80000000,       // Lua_UnitIsQuestBoss (任务首领)
};

DEFINE_ENUM_FLAG(CreatureTypeFlags);

#endif