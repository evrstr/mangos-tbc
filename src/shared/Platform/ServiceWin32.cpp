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

#ifdef _WIN32

#include "Common.h"
#include "Log/Log.h"
#include <cstring>
#include <windows.h>
#include <winsvc.h>

#if !defined(WINADVAPI)
#if !defined(_ADVAPI32_)
#define WINADVAPI DECLSPEC_IMPORT
#else
#define WINADVAPI
#endif
#endif

extern int main(int argc, char** argv);
extern char serviceLongName[];
extern char serviceName[];
extern char serviceDescription[];

extern int m_ServiceStatus;

SERVICE_STATUS serviceStatus;

SERVICE_STATUS_HANDLE serviceStatusHandle = nullptr;

typedef WINADVAPI BOOL (WINAPI* CSD_T)(SC_HANDLE, DWORD, LPCVOID);

bool WinServiceInstall()
{
    SC_HANDLE serviceControlManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);

    if (!serviceControlManager)
    {
        sLog.outError("SERVICE: No access to service control manager.");
        return false;
    }

    char path[_MAX_PATH + 10];
    if (!GetModuleFileName(nullptr, path, sizeof(path) / sizeof(path[0])))
    {
        CloseServiceHandle(serviceControlManager);
        sLog.outError("SERVICE: Can't get service binary filename.");
        return false;
    }

    std::strcat(path, " -s run");

    SC_HANDLE service = CreateService(serviceControlManager,
                                      serviceName,          // name of service | 服务内部名称
                                      serviceLongName,      // service name to display | 服务显示名称
                                      SERVICE_ALL_ACCESS,   // desired access | 请求全部访问权限
                                      // service type | 服务类型（独立进程 + 可交互服务）
                                      SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
                                      SERVICE_AUTO_START,   // start type | 开机自启
                                      SERVICE_ERROR_IGNORE, // error control type | 忽略启动错误
                                      path,                 // service's binary | 服务可执行文件路径及参数
                                      nullptr,              // no load ordering group | 无加载顺序组
                                      nullptr,              // no tag identifier | 无标签 ID
                                      nullptr,              // no dependencies | 无依赖服务
                                      nullptr,              // LocalSystem account | 使用 LocalSystem 本地系统账户
                                      nullptr);             // no password | 无密码

    if (!service)
    {
        CloseServiceHandle(serviceControlManager);
        sLog.outError("SERVICE: Can't register service for: %s", path);
        return false;
    }

    HMODULE advapi32 = GetModuleHandle("ADVAPI32.DLL");
    if (!advapi32)
    {
        sLog.outError("SERVICE: Can't access ADVAPI32.DLL");
        CloseServiceHandle(service);
        CloseServiceHandle(serviceControlManager);
        return false;
    }

    CSD_T ChangeService_Config2 = (CSD_T)GetProcAddress(advapi32, "ChangeServiceConfig2A");
    if (!ChangeService_Config2)
    {
        sLog.outError("SERVICE: Can't access ChangeServiceConfig2A from ADVAPI32.DLL");
        CloseServiceHandle(service);
        CloseServiceHandle(serviceControlManager);
        return false;
    }

    SERVICE_DESCRIPTION sdBuf;
    sdBuf.lpDescription = serviceDescription;
    ChangeService_Config2(
        service,                                // handle to service | 服务句柄
        SERVICE_CONFIG_DESCRIPTION,             // change: description | 修改服务描述信息
        &sdBuf);                                // new data | 新描述数据

    SC_ACTION _action[1];
    _action[0].Type = SC_ACTION_RESTART;
    _action[0].Delay = 10000;
    SERVICE_FAILURE_ACTIONS sfa;
    ZeroMemory(&sfa, sizeof(SERVICE_FAILURE_ACTIONS));
    sfa.lpsaActions = _action;
    sfa.cActions = 1;
    sfa.dwResetPeriod = INFINITE;
    ChangeService_Config2(
        service,                                // handle to service | 服务句柄
        SERVICE_CONFIG_FAILURE_ACTIONS,         // information level | 修改失败恢复策略（10 秒后自动重启）
        &sfa);                                  // new data | 恢复策略配置

    CloseServiceHandle(service);
    CloseServiceHandle(serviceControlManager);
    return true;
}

