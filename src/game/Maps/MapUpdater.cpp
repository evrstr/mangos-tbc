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

#include "MapUpdater.h"
#include "MapWorkers.h"

// 构造函数：立即启动指定数量的地图更新工作线程
MapUpdater::MapUpdater(size_t num_threads) : _cancelationToken(false), pending_requests(0)
{
    for (size_t i = 0; i < num_threads; ++i)
        _workerThreads.push_back(std::thread(&MapUpdater::WorkerThread, this));
}

// 若尚未激活工作线程池，则创建指定数量的工作线程
void MapUpdater::activate(size_t num_threads)
{
    if (activated())
        return;

    for (size_t i = 0; i < num_threads; ++i)
        _workerThreads.push_back(std::thread(&MapUpdater::WorkerThread, this));
}

// 发出取消信号、唤醒队列阻塞线程并等待所有工作线程结束
void MapUpdater::deactivate()
{
    _cancelationToken = true;

    _queue.Cancel();

    for (auto& thread : _workerThreads)
        thread.join();
}

// 阻塞等待直至所有已调度的更新任务执行完毕
void MapUpdater::wait()
{
    std::unique_lock<std::mutex> lock(_lock);

    while (pending_requests > 0)
        _condition.wait(lock);
}

// 先等待当前任务完成，再关闭并回收线程池
void MapUpdater::join()
{
    wait();
    deactivate();
}

// 返回工作线程池是否已初始化并运行
bool MapUpdater::activated()
{
    return _workerThreads.size() > 0;
}

// 任务完成回调：递减未完成计数并通知等待主线程
void MapUpdater::update_finished()
{
    std::lock_guard<std::mutex> lock(_lock);

    --pending_requests;
    _condition.notify_all();
}

// 提交一个 Worker 任务到线程安全队列并增加待处理计数
void MapUpdater::schedule_update(Worker* worker)
{
    std::lock_guard<std::mutex> lock(_lock);

    ++pending_requests;
    _queue.Push(std::move(worker));
}

// 工作线程主循环：持续从队列取出 Worker 并执行，收到取消令牌时退出
void MapUpdater::WorkerThread()
{
    while (true)
    {
        Worker* request = nullptr;

        _queue.WaitAndPop(request);

        if (_cancelationToken)
        {
            delete request;
            return;
        }

        request->execute();

        delete request;
    }
}