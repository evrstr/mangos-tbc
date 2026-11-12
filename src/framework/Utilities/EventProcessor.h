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

#ifndef __EVENTPROCESSOR_H
#define __EVENTPROCESSOR_H

#include "Platform/Define.h"

#include <map>

// Note. All times are in milliseconds here. | 注意：此处所有时间单位均为毫秒。

/// 可调度事件基类
class BasicEvent
{
    public:

        BasicEvent()
            : to_Abort(false)
        {
        }

        virtual ~BasicEvent()                               // override destructor to perform some actions on event removal | 重写析构函数以便在事件移除时执行清理动作
        {
        };

        // this method executes when the event is triggered
        // return false if event does not want to be deleted
        // e_time is execution time, p_time is update interval
        // 当事件触发时执行；若返回 false 表示事件不希望立即被销毁，e_time 为当前执行时间，p_time 为更新间隔
        virtual bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) { return true; }

        virtual bool IsDeletable() const { return true; }   // this event can be safely deleted | 判断该事件是否可安全销毁

        virtual void Abort(uint64 /*e_time*/) {}            // this method executes when the event is aborted | 事件被中止时执行此方法

        bool to_Abort;                                      // set by externals when the event is aborted, aborted events don't execute | 外部置位的中止标志，已中止的事件不会执行
        // and get Abort call when deleted | 并在销毁前调用 Abort

        // these can be used for time offset control | 用于时间偏移控制的时间戳字段
        uint64 m_addTime;                                   // time when the event was added to queue, filled by event handler | 事件加入队列时的时间戳，由事件处理器填充
        uint64 m_execTime;                                  // planned time of next execution, filled by event handler | 计划下次执行的时间戳，由事件处理器填充
};

typedef std::multimap<uint64, BasicEvent*> EventList;

/// 定时事件调度处理器
class EventProcessor
{
    public:

        EventProcessor();
        ~EventProcessor();

        /// 推进内部时钟并执行所有到期事件
        void Update(uint32 p_time);
        /// 中止并清理全部事件（force 为 true 时忽略 IsDeletable 强制释放）
        void KillAllEvents(bool force);
        /// 立即移除并销毁指定事件
        void KillEvent(BasicEvent* Event);
        /// 向队列添加事件并设定目标执行时间 e_time
        void AddEvent(BasicEvent* Event, uint64 e_time, bool set_addtime = true);
        /// 修改已入队事件的计划执行时间
        void ModifyEventTime(BasicEvent* event, uint64 msTime);
        /// 基于当前时间加上偏移量计算绝对执行时间戳
        uint64 CalculateTime(uint64 t_offset) const;
        EventList& GetEvents() { return m_events; }
        bool IsEmpty() const { return m_events.empty(); }

    protected:

        uint64 m_time;
        EventList m_events;
        bool m_aborting;
};

#endif
