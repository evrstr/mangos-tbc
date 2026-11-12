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

#ifndef MANGOS_AI_DEFINES_H
#define MANGOS_AI_DEFINES_H

// AI 内部与脚本通用事件类型枚举
enum AIEventType
{
    // Usable with Event AI (可用于 EventAI 事件触发的事件类型)
    AI_EVENT_JUST_DIED          = 0,                        // Sender = Killed Npc, Invoker = Killer (刚刚死亡：发送者为死亡 NPC，触发者为击杀者)
    AI_EVENT_CRITICAL_HEALTH    = 1,                        // Sender = Hurt Npc, Invoker = DamageDealer - Expected to be sent by 10% health (生命危急：通常在 10% 血量时发送)
    AI_EVENT_LOST_HEALTH        = 2,                        // Sender = Hurt Npc, Invoker = DamageDealer - Expected to be sent by 50% health (生命过半：通常在 50% 血量时发送)
    AI_EVENT_LOST_SOME_HEALTH   = 3,                        // Sender = Hurt Npc, Invoker = DamageDealer - Expected to be sent by 90% health (受到轻伤：通常在 90% 血量时发送)
    AI_EVENT_GOT_FULL_HEALTH    = 4,                        // Sender = Healed Npc, Invoker = Healer (恢复满血：发送者为受治疗 NPC，触发者为治疗者)
    AI_EVENT_CUSTOM_EVENTAI_A   = 5,                        // Sender = Npc that throws custom event, Invoker = TARGET_T_ACTION_INVOKER (if exists) (EventAI 自定义事件 A)
    AI_EVENT_CUSTOM_EVENTAI_B   = 6,                        // Sender = Npc that throws custom event, Invoker = TARGET_T_ACTION_INVOKER (if exists) (EventAI 自定义事件 B)
    AI_EVENT_GOT_CCED           = 7,                        // Sender = CCed Npc, Invoker = Caster that CCed (受到群体控制：触发者为施法者)
    AI_EVENT_CUSTOM_EVENTAI_C   = 8,                        // Sender = Npc that throws custom event, Invoker = TARGET_T_ACTION_INVOKER (if exists) (EventAI 自定义事件 C)
    AI_EVENT_CUSTOM_EVENTAI_D   = 9,                        // Sender = Npc that throws custom event, Invoker = TARGET_T_ACTION_INVOKER (if exists) (EventAI 自定义事件 D)
    AI_EVENT_CUSTOM_EVENTAI_E   = 10,                       // Sender = Npc that throws custom event, Invoker = TARGET_T_ACTION_INVOKER (if exists) (EventAI 自定义事件 E)
    AI_EVENT_CUSTOM_EVENTAI_F   = 11,                       // Sender = Npc that throws custom event, Invoker = TARGET_T_ACTION_INVOKER (if exists) (EventAI 自定义事件 F)
    MAXIMAL_AI_EVENT_EVENTAI    = 12,

    // Internal Use (核心内部使用事件)
    AI_EVENT_CALL_ASSISTANCE    = 13,                       // Sender = Attacked Npc, Invoker = Enemy (呼叫周围友军支援事件)

    // Predefined for SD2 (ScriptDev2 预定义脚本事件)
    AI_EVENT_START_ESCORT       = 100,                      // Invoker = Escorting Player (开始护送任务事件)
    AI_EVENT_START_ESCORT_B     = 101,                      // Invoker = Escorting Player (开始护送任务事件 B)
    AI_EVENT_START_EVENT        = 102,                      // Invoker = EventStarter (开始脚本事件)
    AI_EVENT_START_EVENT_A      = 103,                      // Invoker = EventStarter (开始脚本事件 A)
    AI_EVENT_START_EVENT_B      = 104,                      // Invoker = EventStarter (开始脚本事件 B)

    // Some IDs for special cases in SD2 (ScriptDev2 特殊自定义事件 ID)
    AI_EVENT_CUSTOM_A           = 1000,
    AI_EVENT_CUSTOM_B           = 1001,
    AI_EVENT_CUSTOM_C           = 1002,
    AI_EVENT_CUSTOM_D           = 1003,
    AI_EVENT_CUSTOM_E           = 1004,
    AI_EVENT_CUSTOM_F           = 1005,
};

#endif