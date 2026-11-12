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

#ifndef MANGOS_SPELLEFFECTDEFINES_H
#define MANGOS_SPELLEFFECTDEFINES_H

#include "Spells/SpellTargetDefines.h"

// 法术效果类型枚举（对应 DBC 中的 Effect[0..2]）
enum SpellEffects
{
    SPELL_EFFECT_NONE                      = 0,             // 无效果
    SPELL_EFFECT_INSTAKILL                 = 1,             // 立即击杀目标
    SPELL_EFFECT_SCHOOL_DAMAGE             = 2,             // 法术系别直接伤害
    SPELL_EFFECT_DUMMY                     = 3,             // 虚拟占位效果（由脚本实现具体逻辑）
    SPELL_EFFECT_PORTAL_TELEPORT           = 4,             // 传送门传送（旧版/未使用）
    SPELL_EFFECT_TELEPORT_UNITS            = 5,             // 传送单位至指定位置
    SPELL_EFFECT_APPLY_AURA                = 6,             // 为目标施加光环
    SPELL_EFFECT_ENVIRONMENTAL_DAMAGE      = 7,             // 环境伤害（如熔岩/溺水）
    SPELL_EFFECT_POWER_DRAIN               = 8,             // 能量吸取（抽蓝/抽能）
    SPELL_EFFECT_HEALTH_LEECH              = 9,             // 生命吸取（造成伤害并治疗自身）
    SPELL_EFFECT_HEAL                      = 10,            // 直接治疗
    SPELL_EFFECT_BIND                      = 11,            // 绑定炉石点（旅店绑定）
    SPELL_EFFECT_PORTAL                    = 12,            // 传送门召唤（未直接使用）
    SPELL_EFFECT_RITUAL_BASE               = 13,            // 召唤仪式基础效果
    SPELL_EFFECT_RITUAL_SPECIALIZE         = 14,            // 仪式专精效果
    SPELL_EFFECT_RITUAL_ACTIVATE_PORTAL    = 15,            // 激活仪式传送门
    SPELL_EFFECT_QUEST_COMPLETE            = 16,            // 直接完成指定任务
    SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL    = 17,            // 无系别武器附加伤害
    SPELL_EFFECT_RESURRECT                 = 18,            // 复活目标（旧版）
    SPELL_EFFECT_ADD_EXTRA_ATTACKS         = 19,            // 增加额外近战攻击次数
    SPELL_EFFECT_DODGE                     = 20,            // 被动：启用躲闪能力
    SPELL_EFFECT_EVADE                     = 21,            // 脱离战斗/闪避
    SPELL_EFFECT_PARRY                     = 22,            // 被动：启用招架能力
    SPELL_EFFECT_BLOCK                     = 23,            // 被动：启用格挡能力
    SPELL_EFFECT_CREATE_ITEM               = 24,            // 制造物品（专业/法术造物）
    SPELL_EFFECT_WEAPON                    = 25,            // 被动：启用武器类型熟练度
    SPELL_EFFECT_DEFENSE                   = 26,            // 被动：防御能力
    SPELL_EFFECT_PERSISTENT_AREA_AURA      = 27,            // 持续性地面区域光环（DynamicObject）
    SPELL_EFFECT_SUMMON                    = 28,            // 召唤生物或图腾等实体
    SPELL_EFFECT_LEAP                      = 29,            // 跳跃/突进至目标点
    SPELL_EFFECT_ENERGIZE                  = 30,            // 恢复法力/怒气/能量（固定值）
    SPELL_EFFECT_WEAPON_PERCENT_DAMAGE     = 31,            // 按武器伤害百分比造成伤害
    SPELL_EFFECT_TRIGGER_MISSILE           = 32,            // 触发弹道法术
    SPELL_EFFECT_OPEN_LOCK                 = 33,            // 开锁/采集/开启游戏对象
    SPELL_EFFECT_SUMMON_CHANGE_ITEM        = 34,            // 召唤并替换物品
    SPELL_EFFECT_APPLY_AREA_AURA_PARTY     = 35,            // 对小队成员施加区域光环
    SPELL_EFFECT_LEARN_SPELL               = 36,            // 学习法术
    SPELL_EFFECT_SPELL_DEFENSE             = 37,            // 法术防御（未使用）
    SPELL_EFFECT_DISPEL                    = 38,            // 驱散增益或减益光环
    SPELL_EFFECT_LANGUAGE                  = 39,            // 习得语言
    SPELL_EFFECT_DUAL_WIELD                = 40,            // 被动：启用双持武器
    SPELL_EFFECT_41                        = 41,            // old SPELL_EFFECT_SUMMON_WILD (旧版：召唤野生生物)
    SPELL_EFFECT_42                        = 42,            // old SPELL_EFFECT_SUMMON_GUARDIAN (旧版：召唤守卫)
    SPELL_EFFECT_TELEPORT_UNITS_FACE_CASTER = 43,           // 传送单位并使其面向施法者
    SPELL_EFFECT_SKILL_STEP                = 44,            // 提升技能上限阶梯（如专家级/大师级）
    SPELL_EFFECT_ADD_HONOR                 = 45,            // 增加荣誉点数
    SPELL_EFFECT_SPAWN                     = 46,            // 生成实体（未使用）
    SPELL_EFFECT_TRADE_SKILL               = 47,            // 打开商业技能制造界面
    SPELL_EFFECT_STEALTH                   = 48,            // 潜行（未使用）
    SPELL_EFFECT_DETECT                    = 49,            // 侦测（未使用）
    SPELL_EFFECT_TRANS_DOOR                = 50,            // 生成传送门游戏对象
    SPELL_EFFECT_FORCE_CRITICAL_HIT        = 51,            // 强制暴击（未使用）
    SPELL_EFFECT_GUARANTEE_HIT             = 52,            // 必定命中（未使用）
    SPELL_EFFECT_ENCHANT_ITEM              = 53,            // 永久附魔物品
    SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY    = 54,            // 临时附魔物品（如磨刀石/毒药/武器增益）
    SPELL_EFFECT_TAMECREATURE              = 55,            // 驯服野兽为宠物
    SPELL_EFFECT_SUMMON_PET                = 56,            // 召唤已驯服的宠物
    SPELL_EFFECT_LEARN_PET_SPELL           = 57,            // 教导宠物学习技能
    SPELL_EFFECT_WEAPON_DAMAGE             = 58,            // 武器伤害加上固定附加伤害
    SPELL_EFFECT_OPEN_LOCK_ITEM            = 59,            // 使用物品开锁（如盗贼开锁工具/钥匙）
    SPELL_EFFECT_PROFICIENCY               = 60,            // 习得护甲或武器专精熟练度
    SPELL_EFFECT_SEND_EVENT                = 61,            // 触发世界事件/脚本事件
    SPELL_EFFECT_POWER_BURN                = 62,            // 法力燃烧（烧蓝并造成伤害）
    SPELL_EFFECT_THREAT                    = 63,            // 增加或减少固定威胁值
    SPELL_EFFECT_TRIGGER_SPELL             = 64,            // 立即触发另一个法术
    SPELL_EFFECT_HEALTH_FUNNEL             = 65,            // 生命通道（未使用）
    SPELL_EFFECT_POWER_FUNNEL              = 66,            // 能量通道（未使用）
    SPELL_EFFECT_HEAL_MAX_HEALTH           = 67,            // 恢复至满生命值
    SPELL_EFFECT_INTERRUPT_CAST            = 68,            // 打断目标当前施法并封锁同系法术
    SPELL_EFFECT_DISTRACT                  = 69,            // 扰乱（使目标转向指定位置）
    SPELL_EFFECT_PULL                      = 70,            // 拉扯目标
    SPELL_EFFECT_PICKPOCKET                = 71,            // 偷窃（生成偷窃战利品）
    SPELL_EFFECT_ADD_FARSIGHT              = 72,            // 添加远视视野点
    SPELL_EFFECT_73                        = 73,            // old SPELL_EFFECT_SUMMON_POSSESSED (旧版：召唤附身生物)
    SPELL_EFFECT_74                        = 74,            // old SPELL_EFFECT_SUMMON_TOTEM (旧版：召唤图腾)
    SPELL_EFFECT_HEAL_MECHANICAL           = 75,            // 修理/治疗机械单位
    SPELL_EFFECT_SUMMON_OBJECT_WILD        = 76,            // 在世界坐标召唤野外游戏对象
    SPELL_EFFECT_SCRIPT_EFFECT             = 77,            // 脚本定制效果
    SPELL_EFFECT_ATTACK                    = 78,            // 开始自动攻击
    SPELL_EFFECT_SANCTUARY                 = 79,            // 圣域保护（脱战并清除仇恨，如假死/消失）
    SPELL_EFFECT_ADD_COMBO_POINTS          = 80,            // 增加连击点数
    SPELL_EFFECT_CREATE_HOUSE              = 81,            // 创建房屋（未实现/废弃）
    SPELL_EFFECT_BIND_SIGHT                = 82,            // 绑定视野
    SPELL_EFFECT_DUEL                      = 83,            // 发起决斗
    SPELL_EFFECT_STUCK                     = 84,            // 脱离卡死
    SPELL_EFFECT_SUMMON_PLAYER             = 85,            // 召唤玩家到身边
    SPELL_EFFECT_ACTIVATE_OBJECT           = 86,            // 激活游戏对象动作
    SPELL_EFFECT_87                        = 87,            // old SPELL_EFFECT_SUMMON_TOTEM_SLOT1 (旧版：召唤图腾槽 1)
    SPELL_EFFECT_88                        = 88,            // old SPELL_EFFECT_SUMMON_TOTEM_SLOT2 (旧版：召唤图腾槽 2)
    SPELL_EFFECT_89                        = 89,            // old SPELL_EFFECT_SUMMON_TOTEM_SLOT3 (旧版：召唤图腾槽 3)
    SPELL_EFFECT_90                        = 90,            // old SPELL_EFFECT_SUMMON_TOTEM_SLOT4 (旧版：召唤图腾槽 4)
    SPELL_EFFECT_THREAT_ALL                = 91,            // 修改对所有敌对目标的威胁值
    SPELL_EFFECT_ENCHANT_HELD_ITEM         = 92,            // 附魔当前装备中的物品
    SPELL_EFFECT_93                        = 93,            // old SPELL_EFFECT_SUMMON_PHANTASM (旧版：召唤幻象)
    SPELL_EFFECT_SELF_RESURRECT            = 94,            // 自我复活（如萨满复生/灵魂石）
    SPELL_EFFECT_SKINNING                  = 95,            // 剥皮/采矿/草药尸体外皮采集
    SPELL_EFFECT_CHARGE                    = 96,            // 冲锋至目标位置
    SPELL_EFFECT_97                        = 97,            // old SPELL_EFFECT_SUMMON_CRITTER (旧版：召唤小动物)
    SPELL_EFFECT_KNOCK_BACK                = 98,            // 击退目标
    SPELL_EFFECT_DISENCHANT                = 99,            // 附魔分解物品
    SPELL_EFFECT_INEBRIATE                 = 100,           // 醉酒效果（增加醉酒度）
    SPELL_EFFECT_FEED_PET                  = 101,           // 喂养宠物
    SPELL_EFFECT_DISMISS_PET               = 102,           // 解散宠物
    SPELL_EFFECT_REPUTATION                = 103,           // 增加声望点数
    SPELL_EFFECT_SUMMON_OBJECT_SLOT1       = 104,           // 在图腾/对象槽 1 召唤对象
    SPELL_EFFECT_SUMMON_OBJECT_SLOT2       = 105,           // 在图腾/对象槽 2 召唤对象
    SPELL_EFFECT_SUMMON_OBJECT_SLOT3       = 106,           // 在图腾/对象槽 3 召唤对象
    SPELL_EFFECT_SUMMON_OBJECT_SLOT4       = 107,           // 在图腾/对象槽 4 召唤对象
    SPELL_EFFECT_DISPEL_MECHANIC           = 108,           // 按法术机制驱散效果
    SPELL_EFFECT_SUMMON_DEAD_PET           = 109,           // 复活死亡的宠物
    SPELL_EFFECT_DESTROY_ALL_TOTEMS        = 110,           // 摧毁所有已召唤的图腾
    SPELL_EFFECT_DURABILITY_DAMAGE         = 111,           // 损坏固定点数的装备耐久度
    SPELL_EFFECT_112                       = 112,           // old SPELL_EFFECT_SUMMON_DEMON (旧版：召唤恶魔)
    SPELL_EFFECT_RESURRECT_NEW             = 113,           // 新版复活效果
    SPELL_EFFECT_ATTACK_ME                 = 114,           // 强迫目标攻击自己（嘲讽）
    SPELL_EFFECT_DURABILITY_DAMAGE_PCT     = 115,           // 按百分比损坏装备耐久度
    SPELL_EFFECT_SKIN_PLAYER_CORPSE        = 116,           // 剥取玩家尸体（战场徽章等）
    SPELL_EFFECT_SPIRIT_HEAL               = 117,           // 灵魂医者复活治疗
    SPELL_EFFECT_SKILL                     = 118,           // 提升指定技能等级
    SPELL_EFFECT_APPLY_AREA_AURA_PET       = 119,           // 对宠物施加区域光环
    SPELL_EFFECT_TELEPORT_GRAVEYARD        = 120,           // 传送至最近墓地
    SPELL_EFFECT_NORMALIZED_WEAPON_DMG     = 121,           // 标准化武器伤害（按武器类型固定速度计算）
    SPELL_EFFECT_122                       = 122,           // 保留效果 122
    SPELL_EFFECT_SEND_TAXI                 = 123,           // 触发飞行路线传送
    SPELL_EFFECT_PULL_TOWARDS              = 124,           // 将目标拉向施法者
    SPELL_EFFECT_MODIFY_THREAT_PERCENT     = 125,           // 按百分比修改威胁值
    SPELL_EFFECT_STEAL_BENEFICIAL_BUFF     = 126,           // 法术偷取（窃取目标有益魔法效果）
    SPELL_EFFECT_PROSPECTING               = 127,           // 珠宝加工选矿
    SPELL_EFFECT_APPLY_AREA_AURA_FRIEND    = 128,           // 对范围内友方施加区域光环
    SPELL_EFFECT_APPLY_AREA_AURA_ENEMY     = 129,           // 对范围内敌方施加区域光环
    SPELL_EFFECT_REDIRECT_THREAT           = 130,           // 转移威胁值（如误导）
    SPELL_EFFECT_PLAY_SOUND                = 131,           // 播放客户端音效
    SPELL_EFFECT_PLAY_MUSIC                = 132,           // 播放客户端背景音乐
    SPELL_EFFECT_UNLEARN_SPECIALIZATION    = 133,           // 遗忘商业技能专精
    SPELL_EFFECT_KILL_CREDIT_GROUP         = 134,           // 为小队/团队授予任务击杀计数
    SPELL_EFFECT_CALL_PET                  = 135,           // 召唤宠物（带坐标）
    SPELL_EFFECT_HEAL_PCT                  = 136,           // 按最大生命值百分比治疗
    SPELL_EFFECT_ENERGIZE_PCT              = 137,           // 按最大能量上限百分比恢复资源
    SPELL_EFFECT_LEAP_BACK                 = 138,           // 向后跳跃（如逃脱）
    SPELL_EFFECT_CLEAR_QUEST               = 139,           // 重置/清除任务状态
    SPELL_EFFECT_FORCE_CAST                = 140,           // 强制目标施放指定法术
    SPELL_EFFECT_FORCE_CAST_WITH_VALUE     = 141,           // 强制目标以指定基础数值施放法术
    SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE  = 142,           // 以指定数值触发法术
    SPELL_EFFECT_APPLY_AREA_AURA_OWNER     = 143,           // 对召唤者施加区域光环
    SPELL_EFFECT_KNOCKBACK_FROM_POSITION   = 144,           // 从指定坐标击退目标
    SPELL_EFFECT_PULL_TOWARDS_DEST         = 145,           // 将目标拉向目标坐标
    SPELL_EFFECT_146                       = 146,           // 保留效果 146
    SPELL_EFFECT_QUEST_FAIL                = 147,           // 使指定任务失败
    SPELL_EFFECT_148                       = 148,           // 保留效果 148
    SPELL_EFFECT_CHARGE_DEST               = 149,           // 冲锋至目标坐标
    SPELL_EFFECT_150                       = 150,           // 保留效果 150
    SPELL_EFFECT_TRIGGER_SPELL_2           = 151,           // 触发法术（目标点变体）
    SPELL_EFFECT_SUMMON_RAF_FRIEND         = 152, // TODO: Implement (战友招募召唤，待实现)
    SPELL_EFFECT_CREATE_PET                = 153, // TODO: Implement (创建宠物，待实现)
    MAX_SPELL_EFFECTS                      = 154            // 法术效果类型总数上限
};

// 单个法术效果的元信息（名称、所需目标类型、默认隐式目标）
struct SpellEffectInfo
{
    char const* name;                                       // 效果名称字符串
    SpellTargetImplicitType requiredTarget;                 // Needs to correspond to what handler can handle (处理器所需的隐式目标类型)
    SpellTarget defaultTarget;                              // [TARGET_NONE,TARGET_NONE] case (当 A/B 目标均为空时使用的默认目标)
};

// 全局法术效果信息表
extern SpellEffectInfo SpellEffectInfoTable[MAX_SPELL_EFFECTS];

#endif