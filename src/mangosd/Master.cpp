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

/** \file
    \ingroup mangosd
*/

#include "Common.h"
#include "Master.h"
#include "Server/WorldSocket.h"
#include "WorldRunnable.h"
#include "World/World.h"
#include "Log/Log.h"
#include "Util/Timer.h"
#include "SystemConfig.h"
#include "CliRunnable.h"
#include "RASocket.h"
#include "Util/Util.h"
#include "revision_sql.h"
#include "MaNGOSsoap.h"
#include "Mails/MassMailMgr.h"
#include "Server/DBCStores.h"

#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "Network/AsyncListener.hpp"
#include "Network/AsyncSocket.hpp"

#include <boost/thread.hpp>

#include <memory>

#ifdef _WIN32
#include "Platform/ServiceWin32.h"
extern int m_ServiceStatus;
#else
#include "Platform/PosixDaemon.h"
#endif

INSTANTIATE_SINGLETON_1(Master);

volatile bool Master::m_canBeKilled = false;

/// 世界线程卡死检测线程类：定期检查主循环计数是否推进，超时未响应时触发崩溃转储
class FreezeDetectorRunnable : public MaNGOS::Runnable
{
    public:
        FreezeDetectorRunnable() { _delaytime = 0; }
        uint32 m_loops, m_lastchange;
        uint32 w_loops, w_lastchange;
        uint32 _delaytime;
        /// 设置判定为卡死的最大超时时间（毫秒）
        void SetDelayTime(uint32 t) { _delaytime = t; }
        /// 检测线程主循环
        void run(void)
        {
            if (!_delaytime)
                return;
            sLog.outString("Starting up anti-freeze thread (%u seconds max stuck time)...", _delaytime / 1000);
            m_loops = 0;
            w_loops = 0;
            m_lastchange = 0;
            w_lastchange = 0;
            while (!World::IsStopped())
            {
                MaNGOS::Thread::Sleep(1000);

                uint32 curtime = WorldTimer::getMSTime();

                // normal work | 世界主循环正常工作（计数有递增）
                if (w_loops != World::m_worldLoopCounter)
                {
                    w_lastchange = curtime;
                    w_loops = World::m_worldLoopCounter;
                }
                // possible freeze | 检测到世界主循环可能已卡死（超过最大停滞时间）
                else if (WorldTimer::getMSTimeDiff(w_lastchange, curtime) > _delaytime)
                {
                    sLog.outError("World Thread hangs, kicking out server!");
                    *((uint32 volatile*)nullptr) = 0;          // bang crash | 故意触发空指针崩溃以强制终止服务端并生成转储
                }
            }
            sLog.outString("Anti-freeze thread exiting without problems.");
        }
};