bool WinServiceUninstall()
{
    SC_HANDLE serviceControlManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);

    if (!serviceControlManager)
    {
        sLog.outError("SERVICE: No access to service control manager.");
        return false;
    }

    SC_HANDLE service = OpenService(serviceControlManager,
                                    serviceName, SERVICE_QUERY_STATUS | DELETE);

    if (!service)
    {
        CloseServiceHandle(serviceControlManager);
        sLog.outError("SERVICE: Service not found: %s", serviceName);
        return false;
    }

    SERVICE_STATUS serviceStatus2;
    if (QueryServiceStatus(service, &serviceStatus2))
    {
        if (serviceStatus2.dwCurrentState == SERVICE_STOPPED)
            DeleteService(service);
    }

    CloseServiceHandle(service);
    CloseServiceHandle(serviceControlManager);
    return true;
}

void WINAPI ServiceControlHandler(DWORD controlCode)
{
    switch (controlCode)
    {
        case SERVICE_CONTROL_INTERROGATE:
            break;

        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
            serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(serviceStatusHandle, &serviceStatus);

            m_ServiceStatus = 0;
            return;

        case SERVICE_CONTROL_PAUSE:
            m_ServiceStatus = 2;
            serviceStatus.dwCurrentState = SERVICE_PAUSED;
            SetServiceStatus(serviceStatusHandle, &serviceStatus);
            break;

        case SERVICE_CONTROL_CONTINUE:
            serviceStatus.dwCurrentState = SERVICE_RUNNING;
            SetServiceStatus(serviceStatusHandle, &serviceStatus);
            m_ServiceStatus = 1;
            break;

        default:
        {
            if (controlCode >= 128 && controlCode <= 255)
                // user defined control code | 用户自定义控制码
                break;
                // unrecognized control code | 未识别的控制码
            break;
        }
    }

    SetServiceStatus(serviceStatusHandle, &serviceStatus);
}

void WINAPI ServiceMain(DWORD argc, char* argv[])
{
    // initialise service status | 初始化服务运行状态结构体
    serviceStatus.dwServiceType = SERVICE_WIN32;
    serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
    serviceStatus.dwWin32ExitCode = NO_ERROR;
    serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;

    serviceStatusHandle = RegisterServiceCtrlHandler(serviceName, ServiceControlHandler);

    if (serviceStatusHandle)
    {
        char path[_MAX_PATH + 1];
        unsigned int last_slash = 0;

        GetModuleFileName(nullptr, path, sizeof(path) / sizeof(path[0]));

        for (unsigned int i = 0; i < std::strlen(path); ++i)
        {
            if (path[i] == '\\') last_slash = i;
        }

        path[last_slash] = 0;

        // service is starting | 通知 SCM 服务正在启动
        serviceStatus.dwCurrentState = SERVICE_START_PENDING;
        SetServiceStatus(serviceStatusHandle, &serviceStatus);

        // do initialisation here | 将当前工作目录切换到服务可执行文件所在目录
        SetCurrentDirectory(path);

        // running | 上报服务已进入运行状态
        serviceStatus.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
        serviceStatus.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus(serviceStatusHandle, &serviceStatus);

        ////////////////////////
        // service main cycle // | 服务主循环入口
        ////////////////////////

        m_ServiceStatus = 1;
        argc = 1;
        main(argc, argv);

        // service was stopped | 通知 SCM 服务正在停止
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(serviceStatusHandle, &serviceStatus);

        // do cleanup here | 清理阶段

        // service is now stopped | 上报服务已完全停止
        serviceStatus.dwControlsAccepted &= ~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(serviceStatusHandle, &serviceStatus);
    }
}

bool WinServiceRun()
{
    SERVICE_TABLE_ENTRY serviceTable[] =
    {
        { serviceName, ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcher(serviceTable))
    {
        sLog.outError("StartService Failed. Error [%u]", ::GetLastError());
        return false;
    }
    return true;
}
#endif
