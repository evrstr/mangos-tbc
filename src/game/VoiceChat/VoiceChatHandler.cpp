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

#include "Common.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "Log/Log.h"

// 处理客户端启用/禁用语音聊天会话请求（当前仅跳过字段并输出十六进制调试日志）
void WorldSession::HandleVoiceSessionEnableOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_VOICE_SESSION_ENABLE");
    // uint8 isVoiceEnabled, uint8 isMicrophoneEnabled (uint8 是否启用语音，uint8 是否启用麦克风)
    recv_data.read_skip<uint8>();
    recv_data.read_skip<uint8>();
    recv_data.hexlike();
}

// 处理频道右键菜单开启语音请求（仅输出调试包内容）
void WorldSession::HandleChannelVoiceOnOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_CHANNEL_VOICE_ON");
    // Enable Voice button in channel context menu (在频道上下文菜单中启用语音按钮)
    recv_data.hexlike();
}

// 处理设置当前激活语音频道请求（仅跳过参数并输出调试包内容）
void WorldSession::HandleSetActiveVoiceChannel(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_SET_ACTIVE_VOICE_CHANNEL");
    recv_data.read_skip<uint32>();
    recv_data.read_skip<char*>();
    recv_data.hexlike();
}
