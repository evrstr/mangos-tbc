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
SDName: Boss_Anzu
SD%Complete: 80
SDComment:
SDCategory: Auchindoun, Sethekk Halls
EndScriptData */

// 安苏（乌鸦之神，塞泰克大厅英雄难度首领）
/* 遭遇战概述：
 * 安苏是塞泰克大厅英雄难度下的隐藏首领（德鲁伊史诗飞行形态任务召唤）；
 * 战斗中会施放撕裂肉体、尖啸、法术炸弹与旋风术；
 * 在生命值降至 70% 与 30% 时放逐自身并召唤群鸟（安苏之子）攻击玩家；
 * 玩家可激活场上的猛禽神灵（鹰、隼、雕之灵）获取强大的团队光环增益。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "sethekk_halls.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "Entities/TemporarySpawn.h"

// 台词、法术、随从与路径枚举
enum
{
    SAY_ANZU_INTRO_1    = 20797,                        // 出场台词 1
    SAY_ANZU_INTRO_2    = 20799,                        // 出场台词 2

    SAY_BANISH          = 20991,                        // 放逐自身台词
    SAY_WHISPER_MAGIC_1 = 21015,                        // 法术炸弹密语 1
    SAY_WHISPER_MAGIC_2 = 21016,                        // 法术炸弹密语 2
    SAY_WHISPER_MAGIC_3 = 21017,                        // 法术炸弹密语 3
    EMOTE_BIRD_STONE    = 20980,                        // 神灵石化表情提示

    // Intro spells (开场过场法术)
    SPELL_SHADOWFORM    = 37816,                        // 暗影形态

    // combat spells (战斗法术)
    SPELL_FLESH_RIP     = 40199,                        // 撕裂肉体
    SPELL_SCREECH       = 40184,                        // 刺耳尖啸
    SPELL_SPELL_BOMB    = 40303,                        // 法术炸弹
    SPELL_CYCLONE       = 40321,                        // 旋风术
    SPELL_BANISH_SELF   = 42354,                        // 放逐自身
    // prenerf (削弱前技能)
    SPELL_DIVE          = 40279,                        // 俯冲攻击

    // Brood spells (安苏之子法术)
    SPELL_HATE_TO_ZERO  = 9205,                         // 仇恨归零

    // Helper birds (协助猛禽神灵条目)
    NPC_HAWK_SPIRIT     = 23134,                // casts 40237 (鹰之灵)
    NPC_FALCON_SPIRIT   = 23135,                // casts 40241 (隼之灵)
    NPC_EAGLE_SPIRIT    = 23136,                // casts 40240 (雕之灵)

    // Bird spells (神灵法术)
    SPELL_IMPROVED_DURATION = 40250,                    // 延长持续时间
    SPELL_FREEZE_ANIM       = 16245,                    // 定身动画
    SPELL_SPIRIT_STONEFORM  = 40308,                    // 石化形态

    SPELL_PROTECTION_OF_THE_HAWK = 40237,               // 鹰之守护（减免伤害）
    SPELL_SPITE_OF_THE_EAGLE     = 40240,               // 雕之恶意（反射暗影伤害）
    SPELL_SPEED_OF_THE_FALCON    = 40241,               // 隼之迅捷（提升施法急速）

    MAX_BROODS          = 6,                            // 每次刷新的安苏之子数量

    PATH_ID_1           = 1,                            // 巡逻路径 1
    PATH_ID_2           = 2,                            // 巡逻路径 2
};

// 神灵生物条目列表
static const uint32 aSpiritsEntries[3] = {NPC_FALCON_SPIRIT, NPC_HAWK_SPIRIT, NPC_EAGLE_SPIRIT};
// 神灵坐标位置列表
static const Position aSpiritsPos[3] =
{
    Position(-72.34341f, 290.8609f, 26.48511f, 3.298672f ),
    Position(-96.48158f, 304.236f , 26.51348f, 5.235988f ),
    Position(-99.59056f, 276.6608f, 26.84667f, 0.7504916f)
};

// 安苏之子高空刷新点位
float spiritSpawns[][4] = 
{
    { -118.1717f, 284.5299f, 121.2287f, 2.775074f },
    { -87.59662f, 263.5181f, 92.70478f, 1.658063f },
    { -60.94724f, 272.2065f, 111.3746f, 5.759586f },
    { -98.15528f, 293.4469f, 109.2385f, 0.1745329f },
    { -99.7016f,  270.1699f, 98.27389f, 6.178465f },
    { -69.25543f, 303.0768f, 97.84479f, 5.532694f },
};

// 安苏战斗行为枚举
enum AnzuActions
{
    ANZU_ACTION_SPAWN_BROODS,                           // 召唤安苏之子动作
    ANZU_ACTION_BANISH,                                 // 放逐自身动作
    ANZU_COMBAT_ACTION_MAX,
    ANZU_INTRO_TALK,                                    // 开场喊话动作
    ANZU_INTRO_FLAGS,                                   // 开场解除免疫与暗影形态
    ANZU_BROOD_ATTACK,                                  // 群鸟转为主动攻击动作
};

// 乌鸦之神安苏 AI
struct boss_anzuAI : public CombatAI
{
    boss_anzuAI(Creature* creature) : CombatAI(creature, ANZU_COMBAT_ACTION_MAX),
        m_instance(static_cast<instance_sethekk_halls*>(creature->GetInstanceData()))
    {
        AddTimerlessCombatAction(ANZU_ACTION_SPAWN_BROODS, true);
        AddTimerlessCombatAction(ANZU_ACTION_BANISH, true);
        AddCustomAction(ANZU_INTRO_TALK, true, [&]
        {
            DoBroadcastText(SAY_ANZU_INTRO_2, m_creature); // is sent to despawned NPC_INVIS_RAVEN_GOD_TARGET in sniff (播放开场对话 2)
        });
        AddCustomAction(ANZU_INTRO_FLAGS, true, [&]
        {
            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PLAYER | UNIT_FLAG_IMMUNE_TO_NPC);
            m_creature->RemoveAurasDueToSpell(SPELL_SHADOWFORM);
        });
        AddCustomAction(ANZU_BROOD_ATTACK, true, [&]
        {
            for (ObjectGuid guid : m_broodGuidList)
                if (Creature* brood = m_creature->GetMap()->GetCreature(guid))
                    brood->AI()->SetReactState(REACT_AGGRESSIVE);
        });
    }

    instance_sethekk_halls* m_instance;                 // 副本数据管理指针

    float m_healthBroodCheck;                           // 召唤群鸟血量阈值（73% / 33%）
    float m_healthBanishCheck;                          // 放逐自身血量阈值（70% / 30%）

    GuidVector m_birdsGuidList;                         // 三大猛禽神灵 GUID 列表
    GuidVector m_broodGuidList;                         // 当前存活的安苏之子 GUID 列表

    void Reset() override                               // 遭遇战重置回调
    {
        CombatAI::Reset();

        m_healthBroodCheck = 73.f;
        m_healthBanishCheck = 70.f;
    }

    void JustRespawned() override                       // 首领生成初始化回调
    {
        CombatAI::JustRespawned();
        DoBroadcastText(SAY_ANZU_INTRO_1, m_creature, m_creature->GetSpawner());
        DoCastSpellIfCan(nullptr, SPELL_SHADOWFORM, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        DoSummonBirdHelpers();
        ResetTimer(ANZU_INTRO_TALK, 5000);
        ResetTimer(ANZU_INTRO_FLAGS, 10000);
    }

    void Aggro(Unit* /*who*/) override                  // 进入战斗回调：激活神灵并置状态为进行中
    {        
        EnableBirdHelpers();

        if (m_instance)
            m_instance->SetData(TYPE_ANZU, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override            // 击杀死亡回调：清理神灵并置状态为已完成
    {
        DespawnBirdHelpers();

        if (m_instance)
            m_instance->SetData(TYPE_ANZU, DONE);
    }

    void JustReachedHome() override                     // 脱战回位回调：清理神灵并强制消失
    {
        DespawnBirdHelpers();
        m_creature->ForcedDespawn();

        if (m_instance)
            m_instance->SetData(TYPE_ANZU, FAIL);
    }

    void JustSummoned(Creature* summoned) override      // 召唤物生成登记回调
    {
        if (summoned->GetEntry() == NPC_BROOD_OF_ANZU)
        {
            summoned->AI()->DoCastSpellIfCan(nullptr, SPELL_HATE_TO_ZERO, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
            summoned->AI()->SetReactState(REACT_DEFENSIVE);
            m_broodGuidList.push_back(summoned->GetObjectGuid());
        }
        else
        {
            summoned->AI()->DoCastSpellIfCan(nullptr, SPELL_IMPROVED_DURATION, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
            m_birdsGuidList.push_back(summoned->GetObjectGuid());
        }
    }

    void SummonedCreatureJustDied(Creature* summoned) override // 召唤物死亡回调：清空群鸟后解除自身放逐
    {
        if (summoned->GetEntry() == NPC_BROOD_OF_ANZU)
        {
            m_broodGuidList.erase(std::remove(m_broodGuidList.begin(), m_broodGuidList.end(), summoned->GetObjectGuid()), m_broodGuidList.end());
            if (m_broodGuidList.size() == 0)
                m_creature->RemoveAurasDueToSpell(SPELL_BANISH_SELF);
        }
    }

    void DoSummonBroodsOfAnzu()                          // 召唤安苏之子群鸟
    {
        if (!m_instance)
            return;

        for (uint8 i = 0; i < MAX_BROODS; ++i)
            m_creature->SummonCreature(NPC_BROOD_OF_ANZU, spiritSpawns[i][0], spiritSpawns[i][1], spiritSpawns[i][2], spiritSpawns[i][3], TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 30000, true, true, urand(1, 2));
    }

    void DoSummonBirdHelpers()                          // 召唤三只猛禽神灵图腾
    {
        for (uint8 i = 0; i < 3; ++i)
            m_creature->SummonCreature(aSpiritsEntries[i], aSpiritsPos[i].x, aSpiritsPos[i].y, aSpiritsPos[i].z, aSpiritsPos[i].o, TEMPSPAWN_CORPSE_DESPAWN, 0);
    }

    void EnableBirdHelpers()                            // 激活三只猛禽神灵
    {
        for (ObjectGuid guid : m_birdsGuidList)
            if (Creature* bird = m_creature->GetMap()->GetCreature(guid))
                bird->AI()->SendAIEvent(AI_EVENT_CUSTOM_B, m_creature, bird);
    }

    void DespawnBirdHelpers()                           // 驱散猛禽神灵
    {
        DespawnGuids(m_birdsGuidList);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override // 接收 AI 事件（放逐结束恢复近战）
    {
        if (eventType == AI_EVENT_CUSTOM_A) // end of banish (放逐结束)
            SetMeleeEnabled(true);
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* target) override // 施法成功触发台词
    {
        if (spellInfo->Id == SPELL_SPELL_BOMB)
        {
            switch (urand(0, 2))
            {
                case 0: DoBroadcastText(SAY_WHISPER_MAGIC_1, m_creature, target); break;
                case 1: DoBroadcastText(SAY_WHISPER_MAGIC_2, m_creature, target); break;
                case 2: DoBroadcastText(SAY_WHISPER_MAGIC_3, m_creature, target); break;
            }
        }
    }

    void ExecuteAction(uint32 i) override               // 执行群鸟召唤与放逐阶段动作
    {
        switch (i)
        {
            case ANZU_ACTION_SPAWN_BROODS:
                if (m_creature->GetHealthPercent() < m_healthBanishCheck && CanExecuteCombatAction())
                {
                    DoSummonBroodsOfAnzu();
                    m_healthBroodCheck -= 40.0f;
                }
                return;
            case ANZU_ACTION_BANISH:
                if (m_creature->GetHealthPercent() < m_healthBanishCheck && CanExecuteCombatAction())
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_BANISH_SELF) == CAST_OK)
                    {
                        DoBroadcastText(SAY_BANISH, m_creature);
                        SetMeleeEnabled(false);
                        m_healthBanishCheck -= 40.0f;
                        ResetTimer(ANZU_BROOD_ATTACK, 15000);
                        return;
                    }
                }
                return;
        }
    }
};

// 猛禽神灵 AI（鹰/隼/雕图腾）
struct npc_anzu_bird_spiritAI : public ScriptedAI
{
    npc_anzu_bird_spiritAI(Creature* creature) : ScriptedAI(creature), m_spiritSpellId(GetSpellId()), m_duration(0) {}

    uint32 m_spiritSpellId;                             // 神灵赋予的增益法术 ID
    uint32 m_duration;                                  // 当前增益激活持续时间
    uint32 m_refreshTimer;                              // 增益光环刷新计时器

    uint32 GetSpellId()                                 // 根据神灵生物条目获取对应法术
    {
        switch (m_creature->GetEntry())
        {
            case NPC_HAWK_SPIRIT: return SPELL_PROTECTION_OF_THE_HAWK;
            case NPC_FALCON_SPIRIT: return SPELL_SPEED_OF_THE_FALCON;
            case NPC_EAGLE_SPIRIT: return SPELL_SPITE_OF_THE_EAGLE;
            default: return 0;
        }
    }

    void Reset() override
    {

    }

    void FreezeSpirits()                                // 神灵进入石化休眠状态
    {
        DoCastSpellIfCan(nullptr, SPELL_FREEZE_ANIM, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        DoCastSpellIfCan(nullptr, SPELL_SPIRIT_STONEFORM, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        DoBroadcastText(EMOTE_BIRD_STONE, m_creature);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 miscValue) override // 接收唤醒事件
    {
        switch (eventType)
        {
            case AI_EVENT_CUSTOM_A:
            {
                if (m_duration == 0)
                {
                    m_creature->RemoveAurasDueToSpell(SPELL_FREEZE_ANIM);
                    m_creature->RemoveAurasDueToSpell(SPELL_SPIRIT_STONEFORM);
                }
                if (m_duration < miscValue)
                    m_duration = miscValue;
                break;
            }
            case AI_EVENT_CUSTOM_B:
            {
                FreezeSpirits();
                break;
            }
            default: break;
        }
    }

    void UpdateAI(const uint32 diff) override           // 神灵光环刷新步进
    {
        if (m_duration)
        {
            if (m_duration <= diff)
            {
                m_duration = 0;
                FreezeSpirits();
                return;
            }
            else m_duration -= diff;

            if (m_refreshTimer <= diff)
            {
                m_refreshTimer = 2000;
                DoCastSpellIfCan(nullptr, m_spiritSpellId);
            }
            else m_refreshTimer -= diff;
        }
    }
};

// 42354 - Banish Self (安苏自放逐光环脚本：放逐移除时通知首领)
struct AnzuBanishSelf : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply) // Anzu - Banish (放逐结束移除)
            if (UnitAI* ai = aura->GetTarget()->AI())
                ai->SendAIEvent(AI_EVENT_CUSTOM_A, aura->GetTarget(), aura->GetTarget());
    }
};

// 注册安苏及神灵脚本
void AddSC_boss_anzu()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_anzu";
    pNewScript->GetAI = &GetNewAIInstance<boss_anzuAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_anzu_bird_spirit";
    pNewScript->GetAI = &GetNewAIInstance<npc_anzu_bird_spiritAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<AnzuBanishSelf>("spell_anzu_banish_self");
}
