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

#ifndef MANGOS_MESSAGER_H
#define MANGOS_MESSAGER_H

#include <vector>
#include <mutex>
#include <functional>

/// 线程安全的消息投递器模板类：用于跨线程向目标对象（如 World、Map、WorldSession）异步派发回调任务
template <class T>
class Messager
{
    public:
        /// 投递一条作用于目标对象指针的回调消息（线程安全）
        void AddMessage(const std::function<void(T*)>& message)
        {
            std::lock_guard<std::mutex> guard(m_messageMutex);
            m_messageVector.push_back(message);
        }
        /// 交换出当前队列中的所有消息并在目标线程上下文中依次执行（避免持锁回调导致死锁）
        void Execute(T* object)
        {
            std::vector<std::function<void(T*)>> messageVectorCopy;
            {
                std::lock_guard<std::mutex> guard(m_messageMutex);
                std::swap(m_messageVector, messageVectorCopy);
            }
            for (auto& message : messageVectorCopy)
                message(object);

            messageVectorCopy.clear();
        }
    private:
        std::vector<std::function<void(T*)>> m_messageVector;   ///< 待执行的消息回调队列
        std::mutex m_messageMutex;                              ///< 保护消息队列的互斥锁
};

#endif