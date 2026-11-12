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
#include "Log/Log.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "World/World.h"
#include "Server/Opcodes.h"
#include "Globals/ObjectMgr.h"
#include "Chat/Chat.h"
#include "Chat/ChannelMgr.h"
#include "Groups/Group.h"
#include "Guilds/Guild.h"
#include "Guilds/GuildMgr.h"
#include "Entities/Player.h"
#include "Spells/SpellAuras.h"
#include "Tools/Language.h"
#include "Util/Util.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"
#include "GMTickets/GMTicketMgr.h"
#include "Anticheat/Anticheat.hpp"

#ifdef ENABLE_PLAYERBOTS
#include "playerbot/playerbot.h"
#include "playerbot/RandomPlayerbotMgr.h"
#endif

/// 校验聊天消息长度、不可见控制字符与链接转义序列安全性
bool WorldSession::CheckChatMessage(std::string& msg, bool addon/* = false*/)
{
    // check max length: as of 2.3.x+ no longer disconnects, silently truncates to 255 (wowwiki) | 检查最大长度：2.3.x 起不再踢线，而是静默截断至 255 字节
    if (msg.length() > 255)
        utf8limit(msg, 255);

    // skip remaining checks for addon messages or higher sec level accounts | 插件消息或高权限账号跳过后续过滤检查
    if (addon || GetSecurity() > SEC_PLAYER)
        return true;

    // strip invisible characters for non-addon messages | 非插件消息剥离不可见字符防止伪造消息
    if (sWorld.getConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING))
        stripLineInvisibleChars(msg);

    // check links | 严格检查聊天中的颜色/超链接转义序列合法性
    if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY))
    {
        if (!ChatHandler::CheckEscapeSequences(msg.c_str()))
        {
            sLog.outError("Player %s (GUID: %u) sent a chat message with an invalid escape sequence: \"%s\"", GetPlayer()->GetName(),
                          GetPlayer()->GetGUIDLow(), msg.c_str());

            if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK))
                KickPlayer();

            return false;
        }
    }
    return true;
}

