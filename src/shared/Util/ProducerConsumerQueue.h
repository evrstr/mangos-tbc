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

#ifndef _PCQ_H
#define _PCQ_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <atomic>
#include <type_traits>

/// 线程安全的生产者-消费者阻塞/非阻塞队列模板类（支持关闭唤醒与指针元素自动释放）
template <typename T>
class ProducerConsumerQueue
{
    public:
        ProducerConsumerQueue() : m_shutdown(false) { }
        ProducerConsumerQueue(const ProducerConsumerQueue<T>&) = delete;

        /// 生产者入队并唤醒一个等待中的消费者线程
        void Push(const T&& value)
        {
            std::lock_guard<std::mutex> lock(m_queueLock);
            m_queue.push(std::move(value));

            m_condition.notify_one();
        }

        /// 判断队列是否为空（线程安全）
        bool Empty()
        {
            std::lock_guard<std::mutex> lock(m_queueLock);

            return m_queue.empty();
        }

        /// 非阻塞出队：队列为空或已关闭时返回 false
        bool Pop(T& value)
        {
            std::lock_guard<std::mutex> lock(m_queueLock);

            if (m_queue.empty() || m_shutdown)
                return false;

            value = m_queue.front();

            m_queue.pop();

            return true;
        }

        /// 阻塞等待直至有元素可出队或队列被取消关闭
        void WaitAndPop(T& value)
        {
            std::unique_lock<std::mutex> lock(m_queueLock);

            // we could be using .wait(lock, predicate) overload here but it is broken
            // https://connect.microsoft.com/VisualStudio/feedback/details/1098841
            // | 因旧版 MSVC 上 wait(lock, predicate) 存在缺陷，改用显式 while 循环等待条件变量
            while (m_queue.empty() && !m_shutdown)
                m_condition.wait(lock);

            if (m_queue.empty() || m_shutdown)
                return;

            value = m_queue.front();

            m_queue.pop();
        }

        /// 清空队列（若元素为指针类型则 delete 释放）、标记关闭并唤醒所有等待线程
        void Cancel()
        {
            std::unique_lock<std::mutex> lock(m_queueLock);

            while (!m_queue.empty())
            {
                T& value = m_queue.front();

                DeleteQueuedObject(value);

                m_queue.pop();
            }

            m_shutdown = true;

            m_condition.notify_all();
        }

    private:
        std::mutex m_queueLock;
        std::queue<T> m_queue;
        std::condition_variable m_condition;
        std::atomic<bool> m_shutdown;

        template<typename E = T>
        typename std::enable_if<std::is_pointer<E>::value>::type DeleteQueuedObject(E& obj) { delete obj; }

        template<typename E = T>
        typename std::enable_if<!std::is_pointer<E>::value>::type DeleteQueuedObject(E const& /*packet*/) { }
};

#endif