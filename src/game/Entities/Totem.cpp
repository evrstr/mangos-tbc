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

#include "Entities/Totem.h"
#include "Log/Log.h"
#include "Groups/Group.h"
#include "Entities/Player.h"
#include "Globals/ObjectMgr.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Server/DBCStores.h"
#include "AI/BaseAI/CreatureAI.h"
#include "Maps/InstanceData.h"

// 构造图腾对象，默认持续时间为 0、类型为被动图腾
Totem::Totem() : Creature(CREATURE_SUBTYPE_TOTEM)
{
    m_duration = 0;
    m_type = TOTEM_PASSIVE;
}

// 创建图腾实体：选取种族外观模型、修正高度、初始化法术列表与免疫标记
bool Totem::Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, Unit* owner)
{
    SetMap(cPos.GetMap());

    if (!CreateFromProto(guidlow, guidlow, cinfo))
        return false;

    // special model selection case for totems (根据玩家召唤者的种族选取对应的图腾外观模型)
    if (owner->GetTypeId() == TYPEID_PLAYER)
        if (uint32 modelid_race = sObjectMgr.GetModelForRace(GetNativeDisplayId(), owner->getRaceMask()))
            SetDisplayId(modelid_race);

    cPos.SelectFinalPoint(this, false);

    // totem must be at same Z in case swimming caster and etc. (若召唤者正在游泳等导致高度差过大，将图腾 Z 轴对齐召唤者)
    if (fabs(cPos.m_pos.z - owner->GetPositionZ()) > 5.0f)
        cPos.m_pos.z = owner->GetPositionZ();

    if (!cPos.Relocate(this))
        return false;

    // Notify the map's instance data.
    // Only works if you create the object in it, not if it is moves to that map.
    // Normally non-players do not teleport to other maps.
    // 通知副本地图脚本实例创建了生物对象（仅在当前地图创建时触发）
    if (InstanceData* iData = GetMap()->GetInstanceData())
        iData->OnCreatureCreate(this);

    LoadCreatureAddon(false);

    SetCanDodge(false);
    SetCanParry(false);
    SetCanBlock(false);

    if (GetCreatureInfo()->SpellList)
        SetSpellList(GetCreatureInfo()->SpellList);
    else // legacy compatibility (兼容旧版按 Entry*100 约定加载法术列表)
        SetSpellList(cinfo->Entry * 100 + 0);

    SetAOEImmune(true);
    SetChainImmune(true);

    return true;
}

// 每帧更新图腾存活状态与剩余持续时间，超时或主人死亡时自动收回
void Totem::Update(const uint32 diff)
{
    Unit* owner = GetOwner();
    if (!owner || !owner->IsAlive() || !IsAlive())
    {
        UnSummon();                                         // remove self (主人不存在/已死亡或图腾被摧毁时移除自身)
        return;
    }

    if (m_duration <= diff)
    {
        UnSummon();                                         // remove self (持续时间耗尽时移除自身)
        return;
    }
    else
        m_duration -= diff;

    Creature::Update(diff);
}

// 将图腾加入地图、播放生成动画并按类型触发被动/雕像法术
void Totem::Summon(Unit* owner)
{
    owner->GetMap()->Add((Creature*)this);
    AIM_Initialize();

    WorldPacket data(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE, 8);
    data << GetObjectGuid();
    SendMessageToSet(data, true);

    if (owner->AI())
        owner->AI()->JustSummoned((Creature*)this);

    // there are some totems, which exist just for their visual appeareance (部分图腾仅作视觉存在，其余则按类型施放关联法术)
    for (auto& data : m_spellList.Spells)
    {
        uint32 spellId = data.second.SpellId;
        if (!spellId)
            break;
        switch (m_type)
        {
            case TOTEM_PASSIVE:
                CastSpell(nullptr, spellId, TRIGGERED_OLD_TRIGGERED);
                break;
            case TOTEM_STATUE:
                CastSpell(GetOwner(), spellId, TRIGGERED_OLD_TRIGGERED);
                break;
            default: break;
        }
    }
}

// 解除召唤图腾：播放消失动画、清理主人与小队成员身上图腾光环并标记销毁
void Totem::UnSummon()
{
    SendObjectDeSpawnAnim(GetObjectGuid());

    CombatStop(true);
    RemoveAurasDueToSpell(GetSpell());

    AI()->OnUnsummon();

    if (Unit* owner = GetOwner())
    {
        owner->_RemoveTotem(this);
        owner->RemoveAurasDueToSpell(GetSpell());

        // remove aura all party members too (同时移除同小队成员身上由该图腾施加的光环)
        if (owner->GetTypeId() == TYPEID_PLAYER)
        {
            // Not only the player can summon the totem (scripted AI) (不仅玩家可召唤图腾，脚本 AI 也可召唤)
            if (Group* pGroup = ((Player*)owner)->GetGroup())
            {
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
                {
                    Player* Target = itr->getSource();
                    if (Target && pGroup->SameSubGroup((Player*)owner, Target))
                        Target->RemoveAurasDueToSpell(GetSpell());
                }
            }
        }

        if (owner->AI())
            owner->AI()->SummonedCreatureDespawn((Creature*)this);
    }

    // any totem unsummon look like as totem kill, req. for proper animation (任何图腾回收均按击杀处理以触发正确的客户端销毁动画)
    if (IsAlive())
        SetDeathState(DEAD);

    AddObjectToRemoveList();
}

// 获取图腾关联的主法术 ID（取法术列表第一项）
uint32 Totem::GetSpell() const
{
    if (m_spellList.Spells.empty())
        return 0;

    return m_spellList.Spells.begin()->second.SpellId;
}