/// 处理客户端聊天消息主封包 (CMSG_MESSAGECHAT)：校验语言、禁言与 GM 命令并按频道类型分发
void WorldSession::HandleMessagechatOpcode(WorldPacket& recv_data)
{
    uint32 type;
    uint32 lang;

    recv_data >> type;
    recv_data >> lang;

    if (type >= MAX_CHAT_MSG_TYPE)
    {
        sLog.outError("CHAT: Wrong message type received: %u", type);
        return;
    }

    DEBUG_LOG("CHAT: packet received. type %u, lang %u", type, lang);

    // prevent talking at unknown language (cheating) | 校验语言 ID 合法性及角色是否已掌握对应语言技能或理解光环（防作弊）
    LanguageDesc const* langDesc = GetLanguageDescByID(lang);
    if (!langDesc)
    {
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        return;
    }
    if (langDesc->skill_id != 0 && !_player->HasSkill(langDesc->skill_id))
    {
        // also check SPELL_AURA_COMPREHEND_LANGUAGE (client offers option to speak in that language) | 同时检查语言理解光环（客户端允许使用该光环对应的语言发言）
        Unit::AuraList const& langAuras = _player->GetAurasByType(SPELL_AURA_COMPREHEND_LANGUAGE);
        bool foundAura = false;
        for (auto langAura : langAuras)
        {
            if (langAura->GetModifier()->m_miscvalue == int32(lang))
            {
                foundAura = true;
                break;
            }
        }
        if (!foundAura)
        {
            SendNotification(LANG_NOT_LEARNED_LANGUAGE);
            return;
        }
    }

    if (lang == LANG_ADDON)
    {
        // Disabled addon channel? | 服务器是否关闭了插件通信频道
        if (!sWorld.getConfig(CONFIG_BOOL_ADDON_CHANNEL))
            return;
    }
    // LANG_ADDON should not be changed nor be affected by flood control | 插件频道不改写语言且不受刷屏频率控制
    else
    {
        // send in universal language if player in .gmon mode (ignore spell effects) | GM 模式下强制使用通用语且忽略语言变更光环
        if (_player->IsGameMaster())
            lang = LANG_UNIVERSAL;
        else
        {
            // send in universal language in two side iteration allowed mode | 若开启跨阵营聊天互通则统一使用通用语
            if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                lang = LANG_UNIVERSAL;
            else
            {
                switch (type)
                {
                    case CHAT_MSG_PARTY:
                    case CHAT_MSG_RAID:
                    case CHAT_MSG_RAID_LEADER:
                    case CHAT_MSG_RAID_WARNING:
                        // allow two side chat at group channel if two side group allowed | 开启跨阵营组队时允许队伍/团队内使用通用语
                        if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                            lang = LANG_UNIVERSAL;
                        break;
                    case CHAT_MSG_GUILD:
                    case CHAT_MSG_OFFICER:
                        // allow two side chat at guild channel if two side guild allowed | 开启跨阵营公会时允许公会内使用通用语
                        if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                            lang = LANG_UNIVERSAL;
                        break;
                }
            }

            // but overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single case used) | 若存在强制变语光环（如变熊/醉酒等）则覆盖语言类型
            Unit::AuraList const& ModLangAuras = _player->GetAurasByType(SPELL_AURA_MOD_LANGUAGE);
            if (!ModLangAuras.empty())
                lang = ModLangAuras.front()->GetModifier()->m_miscvalue;
        }

        if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
        {
            if (!_player->CanSpeak())
            {
                std::string timeStr = secsToTimeString(m_muteTime - time(nullptr));
                SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
                return;
            }

            GetPlayer()->UpdateSpeakTime();
        }
    }

    switch (type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!CheckChatMessage(msg))
                return;

#ifdef ENABLE_PLAYERBOTS
            if (GetSecurity() > SEC_PLAYER && GetPlayer()->IsGameMaster())
            {
                sRandomPlayerbotMgr.HandleCommand(type, msg, *_player, "", TEAM_BOTH_ALLOWED, lang);
            }
            else
            {
                sRandomPlayerbotMgr.HandleCommand(type, msg, *_player, "", GetPlayer()->GetTeam(), lang);
            }

            // apply to own bots | 转发给玩家自身控制的机器人
            if (_player->GetPlayerbotMgr())
            {
                _player->GetPlayerbotMgr()->HandleCommand(type, msg, lang);
            }
#endif

            if (type == CHAT_MSG_SAY)
            {
                GetPlayer()->Say(msg, lang);

                if (lang != LANG_ADDON && !m_anticheat->IsSilenced())
                    m_anticheat->Say(msg);
            }
            else if (type == CHAT_MSG_EMOTE)
            {
                GetPlayer()->TextEmote(msg);

                if (lang != LANG_ADDON && !m_anticheat->IsSilenced())
                    m_anticheat->Say(msg);
            }
            else if (type == CHAT_MSG_YELL)
            {
                GetPlayer()->Yell(msg, lang);

                if (lang != LANG_ADDON && !m_anticheat->IsSilenced())
                    m_anticheat->Say(msg);
            }
        } break;

        case CHAT_MSG_WHISPER:
        {
            std::string to, msg;
            recv_data >> to;

            // WHISPER channel is available for addons since 2.1.0 | 2.1.0 起插件消息也允许使用密语频道
            if (lang == LANG_ADDON)
                recv_data.read(msg , false);
            else
                recv_data >> msg;

            if (msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!CheckChatMessage(msg, (lang == LANG_ADDON)))
                return;

            if (!normalizePlayerName(to))
            {
                SendPlayerNotFoundNotice(to);
                break;
            }

            Player* player = sObjectMgr.GetPlayer(to.c_str());

            std::pair<bool, bool> hookresult = sTicketMgr.HookGMTicketWhisper(GetPlayer(), to, player, msg, (lang == LANG_ADDON));

            if (hookresult.first)
            {
                if (!hookresult.second)
                    SendPlayerNotFoundNotice(to);
                return;
            }

            uint32 tSecurity = GetSecurity();
            uint32 pSecurity = player ? player->GetSession()->GetSecurity() : SEC_PLAYER;
            if (!player || (tSecurity == SEC_PLAYER && pSecurity > SEC_PLAYER && !player->isAcceptWhispers()))
            {
                SendPlayerNotFoundNotice(to);
                return;
            }

            if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && tSecurity == SEC_PLAYER && pSecurity == SEC_PLAYER)
            {
                if (GetPlayer()->GetTeam() != player->GetTeam())
                {
                    SendWrongFactionNotice();
                    return;
                }
            }

            if (player->isEnabledWhisperRestriction())
            {
                if (!GetPlayer()->IsGameMaster() && !player->isAllowedWhisperFrom(this->GetPlayer()->GetObjectGuid()))
                {
                    ChatHandler(this).PSendSysMessage("%s did not receive your message because they are currently restricting whispers to their friends, guild, and group.", player->GetName());
                    return;
                }
            }

