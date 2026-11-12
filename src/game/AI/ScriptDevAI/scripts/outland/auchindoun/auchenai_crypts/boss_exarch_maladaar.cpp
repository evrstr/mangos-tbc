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
SDName: Boss_Exarch_Maladaar
SD%Complete: 95
SDComment: Most of event implemented, possibly make some better code for switching his dark side in to better "images" of player.
SDCategory: Auchindoun, Auchenai Crypts
EndScriptData */

/* ContentData
mob_stolen_soul
boss_exarch_maladaar
EndContentData */

// 大主教玛拉达尔（奥金尼地穴最终首领）
/* 遭遇战概述：
 * 大主教玛拉达尔会周期性使用偷取灵魂复制目标玩家镜像，施放灵魂尖啸（恐惧）与灵魂之带；
 * 生命值低于 25% 时召唤强大的殉难者的化身协助战斗。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 偷取灵魂与对应职业技能法术枚举
enum
{
    SPELL_STOLEN_SOUL_DISPEL = 33326,                   // 驱散偷取灵魂减益

    SPELL_MOONFIRE          = 37328,                    // 月火术（德鲁伊）
    SPELL_FIREBALL          = 37329,                    // 火球术（法师）
    SPELL_MIND_FLAY         = 37330,                    // 精神鞭笞（牧师）
    SPELL_HEMORRHAGE        = 37331,                    // 出血（潜行者）
    SPELL_FROSTSHOCK        = 37332,                    // 冰霜震击（萨满祭司）
    SPELL_CURSE_OF_AGONY    = 37334,                    // 痛苦诅咒（术士）
    SPELL_MORTAL_STRIKE     = 37335,                    // 致死打击（战士）
    SPELL_FREEZING_TRAP     = 37368,                    // 冰冻陷阱（猎人）
    SPELL_HAMMER_OF_JUSTICE = 37369,                    // 制裁之锤（圣骑士）
    SPELL_PLAGUE_STRIKE     = 58339,                    // 脓疮打击（死亡骑士）

    SPELL_SET_BASE          = 1844100,                  // 职业技能列表基准 ID
};

// 被偷取的灵魂 AI：复制目标玩家的外观与职业法术
struct mob_stolen_soulAI : public ScriptedAI
{
    mob_stolen_soulAI(Creature* creature) : ScriptedAI(creature), m_uiStolenClass(0){ }

    uint8 m_uiStolenClass;                              // 被复制的玩家职业类别

    ObjectGuid m_targetGuid;                            // 来源玩家的 GUID

    void SetSoulInfo(Unit* target)                      // 初始化灵魂模型外观及职业法术列表
    {
        m_uiStolenClass = target->getClass();
        m_targetGuid = target->GetObjectGuid();
        m_creature->SetDisplayId(target->GetDisplayId());
        m_creature->SetSpellList(SPELL_SET_BASE + m_uiStolenClass);
    }

    void EnterEvadeMode() override                      // 脱离战斗时强制消失
    {
        m_creature->ForcedDespawn();
    }

    void JustDied(Unit* /*killer*/) override            // 死亡时驱散对应玩家身上的灵魂减益
    {
        if (Unit* target = m_creature->GetMap()->GetUnit(m_targetGuid))
            DoCastSpellIfCan(target, SPELL_STOLEN_SOUL_DISPEL, CAST_TRIGGERED);
    }
};

// 玛拉达尔台词、法术与随从数据枚举
enum
{
    SAY_SUMMON               = 15466,                   // 召唤化身台词
    SAY_AGGRO_1              = 17647,                   // 开战台词 1
    SAY_AGGRO_2              = 17648,                   // 开战台词 2
    SAY_AGGRO_3              = 17649,                   // 开战台词 3
    SAY_ROAR                 = 17644,                   // 怒吼台词
    SAY_SOUL_CLEAVE          = 17645,                   // 灵魂顺劈台词
    SAY_SLAY_1               = 17650,                   // 击杀玩家台词 1
    SAY_SLAY_2               = 17651,                   // 击杀玩家台词 2
    SAY_DEATH                = 17646,                   // 死亡台词

    SPELL_RIBBON_OF_SOULS    = 32422,                   // 灵魂之带
    SPELL_SOUL_SCREAM        = 32421,                   // 灵魂尖啸（恐惧）
    SPELL_STOLEN_SOUL        = 32346,                   // 偷取灵魂
    SPELL_STOLEN_SOUL_VISUAL = 32395,                   // 偷取灵魂视觉效果
    SPELL_SUMMON_AVATAR      = 32424,                   // 召唤殉难者的化身
    SPELL_PHASE_IN           = 33422,                   // 现身相位法术

    SPELL_STOLEN_SOUL_SUMMON = 32360,                   // 偷取灵魂召唤法术

    NPC_STOLEN_SOUL          = 18441,                   // 被偷取的灵魂生物条目
    NPC_DORE                 = 19412,                   // 德欧里（击败后现身的纳鲁）
    NPC_AVATAR_MARTYRED      = 18478,                   // 殉难者的化身
};