/// Main function | 服务端主控运行流程（初始化监听、数据库、工作线程及停机清理）
int Master::Run()
{
    ///- Reserve the listening ports first, so that an instance which cannot open them stops before touching the PID file and the databases | 优先预占监听端口，若端口绑定失败则在创建 PID 文件和连接数据库前提前退出
    std::unique_ptr<MaNGOS::AsyncListener<WorldSocket>> listener = MaNGOS::AsyncListener<WorldSocket>::Create(m_context,
            sConfig.GetStringDefault("BindIP", "0.0.0.0"), sConfig.GetIntDefault("WorldServerPort", DEFAULT_WORLDSERVER_PORT));
    if (!listener)
    {
        Log::WaitBeforeContinueIfNeed();
        return 1;
    }

    std::unique_ptr<MaNGOS::AsyncListener<RASocket>> raListener;
    if (sConfig.GetBoolDefault("Ra.Enable", false))
    {
        raListener = MaNGOS::AsyncListener<RASocket>::Create(m_raContext, sConfig.GetStringDefault("Ra.IP", "0.0.0.0"), sConfig.GetIntDefault("Ra.Port", 3443));
        if (!raListener)
        {
            Log::WaitBeforeContinueIfNeed();
            return 1;
        }
    }

    std::unique_ptr<SOAPThread> soapThread;
    if (sConfig.GetBoolDefault("SOAP.Enabled", false))
    {
        soapThread = SOAPThread::Create(sConfig.GetStringDefault("SOAP.IP", "127.0.0.1"), sConfig.GetIntDefault("SOAP.Port", 7878));
        if (!soapThread)
        {
            Log::WaitBeforeContinueIfNeed();
            return 1;
        }
    }

    /// worldd PID file creation | 创建 worldd 进程 PID 文件
    std::string pidfile = sConfig.GetStringDefault("PidFile");
    if (!pidfile.empty())
    {
        uint32 pid = CreatePIDFile(pidfile);
        if (!pid)
        {
            sLog.outError("Cannot create PID file %s.\n", pidfile.c_str());
            Log::WaitBeforeContinueIfNeed();
            return 1;
        }

        sLog.outString("Daemon PID: %u\n", pid);
    }

    ///- Start the databases | 启动并初始化所有数据库连接
    if (!_StartDB())
    {
        Log::WaitBeforeContinueIfNeed();
        return 1;
    }

    ///- Initialize the World | 初始化世界数据与核心管理器
    sWorld.SetInitialWorldSettings();

#ifndef _WIN32
    detachDaemon();
#endif
    // server loaded successfully => enable async DB requests | 服务端加载成功后开启异步数据库事务请求
    // this is done to forbid any async transactions during server startup! | 这样做是为了禁止在服务器启动阶段执行任何异步数据库事务
    CharacterDatabase.AllowAsyncTransactions();
    WorldDatabase.AllowAsyncTransactions();
    LoginDatabase.AllowAsyncTransactions();
    LogsDatabase.AllowAsyncTransactions();

    ///- Catch termination signals | 注册捕获系统终止信号
    _HookSignals();

    ///- Launch WorldRunnable thread | 启动世界主循环线程（最高优先级）
    MaNGOS::Thread world_thread(new WorldRunnable);
    world_thread.setPriority(MaNGOS::Priority_Highest);

    // set realmbuilds depend on mangosd expected builds, and set server online | 根据服务端支持的客户端版本号更新 realmlist 的 realmbuilds 字段，并将分区设为在线状态
    {
        std::string builds = AcceptableClientBuildsListStr();
        LoginDatabase.escape_string(builds);
        LoginDatabase.DirectPExecute("UPDATE realmlist SET realmflags = realmflags & ~(%u), population = 0, realmbuilds = '%s'  WHERE id = '%u'", REALM_FLAG_OFFLINE, builds.c_str(), realmID);
    }

    sWorld.StartLFGQueueThread();
    sWorld.StartBGQueueThread();

    MaNGOS::Thread* cliThread = nullptr;

#ifdef _WIN32
    if (sConfig.GetBoolDefault("Console.Enable", true) && (m_ServiceStatus == -1)/* need disable console in service mode | Windows 服务模式下必须禁用控制台线程*/)
#else
    if (sConfig.GetBoolDefault("Console.Enable", true))
#endif
    {
        ///- Launch CliRunnable thread | 启动命令行交互线程
        cliThread = new MaNGOS::Thread(new CliRunnable);
    }

    ///- Handle affinity for multiple processors and process priority on Windows | 在 Windows 平台处理多核 CPU 亲和性掩码与进程优先级
#ifdef _WIN32
    {
        HANDLE hProcess = GetCurrentProcess();

        uint32 Aff = sConfig.GetIntDefault("UseProcessors", 0);
        if (Aff > 0)
        {
            ULONG_PTR appAff;
            ULONG_PTR sysAff;

            if (GetProcessAffinityMask(hProcess, &appAff, &sysAff))
            {
                ULONG_PTR curAff = Aff & appAff;            // remove non accessible processors | 过滤掉当前进程不可访问的处理器核心位

                if (!curAff)
                {
                    sLog.outError("Processors marked in UseProcessors bitmask (hex) %x not accessible for mangosd. Accessible processors bitmask (hex): %x", Aff, appAff);
                }
                else
                {
                    if (SetProcessAffinityMask(hProcess, curAff))
                        sLog.outString("Using processors (bitmask, hex): %x", curAff);
                    else
                        sLog.outError("Can't set used processors (hex): %x", curAff);
                }
            }
            sLog.outString();
        }

        bool Prio = sConfig.GetBoolDefault("ProcessPriority", false);

//        if(Prio && (m_ServiceStatus == -1)/* need set to default process priority class in service mode*/)
        if (Prio)
        {
            if (SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS))
                sLog.outString("mangosd process priority class set to HIGH");
            else
                sLog.outError("Can't set mangosd process priority class.");
            sLog.outString();
        }
    }