#ifdef ENABLE_PLAYERBOTS
            if (player->GetPlayerbotAI())
            {
                player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer(), lang);
                GetPlayer()->m_speakTime = 0;
                GetPlayer()->m_speakCount = 0;
            }

            if (msg.find("BOT\t") != 0) //These are spoofed SendAddonMessage with channel "WHISPER". | 过滤以 "BOT\t" 开头的伪造密语插件控制指令
#endif

            GetPlayer()->Whisper(msg, lang, player->GetObjectGuid());

            if (lang != LANG_ADDON && !m_anticheat->IsSilenced())
                m_anticheat->Whisper(msg, player->GetObjectGuid());
        } break;

        case CHAT_MSG_PARTY:
        {
            std::string msg;

            if (lang == LANG_ADDON)
                recv_data.read(msg , false);
            else
                recv_data >> msg;

            if (msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!CheckChatMessage(msg, (lang == LANG_ADDON)))
                return;

            // if player is in battleground, party chat is sent only to members of normal group | 若玩家在战场团队中，小队聊天仅发送给原始普通小队成员
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup() | 战场团队始终位于 GetGroup()，原始队伍位于 GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (group && group->IsBattleGroup())
                group = _player->GetOriginalGroup();

            if (!group)
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, ChatMsg(type), msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false, group->GetMemberGroup(GetPlayer()->GetObjectGuid()));

#ifdef ENABLE_PLAYERBOTS
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* player = itr->getSource();
                if (player && player->GetPlayerbotAI() && player->IsInGroup(GetPlayer(), true))
                {
                    player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer(), lang);
                    GetPlayer()->m_speakTime = 0;
                    GetPlayer()->m_speakCount = 0;
                }
            }
#endif

            break;
        }
        case CHAT_MSG_GUILD:
        {
            std::string msg;

            if (lang == LANG_ADDON)
                recv_data.read(msg , false);
            else
                recv_data >> msg;

            if (msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!CheckChatMessage(msg, (lang == LANG_ADDON)))
                return;

            if (GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                    guild->BroadcastToGuild(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);

#ifdef ENABLE_PLAYERBOTS
            PlayerbotMgr* mgr = GetPlayer()->GetPlayerbotMgr();
            if (mgr && GetPlayer()->GetGuildId())
            {
                mgr->ForEachPlayerbot([&](Player* const bot)
                {
                    if (bot->GetGuildId() == GetPlayer()->GetGuildId())
                    {
                        bot->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer(), lang);
                    }
                });
            }

            sRandomPlayerbotMgr.HandleCommand(type, msg, *_player, "", GetPlayer()->GetTeam(), lang);
#endif

            break;
        }
        case CHAT_MSG_OFFICER:
        {
            std::string msg;

            if (lang == LANG_ADDON)
                recv_data.read(msg , false);
            else
                recv_data >> msg;

            if (msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!CheckChatMessage(msg, (lang == LANG_ADDON)))
                return;

            if (GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                    guild->BroadcastToOfficers(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);

            break;
        }
        case CHAT_MSG_RAID:
        {
            std::string msg;

            if (lang == LANG_ADDON)
                recv_data.read(msg , false);
            else
                recv_data >> msg;

            if (msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!CheckChatMessage(msg, (lang == LANG_ADDON)))
                return;

            // if player is in battleground, raid chat is sent only to members of normal group
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group)
                return;

            if (group->IsBattleGroup())
                group = _player->GetOriginalGroup();

            if (!group || !group->IsRaidGroup())
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false);

