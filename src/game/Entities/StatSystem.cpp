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

#include "Entities/Unit.h"
#include "Entities/Player.h"
#include "Entities/Pet.h"
#include "Entities/Creature.h"
#include "Globals/SharedDefines.h"
#include "Spells/SpellAuras.h"

/*#######################################
########                         ########
########   PLAYERS STAT SYSTEM   ########
########      玩家属性系统       ########
########                         ########
#######################################*/

// 更新指定基础属性（力量/敏捷/耐力/智力/精神）并级联刷新相关衍生属性
bool Player::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
        return false;

    // value = ((base_value * base_pct) + total_value) * total_pct (总属性值计算公式)
    float value  = GetTotalStatValue(stat);

    int32 oldValue = GetStat(stat);
    SetStat(stat, int32(value));

    // 耐力或智力变化时刷新宠物缩放光环
    if (oldValue != value)
        if (stat == STAT_STAMINA || stat == STAT_INTELLECT)
            if (Pet* pet = GetPet())
                pet->UpdateScalingAuras();

    switch (stat)
    {
        case STAT_STRENGTH:
            UpdateShieldBlockValue();                               // 力量影响盾牌格挡值
            break;
        case STAT_AGILITY:
            UpdateArmor();                                          // 敏捷影响护甲、物理暴击与躲闪
            UpdateAllCritPercentages();
            UpdateDodgePercentage();
            break;
        case STAT_STAMINA:   UpdateMaxHealth(); break;              // 耐力影响最大生命值
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);                             // 智力影响法力上限与法术暴击
            UpdateAllSpellCritChances();
            UpdateArmor();                                  // SPELL_AURA_MOD_RESISTANCE_OF_INTELLECT_PERCENT, only armor currently (按智力百分比加成抗性光环，当前仅作用于护甲)
            break;

        case STAT_SPIRIT:
            break;

        default:
            break;
    }
    // Need update (exist AP from stat auras) (存在属性转攻击强度光环，需同步刷新攻强与法伤/回蓝)
    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);

    UpdateSpellHealingBonus();
    UpdateSpellDamageBonus();
    UpdateManaRegen();

    return true;
}

// 更新客户端显示的治疗加成属性字段
void Player::UpdateSpellHealingBonus()
{
    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone (实际法术加成计算位于 Unit::SpellDamageBonusDone)
    // This information for client side use only (此字段仅用于客户端面板显示)
    // Get healing bonus for all schools (获取全系法术治疗加成)
    SetStatInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL));
}

// 更新客户端显示的各系法术伤害加成并刷新宠物缩放光环
void Player::UpdateSpellDamageBonus()
{
    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone (实际法术加成计算位于 Unit::SpellDamageBonusDone)
    // This information for client side use only (此字段仅用于客户端面板显示)
    // Get damage bonus for all schools (获取除物理外的各法术学派伤害加成)
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        SetStatInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, SpellBaseDamageBonusDone(SpellSchoolMask(1 << i)));

    if (Pet* pet = GetPet())
        pet->UpdateScalingAuras();
}

// 全量刷新玩家所有基础属性与衍生战斗属性
bool Player::UpdateAllStats()
{
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        float value = GetTotalStatValue(Stats(i));
        SetStat(Stats(i), (int32)value);
    }

    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);
    UpdateArmor();
    UpdateMaxHealth();

    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllRatings();
    UpdateAllCritPercentages();
    UpdateAllSpellCritChances();
    UpdateDefenseBonusesMod();
    UpdateShieldBlockValue();
    UpdateSpellHealingBonus();
    UpdateSpellDamageBonus();
    UpdateManaRegen();
    UpdateExpertise(BASE_ATTACK);
    UpdateExpertise(OFF_ATTACK);
    UpdateWeaponHitChances(BASE_ATTACK);
    UpdateWeaponHitChances(OFF_ATTACK);
    UpdateWeaponHitChances(RANGED_ATTACK);
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);

    return true;
}

// 更新指定法术学派抗性（物理系转入护甲更新）
void Player::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        int32 value = GetTotalResistanceValue(SpellSchools(school));
        int32 oldValue = GetResistance(SpellSchools(school));
        SetResistance(SpellSchools(school), value);

        if (value != oldValue)
            if (Pet* pet = GetPet())
                pet->UpdateScalingAuras();
    }
    else
        UpdateArmor();
}

// 更新玩家护甲值（含敏捷 2 倍护甲及属性按比例转护甲光环）
void Player::UpdateArmor()
{
    float dynamic = (GetStat(STAT_AGILITY) * 2.0f);

    // Add dynamic flat mods (累加基于属性百分比转化而来的动态护甲固定修正)
    for (auto& i : GetAurasByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT))
    {
        if (Modifier* mod = i->GetModifier())
        {
            if (mod->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL)
                dynamic += (GetStat(Stats(i->GetMiscBValue())) * (mod->m_amount * 0.01f));
        }
    }

    m_auraModifiersGroup[UNIT_MOD_ARMOR][TOTAL_VALUE] += dynamic;
    int32 value = GetTotalResistanceValue(SPELL_SCHOOL_NORMAL);
    int32 oldValue = GetArmor();
    SetArmor(value);
    m_auraModifiersGroup[UNIT_MOD_ARMOR][TOTAL_VALUE] -= dynamic;

    if (value != oldValue)
        if (Pet* pet = GetPet())
            pet->UpdateScalingAuras();
}

