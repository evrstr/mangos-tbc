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
SDName: Boss_Grandmaster_Vorpil
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

// 沃匹尔大师（暗影迷宫三号首领）
/* 遭遇战概述：
 * 沃匹尔大师是暗影迷宫的三号首领，战斗中会使用暗影牵引将所有玩家拉向自己并施放烈焰风暴；
 * 此外会激活五个虚空传送门，周期性召唤虚空行者（虚空旅行者）缓慢走向沃匹尔，
 * 虚空行者一旦触碰首领便会牺牲自爆，为首领回复生命值并对全团造成暗影新星伤害。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "shadow_labyrinth.h"

// 台词、法术与随从条目枚举
enum
{
    SAY_INTRO                       = -1555028, // does not have bct but does have sound (开场台词语音)
    SAY_AGGRO_1                     = 17868,                    // 开战台词 1
    SAY_AGGRO_2                     = 17869,                    // 开战台词 2
    SAY_AGGRO_3                     = 17870,                    // 开战台词 3
    SAY_HELP                        = 17867,                    // 召唤行者呼救台词
    SAY_SLAY_1                      = 17871,                    // 击杀玩家台词 1
    SAY_SLAY_2                      = 17872,                    // 击杀玩家台词 2
    SAY_DEATH                       = 17873,                    // 死亡台词

    SPELL_DRAW_SHADOWS              = 33563,                    // 暗影牵引（将玩家传送至身边）

    SPELL_VOID_PORTAL_A             = 33566,                    // 虚空传送门 A
    SPELL_VOID_PORTAL_B             = 33614,                    // 虚空传送门 B
    SPELL_VOID_PORTAL_C             = 33615,                    // 虚空传送门 C
    SPELL_VOID_PORTAL_D             = 33567,                    // 虚空传送门 D
    SPELL_VOID_PORTAL_E             = 33616,                    // 虚空传送门 E

    SPELL_SHADOW_BOLT_VOLLEY        = 32963,                    // 暗影箭雨
    SPELL_RAIN_OF_FIRE              = 33617,                    // 烈焰风暴（普通难度）
    SPELL_RAIN_OF_FIRE_H            = 39363,                    // 烈焰风暴（英雄难度）
    SPELL_BANISH_H                  = 38791,                    // 放逐术（英雄难度）
    SPELL_SUMMON_VOID_SUMMONER      = 33927,                    // 召唤虚空召唤者
    SPELL_SUMMON_VOIDWALKER_A       = 33582,                // the void travelers are summond at portal locations according to DB coords (传送门 A 召唤行者)
    SPELL_SUMMON_VOIDWALKER_B       = 33583,                    // 传送门 B 召唤行者
    SPELL_SUMMON_VOIDWALKER_C       = 33584,                    // 传送门 C 召唤行者
    SPELL_SUMMON_VOIDWALKER_D       = 33585,                    // 传送门 D 召唤行者
    SPELL_SUMMON_VOIDWALKER_E       = 33586,                    // 传送门 E 召唤行者

    SPELL_VOID_PORTAL_VISUAL        = 33569,                    // 虚空传送门视觉效果

    SPELL_EMPOWERING_SHADOWS        = 33783,                    // 强化暗影（普通难度行者牺牲治疗沃匹尔）
    SPELL_EMPOWERING_SHADOWS_H      = 39364,                    // 强化暗影（英雄难度行者牺牲治疗沃匹尔）
    SPELL_SHADOW_NOVA               = 33846,                    // 暗影新星伤害

    SPELL_INSTAKILL_SELF            = 29878,                    // 自毁法术

    NPC_VOID_PORTAL                 = 19224,                    // 虚空传送门 NPC
    NPC_VOID_TRAVELER               = 19226,                    // 虚空旅行者 NPC
    NPC_VOID_SUMMONER               = 19427,                    // 虚空召唤者触发器

    MAX_PORTALS                     = 4                         // 传送门上限
};

// 沃匹尔暗影牵引传送目标中心坐标
static const float aVorpilTeleportLoc[3] = { -253.06f, -264.02f, 17.08f};

// 各传送门召唤虚空旅行者法术数组
static const uint32 aTravelerSummonSpells[5] = {SPELL_SUMMON_VOIDWALKER_A, SPELL_SUMMON_VOIDWALKER_B, SPELL_SUMMON_VOIDWALKER_C, SPELL_SUMMON_VOIDWALKER_D, SPELL_SUMMON_VOIDWALKER_E};
// 各传送门生成法术数组
static const uint32 aPortalSpells[5] = { SPELL_VOID_PORTAL_A, SPELL_VOID_PORTAL_B, SPELL_VOID_PORTAL_C, SPELL_VOID_PORTAL_D, SPELL_VOID_PORTAL_E };

// 沃匹尔战斗动作枚举
enum GrandmasterVorpilActions
{
    VORPIL_ACTION_MAX,
};

// 沃匹尔大师 AI
struct boss_grandmaster_vorpilAI : public CombatAI
{
    boss_grandmaster_vorpilAI(Creature* creature) : CombatAI(creature, VORPIL_ACTION_MAX),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_isRegularMode(creature->GetMap()->IsRegularDifficulty()),
        m_hasDoneIntro(false)
    {
        AddOnKillText(SAY_SLAY_1, SAY_SLAY_2);
    }

    ScriptedInstance* m_instance;                               // 副本数据管理指针
    bool m_isRegularMode;                                       // 是否为普通难度模式
    bool m_hasDoneIntro;                                        // 是否已触发进视线台词

    GuidVector m_spawns;                                        // 召唤出的传送门与召唤者 GUID 列表
    ObjectGuid m_summoner;                                      // 召唤者触发器 GUID

    void Reset() override                                       // 遭遇战重置回调
    {
        CombatAI::Reset();
        SetCombatMovement(true);
        SetCombatScriptStatus(false);
        DespawnGuids(m_spawns);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override // 接收 AI 事件（暗影牵引完成，开始引导烈焰风暴）
    {
        if (eventType == AI_EVENT_CUSTOM_A)
        {
            SetCombatMovement(true);
            SetCombatScriptStatus(false);
            DoCastSpellIfCan(nullptr, m_isRegularMode ? SPELL_RAIN_OF_FIRE : SPELL_RAIN_OF_FIRE_H);
        }
    }

    void MoveInLineOfSight(Unit* who) override                  // 玩家进入视线触发开场语音
    {
        // not sure about right radius (检测进入 50 码视线)
        if (!m_hasDoneIntro && who->IsPlayer() && who->IsWithinDistInMap(m_creature, 50.0f) && who->IsWithinLOSInMap(m_creature))
        {
            DoScriptText(SAY_INTRO, m_creature);
            m_hasDoneIntro = true;
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void Aggro(Unit* /*who*/) override                          // 进入战斗：喊话、召唤五个传送门及召唤触发器
    {
        switch (urand(0, 2))
        {
            case 0: DoBroadcastText(SAY_AGGRO_1, m_creature); break;
            case 1: DoBroadcastText(SAY_AGGRO_2, m_creature); break;
            case 2: DoBroadcastText(SAY_AGGRO_3, m_creature); break;
        }

        for (uint32 spellId : aPortalSpells)
            DoCastSpellIfCan(nullptr, spellId);

        m_creature->CastSpell(nullptr, SPELL_SUMMON_VOID_SUMMONER, TRIGGERED_OLD_TRIGGERED);

        if (m_instance)
            m_instance->SetData(TYPE_VORPIL, IN_PROGRESS);
    }

    void JustReachedHome() override                             // 脱战回位回调：置状态为失败
    {
        if (m_instance)
            m_instance->SetData(TYPE_VORPIL, FAIL);
    }

    void JustSummoned(Creature* summoned) override              // 召唤物生成回调：为传送门附加光效并记录
    {
        if (summoned->GetEntry() == NPC_VOID_PORTAL)
            summoned->CastSpell(nullptr, SPELL_VOID_PORTAL_VISUAL, TRIGGERED_OLD_TRIGGERED);

        m_spawns.push_back(summoned->GetObjectGuid());
    }

    void JustDied(Unit* /*killer*/) override                    // 首领死亡回调：播放遗言、置状态为完成并清除传送门
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_VORPIL, DONE);

        DespawnGuids(m_spawns);
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override // 施法成功回调
    {
        if (spellInfo->Id == SPELL_DRAW_SHADOWS)
        {
            SetCombatMovement(false);
            SetCombatScriptStatus(true);
        }
    }
};

