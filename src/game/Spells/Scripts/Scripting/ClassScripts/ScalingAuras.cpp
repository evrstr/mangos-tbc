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

#include "Spells/Scripts/SpellScript.h"
#include "Spells/SpellAuras.h"

enum
{
    SPELL_PET_RESISTANCE = 37386,                           // 宠物抗性套装/光环加成
    SPELL_INCREASED_ATTACK_POWER = 38297,                   // 猎人套装额外宠物耐力/攻强/护甲加成
};

// 猎人宠物缩放光环 1：继承主人耐力（30%）、远程攻强（25%）与法强（12.5%）
struct HunterPetScaling1 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // stamina (耐力缩放)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                    if (owner->HasAura(SPELL_INCREASED_ATTACK_POWER))
                        value += 52;
                }
                break;
            case EFFECT_INDEX_1: // attack power (攻击强度缩放)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = owner->GetTotalAttackPowerValue(RANGED_ATTACK) * 0.25f;
                    if (owner->HasAura(SPELL_INCREASED_ATTACK_POWER))
                        value += 70;
                }
                break;
            case EFFECT_INDEX_2: // spell power (法术强度缩放)
                if (Unit* owner = data.caster->GetOwner())
                    value = owner->GetTotalAttackPowerValue(RANGED_ATTACK) * 0.125f;
                break;
        }
        return value;
    }
};

// 猎人宠物缩放光环 2：继承主人火、冰、自然抗性的 40%
struct HunterPetScaling2 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (火焰抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FIRE) * 0.4f);
                break;
            case EFFECT_INDEX_1: // resistance (冰霜抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FROST) * 0.4f);
                break;
            case EFFECT_INDEX_2: // resistance (自然抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_NATURE) * 0.4f);
                break;
        }
        return value;
    }
};

// 猎人宠物缩放光环 3：继承主人暗影/奥术抗性的 40% 及护甲的 35%
struct HunterPetScaling3 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (暗影抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_SHADOW) * 0.4f);
                break;
            case EFFECT_INDEX_1: // resistance (奥术抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_ARCANE) * 0.4f);
                break;
            case EFFECT_INDEX_2: // armor (护甲缩放)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = owner->GetArmor() * 0.35f;
                    if (owner->HasAura(SPELL_INCREASED_ATTACK_POWER))
                        value += 490;
                }
                break;
        }
        return value;
    }
};

// 术士宠物缩放光环 1：继承主人耐力（30%）及暗/火最高法伤换算攻强（57%）与法强（15%）
struct WarlockPetScaling1 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // stamina (耐力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // attack power (近战攻强)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 fire = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_FIRE))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_FIRE));
                        int32 shadow = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_SHADOW))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_SHADOW));
                        int32 maximum = (fire > shadow) ? fire : shadow;
                        if (maximum < 0)
                            maximum = 0;
                        value = maximum * 0.57f;
                    }
                }
                break;
            case EFFECT_INDEX_2: // spell power (法术强度)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 fire = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_FIRE))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_FIRE));
                        int32 shadow = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_SHADOW))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_SHADOW));
                        int32 maximum = (fire > shadow) ? fire : shadow;
                        if (maximum < 0)
                            maximum = 0;
                        value = maximum * 0.15f;
                    }
                }
                break;
        }
        return value;
    }
};

// 术士宠物缩放光环 2：智力、护甲（35%）与火焰抗性（40% + 套装加成）
struct WarlockPetScaling2 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // intelect (智力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // armor (护甲)
                if (Unit* owner = data.caster->GetOwner())
                    value = owner->GetArmor() * 0.35f;
                break;
            case EFFECT_INDEX_2: // resistance (火焰抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_FIRE) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
        }
        return value;
    }
};

// 术士宠物缩放光环 3：冰霜、奥术、自然抗性缩放
struct WarlockPetScaling3 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (冰霜抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_FROST) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
            case EFFECT_INDEX_1: // resistance (奥术抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_ARCANE) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
            case EFFECT_INDEX_2: // resistance (自然抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_NATURE) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
        }
        return value;
    }
};

// 术士宠物缩放光环 4：暗影抗性与法力恢复缩放
struct WarlockPetScaling4 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (暗影抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_SHADOW) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
            case EFFECT_INDEX_1: // mana regen (5 秒法力恢复缩放)
                if (Unit* owner = data.caster->GetOwner())
                    if (owner->IsPlayer())
                        value = static_cast<Player*>(owner)->GetFloatValue(PLAYER_FIELD_MOD_MANA_REGEN) * 0.5f * 5.f;
                break;
            default: break;
        }
        return value;
    }
};