#ifdef ENABLE_PLAYERBOTS
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* player = itr->getSource();
                if (player && player->GetPlayerbotAI())
                {
                    player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer(), lang);
                    GetPlayer()->m_speakTime = 0;
                    GetPlayer()->m_speakCount = 0;
                }
            }
#endif

        } break;
        case CHAT_MSG_RAID_LEADER:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!CheckChatMessage(msg))
                return;

            // if player is in battleground, raid chat is sent only to members of normal group
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group)
                return;

            if (group->IsBattleGroup())
                group = _player->GetOriginalGroup();

            if (!group || !group->IsRaidGroup() || !group->IsLeader(_player->GetObjectGuid()))
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_LEADER, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false);

#ifdef ENABLE_PLAYERBOTS
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* player = itr->getSource();
                if (player && player->GetPlayerbotAI())
                {
                    player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer(), lang);
                    GetPlayer()->m_speakTime = 0;
                    GetPlayer()->m_speakCount = 0;
                }
            }
#endif

        } break;

        case CHAT_MSG_RAID_WARNING:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                break;

            if (!CheckChatMessage(msg))
                return;

            // if player is in battleground, raid warning is sent only to players in battleground
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group)
                return;
            else if (group->IsRaidGroup())
            {
                if (!group->IsLeader(_player->GetObjectGuid()) && !group->IsAssistant(_player->GetObjectGuid()))
                    return;
            }
            else if (sWorld.getConfig(CONFIG_BOOL_CHAT_RESTRICTED_RAID_WARNINGS))
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_WARNING, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, true);

#ifdef ENABLE_PLAYERBOTS
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* player = itr->getSource();
                if (player && player->GetPlayerbotAI())
                {
                    player->GetPlayerbotAI()->HandleCommand(type, msg, *GetPlayer(), lang);
                    GetPlayer()->m_speakTime = 0;
                    GetPlayer()->m_speakCount = 0;
                }
            }
