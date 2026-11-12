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

#ifndef _SCRIPTMGRDEF_H
#define _SCRIPTMGRDEF_H

#include <string>
#include <map>

/// 数据库脚本表类型枚举（决定脚本的触发来源与对应数据库表）
enum ScriptMapType
{
    SCRIPT_TYPE_QUEST_END           = 0,                    // 任务完成脚本 (dbscripts_on_quest_end)
    SCRIPT_TYPE_QUEST_START         = 1,                    // 任务接取脚本 (dbscripts_on_quest_start)
    SCRIPT_TYPE_SPELL               = 2,                    // 法术触发脚本 (dbscripts_on_spell)
    SCRIPT_TYPE_GAMEOBJECT          = 3,                    // 游戏对象交互脚本 (dbscripts_on_go_use)
    SCRIPT_TYPE_GAMEOBJECT_TEMPLATE = 4,                    // 游戏对象模板脚本 (dbscripts_on_go_template_use)
    SCRIPT_TYPE_EVENT               = 5,                    // 事件脚本 (dbscripts_on_event)
    SCRIPT_TYPE_GOSSIP              = 6,                    // 对话菜单脚本 (dbscripts_on_gossip)
    SCRIPT_TYPE_CREATURE_DEATH      = 7,                    // 生物死亡脚本 (dbscripts_on_creature_death)
    SCRIPT_TYPE_CREATURE_MOVEMENT   = 8,                    // 生物路径移动脚本 (dbscripts_on_creature_movement)
    SCRIPT_TYPE_RELAY               = 9,                    // 中继链式脚本 (dbscripts_on_relay)
    SCRIPT_TYPE_INTERNAL            = 10, // must be last   // 内部临时脚本（必须保持在枚举最后）
    SCRIPT_TYPE_MAX
};

/// 字符串标识定义（将整数 ID 与可读字符串名称双向映射，用于脚本引用生物/对象）
struct StringId
{
    int32 Id;
    std::string Name;
};

typedef std::map<int32, StringId> StringIdMap;
typedef std::map<std::string, StringId> StringIdMapByString;

#endif