// 根据耐力计算生命值加成（前 20 点 1:1，超出部分 1:10）
float Unit::GetHealthBonusFromStamina(float stamina)
{
    float baseStam = stamina < 20 ? stamina : 20;
    float moreStam = stamina - baseStam;

    return baseStam + (moreStam * 10.0f);
}

// 获取当前单位由耐力提供的生命值加成
float Unit::GetHealthBonusFromStamina() const
{
    float stamina = GetStat(STAT_STAMINA);

    return Unit::GetHealthBonusFromStamina(stamina);
}

// 根据智力计算法力值加成（前 20 点 1:1，超出部分 1:15）
float Unit::GetManaBonusFromIntellect(float intellect)
{
    float baseInt = intellect < 20 ? intellect : 20;
    float moreInt = intellect - baseInt;

    return baseInt + (moreInt * 15.0f);
}

// 获取当前单位由智力提供的法力值加成
float Unit::GetManaBonusFromIntellect() const
{
    float intellect = GetStat(STAT_INTELLECT);

    return Unit::GetManaBonusFromIntellect(intellect);
}

// 重新计算并设置单位最大生命值
void Unit::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + GetHealthBonusFromStamina();
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxHealth(uint32(std::round(std::max(value, 1.f))));
}

// 重新计算并设置指定能量类型的上限
void Unit::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(static_cast<uint32>(UNIT_MOD_POWER_START) + static_cast<uint32>(power));

    uint32 create_power = GetCreatePowers(power);

    // ignore classes without mana (忽略无基础法力值的职业)
    float bonusPower = (power == POWER_MANA && create_power > 0) ? GetManaBonusFromIntellect() : 0;

    float value = GetModifierValue(unitMod, BASE_VALUE) + create_power;
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) +  bonusPower;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxPower(power, uint32(std::round(value)));
}

// 更新玩家近战或远程攻击强度，并联动更新武器物理伤害与宠物属性
void Player::UpdateAttackPowerAndDamage(bool ranged)
{
    float val2 = 0.0f;
    float level = float(GetLevel());

    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 index_mod = UNIT_FIELD_ATTACK_POWER_MODS;
    uint16 index_mult = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;
    AttackPowerMod mod = AttackPowerMod::MELEE_ATTACK_POWER;

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        index_mod = UNIT_FIELD_RANGED_ATTACK_POWER_MODS;
        index_mult = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;

        mod = AttackPowerMod::RANGED_ATTACK_POWER;

        switch (getClass())
        {
            case CLASS_HUNTER: val2 = level * 2.0f + GetStat(STAT_AGILITY) - 10.0f;    break;
            case CLASS_ROGUE:  val2 = level        + GetStat(STAT_AGILITY) - 10.0f;    break;
            case CLASS_WARRIOR: val2 = level        + GetStat(STAT_AGILITY) - 10.0f;    break;
            case CLASS_DRUID:
                switch (GetShapeshiftForm())
                {
                    case FORM_CAT:
                    case FORM_BEAR:
                    case FORM_DIREBEAR:
                        val2 = 0.0f; break;
                    default:
                        val2 = GetStat(STAT_AGILITY) - 10.0f; break;
                }
                break;
            default: val2 = GetStat(STAT_AGILITY) - 10.0f; break;
        }
    }
    else
    {
        switch (getClass())
        {
            case CLASS_WARRIOR:      val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f                    - 20.0f; break;
            case CLASS_PALADIN:      val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f                    - 20.0f; break;
            case CLASS_ROGUE:        val2 = level * 2.0f + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY)   - 20.0f; break;
            case CLASS_HUNTER:       val2 = level * 2.0f + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY)   - 20.0f; break;
            case CLASS_SHAMAN:       val2 = level * 2.0f + GetStat(STAT_STRENGTH) * 2.0f                    - 20.0f; break;
            case CLASS_DRUID:
            {
                val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                if (ShapeshiftForm form = GetShapeshiftForm())
                    if (SpellShapeshiftFormEntry const* entry = sSpellShapeshiftFormStore.LookupEntry(form))
                        if (entry->flags1 & SHAPESHIFT_FLAG_AGILITY_ATTACK_BONUS)
                            val2 += GetStat(STAT_AGILITY);
                break;
            }
            case CLASS_MAGE:    val2 =              GetStat(STAT_STRENGTH)                         - 10.0f; break;
            case CLASS_PRIEST:  val2 =              GetStat(STAT_STRENGTH)                         - 10.0f; break;
            case CLASS_WARLOCK: val2 =              GetStat(STAT_STRENGTH)                         - 10.0f; break;
        }
    }

    SetModifierValue(unitMod, BASE_VALUE, val2);

    float base_attPower  = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float statBonus = 0.f;

    // add dynamic flat mods (累加按基础属性百分比转化的远程攻强固定加成)
    if (ranged)
    {
        if ((getClassMask() & CLASSMASK_WAND_USERS) == 0)
        {
            AuraList const& mRAPbyStat = GetAurasByType(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT);
            for (auto i : mRAPbyStat)
                statBonus += int32(GetStat(Stats(i->GetModifier()->m_miscvalue)) * i->GetModifier()->m_amount / 100.0f);
        }
    }

    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    SetInt32Value(index, (uint32)base_attPower);            // UNIT_FIELD_(RANGED)_ATTACK_POWER field (写入基础攻强字段)
    SetInt16Value(index_mod, 0, int32(m_attackPowerMod[size_t(mod)][size_t(AttackPowerModSign::MOD_SIGN_POS)] + statBonus));
    SetInt16Value(index_mod, 1, m_attackPowerMod[size_t(mod)][size_t(AttackPowerModSign::MOD_SIGN_NEG)]);
    SetFloatValue(index_mult, attPowerMultiplier);          // UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field (写入攻强百分比乘数)

    // automatically update weapon damage after attack power modification (攻强变更后自动刷新对应武器物理伤害)
    if (ranged)
    {
        UpdateDamagePhysical(RANGED_ATTACK);

        if (Pet* pet = GetPet()) // update pet's AP (同步刷新宠物缩放属性)
            pet->UpdateScalingAuras();
    }
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        if (CanDualWield() && hasOffhandWeaponForAttack())          // allow update offhand damage only if player knows DualWield Spec and has equipped offhand weapon (仅当习得双持且装备副手武器时更新副手伤害)
            UpdateDamagePhysical(OFF_ATTACK);
    }
}