// 虚空召唤者触发器 AI：周期性从随机传送门召唤虚空旅行者
struct npc_voidwalker_summoner : public CombatAI
{
    npc_voidwalker_summoner(Creature* creature) : CombatAI(creature, 0), m_lastTimer(15000)
    {
        AddCustomAction(1, 11000u, [&]() { HandleTravellerSummon(); });
        SetReactState(REACT_PASSIVE);
        SetCombatMovement(false);
    }

    uint32 m_lastTimer;                                         // 召唤间隔递减计时器

    void JustSummoned(Creature* summoned) override              // 召唤旅行者回调：命令其走向沃匹尔
    {
        if (summoned->GetEntry() == NPC_VOID_TRAVELER && m_creature->GetInstanceData())
        {
            if (Unit* vorpil = m_creature->GetSpawner())
            {
                DoBroadcastText(SAY_HELP, vorpil);
                summoned->GetMotionMaster()->MoveChase(vorpil, 0.0f, 0.0f);
            }
        }
    }

    void HandleTravellerSummon()                                 // 执行召唤旅行者并递减下次间隔
    {
        DoCastSpellIfCan(m_creature, aTravelerSummonSpells[urand(0, 4)]);
        ResetTimer(1, m_lastTimer);
        m_lastTimer = std::max(5000u, m_lastTimer - 1500);
    }
};

