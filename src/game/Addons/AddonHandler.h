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

#ifndef __ADDONHANDLER_H
#define __ADDONHANDLER_H

#include "Common.h"
#include "Policies/Singleton.h"
#include "Server/WorldPacket.h"

/// 客户端插件验证处理器，负责解析客户端上传的插件列表并构建服务端响应包
class AddonHandler
{
    public:
        /* Construction (构造与析构) */
        AddonHandler();
        ~AddonHandler();
        // built addon packet (解析压缩插件数据包并生成 SMSG_ADDON_INFO 响应)
        bool BuildAddonPacket(WorldPacket& Source, WorldPacket& Target);
};
#define sAddOnHandler MaNGOS::Singleton<AddonHandler>::Instance()
#endif
