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

#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Globals/ObjectMgr.h"
#include "Spells/SpellAuraDefines.h"
#include "Util/ProgressBar.h"
#include "Server/DBCStores.h"
#include "Server/SQLStorages.h"
#include "Chat/Chat.h"
#include "Spells/Spell.h"
#include "Entities/Unit.h"
#include "World/World.h"

/// 判断指定技能 ID 是否为主专业技能（查询 SkillLine.dbc 分类）
bool IsPrimaryProfessionSkill(uint32 skill)
{
    SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skill);
    if (!pSkill)
        return false;

    if (pSkill->categoryId != SKILL_CATEGORY_PROFESSION)
        return false;

    return true;
}

/// 构造函数
SpellMgr::SpellMgr()
{
}

/// 析构函数
SpellMgr::~SpellMgr()
{
}

/// 获取 SpellMgr 全局唯一单例
SpellMgr& SpellMgr::Instance()
{
    static SpellMgr spellMgr;
    return spellMgr;
}

/// 获取法术基础持续时间（毫秒，-1 表示永久）
int32 GetSpellDuration(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return 0;
    SpellDurationEntry const* du = sSpellDurationStore.LookupEntry(spellInfo->DurationIndex);
    if (!du)
        return 0;
    return (du->Duration[0] == -1) ? -1 : abs(du->Duration[0]);
}

/// 获取法术最大持续时间（毫秒，连击点终结技等会使用该上限）
int32 GetSpellMaxDuration(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return 0;
    SpellDurationEntry const* du = sSpellDurationStore.LookupEntry(spellInfo->DurationIndex);
    if (!du)
        return 0;
    return (du->Duration[2] == -1) ? -1 : abs(du->Duration[2]);
}

/// 计算最终法术持续时间（综合连击点数插值、光环脚本、天赋加成及急速缩减）
int32 CalculateSpellDuration(SpellEntry const* spellInfo, Unit const* caster, Unit const* target, AuraScript* auraScript)
{
    int32 duration = GetSpellDuration(spellInfo);

    if (duration != -1 && caster && !spellInfo->HasAttribute(SPELL_ATTR_EX3_IGNORE_CASTER_MODIFIERS))
    {
        int32 maxduration = GetSpellMaxDuration(spellInfo);

        // 玩家施放随连击点变化的终结技时，按当前连击点在基础与最大持续时间之间线性插值
        if (duration != maxduration && caster->IsPlayer())
            duration += int32((maxduration - duration) * caster->GetComboPoints() / 5);

        if (auraScript)
            duration = auraScript->OnDurationCalculate(caster, target, duration);

        // 应用施法者玩家身上的持续时间法术修饰器及急速对周期性法术的影响
        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->ApplySpellMod(spellInfo->Id, SPELLMOD_DURATION, duration);

            if (spellInfo->HasAttribute(SPELL_ATTR_EX5_SPELL_HASTE_AFFECTS_PERIODIC))
                duration = int32(duration * caster->GetFloatValue(UNIT_MOD_CAST_SPEED));

            if (duration < 0)
                duration = 0;
        }
    }
    else if (auraScript)
        duration = auraScript->OnDurationCalculate(caster, target, duration);

    return duration;
}

/// 计算法术最终施法时间（处理特殊物品/法术特例、交易栏瞬发、等级缩放、猎人远程补偿与急速加成）
uint32 GetSpellCastTime(SpellEntry const* spellInfo, WorldObject* caster, Spell* spell, bool consume)
{
    if (spell)
    {
        // Workaround for custom cast time (针对特定法术/物品的自定义施法时间特例处理)
        switch (spellInfo->Id)
        {
            case 3366:  // Opening - seems to have a settable timer per usage
                if (Item* item = spell->GetCastItem())
                {
                    switch (item->GetEntry())
                    {
                        case 31088: // Tainted Core - instant opening
                            return 0;
                        default:
                            break;
                    }
                }
                break;
            case 6249:
                return 50000;
            case 46546: // Ritual of Summoning
                return 0;
            default:
                break;
        }

        // some triggered spells have data only usable for client (部分触发法术的施法时间仅用于客户端显示，服务端按瞬发处理)
        if (spell->IsTriggeredSpellWithRedundantCastTime())
            return 0;

        // spell targeted to non-trading trade slot item instant at trade success apply (对交易栏“不被交易物品”槽位附魔时瞬间完成施法)
        if (caster->IsPlayer())
            if (TradeData* my_trade = static_cast<Player*>(caster)->GetTradeData())
                if (Item* nonTrade = my_trade->GetTraderData()->GetItem(TRADE_SLOT_NONTRADED))
                    if (nonTrade == spell->m_targets.getItemTarget())
                        return 0;
    }

    SpellCastTimesEntry const* spellCastTimeEntry = sSpellCastTimesStore.LookupEntry(spellInfo->CastingTimeIndex);

    // not all spells have cast time index and this is all is passive abilities (无施法时间索引的法术均为被动技能或瞬发，返回 0)
    if (!spellCastTimeEntry)
        return 0;

    // 根据施法者相对法术基准等级的差值按等级缩放基础施法时间，并限制不低于最小施法时间
    int32 spellRank = caster && caster->IsUnit() ? static_cast<Unit*>(caster)->GetSpellRank(spellInfo) : 0;
    int32 castTime = spellCastTimeEntry->CastTime + spellCastTimeEntry->CastTimePerLevel * (spellRank / 5 - spellInfo->baseLevel);
    castTime = std::max(castTime, spellCastTimeEntry->MinCastTime);

    // Hunter Ranged spells need cast time + 0.5s to reflect tooltips, excluding Auto Shot (猎人远程技能除自动射击外需额外加 0.5 秒以匹配技能说明)
    if (spellInfo->HasAttribute(SPELL_ATTR_USES_RANGED_SLOT) && (!spell || !spell->IsAutoRepeat()))
        castTime += 500;

    // 应用玩家天赋/光环对施法时间的平直/百分比修饰，并乘以施法速度或远程攻速倍率
    if (spell)
    {
        if (Player* modOwner = caster->GetSpellModOwner())
            modOwner->ApplySpellMod(spellInfo->Id, SPELLMOD_CASTING_TIME, castTime, consume);

        if (caster->IsUnit())
        {
            Unit* unitCaster = static_cast<Unit*>(caster);
            if (!spellInfo->HasAttribute(SPELL_ATTR_IS_ABILITY) && !spellInfo->HasAttribute(SPELL_ATTR_IS_TRADESKILL) && !spellInfo->HasAttribute(SPELL_ATTR_EX3_IGNORE_CASTER_MODIFIERS))
                castTime = int32(castTime * unitCaster->GetFloatValue(UNIT_MOD_CAST_SPEED));
            else if (spell->IsRangedSpell() && !spell->IsAutoRepeat())
                castTime = int32(castTime * unitCaster->m_modAttackSpeedPct[RANGED_ATTACK]);
        }
    }

    return (castTime > 0) ? uint32(castTime) : 0;
}

/// 计算用于法术强度/治疗加成系数公式的有效施法时间（含 1.5s~7s 截断、混合伤害/DoT 分摊、AOE/吸取与额外效果惩罚）
uint32 GetSpellCastTimeForBonus(SpellEntry const* spellProto, DamageEffectType damagetype)
{
    uint32 CastingTime = (!IsChanneledSpell(spellProto)) || spellProto->HasAttribute(SPELL_ATTR_EX5_SPELL_HASTE_AFFECTS_PERIODIC) ? GetSpellCastTime(spellProto, nullptr) : GetSpellDuration(spellProto);

    if (CastingTime > 7000) CastingTime = 7000;
    if (CastingTime < 1500) CastingTime = 1500;

    if (damagetype == DOT && !IsChanneledSpell(spellProto))
        CastingTime = 3500;

    int32 overTime    = 0;
    uint8 effects     = 0;
    bool DirectDamage = false;
    bool AreaEffect   = false;

    // 扫描各效果目标，判断是否包含范围（AOE）效果
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (IsAreaEffectTarget(SpellTarget(spellProto->EffectImplicitTargetA[i])) || IsAreaEffectTarget(SpellTarget(spellProto->EffectImplicitTargetB[i])))
            AreaEffect = true;

    // 统计直接伤害/治疗、持续时间（overTime）以及额外控制/减速/占位效果惩罚计数
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (spellProto->Effect[i])
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_POWER_DRAIN:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_ENVIRONMENTAL_DAMAGE:
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_HEAL:
                DirectDamage = true;
                break;
            case SPELL_EFFECT_APPLY_AURA:
                switch (spellProto->EffectApplyAuraName[i])
                {
                    case SPELL_AURA_PERIODIC_DAMAGE:
                    case SPELL_AURA_PERIODIC_HEAL:
                    case SPELL_AURA_PERIODIC_LEECH:
                        if (GetSpellDuration(spellProto))
                            overTime = GetSpellDuration(spellProto);
                        break;
                    // Penalty for additional effects (附加非伤害效果的系数惩罚计数)
                    case SPELL_AURA_DUMMY:
                        ++effects;
                        break;
                    case SPELL_AURA_MOD_DECREASE_SPEED:
                        ++effects;
                        break;
                    case SPELL_AURA_MOD_CONFUSE:
                    case SPELL_AURA_MOD_STUN:
                    case SPELL_AURA_MOD_ROOT:
                        // -10% per effect (控制类效果按双倍权重扣减加成系数)
                        effects += 2;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    // Combined Spells with Both Over Time and Direct Damage (兼具直接伤害与持续伤害/治疗的混合法术按各自占比拆分施法时间)
    if (overTime > 0 && CastingTime > 0 && DirectDamage)
    {
        // mainly for DoTs which are 3500 here otherwise
        uint32 OriginalCastTime = GetSpellCastTime(spellProto, nullptr);
        if (OriginalCastTime > 7000) OriginalCastTime = 7000;
        if (OriginalCastTime < 1500) OriginalCastTime = 1500;
        // Portion to Over Time (计算持续部分所占比例 PtOT)
        float PtOT = (overTime / 15000.0f) / ((overTime / 15000.0f) + (OriginalCastTime / 3500.0f));

        if (damagetype == DOT)
            CastingTime = uint32(CastingTime * PtOT);
        else if (PtOT < 1.0f)
            CastingTime  = uint32(CastingTime * (1 - PtOT));
        else
            CastingTime = 0;
    }

    // Area Effect Spells receive only half of bonus (群体范围法术的法术加成系数减半)
    if (AreaEffect)
        CastingTime /= 2;

    // 50% for damage and healing spells for leech spells from damage bonus and 0% from healing (生命吸取类法术仅享有一半系数加成)
    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        if (spellProto->Effect[j] == SPELL_EFFECT_HEALTH_LEECH ||
                (spellProto->Effect[j] == SPELL_EFFECT_APPLY_AURA && spellProto->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_LEECH))
        {
            CastingTime /= 2;
            break;
        }
    }

    // -5% of total per any additional effect (multiplicative) (每个附加效果叠乘 0.95 衰减系数)
    for (int i = 0; i < effects; ++i)
        CastingTime *= 0.95f;

    return CastingTime;
}

/// 获取法术光环的最大跳数（依据持续时间上限 30 秒与周期振幅计算，默认返回 6）
uint16 GetSpellAuraMaxTicks(SpellEntry const* spellInfo)
{
    int32 DotDuration = GetSpellDuration(spellInfo);
    if (DotDuration == 0)
        return 1;

    // 200% limit (持续时间最长按 30 秒上限折算跳数)
    if (DotDuration > 30000)
        DotDuration = 30000;

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        if (spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AURA && (
                    spellInfo->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_DAMAGE ||
                    spellInfo->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_HEAL ||
                    spellInfo->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_LEECH))
        {
            if (spellInfo->EffectAmplitude[j] != 0)
                return DotDuration / spellInfo->EffectAmplitude[j];
            break;
        }
    }

    return 6;
}

/// 按法术 ID 获取光环最大周期跳数
uint16 GetSpellAuraMaxTicks(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellInfo)
    {
        sLog.outError("GetSpellAuraMaxTicks: Spell %u not exist!", spellId);
        return 1;
    }

    return GetSpellAuraMaxTicks(spellInfo);
}

/// 计算法术默认伤害/治疗加成系数（TBC 标准 3.5 秒规则与 DoT 15 秒规则）
float CalculateDefaultCoefficient(SpellEntry const* spellProto, DamageEffectType const damagetype)
{
    // Damage over Time spells bonus calculation (持续性伤害/治疗法术按 15 秒基准与总跳数分摊单跳系数)
    float DotFactor = 1.0f;
    if (damagetype == DOT)
    {
        if (!IsChanneledSpell(spellProto))
            DotFactor = GetSpellDuration(spellProto) / 15000.0f;

        if (uint16 DotTicks = GetSpellAuraMaxTicks(spellProto))
            DotFactor /= DotTicks;
    }

    // Distribute Damage over multiple effects, reduce by AoE (按有效施法时间除以 3.5 秒得到基础加成系数)
    float coeff = GetSpellCastTimeForBonus(spellProto, damagetype) / 3500.0f;

    return coeff * DotFactor;
}

/// 判断法术关联的武器攻击槽位（主手/副手/远程）
WeaponAttackType GetWeaponAttackType(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return BASE_ATTACK;

    switch (spellInfo->DmgClass)
    {
        case SPELL_DAMAGE_CLASS_MELEE:
            if (spellInfo->HasAttribute(SPELL_ATTR_EX3_REQUIRES_OFFHAND_WEAPON))
                return OFF_ATTACK;
            return BASE_ATTACK;
        case SPELL_DAMAGE_CLASS_RANGED:
            return RANGED_ATTACK;
        default:
        {
            // Wands (魔杖攻击使用远程攻击类型)
            if (spellInfo->HasAttribute(SPELL_ATTR_EX3_NORMAL_RANGED_ATTACK))
                return RANGED_ATTACK;
            if (spellInfo->HasAttribute(SPELL_ATTR_EX3_REQUIRES_OFFHAND_WEAPON))
                return OFF_ATTACK;
            return BASE_ATTACK;
        }
    }
}

/// 判断目标类型是否为显式指定的友方单体/小队/团队目标
bool IsExplicitPositiveTarget(uint32 targetA)
{
    // positive targets that in target selection code expect target in m_targets, so not that auto-select target by spell data by m_caster and etc (需要显式/预设友方目标的目标模式)
    switch (targetA)
    {
        case TARGET_UNIT_FRIEND:
        case TARGET_UNIT_PARTY:
        case TARGET_UNIT_FRIEND_CHAIN_HEAL:
        case TARGET_UNIT_RAID:
        case TARGET_UNIT_RAID_AND_CLASS:
            return true;
        default:
            break;
    }
    return false;
}

