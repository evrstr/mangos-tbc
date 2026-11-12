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
#ifndef DEF_GAMEOBJECT_AI_H
#define DEF_GAMEOBJECT_AI_H

#include "Platform/Define.h"
#include "AI/BaseAI/AIDefines.h"

class GameObject;
class Unit;
struct SpellEntry;

// 游戏对象 (GameObject) AI 基类：处理定时更新、世界事件、拾取状态变更、刷出/消失及交互回调
class GameObjectAI
{
    public:
        explicit GameObjectAI(GameObject* go);
        virtual ~GameObjectAI();

        /**
        * Called at World update tick, by default every 100ms
        * This setting is dependend on CONFIG_UINT32_INTERVAL_MAPUPDATE
        * Note: Use this function to handle Timers
        * @param uiDiff Passed time since last call
        * 在世界更新刻度调用（默认每 100ms，取决于 CONFIG_UINT32_INTERVAL_MAPUPDATE），用于驱动计时器逻辑
        */
        virtual void UpdateAI(const uint32 /*diff*/) {}

        /**
        * Called when a Game Event starts or ends
        * @param eventId to specify id of event from database
        * @param activate to specify if it started or stopped
        * @param resume to specify whether it launched normally or was resumed after a restart
        * 当游戏活动 (GameEvent) 开启或结束时调用
        */
        virtual void OnEventHappened(uint16 /*eventId*/, bool /*activate*/, bool /*resume*/) {}

        /**
        * Called when the GO has its state changed in GameObject::SetLootState (whatever the reason is)
        * LootState to which the GO is changed to is accessible in the GameObjectAI through GetLootState()
        * User can be nullptr
        * 当游戏对象的拾取/激活状态在 GameObject::SetLootState 中改变时调用
        */
        virtual void OnLootStateChange(Unit* /*user*/) {}

        /*
        * Called when a GO appears in the world to normal observers
        * 当游戏对象在世界中刷新出现时调用
        */
        virtual void JustSpawned() {}

        /*
        * Called when a GO disappears from the world to normal observers
        * 当游戏对象从世界中消失时调用
        */
        virtual void JustDespawned() {}

        /*
        * Enables generic receiving of events
        * 接收通用 AI 事件
        */
        virtual void ReceiveAIEvent(AIEventType /*eventType*/, uint32 /*miscValue*/ = 0) {}
        
        /*
         * Enables handling of GO Use by all units
         * 当任意单位使用/交互该游戏对象时调用
         */
        virtual void OnUse(Unit* /*user*/, SpellEntry const* /*spellInfo*/);

        /*
         * Notifies AI on object heartbeat
         * 在对象心跳周期触发时通知 AI
         */
        virtual void OnHeartbeat() {}

    protected:
        GameObject* m_go;                                               // 所属游戏对象指针
};

#endif

