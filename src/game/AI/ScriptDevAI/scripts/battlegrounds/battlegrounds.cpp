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
SDName: Battleground
SD%Complete: 100
SDComment: Spirit guides in battlegrounds will revive all players every 30 sec (战场灵魂向导每30秒复活周围所有玩家)
SDCategory: Battlegrounds
EndScriptData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "Spells/Scripts/SpellScript.h"
#include "OutdoorPvP/OutdoorPvP.h"

// **** Script Info ****
// Spiritguides in battlegrounds resurrecting many players at once
// every 30 seconds - through a channeled spell, which gets autocasted
// the whole time
// if spiritguide despawns all players around him will get teleported
// to the next spiritguide
// here i'm not sure, if a dummyspell exist for it
// 【脚本说明】
// 战场中的灵魂向导每 30 秒通过引导法术一次性复活多个玩家，该法术全程自动施放。
// 若灵魂向导消失，周围的所有玩家都将被传送到下一个灵魂向导处。

// **** Quick Info ****
// battleground spiritguides - this script handles gossipHello
// and JustDied also it let autocast the channel-spell
// 【快速概述】
// 战场灵魂向导：处理对话问候(gossipHello)以及尸体清除，同时自动引导复活法术。

enum
{
    SPELL_SPIRIT_HEAL_CHANNEL       = 22011,                // Spirit Heal Channel (灵魂复活引导)

    SPELL_SPIRIT_HEAL               = 22012,                // Spirit Heal (灵魂复活)
    SPELL_SPIRIT_HEAL_MANA          = 44535,                // in battlegrounds player get this no-mana-cost-buff (战场复活后无耗蓝增益)

    SPELL_WAITING_TO_RESURRECT      = 2584                  // players who cancel this aura don't want a resurrection (等待复活光环：取消此光环表示不想复活)
};

// 战场灵魂向导 AI (npc_spirit_guideAI)
struct npc_spirit_guideAI : public ScriptedAI
{
    npc_spirit_guideAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        pCreature->SetActiveObjectState(true);
        Reset();
    }

    void Reset() override {}

    // 每帧更新：保持自动引导复活法术
    void UpdateAI(const uint32 /*uiDiff*/) override
    {
        // auto cast the whole time this spell (全程持续自动施放复活引导法术)
        if (!m_creature->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            m_creature->CastSpell(m_creature, SPELL_SPIRIT_HEAL_CHANNEL, TRIGGERED_NONE);
    }

    // 接收 AI 自定义事件：打断引导并传送至墓地
    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override
    {
        if (eventType == AI_EVENT_CUSTOM_A)
        {
            m_creature->InterruptSpell(CURRENT_CHANNELED_SPELL);
            m_creature->CastSpell(nullptr, SPELL_GRAVEYARD_TELEPORT, TRIGGERED_OLD_TRIGGERED);
        }
    }

    // 尸体清除回调：向导消失时重新分配附近等待复活的玩家
    void CorpseRemoved(uint32&) override
    {
        // TODO: would be better to cast a dummy spell
        Map* pMap = m_creature->GetMap();

        if (!pMap || !pMap->IsBattleGround())
            return;

        Map::PlayerList const& PlayerList = pMap->GetPlayers();

        for (const auto& itr : PlayerList)
        {
            Player* pPlayer = itr.getSource();
            if (!pPlayer || !pPlayer->IsWithinDistInMap(m_creature, 20.0f) || !pPlayer->HasAura(SPELL_WAITING_TO_RESURRECT))
                continue;

            // repop player again - now this node won't be counted and another node is searched (玩家重新在其他墓地复活点出现)
            pPlayer->RepopAtGraveyard();
        }
    }

    // 复活法术命中玩家：附加免费法力消耗 Buff
    void SpellHitTarget(Unit* pUnit, const SpellEntry* pSpellEntry) override
    {
        if (pSpellEntry->Id == SPELL_SPIRIT_HEAL && pUnit->GetTypeId() == TYPEID_PLAYER
                && pUnit->HasAura(SPELL_WAITING_TO_RESURRECT))
            pUnit->CastSpell(pUnit, SPELL_SPIRIT_HEAL_MANA, TRIGGERED_OLD_TRIGGERED);
    }
};

