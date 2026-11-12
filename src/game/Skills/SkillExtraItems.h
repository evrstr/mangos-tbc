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

#ifndef MANGOS_SKILL_EXTRA_ITEMS_H
#define MANGOS_SKILL_EXTRA_ITEMS_H

#include "Common.h"

// predef classes used in functions (前置声明)
class Player;
// returns true and sets the appropriate info if the player can create extra items with the given spellId
// 判断玩家施放指定制造法术时是否可触发专业专精额外产出，若可则返回 true 并输出概率与最大额外数量
bool canCreateExtraItems(Player* player, uint32 spellId, float& additionalChance, uint8& additionalMax);
// function to load the extra item creation info from DB
// 从数据库加载专业专精额外产出配置表
void LoadSkillExtraItemTable();
#endif
