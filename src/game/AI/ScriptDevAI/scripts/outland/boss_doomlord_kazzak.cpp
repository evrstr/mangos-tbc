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
SDName: Boss_Doomlord_Kazzak
SD%Complete: 90
SDComment: Timers
SDCategory: Hellfire Peninsula
EndScriptData */

// 末日领主卡扎克（世界首领）
/* 遭遇战概述：
 * 末日领主卡扎克位于地狱火半岛基尔加丹王座，核心灭团技为卡扎克印记（抽蓝爆炸）与捕获灵魂（任意玩家/随从死亡为卡扎克回复海量生命值并狂暴）。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 相关常量、法术与文本条目枚举
enum
{
    SAY_INTRO                       = 20076,
    SAY_INTRO_2                     = 15895,        // no sound
    SAY_AGGRO1                      = 20077,
    SAY_AGGRO2                      = 20078,
    SAY_SUPREME_1                   = 20079,
    SAY_SUPREME_2                   = 20080,
    SAY_KILL1                       = 20081,
    SAY_KILL2                       = 20082,
    SAY_KILL3                       = 20083,
    SAY_DEATH                       = 20084,
    SAY_DEATH_2                     = 16432,
    EMOTE_GENERIC_FRENZY            = 1191,

    SPELL_SHADOW_VOLLEY             = 32963,
    SPELL_CLEAVE                    = 16044,
    SPELL_THUNDERCLAP               = 36706,
    SPELL_VOID_BOLT                 = 21066,
    SPELL_MARK_OF_KAZZAK            = 32960,
    SPELL_ENRAGE                    = 32964,        // triggers 32963
    SPELL_CAPTURE_SOUL              = 48473,        // procs 32966 on player kill
    SPELL_TWISTED_REFLECTION        = 21063,
    SPELL_BERSERK                   = 32965,        // triggers 32963
};

// boss_doomlordkazzakAI 逻辑实现
struct boss_doomlordkazzakAI : public CombatAI
{
    boss_doomlordkazzakAI(Creature* creature) : CombatAI(creature, 0)
    {
        AddOnKillText(SAY_KILL1, SAY_KILL2, SAY_KILL3);
    }

    void JustRespawned() override
    {
        CombatAI::JustRespawned();
        DoBroadcastText(urand(0, 1) ? SAY_INTRO : SAY_INTRO_2, m_creature);
    }

    // 进入战斗回调
    void Aggro(Unit* /*who*/) override
    {
        DoBroadcastText(urand(0, 1) ? SAY_AGGRO1 : SAY_AGGRO2, m_creature);
        DoCastSpellIfCan(nullptr, SPELL_CAPTURE_SOUL, CAST_TRIGGERED);
    }

    // 死亡回调
    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(urand(0, 1) ? SAY_DEATH : SAY_DEATH_2, m_creature);
    }

    // 施法完成回调
    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        switch (spellInfo->Id)
        {
            case SPELL_ENRAGE:
                DoBroadcastText(EMOTE_GENERIC_FRENZY, m_creature);
                DoBroadcastText(urand(0, 1) ? SAY_SUPREME_1 : SAY_SUPREME_2, m_creature);
                break;
        }
    }
};

// 32960 - Mark of Kazzak
// MarkOfDoomlordKazzak 光环脚本
struct MarkOfDoomlordKazzak : public AuraScript
{
    void OnPeriodicCalculateAmount(Aura* aura, uint32& amount) const override
    {
        if (aura->GetTarget()->HasMana())
            amount = aura->GetTarget()->GetMaxPower(POWER_MANA) * 5 / 100;        
    }

    void OnPeriodicTickEnd(Aura* aura) const override
    {
        if (aura->GetTarget()->HasMana())
        {
            if (aura->GetTarget()->GetPower(POWER_MANA) == 0)
            {
                aura->GetTarget()->CastSpell(nullptr, 32961, TRIGGERED_OLD_TRIGGERED);
                aura->GetTarget()->RemoveAurasDueToSpell(aura->GetId());
            }
        }
    }
};

// 21063 - Twisted Reflection
// TwistedReflection 光环脚本
struct TwistedReflection : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* /*aura*/, ProcExecutionData& procData) const override
    {
        procData.triggerTarget = procData.attacker;
        procData.triggeredSpellId = 21064;
        return SPELL_AURA_PROC_OK;
    }
};

// 注册 boss_doomlordkazzak 脚本
void AddSC_boss_doomlordkazzak()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_doomlord_kazzak";
    pNewScript->GetAI = &GetNewAIInstance<boss_doomlordkazzakAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<MarkOfDoomlordKazzak>("spell_mark_of_doomlord_kazzak");
    RegisterSpellScript<TwistedReflection>("spell_twisted_reflection");
}