// 与灵魂向导对话：赋予玩家等待复活光环
bool GossipHello_npc_spirit_guide(Player* pPlayer, Creature* /*pCreature*/)
{
    pPlayer->CastSpell(pPlayer, SPELL_WAITING_TO_RESURRECT, TRIGGERED_OLD_TRIGGERED);
    return true;
}

enum
{
    SPELL_OPENING_ANIM = 24390,                             // 开旗动画法术
};

// 占旗开始动画脚本
struct OpeningCapping : public SpellScript
{
    void OnSuccessfulStart(Spell* spell) const
    {
        spell->GetCaster()->CastSpell(nullptr, SPELL_OPENING_ANIM, TRIGGERED_OLD_TRIGGERED);
    }
};

// 战场限定检测脚本：仅允许在战场内施放
struct InactiveBattleground : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        Player* player = spell->GetCaster()->GetBeneficiaryPlayer();
        return player && player->InBattleGround() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_BATTLEGROUNDS;
    }
};

// 战场旗帜光环脚本：处理扛旗状态、免疫驱散与丢旗
struct FlagAuraBg : public AuraScript, public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_1)
        {
            // for EOS caster is player, for WSG caster is GO (风暴之眼施法者为玩家，战歌峡谷为游戏对象)
            if (spell->m_spellInfo->Id == 34976)
                spell->SetEventTarget(spell->GetUnitTarget());
            else
                spell->SetEventTarget(spell->GetAffectiveCasterObject());
        }
    }

    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        // procs on taken spell - if acquired immune flag, remove it - maybe other conditions too (若获得免疫状态则移除扛旗光环)
        if (procData.victim && procData.victim->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE))
            aura->GetTarget()->RemoveSpellAuraHolder(aura->GetHolder());
        return SPELL_AURA_PROC_OK;
    }

    // 光环生效/移除：更新玩家 PvP 扛旗标记，丢旗时触发战场或户外 PvP 丢旗逻辑
    void OnApply(Aura* aura, bool apply) const override
    {
        Unit* unitTarget = aura->GetTarget();
        if (!unitTarget || !unitTarget->IsPlayer())
            return;

        Player *player = static_cast<Player*>(unitTarget);

        if (apply)
            player->pvpInfo.isPvPFlagCarrier = true;
        else
        {
            player->pvpInfo.isPvPFlagCarrier = false;

            if (BattleGround* bg = player->GetBattleGround())
                bg->HandlePlayerDroppedFlag(player);
            else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
                outdoorPvP->HandleDropFlag(player, aura->GetSpellProto()->Id);
        }
    }
};

// 战场点击拿旗法术脚本
struct FlagClickBg : public SpellScript
{
    // 检测施法条件：地图匹配与战场状态
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        switch (spell->m_spellInfo->Id)
        {
            case 23333:                                         // Warsong Flag (战歌旗帜)
            case 23335:                                         // Silverwing Flag (银翼旗帜)
                return spell->GetTrueCaster()->GetMapId() == 489 && spell->GetTrueCaster()->GetMap()->IsBattleGround() ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA;
            case 34976:                                         // Netherstorm Flag (虚空风暴旗帜)
                return spell->GetTrueCaster()->GetMapId() == 566 && spell->GetTrueCaster()->GetMap()->IsBattleGround() ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA;
        }
        return SPELL_CAST_OK;
    }

    // 施法效果生效：捡起旗帜并施加扛旗状态
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* target = spell->GetUnitTarget();
        uint32 spellId = 0;
        switch (spell->m_spellInfo->Id)
        {
            case 23383: spellId = 23335; break; // Alliance Flag Pickup (捡起联盟旗帜)
            case 23384: spellId = 23333; break; // Horde Flag Pickup (捡起部落旗帜)
        }

        // flagstand and flagdrop share spells (插旗座与掉落旗帜共享法术)
        if (spell->GetTrueCaster()->IsGameObject() && static_cast<GameObject*>(spell->GetTrueCaster())->GetGoType() == GAMEOBJECT_TYPE_FLAGDROP)
        {
            if (WorldObject* spawner = static_cast<GameObject*>(spell->GetTrueCaster())->GetSpawner())
                if (spawner->IsPlayer() && target->IsPlayer())
                    if (static_cast<Player*>(spawner)->GetTeam() != static_cast<Player*>(target)->GetTeam())
                        return; // return of own flag already handled (己方旗帜归还已处理)
        }

        // misusing original caster to pass along original flag GO - if in future conflicts, substitute it for something else (借用施法者传递原始旗帜游戏对象)
        target->CastSpell(target, spellId, TRIGGERED_IGNORE_GCD | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL, nullptr, nullptr, spell->GetTrueCaster()->GetObjectGuid());
    }
};

