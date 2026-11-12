// add here most rarely modified headers to speed up debug build compilation (在此添加最少修改的头文件以加速调试版本编译 - 预编译头文件 PCH)
#ifndef PCHDEF_H
#define PCHDEF_H

// 基础平台与 C/C++ 标准库头文件
#include "Platform/Define.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cmath>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <set>
#include <list>
#include <string>
#include <map>
#include <queue>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

// 通用工具、多线程与网络库
#include "Util/Errors.h"
#include "Multithreading/Threading.h"
#include <thread>
#include <boost/asio.hpp>

// 核心共享定义与日志系统
#include "Common.h"
#include "Log/Log.h"

// 数据库与 DBC 数据存储
#include "Server/SQLStorages.h"
#include "Server/DBCStructure.h"
#include "Server/DBCStores.h"

// 游戏全局访问器、网络操作码与数据包
#include "Globals/ObjectAccessor.h"
#include "Server/Opcodes.h"
#include "Util/ByteBuffer.h"

// 实体更新与世界对象管理
#include "Entities/UpdateFields.h"
#include "Entities/UpdateData.h"
#include "Globals/SharedDefines.h"
#include "Globals/ObjectMgr.h"
#include "DBScripts/ScriptMgr.h"
#include "Entities/ObjectGuid.h"
#include "Entities/Object.h"

// AI 基础与 ScriptDevAI 脚本系统头文件
#include "AI/BaseAI/CreatureAI.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "AI/ScriptDevAI/include/sc_creature.h"
#include "AI/ScriptDevAI/include/sc_gossip.h"
#include "AI/ScriptDevAI/include/sc_grid_searchers.h"
#include "AI/ScriptDevAI/include/sc_instance.h"
#endif // PCHDEF_H