// 根据召唤法术与图腾自身法术特征判定图腾类型（主动/被动/雕像）
void Totem::SetTypeBySummonSpell(SpellEntry const* spellProto)
{
    // Get spell casted by totem (获取图腾实际施放的法术)
    SpellEntry const* totemSpell = sSpellTemplate.LookupEntry<SpellEntry>(GetSpell());
    if (totemSpell)
    {
        // If spell have cast time -> so its active totem (若法术具有施法时间，则为主动攻击型图腾)
        if (GetSpellCastTime(totemSpell, this))
            m_type = TOTEM_ACTIVE;
    }
    if (spellProto->SpellIconID == 2056)
        m_type = TOTEM_STATUE;                              // Jewelery statue (珠宝雕像图标对应雕像类型)
}

// 返回图腾所属的玩家法术修正拥有者
Player* Totem::GetSpellModOwner() const
{
    Unit* owner = GetOwner();
    if (owner && owner->GetTypeId() == TYPEID_PLAYER)
        return static_cast<Player*>(owner);
    return nullptr;
}

// 获取图腾物理暴击率（继承主人暴击率）
float Totem::GetCritChance(WeaponAttackType attackType) const
{
    // Totems use owner's crit chance (when owner is available) (存在主人时继承主人的物理暴击率)
    if (const Unit* owner = GetOwner())
        return owner->GetCritChance(attackType);
    return Creature::GetCritChance(attackType);
}

// 获取图腾法术暴击率（继承主人法术暴击率）
float Totem::GetCritChance(SpellSchoolMask schoolMask) const
{
    // Totems use owner's crit chance (when owner is available) (存在主人时继承主人的法术暴击率)
    if (const Unit* owner = GetOwner())
        return owner->GetCritChance(schoolMask);
    return Creature::GetCritChance(schoolMask);
}

// 获取图腾暴击伤害/治疗倍率（继承主人暴击倍率）
float Totem::GetCritMultiplier(SpellSchoolMask dmgSchoolMask, uint32 creatureTypeMask, const SpellEntry* spell, bool heal) const
{
    // Totems use owner's crit multiplier (继承主人的暴击倍率)
    if (const Unit* owner = GetOwner())
        return owner->GetCritMultiplier(dmgSchoolMask, creatureTypeMask, spell, heal);
    return Creature::GetCritMultiplier(dmgSchoolMask, creatureTypeMask, spell, heal);
}

// 获取图腾物理命中率（继承主人命中率）
float Totem::GetHitChance(WeaponAttackType attackType) const
{
    // Totems use owner's hit chance (when owner is available) (存在主人时继承主人的物理命中率)
    if (const Unit* owner = GetOwner())
        return owner->GetHitChance(attackType);
    return Creature::GetHitChance(attackType);
}

// 获取图腾法术命中率（继承主人法术命中率）
float Totem::GetHitChance(SpellSchoolMask schoolMask) const
{
    // Totems use owner's hit chance (when owner is available) (存在主人时继承主人的法术命中率)
    if (const Unit* owner = GetOwner())
        return owner->GetHitChance(schoolMask);
    return Creature::GetHitChance(schoolMask);
}

// 图腾无固有物理未命中率
float Totem::GetMissChance(WeaponAttackType /*attackType*/) const
{
    // Totems have no inherit miss chance (图腾不具备固有物理未命中率)
    return 0.0f;
}

// 图腾无固有法术未命中率
float Totem::GetMissChance(SpellSchoolMask /*schoolMask*/) const
{
    // Totems have no inherit miss chance (图腾不具备固有法术未命中率)
    return 0.0f;
}

// 获取图腾抗性穿透值（继承主人抗性穿透）
int32 Totem::GetResistancePenetration(SpellSchools school) const
{
    // Totems use owner's penetration (when owner is available) (存在主人时继承主人的抗性穿透)
    if (const Unit* owner = GetOwner())
        return owner->GetResistancePenetration(school);
    return Creature::GetResistancePenetration(school);
}

// 判断图腾是否对指定法术效果免疫（免疫治疗/回蓝/嘲讽及所有负面/周期恢复光环，萨满回蓝回血图腾自身法术除外）
bool Totem::IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const
{
    // Totem may affected by some specific spells
    // Mana Spring, Healing stream, Mana tide
    // Flags : 0x00000002000 | 0x00000004000 | 0x00004000000 -> 0x00004006000
    // 法力之泉、治疗之泉与法力之潮图腾可作用于图腾自身
    if (spellInfo->SpellFamilyName == SPELLFAMILY_SHAMAN && spellInfo->IsFitToFamilyMask(uint64(0x00004006000)))
        return false;

    switch (spellInfo->Effect[index])
    {
        case SPELL_EFFECT_ATTACK_ME:
        // immune to any type of regeneration effects hp/mana etc. (免疫所有生命/法力恢复及强制攻击效果)
        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_HEAL_MAX_HEALTH:
        case SPELL_EFFECT_HEAL_MECHANICAL:
        case SPELL_EFFECT_HEAL_PCT:
        case SPELL_EFFECT_ENERGIZE:
        case SPELL_EFFECT_ENERGIZE_PCT:
            return true;
        default:
            break;
    }

    if (!IsPositiveSpell(spellInfo))
    {
        // immune to all negative auras (免疫所有负面光环)
        if (IsAuraApplyEffect(spellInfo, index))
            return true;
    }
    else
    {
        // immune to any type of regeneration auras hp/mana etc. (免疫所有周期性恢复类光环)
        if (IsPeriodicRegenerateEffect(spellInfo, index))
            return true;
    }

    return Creature::IsImmuneToSpellEffect(spellInfo, index, castOnSelf);
}
