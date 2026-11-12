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

#include "Common.h"
#include "Tools/CharacterDatabaseCleaner.h"
#include "World/World.h"
#include "Database/DatabaseEnv.h"
#include "Server/DBCStores.h"
#include "Util/ProgressBar.h"
#include "Server/SQLStorages.h"

// 若配置启用且 saved_variables 存在待清理标志，则按标志位清理角色数据库并复位标志
void CharacterDatabaseCleaner::CleanDatabase()
{
    // config to disable (受配置开关控制是否执行清理)
    if (!sWorld.getConfig(CONFIG_BOOL_CLEAN_CHARACTER_DB))
        return;

    sLog.outString("Cleaning character database...");

    // check flags which clean ups are necessary (读取 saved_variables 表中的清理标志位)
    auto queryResult = CharacterDatabase.PQuery("SELECT cleaning_flags FROM saved_variables");
    if (!queryResult)
        return;
    uint32 flags = (*queryResult)[0].GetUInt32();

    // clean up (按标志位执行对应表的失效数据清理)
    if (flags & CLEANING_FLAG_SKILLS)
        CleanCharacterSkills();
    if (flags & CLEANING_FLAG_SPELLS)
        CleanCharacterSpell();
    CharacterDatabase.Execute("UPDATE saved_variables SET cleaning_flags = 0");
}

// 扫描表中不重复的字段值，收集未通过校验函数的 ID 并批量删除
void CharacterDatabaseCleaner::CheckUnique(const char* column, const char* table, bool (*check)(uint32))
{
    auto queryResult = CharacterDatabase.PQuery("SELECT DISTINCT %s FROM %s", column, table);
    if (!queryResult)
    {
        sLog.outString("Table %s is empty.", table);
        return;
    }

    bool found = false;
    std::ostringstream ss;
    BarGoLink bar(queryResult->GetRowCount());
    do
    {
        bar.step();

        Field* fields = queryResult->Fetch();

        uint32 id = fields[0].GetUInt32();

        if (!check(id))
        {
            if (!found)
            {
                ss << "DELETE FROM " << table << " WHERE " << column << " IN (";
                found = true;
            }
            else
                ss << ",";
            ss << id;
        }
    }
    while (queryResult->NextRow());

    if (found)
    {
        ss << ")";
        CharacterDatabase.Execute(ss.str().c_str());
    }
}

// 校验技能 ID 是否有效
bool CharacterDatabaseCleaner::SkillCheck(uint32 skill)
{
    return sSkillLineStore.LookupEntry(skill) != nullptr;
}

// 清理角色技能表中的失效技能
void CharacterDatabaseCleaner::CleanCharacterSkills()
{
    CheckUnique("skill", "character_skills", &SkillCheck);
}

// 校验法术 ID 是否有效
bool CharacterDatabaseCleaner::SpellCheck(uint32 spell_id)
{
    return !!sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
}

// 清理角色法术表中的失效法术
void CharacterDatabaseCleaner::CleanCharacterSpell()
{
    CheckUnique("spell", "character_spell", &SpellCheck);
}
