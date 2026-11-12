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

#ifndef _ITEM_ENCHANTMENT_MGR_H
#define _ITEM_ENCHANTMENT_MGR_H

#include "Common.h"

// 从数据库加载物品随机附魔模板表 (item_enchantment_template)
void LoadRandomEnchantmentsTable();
// 根据随机附魔组 ID 按概率抽取一个附魔条目 ID
uint32 GetItemEnchantMod(uint32 entry);
// 根据物品等级、装备部位和品质计算随机后缀属性缩放系数
uint32 GenerateEnchSuffixFactor(uint32 item_id);
#endif