// 更新玩家盾牌格挡值
void Player::UpdateShieldBlockValue()
{
    SetUInt32Value(PLAYER_SHIELD_BLOCK, GetShieldBlockValue());
}

// 增减指定攻击槽位的武器附魔固定伤害修正
void Player::SetEnchantmentModifier(uint32 value, WeaponAttackType attType, bool apply)
{
    if (apply)
        m_enchantmentFlatMod[attType] += value;
    else
        m_enchantmentFlatMod[attType] -= value;
}

// 获取指定攻击槽位的武器附魔固定伤害修正
uint32 Player::GetEnchantmentModifier(WeaponAttackType attType)
{
    return m_enchantmentFlatMod[attType];
}

// 计算玩家指定攻击槽位的最小/最大物理武器伤害（支持标准攻速归一化）
void Player::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, float& min_damage, float& max_damage, uint8 index)
{
    UnitMods unitMod;

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    float att_speed = GetAPMultiplier(attType, normalized);

    float base_value  = GetModifierValue(unitMod, BASE_VALUE) + GetTotalAttackPowerValue(attType) / 14.0f * att_speed;
    float base_pct    = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct   = GetModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetBaseWeaponDamage(attType, MINDAMAGE, index);
    float weapon_maxdamage = GetBaseWeaponDamage(attType, MAXDAMAGE, index);

    if (IsAttackSpeedOverridenShapeShift()) // forms with no override on attack speed use normal weapon damage (覆盖攻速的变形形态使用等级缩放空手伤害，否则使用武器伤害)
    {
        uint32 lvl = GetLevel();
        if (lvl > 60)
            lvl = 60;

        weapon_mindamage = lvl * 0.85f * att_speed;
        weapon_maxdamage = lvl * 1.25f * att_speed;
    }
    else if (!CanUseEquippedWeapon(attType))                // check if player not in form but still can't use weapon (broken/etc) (武器损坏或不可用时退化为基础徒手伤害)
    {
        weapon_mindamage = BASE_MINDAMAGE;
        weapon_maxdamage = BASE_MAXDAMAGE;
    }
    else
    {
        total_value += GetEnchantmentModifier(attType);
        if (attType == RANGED_ATTACK)                      // add ammo DPS to ranged damage (远程攻击叠加弹药秒伤加成)
        {
            auto ammoDps = GetAmmoDPS();
            weapon_mindamage += ammoDps.first * att_speed;
            weapon_maxdamage += ammoDps.second * att_speed;
        }

        if (index != 0)
        {
            base_value = 0.0f;
            total_value = 0.0f;
        }
    }

    min_damage = ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    max_damage = ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;
}

// 刷新玩家对应攻击槽位的物理伤害面板字段
void Player::UpdateDamagePhysical(WeaponAttackType attType)
{
    float mindamage;
    float maxdamage;

    CalculateMinMaxDamage(attType, false, mindamage, maxdamage);

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            SetStatFloatValue(UNIT_FIELD_MINDAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, maxdamage);
            break;
        case OFF_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, maxdamage);
            break;
        case RANGED_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, maxdamage);
            break;
    }
}

// 统一刷新格挡、招架、躲闪三项防御属性
void Player::UpdateDefenseBonusesMod()
{
    UpdateBlockPercentage();
    UpdateParryPercentage();
    UpdateDodgePercentage();
}

// 计算并更新格挡几率
void Player::UpdateBlockPercentage()
{
    float value = 0.0f;
    float real = 0.0f;
    if (CanBlock())
    {
        // Base value (基础格挡率 5%)
        value = 5.0f;
        // Increase from SPELL_AURA_MOD_BLOCK_PERCENT aura (累加格挡百分比光环)
        value += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);
        // Increase from rating (累加格挡等级转换)
        value += GetRatingBonusValue(CR_BLOCK);
        real = value;
        // Set UI display value: modify value from defense skill against same level target (面板显示值附加与同级目标对比的防御技能修正)
        value += (int32(GetDefenseSkillValue()) - int32(GetSkillMaxForLevel())) * 0.04f;
    }
    m_modBlockChance = real;
    SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, std::max(0.0f, std::min(value, 100.0f)));
}