// 法师水元素缩放光环 1：继承主人耐力（30%）与冰霜法伤（40%）
struct MagePetScaling1 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // stamina (耐力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // attack power (近战攻强，水元素不用)
                break;
            case EFFECT_INDEX_2: // spell power (法术强度)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 frost = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_FROST))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_FROST));
                        if (frost < 0)
                            frost = 0;
                        value = frost * 0.4f;
                    }
                }
                break;
        }
        return value;
    }
};

// 法师水元素缩放光环 2：智力、护甲与火焰抗性
struct MagePetScaling2 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // intelect (智力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // armor (护甲)
                if (Unit* owner = data.caster->GetOwner())
                    value = owner->GetArmor() * 0.35f;
                break;
            case EFFECT_INDEX_2: // resistance (火焰抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FIRE) * 0.4f);
                break;
        }
        return value;
    }
};

// 法师水元素缩放光环 3：冰霜、奥术、自然抗性
struct MagePetScaling3 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (冰霜抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FROST) * 0.4f);
                break;
            case EFFECT_INDEX_1: // resistance (奥术抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_ARCANE) * 0.4f);
                break;
            case EFFECT_INDEX_2: // resistance (自然抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_NATURE) * 0.4f);
                break;
        }
        return value;
    }
};

// 法师水元素缩放光环 4：暗影抗性
struct MagePetScaling4 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (暗影抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_SHADOW) * 0.4f);
                break;
            default: break;
        }
        return value;
    }
};

// 牧师暗影魔缩放光环 1：继承主人耐力（30%）与暗影法伤换算近战攻强（65%）
struct PriestPetScaling1 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // stamina (耐力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // attack power (攻击强度)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 shadow = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_SHADOW))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_SHADOW));
                        if (shadow < 0)
                            shadow = 0;
                        value = shadow * 0.65f;
                    }
                }
                break;
            case EFFECT_INDEX_2: // spell power
                break;
        }
        return value;
    }
};

// 牧师暗影魔缩放光环 2：智力、护甲与火焰抗性
struct PriestPetScaling2 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // intelect (智力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // armor (护甲)
                if (Unit* owner = data.caster->GetOwner())
                    value = owner->GetArmor() * 0.35f;
                break;
            case EFFECT_INDEX_2: // resistance (火焰抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FIRE) * 0.4f);
                break;
        }
        return value;
    }
};

// 牧师暗影魔缩放光环 3：冰霜、奥术、自然抗性
struct PriestPetScaling3 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (冰霜抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FROST) * 0.4f);
                break;
            case EFFECT_INDEX_1: // resistance (奥术抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_ARCANE) * 0.4f);
                break;
            case EFFECT_INDEX_2: // resistance (自然抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_NATURE) * 0.4f);
                break;
        }
        return value;
    }
};

// 牧师暗影魔缩放光环 4：暗影抗性
struct PriestPetScaling4 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (暗影抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_SHADOW) * 0.4f);
                break;
            default: break;
        }
        return value;
    }
};

// 萨满元素守卫缩放光环 1：自然法伤换算近战攻强（57%）与法强（65%）
struct ElementalPetScaling1 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // dummy - unk (未知占位)
                break;
            case EFFECT_INDEX_1: // attack power (近战攻强)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 nature = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_NATURE))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_NATURE));
                        if (nature < 0)
                            nature = 0;
                        value = nature * 0.57f;
                    }
                }
                break;
            case EFFECT_INDEX_2: // spell power (法术强度)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 nature = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_NATURE))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_NATURE));
                        if (nature < 0)
                            nature = 0;
                        value = nature * 0.65f;
                    }
                }
                break;
        }
        return value;
    }
};

// 萨满元素守卫缩放光环 2：智力、护甲与火焰抗性
struct ElementalPetScaling2 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // intelect (智力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // armor (护甲)
                if (Unit* owner = data.caster->GetOwner())
                    value = owner->GetArmor() * 0.35f;
                break;
            case EFFECT_INDEX_2: // resistance (火焰抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FIRE) * 0.4f);
                break;
        }
        return value;
    }
};

// 萨满元素守卫缩放光环 3：冰霜、奥术、自然抗性
struct ElementalPetScaling3 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (冰霜抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FROST) * 0.4f);
                break;
            case EFFECT_INDEX_1: // resistance (奥术抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_ARCANE) * 0.4f);
                break;
            case EFFECT_INDEX_2: // resistance (自然抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_NATURE) * 0.4f);
                break;
        }
        return value;
    }
};

// 萨满元素守卫缩放光环 4：暗影抗性
struct ElementalPetScaling4 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (暗影抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_SHADOW) * 0.4f);
                break;
            default: break;
        }
        return value;
    }
};

