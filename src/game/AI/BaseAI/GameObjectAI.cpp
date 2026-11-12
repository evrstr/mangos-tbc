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
#include "AI/BaseAI/GameObjectAI.h"
#include "Entities/GameObject.h"


// 构造游戏对象 AI 并保存所属 GameObject 指针
GameObjectAI::GameObjectAI(GameObject* go) : m_go(go)
{
}


// 虚析构函数
GameObjectAI::~GameObjectAI()
{
}

// 玩家或单位交互使用该游戏对象时的默认空实现
void GameObjectAI::OnUse(Unit*, SpellEntry const*)
{
}
