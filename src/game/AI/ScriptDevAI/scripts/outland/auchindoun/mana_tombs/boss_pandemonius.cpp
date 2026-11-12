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
SDName: Boss_Pandemonius
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Mana Tombs
EndScriptData */

// 潘德莫纽斯（法力陵墓一号首领）
/* 遭遇战概述：
 * 潘德莫纽斯是一个虚空领主，战斗中会连续施放虚空冲击（连发暗影箭）；
 * 并周期性开启暗影外壳，获得伤害吸收并反射物理近战攻击与法术伤害。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 台词、表情与法术枚举
enum
{
    SAY_AGGRO_1                     = 17771,                    // 开战台词 1
    SAY_AGGRO_2                     = 17772,                    // 开战台词 2
    SAY_AGGRO_3                     = 17773,                    // 开战台词 3
    SAY_KILL_1                      = 17774,                    // 击杀玩家台词 1
    SAY_KILL_2                      = 17775,                    // 击杀玩家台词 2
    SAY_DEATH                       = 17776,                    // 死亡台词
    EMOTE_DARK_SHELL                = 21067,                    // 暗影外壳表情广播文本

    SPELL_VOID_BLAST                = 32325,                    // 虚空冲击（普通难度）
    SPELL_VOID_BLAST_H              = 38760,                    // 虚空冲击（英雄难度）
    SPELL_DARK_SHELL                = 32358,                    // 暗影外壳（普通难度）
    SPELL_DARK_SHELL_H              = 38759,                    // 暗影外壳（英雄难度）

    MAX_VOID_BLASTS                 = 5,                        // 虚空冲击最大连发次数
};

// 潘德莫纽斯 AI
struct boss_pandemoniusAI : public CombatAI
{
    boss_pandemoniusAI(Creature* creature) : CombatAI(creature, 0), m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddOnKillText(SAY_KILL_1, SAY_KILL_2);
    }

    bool m_isRegularMode;                                       // 是否为普通难度模式

    void JustDied(Unit* /*killer*/) override                    // 首领死亡回调
    {
        DoBroadcastText(SAY_DEATH, m_creature);
    }

    void Aggro(Unit* /*who*/) override                          // 进入战斗随机喊话
    {
        switch (urand(0, 2))
        {
            case 0: DoBroadcastText(SAY_AGGRO_1, m_creature); break;
            case 1: DoBroadcastText(SAY_AGGRO_2, m_creature); break;
            case 2: DoBroadcastText(SAY_AGGRO_3, m_creature); break;
        }
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override // 施法完成回调
    {
        if (spellInfo->Id == SPELL_DARK_SHELL || spellInfo->Id == SPELL_DARK_SHELL_H)
            DoBroadcastText(EMOTE_DARK_SHELL, m_creature);
    }
};

// 虚空冲击法术脚本：实现多发连续轰击
struct VoidBlast : public SpellScript
{
    void OnCast(Spell* spell) const override                    // 施法回调：递增连发计数并连续施法
    {
        Unit* caster = spell->GetCaster();
        uint64 scriptVal = spell->GetScriptValue();
        if (scriptVal == MAX_VOID_BLASTS)
            return;
        SpellCastArgs args;
        args.SetScriptValue(scriptVal + 1);
        caster->CastSpell(args, spell->m_spellInfo, TRIGGERED_NORMAL_COMBAT_CAST | TRIGGERED_IGNORE_COOLDOWNS);
    }
};

// 注册潘德莫纽斯脚本
void AddSC_boss_pandemonius()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_pandemonius";
    pNewScript->GetAI = &GetNewAIInstance<boss_pandemoniusAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<VoidBlast>("spell_void_blast");
}
