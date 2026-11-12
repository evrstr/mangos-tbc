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

#ifndef CONFIG_H
#define CONFIG_H

#include "Common.h"
#include "Policies/Singleton.h"
#include "Platform/Define.h"
#include <mutex>

#include <string>
#include <unordered_map>

/// 配置文件解析与管理单例类：读取 INI 格式配置并支持通过环境变量覆盖配置项
class Config
{
    private:
        std::string m_filename;     ///< 当前加载的配置文件路径
        std::string m_envVarPrefix; ///< 环境变量前缀（如 MANGOSD_）
        std::unordered_map<std::string, std::string> m_entries; // keys are converted to lower case.  values cannot be. | 键值对映射表（键名统一转为小写，值保持原样）

    public:
        /// 指定配置文件路径与环境变量前缀并立即加载配置
        bool SetSource(const std::string& file, const std::string& envVarPrefix);
        /// 重新读取并解析配置文件（线程安全）
        bool Reload();

        /// 判断指定配置项是否存在于已加载的配置文件中
        bool IsSet(const std::string& name) const;

        /// 获取字符串配置项，若缺失则尝试从环境变量回退，否则返回默认值
        const std::string GetStringDefault(const std::string& name, const std::string& def = "") const;
        /// 获取布尔配置项（支持 true/1/yes，忽略大小写）
        bool GetBoolDefault(const std::string& name, bool def) const;
        /// 获取整型配置项
        int32 GetIntDefault(const std::string& name, int32 def) const;
        /// 获取浮点型配置项
        float GetFloatDefault(const std::string& name, float def) const;

        /// 获取当前配置文件名
        const std::string& GetFilename() const { return m_filename; }
        std::mutex m_configLock;    ///< 配置重载互斥锁
};

#define sConfig MaNGOS::Singleton<Config>::Instance()

#endif