// 虚空旅行者 AI：触碰沃匹尔时自爆治疗首领并造成暗影新星
struct npc_void_travelerAI : public CombatAI
{
    npc_void_travelerAI(Creature* creature) : CombatAI(creature, 0), m_isRegularMode(creature->GetMap()->IsRegularDifficulty()),
        m_hasExploded(false)
    {
        SetReactState(REACT_PASSIVE);
        AddCustomAction(1, true, [&]() { HandleInstakill(); });
        SetCombatScriptStatus(true);
    }

    bool m_isRegularMode;                                       // 是否为普通难度模式
    bool m_hasExploded;                                         // 是否已触发爆炸

    void MoveInLineOfSight(Unit* who) override                  // 靠近沃匹尔 3 码内触发献祭爆炸
    {
        if (!m_hasExploded && who->GetEntry() == NPC_VORPIL && who->IsWithinDistInMap(m_creature, 3.0f))
        {
            if (DoCastSpellIfCan(nullptr, SPELL_SHADOW_NOVA) == CAST_OK)
            {
                DoCastSpellIfCan(nullptr, m_isRegularMode ? SPELL_EMPOWERING_SHADOWS : SPELL_EMPOWERING_SHADOWS_H, CAST_TRIGGERED);
                m_hasExploded = true;
                ResetTimer(1, 1);
            }
        }
    }

    void HandleInstakill()                                       // 献祭后自杀销毁
    {
        m_creature->CastSpell(nullptr, SPELL_INSTAKILL_SELF, TRIGGERED_OLD_TRIGGERED);
    }
};

// 33558 - Draw Shadows (暗影牵引法术脚本：牵引结束后触发首领引导烈焰风暴)
struct DrawShadowsTrigger : public SpellScript
{
    void OnSuccessfulFinish(Spell* spell) const override
    {
        if (UnitAI* ai = spell->GetCaster()->AI())
            ai->SendAIEvent(AI_EVENT_CUSTOM_A, spell->GetCaster(), spell->GetCaster());
    }
};

// 注册沃匹尔大师及相关随从脚本
void AddSC_boss_grandmaster_vorpil()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_grandmaster_vorpil";
    pNewScript->GetAI = &GetNewAIInstance<boss_grandmaster_vorpilAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_voidwalker_summoner";
    pNewScript->GetAI = &GetNewAIInstance<npc_voidwalker_summoner>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_void_traveler";
    pNewScript->GetAI = &GetNewAIInstance<npc_void_travelerAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<DrawShadowsTrigger>("spell_draw_shadows_trigger");
}
