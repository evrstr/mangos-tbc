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

#ifndef _PLAYER_DEF_H
#define _PLAYER_DEF_H

#include "Globals/SharedDefines.h"
#include "Spells/SpellDefines.h"
#include "Server/DBCStructure.h"
#include <set>
#include <list>
#include <vector>
#include <string>

class Aura;

// Note: SPELLMOD_* values is aura types in fact
// 注意：SPELLMOD_* 值实际上对应光环类型（AuraType）
enum SpellModType
{
    SPELLMOD_FLAT = 107,                      // SPELL_AURA_ADD_FLAT_MODIFIER (固定值法术修正)
    SPELLMOD_PCT  = 108                       // SPELL_AURA_ADD_PCT_MODIFIER (百分比法术修正)
};

// Spell modifier (used for modify other spells)
// 法术修正器结构（用于修改其他法术的属性或效果）
struct SpellModifier
{
    static uint64 modCounter;                 // 全局修正器唯一 ID 自增计数器

    // 分配并返回下一个唯一的修正器 ID
    uint64 GetModId() { return modCounter++; }

    SpellModifier() : charges(0), priority(0), isFinite(false) {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, uint32 _spellId, uint64 _mask, int32 _priority = 0, int16 _charges = 0)
        : op(_op), type(_type), charges(_charges), value(_value), mask(_mask), spellId(_spellId), priority(_priority), isFinite(_charges>0), modId(GetModId())
    {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, uint32 _spellId, ClassFamilyMask _mask, int32 _priority = 0, int16 _charges = 0)
        : op(_op), type(_type), charges(_charges), value(_value), mask(_mask), spellId(_spellId), priority(_priority), isFinite(_charges>0), modId(GetModId())
    {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, SpellEntry const* spellEntry, SpellEffectIndex eff, int32 _priority = 0, int16 _charges = 0);

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, Aura const* aura, int32 _priority = 0, int16 _charges = 0);

    // 判断指定法术是否受此修正器影响
    bool isAffectedOnSpell(SpellEntry const* spell) const;

    SpellModOp   op : 8;                      // 修正操作类型
    SpellModType type : 8;                    // 修正方式（固定值或百分比）
    int16 charges : 16;                       // 剩余可用次数（0 表示无限制或已耗尽）
    int32 value;                              // 修正数值
    ClassFamilyMask mask;                     // 职业技能族掩码（匹配受影响法术）
    uint32 spellId;                           // 提供该修正的光环/天赋法术 ID
    int32 priority;                           // 优先级（用于排序应用顺序）
    bool isFinite;                            // 是否为有限次数修正器
    uint64 modId;                             // 修正器实例唯一标识
};

// 法术修正器优先级降序比较器
struct SpellModifierComparator
{
    bool operator() (SpellModifier* a, SpellModifier* b) { return (a->priority > b->priority); }
};

typedef std::list<SpellModifier*> SpellModList;
typedef std::vector<SpellModifier*> SpellModVector;

// 已消耗法术修正器标识对（法术 ID + 修正器 ID）
struct SpellModifierPair
{
    uint32 spellId;                           // 关联法术 ID
    uint64 modId;                             // 修正器唯一 ID
    SpellModifierPair() : spellId(0), modId(0) {}
    SpellModifierPair(uint32 _spellId, uint64 _modifierId) : spellId(_spellId), modId(_modifierId) {}
    bool operator<(const SpellModifierPair &rhs) const
    {
        return spellId < rhs.spellId;
    }
    bool operator>(const SpellModifierPair &rhs) const
    {
        return spellId > rhs.spellId;
    }
};

// typedef std::set<SpellModifierPair>  ConsumedSpellModifiers; (已注释：原已消耗修正器集合类型定义)

#endif