/// 判断目标类型是否为显式指定的敌方单体或目标点
bool IsExplicitNegativeTarget(uint32 targetA)
{
    // non-positive targets that in target selection code expect target in m_targets, so not that auto-select target by spell data by m_caster and etc (需要显式/预设敌方目标或坐标的目标模式)
    switch (targetA)
    {
        case TARGET_UNIT_ENEMY:
        case TARGET_LOCATION_CASTER_TARGET_POSITION:
            return true;
        default:
            break;
    }
    return false;
}

/// 检查在当前变形姿态下施放法术是否满足姿态限制，返回对应施法错误码
SpellCastResult GetErrorAtShapeshiftedCast(SpellEntry const* spellInfo, uint32 form)
{
    // talents that learn spells can have stance requirements that need ignore (学习法术的天赋忽略姿态限制，该限制仅作用于客户端提示)
    // (this requirement only for client-side stance show in talent description)
    if (GetTalentSpellCost(spellInfo->Id) > 0 &&
            (spellInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_LEARN_SPELL || spellInfo->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_LEARN_SPELL || spellInfo->Effect[EFFECT_INDEX_2] == SPELL_EFFECT_LEARN_SPELL))
        return SPELL_CAST_OK;

    uint32 stanceMask = (form ? convertEnumToFlag(form) : 0);

    if (stanceMask & spellInfo->StancesNot)                 // can explicitly not be casted in this stance (明确禁止在当前姿态施放)
        return SPELL_FAILED_NOT_SHAPESHIFT;

    if (stanceMask & spellInfo->Stances)                    // can explicitly be casted in this stance (允许在当前姿态施放)
        return SPELL_CAST_OK;

    bool actAsShifted = false;
    if (form > 0)
    {
        SpellShapeshiftFormEntry const* shapeInfo = sSpellShapeshiftFormStore.LookupEntry(form);
        if (!shapeInfo)
        {
            sLog.outError("GetErrorAtShapeshiftedCast: unknown shapeshift %u", form);
            return SPELL_CAST_OK;
        }
        actAsShifted = !(shapeInfo->flags1 & 1);            // shapeshift acts as normal form for spells (判断该变形是否视为正式变形姿态)
    }

    if (actAsShifted)
    {
        if (spellInfo->HasAttribute(SPELL_ATTR_NOT_SHAPESHIFT)) // not while shapeshifted (变形期间禁止施放)
            return SPELL_FAILED_NOT_SHAPESHIFT;
        if (spellInfo->Stances != 0)                   // needs other shapeshift (需要特定变形姿态才能施放)
            return SPELL_FAILED_ONLY_SHAPESHIFT;
    }
    else
    {
        // needs shapeshift (未变形但法术要求处于变形状态)
        if (!spellInfo->HasAttribute(SPELL_ATTR_EX2_ALLOW_WHILE_NOT_SHAPESHIFTED) && spellInfo->Stances != 0)
            return SPELL_FAILED_ONLY_SHAPESHIFT;
    }

    return SPELL_CAST_OK;
}

/// 从数据库 spell_target_position 表加载并校验法术固定目标传送/落点坐标
void SpellMgr::LoadSpellTargetPositions()
{
    mSpellTargetPositions.clear();                          // need for reload case (重载前清空旧缓存)

    uint32 count = 0;

    //                                                0   1           2                  3                  4                  5
    auto queryResult = WorldDatabase.Query("SELECT id, target_map, target_position_x, target_position_y, target_position_z, target_orientation FROM spell_target_position");
    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u spell target destination coordinates", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        Field* fields = queryResult->Fetch();

        bar.step();

        uint32 Spell_ID = fields[0].GetUInt32();

        SpellTargetPosition st;

        st.target_mapId       = fields[1].GetUInt32();
        st.target_X           = fields[2].GetFloat();
        st.target_Y           = fields[3].GetFloat();
        st.target_Z           = fields[4].GetFloat();
        st.target_Orientation = fields[5].GetFloat();

        MapEntry const* mapEntry = sMapStore.LookupEntry(st.target_mapId);
        if (!mapEntry)
        {
            sLog.outErrorDb("Spell (ID:%u) target map (ID: %u) does not exist in `Map.dbc`.", Spell_ID, st.target_mapId);
            continue;
        }

        if (st.target_X == 0 && st.target_Y == 0 && st.target_Z == 0)
        {
            sLog.outErrorDb("Spell (ID:%u) target coordinates not provided.", Spell_ID);
            continue;
        }

        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(Spell_ID);
        if (!spellInfo)
        {
            sLog.outErrorDb("Spell (ID:%u) listed in `spell_target_position` does not exist.", Spell_ID);
            continue;
        }

        bool found = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (spellInfo->EffectImplicitTargetA[i] == TARGET_LOCATION_DATABASE || spellInfo->EffectImplicitTargetB[i] == TARGET_LOCATION_DATABASE)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            sLog.outErrorDb("Spell (Id: %u) listed in `spell_target_position` does not have target TARGET_LOCATION_DATABASE (17).", Spell_ID);
            continue;
        }

        mSpellTargetPositions[Spell_ID] = st;
        ++count;
    }
    while (queryResult->NextRow());

    sLog.outString(">> Loaded %u spell target destination coordinates", count);
    sLog.outString();
}

/// 法术等级链批量加载辅助模板：记录各等级法术，并将第一级配置自动填充到缺失的高等级法术中
template <typename EntryType, typename WorkerType, typename StorageType>
struct SpellRankHelper
{
    SpellRankHelper(SpellMgr& _mgr, StorageType& _storage): mgr(_mgr), worker(_storage), customRank(0) {}
    void RecordRank(EntryType& entry, uint32 spell_id)
    {
        const SpellEntry* spell = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
        if (!spell)
        {
            sLog.outErrorDb("Spell %u listed in `%s` does not exist", spell_id, worker.TableName());
            return;
        }

        uint32 first_id = mgr.GetFirstSpellInChain(spell_id);

        // most spell ranks expected same data (大多数法术等级链共享第一级配置)
        if (first_id)
        {
            firstRankSpells.insert(first_id);

            if (first_id != spell_id)
            {
                if (!worker.IsValidCustomRank(entry, spell_id, first_id))
                    return;
                // for later check that first rank also added
                firstRankSpellsWithCustomRanks.insert(first_id);
                ++customRank;
            }
        }

        worker.AddEntry(entry, spell);
    }
    void FillHigherRanks()
    {
        // check that first rank added for custom ranks (校验存在自定义高等级配置时第一级也必须已录入)
        for (std::set<uint32>::const_iterator itr = firstRankSpellsWithCustomRanks.begin(); itr != firstRankSpellsWithCustomRanks.end(); ++itr)
            if (!worker.HasEntry(*itr))
                sLog.outErrorDb("Spell %u must be listed in `%s` as first rank for listed custom ranks of spell but not found!", *itr, worker.TableName());

        // fill absent non first ranks data base at first rank data (将第一级数据自动补全到未单独配置的后续等级)
        for (std::set<uint32>::const_iterator itr = firstRankSpells.begin(); itr != firstRankSpells.end(); ++itr)
        {
            if (worker.SetStateToEntry(*itr))
                mgr.doForHighRanks(*itr, worker);
        }
    }
    std::set<uint32> firstRankSpells;
    std::set<uint32> firstRankSpellsWithCustomRanks;

    SpellMgr& mgr;
    WorkerType worker;
    uint32 customRank;
};

/// spell_proc_event 表加载回调工作器：校验并填充法术触发事件配置
struct DoSpellProcEvent
{
    DoSpellProcEvent(SpellProcEventMap& _spe_map) : spe_map(_spe_map), customProc(0), count(0) {}
    void operator()(uint32 spell_id)
    {
        SpellProcEventEntry const& spe = state->second;
        // add ranks only for not filled data (some ranks have ppm data different for ranks for example) (仅为未单独配置的等级填充触发规则)
        SpellProcEventMap::const_iterator spellItr = spe_map.find(spell_id);
        if (spellItr == spe_map.end())
            spe_map[spell_id] = spe;
        // if custom rank data added then it must be same except ppm (若高等级有自定义条目，除 ppm 外其余字段必须与第一级一致)
        else
        {
            SpellProcEventEntry const& r_spe = spellItr->second;
            if (spe.schoolMask != r_spe.schoolMask)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different schoolMask from first rank in chain", spell_id);

            if (spe.spellFamilyName != r_spe.spellFamilyName)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different spellFamilyName from first rank in chain", spell_id);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (spe.spellFamilyMask[i] != r_spe.spellFamilyMask[i])
                {
                    sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different spellFamilyMask from first rank in chain", spell_id);
                    break;
                }
            }

            if (spe.procFlags != r_spe.procFlags)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different procFlags from first rank in chain", spell_id);

            if (spe.procEx != r_spe.procEx)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different procEx from first rank in chain", spell_id);

            // only ppm allowed has been different from first rank (仅允许 ppmRate 随法术等级变化)

            if (spe.customChance != r_spe.customChance)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different customChance from first rank in chain", spell_id);

            if (spe.cooldown != r_spe.cooldown)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different cooldown from first rank in chain", spell_id);
        }
    }

    static const char* TableName() { return "spell_proc_event"; }
    bool IsValidCustomRank(SpellProcEventEntry const& spe, uint32 entry, uint32 first_id) const
    {
        // let have independent data in table for spells with ppm rates (exist rank dependent ppm rate spells) (仅带有 ppmRate 的高等级条目允许独立录入)
        if (!spe.ppmRate)
        {
            sLog.outErrorDb("Spell %u listed in `spell_proc_event` is not first rank (%u) in chain", entry, first_id);
            // prevent loading since it won't have an effect anyway
            return false;
        }
        return true;
    }
    void AddEntry(SpellProcEventEntry const& spe, SpellEntry const* spell)
    {
        spe_map[spell->Id] = spe;

        bool isCustom = false;

        if (spe.procFlags == 0)
        {
            if (spell->procFlags == 0)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` probably not triggered spell (no proc flags)", spell->Id);
        }
        else
        {
            if (spell->procFlags == spe.procFlags)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` has exactly same proc flags as in spell.dbc, field value redundant", spell->Id);
            else
                isCustom = true;
        }

        if (spe.customChance == 0)
        {
            /* enable for re-check cases, 0 chance ok for some cases because in some cases it set by another spell/talent spellmod)
            if (spell->procChance==0 && !spe.ppmRate)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` probably not triggered spell (no chance or ppm)", spell->Id);
            */
        }
        else
        {
            if (spell->procChance == spe.customChance)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` has exactly same custom chance as in spell.dbc, field value redundant", spell->Id);
            else
                isCustom = true;
        }

        // totally redundant record (检测所有字段均为空的无效冗余记录)
        if (!spe.schoolMask && !spe.procFlags &&
                !spe.procEx && !spe.ppmRate && !spe.customChance && !spe.cooldown)
        {
            bool empty = spe.spellFamilyName == 0;
            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (spe.spellFamilyMask[i])
                {
                    empty = false;
                    break;
                }
            }
            if (empty)
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` doesn't have any useful data", spell->Id);
        }

        if (isCustom)
            ++customProc;
        else
            ++count;
    }

    bool HasEntry(uint32 spellId) const { return spe_map.find(spellId) != spe_map.end(); }
    bool SetStateToEntry(uint32 spellId) { return (state = spe_map.find(spellId)) != spe_map.end(); }
    SpellProcEventMap& spe_map;
    SpellProcEventMap::const_iterator state;

    uint32 customProc;
    uint32 count;
};

/// 从数据库 spell_proc_event 表加载法术触发事件规则，并自动补全高等级法术链
void SpellMgr::LoadSpellProcEvents()
{
    mSpellProcEventMap.clear();                             // need for reload case (重载前清空触发事件表)

    //                                             0      1           2                3                 4                 5                 6          7       8        9             10
    auto queryResult = WorldDatabase.Query("SELECT entry, SchoolMask, SpellFamilyName, SpellFamilyMask0, SpellFamilyMask1, SpellFamilyMask2, procFlags, procEx, ppmRate, CustomChance, Cooldown FROM spell_proc_event");
    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> No spell proc event conditions loaded");
        return;
    }

    SpellRankHelper<SpellProcEventEntry, DoSpellProcEvent, SpellProcEventMap> rankHelper(*this, mSpellProcEventMap);

    BarGoLink bar(queryResult->GetRowCount());
    do
    {
        Field* fields = queryResult->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();

        SpellProcEventEntry spe;

        spe.schoolMask      = fields[1].GetUInt32();
        spe.spellFamilyName = fields[2].GetUInt32();

        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            spe.spellFamilyMask[i] = ClassFamilyMask(fields[3 + i].GetUInt64());

        spe.procFlags       = fields[6].GetUInt32();
        spe.procEx          = fields[7].GetUInt32();
        spe.ppmRate         = fields[8].GetFloat();
        spe.customChance    = fields[9].GetFloat();
        spe.cooldown        = fields[10].GetUInt32();

        rankHelper.RecordRank(spe, entry);
    }
    while (queryResult->NextRow());

    rankHelper.FillHigherRanks();

    sLog.outString(">> Loaded %u extra spell proc event conditions +%u custom proc (inc. +%u custom ranks)",  rankHelper.worker.count, rankHelper.worker.customProc, rankHelper.customRank);
    sLog.outString();
}

/// spell_proc_item_enchant 表加载辅助回调：为高等级法术复制物品附魔 PPM 概率
struct DoSpellProcItemEnchant
{
    DoSpellProcItemEnchant(SpellProcItemEnchantMap& _procMap, float _ppm) : procMap(_procMap), ppm(_ppm) {}
    void operator()(uint32 spell_id) { procMap[spell_id] = ppm; }

    SpellProcItemEnchantMap& procMap;
    float ppm;
};

