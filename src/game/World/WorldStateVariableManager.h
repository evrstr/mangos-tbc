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

#ifndef WORLD_STATE_MANAGER_DEFINES_H
#define WORLD_STATE_MANAGER_DEFINES_H

#include "Platform/Define.h"
#include <map>
#include <functional>
#include <string>
#include <vector>

class ByteBuffer;
class Map;

/// 地图级世界状态变量存储条目
struct WorldStateVariable
{
    int32 value;                                            ///< 当前变量数值
    bool send;                                              ///< 变更时是否向客户端下发 SMSG_UPDATE_WORLD_STATE
    uint32 zoneId;                                          ///< 仅限指定 Zone 可见（0 表示全地图）
    uint32 areaId;                                          ///< 仅限指定 Area 可见（0 表示不限）
    std::vector<std::function<void()>> executors;           ///< 变量变更回调列表

    WorldStateVariable() : value(0), send(false), zoneId(0), areaId(0) {}
};

/// 地图实例级世界状态变量管理器（每个 Map 持有一个实例，负责变量读写、初始包填充与客户端广播）
class WorldStateVariableManager
{
    public:
        WorldStateVariableManager(Map* map);

        /// 根据已完成首领掩码初始化地下城遭遇战对应的世界状态变量
        void Initialize(uint32 completedEncounterMask);

        /// 获取指定 ID 的世界状态值（未定义返回 0）
        int32 GetVariable(int32 Id) const;
        /// 获取指定 ID 的世界状态完整元数据指针
        WorldStateVariable const* GetVariableData(int32 Id) const;
        /// 设置变量值并在启用发送标志且值变化时广播给地图内玩家
        void SetVariable(int32 Id, int32 value);
        /// 配置变量的客户端发送开关及可见区域过滤条件（zoneId / areaId）
        void SetVariableData(int32 Id, bool send, uint32 zoneId, uint32 areaId);
        /// 为变量注册变更回调执行器
        void AddVariableExecutor(int32 Id, std::function<void()>& executor);

        /// 填充玩家进入区域时的初始世界状态包 SMSG_INIT_WORLD_STATES
        void FillInitialWorldStates(ByteBuffer& data, uint32& count, uint32 zoneId, uint32 areaId);
        /// 向地图内符合区域条件的在线玩家广播单个世界状态更新
        void BroadcastVariable(int32 Id) const;

        /// 导出当前所有世界状态变量 ID、值及名称的可读调试列表
        std::string GetVariableList() const;

        /// 设置兼容旧遭遇战编号的世界状态开关（encounterId * 100 / +1）
        void SetEncounterVariable(uint32 encounterId, bool state);

    private:
        std::map<int32, WorldStateVariable> m_variables;    ///< 世界状态 ID 到变量数据的映射表
        Map* m_owner;                                       ///< 所属地图实例指针
};

#endif