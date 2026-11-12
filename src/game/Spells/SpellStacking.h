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

#ifndef _SPELLSTACKING_H
#define _SPELLSTACKING_H

#include "Server/DBCEnums.h"
#include "Platform/Define.h"
#include "Globals/FlagConvertibleEnum.h"
#include <map>
#include <vector>
#include <string>

struct SpellEntry;
class Unit;

// 法术组叠加规则枚举（对应数据库 spell_group.Rule）
enum class SpellGroupRule
{
    UNIQUE              = 1,                                // 组内法术互斥，同一目标只能存在一个
    UNIQUE_PER_CASTER   = 2,                                // 同施法者互斥（不同施法者可共存）
    MAX                 = 3                                 // 规则枚举上限
};

// 硬编码或特殊处理的法术组 ID
enum class SpellGroupId
{
    WELL_FED = 3, // hardcoded for current compliance with old code (进食充分食物增益组，硬编码兼容旧逻辑)
    MAX = 64,     // 64 位位掩码支持的法术组上限
};

DEFINE_FLAG_CONVERTIBLE_ENUM_64(SpellGroupId);

// 数据库法术组定义（对应 spell_group 表）
struct SpellGroup
{
    uint32 Id;                                              // 法术组 ID
    SpellGroupRule rule;                                    // 叠加/互斥规则
    std::string name;                                       // 法术组名称
    std::vector<uint32> spellIds;                           // 属于该组的法术 ID 列表
};

// 单个法术所属组的位掩码与规则缓存
struct SpellGroupSpellData
{
    uint64 mask;                                            // 所属法术组 ID 位掩码（bit = groupId - 1）
    SpellGroupRule rule;                                    // 该法术适用的组规则

    // 判断该法术是否属于指定枚举标识的法术组
    bool HasFlag(FlagConvertibleEnum64 flag) const
    {
        return bool(FlagConvertibleEnum64(mask) & flag);
    }

    SpellGroupSpellData() : mask(0), rule((SpellGroupRule)0) {}
};

// 法术叠加规则管理器（负责加载 spell_group 并判定光环/法术是否可同时共存）
class SpellStacker
{
    public:
        SpellStacker() {}
        ~SpellStacker() {}

        static SpellStacker& Instance();

        // 从世界数据库加载 spell_group 与 spell_group_spell 表
        void LoadSpellGroups();

        // 判定两个法术在指定效果索引下的光环是否允许叠加
        bool IsStackableAuraEffect(SpellEntry const* entry, SpellEntry const* entry2, SpellEffectIndex effIdx, Unit* target) const;
        // 逐效果检查两个法术整体是否允许叠加
        bool IsStackableSpell(SpellEntry const* entry, SpellEntry const* entry2, Unit* target) const;
        // 判断两个法术（同施法者视角）是否可叠加
        bool IsSpellStackableWithSpell(SpellEntry const* entry1, SpellEntry const* entry2, Unit* target) const;
        // 判断不同施法者施放的两个法术是否可叠加
        bool IsSpellStackableWithSpellForDifferentCasters(SpellEntry const* entry1, SpellEntry const* entry2, bool isSameChain, Unit* target) const;

        // 获取指定法术所属的法术组数据（无则返回 nullptr）
        SpellGroupSpellData const* GetSpellGroupDataForSpell(uint32 spellId) const;

    private:
        std::map<uint32, SpellGroup> m_spellGroups;         // 按组 ID 存储的所有法术组
        std::map<uint32, SpellGroupSpellData> m_spellGroupSpellData; // 按法术 ID 存储的组掩码映射
};

#define sSpellStacker SpellStacker::Instance()

#endif