/// 从数据库 spell_proc_item_enchant 表加载物品附魔触发概率（PPM）配置
void SpellMgr::LoadSpellProcItemEnchant()
{
    mSpellProcItemEnchantMap.clear();                       // need for reload case (重载前清空物品附魔触发映射)

    uint32 count = 0;

    //                                             0      1
    auto queryResult = WorldDatabase.Query("SELECT entry, ppmRate FROM spell_proc_item_enchant");
    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u proc item enchant definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        Field* fields = queryResult->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();
        float ppmRate = fields[1].GetFloat();

        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(entry);

        if (!spellInfo)
        {
            sLog.outErrorDb("Spell %u listed in `spell_proc_item_enchant` does not exist", entry);
            continue;
        }

        uint32 first_id = GetFirstSpellInChain(entry);

        if (first_id != entry)
        {
            sLog.outErrorDb("Spell %u listed in `spell_proc_item_enchant` is not first rank (%u) in chain", entry, first_id);
            // prevent loading since it won't have an effect anyway
            continue;
        }

        mSpellProcItemEnchantMap[entry] = ppmRate;

        // also add to high ranks
        DoSpellProcItemEnchant worker(mSpellProcItemEnchantMap, ppmRate);
        doForHighRanks(entry, worker);

        ++count;
    }
    while (queryResult->NextRow());

    sLog.outString(">> Loaded %u proc item enchant definitions", count);
    sLog.outString();
}

/// 判断光环是否为在施法结束（CAST_END）时才触发消耗的法术修饰光环
bool IsCastEndProcModifierAura(SpellEntry const* spellInfo, SpellEffectIndex effecIdx, SpellEntry const* procSpell)
{
    // modifier auras that can proc on cast end (在施法完成瞬间触发消耗的修饰类光环)
    switch (AuraType(spellInfo->EffectApplyAuraName[effecIdx]))
    {
        case SPELL_AURA_ADD_FLAT_MODIFIER:
        case SPELL_AURA_ADD_PCT_MODIFIER:
        {
            switch (spellInfo->EffectMiscValue[effecIdx])
            {
                case SPELLMOD_RANGE:
                case SPELLMOD_RADIUS:
                case SPELLMOD_NOT_LOSE_CASTING_TIME:
                case SPELLMOD_CASTING_TIME:
                case SPELLMOD_COOLDOWN:
                case SPELLMOD_COST:
                case SPELLMOD_GLOBAL_COOLDOWN:
                    return true;
                default:
                    break;
            }
        }
        case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE:
        {
            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                if (IsEffectHandledImmediatelySpellLaunch(procSpell, SpellEffectIndex(i)))
                    return true;

            return false;
        }
        default:
            return false;
    }
}

/// 判定指定触发事件配置与光环触发标志是否允许由当前候选法术/攻击结果触发
bool SpellMgr::IsSpellProcEventCanTriggeredBy(SpellProcEventEntry const* spellProcEvent, uint32 EventProcFlag, SpellEntry const* spellInfo, uint32 procFlags, uint32 procExtra)
{
    // No extra req need
    uint32 procEvent_procEx = PROC_EX_NONE;

    // check prockFlags for condition (校验触发事件标志位是否匹配)
    if ((procFlags & EventProcFlag) == 0)
        return false;

    // Always trigger for this (心跳、击杀、陷阱激活、死亡事件无条件放行后续判定)
    if (EventProcFlag & (PROC_FLAG_HEARTBEAT | PROC_FLAG_KILL | PROC_FLAG_ON_TRAP_ACTIVATION | PROC_FLAG_DEATH))
        return true;

    // 区分周期性治疗（HoT）与周期性伤害（DoT），防止纯伤害光环被 HoT 错误触发或反之
    if (procFlags & PROC_FLAG_DEAL_HARMFUL_PERIODIC && EventProcFlag & PROC_FLAG_DEAL_HARMFUL_PERIODIC)
    {
        if (procExtra & PROC_EX_INTERNAL_HOT)
        {
            if (EventProcFlag == PROC_FLAG_DEAL_HARMFUL_PERIODIC)
            {
                // no aura with only PROC_FLAG_DONE_PERIODIC and spellFamilyName == 0 can proc from a HOT. (仅含周期触发且无家族名的光环不可由 HoT 触发)
                if (!spellInfo->SpellFamilyName)
                    return false;
            }
            // Aura must have positive procflags for a HOT to proc
            else if (!(EventProcFlag & (PROC_FLAG_DEAL_HELPFUL_SPELL | PROC_FLAG_DEAL_HELPFUL_ABILITY)))
                return false;
        }
        // Aura must have negative or neutral(PROC_FLAG_DONE_PERIODIC only) procflags for a DOT to proc
        else if (EventProcFlag != PROC_FLAG_DEAL_HARMFUL_PERIODIC)
            if (!(EventProcFlag & (PROC_FLAG_DEAL_HARMFUL_SPELL | PROC_FLAG_DEAL_HARMFUL_ABILITY)))
                return false;
    }

    if (procFlags & PROC_FLAG_TAKE_HARMFUL_PERIODIC && EventProcFlag & PROC_FLAG_TAKE_HARMFUL_PERIODIC)
    {
        if (procExtra & PROC_EX_INTERNAL_HOT)
        {
            // No aura that only has PROC_FLAG_TAKEN_PERIODIC can proc from a HOT.
            if (EventProcFlag == PROC_FLAG_TAKE_HARMFUL_PERIODIC)
                return false;
            // Aura must have positive procflags for a HOT to proc
            if (!(EventProcFlag & (PROC_FLAG_TAKE_HELPFUL_SPELL | PROC_FLAG_TAKE_HELPFUL_ABILITY)))
                return false;
        }
        // Aura must have negative or neutral(PROC_FLAG_TAKEN_PERIODIC only) procflags for a DOT to proc
        else if (EventProcFlag != PROC_FLAG_TAKE_HARMFUL_PERIODIC)
            if (!(EventProcFlag & (PROC_FLAG_TAKE_HARMFUL_SPELL | PROC_FLAG_TAKE_HARMFUL_ABILITY)))
                return false;
    }

    if (spellProcEvent)     // Exist event data (存在数据库自定义触发条件时校验学派与家族名)
    {
        // Store extra req
        procEvent_procEx = spellProcEvent->procEx;

        // For melee triggers
        if (spellInfo == nullptr)
        {
            // Check (if set) for school (melee attack have Normal school)
            if (spellProcEvent->schoolMask && (spellProcEvent->schoolMask & SPELL_SCHOOL_MASK_NORMAL) == 0)
                return false;
        }
        else // For spells need check school/spell family/family mask
        {
            // Check (if set) for school
            if (spellProcEvent->schoolMask && (spellProcEvent->schoolMask & spellInfo->SchoolMask) == 0)
                return false;

            // Check (if set) for spellFamilyName
            if (spellProcEvent->spellFamilyName && (spellProcEvent->spellFamilyName != spellInfo->SpellFamilyName))
                return false;
        }
    }

    // Check for extra req (if none) and hit/crit (校验扩展命中结果掩码)
    if (procEvent_procEx == PROC_EX_NONE)
    {
        // No extra req, so can trigger for (damage/healing present) and cast end/hit/crit (未指定扩展要求时默认在施法结束、普通命中或暴击时触发)
        if (procExtra & (PROC_EX_CAST_END | PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT))
            return true;
    }
    else // all spells hits here only if resist/reflect/immune/evade (存在自定义扩展掩码时按位匹配具体命中结果)
    {
        // Exist req for PROC_EX_EX_TRIGGER_ON_NO_DAMAGE
        if (procEvent_procEx & PROC_EX_EX_TRIGGER_ON_NO_DAMAGE)
            return true;
        // Check Extra Requirement like (hit/crit/miss/resist/parry/dodge/block/immune/reflect/absorb and other)
        if (procEvent_procEx & procExtra)
            return true;
    }
    return false;
}

/// 从数据库 spell_elixir 表加载药剂、合剂及食物互斥掩码
void SpellMgr::LoadSpellElixirs()
{
    mSpellElixirs.clear();                                  // need for reload case (重载前清空药剂分类映射)

    uint32 count = 0;

    //                                             0      1
    auto queryResult = WorldDatabase.Query("SELECT entry, mask FROM spell_elixir");
    if (!queryResult)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString(">> Loaded %u spell elixir definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        Field* fields = queryResult->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();
        uint8 mask = fields[1].GetUInt8();

        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(entry);

        if (!spellInfo)
        {
            sLog.outErrorDb("Spell %u listed in `spell_elixir` does not exist", entry);
            continue;
        }

        mSpellElixirs[entry] = mask;

        ++count;
    }
    while (queryResult->NextRow());

    sLog.outString(">> Loaded %u spell elixir definitions", count);
    sLog.outString();
}

/// spell_threat 表加载回调工作器：校验多目标效果冲突并填充法术仇恨修正数据
struct DoSpellThreat
{
    DoSpellThreat(SpellThreatMap& _threatMap) : threatMap(_threatMap), count(0) {}
    void operator()(uint32 spell_id)
    {
        SpellThreatEntry const& ste = state->second;
        // add ranks only for not filled data (spells adding flat threat are usually different for ranks) (仅填充尚未单独配置的等级)
        SpellThreatMap::const_iterator spellItr = threatMap.find(spell_id);
        if (spellItr == threatMap.end())
            threatMap[spell_id] = ste;

        // just assert that entry is not redundant
        else
        {
            SpellThreatEntry const& r_ste = spellItr->second;
            if (ste.threat == r_ste.threat && ste.multiplier == r_ste.multiplier && ste.ap_bonus == r_ste.ap_bonus)
                sLog.outErrorDb("Spell %u listed in `spell_threat` as custom rank has same data as Rank 1, so redundant", spell_id);
        }
    }
    const char* TableName() const { return "spell_threat"; }
    bool IsValidCustomRank(SpellThreatEntry const& ste, uint32 entry, uint32 first_id) const
    {
        if (!ste.threat)
        {
            sLog.outErrorDb("Spell %u listed in `spell_threat` is not first rank (%u) in chain and has no threat", entry, first_id);
            // prevent loading unexpected data
            return false;
        }
        return true;
    }
    void AddEntry(SpellThreatEntry const& ste, SpellEntry const* spell)
    {
        threatMap[spell->Id] = ste;

        // flat threat bonus and attack power bonus currently only work properly when all (固定仇恨与 AP 加成在多效果目标不同时需配合 inverseEffectMask 使用)
        // effects have same targets, otherwise, we'd need to separate it by effect index
        if (ste.inverseEffectMask == 0 && (ste.threat || ste.ap_bonus != 0.f))
        {
            const uint32* targetA = spell->EffectImplicitTargetA;

            uint32 target = 0;
            bool failed = false;
            for (int i = 0; i < MAX_EFFECT_INDEX; i++)
            {
                if (targetA[i] != 0)
                {
                    if (target == 0)
                        target = targetA[i];
                    else if (targetA[i] != target)
                        failed = true;
                }
            }
            if (failed)
                sLog.outErrorDb("Spell %u listed in `spell_threat` has effects with different targets, threat may be assigned incorrectly. Consider using inverse effect mask.", spell->Id);
        }
        ++count;
    }
    bool HasEntry(uint32 spellId) const { return threatMap.count(spellId) > 0; }
    bool SetStateToEntry(uint32 spellId) { return (state = threatMap.find(spellId)) != threatMap.end(); }

    SpellThreatMap& threatMap;
    SpellThreatMap::const_iterator state;
    uint32 count;
};

/// 从数据库 spell_threat 表加载法术固定仇恨、仇恨倍率与 AP 仇恨加成
void SpellMgr::LoadSpellThreats()
{
    mSpellThreatMap.clear();                                // need for reload case (重载前清空仇恨映射)

    //                                             0      1       2           3         4
    auto queryResult = WorldDatabase.Query("SELECT entry, Threat, multiplier, ap_bonus, InverseEffectMask FROM spell_threat");
    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> No spell threat entries loaded.");
        sLog.outString();
        return;
    }

    SpellRankHelper<SpellThreatEntry, DoSpellThreat, SpellThreatMap> rankHelper(*this, mSpellThreatMap);

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        Field* fields = queryResult->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();

        SpellThreatEntry ste;
        ste.threat = fields[1].GetUInt16();
        ste.multiplier = fields[2].GetFloat();
        ste.ap_bonus = fields[3].GetFloat();
        ste.inverseEffectMask = fields[4].GetUInt32();

        rankHelper.RecordRank(ste, entry);
    }
    while (queryResult->NextRow());

    rankHelper.FillHigherRanks();

    sLog.outString(">> Loaded %u spell threat entries", rankHelper.worker.count);
    sLog.outString();
}

/// 判断法术 1 的各效果家族影响掩码是否覆盖法术 2
bool SpellMgr::IsSpellCanAffectSpell(SpellEntry const* spellInfo_1, SpellEntry const* spellInfo_2) const
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        ClassFamilyMask mask = sSpellMgr.GetSpellAffectMask(spellInfo_1->Id, SpellEffectIndex(i));
        if (spellInfo_2->IsFitToFamilyMask(mask))
            return true;
    }
    return false;
}

/// 判断法术是否为专业技能或骑术学习法术
bool SpellMgr::IsProfessionOrRidingSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellInfo)
        return false;

    if (spellInfo->Effect[EFFECT_INDEX_1] != SPELL_EFFECT_SKILL)
        return false;

    uint32 skill = spellInfo->EffectMiscValue[EFFECT_INDEX_1];

    return IsProfessionOrRidingSkill(skill);
}

/// 判断法术是否为专业技能学习法术
bool SpellMgr::IsProfessionSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellInfo)
        return false;

    if (spellInfo->Effect[EFFECT_INDEX_1] != SPELL_EFFECT_SKILL)
        return false;

    uint32 skill = spellInfo->EffectMiscValue[EFFECT_INDEX_1];

    return IsProfessionSkill(skill);
}

/// 判断法术是否为主专业技能学习法术
bool SpellMgr::IsPrimaryProfessionSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellInfo)
        return false;

    if (spellInfo->Effect[EFFECT_INDEX_1] != SPELL_EFFECT_SKILL)
        return false;

    uint32 skill = spellInfo->EffectMiscValue[EFFECT_INDEX_1];

    return IsPrimaryProfessionSkill(skill);
}

/// 判断法术是否为主专业技能的初级（第 1 阶）学习法术
bool SpellMgr::IsPrimaryProfessionFirstRankSpell(uint32 spellId) const
{
    return IsPrimaryProfessionSpell(spellId) && GetSpellRank(spellId) == 1;
}

/// 判断法术是否为达到特定技能点自动学会的技能奖励法术
bool SpellMgr::IsSkillBonusSpell(uint32 spellId) const
{
    SkillLineAbilityMapBounds bounds = GetSkillLineAbilityMapBoundsBySpellId(spellId);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* pAbility = _spell_idx->second;
        if (!pAbility || pAbility->learnOnGetSkill != ABILITY_LEARNED_ON_GET_PROFESSION_SKILL)
            continue;

        if (pAbility->req_skill_value > 0)
            return true;
    }

    return false;
}