#endif

    ///- Start up freeze catcher thread | 启动世界线程卡死检测线程
    MaNGOS::Thread* freeze_thread = nullptr;
    if (uint32 freeze_delay = sConfig.GetIntDefault("MaxCoreStuckTime", 0))
    {
        FreezeDetectorRunnable* fdr = new FreezeDetectorRunnable();
        fdr->SetDelayTime(freeze_delay * 1000);
        freeze_thread = new MaNGOS::Thread(fdr);
        freeze_thread->setPriority(MaNGOS::Priority_Highest);
    }

    {
        int32 networkThreadCount = sConfig.GetIntDefault("Network.Threads", 1);
        if (networkThreadCount <= 0)
        {
            sLog.outError("Invalid network thread workers setting in mangosd.conf. (%d) should be > 0", networkThreadCount);
            networkThreadCount = 1;
        }
        WorldSocket::LoadPacketCooldowns();
        listener->Start();

        std::vector<std::thread> threads;
        for (int32 i = 0; i < networkThreadCount; ++i)
            threads.emplace_back([&]() { m_context.run(); });

        std::thread m_raThread;
        if (raListener)
        {
            raListener->Start();
            m_raThread = std::thread([this]() { m_raContext.run(); });
        }

        if (soapThread)
            soapThread->Start();

        // wait for shut down and then let things go out of scope to close them down | 等待服务端停止信号，随后让网络监听与服务对象离开作用域以自动关闭
        while (!World::IsStopped())
            std::this_thread::sleep_for(std::chrono::seconds(1));

        world_thread.wait();

        m_context.stop();

        if (m_raThread.joinable())
        {
            m_raContext.stop();
            m_raThread.join();
        }

        for (int32 i = 0; i < networkThreadCount; ++i)
            threads[i].join();

        soapThread.reset();
        raListener.reset();
        listener.reset();
    }

    ///- Stop freeze protection before shutdown tasks | 在执行停机清理任务前先停止防卡死检测线程
    if (freeze_thread)
    {
        freeze_thread->destroy();
        delete freeze_thread;
    }

    ///- Set server offline in realmlist | 在 realmlist 表中将服务器标记为离线
    LoginDatabase.DirectPExecute("UPDATE realmlist SET realmflags = realmflags | %u WHERE id = '%u'", REALM_FLAG_OFFLINE, realmID);

    ///- Remove signal handling before leaving | 退出前移除系统信号处理钩子
    _UnhookSignals();

    // when the main thread closes the singletons get unloaded | 主线程退出时各个单例将被卸载
    // since worldrunnable uses them, it will crash if unloaded after master | 由于 WorldRunnable 依赖这些单例，必须在 Master 销毁前等待其完全退出，否则会崩溃
    world_thread.wait();

    ///- Clean account database before leaving | 退出前清理账号和角色的在线状态
    clearOnlineAccounts();

    // send all still queued mass mails (before DB connections shutdown) | 在关闭数据库连接前发送所有仍在队列中的群发邮件
    sMassMailMgr.Update(true);

#ifdef ENABLE_PLAYERBOTS
    // kick and save all players | 踢出并保存所有玩家（包含机器人）数据
    sWorld.KickAll(true);