// 德鲁伊树人缩放光环 1：耐力（15%）与自然法伤换算近战攻强（50%）
struct DruidPetScaling1 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // stamina (耐力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.15f; // unconfirmed value
                break;
            case EFFECT_INDEX_1: // attack power (近战攻强)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 nature = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_NATURE))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_NATURE));
                        if (nature < 0)
                            nature = 0;
                        value = nature * 0.5f;
                    }
                }
                break;
            case EFFECT_INDEX_2: // spell power
                break;
        }
        return value;
    }
};

// 德鲁伊树人缩放光环 2：智力、护甲与火焰抗性
struct DruidPetScaling2 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // intelect (智力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // armor (护甲)
                if (Unit* owner = data.caster->GetOwner())
                    value = owner->GetArmor() * 0.35f;
                break;
            case EFFECT_INDEX_2: // resistance (火焰抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FIRE) * 0.4f);
                break;
        }
        return value;
    }
};

// 德鲁伊树人缩放光环 3：冰霜、奥术、自然抗性
struct DruidPetScaling3 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (冰霜抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FROST) * 0.4f);
                break;
            case EFFECT_INDEX_1: // resistance (奥术抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_ARCANE) * 0.4f);
                break;
            case EFFECT_INDEX_2: // resistance (自然抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_NATURE) * 0.4f);
                break;
        }
        return value;
    }
};

// 德鲁伊树人缩放光环 4：暗影抗性
struct DruidPetScaling4 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (暗影抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_SHADOW) * 0.4f);
                break;
            default: break;
        }
        return value;
    }
};

// 增强萨满宠物缩放光环 1：耐力（30%）、自然法伤换算近战攻强（50%）与法强（65%）
struct EnhancementPetScaling1 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // stamina (耐力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // attack power (近战攻强)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 nature = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_NATURE))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_NATURE));
                        if (nature < 0)
                            nature = 0;
                        value = nature * 0.5f;
                    }
                }
                break;
            case EFFECT_INDEX_2: // spell power (法术强度)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 nature = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_NATURE))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_NATURE));
                        if (nature < 0)
                            nature = 0;
                        value = nature * 0.65f;
                    }
                }
                break;
        }
        return value;
    }
};

// 增强萨满宠物缩放光环 2：火、冰、自然抗性
struct EnhancementPetScaling2 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (火焰抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FIRE) * 0.4f);
                break;
            case EFFECT_INDEX_1: // resistance (冰霜抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_FROST) * 0.4f);
                break;
            case EFFECT_INDEX_2: // resistance (自然抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_NATURE) * 0.4f);
                break;
        }
        return value;
    }
};

// 增强萨满宠物缩放光环 3：暗影抗性、奥术抗性与护甲
struct EnhancementPetScaling3 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (暗影抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_SHADOW) * 0.4f);
                break;
            case EFFECT_INDEX_1: // resistance (奥术抗性)
                if (Unit* owner = data.caster->GetOwner())
                    value = (owner->GetResistance(SPELL_SCHOOL_ARCANE) * 0.4f);
                break;
            case EFFECT_INDEX_2: // armor (护甲)
                if (Unit* owner = data.caster->GetOwner())
                    value = owner->GetArmor() * 0.35f;
                break;
        }
        return value;
    }
};

// 术士地狱火缩放光环 1：耐力（30%）、火/暗最高法伤换算近战攻强（57%）与法强（15%）
struct InfernalPetScaling1 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // stamina (耐力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // attack power (近战攻强)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 fire = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_FIRE))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_FIRE));
                        int32 shadow = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_SHADOW))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_SHADOW));
                        int32 maximum = (fire > shadow) ? fire : shadow;
                        if (maximum < 0)
                            maximum = 0;
                        value = maximum * 0.57f;
                    }
                }
                break;
            case EFFECT_INDEX_2: // spell power (法术强度)
                if (Unit* owner = data.caster->GetOwner())
                {
                    if (owner->IsPlayer())
                    {
                        int32 fire = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_FIRE))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_FIRE));
                        int32 shadow = int32(owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS) + static_cast<uint16>(SPELL_SCHOOL_SHADOW))) - owner->GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG) + static_cast<uint16>(SPELL_SCHOOL_SHADOW));
                        int32 maximum = (fire > shadow) ? fire : shadow;
                        if (maximum < 0)
                            maximum = 0;
                        value = maximum * 0.15f;
                    }
                }
                break;
        }
        return value;
    }
};