// 计算并更新指定攻击槽位的物理暴击率
void Player::UpdateCritPercentage(WeaponAttackType attType)
{
    BaseModGroup modGroup;
    uint16 index;
    CombatRating cr;

    switch (attType)
    {
        case OFF_ATTACK:
            modGroup = OFFHAND_CRIT_PERCENTAGE;
            index = PLAYER_OFFHAND_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
        case RANGED_ATTACK:
            modGroup = RANGED_CRIT_PERCENTAGE;
            index = PLAYER_RANGED_CRIT_PERCENTAGE;
            cr = CR_CRIT_RANGED;
            break;
        case BASE_ATTACK:
        default:
            modGroup = CRIT_PERCENTAGE;
            index = PLAYER_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
    }

    float value = GetTotalPercentageModValue(modGroup) + GetRatingBonusValue(cr);
    m_modCritChance[attType] = value;
    // Modify crit from weapon skill and maximized defense skill of same level victim difference (面板显示值按武器技能与同级目标防御技能差值修正)
    value += (int32(GetWeaponSkillValue(attType)) - int32(GetSkillMaxForLevel())) * 0.04f;
    SetStatFloatValue(index, std::max(0.0f, std::min(value, 100.0f)));
}

// 刷新主手、副手与远程全部物理暴击率
void Player::UpdateAllCritPercentages()
{
    float value = GetMeleeCritFromAgility();

    SetBaseModValue(CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(OFFHAND_CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(RANGED_CRIT_PERCENTAGE, PCT_MOD, value);

    UpdateCritPercentage(BASE_ATTACK);
    UpdateCritPercentage(OFF_ATTACK);
    UpdateCritPercentage(RANGED_ATTACK);
}

// 计算并更新招架几率
void Player::UpdateParryPercentage()
{
    float value = 0.0f;
    float real = 0.0f;
    if (CanParry())
    {
        // Base parry (基础招架率 5%)
        value  = 5.0f;
        // Parry from SPELL_AURA_MOD_PARRY_PERCENT aura (累加招架百分比光环)
        value += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);
        // Parry from rating (累加招架等级转换)
        value += GetRatingBonusValue(CR_PARRY);
        real = value;
        // Set UI display value: modify value from defense skill against same level target (面板显示值附加与同级目标对比的防御技能修正)
        value += (int32(GetDefenseSkillValue()) - int32(GetSkillMaxForLevel())) * 0.04f;
    }
    // Set current dodge chance (保存实际招架几率并写入面板字段)
    m_modParryChance = real;
    SetStatFloatValue(PLAYER_PARRY_PERCENTAGE, std::max(0.0f, std::min(value, 100.0f)));
}

// Base static dodge values in percentages (%) (各职业静态基础躲闪率常量表，单位：%)
static const float PLAYER_BASE_DODGE[MAX_CLASSES] =
{
    0.0000f, // [0]  <Unused>
    0.7500f, // [1]  Warrior
    0.6520f, // [2]  Paladin
    -5.4500f, // [3]  Hunter
    -0.5900f, // [4]  Rogue
    3.1830f, // [5]  Priest
    1.1400f, // [6]  DK
    1.6700f, // [7]  Shaman
    3.4575f, // [8]  Mage
    2.0110f, // [9]  Warlock
    0.0000f, // [10] <Unused>
    -1.8700f, // [11] Druid
};

// 计算并更新躲闪几率
void Player::UpdateDodgePercentage()
{
    // Base dodge (职业基础躲闪率)
    float value = (getClass() < MAX_CLASSES) ? PLAYER_BASE_DODGE[getClass()] : 0.0f;
    // Dodge from agility (敏捷转换躲闪)
    value += GetDodgeFromAgility(GetStat(STAT_AGILITY));
    // Dodge from SPELL_AURA_MOD_DODGE_PERCENT aura (躲闪百分比光环加成)
    value += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);
    // Dodge from rating (躲闪等级转换加成)
    value += GetRatingBonusValue(CR_DODGE);
    // Set current dodge chance (保存当前实际躲闪几率)
    m_modDodgeChance = value;
    // Set UI display value: modify value from defense skill against same level target (面板显示值附加与同级目标对比的防御技能修正)
    value += (int32(GetDefenseSkillValue()) - int32(GetSkillMaxForLevel())) * 0.04f;
    SetStatFloatValue(PLAYER_DODGE_PERCENTAGE, std::max(0.0f, std::min(value, 100.0f)));
}