#endif

    ///- Wait for DB delay threads to end | 等待所有数据库异步延迟写入线程结束
    CharacterDatabase.HaltDelayThread();
    WorldDatabase.HaltDelayThread();
    LoginDatabase.HaltDelayThread();
    LogsDatabase.HaltDelayThread();

    sLog.outString("Halting process...");

    if (cliThread)
    {
#ifdef _WIN32
        // send keyboard input to safely unblock the CLI thread that is waiting for an user input | 模拟发送回车按键事件，安全唤醒正在阻塞等待标准输入的 CLI 线程
        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        INPUT_RECORD ir[2] = { 0 };

        ir[0].EventType = KEY_EVENT;
        ir[0].Event.KeyEvent.bKeyDown = TRUE;
        ir[0].Event.KeyEvent.wRepeatCount = 1;
        ir[0].Event.KeyEvent.uChar.AsciiChar = VK_RETURN;
        ir[0].Event.KeyEvent.dwControlKeyState = 0;

        ir[1] = ir[0];
        ir[1].Event.KeyEvent.bKeyDown = FALSE;

        DWORD dwNumWrtn = -1;
        WriteConsoleInput(hStdIn, ir, 2, &dwNumWrtn);

        cliThread->wait();
#else

        cliThread->destroy();

#endif

        delete cliThread;
    }

    // mark this can be killable | 标记当前进程已完成清理、可安全终止
    m_canBeKilled = true;

    ///- Exit the process with specified return value | 以指定的世界退出码退出主进程
    return World::GetExitCode();
}

/// Initialize connection to the databases | 初始化并校验世界、角色、登录与日志四个数据库连接
bool Master::_StartDB()
{
    ///- Get world database info from configuration file | 从配置文件读取世界数据库连接信息
    std::string dbstring = sConfig.GetStringDefault("WorldDatabaseInfo");
    int nConnections = sConfig.GetIntDefault("WorldDatabaseConnections", 1);
    if (dbstring.empty())
    {
        sLog.outError("Database not specified in configuration file");
        return false;
    }
    sLog.outString("World Database total connections: %i", nConnections + 1);

    ///- Initialise the world database | 初始化世界数据库连接池
    if (!WorldDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        sLog.outError("Cannot connect to world database %s", dbstring.c_str());
        return false;
    }

    if (!WorldDatabase.CheckRequiredField("db_version", REVISION_DB_MANGOS))
    {
        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        return false;
    }

    dbstring = sConfig.GetStringDefault("CharacterDatabaseInfo");
    nConnections = sConfig.GetIntDefault("CharacterDatabaseConnections", 1);
    if (dbstring.empty())
    {
        sLog.outError("Character Database not specified in configuration file");

        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        return false;
    }
    sLog.outString("Character Database total connections: %i", nConnections + 1);

    ///- Initialise the Character database | 初始化角色数据库连接池
    if (!CharacterDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        sLog.outError("Cannot connect to Character database %s", dbstring.c_str());

        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        return false;
    }

    if (!CharacterDatabase.CheckRequiredField("character_db_version", REVISION_DB_CHARACTERS))
    {
        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        return false;
    }

    ///- Get login database info from configuration file | 从配置文件读取登录数据库连接信息
    dbstring = sConfig.GetStringDefault("LoginDatabaseInfo");
    nConnections = sConfig.GetIntDefault("LoginDatabaseConnections", 1);
    if (dbstring.empty())
    {
        sLog.outError("Login database not specified in configuration file");

        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        return false;
    }

    ///- Initialise the login database | 初始化登录数据库连接池
    sLog.outString("Login Database total connections: %i", nConnections + 1);
    if (!LoginDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        sLog.outError("Cannot connect to login database %s", dbstring.c_str());

        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        return false;
    }

    if (!LoginDatabase.CheckRequiredField("realmd_db_version", REVISION_DB_REALMD))
    {
        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        LoginDatabase.HaltDelayThread();
        return false;
    }

    ///- Get logs database info from configuration file | 从配置文件读取日志数据库连接信息
    dbstring = sConfig.GetStringDefault("LogsDatabaseInfo", "");
    nConnections = sConfig.GetIntDefault("LogsDatabaseConnections", 1);
    if (dbstring.empty())
    {
        sLog.outError("logs database not specified in configuration file");

        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        LoginDatabase.HaltDelayThread();
        return false;
    }

    ///- Initialise the logs database | 初始化日志数据库连接池
    sLog.outString("Logs Database total connections: %i", nConnections + 1);
    if (!LogsDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        sLog.outError("Cannot connect to logs database %s", dbstring.c_str());

        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        LoginDatabase.HaltDelayThread();
        return false;
    }

    if (!LogsDatabase.CheckRequiredField("logs_db_version", REVISION_DB_LOGS))
    {
        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        LoginDatabase.HaltDelayThread();
        return false;
    }

    sLog.outString();

    ///- Get the realm Id from the configuration file | 从配置文件读取当前分区（Realm）ID
    realmID = sConfig.GetIntDefault("RealmID", 0);
    if (!realmID)
    {
        sLog.outError("Realm ID not defined in configuration file");

        ///- Wait for already started DB delay threads to end | 等待已启动的数据库延迟线程结束
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        LoginDatabase.HaltDelayThread();
        return false;
    }

    sLog.outString("Realm running as realm ID %d", realmID);
    sLog.outString();

    ///- Clean the database before starting | 启动服务前先清理残留的在线状态
    clearOnlineAccounts();

    sWorld.LoadDBVersion();

    sLog.outString("Using World DB: %s", sWorld.GetDBVersion());
    sLog.outString("Using creature EventAI: %s", sWorld.GetCreatureEventAIVersion());
    sLog.outString();
    return true;
}