/// 根据目标等级自动降级选择合适的增益法术等级（实现低等级玩家增益自动降级规则）
SpellEntry const* SpellMgr::SelectAuraRankForLevel(SpellEntry const* spellInfo, uint32 level) const
{
    // fast case (目标等级在法术等级-10范围内无需降级)
    if (level + 10 >= spellInfo->spellLevel)
        return spellInfo;

    // ignore selection for passive spells (被动法术不做自动降级)
    if (IsPassiveSpell(spellInfo))
        return spellInfo;

    bool needRankSelection = false;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        // for simple aura in check apply to any non caster based targets, in rank search mode to any explicit targets
        if (((spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
                (IsExplicitPositiveTarget(spellInfo->EffectImplicitTargetA[i]) ||
                 IsAreaEffectPositiveTarget(SpellTarget(spellInfo->EffectImplicitTargetA[i])))) ||
                spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AREA_AURA_PARTY) &&
                IsPositiveEffect(spellInfo, SpellEffectIndex(i)))
        {
            needRankSelection = true;
            break;
        }
    }

    // not required (rank check more slow so check it here)
    if (!needRankSelection || GetSpellRank(spellInfo->Id) == 0)
        return spellInfo;

    // Check for lower rank of same spell if config is set to auto downrank (若开启自动降级配置，沿等级链向下寻找适配等级的法术)
    if (sWorld.getConfig(CONFIG_BOOL_AUTO_DOWNRANK))
    {
        for (uint32 nextSpellId = spellInfo->Id; nextSpellId != 0; nextSpellId = GetPrevSpellInChain(nextSpellId))
        {
            SpellEntry const* nextSpellInfo = sSpellTemplate.LookupEntry<SpellEntry>(nextSpellId);
            if (!nextSpellInfo)
                break;

            // if found appropriate level
            if (level + 10 >= nextSpellInfo->spellLevel)
                return nextSpellInfo;

            // one rank less then
        }
    }

    // Recipient is too low level
    return nullptr;
}

typedef std::unordered_map<uint32, uint32> AbilitySpellPrevMap;

/// 递归辅助函数：根据 SkillLineAbility.dbc 的 forward_spellid 链路构建法术等级链节点
static void LoadSpellChains_AbilityHelper(SpellChainMap& chainMap, AbilitySpellPrevMap const& prevRanks, uint32 spell_id, uint32 prev_id, uint32 deep = 30)
{
    // spell already listed in chains store
    SpellChainMap::const_iterator chain_itr = chainMap.find(spell_id);
    if (chain_itr != chainMap.end())
    {
        MANGOS_ASSERT(chain_itr->second.prev == prev_id && "LoadSpellChains_AbilityHelper: Conflicting data in talents or spell abilities dbc");
        return;
    }

    // prev rank listed in main chain table (can fill correct data directly)
    SpellChainMap::const_iterator prev_chain_itr = chainMap.find(prev_id);
    if (prev_chain_itr != chainMap.end())
    {
        SpellChainNode node;
        node.prev  = prev_id;
        node.first = prev_chain_itr->second.first;
        node.rank  = prev_chain_itr->second.rank + 1;
        node.req   = 0;
        chainMap[spell_id] = node;
        return;
    }

    // prev spell not listed in prev ranks store, so it first rank
    AbilitySpellPrevMap::const_iterator prev_itr = prevRanks.find(prev_id);
    if (prev_itr == prevRanks.end())
    {
        SpellChainNode prev_node;
        prev_node.prev  = 0;
        prev_node.first = prev_id;
        prev_node.rank  = 1;
        prev_node.req   = 0;
        chainMap[prev_id] = prev_node;

        SpellChainNode node;
        node.prev  = prev_id;
        node.first = prev_id;
        node.rank  = 2;
        node.req   = 0;
        chainMap[spell_id] = node;
        return;
    }

    if (deep == 0)
    {
        MANGOS_ASSERT(false && "LoadSpellChains_AbilityHelper: Infinity cycle in spell ability data");
        return;
    }

    // prev rank listed, so process it first
    LoadSpellChains_AbilityHelper(chainMap, prevRanks, prev_id, prev_itr->second, deep - 1);

    // prev rank must be listed now
    prev_chain_itr = chainMap.find(prev_id);
    if (prev_chain_itr == chainMap.end())
        return;

    SpellChainNode node;
    node.prev  = prev_id;
    node.first = prev_chain_itr->second.first;
    node.rank  = prev_chain_itr->second.rank + 1;
    node.req   = 0;
    chainMap[spell_id] = node;
}

