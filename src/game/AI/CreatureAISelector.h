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

#ifndef MANGOS_CREATUREAISELECTOR_H
#define MANGOS_CREATUREAISELECTOR_H

#include <string>

class UnitAI;
class Unit;
class Creature;
class MovementGenerator;

// 生物 AI 与默认移动生成器工厂选择器
namespace FactorySelector
{
    UnitAI* selectAI(Creature*);                                        // 为指定生物选择最合适的 AI 实例
    UnitAI* GetSpecificAI(Unit* unit, std::string const& ainame);       // 根据指定 AI 名称为单位创建特定 AI
    MovementGenerator* selectMovementGenerator(Creature*);              // 为指定生物选择默认移动生成器
}
#endif
