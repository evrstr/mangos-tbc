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

#include "Skills/SkillExtraItems.h"
#include "Database/DatabaseEnv.h"
#include "Log/Log.h"
#include "Util/ProgressBar.h"
#include "Entities/Player.h"
#include "Server/DBCStores.h"

#include <map>

// some type definitions
// no use putting them in the header file, they're only used in this .cpp
// 仅在当前实现文件内使用的专精额外产出数据结构

// struct to store information about extra item creation
// one entry for every spell that is able to create an extra item
// 专业专精额外产出配置项（对应一个制造法术）
struct SkillExtraItemEntry
{
    // the spell id of the specialization required to create extra items (触发额外产出所需拥有的专精法术 ID，如药水专精/转化专精)
    uint32 requiredSpecialization;
    // the chance to create one additional item (每次额外产出一个物品的概率)
    float additionalCreateChance;
    // maximum number of extra items created per crafting (单次制造最多可额外产出的物品数量)
    uint8 additionalMaxNum;

    SkillExtraItemEntry()
        : requiredSpecialization(0), additionalCreateChance(0.0f), additionalMaxNum(0) {}

    SkillExtraItemEntry(uint32 rS, float aCC, uint8 aMN)
        : requiredSpecialization(rS), additionalCreateChance(aCC), additionalMaxNum(aMN) {}
};

// map to store the extra item creation info, the key is the spellId of the creation spell, the mapped value is the assigned SkillExtraItemEntry
// 制造法术 ID 到额外产出配置的映射表
typedef std::map<uint32, SkillExtraItemEntry> SkillExtraItemMap;

SkillExtraItemMap SkillExtraItemStore;

// loads the extra item creation info from DB (从 skill_extra_item_template 表加载专精额外产出规则)
void LoadSkillExtraItemTable()
{
    uint32 count = 0;

    SkillExtraItemStore.clear();                            // need for reload (支持重载)

    //                                                 0        1                       2                       3
    auto queryResult = WorldDatabase.Query("SELECT spellId, requiredSpecialization, additionalCreateChance, additionalMaxNum FROM skill_extra_item_template");

    if (queryResult)
    {
        BarGoLink bar(queryResult->GetRowCount());

        do
        {
            Field* fields = queryResult->Fetch();
            bar.step();

            uint32 spellId = fields[0].GetUInt32();

            if (!sSpellTemplate.LookupEntry<SpellEntry>(spellId))
            {
                sLog.outError("Skill specialization %u has nonexistent spell id in `skill_extra_item_template`!", spellId);
                continue;
            }

            uint32 requiredSpecialization = fields[1].GetUInt32();
            if (!sSpellTemplate.LookupEntry<SpellEntry>(requiredSpecialization))
            {
                sLog.outError("Skill specialization %u have nonexistent required specialization spell id %u in `skill_extra_item_template`!", spellId, requiredSpecialization);
                continue;
            }

            float additionalCreateChance = fields[2].GetFloat();
            if (additionalCreateChance <= 0.0f)
            {
                sLog.outError("Skill specialization %u has too low additional create chance in `skill_extra_item_template`!", spellId);
                continue;
            }

            uint8 additionalMaxNum = fields[3].GetUInt8();
            if (!additionalMaxNum)
            {
                sLog.outError("Skill specialization %u has 0 max number of extra items in `skill_extra_item_template`!", spellId);
                continue;
            }

            SkillExtraItemEntry& skillExtraItemEntry = SkillExtraItemStore[spellId];

            skillExtraItemEntry.requiredSpecialization = requiredSpecialization;
            skillExtraItemEntry.additionalCreateChance = additionalCreateChance;
            skillExtraItemEntry.additionalMaxNum       = additionalMaxNum;

            ++count;
        }
        while (queryResult->NextRow());

        sLog.outString(">> Loaded %u spell specialization definitions", count);
    }
    else
        sLog.outString(">> Loaded 0 spell specialization definitions. DB table `skill_extra_item_template` is empty.");

    sLog.outString();
}

// 检查玩家是否已学会所需专精并输出对应的额外产出概率与数量上限
bool canCreateExtraItems(Player* player, uint32 spellId, float& additionalChance, uint8& additionalMax)
{
    // get the info for the specified spell (查找该制造法术的专精配置)
    SkillExtraItemMap::const_iterator ret = SkillExtraItemStore.find(spellId);
    if (ret == SkillExtraItemStore.end())
        return false;

    SkillExtraItemEntry const* specEntry = &ret->second;

    // if no entry, then no extra items can be created
    if (!specEntry)
        return false;

    // the player doesn't have the required specialization, return false (玩家未学会所需专精法术则不触发)
    if (!player->HasSpell(specEntry->requiredSpecialization))
        return false;

    // set the arguments to the appropriate values (回填额外产出概率与最大额外件数)
    additionalChance = specEntry->additionalCreateChance;
    additionalMax = specEntry->additionalMaxNum;

    // enable extra item creation
    return true;
}