/// 从 Talent.dbc、SkillLineAbility.dbc 与数据库 spell_chain 表加载并校验全部法术等级链
void SpellMgr::LoadSpellChains()
{
    mSpellChains.clear();                                   // need for reload case (重载前清空法术等级链)
    mSpellChainsNext.clear();                               // need for reload case

    // load known data for talents (从 Talent.dbc 加载多级天赋等级链)
    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
            continue;

        // not add ranks for 1 ranks talents (if exist non ranks spells then it will included in table data)
        if (!talentInfo->RankID[1])
            continue;

        for (int j = 0; j < MAX_TALENT_RANK; ++j)
        {
            uint32 spell_id = talentInfo->RankID[j];
            if (!spell_id)
                continue;

            if (!sSpellTemplate.LookupEntry<SpellEntry>(spell_id))
            {
                // sLog.outErrorDb("Talent %u not exist as spell",spell_id);
                continue;
            }

            SpellChainNode node;
            node.prev  = (j > 0) ? talentInfo->RankID[j - 1] : 0;
            node.first = talentInfo->RankID[0];
            node.rank  = j + 1;
            node.req   = 0;

            mSpellChains[spell_id] = node;
        }
    }

    // load known data from spell abilities (从 SkillLineAbility.dbc 的前向法术指针构建技能等级链)
    {
        // we can calculate ranks only after full data generation
        AbilitySpellPrevMap prevRanks;
        for (SkillLineAbilityMap::const_iterator ab_itr = mSkillLineAbilityMapBySpellId.begin(); ab_itr != mSkillLineAbilityMapBySpellId.end(); ++ab_itr)
        {
            uint32 spell_id = ab_itr->first;

            // skip GM/test/internal spells.begin Its not have ranks anyway
            if (ab_itr->second->skillId == SKILL_INTERNAL)
                continue;

            // some forward spells not exist and can be ignored (some outdated data)
            SpellEntry const* spell_entry = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
            if (!spell_entry)                               // no cases
                continue;

            // ignore spell without forwards (non ranked or missing info in skill abilities)
            uint32 forward_id = ab_itr->second->forward_spellid;

            // by some strange reason < 3.x clients not have forward spell for 2366
            if (spell_id == 2366)                           // Herb Gathering, Apprentice
                forward_id = 2368;

            if (!forward_id)
                continue;

            // some forward spells not exist and can be ignored (some outdated data)
            SpellEntry const* forward_entry = sSpellTemplate.LookupEntry<SpellEntry>(forward_id);
            if (!forward_entry)
                continue;

            // some forward spells still exist but excluded from real use as ranks and not listed in skill abilities now
            SkillLineAbilityMapBounds bounds = mSkillLineAbilityMapBySpellId.equal_range(forward_id);
            if (bounds.first == bounds.second)
                continue;

            // spell already listed in chains store
            SpellChainMap::const_iterator chain_itr = mSpellChains.find(forward_id);
            if (chain_itr != mSpellChains.end())
            {
                MANGOS_ASSERT(chain_itr->second.prev == spell_id && "Conflicting data in talents or spell abilities dbc");
                continue;
            }

            // spell already listed in prev ranks store
            AbilitySpellPrevMap::const_iterator prev_itr = prevRanks.find(forward_id);
            if (prev_itr != prevRanks.end())
            {
                MANGOS_ASSERT(prev_itr->second == spell_id && "Conflicting data in talents or spell abilities dbc");
                continue;
            }

            // prev rank listed in main chain table (can fill correct data directly)
            SpellChainMap::const_iterator prev_chain_itr = mSpellChains.find(spell_id);
            if (prev_chain_itr != mSpellChains.end())
            {
                SpellChainNode node;
                node.prev  = spell_id;
                node.first = prev_chain_itr->second.first;
                node.rank  = prev_chain_itr->second.rank + 1;
                node.req   = 0;

                mSpellChains[forward_id] = node;
                continue;
            }

            // need temporary store for later rank calculation
            prevRanks[forward_id] = spell_id;
        }

        while (!prevRanks.empty())
        {
            uint32 spell_id = prevRanks.begin()->first;
            uint32 prev_id  = prevRanks.begin()->second;
            prevRanks.erase(prevRanks.begin());

            LoadSpellChains_AbilityHelper(mSpellChains, prevRanks, spell_id, prev_id);
        }
    }

    // load custom case (从数据库 spell_chain 表加载补充或自定义等级链并校验一致性)
    auto queryResult = WorldDatabase.Query("SELECT spell_id, prev_spell, first_spell, `rank`, req_spell FROM spell_chain");
    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString(">> Loaded 0 spell chain records");
        sLog.outErrorDb("`spell_chains` table is empty!");
        sLog.outString();
        return;
    }

    uint32 dbc_count = mSpellChains.size();
    uint32 new_count = 0;
    uint32 req_count = 0;

    BarGoLink bar(queryResult->GetRowCount());
    do
    {
        bar.step();
        Field* fields = queryResult->Fetch();

        uint32 spell_id = fields[0].GetUInt32();

        SpellChainNode node;
        node.prev  = fields[1].GetUInt32();
        node.first = fields[2].GetUInt32();
        node.rank  = fields[3].GetUInt8();
        node.req   = fields[4].GetUInt32();

        if (!sSpellTemplate.LookupEntry<SpellEntry>(spell_id))
        {
            sLog.outErrorDb("Spell %u listed in `spell_chain` does not exist", spell_id);
            continue;
        }

        SpellChainMap::iterator chain_itr = mSpellChains.find(spell_id);
        if (chain_itr != mSpellChains.end())
        {
            if (chain_itr->second.rank != node.rank)
            {
                sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` expected rank %u by DBC data.",
                                spell_id, node.prev, node.first, node.rank, node.req, chain_itr->second.rank);
                continue;
            }

            if (chain_itr->second.prev != node.prev)
            {
                sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` expected prev %u by DBC data.",
                                spell_id, node.prev, node.first, node.rank, node.req, chain_itr->second.prev);
                continue;
            }

            if (chain_itr->second.first != node.first)
            {
                sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` expected first %u by DBC data.",
                                spell_id, node.prev, node.first, node.rank, node.req, chain_itr->second.first);
                continue;
            }

            // update req field by table data
            if (node.req)
            {
                chain_itr->second.req = node.req;
                ++req_count;
                continue;
            }

            // in other case redundant
            sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) already added (talent or spell ability with forward) and non need in `spell_chain`",
                            spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }

        if (node.prev != 0 && !sSpellTemplate.LookupEntry<SpellEntry>(node.prev))
        {
            sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has nonexistent previous rank spell.",
                            spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }

        if (!sSpellTemplate.LookupEntry<SpellEntry>(node.first))
        {
            sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has not existing first rank spell.",
                            spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }

        // check basic spell chain data integrity (note: rank can be equal 0 or 1 for first/single spell)
        if ((spell_id == node.first) != (node.rank <= 1) ||
                (spell_id == node.first) != (node.prev == 0) ||
                (node.rank <= 1) != (node.prev == 0))
        {
            sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has not compatible chain data.",
                            spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }

        if (node.req != 0 && !sSpellTemplate.LookupEntry<SpellEntry>(node.req))
        {
            sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has not existing required spell.",
                            spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }

        // talents not required data in spell chain for work, but must be checked if present for integrity
        if (TalentSpellPos const* pos = GetTalentSpellPos(spell_id))
        {
            if (node.rank != pos->rank + 1)
            {
                sLog.outErrorDb("Talent %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has wrong rank.",
                                spell_id, node.prev, node.first, node.rank, node.req);
                continue;
            }

            if (TalentEntry const* talentEntry = sTalentStore.LookupEntry(pos->talent_id))
            {
                if (node.first != talentEntry->RankID[0])
                {
                    sLog.outErrorDb("Talent %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has wrong first rank spell.",
                                    spell_id, node.prev, node.first, node.rank, node.req);
                    continue;
                }

                if (node.rank > 1 && node.prev != talentEntry->RankID[node.rank - 1 - 1])
                {
                    sLog.outErrorDb("Talent %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has wrong prev rank spell.",
                                    spell_id, node.prev, node.first, node.rank, node.req);
                    continue;
                }

                if (node.req != talentEntry->DependsOnSpell)
                {
                    sLog.outErrorDb("Talent %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has wrong required spell.",
                                    spell_id, node.prev, node.first, node.rank, node.req);
                    continue;
                }
            }
        }

        mSpellChains[spell_id] = node;

        ++new_count;
    }
    while (queryResult->NextRow());

    // additional integrity checks (二次遍历执行全链条前驱与前置法术完整性校验)
    for (SpellChainMap::const_iterator i = mSpellChains.begin(); i != mSpellChains.end(); ++i)
    {
        if (i->second.prev)
        {
            SpellChainMap::const_iterator i_prev = mSpellChains.find(i->second.prev);
            if (i_prev == mSpellChains.end())
            {
                sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has not found previous rank spell in table.",
                                i->first, i->second.prev, i->second.first, i->second.rank, i->second.req);
            }
            else if (i_prev->second.first != i->second.first)
            {
                sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has different first spell in chain compared to previous rank spell (prev: %u, first: %u, rank: %d, req: %u).",
                                i->first, i->second.prev, i->second.first, i->second.rank, i->second.req,
                                i_prev->second.prev, i_prev->second.first, i_prev->second.rank, i_prev->second.req);
            }
            else if (i_prev->second.rank + 1 != i->second.rank)
            {
                sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has different rank compared to previous rank spell (prev: %u, first: %u, rank: %d, req: %u).",
                                i->first, i->second.prev, i->second.first, i->second.rank, i->second.req,
                                i_prev->second.prev, i_prev->second.first, i_prev->second.rank, i_prev->second.req);
            }
        }

        if (i->second.req)
        {
            SpellChainMap::const_iterator i_req = mSpellChains.find(i->second.req);
            if (i_req == mSpellChains.end())
            {
                sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has not found required rank spell in table.",
                                i->first, i->second.prev, i->second.first, i->second.rank, i->second.req);
            }
            else if (i_req->second.first == i->second.first)
            {
                sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has required rank spell from same spell chain (prev: %u, first: %u, rank: %d, req: %u).",
                                i->first, i->second.prev, i->second.first, i->second.rank, i->second.req,
                                i_req->second.prev, i_req->second.first, i_req->second.rank, i_req->second.req);
            }
            else if (i_req->second.req)
            {
                sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has required rank spell with required spell (prev: %u, first: %u, rank: %d, req: %u).",
                                i->first, i->second.prev, i->second.first, i->second.rank, i->second.req,
                                i_req->second.prev, i_req->second.first, i_req->second.rank, i_req->second.req);
            }
        }
    }

    // fill next rank cache (构建反向索引 mSpellChainsNext 以便快速查询后续升级法术)
    for (SpellChainMap::const_iterator i = mSpellChains.begin(); i != mSpellChains.end(); ++i)
    {
        uint32 spell_id = i->first;
        SpellChainNode const& node = i->second;

        if (node.prev)
            mSpellChainsNext.insert(SpellChainMapNext::value_type(node.prev, spell_id));

        if (node.req)
            mSpellChainsNext.insert(SpellChainMapNext::value_type(node.req, spell_id));
    }

    // check single rank redundant cases (single rank talents not added by default so this can be only custom cases)
    for (SpellChainMap::const_iterator i = mSpellChains.begin(); i != mSpellChains.end(); ++i)
    {
        // skip non-first ranks, and spells with additional reqs
        if (i->second.rank > 1 || i->second.req)
            continue;

        if (mSpellChainsNext.find(i->first) == mSpellChainsNext.end())
        {
            sLog.outErrorDb("Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in `spell_chain` has single rank data, so redundant.",
                            i->first, i->second.prev, i->second.first, i->second.rank, i->second.req);
        }
    }

    sLog.outString(">> Loaded %u spell chain records (%u from DBC data with %u req field updates, and %u loaded from table)", dbc_count + new_count, dbc_count, req_count, new_count);
    sLog.outString();
}

/// 从 Spell.dbc 扫描 SPELL_EFFECT_SKILL 效果并建立法术学习技能映射
void SpellMgr::LoadSpellLearnSkills()
{
    mSpellLearnSkills.clear();                              // need for reload case (重载前清空学习技能映射)

    // search auto-learned skills and add its to map also for use in unlearn spells/talents (扫描 Spell.dbc 中包含学习技能效果的法术)
    uint32 dbc_count = 0;
    BarGoLink bar(sSpellTemplate.GetMaxEntry());
    for (uint32 spell = 0; spell < sSpellTemplate.GetMaxEntry(); ++spell)
    {
        bar.step();
        SpellEntry const* entry = sSpellTemplate.LookupEntry<SpellEntry>(spell);

        if (!entry)
            continue;

        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (entry->Effect[i] == SPELL_EFFECT_SKILL)
            {
                SpellLearnSkillNode dbc_node;
                dbc_node.skill    = entry->EffectMiscValue[i];
                dbc_node.step     = entry->CalculateSimpleValue(SpellEffectIndex(i));
                dbc_node.effect   = SpellEffects(entry->Effect[i]);

                if (dbc_node.skill && dbc_node.step)
                {
                    mSpellLearnSkills[spell] = dbc_node;
                    ++dbc_count;
                    break;
                }
            }
        }
    }

    sLog.outString(">> Loaded %u Spell Learn Skills from DBC", dbc_count);
    sLog.outString();
}

/// 从数据库 spell_learn_spell 表及 Spell.dbc 扫描加载法术连带学习关系
void SpellMgr::LoadSpellLearnSpells()
{
    mSpellLearnSpells.clear();                              // need for reload case (重载前清空连带学习法术映射)

    //                                             0      1        2
    auto queryResult = WorldDatabase.Query("SELECT entry, SpellID, Active FROM spell_learn_spell");
    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 spell learn spells");
        sLog.outErrorDb("`spell_learn_spell` table is empty!");
        return;
    }

    uint32 count = 0;

    BarGoLink bar(queryResult->GetRowCount());
    do
    {
        bar.step();
        Field* fields = queryResult->Fetch();

        uint32 spell_id    = fields[0].GetUInt32();

        SpellLearnSpellNode node;
        node.spell      = fields[1].GetUInt32();
        node.active     = fields[2].GetBool();
        node.autoLearned = false;

        if (!sSpellTemplate.LookupEntry<SpellEntry>(spell_id))
        {
            sLog.outErrorDb("Spell %u listed in `spell_learn_spell` does not exist", spell_id);
            continue;
        }

        if (!sSpellTemplate.LookupEntry<SpellEntry>(node.spell))
        {
            sLog.outErrorDb("Spell %u listed in `spell_learn_spell` learning nonexistent spell %u", spell_id, node.spell);
            continue;
        }

        if (GetTalentSpellCost(node.spell))
        {
            sLog.outErrorDb("Spell %u listed in `spell_learn_spell` attempt learning talent spell %u, skipped", spell_id, node.spell);
            continue;
        }

        mSpellLearnSpells.insert(SpellLearnSpellMap::value_type(spell_id, node));

        ++count;
    }
    while (queryResult->NextRow());

    // search auto-learned spells and add its to map also for use in unlearn spells/talents (补充扫描 Spell.dbc 中的 SPELL_EFFECT_LEARN_SPELL 自动学习关系)
    uint32 dbc_count = 0;
    for (uint32 spell = 0; spell < sSpellTemplate.GetMaxEntry(); ++spell)
    {
        SpellEntry const* entry = sSpellTemplate.LookupEntry<SpellEntry>(spell);

        if (!entry)
            continue;

        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (entry->Effect[i] == SPELL_EFFECT_LEARN_SPELL)
            {
                SpellLearnSpellNode dbc_node;
                dbc_node.spell       = entry->EffectTriggerSpell[i];
                dbc_node.active      = true;                // all dbc based learned spells is active (show in spell book or hide by client itself)

                // ignore learning nonexistent spells (broken/outdated/or generic learning spell 483
                if (!sSpellTemplate.LookupEntry<SpellEntry>(dbc_node.spell))
                    continue;

                // talent or passive spells or skill-step spells auto-casted and not need dependent learning,
                // pet teaching spells don't must be dependent learning (casted)
                // other required explicit dependent learning
                dbc_node.autoLearned = entry->EffectImplicitTargetA[i] == TARGET_UNIT_CASTER_PET || GetTalentSpellCost(spell) > 0 || IsPassiveSpell(entry) || IsSpellHaveEffect(entry, SPELL_EFFECT_SKILL_STEP);

                SpellLearnSpellMapBounds db_node_bounds = GetSpellLearnSpellMapBounds(spell);

                bool found = false;
                for (SpellLearnSpellMap::const_iterator itr = db_node_bounds.first; itr != db_node_bounds.second; ++itr)
                {
                    if (itr->second.spell == dbc_node.spell)
                    {
                        sLog.outErrorDb("Spell %u auto-learn spell %u in spell.dbc then the record in `spell_learn_spell` is redundant, please fix DB.",
                                        spell, dbc_node.spell);
                        found = true;
                        break;
                    }
                }

                if (!found)                                 // add new spell-spell pair if not found
                {
                    mSpellLearnSpells.insert(SpellLearnSpellMap::value_type(spell, dbc_node));
                    ++dbc_count;
                }
            }
        }
    }

    sLog.outString(">> Loaded %u spell learn spells + %u found in DBC", count, dbc_count);
    sLog.outString();
}

/// 加载并严格校验 spell_script_target 表中的法术脚本目标（生物/游戏对象/GUID/StringId）
void SpellMgr::LoadSpellScriptTarget()
{
    sSpellScriptTargetStorage.Load();

    // Check content (校验数据库条目法术、目标模式、实体模板/GUID/StringId 的合法性)
    for (SQLMultiStorage::SQLSIterator<SpellTargetEntry> itr = sSpellScriptTargetStorage.getDataBegin<SpellTargetEntry>(); itr < sSpellScriptTargetStorage.getDataEnd<SpellTargetEntry>(); ++itr)
    {
        SpellEntry const* spellProto = sSpellTemplate.LookupEntry<SpellEntry>(itr->spellId);
        if (!spellProto)
        {
            sLog.outErrorDb("Table `spell_script_target`: spellId %u listed for TargetEntry %u does not exist.", itr->spellId, itr->targetEntry);
            sSpellScriptTargetStorage.EraseEntry(itr->spellId);
            continue;
        }

        bool targetfound = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (spellProto->Effect[i] == SPELL_EFFECT_PERSISTENT_AREA_AURA) // always scriptable
            {
                targetfound = true;
                break;
            }

            if (spellProto->EffectImplicitTargetA[i] == TARGET_UNIT_SCRIPT_NEAR_CASTER ||
                    spellProto->EffectImplicitTargetB[i] == TARGET_UNIT_SCRIPT_NEAR_CASTER ||
                    spellProto->EffectImplicitTargetA[i] == TARGET_LOCATION_SCRIPT_NEAR_CASTER ||
                    spellProto->EffectImplicitTargetB[i] == TARGET_LOCATION_SCRIPT_NEAR_CASTER ||
                    spellProto->EffectImplicitTargetA[i] == TARGET_GAMEOBJECT_SCRIPT_NEAR_CASTER ||
                    spellProto->EffectImplicitTargetB[i] == TARGET_GAMEOBJECT_SCRIPT_NEAR_CASTER ||
                    spellProto->EffectImplicitTargetA[i] == TARGET_ENUM_UNITS_SCRIPT_AOE_AT_SRC_LOC ||
                    spellProto->EffectImplicitTargetB[i] == TARGET_ENUM_UNITS_SCRIPT_AOE_AT_SRC_LOC ||
                    spellProto->EffectImplicitTargetA[i] == TARGET_ENUM_UNITS_SCRIPT_AOE_AT_DEST_LOC ||
                    spellProto->EffectImplicitTargetB[i] == TARGET_ENUM_UNITS_SCRIPT_AOE_AT_DEST_LOC ||
                    spellProto->EffectImplicitTargetA[i] == TARGET_ENUM_GAMEOBJECTS_SCRIPT_AOE_AT_SRC_LOC ||
                    spellProto->EffectImplicitTargetB[i] == TARGET_ENUM_GAMEOBJECTS_SCRIPT_AOE_AT_SRC_LOC ||
                    spellProto->EffectImplicitTargetA[i] == TARGET_ENUM_GAMEOBJECTS_SCRIPT_AOE_AT_DEST_LOC ||
                    spellProto->EffectImplicitTargetB[i] == TARGET_ENUM_GAMEOBJECTS_SCRIPT_AOE_AT_DEST_LOC ||
                    spellProto->EffectImplicitTargetA[i] == TARGET_ENUM_UNITS_SCRIPT_IN_CONE_60 ||
                    spellProto->EffectImplicitTargetB[i] == TARGET_ENUM_UNITS_SCRIPT_IN_CONE_60)
            {
                targetfound = true;
                break;
            }
        }
        if (!targetfound)
        {
            sLog.outErrorDb("Table `spell_script_target`: spellId %u listed for TargetEntry %u does not have any implicit target TARGET_UNIT_SCRIPT_NEAR_CASTER(38) or TARGET_LOCATION_SCRIPT_NEAR_CASTER (46) or TARGET_GAMEOBJECT_SCRIPT_NEAR_CASTER (40).", itr->spellId, itr->targetEntry);
            sSpellScriptTargetStorage.EraseEntry(itr->spellId);
            continue;
        }

        if (itr->type >= MAX_SPELL_TARGET_TYPE)
        {
            sLog.outErrorDb("Table `spell_script_target`: target type %u for TargetEntry %u is incorrect.", itr->type, itr->targetEntry);
            sSpellScriptTargetStorage.EraseEntry(itr->spellId);
            continue;
        }

        // Checks by target type
        switch (itr->type)
        {
            case SPELL_TARGET_TYPE_GAMEOBJECT:
            {
                if (!itr->targetEntry)
                    break;

                if (!sGOStorage.LookupEntry<GameObjectInfo>(itr->targetEntry))
                {
                    sLog.outErrorDb("Table `spell_script_target`: gameobject template entry %u does not exist.", itr->targetEntry);
                    sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                }
                break;
            }
            case SPELL_TARGET_TYPE_GAMEOBJECT_GUID:
            {
                if (!itr->targetEntry)
                    break;

                if (!sObjectMgr.GetGOData(itr->targetEntry))
                {
                    sLog.outErrorDb("Table `spell_script_target`: gameobject guid %u does not exist.", itr->targetEntry);
                    sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                }
                break;
            }
            case SPELL_TARGET_TYPE_CREATURE_GUID:
            {
                if (!sObjectMgr.GetCreatureData(itr->targetEntry))
                {
                    sLog.outErrorDb("Table `spell_script_target`: creature guid %u does not exist.", itr->targetEntry);
                    sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                }
                break;
            }
            case SPELL_TARGET_TYPE_STRING_ID:
            {
                if (!sScriptMgr.ExistsStringId(itr->targetEntry))
                {
                    sLog.outErrorDb("Table `spell_script_target`: stringId %u does not exist.", itr->targetEntry);
                    sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                }
                break;
            }
            default:
                if (!itr->targetEntry)
                {
                    sLog.outErrorDb("Table `spell_script_target`: target entry == 0 for not GO target type (%u).", itr->type);
                    sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                    continue;
                }
                if (const CreatureInfo* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(itr->targetEntry))
                {
                    if (itr->spellId == 30427 && !cInfo->SkinningLootId)
                    {
                        sLog.outErrorDb("Table `spell_script_target` has creature %u as a target of spellid 30427, but this creature has no SkinningLootId. Gas extraction will not work!", cInfo->Entry);
                        sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                    }
                }
                else
                {
                    sLog.outErrorDb("Table `spell_script_target`: creature template entry %u does not exist.", itr->targetEntry);
                    sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                }
                break;
        }
    }

    // Check all spells (反向巡检 Spell.dbc 中使用脚本附近单位目标的法术是否缺失数据库记录)
    if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
    {
        for (uint32 i = 1; i < sSpellTemplate.GetMaxEntry(); ++i)
        {
            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(i);
            if (!spellInfo)
                continue;

            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
            {
                if (spellInfo->Effect[j] && (spellInfo->EffectImplicitTargetA[j] == TARGET_UNIT_SCRIPT_NEAR_CASTER ||
                                             (spellInfo->EffectImplicitTargetA[j] != TARGET_UNIT_CASTER && spellInfo->EffectImplicitTargetB[j] == TARGET_UNIT_SCRIPT_NEAR_CASTER)))
                {
                    SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(i);
                    if (bounds.first == bounds.second)
                    {
                        sLog.outErrorDb("Spell (ID: %u) has effect EffectImplicitTargetA/EffectImplicitTargetB = %u (TARGET_UNIT_SCRIPT_NEAR_CASTER), but does not have record in `spell_script_target`", spellInfo->Id, TARGET_UNIT_SCRIPT_NEAR_CASTER);
                        break;                              // effects of spell
                    }
                }
            }
        }
    }

    sLog.outString(">> Loaded %u spell_script_target definitions", sSpellScriptTargetStorage.GetRecordCount());
    sLog.outString();
}

/// 从数据库 spell_pet_auras 表加载主人法术与宠物被动光环的映射关系
void SpellMgr::LoadSpellPetAuras()
{
    mSpellPetAuraMap.clear();                               // need for reload case (重载前清空宠物光环映射)

    uint32 count = 0;

    //                                             0      1    2
    auto queryResult = WorldDatabase.Query("SELECT spell, pet, aura FROM spell_pet_auras");
    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u spell pet auras", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        Field* fields = queryResult->Fetch();

        bar.step();

        uint32 spell = fields[0].GetUInt32();
        uint32 pet = fields[1].GetUInt32();
        uint32 aura = fields[2].GetUInt32();

        SpellPetAuraMap::iterator itr = mSpellPetAuraMap.find(spell);
        if (itr != mSpellPetAuraMap.end())
        {
            itr->second.AddAura(pet, aura);
        }
        else
        {
            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spell);
            if (!spellInfo)
            {
                sLog.outErrorDb("Spell %u listed in `spell_pet_auras` does not exist", spell);
                continue;
            }
            int i = 0;
            for (; i < MAX_EFFECT_INDEX; ++i)
                if ((spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
                        spellInfo->EffectApplyAuraName[i] == SPELL_AURA_DUMMY) ||
                        spellInfo->Effect[i] == SPELL_EFFECT_DUMMY)
                    break;

            if (i == MAX_EFFECT_INDEX)
            {
                sLog.outError("Spell %u listed in `spell_pet_auras` does not have dummy aura or dummy effect", spell);
                continue;
            }

            SpellEntry const* spellInfo2 = sSpellTemplate.LookupEntry<SpellEntry>(aura);
            if (!spellInfo2)
            {
                sLog.outErrorDb("Aura %u listed in `spell_pet_auras` does not exist", aura);
                continue;
            }

            PetAura pa(pet, aura, spellInfo->EffectImplicitTargetA[i] == TARGET_UNIT_CASTER_PET, spellInfo->CalculateSimpleValue(SpellEffectIndex(i)));
            mSpellPetAuraMap[spell] = pa;
        }

        ++count;
    }
    while (queryResult->NextRow());

    sLog.outString(">> Loaded %u spell pet auras", count);
    sLog.outString();
}

/// 校验法术有效性（如制造物品、关联学习法术及所需材料是否存在于模板表）
/// Some checks for spells, to prevent adding deprecated/broken spells for trainers, spell book, etc
bool SpellMgr::IsSpellValid(SpellEntry const* spellInfo, Player* pl, bool msg)
{
    // not exist
    if (!spellInfo)
        return false;

    bool need_check_reagents = false;

    // check effects
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (spellInfo->Effect[i])
        {
            case SPELL_EFFECT_NONE:
                continue;

            // craft spell for crafting nonexistent item (break client recipes list show)
            case SPELL_EFFECT_CREATE_ITEM:
            {
                if (!ObjectMgr::GetItemPrototype(spellInfo->EffectItemType[i]))
                {
                    if (msg)
                    {
                        if (pl)
                            ChatHandler(pl).PSendSysMessage("Craft spell %u create item (Entry: %u) but item does not exist in item_template.", spellInfo->Id, spellInfo->EffectItemType[i]);
                        else
                            sLog.outErrorDb("Craft spell %u create item (Entry: %u) but item does not exist in item_template.", spellInfo->Id, spellInfo->EffectItemType[i]);
                    }
                    return false;
                }

                need_check_reagents = true;
                break;
            }
            case SPELL_EFFECT_LEARN_SPELL:
            {
                SpellEntry const* spellInfo2 = sSpellTemplate.LookupEntry<SpellEntry>(spellInfo->EffectTriggerSpell[i]);
                if (!IsSpellValid(spellInfo2, pl, msg))
                {
                    if (msg)
                    {
                        if (pl)
                            ChatHandler(pl).PSendSysMessage("Spell %u learn to broken spell %u, and then...", spellInfo->Id, spellInfo->EffectTriggerSpell[i]);
                        else
                            sLog.outErrorDb("Spell %u learn to invalid spell %u, and then...", spellInfo->Id, spellInfo->EffectTriggerSpell[i]);
                    }
                    return false;
                }
                break;
            }
        }
    }

    if (need_check_reagents)
    {
        for (int j : spellInfo->Reagent)
        {
            if (j > 0 && !ObjectMgr::GetItemPrototype(j))
            {
                if (msg)
                {
                    if (pl)
                        ChatHandler(pl).PSendSysMessage("Craft spell %u requires reagent item (Entry: %u) but item does not exist in item_template.", spellInfo->Id, j);
                    else
                        sLog.outErrorDb("Craft spell %u requires reagent item (Entry: %u) but item does not exist in item_template.", spellInfo->Id, j);
                }
                return false;
            }
        }
    }

    return true;
}

/// 从数据库 spell_area 表加载区域限制与进出区域自动施放/移除光环配置
void SpellMgr::LoadSpellAreas()
{
    mSpellAreaMap.clear();                                  // need for reload case (重载前清空区域法术映射)
    mSpellAreaForAuraMap.clear();

    uint32 count = 0;

    //                                             0      1     2            3                   4          5             6           7         8       9
    auto queryResult = WorldDatabase.Query("SELECT spell, area, quest_start, quest_start_active, quest_end, condition_id, aura_spell, racemask, gender, autocast FROM spell_area");

    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u spell area requirements", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        Field* fields = queryResult->Fetch();

        bar.step();

        uint32 spell = fields[0].GetUInt32();
        SpellArea spellArea;
        spellArea.spellId             = spell;
        spellArea.areaId              = fields[1].GetUInt32();
        spellArea.questStart          = fields[2].GetUInt32();
        spellArea.questStartCanActive = fields[3].GetBool();
        spellArea.questEnd            = fields[4].GetUInt32();
        spellArea.conditionId         = fields[5].GetUInt16();
        spellArea.auraSpell           = fields[6].GetInt32();
        spellArea.raceMask            = fields[7].GetUInt32();
        spellArea.gender              = Gender(fields[8].GetUInt8());
        spellArea.autocast            = fields[9].GetBool();

        if (!sSpellTemplate.LookupEntry<SpellEntry>(spell))
        {
            sLog.outErrorDb("Spell %u listed in `spell_area` does not exist", spell);
            continue;
        }

        {
            bool ok = true;
            SpellAreaMapBounds sa_bounds = GetSpellAreaMapBounds(spellArea.spellId);
            for (SpellAreaMap::const_iterator itr = sa_bounds.first; itr != sa_bounds.second; ++itr)
            {
                if (spellArea.spellId != itr->second.spellId)
                    continue;
                if (spellArea.areaId != itr->second.areaId)
                    continue;
                if (spellArea.questStart != itr->second.questStart)
                    continue;
                if (spellArea.auraSpell != itr->second.auraSpell)
                    continue;
                if ((spellArea.raceMask & itr->second.raceMask) == 0)
                    continue;
                if (spellArea.gender != itr->second.gender)
                    continue;

                // duplicate by requirements
                ok = false;
                break;
            }

            if (!ok)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` already listed with similar requirements.", spell);
                continue;
            }
        }

        if (spellArea.areaId && !GetAreaEntryByAreaID(spellArea.areaId))
        {
            sLog.outErrorDb("Spell %u listed in `spell_area` have wrong area (%u) requirement", spell, spellArea.areaId);
            continue;
        }

        if (spellArea.conditionId && !sConditionStorage.LookupEntry<ConditionEntry>(spellArea.conditionId))
        {
            sLog.outErrorDb("Spell %u listed in `spell_area` have wrong conditionId (%u) requirement", spell, spellArea.conditionId);
            continue;
        }
        if (!spellArea.conditionId)
        {
            if (spellArea.questStart && !sObjectMgr.GetQuestTemplate(spellArea.questStart))
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have wrong start quest (%u) requirement", spell, spellArea.questStart);
                continue;
            }

            if (spellArea.questEnd)
            {
                if (!sObjectMgr.GetQuestTemplate(spellArea.questEnd))
                {
                    sLog.outErrorDb("Spell %u listed in `spell_area` have wrong end quest (%u) requirement", spell, spellArea.questEnd);
                    continue;
                }

                if (spellArea.questEnd == spellArea.questStart && !spellArea.questStartCanActive)
                {
                    sLog.outErrorDb("Spell %u listed in `spell_area` have quest (%u) requirement for start and end in same time", spell, spellArea.questEnd);
                    continue;
                }
            }

            if (spellArea.raceMask && (spellArea.raceMask & RACEMASK_ALL_PLAYABLE) == 0)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have wrong race mask (%u) requirement", spell, spellArea.raceMask);
                continue;
            }

            if (spellArea.gender != GENDER_NONE && spellArea.gender != GENDER_FEMALE && spellArea.gender != GENDER_MALE)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have wrong gender (%u) requirement", spell, spellArea.gender);
                continue;
            }
        }

        if (spellArea.auraSpell)
        {
            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(abs(spellArea.auraSpell));
            if (!spellInfo)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have wrong aura spell (%u) requirement", spell, abs(spellArea.auraSpell));
                continue;
            }

            bool validSpellEffect = false;
            for (uint32 i = EFFECT_INDEX_0; i < MAX_EFFECT_INDEX; ++i)
            {
                switch (spellInfo->EffectApplyAuraName[i])
                {
                    case SPELL_AURA_DUMMY:
                    case SPELL_AURA_GHOST:
                        validSpellEffect = true;
                        break;
                }
            }

            if (!validSpellEffect)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have aura spell requirement (%u) without dummy/ghost aura in spell effects", spell, abs(spellArea.auraSpell));
                continue;
            }

            if (uint32(abs(spellArea.auraSpell)) == spellArea.spellId)
            {
                sLog.outErrorDb("Spell %u listed in `spell_area` have aura spell (%u) requirement for itself", spell, abs(spellArea.auraSpell));
                continue;
            }

            // not allow autocast chains by auraSpell field (but allow use as alternative if not present) (禁止通过 auraSpell 形成自动施放链式循环)
            if (spellArea.autocast && spellArea.auraSpell > 0)
            {
                bool chain = false;
                SpellAreaForAuraMapBounds saBound = GetSpellAreaForAuraMapBounds(spellArea.spellId);
                for (SpellAreaForAuraMap::const_iterator itr = saBound.first; itr != saBound.second; ++itr)
                {
                    if (itr->second->autocast && itr->second->auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    sLog.outErrorDb("Spell %u listed in `spell_area` have aura spell (%u) requirement that itself autocast from aura", spell, spellArea.auraSpell);
                    continue;
                }

                SpellAreaMapBounds saBound2 = GetSpellAreaMapBounds(spellArea.auraSpell);
                for (SpellAreaMap::const_iterator itr2 = saBound2.first; itr2 != saBound2.second; ++itr2)
                {
                    if (itr2->second.autocast && itr2->second.auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    sLog.outErrorDb("Spell %u listed in `spell_area` have aura spell (%u) requirement that itself autocast from aura", spell, spellArea.auraSpell);
                    continue;
                }
            }
        }

        SpellArea const* sa = &mSpellAreaMap.insert(SpellAreaMap::value_type(spell, spellArea))->second;

        // for search by current zone/subzone at zone/subzone change
        if (spellArea.areaId)
            mSpellAreaForAreaMap.insert(SpellAreaForAreaMap::value_type(spellArea.areaId, sa));

        // for search at aura apply
        if (spellArea.auraSpell)
            mSpellAreaForAuraMap.insert(SpellAreaForAuraMap::value_type(abs(spellArea.auraSpell), sa));

        ++count;
    }
    while (queryResult->NextRow());

    sLog.outString(">> Loaded %u spell area requirements", count);
    sLog.outString();
}