/// Clear 'online' status for all accounts with characters in this realm | 清除当前分区内所有账号及角色的在线状态标记
void Master::clearOnlineAccounts()
{
    // Cleanup online status for characters hosted at current realm | 清理当前分区托管角色与账号的在线状态
    /// \todo Only accounts with characters logged on *this* realm should have online status reset. Move the online column from 'account' to 'realmcharacters'? | 待办：应仅重置登录到当前分区的账号在线状态，考虑将 online 列从 account 表移至 realmcharacters 表？
    LoginDatabase.PExecute("UPDATE account SET active_realm_id = 0 WHERE active_realm_id = '%u'", realmID);

    CharacterDatabase.Execute("UPDATE characters SET online = 0 WHERE online<>0");

    // Battleground instance ids reset at server restart | 服务器重启时重置所有角色的战场副本实例 ID
    CharacterDatabase.Execute("UPDATE character_battleground_data SET instance_id = 0");
}

/// Handle termination signals | 处理系统终止信号（触发重启或正常关机，并等待主线程完成清理）
void Master::_OnSignal(int s)
{
    switch (s)
    {
        case SIGINT:
            World::StopNow(RESTART_EXIT_CODE);
            break;
        case SIGTERM:
#ifdef _WIN32
        case SIGBREAK:
#endif
            World::StopNow(SHUTDOWN_EXIT_CODE);
            break;
    }

    // give a 30 sec timeout in case of Master cannot finish properly | 等待主线程清理完成（最多约 20 秒超时，防止 Master 无法正常结束）
    int32 timeOut = 200;
    while (!m_canBeKilled && timeOut > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        --timeOut;
    }

    signal(s, _OnSignal);
}

/// Define hook '_OnSignal' for all termination signals | 为所有终止信号注册 '_OnSignal' 处理函数
void Master::_HookSignals()
{
    signal(SIGINT, _OnSignal);
    signal(SIGTERM, _OnSignal);
#ifdef _WIN32
    signal(SIGBREAK, _OnSignal);
#endif
}

/// Unhook the signals before leaving | 退出前注销信号处理函数
void Master::_UnhookSignals()
{
    signal(SIGINT, nullptr);
    signal(SIGTERM, nullptr);
#ifdef _WIN32
    signal(SIGBREAK, nullptr);
#endif
}
