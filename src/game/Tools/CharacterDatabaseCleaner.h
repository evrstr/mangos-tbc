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

#ifndef CHARACTERDATABASECLEANER_H
#define CHARACTERDATABASECLEANER_H

// 角色数据库启动清理工具：根据 saved_variables.cleaning_flags 清理失效的技能与法术记录
namespace CharacterDatabaseCleaner
{
    // 清理任务标志掩码
    enum CleaningFlags
    {
        // reserved for next version          0x1 (保留给后续版本使用)
        CLEANING_FLAG_SKILLS                = 0x2,          // 清理无效技能记录
        CLEANING_FLAG_SPELLS                = 0x4,          // 清理无效法术记录
        // reserved for next version          0x8 (保留给后续版本使用)
    };


    void CleanDatabase();                                   // 执行数据库清理总入口

    // 去重查询指定表的唯一 ID，并删除校验回调返回 false 的无效记录
    void CheckUnique(const char* column, const char* table, bool (*check)(uint32));

    bool SkillCheck(uint32 skill);                          // 校验技能 ID 是否存在于 SkillLine.dbc
    bool SpellCheck(uint32 spell_id);                       // 校验法术 ID 是否存在于 Spell 模板表

    void CleanCharacterSkills();                            // 清理 character_skills 表中失效技能
    void CleanCharacterSpell();                             // 清理 character_spell 表中失效法术
}

#endif