/// 检查法术在当前地图、区域、子区域及战场/竞技场环境下的施放合法性
SpellCastResult SpellMgr::GetSpellAllowedInLocationError(SpellEntry const* spellInfo, uint32 map_id, uint32 zone_id, uint32 area_id, Player const* player) const
{
    // normal case (检查 Spell.dbc 自身的 AreaId 区域限制)
    if (spellInfo->AreaId > 0 && spellInfo->AreaId != zone_id && spellInfo->AreaId != area_id)
        return SPELL_FAILED_REQUIRES_AREA;

    // continent limitation (virtual continent), ignore for GM (仅限外域/飞行大陆限制，GM 豁免)
    if (spellInfo->HasAttribute(SPELL_ATTR_EX4_ONLY_FLYING_AREAS) && !(player && player->IsGameMaster()))
    {
        uint32 v_map = GetVirtualMapForMapAndZone(map_id, zone_id);
        MapEntry const* mapEntry = sMapStore.LookupEntry(v_map);
        if (!mapEntry || mapEntry->addon < 1 || !mapEntry->IsContinent())
            return SPELL_FAILED_REQUIRES_AREA;
    }

    // raid instance limitation (团队副本禁用限制)
    if (spellInfo->HasAttribute(SPELL_ATTR_EX6_NOT_IN_RAID_INSTANCES))
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(map_id);
        if (!mapEntry || mapEntry->IsRaid())
            return SPELL_FAILED_REQUIRES_AREA;
    }

    // DB base check (if non empty then must fit at least single for allow) (若 spell_area 存在配置，至少需满足其中一条)
    SpellAreaMapBounds saBounds = GetSpellAreaMapBounds(spellInfo->Id);
    if (saBounds.first != saBounds.second)
    {
        for (SpellAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        {
            if (itr->second.IsFitToRequirements(player, zone_id, area_id))
                return SPELL_CAST_OK;
        }
        return SPELL_FAILED_REQUIRES_AREA;
    }

    // bg spell checks

    // do not allow spells to be cast in arenas (竞技场内禁止标志或冷却超过 15 分钟的法术)
    // - with SPELL_ATTR_EX4_NOT_USABLE_IN_ARENA flag
    // - with greater than 15 min CD
    if (spellInfo->HasAttribute(SPELL_ATTR_EX4_NOT_IN_ARENA) ||
            (GetSpellRecoveryTime(spellInfo) > 15 * MINUTE * IN_MILLISECONDS && !spellInfo->HasAttribute(SPELL_ATTR_EX4_IGNORE_DEFAULT_ARENA_RESTRICTIONS)))
        if (player && player->InArena())
            return SPELL_FAILED_NOT_IN_ARENA;

    // Spell casted only on battleground (仅限战场使用的法术)
    if (spellInfo->HasAttribute(SPELL_ATTR_EX3_ONLY_BATTLEGROUNDS))
        if (!player || !player->InBattleGround())
            return SPELL_FAILED_ONLY_BATTLEGROUNDS;

    switch (spellInfo->Id)
    {
        // a trinket in alterac valley allows to teleport to the boss
        case 22564:                                         // recall
        case 22563:                                         // recall
        {
            if (!player)
                return SPELL_FAILED_REQUIRES_AREA;
            BattleGround* bg = player->GetBattleGround();
            return map_id == 30 && bg && bg->GetStatus() != STATUS_WAIT_JOIN ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA;
        }
        case 2584:                                          // Waiting to Resurrect
        case 42792:                                         // Recently Dropped Flag
        case 22011:                                         // Spirit Heal Channel
        case 22012:                                         // Spirit Heal
        case 24171:                                         // Resurrection Impact Visual
        case 44535:                                         // Spirit Heal (mana)
        {
            MapEntry const* mapEntry = sMapStore.LookupEntry(map_id);
            if (!mapEntry)
                return SPELL_FAILED_REQUIRES_AREA;
            return mapEntry->IsBattleGround() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_BATTLEGROUNDS;
        }
        case 44521:                                         // Preparation
        {
            if (!player)
                return SPELL_FAILED_REQUIRES_AREA;

            BattleGround* bg = player->GetBattleGround();
            return bg && bg->GetStatus() == STATUS_WAIT_JOIN ? SPELL_CAST_OK : SPELL_FAILED_ONLY_BATTLEGROUNDS;
        }
        case 32724:                                         // Gold Team (Alliance)
        case 32725:                                         // Green Team (Alliance)
        case 35774:                                         // Gold Team (Horde)
        case 35775:                                         // Green Team (Horde)
        {
            return player && player->InArena() ? SPELL_CAST_OK : SPELL_FAILED_ONLY_IN_ARENA;
        }
        case 32727:                                         // Arena Preparation
        {
            if (!player)
                return SPELL_FAILED_REQUIRES_AREA;
            if (!player->InArena())
                return SPELL_FAILED_REQUIRES_AREA;

            BattleGround* bg = player->GetBattleGround();
            return bg && bg->GetStatus() == STATUS_WAIT_JOIN ? SPELL_CAST_OK : SPELL_FAILED_ONLY_IN_ARENA;
        }
    }

    return SPELL_CAST_OK;
}