// 计算并更新指定法术学派的法术暴击率
void Player::UpdateSpellCritChance(uint32 school)
{
    float crit = 0.0f;
    // Base spell crit and spell crit from Intellect (基础法术暴击及智力转换暴击)
    crit += GetSpellCritFromIntellect();
    // Increase crit from SPELL_AURA_MOD_SPELL_CRIT_CHANCE (全系法术暴击光环加成)
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_CRIT_CHANCE);
    // Increase crit by school from SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL (单系法术暴击光环加成)
    crit += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, (1 << school));
    // Increase crit from spell crit ratings (法术暴击等级转换加成)
    crit += GetRatingBonusValue(CR_CRIT_SPELL);
    // Set current crit chance (保存实际暴击几率)
    m_modSpellCritChance[school] = crit;
    // Set UI display value: (写入面板显示字段，钳位在 0%~100%)
    SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + school, std::max(0.0f, std::min(crit, 100.0f)));
}

// 计算并更新指定武器槽位的物理命中几率加成
void Player::UpdateWeaponHitChances(WeaponAttackType attType)
{
    int32 weaponHitChance = 0;
    Item* weapon = GetWeaponForAttack(attType);

    AuraList const& hitAura = GetAurasByType(SPELL_AURA_MOD_HIT_CHANCE);
    for (auto hitAura : hitAura)
    {
        // item neutral spell (不限定武器类型的命中光环)
        if (hitAura->GetSpellProto()->EquippedItemClass == -1)
            weaponHitChance += hitAura->GetModifier()->m_amount;
        // item dependent spell (需匹配当前已装备武器类型的命中光环)
        else if (weapon && weapon->IsFitToSpellRequirements(hitAura->GetSpellProto()))
            weaponHitChance += hitAura->GetModifier()->m_amount;
    }
    m_modWeaponHitChance[attType] = weaponHitChance;
    m_modWeaponHitChance[attType] += attType == RANGED_ATTACK ? GetRatingBonusValue(CR_HIT_RANGED) : GetRatingBonusValue(CR_HIT_MELEE);
}

// 计算并更新法术命中几率加成
void Player::UpdateSpellHitChances()
{
    m_modSpellHitChance = GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_HIT_CHANCE);
    m_modSpellHitChance += GetRatingBonusValue(CR_HIT_SPELL);
}

// 刷新全部法术学派的暴击几率
void Player::UpdateAllSpellCritChances()
{
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateSpellCritChance(i);
}

// 计算并更新主手/副手精准值
void Player::UpdateExpertise(WeaponAttackType attack)
{
    if (attack == RANGED_ATTACK)
        return;

    int32 expertise = int32(GetRatingBonusValue(CR_EXPERTISE));

    Item* weapon = GetWeaponForAttack(attack);

    AuraList const& expAuras = GetAurasByType(SPELL_AURA_MOD_EXPERTISE);
    for (auto expAura : expAuras)
    {
        // item neutral spell (不限定武器类型的精准光环)
        if (expAura->GetSpellProto()->EquippedItemClass == -1)
            expertise += expAura->GetModifier()->m_amount;
        // item dependent spell (需匹配武器类型的精准光环)
        else if (weapon && weapon->IsFitToSpellRequirements(expAura->GetSpellProto()))
            expertise += expAura->GetModifier()->m_amount;
    }

    if (expertise < 0)
        expertise = 0;

    switch (attack)
    {
        case BASE_ATTACK: SetUInt32Value(PLAYER_EXPERTISE, expertise);         break;
        case OFF_ATTACK:  SetUInt32Value(PLAYER_OFFHAND_EXPERTISE, expertise); break;
        default: break;
    }
}

// 更新法力恢复速率（含精神智力回蓝、MP5 与施法打断回蓝比例）
void Player::UpdateManaRegen()
{
    // need to award mana based on previous rate - Patch 2.2 (改变回蓝速率前先按旧速率结算已累积的回复量 - Patch 2.2)
    if (GetHealth() > 0) // on death we must never do this (死亡状态下不得触发回复结算)
        RegenerateAll(std::min(uint32(REGEN_TIME_FULL), m_regenTimer));

    float Intellect = GetStat(STAT_INTELLECT);
    // Mana regen from spirit and intellect (精神与智力共同决定的基础法力回复)
    float power_regen = sqrt(Intellect) * OCTRegenMPPerSpirit();
    // Apply PCT bonus from SPELL_AURA_MOD_POWER_REGEN_PERCENT aura on spirit base regen (对精神基础回蓝应用百分比光环乘数)
    power_regen *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

    // Mana regen from SPELL_AURA_MOD_POWER_REGEN aura (每 5 秒固定回蓝 MP5 折算为每秒回复)
    float power_regen_mp5 = GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) / 5.0f;

    // Get bonus from SPELL_AURA_MOD_MANA_REGEN_FROM_STAT aura (按基础属性百分比提供的固定回蓝，如智慧祝福/奥术冥想等转换)
    AuraList const& regenAura = GetAurasByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);
    for (auto i : regenAura)
    {
        Modifier* mod = i->GetModifier();
        power_regen_mp5 += GetStat(Stats(mod->m_miscvalue)) * mod->m_amount / 500.0f;
    }

    // Set regen rate in cast state apply only on spirit based regen (施法状态下仅精神回蓝受五秒规则/打断比例影响，MP5 不受限制)
    int32 modManaRegenInterrupt = GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
    if (modManaRegenInterrupt > 100)
        modManaRegenInterrupt = 100;
    SetStatFloatValue(PLAYER_FIELD_MOD_MANA_REGEN_INTERRUPT, power_regen_mp5 + power_regen * modManaRegenInterrupt / 100.0f);

    float value = power_regen_mp5 + power_regen;
    float oldValue = GetFloatValue(PLAYER_FIELD_MOD_MANA_REGEN);
    SetStatFloatValue(PLAYER_FIELD_MOD_MANA_REGEN, value);

    if (value != oldValue)
        if (Pet* pet = GetPet())
            pet->UpdateScalingAuras();
}

