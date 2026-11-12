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

#ifndef _PLAYER_DUMP_H
#define _PLAYER_DUMP_H

#include <set>

// 角色导出表分类枚举（决定导出时的 WHERE 条件及导入时的 GUID 重映射策略）
enum DumpTableType
{
    DTT_CHARACTER,      //    -> guid, name                 // characters (角色主表，导出新 GUID 与角色名)

    DTT_CHAR_TABLE,     //                                  // character_action, (普通角色附属表)
    // character_aura
    // character_homebind, character_queststatus,
    // character_reputation, character_spell,
    // character_spell_cooldown

    DTT_CHAR_NAME_TABLE,// <- guid, name                    // character_declinedname (俄语等变格名表，改名时忽略)

    DTT_INVENTORY,      //    -> item guids collection      // character_inventory (背包映射表，收集物品 GUID)

    DTT_MAIL,           //    -> mail ids collection        // mail (邮件表，收集邮件 ID 与邮件正文 ID)
    //    -> item_text

    DTT_MAIL_ITEM,      // <- mail ids                      // mail_items (邮件附件表，收集附件物品 GUID)
    //    -> item guids collection

    DTT_ITEM,           // <- item guids                    // item_instance (物品实例表，收集物品正文 ID)
    //    -> item_text

    DTT_ITEM_GIFT,      // <- item guids                    // character_gifts (礼物表)

    DTT_ITEM_LOOT,      // <- item guids                    // item_loot (物品拾取表)

    DTT_PET,            //    -> pet guids collection       // character_pet (宠物主表，收集宠物编号)
    DTT_PET_TABLE,      // <- pet guids                     // pet_aura, pet_spell, pet_spell_cooldown (宠物附属表)
    DTT_ITEM_TEXT,      // <- item_text                     // item_text (物品/邮件文本表)
    DTT_PET_DECL,       // <- pet guids                     // character_pet_declinedname (宠物变格名表)
};

// 角色导出/导入操作返回码
enum DumpReturn
{
    DUMP_SUCCESS,                                           // 成功
    DUMP_FILE_OPEN_ERROR,                                   // 文件打开失败
    DUMP_TOO_MANY_CHARS,                                    // 目标账号角色数量已达上限
    DUMP_UNEXPECTED_END,                                    // 文件意外结束
    DUMP_FILE_BROKEN,                                       // 转储文件格式损坏或 SQL 执行失败
};


// 角色数据导出器：将指定角色及相关物品、邮件、宠物记录导出为 SQL 转储文本或文件
class PlayerDumpWriter
{
    public:
        PlayerDumpWriter() {}

        std::string GetDump(uint32 guid);                   // 生成完整角色 SQL 转储字符串
        DumpReturn WriteDump(const std::string& file, uint32 guid); // 将角色转储写入磁盘文件
    private:
        typedef std::set<uint32> GUIDs;

        void DumpTableContent(std::string& dump, uint32 guid, char const* tableFrom, char const* tableTo, DumpTableType type); // 导出单张表并收集关联子 GUID
        static std::string GenerateWhereStr(char const* field, GUIDs const& guids, GUIDs::const_iterator& itr);
        static std::string GenerateWhereStr(char const* field, uint32 guid);

        GUIDs pets;                                         // 关联宠物编号集合
        GUIDs mails;                                        // 关联邮件 ID 集合
        GUIDs items;                                        // 关联物品 GUID 集合
        GUIDs texts;                                        // 关联文本 ID 集合
};

// 角色数据导入器：解析 SQL 转储文件、重映射所有 GUID/ID 并在事务中导入指定账号
class PlayerDumpReader
{
    public:
        PlayerDumpReader() {}

        static DumpReturn LoadDump(const std::string& file, uint32 account, std::string name, uint32 guid);
};

#endif