/// 从 SkillLineAbility.dbc 构建按法术 ID 与技能 ID 索引的双重映射表
void SpellMgr::LoadSkillLineAbilityMaps()
{
    mSkillLineAbilityMapBySpellId.clear();
    mSkillLineAbilityMapBySkillId.clear();

    const uint32 rows = sSkillLineAbilityStore.GetNumRows();
    uint32 count = 0;

    BarGoLink bar(rows);
    for (uint32 row = 0; row < rows; ++row)
    {
        bar.step();
        if (SkillLineAbilityEntry const* entry = sSkillLineAbilityStore.LookupEntry(row))
        {
            mSkillLineAbilityMapBySpellId.insert(SkillLineAbilityMap::value_type(entry->spellId, entry));
            mSkillLineAbilityMapBySkillId.insert(SkillLineAbilityMap::value_type(entry->skillId, entry));
            ++count;
        }
    }

    sLog.outString(">> Loaded %u SkillLineAbility MultiMaps Data", count);
    sLog.outString();
}

/// 从 SkillRaceClassInfo.dbc 构建按技能 ID 索引的种族/职业限制映射表
void SpellMgr::LoadSkillRaceClassInfoMap()
{
    mSkillRaceClassInfoMap.clear();

    BarGoLink bar(sSkillRaceClassInfoStore.GetNumRows());
    uint32 count = 0;

    for (uint32 i = 0; i < sSkillRaceClassInfoStore.GetNumRows(); ++i)
    {
        bar.step();
        SkillRaceClassInfoEntry const* skillRCInfo = sSkillRaceClassInfoStore.LookupEntry(i);
        if (!skillRCInfo)
            continue;

        // not all skills really listed in ability skills list (过滤无对应 SkillLine 的无效技能条目)
        if (!sSkillLineStore.LookupEntry(skillRCInfo->skillId))
            continue;

        mSkillRaceClassInfoMap.insert(SkillRaceClassInfoMap::value_type(skillRCInfo->skillId, skillRCInfo));

        ++count;
    }

    sLog.outString(">> Loaded %u SkillRaceClassInfo MultiMap Data", count);
    sLog.outString();
}

/// 启动校验工具：对照数据库校验表核对代码中硬编码引用的法术 ID、家族掩码、图标与效果是否匹配
void SpellMgr::CheckUsedSpells(char const* table) const
{
    uint32 countSpells = 0;
    uint32 countMasks = 0;

    //                                              0       1               2               3         4           5             6          7          8         9    10
    auto queryResult = WorldDatabase.PQuery("SELECT spellid,SpellFamilyName,SpellFamilyMask,SpellIcon,SpellVisual,SpellCategory,EffectType,EffectAura,EffectIdx,Name,Code FROM %s", table);

    if (!queryResult)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb("`%s` table is empty!", table);
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        Field* fields = queryResult->Fetch();

        bar.step();

        uint32 spell       = fields[0].GetUInt32();
        int32  family      = fields[1].GetInt32();
        uint64 familyMask  = fields[2].GetUInt64();
        int32  spellIcon   = fields[3].GetInt32();
        int32  spellVisual = fields[4].GetInt32();
        int32  category    = fields[5].GetInt32();
        int32  effectType  = fields[6].GetInt32();
        int32  auraType    = fields[7].GetInt32();
        int32  effectIdx   = fields[8].GetInt32();
        std::string name   = fields[9].GetCppString();
        std::string code   = fields[10].GetCppString();

        // checks of correctness requirements itself

        if (family < -1 || family > SPELLFAMILY_UNK3)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellFamily value(%u), skipped.", table, spell, family);
            continue;
        }

        // TODO: spellIcon check need dbc loading
        if (spellIcon < -1)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellIcon value(%u), skipped.", table, spell, spellIcon);
            continue;
        }

        // TODO: spellVisual check need dbc loading
        if (spellVisual < -1)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellVisual value(%u), skipped.", table, spell, spellVisual);
            continue;
        }

        // TODO: for spellCategory better check need dbc loading
        if (category < -1 || (category >= 0 && sSpellCategoryStore.find(category) == sSpellCategoryStore.end()))
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellCategory value(%u), skipped.", table, spell, category);
            continue;
        }

        if (effectType < -1 || effectType >= MAX_SPELL_EFFECTS)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellEffect type value(%u), skipped.", table, spell, effectType);
            continue;
        }

        if (auraType < -1 || auraType >= TOTAL_AURAS)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellAura type value(%u), skipped.", table, spell, auraType);
            continue;
        }

        if (effectIdx < -1 || effectIdx >= 3)
        {
            sLog.outError("Table '%s' for spell %u have wrong EffectIdx value(%u), skipped.", table, spell, effectIdx);
            continue;
        }

        // now checks of requirements (对照 Spell.dbc 模板逐项核对法术属性)

        if (spell)
        {
            ++countSpells;

            SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(spell);
            if (!spellEntry)
            {
                sLog.outError("Spell %u '%s' not exist but used in %s.", spell, name.c_str(), code.c_str());
                continue;
            }

            if (family >= 0 && spellEntry->SpellFamilyName != uint32(family))
            {
                sLog.outError("Spell %u '%s' family(%u) <> %u but used in %s.", spell, name.c_str(), spellEntry->SpellFamilyName, family, code.c_str());
                continue;
            }

            if (familyMask != uint64(0xFFFFFFFFFFFFFFFF))
            {
                if (familyMask == uint64(0x0000000000000000))
                {
                    if (spellEntry->SpellFamilyFlags)
                    {
                        sLog.outError("Spell %u '%s' not fit to (" UI64FMTD ") but used in %s.",
                                      spell, name.c_str(), familyMask, code.c_str());
                        continue;
                    }
                }
                else
                {
                    if (!spellEntry->IsFitToFamilyMask(familyMask))
                    {
                        sLog.outError("Spell %u '%s' not fit to (" UI64FMTD ") but used in %s.", spell, name.c_str(), familyMask, code.c_str());
                        continue;
                    }
                }
            }

            if (spellIcon >= 0 && spellEntry->SpellIconID != uint32(spellIcon))
            {
                sLog.outError("Spell %u '%s' icon(%u) <> %u but used in %s.", spell, name.c_str(), spellEntry->SpellIconID, spellIcon, code.c_str());
                continue;
            }

            if (spellVisual >= 0 && spellEntry->SpellVisual != uint32(spellVisual))
            {
                sLog.outError("Spell %u '%s' visual(%u) <> %u but used in %s.", spell, name.c_str(), spellEntry->SpellVisual, spellVisual, code.c_str());
                continue;
            }

            if (category >= 0 && spellEntry->Category != uint32(category))
            {
                sLog.outError("Spell %u '%s' category(%u) <> %u but used in %s.", spell, name.c_str(), spellEntry->Category, category, code.c_str());
                continue;
            }

            if (effectIdx >= EFFECT_INDEX_0)
            {
                if (effectType >= 0 && spellEntry->Effect[effectIdx] != uint32(effectType))
                {
                    sLog.outError("Spell %u '%s' effect%d <> %u but used in %s.", spell, name.c_str(), effectIdx + 1, effectType, code.c_str());
                    continue;
                }

                if (auraType >= 0 && spellEntry->EffectApplyAuraName[effectIdx] != uint32(auraType))
                {
                    sLog.outError("Spell %u '%s' aura%d <> %u but used in %s.", spell, name.c_str(), effectIdx + 1, auraType, code.c_str());
                }
            }
            else
            {
                if (effectType >= 0 && !IsSpellHaveEffect(spellEntry, SpellEffects(effectType)))
                {
                    sLog.outError("Spell %u '%s' not have effect %u but used in %s.", spell, name.c_str(), effectType, code.c_str());
                    continue;
                }

                if (auraType >= 0 && !IsSpellHaveAura(spellEntry, AuraType(auraType)))
                {
                    sLog.outError("Spell %u '%s' not have aura %u but used in %s.", spell, name.c_str(), auraType, code.c_str());
                }
            }
        }
        else
        {
            ++countMasks;

            bool found = false;
            for (uint32 spellId = 1; spellId < sSpellTemplate.GetMaxEntry(); ++spellId)
            {
                SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
                if (!spellEntry)
                    continue;

                if (family >= 0 && spellEntry->SpellFamilyName != uint32(family))
                    continue;

                if (familyMask != uint64(0xFFFFFFFFFFFFFFFF))
                {
                    if (familyMask == uint64(0x0000000000000000))
                    {
                        if (spellEntry->SpellFamilyFlags)
                            continue;
                    }
                    else
                    {
                        if (!spellEntry->IsFitToFamilyMask(familyMask))
                            continue;
                    }
                }

                if (spellIcon >= 0 && spellEntry->SpellIconID != uint32(spellIcon))
                    continue;

                if (spellVisual >= 0 && spellEntry->SpellVisual != uint32(spellVisual))
                    continue;

                if (category >= 0 && spellEntry->Category != uint32(category))
                    continue;

                if (effectIdx >= 0)
                {
                    if (effectType >= 0 && spellEntry->Effect[effectIdx] != uint32(effectType))
                        continue;

                    if (auraType >= 0 && spellEntry->EffectApplyAuraName[effectIdx] != uint32(auraType))
                        continue;
                }
                else
                {
                    if (effectType >= 0 && !IsSpellHaveEffect(spellEntry, SpellEffects(effectType)))
                        continue;

                    if (auraType >= 0 && !IsSpellHaveAura(spellEntry, AuraType(auraType)))
                        continue;
                }

                found = true;
                break;
            }

            if (!found)
            {
                if (effectIdx >= 0)
                    sLog.outError("Spells '%s' not found for family %i (" UI64FMTD ") icon(%i) visual(%i) category(%i) effect%d(%i) aura%d(%i) but used in %s",
                                  name.c_str(), family, familyMask, spellIcon, spellVisual, category, effectIdx + 1, effectType, effectIdx + 1, auraType, code.c_str());
                else
                    sLog.outError("Spells '%s' not found for family %i (" UI64FMTD ") icon(%i) visual(%i) category(%i) effect(%i) aura(%i) but used in %s",
                                  name.c_str(), family, familyMask, spellIcon, spellVisual, category, effectType, auraType, code.c_str());
            }
        }
    }
    while (queryResult->NextRow());

    sLog.outString();
    sLog.outString(">> Checked %u spells and %u spell masks", countSpells, countMasks);
}