// 玛拉达尔战斗行为枚举
enum MaladaarActions
{
    MALADAAR_SUMMON_AVATAR,                             // 召唤化身行为
    MALADAAR_ACTION_MAX,
};

// 大主教玛拉达尔 AI
struct boss_exarch_maladaarAI : public CombatAI
{
    boss_exarch_maladaarAI(Creature* creature) : CombatAI(creature, MALADAAR_ACTION_MAX), m_bHasTaunted(false)
    {
        AddTimerlessCombatAction(MALADAAR_SUMMON_AVATAR, true);
        AddOnKillText(SAY_SLAY_1, SAY_SLAY_2);
    }

    ObjectGuid m_avatar;                                // 殉难者的化身 GUID

    bool m_bHasTaunted;                                 // 是否已播放进视线嘲讽

    void Reset() override                               // 遭遇战重置回调
    {
        CombatAI::Reset();
        if (Creature* avatar = m_creature->GetMap()->GetCreature(m_avatar))
            avatar->ForcedDespawn();
    }

    void MoveInLineOfSight(Unit* who) override          // 玩家进入视线触发开场嘲讽音效
    {
        if (!m_bHasTaunted && who->GetTypeId() == TYPEID_PLAYER && m_creature->IsWithinDistInMap(who, 150.0f) && m_creature->IsWithinLOSInMap(who))
        {
            m_creature->PlayDistanceSound(10509, PlayPacketSettings::ZONE);
            m_bHasTaunted = true;
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void Aggro(Unit* /*who*/) override                  // 进入战斗喊话
    {
        switch (urand(0, 2))
        {
            case 0: DoBroadcastText(SAY_AGGRO_1, m_creature); break;
            case 1: DoBroadcastText(SAY_AGGRO_2, m_creature); break;
            case 2: DoBroadcastText(SAY_AGGRO_3, m_creature); break;
        }
    }

    void JustSummoned(Creature* summoned) override      // 召唤物生成回调：记录化身 GUID
    {
        switch (summoned->GetEntry())
        {
            case NPC_AVATAR_MARTYRED:
            {
                m_avatar = summoned->GetObjectGuid();
                break;
            }
        }
    }

    void JustDied(Unit* /*killer*/) override            // 首领死亡回调：播放遗言并召唤德欧里
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        // When Exarch Maladaar is defeated D'ore appear. (击败玛拉达尔后召唤纳鲁德欧里)
        m_creature->SummonCreature(NPC_DORE, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSPAWN_TIMED_DESPAWN, 600000);
    }

    void SpellHitTarget(Unit* target, const SpellEntry* spellInfo) override // 法术命中目标回调
    {
        if (spellInfo->Id == SPELL_STOLEN_SOUL && target->IsPlayer())
            target->CastSpell(nullptr, SPELL_STOLEN_SOUL_SUMMON, TRIGGERED_OLD_TRIGGERED);
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override // 施法成功回调
    {
        if (spellInfo->Id == SPELL_STOLEN_SOUL && urand(0, 1))
            DoBroadcastText(urand(0, 1) ? SAY_ROAR : SAY_SOUL_CLEAVE, m_creature);
    }

    void ExecuteAction(uint32 action) override          // 执行战斗动作：血量低于 25% 召唤化身
    {
        if (action == MALADAAR_SUMMON_AVATAR && m_creature->GetHealthPercent() < 25.0f)
        {
            if (DoCastSpellIfCan(nullptr, SPELL_SUMMON_AVATAR) == CAST_OK)
            {
                DoBroadcastText(SAY_SUMMON, m_creature);
                SetActionReadyStatus(action, false);
            }
        }
    }
};

// 偷取灵魂召唤法术脚本：初始化镜像职业与攻击目标
struct StolenSoulSummon : public SpellScript
{
    void OnSummon(Spell* spell, Creature* summon) const override
    {
        // SPELL_STOLEN_SOUL_VISUAL has shapeshift effect, but not implemented feature in mangos for this spell. (视觉变形法术未实现)
        summon->CastSpell(nullptr, SPELL_STOLEN_SOUL_VISUAL, TRIGGERED_NONE);

        if (mob_stolen_soulAI* pSoulAI = dynamic_cast<mob_stolen_soulAI*>(summon->AI()))
            pSoulAI->SetSoulInfo(spell->GetCaster());

        summon->AI()->AttackStart(spell->GetCaster());
    }
};

// 注册大主教玛拉达尔及相关脚本
void AddSC_boss_exarch_maladaar()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_exarch_maladaar";
    pNewScript->GetAI = &GetNewAIInstance<boss_exarch_maladaarAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_stolen_soul";
    pNewScript->GetAI = &GetNewAIInstance<mob_stolen_soulAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<StolenSoulSummon>("spell_stolen_soul_summon");
}