// 更新能量恢复速率倍率
void Player::UpdateEnergyRegen()
{
    // need to award mana based on previous rate - Patch 2.2 (速率变化前先按旧速率结算已累积的能量回复)
    if (GetHealth() > 0) // on death we must never do this (死亡状态下不得触发回复结算)
        RegenerateAll(std::min(uint32(REGEN_TIME_FULL), m_regenTimer));

    m_energyRegenRate = GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_ENERGY);
}

// 武器更换后刷新依赖武器类型的相关属性（命中、精准、弹药急速）
void Player::UpdateWeaponDependantStats(WeaponAttackType attType)
{
    switch (attType)
    {
        case BASE_ATTACK:
            UpdateWeaponHitChances(attType);
            UpdateExpertise(attType);
            break;
        case OFF_ATTACK:
            UpdateWeaponHitChances(attType);
            UpdateExpertise(attType);
            break;
        case RANGED_ATTACK:
            UpdateWeaponHitChances(attType);
            UpdateRangedWeaponDependantAmmoHasteAura();
            break;
    }
}

// 批量应用所有光环和装备属性加成并触发全量属性重算
void Player::_ApplyAllStatBonuses()
{
    SetCanModifyStats(false);

    _ApplyAllAuraMods();
    _ApplyAllItemMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

// 批量移除所有装备和光环属性加成并触发全量属性重算
void Player::_RemoveAllStatBonuses()
{
    SetCanModifyStats(false);

    _RemoveAllItemMods();
    _RemoveAllAuraMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

/*#######################################
########                         ########
########    MOBS STAT SYSTEM     ########
########     怪物属性系统        ########
########                         ########
#######################################*/

// 更新生物指定基础属性并联动更新衍生战斗属性
bool Creature::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
        return false;

    // value = ((base_value * base_pct) + total_value) * total_pct (总属性值计算公式)
    float value = GetTotalStatValue(stat);

    float oldValue = GetStat(stat);
    SetStat(stat, int32(value));

    switch (stat)
    {
        case STAT_STRENGTH:         UpdateAttackPowerAndDamage();        break;
        case STAT_AGILITY:          UpdateArmor(); UpdateAttackPowerAndDamage(true); break;
        case STAT_STAMINA:          UpdateMaxHealth();                   break;
        case STAT_INTELLECT:        UpdateMaxPower(POWER_MANA);          break;
        case STAT_SPIRIT:
        default:
            break;
    }

    return true;
}

// 全量刷新生物所有属性、能量上限与各系抗性
bool Creature::UpdateAllStats()
{
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
        UpdateStats(Stats(i));    

    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);

    return true;
}

// 更新生物指定法术学派抗性（物理系转入护甲更新）
void Creature::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        int32 value = GetTotalResistanceValue(SpellSchools(school));
        SetResistance(SpellSchools(school), value);
    }
    else
        UpdateArmor();
}

// 更新生物护甲值（包含敏捷 2 倍护甲加成）
void Creature::UpdateArmor()
{
    float dynamic = (GetStat(STAT_AGILITY) * 2.0f);

    m_auraModifiersGroup[UNIT_MOD_ARMOR][TOTAL_VALUE] += dynamic;
    int32 value = GetTotalResistanceValue(SPELL_SCHOOL_NORMAL);
    int32 oldValue = GetArmor();
    SetArmor(value);
    m_auraModifiersGroup[UNIT_MOD_ARMOR][TOTAL_VALUE] -= dynamic;
}

// 更新生物近战或远程攻击强度，并联动更新物理伤害
void Creature::UpdateAttackPowerAndDamage(bool ranged)
{
    float val2 = 0.0f;
    float level = float(GetLevel());

    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 index_mod = UNIT_FIELD_ATTACK_POWER_MODS;
    uint16 index_mult = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;
    AttackPowerMod mod = AttackPowerMod::MELEE_ATTACK_POWER;

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        index_mod = UNIT_FIELD_RANGED_ATTACK_POWER_MODS;
        index_mult = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;

        mod = AttackPowerMod::RANGED_ATTACK_POWER;

        val2 = GetStat(STAT_AGILITY) - GetCreateStat(STAT_AGILITY);

        val2 += m_baseRAP * m_damageMultiplier;
    }
    else
    {
        switch (getClass())
        {
            case CLASS_MAGE:
                val2 = GetStat(STAT_STRENGTH);
                break;
            case CLASS_ROGUE:
                val2 = (GetStat(STAT_STRENGTH) - GetCreateStat(STAT_STRENGTH)) + GetStat(STAT_AGILITY) - GetCreateStat(STAT_AGILITY);
                break;
            default:
                val2 = (GetStat(STAT_STRENGTH) - GetCreateStat(STAT_STRENGTH)) * 2.f;
                break;
        }

        // damage multiplier confirmed to apply to AP bonus (已确认伤害倍率同样作用于怪物基础攻击强度加成)
        val2 += m_baseAP * m_damageMultiplier;
    }

    SetModifierValue(unitMod, BASE_VALUE, val2);
    float base_attPower = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float attPowerMod = GetModifierValue(unitMod, TOTAL_VALUE);

    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    SetInt32Value(index, (uint32)base_attPower);            // UNIT_FIELD_(RANGED)_ATTACK_POWER field (写入基础攻强字段)
    SetInt16Value(index_mod, 0, m_attackPowerMod[size_t(mod)][size_t(AttackPowerModSign::MOD_SIGN_POS)]);
    SetInt16Value(index_mod, 1, m_attackPowerMod[size_t(mod)][size_t(AttackPowerModSign::MOD_SIGN_NEG)]);
    SetFloatValue(index_mult, attPowerMultiplier);          // UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field (写入攻强百分比乘数)

    // automatically update weapon damage after attack power modification (攻强更新后自动刷新近战/副手或远程物理伤害)
    if (!ranged)
    {
        UpdateDamagePhysical(BASE_ATTACK);
        UpdateDamagePhysical(OFF_ATTACK);
    }
    else
        UpdateDamagePhysical(RANGED_ATTACK);
}