// 竞技场准备阶段光环：让双方队伍在准备阶段互不可见
struct ArenaPreparation : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_1)
            if (apply)
                if (Unit* target = aura->GetTarget())
                    if (target->IsPlayer())
                        if (Player* p = static_cast<Player*>(target))
                            if (p->InArena() && p->GetBattleGround() && p->GetBGTeam() == HORDE && p->GetBattleGround()->GetStatus() == STATUS_WAIT_JOIN)
                                aura->GetModifier()->m_miscvalue = 5; // make teams invisible to eachother during prep phase (default value is 4) (准备阶段两队互不可见)
    }
};

/*#####
# spell_battleground_banner_trigger
#
# These are generic spells that handle player click on battleground banners; All spells are triggered by GO type 10
# Contains following spells:
# Arathi Basin: 23932, 23935, 23936, 23937, 23938
# Alterac Valley: 24677
# 通用战场旗帜触发法术：处理玩家点击战场旗帜
#####*/
struct spell_battleground_banner_trigger : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        // TODO: Fix when go casting is fixed
        WorldObject* obj = spell->GetAffectiveCasterObject();

        if (obj->IsGameObject() && spell->GetUnitTarget()->IsPlayer())
        {
            Player* player = static_cast<Player*>(spell->GetUnitTarget());
            if (BattleGround* bg = player->GetBattleGround())
                bg->HandlePlayerClickedOnFlag(player, static_cast<GameObject*>(obj));
        }
    }
};

/*#####
# spell_outdoor_pvp_banner_trigger
#
# These are generic spells that handle player click on outdoor PvP banners; All spells are triggered by GO type 10
# Contains following spells used in Zangarmarsh: 32433, 32438
# 通用野外 PvP 旗帜触发法术：赞加沼泽等野外占旗处理
#####*/
struct spell_outdoor_pvp_banner_trigger : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        // TODO: Fix when go casting is fixed
        WorldObject* obj = spell->GetAffectiveCasterObject();

        if (obj->IsGameObject() && spell->GetUnitTarget()->IsPlayer())
        {
            Player* player = static_cast<Player*>(spell->GetUnitTarget());

            if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
                outdoorPvP->HandleGameObjectUse(player, static_cast<GameObject*>(obj));
        }
    }
};

// 野外 PvP 通知游戏对象 AI
struct OutdoorPvpNotifyAI : public GameObjectAI
{
    using GameObjectAI::GameObjectAI;

    void OnUse(Unit* user, SpellEntry const* /*spellInfo*/) override
    {
        if (!user->IsPlayer())
            return;

        Player* player = static_cast<Player*>(user);
        if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
            outdoorPvP->HandleGameObjectUse(player, m_go);
    }
};

// 注册战场与 PvP 相关脚本
void AddSC_battleground()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_spirit_guide";
    pNewScript->GetAI = &GetNewAIInstance<npc_spirit_guideAI>;
    pNewScript->pGossipHello = &GossipHello_npc_spirit_guide;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_outdoor_pvp_notify";
    pNewScript->GetGameObjectAI = &GetNewAIInstance<OutdoorPvpNotifyAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<OpeningCapping>("spell_opening_capping");
    RegisterSpellScript<FlagAuraBg>("spell_flag_aura_bg");
    RegisterSpellScript<FlagClickBg>("spell_flag_click_bg");
    RegisterSpellScript<InactiveBattleground>("spell_inactive");
    RegisterSpellScript<ArenaPreparation>("spell_arena_preparation");
    RegisterSpellScript<spell_battleground_banner_trigger>("spell_battleground_banner_trigger");
    RegisterSpellScript<spell_outdoor_pvp_banner_trigger>("spell_outdoor_pvp_banner_trigger");
}
