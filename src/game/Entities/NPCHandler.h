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

#ifndef __NPCHANDLER_H
#define __NPCHANDLER_H

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
// GCC 使用不同的 #pragma pack(N) 语法，旧版 GCC 不支持 pack(push, N)，且部分平台版本亦不支持
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

// 书页文本结构（按 1 字节对齐）
struct PageText
{
    uint32 Page_ID;                                         // 书页 ID
    char* Text;                                             // 书页正文文本

    uint32 Next_Page;                                       // 下一页 ID（0 表示末页）
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
// GCC 使用不同的 #pragma pack() 语法，旧版 GCC 不支持 pack(pop)，且部分平台版本亦不支持
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

// 书页文本本地化信息
struct PageTextLocale
{
    std::vector<std::string> Text;                          // 按区域语言索引的书页文本列表
};

// NPC 对话文本本地化信息
struct NpcTextLocale
{
    NpcTextLocale() { Text_0.resize(8); Text_1.resize(8); }

    std::vector<std::vector<std::string> > Text_0;          // 男性/默认对白文本（最多 8 个选项）
    std::vector<std::vector<std::string> > Text_1;          // 女性/备用对白文本（最多 8 个选项）
};

// 对话动作表情与延迟
struct QEmote
{
    uint32 _Emote;                                          // 表情动作 ID
    uint32 _Delay;                                          // 延迟时间（毫秒）
};

// NPC Gossip 单条文本选项配置
struct GossipTextOption
{
    std::string Text_0;                                     // 默认/男性角色看到的文本
    std::string Text_1;                                     // 女性角色看到的文本
    uint32 Language;                                        // 语言 ID
    float Probability;                                      // 出现概率权重
    QEmote Emotes[3];                                       // 关联的 3 组表情动作
    uint32 broadcastTextId;                                 // 广播文本 ID
};

// 单条 NPC 文本最多包含的对话选项数
#define MAX_GOSSIP_TEXT_OPTIONS 8

// NPC Gossip 对话文本集合
struct GossipText
{
    GossipTextOption Options[MAX_GOSSIP_TEXT_OPTIONS];      // 8 组可选文本项
};

#endif