#endif

        } break;

        case CHAT_MSG_BATTLEGROUND:
        {
            std::string msg;

            if (lang == LANG_ADDON)
                recv_data.read(msg , false);
            else
                recv_data >> msg;

            if (msg.empty())
                break;

            if (!CheckChatMessage(msg, (lang == LANG_ADDON)))
                return;

            // battleground chat is sent only to players in battleground
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group || !group->IsBattleGroup())
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND_LEADER:
        {
            std::string msg;
            recv_data >> msg;

            if (msg.empty())
                break;

            if (!CheckChatMessage(msg))
                return;

            // battleground chat is sent only to players in battleground
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group || !group->IsBattleGroup() || !group->IsLeader(_player->GetObjectGuid()))
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND_LEADER, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false);
        } break;

        case CHAT_MSG_CHANNEL:
        {
            std::string channel, msg;
            recv_data >> channel;
            recv_data >> msg;

            if (msg.empty())
                break;

            if (!CheckChatMessage(msg))
                return;

            if (ChannelMgr* cMgr = channelMgr(_player->GetTeam()))
            {
                if (Channel* chn = cMgr->GetChannel(channel, _player))
                {
                    chn->Say(_player, msg.c_str(), lang);

                    if (lang != LANG_ADDON && (chn->HasFlag(Channel::ChannelFlags::CHANNEL_FLAG_GENERAL) || chn->IsStatic()))
                        m_anticheat->Channel(msg);

#ifdef ENABLE_PLAYERBOTS
                    // if GM apply to all random bots | 若为 GM 则向全体随机机器人分发命令
                    if (GetSecurity() > SEC_PLAYER && GetPlayer()->IsGameMaster())
                    {
                        sRandomPlayerbotMgr.HandleCommand(type, msg, *_player, "", TEAM_BOTH_ALLOWED, lang);
                    }
                    else
                    {
                        sRandomPlayerbotMgr.HandleCommand(type, msg, *_player, "", GetPlayer()->GetTeam(), lang);
                    }

                    // apply to own bots | 若为综合/本地防务等标志频道，转发给自身控制机器人
                    if (_player->GetPlayerbotMgr() && chn->GetFlags() & 0x18)
                    {
                        _player->GetPlayerbotMgr()->HandleCommand(type, msg, lang);
                    }
#endif
                }
            }
        } break;

        case CHAT_MSG_AFK:
        case CHAT_MSG_DND:
        {
            std::string msg;
            recv_data >> msg;

            const bool flagged = (type == CHAT_MSG_AFK ? _player->isAFK() : _player->isDND());

            if (flagged)                            // Already flagged | 已处于离开/请勿打扰状态
            {
                if (msg.empty())                    // Remove flag | 空消息表示取消状态标记
                {
                    if (type == CHAT_MSG_AFK)
                        _player->ToggleAFK();
                    else
                        _player->ToggleDND();
                }
                else                                // Update message | 非空消息则更新自动回复文本
                {
                    if (!CheckChatMessage(msg))
                        msg = GetMangosString(type == CHAT_MSG_AFK ? LANG_PLAYER_AFK_DEFAULT : LANG_PLAYER_DND_DEFAULT);
                    else
                        m_anticheat->AutoReply(msg);

                    _player->autoReplyMsg = msg;
                }
            }
            else                                    // New AFK/DND mode | 新开启离开/请勿打扰状态（互斥切换）
            {
                if (msg.empty() || !CheckChatMessage(msg))
                    msg = GetMangosString(type == CHAT_MSG_AFK ? LANG_PLAYER_AFK_DEFAULT : LANG_PLAYER_DND_DEFAULT);
                else
                    m_anticheat->AutoReply(msg);

                _player->autoReplyMsg = msg;

                if (type == CHAT_MSG_AFK)
                {
                    if (_player->isDND())
                        _player->ToggleDND();

                    _player->ToggleAFK();
                }
                else
                {
                    if (_player->isAFK())
                        _player->ToggleAFK();

                    _player->ToggleDND();
                }
            }
            break;
        }

        default:
            sLog.outError("CHAT: unknown message type %u, lang: %u", type, lang);
            break;
    }
}

/// 处理客户端单次动作表情请求 (CMSG_EMOTE)
void WorldSession::HandleEmoteOpcode(WorldPacket& recv_data)
{
    uint32 emote;
    recv_data >> emote;

    if (!GetPlayer()->IsAlive() || GetPlayer()->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PREVENT_ANIM))
        return;

    // restrict to the only emotes hardcoded in client | 仅允许客户端硬编码触发的表情动作
    if (emote != EMOTE_ONESHOT_NONE && emote != EMOTE_ONESHOT_WAVE)
        return;

    GetPlayer()->InterruptSpellsAndAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ANIM_CANCELS);
    GetPlayer()->HandleEmoteCommand(emote);
}

namespace MaNGOS
{
    /// 文本表情数据包本地化构建器
    class EmoteChatBuilder
    {
        public:
            EmoteChatBuilder(Player const& pl, uint32 text_emote, uint32 emote_num, Unit const* target)
                : i_player(pl), i_text_emote(text_emote), i_emote_num(emote_num), i_target(target) {}

            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* nam = i_target ? i_target->GetNameForLocaleIdx(loc_idx) : nullptr;
                uint32 namlen = (nam ? strlen(nam) : 0) + 1;

