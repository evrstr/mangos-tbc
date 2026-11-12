/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "Common.h"
#include "Log/Log.h"

/// 启动 POSIX 守护进程（fork 到后台并等待就绪信号或超时）
void startDaemon(uint32_t timeout = 10);
/// 读取 PidFile 并向运行中的守护进程发送 SIGINT 停止信号
void stopDaemon();
/// 子进程初始化完成后通知父进程安全退出（SIGUSR1）
void detachDaemon();
