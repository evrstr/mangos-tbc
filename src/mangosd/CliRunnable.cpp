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

/// \addtogroup mangosd
/// @{
/// \file

#include "Common.h"
#include "Tools/Language.h"
#include "Log/Log.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Server/WorldSession.h"
#include "Config/Config.h"
#include "Util/Util.h"
#include "Accounts/AccountMgr.h"
#include "CliRunnable.h"
#include "Maps/MapManager.h"
#include "Entities/Player.h"
#include "Chat/Chat.h"

/// 将 UTF-8 字符串输出到控制台（Windows 下先转换为 OEM 编码）
void utf8print(const char* str)
{
#if PLATFORM == PLATFORM_WINDOWS
    std::wstring wtemp_buf;
    std::string temp_buf(str);

    if (!Utf8toWStr(temp_buf, wtemp_buf, 6000))
        return;

    // Guarantee null termination | 确保字符串以空字符结尾
    if (!temp_buf.empty())
    {
        wtemp_buf.push_back('\0');
        temp_buf.resize(wtemp_buf.size());
        CharToOemBuffW(&wtemp_buf[0], &temp_buf[0], wtemp_buf.size());
    }

    printf("%s", temp_buf.c_str());
#else
    printf("%s", str);
#endif
}

/// 命令执行完成回调：重新打印命令行提示符并刷新标准输出
void commandFinished(bool)
{
    printf("mangos>");
    fflush(stdout);
}

/// @}

#ifdef __unix__
// Non-blocking keypress detector, when return pressed, return 1, else always return 0 | 非阻塞按键检测：按下回车返回 1，否则返回 0
int kb_hit_return()
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}
#endif

/// %Thread start | 线程启动入口
void CliRunnable::run()
{
    ///- Init new SQL thread for the world database (one connection call enough) | 为世界数据库初始化新的 SQL 线程（单次连接调用即可）
    WorldDatabase.ThreadStart();                            // let thread do safe mySQL requests | 使当前线程能够安全执行 MySQL 请求

    char commandbuf[256];

    ///- Display the list of available CLI functions then beep | 输出空行并提示可用 CLI 功能，随后发出提示音
    sLog.outString();

    if (sConfig.GetBoolDefault("BeepAtStart", true))
        printf("\a");                                       // \a = Alert | \a 为响铃提示符

    // print this here the first time | 首次启动时先打印提示符
    // later it will be printed after command queue updates | 后续提示符将在命令队列执行完毕后由回调打印
    printf("mangos>");

#ifdef __unix__
    //Set stdin IO to nonblocking - prevent Server from hanging in shutdown process till enter is pressed | 将标准输入设为非阻塞 I/O，防止服务器在关闭流程中因等待回车而挂起
    int fd = fileno(stdin);
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
#endif

    ///- As long as the World is running (no World::m_stopEvent), get the command line and handle it | 只要世界线程仍在运行（未触发停止事件），就持续读取并处理命令行输入
    while (!World::IsStopped())
    {
        fflush(stdout);
#ifdef __unix__
        while (!kb_hit_return() && !World::IsStopped())
        {
            // With this, we limit CLI to 10commands/second | 通过休眠将 CLI 轮询频率限制为每秒约 10 次命令检测
            std::this_thread::sleep_for(std::chrono::nanoseconds(100000));
            // Check for world stoppage after each sleep interval | 每次休眠结束后检查世界是否已停止
            if (World::IsStopped())
                break;
        }
#endif
        char* command_str = fgets(commandbuf, sizeof(commandbuf), stdin);
        if (command_str != nullptr)
        {
            for (int x = 0; command_str[x]; ++x)
                if (command_str[x] == '\r' || command_str[x] == '\n')
                {
                    command_str[x] = 0;
                    break;
                }


            if (!*command_str)
            {
                printf("mangos>");
                continue;
            }

            std::string command;
            if (!consoleToUtf8(command_str, command))       // convert from console encoding to utf8 | 将控制台编码转换为 UTF-8
            {
                printf("mangos>");
                continue;
            }

            sWorld.QueueCliCommand(new CliCommandHolder(0, SEC_CONSOLE, command.c_str(), &utf8print, &commandFinished));
        }
        else if (feof(stdin))
        {
            World::StopNow(SHUTDOWN_EXIT_CODE);
        }
    }

    ///- End the database thread | 结束数据库线程
    WorldDatabase.ThreadEnd();                              // free mySQL thread resources | 释放 MySQL 线程资源
}