                data.Initialize(SMSG_TEXT_EMOTE, (20 + namlen));
                data << ObjectGuid(i_player.GetObjectGuid());
                data << uint32(i_text_emote);
                data << uint32(i_emote_num);
                data << uint32(namlen);
                if (namlen > 1)
                    data.append(nam, namlen);
                else
                    data << uint8(0x00);
            }

        private:
            Player const& i_player;
            uint32        i_text_emote;
            uint32        i_emote_num;
            Unit const*   i_target;
    };
}                                                           // namespace MaNGOS

/// 处理客户端文本表情请求 (CMSG_TEXT_EMOTE)：播放动画、按视野范围广播并通知目标 AI
void WorldSession::HandleTextEmoteOpcode(WorldPacket& recv_data)
{
    uint32 textEmote, emoteNum;
    ObjectGuid guid;

    recv_data >> textEmote;
    recv_data >> emoteNum;
    recv_data >> guid;

    if (!GetPlayer()->IsAlive() || GetPlayer()->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PREVENT_ANIM))
        return;

    if (!GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(m_muteTime - time(nullptr));
        SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    EmotesTextEntry const* em = sEmotesTextStore.LookupEntry(textEmote);
    if (!em)
        return;

    uint32 emoteId = em->textid;

    switch (emoteId)
    {
        case EMOTE_STATE_SLEEP:
        case EMOTE_STATE_SIT:
        case EMOTE_STATE_KNEEL:
        case EMOTE_ONESHOT_NONE:
            break;
        default:
        {
            GetPlayer()->InterruptSpellsAndAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ANIM_CANCELS);
            GetPlayer()->HandleEmote(emoteId);
            break;
        }
    }

    Unit* unit = GetPlayer()->GetMap()->GetUnit(guid);

    MaNGOS::EmoteChatBuilder emote_builder(*GetPlayer(), textEmote, emoteNum, unit);
    MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > emote_do(emote_builder);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > > emote_worker(GetPlayer(), sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), emote_do);
    Cell::VisitWorldObjects(GetPlayer(), emote_worker, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));

    // Send scripted event call | 通知目标单位的脚本 AI 响应表情交互
    if (unit && unit->AI())
        unit->AI()->ReceiveEmote(GetPlayer(), textEmote);
}

/// 处理玩家屏蔽对方聊天通知封包 (CMSG_CHAT_IGNORED)
void WorldSession::HandleChatIgnoredOpcode(WorldPacket& recv_data)
{
    ObjectGuid iguid;
    uint8 unk;
    // DEBUG_LOG("WORLD: Received opcode CMSG_CHAT_IGNORED");

    recv_data >> iguid;
    recv_data >> unk;                                       // probably related to spam reporting | 疑似与垃圾信息举报相关

    Player* player = sObjectMgr.GetPlayer(iguid);
    if (!player || !player->GetSession())
        return;

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_IGNORED, _player->GetName(), LANG_UNIVERSAL, CHAT_TAG_NONE, _player->GetObjectGuid());
    player->GetSession()->SendPacket(data);
}

/// 向客户端发送“未找到玩家”提示封包
void WorldSession::SendPlayerNotFoundNotice(const std::string& name) const
{
    WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, name.size() + 1);
    data << name;
    SendPacket(data);
}

/// 向客户端发送“敌对阵营无法密语”提示封包
void WorldSession::SendWrongFactionNotice() const
{
    WorldPacket data(SMSG_CHAT_WRONG_FACTION, 0);
    SendPacket(data);
}

/// 向客户端发送聊天受限提示封包
void WorldSession::SendChatRestrictedNotice(ChatRestrictionType restriction) const
{
    WorldPacket data(SMSG_CHAT_RESTRICTED, 1);
    data << uint8(restriction);
    SendPacket(data);
}
