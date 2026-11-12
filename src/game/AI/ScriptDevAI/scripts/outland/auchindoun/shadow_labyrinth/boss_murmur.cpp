/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
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

/* ScriptData
SDName: Boss_Murmur
SD%Complete: 80
SDComment:
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

// 摩摩尔（暗影迷宫最终首领，声音之神）
/* 遭遇战概述：
 * 摩摩尔是暗影迷宫的最终首领，开战时处于虚弱状态（初始生命值仅为 40%）且固定在中央无法移动；
 * 核心机制为音速爆炸（大范围高伤害 AOE，玩家必须及时退至外圈边缘规避）；
 * 若近战范围内无玩家，首领会施放共鸣造成持续全团剧烈伤害；
 * 此外还会随机对目标施放摩摩尔之触、磁力吸引以及雷霆风暴（英雄难度）。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "shadow_labyrinth.h"

// 表情与法术枚举
enum
{
    EMOTE_SONIC_BOOM            = 18799,                    // 音速爆炸警示表情

    // Intro spells - used on npcs (开场剧情击杀小怪法术)
    SPELL_SUPPRESSION_BLAST     = 33332,                    // 压制冲击
    SPELL_MURMURS_WRATH         = 33331,                    // 摩摩尔之怒
    SPELL_MURMURS_WRATH_2       = 33329,

    SPELL_MAGNETIC_PULL         = 33689,                    // 磁力吸引
    SPELL_SONIC_BOOM            = 33923,        // dummy spell - triggers 33666 (音速爆炸，普通难度)
    SPELL_SONIC_BOOM_H          = 38796,        // dummy spell - triggers 38795 (音速爆炸，英雄难度)
    SPELL_MURMURS_TOUCH         = 33711,        // on expire silences the party members using shockwave - 33686 - also related to spell 33760 (摩摩尔之触，普通难度)
    SPELL_MURMURS_TOUCH_H       = 38794,                    // 摩摩尔之触（英雄难度）
    SPELL_RESONANCE             = 33657,                    // 共鸣（近战无人时全团高伤）

    SPELL_SONIC_SHOCK           = 38797,        // Heroic Spell (音速震击，英雄难度技能)
    SPELL_THUNDERING_STORM      = 39365,        // Heroic Spell (雷霆风暴，英雄难度技能)
};

// 摩摩尔行为枚举
enum MurmurActions
{
    MURMUR_ACTION_MAX,
    // After door opens 2 Cabal Spellbinder run out of the room, both get killed from murmur individually (大门开启后击杀逃跑的苦修者)
    MURMUR_INTRO_KILL_01,
    MURMUR_INTRO_KILL_02,
    // Random cast of Suppresion Blast or Murmurs Wrath (脱战剧情随机施法)
    MURMUR_OOC_RP_ATTACK,
};

// 摩摩尔 AI
struct boss_murmurAI : public CombatAI
{
    boss_murmurAI(Creature* creature) : CombatAI(creature, MURMUR_ACTION_MAX),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_bIsRegularMode(creature->GetMap()->IsRegularDifficulty()),
        m_firstCast(false)
    {
        AddCustomAction(MURMUR_INTRO_KILL_01, true, [&]() { HandleIntroKill01(); }, TIMER_COMBAT_OOC);
        AddCustomAction(MURMUR_INTRO_KILL_02, true, [&]() { HandleIntroKill02(); }, TIMER_COMBAT_OOC);
        AddCustomAction(MURMUR_OOC_RP_ATTACK, true, [&]() { HandleOocAttack(); }, TIMER_COMBAT_OOC);
    }

    ScriptedInstance* m_instance;                           // 副本数据指针
    bool m_bIsRegularMode;                                   // 是否为普通难度模式
    bool m_firstCast;                                       // 开门后是否已完成首次压制施法

    uint32 m_uiResonanceTimer;                              // 共鸣计时器
    uint32 m_uiThunderingStormTimer;                        // 雷霆风暴计时器

    void Reset() override                                   // 遭遇战重置：初始生命值置为 40% 且无法移动
    {
        CombatAI::Reset();

        m_creature->SetHealthPercent(40.f);
        SetAIImmobilizedState(true);
    }


    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override // 接收 AI 事件（大门打开启动剧情击杀）
    {
        if (eventType == AI_EVENT_CUSTOM_A)
        {
            // Murmur can cast Supression Blast first after door opens (大门开启后启动逃跑信使击杀计时)
            // resulting in a higher timer to kill the 2 Spellbinders that run out of the room
            ResetTimer(MURMUR_INTRO_KILL_01, urand(0, 5000));
            ResetTimer(MURMUR_INTRO_KILL_02, urand(0, 5000));
            // Murmur can start casting Supression Blast pretty fast after door opens
            ResetTimer(MURMUR_OOC_RP_ATTACK, urand(0, 3000));
        }
    }

    // After door opens 2 Cabal Spellbinder run out of the room, both get killed from murmur individually (开门剧情：击杀逃跑的第1名信使)
    void HandleIntroKill01()
    {
        std::vector<Creature*> const* killTarget = m_creature->GetMap()->GetCreatures(MURMURS_WRATH_TARGETS_01);
        if (killTarget)
        {
            for (Creature* creature : *killTarget)
                DoCastSpellIfCan(creature, SPELL_MURMURS_WRATH);
        }       
    }

    // 开门剧情：击杀逃跑的第2名信使
    void HandleIntroKill02()
    {
        std::vector<Creature*> const* killTarget = m_creature->GetMap()->GetCreatures(MURMURS_WRATH_TARGETS_02);
        if (killTarget)
        {
            for (Creature* creature : *killTarget)
                DoCastSpellIfCan(creature, SPELL_MURMURS_WRATH);
        }
    }

    // 脱战状态下的随机剧情轰击处理
    void HandleOocAttack()
    {
        if (m_creature->IsInCombat())
            return;

        // First cast after door opens should always be a Suprression Blast (开门后首次必施放压制冲击)
        if (!m_firstCast)
        {
            DoCastSpellIfCan(nullptr, SPELL_SUPPRESSION_BLAST);
            m_firstCast = true;
        }
        else
        { 
            // Kill a moving target (随机击杀移动目标)
            if (urand(0, 1))
            {
                GuidVector WrathTargetGuid;
                std::vector<Creature*> const* WrathTarget = m_creature->GetMap()->GetCreatures(MURMURS_WRATH_TARGETS_03);
                if (WrathTarget)
                {
                    for (Creature* creature : *WrathTarget)
                        if(creature->IsAlive() && !creature->IsInCombat())
                            WrathTargetGuid.push_back(creature->GetObjectGuid());
                }

                if (WrathTargetGuid.size() > 0)
                {
                    ObjectGuid guid = WrathTargetGuid[urand(0, WrathTargetGuid.size() - 1)];
                    if (Creature* creature = m_creature->GetMap()->GetCreature(guid))
                    {
                        DoCastSpellIfCan(creature, SPELL_MURMURS_WRATH);
                    }                    
                }
            }
            // stun 5 targets (击晕 5 个目标)
            else
                DoCastSpellIfCan(nullptr, SPELL_SUPPRESSION_BLAST);
        }

        ResetTimer(MURMUR_OOC_RP_ATTACK, 3000);
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override // 施法成功触发音速爆炸警示
    {
        if (spellInfo->Id == SPELL_SONIC_BOOM || spellInfo->Id == SPELL_SONIC_BOOM_H)
            DoBroadcastText(EMOTE_SONIC_BOOM, m_creature);
    }
};

// 33332 - Suppression Blast (压制冲击法术脚本：脱战最多命中 5 个目标)
struct SuppressionBlast : public SpellScript
{
    void OnInit(Spell* spell) const override
    {
        spell->SetMaxAffectedTargets(5);
    }

    bool OnCheckTarget(const Spell* /*spell*/, Unit* target, SpellEffectIndex /*eff*/) const override
    {
        if (target->IsInCombat())
            return false;

        return true;
    }
};

// 39365 - Thundering Storm (雷霆风暴法术脚本：仅命中距离 25 至 100 码内的外圈目标)
struct ThunderingStorm : public SpellScript
{
    bool OnCheckTarget(const Spell* spell, Unit* target, SpellEffectIndex /*eff*/) const override
    {
        Unit* caster = spell->GetCaster();
        float dist = sqrt(target->GetDistance(caster->GetPositionX(), caster->GetPositionY(), caster->GetPositionZ(), DIST_CALC_NONE));
        if (dist < 25.f || dist > 100.f)
            return false;
        return true;
    }
};

// 注册摩摩尔脚本
void AddSC_boss_murmur()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_murmur";
    pNewScript->GetAI = &GetNewAIInstance<boss_murmurAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<SuppressionBlast>("spell_suppression_blast");
    RegisterSpellScript<ThunderingStorm>("spell_thundering_storm");
}