// 计算并更新生物指定攻击槽位的物理伤害范围
void Creature::UpdateDamagePhysical(WeaponAttackType attType)
{
    UnitMods unitMod;
    UnitMods attPower;

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            attPower = UNIT_MOD_ATTACK_POWER;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            attPower = UNIT_MOD_ATTACK_POWER;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            attPower = UNIT_MOD_ATTACK_POWER_RANGED;
            break;
    }

    float base_value  = GetModifierValue(unitMod, BASE_VALUE) + (GetTotalAttackPowerValue(attType) / 14.0f);
    float base_pct    = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct   = GetModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetBaseWeaponDamage(attType, MINDAMAGE) * m_damageMultiplier;
    float weapon_maxdamage = GetBaseWeaponDamage(attType, MAXDAMAGE) * m_damageMultiplier;

    float mindamage = ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    float maxdamage = ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;

    float conditionalMod = GetConditionalTotalPhysicalDamageModifier(attType);
    mindamage *= conditionalMod;
    maxdamage *= conditionalMod;

    uint16 fieldmin, fieldmax;

    switch (attType)
    {
        case RANGED_ATTACK:
            fieldmin = UNIT_FIELD_MINRANGEDDAMAGE;
            fieldmax = UNIT_FIELD_MAXRANGEDDAMAGE;
            break;
        case BASE_ATTACK:
            fieldmin = UNIT_FIELD_MINDAMAGE;
            fieldmax = UNIT_FIELD_MAXDAMAGE;
            break;
        case OFF_ATTACK:
            fieldmin = UNIT_FIELD_MINOFFHANDDAMAGE;
            fieldmax = UNIT_FIELD_MAXOFFHANDDAMAGE;
            break;
        default:
            return;
    }

    SetStatFloatValue(fieldmin, mindamage);
    SetStatFloatValue(fieldmax, maxdamage);
}

// 获取生物条件性物理伤害倍率（如武器被缴械时伤害减半）
float Creature::GetConditionalTotalPhysicalDamageModifier(WeaponAttackType attType) const
{
    float result = 1.f;
    if (hasWeapon(attType) && !hasWeaponForAttack(attType))
        result *= 0.5f;
    return result;
}

// 重新计算并设置生物最大生命值（应用 m_healthMultiplier 倍率）
void Creature::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetModifierValue(unitMod, BASE_PCT) * m_healthMultiplier; // health multiplier affects base health AND stamina increase (生命倍率同时作用于基础血量与耐力加成)
    value += GetModifierValue(unitMod, TOTAL_VALUE) + GetHealthBonusFromStamina() * m_healthMultiplier;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxHealth(uint32(std::round(std::max(value, 1.f))));
}

// 计算生物由额外耐力差值提供的生命值加成
float Creature::GetHealthBonusFromStamina() const
{
    // only use diff until stamina per level coefficient for npcs is known (在明确 NPC 每级耐力系数前，仅基于超出创建基础耐力的差值计算血量加成)
    float stamina = GetStat(STAT_STAMINA);
    return Unit::GetHealthBonusFromStamina(stamina) - Unit::GetHealthBonusFromStamina(GetCreateStat(STAT_STAMINA));
}

/*#######################################
########                         ########
########    PETS STAT SYSTEM     ########
########     宠物属性系统        ########
########                         ########
#######################################*/