// 术士地狱火缩放光环 2：智力、护甲与火焰抗性
struct InfernalPetScaling2 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // intelect (智力)
                if (Unit* owner = data.caster->GetOwner())
                    value = float(owner->GetStat(STAT_STAMINA)) * 0.3f;
                break;
            case EFFECT_INDEX_1: // armor (护甲)
                if (Unit* owner = data.caster->GetOwner())
                    value = owner->GetArmor() * 0.35f;
                break;
            case EFFECT_INDEX_2: // resistance (火焰抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_FIRE) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
        }
        return value;
    }
};

// 术士地狱火缩放光环 3：冰霜、奥术、自然抗性
struct InfernalPetScaling3 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (冰霜抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_FROST) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
            case EFFECT_INDEX_1: // resistance (奥术抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_ARCANE) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
            case EFFECT_INDEX_2: // resistance (自然抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_NATURE) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
        }
        return value;
    }
};

// 术士地狱火缩放光环 4：暗影抗性与法力恢复
struct InfernalPetScaling4 : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        switch (data.effIdx)
        {
            case EFFECT_INDEX_0: // resistance (暗影抗性)
                if (Unit* owner = data.caster->GetOwner())
                {
                    value = (owner->GetResistance(SPELL_SCHOOL_SHADOW) * 0.4f);
                    if (owner->HasAura(SPELL_PET_RESISTANCE))
                        value += 130;
                }
                break;
            case EFFECT_INDEX_1: // mana regen (5 秒法力恢复)
                if (Unit* owner = data.caster->GetOwner())
                    if (owner->IsPlayer())
                        value = static_cast<Player*>(owner)->GetFloatValue(PLAYER_FIELD_MOD_MANA_REGEN) * 0.5f * 5.f;
                break;
            default: break;
        }
        return value;
    }
};

// 注册所有宠物属性缩放光环脚本
void LoadScalingScripts()
{
    RegisterSpellScript<HunterPetScaling1>("spell_hunter_pet_scaling_1");
    RegisterSpellScript<HunterPetScaling2>("spell_hunter_pet_scaling_2");
    RegisterSpellScript<HunterPetScaling3>("spell_hunter_pet_scaling_3");

    RegisterSpellScript<WarlockPetScaling1>("spell_warlock_pet_scaling_1");
    RegisterSpellScript<WarlockPetScaling2>("spell_warlock_pet_scaling_2");
    RegisterSpellScript<WarlockPetScaling3>("spell_warlock_pet_scaling_3");
    RegisterSpellScript<WarlockPetScaling4>("spell_warlock_pet_scaling_4");

    RegisterSpellScript<MagePetScaling1>("spell_mage_pet_scaling_1");
    RegisterSpellScript<MagePetScaling2>("spell_mage_pet_scaling_2");
    RegisterSpellScript<MagePetScaling3>("spell_mage_pet_scaling_3");
    RegisterSpellScript<MagePetScaling4>("spell_mage_pet_scaling_4");

    RegisterSpellScript<PriestPetScaling1>("spell_priest_pet_scaling_1");
    RegisterSpellScript<PriestPetScaling2>("spell_priest_pet_scaling_2");
    RegisterSpellScript<PriestPetScaling3>("spell_priest_pet_scaling_3");
    RegisterSpellScript<PriestPetScaling4>("spell_priest_pet_scaling_4");

    RegisterSpellScript<ElementalPetScaling1>("spell_elemental_pet_scaling_1");
    RegisterSpellScript<ElementalPetScaling2>("spell_elemental_pet_scaling_2");
    RegisterSpellScript<ElementalPetScaling3>("spell_elemental_pet_scaling_3");
    RegisterSpellScript<ElementalPetScaling4>("spell_elemental_pet_scaling_4");

    RegisterSpellScript<DruidPetScaling1>("spell_druid_pet_scaling_1");
    RegisterSpellScript<DruidPetScaling2>("spell_druid_pet_scaling_2");
    RegisterSpellScript<DruidPetScaling3>("spell_druid_pet_scaling_3");
    RegisterSpellScript<DruidPetScaling4>("spell_druid_pet_scaling_4");

    RegisterSpellScript<EnhancementPetScaling1>("spell_enhancement_pet_scaling_1");
    RegisterSpellScript<EnhancementPetScaling2>("spell_enhancement_pet_scaling_2");
    RegisterSpellScript<EnhancementPetScaling3>("spell_enhancement_pet_scaling_3");

    RegisterSpellScript<InfernalPetScaling1>("spell_infernal_pet_scaling_1");
    RegisterSpellScript<InfernalPetScaling2>("spell_infernal_pet_scaling_2");
    RegisterSpellScript<InfernalPetScaling3>("spell_infernal_pet_scaling_3");
    RegisterSpellScript<InfernalPetScaling4>("spell_infernal_pet_scaling_4");
}