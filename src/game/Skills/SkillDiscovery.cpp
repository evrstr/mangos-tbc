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

#include "Database/DatabaseEnv.h"
#include "Log/Log.h"
#include "Util/ProgressBar.h"
#include "Policies/Singleton.h"
#include "Entities/Player.h"
#include "World/World.h"
#include "Util/Util.h"
#include "Skills/SkillDiscovery.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Entities/Player.h"
#include "Server/DBCStores.h"
#include <map>

// 单条专业技能领悟记录
struct SkillDiscoveryEntry
{
    uint32  spellId;                                        // discavered spell (可领悟到的目标法术/配方 ID)
    float   chance;                                         // chance (基础领悟触发概率，百分比)

    SkillDiscoveryEntry()
        : spellId(0), chance(0) {}

    SkillDiscoveryEntry(uint32 _spellId, float _chance)
        : spellId(_spellId), chance(_chance) {}
};

typedef std::list<SkillDiscoveryEntry> SkillDiscoveryList;
// 正数键表示前置法术 ID，负数键表示专业技能 ID 的相反数（reqSpell=0 时按技能匹配）
typedef std::unordered_map<int32, SkillDiscoveryList> SkillDiscoveryMap;

static SkillDiscoveryMap SkillDiscoveryStore;

// 从 skill_discovery_template 表加载全部配方领悟规则并校验法术有效性
void LoadSkillDiscoveryTable()
{
    SkillDiscoveryStore.clear();                            // need for reload (支持运行时重载)

    uint32 count = 0;

    //                                                0        1         2
    auto queryResult = WorldDatabase.Query("SELECT spellId, reqSpell, chance FROM skill_discovery_template");

    if (!queryResult)
    {
        sLog.outString(">> Loaded 0 skill discovery definitions. DB table `skill_discovery_template` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    std::ostringstream ssNonDiscoverableEntries;
    std::set<uint32> reportedReqSpells;

    do
    {
        Field* fields = queryResult->Fetch();
        bar.step();

        uint32 spellId         = fields[0].GetUInt32();
        int32  reqSkillOrSpell = fields[1].GetInt32();
        float  chance          = fields[2].GetFloat();

        if (chance <= 0)                                    // chance (概率非正则无法触发领悟，记录告警)
        {
            ssNonDiscoverableEntries << "spellId = " << spellId << " reqSkillOrSpell = " << reqSkillOrSpell
                                     << " chance = " << chance << "\n";
            continue;
        }

        if (reqSkillOrSpell > 0)                            // spell case (按特定制作法术触发领悟)
        {
            SpellEntry const* reqSpellEntry = sSpellTemplate.LookupEntry<SpellEntry>(reqSkillOrSpell);
            if (!reqSpellEntry)
            {
                if (reportedReqSpells.find(reqSkillOrSpell) == reportedReqSpells.end())
                {
                    sLog.outErrorDb("Spell (ID: %u) have nonexistent spell (ID: %i) in `reqSpell` field in `skill_discovery_template` table", spellId, reqSkillOrSpell);
                    reportedReqSpells.insert(reqSkillOrSpell);
                }
                continue;
            }

            if (reqSpellEntry->Mechanic != MECHANIC_DISCOVERY)
            {
                if (reportedReqSpells.find(reqSkillOrSpell) == reportedReqSpells.end())
                {
                    sLog.outErrorDb("Spell (ID: %u) not have MECHANIC_DISCOVERY (28) value in Mechanic field in spell.dbc but listed for spellId %u (and maybe more) in `skill_discovery_template` table", reqSkillOrSpell, spellId);
                    reportedReqSpells.insert(reqSkillOrSpell);
                }
                continue;
            }

            SkillDiscoveryStore[reqSkillOrSpell].push_back(SkillDiscoveryEntry(spellId, chance));
        }
        else if (reqSkillOrSpell == 0)                      // skill case (按所属专业技能任意制作触发领悟)
        {
            SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(spellId);

            if (bounds.first == bounds.second)
            {
                sLog.outErrorDb("Spell (ID: %u) not listed in `SkillLineAbility.dbc` but listed with `reqSpell`=0 in `skill_discovery_template` table", spellId);
                continue;
            }

            for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
                SkillDiscoveryStore[-int32(_spell_idx->second->skillId)].push_back(SkillDiscoveryEntry(spellId, chance));
        }
        else
        {
            sLog.outErrorDb("Spell (ID: %u) have negative value in `reqSpell` field in `skill_discovery_template` table", spellId);
            continue;
        }

        ++count;
    }
    while (queryResult->NextRow());

    if (!ssNonDiscoverableEntries.str().empty())
        sLog.outErrorDb("Some items can't be successfully discovered: have in chance field value < 0.000001 in `skill_discovery_template` DB table . List:\n%s", ssNonDiscoverableEntries.str().c_str());
}

// 判定玩家施放 spellId（或所属专业 skillId）时是否领悟到新配方（未学会且随机命中）
uint32 GetSkillDiscoverySpell(uint32 skillId, uint32 spellId, Player* player)
{
    // check spell case (优先检查特定法术关联的领悟列表)
    SkillDiscoveryMap::const_iterator tab = SkillDiscoveryStore.find(spellId);

    if (tab != SkillDiscoveryStore.end())
    {
        for (auto item_iter : tab->second)
        {
            if (roll_chance_f(item_iter.chance * sWorld.getConfig(CONFIG_FLOAT_RATE_SKILL_DISCOVERY)) &&
                    !player->HasSpell(item_iter.spellId))
                return item_iter.spellId;
        }

        return 0;
    }

    if (!skillId)
        return 0;

    // check skill line case (再检查按专业技能 ID 注册的通用领悟列表)
    tab = SkillDiscoveryStore.find(-(int32)skillId);
    if (tab != SkillDiscoveryStore.end())
    {
        for (auto item_iter : tab->second)
        {
            if (roll_chance_f(item_iter.chance * sWorld.getConfig(CONFIG_FLOAT_RATE_SKILL_DISCOVERY)) &&
                    !player->HasSpell(item_iter.spellId))
                return item_iter.spellId;
        }

        return 0;
    }

    return 0;
}