// 更新宠物指定基础属性并在需要时通知主人缩放光环
bool Pet::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
        return false;

    // value = ((base_value * base_pct) + total_value) * total_pct (总属性值计算公式)
    float value  = GetTotalStatValue(stat);
    
    float oldValue = GetStat(stat);
    SetStat(stat, int32(value));

    switch (stat)
    {
        case STAT_STRENGTH:         UpdateAttackPowerAndDamage();        break;
        case STAT_AGILITY:          UpdateArmor(); UpdateAttackPowerAndDamage(true); break;
        case STAT_STAMINA:          UpdateMaxHealth();                   break;
        case STAT_INTELLECT:        UpdateMaxPower(POWER_MANA);          break;
        case STAT_SPIRIT:
        default:
            break;
    }

    if (m_inStatsUpdate)
        return true;

    m_inStatsUpdate = true;
    if (oldValue != value)
        if (GetSpellAuraHolder(35696))                              // 恶魔学识大师/宠物属性联动光环 (ID 35696)
            if (stat == STAT_STAMINA || stat == STAT_INTELLECT)
                if (Unit* owner = GetOwner())
                    if (Aura* aura = owner->GetAura(35696, EFFECT_INDEX_0))
                        aura->UpdateAuraScaling();

    m_inStatsUpdate = false;
    return true;
}

// 重新计算并设置宠物最大生命值
void Pet::UpdateMaxHealth()
{
    if (m_scaleWithCls) // handled by regular cls (按常规生物等级缩放时直接复用 Creature 逻辑)
    {
        Creature::UpdateMaxHealth();
        return;
    }

    UnitMods unitMod = UNIT_MOD_HEALTH;
    float stamina = GetStat(STAT_STAMINA) - GetCreateStat(STAT_STAMINA); // TODO: Remove create stat usage like this (待办：移除此类依赖创建初始属性的差值计算)
    float addedStaminaBonus = std::max((stamina - 20) * 10, 0.f) + std::min(20.f, stamina);

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + addedStaminaBonus;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxHealth((uint32)value);
}

// 重新计算并设置宠物指定能量类型的上限
void Pet::UpdateMaxPower(Powers power)
{
    if (getPetType() == GUARDIAN_PET) // handled by regular cls (守护者类型宠物沿用常规生物逻辑)
    {
        Creature::UpdateMaxPower(power);
        return;
    }

    UnitMods unitMod = UnitMods(static_cast<uint32>(UNIT_MOD_POWER_START) + static_cast<uint32>(power));

    float addValue = (power == POWER_MANA) ? GetStat(STAT_INTELLECT) - GetCreateStat(STAT_INTELLECT) : 0.0f;

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreatePowers(power);
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + std::max((addValue - 20) * 15 + 20, 0.f);
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxPower(power, uint32(std::round(value)));
}

// 更新宠物攻击强度并刷新近战/副手物理伤害
void Pet::UpdateAttackPowerAndDamage(bool ranged)
{
    if (getPetType() == GUARDIAN_PET) // handled by regular cls (守护者类型宠物沿用常规生物逻辑)
    {
        Creature::UpdateAttackPowerAndDamage(ranged);
        return;
    }

    if (ranged)
        return;

    float val = 0.0f;
    UnitMods unitMod = UNIT_MOD_ATTACK_POWER;

    val = 2 * GetStat(STAT_STRENGTH) - 20.0f;

    SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, val);

    // in BASE_VALUE of UNIT_MOD_ATTACK_POWER for creatures we store data of meleeattackpower field in DB (生物类 UNIT_MOD_ATTACK_POWER 的 BASE_VALUE 中存储了数据库配置的近战攻强)
    float base_attPower  = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    // float attPowerMod = GetModifierValue(unitMod, TOTAL_VALUE);
    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    // UNIT_FIELD_(RANGED)_ATTACK_POWER field (写入基础攻强字段)
    SetInt32Value(UNIT_FIELD_ATTACK_POWER, (int32)base_attPower);
    // UNIT_FIELD_(RANGED)_ATTACK_POWER_MODS field (写入攻强正负修正字段)
    SetInt16Value(UNIT_FIELD_ATTACK_POWER_MODS, 0, m_attackPowerMod[size_t(AttackPowerMod::MELEE_ATTACK_POWER)][size_t(AttackPowerModSign::MOD_SIGN_POS)]);
    SetInt16Value(UNIT_FIELD_ATTACK_POWER_MODS, 1, m_attackPowerMod[size_t(AttackPowerMod::MELEE_ATTACK_POWER)][size_t(AttackPowerModSign::MOD_SIGN_NEG)]);
    // UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field (写入攻强百分比乘数)
    SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, attPowerMultiplier);

    // automatically update weapon damage after attack power modification (攻强变更后自动刷新主副手物理伤害)
    UpdateDamagePhysical(BASE_ATTACK);
    UpdateDamagePhysical(OFF_ATTACK);
}

// 获取宠物条件性物理伤害倍率（猎人宠物依据快乐度调整为 125% / 100% / 75%）
float Pet::GetConditionalTotalPhysicalDamageModifier(WeaponAttackType attType) const
{
    float result = Creature::GetConditionalTotalPhysicalDamageModifier(attType);
    //  Pet's base damage changes depending on happiness (猎人宠物基础伤害随快乐度等级变化)
    if (getPetType() == HUNTER_PET && attType == BASE_ATTACK)
    {
        switch (GetHappinessState())
        {
            case HAPPY:
                // 125% of normal damage (快乐：125% 伤害)
                result *= 1.25f;
                break;
            case CONTENT:
                // 100% of normal damage, nothing to modify (满足：100% 正常伤害)
                break;
            case UNHAPPY:
                // 75% of normal damage (不快：75% 伤害)
                result *= 0.75f;
                break;
        }
    }
    return result;
}