/// 获取法术所属的控制递减（DR）组别（优先匹配职业家族特例，再回退到机制掩码）
DiminishingGroup GetDiminishingReturnsGroupForSpell(SpellEntry const* spellproto, bool triggered)
{
    // Explicit Diminishing Groups (显式按职业家族和法术 ID 映射特定递减组别)
    switch (spellproto->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
            // some generic arena related spells have by some strange reason MECHANIC_TURN
            if (spellproto->Mechanic == MECHANIC_TURN)
                return DIMINISHING_NONE;
            // Hunter Pet Intimidation
            else if (spellproto->Id == 24394)
                return DIMINISHING_CONTROL_STUN;
            break;
        case SPELLFAMILY_MAGE:
        {
            if (spellproto->IsFitToFamilyMask(uint64(0x000800000)))
                return DIMINISHING_DRAGONS_BREATH;
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            // Kidney Shot
            if (spellproto->IsFitToFamilyMask(uint64(0x00000200000)))
                return DIMINISHING_KIDNEYSHOT;
            // Blind
            else if (spellproto->IsFitToFamilyMask(uint64(0x00001000000)))
                return DIMINISHING_BLIND_CYCLONE;
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            // Freezing Trap
            if (spellproto->IsFitToFamilyMask(uint64(0x00000000008)))
                return DIMINISHING_FREEZE;
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Seduction
            // ToDo: Fix  diminishing returns aspect of this spell, currently limited to 10secs in pvp only as setting DIMINISHING_FEAR to DRTYPE_ALL messes with PvE
            if (spellproto->IsFitToFamilyMask(uint64(0x00040000000)) && spellproto->Id != 7870) // Exclude Lesser Invisibility
                return DIMINISHING_FEAR;
            /* Unstable Affliction Dispel Silence
            // ToDo: Fix diminishing returns aspect for this spell 5 2.5 1.25 0
            else if (spellproto->Id == 31117)
                return DIMINISHING_UNSTABLE_AFFLICTION_SILENCE;*/
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Cyclone
            if (spellproto->IsFitToFamilyMask(uint64(0x02000000000)))
                return DIMINISHING_BLIND_CYCLONE;
            // Nature's Grasp Triggered Root
            else if (spellproto->SpellFamilyFlags & 0x00000000200LL && spellproto->Attributes == 0x49010000)
                return DIMINISHING_CONTROL_ROOT;
            // Feral Charge Root Effect
            else if (spellproto->Id == 45334)
                return DIMINISHING_NONE;
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            // Hamstring - limit duration to 10s in PvP
            if (spellproto->IsFitToFamilyMask(uint64(0x00000000002)))
                return DIMINISHING_LIMITONLY;
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // ToDo: Fix  diminishing returns aspect of this spell, currently limited to 10secs in pvp only as setting DIMINISHING_FEAR to DRTYPE_ALL messes with PvE
            // Turn Evil - Turn Undead(Rank 3):This spell has been reworked and has been renamed to "Turn Evil".
            // It will now work on Demons in addition to Undead. Turn Evil is subject to diminishing returns, and lasts 10 seconds in PvP. - https://wowwiki.fandom.com/wiki/Patch_2.4.0
            if (spellproto->IsFitToFamilyMask(uint64(0x0000400000000000)) && spellproto->Attributes == 0x40010000)
                return DIMINISHING_FEAR;
            break;
        }
        case SPELLFAMILY_UNK1:
            return DIMINISHING_NONE;
        default:
            break;
    }

    // Get by mechanic (未命中职业特例时，依据法术控制机制类型映射递减组别)
    uint32 mechanic = GetAllSpellMechanicMask(spellproto);
    if (!mechanic)
        return DIMINISHING_NONE;

    if (mechanic & convertEnumToFlag(MECHANIC_STUN))
        return triggered ? DIMINISHING_TRIGGER_STUN : DIMINISHING_CONTROL_STUN;
    if (mechanic & convertEnumToFlag(MECHANIC_SLEEP))
        return DIMINISHING_SLEEP;
    if (mechanic & convertEnumToFlag(MECHANIC_KNOCKOUT, MECHANIC_SAPPED, MECHANIC_POLYMORPH))
        return DIMINISHING_KNOCKOUT_POLYMORPH_SAPPED;
    if (mechanic & convertEnumToFlag(MECHANIC_ROOT))
        return triggered ? DIMINISHING_TRIGGER_ROOT : DIMINISHING_CONTROL_ROOT;
    if (mechanic & convertEnumToFlag(MECHANIC_FEAR))
        return DIMINISHING_FEAR;
    if (mechanic & convertEnumToFlag(MECHANIC_CHARM))
        return DIMINISHING_CHARM;
    //if (mechanic & convertEnumToFlag(MECHANIC_SILENCE))
    //    return DIMINISHING_SILENCE;
    if (mechanic & convertEnumToFlag(MECHANIC_DISARM))
        return DIMINISHING_DISARM;
    if (mechanic & convertEnumToFlag(MECHANIC_FREEZE))
        return DIMINISHING_FREEZE;
    if (mechanic & convertEnumToFlag(MECHANIC_BANISH))
        return DIMINISHING_BANISH;
    if (mechanic & convertEnumToFlag(MECHANIC_HORROR))
        return DIMINISHING_DEATHCOIL;

    return DIMINISHING_NONE;
}

/// 判断指定的控制递减组别在 PvP 中是否有最大持续时间上限
bool IsDiminishingReturnsGroupDurationLimited(DiminishingGroup group)
{
    switch (group)
    {
        case DIMINISHING_CONTROL_STUN:
        case DIMINISHING_TRIGGER_STUN:
        case DIMINISHING_KIDNEYSHOT:
        case DIMINISHING_SLEEP:
        case DIMINISHING_CONTROL_ROOT:
        case DIMINISHING_TRIGGER_ROOT:
        case DIMINISHING_FEAR:
        case DIMINISHING_CHARM:
        case DIMINISHING_KNOCKOUT_POLYMORPH_SAPPED:
        case DIMINISHING_FREEZE:
        case DIMINISHING_BLIND_CYCLONE:
        case DIMINISHING_BANISH:
        case DIMINISHING_LIMITONLY:
        case DIMINISHING_DRAGONS_BREATH:
            return true;
        default:
            return false;
    }
}

/// 判断指定递减组别在当前战斗类型（PvP/PvE）下是否对持续时间进行递减折算
bool IsDiminishingReturnsGroupDurationDiminished(DiminishingGroup group, bool pvp)
{
    switch (group)
    {
        case DIMINISHING_CHARM:
            return pvp;
        default:
            return true;
    }
}

/// 获取递减组别的适用范围类型（仅对玩家生效 DRTYPE_PLAYER 或全单位生效 DRTYPE_ALL）
DiminishingReturnsType GetDiminishingReturnsGroupType(DiminishingGroup group)
{
    switch (group)
    {
        case DIMINISHING_BLIND_CYCLONE:
        case DIMINISHING_CONTROL_STUN:
        case DIMINISHING_TRIGGER_STUN:
        case DIMINISHING_KIDNEYSHOT:
        case DIMINISHING_CHARM:
        //case DIMINISHING_FEAR: Fix PvE no DR vs PvP DR
            return DRTYPE_ALL;
        case DIMINISHING_SLEEP:
        case DIMINISHING_CONTROL_ROOT:
        case DIMINISHING_TRIGGER_ROOT:
        case DIMINISHING_FEAR:
        case DIMINISHING_KNOCKOUT_POLYMORPH_SAPPED:
        //case DIMINISHING_SILENCE: Patch 3.0.8+ only
        case DIMINISHING_DISARM:
        case DIMINISHING_DEATHCOIL:
        case DIMINISHING_FREEZE:
        case DIMINISHING_BANISH:
        case DIMINISHING_DRAGONS_BREATH:
            return DRTYPE_PLAYER;
        default:
            break;
    }

    return DRTYPE_NONE;
}

/// 判断指定递减组别在当前战斗环境下是否追踪递减层数
bool IsSubjectToDiminishingLevels(DiminishingGroup group, bool pvp)
{
    if ((GetDiminishingReturnsGroupType(group) == DRTYPE_PLAYER && pvp) || GetDiminishingReturnsGroupType(group) == DRTYPE_ALL)
        return true;
    return false;
}

/// 判断法术是否为特殊的生物施放且受控递减的技能
bool IsCreatureDRSpell(SpellEntry const* spellInfo)
{
    switch (spellInfo->Id)
    {
        case 36924: // Harbinger Skyriss - Mind Rend
        case 31408: // Lesser Doomguard - War Stomp
        case 31480: return true; // Kazrogal War Stomp - confirmed via video
        default: return false;
    }
}

/// 检查玩家在当前区域与子区域是否满足该 SpellArea 条目的全部前置条件
bool SpellArea::IsFitToRequirements(Player const* player, uint32 newZone, uint32 newArea) const
{
    if (conditionId)
    {
        if (!player || !sObjectMgr.IsConditionSatisfied(conditionId, player, player->GetMap(), nullptr, CONDITION_FROM_SPELL_AREA))
            return false;
    }
    else                                                    // This block will be removed
    {
        if (gender != GENDER_NONE)
        {
            // not in expected gender
            if (!player || gender != player->getGender())
                return false;
        }

        if (raceMask)
        {
            // not in expected race
            if (!player || !(raceMask & player->getRaceMask()))
                return false;
        }

        if (questStart)
        {
            // not in expected required quest state
            if (!player || ((!questStartCanActive || !player->IsActiveQuest(questStart)) && !player->GetQuestRewardStatus(questStart)))
                return false;
        }

        if (questEnd)
        {
            // not in expected forbidden quest state
            if (!player || player->GetQuestRewardStatus(questEnd))
                return false;
        }
    }

    if (areaId)
    {
        // not in expected zone
        if (newZone != areaId && newArea != areaId)
            return false;
    }

    if (auraSpell)
    {
        // not have expected aura
        if (!player)
            return false;
        if (auraSpell > 0)
            // have expected aura
            return player->HasAura(auraSpell);
            // not have expected aura
        return !player->HasAura(-auraSpell);
    }

    return true;
}

/// 根据区域条件检查结果，为玩家自动施加或移除对应的区域光环法术
void SpellArea::ApplyOrRemoveSpellIfCan(Player* player, uint32 newZone, uint32 newArea, bool onlyApply) const
{
    MANGOS_ASSERT(player);

    if (IsFitToRequirements(player, newZone, newArea))
    {
        if (autocast && !player->HasAura(spellId))
            player->CastSpell(player, spellId, TRIGGERED_OLD_TRIGGERED);
    }
    else if (!onlyApply && player->HasAura(spellId))
        player->RemoveAurasDueToSpell(spellId);
}

/// spell_affect 表加载回调工作器：将影响掩码同步填充至同等级链的高等级法术
struct DoSpellAffectMasks
{
    DoSpellAffectMasks(SpellAffectMap& _spellAffectMap, uint8& _effectId, uint64& _spellAffectMask) : spellAffectMap(_spellAffectMap), effectId(_effectId), spellAffectMask(_spellAffectMask) {}
    void operator()(uint32 spell_id) { spellAffectMap.insert(SpellAffectMap::value_type((spell_id << 8) + effectId, spellAffectMask)); }

    SpellAffectMap& spellAffectMap;
    uint8& effectId;
    uint64& spellAffectMask;
};

/// 从数据库 spell_affect 表加载法术家族影响掩码，并校验可能缺失配置的修饰类光环
void SpellMgr::LoadSpellAffects()
{
    mSpellAffectMap.clear();                                // need for reload case (重载前清空影响掩码映射)

    uint32 count = 0;

    //                                             0      1         2
    auto queryResult = WorldDatabase.Query("SELECT entry, effectId, SpellFamilyMask FROM spell_affect");
    if (!queryResult)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u spell affect definitions", count);
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        Field* fields = queryResult->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();
        uint8 effectId = fields[1].GetUInt8();

        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(entry);

        if (!spellInfo)
        {
            sLog.outErrorDb("Spell %u listed in `spell_affect` does not exist", entry);
            continue;
        }

        if (effectId >= MAX_EFFECT_INDEX)
        {
            sLog.outErrorDb("Spell %u listed in `spell_affect` have invalid effect index (%u)", entry, effectId);
            continue;
        }

        uint32 first_id = GetFirstSpellInChain(entry);

        if (first_id != entry)
        {
            sLog.outErrorDb("Spell %u listed in `spell_affect` is not first rank (%u) in chain", entry, first_id);
            // prevent loading since it won't have an effect anyway
            continue;
        }

        if (spellInfo->Effect[effectId] != SPELL_EFFECT_APPLY_AURA || (
                    spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_DUMMY &&
                    spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_ADD_FLAT_MODIFIER &&
                    spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_ADD_PCT_MODIFIER  &&
                    spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_ADD_TARGET_TRIGGER &&
                    spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_IGNORE_COMBAT_RESULT &&
                    spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_OVERRIDE_CLASS_SCRIPTS))
        {
            sLog.outErrorDb("Spell %u listed in `spell_affect` have not SPELL_AURA_ADD_FLAT_MODIFIER (%u) or SPELL_AURA_ADD_PCT_MODIFIER (%u) or SPELL_AURA_ADD_TARGET_TRIGGER (%u) or SPELL_AURA_IGNORE_COMBAT_RESULT (%u) or SPELL_AURA_OVERRIDE_CLASS_SCRIPTS (%u) for effect index (%u)", entry, SPELL_AURA_ADD_FLAT_MODIFIER, SPELL_AURA_ADD_PCT_MODIFIER, SPELL_AURA_ADD_TARGET_TRIGGER, SPELL_AURA_IGNORE_COMBAT_RESULT, SPELL_AURA_OVERRIDE_CLASS_SCRIPTS, effectId);
            continue;
        }

        uint64 spellAffectMask = fields[2].GetUInt64();

        // Spell.dbc have own data for low part of SpellFamilyMask (若与 DBC 自带 EffectItemType 完全一致则提示冗余跳过)
        if (spellInfo->EffectItemType[effectId])
        {
            if (static_cast<uint64>(spellInfo->EffectItemType[effectId]) == spellAffectMask)
            {
                sLog.outErrorDb("Spell %u listed in `spell_affect` have redundant (same with EffectItemType%d) data for effect index (%u) and not needed, skipped.", entry, effectId + 1, effectId);
                continue;
            }
        }

        mSpellAffectMap.insert(SpellAffectMap::value_type((entry << 8) + effectId, spellAffectMask));

        DoSpellAffectMasks worker(mSpellAffectMap, effectId, spellAffectMask);
        doForHighRanks(entry, worker);

        ++count;
    }
    while (queryResult->NextRow());

    sLog.outString();
    sLog.outString(">> Loaded %u spell affect definitions", count);

    for (uint32 id = 0; id < sSpellTemplate.GetMaxEntry(); ++id)
    {
        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(id);
        if (!spellInfo)
            continue;

        for (int effectId = 0; effectId < MAX_EFFECT_INDEX; ++effectId)
        {
            if (spellInfo->Effect[effectId] != SPELL_EFFECT_APPLY_AURA || (
                        spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_ADD_FLAT_MODIFIER &&
                        spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_ADD_PCT_MODIFIER  &&
                        spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_ADD_TARGET_TRIGGER &&
                        spellInfo->EffectApplyAuraName[effectId] != SPELL_AURA_IGNORE_COMBAT_RESULT))
                continue;

            if (spellInfo->EffectItemType[effectId] != 0)
                continue;

            if (mSpellAffectMap.find((id << 8) + effectId) !=  mSpellAffectMap.end())
                continue;

            sLog.outErrorDb("Spell %u (%s) misses spell_affect for effect %u", id, spellInfo->SpellName[sWorld.GetDefaultDbcLocale()], effectId);
        }
    }
}
