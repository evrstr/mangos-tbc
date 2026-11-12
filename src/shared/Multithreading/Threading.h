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

#ifndef THREADING_H
#define THREADING_H

#include <thread>
#include <atomic>

namespace MaNGOS
{
    /// 可运行任务抽象基类：带原子引用计数，供 Thread 托管生命周期
    class Runnable
    {
        public:
            virtual ~Runnable() {}
            /// 子类实现的具体线程执行体
            virtual void run() = 0;

            /// 增加引用计数
            void incReference() { ++m_refs; }
            /// 减少引用计数，归零时自动释放自身
            void decReference()
            {
                if (!--m_refs)
                    delete this;
            }
        private:
            std::atomic_long m_refs { 0 };  ///< 原子引用计数器
    };

    /// 线程调度优先级枚举
    enum Priority
    {
        Priority_Idle,
        Priority_Lowest,
        Priority_Low,
        Priority_Normal,
        Priority_High,
        Priority_Highest,
        Priority_Realtime,
    };

    /// 跨平台线程封装类（基于 std::thread 与 Runnable 引用计数）
    class Thread
    {
        public:
            Thread();
            /// 构造并立即启动新线程执行 instance->run()
            explicit Thread(Runnable* instance);
            ~Thread();

            /// 阻塞等待线程结束（join）
            bool wait();
            /// 等待线程退出并重置线程 ID
            void destroy();

            /// 设置线程调度优先级（目前仅在原生 Windows 线程下生效）
            void setPriority(Priority priority);

            /// 当前线程休眠指定毫秒数
            static void Sleep(unsigned long msecs);
            /// 获取当前执行线程的 std::thread::id
            static std::thread::id currentId();

        private:
            Thread(const Thread&);
            Thread& operator=(const Thread&);

            /// 静态线程入口函数，转发调用 Runnable::run()
            static void ThreadTask(void* param);

            Runnable* const m_task;         ///< 绑定的任务对象指针
            std::thread::id m_iThreadId;    ///< 线程唯一标识
            std::thread m_ThreadImp;        ///< 底层 std::thread 对象
    };
}
#endif