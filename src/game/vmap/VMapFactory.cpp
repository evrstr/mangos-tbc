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

#include "VMapFactory.h"
#include "VMapManager2.h"

using namespace G3D;

namespace VMAP
{
    // 去除字符串首尾空白符、回车换行与引号
    void VMapFactory::chompAndTrim(std::string& str)
    {
        while (str.length() > 0)
        {
            char lc = str[str.length() - 1];
            if (lc == '\r' || lc == '\n' || lc == ' ' || lc == '"' || lc == '\'')
            {
                str = str.substr(0, str.length() - 1);
            }
            else
            {
                break;
            }
        }
        while (str.length() > 0)
        {
            char lc = str[0];
            if (lc == ' ' || lc == '"' || lc == '\'')
            {
                str = str.substr(1, str.length() - 1);
            }
            else
            {
                break;
            }
        }
    }

    IVMapManager* gVMapManager = nullptr;                       // 全局 VMapManager 单例指针

    //===============================================
    // result false, if no more id are found (从逗号分隔字符串解析下一个整数 ID，无更多 ID 时返回 false)

    bool VMapFactory::getNextId(const std::string& pString, unsigned int& pStartPos, unsigned int& pId)
    {
        bool result = false;
        unsigned int i;
        for (i = pStartPos; i < pString.size(); ++i)
        {
            if (pString[i] == ',')
            {
                break;
            }
        }
        if (i > pStartPos)
        {
            std::string idString = pString.substr(pStartPos, i - pStartPos);
            pStartPos = i + 1;
            chompAndTrim(idString);
            pId = atoi(idString.c_str());
            result = true;
        }
        return result;
    }

    //===============================================
    // just return the instance (惰性创建并返回全局唯一 VMapManager2 实例)
    IVMapManager* VMapFactory::createOrGetVMapManager()
    {
        if (!gVMapManager) // called once on load
            gVMapManager = new VMapManager2();              // should be taken from config ... Please change if you like :-)
        return gVMapManager;
    }

    //===============================================
    // delete all internal data structures (释放全局 VMapManager 实例及其缓存的所有地图/模型资源)
    void VMapFactory::clear()
    {
        delete gVMapManager; // called once on world destruction where single threaded is guaranteed
        gVMapManager = nullptr;
    }
}
