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

#include "Entities/Player.h"
#include "Tools/Language.h"
#include "Database/DatabaseEnv.h"
#include "Log/Log.h"
#include "Server/Opcodes.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "World/World.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "UpdateMask.h"
#include "Skills/SkillDiscovery.h"
#include "Quests/QuestDef.h"
#include "Entities/GossipDef.h"
#include "Entities/UpdateData.h"
#include "Chat/Channel.h"
#include "Chat/ChannelMgr.h"
#include "Maps/MapManager.h"
#include "Maps/MapPersistentStateMgr.h"
#include "Maps/InstanceData.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"
#include "Globals/ObjectMgr.h"
#include "Globals/ObjectAccessor.h"
#include "Tools/Formulas.h"
#include "Groups/Group.h"
#include "Guilds/Guild.h"
#include "GMTickets/GMTicketMgr.h"
#include "AuctionHouse/AuctionHouseMgr.h"
#include "Guilds/GuildMgr.h"
#include "Entities/Pet.h"
#include "Util/Util.h"
#include "Entities/Transports.h"
#include "Weather/Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Arena/ArenaTeam.h"
#include "Chat/Chat.h"
#include "Database/DatabaseImpl.h"
#include "Spells/Spell.h"
#include "DBScripts/ScriptMgr.h"
#include "Social/SocialMgr.h"
#include "Mails/Mail.h"
#include "Server/DBCStores.h"
#include "Server/SQLStorages.h"
#include "Loot/LootMgr.h"
#include "World/WorldStateDefines.h"
#include "World/WorldState.h"
#include "Anticheat/Anticheat.hpp"

#ifdef ENABLE_PLAYERBOTS
#include "playerbot/playerbot.h"
#include "playerbot/PlayerbotAIConfig.h"
#endif

#include <algorithm>
#include <cmath>

#define ZONE_UPDATE_INTERVAL (1*IN_MILLISECONDS)

#define PLAYER_SKILL_INDEX(x)       (PLAYER_SKILL_INFO_1_1 + ((x)*3))
#define PLAYER_SKILL_VALUE_INDEX(x) (PLAYER_SKILL_INDEX(x)+1)
#define PLAYER_SKILL_BONUS_INDEX(x) (PLAYER_SKILL_INDEX(x)+2)

#define SKILL_VALUE(x)         PAIR32_LOPART(x)
#define SKILL_MAX(x)           PAIR32_HIPART(x)
#define MAKE_SKILL_VALUE(v, m) MAKE_PAIR32(v,m)

#define SKILL_TEMP_BONUS(x)    int16(PAIR32_LOPART(x))
#define SKILL_PERM_BONUS(x)    int16(PAIR32_HIPART(x))
#define MAKE_SKILL_BONUS(t, p) MAKE_PAIR32(t,p)

enum CharacterFlags
{
    CHARACTER_FLAG_NONE                 = 0x00000000,
    CHARACTER_FLAG_UNK1                 = 0x00000001,
    CHARACTER_FLAG_UNK2                 = 0x00000002,
    CHARACTER_LOCKED_FOR_TRANSFER       = 0x00000004,
    CHARACTER_FLAG_UNK4                 = 0x00000008,
    CHARACTER_FLAG_UNK5                 = 0x00000010,
    CHARACTER_FLAG_UNK6                 = 0x00000020,
    CHARACTER_FLAG_UNK7                 = 0x00000040,
    CHARACTER_FLAG_UNK8                 = 0x00000080,
    CHARACTER_FLAG_UNK9                 = 0x00000100,
    CHARACTER_FLAG_UNK10                = 0x00000200,
    CHARACTER_FLAG_HIDE_HELM            = 0x00000400,
    CHARACTER_FLAG_HIDE_CLOAK           = 0x00000800,
    CHARACTER_FLAG_UNK13                = 0x00001000,
    CHARACTER_FLAG_GHOST                = 0x00002000,
    CHARACTER_FLAG_RENAME               = 0x00004000,
    CHARACTER_FLAG_UNK16                = 0x00008000,
    CHARACTER_FLAG_UNK17                = 0x00010000,
    CHARACTER_FLAG_UNK18                = 0x00020000,
    CHARACTER_FLAG_UNK19                = 0x00040000,
    CHARACTER_FLAG_UNK20                = 0x00080000,
    CHARACTER_FLAG_UNK21                = 0x00100000,
    CHARACTER_FLAG_UNK22                = 0x00200000,
    CHARACTER_FLAG_UNK23                = 0x00400000,
    CHARACTER_FLAG_UNK24                = 0x00800000,
    CHARACTER_FLAG_LOCKED_BY_BILLING    = 0x01000000,
    CHARACTER_FLAG_DECLINED             = 0x02000000,
    CHARACTER_FLAG_UNK27                = 0x04000000,
    CHARACTER_FLAG_UNK28                = 0x08000000,
    CHARACTER_FLAG_UNK29                = 0x10000000,
    CHARACTER_FLAG_UNK30                = 0x20000000,
    CHARACTER_FLAG_UNK31                = 0x40000000,
    CHARACTER_FLAG_UNK32                = 0x80000000
};

// corpse reclaim times (尸体复活/拾取延迟时间表，单位：秒)
#define DEATH_EXPIRE_STEP (5*MINUTE)
#define MAX_DEATH_COUNT 3

static const uint32 corpseReclaimDelay[MAX_DEATH_COUNT] = {30, 60, 120};

/// 拉取并消费当前镜像计时器的待发送状态更新
MirrorTimer::Status MirrorTimer::FetchStatus()
{
    Status status = m_status;
    m_status = UNCHANGED;
    return status;
}

/// 停止镜像计时器并重置状态
void MirrorTimer::Stop()
{
    if (m_active)
    {
        m_active = false;
        m_pulse.SetCurrent(0);
        m_tracker.SetCurrent(0);
        m_status = STATUS_UPDATE;
    }
}

/// 启动镜像计时器（指定总时长与触发法术 ID）
void MirrorTimer::Start(uint32 interval, uint32 spellId/* = 0*/)
{
    if (m_scale < 0)
    {
        m_active = true;
        m_pulse.SetCurrent(0);
        m_pulse.SetInterval(2 * IN_MILLISECONDS);
        m_tracker.SetCurrent(0);
        m_tracker.SetInterval(interval);
        m_spellId = spellId;
        m_status = FULL_UPDATE;
    }
    else
        Stop();
}

void MirrorTimer::Start(uint32 current, uint32 max, uint32 spellId)
{
    Start(max, spellId);

    if (m_active)
    {
        m_tracker.SetCurrent(max - current);
        SetFrozen(false);
    }
}

void MirrorTimer::SetRemaining(uint32 duration)
{
    if (!duration)
        return Stop();

    if (IsActive() && duration != GetRemaining())
        m_status = FULL_UPDATE;

    m_tracker.SetCurrent(GetDuration() - duration);
}

void MirrorTimer::SetDuration(uint32 duration)
{
    if (!duration)
        return Stop();

    if (IsActive() && duration != GetDuration())
        m_status = FULL_UPDATE;

    m_tracker.SetInterval(duration);
}

void MirrorTimer::SetFrozen(bool state)
{
    if (IsActive() && state != IsFrozen())
        m_status = STATUS_UPDATE;

    m_frozen = state;
}

void MirrorTimer::SetScale(int32 scale)
{
    if (!scale)
        return SetFrozen(true);

    if (IsActive() && scale != m_scale)
        m_status = FULL_UPDATE;

    m_scale = scale;
}

/// 每帧推进镜像计时器并触发到期周期伤害/恢复脉冲
bool MirrorTimer::Update(uint32 diff)
{
    if (!IsActive() || IsFrozen())
        return true;

    diff *= uint32(std::abs(m_scale));

    if (m_scale < 0)    // Timer running out (计时器递减消耗中)
    {
        m_tracker.Update(diff);

        if (!m_tracker.Passed())
            return true;

        const uint32 interval = m_tracker.GetInterval();
        const uint32 overflow = (m_tracker.GetCurrent() - interval);

        m_tracker.SetCurrent(interval);

        if (overflow == diff)   // Pulse: subsequent ticks after instant tick on expiration (到期立即触发首次效果后的后续周期脉冲)
        {
            m_pulse.Update(overflow);

            if (!m_pulse.Passed())
                return true;

            m_pulse.Reset();
        }

        return false;
    }
    else                // Timer regenerating (计时器恢复中)
    {
        const uint32 current = m_tracker.GetCurrent();

        if (current > diff)
        {
            m_tracker.SetCurrent(current - diff);
            m_pulse.SetCurrent(0);
        }
        else
            Stop();

        return true;
    }
}

//== PlayerTaxi (玩家飞行路线管理器) ====================================

/// 构造函数：清空飞行点位图掩码
PlayerTaxi::PlayerTaxi()
{
    // Taxi nodes (飞行点掩码初始化)
    memset(m_taximask, 0, sizeof(m_taximask));
}

/// 根据玩家种族和等级初始化默认已知飞行节点
void PlayerTaxi::InitTaxiNodesForLevel(uint32 race, uint32 /*level*/)
{
    // race specific initial known nodes: capital and taxi hub masks (各种族初始已知飞行点：主城与核心交通枢纽掩码)
    switch (race)
    {
        case RACE_HUMAN:    SetTaximaskNode(2);  break;     // Human (人类)
        case RACE_ORC:      SetTaximaskNode(23); break;     // Orc (兽人)
        case RACE_DWARF:    SetTaximaskNode(6);  break;     // Dwarf (矮人)
        case RACE_NIGHTELF: SetTaximaskNode(26);
            SetTaximaskNode(27); break;     // Night Elf (暗夜精灵)
        case RACE_UNDEAD:   SetTaximaskNode(11); break;     // Undead (亡灵)
        case RACE_TAUREN:   SetTaximaskNode(22); break;     // Tauren (牛头人)
        case RACE_GNOME:    SetTaximaskNode(6);  break;     // Gnome (侏儒)
        case RACE_TROLL:    SetTaximaskNode(23); break;     // Troll (巨魔)
        case RACE_BLOODELF: SetTaximaskNode(82); break;     // Blood Elf (血精灵)
        case RACE_DRAENEI:  SetTaximaskNode(94); break;     // Draenei (德莱尼)
    }

    // new continent starting masks (It will be accessible only at new map) (新大陆初始飞行点掩码，仅在新地图可访问)
    switch (Player::TeamForRace(race))
    {
        case ALLIANCE: SetTaximaskNode(100); break;
        case HORDE:    SetTaximaskNode(99);  break;
        default: break;
    }
    // level dependent taxi hubs (随等级解锁的飞行枢纽)

#ifndef PRENERF_2_4
    SetTaximaskNode(213);                                   // Shattered Sun Staging Area (破碎残阳集结地/奎尔丹纳斯岛)
#endif
}

/// 从数据库字符串加载已解锁飞行节点位图
void PlayerTaxi::LoadTaxiMask(const char* data)
{
    Tokens tokens = StrSplit(data, " ");

    int index;
    Tokens::iterator iter;
    for (iter = tokens.begin(), index = 0;
            (index < TaxiMaskSize) && (iter != tokens.end()); ++iter, ++index)
    {
        // load and set bits only for existing taxi nodes (仅加载并设置数据库中存在的飞行节点位)
        m_taximask[index] = sTaxiNodesMask[index] & uint32(std::stoul(*iter));
    }
}

/// 将已知（或全部）飞行节点位图写入数据包发送给客户端
void PlayerTaxi::AppendTaximaskTo(ByteBuffer& data, bool all)
{
    if (all)
    {
        for (unsigned int i : sTaxiNodesMask)
            data << uint32(i);              // all existing nodes (所有存在的飞行节点)
    }
    else
    {
        for (unsigned int i : m_taximask)
            data << uint32(i);                  // known nodes (玩家已知的飞行节点)
    }
}

std::ostringstream& operator<<(std::ostringstream& ss, PlayerTaxi const& taxi)
{
    for (unsigned int i : taxi.m_taximask)
        ss << i << " ";
    return ss;
}

uint64 SpellModifier::modCounter = 0;

SpellModifier::SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, SpellEntry const* spellEntry, SpellEffectIndex eff, int32 _priority, int16 _charges /*= 0*/) : op(_op), type(_type), charges(_charges), value(_value), spellId(spellEntry->Id), priority(_priority), isFinite(_charges > 0), modId(GetModId())
{
    mask = sSpellMgr.GetSpellAffectMask(spellEntry->Id, eff);
}

SpellModifier::SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, Aura const* aura, int32 _priority, int16 _charges /*= 0*/) : op(_op), type(_type), charges(_charges), value(_value), spellId(aura->GetId()), priority(_priority), isFinite(_charges > 0), modId(GetModId())
{
    mask = sSpellMgr.GetSpellAffectMask(aura->GetId(), aura->GetEffIndex());
}

/// 判断法术修正器是否作用于指定法术
bool SpellModifier::isAffectedOnSpell(SpellEntry const* spell) const
{
    SpellEntry const* affect_spell = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    // False if affect_spell == nullptr or spellFamily not equal (若目标法术为空或法术族不匹配则返回 false)
    if (!affect_spell || affect_spell->SpellFamilyName != spell->SpellFamilyName)
        return false;
    return spell->IsFitToFamilyMask(mask);
}

//== TradeData (玩家交易数据) ============================================

TradeData* TradeData::GetTraderData() const
{
    return m_trader->GetTradeData();
}

Item* TradeData::GetItem(TradeSlots slot) const
{
    return m_items[slot] ? m_player->GetItemByGuid(m_items[slot]) : nullptr;
}

bool TradeData::HasItem(ObjectGuid item_guid) const
{
    for (auto m_item : m_items)
        if (m_item == item_guid)
            return true;
    return false;
}


Item* TradeData::GetSpellCastItem() const
{
    return m_spellCastItem ?  m_player->GetItemByGuid(m_spellCastItem) : nullptr;
}

/// 设置交易栏指定槽位的物品并同步更新双方状态
void TradeData::SetItem(TradeSlots slot, Item* item)
{
    ObjectGuid itemGuid = item ? item->GetObjectGuid() : ObjectGuid();

    if (m_items[slot] == itemGuid)
    {
        Update(false);                                      // the client drops the updates of its offer sent before this request (客户端会丢弃在此请求之前发送的己方报价更新)
        return;
    }

    m_items[slot] = itemGuid;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    ++m_serverStateIndex;
    Update(true);
    Update(false);

    // need remove possible trader spell applied to changed item (需要移除交易对手此前施加在变更物品上的附魔/开锁法术)
    if (slot == TRADE_SLOT_NONTRADED)
        GetTraderData()->SetSpell(0);

    // need remove possible player spell applied (possible move reagent) (需要移除己方施加的法术，以防移动了施法材料物品)
    SetSpell(0);
}

/// 设置交易附魔/开锁槽位的法术与施法物品
void TradeData::SetSpell(uint32 spell_id, Item* castItem /*= nullptr*/)
{
    ObjectGuid itemGuid = castItem ? castItem->GetObjectGuid() : ObjectGuid();

    if (m_spell == spell_id && m_spellCastItem == itemGuid)
        return;

    m_spell = spell_id;
    m_spellCastItem = itemGuid;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    ++m_serverStateIndex;
    Update(true);                                           // send spell info to item owner (向物品拥有者发送附魔/法术信息)
    Update(false);                                          // send spell info to caster self (向施法者自身发送附魔/法术信息)
}

/// 设置交易金币数额并刷新交易状态
void TradeData::SetMoney(uint32 money)
{
    if (m_money == money)
    {
        Update(false);                                      // the client drops the updates of its offer sent before this request (客户端会丢弃在此请求之前发送的己方报价更新)
        return;
    }

    if (money > m_player->GetMoney())
    {
        TradeStatusInfo info;
        info.Status = TRADE_STATUS_CLOSE_WINDOW;
        info.Result = EQUIP_ERR_NOT_ENOUGH_MONEY;
        m_player->GetSession()->SendTradeStatus(info);
        return;
    }

    m_money = money;

    SetAccepted(false);
    GetTraderData()->SetAccepted(false);

    ++m_serverStateIndex;
    Update(true);
    Update(false);
}

/// 向交易双方发送当前交易栏状态数据包
void TradeData::Update(bool for_trader /*= true*/) const
{
    if (for_trader)
        m_trader->GetSession()->SendUpdateTrade(true);      // player state for trader (向交易对方发送己方交易状态)
    else
        m_player->GetSession()->SendUpdateTrade(false);     // player state for player (向己方发送己方交易状态)
}

/// 设置交易确认状态，并在双方均确认时完成物品与金币交换
void TradeData::SetAccepted(bool state, bool crosssend /*= false*/)
{
    m_accepted = state;

    if (!state)
    {
        TradeStatusInfo info;
        info.Status = TRADE_STATUS_BACK_TO_TRADE;
        if (crosssend)
            m_trader->GetSession()->SendTradeStatus(info);
        else
            m_player->GetSession()->SendTradeStatus(info);
    }
}

//== Player (玩家核心实现) ================================================

/// Player 构造函数：初始化玩家实体各子系统默认值与计时器
Player::Player(WorldSession* session): Unit(), m_taxiTracker(*this), m_mover(this), m_pendingClientControlChange(false), m_camera(this), m_reputationMgr(this), m_launched(false)
{
#ifdef ENABLE_PLAYERBOTS
    m_playerbotAI = nullptr;
    m_playerbotMgr = nullptr;
#endif
    m_speakTime = 0;
    m_speakCount = 0;

    m_objectType |= TYPEMASK_PLAYER;
    m_objectTypeId = TYPEID_PLAYER;

    m_valuesCount = PLAYER_END;

    SetActiveObjectState(true);                             // player is always active object (玩家始终作为地图中的活跃对象)

    m_session = session;

    m_ExtraFlags = 0;
    if (GetSession()->GetSecurity() >= SEC_GAMEMASTER)
        SetAcceptTicket(true);

    // players always accept (玩家默认始终接受请求)
    if (GetSession()->GetSecurity() == SEC_PLAYER)
        SetAcceptWhispers(true);

    m_usedTalentCount = 0;

    m_zoneUpdateId = 0;
    m_zoneUpdateTimer = 0;
    m_positionStatusUpdateTimer = 0;

    m_areaUpdateId = 0;

    m_nextSave = sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE);

    // randomize first save time in range [CONFIG_UINT32_INTERVAL_SAVE] around [CONFIG_UINT32_INTERVAL_SAVE] (在保存间隔配置范围内随机化首次自动保存时间)
    // this must help in case next save after mass player load after server startup (用于错开开服后大量玩家同时登录导致的集中存盘峰值)
    m_nextSave = urand(m_nextSave / 2, m_nextSave * 3 / 2);

    clearResurrectRequestData();

    m_SpellModRemoveCount = 0;

    memset(m_items, 0, sizeof(Item*)*PLAYER_SLOTS_COUNT);

    m_social = nullptr;

    // group is initialized in the reference constructor (队伍引用在成员引用构造函数中初始化)
    SetGroupInvite(nullptr);
    m_groupUpdateMask = 0;

    duel = nullptr;

    m_GuildIdInvited = 0;
    m_ArenaTeamIdInvited = 0;

    m_atLoginFlags = AT_LOGIN_NONE;

    m_semaphoreTeleport_Near = false;
    m_semaphoreTeleport_Far = false;

    m_DelayedOperations = 0;
    m_bCanDelayTeleport = false;
    m_bHasDelayedTeleport = false;
    m_bHasBeenAliveAtDelayedTeleport = true;                // overwrite always at setup teleport data, so not used infact (设置传送数据时总是会被覆盖，实际未直接使用)
    m_teleport_options = 0;

    m_needsZoneUpdate = false;

    m_trade = nullptr;

    m_cinematic = 0;

    m_playerMenu =  std::make_unique<PlayerMenu>(GetSession());
    m_currentBuybackSlot = BUYBACK_SLOT_START;

    m_DailyQuestChanged = false;
    m_WeeklyQuestChanged = false;
    m_MonthlyQuestChanged = false;

    m_lastLiquid = nullptr;

    m_drunkTimer = 0;
    m_drunk = 0;
    m_restTime = 0;
    m_deathTimer = 0;
    m_deathExpireTime = 0;

    m_swingErrorMsg = SWING_ERROR_NONE;

    for (auto& j : m_bgBattleGroundQueueID)
    {
        j.bgQueueTypeId  = BATTLEGROUND_QUEUE_NONE;
        j.invitedToInstance = 0;
    }

    m_logintime = time(nullptr);
    m_Last_tick = m_logintime;
    m_WeaponProficiency = 0;
    m_ArmorProficiency = 0;
    m_canParry = false;
    m_canBlock = false;
    m_ammoDPSMin = 0.0f;
    m_ammoDPSMax = 0.0f;
    m_highestAmmoMod = 0;

    m_temporaryUnsummonedPetNumber = 0;
    m_BGPetSpell = 0;

    //////////////////// Rest System (休息系统) /////////////////////
    time_inn_enter = 0;
    inn_trigger_id = 0;
    m_rest_bonus = 0;
    m_restType = REST_TYPE_NO;
    //////////////////// Rest System (休息系统) /////////////////////

    m_mailsUpdated = false;
    unReadMails = 0;
    m_nextMailDelivereTime = 0;

    m_resetTalentsCost = 0;
    m_resetTalentsTime = 0;
    m_itemUpdateQueueBlocked = false;

    for (unsigned char& m_forced_speed_change : m_forced_speed_changes)
        m_forced_speed_change = 0;

    m_stableSlots = 0;

    /////////////////// Instance System (副本系统) ////////////////////

    m_HomebindTimer = 0;
    m_InstanceValid = true;
    m_dungeonDifficulty = DUNGEON_DIFFICULTY_NORMAL;

    for (auto& i : m_auraBaseMod)
    {
        i[FLAT_MOD] = 0.0f;
        i[PCT_MOD] = 1.0f;
    }

    for (short& i : m_baseRatingValue)
        i = 0;

    for (unsigned int& i : m_enchantmentFlatMod)
        i = 0;

    // Honor System (荣誉系统)
    m_lastHonorUpdateTime = time(nullptr);

    // Player summoning (玩家召唤请求)
    m_summon_expire = 0;
    m_summon_mapid = 0;
    m_summon_x = 0.0f;
    m_summon_y = 0.0f;
    m_summon_z = 0.0f;

    m_declinedname = nullptr;

    m_lastFallTime = 0;
    m_lastFallZ = 0;

    m_createdInstanceClearTimer = MINUTE * IN_MILLISECONDS;

    m_cinematicMgr = nullptr;

    m_energyRegenRate = 1.f;

    m_grantableLevels = 0;

    m_consumedMods = nullptr;
    m_modsSpell = nullptr;

    m_isDebuggingAreaTriggers = false;

    m_fishingSteps = 0;

    m_lastDbGuid = 0;
    m_lastGameObject = false;
}

/// Player 析构函数：释放邮件、社交及背包等堆内存对象
Player::~Player()
{
    CleanupsBeforeDelete();

    // it must be unloaded already in PlayerLogout and accessed only for loggined player (社交数据须在登出时卸载，且仅在玩家在线期间访问)
    // m_social = nullptr;

    // Note: buy back item already deleted from DB when player was saved (注：回购栏物品在角色保存时已从数据库删除)
    for (auto& m_item : m_items)
    {
        delete m_item;
    }
    CleanupChannels();

    // all mailed items should be deleted, also all mail should be deallocated (释放所有邮件附件物品及邮件对象内存)
    for (PlayerMails::const_iterator itr =  m_mail.begin(); itr != m_mail.end(); ++itr)
        delete *itr;

    for (ItemMap::const_iterator iter = mMitems.begin(); iter != mMitems.end(); ++iter)
        delete iter->second;                                // if item is duplicated... then server may crash ... but that item should be deallocated (若同一物品指针被重复放入槽位可能导致崩溃，此处确保不重复释放)

    if (m_transport)
    {
        m_transport->RemovePassenger(this);
    }

    // clean up player-instance binds, may unload some instance saves (清理玩家副本进度绑定，可能会卸载部分副本持久化状态)
    for (auto& m_boundInstance : m_boundInstances)
        for (BoundInstancesMap::iterator itr = m_boundInstance.begin(); itr != m_boundInstance.end(); ++itr)
            itr->second.state->RemovePlayer(this);

#ifdef ENABLE_PLAYERBOTS
    RemovePlayerbotAI();
    RemovePlayerbotMgr();
#endif

    delete m_declinedname;
}

/// 销毁前清理：解除世界引用、卸载社交数据并通知区域脚本
void Player::CleanupsBeforeDelete()
{
    if (m_uint32Values)                                     // only for fully created Object (仅对已完整创建的实体执行清理)
    {
        TradeCancel(false);
        DuelComplete(DUEL_FLED);
    }

    // notify zone scripts for player logout (通知区域脚本玩家已登出)
    sOutdoorPvPMgr.HandlePlayerLeaveZone(this, m_zoneUpdateId);
    sWorldState.HandlePlayerLeaveZone(this, m_zoneUpdateId);
    sWorldState.HandlePlayerLeaveArea(this, m_areaUpdateId);

    Unit::CleanupsBeforeDelete();
}

/// 校验角色外观组合（种族、性别、发型、脸型、肤色等）在 DBC 中的合法性
bool Player::ValidateAppearance(uint8 race, uint8 /*class_*/, uint8 gender, uint8 hairID, uint8 hairColor, uint8 faceID, uint8 facialHair, uint8 skinColor, bool /*create = false*/)
{
    // For Skin type is always 0 (肤色在 CharSections 中类型始终为 0)
    CharSectionsEntry const* skinEntry = GetCharSectionEntry(race, SECTION_TYPE_SKIN, gender, 0, skinColor);
    if (!skinEntry)
        return false;

    // Skin Color defined as Face color, too (脸型颜色与肤色共用编号)
    CharSectionsEntry const* faceEntry = GetCharSectionEntry(race, SECTION_TYPE_FACE, gender, faceID, skinColor);
    if (!faceEntry)
        return false;

    // Check Hair (校验发型与发色)
    CharSectionsEntry const* hairEntry = GetCharSectionEntry(race, SECTION_TYPE_HAIR, gender, hairID, hairColor);
    if (!hairEntry)
        return false;

    // TODO:: fill missing case in db when dbc will be in database (待办：DBC 入库后补全缺失的面部饰物校验)
    //// These combinations don't have an entry of Type SECTION_TYPE_FACIAL_HAIR, exclude them from that check (这些种族/性别组合没有面部饰物 DBC 条目，跳过校验)
    //bool const excludeCheck = (race == RACE_TAUREN) || (gender == GENDER_FEMALE && race != RACE_NIGHTELF && race != RACE_UNDEAD);
    //if (!excludeCheck)
    //{
    //    CharSectionsEntry const* facialHairEntry = GetCharSectionEntry(race, SECTION_TYPE_FACIAL_HAIR, gender, facialHair, hairColor);
    //    if (!facialHairEntry)
    //        return false;
    //}

    CharacterFacialHairStylesEntry const* entry = GetCharFacialHairEntry(race, gender, facialHair);
    if (!entry)
        return false;

    return true;
}

/// 创建新角色实体：初始化外观字节、基础属性、初始法术、动作条与初始装备
bool Player::Create(uint32 guidlow, const std::string& name, uint8 race, uint8 class_, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle, uint8 hairColor, uint8 facialHair, uint8 /*outfitId */)
{
    // FIXME: outfitId not used in player creating (待修复：创建角色时未使用 outfitId)

    Object::_Create(guidlow, guidlow, 0, HIGHGUID_PLAYER);

    m_name = name;

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(race, class_);
    if (!info)
    {
        sLog.outError("Player have incorrect race/class pair. Can't be loaded.");
        return false;
    }

    ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(class_);
    if (!cEntry)
    {
        sLog.outError("Class %u not found in DBC (Wrong DBC files?)", class_);
        return false;
    }

    // player store gender in single bit (玩家性别存储在单个比特位中)
    if (gender != uint8(GENDER_MALE) && gender != uint8(GENDER_FEMALE))
    {
        sLog.outError("Invalid gender %u at player creating", uint32(gender));
        return false;
    }

    for (auto& m_item : m_items)
        m_item = nullptr;

    SetLocationMapId(info->mapId);
    Relocate(info->positionX, info->positionY, info->positionZ, info->orientation);

    SetMap(sMapMgr.CreateMap(info->mapId, this));

    uint8 powertype = cEntry->powerType;

    setFactionForRace(race);

    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_RACE, race);
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_CLASS, class_);
    SetSpellClass(class_);
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_GENDER, gender);
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_POWER_TYPE, powertype);

    InitDisplayIds();                                       // model, scale and model data (模型 ID、缩放比例与模型信息)

    SetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_DEBUFF_LIMIT, UNIT_BYTE2_PLAYER_CONTROLLED_DEBUFF_LIMIT);

    SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);               // fix cast time showed in spell tooltip on client (修正客户端法术提示中显示的施法时间倍率)

    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, -1);  // -1 is default value (-1 为默认无效值)

    SetByteValue(PLAYER_BYTES, 0, skin);
    SetByteValue(PLAYER_BYTES, 1, face);
    SetByteValue(PLAYER_BYTES, 2, hairStyle);
    SetByteValue(PLAYER_BYTES, 3, hairColor);

    SetByteValue(PLAYER_BYTES_2, 0, facialHair);
    SetByteValue(PLAYER_BYTES_2, 3, REST_STATE_NORMAL);

    SetUInt16Value(PLAYER_BYTES_3, 0, gender);              // only GENDER_MALE/GENDER_FEMALE (1 bit) allowed, drunk state = 0 (仅允许男/女 1 位性别值，初始醉酒度为 0)
    SetByteValue(PLAYER_BYTES_3, 3, 0);                     // BattlefieldArenaFaction (0 or 1) (战场/竞技场阵营标志 0 或 1)

    SetUInt32Value(PLAYER_GUILDID, 0);
    SetUInt32Value(PLAYER_GUILDRANK, 0);
    SetUInt32Value(PLAYER_GUILD_TIMESTAMP, 0);

    SetUInt64Value(PLAYER__FIELD_KNOWN_TITLES, 0);          // 0=disabled (0 表示禁用动作条扩展栏)
    SetUInt32Value(PLAYER_CHOSEN_TITLE, 0);

    SetUInt32Value(PLAYER_FIELD_KILLS, 0);
    SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, 0);
    SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, 0);
    SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, 0);

    // set starting level (设置初始等级)
    if (GetSession()->GetSecurity() >= SEC_MODERATOR)
        SetUInt32Value(UNIT_FIELD_LEVEL, sWorld.getConfig(CONFIG_UINT32_START_GM_LEVEL));
    else
        SetUInt32Value(UNIT_FIELD_LEVEL, sWorld.getConfig(CONFIG_UINT32_START_PLAYER_LEVEL));

    SetUInt32Value(PLAYER_FIELD_COINAGE, sWorld.getConfig(CONFIG_UINT32_START_PLAYER_MONEY));
    SetHonorPoints(sWorld.getConfig(CONFIG_UINT32_START_HONOR_POINTS));
    SetArenaPoints(sWorld.getConfig(CONFIG_UINT32_START_ARENA_POINTS));

    // Played time (游戏时间统计初始化)
    m_Last_tick = time(nullptr);
    m_Played_time[PLAYED_TIME_TOTAL] = 0;
    m_Played_time[PLAYED_TIME_LEVEL] = 0;

    // base stats and related field values (初始化基础属性及相关字段)
    InitStatsForLevel();
    InitTaxiNodesForLevel();
    InitTalentForLevel();
    InitPrimaryProfessions();                               // to max set before any spell added (在添加任何法术前先将生命/法力恢复至上限)

    // apply original stats mods before spell loading or item equipment that call before equip _RemoveStatsMods() (在加载法术和装备前先应用初始属性加成)
    UpdateMaxHealth();                                      // Update max Health (for add bonus from stamina) (更新最大生命值以计入耐力加成)
    SetHealth(GetMaxHealth());

    if (GetPowerType() == POWER_MANA)
    {
        UpdateMaxPower(POWER_MANA);                         // Update max Mana (for add bonus from intellect) (更新最大法力值以计入智力加成)
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    }

    LearnDefaultSkills();
    learnDefaultSpells();

    // original action bar (初始化默认动作条按钮)
    for (auto action_itr : info->action)
        addActionButton(action_itr.button, action_itr.action, action_itr.type);

    // original items (发放创建角色初始物品)
    uint32 raceClassGender = GetUInt32Value(UNIT_FIELD_BYTES_0) & 0x00FFFFFF;

    CharStartOutfitEntry const* oEntry = nullptr;
    for (uint32 i = 1; i < sCharStartOutfitStore.GetNumRows(); ++i)
    {
        if (CharStartOutfitEntry const* entry = sCharStartOutfitStore.LookupEntry(i))
        {
            if (entry->RaceClassGender == raceClassGender)
            {
                oEntry = entry;
                break;
            }
        }
    }

    if (oEntry)
    {
        for (int j : oEntry->ItemId)
        {
            if (j <= 0)
                continue;

            uint32 item_id = j;

            // just skip, reported in ObjectMgr::LoadItemPrototypes (跳过无效物品，ObjectMgr::LoadItemPrototypes 已报错)
            ItemPrototype const* iProto = ObjectMgr::GetItemPrototype(item_id);
            if (!iProto)
                continue;

            // BuyCount by default (默认按物品购买堆叠数发放)
            int32 count = iProto->BuyCount;

            // special amount for foor/drink (食物与饮水的特殊初始发放数量)
            if (iProto->Class == ITEM_CLASS_CONSUMABLE && iProto->SubClass == ITEM_SUBCLASS_FOOD)
            {
                switch (iProto->Spells[0].SpellCategory)
                {
                    case 11:                                // food (食物)
                        if (iProto->Stackable > 4)
                            count = 4;
                        break;
                    case 59:                                // drink (饮水)
                        if (iProto->Stackable > 2)
                            count = 2;
                        break;
                }
            }

            StoreNewItemInBestSlots(item_id, count);
        }
    }

    for (auto item_id_itr : info->item)
        StoreNewItemInBestSlots(item_id_itr.item_id, item_id_itr.item_amount);

    // bags and main-hand weapon must equipped at this moment (此时背包和主手武器应已完成装备)
    // now second pass for not equipped (offhand weapon/shield if it attempt equipped before main-hand weapon) (第二轮处理首轮未能装备的副手武器/盾牌)
    // or ammo not equipped in special bag (或未能放入专用箭袋/弹药袋的弹药)
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            uint16 eDest;
            // equip offhand weapon/shield if it attempt equipped before main-hand weapon (补充装备先于主手处理的副手武器/盾牌)
            InventoryResult msg = CanEquipItem(NULL_SLOT, eDest, pItem, false);
            if (msg == EQUIP_ERR_OK)
            {
                RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                EquipItem(eDest, pItem, true);
            }
            // move other items to more appropriate slots (ammo not equipped in special bag) (将其余物品移动至更合适的专用背包槽位)
            else
            {
                ItemPosCountVec sDest;
                uint8 bagSlot = 0;
                msg = CanStoreItem(NULL_BAG, NULL_SLOT, sDest, pItem, bagSlot, false);
                if (msg == EQUIP_ERR_OK)
                {
                    RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                    pItem = StoreItem(sDest, pItem, true);
                }

                // if  this is ammo then use it (若为弹药则自动设为当前使用弹药)
                msg = CanUseAmmo(pItem->GetEntry());
                if (msg == EQUIP_ERR_OK)
                    SetAmmo(pItem->GetEntry());
            }
        }
    }
    // all item positions resolved (所有初始物品位置均已妥善放置)

    return true;
}

bool Player::StoreNewItemInBestSlots(uint32 titem_id, uint32 titem_amount)
{
    DEBUG_LOG("STORAGE: Creating initial item, itemId = %u, count = %u", titem_id, titem_amount);

    // attempt equip by one (尝试逐个直接装备初始物品)
    while (titem_amount > 0)
    {
        uint16 eDest;
        uint8 msg = CanEquipNewItem(NULL_SLOT, eDest, titem_id, false);
        if (msg != EQUIP_ERR_OK)
            break;

        EquipNewItem(eDest, titem_id, true);
        AutoUnequipOffhandIfNeed();
        --titem_amount;
    }

    if (titem_amount == 0)
        return true;                                        // equipped (已成功装备)

    // attempt store (尝试放入行囊存储)
    ItemPosCountVec sDest;
    // store in main bag to simplify second pass (special bags can be not equipped yet at this moment) (先存入主行囊以便第二轮整理，此时专用包可能尚未装备)
    uint8 msg = CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, sDest, titem_id, titem_amount);
    if (msg == EQUIP_ERR_OK)
    {
        StoreNewItem(sDest, titem_id, true, Item::GenerateItemRandomPropertyId(titem_id));
        return true;                                        // stored (已存入背包)
    }

    // item can't be added (无法添加该物品)
    sLog.outError("STORAGE: Can't equip or store initial item %u for race %u class %u , error msg = %u", titem_id, getRace(), getClass(), msg);
    return false;
}

// helper function, mainly for script side, but can be used for simple task in mangos also. (辅助函数：主要供脚本侧使用，也可用于核心简单物品发放)
Item* Player::StoreNewItemInInventorySlot(uint32 itemEntry, uint32 amount)
{
    ItemPosCountVec vDest;

    InventoryResult msg = CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, vDest, itemEntry, amount);

    if (msg == EQUIP_ERR_OK)
    {
        if (Item* pItem = StoreNewItem(vDest, itemEntry, true, Item::GenerateItemRandomPropertyId(itemEntry)))
            return pItem;
    }
    else
        SendEquipError(msg, nullptr, nullptr, itemEntry);

    return nullptr;
}

/// 对玩家造成环境伤害（溺水、疲劳、坠落、岩浆、软泥、火焰）并按需扣除装备耐久
uint32 Player::EnvironmentalDamage(EnviromentalDamage type, uint32 damage)
{
    if (!IsAlive() || IsGameMaster())
        return 0;

    // Absorb, resist some environmental damage type (计算护甲/抗性与法术吸收对环境伤害的减免)
    uint32 absorb = 0;
    int32 resist = 0;
    if (type == DAMAGE_LAVA)
    {
        // TODO: Find out if we should sent packet (待办：确认此处是否应发送环境伤害数据包)
        if (IsImmuneToDamage(SPELL_SCHOOL_MASK_FIRE))
            return 0;

        CalculateDamageAbsorbAndResist(this, SPELL_SCHOOL_MASK_FIRE, DIRECT_DAMAGE, damage, &absorb, &resist);
    }
    else if (type == DAMAGE_SLIME)
        CalculateDamageAbsorbAndResist(this, SPELL_SCHOOL_MASK_NATURE, DIRECT_DAMAGE, damage, &absorb, &resist);

    const uint32 bonus = (resist < 0 ? uint32(std::abs(resist)) : 0);
    damage += bonus;
    const uint32 malus = (resist > 0 ? (absorb + uint32(resist)) : absorb);
    damage = (damage <= malus ? 0 : (damage - malus));

    DamageEffectType damageType = SELF_DAMAGE;

    Unit::DealDamageMods(this, this, damage, &absorb, damageType);

    SendEnvironmentalDamageLog(type, damage, absorb, resist);

    uint32 final_damage = Unit::DealDamage(this, this, damage, nullptr, damageType, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);

    if (type == DAMAGE_FALL && !IsAlive())                  // DealDamage not apply item durability loss at self damage (DealDamage 不会对自伤扣除装备耐久)
    {
        DEBUG_LOG("We are fall to death, loosing 10 percents durability");
        DurabilityLossAll(0.10f, false);
        // durability lost message (扣除 10% 装备耐久并发送通知)
        WorldPacket data2(SMSG_DURABILITY_DAMAGE_DEATH, 0);
        GetSession()->SendPacket(data2);
    }

    return final_damage;
}

/// The player sobers by 256 every 10 seconds (玩家每 10 秒自动醒酒 256 点)
void Player::HandleSobering()
{
    m_drunkTimer = 0;

    uint32 drunk = (m_drunk <= 256) ? 0 : (m_drunk - 256);
    SetDrunkValue(drunk);
}

DrunkenState Player::GetDrunkenstateByValue(uint16 value)
{
    if (value >= 23000)
        return DRUNKEN_SMASHED;
    if (value >= 12800)
        return DRUNKEN_DRUNK;
    if (value & 0xFFFE)
        return DRUNKEN_TIPSY;
    return DRUNKEN_SOBER;
}

void Player::SetDrunkValue(uint16 newDrunkenValue, uint32 itemId)
{
    uint32 oldDrunkenState = Player::GetDrunkenstateByValue(m_drunk);

    m_drunk = newDrunkenValue;
    SetUInt16Value(PLAYER_BYTES_3, 0, uint16(getGender()) | (m_drunk & 0xFFFE));

    uint32 newDrunkenState = Player::GetDrunkenstateByValue(m_drunk);

    // special drunk invisibility detection (根据醉酒程度更新特殊的隐形侦测光环)
    if (newDrunkenState >= DRUNKEN_DRUNK)
        GetVisibilityData().SetInvisibilityDetectMask(INVISIBILITY_DRUNK, true);
    else
        GetVisibilityData().SetInvisibilityDetectMask(INVISIBILITY_DRUNK, false);

    GetVisibilityData().SetInvisibilityValue(6, m_drunk);

    if (newDrunkenState == oldDrunkenState)
        return;

    WorldPacket data(SMSG_CROSSED_INEBRIATION_THRESHOLD, (8 + 4 + 4));
    data << GetObjectGuid();
    data << uint32(newDrunkenState);
    data << uint32(itemId);

    SendMessageToSet(data, true);
}

uint32 Player::GetWaterBreathingInterval() const
{
    return uint32(sWorld.getConfig(CONFIG_UINT32_MIRRORTIMER_BREATH_MAX) * IN_MILLISECONDS * m_environmentBreathingMultiplier);
}

void Player::SetWaterBreathingIntervalMultiplier(float multiplier)
{
    m_environmentBreathingMultiplier = multiplier;

    if (const uint32 interval = GetWaterBreathingInterval())
    {
        m_mirrorTimers[MirrorTimer::BREATH].SetDuration(interval);
        m_mirrorTimers[MirrorTimer::BREATH].SetScale(IsUnderwater() ? -1 : 10);
    }
    else
        m_mirrorTimers[MirrorTimer::BREATH].SetScale(10);
}

/// 设置或清除环境液体状态标志并同步更新仇恨与镜像计时器
void Player::SetEnvironmentFlags(EnvironmentFlags flags, bool apply)
{
    if (bool(m_environmentFlags & flags) == apply)
        return;

    if (apply)
        m_environmentFlags |= flags;
    else
        m_environmentFlags &= ~flags;

    // On liquid in/out (进出液体区域状态变更处理)
    if (flags & ENVIRONMENT_MASK_IN_LIQUID)
    {
        // move player's guid into HateOfflineList of those mobs (将无法游泳的怪物仇恨列表中的玩家移入离线仇恨列表，)
        // which can't swim and move guid back into ThreatList when (待玩家返回水面或陆地时再移回)
        // on surface. (返回水面时恢复)
        // TODO: exist also swimming mobs, and function must be symmetric to enter/leave water (待办：游泳怪物与进出水域处理应对称)
        getHostileRefManager().updateThreatTables();
    }

    // Remove auras that need land or water (移除仅限陆地或仅限水中生效的光环)
    if (flags & ENVIRONMENT_FLAG_HIGH_LIQUID)
        RemoveAurasWithInterruptFlags(apply ?  AURA_INTERRUPT_FLAG_UNDERWATER_CANCELS : AURA_INTERRUPT_FLAG_ABOVEWATER_CANCELS);

    // On moving in/out high sea area: affect fatigue timer (进出深海区域：更新疲劳计时器)
    if (flags & ENVIRONMENT_FLAG_HIGH_SEA)
        m_mirrorTimers[MirrorTimer::FATIGUE].SetScale(apply ? -1 : 10);

    // On swimming down/up liquid surface level: affect breath timer (潜入/浮出液面：更新水下呼吸计时器)
    if (flags & ENVIRONMENT_FLAG_UNDERWATER)
        m_mirrorTimers[MirrorTimer::BREATH].SetScale((apply && GetWaterBreathingInterval()) ? -1 : 10);

    // On moving in/out hazardous liquids: affect environmental timer (进出岩浆/软泥等危险液体：更新环境伤害计时器)
    if ((flags & ENVIRONMENT_MASK_LIQUID_HAZARD))
        m_mirrorTimers[MirrorTimer::ENVIRONMENTAL].SetScale((m_environmentFlags & ENVIRONMENT_MASK_LIQUID_HAZARD) ? -1 : 10);
}

void Player::SendMirrorTimerStart(uint32 type, uint32 remaining, uint32 duration, int32 scale, bool paused/* = false*/, uint32 spellId/* = 0*/)
{
    WorldPacket data(SMSG_START_MIRROR_TIMER, (4 + 4 + 4 + 4 + 1 + 4));
    data << uint32(type);
    data << uint32(remaining);
    data << uint32(duration);
    data << int32(scale);
    data << uint8(paused);
    data << uint32(spellId);
    GetSession()->SendPacket(data);
}

void Player::SendMirrorTimerStop(uint32 type)
{
    WorldPacket data(SMSG_STOP_MIRROR_TIMER, 4);
    data << uint32(type);
    GetSession()->SendPacket(data);
}

void Player::SendMirrorTimerPause(uint32 type, bool state)
{
    // Note: Default UI handler for this is bugged, args dont match (注：客户端默认 UI 处理此包存在参数不匹配的缺陷)
    // Gotta do a full update with SMSG_START_MIRROR_TIMER to avoid lua errors (必须通过 SMSG_START_MIRROR_TIMER 全量重发以避免客户端 Lua 报错)
    WorldPacket data(SMSG_PAUSE_MIRROR_TIMER, (4 + 1));
    data << uint32(type);
    data << uint8(state);
    GetSession()->SendPacket(data);
}

void Player::FreezeMirrorTimers(bool state)
{
    for (auto& timer : m_mirrorTimers)
    {
        if (!timer.GetSpellId())
            timer.SetFrozen(state);
    }
}

void Player::SendMirrorTimers(bool forced/*= false*/)
{
    for (auto& timer : m_mirrorTimers)
    {
        if (timer.GetType() >= MirrorTimer::NUM_CLIENT_TIMERS)
            return;

        MirrorTimer::Status status = timer.FetchStatus();

        if (forced && timer.IsActive())
            status = MirrorTimer::FULL_UPDATE;

        switch (status)
        {
            case MirrorTimer::FULL_UPDATE:
                SendMirrorTimerStart(timer.GetType(), timer.GetRemaining(), timer.GetDuration(), timer.GetScale(), timer.IsFrozen(), timer.GetSpellId());
                break;
            case MirrorTimer::STATUS_UPDATE:
                if (!timer.IsActive())
                    SendMirrorTimerStop(timer.GetType());
                else
                {
                    // NOTE: Replaced with full resend due to clientside UI bug, details inside (注：因客户端 UI 缺陷改为完整重发镜像计时器)
                    // SendMirrorTimerPause(timer.GetType(), timer.IsFrozen());
                    SendMirrorTimerStart(timer.GetType(), timer.GetRemaining(), timer.GetDuration(), timer.GetScale(), timer.IsFrozen(), timer.GetSpellId());
                }
                break;
            default:
                break;
        }
    }
}

void Player::UpdateMirrorTimers(uint32 diff, bool send/* = true*/)
{
    for (auto& timer : m_mirrorTimers)
    {
        const MirrorTimer::Type type = timer.GetType();
        const bool active = timer.IsActive();

        if (active || CheckMirrorTimerActivation(type))
        {
            if (CheckMirrorTimerDeactivation(type))
                 m_mirrorTimers[type].Stop();
            else
            {
                if (active)
                {
                    if (timer.GetSpellId())
                    {
                        if (auto buff = GetMirrorTimerBuff(type))
                            m_mirrorTimers[type].SetRemaining(uint32(std::abs(buff->GetAuraDuration())));
                        else
                            m_mirrorTimers[type].Stop();
                    }

                    if (!timer.Update(diff))
                        OnMirrorTimerExpirationPulse(type);
                }
                else
                {
                    if (auto buff = GetMirrorTimerBuff(type))
                        m_mirrorTimers[type].Start(uint32(std::abs(buff->GetAuraDuration())), uint32(std::abs(buff->GetAuraMaxDuration())), buff->GetId());
                    else
                        m_mirrorTimers[type].Start(GetMirrorTimerMaxDuration(type));
                }
            }
        }
    }

    if (send)
        SendMirrorTimers();
}

bool Player::CheckMirrorTimerActivation(MirrorTimer::Type timer) const
{
    switch (timer)
    {
        case MirrorTimer::FATIGUE:
            return (IsInHighSea() && !IsTaxiFlying() && !GetTransport());
        case MirrorTimer::BREATH:
            return (IsUnderwater() && GetWaterBreathingInterval());
        case MirrorTimer::FEIGNDEATH:
            return (IsFeigningDeath());
        case MirrorTimer::ENVIRONMENTAL:
            return ((m_environmentFlags & ENVIRONMENT_MASK_LIQUID_HAZARD) && !(m_lastLiquid && m_lastLiquid->SpellId));
        default:
            return false;
    }
}

bool Player::CheckMirrorTimerDeactivation(MirrorTimer::Type timer) const
{
    // Spirit of redemption: just drop all mirror timers at once (救赎之魂状态下立即停用所有镜像计时器)
    if (GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION)
        return true;

    switch (timer)
    {
        case MirrorTimer::FATIGUE:
            return (!(m_environmentFlags & ENVIRONMENT_FLAG_LIQUID) || (!IsAlive() && !HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST)));
        case MirrorTimer::BREATH:
            return (!(m_environmentFlags & ENVIRONMENT_FLAG_LIQUID) || !IsAlive());
        case MirrorTimer::FEIGNDEATH:
            return (!IsFeigningDeath());
        case MirrorTimer::ENVIRONMENTAL:
            return (!(m_environmentFlags & ENVIRONMENT_FLAG_LIQUID) || !IsAlive());
        default:
            return false;
    }
}

void Player::OnMirrorTimerExpirationPulse(MirrorTimer::Type timer)
{
    switch (timer)
    {
        case MirrorTimer::FATIGUE:
            if (IsAlive())                                      // Deal damage to living player (对存活玩家造成呼吸耗尽/疲劳/环境伤害)
                EnvironmentalDamage(DAMAGE_EXHAUSTED, ((GetMaxHealth() / 5) + urand(0, (GetLevel() - 1))));
            else if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST)) // Teleport ghost to graveyard (幽灵疲劳耗尽则传送回墓地)
                RepopAtGraveyard();
            break;
        case MirrorTimer::BREATH:
            // TODO: Check this formula (待办：核对溺水/疲劳伤害公式)
            EnvironmentalDamage(DAMAGE_DROWNING, ((GetMaxHealth() / 5) + urand(0, (GetLevel() - 1))));
            break;
        case MirrorTimer::ENVIRONMENTAL:
            if (IsInMagma())
                EnvironmentalDamage(DAMAGE_LAVA, urand(sWorld.getConfig(CONFIG_UINT32_ENVIRONMENTAL_DAMAGE_MIN), sWorld.getConfig(CONFIG_UINT32_ENVIRONMENTAL_DAMAGE_MAX)));
            // FIXME: Need to skip slime damage in Undercity, maybe someone can find better way to handle environmental damage (待修复：需跳过幽暗城软泥伤害)
            //if (IsInSlime() && m_zoneUpdateId != 1497)
            //    EnvironmentalDamage(DAMAGE_SLIME, urand(sWorld.getConfig(CONFIG_UINT32_ENVIRONMENTAL_DAMAGE_MIN), sWorld.getConfig(CONFIG_UINT32_ENVIRONMENTAL_DAMAGE_MAX)));
            break;
        default:
            return;
    }
}

uint32 Player::GetMirrorTimerMaxDuration(MirrorTimer::Type timer) const
{
    switch (timer)
    {
        case MirrorTimer::FATIGUE:
            return (sWorld.getConfig(CONFIG_UINT32_MIRRORTIMER_FATIGUE_MAX) * IN_MILLISECONDS);
        case MirrorTimer::BREATH:
            return GetWaterBreathingInterval();
        case MirrorTimer::FEIGNDEATH:
            return m_mirrorTimers[MirrorTimer::FEIGNDEATH].GetDuration();
        case MirrorTimer::ENVIRONMENTAL:
            return (sWorld.getConfig(CONFIG_UINT32_MIRRORTIMER_ENVIRONMENTAL_MAX) * IN_MILLISECONDS);
        default:
            return 0;
    }
}

SpellAuraHolder const* Player::GetMirrorTimerBuff(MirrorTimer::Type timer) const
{
    switch (timer)
    {
        case MirrorTimer::FEIGNDEATH:
        {
            SpellAuraHolder const* buff = nullptr;
            if (IsFeigningDeath())
            {
                for (auto aura : GetAurasByType(SPELL_AURA_FEIGN_DEATH))
                {
                    if (auto holder = aura->GetHolder())
                    {
                        if (!buff || holder->GetAuraMaxDuration() > buff->GetAuraMaxDuration())
                            buff = holder;
                    }
                }
            }
            return buff;
        }
        default:
            return nullptr;
    }
}

uint32 Player::getCorpseReclaimDelayHelper(time_t deathExpirationTime, time_t time, bool pvp) const
{
    uint32 count;
    if ((pvp && sWorld.getConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP)) ||
        (!pvp && sWorld.getConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE)))
    {
        count = uint32(deathExpirationTime - time) / DEATH_EXPIRE_STEP;
        if (count >= MAX_DEATH_COUNT)
            count = MAX_DEATH_COUNT - 1;
    }
    else
        count = 0;

    return corpseReclaimDelay[count];
}

/// Player 帧更新主循环：处理计时器、延迟传送、限时任务、区域更新、自动存盘与游戏时间
void Player::Update(const uint32 diff)
{
    if (!IsInWorld())
        return;

    // Update ticket squelch timer (更新 GM 密语禁言计时器)
    if (WorldSession* session = GetSession())
        session->m_ticketSquelchTimer.Update(diff);

    // Undelivered mail (处理延迟投递的未送达邮件)
    if (m_nextMailDelivereTime && m_nextMailDelivereTime <= time(nullptr))
    {
        SendNewMail();
        ++unReadMails;

        // It will be recalculate at mailbox open (for unReadMails important non-0 until mailbox open, it also will be recalculated) (打开邮箱时会重新精确计算未读邮件数)
        m_nextMailDelivereTime = 0;
    }

    // Update cinematic location (更新过场动画摄像机位置)
    if (m_cinematicMgr)
    {
        m_cinematicMgr->m_cinematicDiff += diff;
        // update only if CINEMATIC_UPDATEDIFF have passed (仅在达到过场动画更新间隔时更新)
        if (WorldTimer::getMSTimeDiff(m_cinematicMgr->m_lastCinematicCheck, WorldTimer::getMSTime()) > CINEMATIC_UPDATEDIFF)
        {
            m_cinematicMgr->m_lastCinematicCheck = WorldTimer::getMSTime();
            if (!m_cinematicMgr->UpdateCinematicLocation(diff))
                m_cinematicMgr.reset(nullptr);             // if any problem occur during cinematic update we can delete it (若过场动画更新出错可中止)
        }
    }

    UpdateMirrorTimers(diff);

    // Used to implement delayed far teleports (处理延迟执行的跨地图远距离传送)
    SetCanDelayTeleport(true);
    Unit::Update(diff);
    SetCanDelayTeleport(false);

    if (IsHasDelayedTeleport() && m_semaphoreTeleport_Near)
        TeleportTo(m_teleport_dest, m_teleport_options);

    time_t now = time(nullptr);

    UpdatePvPFlagTimer(diff);

    UpdatePvPContestedFlagTimer(diff);

    UpdateDuelFlag(now);

    CheckDuelDistance(now);

    UpdateAfkReport(now);

    // Update items that have just a limited lifetime (更新限时物品的剩余存在时间)
    if (now > m_Last_tick)
        UpdateItemDuration(uint32(now - m_Last_tick));

    if (!m_timedquests.empty())
    {
        QuestSet::iterator iter = m_timedquests.begin();
        while (iter != m_timedquests.end())
        {
            QuestStatusData& q_status = mQuestStatus[*iter];
            if (q_status.m_timer <= diff)
            {
                uint32 quest_id  = *iter;
                ++iter;                                     // Current iter will be removed in FailQuest (FailQuest 会移除当前迭代器，须先递增)
                FailQuest(quest_id);
            }
            else
            {
                q_status.m_timer -= diff;
                if (q_status.uState != QUEST_NEW) q_status.uState = QUEST_CHANGED;
                ++iter;
            }
        }
    }

    if (hasUnitState(UNIT_STAT_MELEE_ATTACKING))
    {
        UpdateMeleeAttackingState();

        Unit* pVictim = GetVictim();
        if (pVictim && !IsNonMeleeSpellCasted(false))
        {
            Player* vOwner = pVictim->GetBeneficiaryPlayer();
            if (vOwner && vOwner->IsPvP() && !IsInDuelWith(vOwner))
            {
                UpdatePvP(true);
                RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_PVP_ACTIVE_CANCELS);
            }
        }
    }

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        if (GetTimeInnEnter() > 0)                          // Freeze update (暂停更新)
        {
            time_t time_inn = now - GetTimeInnEnter();
            if (time_inn >= 10)                             // Freeze update (暂停更新)
            {
                SetRestBonus(GetRestBonus() + ComputeRest(time_inn));
                UpdateInnerTime(now);
            }
        }
    }

    if (m_positionStatusUpdateTimer)
    {
        if (diff >= m_positionStatusUpdateTimer)
            m_positionStatusUpdateTimer = 0;
        else
            m_positionStatusUpdateTimer -= diff;
    }

    if (m_zoneUpdateTimer > 0)
    {
        if (diff >= m_zoneUpdateTimer)
        {
            uint32 newzone, newarea;
            GetZoneAndAreaId(newzone, newarea);

            if (m_zoneUpdateId != newzone)
                UpdateZone(newzone, newarea);               // Also update area (同时刷新子区域信息)
            else
            {
                // Use area updates as well (同时启用子区域状态更新，)
                // Needed for free for all arenas for example (例如自由混战竞技场区域检测需要)
                if (m_areaUpdateId != newarea)
                    UpdateArea(newarea);

                m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;
            }
        }
        else
            m_zoneUpdateTimer -= diff;
    }

    if (IsAlive())
    {
        m_regenTimer += diff;
        if (m_regenTimer >= REGEN_TIME_FULL)
            RegenerateAll(m_regenTimer / 100 * 100);
    }

    if (m_deathState == JUST_DIED)
        KillPlayer();

    if (m_nextSave > 0)
    {
        if (diff >= m_nextSave)
        {
            // m_nextSave reseted in SaveToDB call (m_nextSave 会在 SaveToDB 内重置)
            SaveToDB();
            DETAIL_LOG("Player '%s' (GUID: %u) saved", GetName(), GetGUIDLow());
        }
        else
            m_nextSave -= diff;
    }

    // Played time (累计游戏时间更新)
    if (now > m_Last_tick)
    {
        uint32 elapsed = uint32(now - m_Last_tick);
        m_Played_time[PLAYED_TIME_TOTAL] += elapsed;        // Total played time (总游戏时间)
        m_Played_time[PLAYED_TIME_LEVEL] += elapsed;        // Level played time (当前等级游戏时间)
        m_Last_tick = now;
    }

    if (m_drunk)
    {
        m_drunkTimer += diff;

        if (m_drunkTimer > 10 * IN_MILLISECONDS)
            HandleSobering();
    }

    // Not auto-free ghost from body in instances (副本中死亡不自动释放灵魂)
    if (m_deathTimer > 0  && !GetMap()->Instanceable())
    {
        if (diff >= m_deathTimer)
        {
            m_deathTimer = 0;
            BuildPlayerRepop();
            RepopAtGraveyard();
        }
        else
            m_deathTimer -= diff;
    }

    UpdateEnchantTime(diff);
    UpdateHomebindTime(diff);

    if (m_createdInstanceClearTimer < diff)
    {
        m_createdInstanceClearTimer = MINUTE * IN_MILLISECONDS;
        UpdateNewInstanceIdTimers(std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::now()));
    }
    else
        m_createdInstanceClearTimer -= diff;

    Pet* pet = GetPet();
    if (pet && !pet->IsWithinDistInMap(this, GetMap()->GetVisibilityDistance()) && pet->GetObjectGuid() != GetCharmGuid())
        pet->Unsummon(PET_SAVE_REAGENTS, this);

    if (IsHasDelayedTeleport() && !m_semaphoreTeleport_Near)
        TeleportTo(m_teleport_dest, m_teleport_options);
}

void Player::Heartbeat()
{
    Unit::Heartbeat();
    SendUpdateToOutOfRangeGroupMembers();
}

#ifdef ENABLE_PLAYERBOTS
void Player::CreatePlayerbotAI()
{
    assert(!m_playerbotAI);
    m_playerbotAI = std::make_unique<PlayerbotAI>(this);
}

void Player::RemovePlayerbotAI()
{
    m_playerbotAI = nullptr;
}

void Player::CreatePlayerbotMgr()
{
    assert(!m_playerbotMgr);
    m_playerbotMgr = std::make_unique<PlayerbotMgr>(this);
}

void Player::RemovePlayerbotMgr()
{
    m_playerbotMgr = nullptr;
}

void Player::UpdateAI(const uint32 diff, bool minimal)
{
    if (m_playerbotAI)
    {
        m_playerbotAI->UpdateAI(diff);
    }

    if (m_playerbotMgr)
    {
        m_playerbotMgr->UpdateAI(diff);
    }
}
#endif

/// 设置玩家死亡状态（存活、刚死亡、尸体、幽灵）并处理宠物/光环/荣誉击杀
void Player::SetDeathState(DeathState s)
{
    uint32 ressSpellId = 0;

    bool cur = IsAlive();

    if (s == JUST_DIED && cur)
    {
        // drunken state is cleared on death (死亡时清除醉酒状态)
        SetDrunkValue(0);
        // lost combo points at any target (targeted combo points clear in Unit::SetDeathState) (死亡时清空所有目标上的连击点数)
        ClearComboPoints();

        clearResurrectRequestData();

        bool petAlive = true; // PET_SAVE_REAGENTS when pet is unsummoned (宠物解散时按 PET_SAVE_REAGENTS 保存状态)
        if (Pet* pet = GetPet())
            petAlive = pet->IsAlive();

        RemovePet(petAlive ? PET_SAVE_REAGENTS : PET_SAVE_AS_CURRENT);

        // Remove guardians (only players are supposed to have pets/guardians removed on death) (移除所有守护者随从，仅玩家死亡需清理)
        RemoveGuardians();

        // save value before aura remove in Unit::SetDeathState (在 Unit::SetDeathState 移除光环前保存可复活法术 ID)
        ressSpellId = GetUInt32Value(PLAYER_SELF_RES_SPELL);

        // passive spell (被动复活法术)
        if (!ressSpellId)
            ressSpellId = GetResurrectionSpellId();

        if (InstanceData* mapInstance = GetInstanceData())
            mapInstance->OnPlayerDeath(this);
    }

    Unit::SetDeathState(s);

    // restore resurrection spell id for player after aura remove (光环移除后恢复记录的复活法术 ID)
    if (s == JUST_DIED && cur && ressSpellId)
        SetUInt32Value(PLAYER_SELF_RES_SPELL, ressSpellId);

    if (IsAlive() && !cur)
        // clear aura case after resurrection by another way (spells will be applied before next death) (若通过其他方式复活则清空相关光环缓存)
        SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);

    if (GetGroup())
        SetGroupUpdateFlag(GROUP_UPDATE_FLAG_STATUS);
}

/// 从角色枚举查询结果构建发送给客户端选人界面的角色数据包
bool Player::BuildEnumData(QueryResult* result, WorldPacket& p_data)
{
    //             0               1                2                3                 4                  5                       6                        7
    //    "SELECT characters.guid, characters.name, characters.race, characters.class, characters.gender, characters.playerBytes, characters.playerBytes2, characters.level, "
    //     8                9               10                     11                     12                     13                    14
    //    "characters.zone, characters.map, characters.position_x, characters.position_y, characters.position_z, guild_member.guildid, characters.playerFlags, "
    //    15                    16                   17                     18                   19                         20
    //    "characters.at_login, character_pet.entry, character_pet.modelid, character_pet.level, characters.equipmentCache, character_declinedname.genitive "

    Field* fields = result->Fetch();

    uint32 guid = fields[0].GetUInt32();
    uint8 pRace = fields[2].GetUInt8();
    uint8 pClass = fields[3].GetUInt8();

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(pRace, pClass);
    if (!info)
    {
        sLog.outError("Player %u has incorrect race/class pair. Don't build enum.", guid);
        return false;
    }

    p_data << ObjectGuid(HIGHGUID_PLAYER, guid);
    p_data << fields[1].GetString();                       // name (角色名)
    p_data << uint8(pRace);                                // race (种族)
    p_data << uint8(pClass);                               // class (职业)
    p_data << uint8(fields[4].GetUInt8());                 // gender (性别)

    uint32 playerBytes = fields[5].GetUInt32();
    p_data << uint8(playerBytes);                          // skin (肤色)
    p_data << uint8(playerBytes >> 8);                     // face (脸型)
    p_data << uint8(playerBytes >> 16);                    // hair style (发型)
    p_data << uint8(playerBytes >> 24);                    // hair color (发色)

    uint32 playerBytes2 = fields[6].GetUInt32();
    p_data << uint8(playerBytes2 & 0xFF);                  // facial hair (面部毛发/饰物)

    uint32 atLoginFlags = fields[15].GetUInt32();

    // a character that has never entered the world is sent at level 0, which shows as level 1; (从未进入过世界的角色发送等级 0，客户端显示为 1 级；)
    // one created above level 1 keeps its level (初始等级高于 1 级的角色保留其等级)
    uint8 level = fields[7].GetUInt8();
    p_data << uint8((atLoginFlags & AT_LOGIN_FIRST) && level == 1 ? 0 : level); // level (等级)
    p_data << uint32(fields[8].GetUInt32());               // zone (所在区域 ID)
    p_data << uint32(fields[9].GetUInt32());               // map (所在地图 ID)

    p_data << fields[10].GetFloat();                       // x (X 坐标)
    p_data << fields[11].GetFloat();                       // y (Y 坐标)
    p_data << fields[12].GetFloat();                       // z (Z 坐标)

    p_data << uint32(fields[13].GetUInt32());              // guild id (公会 ID)

    uint32 char_flags = 0;
    uint32 playerFlags = fields[14].GetUInt32();
    if (playerFlags & PLAYER_FLAGS_HIDE_HELM)
        char_flags |= CHARACTER_FLAG_HIDE_HELM;
    if (playerFlags & PLAYER_FLAGS_HIDE_CLOAK)
        char_flags |= CHARACTER_FLAG_HIDE_CLOAK;
    if (playerFlags & PLAYER_FLAGS_GHOST)
        char_flags |= CHARACTER_FLAG_GHOST;
    if (atLoginFlags & AT_LOGIN_RENAME)
        char_flags |= CHARACTER_FLAG_RENAME;
    if (sWorld.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED))
    {
        if (!fields[20].GetCppString().empty())
            char_flags |= CHARACTER_FLAG_DECLINED;
    }
    else
        char_flags |= CHARACTER_FLAG_DECLINED;

    p_data << uint32(char_flags);                          // character flags (角色列表状态标志位)

    // First login (首次登录标记)
    p_data << uint8(atLoginFlags & AT_LOGIN_FIRST ? 1 : 0);

    // Pets info (宠物展示信息)
    {
        uint32 petDisplayId = 0;
        uint32 petLevel = 0;
        uint32 petFamily = 0;

        // show pet at selection character in character list only for non-ghost character (仅非幽灵状态角色在选人界面展示宠物)
        if (result && !(playerFlags & PLAYER_FLAGS_GHOST) && (pClass == CLASS_WARLOCK || pClass == CLASS_HUNTER))
        {
            uint32 entry = fields[16].GetUInt32();
            CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(entry);
            if (cInfo)
            {
                petDisplayId = fields[17].GetUInt32();
                petLevel = fields[18].GetUInt32();
                petFamily = cInfo->Family;
            }
        }

        p_data << uint32(petDisplayId);
        p_data << uint32(petLevel);
        p_data << uint32(petFamily);
    }


    Tokens data = StrSplit(fields[19].GetCppString(), " ");
    for (uint8 slot = 0; slot < INVENTORY_SLOT_BAG_START + 1; ++slot)
    {
        uint32 visualbase = slot * 2;                       // entry, perm ench., temp ench. (装备预览：物品条目、永久附魔、临时附魔)
        uint32 item_id = GetUInt32ValueFromArray(data, visualbase);
        const ItemPrototype* proto = ObjectMgr::GetItemPrototype(item_id);
        if (!proto)
        {
            p_data << uint32(0);
            p_data << uint8(0);
            p_data << uint32(0);
            continue;
        }

        SpellItemEnchantmentEntry const* enchant = nullptr;

        uint32 enchants = GetUInt32ValueFromArray(data, visualbase + 1);
        for (uint8 enchantSlot = PERM_ENCHANTMENT_SLOT; enchantSlot <= TEMP_ENCHANTMENT_SLOT; ++enchantSlot)
        {
            // values stored in 2 uint16 (附魔拆分为两个 uint16 读取)
            uint32 enchantId = 0x0000FFFF & (enchants >> enchantSlot * 16);
            if (!enchantId)
                continue;

            enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (enchant)
                break;
        }

        p_data << uint32(proto->DisplayInfoID);
        p_data << uint8(proto->InventoryType);
        p_data << uint32(enchant ? enchant->aura_id : 0);
    }

    return true;
}

bool Player::Mount(uint32 displayid, const Aura* aura/* = nullptr*/)
{
    if (!Unit::Mount(displayid, aura))
        return false;

    // Custom mount (non-aura such as taxi or command) or in flight: unsummon any pet (自定义坐骑或飞行状态：临时解散所有宠物)
    if (!aura || IsFreeFlying() || IsSpellHaveAura(aura->GetSpellProto(), SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED))
        UnsummonPetTemporaryIfAny();
    // Land mount aura: unsummon only permanent pet (地面坐骑光环：仅临时解散永久宠物)
    else if (aura)
    {
        if (Pet* pet = GetPet())
        {
            if (pet->isControlled() && (!(pet->isTemporarySummoned() || InArena())
                                        || sWorld.getConfig(CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT)))
                UnsummonPetTemporaryIfAny();
            else
                pet->SetModeFlags(PET_MODE_DISABLE_ACTIONS);
        }
    }
    return true;
}

bool Player::Unmount(const Aura* aura/* = nullptr*/)
{
    if (!Unit::Unmount(aura))
        return false;

    // only resummon old pet if the player is already added to a map (仅当玩家已加入地图时才重新召唤暂存宠物，)
    // this prevents adding a pet to a not created map which would otherwise cause a crash (避免将宠物加入未创建的地图导致崩溃)
    // (it could probably happen when logging in after a previous crash) (可能发生在异常宕机后的重连阶段)
    if (Pet* pet = GetPet())
    {
        // Get reaction state and display appropriately (根据对玩家与尸体的声望阵营获取对应敌友反应)
        if (CharmInfo* charmInfo = pet->GetCharmInfo())
            pet->SetModeFlags(PetModeFlags(pet->AI()->GetReactState() | charmInfo->GetCommandState() * 0x100));
    }
    else
        ResummonPetTemporaryUnSummonedIfAny();

    return true;
}

void Player::ToggleAFK()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK);

    if (GetGroup())
        SetGroupUpdateFlag(GROUP_UPDATE_FLAG_STATUS);

    // afk player not allowed in battleground (战场中不允许玩家进入暂离 AFK 状态)
    if (isAFK() && InBattleGround() && !InArena() && !IsGameMaster())
        LeaveBattleground();

    GetSession()->AfkStateChange(isAFK());
}

void Player::ToggleDND()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND);

    if (GetGroup())
        SetGroupUpdateFlag(GROUP_UPDATE_FLAG_STATUS);
}

ChatTagFlags Player::GetChatTag() const
{
    ChatTagFlags tag = CHAT_TAG_NONE;

    if (isAFK())
        tag |= CHAT_TAG_AFK;
    if (isDND())
        tag |= CHAT_TAG_DND;
    if (isGMChat())
        tag |= CHAT_TAG_GM;

    return tag;
}

bool Player::isAcceptTickets() const
{
    AccountTypes level = GetSession()->GetSecurity();

    if (level > SEC_PLAYER && level >= sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_ACCEPT_TICKETS))
        return (m_ExtraFlags & PLAYER_EXTRA_GM_ACCEPT_TICKETS);

    return false;
}

bool Player::isGMChat() const
{
    AccountTypes level = GetSession()->GetSecurity();

    if (level > SEC_PLAYER && level >= sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_CHAT))
        return (m_ExtraFlags & PLAYER_EXTRA_GM_CHAT);

    return false;
}

/// 将玩家传送至指定地图与坐标（支持近距离瞬移、跨图切图、延迟传送及载具传送）
bool Player::TeleportTo(uint32 mapid, float x, float y, float z, float orientation, uint32 options /*=0*/, AreaTrigger const* at /*=nullptr*/, GenericTransport* transport /*=nullptr*/)
{
    // do not let charmed players/creatures teleport (不允许被魅惑的玩家或生物传送)
    if (HasCharmer())
        return false;

    if (!MapManager::IsValidMapCoord(mapid, x, y, z, orientation))
    {
        sLog.outError("TeleportTo: invalid map %d or absent instance template.", mapid);
        return false;
    }

    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);  // Validity checked in IsValidMapCoord (坐标合法性由 IsValidMapCoord 校验)

    // don't let enter battlegrounds without assigned battleground id (for example through areatrigger)... (禁止未分配战场 ID 的玩家进入战场地表)
    // don't let gm level > 1 either (即使是高等级 GM 也不允许无战场 ID 进入)
    if (!InBattleGround() && mEntry->IsBattleGroundOrArena())
        return false;

    // Get MapEntrance trigger if teleport to other -nonBG- map (若传送至非战场地图，获取其入口区域触发器)
    bool assignedAreaTrigger = false;
    if (GetMapId() != mapid && !mEntry->IsBattleGroundOrArena() && !at)
    {
        at = sObjectMgr.GetMapEntranceTrigger(mapid);
        assignedAreaTrigger = true;
    }

    // Check requirements for teleport (检查目标地图与副本的准入条件)
    if (at)
    {
        uint32 miscRequirement = 0;
        AreaLockStatus lockStatus = GetAreaTriggerLockStatus(at, miscRequirement);
        if (lockStatus != AREA_LOCKSTATUS_OK)
        {
            // Teleport not requested by area-trigger (非区域触发器发起的传送，如法术或指令)
            // TODO - Assume a player with expansion 0 travels from BootyBay to Ratched, and he is attempted to be teleported to outlands (待办：若无资料片权限玩家搭乘跨大陆载具，)
            //        then he will repop near BootyBay instead of normally continuing his journey (将其遣返至附近墓地以免卡死)
            // This code is probably added to catch passengers on ships to northrend who shouldn't go there (用于拦截无权限搭乘船只前往受限大陆的乘客)
            if (lockStatus == AREA_LOCKSTATUS_INSUFFICIENT_EXPANSION && !assignedAreaTrigger && GetTransport())
                RepopAtGraveyard();                         // Teleport to near graveyard if on transport, looks blizz like :) (若在载具上则传送至附近墓地，符合官服行为)

            SendTransferAbortedByLockStatus(mEntry, at, lockStatus, miscRequirement);
            return false;
        }
    }

    // if we were on a transport, leave (若当前在载具/船只上则先脱离)
    if (!(options & TELE_TO_NOT_LEAVE_TRANSPORT) && m_transport)
    {
        m_transport->RemovePassenger(this);
        m_transport = nullptr;
        m_movementInfo.ClearTransportData();
    }

    // The player was ported to another map and looses the duel immediately. (玩家被传送到其他地图时立即判为逃离决斗失败)
    // We have to perform this check before the teleport, otherwise the (必须在传送前完成检查，否则)
    // ObjectAccessor won't find the flag. (传送后 ObjectAccessor 将找不到决斗旗帜)
    if (duel && GetMapId() != mapid)
        if (GetMap()->GetGameObject(GetGuidValue(PLAYER_DUEL_ARBITER)))
            DuelComplete(DUEL_FLED);

    // reset movement flags at teleport, because player will continue move with these flags after teleport (传送时重置移动标志位，防止传送后保持原有移动状态)
    m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);
    DisableSpline();

    if (!(options & TELE_TO_NOT_UNSUMMON_PET) || GetMapId() != mapid)
    {
        if (GetMapId() == mapid)
        {
            if (Unit* charm = GetCharm())
            {
                if (!charm->IsWithinDist3d(x, y, z, 100.f))
                    BreakCharmOutgoing(charm);
            }

            if (Pet* pet = GetPet())
            {
                // same map, only remove pet if out of range for new position (同地图传送：仅当宠物超出新位置距离限制时才移除)
                if (!pet->IsWithinDist3d(x, y, z, 50.f))
                    UnsummonPetTemporaryIfAny();
            }
        }
        else
        {
            if (Unit* charm = GetCharm())
                BreakCharmOutgoing(charm);

            if (GetPet())
                UnsummonPetTemporaryIfAny();
        }
    }

    // the client takes only the world position of a near teleport of its own player and leaves any transport: onto a transport, (同地图近距离传送时客户端只认世界坐标且会脱离载具；)
    // whose deck may be moving, the world is reloaded instead and the player created aboard (not for a passenger the transport (若目标是移动中的载具甲板，必须触发重载世界以登上载具)
    // itself moves along) (自身随载具移动除外)
    GenericTransport* destTransport = transport;
    if (!destTransport && m_teleportTransport && GetMapId() == mapid)
        destTransport = GetMap()->GetTransport(m_teleportTransport);
    bool const nearOntoTransport = destTransport && destTransport != m_transport;

    if ((GetMapId() == mapid) && (!m_transport) && !nearOntoTransport) // TODO the !m_transport might have unexpected effects when teleporting from transport to other place on same map (待办：同图从载具传往他处时需留意 !m_transport 边界)
    {
        // lets reset far teleport flag if it wasn't reset during chained teleports (连环传送时确保清除远距离传送标记)
        SetSemaphoreTeleportFar(false);
        // setup delayed teleport flag (设置延迟传送标志)
        // if teleport spell is casted in Unit::Update() func (若在 Unit::Update() 迭代期间触发传送，)
        // then we need to delay it until update process will be finished (必须延迟至更新循环结束后执行)
        if (SetDelayedTeleportFlagIfCan())
        {
            SetSemaphoreTeleportNear(true);
            // lets save teleport destination for player (保存玩家传送目标坐标)
            m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
            m_teleport_options = options;
            m_teleportTransport = transport ? transport->GetObjectGuid() : ObjectGuid();
            return true;
        }

        if (!(options & TELE_TO_NOT_LEAVE_COMBAT))
            CombatStop();

        if (!IsWithinDist3d(x, y, z, GetMap()->GetVisibilityDistance()))
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_WORLD);

        GenericTransport* currentTransport = transport;
        if (!transport && m_teleportTransport)
            currentTransport = GetMap()->GetTransport(m_teleportTransport);
        if (currentTransport)
        {
            m_movementInfo.AddMovementFlag(MOVEFLAG_ONTRANSPORT);
            m_movementInfo.t_guid = GetObjectGuid();
            m_movementInfo.t_time = currentTransport->GetPathProgress();

            if (!HasAtClient(currentTransport)) // in sniff, this aggregates all surroundings and sends them at once (参照抓包：聚合周围实体更新并一次性下发)
            {
                AddAtClient(currentTransport);
                UpdateData data;
                currentTransport->BuildCreateUpdateBlockForPlayer(data, this);
                data.SendData(*GetSession());
            }
        }

        // this will be used instead of the current location in SaveToDB (SaveToDB 将使用此目标位置代替当前位置)
        m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);

        // code for finish transfer called in WorldSession::HandleMovementOpcodes() (近距离传送确认逻辑在 HandleMovementOpcodes 中完成)
        // at client packet MSG_MOVE_TELEPORT_ACK (客户端回传 MSG_MOVE_TELEPORT_ACK 时触发)
        SetSemaphoreTeleportNear(true);
        // near teleport, triggering send MSG_MOVE_TELEPORT_ACK from client at landing (近距离传送：通知客户端落地后发送 MSG_MOVE_TELEPORT_ACK)
        if (!GetSession()->PlayerLogout())
            SendTeleportPacket(x, y, z, orientation, currentTransport);

        if (Loot* loot = sLootMgr.GetLoot(this))
            loot->Release(this);
    }
    else
    {
        // far teleport to another map (跨地图或重载世界远距离传送)
        Map* oldmap = IsInWorld() ? GetMap() : nullptr;
        // check if we can enter before stopping combat / removing pet / totems / interrupting spells (在脱战/收宠/打断施法前先检查是否可进入目标地图)

        // If the map is not created, assume it is possible to enter it. (若目标地图实例尚未创建，暂定允许进入；)
        // It will be created in the WorldPortAck. (将在 WorldPortAck 中创建)
        DungeonPersistentState* state = GetBoundInstanceSaveForSelfOrGroup(mapid);
        Map* map = sMapMgr.FindMap(mapid, state ? state->GetInstanceId() : 0);
        if (!map || (nearOntoTransport && map == GetMap()) || map->CanEnter(this)) // the world reload onto a transport stays on the current map (登船重载世界仍停留在当前地图)
        {
            // lets reset near teleport flag if it wasn't reset during chained teleports (连环传送时确保清除近距离传送标记)
            SetSemaphoreTeleportNear(false);
            // set when this call carries out a teleport delayed by an earlier call (标记本次调用正执行先前延迟的传送)
            bool const delayedTeleport = m_bHasDelayedTeleport;
            // setup delayed teleport flag (设置延迟传送标志)
            // if teleport spell is casted in Unit::Update() func (若在 Unit::Update() 迭代期间触发传送，)
            // then we need to delay it until update process will be finished (必须延迟至更新循环结束后执行)
            if (!GetSession()->PlayerLogout() && SetDelayedTeleportFlagIfCan())
            {
                SetSemaphoreTeleportFar(true);
                // lets save teleport destination for player (保存玩家传送目标位置)
                m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
                m_teleport_options = options;
                m_teleportTransport = transport ? transport->GetObjectGuid() : ObjectGuid();
                // the delayed teleport may be done in the thread of another map: the world position is taken here, where the (延迟传送可能由另一地图线程执行，故在此处先取世界坐标)
                // caller can read the transport (确保调用方安全读取载具位置)
                if (transport)
                {
                    m_teleportWorldDest = Position(x, y, z, orientation);
                    transport->CalculatePassengerPosition(m_teleportWorldDest.x, m_teleportWorldDest.y, m_teleportWorldDest.z, &m_teleportWorldDest.o);
                }
                return true;
            }

            SetSelectionGuid(ObjectGuid());

            CombatStop();

            UpdatePvPContested(false, true);

            // remove player from battleground on far teleport (when changing maps) (跨地图传送时将玩家移出当前战场)
            if (BattleGround const* bg = GetBattleGround())
            {
                // Note: at battleground join battleground id set before teleport (注：进入战场时传送前已设置新战场 ID，)
                // and we already will found "current" battleground (此时会查到目标战场对象，)
                // just need check that this is targeted map or leave (只需检查目标地图是否匹配该战场，否则离开战场)
                if (bg->GetMapId() != mapid)
                    LeaveBattleground(false);               // don't teleport to entry point (不要再传送回战场入口点)
            }

            // remove all dyn objects (移除所有动态法术区域对象)
            RemoveAllDynObjects();

            // stop spellcasting (打断当前施法)
            // not attempt interrupt teleportation spell at caster teleport (不打断正在施放的传送类法术自身)
            if (!(options & TELE_TO_SPELL))
                if (IsNonMeleeSpellCasted(true))
                    InterruptNonMeleeSpells(true);

            // remove auras before removing from map... (移出地图前先清理相关光环)
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_LEAVE_WORLD | AURA_INTERRUPT_FLAG_MOVING | AURA_INTERRUPT_FLAG_TURNING);

            // x, y, z are offsets on the given transport. A delayed teleport is done without the transport, which may be on another (x/y/z 为载具局部偏移坐标；延迟传送因可能跨线程，)
            // map updated by another thread: the guid and the world position taken when the teleport was asked are used instead (改用传送请求时记录的载具 GUID 与世界坐标)
            ObjectGuid targetTransportGuid;
            Position worldDest(x, y, z, orientation);
            if (transport)
            {
                targetTransportGuid = transport->GetObjectGuid();
                transport->CalculatePassengerPosition(worldDest.x, worldDest.y, worldDest.z, &worldDest.o);
            }
            else if (delayedTeleport && m_teleportTransport)
            {
                targetTransportGuid = m_teleportTransport;
                worldDest = m_teleportWorldDest;
            }
            else if (m_transport)
                targetTransportGuid = m_transport->GetObjectGuid();
            // only a passenger crossing to another map with the transport is sent transport offsets: a player joining a transport (仅随船跨图的乘客发送载具局部坐标；)
            // is told the world position, as for a teleport without transport (新登船的玩家仍按无载具传送发送世界坐标)
            bool const withTransport = m_transport && m_transport->GetObjectGuid() == targetTransportGuid;
            if (!GetSession()->PlayerLogout())
            {
                // send transfer packet to display load screen (向客户端发送切图数据包以显示加载界面)
                WorldPacket data(SMSG_TRANSFER_PENDING, (4 + 4 + 4));
                data << uint32(mapid);
                if (withTransport)
                {
                    data << uint32(m_transport->GetEntry());
                    data << uint32(GetMapId());
                }
                GetSession()->SendPacket(data);
            }

            if (Loot* loot = sLootMgr.GetLoot(this))
                loot->Release(this);

            // remove from old map now (立即从旧地图中移除玩家)
            if (oldmap)
                oldmap->Remove(this, false);

            // new final coordinates (更新为新的最终坐标)
            float final_x = x;
            float final_y = y;
            float final_z = z;
            float final_o = orientation;

            Position const& transportPosition = m_movementInfo.GetTransportPos();

            m_teleport_dest = WorldLocation(mapid, final_x, final_y, final_z, final_o);
            m_teleportTransport = targetTransportGuid;
            SetFallInformation(0, worldDest.z);
            // if the player is saved before worldport ack (at logout for example) (若在 WorldPortAck 之前触发保存，如登出，)
            // this will be used instead of the current location in SaveToDB (SaveToDB 将使用传送目标坐标)

            // move packet sent by client always after far teleport (跨地图传送后客户端会发送移动确认包)
            // code for finish transfer to new map called in WorldSession::HandleMoveWorldportAckOpcode at client packet (切图完成逻辑在 HandleMoveWorldportAckOpcode 中处理)
            SetSemaphoreTeleportFar(true);

            if (!GetSession()->PlayerLogout())
            {
                // transfer finished, inform client to start load (切图准备就绪，通知客户端开始加载新世界)
                WorldPacket data(SMSG_NEW_WORLD, (20));
                data << uint32(mapid);
                if (withTransport)
                {
                    data << float(transportPosition.x);
                    data << float(transportPosition.y);
                    data << float(transportPosition.z);
                    data << float(transportPosition.o);
                }
                else
                {
                    data << float(worldDest.x);
                    data << float(worldDest.y);
                    data << float(worldDest.z);
                    data << float(worldDest.o);
                }

                GetSession()->SendPacket(data);
                SendSavedInstances();
            }
        }
        else                                                // !map->CanEnter(this) (无法进入目标地图)
            return false;
    }

    return true;
}

bool Player::TeleportToBGEntryPoint()
{
    return TeleportTo(m_bgData.joinPos);
}

void Player::SetSemaphoreTeleportNear(bool semphsetting)
{
    m_semaphoreTeleport_Near = semphsetting;
    if (!semphsetting)
        GetSession()->DeleteMovementPackets();
}

void Player::SetSemaphoreTeleportFar(bool semphsetting)
{
    m_semaphoreTeleport_Far = semphsetting;
    if (!semphsetting)
        GetSession()->DeleteMovementPackets();
}

/// 执行排队的延迟操作（保存、复活、逃亡者法术）
void Player::ProcessDelayedOperations()
{
    if (m_DelayedOperations == 0)
        return;

    if (m_DelayedOperations & DELAYED_RESURRECT_PLAYER)
        ResurrectUsingRequestDataFinish();

    if (m_DelayedOperations & DELAYED_SAVE_PLAYER)
        SaveToDB();

    if (m_DelayedOperations & DELAYED_SPELL_CAST_DESERTER)
        CastSpell(this, SPELL_ID_BATTLEGROUND_DESERTER, TRIGGERED_OLD_TRIGGERED);               // Deserter (逃亡者减益法术)

    // we have executed ALL delayed ops, so clear the flag (所有延迟操作执行完毕，清除掩码)
    m_DelayedOperations = 0;
}

/// 将玩家实体加入地图世界并初始化状态
void Player::AddToWorld()
{
    if (!IsInWorld())
        sWorld.SetOnlinePlayer(GetTeam(), getRace(), getClass(), true);
    ///- Do not add/remove the player from the object storage (- 请勿从全局对象仓库中增删玩家)
    ///- It will crash when updating the ObjectAccessor (- 更新 ObjectAccessor 时会导致崩溃)
    ///- The player should only be added when logging in (- 玩家仅应在登录时加入全局对象仓库)
    Unit::AddToWorld();

    for (int i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
    {
        if (m_items[i])
            m_items[i]->AddToWorld();
    }
}

/// 将玩家实体从地图世界移除并清理宠物、图腾与决斗旗帜
void Player::RemoveFromWorld()
{
    // cleanup (清理临时状态与控制器)
    if (IsInWorld())
    {
        ///- Release charmed creatures, unsummon totems and remove pets/guardians (- 释放魅惑生物、收回图腾并移除宠物/守护者)
        sWorld.SetOnlinePlayer(GetTeam(), getRace(), getClass(), false);
        UnsummonAllTotems();
    }

    for (int i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
    {
        if (m_items[i])
            m_items[i]->RemoveFromWorld();
    }

    // remove duel before calling Unit::RemoveFromWorld (必须在 Unit::RemoveFromWorld 前移除决斗旗帜)
    // otherwise there will be an existing duel flag pointer but no entry in m_gameObj (否则会导致 m_gameObj 中已无对象而决斗指针悬空)
    DuelComplete(DUEL_INTERRUPTED);

    ///- Do not add/remove the player from the object storage (- 请勿从全局对象仓库中增删玩家)
    ///- It will crash when updating the ObjectAccessor (- 更新 ObjectAccessor 时会导致崩溃)
    ///- The player should only be removed when logging out (- 玩家仅应在登出时移出全局对象仓库)
    if (IsInWorld())
    {
        GetSession()->GetAnticheat()->LeaveWorld();
        GetCamera().ResetView();
    }

    Unit::RemoveFromWorld();
}

void Player::RewardRage(uint32 damage, uint32 weaponSpeedHitFactor, bool attacker)
{
    float addRage;

    float rageconversion = float((0.0091107836 * GetLevel() * GetLevel()) + 3.225598133 * GetLevel()) + 4.2652911f;

    if (attacker)
    {
        addRage = ((damage / rageconversion * 7.5f + weaponSpeedHitFactor) / 2.0f);

        // talent who gave more rage on attack (增加攻击怒气获取的天赋加成)
        addRage *= 1.0f + GetTotalAuraModifier(SPELL_AURA_MOD_RAGE_FROM_DAMAGE_DEALT) / 100.0f;
    }
    else
    {
        addRage = damage / rageconversion * 2.5f;

        // Berserker Rage effect (狂暴之怒额外怒气加成)
        if (HasAura(18499, EFFECT_INDEX_0))
            addRage *= 1.3f;
    }

    addRage *= sWorld.getConfig(CONFIG_FLOAT_RATE_POWER_RAGE_INCOME);

    ModifyPower(POWER_RAGE, uint32(addRage * 10));
}

/// 每帧周期性恢复玩家生命值与能量值（法力/怒气/能量）
void Player::RegenerateAll(uint32 diff)
{
    // Not in combat or they have regeneration (脱战状态或具备战中回复光环)
    if (!IsInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT) ||
            HasAuraType(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT))
    {
        RegenerateHealth(diff);
        if (!IsInCombat() && !HasAuraType(SPELL_AURA_INTERRUPT_REGEN))
            Regenerate(POWER_RAGE, diff);
    }

    Regenerate(POWER_ENERGY, diff);

    Regenerate(POWER_MANA, diff);

    m_regenTimer -= diff;
}

void Player::Regenerate(Powers power, uint32 diff)
{
    uint32 curValue = GetPower(power);
    uint32 maxValue = GetMaxPower(power);

    float addvalue = 0.0f;

    // all powers are precalculated for 2 seconds (所有能量值均按 2 秒周期预计算)
    switch (power)
    {
        case POWER_MANA:
        {
            bool recentCast = IsUnderLastManaUseEffect();
            float ManaIncreaseRate = sWorld.getConfig(CONFIG_FLOAT_RATE_POWER_MANA);
            if (recentCast)
            {
                // Mangos Updates Mana in intervals of 2s, which is correct (MaNGOS 按 2 秒间隔更新法力恢复)
                addvalue = GetFloatValue(PLAYER_FIELD_MOD_MANA_REGEN_INTERRUPT) *  ManaIncreaseRate * uint32(float(diff) / 1000);
            }
            else
            {
                addvalue = GetFloatValue(PLAYER_FIELD_MOD_MANA_REGEN) * ManaIncreaseRate * uint32(float(diff) / 1000);
            }
        }   break;
        case POWER_RAGE:                                    // Regenerate rage (怒气自然衰减/恢复)
        {
            float RageDecreaseRate = sWorld.getConfig(CONFIG_FLOAT_RATE_POWER_RAGE_LOSS);
            addvalue = uint32(float(diff) / 200) * 2.5 * RageDecreaseRate; // decay 2.5 rage per 2 seconds (脱战时每 2 秒衰减 2.5 点怒气)

            AuraList const& ModPowerRegenPCTAuras = GetAurasByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
            for (auto ModPowerRegenPCTAura : ModPowerRegenPCTAuras)
                if (ModPowerRegenPCTAura->GetModifier()->m_miscvalue == int32(power))
                    addvalue *= (ModPowerRegenPCTAura->GetModifier()->m_amount + 100) / 100.0f;
        }   break;
        case POWER_ENERGY:                                  // Regenerate energy (能量恢复)
        {
            float EnergyRate = sWorld.getConfig(CONFIG_FLOAT_RATE_POWER_ENERGY);
            addvalue = uint32(float(diff) / 100) * EnergyRate * m_energyRegenRate;
            break;
        }
        case POWER_FOCUS:
        case POWER_HAPPINESS:
        case POWER_HEALTH:
            return;
    }

    if (power != POWER_RAGE)
    {
        curValue += uint32(addvalue);
        if (curValue > maxValue)
            curValue = maxValue;
    }
    else
    {
        if (curValue <= uint32(addvalue))
            curValue = 0;
        else
            curValue -= uint32(addvalue);
    }
    SetPower(power, curValue);
}

void Player::RegenerateHealth(uint32 diff)
{
    uint32 curValue = GetHealth();
    uint32 maxValue = GetMaxHealth();

    if (curValue >= maxValue) return;

    float HealthIncreaseRate = sWorld.getConfig(CONFIG_FLOAT_RATE_HEALTH);

    float addvalue = 0.0f;

    // normal regen case (maybe partly in combat case) (常规生命恢复，含战斗中部分恢复)
    if (!IsInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
    {
        addvalue = OCTRegenHPPerSpirit() * HealthIncreaseRate;
        if (!IsInCombat())
        {
            AuraList const& mModHealthRegenPct = GetAurasByType(SPELL_AURA_MOD_HEALTH_REGEN_PERCENT);
            for (auto i : mModHealthRegenPct)
                addvalue *= (100.0f + i->GetModifier()->m_amount) / 100.0f;
        }
        else if (HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
            addvalue *= GetTotalAuraModifier(SPELL_AURA_MOD_REGEN_DURING_COMBAT) / 100.0f;

        if (!IsStandState())
            addvalue *= 1.5;
    }

    // always regeneration bonus (including combat) (始终生效的生命回复加成，含战斗中)
    addvalue += GetTotalAuraModifier(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT);

    if (addvalue < 0)
        addvalue = 0;

    ModifyHealth(int32(addvalue * float(diff) / 1000));
}

Creature* Player::GetNPCIfCanInteractWith(ObjectGuid guid, uint32 npcflagmask)
{
    // some basic checks (基础存活/晕眩等状态检查)
    if (!guid || !IsInWorld() || IsTaxiFlying())
        return nullptr;

    // set player as interacting (标记玩家正在交互，移除潜行等非交互光环)
    DoInteraction();

    // not in interactive state (当前不处于可交互状态)
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
        return nullptr;

    // exist (we need look pets also for some interaction (quest/etc) (查找目标生物或宠物以支持任务等交互)
    Creature* unit = GetMap()->GetAnyTypeCreature(guid);
    if (!unit)
        return nullptr;

    // appropriate npc type (校验 NPC 标志位是否符合交互类型)
    if (npcflagmask && !unit->HasFlag(UNIT_NPC_FLAGS, npcflagmask))
        return nullptr;

    if (npcflagmask == UNIT_NPC_FLAG_STABLEMASTER)
    {
        if (getClass() != CLASS_HUNTER)
            return nullptr;
    }

    if (!CanInteractNow(unit))
        return nullptr;

    if (IsAlive() && unit->isInvisibleForAlive())
        return nullptr;

    // not too far (距离校验：不能超出最大交互距离)
    if (!unit->IsWithinDistInMap(this, INTERACTION_DISTANCE))
        return nullptr;

    return unit;
}

GameObject* Player::GetGameObjectIfCanInteractWith(ObjectGuid guid, uint32 gameobject_type)
{
    // some basic checks (基础状态检查)
    if (!guid || !IsInWorld() || IsTaxiFlying())
        return nullptr;

    // set player as interacting (标记玩家正在交互)
    DoInteraction();

    // not in interactive state (当前不处于可交互状态)
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
        return nullptr;

    if (GameObject* go = GetMap()->GetGameObject(guid))
    {
        if (uint32(go->GetGoType()) == gameobject_type || gameobject_type == MAX_GAMEOBJECT_TYPE)
        {
            if (go->IsAtInteractDistance(this) && go->IsSpawned())
                return go;

            sLog.outError("GetGameObjectIfCanInteractWith: GameObject '%s' [GUID: %u] is too far away from player %s [GUID: %u] to be used by him",
                          go->GetGOInfo()->name,  go->GetGUIDLow(), GetName(), GetGUIDLow());
        }
    }
    return nullptr;
}

bool Player::CanSeeSpecialInfoOf(Unit const* target) const
{
    for (const auto& aura : target->GetAurasByType(SPELL_AURA_EMPATHY))
    {
        if (aura->GetCasterGuid() == GetObjectGuid())
            return true;
    }

    return false;
}

struct SetGameMasterOnHelper
{
    explicit SetGameMasterOnHelper() {}
    void operator()(Unit* unit) const
    {
        unit->setFaction(35);
        unit->getHostileRefManager().clearReferences();
    }
};

struct SetGameMasterOffHelper
{
    explicit SetGameMasterOffHelper(uint32 _faction) : faction(_faction) {}
    void operator()(Unit* unit) const
    {
        unit->setFaction(faction);
        unit->getHostileRefManager().clearReferences();
    }
    uint32 faction;
};

/// 开关玩家的 GM 模式并刷新标志位、阵营状态与聊天徽章
void Player::SetGameMaster(bool on)
{
    if (on)
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_ON;
        setFaction(35);
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
        SetImmuneToNPC(true);
        SetImmuneToPlayer(true);

        CallForAllControlledUnits(SetGameMasterOnHelper(), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);

        FreezeMirrorTimers(true);

        SetPvPFreeForAll(false);
        UpdatePvPContested(false, true);

        CombatStopWithPets();
    }
    else
    {
        m_ExtraFlags &= ~ PLAYER_EXTRA_GM_ON;
        setFactionForRace(getRace());
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
        SetImmuneToNPC(false);
        SetImmuneToPlayer(false);

        CallForAllControlledUnits(SetGameMasterOffHelper(GetFaction()), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);

        FreezeMirrorTimers(false);

        // restore FFA PvP Server state (恢复自由混战 PvP 服务器标志)
        if (sWorld.IsFFAPvPRealm())
            SetPvPFreeForAll(true);

        // restore FFA PvP area state, remove not allowed for GM mounts (恢复自由混战区域状态，并移除非 GM 禁用坐骑)
        UpdateArea(m_areaUpdateId);
    }

    // update dead corpse sparkles (刷新可拾取尸体的闪光状态)
    UnitList deadUnits;
    MaNGOS::AnyDeadUnitCheck u_check(this);
    MaNGOS::UnitListSearcher<MaNGOS::AnyDeadUnitCheck > searcher(deadUnits, u_check);
    Cell::VisitAllObjects(this, searcher, MAX_VISIBILITY_DISTANCE);
    for (auto deadUnit : deadUnits)
    {
        if (deadUnit->GetTypeId() == TYPEID_UNIT)
            deadUnit->ForceValuesUpdateAtIndex(UNIT_DYNAMIC_FLAGS);
    }

    UpdateData data;
    m_camera.UpdateVisibilityForOwner(true, data);
    data.SendData(*GetSession());
    GetMap()->AddUpdateMovementObject(this);
    UpdateEverything();
}

void Player::SetGMVisible(bool on)
{
    if (on)
    {
        m_ExtraFlags &= ~PLAYER_EXTRA_GM_INVISIBLE;         // remove flag (移除 GM 不可见标记)

        // Reapply stealth/invisibility if active or show if not any (重新应用潜行/隐形光环，若无则显形)
        if (HasAuraType(SPELL_AURA_MOD_STEALTH))
            SetVisibility(VISIBILITY_GROUP_STEALTH);
        else if (HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
            SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
        else
            SetVisibility(VISIBILITY_ON);
    }
    else
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_INVISIBLE;          // add flag (添加 GM 不可见标记)

        SetAcceptWhispers(false);
        SetGameMaster(true);

        SetVisibility(VISIBILITY_OFF);
    }
}

bool Player::isAllowedWhisperFrom(ObjectGuid guid)
{
    if (PlayerSocial* social = GetSocial())
        if (social->HasFriend(guid))
            return true;

    if (Group* group = GetGroup())
        if (group->IsMember(guid))
            return true;

    if (Guild* guild = sGuildMgr.GetGuildById(GetGuildId()))
        if (guild->GetMemberSlot(guid))
            return true;

    return false;
}

///- If the player is invited, remove him. If the group if then only 1 person, disband the group. (- 若玩家受邀则移出；若队伍仅剩 1 人则解散队伍)
void Player::UninviteFromGroup()
{
    Group* group = GetGroupInvite();
    if (!group)
        return;

    group->RemoveInvite(this);

    if (group->IsCreated())
    {
        if (group->GetMembersCount() < group->GetMembersMinCount()) // group has not enough members to exist => disband and destroy (队伍成员不足，解散并销毁队伍)
        {
            group->Disband(true);
            sObjectMgr.RemoveGroup(group);
            delete group;
        }
    }
    else
    {
        if (group->GetInviteesCount() <= 1) // group creation was cancelled due to everyone declining invitations => destroy (所有人拒绝邀请导致组队取消，销毁队伍)
        {
            group->RemoveAllInvites();
            delete group;
        }
    }
}

void Player::RemoveFromGroup(Group* group, ObjectGuid guid)
{
    if (group)
    {
        if (group->RemoveMember(guid, 0) <= 1)
        {
            // group->Disband(); already disbanded in RemoveMember (RemoveMember 内部已处理队伍解散)
            sObjectMgr.RemoveGroup(group);
            delete group;
            // removemember sets the player's group pointer to nullptr (RemoveMember 会将玩家的队伍指针置空)
        }
    }
}

void Player::SendLogXPGain(uint32 GivenXP, Unit* victim, uint32 RestXP, bool recruitAFriend, float groupRate) const
{
    WorldPacket data(SMSG_LOG_XPGAIN, 21);
    data << (victim ? victim->GetObjectGuid() : ObjectGuid());// guid (击杀目标 GUID)
    data << uint32(GivenXP + RestXP);                       // given experience (实际获得经验值)
    data << uint8(victim ? 0 : 1);                          // 00-kill_xp type, 01-non_kill_xp type (类型：0 为击杀经验，1 为非击杀经验)
    if (victim)
    {
        data << uint32(GivenXP);                            // experience without rested bonus (未计入双倍加成的经验值)
        data << float(groupRate);                           // 1 - none 0 - 100% raid bonus penalty 100%+ group bonus (组队/团队经验倍率)
    }
    data << uint8(recruitAFriend);                          // Refer-A-Friend bonus used (是否触发战友招募 3 倍经验加成)
    GetSession()->SendPacket(data);
}

/// 给予玩家经验值（处理双倍休息加成、战友招募加成及满级拦截）
void Player::GiveXP(uint32 xp, Creature* victim, float groupRate)
{
    if (xp < 1)
        return;

    if (!IsAlive())
        return;

    uint32 level = GetLevel();

    // XP to money conversion processed in Player::RewardQuest (满级任务经验转金币在 Player::RewardQuest 中处理)
    if (level >= GetMaxAttainableLevel())
        return;

    // handle SPELL_AURA_MOD_XP_PCT auras (应用经验百分比加成光环)
    Unit::AuraList const& ModXPPctAuras = GetAurasByType(SPELL_AURA_MOD_XP_PCT);
    for (auto ModXPPctAura : ModXPPctAuras)
        xp = uint32(xp * (1.0f + ModXPPctAura->GetModifier()->m_amount / 100.0f));

    // XP resting bonus for kill (计算击杀的休息双倍经验加成)
    uint32 bonus_xp;
    bool recruitAFriend = GetsRecruitAFriendBonus();
    // RaF does NOT stack with rested experience (战友招募经验加成不与休息双倍经验叠加)
    if (recruitAFriend)
        bonus_xp = 2 * xp; // xp + bonus_xp must add up to 3 * xp for RaF; calculation for quests done client-side (战友招募总经验为 3 倍；任务经验的加成显示由客户端处理)
    else
        bonus_xp = victim ? GetXPRestBonus(xp) : 0;

    SendLogXPGain(xp, victim, bonus_xp, recruitAFriend, groupRate);

    uint32 curXP = GetUInt32Value(PLAYER_XP);
    uint32 nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    uint32 newXP = curXP + xp + bonus_xp;

    while (newXP >= nextLvlXP && level < GetMaxAttainableLevel())
    {
        newXP -= nextLvlXP;

        if (level < GetMaxAttainableLevel())
            GiveLevel(level + 1);

        level = GetLevel();
        nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    }

    SetUInt32Value(PLAYER_XP, newXP);
}

// Update player to next level (将玩家升级至指定等级)
// Current player experience not update (must be update by caller) (当前经验值不在此处更新，由调用方处理)
/// 将玩家提升至指定等级并刷新属性、技能上限、天赋与宠物等级
void Player::GiveLevel(uint32 level)
{
    if (level == GetLevel())
        return;

    uint32 oldLevel = GetLevel();
    uint32 plClass = getClass();

    PlayerLevelInfo info;
    sObjectMgr.GetPlayerLevelInfo(getRace(), plClass, level, &info);

    PlayerClassLevelInfo classInfo;
    sObjectMgr.GetPlayerClassLevelInfo(plClass, level, &classInfo);

    // send levelup info to client (向客户端发送升级通知包)
    WorldPacket data(SMSG_LEVELUP_INFO, (4 + 4 + MAX_POWERS * 4 + MAX_STATS * 4));
    data << uint32(level);
    data << uint32(int32(classInfo.basehealth) - int32(GetCreateHealth()));
    // for(int i = 0; i < MAX_POWERS; ++i)                  // Powers loop (0-6) (能量循环 0-6)
    data << uint32(int32(classInfo.basemana)   - int32(GetCreateMana()));
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    // end for (循环结束)
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)         // Stats loop (0-4) (五大基础属性变化循环 0-4)
        data << uint32(int32(info.stats[i]) - GetCreateStat(Stats(i)));

    GetSession()->SendPacket(data);

    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, GetMaxAttainableLevel() <= level ? 0 : sObjectMgr.GetXPForLevel(level));

    // update level, max level of skills (更新角色等级与技能上限)
    if (GetLevel() != level)
        m_Played_time[PLAYED_TIME_LEVEL] = 0;               // Level Played Time reset (重置当前等级游戏时间统计)
    SetLevel(level);
    UpdateSkillsForLevel();

    // save base values (bonuses already included in stored stats (保存基础属性值)
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);
    SetCreateMana(classInfo.basemana);

    InitTalentForLevel();
    InitTaxiNodesForLevel();

    UpdateAllStats();

    // set current level health and mana/energy to maximum after applying all mods. (应用所有属性修正后将生命与法力/能量设为满值)
    if (IsAlive())
        SetHealth(GetMaxHealth());
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));

    // update level to hunter/summon pet (同步更新猎人/术士宠物等级)
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();

    for (uint32 curLevel = oldLevel + 1; curLevel <= level; ++curLevel)
    {
        if (MailLevelReward const* mailReward = sObjectMgr.GetMailLevelReward(curLevel, getRaceMask()))
            MailDraft(mailReward->mailTemplateId).SendMailTo(this, MailSender(MAIL_CREATURE, mailReward->senderEntry));
    }

    // Refer-A-Friend (战友招募可赠送等级计算)
    if (!GetSession()->IsARecruiter() && GetSession()->GetRecruitingFriendId())
        if (level < sWorld.getConfig(CONFIG_UINT32_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL))
            if (level % 2 == 0)
            {
                ++m_grantableLevels;

                if (!HasByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_RAF_GRANTABLE_LEVEL, 0x01))
                    SetByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_RAF_GRANTABLE_LEVEL, 0x01);
            }

    // resend quests status directly (刷新可接/可交任务 NPC 状态图标)
    GetSession()->SetCurrentPlayerLevel(level);
    SendQuestGiverStatusMultiple();
}

void Player::UpdateFreeTalentPoints(bool resetIfNeed)
{
    uint32 level = GetLevel();
    // talents base at level diff ( talents = level - 9 but some can be used already) (根据等级差计算可用天赋点总数)
    if (level < 10)
    {
        // Remove all talent points (10 级以下清空所有天赋点)
        if (m_usedTalentCount > 0)                          // Free any used talents (退还已点天赋)
        {
            if (resetIfNeed)
                resetTalents(true);
            SetFreeTalentPoints(0);
        }
    }
    else
    {
        uint32 talentPointsForLevel = CalculateTalentsPoints();

        // if used more that have then reset (若已用天赋点超出当前等级上限则重置天赋)
        if (m_usedTalentCount > talentPointsForLevel)
        {
            if (resetIfNeed && GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
                resetTalents(true);
            else
                SetFreeTalentPoints(0);
        }
        // else update amount of free points (否则更新剩余可用天赋点数)
        else
            SetFreeTalentPoints(talentPointsForLevel - m_usedTalentCount);
    }
}

void Player::InitTalentForLevel()
{
    UpdateFreeTalentPoints();
}

/// 按当前等级与种族/职业模板重新初始化基础属性、生命法力与战斗字段
void Player::InitStatsForLevel(bool reapplyMods)
{
    if (reapplyMods)                                        // reapply stats values only on .reset stats (level) command (仅在执行 .reset stats/level 命令时重算属性)
        _RemoveAllStatBonuses();

    PlayerClassLevelInfo classInfo;

    uint32 level = GetLevel();
    uint32 plClass = getClass();
    sObjectMgr.GetPlayerClassLevelInfo(plClass, level, &classInfo);

    PlayerLevelInfo info;
    sObjectMgr.GetPlayerLevelInfo(getRace(), plClass, level, &info);

    SetUInt32Value(PLAYER_FIELD_MAX_LEVEL, sObjectMgr.GetMaxLevelForExpansion(GetSession()->GetExpansion()));
    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, GetMaxAttainableLevel() <= level ? 0 : sObjectMgr.GetXPForLevel(level));

    // reset before any aura state sources (health set/aura apply) (在应用任何光环前重置角色基础状态)
    SetUInt32Value(UNIT_FIELD_AURASTATE, 0);

    UpdateSkillsForLevel();

    // set default cast time multiplier (设置默认施法时间倍率)
    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);

    // save base values (bonuses already included in stored stats (保存当前等级基础五维属性)
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);

    // set create powers (初始化能量类型与基础能量值)
    SetCreateMana(classInfo.basemana);

    SetArmor(int32(m_createStats[STAT_AGILITY] * 2));

    InitStatBuffMods();

    // reset rating fields values (重置所有战斗等级字段)
    for (uint16 index = PLAYER_FIELD_COMBAT_RATING_1; index < PLAYER_FIELD_COMBAT_RATING_1 + MAX_COMBAT_RATING; ++index)
        SetUInt32Value(index, 0);

    SetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, 0);
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i, 0);
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, 0);
        SetFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + i, 1.00f);
    }

    // reset attack power, damage and attack speed fields (重置攻击强度、伤害与攻击速度字段)
    SetFloatValue(UNIT_FIELD_BASEATTACKTIME, 2000.0f);
    SetFloatValue(UNIT_FIELD_BASEATTACKTIME + 1, 2000.0f);  // offhand attack time (副手攻击间隔)
    SetFloatValue(UNIT_FIELD_RANGEDATTACKTIME, 2000.0f);

    SetFloatValue(UNIT_FIELD_MINDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, 0.0f);

    SetInt32Value(UNIT_FIELD_ATTACK_POWER,            0);
    SetInt32Value(UNIT_FIELD_ATTACK_POWER_MODS,       0);
    SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, 0.0f);
    SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER,     0);
    SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER_MODS, 0);
    SetFloatValue(UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER, 0.0f);

    // Base crit values (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset (重置基础暴击率，后续在 UpdateAllStats 中重算)
    SetFloatValue(PLAYER_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_OFFHAND_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE, 0.0f);

    // Init spell schools (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset (初始化各系法术暴击/伤害字段)
    for (uint8 i = 0; i < MAX_SPELL_SCHOOL; ++i)
        SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + i, 0.0f);

    SetFloatValue(PLAYER_PARRY_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_BLOCK_PERCENTAGE, 0.0f);
    SetUInt32Value(PLAYER_SHIELD_BLOCK, 0);

    // Dodge percentage (躲闪率)
    SetFloatValue(PLAYER_DODGE_PERCENTAGE, 0.0f);

    // set armor (resistance 0) to original value (create_agility*2) (将基础护甲重置为初始敏捷*2)
    SetArmor(int32(m_createStats[STAT_AGILITY] * 2));
    SetResistanceBuffMods(SpellSchools(0), true, 0.0f);
    SetResistanceBuffMods(SpellSchools(0), false, 0.0f);
    // set other resistance to original value (0) (重置其余魔法抗性为 0)
    for (int i = 1; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetResistance(SpellSchools(i), 0);
        SetResistanceBuffMods(SpellSchools(i), true, 0.0f);
        SetResistanceBuffMods(SpellSchools(i), false, 0.0f);
    }

    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, 0);
    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE, 0);
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + i, 0);
        SetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + i, 0.0f);
    }
    // Init data for form but skip reapply item mods for form (初始化变形形态数据，暂不重算装备加成)
    InitDataForForm(reapplyMods);

    // save new stats (保存重新初始化后的属性)
    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
        SetMaxPower(Powers(i),  GetCreatePowers(Powers(i)));

    SetMaxHealth(classInfo.basehealth);                     // stamina bonus will applied later (耐力额外加成将在后续应用)

    // cleanup mounted state (it will set correctly at aura loading if player saved at mount. (清理坐骑状态，加载光环时会按需恢复)
    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);

    // cleanup unit flags (will be re-applied if need at aura load). (清理单位标志位，加载光环时按需恢复)
    RemoveFlag(UNIT_FIELD_FLAGS,
               UNIT_FLAG_UNK_0 | UNIT_FLAG_SPAWNING | UNIT_FLAG_CLIENT_CONTROL_LOST | UNIT_FLAG_NOT_ATTACKABLE_1 |
               UNIT_FLAG_IMMUNE_TO_PLAYER | UNIT_FLAG_IMMUNE_TO_NPC    | UNIT_FLAG_LOOTING          |
               UNIT_FLAG_PET_IN_COMBAT  | UNIT_FLAG_SILENCED     | UNIT_FLAG_PACIFIED         |
               UNIT_FLAG_STUNNED        | UNIT_FLAG_IN_COMBAT    | UNIT_FLAG_DISARMED         |
               UNIT_FLAG_CONFUSED       | UNIT_FLAG_FLEEING      | UNIT_FLAG_POSSESSED        |
               UNIT_FLAG_UNINTERACTIBLE | UNIT_FLAG_SKINNABLE    | UNIT_FLAG_MOUNT            |
               UNIT_FLAG_TAXI_FLIGHT    | UNIT_FLAG_PREVENT_ANIM);
    SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);    // must be set (必须保留玩家控制标志位)

    // cleanup player flags (will be re-applied if need at aura load), to avoid have ghost flag without ghost aura, for example. (清理玩家标志位，避免出现无幽灵光环却残留幽灵标志等异常)
    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK | PLAYER_FLAGS_DND | PLAYER_FLAGS_GM | PLAYER_FLAGS_GHOST | PLAYER_FLAGS_PVP_DESIRED | PLAYER_FLAGS_FFA_PVP | PLAYER_FLAGS_TAXI_BENCHMARK | PLAYER_FLAGS_COMMENTATOR | PLAYER_FLAGS_COMMENTATOR_UBER);

    RemoveVisFlags(UNIT_VIS_FLAGS_ALL);                   // one form stealth modified bytes (重置潜行/侦测相关字节标志)

    // restore if need some important flags (按需恢复关键玩家标志位)
    SetUInt32Value(PLAYER_FIELD_BYTES2, 0);                 // flags empty by default (默认不保留标志位)

    if (reapplyMods)                                        // reapply stats values only on .reset stats (level) command (重算物品与光环属性加成)
        _ApplyAllStatBonuses();

    // set current level health and mana/energy to maximum after applying all mods. (应用修正后将生命与能量恢复至上限)
    SetHealth(GetMaxHealth());
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));

    // update level to hunter/summon pet (同步更新宠物等级)
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();
}

void Player::OnExpansionChange()
{
    SetUInt32Value(PLAYER_FIELD_MAX_LEVEL, sObjectMgr.GetMaxLevelForExpansion(GetSession()->GetExpansion()));
    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, GetMaxAttainableLevel() <= GetLevel() ? 0 : sObjectMgr.GetXPForLevel(GetLevel()));
}

void Player::SendInitialSpells() const
{
    uint16 spellCount = 0;

    WorldPacket data(SMSG_INITIAL_SPELLS, (1 + 2 + 4 * m_spells.size() + 2 + m_cooldownMap.size() * (2 + 2 + 2 + 4 + 4)));
    data << uint8(0);

    size_t countPos = data.wpos();
    data << uint16(spellCount);                             // spell count placeholder (法术数量占位符)

    for (const auto& m_spell : m_spells)
    {
        PlayerSpell const& playerSpell = m_spell.second;

        if (playerSpell.state == PLAYERSPELL_REMOVED)
            continue;

        if (!playerSpell.active || playerSpell.disabled)
            continue;

        data << uint16(m_spell.first);
        data << uint16(0);                                  // it's not slot id (此处为 0 占位，非槽位 ID)

        spellCount += 1;
    }

    data.put<uint16>(countPos, spellCount);                 // write real count value (回填实际发送的法术数量)

    // write cooldown data (写入法术冷却数据)
    uint32 cdCount = 0;
    const size_t cdCountPos = data.wpos();
    data << uint16(0);
    auto currTime = GetMap()->GetCurrentClockTime();

    for (auto& cdItr : m_cooldownMap)
    {
        auto& cdData = cdItr.second;
        TimePoint spellRecTime = currTime;
        TimePoint catRecTime = currTime;
        cdData->GetSpellCDExpireTime(spellRecTime);
        cdData->GetCatCDExpireTime(catRecTime);
        uint32 spellCDDuration = 0;
        uint32 catCDDuration = 0;
        if (spellRecTime > currTime)
            spellCDDuration = std::chrono::duration_cast<std::chrono::milliseconds>(spellRecTime - currTime).count();
        if (catRecTime > currTime)
            catCDDuration = std::chrono::duration_cast<std::chrono::milliseconds>(catRecTime - currTime).count();

        if (!spellCDDuration && !catCDDuration && !cdData->IsPermanent())
            continue;

        if (cdData->IsPermanent())
        {
            spellCDDuration = uint32(1);                              // cooldown (剩余单法术冷却毫秒数)
            catCDDuration |= 0x80000000;
        }

        data << uint16(cdData->GetSpellId());
        data << uint16(cdData->GetItemId());                // cast item id (触发冷却的物品 ID)
        data << uint16(cdData->GetCategory());              // spell category (法术冷却类别)
        data << uint32(spellCDDuration);                    // cooldown (单法术冷却时间)
        data << uint32(catCDDuration);                      // category cooldown (类别冷却时间)
        ++cdCount;
    }

    data.put<uint16>(cdCountPos, cdCount);

    GetSession()->SendPacket(data);

    DETAIL_LOG("CHARACTER: Sent Initial Spells");
}

void Player::SendUnlearnSpells() const
{
    WorldPacket data(SMSG_SEND_UNLEARN_SPELLS, 4 + 4 * m_spells.size());

    uint32 spellCount = 0;
    size_t countPos = data.wpos();
    data << uint32(spellCount);                             // spell count placeholder (数量占位符)

    for (auto& spellData : m_spells)
    {
        if (spellData.second.state == PLAYERSPELL_REMOVED)
            continue;

        if (spellData.second.active || spellData.second.disabled)
            continue;

        auto skillLineAbilities = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(spellData.first);
        if (skillLineAbilities.first == skillLineAbilities.second)
            continue;

        bool hasSupercededSpellInfoInClient = false;
        for (auto boundsItr = skillLineAbilities.first; boundsItr != skillLineAbilities.second; ++boundsItr)
        {
            if (boundsItr->second->forward_spellid)
            {
                hasSupercededSpellInfoInClient = true;
                break;
            }
        }

        if (hasSupercededSpellInfoInClient)
            continue;

        uint32 nextRank = sSpellMgr.GetNextSpellInChain(spellData.first);
        if (!nextRank || !HasSpell(nextRank))
            continue;

        data << uint32(spellData.first);

        ++spellCount;
    }

    data.put<uint32>(countPos, spellCount);                  // write real count value (回填实际写入数量)

    SendDirectMessage(data);
}

void Player::SendSupercededSpell(uint32 oldSpell, uint32 newSpell) const
{
    WorldPacket data(SMSG_SUPERCEDED_SPELL, (4));
    data << uint16(oldSpell);
    data << uint16(newSpell);
    GetSession()->SendPacket(data);
}

void Player::SendRemovedSpell(uint32 spellId) const
{
    WorldPacket data(SMSG_REMOVED_SPELL, 4);
    data << uint16(spellId);
    GetSession()->SendPacket(data);
}

void Player::RemoveMail(uint32 id)
{
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        if ((*itr)->messageID == id)
        {
            // do not delete item, because Player::removeMail() is called when returning mail to sender. (不在此处删除物品，退信时仍需保留附件)
            m_mail.erase(itr);
            return;
        }
    }
}

void Player::SendMailResult(uint32 mailId, MailResponseType mailAction, MailResponseResult mailError, uint32 equipError, uint32 item_guid, uint32 item_count) const
{
    WorldPacket data(SMSG_SEND_MAIL_RESULT, (4 + 4 + 4 + (mailError == MAIL_ERR_EQUIP_ERROR ? 4 : (mailAction == MAIL_ITEM_TAKEN ? 4 + 4 : 0))));
    data << (uint32) mailId;
    data << (uint32) mailAction;
    data << (uint32) mailError;
    if (mailError == MAIL_ERR_EQUIP_ERROR)
        data << (uint32) equipError;
    else if (mailAction == MAIL_ITEM_TAKEN)
    {
        data << (uint32) item_guid;                         // item guid low? (物品低位 GUID，非附件操作填 0)
        data << (uint32) item_count;                        // item count? (物品数量，非附件操作填 0)
    }
    GetSession()->SendPacket(data);
}

void Player::SendNewMail() const
{
    // deliver undelivered mail (投递已到时间的未送达邮件)
    WorldPacket data(SMSG_RECEIVED_MAIL, 4);
    data << (uint32) 0;
    GetSession()->SendPacket(data);
}

void Player::UpdateNextMailTimeAndUnreads()
{
    // calculate next delivery time (min. from non-delivered mails (计算下次邮件投递时间，取未送达邮件的最早投递时刻，)
    // and recalculate unReadMail (并重新统计未读邮件数)
    time_t cTime = time(nullptr);
    m_nextMailDelivereTime = 0;
    unReadMails = 0;
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        if ((*itr)->deliver_time > cTime)
        {
            if (!m_nextMailDelivereTime || m_nextMailDelivereTime > (*itr)->deliver_time)
                m_nextMailDelivereTime = (*itr)->deliver_time;
        }
        else if (((*itr)->checked & MAIL_CHECK_MASK_READ) == 0)
            ++unReadMails;
    }
}

void Player::AddNewMailDeliverTime(time_t deliver_time)
{
    if (deliver_time <= time(nullptr))                         // ready now (立即就绪)
    {
        ++unReadMails;
        SendNewMail();
    }
    else                                                    // not ready and no have ready mails (尚未就绪且当前无待投递邮件)
    {
        if (!m_nextMailDelivereTime || m_nextMailDelivereTime > deliver_time)
            m_nextMailDelivereTime =  deliver_time;
    }
}

static inline bool IsUnlearnSpellsPacketNeededForSpell(uint32 spellId)
{
    return sSpellMgr.GetSpellBookSuccessorSpellId(spellId);
}

/// 向玩家添加或更新法术（处理分级替换、依赖技能、天赋点数与被动激活）
bool Player::addSpell(uint32 spell_id, bool active, bool learning, bool dependent, bool disabled)
{
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
    if (!spellInfo)
    {
        // do character spell book cleanup (all characters) (清理角色法术书中的冗余低阶法术)
        if (!IsInWorld() && !learning)                      // spell load case (法术加载时保留完整学习链)
        {
            sLog.outError("Player::addSpell: nonexistent in SpellStore spell #%u request, deleting for all characters in `character_spell`.", spell_id);
            CharacterDatabase.PExecute("DELETE FROM character_spell WHERE spell = '%u'", spell_id);
        }
        else
            sLog.outError("Player::addSpell: nonexistent in SpellStore spell #%u request.", spell_id);

        return false;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, this, false))
    {
        // do character spell book cleanup (all characters) (非加载时清理)
        if (!IsInWorld() && !learning)                      // spell load case (法术加载时检查是否需要发送遗忘包)
        {
            sLog.outError("Player::addSpell: Broken spell #%u learning not allowed, deleting for all characters in `character_spell`.", spell_id);
            CharacterDatabase.PExecute("DELETE FROM character_spell WHERE spell = '%u'", spell_id);
        }
        else
            sLog.outError("Player::addSpell: Broken spell #%u learning not allowed.", spell_id);

        return false;
    }

    PlayerSpellState state = learning ? PLAYERSPELL_NEW : PLAYERSPELL_UNCHANGED;

    bool disabled_case = false;
    bool superceded_old = false;

    PlayerSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr != m_spells.end())
    {
        uint32 next_active_spell_id = 0;
        bool dependent_set = false;

        // fix activate state for non-stackable low rank (and find next spell for !active case) (修正不可叠加法术的激活状态并查找高阶/低阶法术)
        if (uint32 nextId = sSpellMgr.GetSpellBookSuccessorSpellId(spellInfo->Id))
        {
            if (HasSpell(nextId))
            {
                // high rank already known so this must !active (已学会更高阶法术，故当前阶设为非激活)
                active = false;
                next_active_spell_id = nextId;
            }
        }

        PlayerSpell& playerSpell = itr->second;

        // not do anything if already known in expected state (若法术已处于预期状态则跳过)
        if (playerSpell.state != PLAYERSPELL_REMOVED && playerSpell.active == active &&
                playerSpell.dependent == dependent && playerSpell.disabled == disabled)
        {
            if (!IsInWorld() && !learning)                  // explicitly load from DB and then exist in it already and set correctly (从数据库加载时记录已存在，直接标记为未变更)
                playerSpell.state = PLAYERSPELL_UNCHANGED;

            return false;
        }

        // dependent spell known as not dependent, overwrite state (已独立学会的法术覆盖为非依赖状态)
        if (playerSpell.state != PLAYERSPELL_REMOVED && !playerSpell.dependent && dependent)
        {
            playerSpell.dependent = dependent;
            if (playerSpell.state != PLAYERSPELL_NEW)
                playerSpell.state = PLAYERSPELL_CHANGED;
            dependent_set = true;
        }

        // update active state for known spell (更新已知法术的激活状态)
        if (playerSpell.active != active && playerSpell.state != PLAYERSPELL_REMOVED && !playerSpell.disabled)
        {
            playerSpell.active = active;

            if (!IsInWorld() && !learning && !dependent_set)// explicitly load from DB and then exist in it already and set correctly (从数据库加载时设为未变更)
                playerSpell.state = PLAYERSPELL_UNCHANGED;
            else if (playerSpell.state != PLAYERSPELL_NEW)
                playerSpell.state = PLAYERSPELL_CHANGED;

            if (active)
            {
                if (IsNeedCastPassiveLikeSpellAtLearn(spellInfo))
                    CastSpell(this, spell_id, TRIGGERED_OLD_TRIGGERED);
            }
            else if (IsInWorld())
            {
                if (next_active_spell_id)
                {
                    // update spell ranks in spellbook and action bar (更新法术书与动作条中的法术等级显示)
                    SendSupercededSpell(spell_id, next_active_spell_id);
                    if (IsUnlearnSpellsPacketNeededForSpell(spell_id))
                        SendUnlearnSpells();
                }
                else
                    SendRemovedSpell(spell_id);
            }

            return active;                                  // learn (show in spell book if active now) (新增学习该法术)
        }

        if (playerSpell.disabled != disabled && playerSpell.state != PLAYERSPELL_REMOVED)
        {
            if (playerSpell.state != PLAYERSPELL_NEW)
                playerSpell.state = PLAYERSPELL_CHANGED;
            playerSpell.disabled = disabled;

            if (disabled)
                return false;

            disabled_case = true;
        }
        else switch (playerSpell.state)
            {
                case PLAYERSPELL_UNCHANGED:                 // known saved spell (已知且已存库的法术)
                    return false;
                case PLAYERSPELL_REMOVED:                   // re-learning removed not saved spell (重新学习此前标记移除但尚未存库的法术)
                {
                    m_spells.erase(itr);
                    state = PLAYERSPELL_CHANGED;
                    break;                                  // need re-add (需重新标记为新增或已变更)
                }
                default:                                    // known not saved yet spell (new or modified) (已知但尚未存库的法术)
                {
                    // can be in case spell loading but learned at some previous spell loading (可能在加载前期关联法术时已顺带习得)
                    if (!IsInWorld() && !learning && !dependent_set)
                        playerSpell.state = PLAYERSPELL_UNCHANGED;

                    return false;
                }
            }
    }

    TalentSpellPos const* talentPos = GetTalentSpellPos(spell_id);

    if (!disabled_case) // skip new spell adding if spell already known (disabled spells case) (若法术已存在则跳过添加)
    {
        // talent: unlearn all lower talent ranks (天赋法术：遗忘所有低等级天赋)
        if (talentPos)
        {
            if (TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentPos->talent_id))
            {
                for (unsigned int rankSpellId : talentInfo->RankID)
                {
                    // skip learning spell and no rank spell case (跳过学习型法术及无等级法术)
                    if (!rankSpellId)
                        continue;

                    if (rankSpellId == spell_id)
                        break;

                    removeSpell(rankSpellId, false, false);
                }
            }
        }
        // non talent spell: learn low ranks (recursive call) (非天赋法术：若需要则递归学习前置低等级)
        else if (uint32 prev_spell = sSpellMgr.GetPrevSpellInChain(spell_id))
        {
            // Check if a spell is learned by a talent first (先检查低等级法术是否属于当前未学的天赋)
            bool talent = false;
            for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
            {
                TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
                if (!talentInfo)
                    continue;
                TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
                if (!talentTabInfo || !(getClassMask() & talentTabInfo->ClassMask))
                    continue;
                uint32 parent = 0;
                for (unsigned int rank : talentInfo->RankID)
                {
                    if (rank && sSpellMgr.IsSpellLearnToSpell(rank, prev_spell))
                    {
                        parent = rank;
                        break;
                    }
                }
                if (parent)
                {
                    talent = !HasSpell(parent);
                    break;
                }
            }

            if (!talent)
            {
                // We do not learn previous rank if its owned by a talent we don't know (若前置等级由未点亮的天赋授予则不自动学习)
                if (!IsInWorld() || disabled)        // at spells loading, no output, but allow save (法术加载阶段不向客户端发包，但允许存库)
                    addSpell(prev_spell, active, true, true, disabled);
                else                                            // at normal learning (正常游戏内学习阶段，向客户端通知)
                    learnSpell(prev_spell, true);
            }
        }

        PlayerSpell newspell;
        newspell.state     = state;
        newspell.active    = active;
        newspell.dependent = dependent;
        newspell.disabled  = disabled;

        bool needsUnlearnSpellsPacket = false;

        // replace spells in action bars and spellbook to bigger rank if only one spell rank must be accessible (若仅允许保留最高阶，则替换动作条与法术书中的旧等级)
        if (newspell.active && !newspell.disabled)
        {
            for (auto& m_spell : m_spells)
            {
                PlayerSpell& playerSpell2 = m_spell.second;

                if (playerSpell2.state == PLAYERSPELL_REMOVED)
                    continue;

                if (uint32 nextId = sSpellMgr.GetSpellBookSuccessorSpellId(m_spell.first))
                {
                    if (playerSpell2.active)
                    {
                        if (nextId == spell_id)
                        {
                            if (IsInWorld())                // not send spell (re-/over-)learn packets at loading (加载阶段不发送覆盖/重学法术数据包)
                            {
                                SendSupercededSpell(m_spell.first, spell_id);
                                needsUnlearnSpellsPacket = needsUnlearnSpellsPacket || IsUnlearnSpellsPacketNeededForSpell(m_spell.first);
                            }

                            // mark old spell as disable (SMSG_SUPERCEDED_SPELL replace it in client by new) (将旧等级法术标记为禁用，客户端将由新法术替代)
                            playerSpell2.active = false;
                            if (playerSpell2.state != PLAYERSPELL_NEW)
                                playerSpell2.state = PLAYERSPELL_CHANGED;
                            superceded_old = true;          // new spell replace old in action bars and spell book. (新法术在动作条与法术书中替换旧法术)
                        }
                        else if (m_spell.first == spell_id)
                        {
                            if (IsInWorld())                // not send spell (re-/over-)learn packets at loading (加载阶段不发送覆盖法术数据包)
                            {
                                SendSupercededSpell(spell_id, m_spell.first);
                                needsUnlearnSpellsPacket = needsUnlearnSpellsPacket || IsUnlearnSpellsPacketNeededForSpell(spell_id);
                            }

                            // mark new spell as disable (not learned yet for client and will not learned) (将新等级标记为禁用：当前未向客户端下发学习)
                            newspell.active = false;
                            if (newspell.state != PLAYERSPELL_NEW)
                                newspell.state = PLAYERSPELL_CHANGED;
                            playerSpell2.disabled = false;
                            playerSpell2.state = PLAYERSPELL_CHANGED;
                        }
                    }
                }
            }
        }

        m_spells[spell_id] = newspell;

        if (needsUnlearnSpellsPacket)
            SendUnlearnSpells();

        // return false if spell disabled (若法术被禁用则返回 false)
        if (newspell.disabled)
            return false;
    }

    if (talentPos)
    {
        // update used talent points count (更新已使用天赋点计数)
        m_usedTalentCount += GetTalentSpellCost(talentPos);
        UpdateFreeTalentPoints(false);
    }

    // update free primary prof.points (if any, can be none in case GM .learn prof. learning) (更新剩余专业技能点数)
    if (uint32 freeProfs = GetFreePrimaryProfessionPoints())
    {
        if (sSpellMgr.IsPrimaryProfessionFirstRankSpell(spell_id))
            SetFreePrimaryProfessions(freeProfs - 1);
    }

    // cast talents with SPELL_EFFECT_LEARN_SPELL (other dependent spells will learned later as not auto-learned) (施放带 SPELL_EFFECT_LEARN_SPELL 效果的天赋法术)
    // note: all spells with SPELL_EFFECT_LEARN_SPELL isn't passive (注：带学习效果的法术本身并非被动法术)
    if (talentPos && IsSpellHaveEffect(spellInfo, SPELL_EFFECT_LEARN_SPELL))
    {
        // ignore stance requirement for talent learn spell (stance set for spell only for client spell description show) (施放天赋学习法术时忽略姿态限制，姿态仅用于客户端说明展示)
        CastSpell(this, spell_id, TRIGGERED_OLD_TRIGGERED);
    }
    // also cast passive (and passive like) spells (including all talents without SPELL_EFFECT_LEARN_SPELL) with additional checks (同时施放被动及类被动法术，含无学习效果的天赋)
    else if (IsNeedCastPassiveLikeSpellAtLearn(spellInfo))
    {
        CastSpell(this, spell_id, TRIGGERED_OLD_TRIGGERED);
    }
    else if (IsSpellHaveEffect(spellInfo, SPELL_EFFECT_SKILL_STEP))
    {
        CastSpell(this, spell_id, TRIGGERED_OLD_TRIGGERED);
        return false;
    }

    // Add dependent skills (添加关联技能)
    UpdateSpellTrainedSkills(spell_id, true);

    // learn dependent spells (学习关联派生法术)
    SpellLearnSpellMapBounds spell_bounds = sSpellMgr.GetSpellLearnSpellMapBounds(spell_id);

    for (SpellLearnSpellMap::const_iterator itr2 = spell_bounds.first; itr2 != spell_bounds.second; ++itr2)
    {
        SpellLearnSpellNode const& spellLearn = itr2->second;
        if (!spellLearn.autoLearned)
        {
            if (!IsInWorld() || !spellLearn.active)       // at spells loading, no output, but allow save (法术加载阶段无通知输出，但允许存库)
                addSpell(spellLearn.spell, spellLearn.active, true, true, false);
            else                                            // at normal learning (正常游戏内学习阶段)
                learnSpell(spellLearn.spell, true);
        }
    }

    // return true (for send learn packet) only if spell active (in case ranked spells) and not replace old spell (仅当法术激活且未直接替代旧法术时返回 true 以便发送学习通知)
    return active && !disabled && !superceded_old;
}

bool Player::IsNeedCastPassiveLikeSpellAtLearn(SpellEntry const* spellInfo) const
{
    ShapeshiftForm form = GetShapeshiftForm();

    if (spellInfo->HasAttribute(SPELL_ATTR_EX_CAST_WHEN_LEARNED))
        return true;

    if (IsNeedCastSpellAtFormApply(spellInfo, form))        // SPELL_ATTR_PASSIVE | SPELL_ATTR_DO_NOT_DISPLAY spells (被动或隐藏显示法术)
        return true;                                        // all stance req. cases, not have auarastate cases (仅检查姿态要求，不涉及光环状态)

    if (!spellInfo->HasAttribute(SPELL_ATTR_PASSIVE))
        return false;

    // note: form passives activated with shapeshift spells be implemented by HandleShapeshiftBoosts instead of spell_learn_spell (注：变形形态被动由 HandleShapeshiftBoosts 处理，不通过 spell_learn_spell)
    // talent dependent passives activated at form apply have proper stance data (随变形激活的天赋被动自带正确姿态数据)
    bool need_cast = !spellInfo->Stances || (!form && spellInfo->HasAttribute(SPELL_ATTR_EX2_ALLOW_WHILE_NOT_SHAPESHIFTED));

    // Check CasterAuraStates (检查施法者光环状态要求)
    return need_cast && (!spellInfo->CasterAuraState || HasAuraState(AuraState(spellInfo->CasterAuraState)));
}

/// 正式学习指定法术并递归处理低阶前置与高阶依赖
void Player::learnSpell(uint32 spell_id, bool dependent, bool talent)
{
    PlayerSpellMap::iterator itr = m_spells.find(spell_id);

    bool disabled = (itr != m_spells.end()) ? itr->second.disabled : false;
    bool active = disabled ? itr->second.active : true;

    bool learning = addSpell(spell_id, active, true, dependent, false);

    // prevent duplicated entires in spell book, also not send if not in world (loading) (防止法术书出现重复条目；未在世界中时不发送学习包)
    if (learning && IsInWorld())
    {
        WorldPacket data(SMSG_LEARNED_SPELL, 4);
        data << uint32(spell_id);
        GetSession()->SendPacket(data);
    }

    // learn all disabled higher ranks (recursive) - skip for talent spells (递归学习此前禁用的更高阶法术，天赋除外)
    if (disabled || talent)
    {
        SpellChainMapNext const& nextMap = sSpellMgr.GetSpellChainNext();
        for (SpellChainMapNext::const_iterator i = nextMap.lower_bound(spell_id); i != nextMap.upper_bound(spell_id); ++i)
        {
            PlayerSpellMap::iterator iter = m_spells.find(i->second);
            if (iter != m_spells.end() && iter->second.disabled)
                learnSpell(i->second, false);
        }
    }
}

/// 遗忘或禁用指定法术并清理相关光环、技能与降级动作条
void Player::removeSpell(uint32 spell_id, bool disabled, bool learn_low_rank, bool sendUpdate)
{
    PlayerSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return;

    PlayerSpell& playerSpell = itr->second;
    if (playerSpell.state == PLAYERSPELL_REMOVED)
        return;

    PlayerSpellState savedState = playerSpell.state;
    playerSpell.state = PLAYERSPELL_REMOVED; // recursion protection (递归防重入保护)

    // Always try to remove all dependent spells if present (needed to reset some talents properly) (始终尝试移除所有关联派生法术，确保某些天赋可正确重置)
    SpellLearnSpellMapBounds spell_bounds = sSpellMgr.GetSpellLearnSpellMapBounds(spell_id);
    for (SpellLearnSpellMap::const_iterator child_itr = spell_bounds.first; child_itr != spell_bounds.second; ++child_itr)
        removeSpell(child_itr->second.spell, !IsPassiveSpell(child_itr->second.spell), !IsPassiveSpell(child_itr->second.spell));

    // search again just in case (为防迭代器失效重新查找)
    itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return;

    playerSpell.state = savedState;
    if (playerSpell.state == PLAYERSPELL_REMOVED || (disabled && playerSpell.disabled))
        return;

    if (disabled && GetTalentSpellPos(spell_id))
        disabled = false; // talents should never be marked as disabled (天赋法术不应标记为禁用状态)

    // unlearn non talent higher ranks (recursive) (递归遗忘非天赋类的更高阶法术)
    SpellChainMapNext const& nextMap = sSpellMgr.GetSpellChainNext();
    for (SpellChainMapNext::const_iterator itr2 = nextMap.lower_bound(spell_id); itr2 != nextMap.upper_bound(spell_id); ++itr2)
        if (HasSpell(itr2->second) && !GetTalentSpellPos(itr2->second))
            removeSpell(itr2->second, !IsPassiveSpell(itr2->second), false, sendUpdate);

    // re-search, it can be corrupted in prev loop (前一循环可能修改容器，重新查找)
    itr = m_spells.find(spell_id);
    if (itr == m_spells.end() || playerSpell.state == PLAYERSPELL_REMOVED)
        return;                                             // already unleared (已被遗忘)

    bool cur_active = playerSpell.active;
    bool cur_dependent = playerSpell.dependent;

    if (disabled)
    {
        playerSpell.disabled = disabled;
        if (playerSpell.state != PLAYERSPELL_NEW)
            playerSpell.state = PLAYERSPELL_CHANGED;
    }
    else
    {
        if (playerSpell.state == PLAYERSPELL_NEW)
            m_spells.erase(itr);
        else
            playerSpell.state = PLAYERSPELL_REMOVED;
    }

    RemoveAurasByCasterSpell(spell_id, GetObjectGuid());
    RemoveAurasTriggeredBySpell(spell_id, GetObjectGuid());

    // remove pet auras (移除相关宠物光环)
    if (PetAura const* petSpell = sSpellMgr.GetPetAura(spell_id))
        RemovePetAura(petSpell);

    TalentSpellPos const* talentPos = GetTalentSpellPos(spell_id);
    if (talentPos)
    {
        // free talent points (返还天赋点数)
        uint32 talentCosts = GetTalentSpellCost(talentPos);

        if (talentCosts < m_usedTalentCount)
            m_usedTalentCount -= talentCosts;
        else
            m_usedTalentCount = 0;

        UpdateFreeTalentPoints(false);
    }

    // update free primary prof.points (if not overflow setting, can be in case GM use before .learn prof. learning) (返还主专业技能点，防止超出上限)
    if (sSpellMgr.IsPrimaryProfessionFirstRankSpell(spell_id))
    {
        uint32 freeProfs = GetFreePrimaryProfessionPoints() + 1;
        uint32 maxProfs = GetSession()->GetSecurity() < AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT)) ? sWorld.getConfig(CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL) : 10;
        if (freeProfs <= maxProfs)
            SetFreePrimaryProfessions(freeProfs);
    }

    // Remove dependent skills (移除关联技能)
    UpdateSpellTrainedSkills(spell_id, false);

    // activate lesser rank in spellbook/action bar, and cast it if need (在法术书和动作条中激活较低等级并按需施放)
    bool prev_activate = false;
    bool needsUnlearnSpellsPacket = false;

    if (uint32 prev_id = sSpellMgr.GetPrevSpellInChain(spell_id))
    {
        uint32 nextId = sSpellMgr.GetSpellBookSuccessorSpellId(prev_id);

        // if talent then lesser rank also talent and need learn (若为天赋则低等级同样为天赋，需重新学习)
        if (talentPos)
        {
            if (learn_low_rank)
                learnSpell(prev_id, false);
        }
        // if ranked non-stackable spell: need activate lesser rank and update dependence state (不可叠加的分级法术：激活低等级并更新依赖状态)
        else if (cur_active && nextId == spell_id)
        {
            // need manually update dependence state (learn spell ignore like attempts) (手动更新依赖标记)
            PlayerSpellMap::iterator prev_itr = m_spells.find(prev_id);
            if (prev_itr != m_spells.end())
            {
                PlayerSpell& spell = prev_itr->second;
                if (spell.dependent != cur_dependent)
                {
                    spell.dependent = cur_dependent;
                    if (spell.state != PLAYERSPELL_NEW)
                        spell.state = PLAYERSPELL_CHANGED;
                }

                // now re-learn if need re-activate (若需重新激活则执行重学)
                if (cur_active && !spell.active && learn_low_rank)
                {
                    if (addSpell(prev_id, true, false, spell.dependent, spell.disabled))
                    {
                        // downgrade spell ranks in spellbook and action bar (在法术书和动作条中将法术降级为低阶版本)
                        SendSupercededSpell(spell_id, prev_id);
                        needsUnlearnSpellsPacket = IsUnlearnSpellsPacketNeededForSpell(prev_id);
                        prev_activate = true;
                    }
                }
            }
        }
    }

    // for shaman Dual-wield (适用于萨满双持天赋)
    if (CanDualWield())
    {
        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);

        if (IsSpellHaveEffect(spellInfo, SPELL_EFFECT_DUAL_WIELD))
            SetCanDualWield(false);
    }

    // for talents and normal spell unlearn that allow offhand use for some weapons (用于遗忘允许副手使用特定武器的天赋或法术后自动卸下)
    if (sWorld.getConfig(CONFIG_BOOL_OFFHAND_CHECK_AT_TALENTS_RESET))
        AutoUnequipOffhandIfNeed();

    if (needsUnlearnSpellsPacket)
        SendUnlearnSpells();

    // remove from spell book if not replaced by lesser rank (若无低阶替代则从法术书中移除)
    if (!prev_activate && sendUpdate)
        SendRemovedSpell(spell_id);
}

void Player::RemoveArenaSpellCooldowns()
{
    // remove cooldowns on spells that has < 15 min CD - has to by in sync with spell check (移除冷却时间小于 15 分钟的法术冷却，用于竞技场进入/重置)
    const uint32 MaxCDDelay = 15 * MINUTE * IN_MILLISECONDS;
    auto cdCheck = [&](SpellEntry const & spellEntry) -> bool { return (spellEntry.RecoveryTime <= MaxCDDelay && (spellEntry.CategoryRecoveryTime <= MaxCDDelay)); };
    RemoveSomeCooldown(cdCheck);
}

void Player::_LoadSpellCooldowns(std::unique_ptr<QueryResult> queryResult)
{
    // some cooldowns can be already set at aura loading... (加载光环时可能已设置部分冷却)
    // QueryResult *result = CharacterDatabase.PQuery("SELECT SpellId, SpellExpireTime, Category, CategoryExpireTime, ItemId FROM character_spell_cooldown WHERE guid = '%u'",GetGUIDLow());

    if (queryResult)
    {
        auto curTime = GetMap()->GetCurrentClockTime();

        do
        {
            Field* fields = queryResult->Fetch();

            uint32 spell_id = fields[0].GetUInt32();
            uint64 spell_time = fields[1].GetUInt64();
            uint32 category = fields[2].GetUInt32();
            uint64 cat_time = fields[3].GetUInt64();
            uint32 item_id = fields[4].GetUInt32();

            SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
            if (!spellEntry)
            {
                sLog.outError("%s has unknown spell %u in `character_spell_cooldown`, skipping.", GetGuidStr().c_str(), spell_id);
                continue;
            }

            ItemPrototype const* itemProto = nullptr;
            if (item_id)
            {
                itemProto = ObjectMgr::GetItemPrototype(item_id);
                if (!itemProto)
                {
                    sLog.outError("%s has unknown item ID %u in `character_spell_cooldown`, skipping.", GetGuidStr().c_str(), item_id);
                    continue;
                }
            }

            TimePoint spellExpireTime = std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::from_time_t(spell_time));
            TimePoint catExpireTime = std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::from_time_t(cat_time));
            std::chrono::milliseconds spellRecTime = std::chrono::milliseconds::zero();
            std::chrono::milliseconds catRecTime = std::chrono::milliseconds::zero();
            if (spellExpireTime > curTime)
                spellRecTime = std::chrono::duration_cast<std::chrono::milliseconds>(spellExpireTime - curTime);
            if (catExpireTime > curTime)
                catRecTime = std::chrono::duration_cast<std::chrono::milliseconds>(catExpireTime - curTime);

            // skip outdated cooldown (跳过已过期的冷却记录)
            if (spellRecTime == std::chrono::milliseconds::zero() && catRecTime == std::chrono::milliseconds::zero())
                continue;

            m_cooldownMap.AddCooldown(curTime, spell_id, uint32(spellRecTime.count()), category, uint32(catRecTime.count()), item_id);
#ifdef _DEBUG
            uint32 spellCDDuration = std::chrono::duration_cast<std::chrono::seconds>(spellRecTime).count();
            uint32 catCDDuration = std::chrono::duration_cast<std::chrono::seconds>(catRecTime).count();
            std::string itemStr = "";
            if (item_id)
                itemStr = " caused by item id(" + std::to_string(item_id) + ") ";
            sLog.outDebug("Adding spell cooldown to %s, SpellID(%u), recDuration(%us), category(%u), catRecDuration(%us)%s.", GetGuidStr().c_str(),
                          spell_id, spellCDDuration, category, catCDDuration, itemStr.c_str());
#endif
        }
        while (queryResult->NextRow());
    }
}

void Player::_SaveSpellCooldowns()
{
    static SqlStatementID deleteSpellCooldown;

    // delete all old cooldown (删除旧的法术冷却数据库记录)
    SqlStatement stmt = CharacterDatabase.CreateStatement(deleteSpellCooldown, "DELETE FROM character_spell_cooldown WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    static SqlStatementID insertSpellCooldown;

    TimePoint now = GetMap()->GetCurrentClockTime();
    for (auto& cdItr : m_cooldownMap)
    {
        auto& cdData = cdItr.second;
        if (!cdData->IsPermanent())
        {
            TimePoint sTime = TimePoint::min();
            TimePoint cTime = TimePoint::min();
            cdData->GetSpellCDExpireTime(sTime);
            cdData->GetCatCDExpireTime(cTime);

            // Skip entries where both cooldowns have already expired - no point persisting them (跳过法术与类别冷却均已过期的条目，无需存库)
            if (sTime <= now && cTime <= now)
                continue;

            uint64 spellExpireTime = uint64(Clock::to_time_t(sTime));
            uint64 catExpireTime = uint64(Clock::to_time_t(cTime));

            stmt = CharacterDatabase.CreateStatement(insertSpellCooldown, "INSERT INTO character_spell_cooldown (guid, SpellId, SpellExpireTime, Category, CategoryExpireTime, ItemId) VALUES( ?, ?, ?, ?, ?, ?)");
            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt32(cdData->GetSpellId());
            stmt.addUInt64(spellExpireTime);
            stmt.addUInt32(cdData->GetCategory());
            stmt.addUInt64(catExpireTime);
            stmt.addUInt32(cdData->GetItemId());
            stmt.Execute();
        }
    }
}


uint32 Player::resetTalentsCost() const
{
    // The first time reset costs 1 gold (首次重置天赋花费 1 金币)
    if (m_resetTalentsCost < 1 * GOLD)
        return 1 * GOLD;
    // then 5 gold (第二次花费 5 金币)
    if (m_resetTalentsCost < 5 * GOLD)
        return 5 * GOLD;
    // After that it increases in increments of 5 gold (之后每次递增 5 金币)
    if (m_resetTalentsCost < 10 * GOLD)
        return 10 * GOLD;
    time_t months = (sWorld.GetGameTime() - m_resetTalentsTime) / MONTH;
    if (months > 0)
    {
        // This cost will be reduced by a rate of 5 gold per month (每月未重置费用递减 5 金币)
        int32 new_cost = int32((m_resetTalentsCost) - 5 * GOLD * months);
        // to a minimum of 10 gold. (最低降至 10 金币)
        return uint32(new_cost < 10 * GOLD ? 10 * GOLD : new_cost);
    }
    // After that it increases in increments of 5 gold (之后再次按每次 5 金币递增)
    int32 new_cost = m_resetTalentsCost + 5 * GOLD;
    // until it hits a cap of 50 gold. (最高封顶 50 金币)
    if (new_cost > 50 * GOLD)
        new_cost = 50 * GOLD;
    return new_cost;
}

/// 重置玩家所有已点天赋并退还天赋点数
bool Player::resetTalents(bool no_cost)
{
    // not need after this call (此处无需重复计算，后续或已扣费)
    if (HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
        RemoveAtLoginFlag(AT_LOGIN_RESET_TALENTS, true);

    if (!m_usedTalentCount)
        no_cost = true;

    uint32 cost = 0;

    if (!no_cost)
    {
        cost = resetTalentsCost();

        if (GetMoney() < cost)
        {
            SendBuyError(BuyResult::NOT_ENOUGHT_MONEY, nullptr, 0);
            return false;
        }
    }

    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);

        if (!talentInfo)
            continue;

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);

        if (!talentTabInfo)
            continue;

        // unlearn only talents for character class (仅遗忘当前角色所属职业的天赋)
        // some spell learned by one class as normal spells or know at creation but another class learn it as talent, (某些法术在甲职业为常规法术而在乙职业为天赋，)
        // to prevent unexpected lost normal learned spell skip another class talents (跳过其他职业天赋以免误删常规法术)
        if ((getClassMask() & talentTabInfo->ClassMask) == 0)
            continue;

        for (unsigned int j : talentInfo->RankID)
            if (j)
                removeSpell(j, !IsPassiveSpell(j), false);
    }

    UpdateFreeTalentPoints(false);

    if (!no_cost)
    {
        ModifyMoney(-(int32)cost);

        m_resetTalentsCost = cost;
        m_resetTalentsTime = time(nullptr);
    }

    // FIXME: remove pet before or after unlearn spells? for now after unlearn to allow removing of talent related, pet affecting auras (待修复：在遗忘法术之后解散宠物以便正确移除受天赋影响的宠物光环)
    RemovePet(PET_SAVE_REAGENTS);
    return true;
}

Mail* Player::GetMail(uint32 id)
{
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        if ((*itr)->messageID == id)
        {
            return (*itr);
        }
    }
    return nullptr;
}

void Player::SaveItemToInventory(Item* item)
{
    Bag* container = item->GetContainer();
    uint32 bag_guid = container ? container->GetGUIDLow() : 0;

    static SqlStatementID insertInventory;
    static SqlStatementID updateInventory;
    static SqlStatementID deleteInventory;

    switch (item->GetState())
    {
        case ITEM_NEW:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(insertInventory, "INSERT INTO character_inventory (guid,bag,slot,item,item_template) VALUES (?, ?, ?, ?, ?)");
            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt32(bag_guid);
            stmt.addUInt8(item->GetSlot());
            stmt.addUInt32(item->GetGUIDLow());
            stmt.addUInt32(item->GetEntry());
            stmt.Execute();
        }
        break;
        case ITEM_CHANGED:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(updateInventory, "UPDATE character_inventory SET guid = ?, bag = ?, slot = ?, item_template = ? WHERE item = ?");
            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt32(bag_guid);
            stmt.addUInt8(item->GetSlot());
            stmt.addUInt32(item->GetEntry());
            stmt.addUInt32(item->GetGUIDLow());
            stmt.Execute();
        }
        break;
        case ITEM_REMOVED:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(deleteInventory, "DELETE FROM character_inventory WHERE item = ?");
            stmt.PExecute(item->GetGUIDLow());
        }
        break;
        case ITEM_UNCHANGED:
            break;
        default:
            throw std::domain_error("Unrecognized item state");
    }

    item->SaveToDB();                                   // item have unchanged inventory record and can be save standalone (物品背包位置未变，可单独存库)
}

void Player::BuildCreateUpdateBlockForPlayer(UpdateData& data, Player* target) const
{
    if (target == this)
    {
        for (int i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (m_items[i] == nullptr)
                continue;

            m_items[i]->BuildCreateUpdateBlockForPlayer(data, target);
        }
        for (int i = INVENTORY_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (m_items[i] == nullptr)
                continue;

            m_items[i]->BuildCreateUpdateBlockForPlayer(data, target);
        }
        for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
        {
            if (m_items[i] == nullptr)
                continue;

            m_items[i]->BuildCreateUpdateBlockForPlayer(data, target);
        }
        for (int i = BUYBACK_SLOT_START; i < BUYBACK_SLOT_END; ++i)
        {
            if (m_items[i] == nullptr)
                continue;

            m_items[i]->BuildCreateUpdateBlockForPlayer(data, target);
        }
    }

    Unit::BuildCreateUpdateBlockForPlayer(data, target);
}

void Player::DestroyForPlayer(Player* target) const
{
    Unit::DestroyForPlayer(target);

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i] == nullptr)
            continue;

        m_items[i]->DestroyForPlayer(target);
    }

    if (target == this)
    {
        for (int i = INVENTORY_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (m_items[i] == nullptr)
                continue;

            m_items[i]->DestroyForPlayer(target);
        }
        for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
        {
            if (m_items[i] == nullptr)
                continue;

            m_items[i]->DestroyForPlayer(target);
        }
    }
}

bool Player::HasSpell(uint32 spell) const
{
    PlayerSpellMap::const_iterator itr = m_spells.find(spell);
    if (itr == m_spells.end())
        return false;

    PlayerSpell const& playerSpell = itr->second;
    return playerSpell.state != PLAYERSPELL_REMOVED && !playerSpell.disabled;
}

bool Player::HasActiveSpell(uint32 spell) const
{
    PlayerSpellMap::const_iterator itr = m_spells.find(spell);
    if (itr == m_spells.end())
        return false;

    PlayerSpell const& playerSpell = itr->second;
    return playerSpell.state != PLAYERSPELL_REMOVED && playerSpell.active && !playerSpell.disabled;
}

TrainerSpellState Player::GetTrainerSpellState(TrainerSpell const* trainer_spell, uint32 reqLevel) const
{
    if (!trainer_spell)
        return TRAINER_SPELL_RED;

    // known spell (已学法术)
    if (HasSpell(trainer_spell->spell))
        return TRAINER_SPELL_GRAY;

    // check race/class requirement (检查种族与职业限制)
    if (!IsSpellFitByClassAndRace(trainer_spell->spell))
        return TRAINER_SPELL_RED;

    bool prof = SpellMgr::IsProfessionSpell(trainer_spell->spell);

    // check level requirement (检查等级要求)
    if (!prof || GetSession()->GetSecurity() < AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_LEVEL)))
        if (GetLevel() < reqLevel)
            return TRAINER_SPELL_RED;
    
    bool hasLearnSpellEffect = trainer_spell->learnedSpell.size() > 1;
    bool knowsAllLearnedSpells = true;
    for (uint32 learnedSpell : trainer_spell->learnedSpell)
    {
        if (trainer_spell->spell != learnedSpell && !HasSpell(learnedSpell))
            knowsAllLearnedSpells = false;

        if (SpellChainNode const* spell_chain = sSpellMgr.GetSpellChainNode(learnedSpell))
        {
            // check prev.rank requirement (检查前置等级法术要求)
            if (spell_chain->prev && !HasSpell(spell_chain->prev))
                return TRAINER_SPELL_RED;

            // check additional spell requirement (检查额外前置法术要求)
            if (spell_chain->req && !HasSpell(spell_chain->req) &&
                std::find(trainer_spell->learnedSpell.begin(), trainer_spell->learnedSpell.end(), spell_chain->req) == trainer_spell->learnedSpell.end())
                return TRAINER_SPELL_RED;
        }

            // exist, already checked at loading (法术条目存在，加载时已校验)
        SpellEntry const* spell = sSpellTemplate.LookupEntry<SpellEntry>(learnedSpell);

        // secondary prof. or not prof. spell (辅助专业或非专业技能法术)
        uint32 skill = spell->EffectMiscValue[1];

        if (spell->Effect[1] != SPELL_EFFECT_SKILL || !IsPrimaryProfessionSkill(skill))
            continue;

        // check primary prof. limit (检查主专业数量上限)
        if (sSpellMgr.IsPrimaryProfessionFirstRankSpell(spell->Id) && GetFreePrimaryProfessionPoints() == 0)
            return TRAINER_SPELL_GREEN_DISABLED;
    }

    if (hasLearnSpellEffect && knowsAllLearnedSpells)
        return TRAINER_SPELL_GRAY;

    // check skill requirement (检查技能点要求)
    if (!prof || GetSession()->GetSecurity() < AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_SKILL)))
        if (trainer_spell->reqSkill && GetSkillValueBase(trainer_spell->reqSkill) < trainer_spell->reqSkillValue)
            return TRAINER_SPELL_RED;

    for (uint32 i = 0; i < 3; ++i)
        if (trainer_spell->reqAbility[i])
            if (!HasSpell(*trainer_spell->reqAbility[i]))
                return TRAINER_SPELL_RED;

    return TRAINER_SPELL_GREEN;
}

/**
 * Deletes a character from the database (从数据库中删除角色)
 *
 * The way, how the characters will be deleted is decided based on the config option.
 * 角色删除方式（软删除或彻底清除）由服务器配置项决定。
 *
 * @see Player::DeleteOldCharacters
 *
 * @param playerguid       the low-GUID from the player which should be deleted (待删除角色的低位 GUID)
 * @param accountId        the account id from the player (角色所属账号 ID)
 * @param updateRealmChars when this flag is set, the amount of characters on that realm will be updated in the realmlist (是否更新服务器分区角色数量)
 * @param deleteFinally    if this flag is set, the config option will be ignored and the character will be permanently removed from the database (是否忽略配置直接永久物理删除)
 */
/// 从数据库删除指定角色（支持软删除保留与彻底物理清除）
void Player::DeleteFromDB(ObjectGuid playerguid, uint32 accountId, bool updateRealmChars, bool deleteFinally)
{
    // the guid can come from a client packet: only its counter is trusted, whatever type its high part claims (GUID 可能来自客户端包，仅信任低位计数器并重新构造玩家 GUID)
    playerguid = ObjectGuid(HIGHGUID_PLAYER, playerguid.GetCounter());

    // for nonexistent account avoid update realm (账号不存在时不更新分区角色计数)
    if (accountId == 0)
        updateRealmChars = false;

    uint32 charDelete_method = sWorld.getConfig(CONFIG_UINT32_CHARDELETE_METHOD);
    uint32 charDelete_minLvl = sWorld.getConfig(CONFIG_UINT32_CHARDELETE_MIN_LEVEL);

    // if we want to finally delete the character or the character does not meet the level requirement, we set it to mode 0 (若需彻底删除或未达保留等级门槛，则强制模式为 0 物理删除)
    if (deleteFinally || Player::GetLevelFromDB(playerguid) < charDelete_minLvl)
        charDelete_method = 0;

    uint32 lowguid = playerguid.GetCounter();

    // convert corpse to bones if exist (to prevent exiting Corpse in World without DB entry) (若存在尸体则转为白骨，避免世界中残留无数据库记录的尸体)
    // bones will be deleted by corpse/bones deleting thread shortly (白骨将由后续清理线程自动清除)
    sObjectAccessor.ConvertCorpseForPlayer(playerguid);

    // remove from guild (从公会中移除)
    if (uint32 guildId = GetGuildIdFromDB(playerguid))
        if (Guild* guild = sGuildMgr.GetGuildById(guildId))
            if (guild->DelMember(playerguid))
                guild->Disband();

    // remove from arena teams (从所有竞技场战队中移除)
    LeaveAllArenaTeams(playerguid);

    // the player was uninvited already on logout so just remove from group (登出时已撤销邀请，此处直接移出队伍)
    auto resultGroup = CharacterDatabase.PQuery("SELECT groupId FROM group_member WHERE memberGuid='%u'", lowguid);
    if (resultGroup)
    {
        uint32 groupId = (*resultGroup)[0].GetUInt32();
        if (Group* group = sObjectMgr.GetGroupById(groupId))
            RemoveFromGroup(group, playerguid);
    }

    // remove signs from petitions (also remove petitions if owner); (移除请愿书签名；若为发起人则删除请愿书)
    RemovePetitionsAndSigns(playerguid, 10);

    // Sends mail, so it has to come before the transaction below (内部会发送退信邮件，须在下方数据库事务前执行)
    if (charDelete_method == 0)
        sAuctionMgr.RemovePlayerAuctions(playerguid);

    switch (charDelete_method)
    {
        // completely remove from the database (从数据库中彻底抹除角色数据)
        case 0:
        {
            // return back all mails with COD and Item         0  1           2              3      4       5          6     7 (退回所有带付费或附件的邮件)
            auto resultMail = CharacterDatabase.PQuery("SELECT id,messageType,mailTemplateId,sender,subject,itemTextId,money,has_items FROM mail WHERE receiver='%u' AND has_items<>0 AND cod<>0", lowguid);
            if (resultMail)
            {
                do
                {
                    Field* fields = resultMail->Fetch();

                    uint32 mail_id       = fields[0].GetUInt32();
                    uint16 mailType      = fields[1].GetUInt16();
                    uint16 mailTemplateId = fields[2].GetUInt16();
                    uint32 sender        = fields[3].GetUInt32();
                    std::string subject  = fields[4].GetCppString();
                    uint32 itemTextId    = fields[5].GetUInt32();
                    uint32 money         = fields[6].GetUInt32();
                    bool has_items       = fields[7].GetBool();

                    // we can return mail now (执行退信并删除原邮件)
                    // so firstly delete the old one (先删除旧邮件记录)
                    CharacterDatabase.PExecute("DELETE FROM mail WHERE id = '%u'", mail_id);

                    // mail not from player (非玩家寄出的系统/拍卖行邮件)
                    if (mailType != MAIL_NORMAL)
                    {
                        if (has_items)
                            CharacterDatabase.PExecute("DELETE FROM mail_items WHERE mail_id = '%u'", mail_id);
                        continue;
                    }

                    MailDraft draft;
                    if (mailTemplateId)
                        draft.SetMailTemplate(mailTemplateId, false);// items already included (退信时物品已随邮件归还)
                    else
                        draft.SetSubjectAndBodyId(subject, itemTextId);

                    if (has_items)
                    {
                        // data needs to be at first place for Item::LoadFromDB (字段顺序须与 Item::LoadFromDB 保持一致)
                        //                                                  0          1            2                3      4         5        6      7             8                 9           10          11         12
                        auto resultItems = CharacterDatabase.PQuery("SELECT itemEntry, creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, randomPropertyId, durability, itemTextId, item_guid, item_template FROM mail_items JOIN item_instance ON item_guid = guid WHERE mail_id='%u'", mail_id);
                        if (resultItems)
                        {
                            do
                            {
                                Field* fields2 = resultItems->Fetch();

                                uint32 item_guidlow = fields2[11].GetUInt32();
                                uint32 item_template = fields2[12].GetUInt32();

                                ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(item_template);
                                if (!itemProto)
                                {
                                    CharacterDatabase.PExecute("DELETE FROM item_instance WHERE guid = '%u'", item_guidlow);
                                    continue;
                                }

                                Item* pItem = NewItemOrBag(itemProto);
                                if (!pItem->LoadFromDB(item_guidlow, fields2, playerguid))
                                {
                                    pItem->FSetState(ITEM_REMOVED);
                                    pItem->SaveToDB();      // it also deletes item object ! (同时释放物品内存对象)
                                    continue;
                                }

                                draft.AddItem(pItem);
                            }
                            while (resultItems->NextRow());
                        }
                    }

                    CharacterDatabase.PExecute("DELETE FROM mail_items WHERE mail_id = '%u'", mail_id);

                    uint32 pl_account = sObjectMgr.GetPlayerAccountIdByGUID(playerguid);

                    draft.SetMoney(money).SendReturnToSender(pl_account, playerguid, ObjectGuid(HIGHGUID_PLAYER, sender));
                }
                while (resultMail->NextRow());
            }

            // unsummon and delete for pets in world is not required: player deleted from CLI or character list with not loaded pet. (从命令行或选人界面删角时宠物未加载，无需在世界中解散)
            // Get guids of character's pets, will deleted in transaction (查询角色名下所有宠物 GUID 以便在事务中删除)
            auto resultPets = CharacterDatabase.PQuery("SELECT id FROM character_pet WHERE owner = '%u'", lowguid);

            // delete char from friends list when selected chars is online (non existing - error) (清理好友关系及离线记录)
            auto  resultFriend = CharacterDatabase.PQuery("SELECT DISTINCT guid FROM character_social WHERE friend = '%u'", lowguid);

            // NOW we can finally clear other DB data related to character (现在可以清理与该角色关联的所有数据库表)
            CharacterDatabase.BeginTransaction();
            if (resultPets)
            {
                do
                {
                    Field* fields3 = resultPets->Fetch();
                    uint32 petguidlow = fields3[0].GetUInt32();
                    // do not create separate transaction for pet delete otherwise we will get fatal error! (切勿在事务内另开宠物删除事务，否则会引发数据库致命错误)
                    Pet::DeleteFromDB(petguidlow, false);
                }
                while (resultPets->NextRow());
            }

            // cleanup friends for online players, offline case will cleanup later in code (清理在线玩家的好友列表；离线情况由上文 SQL 处理)
            if (resultFriend)
            {
                do
                {
                    Field* fieldsFriend = resultFriend->Fetch();
                    if (Player* sFriend = sObjectAccessor.FindPlayer(ObjectGuid(HIGHGUID_PLAYER, fieldsFriend[0].GetUInt32())))
                    {
                        if (sFriend->IsInWorld())
                        {
                            sFriend->GetSocial()->RemoveFromSocialList(playerguid, false);
                            sSocialMgr.SendFriendStatus(sFriend, FRIEND_REMOVED, playerguid, false);
                        }
                    }
                }
                while (resultFriend->NextRow());
            }

            CharacterDatabase.PExecute("DELETE FROM characters WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_declinedname WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_action WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_aura WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_battleground_data WHERE guid = '%u'", lowguid);
            // A gift it wrapped and already sent belongs to its receiver: only the ones it still holds go (已寄出的礼物归收件人所有，仅销毁角色当前持有的礼物记录)
            CharacterDatabase.PExecute("DELETE FROM character_gifts WHERE guid = '%u' AND item_guid IN (SELECT guid FROM item_instance WHERE owner_guid = '%u')", lowguid, lowguid);
            CharacterDatabase.PExecute("UPDATE character_gifts SET guid = 0 WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_homebind WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_instance WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM group_instance WHERE leaderGuid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_inventory WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_queststatus WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_queststatus_daily WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_reputation WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_skills WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_spell WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_spell_cooldown WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM item_instance WHERE owner_guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_social WHERE guid = '%u' OR friend='%u'", lowguid, lowguid);
            CharacterDatabase.PExecute("DELETE FROM mail WHERE receiver = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM mail_items WHERE receiver = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_pet WHERE owner = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_pet_declinedname WHERE owner = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM guild_eventlog WHERE PlayerGuid1 = '%u' OR PlayerGuid2 = '%u'", lowguid, lowguid);
            CharacterDatabase.PExecute("DELETE FROM guild_bank_eventlog WHERE PlayerGuid = '%u'", lowguid);

            // What other tables still keep of the character, so that nothing is left to name it (清理其他表对该角色 GUID/名称的引用)
            CharacterDatabase.PExecute("DELETE FROM character_account_data WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_stats WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_queststatus_monthly WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM character_queststatus_weekly WHERE guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM item_loot WHERE owner_guid = '%u'", lowguid);
            CharacterDatabase.PExecute("DELETE FROM pvpstats_players WHERE character_guid = '%u'", lowguid);
            // Auras it cast on others: caster_guid holds the full guid (清理该角色施放在他人身上的持久化光环)
            CharacterDatabase.PExecute("DELETE FROM character_aura WHERE caster_guid = '" UI64FMTD "'", playerguid.GetRawValue());
            CharacterDatabase.PExecute("DELETE FROM pet_aura WHERE caster_guid = '" UI64FMTD "'", playerguid.GetRawValue());
            // Items it crafted that now belong to others. The gift creator is left alone: a wrapped item is (清除已流转到他人身上的制造者签名；礼物签名保留)
            // recognised by a non-empty one. (用于区分包裹礼物)
            CharacterDatabase.PExecute("UPDATE item_instance SET creatorGuid = 0 WHERE creatorGuid = '%u'", lowguid);
            // GM tickets: its own are removed, the ones it handled as a GM go back to unassigned (删除自身提交的 GM 工单，并释放其作为 GM 接手的工单)
            CharacterDatabase.PExecute("DELETE FROM gm_tickets WHERE author_guid = '%u'", lowguid);
            CharacterDatabase.PExecute("UPDATE gm_tickets SET assignee_guid = 0, assignee_name = '', status = CASE WHEN status = '%u' THEN '%u' ELSE status END "
                                       "WHERE assignee_guid = '%u'", uint32(GMTICKET_ASSIGNEDTOGM_STATUS_ASSIGNED), uint32(GMTICKET_ASSIGNEDTOGM_STATUS_NOT_ASSIGNED), lowguid);
            sTicketMgr.RemovePlayerReferences(playerguid);
            CharacterDatabase.CommitTransaction();
            break;
        }
        // The character gets unlinked from the account, the name gets freed up and appears as deleted ingame (软删除：解除账号绑定、释放角色名并标记为已删除)
        case 1:
            CharacterDatabase.PExecute("UPDATE characters SET deleteInfos_Name=name, deleteInfos_Account=account, deleteDate='" UI64FMTD "', name='', account=0 WHERE guid=%u", uint64(time(nullptr)), lowguid);
            break;
        default:
            sLog.outError("Player::DeleteFromDB: Unsupported delete method: %u.", charDelete_method);
    }

    if (updateRealmChars)
        sWorld.UpdateRealmCharCount(accountId);
}

/**
 * Characters which were kept back in the database after being deleted and are now too old (see config option "CharDelete.KeepDays"), will be completely deleted.
 * 彻底清理数据库中超过保留天数（CharDelete.KeepDays）的已删除角色。
 *
 * @see Player::DeleteFromDB
 */
void Player::DeleteOldCharacters()
{
    uint32 keepDays = sWorld.getConfig(CONFIG_UINT32_CHARDELETE_KEEP_DAYS);
    if (!keepDays)
        return;

    Player::DeleteOldCharacters(keepDays);
}

/**
 * Characters which were kept back in the database after being deleted and are older than the specified amount of days, will be completely deleted.
 * 彻底清理数据库中保留超过指定天数的已删除角色。
 *
 * @see Player::DeleteFromDB
 *
 * @param keepDays overrite the config option by another amount of days (覆盖配置选项的保留天数)
 */
void Player::DeleteOldCharacters(uint32 keepDays)
{
    sLog.outString("Player::DeleteOldChars: Deleting all characters which have been deleted %u days before...", keepDays);

    auto resultChars = CharacterDatabase.PQuery("SELECT guid, deleteInfos_Account FROM characters WHERE deleteDate IS NOT NULL AND deleteDate < '" UI64FMTD "'", uint64(time(nullptr) - time_t(keepDays * DAY)));
    if (resultChars)
    {
        sLog.outString("Player::DeleteOldChars: Found %u character(s) to delete", uint32(resultChars->GetRowCount()));
        do
        {
            Field* charFields = resultChars->Fetch();
            ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, charFields[0].GetUInt32());
            Player::DeleteFromDB(guid, charFields[1].GetUInt32(), true, true);
        }
        while (resultChars->NextRow());
    }
    sLog.outString();
}

/* Preconditions (前置条件):
  - a resurrectable corpse must not be loaded for the player (only bones) (玩家当前不可已有可复活尸体，最多仅允许白骨)
  - the player must be in world (玩家必须处于世界中)
*/
/// 释放灵魂并在玩家死亡位置生成可复活尸体，角色转为幽灵形态
void Player::BuildPlayerRepop()
{
    if (getRace() == RACE_NIGHTELF)
        CastSpell(this, 20584, TRIGGERED_OLD_TRIGGERED);                       // auras SPELL_AURA_INCREASE_SPEED(+speed in wisp form), SPELL_AURA_INCREASE_SWIM_SPEED(+swim speed in wisp form), SPELL_AURA_TRANSFORM (to wisp form) (暗夜精灵小精灵形态加速、游泳加速与变身光环)
    CastSpell(this, 8326, TRIGGERED_OLD_TRIGGERED);                            // auras SPELL_AURA_GHOST, SPELL_AURA_INCREASE_SPEED(why?), SPELL_AURA_INCREASE_SWIM_SPEED(why?) (常规幽灵光环与移速/游速光环)

    // there must be SMSG.FORCE_RUN_SPEED_CHANGE, SMSG.FORCE_SWIM_SPEED_CHANGE, SMSG.MOVE_WATER_WALK (inside ghost aura handler) (幽灵光环处理器会下发速度变更与水上行走数据包)
    // there must be SMSG.STOP_MIRROR_TIMER (停止呼吸/疲劳等镜像计时器)
    // there we must send 888 opcode (下发尸体位置与灵魂释放相关操作码)

    // the player cannot have a corpse already, only bones which are not returned by GetCorpse (玩家不能已有未腐化的尸体，GetCorpse 不返回白骨)
    if (GetCorpse())
    {
        sLog.outError("BuildPlayerRepop: player %s(%d) already has a corpse", GetName(), GetGUIDLow());
        MANGOS_ASSERT(false);
    }

    // create a corpse and place it at the player's location (在玩家当前位置创建尸体)
    Corpse* corpse = CreateCorpse();
    if (!corpse)
    {
        sLog.outError("Error creating corpse for Player %s [%u]", GetName(), GetGUIDLow());
        return;
    }
    GetMap()->Add(corpse);

    // convert player body to ghost (将玩家角色转换为幽灵状态)
    SetHealth(1);

    if (!IsImmobilizedState())
        SendMoveRoot(false);

    // BG - remove insignia related (清除战场徽记可拾取标记)
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);

    SendCorpseReclaimDelay();

    // to prevent cheating (清除飞行标记以防作弊)
    corpse->ResetGhostTime();

    // set and clear other (更新死亡释放计时并发送尸体回收延迟)
    SetByteValue(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_MISC_FLAGS, UNIT_BYTE1_FLAG_ALWAYS_STAND);

    FailQuestsOnDeath(); // confirmed to be on release (确认释放灵魂时触发)
}

/// 复活玩家并按比例恢复生命与能量，可选施加复活虚弱
void Player::ResurrectPlayer(float restore_percent, bool applySickness)
{
    WorldPacket data(SMSG_DEATH_RELEASE_LOC, 4 * 4);        // remove spirit healer position (清除客户端显示的灵魂医者位置标记)
    data << uint32(-1);
    data << float(0);
    data << float(0);
    data << float(0);
    GetSession()->SendPacket(data);

    // speed change, land walk (通知地面行走与速度恢复)

    // remove death flag + set aura (移除死亡状态标记并恢复存活态)
    SetByteValue(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_MISC_FLAGS, 0x00);

    SetDeathState(ALIVE);

    if (getRace() == RACE_NIGHTELF)
        RemoveAurasDueToSpell(20584);                       // speed bonuses (重新计算移动速度加成)
    RemoveAurasDueToSpell(8326);                            // SPELL_AURA_GHOST (移除幽灵光环)

    if ((GetSession()->GetRecruitingFriendId() != 0))
        SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_REFER_A_FRIEND);

    SendMoveRoot(false);

    m_deathTimer = 0;

    // set health/powers (0- will be set in caller) (按比例恢复生命与能量，若为 0 则由调用方设置)
    if (restore_percent > 0.0f)
    {
        SetHealth(uint32(GetMaxHealth()*restore_percent));
        SetPower(POWER_MANA, uint32(GetMaxPower(POWER_MANA)*restore_percent));
        SetPower(POWER_RAGE, 0);
        SetPower(POWER_ENERGY, uint32(GetMaxPower(POWER_ENERGY)*restore_percent));
    }

    // trigger update zone for alive state zone updates (触发存活状态下的区域更新)
    uint32 newzone, newarea;
    GetZoneAndAreaId(newzone, newarea);
    UpdateZone(newzone, newarea, true); // must force zone script updates too to reapply auras (强制触发区域脚本以重新应用区域光环)

    // update visibility of world around viewpoint (刷新视点周围世界对象的可见性)
    // update visibility of player for nearby cameras (刷新周围观察者视角对玩家的可见性)
    if (IsInWorld())
    {
        GetMap()->AddUpdateCreateObject(this);

        if (InstanceData* instanceData = GetMap()->GetInstanceData())
            instanceData->OnPlayerResurrect(this);
    }

    if (!applySickness)
        return;

    // Characters from level 1-10 are not affected by resurrection sickness. (1-10 级角色不受复活虚弱影响)
    // Characters from level 11-19 will suffer from one minute of sickness (11-19 级角色每高于 10 级承受 1 分钟复活虚弱)
    // for each level they are above 10. (每超过 10 级增加 1 分钟)
    // Characters level 20 and up suffer from ten minutes of sickness. (20 级及以上角色承受 10 分钟复活虚弱)
    int32 startLevel = sWorld.getConfig(CONFIG_INT32_DEATH_SICKNESS_LEVEL);

    if (int32(GetLevel()) >= startLevel)
    {
        // set resurrection sickness (施加复活虚弱法术)
        CastSpell(this, SPELL_ID_PASSIVE_RESURRECTION_SICKNESS, TRIGGERED_OLD_TRIGGERED);

        // not full duration (11-19 级缩短复活虚弱持续时间)
        if (int32(GetLevel()) < startLevel + 9)
        {
            int32 delta = (int32(GetLevel()) - startLevel + 1) * MINUTE;

            if (SpellAuraHolder* holder = GetSpellAuraHolder(SPELL_ID_PASSIVE_RESURRECTION_SICKNESS))
            {
                holder->SetAuraDuration(delta * IN_MILLISECONDS);
                holder->UpdateAuraDuration();
            }
        }
    }
}

std::pair<bool, AreaTrigger const*> Player::CheckAndRevivePlayerOnDungeonEnter(MapEntry const* targetMapEntry, uint32 targetMapId)
{
    AreaTrigger const* resultingAt = nullptr;

    uint32 corpseMapId = 0;
    if (Corpse* corpse = GetCorpse())
        corpseMapId = corpse->GetMapId();

    // check back way from corpse to entrance (检查从尸体所在副本返回入口传送触发器)
    uint32 instance_map = corpseMapId;
    do
    {
        // most often fast case (最常见的快速路径：直接命中或无需重连)
        if (instance_map == targetMapEntry->MapID)
            break;

        InstanceTemplate const* instance = ObjectMgr::GetInstanceTemplate(instance_map);
        instance_map = instance ? instance->parent : 0;
    } while (instance_map);

    // corpse not in dungeon or some linked deep dungeons (尸体不在副本内或位于多层关联地下城中)
    if (!instance_map)
    {
        GetSession()->SendAreaTriggerMessage("You cannot enter %s while in a ghost mode",
            targetMapEntry->name[GetSession()->GetSessionDbcLocale()]);
        return { false, nullptr };
    }

    // need find areatrigger to inner dungeon for landing point (查找通往深层副本的入口区域触发器作为落脚点)
    if (targetMapId != corpseMapId)
    {
        if (AreaTrigger const* corpseAt = sObjectMgr.GetMapEntranceTrigger(corpseMapId))
        {
            resultingAt = corpseAt;
            targetMapEntry = sMapStore.LookupEntry(targetMapId);
            if (!targetMapEntry)
                return { false, nullptr };
        }
    }

    // now we can resurrect player, and then check teleport requirements (先复活玩家，随后检查副本进入权限)
    ResurrectPlayer(0.5f);
    SpawnCorpseBones();
    return { true, resultingAt };
}

/// 击杀玩家并设置 PvP 死亡状态与释放倒计时
void Player::KillPlayer()
{
    SendMoveRoot(true);

    SetDeathState(CORPSE);
    // SetFlag( UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_IN_PVP );

    SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
    ApplyModByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_FLAGS, PLAYER_FIELD_BYTE_RELEASE_TIMER, !sMapStore.LookupEntry(GetMapId())->Instanceable());

    // 6 minutes until repop at graveyard (6 分钟后自动释放灵魂至墓地)
    m_deathTimer = 6 * MINUTE * IN_MILLISECONDS;

    UpdateCorpseReclaimDelay();                             // dependent at use SetDeathPvP() call before kill (取决于击杀前设置的 PvP 死亡标志)

    // don't create corpse at this moment, player might be falling (此时暂不生成尸体，玩家可能处于坠落中)
}

Corpse* Player::CreateCorpse()
{
    // prevent existence 2 corpse for player (防止同时存在两个属于同一玩家的尸体)
    SpawnCorpseBones();

    Corpse* corpse = new Corpse((m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH) ? CORPSE_RESURRECTABLE_PVP : CORPSE_RESURRECTABLE_PVE);
    SetPvPDeath(false);

    if (!corpse->Create(sObjectMgr.GenerateCorpseLowGuid(), this))
    {
        delete corpse;
        return nullptr;
    }

    uint8 skin       = GetByteValue(PLAYER_BYTES, 0);
    uint8 face       = GetByteValue(PLAYER_BYTES, 1);
    uint8 hairstyle  = GetByteValue(PLAYER_BYTES, 2);
    uint8 haircolor  = GetByteValue(PLAYER_BYTES, 3);
    uint8 facialhair = GetByteValue(PLAYER_BYTES_2, 0);

    corpse->SetByteValue(CORPSE_FIELD_BYTES_1, 1, getRace());
    corpse->SetByteValue(CORPSE_FIELD_BYTES_1, 2, getGender());
    corpse->SetByteValue(CORPSE_FIELD_BYTES_1, 3, skin);

    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 0, face);
    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 1, hairstyle);
    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 2, haircolor);
    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 3, facialhair);

    uint32 flags = 0;
    if (IsPvP())
        flags |= CORPSE_FLAG_PVP;
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM))
        flags |= CORPSE_FLAG_HIDE_HELM;
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK))
        flags |= CORPSE_FLAG_HIDE_CLOAK;
    if (InBattleGround() && !InArena())
        flags |= CORPSE_FLAG_LOOTABLE;                      // to be able to remove insignia (战场中允许敌方玩家拔除徽记)
    corpse->SetUInt32Value(CORPSE_FIELD_FLAGS, flags);

    corpse->SetUInt32Value(CORPSE_FIELD_DISPLAY_ID, GetNativeDisplayId());

    corpse->SetUInt32Value(CORPSE_FIELD_GUILD, GetGuildId());

    for (int i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (m_items[i])
        {
            uint32 iDisplayID = m_items[i]->GetProto()->DisplayInfoID;
            uint32 iIventoryType = m_items[i]->GetProto()->InventoryType;

            uint32 _cfi = iDisplayID | (iIventoryType << 24);
            corpse->SetUInt32Value(CORPSE_FIELD_ITEM + i, _cfi);
        }
    }

    // we not need saved corpses for BG/arenas (战场与竞技场中的尸体无需持久化到数据库)
    if (!GetMap()->IsBattleGroundOrArena())
        corpse->SaveToDB();

    // register for player, but not show (注册尸体但不立即对存活可见对象展示)
    sObjectAccessor.AddCorpse(corpse);
    return corpse;
}

void Player::SpawnCorpseBones()
{
    if (sObjectAccessor.ConvertCorpseForPlayer(GetObjectGuid()))
        if (!GetSession()->PlayerLogoutWithSave())          // at logout we will already store the player (登出时玩家已被保存，此处无需额外保存)
            SaveToDB();                                     // prevent loading as ghost without corpse (将尸体转为白骨以避免玩家作为无尸幽灵加载)
}

Corpse* Player::GetCorpse() const
{
    return sObjectAccessor.GetCorpseForPlayerGUID(GetObjectGuid());
}

void Player::DurabilityLossAll(double percent, bool inventory)
{
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            DurabilityLoss(pItem, percent);

    if (inventory)
    {
        // bags not have durability (背包栏本身无耐久度)
        // for(int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)

        for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                DurabilityLoss(pItem, percent);

        // keys not have durability (钥匙链无耐久度)
        // for(int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)

        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    if (Item* pItem = GetItemByPos(i, j))
                        DurabilityLoss(pItem, percent);
    }
}

void Player::DurabilityLoss(Item* item, double percent)
{
#ifdef ENABLE_PLAYERBOTS
    PlayerbotAI* bot = GetPlayerbotAI();
    if (bot)
    {
        bot->DurabilityLoss(item, percent);
        return;
    }
#endif

    if (!item)
        return;

    uint32 pMaxDurability =  item ->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);

    if (!pMaxDurability)
        return;

    uint32 pDurabilityLoss = uint32(pMaxDurability * percent);

    if (pDurabilityLoss < 1)
        pDurabilityLoss = 1;

    DurabilityPointsLoss(item, pDurabilityLoss);
}

void Player::DurabilityPointsLossAll(int32 points, bool inventory)
{
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            DurabilityPointsLoss(pItem, points);

    if (inventory)
    {
        // bags not have durability (背包栏本身无耐久度)
        // for(int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)

        for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                DurabilityPointsLoss(pItem, points);

        // keys not have durability (钥匙链无耐久度)
        // for(int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)

        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    if (Item* pItem = GetItemByPos(i, j))
                        DurabilityPointsLoss(pItem, points);
    }
}

void Player::DurabilityPointsLoss(Item* item, int32 points)
{
    int32 pMaxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    int32 pOldDurability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
    int32 pNewDurability = pOldDurability - points;

    if (pNewDurability < 0)
        pNewDurability = 0;
    else if (pNewDurability > pMaxDurability)
        pNewDurability = pMaxDurability;

    if (pOldDurability != pNewDurability)
    {
        // modify item stats _before_ Durability set to 0 to pass _ApplyItemMods internal check (在耐久度归 0 前移除装备属性，以通过 _ApplyItemMods 内部检查)
        if (pNewDurability == 0 && pOldDurability > 0 && item->IsEquipped())
            _ApplyItemMods(item, item->GetSlot(), false);

        item->SetUInt32Value(ITEM_FIELD_DURABILITY, pNewDurability);

        // modify item stats _after_ restore durability to pass _ApplyItemMods internal check (恢复耐久度后再重新应用装备属性，以通过内部检查)
        if (pNewDurability > 0 && pOldDurability == 0 && item->IsEquipped())
            _ApplyItemMods(item, item->GetSlot(), true);

        item->SetState(ITEM_CHANGED, this);
    }
}

void Player::DurabilityPointLossForEquipSlot(EquipmentSlots slot)
{
    if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        DurabilityPointsLoss(pItem, 1);
}

uint32 Player::DurabilityRepairAll(bool cost, float discountMod, bool guildBank)
{
    uint32 TotalCost = 0;
    // equipped, backpack, bags itself (修理已装备部位与行囊主格物品)
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        TotalCost += DurabilityRepair(((INVENTORY_SLOT_BAG_0 << 8) | i), cost, discountMod, guildBank);

    // bank, buyback and keys not repaired (银行、回购栏与钥匙链不修理)

    // items in inventory bags (修理背包内各格物品)
    for (int j = INVENTORY_SLOT_BAG_START; j < INVENTORY_SLOT_BAG_END; ++j)
        for (int i = 0; i < MAX_BAG_SIZE; ++i)
            TotalCost += DurabilityRepair(((j << 8) | i), cost, discountMod, guildBank);
    return TotalCost;
}

/// 修理指定背包/装备槽位的物品耐久度并返回花费金币
uint32 Player::DurabilityRepair(uint16 pos, bool cost, float discountMod, bool guildBank)
{
    Item* item = GetItemByPos(pos);

    uint32 TotalCost = 0;
    if (!item)
        return TotalCost;

    // a wrapped item shows no durability (已包装的礼物不参与修理)
    if (item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
        return TotalCost;

    uint32 maxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    if (!maxDurability)
        return TotalCost;

    uint32 curDurability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);

    if (cost)
    {
        uint32 costs = item->CalculateDurabilityRepairCost(discountMod);
        if (costs > 0)
        {
            if (guildBank)
            {
                if (GetGuildId() == 0)
                {
                    DEBUG_LOG("You are not member of a guild");
                    return TotalCost;
                }

                Guild* pGuild = sGuildMgr.GetGuildById(GetGuildId());
                if (!pGuild)
                    return TotalCost;

                if (!pGuild->HasRankRight(GetRank(), GR_RIGHT_WITHDRAW_REPAIR))
                {
                    DEBUG_LOG("You do not have rights to withdraw for repairs");
                    return TotalCost;
                }

                if (pGuild->GetMemberMoneyWithdrawRem(GetGUIDLow()) < costs)
                {
                    DEBUG_LOG("You do not have enough money withdraw amount remaining");
                    return TotalCost;
                }

                if (pGuild->GetGuildBankMoney() < costs)
                {
                    DEBUG_LOG("There is not enough money in bank");
                    return TotalCost;
                }

                pGuild->MemberMoneyWithdraw(costs, GetGUIDLow());
                TotalCost = costs;
            }
            else if (GetMoney() < costs)
            {
                DEBUG_LOG("You do not have enough money");
                return TotalCost;
            }
            else
                ModifyMoney(-int32(costs));
        }
    }

    item->SetUInt32Value(ITEM_FIELD_DURABILITY, maxDurability);
    item->SetState(ITEM_CHANGED, this);

    // reapply mods for total broken and repaired item if equipped (若受损归零装备被修好且已穿戴，重新应用属性加成)
    if (IsEquipmentPos(pos) && !curDurability)
        _ApplyItemMods(item, pos & 255, true);
    return TotalCost;
}

/// 将灵魂状态的玩家传送至最近可用墓地（不可达区域则自动复活）
void Player::RepopAtGraveyard()
{
    // note: this can be called also when the player is alive (注：玩家存活时也可能调用此函数，)
    // for example from WorldSession::HandleMovementOpcodes (例如落入虚空时从移动处理器调用)

    AreaTableEntry const* zone = GetAreaEntryByAreaID(GetAreaId());

    // Such zones are considered unreachable as a ghost and the player must be automatically revived (此类区域幽灵无法抵达，须自动原地/墓地复活)
    if ((!IsAlive() && zone && zone->flags & AREA_FLAG_NEED_FLY) || GetTransport())
    {
        ResurrectPlayer(0.5f);
        SpawnCorpseBones();
    }

    WorldSafeLocsEntry const* ClosestGrave;
    ClosestGrave = GetMap()->GetGraveyardManager().GetClosestGraveYard(GetPositionX(), GetPositionY(), GetPositionZ(), GetMapId(), GetTeam());

    // stop countdown until repop (停止自动释放/传送倒计时)
    m_deathTimer = 0;

    // if no grave found, stay at the current location (若未找到安全墓地则留在当前位置)
    // and don't show spirit healer location (且不显示灵魂医者位置)
    if (ClosestGrave)
    {
        bool updateVisibility = IsInWorld() && GetMapId() == ClosestGrave->map_id;
        TeleportTo(ClosestGrave->map_id, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, ClosestGrave->o);
        if (IsDead())                                       // not send if alive, because it used in TeleportTo() (存活状态下不发送，避免影响常规传送)
        {
            WorldPacket data(SMSG_DEATH_RELEASE_LOC, 4 * 4);// show spirit healer position on minimap (向客户端发送灵魂医者墓地坐标标记)
            data << ClosestGrave->map_id;
            data << ClosestGrave->x;
            data << ClosestGrave->y;
            data << ClosestGrave->z;
            GetSession()->SendPacket(data);
        }
        if (updateVisibility && IsInWorld())
            UpdateVisibilityAndView();
    }
}

void Player::JoinedChannel(Channel* c)
{
    m_channels.push_back(c);
}

void Player::LeftChannel(Channel* c)
{
    m_channels.remove(c);
}

void Player::CleanupChannels()
{
    while (!m_channels.empty())
    {
        Channel* ch = *m_channels.begin();
        m_channels.erase(m_channels.begin());               // remove from player's channel list (从玩家已加入频道列表中移除)
        ch->Leave(this, false);                             // not send to client, not remove from player's channel list (清理时直接退出，不向客户端发包)
        if (ChannelMgr* cMgr = channelMgr(GetTeam()))
            cMgr->LeftChannel(ch->GetName());               // deleted channel if empty (若频道已无成员则销毁频道)
    }
    DEBUG_LOG("Player: channels cleaned up!");
}

void Player::UpdateLocalChannels(uint32 newZone)
{
    if (m_channels.empty())
        return;

    AreaTableEntry const* current_zone = GetAreaEntryByAreaID(newZone);
    if (!current_zone)
        return;

    ChannelMgr* cMgr = channelMgr(GetTeam());
    if (!cMgr)
        return;

    std::string current_zone_name = current_zone->area_name[GetSession()->GetSessionDbcLocale()];

    for (JoinedChannelsList::iterator i = m_channels.begin(), next; i != m_channels.end(); i = next)
    {
        next = i; ++next;

        ChatChannelsEntry const* ch = (*i)->GetChannelEntry();

        // skip non built-in channels or global channel without zone name in pattern (跳过自定义频道或不带区域名的全局频道)
        if (!ch || (ch->flags & 4) == 4)
            continue;

        //  new channel (构造新区域的本地频道名称)
        char new_channel_name_buf[100];
        snprintf(new_channel_name_buf, 100, ch->pattern[m_session->GetSessionDbcLocale()], current_zone_name.c_str());
        Channel* new_channel = cMgr->GetJoinChannel(new_channel_name_buf, ch->ChannelID);

        if ((*i) != new_channel)
        {
            new_channel->Join(this, "");                    // will output Changed Channel: N. Name (向客户端发送切换本地频道通知)

            // leave old channel (离开旧区域本地频道)
            (*i)->Leave(this, false);                       // not send leave channel, it already replaced at client (不发送退出消息，客户端已自动替换)
            std::string name = (*i)->GetName();             // store name, (*i)erase in LeftChannel (先保存频道名，LeftChannel 会删除迭代器)
            LeftChannel(*i);                                // remove from player's channel list (从玩家频道列表移除)
            cMgr->LeftChannel(name);                        // delete if empty (若旧频道无人则销毁)
        }
    }
    DEBUG_LOG("Player: channels cleaned up!");
}

void Player::LeaveLFGChannel()
{
    for (auto& m_channel : m_channels)
    {
        if (m_channel->IsLFG())
        {
            m_channel->Leave(this);
            break;
        }
    }
}

void Player::UpdateDefense(uint32 /*procEx*/)
{
    uint32 defense_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_DEFENSE);

    if (defense_skill_gain == 0)
        return;

    UpdateSkill(SKILL_DEFENSE, defense_skill_gain);
    UpdateDefenseBonusesMod();
}

void Player::HandleBaseModValue(BaseModGroup modGroup, BaseModType modType, float amount, bool apply)
{
    if (modGroup >= BASEMOD_END || modType >= MOD_END)
    {
        sLog.outError("ERROR in HandleBaseModValue(): nonexistent BaseModGroup of wrong BaseModType!");
        return;
    }

    float val;

    switch (modType)
    {
        case FLAT_MOD:
            m_auraBaseMod[modGroup][modType] += apply ? amount : -amount;
            break;
        case PCT_MOD:
            if (amount <= -100.0f)
                amount = -200.0f;

            val = (100.0f + amount) / 100.0f;
            m_auraBaseMod[modGroup][modType] *= apply ? val : (1.0f / val);
            break;
        default:
            break;
    }

    if (!CanModifyStats())
        return;

    switch (modGroup)
    {
        case CRIT_PERCENTAGE:              UpdateCritPercentage(BASE_ATTACK);                          break;
        case RANGED_CRIT_PERCENTAGE:       UpdateCritPercentage(RANGED_ATTACK);                        break;
        case OFFHAND_CRIT_PERCENTAGE:      UpdateCritPercentage(OFF_ATTACK);                           break;
        case SHIELD_BLOCK_VALUE:           UpdateShieldBlockValue();                                   break;
        default: break;
    }
}

float Player::GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const
{
    if (modGroup >= BASEMOD_END || modType > MOD_END)
    {
        sLog.outError("trial to access nonexistent BaseModGroup or wrong BaseModType!");
        return 0.0f;
    }

    if (modType == PCT_MOD && m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
        return 0.0f;

    return m_auraBaseMod[modGroup][modType];
}

float Player::GetTotalBaseModValue(BaseModGroup modGroup) const
{
    if (modGroup >= BASEMOD_END)
    {
        sLog.outError("wrong BaseModGroup in GetTotalBaseModValue()!");
        return 0.0f;
    }

    if (m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
        return 0.0f;

    return m_auraBaseMod[modGroup][FLAT_MOD] * m_auraBaseMod[modGroup][PCT_MOD];
}

uint32 Player::GetShieldBlockValue() const
{
    float value = (m_auraBaseMod[SHIELD_BLOCK_VALUE][FLAT_MOD] + GetStat(STAT_STRENGTH) / 20 - 1) * m_auraBaseMod[SHIELD_BLOCK_VALUE][PCT_MOD];

    value = (value < 0) ? 0 : value;

    return uint32(value);
}

float Player::GetMeleeCritFromAgility() const
{
    uint32 level = GetLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL) level = GT_MAX_LEVEL;

    GtChanceToMeleeCritBaseEntry const* critBase  = sGtChanceToMeleeCritBaseStore.LookupEntry(pclass - 1);
    GtChanceToMeleeCritEntry     const* critRatio = sGtChanceToMeleeCritStore.LookupEntry((pclass - 1) * GT_MAX_LEVEL + level - 1);
    if (critBase == nullptr || critRatio == nullptr)
        return 0.0f;

    float crit = critBase->base + GetStat(STAT_AGILITY) * critRatio->ratio;
    return crit * 100.0f;
}

// Static multipliers for converting agi into dodge based on agi to crit conversion ratio (各职业敏捷转躲闪静态系数，基于敏捷转暴击比率)
static const float PLAYER_AGI_TO_CRIT_TO_DODGE[MAX_CLASSES] =
{
    0.0f, // [0]  <Unused> ([0] 未使用)
    1.1f, // [1]  Warrior ([1] 战士)
    1.0f, // [2]  Paladin ([2] 圣骑士)
    1.6f, // [3]  Hunter ([3] 猎人)
    2.0f, // [4]  Rogue ([4] 潜行者)
    1.0f, // [5]  Priest ([5] 牧师)
    1.0f, // [6]  DK ([6] 死亡骑士)
    1.0f, // [7]  Shaman ([7] 萨满祭司)
    1.0f, // [8]  Mage ([8] 法师)
    1.0f, // [9]  Warlock ([9] 术士)
    0.0f, // [10] <Unused> ([10] 未使用)
    1.7f, // [11] Druid ([11] 德鲁伊)
};

float Player::GetDodgeFromAgility(float amount) const
{
    const uint32 pclass = getClass();
    if (pclass >= MAX_CLASSES)
        return 0.0f;
    const uint32 level = std::min(GetLevel(), uint32(GT_MAX_LEVEL));
    const uint32 index = ((pclass - 1) * GT_MAX_LEVEL) + (level - 1);

    // Dodge per agility is proportional to crit per agility, which is available from DBC files (每点敏捷提供的躲闪率与 DBC 中的敏捷暴击系数成正比)
    const GtChanceToMeleeCritEntry* entry = sGtChanceToMeleeCritStore.LookupEntry(index);
    if (!entry)
        return 0.0f;
    return (100.0f * amount * entry->ratio * PLAYER_AGI_TO_CRIT_TO_DODGE[pclass]);
}

float Player::GetSpellCritFromIntellect() const
{
    uint32 level = GetLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL) level = GT_MAX_LEVEL;

    GtChanceToSpellCritBaseEntry const* critBase  = sGtChanceToSpellCritBaseStore.LookupEntry(pclass - 1);
    GtChanceToSpellCritEntry     const* critRatio = sGtChanceToSpellCritStore.LookupEntry((pclass - 1) * GT_MAX_LEVEL + level - 1);
    if (critBase == nullptr || critRatio == nullptr)
        return 0.0f;

    float crit = critBase->base + GetStat(STAT_INTELLECT) * critRatio->ratio;
    return crit * 100.0f;
}

float Player::GetRatingMultiplier(CombatRating cr) const
{
    uint32 level = GetLevel();

    if (level > GT_MAX_LEVEL) level = GT_MAX_LEVEL;

    GtCombatRatingsEntry const* Rating = sGtCombatRatingsStore.LookupEntry(cr * GT_MAX_LEVEL + level - 1);
    if (!Rating)
        return 1.0f;                                        // By default use minimum coefficient (not must be called) (默认返回最低系数，正常不应命中)

    return 1.0f / Rating->ratio;
}

float Player::GetRatingBonusValue(CombatRating cr) const
{
    return float(GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + static_cast<uint16>(cr))) * GetRatingMultiplier(cr);
}

void Player::ApplyRatingMod(CombatRating cr, int32 value, bool apply)
{
    m_baseRatingValue[cr] += (apply ? value : -value);

    // explicit affected values (同步更新受战斗等级影响的衍生属性)
    switch (cr)
    {
        case CR_HASTE_MELEE:
        {
            float RatingChange = value * GetRatingMultiplier(cr);
            ApplyAttackTimePercentMod(BASE_ATTACK, RatingChange, apply);
            ApplyAttackTimePercentMod(OFF_ATTACK, RatingChange, apply);
            break;
        }
        case CR_HASTE_RANGED:
        {
            float RatingChange = value * GetRatingMultiplier(cr);
            ApplyAttackTimePercentMod(RANGED_ATTACK, RatingChange, apply);
            break;
        }
        case CR_HASTE_SPELL:
        {
            float RatingChange = value * GetRatingMultiplier(cr);
            ApplyCastTimePercentMod(RatingChange, apply);
            break;
        }
        default:
            break;
    }

    UpdateRating(cr);
}

void Player::UpdateRating(CombatRating cr)
{
    int32 amount = m_baseRatingValue[cr];
    if (amount < 0)
        amount = 0;
    SetUInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + static_cast<uint16>(cr), uint32(amount));

    bool affectStats = CanModifyStats();

    switch (cr)
    {
        case CR_WEAPON_SKILL:                               // Implemented in Unit::RollMeleeOutcomeAgainst (招架/防御技能在 Unit::RollMeleeOutcomeAgainst 中应用)
        case CR_DEFENSE_SKILL:
            UpdateDefenseBonusesMod();
            break;
        case CR_DODGE:
            UpdateDodgePercentage();
            break;
        case CR_PARRY:
            UpdateParryPercentage();
            break;
        case CR_BLOCK:
            UpdateBlockPercentage();
            break;
        case CR_HIT_MELEE:
            UpdateWeaponHitChances(BASE_ATTACK);
            UpdateWeaponHitChances(OFF_ATTACK);
            break;
        case CR_HIT_RANGED:
            UpdateWeaponHitChances(RANGED_ATTACK);
            break;
        case CR_HIT_SPELL:
            UpdateSpellHitChances();
            break;
        case CR_CRIT_MELEE:
            if (affectStats)
            {
                UpdateCritPercentage(BASE_ATTACK);
                UpdateCritPercentage(OFF_ATTACK);
            }
            break;
        case CR_CRIT_RANGED:
            if (affectStats)
                UpdateCritPercentage(RANGED_ATTACK);
            break;
        case CR_CRIT_SPELL:
            if (affectStats)
                UpdateAllSpellCritChances();
            break;
        case CR_HIT_TAKEN_MELEE:                            // Implemented in Unit::MeleeMissChanceCalc (近战未命中率在 Unit::MeleeMissChanceCalc 中计算)
        case CR_HIT_TAKEN_RANGED:
            break;
        case CR_HIT_TAKEN_SPELL:                            // Implemented in Unit::MagicSpellHitResult (法术命中率在 Unit::MagicSpellHitResult 中计算)
            break;
        case CR_CRIT_TAKEN_MELEE:                           // Implemented in Unit::CalculateEffectiveCritChance (only for chance to crit) (韧性免爆在 Unit::CalculateEffectiveCritChance 中计算)
        case CR_CRIT_TAKEN_RANGED:
            break;
        case CR_CRIT_TAKEN_SPELL:                           // Implemented in Unit::SpellCriticalBonus (only for chance to crit) (法术爆伤韧性减免在 Unit::SpellCriticalBonus 中计算)
            break;
        case CR_HASTE_MELEE:                                // Implemented in Player::ApplyRatingMod (武器技能等级在 ApplyRatingMod 中处理)
        case CR_HASTE_RANGED:
        case CR_HASTE_SPELL:
            break;
        case CR_WEAPON_SKILL_MAINHAND:                      // Implemented in Unit::RollMeleeOutcomeAgainst (精准等级在 Unit::RollMeleeOutcomeAgainst 中计算)
        case CR_WEAPON_SKILL_OFFHAND:
        case CR_WEAPON_SKILL_RANGED:
            break;
        case CR_EXPERTISE:
            if (affectStats)
            {
                UpdateExpertise(BASE_ATTACK);
                UpdateExpertise(OFF_ATTACK);
            }
            break;
    }
}

void Player::UpdateAllRatings()
{
    for (int cr = 0; cr < MAX_COMBAT_RATING; ++cr)
        UpdateRating(CombatRating(cr));
}

void Player::SetRegularAttackTime()
{
    for (int i = 0; i < MAX_ATTACK; ++i)
    {
        Item* tmpitem = GetWeaponForAttack(WeaponAttackType(i), true, false);
        if (tmpitem)
        {
            ItemPrototype const* proto = tmpitem->GetProto();
            if (proto->Delay)
                SetAttackTime(WeaponAttackType(i), proto->Delay);
            else
                SetAttackTime(WeaponAttackType(i), BASE_ATTACK_TIME);
        }
    }
}

/// 更新玩家指定技能的当前熟练度并判定是否成功升级
void Player::UpdateSkill(uint16 id, uint16 diff)
{
    if (!id)
        return;

    if (diff == 0)
        return;

    SkillStatusMap::iterator itr = mSkillStatus.find(id);
    if (itr == mSkillStatus.end())
        return;

    SkillStatusData& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
        return;

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);
    uint32 data = GetUInt32Value(valueIndex);
    uint32 value = SKILL_VALUE(data);
    uint32 max = SKILL_MAX(data);

    if ((!max) || (!value) || (value >= max))
        return;

    uint32 new_value = value + diff;
    if (new_value > max)
        new_value = max;

    SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, max));

    if (skillStatus.uState != SKILL_NEW)
        skillStatus.uState = SKILL_CHANGED;

    return;
}

inline int SkillGainChance(uint32 SkillValue, uint32 GrayLevel, uint32 GreenLevel, uint32 YellowLevel)
{
    if (SkillValue >= GrayLevel)
        return sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_GREY) * 10;
    if (SkillValue >= GreenLevel)
        return sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_GREEN) * 10;
    if (SkillValue >= YellowLevel)
        return sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_YELLOW) * 10;
    return sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_ORANGE) * 10;
}

bool Player::UpdateCraftSkill(uint32 spellid)
{
    DEBUG_LOG("UpdateCraftSkill spellid %d", spellid);

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(spellid);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* skill = _spell_idx->second;
        if (skill->skillId)
        {
            uint32 SkillValue = GetSkillValuePure(skill->skillId);

            // Alchemy Discoveries here (炼金领悟等商业技能制造触发)
            SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(spellid);
            if (spellEntry && spellEntry->Mechanic == MECHANIC_DISCOVERY)
            {
                if (uint32 discoveredSpell = GetSkillDiscoverySpell(skill->skillId, spellid, this))
                    learnSpell(discoveredSpell, false);
            }

            uint32 craft_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_CRAFTING);

            return UpdateSkillPro(skill->skillId, SkillGainChance(SkillValue,
                                  skill->max_value,
                                  (skill->max_value + skill->min_value) / 2,
                                  skill->min_value),
                                  craft_skill_gain);
        }
    }
    return false;
}

bool Player::UpdateGatherSkill(uint32 SkillId, uint32 SkillValue, uint32 RedLevel, uint32 Multiplicator)
{
    DEBUG_LOG("UpdateGatherSkill(SkillId %d SkillLevel %d RedLevel %d)", SkillId, SkillValue, RedLevel);

    uint32 gathering_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING);

    if (RedLevel == 1) // stuff that starts at 1 should stop at 105 not 101 (初始 1 点的采集技能在 105 点变灰而非 101 点)
        RedLevel = 5;

    // For skinning and Mining chance decrease with level. 1-74 - no decrease, 75-149 - 2 times, 225-299 - 8 times (剥皮与采矿随技能段提升衰减升级概率)
    switch (SkillId)
    {
        case SKILL_HERBALISM:
        case SKILL_LOCKPICKING:
        case SKILL_JEWELCRAFTING:
            return UpdateSkillPro(SkillId, SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator, gathering_skill_gain);
        case SKILL_SKINNING:
        {
            if (sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS) == 0)
                return UpdateSkillPro(SkillId, SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator, gathering_skill_gain);
            return UpdateSkillPro(SkillId, (SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator) >> (SkillValue / sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS)), gathering_skill_gain);
        }
        case SKILL_MINING:
        {
            if (sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS) == 0)
                return UpdateSkillPro(SkillId, SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator, gathering_skill_gain);
            return UpdateSkillPro(SkillId, (SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator) >> (SkillValue / sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS)), gathering_skill_gain);
        }
    }
    return false;
}

uint8 GetFishingStepsNeededToLevelUp(uint32 SkillValue)
{
    // These formulas are guessed to be as close as possible to how the skill difficulty curve for fishing was on Retail. (钓鱼升级所需成功次数曲线，尽量贴近官服)
    if (SkillValue < 75)
        return 1;

    if (SkillValue <= 300)
        return SkillValue / 44;

    return SkillValue / 31;
}

bool Player::UpdateFishingSkill()
{
    DEBUG_LOG("UpdateFishingSkill");

    uint32 SkillValue = GetSkillValuePure(SKILL_FISHING);

    if (SkillValue >= GetSkillMax(SKILL_FISHING))
        return false;

    uint8 stepsNeededToLevelUp = GetFishingStepsNeededToLevelUp(SkillValue);
    ++m_fishingSteps;
    // random component, gets more rare as skill gets higher - evidence - when 4 skillups are needed, can be between 2 and 4 (随机波动分量：技能越高越稀有)
    // actual chances unknown - fishing steps acts as a grace mechanism and random component acts as quasi-normal skillup (钓鱼步数作为保底机制，随机项模拟准正态升级)
    if (urand(0, stepsNeededToLevelUp) == 0)
        ++m_fishingSteps;

    // impl of SPELL_ATTR_EX_SPECIAL_SKILLUP (SPELL_ATTR_EX_SPECIAL_SKILLUP 特殊钓鱼提升逻辑)
    if (m_fishingSteps >= stepsNeededToLevelUp)
    {
        m_fishingSteps -= stepsNeededToLevelUp;

        return UpdateSkillPro(SKILL_FISHING, 100*10, 1);
    }

    return false;
}

bool Player::UpdateSkillPro(uint16 SkillId, int32 Chance, uint16 diff)
{
    DEBUG_LOG("UpdateSkillPro(SkillId %d, Chance %3.1f%%)", SkillId, Chance / 10.0);
    if (!SkillId)
        return false;

    if (Chance <= 0)                                        // speedup in 0 chance case (概率为 0 时快速返回)
    {
        DEBUG_LOG("Player::UpdateSkillPro Chance=%3.1f%% missed", Chance / 10.0);
        return false;
    }

    SkillStatusMap::iterator itr = mSkillStatus.find(SkillId);
    if (itr == mSkillStatus.end())
        return false;

    SkillStatusData& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
        return false;

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);

    uint32 data = GetUInt32Value(valueIndex);
    uint16 SkillValue = SKILL_VALUE(data);
    uint16 MaxValue   = SKILL_MAX(data);

    if (!MaxValue || !SkillValue || SkillValue >= MaxValue)
        return false;

    int32 Roll = irand(1, 1000);

    if (Roll <= Chance)
    {
        uint32 new_value = SkillValue + diff;
        if (new_value > MaxValue)
            new_value = MaxValue;

        SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, MaxValue));

        if (skillStatus.uState != SKILL_NEW)
            skillStatus.uState = SKILL_CHANGED;

        DEBUG_LOG("Player::UpdateSkillPro Chance=%3.1f%% taken", Chance / 10.0);
        return true;
    }

    DEBUG_LOG("Player::UpdateSkillPro Chance=%3.1f%% missed", Chance / 10.0);
    return false;
}

void Player::UpdateWeaponSkill(WeaponAttackType attType)
{
    // no skill gain in pvp (PvP 战斗中不提升武器/防御技能)
    Unit* victim = GetVictim();
    if (victim)
    {
        if (victim->IsPlayerControlled())
            return;

        if (victim->IsNoWeaponSkillGain())
            return;
    }

    if (IsNoWeaponShapeShift())
        return;

    uint32 weaponSkillGain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_WEAPON);

    Item* pWeapon = GetWeaponForAttack(attType, true, true);
    if (pWeapon && pWeapon->GetProto()->SubClass != ITEM_SUBCLASS_WEAPON_FISHING_POLE)
        UpdateSkill(pWeapon->GetSkill(), weaponSkillGain);
    else if (!pWeapon && attType == BASE_ATTACK)
        UpdateSkill(SKILL_UNARMED, weaponSkillGain);

    UpdateAllCritPercentages();
}

void Player::UpdateCombatSkills(uint32 procEx, WeaponAttackType attType, bool defence)
{
    const uint16 skillId = (defence ? SKILL_DEFENSE : GetWeaponSkillIdForAttack(attType));
    const uint16 skill = GetSkillValuePure(skillId);
    const uint16 skillMax = GetSkillMaxPure(skillId);
    const int32 room = int32(skillMax - skill);
    const int32 level = GetLevel();

    // Skill already capped or invalid state: nothing to gain (技能已满或状态无效，无法提升)
    if (skillMax == 0 || level == 0 || skill >= skillMax)
        return;

    // skillGapLogGrowth controls the slope of the logarithmic catch-up portion (skillGapLogGrowth 控制追赶区段对数增长斜率)
    const double skillGapLogGrowth = 0.22 * (1.0 + (12.0 / level));
    // levelCapScale normalizes the curve across realm caps and anchors the seam at room = 7 (levelCapScale 归一化跨等级上限曲线并在差值 7 处平滑衔接)
    // TODO: Figure out if scaling of 295 to 300 is same in vanilla, tbc and wotlk when level 60 (待办：确认 60 级时 295-300 区段在各资料片的缩放规律)
    const double levelCapScale = 1;

    double baseChance = 0.0;

    // Piecewise curve: logarithmic growth for catch-up, quadratic decay near the cap (分段概率曲线：低技能对数追赶，临近上限二次衰减)
    if (room >= 7)
        baseChance = (7.0 / skillMax) * levelCapScale
                   + skillGapLogGrowth * std::log((room + 5.0) / 12.0);
    else
        baseChance = (room * room) / (7.0 * skillMax) * levelCapScale;

    baseChance = std::clamp(baseChance, 0.0, 1.0);

    // Weapon skills gain a small bonus from intellect (智力对武器技能提升速度提供小幅加成)
    double intellectBonus = 0.0;
    if (!defence)
    {
        const double maxIntellectBonus = 0.10;
        const double intellect = GetStat(STAT_INTELLECT);
        // TODO: Figure out actual intellect scaling contribution - likely a per level value like everything else and not linearly (待办：确认智力对技能提升的准确等级缩放公式)
        const int levelBracket =    (level <= 60) ? 60 :
                                    (level <= 70) ? 70 :
                                    (level <= 80) ? 80 : 255;

        const double intellectMax = (level <= 60) ? 750 :
                                    (level <= 70) ? 1500 :
                                    (level <= 80) ? 3000 : 6000; // default fallback (默认回退值)

        intellectBonus = maxIntellectBonus *
                         (intellect / intellectMax) *
                         (static_cast<double>(levelBracket) / static_cast<double>(level));

        intellectBonus = std::clamp(intellectBonus, 0.0, maxIntellectBonus);
    }

    // Final skill-up probability (最终技能提升概率)
    const float finalChance = std::clamp(baseChance + intellectBonus, 0.0, 1.0) * 100.0f;

    if (roll_chance_f(finalChance))
    {
        if (defence)
            UpdateDefense(procEx);
        else
            UpdateWeaponSkill(attType);
    }
}

uint16 Player::GetWeaponSkillIdForAttack(WeaponAttackType attType) const
{
    Item* item = GetWeaponForAttack(attType, true, true);

    // unarmed only with base attack (徒手攻击仅适用于主手基础攻击)
    if (attType != BASE_ATTACK && !item)
        return 0;

    // weapon skill or (unarmed for base attack) (返回武器对应技能或主手徒手技能)
    return (item ? item->GetSkill() : uint16(SKILL_UNARMED));
}

SkillRaceClassInfoEntry const* Player::GetSkillInfo(uint16 id, std::function<bool (SkillRaceClassInfoEntry const&)> filterfunc/* = nullptr*/) const
{
    if (!id)
        return nullptr;

    uint32 raceMask = getRaceMask();
    uint32 classMask = getClassMask();

    auto bounds = sSpellMgr.GetSkillRaceClassInfoMapBounds(id);

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        SkillRaceClassInfoEntry const* entry = itr->second;

        if (!(entry->raceMask & raceMask))
            continue;

        if (!(entry->classMask & classMask))
            continue;

        if (filterfunc == nullptr || filterfunc(*entry))
            return entry;
    }

    return nullptr;
}

bool Player::HasSkill(uint16 id) const
{
    if (!id)
        return false;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(id);
    return (itr != mSkillStatus.end() && itr->second.uState != SKILL_DELETED);
}

// This functions sets a skill line value (and adds if doesn't exist yet) (设置技能条数值，若不存在则新增)
// To "remove" a skill line, set it's values to zero (将当前值与上限均设为 0 即可移除该技能条)
void Player::SetSkill(SkillStatusMap::iterator itr, uint16 value, uint16 max, uint16 step/* = 0*/)
{
    if (itr == mSkillStatus.end())
        return;

    const uint16 id = uint16(itr->first);
    SkillStatusData& status = itr->second;

    if (status.uState == SKILL_DELETED)
        return;

    if (value)  // Update (更新现有技能值)
    {
        if (step)
            SetUInt32Value(PLAYER_SKILL_INDEX(status.pos), MAKE_PAIR32(id, step));

        SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(status.pos), MAKE_SKILL_VALUE(value, max));

        if (status.uState != SKILL_NEW)
            status.uState = SKILL_CHANGED;
    }
    else        // Remove (移除技能条)
    {
        SetUInt32Value(PLAYER_SKILL_INDEX(status.pos), 0);
        SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(status.pos), 0);
        SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(status.pos), 0);

        if (status.uState == SKILL_NEW)
            mSkillStatus.erase(itr);
        else
            status.uState = SKILL_DELETED;
    }

    // Learn/unlearn all spells auto-trained by this skill on change (技能变更时自动学习或遗忘关联的自动训练法术)
    UpdateSkillTrainedSpells(id, value);

    // On updating specific skills values (更新特定技能时的连带刷新，如骑术刷新光环、防御刷新抗暴等)
    switch (id)
    {
        case SKILL_DEFENSE:     UpdateDefenseBonusesMod();  break;
    }
}

void Player::SetSkill(uint16 id, uint16 value, uint16 max, uint16 step/* = 0*/)
{
    if (!id)
        return;

    auto itr = mSkillStatus.find(id);
    const bool exists = (itr != mSkillStatus.end());

    if (exists && itr->second.uState != SKILL_DELETED)  // Update/remove existing (已存在该技能，执行更新或移除)
        SetSkill(itr, value, max, step);
    else if (value)                                     // Add new (新增技能条)
    {
        if (!exists)
        {
            SkillLineEntry const* entry = sSkillLineStore.LookupEntry(id);
            if (!entry)
            {
                sLog.outError("Skill not found in SkillLineStore: skill #%u", id);
                return;
            }
        }

        for (uint8 pos = 0; pos < PLAYER_MAX_SKILLS; ++pos)
        {
            if (GetUInt32Value(PLAYER_SKILL_INDEX(pos)))
                continue;

            if (exists) // Re-use and move data for old status entry if not deleted yet (若此前存在被删除但未存库的技能槽，则复用其槽位)
            {
                itr->second.pos = pos;
                itr->second.uState = SKILL_CHANGED;
            }
            else        // Add a completely new status entry (查找空闲槽位以添加全新技能状态)
            {
                auto result = mSkillStatus.insert(SkillStatusMap::value_type(id, SkillStatusData(pos, SKILL_NEW)));

                if (!result.second) // Insert failed (找不到空槽位，插入失败)
                    return;

                itr = result.first;
            }

            SetUInt32Value(PLAYER_SKILL_INDEX(pos), MAKE_PAIR32(id, step));         // Set/reset skill id and step (写入技能 ID 与进阶阶段)
            SetSkill(itr, value, max);                                              // Set current and max values (写入当前值与最大值)
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(pos), 0);                       // Reset bonus data (重置加成数据)
            for (auto type : { SPELL_AURA_MOD_SKILL, SPELL_AURA_MOD_SKILL_TALENT }) // Set temporary, permanent bonuses (设置临时加成与永久加成)
            {
                for (auto aura : GetAurasByType(type))
                {
                    if (aura->GetModifier()->m_miscvalue == int32(id))
                        aura->ApplyModifier(true);
                }
            }
            break;
        }
    }
}

uint16 Player::GetSkill(uint16 id, bool bonusPerm, bool bonusTemp, bool max/* = false*/) const
{
    if (!id)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(id);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    SkillStatusData const& skillStatus = itr->second;

    uint32 field = GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos));

    // Pure value (纯基础技能值)
    int32 value = (max ? SKILL_MAX(field) : SKILL_VALUE(field));

    // Bonus values (计算永久与临时加成)
    if (bonusPerm || bonusTemp)
    {
        uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos));

        if (bonusPerm)
            value += SKILL_PERM_BONUS(bonus);

        if (bonusTemp)
            value += SKILL_TEMP_BONUS(bonus);
    }

    return uint16(std::max(0, value));
}

void Player::SetSkillStep(uint16 id, uint16 step)
{
    if (!id || step > MAX_SKILL_STEP)
        return;

    if (step)   // Set step (设置技能阶位)
    {
        uint16 val = 0;
        uint16 max = 0;

        auto filterfunc = [&] (SkillRaceClassInfoEntry const& entry)
        {
            if (entry.skillTierId)
            {
                if (SkillTiersEntry const* steps = sSkillTiersStore.LookupEntry(entry.skillTierId))
                {
                    val = uint16(steps->skillValue[(step - 1)]);
                    max = uint16(steps->maxSkillValue[(step - 1)]);
                    return true;
                }
            }
            return false;
        };

        bool maxed = false;

        if (SkillRaceClassInfoEntry const* entry = GetSkillInfo(id, filterfunc))
            maxed = (entry->flags & SKILL_FLAG_MAXIMIZED);

        if (max)
        {
            // Note: Some SkillTiers entries contain 0 as starting value for first step, needs investigation (sanitized for now) (注：部分 SkillTiers 首阶起始值为 0，暂做容错处理)
            const uint16 value = std::max(uint16(1), uint16(maxed ? max : GetSkillValuePure(id)));
            SetSkill(id, value, max, step);
        }
    }
    else        // Unlearn (当 max 传 0 时触发遗忘技能)
        SetSkill(id, 0, 0);
}

uint16 Player::GetSkillStep(uint16 id) const
{
    if (!id)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(id);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    SkillStatusData const& skillStatus = itr->second;

    return PAIR32_HIPART(GetUInt32Value(PLAYER_SKILL_INDEX(skillStatus.pos)));
}

bool Player::ModifySkillBonus(uint16 id, int16 diff, bool permanent/* = false*/)
{
    if (!id || !diff)
        return false;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(id);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return false;

    uint16 bonusIndex = PLAYER_SKILL_BONUS_INDEX(itr->second.pos);
    uint32 bonus = GetUInt32Value(bonusIndex);
    const int16 bonusTemporary = SKILL_TEMP_BONUS(bonus);
    const int16 bonusPermanent = SKILL_PERM_BONUS(bonus);

    if (permanent)
        SetUInt32Value(bonusIndex, MAKE_SKILL_BONUS(bonusTemporary, (bonusPermanent + diff))); // permanent bonus stored in high part (永久加成存储在高 16 位)
    else
        SetUInt32Value(bonusIndex, MAKE_SKILL_BONUS((bonusTemporary + diff), bonusPermanent)); // temporary/item bonus stored in low part (临时/装备加成存储在低 16 位)

    // On updating specific skills values (更新特定技能时的连带刷新)
    switch (id)
    {
        case SKILL_DEFENSE:     UpdateDefenseBonusesMod();  break;
    }

    return true;
}

int16 Player::GetSkillBonus(uint16 id, bool permanent/*= false*/) const
{
    if (!id)
        return 0;

    SkillStatusMap::const_iterator itr = mSkillStatus.find(id);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    SkillStatusData const& skillStatus = itr->second;

    uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos));

    return (permanent ? SKILL_PERM_BONUS(bonus) : SKILL_TEMP_BONUS(bonus));
}

void Player::UpdateSkillsForLevel(bool maximize/* = false*/)
{
    if (!maximize)
        maximize = sWorld.getConfig(CONFIG_BOOL_ALWAYS_MAX_SKILL_FOR_LEVEL);

    const uint16 maxPeronal = GetSkillMaxForLevel();
    const uint16 maxGlobal = sWorld.GetConfigMaxSkillValue();
    const uint16 maxNew = std::min(maxPeronal, maxGlobal);

    for (auto& pair : mSkillStatus)
    {
        SkillStatusData& skillStatus = pair.second;
        if (skillStatus.uState == SKILL_DELETED)
            continue;

        uint16 skillId = uint16(pair.first);

        SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skillId);
        if (!pSkill)
            continue;

        SkillRangeType skillType = GetSkillRangeType(pSkill, false);
        if (skillType != SKILL_RANGE_LEVEL && skillType != SKILL_RANGE_MONO)
            continue;

        bool maxed = maximize;

        if (!maxed)
        {
            if (SkillRaceClassInfoEntry const* entry = GetSkillInfo(skillId))
                maxed = (entry->flags & SKILL_FLAG_MAXIMIZED);
        }

        const uint32 data = GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos));
        const uint16 max = SKILL_MAX(data);

        if (max != 1)
        {
            const uint16 val = SKILL_VALUE(data);
            const uint16 valNew = (maxed ? maxNew : val);

            if (maxNew != max || valNew != val)
                SetSkill(skillId, valNew, maxNew);
        }
    }
}

void Player::UpdateSkillTrainedSpells(uint16 id, uint16 currVal)
{
    uint32 raceMask  = getRaceMask();
    uint32 classMask = getClassMask();
    uint16 step = GetSkillStep(id);

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySkillId(id);

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (SkillLineAbilityEntry const* pAbility = itr->second)
        {
            // Update training: skill removal mode, wipe all dependent spells regardless of training method (技能移除模式：清除所有关联派生法术)
            if (!currVal)
            {
                removeSpell(pAbility->spellId, false, false, true);
                continue;
            }

            // Check race if set (检查种族匹配)
            if (pAbility->racemask && !(pAbility->racemask & raceMask))
                continue;

            // Check class if set (检查职业匹配)
            if (pAbility->classmask && !(pAbility->classmask & classMask))
                continue;

            // Check if auto-training method is set, skip if not (检查是否设置了自动学习方式，未设置则跳过)
            if (!pAbility->learnOnGetSkill)
            {
                // Check if its actually an original profession/tradeskill spell and we miss it somehow - repair (若为遗漏的基础商业技能法术则自动补学)
                if (SpellLearnSkillNode const* training = sSpellMgr.GetSpellLearnSkill(pAbility->spellId))
                {
                    if (training->skill == id && training->effect == SPELL_EFFECT_SKILL && training->step < step)
                    {
                        if (!HasSpell(pAbility->spellId))
                        {
                            if (!IsInWorld())
                                addSpell(pAbility->spellId, true, true, true, false);
                            else
                                learnSpell(pAbility->spellId, true);
                        }
                    }
                }
                continue;
            }

            // Update training: needs unlearning spell if current value is too low (当前技能不足时遗忘对应法术)
            if (currVal < pAbility->req_skill_value)
                removeSpell(pAbility->spellId);
            // Update training: needs learning (达到所需技能点时自动学习)
            else if (!IsInWorld())
                addSpell(pAbility->spellId, true, true, true, false);
            else
                learnSpell(pAbility->spellId, true);
        }
    }
}

void Player::UpdateSpellTrainedSkills(uint32 spellId, bool apply)
{
    if (SpellLearnSkillNode const* skillLearnInfo = sSpellMgr.GetSpellLearnSkill(spellId))
    {
        // Specifically defined: no checks needed (显式定义的目标技能：无需额外过滤)
        if (apply)
            SetSkillStep(skillLearnInfo->skill, skillLearnInfo->step);
        else if (HasSkill(skillLearnInfo->skill))
            SetSkillStep(skillLearnInfo->skill, (skillLearnInfo->step ? (skillLearnInfo->step - 1) : 0));
    }
    else
    {
        // Detect obtained child spells of specific skills (检测由特定技能获取的子法术)
        auto bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(spellId);

        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            SkillLineAbilityEntry const* info = itr->second;

            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(info->skillId);
            if (!pSkill)
                continue;

            const uint16 skillId = uint16(pSkill->id);

            switch (skillId)            // Legacy workarounds: (历史兼容处理：)
            {
                case SKILL_POISONS:     // Poisons/lockpicking special case: no ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL (制毒与开锁特例：不依赖常规技能自动获取标记)
                case SKILL_LOCKPICKING:
                    if (info->max_value)
                        continue;
                    break;
                default:                // Spells obtained via ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL only (仅通过种族/职业技能获得标记学习的法术)
                    if (info->learnOnGetSkill != ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL)
                        continue;
            }

            if (apply)
            {
                if (HasSkill(skillId))
                    continue;

                // Check if obtainable (检查玩家种族/职业是否可获得)
                SkillRaceClassInfoEntry const* entry = GetSkillInfo(skillId);
                if (!entry)
                    continue;

                switch (GetSkillRangeType(pSkill, info->racemask != 0))
                {
                    case SKILL_RANGE_LANGUAGE:
                        SetSkill(skillId, 300, 300);
                        break;
                    case SKILL_RANGE_LEVEL:
                        SetSkill(skillId, 1, GetSkillMaxForLevel());
                        break;
                    case SKILL_RANGE_MONO:
                        if (entry->flags & SKILL_FLAG_MAXIMIZED)
                            SetSkill(skillId, GetSkillMaxForLevel(), GetSkillMaxForLevel());
                        else
                            SetSkill(skillId, 1, 1);
                        break;
                    default:
                        break;
                }
            }
            else
            {
                switch (pSkill->categoryId)         // Legacy workarounds: (历史兼容处理：)
                {
                    case SKILL_CATEGORY_CLASS:      // not unlearn class skills (spellbook/talent pages) (不遗忘职业核心技能页)
                        continue;
                    case SKILL_CATEGORY_SECONDARY:  // not reset skills for professions and racial abilities (不重置商业技能与种族特长技能)
                    case SKILL_CATEGORY_PROFESSION:
                        if (IsProfessionSkill(skillId) || info->racemask != 0)
                            continue;
                        SetSkill(skillId, 0, 0);
                        break;

                    default:
                        SetSkill(skillId, 0, 0);
                }
            }
        }
    }
}

void Player::LearnDefaultSkills()
{
    // learn default race/class skills (学习默认种族与职业技能)
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());

    for (auto tskill : info->skill)
    {
        if (HasSkill(tskill.SkillId))
            continue;

        DEBUG_LOG("PLAYER (Class: %u Race: %u): Adding initial skill, id = %u", uint32(getClass()), uint32(getRace()), uint32(tskill.SkillId));

        uint16 value = 0;
        uint16 max = 0;
        uint16 step = 0;

        if (SkillLineEntry const* entry = sSkillLineStore.LookupEntry(tskill.SkillId))
        {
            switch (GetSkillRangeType(entry, false))
            {
                case SKILL_RANGE_LANGUAGE:
                    value = max = 300;
                    break;
                case SKILL_RANGE_LEVEL:
                    value = 1;
                    max = GetSkillMaxForLevel();
                    break;
                case SKILL_RANGE_MONO:
                case SKILL_RANGE_RANK:
                {
                    const bool predefined = (tskill.Step != 0);
                    step = (predefined ? tskill.Step : 1);
                    uint32 stepIndex = (step - 1);

                    auto filterfunc = [&] (SkillRaceClassInfoEntry const& entry) -> bool
                    {
                        if (entry.skillTierId)
                        {
                            if (SkillTiersEntry const* steps = sSkillTiersStore.LookupEntry(entry.skillTierId))
                            {
                                value = 1;
                                max = uint16(steps->maxSkillValue[stepIndex]);

                                if (entry.flags & SKILL_FLAG_MAXIMIZED)
                                    value = max;
                                else if (predefined)
                                    value = std::max(uint16(1), uint16((GetLevel() - 1) * 5));
                                return true;
                            }
                        }
                        else if (entry.flags & SKILL_FLAG_MAXIMIZED)
                        {
                            value = max = GetSkillMaxForLevel();
                            return true;
                        }

                        return false;
                    };

                    if (!GetSkillInfo(tskill.SkillId, filterfunc))
                        continue;

                    break;
                }
                default:
                    break;
            }
        }
        SetSkill(tskill.SkillId, value, max, step);
    }
}

uint32 Player::GetSpellRank(SpellEntry const* spellInfo) const
{
    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(spellInfo->Id);
    if (bounds.first != bounds.second)
    {
        SkillLineAbilityEntry const* skillInfo = bounds.first->second;
        uint32 spellRank = GetSkillValue(skillInfo->skillId);
        if (spellInfo->maxLevel > 0 && spellRank >= spellInfo->maxLevel * 5)
            spellRank = spellInfo->maxLevel * 5;
        return spellRank;
    }
    else
        return 0;
}

void Player::SendInitialActionButtons() const
{
    DETAIL_LOG("Initializing Action Buttons for '%u'", GetGUIDLow());

    WorldPacket data(SMSG_ACTION_BUTTONS, (MAX_ACTION_BUTTONS * 4));
    for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
    {
        ActionButtonList::const_iterator itr = m_actionButtons.find(button);
        if (itr != m_actionButtons.end() && itr->second.uState != ACTIONBUTTON_DELETED)
            data << uint32(itr->second.packedData);
        else
            data << uint32(0);
    }

    GetSession()->SendPacket(data);
    DETAIL_LOG("Action Buttons for '%u' Initialized", GetGUIDLow());
}

bool Player::IsActionButtonDataValid(uint8 button, uint32 action, uint8 type, Player* player)
{
    if (button >= MAX_ACTION_BUTTONS)
    {
        if (player)
            sLog.outError("Action %u not added into button %u for player %s: button must be < %u", action, button, player->GetName(), MAX_ACTION_BUTTONS);
        else
            sLog.outError("Table `playercreateinfo_action` have action %u into button %u : button must be < %u", action, button, MAX_ACTION_BUTTONS);
        return false;
    }

    if (action >= MAX_ACTION_BUTTON_ACTION_VALUE)
    {
        if (player)
            sLog.outError("Action %u not added into button %u for player %s: action must be < %u", action, button, player->GetName(), MAX_ACTION_BUTTON_ACTION_VALUE);
        else
            sLog.outError("Table `playercreateinfo_action` have action %u into button %u : action must be < %u", action, button, MAX_ACTION_BUTTON_ACTION_VALUE);
        return false;
    }

    switch (type)
    {
        case ACTION_BUTTON_SPELL:
        {
            SpellEntry const* spellProto = sSpellTemplate.LookupEntry<SpellEntry>(action);
            if (!spellProto)
            {
                if (player)
                    sLog.outError("Spell action %u not added into button %u for player %s: spell not exist", action, button, player->GetName());
                else
                    sLog.outError("Table `playercreateinfo_action` have spell action %u into button %u: spell not exist", action, button);
                return false;
            }

            if (player)
            {
                if (!player->HasSpell(spellProto->Id))
                {
                    sLog.outError("Spell action %u not added into button %u for player %s: player don't known this spell", action, button, player->GetName());
                    return false;
                }
                if (IsPassiveSpell(spellProto))
                {
                    sLog.outError("Spell action %u not added into button %u for player %s: spell is passive", action, button, player->GetName());
                    return false;
                }
            }
            break;
        }
        case ACTION_BUTTON_ITEM:
        {
            if (!ObjectMgr::GetItemPrototype(action))
            {
                if (player)
                    sLog.outError("Item action %u not added into button %u for player %s: item not exist", action, button, player->GetName());
                else
                    sLog.outError("Table `playercreateinfo_action` have item action %u into button %u: item not exist", action, button);
                return false;
            }
            break;
        }
        default:
            break;                                          // other cases not checked at this moment (其余动作条类型暂不做额外校验)
    }

    return true;
}

ActionButton* Player::addActionButton(uint8 button, uint32 action, uint8 type)
{
    if (!IsActionButtonDataValid(button, action, type, this))
        return nullptr;

    // it create new button (NEW state) if need or return existing (查找已有按钮或创建新按钮条目)
    ActionButton& ab = m_actionButtons[button];

    // set data and update to CHANGED if not NEW (更新按钮内容并在非 NEW 时标记为 CHANGED)
    ab.SetActionAndType(action, ActionButtonType(type));

    DETAIL_LOG("Player '%u' Added Action '%u' (type %u) to Button '%u'", GetGUIDLow(), action, uint32(type), button);
    return &ab;
}

void Player::removeActionButton(uint8 button)
{
    ActionButtonList::iterator buttonItr = m_actionButtons.find(button);
    if (buttonItr == m_actionButtons.end())
        return;

    if (buttonItr->second.uState == ACTIONBUTTON_NEW)
        m_actionButtons.erase(buttonItr);                   // new and not saved (新建且尚未存库，直接从内存移除)
    else
        buttonItr->second.uState = ACTIONBUTTON_DELETED;    // saved, will deleted at next save (已存库按钮，标记为 DELETED 待下次存盘删除)

    DETAIL_LOG("Action Button '%u' Removed from Player '%u'", button, GetGUIDLow());
}

/// 更新玩家服务端位置坐标、可见性及定时地形/探索状态
bool Player::SetPosition(float x, float y, float z, float orientation, bool teleport)
{
    // prevent crash when a bad coord is sent by the client (校验坐标合法性，防止客户端发来非法坐标导致崩溃)
    if (!MaNGOS::IsValidMapCoord(x, y, z, orientation))
    {
        DEBUG_LOG("Player::SetPosition(%f, %f, %f, %f, %d) .. bad coordinates for player %d!", x, y, z, orientation, teleport, GetGUIDLow());
        return false;
    }

    Map* m = GetMap();

    const float old_x = GetPositionX();
    const float old_y = GetPositionY();
    const float old_z = GetPositionZ();
    const float old_r = GetOrientation();

    if (teleport || old_x != x || old_y != y || old_z != z || old_r != orientation)
    {
        if (teleport || old_x != x || old_y != y || old_z != z)
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MOVING | AURA_INTERRUPT_FLAG_TURNING);
        else
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TURNING);

        // move and update visible state if need (执行地图重定位并更新周围可见性)
        m->PlayerRelocation(this, x, y, z, orientation);

        // reread after Map::Relocation (重定位后重新读取坐标)
        m = GetMap();
        x = GetPositionX();
        y = GetPositionY();
        z = GetPositionZ();

        // group update (标记队伍位置更新)
        if (GetGroup() && (old_x != x || old_y != y))
            SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POSITION);

        if (GetTrader() && !IsWithinDistInMap(GetTrader(), TRADE_DISTANCE, false))
            TradeCancel(true);

        if (m_needsZoneUpdate)
        {
            // Get server side data (读取服务端真实区域信息)
            uint32 newzone, newarea;
            GetZoneAndAreaId(newzone, newarea);
            if (newzone != m_newZone)
            {
                sLog.outError("Delayed Zone Update: Client sent invalid zoneId for X,Y & MAP Coordinates. GUID: %u zoneId: %u Expected %u, Coords: %f %f %f", GetGUIDLow(), m_newZone, newzone, x, y, z);
                m_newZone = newzone;
            }

            UpdateZone(m_newZone, newarea);
            m_needsZoneUpdate = false;
        }
    }

    if (m_positionStatusUpdateTimer)                        // Update position's state only on interval (按间隔节流更新位置相关环境状态)
        return true;
    m_positionStatusUpdateTimer = 100;

    // code block for underwater state update (水下/地形环境状态更新)
    UpdateTerainEnvironmentFlags(m, x, y, z);

    // code block for outdoor state and area-explore check (室外状态与地图探索检查)
    CheckAreaExploreAndOutdoor();

    return true;
}

void Player::SaveRecallPosition()
{
    m_recallMap = GetMapId();
    m_recallX = GetPositionX();
    m_recallY = GetPositionY();
    m_recallZ = GetPositionZ();
    m_recallO = GetOrientation();
}

void Player::SendMessageToSet(WorldPacket const& data, bool self) const
{
    if (IsInWorld())
        GetMap()->MessageBroadcast(this, data, false);

    // if player is not in world and map in not created/already destroyed (若玩家不在世界中或地图未就绪/已销毁，)
    // no need to create one, just send packet for itself! (无需访问地图，仅向自身发送数据包)
    if (self)
        GetSession()->SendPacket(data);
}

void Player::SendMessageToSetInRange(WorldPacket const& data, float dist, bool self) const
{
    if (IsInWorld())
        GetMap()->MessageDistBroadcast(this, data, dist, false);

    if (self)
        GetSession()->SendPacket(data);
}

void Player::SendMessageToSetInRange(WorldPacket const& data, float dist, bool self, bool own_team_only) const
{
    if (IsInWorld())
        GetMap()->MessageDistBroadcast(this, data, dist, false, own_team_only);

    if (self)
        GetSession()->SendPacket(data);
}

void Player::SendMessageToAllWhoSeeMe(WorldPacket const& data, bool self) const
{
    Unit::SendMessageToAllWhoSeeMe(data, self);

    if (self)
        GetSession()->SendPacket(data);
}

void Player::SendDirectMessage(WorldPacket const& data) const
{
    GetSession()->SendPacket(data);
}

void Player::SendCinematicStart(uint32 CinematicSequenceId)
{
    WorldPacket data(SMSG_TRIGGER_CINEMATIC, 4);
    data << uint32(CinematicSequenceId);
    SendDirectMessage(data);

    if (CinematicSequencesEntry const* sequence = sCinematicSequencesStore.LookupEntry(CinematicSequenceId))
    {
        // we can start server side dynamic follow (触发过场动画开始)
        m_cinematicMgr.reset(new CinematicMgr(this));
        m_cinematicMgr->SetActiveCinematicCamera(sequence->cinematicCamera);
    }
}

/// 检查当前区域探索发现状态、旅店休息区与室外被动光环
void Player::CheckAreaExploreAndOutdoor()
{
    if (!IsAlive())
        return;

    if (IsTaxiFlying())
        return;

    bool isOutdoor;
    uint16 areaFlag = GetTerrain()->GetAreaFlag(GetPositionX(), GetPositionY(), GetPositionZ(), &isOutdoor);

    if (isOutdoor)
    {
        if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) && GetRestType() == REST_TYPE_IN_TAVERN)
        {
            AreaTriggerEntry const* at = sAreaTriggerStore.LookupEntry(inn_trigger_id);
            if (!at || !IsPointInAreaTriggerZone(at, GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ()))
            {
                // Player left inn (REST_TYPE_IN_CITY overrides REST_TYPE_IN_TAVERN, so just clear rest) (离开旅店触发器；主城休息状态优先并覆盖旅店状态)
                SetRestType(REST_TYPE_NO);
            }
        }
        // Check if we need to reaply outdoor only passive spells (检查是否需刷新仅限室外生效的被动法术)
        const PlayerSpellMap& sp_list = GetSpellMap();
        for (const auto& itr : sp_list)
        {
            if (itr.second.state == PLAYERSPELL_REMOVED)
                continue;
            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(itr.first);
            if (!spellInfo || !IsNeedCastSpellAtOutdoor(spellInfo) || HasAura(itr.first))
                continue;
            if ((spellInfo->Stances || spellInfo->StancesNot) && !IsNeedCastSpellAtFormApply(spellInfo, GetShapeshiftForm()))
                continue;
            CastSpell(this, spellInfo, TRIGGERED_OLD_TRIGGERED);
        }
        for (auto& setData : m_itemSetEffects)
        {
            for (auto spellInfo : setData.second.spells)
            {
                if (!spellInfo || !IsNeedCastSpellAtOutdoor(spellInfo) || HasAura(spellInfo->Id))
                    continue;
                if ((spellInfo->Stances || spellInfo->StancesNot) && !IsNeedCastSpellAtFormApply(spellInfo, GetShapeshiftForm()))
                    continue;
                CastSpell(this, spellInfo, TRIGGERED_OLD_TRIGGERED);
            }
        }
    }
    else if (sWorld.getConfig(CONFIG_BOOL_VMAP_INDOOR_CHECK) && !IsGameMaster())
        RemoveAurasWithAttribute(SPELL_ATTR_ONLY_OUTDOORS);

    if (areaFlag == 0xffff)
        return;
    int offset = areaFlag / 32;

    if (offset >= PLAYER_EXPLORED_ZONES_SIZE)
    {
        sLog.outError("Wrong area flag %u in map data for (X: %f Y: %f) point to field PLAYER_EXPLORED_ZONES_1 + %u ( %u must be < %u ).", areaFlag, GetPositionX(), GetPositionY(), offset, offset, PLAYER_EXPLORED_ZONES_SIZE);
        return;
    }

    uint32 val = (uint32)(1 << (areaFlag % 32));
    uint32 currFields = GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);

    if (!(currFields & val))
    {
        SetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset, (uint32)(currFields | val));

        AreaTableEntry const* p = GetAreaEntryByAreaFlagAndMap(areaFlag, GetMapId());
        if (!p)
        {
            sLog.outError("PLAYER: Player %u discovered unknown area (x: %f y: %f map: %u", GetGUIDLow(), GetPositionX(), GetPositionY(), GetMapId());
        }
        else if (p->area_level > 0)
        {
            GetSession()->GetAnticheat()->OnExplore(p);

            uint32 area = p->ID;
            if (GetLevel() >= GetMaxAttainableLevel())
            {
                SendExplorationExperience(area, 0);
            }
            else
            {
                int32 diff = int32(GetLevel()) - p->area_level;
                uint32 XP;
                if (diff < -5)
                {
                    XP = uint32(sObjectMgr.GetBaseXP(GetLevel() + 5) * GetMap()->GetXPModRate(RateModType::EXPLORE));
                }
                else if (diff > 5)
                {
                    int32 exploration_percent = (100 - ((diff - 5) * 5));
                    if (exploration_percent > 100)
                        exploration_percent = 100;
                    else if (exploration_percent < 0)
                        exploration_percent = 0;

                    XP = uint32(sObjectMgr.GetBaseXP(p->area_level) * exploration_percent / 100 * GetMap()->GetXPModRate(RateModType::EXPLORE));
                }
                else
                {
                    XP = uint32(sObjectMgr.GetBaseXP(p->area_level) * GetMap()->GetXPModRate(RateModType::EXPLORE));
                }

                GiveXP(XP, nullptr);
                SendExplorationExperience(area, XP);
            }
            DETAIL_LOG("PLAYER: Player %u discovered a new area: %u", GetGUIDLow(), area);
        }
    }
}

Team Player::TeamForRace(uint8 race)
{
    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    if (!rEntry)
    {
        sLog.outError("Race %u not found in DBC: wrong DBC files?", uint32(race));
        return ALLIANCE;
    }

    switch (rEntry->TeamID)
    {
        case 7: return ALLIANCE;
        case 1: return HORDE;
    }

    sLog.outError("Race %u have wrong teamid %u in DBC: wrong DBC files?", uint32(race), rEntry->TeamID);
    return TEAM_NONE;
}

uint32 Player::getFactionForRace(uint8 race)
{
    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    if (!rEntry)
    {
        sLog.outError("Race %u not found in DBC: wrong DBC files?", uint32(race));
        return 0;
    }

    return rEntry->FactionID;
}

void Player::setFactionForRace(uint8 race)
{
    m_team = TeamForRace(race);
    setFaction(getFactionForRace(race));
}

ReputationRank Player::GetReputationRank(uint32 faction) const
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry<FactionEntry>(faction);
    return GetReputationMgr().GetRank(factionEntry);
}

// Calculate total reputation percent player gain with quest/creature level (计算任务或击杀生物的最终声望收益)
/// 计算任务或击杀生物的最终声望增减值（含等级差衰减与倍率加成）
int32 Player::CalculateReputationGain(ReputationSource source, int32 rep, int32 maxRep, int32 faction, uint32 creatureOrQuestLevel, bool noAuraBonus)
{
    float percent = 100.0f;

    float repMod = noAuraBonus ? 0.0f : (float)GetTotalAuraModifier(SPELL_AURA_MOD_REPUTATION_GAIN);

    // faction specific auras only seem to apply to kills (阵营专属声望加成光环似乎仅对击杀生效)
    if (source == REPUTATION_SOURCE_KILL)
        repMod += GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_FACTION_REPUTATION_GAIN, faction);

    percent += rep > 0 ? repMod : -repMod;

    float minRate;
    switch (source)
    {
        case REPUTATION_SOURCE_KILL:
            minRate = sWorld.getConfig(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_KILL);
            break;
        case REPUTATION_SOURCE_QUEST:
            minRate = sWorld.getConfig(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_QUEST);
            break;
        default:
            minRate = 1.0f;
            break;
    }

    uint32 currentLevel = GetLevel();

    // Level zero seems to be treated as always equal to players current level in IsTrivialLevelDifference therefore I have skipped level difference penalty computations for that value (等级 0 视为与玩家同级，跳过灰名等级衰减)
    if (creatureOrQuestLevel > 0)
    {
        if (source == REPUTATION_SOURCE_KILL)
        {
            if (MaNGOS::XP::IsTrivialLevelDifference(currentLevel, creatureOrQuestLevel))
            {
                const uint32 greenRange = MaNGOS::XP::GetQuestGreenRange(currentLevel);
                percent *= std::max(minRate, (1.0f - (0.2f * (currentLevel - greenRange - creatureOrQuestLevel))));
            }
        }
        else if (source == REPUTATION_SOURCE_QUEST)
        {
            // Pre-3.0.8: Declines with 20% for each level if 6 levels or more below the player down to a minimum (default: 20%) (3.0.8 前规则：低于玩家 6 级及以上时每级衰减 20%，保底 20%)
            const uint32 treshold = (creatureOrQuestLevel + 5);

            if (currentLevel > treshold)
                percent *= std::max(minRate, (1.0f - (0.2f * (currentLevel - treshold))));
        }
    }

    if (percent <= 0.0f)
        return 0;

    // Multiply result with the faction specific rate (乘以阵营专属声望倍率)
    if (const RepRewardRate* repData = sObjectMgr.GetRepRewardRate(faction))
    {
        float repRate = 0.0f;
        switch (source)
        {
            case REPUTATION_SOURCE_KILL:
                repRate = repData->creature_rate;
                break;
            case REPUTATION_SOURCE_QUEST:
                repRate = repData->quest_rate;
                break;
            case REPUTATION_SOURCE_SPELL:
                repRate = repData->spell_rate;
                break;
        }

        // for custom, a rate of 0.0 will totally disable reputation gain for this faction/type (若自定义倍率为 0.0 则完全禁用该类型声望获取)
        if (repRate <= 0.0f)
            return 0;

        percent *= repRate;
    }

    int32 result = int32(sWorld.getConfig(CONFIG_FLOAT_RATE_REPUTATION_GAIN) * rep * percent / 100.0f);

    if (source == REPUTATION_SOURCE_QUEST && result && faction)
    {
        if (FactionEntry const* factionEntry = sFactionStore.LookupEntry<FactionEntry>(faction))
        {
            int32 current = GetReputationMgr().GetReputation(factionEntry);

            if (current < maxRep)
            {
                int32 diff = maxRep - current;

                if (result > diff)
                    result = diff;
            }
            else
                result = 0;
        }
    }

    return result;
}

// Calculates how many reputation points player gains in victim's enemy factions (计算击杀生物时在关联阵营获得的声望)
void Player::RewardReputation(Creature* victim, float rate)
{
    if (victim->IsNoReputation())
        return;

    // used current difficulty creature entry instead normal version (GetEntry()) (使用当前副本难度对应的生物条目)
    ReputationOnKillEntry const* Rep = sObjectMgr.GetReputationOnKillEntry(victim->GetCreatureInfo()->Entry);

    if (!Rep)
        return;

    if (Rep->repfaction1 && (!Rep->team_dependent || GetTeam() == ALLIANCE))
    {
        int32 donerep1 = CalculateReputationGain(REPUTATION_SOURCE_KILL, Rep->repvalue1, 0, Rep->repfaction1, victim->GetLevel());
        donerep1 = int32(donerep1 * rate);
        FactionEntry const* factionEntry1 = sFactionStore.LookupEntry<FactionEntry>(Rep->repfaction1);
        uint32 current_reputation_rank1 = GetReputationMgr().GetRank(factionEntry1);
        if (factionEntry1 && current_reputation_rank1 <= Rep->reputation_max_cap1)
            GetReputationMgr().ModifyReputation(factionEntry1, donerep1);

        // Wiki: Team factions value divided by 2 (主城阵营溢出声望按 1/2 折算)
        if (factionEntry1 && Rep->is_teamaward1)
        {
            FactionEntry const* team1_factionEntry = sFactionStore.LookupEntry<FactionEntry>(factionEntry1->team);
            if (team1_factionEntry)
                GetReputationMgr().ModifyReputation(team1_factionEntry, donerep1 / 2);
        }
    }

    if (Rep->repfaction2 && (!Rep->team_dependent || GetTeam() == HORDE))
    {
        int32 donerep2 = CalculateReputationGain(REPUTATION_SOURCE_KILL, Rep->repvalue2, 0, Rep->repfaction2, victim->GetLevel());
        donerep2 = int32(donerep2 * rate);
        FactionEntry const* factionEntry2 = sFactionStore.LookupEntry<FactionEntry>(Rep->repfaction2);
        uint32 current_reputation_rank2 = GetReputationMgr().GetRank(factionEntry2);
        if (factionEntry2 && current_reputation_rank2 <= Rep->reputation_max_cap2)
            GetReputationMgr().ModifyReputation(factionEntry2, donerep2);

        // Wiki: Team factions value divided by 2 (同阵营联动声望按 1/2 折算)
        if (factionEntry2 && Rep->is_teamaward2)
        {
            FactionEntry const* team2_factionEntry = sFactionStore.LookupEntry<FactionEntry>(factionEntry2->team);
            if (team2_factionEntry)
                GetReputationMgr().ModifyReputation(team2_factionEntry, donerep2 / 2);
        }
    }
}

// Calculate how many reputation points player gain with the quest (计算完成任务获得的声望奖励/惩罚)
void Player::RewardReputation(Quest const* pQuest)
{
    // quest reputation reward/loss (处理任务固定及可变声望增减)
    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
    {
        if (!pQuest->RewRepFaction[i])
            continue;

        if (pQuest->RewRepValue[i])
        {
            int32 rep = CalculateReputationGain(REPUTATION_SOURCE_QUEST, pQuest->RewRepValue[i], pQuest->RewMaxRepValue[i], pQuest->RewRepFaction[i], GetQuestLevelForPlayer(pQuest));

            bool noSpillover = (pQuest->GetRewFactionFlags() & (1 << i)) != 0;
            if (FactionEntry const* factionEntry = sFactionStore.LookupEntry<FactionEntry>(pQuest->RewRepFaction[i]))
                GetReputationMgr().ModifyReputation(factionEntry, rep, noSpillover);
        }
    }
}

void Player::UpdateArenaFields(void)
{
    /* arena calcs go here (竞技场字段计算预留) */
}

void Player::UpdateHonorFields()
{
    /// called when rewarding honor and at each save (在奖励荣誉和每次保存角色时调用，跨午夜刷新昨日/今日统计)
    time_t now = time(nullptr);
    time_t today = (time(nullptr) / DAY) * DAY;

    if (m_lastHonorUpdateTime < today)
    {
        time_t yesterday = today - DAY;

        uint16 kills_today = PAIR32_LOPART(GetUInt32Value(PLAYER_FIELD_KILLS));

        // update yesterday's contribution (将昨日数据结转为前一日累计并重置今日数据)
        if (m_lastHonorUpdateTime >= yesterday)
        {
            SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));

            // this is the first update today, reset today's contribution (今日首次更新，将昨日数据移入昨日字段并清空今日计数)
            SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, 0);
            SetUInt32Value(PLAYER_FIELD_KILLS, MAKE_PAIR32(0, kills_today));
        }
        else
        {
            // no honor/kills yesterday or today, reset (前天及昨天均无荣誉或击杀，全部清零)
            SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, 0);
            SetUInt32Value(PLAYER_FIELD_KILLS, 0);
        }
    }

    m_lastHonorUpdateTime = now;
}

/// Calculate the amount of honor gained based on the victim (根据击杀目标、分摊人数或指定荣誉值计算并奖励荣誉)
/// and the size of the group for which the honor is divided (支持按队伍人数平摊荣誉)
/// An exact honor value can also be given (overriding the calcs) (也可直接传入精确荣誉值覆盖公式计算)
/// 击杀敌对玩家或阵营领袖后计算并奖励荣誉点数与击杀统计
bool Player::RewardHonor(Unit* uVictim, uint32 groupsize, float honor)
{
    // do not reward honor in arenas, but enable onkill spellproc (竞技场内不直接奖励荣誉点，但仍触发击杀法术特效)
    if (InArena())
    {
        if (!uVictim || uVictim == this || uVictim->GetTypeId() != TYPEID_PLAYER)
            return false;

        if (GetBGTeam() == ((Player*)uVictim)->GetBGTeam())
            return false;

        return true;
    }

    // 'Inactive' this aura prevents the player from gaining honor points and battleground tokens (战场“暂离/不活跃”光环阻止玩家获得荣誉与战场奖章)
    if (GetDummyAura(SPELL_AURA_PLAYER_INACTIVE))
        return false;

    ObjectGuid victim_guid;
    uint32 victim_rank = 0;

    // need call before fields update to have chance move yesterday data to appropriate fields before today data change. (需在修改今日字段前调用以先完成跨日数据结转)
    UpdateHonorFields();

    if (honor <= 0)
    {
        if (!uVictim || uVictim == this || uVictim->HasAuraType(SPELL_AURA_NO_PVP_CREDIT))
            return false;

        victim_guid = uVictim->GetObjectGuid();

        if (uVictim->GetTypeId() == TYPEID_PLAYER)
        {
            Player* pVictim = (Player*)uVictim;

            if (GetTeam() == pVictim->GetTeam() && !sWorld.IsFFAPvPRealm())
                return false;

            float f = 1;                                    // need for total kills (?? need more info) (计入总击杀统计)
            uint32 k_level = GetLevel();
            uint32 v_level = pVictim->GetLevel();

            victim_rank = pVictim->GetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_LIFETIME_MAX_PVP_RANK); 

            uint32 k_grey = MaNGOS::XP::GetGrayLevel(k_level);

            if (v_level <= k_grey)
                return false;

            float diff_level = (k_level == k_grey) ? 1 : ((float(v_level) - float(k_grey)) / (float(k_level) - float(k_grey)));

            int32 v_rank = 1;                               // need more info (击杀等级/职业系数占位)

            honor = ((f * diff_level * (190 + v_rank * 10)) / 6);
            honor *= float(k_level) / 70.0f;                // factor of dependence on levels of the killer (根据击杀者等级计算基础荣誉系数)

            // count the number of playerkills in one day (累计今日击杀人数)
            ApplyModUInt32Value(PLAYER_FIELD_KILLS, 1, true);
            // and those in a lifetime (累计生涯总击杀人数)
            ApplyModUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, 1, true);
        }
        else
        {
            Creature* cVictim = (Creature*)uVictim;

            if (!cVictim->IsRacialLeader())
                return false;

            honor = 100;                                    // ??? need more info (未知字段：目标阵营/类型)
            victim_rank = 19;                               // HK: Leader (荣誉击杀：阵营领袖)
        }
    }

    if (uVictim != nullptr)
    {
        honor *= sWorld.getConfig(CONFIG_FLOAT_RATE_HONOR);

        if (groupsize > 1)
            honor /= groupsize;

        honor *= (((float)urand(8, 12)) / 10);              // approx honor: 80% - 120% of real honor (同日重复击杀递减系数：100% 递减至 10%)
    }

    // honor - for show honor points in log (日志显示的荣誉值)
    // victim_guid - for show victim name in log (受害者 GUID)
    // victim_rank 0       HK: <lowest alliance\horde rank> (0 级显示最低军衔)
    // victim_rank [1..4]  HK: <dishonored rank> (1-4 为非荣誉阶)
    // victim_rank [5..19] HK: <alliance\horde rank> (5-19 为联盟/部落军衔)
    WorldPacket data(SMSG_PVP_CREDIT, 4 + 8 + 4);
    data << uint32(honor);
    data << ObjectGuid(victim_guid);
    data << uint32(victim_rank);
    GetSession()->SendPacket(data);

    // add honor points (增加玩家荣誉点数)
    ModifyHonorPoints(int32(honor));

    ApplyModUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, uint32(honor), true);
    return true;
}

void Player::SetHonorPoints(uint32 value)
{
    if (value > sWorld.getConfig(CONFIG_UINT32_MAX_HONOR_POINTS))
        value = sWorld.getConfig(CONFIG_UINT32_MAX_HONOR_POINTS);

    SetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY, value);
}

void Player::SetArenaPoints(uint32 value)
{
    if (value > sWorld.getConfig(CONFIG_UINT32_MAX_ARENA_POINTS))
        value = sWorld.getConfig(CONFIG_UINT32_MAX_ARENA_POINTS);

    SetUInt32Value(PLAYER_FIELD_ARENA_CURRENCY, value);
}

void Player::ModifyHonorPoints(int32 value)
{
    int32 newValue = (int32)GetHonorPoints() + value;

    if (newValue < 0)
        newValue = 0;

    SetHonorPoints(newValue);
}

void Player::ModifyArenaPoints(int32 value)
{
    int32 newValue = (int32)GetArenaPoints() + value;

    if (newValue < 0)
        newValue = 0;

    SetArenaPoints(newValue);
}

uint32 Player::GetGuildIdFromDB(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();

    auto queryResult = CharacterDatabase.PQuery("SELECT guildid FROM guild_member WHERE guid='%u'", lowguid);
    if (!queryResult)
        return 0;

    uint32 id = queryResult->Fetch()[0].GetUInt32();
    return id;
}

uint32 Player::GetRankFromDB(ObjectGuid guid)
{
    auto queryResult = CharacterDatabase.PQuery("SELECT `rank` FROM guild_member WHERE guid='%u'", guid.GetCounter());
    if (queryResult)
    {
        uint32 v = queryResult->Fetch()[0].GetUInt32();
        return v;
    }
    return 0;
}

uint32 Player::GetArenaTeamIdFromDB(ObjectGuid guid, ArenaType type)
{
    auto queryResult = CharacterDatabase.PQuery("SELECT arena_team_member.arenateamid FROM arena_team_member JOIN arena_team ON arena_team_member.arenateamid = arena_team.arenateamid WHERE guid='%u' AND type='%u' LIMIT 1", guid.GetCounter(), type);
    if (!queryResult)
        return 0;

    uint32 id = (*queryResult)[0].GetUInt32();
    return id;
}

uint32 Player::GetZoneIdFromDB(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();
    auto queryResult = CharacterDatabase.PQuery("SELECT zone FROM characters WHERE guid='%u'", lowguid);
    if (!queryResult)
        return 0;
    Field* fields = queryResult->Fetch();
    uint32 zone = fields[0].GetUInt32();

    if (!zone)
    {
        // stored zone is zero, use generic and slow zone detection (数据库缓存区域为 0 时通过坐标反查区域 ID)
        queryResult = CharacterDatabase.PQuery("SELECT map,position_x,position_y,position_z FROM characters WHERE guid='%u'", lowguid);
        if (!queryResult)
            return 0;
        fields = queryResult->Fetch();
        uint32 map = fields[0].GetUInt32();
        float posx = fields[1].GetFloat();
        float posy = fields[2].GetFloat();
        float posz = fields[3].GetFloat();

        zone = sTerrainMgr.GetZoneId(map, posx, posy, posz);

        if (zone > 0)
            CharacterDatabase.PExecute("UPDATE characters SET zone='%u' WHERE guid='%u'", zone, lowguid);
    }

    return zone;
}

uint32 Player::GetLevelFromDB(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();

    auto queryResult = CharacterDatabase.PQuery("SELECT level FROM characters WHERE guid='%u'", lowguid);
    if (!queryResult)
        return 0;

    Field* fields = queryResult->Fetch();
    uint32 level = fields[0].GetUInt32();

    return level;
}

void Player::UpdateArea(uint32 newArea)
{
    // group update (标记队伍区域更新)
    if (m_areaUpdateId != newArea && GetGroup())
        SetGroupUpdateFlag(GROUP_UPDATE_FLAG_ZONE);

    m_areaUpdateId    = newArea;

    AreaTableEntry const* area = GetAreaEntryByAreaID(newArea);

    // FFA_PVP flags are area and not zone id dependent (自由混战 FFA PvP 标志取决于子区域而非主区域，)
    // so apply them accordingly (据此按子区域刷新标志)
    if (area && (area->flags & AREA_FLAG_ARENA))
    {
        if (!IsGameMaster())
            SetPvPFreeForAll(true);
    }
    else
    {
        // remove ffa flag only if not ffapvp realm (仅非自由混战服务器才移除 FFA 标志，)
        // removal in sanctuaries and capitals is handled in zone update (庇护所与主城的移除在 UpdateZone 中处理)
        if (IsPvPFreeForAll() && !sWorld.IsFFAPvPRealm())
            SetPvPFreeForAll(false);
    }

    UpdateAreaDependentAuras();
}

bool Player::CanUseCapturePoint() const
{
    return IsAlive() &&                                     // living (必须存活)
           !HasStealthAura() &&                             // not stealthed (不可处于潜行状态)
           !HasInvisibilityAura() &&                        // visible (必须对世界可见，非 GM 隐身)
           (IsPvP() || sWorld.IsPvPRealm()) &&
           !HasMovementFlag(MOVEFLAG_FLYING) &&
           !IsTaxiFlying() &&
           !IsGameMaster();
}

/// 玩家切换主区域或子区域时刷新 PvP 状态、休息区、频道与区域光环
void Player::UpdateZone(uint32 newZone, uint32 newArea, bool force)
{
    AreaTableEntry const* zone = GetAreaEntryByAreaID(newZone);
    if (!zone)
        return;

    bool updateZone = m_zoneUpdateId != newZone || force;
    if (updateZone)
    {
        // handle outdoor pvp zones (通知野外 PvP 管理器处理区域切换)
        sOutdoorPvPMgr.HandlePlayerLeaveZone(this, m_zoneUpdateId);
        sWorldState.HandlePlayerLeaveZone(this, m_zoneUpdateId);
        sOutdoorPvPMgr.HandlePlayerEnterZone(this, newZone);
        sWorldState.HandlePlayerEnterZone(this, newZone);

        if (sWorld.getConfig(CONFIG_BOOL_WEATHER))
        {
            Weather* wth = GetMap()->GetWeatherSystem()->FindOrCreateWeather(newZone);
            wth->SendWeatherUpdateToPlayer(this);
        }
    }

    bool updateArea = m_areaUpdateId != newArea || force;
    if (updateArea)
    {
        SendInitWorldStates(newZone, newArea); // only if really enters to new zone, not just area change, works strange... (仅当真正进入新主区域时更新探索发现状态)

        sWorldState.HandlePlayerLeaveArea(this, m_areaUpdateId);
        sWorldState.HandlePlayerEnterArea(this, newArea);
    }

    if (updateZone || updateArea)
        GetMap()->SendZoneDynamicInfo(this, updateZone, updateArea);

    m_zoneUpdateId    = newZone;
    m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;

    // zone changed, so area changed as well, update it (主区域变更必然伴随子区域变更，同步更新)
    UpdateArea(newArea);

    // in PvP, any not controlled zone (except zone->team == 6, default case) (PvP 服中任何中立或敌对区域均自动开启 PvP)
    // in PvE, only opposition team capital (PvE 服仅进入敌对阵营主城才强制开启 PvP)
    switch (zone->team)
    {
        case AREATEAM_ALLY:
            pvpInfo.inPvPEnforcedArea = GetTeam() != ALLIANCE && (sWorld.IsPvPRealm() || zone->flags & AREA_FLAG_CAPITAL);
            break;
        case AREATEAM_HORDE:
            pvpInfo.inPvPEnforcedArea = GetTeam() != HORDE && (sWorld.IsPvPRealm() || zone->flags & AREA_FLAG_CAPITAL);
            break;
        case AREATEAM_NONE:
            // overwrite for battlegrounds, maybe batter some zone flags but current known not 100% fit to this (战场地图强制开启 PvP 标志)
            pvpInfo.inPvPEnforcedArea = sWorld.IsPvPRealm() || InBattleGround();
            break;
        default:                                            // 6 in fact (实际上属于安全/中立类)
            pvpInfo.inPvPEnforcedArea = false;
            break;
    }

    if (pvpInfo.inPvPEnforcedArea)                              // in hostile area (处于敌对区域，激活 PvP 标志)
        UpdatePvP(true);

    if (zone->flags & AREA_FLAG_SANCTUARY)                  // in sanctuary (处于庇护所安全区，清除 PvP 并冻结计时)
    {
        SetPvPSanctuary(true);

        UpdatePvP(false, true);

        if (sWorld.IsFFAPvPRealm())
            SetPvPFreeForAll(false);
    }
    else
    {
        SetPvPSanctuary(false);
    }

    if (zone->flags & AREA_FLAG_CAPITAL)                    // in capital city (处于己方主城，开启主城休息双倍状态)
        SetRestType(REST_TYPE_IN_CITY);
    else if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) && GetRestType() != REST_TYPE_IN_TAVERN)
        // resting and not in tavern (leave city then); tavern leave handled in CheckAreaExploreAndOutdoor (离开主城时清除城市休息状态；离开旅店由 CheckAreaExploreAndOutdoor 处理)
        SetRestType(REST_TYPE_NO);

    // remove items with area/map limitations (delete only for alive player to allow back in ghost mode) (移除受地图/区域限制的物品，仅对存活玩家执行以免灵魂跑尸被删)
    // if player resurrected at teleport this will be applied in resurrect code (若传送后复活则会在复活流程中再次检查)
    if (IsAlive())
        DestroyZoneLimitedItem(true, newZone);

    // check some item equip limitations (in result lost CanTitanGrip at talent reset, for example) (检查装备区域或天赋限制，如遗忘泰坦之握等)
    AutoUnequipOffhandIfNeed();

    // recent client version not send leave/join channel packets for built-in local channels (更新本地内置聊天频道)
    UpdateLocalChannels(newZone);

    UpdateZoneDependentAuras();
    UpdateZoneDependentPets();
}

// If players are too far way of duel flag... then player loose the duel (若玩家远离决斗旗帜超时则判逃离失败)
void Player::CheckDuelDistance(time_t currTime)
{
    if (!duel)
        return;

    GameObject* obj = GetMap()->GetGameObject(GetGuidValue(PLAYER_DUEL_ARBITER));
    if (!obj)
    {
        // player not at duel start map (玩家已离开决斗发起所在地图)
        DuelComplete(DUEL_FLED);
        return;
    }

    if (duel->outOfBound == 0)
    {
        if (!IsWithinDistInMap(obj, 50))
        {
            duel->outOfBound = currTime;

            WorldPacket data(SMSG_DUEL_OUTOFBOUNDS, 0);
            GetSession()->SendPacket(data);
        }
    }
    else
    {
        if (IsWithinDistInMap(obj, 40))
        {
            duel->outOfBound = 0;

            WorldPacket data(SMSG_DUEL_INBOUNDS, 0);
            GetSession()->SendPacket(data);
        }
        else if (currTime >= (duel->outOfBound + 10))
        {
            DuelComplete(DUEL_FLED);
        }
    }
}

/// 结束决斗（胜出、逃离或中断）并清理决斗旗帜与双方状态
void Player::DuelComplete(DuelCompleteType type)
{
    // duel not requested (当前未处于决斗状态)
    if (!duel)
        return;

    WorldPacket data(SMSG_DUEL_COMPLETE, (1));
    data << (uint8)((type != DUEL_INTERRUPTED) ? 1 : 0);
    GetSession()->SendPacket(data);
    duel->opponent->GetSession()->SendPacket(data);

    if (type != DUEL_INTERRUPTED)
    {
        data.Initialize(SMSG_DUEL_WINNER, (1 + 20));        // we guess size (发送决斗胜负结果)
        data << (uint8)((type == DUEL_WON) ? 0 : 1);        // 0 = just won; 1 = fled (0 = 正常胜出，1 = 逃离)
        data << duel->opponent->GetName();
        data << GetName();
        SendMessageToSet(data, true);
    }

    // Remove Duel Flag object (从地图中移除决斗旗帜对象)
    if (GameObject* obj = GetMap()->GetGameObject(GetGuidValue(PLAYER_DUEL_ARBITER)))
        duel->initiator->RemoveGameObject(obj, true);

    /* remove auras (移除决斗相关减益与光环) */
    std::vector<uint32> auras2remove;
    SpellAuraHolderMap const& vAuras = duel->opponent->GetSpellAuraHolderMap();
    for (const auto& vAura : vAuras)
    {
        SpellAuraHolder const* aura = vAura.second;
        if (!aura->IsPositive() && aura->GetCasterGuid() == GetObjectGuid() && aura->GetAuraApplyTime() >= duel->startTime)
            auras2remove.push_back(aura->GetId());
    }

    for (unsigned int i : auras2remove)
        duel->opponent->RemoveAurasDueToSpell(i);

    auras2remove.clear();
    SpellAuraHolderMap const& auras = GetSpellAuraHolderMap();
    for (const auto& i : auras)
    {
        SpellAuraHolder const* aura = i.second;
        if (!aura->IsPositive() && aura->GetCasterGuid() == duel->opponent->GetObjectGuid() && aura->GetAuraApplyTime() >= duel->startTime)
            auras2remove.push_back(aura->GetId());
    }
    for (unsigned int i : auras2remove)
        RemoveAurasDueToSpell(i);

    // cleanup combo points (清空决斗产生的连击点数)
    if (GetComboTargetGuid() == duel->opponent->GetObjectGuid())
        ClearComboPoints();
    else if (GetComboTargetGuid() == duel->opponent->GetPetGuid())
        ClearComboPoints();

    if (duel->opponent->GetComboTargetGuid() == GetObjectGuid())
        duel->opponent->ClearComboPoints();
    else if (duel->opponent->GetComboTargetGuid() == GetPetGuid())
        duel->opponent->ClearComboPoints();

    // cleanups (重置双方决斗指针并释放 DuelInfo)
    SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid());
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);
    duel->opponent->SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid());
    duel->opponent->SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    delete duel->opponent->duel;
    duel->opponent->duel = nullptr;
    delete duel;
    duel = nullptr;
}

//---------------------------------------------------------//

/// 应用或移除指定槽位装备的属性修正与多彩宝石激活状态
void Player::_ApplyItemMods(Item* item, uint8 slot, bool apply)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !item)
        return;

    // not apply/remove mods for broken item (耐久度为 0 的破损装备不应用属性加成)
    if (item->IsBroken())
        return;

    ItemPrototype const* proto = item->GetProto();

    if (!proto)
        return;

    DETAIL_LOG("applying mods for item %u ", item->GetGUIDLow());

    uint32 attacktype = Player::GetAttackBySlot(slot);
    if (attacktype < MAX_ATTACK)
        _ApplyWeaponDependentAuraMods(item, WeaponAttackType(attacktype), apply);

    _ApplyItemBonuses(proto, slot, apply);

    if (slot == EQUIPMENT_SLOT_RANGED)
        _ApplyAmmoBonuses();

    ApplyItemEquipSpell(item, apply);
    ApplyEnchantment(item, apply);

    if (proto->Socket[0].Color)                             // only (un)equipping of items with sockets can influence metagems, so no need to waste time with normal items (仅带插槽的装备穿脱才会影响多彩宝石激活条件)
        CorrectMetaGemEnchants(slot, apply);

    DEBUG_LOG("_ApplyItemMods complete.");
}

void Player::_ApplyItemBonuses(ItemPrototype const* proto, uint8 slot, bool apply)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !proto)
        return;

    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        float val = float(proto->ItemStat[i].ItemStatValue);

        if (val == 0)
            continue;

        switch (proto->ItemStat[i].ItemStatType)
        {
            case ITEM_MOD_MANA:
                HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_HEALTH:                           // modify HP (修改最大生命值)
                HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_AGILITY:                          // modify agility (修改敏捷)
                HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
                break;
            case ITEM_MOD_STRENGTH:                         // modify strength (修改力量)
                HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
                break;
            case ITEM_MOD_INTELLECT:                        // modify intellect (修改智力)
                HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
                break;
            case ITEM_MOD_SPIRIT:                           // modify spirit (修改精神)
                HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
                break;
            case ITEM_MOD_STAMINA:                          // modify stamina (修改耐力)
                HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
                ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
                break;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
                ApplyRatingMod(CR_DEFENSE_SKILL, int32(val), apply);
                break;
            case ITEM_MOD_DODGE_RATING:
                ApplyRatingMod(CR_DODGE, int32(val), apply);
                break;
            case ITEM_MOD_PARRY_RATING:
                ApplyRatingMod(CR_PARRY, int32(val), apply);
                break;
            case ITEM_MOD_BLOCK_RATING:
                ApplyRatingMod(CR_BLOCK, int32(val), apply);
                break;
            case ITEM_MOD_HIT_MELEE_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HIT_RANGED_RATING:
                ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HIT_SPELL_RATING:
                ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_MELEE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_RANGED_RATING:
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_SPELL_RATING:
                ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_RESILIENCE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_EXPERTISE_RATING:
                ApplyRatingMod(CR_EXPERTISE, int32(val), apply);
                break;
        }
    }

    uint32 armor = proto->Armor;
    // Add armor bonus from ArmorDamageModifier if > 0 (额外护甲加成说明)
    /*if (proto->ArmorDamageModifier > 0) This is wrong, the member is used by client to determine bonus armor. proto->Armor already has full armor
        armor += uint32(proto->ArmorDamageModifier); (注：proto->Armor 已包含完整护甲，无需再叠加 ArmorDamageModifier)*/

    if (armor)
        HandleStatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(armor), apply);

    if (proto->Block)
        HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(proto->Block), apply);

    if (proto->HolyRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(proto->HolyRes), apply);

    if (proto->FireRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(proto->FireRes), apply);

    if (proto->NatureRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(proto->NatureRes), apply);

    if (proto->FrostRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(proto->FrostRes), apply);

    if (proto->ShadowRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(proto->ShadowRes), apply);

    if (proto->ArcaneRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(proto->ArcaneRes), apply);

    if (proto->IsWeapon())
    {
        WeaponAttackType attType = BASE_ATTACK;

        if (slot == EQUIPMENT_SLOT_RANGED && proto->IsRangedWeapon())
            attType = RANGED_ATTACK;
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
            attType = OFF_ATTACK;

        bool hasDamage = false;
        m_weaponDamageInfo.weapon[attType].lines = 0;

        for (int i = 0; i < MAX_ITEM_PROTO_DAMAGES; i++)
        {
            if (proto->Damage[i].DamageMax == 0)
                break;

            hasDamage = true;

            float minDamage = 0.0f;
            float maxDamage = 0.0f;
            SpellSchools school = SPELL_SCHOOL_NORMAL;

            if (apply)
            {
                minDamage = proto->Damage[i].DamageMin;
                maxDamage = proto->Damage[i].DamageMax;
                school = SpellSchools(proto->Damage[i].DamageType);

                m_weaponDamageInfo.weapon[attType].lines++;
            }

            SetBaseWeaponDamage(attType, MINDAMAGE, minDamage, i);
            SetBaseWeaponDamage(attType, MAXDAMAGE, maxDamage, i);
            SetWeaponDamageSchool(attType, school, i);
        }

        if (!m_weaponDamageInfo.weapon[attType].lines)
            m_weaponDamageInfo.weapon[attType].lines = 1;

        if (!CanUseEquippedWeapon(attType))
            return;

        if (proto->Delay)
        {
            if (slot == EQUIPMENT_SLOT_RANGED)
                SetAttackTime(RANGED_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
            else if (slot == EQUIPMENT_SLOT_MAINHAND)
                SetAttackTime(BASE_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
            else if (slot == EQUIPMENT_SLOT_OFFHAND)
                SetAttackTime(OFF_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
        }

        if (CanModifyStats() && proto->Delay)
            UpdateDamagePhysical(attType);
    }
}

void Player::_ApplyWeaponDependentAuraMods(Item* item, WeaponAttackType attackType, bool apply)
{
    AuraList const& auraCritList = GetAurasByType(SPELL_AURA_MOD_CRIT_PERCENT);
    for (auto itr : auraCritList)
        _ApplyWeaponDependentAuraCritMod(item, attackType, itr, apply);

    AuraList const& auraDamageFlatList = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (auto itr : auraDamageFlatList)
        _ApplyWeaponDependentAuraDamageMod(item, attackType, itr, apply);

    AuraList const& auraDamagePCTList = GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for (auto itr : auraDamagePCTList)
        _ApplyWeaponDependentAuraDamageMod(item, attackType, itr, apply);
}

void Player::_ApplyWeaponDependentAuraCritMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply)
{
    // generic not weapon specific case processes in aura code (非限定武器的通用暴击修正由光环代码处理)
    if (aura->GetSpellProto()->EquippedItemClass == -1)
        return;

    BaseModGroup mod = BASEMOD_END;
    switch (attackType)
    {
        case BASE_ATTACK:   mod = CRIT_PERCENTAGE;        break;
        case OFF_ATTACK:    mod = OFFHAND_CRIT_PERCENTAGE; break;
        case RANGED_ATTACK: mod = RANGED_CRIT_PERCENTAGE; break;
        default: return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellProto()))
    {
        HandleBaseModValue(mod, FLAT_MOD, float(aura->GetModifier()->m_amount), apply);
    }
}

void Player::_ApplyWeaponDependentAuraDamageMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply)
{
    // ignore spell mods for not wands (非法杖类武器不应用此类光环)
    Modifier const* modifier = aura->GetModifier();
    if ((modifier->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) == 0 && (getClassMask() & CLASSMASK_WAND_USERS) == 0)
        return;

    // generic not weapon specific case processes in aura code (非限定武器的通用伤害修正由光环代码处理)
    if (aura->GetSpellProto()->EquippedItemClass == -1)
        return;

    UnitMods unitMod = UNIT_MOD_END;
    switch (attackType)
    {
        case BASE_ATTACK:   unitMod = UNIT_MOD_DAMAGE_MAINHAND; break;
        case OFF_ATTACK:    unitMod = UNIT_MOD_DAMAGE_OFFHAND;  break;
        case RANGED_ATTACK: unitMod = UNIT_MOD_DAMAGE_RANGED;   break;
        default: return;
    }

    UnitModifierType unitModType = TOTAL_VALUE;
    switch (modifier->m_auraname)
    {
        case SPELL_AURA_MOD_DAMAGE_DONE:         unitModType = TOTAL_VALUE; break;
        case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE: unitModType = TOTAL_PCT;   break;
        default: return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellProto()))
    {
        HandleStatModifier(unitMod, unitModType, float(modifier->m_amount), apply);
    }
}

void Player::ApplyItemEquipSpell(Item* item, bool apply, bool form_change)
{
    if (!item)
        return;

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
        return;

    for (const auto& spellData : proto->Spells)
    {
        // no spell (无绑定法术)
        if (!spellData.SpellId)
            continue;

        if (apply)
        {
            // apply only at-equip spells (仅处理装备时触发型法术)
            if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
                continue;
        }
        else
        {
            // at un-apply remove all spells (not only at-apply, so any at-use active affects from item and etc) (卸下时移除物品所有关联光环，含使用型法术效果)
            // except on form change and with at-use with negative charges, so allow consuming item spells (including with extra flag that prevent consume really) (变形切换及消耗品负充能情况除外)
            // applied to player after item remove from equip slot (卸下装备后清理残余光环)
            if (spellData.SpellTrigger == ITEM_SPELLTRIGGER_ON_USE && (form_change || spellData.SpellCharges < 0))
                continue;
        }

        // check if it is valid spell (校验法术模板有效性)
        SpellEntry const* spellproto = sSpellTemplate.LookupEntry<SpellEntry>(spellData.SpellId);
        if (!spellproto)
            continue;

        ApplyEquipSpell(spellproto, item, apply, form_change);
    }
}

void Player::ApplyEquipSpell(SpellEntry const* spellInfo, Item* item, bool apply, bool form_change)
{
    if (apply)
    {
        // Cannot be used in this stance/form (在当前变形形态下无法生效)
        if (GetErrorAtShapeshiftedCast(spellInfo, GetShapeshiftForm()) != SPELL_CAST_OK)
            return;

        if (form_change)                                    // check aura active state from other form (检查其他形态下是否已激活该光环)
        {
            bool found = false;
            for (int k = 0; k < MAX_EFFECT_INDEX; ++k)
            {
                SpellAuraHolderBounds spair = GetSpellAuraHolderBounds(spellInfo->Id);
                for (SpellAuraHolderMap::const_iterator iter = spair.first; iter != spair.second; ++iter)
                {
                    if (!item || iter->second->GetCastItemGuid() == item->GetObjectGuid())
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;
            }

            if (found)                                      // and skip re-cast already active aura at form change (变形切换时若光环已激活且兼容则跳过重复施放)
                return;
        }

        DEBUG_LOG("WORLD: cast %s Equip spellId - %i", (item ? "item" : "itemset"), spellInfo->Id);

        CastSpell(nullptr, spellInfo, TRIGGERED_OLD_TRIGGERED, item);
    }
    else
    {
        if (form_change)                                    // check aura compatibility (检查光环移除与形态兼容性)
        {
            // Cannot be used in this stance/form (当前形态不兼容该装备法术)
            if (GetErrorAtShapeshiftedCast(spellInfo, GetShapeshiftForm()) == SPELL_CAST_OK)
                return;                                     // and remove only not compatible at form change (变形时仅移除不兼容的装备法术光环)
        }

        if (item)
            RemoveAurasDueToItemSpell(item, spellInfo->Id); // un-apply all spells , not only at-equipped (完全卸下装备时移除所有相关光环)
        else
            RemoveAurasDueToSpell(spellInfo->Id);           // un-apply spell (item set case) (非特定物品触发的套装法术直接按法术 ID 移除)
    }
}

void Player::UpdateEquipSpellsAtFormChange()
{
    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i] && !m_items[i]->IsBroken())
        {
            ApplyItemEquipSpell(m_items[i], false, true);   // remove spells that not fit to form (移除不适应当前变形形态的装备法术)
            ApplyItemEquipSpell(m_items[i], true, true);    // add spells that fit form but not active (补加适应当前形态但尚未激活的装备法术)
        }
    }

    // item set bonuses not dependent from item broken state (套装效果不随单件耐久归零失效)
    for (auto& setData : m_itemSetEffects)
    {
        for (auto spellInfo : setData.second.spells)
        {
            if (!spellInfo)
                continue;

            ApplyEquipSpell(spellInfo, nullptr, false, true);  // remove spells that not fit to form (移除不适应当前形态的套装法术)
            ApplyEquipSpell(spellInfo, nullptr, true, true);   // add spells that fit form but not active (补加适应当前形态的套装法术)
        }
    }
}

/**
 * (un-)Apply item spells triggered at adding item to inventory ITEM_SPELLTRIGGER_ON_STORE
 * 应用或移除放入背包时触发的物品法术 (ITEM_SPELLTRIGGER_ON_STORE)
 *
 * @param item  added/removed item to/from inventory (加入或移出背包的物品)
 * @param apply (un-)apply spell affects. (true 应用 / false 移除)
 *
 * Note: item moved from slot to slot in 2 steps RemoveItem and StoreItem/EquipItem
 * In result function not called in RemoveItem for prevent unexpected re-apply auras from related spells
 * with duration reset and etc. Instead unapply done in StoreItem/EquipItem and in specialized
 * functions for item final remove/destroy from inventory. If new RemoveItem calls added need be sure that
 * function will call after it in some way if need.
 * 注：物品在背包槽位间移动分为 RemoveItem 与 StoreItem/EquipItem 两步，故不在 RemoveItem 中直接移除以免重置持续时间，而是在目标槽位不合法或最终销毁时移除。
 */

void Player::ApplyItemOnStoreSpell(Item* item, bool apply)
{
    if (!item)
        return;

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
        return;

    for (const auto& spellData : proto->Spells)
    {
        // no spell (无绑定法术)
        if (!spellData.SpellId)
            continue;

        // apply/unapply only at-store spells (仅处理入包触发型法术)
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_STORE)
            continue;

        if (apply)
        {
            // can be attempt re-applied at move in inventory slots (在背包内移动时可能重复触发，已存在则跳过)
            if (!HasAura(spellData.SpellId))
                CastSpell(this, spellData.SpellId, TRIGGERED_OLD_TRIGGERED, item);
        }
        else
            RemoveAurasDueToItemSpell(item, spellData.SpellId);
    }
}

void Player::DestroyItemWithOnStoreSpell(Item* item, uint32 spellId)
{
    if (!item)
        return;

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
        return;

    for (const auto& spellData : proto->Spells)
    {
        if (spellData.SpellId != spellId)
            continue;

        // apply/unapply only at-store spells (仅处理入包触发型法术)
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_STORE)
            continue;

        DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        break;
    }
}

/// 触发物理/法术攻击时物品自带及武器附魔的战斗特效
void Player::CastItemCombatSpell(Unit* Target, WeaponAttackType attType, bool spellProc)
{
    Item* item = GetWeaponForAttack(attType, true, true);
    if (!item)
        return;

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
        return;

    if (!Target || Target == this)
        return;

    for (const auto& spellData : proto->Spells)
    {
        // no spell (无触发法术)
        if (!spellData.SpellId)
            continue;

        // wrong triggering type (非击中触发型)
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
            continue;

        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellData.SpellId);
        if (!spellInfo)
        {
            sLog.outError("WORLD: unknown Item spellid %i", spellData.SpellId);
            continue;
        }

        // check if spell is under CD. TODO : this may be fixed in better way using TRIGGERED_ITEM_TRIGGERED enum for castspell (检查法术冷却状态)
        if (HasGCD(spellInfo) || !IsSpellReady(*spellInfo, proto))
            return;

        // not allow proc extra attack spell at extra attack (额外攻击期间不再触发物品额外攻击特效)
        if (m_extraAttacks && IsSpellHaveEffect(spellInfo, SPELL_EFFECT_ADD_EXTRA_ATTACKS))
            return;

        float chance = (float)spellInfo->procChance;

        if (spellData.SpellPPMRate)
        {
            uint32 WeaponSpeed = proto->Delay;
            chance = GetPPMProcChance(WeaponSpeed, spellData.SpellPPMRate);
        }
        else if (chance > 100.0f)
        {
            chance = GetWeaponProcChance();
        }

        if (roll_chance_f(chance))
            CastSpell(Target, spellInfo->Id, TRIGGERED_OLD_TRIGGERED, item);
    }

    // item combat enchantments (触发武器临时/永久战斗附魔效果)
    for (int e_slot = 0; e_slot < MAX_ENCHANTMENT_SLOT; ++e_slot)
    {
        uint32 enchant_id = item->GetEnchantmentId(EnchantmentSlot(e_slot));
        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant) continue;
        for (int s = 0; s < 3; ++s)
        {
            uint32 proc_spell_id = pEnchant->spellid[s];

            if (pEnchant->type[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                continue;

            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(proc_spell_id);
            if (!spellInfo)
            {
                sLog.outError("Player::CastItemCombatSpell Enchant %i, cast unknown spell %i", pEnchant->ID, proc_spell_id);
                continue;
            }

            // not allow proc extra attack spell at extra attack (额外攻击期间不再触发附魔额外攻击)
            if (m_extraAttacks && IsSpellHaveEffect(spellInfo, SPELL_EFFECT_ADD_EXTRA_ATTACKS))
                continue;

            // some weapon enchantments have proc flags which need to be checked (部分武器附魔带有需校验的触发标志位)
            if (spellInfo->procFlags)
                if (!(spellInfo->procFlags & PROC_FLAG_DEAL_MELEE_ABILITY) && spellProc)
                    continue;

            // Use first rank to access spell item enchant procs (通过一阶法术 ID 查询武器附魔触发几率表)
            float ppmRate = sSpellMgr.GetItemEnchantProcChance(spellInfo->Id);

            float chance = ppmRate
                           ? GetPPMProcChance(proto->Delay, ppmRate)
                           : pEnchant->amount[s] != 0 ? float(pEnchant->amount[s]) : GetWeaponProcChance();

            ApplySpellMod(spellInfo->Id, SPELLMOD_CHANCE_OF_SUCCESS, chance);

            if (roll_chance_f(chance))
            {
                if (IsPositiveSpell(spellInfo->Id, this, Target))
                    CastSpell(this, spellInfo->Id, TRIGGERED_OLD_TRIGGERED, item, nullptr, ObjectGuid(), nullptr);
                else
                    CastSpell(Target, spellInfo->Id, TRIGGERED_OLD_TRIGGERED, item, nullptr, ObjectGuid(), nullptr);
            }
        }
    }
}

void Player::CastItemUseSpell(Item* item, SpellCastTargets& targets, uint8 cast_count, uint8 spell_index)
{
    ItemPrototype const* proto = item->GetProto();
    // special learning case (特殊学习物品使用情况)
    if (proto->Spells[0].SpellId == SPELL_ID_GENERIC_LEARN)
    {
        uint32 learn_spell_id = proto->Spells[0].SpellId;
        uint32 learning_spell_id = proto->Spells[1].SpellId;

        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(learn_spell_id);
        if (!spellInfo)
        {
            sLog.outError("Player::CastItemUseSpell: Item (Entry: %u) in have wrong spell id %u, ignoring ", proto->ItemId, learn_spell_id);
            SendEquipError(EQUIP_ERR_NONE, item);
            return;
        }

        Spell* spell = new Spell(this, spellInfo, TRIGGERED_NONE);
        spell->SetCastItem(item);
        item->SetUsedInSpell(true);
        spell->m_cast_count = cast_count;                   // set count of casts (消耗一次物品使用次数)
        spell->m_clientCast = true;
        spell->m_currentBasePoints[EFFECT_INDEX_0] = learning_spell_id;
        spell->SpellStart(&targets);
        return;
    }

    // use triggered flag only for items with many spell casts and for not first cast (同一物品多次施法时，后续法术使用自动触发标记)
    int count = 0;

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell (无绑定法术)
        if (!spellData.SpellId)
            continue;

        // wrong triggering type (非使用触发或无延迟使用型)
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
            continue;

        if (i != spell_index)
            continue;

        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellData.SpellId);
        if (!spellInfo)
        {
            sLog.outError("Player::CastItemUseSpell: Item (Entry: %u) in have wrong spell id %u, ignoring", proto->ItemId, spellData.SpellId);
            continue;
        }

        if (HasMissingTargetFromClient(spellInfo))
            targets.setUnitTarget(GetTarget());

        Spell* spell = new Spell(this, spellInfo, (count > 0) ? TRIGGERED_OLD_TRIGGERED : TRIGGERED_NONE);
        spell->SetCastItem(item);
        item->SetUsedInSpell(true);
        spell->m_cast_count = cast_count;                   // set count of casts (记录物品剩余使用次数)
        spell->m_clientCast = true;
        spell->SpellStart(&targets);

        ++count;
    }
}

void Player::_RemoveAllItemMods()
{
    DEBUG_LOG("_RemoveAllItemMods start.");

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            ItemPrototype const* proto = m_items[i]->GetProto();
            if (!proto)
                continue;

            // item set bonuses not dependent from item broken state (套装属性不依赖装备是否破损)
            if (proto->ItemSet)
                RemoveItemsSetItem(this, proto);

            if (m_items[i]->IsBroken())
                continue;

            ApplyItemEquipSpell(m_items[i], false);
            ApplyEnchantment(m_items[i], false);
        }
    }

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken())
                continue;
            ItemPrototype const* proto = m_items[i]->GetProto();
            if (!proto)
                continue;

            uint32 attacktype = Player::GetAttackBySlot(i);
            if (attacktype < MAX_ATTACK)
                _ApplyWeaponDependentAuraMods(m_items[i], WeaponAttackType(attacktype), false);

            _ApplyItemBonuses(proto, i, false);

            if (i == EQUIPMENT_SLOT_RANGED)
                _ApplyAmmoBonuses();
        }
    }

    DEBUG_LOG("_RemoveAllItemMods complete.");
}

void Player::_ApplyAllItemMods()
{
    DEBUG_LOG("_ApplyAllItemMods start.");

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken())
                continue;

            ItemPrototype const* proto = m_items[i]->GetProto();
            if (!proto)
                continue;

            uint32 attacktype = Player::GetAttackBySlot(i);
            if (attacktype < MAX_ATTACK)
                _ApplyWeaponDependentAuraMods(m_items[i], WeaponAttackType(attacktype), true);

            _ApplyItemBonuses(proto, i, true);

            if (i == EQUIPMENT_SLOT_RANGED)
                _ApplyAmmoBonuses();
        }
    }

    for (int i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            ItemPrototype const* proto = m_items[i]->GetProto();
            if (!proto)
                continue;

            // item set bonuses not dependent from item broken state (套装属性不依赖装备是否破损)
            if (proto->ItemSet)
                AddItemsSetItem(this, m_items[i]);

            if (m_items[i]->IsBroken())
                continue;

            ApplyItemEquipSpell(m_items[i], true);
            ApplyEnchantment(m_items[i], true);
        }
    }

    DEBUG_LOG("_ApplyAllItemMods complete.");
}

void Player::_ApplyAmmoBonuses()
{
    // check ammo (检查当前装备的弹药合法性)
    uint32 ammo_id = GetUInt32Value(PLAYER_AMMO_ID);
    if (!ammo_id)
        return;

    float currentAmmoDPSMin;
    float currentAmmoDPSMax;

    ItemPrototype const* ammo_proto = ObjectMgr::GetItemPrototype(ammo_id);
    if (!ammo_proto || ammo_proto->Class != ITEM_CLASS_PROJECTILE || !CheckAmmoCompatibility(ammo_proto))
    {
        currentAmmoDPSMin = 0.f;
        currentAmmoDPSMax = 0.f;
    }
    else
    {
        currentAmmoDPSMin = ammo_proto->Damage[0].DamageMin;
        currentAmmoDPSMax = ammo_proto->Damage[0].DamageMax;
    }

    if (std::make_pair(currentAmmoDPSMin, currentAmmoDPSMax) == GetAmmoDPS())
        return;

    m_ammoDPSMin = currentAmmoDPSMin;
    m_ammoDPSMax = currentAmmoDPSMax;

    if (CanModifyStats())
        UpdateDamagePhysical(RANGED_ATTACK);
}

bool Player::CheckAmmoCompatibility(const ItemPrototype* ammo_proto) const
{
    if (!ammo_proto)
        return false;

    // check ranged weapon (检查远程武器是否已装备且未破损)
    Item* weapon = GetWeaponForAttack(RANGED_ATTACK, true, false);
    if (!weapon)
        return false;

    ItemPrototype const* weapon_proto = weapon->GetProto();
    if (!weapon_proto || weapon_proto->Class != ITEM_CLASS_WEAPON)
        return false;

    // check ammo ws. weapon compatibility (校验弹药与远程武器类型匹配度，如弓配箭、枪配弹)
    switch (weapon_proto->SubClass)
    {
        case ITEM_SUBCLASS_WEAPON_BOW:
        case ITEM_SUBCLASS_WEAPON_CROSSBOW:
            if (ammo_proto->SubClass != ITEM_SUBCLASS_ARROW)
                return false;
            break;
        case ITEM_SUBCLASS_WEAPON_GUN:
            if (ammo_proto->SubClass != ITEM_SUBCLASS_BULLET)
                return false;
            break;
        default:
            return false;
    }

    return true;
}

/*  If in a battleground a player dies, and an enemy removes the insignia, the player's bones is lootable
    Called by remove insignia spell effect
    在战场中玩家死亡并被敌方拔除徽记后，其尸骨变为可拾取状态（由移除徽记法术效果调用） */
void Player::RemovedInsignia(Player* looterPlr)
{
    if (!GetBattleGroundId())
        return;

    // If not released spirit, do it ! (若尚未释放灵魂则强制释放)
    bool repop = false;
    if (m_deathTimer > 0)
    {
        m_deathTimer = 0;
        BuildPlayerRepop();
        RepopAtGraveyard();
        repop = true;
    }

    Corpse* corpse = GetCorpse();
    if (!corpse)
        return;

    WorldPacket data(SMSG_PLAYER_SKINNED, 0);
    data << uint8(repop);
    GetSession()->SendPacket(data);

    // We have to convert player corpse to bones, not to be able to resurrect there (将玩家尸体转为白骨，使其无法原地复活)
    // SpawnCorpseBones isn't handy, 'cos it saves player while he in BG (不使用 SpawnCorpseBones 以免在战场中触发角色存库)
    Corpse* bones = sObjectAccessor.ConvertCorpseForPlayer(GetObjectGuid(), true);
    if (!bones)
        return;

    // Now we must make bones lootable, and send player loot (使白骨变为可拾取战利品状态)
    bones->SetFlag(CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE);

    // We store the level of our player in the gold field (将玩家等级暂存于金币字段中，)
    // We retrieve this information at Player::SendLoot() (供 Player::SendLoot() 读取计算)
    bones->lootRecipient = looterPlr;

    Loot*& bonesLoot = bones->m_loot;
    if (!bonesLoot)
        bonesLoot = new Loot(looterPlr, bones, LOOT_INSIGNIA);
    else
    {
        if (bonesLoot->GetLootType() != LOOT_INSIGNIA)
        {
            delete bonesLoot;
            bonesLoot = new Loot(looterPlr, bones, LOOT_INSIGNIA);
        }
    }

    bonesLoot->ShowContentTo(looterPlr);
}

void Player::SendUpdateWorldState(uint32 Field, uint32 Value) const
{
    WorldPacket data(SMSG_UPDATE_WORLD_STATE, 8);
    data << Field;
    data << Value;
    GetSession()->SendPacket(data);
}

void Player::SendInitWorldStates(uint32 zoneid, uint32 areaid) const
{
    // data depends on zoneid/mapid... (世界状态数据取决于当前区域或地图 ID)
    BattleGround* bg = GetBattleGround();
    uint32 mapid = GetMapId();

    DEBUG_LOG("Sending SMSG_INIT_WORLD_STATES to Map:%u, Zone: %u", mapid, zoneid);

    uint32 count = 0;                                       // count of world states in packet (数据包中世界状态条目数)

    WorldPacket data(SMSG_INIT_WORLD_STATES, (4 + 4 + 4 + 2 + 8 * 8)); // guess (未知字段，通常为 0)
    data << uint32(mapid);                                  // mapid (地图 ID)
    data << uint32(zoneid);                                 // zone id (区域 ID)
    data << uint32(areaid);                                 // area id, new 2.1.0 (子区域 ID，2.1.0 新增)
    size_t count_pos = data.wpos();
    data << uint16(0);                                      // count of uint64 blocks, placeholder (世界状态数量占位符)

    // Current arena season (当前竞技场赛季编号)
    FillInitialWorldState(data, count, 0xC77, sWorld.getConfig(CONFIG_UINT32_ARENA_SEASON_ID));

    switch (zoneid)
    {
        case 139:                                           // Eastern Plaguelands (东瘟疫之地)
        case 1377:                                          // Silithus (希利苏斯)
        case 3483:                                          // Hellfire Peninsula (地狱火半岛)
        case 3518:                                          // Nagrand (纳格兰)
        case 3519:                                          // Terokkar Forest (泰罗卡森林)
        case 3521:                                          // Zangarmarsh (赞加沼泽)
            if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(zoneid))
                outdoorPvP->FillInitialWorldStates(data, count);
            break;
    }

    if (InstanceData* instanceData = GetMap()->GetInstanceData())
        instanceData->FillInitialWorldStates(data, count, zoneid, areaid);

    sWorldState.FillInitialWorldStates(data, count, zoneid, areaid);

    GetMap()->GetVariableManager().FillInitialWorldStates(data, count, zoneid, areaid);

    data.put<uint16>(count_pos, count);                     // set actual world state amount (回填实际世界状态条目数量)

    GetSession()->SendPacket(data);
}

uint32 Player::GetXPRestBonus(uint32 xp)
{
    uint32 rested_bonus = (uint32)GetRestBonus();           // xp for each rested bonus (计算双倍精力经验加成：每点双倍额度抵 1 点额外经验)

    if (rested_bonus > xp)                                  // max rested_bonus == xp or (r+x) = 200% xp (双倍经验上限等于基础经验，即总计 200%)
        rested_bonus = xp;

    SetRestBonus(GetRestBonus() - rested_bonus);

    DETAIL_LOG("Player gain %u xp (+ %u Rested Bonus). Rested points=%f", xp + rested_bonus, rested_bonus, GetRestBonus());
    return rested_bonus;
}

void Player::SetBindPoint(ObjectGuid guid) const
{
    WorldPacket data(SMSG_BINDER_CONFIRM, 8);
    data << ObjectGuid(guid);
    GetSession()->SendPacket(data);
}

void Player::SendTalentWipeConfirm(ObjectGuid guid) const
{
    WorldPacket data(MSG_TALENT_WIPE_CONFIRM, (8 + 4));
    data << ObjectGuid(guid);
    data << uint32(resetTalentsCost());
    GetSession()->SendPacket(data);
}

void Player::SendPetSkillWipeConfirm() const
{
    Pet* pet = GetPet();
    if (!pet)
        return;
    WorldPacket data(SMSG_PET_UNLEARN_CONFIRM, (8 + 4));
    data << ObjectGuid(pet->GetObjectGuid());
    data << uint32(pet->resetTalentsCost());
    GetSession()->SendPacket(data);
}

/*********************************************************/
/***             STORAGE SYSTEM (物品存储系统)           ***/
/*********************************************************/

void Player::SetVirtualItemSlot(uint8 i, Item* item)
{
    MANGOS_ASSERT(i < 3);
    if (i < 2 && item)
    {
        if (!item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
            return;
        uint32 charges = item->GetEnchantmentCharges(TEMP_ENCHANTMENT_SLOT);
        if (charges == 0)
            return;
        if (charges > 1)
            item->SetEnchantmentCharges(TEMP_ENCHANTMENT_SLOT, charges - 1);
        else if (charges <= 1)
        {
            ApplyEnchantment(item, TEMP_ENCHANTMENT_SLOT, false);
            item->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
        }
    }
}

void Player::SetSheath(SheathState sheathed)
{
    switch (sheathed)
    {
        case SHEATH_STATE_UNARMED:                          // no prepared weapon (收起所有武器)
            SetVirtualItemSlot(0, nullptr);
            SetVirtualItemSlot(1, nullptr);
            SetVirtualItemSlot(2, nullptr);
            break;
        case SHEATH_STATE_MELEE:                            // prepared melee weapon (拔出近战武器)
        {
            SetVirtualItemSlot(0, GetWeaponForAttack(BASE_ATTACK, true, true));
            SetVirtualItemSlot(1, GetWeaponForAttack(OFF_ATTACK, true, true));
            SetVirtualItemSlot(2, nullptr);
        }
        break;
        case SHEATH_STATE_RANGED:                           // prepared ranged weapon (拔出远程武器)
            SetVirtualItemSlot(0, nullptr);
            SetVirtualItemSlot(1, nullptr);
            SetVirtualItemSlot(2, GetWeaponForAttack(RANGED_ATTACK, true, true));
            break;
        default:
            SetVirtualItemSlot(0, nullptr);
            SetVirtualItemSlot(1, nullptr);
            SetVirtualItemSlot(2, nullptr);
            break;
    }
    Unit::SetSheath(sheathed);                              // this must visualize Sheath changing for other players... (更新武器拔出/收起状态字节以同步给其他玩家)
}

/// 为物品查找最合适的装备槽位（优先空位，必要时替换）
uint8 Player::FindEquipSlot(ItemPrototype const* proto, uint32 slot, bool swap) const
{
    uint8 slots[4];

    if (!ViableEquipSlots(proto, &slots[0]))
    {
        //DEBUG_LOG("**** [Player::FindEquipSlot] ViableEquipSlots returned FALSE  ****");
        return NULL_SLOT;
    }

    if (slot != NULL_SLOT)
    {
        if (swap || !GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            for (uint8 currentSlot : slots)
            {
                if (currentSlot == slot)
                    return slot;
            }
        }
    }
    else
    {
        // search free slot at first (优先查找空闲装备槽位)
        for (uint8 currentSlot : slots)
        {
            if (currentSlot != NULL_SLOT && !GetItemByPos(INVENTORY_SLOT_BAG_0, currentSlot))
            {
                // not considered free if equipping 1H into offhand slot while wielding 2H (手持双手武器时，副手槽位不视为空闲)
                // also not free if equipping 2H into offhand slot while wielding 1H (手持单手武器时，向副手装双手武器也不视为空闲)
                // in both scenarios, a swap is required (上述两种情况均须执行替换操作)
                if (currentSlot == EQUIPMENT_SLOT_OFFHAND && (IsTwoHandUsed() || proto->InventoryType == INVTYPE_2HWEAPON))
                    continue;

                return currentSlot;
            }
        }

        // if not found free and can swap return first appropriate from used (若无空闲槽位且允许替换，则返回首个可用槽位)
        for (uint8 currentSlot : slots)
        {
            if (currentSlot != NULL_SLOT && swap)
                return currentSlot;
        }
    }

    // no free position (无可用装备位置)
    return NULL_SLOT;
}


bool Player::ViableEquipSlots(ItemPrototype const* proto, uint8* viable_slots) const
{
    uint8 pClass;

    //DEBUG_LOG("**** [Player::ViableEquipSlots] Start ****");

    if (!viable_slots)
    {
        //DEBUG_LOG("**** [Player::ViableEquipSlots] Return array is NULL ****");
        return false;
    }

    //DEBUG_LOG("**** [Player::ViableEquipSlots] Initialize return array ****");
    viable_slots[0] = NULL_SLOT;
    viable_slots[1] = NULL_SLOT;
    viable_slots[2] = NULL_SLOT;
    viable_slots[3] = NULL_SLOT;

    if (CanUseItem(proto) == EQUIP_ERR_OK)
    {
        //DEBUG_LOG("**** [Player::ViableEquipSlots] Class/Race/Faction determined viable ****");

        //DEBUG_LOG("**** [Player::ViableEquipSlots] switch (proto->InventoryType) ****");
        switch (proto->InventoryType)
        {
            case INVTYPE_HEAD:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_HEAD ****");
                viable_slots[0] = EQUIPMENT_SLOT_HEAD;
                break;
            case INVTYPE_NECK:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_NECK ****");
                viable_slots[0] = EQUIPMENT_SLOT_NECK;
                break;
            case INVTYPE_SHOULDERS:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_SHOULDERS ****");
                viable_slots[0] = EQUIPMENT_SLOT_SHOULDERS;
                break;
            case INVTYPE_BODY:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_BODY ****");
                viable_slots[0] = EQUIPMENT_SLOT_BODY;
                break;
            case INVTYPE_CHEST:
            case INVTYPE_ROBE:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == %s ****",(INVTYPE_CHEST ? "INVTYPE_CHEST" : "INVTYPE_ROBE"));
                viable_slots[0] = EQUIPMENT_SLOT_CHEST;
                break;
            case INVTYPE_WAIST:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WAIST ****");
                viable_slots[0] = EQUIPMENT_SLOT_WAIST;
                break;
            case INVTYPE_LEGS:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_LEGS ****");
                viable_slots[0] = EQUIPMENT_SLOT_LEGS;
                break;
            case INVTYPE_FEET:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_FEET ****");
                viable_slots[0] = EQUIPMENT_SLOT_FEET;
                break;
            case INVTYPE_WRISTS:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WRISTS ****");
                viable_slots[0] = EQUIPMENT_SLOT_WRISTS;
                break;
            case INVTYPE_HANDS:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_HANDS ****");
                viable_slots[0] = EQUIPMENT_SLOT_HANDS;
                break;
            case INVTYPE_FINGER:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_FINGER ****");
                viable_slots[0] = EQUIPMENT_SLOT_FINGER1;
                viable_slots[1] = EQUIPMENT_SLOT_FINGER2;
                break;
            case INVTYPE_TRINKET:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_TRINKET ****");
                viable_slots[0] = EQUIPMENT_SLOT_TRINKET1;
                viable_slots[1] = EQUIPMENT_SLOT_TRINKET2;
                break;
            case INVTYPE_CLOAK:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_CLOAK ****");
                viable_slots[0] = EQUIPMENT_SLOT_BACK;
                break;
            case INVTYPE_WEAPON:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WEAPON ****");
                viable_slots[0] = EQUIPMENT_SLOT_MAINHAND;

                //DEBUG_LOG("**** [Player::ViableEquipSlots] INVTYPE_WEAPON/Determining if can dual weild ****");
                if (CanDualWield())
                {
                    //DEBUG_LOG("**** [Player::ViableEquipSlots] INVTYPE_WEAPON/CanDualWield() == TRUE  ****");
                    viable_slots[1] = EQUIPMENT_SLOT_OFFHAND;
                }
                break;
            case INVTYPE_SHIELD:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_SHIELD ****");
                viable_slots[0] = EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_RANGED:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RANGED ****");
                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                break;
            case INVTYPE_2HWEAPON:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_2HWEAPON ****");
                viable_slots[0] = EQUIPMENT_SLOT_MAINHAND;

                //DEBUG_LOG("**** [Player::ViableEquipSlots] INVTYPE_2HWEAPON/Determining if can dual weild and Titian Grip ****");
                if (CanDualWield())
                {
                    //DEBUG_LOG("**** [Player::ViableEquipSlots] INVTYPE_2HWEAPON/CanDualWield() && CanTitanGrip() == TRUE  ****");
                    viable_slots[1] = EQUIPMENT_SLOT_OFFHAND;
                }
                break;
            case INVTYPE_TABARD:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_TABARD ****");
                viable_slots[0] = EQUIPMENT_SLOT_TABARD;
                break;
            case INVTYPE_WEAPONMAINHAND:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WEAPONMAINHAND ****");
                viable_slots[0] = EQUIPMENT_SLOT_MAINHAND;
                break;
            case INVTYPE_WEAPONOFFHAND:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_WEAPONOFFHAND ****");
                viable_slots[0] = EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_HOLDABLE:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_HOLDABLE ****");
                viable_slots[0] = EQUIPMENT_SLOT_OFFHAND;
                break;
            case INVTYPE_THROWN:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_THROWN ****");
                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                break;
            case INVTYPE_RANGEDRIGHT:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RANGEDRIGHT ****");
                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                break;
            case INVTYPE_BAG:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_BAG ****");
                viable_slots[0] = INVENTORY_SLOT_BAG_START + 0;
                viable_slots[1] = INVENTORY_SLOT_BAG_START + 1;
                viable_slots[2] = INVENTORY_SLOT_BAG_START + 2;
                viable_slots[3] = INVENTORY_SLOT_BAG_START + 3;
                break;
            case INVTYPE_RELIC:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC ****");

                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC - Determine Play Class ****");
                pClass = getClass();

                if (pClass)
                {
                    //DEBUG_LOG("**** [Player::ViableEquipSlots]  proto->InventoryType == INVTYPE_RELIC - Call to getClass() returned sucess ****");

                    switch (proto->SubClass)
                    {
                        case ITEM_SUBCLASS_ARMOR_LIBRAM:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == ITEM_SUBCLASS_ARMOR_LIBRAM ****");
                            if (pClass == CLASS_PALADIN)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        case ITEM_SUBCLASS_ARMOR_IDOL:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == ITEM_SUBCLASS_ARMOR_IDOL ****");
                            if (pClass == CLASS_DRUID)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        case ITEM_SUBCLASS_ARMOR_TOTEM:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == ITEM_SUBCLASS_ARMOR_TOTEM ****");
                            if (pClass == CLASS_SHAMAN)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        case ITEM_SUBCLASS_ARMOR_MISC:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == ITEM_SUBCLASS_ARMOR_MISC ****");
                            if (pClass == CLASS_WARLOCK)
                            {
                                viable_slots[0] = EQUIPMENT_SLOT_RANGED;
                            }
                            break;
                        default:
                            //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == INVTYPE_RELIC / proto->SubClass == UNKNOWN ****");
                            break;
                    }
                }
                break;
            default:
                //DEBUG_LOG("**** [Player::ViableEquipSlots] proto->InventoryType == UNKNOWN ****");
                break;
        }
    }
    return (viable_slots[0] != NULL_SLOT);
}

InventoryResult Player::CanUnequipItems(uint32 item, uint32 count) const
{
    Item* pItem;
    uint32 tempcount = 0;

    InventoryResult res = EQUIP_ERR_OK;

    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            InventoryResult ires = CanUnequipItem(INVENTORY_SLOT_BAG_0 << 8 | i, false);
            if (ires == EQUIP_ERR_OK)
            {
                tempcount += pItem->GetCount();
                if (tempcount >= count)
                    return EQUIP_ERR_OK;
            }
            else
                res = ires;
        }
    }
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
                return EQUIP_ERR_OK;
        }
    }
    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
                return EQUIP_ERR_OK;
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pBag)
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                pItem = GetItemByPos(i, j);
                if (pItem && pItem->GetEntry() == item)
                {
                    tempcount += pItem->GetCount();
                    if (tempcount >= count)
                        return EQUIP_ERR_OK;
                }
            }
        }
    }

    // not found req. item count and have unequippable items (未满足物品数量要求且存在不可卸下物品)
    return res;
}

uint32 Player::GetItemCount(uint32 item, bool inBankAlso, Item* skipItem) const
{
    uint32 count = 0;
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem != skipItem &&  pItem->GetEntry() == item)
            count += pItem->GetCount();
    }
    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem != skipItem && pItem->GetEntry() == item)
            count += pItem->GetCount();
    }
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pBag)
            count += pBag->GetItemCount(item, skipItem);
    }

    if (skipItem && skipItem->GetProto()->GemProperties)
    {
        for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem != skipItem && pItem->GetProto()->Socket[0].Color)
                count += pItem->GetGemCountWithID(item);
        }
    }

    if (inBankAlso)
    {
        for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem != skipItem && pItem->GetEntry() == item)
                count += pItem->GetCount();
        }
        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pBag)
                count += pBag->GetItemCount(item, skipItem);
        }

        if (skipItem && skipItem->GetProto()->GemProperties)
        {
            for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
            {
                Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (pItem && pItem != skipItem && pItem->GetProto()->Socket[0].Color)
                    count += pItem->GetGemCountWithID(item);
            }
        }
    }

    return count;
}

Item* Player::GetItemByGuid(ObjectGuid guid) const
{
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetObjectGuid() == guid)
                return pItem;

    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetObjectGuid() == guid)
                return pItem;

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* pItem = pBag->GetItemByPos(j))
                    if (pItem->GetObjectGuid() == guid)
                        return pItem;

    for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetObjectGuid() == guid)
                return pItem;

    for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* pItem = pBag->GetItemByPos(j))
                    if (pItem->GetObjectGuid() == guid)
                        return pItem;

    return nullptr;
}

Item* Player::GetItemByPos(uint16 pos) const
{
    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;
    return GetItemByPos(bag, slot);
}

Item* Player::GetItemByPos(uint8 bag, uint8 slot) const
{
    if (bag == INVENTORY_SLOT_BAG_0 && (slot < BANK_SLOT_BAG_END || (slot >= KEYRING_SLOT_START && slot < KEYRING_SLOT_END)))
        return m_items[slot];
    if ((bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
        || (bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END))
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
            return pBag->GetItemByPos(slot);
    }
    return nullptr;
}

Item* Player::GetItemByEntry(uint32 item) const
{
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetEntry() == item)
            {
                return pItem;
            }
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (Item* itemPtr = pBag->GetItemByEntry(item))
            {
                return itemPtr;
            }
        }
    }

    return nullptr;
}

uint32 Player::GetItemDisplayIdInSlot(uint8 bag, uint8 slot) const
{
    const Item* pItem = GetItemByPos(bag, slot);

    if (!pItem)
        return 0;

    return pItem->GetProto()->DisplayInfoID;
}

Item* Player::GetWeaponForAttack(WeaponAttackType attackType, bool nonbroken, bool useable) const
{
    uint8 slot;
    switch (attackType)
    {
        case BASE_ATTACK:   slot = EQUIPMENT_SLOT_MAINHAND; break;
        case OFF_ATTACK:    slot = EQUIPMENT_SLOT_OFFHAND;  break;
        case RANGED_ATTACK: slot = EQUIPMENT_SLOT_RANGED;   break;
        default: return nullptr;
    }

    Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!item || item->GetProto()->Class != ITEM_CLASS_WEAPON)
        return nullptr;

    if (useable && !CanUseEquippedWeapon(attackType))
        return nullptr;

    if (nonbroken && item->IsBroken())
        return nullptr;

    return item;
}

Item* Player::GetShield(bool useable) const
{
    Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!item || item->GetProto()->Class != ITEM_CLASS_ARMOR)
        return nullptr;

    if (!useable)
        return item;

    if (item->IsBroken() || !CanUseEquippedWeapon(OFF_ATTACK))
        return nullptr;

    return item;
}

uint32 Player::GetAttackBySlot(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_MAINHAND: return BASE_ATTACK;
        case EQUIPMENT_SLOT_OFFHAND:  return OFF_ATTACK;
        case EQUIPMENT_SLOT_RANGED:   return RANGED_ATTACK;
        default:                      return MAX_ATTACK;
    }
}

bool Player::IsInventoryPos(uint8 bag, uint8 slot)
{
    if (bag == INVENTORY_SLOT_BAG_0 && slot == NULL_SLOT)
        return true;
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END))
        return true;
    if (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
        return true;
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= KEYRING_SLOT_START && slot < KEYRING_SLOT_END))
        return true;
    return false;
}

bool Player::IsEquipmentPos(uint8 bag, uint8 slot)
{
    if (bag == INVENTORY_SLOT_BAG_0 && (slot < EQUIPMENT_SLOT_END))
        return true;
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= INVENTORY_SLOT_BAG_START && slot < INVENTORY_SLOT_BAG_END))
        return true;
    return false;
}

bool Player::IsBankPos(uint8 bag, uint8 slot)
{
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= BANK_SLOT_ITEM_START && slot < BANK_SLOT_ITEM_END))
        return true;
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END))
        return true;
    if (bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END)
        return true;
    return false;
}

bool Player::IsBagPos(uint16 pos)
{
    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= INVENTORY_SLOT_BAG_START && slot < INVENTORY_SLOT_BAG_END))
        return true;
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END))
        return true;
    return false;
}

bool Player::IsValidPos(uint8 bag, uint8 slot, bool explicit_pos) const
{
    // post selected (指定了具体背包与槽位)
    if (bag == NULL_BAG && !explicit_pos)
        return true;

    if (bag == INVENTORY_SLOT_BAG_0)
    {
        // any post selected (允许任意槽位)
        if (slot == NULL_SLOT && !explicit_pos)
            return true;

        // equipment (装备栏槽位)
        if (slot < EQUIPMENT_SLOT_END)
            return true;

        // bag equip slots (背包栏位)
        if (slot >= INVENTORY_SLOT_BAG_START && slot < INVENTORY_SLOT_BAG_END)
            return true;

        // backpack slots (主行囊槽位)
        if (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END)
            return true;

        // keyring slots (钥匙链槽位)
        if (slot >= KEYRING_SLOT_START && slot < KEYRING_SLOT_END)
            return true;

        // bank main slots (银行主格槽位)
        if (slot >= BANK_SLOT_ITEM_START && slot < BANK_SLOT_ITEM_END)
            return true;

        // bank bag slots (银行背包栏位)
        if (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END)
            return true;

        return false;
    }

    // bag content slots (已装备背包内部槽位)
    if (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
            return false;

        // any post selected (背包内任意槽位)
        if (slot == NULL_SLOT && !explicit_pos)
            return true;

        return slot < pBag->GetBagSize();
    }

    // bank bag content slots (银行背包内部槽位)
    if (bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
            return false;

        // any post selected (银行背包内任意槽位)
        if (slot == NULL_SLOT && !explicit_pos)
            return true;

        return slot < pBag->GetBagSize();
    }

    // where this? (未知非法位置)
    return false;
}


bool Player::HasItemCount(uint32 item, uint32 count, bool inBankAlso) const
{
    uint32 tempcount = 0;
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
                return true;
        }
    }
    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
                return true;
        }
    }
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                Item* pItem = GetItemByPos(i, j);
                if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
                {
                    tempcount += pItem->GetCount();
                    if (tempcount >= count)
                        return true;
                }
            }
        }
    }

    if (inBankAlso)
    {
        for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                tempcount += pItem->GetCount();
                if (tempcount >= count)
                    return true;
            }
        }
        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    Item* pItem = GetItemByPos(i, j);
                    if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
                    {
                        tempcount += pItem->GetCount();
                        if (tempcount >= count)
                            return true;
                    }
                }
            }
        }
    }

    return false;
}

bool Player::HasItemOrGemWithIdEquipped(uint32 item, uint32 count, uint8 except_slot) const
{
    uint32 tempcount = 0;
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (i == int(except_slot))
            continue;

        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
                return true;
        }
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (pProto && pProto->GemProperties)
    {
        for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (i == int(except_slot))
                continue;

            Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem->GetProto()->Socket[0].Color)
            {
                tempcount += pItem->GetGemCountWithID(item);
                if (tempcount >= count)
                    return true;
            }
        }
    }

    return false;
}

InventoryResult Player::_CanTakeMoreSimilarItems(uint32 entry, uint32 count, Item* pItem, uint32* no_space_count) const
{
    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(entry);
    if (!pProto)
    {
        if (no_space_count)
            *no_space_count = count;
        return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
    }

    // no maximum (无最大堆叠持有上限)
    if (pProto->MaxCount > 0)
    {
        uint32 curcount = GetItemCount(pProto->ItemId, true, pItem);

        if (curcount + count > uint32(pProto->MaxCount))
        {
            if (no_space_count)
                *no_space_count = count + curcount - pProto->MaxCount;
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }
    }


    return EQUIP_ERR_OK;
}

bool Player::HasItemTotemCategory(uint32 TotemCategory) const
{
    Item* pItem;
    for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && IsTotemCategoryCompatiableWith(pItem->GetProto()->TotemCategory, TotemCategory))
            return true;
    }
    for (uint8 i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && IsTotemCategoryCompatiableWith(pItem->GetProto()->TotemCategory, TotemCategory))
            return true;
    }
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                pItem = GetItemByPos(i, j);
                if (pItem && IsTotemCategoryCompatiableWith(pItem->GetProto()->TotemCategory, TotemCategory))
                    return true;
            }
        }
    }
    return false;
}

InventoryResult Player::_CanStoreItem_InSpecificSlot(uint8 bag, uint8 slot, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool swap, Item* pSrcItem, uint8& bagSlot) const
{
    Item* pItem2 = GetItemByPos(bag, slot);

    // ignore move item (this slot will be empty at move) (忽略正在移动的源物品所在格，移动后该格将腾空)
    if (pItem2 == pSrcItem)
        pItem2 = nullptr;

    uint32 need_space;

    // empty specific slot - check item fit to slot (目标指定槽位为空：校验物品是否可放入)
    if (!pItem2 || swap)
    {
        if (bag == INVENTORY_SLOT_BAG_0)
        {
            // keyring case (钥匙链仅允许存放钥匙)
            if (slot >= KEYRING_SLOT_START && slot < KEYRING_SLOT_START + GetMaxKeyringSize() && !(pProto->BagFamily & BAG_FAMILY_MASK_KEYS))
            {
                bagSlot = bag;
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2;
            }

            // prevent cheating (禁止将非空背包放入背包内部，防止套娃作弊)
            if ((slot >= BUYBACK_SLOT_START && slot < BUYBACK_SLOT_END) || slot >= PLAYER_SLOT_END)
            {
                bagSlot = bag;
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2;
            }
        }
        else
        {
            Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
            if (!pBag)
            {
                bagSlot = bag;
                return EQUIP_ERR_INT_BAG_ERROR;
            }

            ItemPrototype const* pBagProto = pBag->GetProto();
            if (!pBagProto)
            {
                bagSlot = bag;
                return EQUIP_ERR_INT_BAG_ERROR;
            }

            if (slot >= pBagProto->ContainerSlots)
            {
                bagSlot = bag;
                return EQUIP_ERR_INT_BAG_ERROR;
            }

            if (!ItemCanGoIntoBag(pProto, pBagProto))
            {
                bagSlot = bag;
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2;
            }
        }

        // non empty stack with space (目标格已有可叠加物品且尚有堆叠余量)
        need_space = pProto->Stackable;
    }
    // non empty slot, check item type (目标格非空，检查是否为同类物品)
    else
    {
        // can be merged at least partly (至少可以部分合并堆叠)
        InventoryResult res  = pItem2->CanBeMergedPartlyWith(pProto);
        if (res != EQUIP_ERR_OK)
            return res;

        need_space = pProto->Stackable - pItem2->GetCount();
    }

    if (need_space > count)
        need_space = count;

    ItemPosCount newPosition = ItemPosCount((bag << 8) | slot, need_space);
    if (!newPosition.isContainedIn(dest))
    {
        dest.push_back(newPosition);
        count -= need_space;
    }
    return EQUIP_ERR_OK;
}

InventoryResult Player::_CanStoreItem_InBag(uint8 bag, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, bool non_specialized, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot, uint8& bagSlot) const
{
    // skip specific bag already processed in first called _CanStoreItem_InBag (跳过首次已检查过的指定背包)
    if (bag == skip_bag)
    {
        bagSlot = bag;
        return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2;
    }

    // skip nonexistent bag or self targeted bag (跳过不存在的背包或物品自身所在的背包)
    Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
    if (!pBag || pBag == pSrcItem)
    {
        bagSlot = bag;
        return EQUIP_ERR_INT_BAG_ERROR;
    }

    ItemPrototype const* pBagProto = pBag->GetProto();
    if (!pBagProto)
    {
        bagSlot = bag;
        return EQUIP_ERR_INT_BAG_ERROR;
    }

    // specialized bag mode or non-specialized (按专用背包模式或普通背包模式匹配)
    if (non_specialized != (pBagProto->Class == ITEM_CLASS_CONTAINER && pBagProto->SubClass == ITEM_SUBCLASS_CONTAINER))
    {
        bagSlot = bag;
        return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2;
    }

    if (!ItemCanGoIntoBag(pProto, pBagProto))
    {
        bagSlot = bag;
        return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2;
    }

    for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
    {
        // skip specific slot already processed in first called _CanStoreItem_InSpecificSlot (跳过已单独检查过的指定槽位)
        if (j == skip_slot)
            continue;

        Item* pItem2 = GetItemByPos(bag, j);

        // ignore move item (this slot will be empty at move) (忽略待移动源物品所在格)
        if (pItem2 == pSrcItem)
            pItem2 = nullptr;

        // if merge skip empty, if !merge skip non-empty (合并阶段跳过空格，找空格阶段跳过非空格)
        if ((pItem2 != nullptr) != merge)
            continue;

        uint32 need_space = pProto->GetMaxStackSize();

        if (pItem2)
        {
            // can be merged at least partly (至少可部分叠加)
            uint8 res  = pItem2->CanBeMergedPartlyWith(pProto);
            if (res != EQUIP_ERR_OK)
                continue;

            // decrease at current stacksize (扣减可合并的剩余数量)
            need_space -= pItem2->GetCount();
        }

        if (need_space > count)
            need_space = count;

        ItemPosCount newPosition = ItemPosCount((bag << 8) | j, need_space);
        if (!newPosition.isContainedIn(dest))
        {
            dest.push_back(newPosition);
            count -= need_space;

            if (count == 0)
                return EQUIP_ERR_OK;
        }
    }
    return EQUIP_ERR_OK;
}

InventoryResult Player::_CanStoreItem_InInventorySlots(uint8 slot_begin, uint8 slot_end, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot) const
{
    for (uint32 j = slot_begin; j < slot_end; ++j)
    {
        // skip specific slot already processed in first called _CanStoreItem_InSpecificSlot (跳过已单独检查过的指定槽位)
        if (INVENTORY_SLOT_BAG_0 == skip_bag && j == skip_slot)
            continue;

        Item* pItem2 = GetItemByPos(INVENTORY_SLOT_BAG_0, j);

        // ignore move item (this slot will be empty at move) (忽略待移动源物品所在格)
        if (pItem2 == pSrcItem)
            pItem2 = nullptr;

        // if merge skip empty, if !merge skip non-empty (合并阶段跳过空格，找空格阶段跳过非空格)
        if ((pItem2 != nullptr) != merge)
            continue;

        uint32 need_space = pProto->GetMaxStackSize();

        if (pItem2)
        {
            // can be merged at least partly (至少可部分叠加)
            uint8 res  = pItem2->CanBeMergedPartlyWith(pProto);
            if (res != EQUIP_ERR_OK)
                continue;

            // descrease at current stacksize (扣减可合并的剩余数量)
            need_space -= pItem2->GetCount();
        }

        if (need_space > count)
            need_space = count;

        ItemPosCount newPosition = ItemPosCount((INVENTORY_SLOT_BAG_0 << 8) | j, need_space);
        if (!newPosition.isContainedIn(dest))
        {
            dest.push_back(newPosition);
            count -= need_space;

            if (count == 0)
                return EQUIP_ERR_OK;
        }
    }
    return EQUIP_ERR_OK;
}

/// 检查背包/银行/行囊是否有足够空间存放指定物品并输出目标位置列表
InventoryResult Player::_CanStoreItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 entry, uint32 count, uint8& bagSlot, Item* pItem, bool swap, uint32* no_space_count) const
{
    DEBUG_LOG("STORAGE: CanStoreItem bag = %u, slot = %u, item = %u, count = %u", bag, slot, entry, count);

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(entry);
    if (!pProto)
    {
        if (no_space_count)
            *no_space_count = count;
        return swap ? EQUIP_ERR_ITEMS_CANT_BE_SWAPPED : EQUIP_ERR_ITEM_NOT_FOUND;
    }

    if (pItem)
    {
        // item used (物品当前正在被使用或处于不可移动状态)
        if (pItem->HasTemporaryLoot())
        {
            if (no_space_count)
                *no_space_count = count;
            return EQUIP_ERR_ALREADY_LOOTED;
        }

        if (pItem->IsBindedNotWith(this))
        {
            if (no_space_count)
                *no_space_count = count;
            return EQUIP_ERR_DONT_OWN_THAT_ITEM;
        }
    }

    // check count of items (skip for auto move for same player from bank) (检查唯一持有数量限制，自身银行取回除外)
    uint32 no_similar_count = 0;                            // can't store this amount similar items (超出同类物品最大持有数量)
    InventoryResult res = _CanTakeMoreSimilarItems(entry, count, pItem, &no_similar_count);
    if (res != EQUIP_ERR_OK)
    {
        if (count == no_similar_count)
        {
            if (no_space_count)
                *no_space_count = no_similar_count;
            return res;
        }
        count -= no_similar_count;
    }

    // in specific slot (指定了具体存放槽位)
    if (bag != NULL_BAG && slot != NULL_SLOT)
    {
        res = _CanStoreItem_InSpecificSlot(bag, slot, dest, pProto, count, swap, pItem, bagSlot);
        if (res != EQUIP_ERR_OK)
        {
            if (no_space_count)
                *no_space_count = count + no_similar_count;
            return res;
        }

        if (count == 0)
        {
            if (no_similar_count == 0)
                return EQUIP_ERR_OK;

            if (no_space_count)
                *no_space_count = count + no_similar_count;
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }
    }

    // not specific slot or have space for partly store only in specific slot (未指定槽位或指定槽位仅能部分存放)

    // in specific bag (指定了具体背包)
    if (bag != NULL_BAG)
    {
        // search stack in bag for merge to (在指定背包中查找可叠加堆叠)
        if (pProto->Stackable > 1)
        {
            if (bag == INVENTORY_SLOT_BAG_0)               // inventory (主行囊区域)
            {
                res = _CanStoreItem_InInventorySlots(KEYRING_SLOT_START, KEYRING_SLOT_END, dest, pProto, count, true, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                {
                    if (no_space_count)
                        *no_space_count = count + no_similar_count;
                    return res;
                }

                if (count == 0)
                {
                    if (no_similar_count == 0)
                        return EQUIP_ERR_OK;

                    if (no_space_count)
                        *no_space_count = count + no_similar_count;
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }

                res = _CanStoreItem_InInventorySlots(INVENTORY_SLOT_ITEM_START, INVENTORY_SLOT_ITEM_END, dest, pProto, count, true, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                {
                    if (no_space_count)
                        *no_space_count = count + no_similar_count;
                    return res;
                }

                if (count == 0)
                {
                    if (no_similar_count == 0)
                        return EQUIP_ERR_OK;

                    if (no_space_count)
                        *no_space_count = count + no_similar_count;
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }
            }
            else                                            // equipped bag (已装备背包)
            {
                // we need check 2 time (specialized/non_specialized), use NULL_BAG to prevent skipping bag (需按专用/通用检查两次，传 NULL_BAG 以免跳过)
                res = _CanStoreItem_InBag(bag, dest, pProto, count, true, false, pItem, NULL_BAG, slot, bagSlot);
                if (res != EQUIP_ERR_OK)
                    res = _CanStoreItem_InBag(bag, dest, pProto, count, true, true, pItem, NULL_BAG, slot, bagSlot);

                if (res != EQUIP_ERR_OK)
                {
                    if (no_space_count)
                        *no_space_count = count + no_similar_count;
                    return res;
                }

                if (count == 0)
                {
                    if (no_similar_count == 0)
                        return EQUIP_ERR_OK;

                    if (no_space_count)
                        *no_space_count = count + no_similar_count;
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }
            }
        }

        // search free slot in bag for place to (在指定背包中查找空闲槽位)
        if (bag == INVENTORY_SLOT_BAG_0)                    // inventory (主行囊)
        {
            // search free slot - keyring case (在钥匙链中查找空闲槽位)
            if (pProto->BagFamily & BAG_FAMILY_MASK_KEYS)
            {
                uint32 keyringSize = GetMaxKeyringSize();
                res = _CanStoreItem_InInventorySlots(KEYRING_SLOT_START, KEYRING_SLOT_START + keyringSize, dest, pProto, count, false, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                {
                    if (no_space_count)
                        *no_space_count = count + no_similar_count;
                    return res;
                }

                if (count == 0)
                {
                    if (no_similar_count == 0)
                        return EQUIP_ERR_OK;

                    if (no_space_count)
                        *no_space_count = count + no_similar_count;
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }
            }

            res = _CanStoreItem_InInventorySlots(INVENTORY_SLOT_ITEM_START, INVENTORY_SLOT_ITEM_END, dest, pProto, count, false, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
            {
                if (no_space_count)
                    *no_space_count = count + no_similar_count;
                return res;
            }

            if (count == 0)
            {
                if (no_similar_count == 0)
                    return EQUIP_ERR_OK;

                if (no_space_count)
                    *no_space_count = count + no_similar_count;
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }
        else                                                // equipped bag (已装备背包)
        {
            res = _CanStoreItem_InBag(bag, dest, pProto, count, false, false, pItem, NULL_BAG, slot, bagSlot);
            if (res != EQUIP_ERR_OK)
                res = _CanStoreItem_InBag(bag, dest, pProto, count, false, true, pItem, NULL_BAG, slot, bagSlot);

            if (res != EQUIP_ERR_OK)
            {
                if (no_space_count)
                    *no_space_count = count + no_similar_count;
                return res;
            }

            if (count == 0)
            {
                if (no_similar_count == 0)
                    return EQUIP_ERR_OK;

                if (no_space_count)
                    *no_space_count = count + no_similar_count;
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }
    }

    // not specific bag or have space for partly store only in specific bag (未指定背包或指定背包仅能部分存放)

    // search stack for merge to (全局查找可叠加物品堆叠)
    if (pProto->Stackable > 1)
    {
        res = _CanStoreItem_InInventorySlots(KEYRING_SLOT_START, KEYRING_SLOT_END, dest, pProto, count, true, pItem, bag, slot);
        if (res != EQUIP_ERR_OK)
        {
            if (no_space_count)
                *no_space_count = count + no_similar_count;
            return res;
        }

        if (count == 0)
        {
            if (no_similar_count == 0)
                return EQUIP_ERR_OK;

            if (no_space_count)
                *no_space_count = count + no_similar_count;
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }

        res = _CanStoreItem_InInventorySlots(INVENTORY_SLOT_ITEM_START, INVENTORY_SLOT_ITEM_END, dest, pProto, count, true, pItem, bag, slot);
        if (res != EQUIP_ERR_OK)
        {
            if (no_space_count)
                *no_space_count = count + no_similar_count;
            return res;
        }

        if (count == 0)
        {
            if (no_similar_count == 0)
                return EQUIP_ERR_OK;

            if (no_space_count)
                *no_space_count = count + no_similar_count;
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }

        if (pProto->BagFamily)
        {
            for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                res = _CanStoreItem_InBag(i, dest, pProto, count, true, false, pItem, bag, slot, bagSlot);
                if (res != EQUIP_ERR_OK)
                    continue;

                if (count == 0)
                {
                    if (no_similar_count == 0)
                        return EQUIP_ERR_OK;

                    if (no_space_count)
                        *no_space_count = count + no_similar_count;
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }
            }
        }

        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            res = _CanStoreItem_InBag(i, dest, pProto, count, true, true, pItem, bag, slot, bagSlot);
            if (res != EQUIP_ERR_OK)
                continue;

            if (count == 0)
            {
                if (no_similar_count == 0)
                    return EQUIP_ERR_OK;

                if (no_space_count)
                    *no_space_count = count + no_similar_count;
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }
    }

    // search free slot - special bag case (优先在专用背包如箭袋/草药袋中查找空位)
    if (pProto->BagFamily)
    {
        if (pProto->BagFamily & BAG_FAMILY_MASK_KEYS)
        {
            uint32 keyringSize = GetMaxKeyringSize();
            res = _CanStoreItem_InInventorySlots(KEYRING_SLOT_START, KEYRING_SLOT_START + keyringSize, dest, pProto, count, false, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
            {
                if (no_space_count)
                    *no_space_count = count + no_similar_count;
                return res;
            }

            if (count == 0)
            {
                if (no_similar_count == 0)
                    return EQUIP_ERR_OK;

                if (no_space_count)
                    *no_space_count = count + no_similar_count;
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }

        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            res = _CanStoreItem_InBag(i, dest, pProto, count, false, false, pItem, bag, slot, bagSlot);
            if (res != EQUIP_ERR_OK)
                continue;

            if (count == 0)
            {
                if (no_similar_count == 0)
                    return EQUIP_ERR_OK;

                if (no_space_count)
                    *no_space_count = count + no_similar_count;
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }
    }

    // Normally it would be impossible to autostore not empty bags (通常非空背包不应直接自动存入)
    if (pItem && pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
        return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;

    // search free slot (查找普通空位)
    res = _CanStoreItem_InInventorySlots(INVENTORY_SLOT_ITEM_START, INVENTORY_SLOT_ITEM_END, dest, pProto, count, false, pItem, bag, slot);
    if (res != EQUIP_ERR_OK)
    {
        if (no_space_count)
            *no_space_count = count + no_similar_count;
        return res;
    }

    if (count == 0)
    {
        if (no_similar_count == 0)
            return EQUIP_ERR_OK;

        if (no_space_count)
            *no_space_count = count + no_similar_count;
        return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        res = _CanStoreItem_InBag(i, dest, pProto, count, false, true, pItem, bag, slot, bagSlot);
        if (res != EQUIP_ERR_OK)
            continue;

        if (count == 0)
        {
            if (no_similar_count == 0)
                return EQUIP_ERR_OK;

            if (no_space_count)
                *no_space_count = count + no_similar_count;
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }
    }

    if (no_space_count)
        *no_space_count = count + no_similar_count;

    return EQUIP_ERR_INVENTORY_FULL;
}

//////////////////////////////////////////////////////////////////////////
InventoryResult Player::CanStoreItems(Item** pItems, int count) const
{
    Item*    pItem2;

    // fill space table (统计行囊及各背包现有空闲槽位表)
    int inv_slot_items[INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START];
    int inv_bags[INVENTORY_SLOT_BAG_END - INVENTORY_SLOT_BAG_START][MAX_BAG_SIZE];
    int inv_keys[KEYRING_SLOT_END - KEYRING_SLOT_START];

    memset(inv_slot_items, 0, sizeof(int) * (INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START));
    memset(inv_bags, 0, sizeof(int) * (INVENTORY_SLOT_BAG_END - INVENTORY_SLOT_BAG_START)*MAX_BAG_SIZE);
    memset(inv_keys, 0, sizeof(int) * (KEYRING_SLOT_END - KEYRING_SLOT_START));

    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        pItem2 = GetItemByPos(INVENTORY_SLOT_BAG_0, i);

        if (pItem2 && !pItem2->IsInTrade())
        {
            inv_slot_items[i - INVENTORY_SLOT_ITEM_START] = pItem2->GetCount();
        }
    }

    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        pItem2 = GetItemByPos(INVENTORY_SLOT_BAG_0, i);

        if (pItem2 && !pItem2->IsInTrade())
        {
            inv_keys[i - KEYRING_SLOT_START] = pItem2->GetCount();
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                pItem2 = GetItemByPos(i, j);
                if (pItem2 && !pItem2->IsInTrade())
                {
                    inv_bags[i - INVENTORY_SLOT_BAG_START][j] = pItem2->GetCount();
                }
            }
        }
    }

    // check free space for all items (逐一校验待存入物品列表是否有足够空间)
    for (int k = 0; k < count; ++k)
    {
        Item*  pItem = pItems[k];

        // no item (跳过空物品指针)
        if (!pItem)  continue;

        DEBUG_LOG("STORAGE: CanStoreItems %i. item = %u, count = %u", k + 1, pItem->GetEntry(), pItem->GetCount());
        ItemPrototype const* pProto = pItem->GetProto();

        // strange item (找不到物品模板原型)
        if (!pProto)
            return EQUIP_ERR_ITEM_NOT_FOUND;

        // item used (物品正在使用中)
        if (pItem->HasTemporaryLoot())
            return EQUIP_ERR_ALREADY_LOOTED;

        // item it 'bind' (已灵魂绑定物品不可交易给他人)
        if (pItem->IsBindedNotWith(this))
            return EQUIP_ERR_DONT_OWN_THAT_ITEM;

        Bag* pBag;
        ItemPrototype const* pBagProto;

        // item is 'one item only' (唯一物品校验)
        InventoryResult res = CanTakeMoreSimilarItems(pItem);
        if (res != EQUIP_ERR_OK)
            return res;

        // search stack for merge to (在虚拟空间表中查找可合并堆叠)
        if (pProto->Stackable > 1)
        {
            bool b_found = false;

            for (int t = KEYRING_SLOT_START; t < KEYRING_SLOT_END; ++t)
            {
                pItem2 = GetItemByPos(INVENTORY_SLOT_BAG_0, t);
                if (pItem2 && pItem2->CanBeMergedPartlyWith(pProto) == EQUIP_ERR_OK && inv_keys[t - KEYRING_SLOT_START] + pItem->GetCount() <= pProto->GetMaxStackSize())
                {
                    inv_keys[t - KEYRING_SLOT_START] += pItem->GetCount();
                    b_found = true;
                    break;
                }
            }
            if (b_found) continue;

            for (int t = INVENTORY_SLOT_ITEM_START; t < INVENTORY_SLOT_ITEM_END; ++t)
            {
                pItem2 = GetItemByPos(INVENTORY_SLOT_BAG_0, t);
                if (pItem2 && pItem2->CanBeMergedPartlyWith(pProto) == EQUIP_ERR_OK && inv_slot_items[t - INVENTORY_SLOT_ITEM_START] + pItem->GetCount() <= pProto->GetMaxStackSize())
                {
                    inv_slot_items[t - INVENTORY_SLOT_ITEM_START] += pItem->GetCount();
                    b_found = true;
                    break;
                }
            }
            if (b_found) continue;

            for (int t = INVENTORY_SLOT_BAG_START; !b_found && t < INVENTORY_SLOT_BAG_END; ++t)
            {
                pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, t);
                if (pBag)
                {
                    for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    {
                        pItem2 = GetItemByPos(t, j);
                        if (pItem2 && pItem2->CanBeMergedPartlyWith(pProto) == EQUIP_ERR_OK && inv_bags[t - INVENTORY_SLOT_BAG_START][j] + pItem->GetCount() <= pProto->GetMaxStackSize())
                        {
                            inv_bags[t - INVENTORY_SLOT_BAG_START][j] += pItem->GetCount();
                            b_found = true;
                            break;
                        }
                    }
                }
            }
            if (b_found) continue;
        }

        // special bag case (优先尝试存入专用背包)
        if (pProto->BagFamily)
        {
            bool b_found = false;
            if (pProto->BagFamily & BAG_FAMILY_MASK_KEYS)
            {
                uint32 keyringSize = GetMaxKeyringSize();
                for (uint32 t = KEYRING_SLOT_START; t < KEYRING_SLOT_START + keyringSize; ++t)
                {
                    if (inv_keys[t - KEYRING_SLOT_START] == 0)
                    {
                        inv_keys[t - KEYRING_SLOT_START] = 1;
                        b_found = true;
                        break;
                    }
                }
            }

            if (b_found) continue;

            for (int t = INVENTORY_SLOT_BAG_START; !b_found && t < INVENTORY_SLOT_BAG_END; ++t)
            {
                pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, t);
                if (pBag)
                {
                    pBagProto = pBag->GetProto();

                    // not plain container check (非空背包不可存入其他背包)
                    if (pBagProto && (pBagProto->Class != ITEM_CLASS_CONTAINER || pBagProto->SubClass != ITEM_SUBCLASS_CONTAINER) &&
                            ItemCanGoIntoBag(pProto, pBagProto))
                    {
                        for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                        {
                            if (inv_bags[t - INVENTORY_SLOT_BAG_START][j] == 0)
                            {
                                inv_bags[t - INVENTORY_SLOT_BAG_START][j] = 1;
                                b_found = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (b_found) continue;
        }

        // search free slot (在行囊及普通背包中查找空闲槽位)
        bool b_found = false;
        for (int t = INVENTORY_SLOT_ITEM_START; t < INVENTORY_SLOT_ITEM_END; ++t)
        {
            if (inv_slot_items[t - INVENTORY_SLOT_ITEM_START] == 0)
            {
                inv_slot_items[t - INVENTORY_SLOT_ITEM_START] = 1;
                b_found = true;
                break;
            }
        }
        if (b_found) continue;

        // search free slot in bags (遍历各背包查找空位)
        for (int t = INVENTORY_SLOT_BAG_START; !b_found && t < INVENTORY_SLOT_BAG_END; ++t)
        {
            pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, t);
            if (pBag)
            {
                pBagProto = pBag->GetProto();

                // special bag already checked (专用背包此前已检查过)
                if (pBagProto && (pBagProto->Class != ITEM_CLASS_CONTAINER || pBagProto->SubClass != ITEM_SUBCLASS_CONTAINER))
                    continue;

                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    if (inv_bags[t - INVENTORY_SLOT_BAG_START][j] == 0)
                    {
                        inv_bags[t - INVENTORY_SLOT_BAG_START][j] = 1;
                        b_found = true;
                        break;
                    }
                }
            }
        }

        // no free slot found? (未找到空闲槽位则空间不足)
        if (!b_found)
            return EQUIP_ERR_BAG_FULL;
    }

    return EQUIP_ERR_OK;
}

//////////////////////////////////////////////////////////////////////////
InventoryResult Player::CanEquipNewItem(uint8 slot, uint16& dest, uint32 item, bool swap) const
{
    dest = 0;
    Item* pItem = Item::CreateItem(item, 1, this);
    if (pItem)
    {
        InventoryResult result = CanEquipItem(slot, dest, pItem, swap);
        delete pItem;
        return result;
    }

    return EQUIP_ERR_ITEM_NOT_FOUND;
}

/// 校验指定物品是否允许装备到目标装备槽位
InventoryResult Player::CanEquipItem(uint8 slot, uint16& dest, Item* pItem, bool swap, bool direct_action) const
{
    dest = 0;
    if (pItem)
    {
        DEBUG_LOG("STORAGE: CanEquipItem slot = %u, item = %u, count = %u", slot, pItem->GetEntry(), pItem->GetCount());
        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto)
        {
            // item used (物品正在使用中)
            if (pItem->HasTemporaryLoot())
                return EQUIP_ERR_ALREADY_LOOTED;

            if (pItem->IsBindedNotWith(this))
                return EQUIP_ERR_DONT_OWN_THAT_ITEM;

            // check count of items (skip for auto move for same player from bank) (校验唯一持有上限)
            InventoryResult res = CanTakeMoreSimilarItems(pItem);
            if (res != EQUIP_ERR_OK)
                return res;

            // check this only in game (仅在角色进入世界后执行战斗/状态限制检查)
            if (direct_action)
            {
                // May be here should be more stronger checks; STUNNED checked (后续可加强限制；目前已检查晕眩状态)
                // ROOT, CONFUSED, DISTRACTED, FLEEING this needs to be checked. (定身、迷惑、扰乱、恐惧等状态校验)
                if (IsStunned())
                    return EQUIP_ERR_YOU_ARE_STUNNED;

                // do not allow equipping gear except weapons, offhands, projectiles, relics in (除武器、副手、弹药、圣物外，以下场景禁止更换装备：)
                // - combat (- 战斗中)
                // - in-progress arenas (- 进行中的竞技场内)
                if (!pProto->CanChangeEquipStateInCombat())
                {
                    if (IsInCombat())
                        return EQUIP_ERR_NOT_IN_COMBAT;

                    if (BattleGround* bg = GetBattleGround())
                        if (bg->IsArena() && bg->GetStatus() == STATUS_IN_PROGRESS)
                            return EQUIP_ERR_NOT_DURING_ARENA_MATCH;
                }

                // prevent equip item in process logout (登出过程中禁止装备物品)
                if (GetSession()->isLogingOut())
                    return EQUIP_ERR_YOU_ARE_STUNNED;

                if (IsNonMeleeSpellCasted(false, false, false, false, false, true))
                    return EQUIP_ERR_CANT_DO_RIGHT_NOW;

                // prevent equip item in Spirit of Redemption (Aura: 27827) (救赎之魂状态下禁止更换装备)
                if (HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
                    return EQUIP_ERR_CANT_DO_RIGHT_NOW;
            }

            uint8 eslot = FindEquipSlot(pProto, slot, swap);
            if (eslot == NULL_SLOT)
                return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;

            InventoryResult msg = CanUseItem(pItem, direct_action);
            if (msg != EQUIP_ERR_OK)
                return msg;
            if (!swap && GetItemByPos(INVENTORY_SLOT_BAG_0, eslot))
                return EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE;

            // if swap ignore item (equipped also) (替换装备时忽略当前目标槽位已装备的唯一性冲突)
            if (InventoryResult res2 = CanEquipUniqueItem(pItem, swap ? eslot : uint8(NULL_SLOT)))
                return res2;

            // check unique-equipped special item classes (校验唯一装备及珠宝唯一装备限制)
            if (pProto->Class == ITEM_CLASS_QUIVER)
            {
                for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
                {
                    if (Item* pBag = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                    {
                        if (pBag != pItem)
                        {
                            if (ItemPrototype const* pBagProto = pBag->GetProto())
                            {
                                if (pBagProto->Class == pProto->Class && (!swap || pBag->GetSlot() != eslot))
                                    return (pBagProto->SubClass == ITEM_SUBCLASS_AMMO_POUCH)
                                           ? EQUIP_ERR_CAN_EQUIP_ONLY1_AMMOPOUCH
                                           : EQUIP_ERR_CAN_EQUIP_ONLY1_QUIVER;
                            }
                        }
                    }
                }
            }

            uint32 type = pProto->InventoryType;

            if (eslot == EQUIPMENT_SLOT_OFFHAND)
            {
                if (type == INVTYPE_WEAPON || type == INVTYPE_WEAPONOFFHAND)
                {
                    if (!CanDualWield())
                        return EQUIP_ERR_CANT_DUAL_WIELD;
                }
                else if (type == INVTYPE_2HWEAPON)
                {
                    return EQUIP_ERR_CANT_DUAL_WIELD;
                }

                if (IsTwoHandUsed())
                    return EQUIP_ERR_CANT_EQUIP_WITH_TWOHANDED;
            }

            // equip two-hand weapon case (with possible unequip 2 items) (装备双手武器时可能需要同时卸下主手和副手两件装备)
            if (type == INVTYPE_2HWEAPON)
            {
                if (eslot != EQUIPMENT_SLOT_MAINHAND)
                    return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;

                // offhand item must can be stored in inventory for offhand item and it also must be unequipped (副手物品必须能够退回背包且允许卸下)
                Item* offItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                ItemPosCountVec off_dest;
                uint8 bagSlot = 0;
                if (offItem && (!direct_action ||
                                CanUnequipItem(uint16(INVENTORY_SLOT_BAG_0) << 8 | EQUIPMENT_SLOT_OFFHAND, false) !=  EQUIP_ERR_OK ||
                                CanStoreItem(NULL_BAG, NULL_SLOT, off_dest, offItem, bagSlot, false) !=  EQUIP_ERR_OK))
                    return swap ? EQUIP_ERR_ITEMS_CANT_BE_SWAPPED : EQUIP_ERR_INVENTORY_FULL;
            }
            dest = ((INVENTORY_SLOT_BAG_0 << 8) | eslot);
            return EQUIP_ERR_OK;
        }
    }

    return !swap ? EQUIP_ERR_ITEM_NOT_FOUND : EQUIP_ERR_ITEMS_CANT_BE_SWAPPED;
}

InventoryResult Player::CanUnequipItem(uint16 pos, bool swap) const
{
    // Applied only to equipped items and bank bags (仅适用于已装备物品或银行背包栏)
    if (!IsEquipmentPos(pos) && !IsBagPos(pos))
        return EQUIP_ERR_OK;

    Item* pItem = GetItemByPos(pos);

    // Applied only to existing equipped item (仅适用于当前实际存在的已装备物品)
    if (!pItem)
        return EQUIP_ERR_OK;

    DEBUG_LOG("STORAGE: CanUnequipItem slot = %u, item = %u, count = %u", pos, pItem->GetEntry(), pItem->GetCount());

    ItemPrototype const* pProto = pItem->GetProto();
    if (!pProto)
        return EQUIP_ERR_ITEM_NOT_FOUND;

    // item used (物品正在使用中)
    if (pItem->HasTemporaryLoot())
        return EQUIP_ERR_ALREADY_LOOTED;

    // do not allow unequipping gear except weapons, offhands, projectiles, relics in (除武器、副手、弹药、圣物外，以下场景禁止卸下装备：)
    // - combat (- 战斗中)
    // - in-progress arenas (- 进行中的竞技场内)
    if (!pProto->CanChangeEquipStateInCombat())
    {
        if (IsInCombat())
            return EQUIP_ERR_NOT_IN_COMBAT;

        if (BattleGround* bg = GetBattleGround())
            if (bg->IsArena() && bg->GetStatus() == STATUS_IN_PROGRESS)
                return EQUIP_ERR_NOT_DURING_ARENA_MATCH;
    }

    // prevent unequip item in process logout (登出过程中禁止卸下装备)
    if (GetSession()->isLogingOut())
        return EQUIP_ERR_YOU_ARE_STUNNED;

    if (!swap && pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
        return EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS;

    return EQUIP_ERR_OK;
}

InventoryResult Player::CanBankItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap, uint8& bagSlot, bool not_loading) const
{
    if (!pItem)
        return swap ? EQUIP_ERR_ITEMS_CANT_BE_SWAPPED : EQUIP_ERR_ITEM_NOT_FOUND;

    uint32 count = pItem->GetCount();

    DEBUG_LOG("STORAGE: CanBankItem bag = %u, slot = %u, item = %u, count = %u", bag, slot, pItem->GetEntry(), pItem->GetCount());
    ItemPrototype const* pProto = pItem->GetProto();
    if (!pProto)
        return swap ? EQUIP_ERR_ITEMS_CANT_BE_SWAPPED : EQUIP_ERR_ITEM_NOT_FOUND;

    // item used (物品正在使用中)
    if (pItem->HasTemporaryLoot())
        return EQUIP_ERR_ALREADY_LOOTED;

    if (pItem->IsBindedNotWith(this))
        return EQUIP_ERR_DONT_OWN_THAT_ITEM;

    // check count of items (skip for auto move for same player from bank) (校验唯一持有上限)
    InventoryResult res = CanTakeMoreSimilarItems(pItem);
    if (res != EQUIP_ERR_OK)
        return res;

    // in specific slot (指定银行具体槽位)
    if (bag != NULL_BAG && slot != NULL_SLOT)
    {
        if (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END)
        {
            if (!pItem->IsBag())
                return EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT;

            if (slot - BANK_SLOT_BAG_START >= GetBankBagSlotCount())
                return EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT;

            res = CanUseItem(pItem, not_loading);
            if (res != EQUIP_ERR_OK)
                return res;
        }

        res = _CanStoreItem_InSpecificSlot(bag, slot, dest, pProto, count, swap, pItem, bagSlot);
        if (res != EQUIP_ERR_OK)
            return res;

        if (count == 0)
            return EQUIP_ERR_OK;
    }

    // not specific slot or have space for partly store only in specific slot (未指定槽位或仅部分存入)

    // in specific bag (指定银行背包)
    if (bag != NULL_BAG)
    {
        if (pProto->InventoryType == INVTYPE_BAG)
        {
            Bag* pBag = (Bag*)pItem;
            if (pBag && !pBag->IsEmpty())
                return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;
        }

        // search stack in bag for merge to (在指定银行背包中查找可叠加堆叠)
        if (pProto->Stackable > 1)
        {
            if (bag == INVENTORY_SLOT_BAG_0)
            {
                res = _CanStoreItem_InInventorySlots(BANK_SLOT_ITEM_START, BANK_SLOT_ITEM_END, dest, pProto, count, true, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                    return res;

                if (count == 0)
                    return EQUIP_ERR_OK;
            }
            else
            {
                res = _CanStoreItem_InBag(bag, dest, pProto, count, true, false, pItem, NULL_BAG, slot, bagSlot);
                if (res != EQUIP_ERR_OK)
                    res = _CanStoreItem_InBag(bag, dest, pProto, count, true, true, pItem, NULL_BAG, slot, bagSlot);

                if (res != EQUIP_ERR_OK)
                    return res;

                if (count == 0)
                    return EQUIP_ERR_OK;
            }
        }

        // search free slot in bag (在指定银行背包中查找空位)
        if (bag == INVENTORY_SLOT_BAG_0)
        {
            res = _CanStoreItem_InInventorySlots(BANK_SLOT_ITEM_START, BANK_SLOT_ITEM_END, dest, pProto, count, false, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
                return res;

            if (count == 0)
                return EQUIP_ERR_OK;
        }
        else
        {
            res = _CanStoreItem_InBag(bag, dest, pProto, count, false, false, pItem, NULL_BAG, slot, bagSlot);
            if (res != EQUIP_ERR_OK)
                res = _CanStoreItem_InBag(bag, dest, pProto, count, false, true, pItem, NULL_BAG, slot, bagSlot);

            if (res != EQUIP_ERR_OK)
                return res;

            if (count == 0)
                return EQUIP_ERR_OK;
        }
    }

    // not specific bag or have space for partly store only in specific bag (未指定银行背包或仅部分存入)

    // search stack for merge to (在银行主格与银行背包中查找可叠加堆叠)
    if (pProto->Stackable > 1)
    {
        // in slots (银行主格)
        res = _CanStoreItem_InInventorySlots(BANK_SLOT_ITEM_START, BANK_SLOT_ITEM_END, dest, pProto, count, true, pItem, bag, slot);
        if (res != EQUIP_ERR_OK)
            return res;

        if (count == 0)
            return EQUIP_ERR_OK;

        // in special bags (银行专用背包)
        if (pProto->BagFamily)
        {
            for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
            {
                res = _CanStoreItem_InBag(i, dest, pProto, count, true, false, pItem, bag, slot, bagSlot);
                if (res != EQUIP_ERR_OK)
                    continue;

                if (count == 0)
                    return EQUIP_ERR_OK;
            }
        }

        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            res = _CanStoreItem_InBag(i, dest, pProto, count, true, true, pItem, bag, slot, bagSlot);
            if (res != EQUIP_ERR_OK)
                continue;

            if (count == 0)
                return EQUIP_ERR_OK;
        }
    }

    // search free place in special bag (在银行专用背包中查找空位)
    if (pProto->BagFamily)
    {
        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            res = _CanStoreItem_InBag(i, dest, pProto, count, false, false, pItem, bag, slot, bagSlot);
            if (res != EQUIP_ERR_OK)
                continue;

            if (count == 0)
                return EQUIP_ERR_OK;
        }
    }

    // search free space (在银行主格与普通银行背包中查找空位)
    res = _CanStoreItem_InInventorySlots(BANK_SLOT_ITEM_START, BANK_SLOT_ITEM_END, dest, pProto, count, false, pItem, bag, slot);
    if (res != EQUIP_ERR_OK)
        return res;

    if (count == 0)
        return EQUIP_ERR_OK;

    for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
    {
        res = _CanStoreItem_InBag(i, dest, pProto, count, false, true, pItem, bag, slot, bagSlot);
        if (res != EQUIP_ERR_OK)
            continue;

        if (count == 0)
            return EQUIP_ERR_OK;
    }
    return EQUIP_ERR_BANK_FULL;
}

InventoryResult Player::CanUseItem(Item* pItem, bool direct_action) const
{
    if (pItem)
    {
        DEBUG_LOG("STORAGE: CanUseItem item = %u", pItem->GetEntry());

        if (!IsAlive() && direct_action)
            return EQUIP_ERR_YOU_ARE_DEAD;

        // if (isStunned())
        //    return EQUIP_ERR_YOU_ARE_STUNNED;

        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto)
        {
            if (pItem->IsBindedNotWith(this))
                return EQUIP_ERR_DONT_OWN_THAT_ITEM;

            InventoryResult msg = CanUseItem(pProto);
            if (msg != EQUIP_ERR_OK)
                return msg;

            if (uint32 skill = pItem->GetSkill())
            {
                // Fist weapons use unarmed skill calculations, but we must query fist weapon skill presence to use this item (拳套使用徒手技能计算，但装备使用仍需检查拳套技能)
                if (pProto->SubClass == ITEM_SUBCLASS_WEAPON_FIST)
                    skill = SKILL_FIST_WEAPONS;
                if (!GetSkillValue(skill))
                    return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
            }

            if (pProto->RequiredReputationFaction && uint32(GetReputationRank(pProto->RequiredReputationFaction)) < pProto->RequiredReputationRank)
                return EQUIP_ERR_CANT_EQUIP_REPUTATION;

            return EQUIP_ERR_OK;
        }
    }
    return EQUIP_ERR_ITEM_NOT_FOUND;
}

InventoryResult Player::CanUseItem(ItemPrototype const* pProto) const
{
    // Used by group, function NeedBeforeGreed, to know if a prototype can be used by a player (供队伍需求优先分配规则判断玩家是否可使用该物品)

    if (pProto)
    {
        if ((pProto->AllowableClass & getClassMask()) == 0 || (pProto->AllowableRace & getRaceMask()) == 0)
            return EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;

        if (pProto->RequiredSkill != 0)
        {
            if (GetSkillValue(pProto->RequiredSkill) == 0)
                return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
            if (GetSkillValue(pProto->RequiredSkill) < pProto->RequiredSkillRank)
                return EQUIP_ERR_CANT_EQUIP_SKILL;
        }

        if (pProto->RequiredSpell != 0 && !HasSpell(pProto->RequiredSpell))
            return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;

        if (GetLevel() < pProto->RequiredLevel)
            return EQUIP_ERR_CANT_EQUIP_LEVEL_I;

        return EQUIP_ERR_OK;
    }
    return EQUIP_ERR_ITEM_NOT_FOUND;
}

InventoryResult Player::CanUseAmmo(uint32 item) const
{
    DEBUG_LOG("STORAGE: CanUseAmmo item = %u", item);
    if (!IsAlive())
        return EQUIP_ERR_YOU_ARE_DEAD;
    // if( isStunned() )
    //    return EQUIP_ERR_YOU_ARE_STUNNED;
    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (pProto)
    {
        if (pProto->InventoryType != INVTYPE_AMMO)
            return EQUIP_ERR_ONLY_AMMO_CAN_GO_HERE;

        InventoryResult msg = CanUseItem(pProto);
        if (msg != EQUIP_ERR_OK)
            return msg;

        /*if ( GetReputationMgr().GetReputation() < pProto->RequiredReputation )
        return EQUIP_ERR_CANT_EQUIP_REPUTATION;
        */

        // Requires No Ammo (该武器类型无需弹药)
        if (GetDummyAura(46699))
            return EQUIP_ERR_BAG_FULL6;

        return EQUIP_ERR_OK;
    }
    return EQUIP_ERR_ITEM_NOT_FOUND;
}

void Player::SetAmmo(uint32 item)
{
    if (!item)
        return;

    // already set (已设置为当前弹药，无需重复应用)
    if (GetUInt32Value(PLAYER_AMMO_ID) == item)
        return;

    // check ammo (校验弹药可用性与数量)
    if (item)
    {
        InventoryResult msg = CanUseAmmo(item);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, nullptr, nullptr, item);
            return;
        }
    }

    SetUInt32Value(PLAYER_AMMO_ID, item);

    _ApplyAmmoBonuses();
}

void Player::RemoveAmmo()
{
    SetUInt32Value(PLAYER_AMMO_ID, 0);

    m_ammoDPSMin = 0.0f;
    m_ammoDPSMax = 0.0f;

    if (CanModifyStats())
        UpdateDamagePhysical(RANGED_ATTACK);
}

// Return stored item (if stored to stack, it can diff. from pItem). And pItem ca be deleted in this case. (返回最终存入的物品指针；若已合并至现有堆叠，原 pItem 可能被销毁)
Item* Player::StoreNewItem(ItemPosCountVec const& dest, uint32 item, bool update, int32 randomPropertyId)
{
    uint32 count = 0;
    for (auto itr : dest)
        count += itr.count;

    Item* pItem = Item::CreateItem(item, count, this, randomPropertyId);
    if (pItem)
    {
        ItemAddedQuestCheck(item, count);
        pItem = StoreItem(dest, pItem, update);
    }
    return pItem;
}

Item* Player::StoreItem(ItemPosCountVec const& dest, Item* pItem, bool update)
{
    if (!pItem)
        return nullptr;

    Item* lastItem = pItem;

    for (ItemPosCountVec::const_iterator itr = dest.begin(); itr != dest.end();)
    {
        uint16 pos = itr->pos;
        uint32 count = itr->count;

        ++itr;

        if (itr == dest.end())
        {
            lastItem = _StoreItem(pos, pItem, count, false, update);
            break;
        }

        lastItem = _StoreItem(pos, pItem, count, true, update);
    }

    return lastItem;
}

// Return stored item (if stored to stack, it can diff. from pItem). And pItem ca be deleted in this case. (返回最终存入的物品指针；若已合并堆叠则可能销毁原对象)
/// 内部核心存入实现：将物品放入或合并到指定背包槽位并触发入包法术与任务计数
Item* Player::_StoreItem(uint16 pos, Item* pItem, uint32 count, bool clone, bool update)
{
    if (!pItem)
        return nullptr;

    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;

    DEBUG_LOG("STORAGE: StoreItem bag = %u, slot = %u, item = %u, count = %u", bag, slot, pItem->GetEntry(), count);

    Item* pItem2 = GetItemByPos(bag, slot);

    if (!pItem2)
    {
        if (clone)
            pItem = pItem->CloneItem(count, this);
        else
            pItem->SetCount(count);

        if (!pItem)
            return nullptr;

        ItemPrototype const* itemProto = pItem->GetProto();
        if (itemProto->Bonding == BIND_WHEN_PICKED_UP
                || itemProto->Bonding == BIND_QUEST_ITEM
                || (itemProto->Bonding == BIND_WHEN_EQUIPPED && IsBagPos(pos)))
            pItem->SetBinding(true);

        if (bag == INVENTORY_SLOT_BAG_0)
        {
            m_items[slot] = pItem;
            SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), pItem->GetObjectGuid());
            pItem->SetGuidValue(ITEM_FIELD_CONTAINED, GetObjectGuid());
            pItem->SetGuidValue(ITEM_FIELD_OWNER, GetObjectGuid());

            pItem->SetSlot(slot);
            pItem->SetContainer(nullptr);

            if (IsInWorld() && update)
            {
                pItem->AddToWorld();
                GetMap()->AddUpdateCreateObject(pItem);
            }

            pItem->SetState(ITEM_CHANGED, this);
        }
        else if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag))
        {
            pBag->StoreItem(slot, pItem);
            if (IsInWorld() && update)
            {
                pItem->AddToWorld();
                GetMap()->AddUpdateCreateObject(pItem);
            }
            pItem->SetState(ITEM_CHANGED, this);
            pBag->SetState(ITEM_CHANGED, this);
        }

        AddEnchantmentDurations(pItem);
        AddItemDurations(pItem);
        sScriptDevAIMgr.OnItemLoot(this, pItem, true);

        // at place into not appropriate slot (bank, for example) remove aura (放入银行等非背包槽位时移除入包触发光环)
        ApplyItemOnStoreSpell(pItem, IsEquipmentPos(pItem->GetBagSlot(), pItem->GetSlot()) || IsInventoryPos(pItem->GetBagSlot(), pItem->GetSlot()));

        return pItem;
    }
    ItemPrototype const* itemProto = pItem2->GetProto();
    if (itemProto->Bonding == BIND_WHEN_PICKED_UP
        || itemProto->Bonding == BIND_QUEST_ITEM
        || (itemProto->Bonding == BIND_WHEN_EQUIPPED && IsBagPos(pos)))
        pItem2->SetBinding(true);

    pItem2->SetCount(pItem2->GetCount() + count);
    if (IsInWorld() && update)
    {
        GetMap()->AddUpdateObject(pItem2);
    }

    if (!clone)
    {
        // delete item (it not in any slot currently) (完整合并后删除已耗尽的源物品)
        if (IsInWorld() && update)
        {
            pItem->RemoveFromWorld();
            GetMap()->AddUpdateRemoveObject({ GetObjectGuid() }, pItem->GetObjectGuid());
        }

        RemoveEnchantmentDurations(pItem);
        RemoveItemDurations(pItem);

        pItem->SetOwnerGuid(GetObjectGuid());           // prevent error at next SetState in case trade/mail/buy from vendor (重置物品变更状态，避免交易/邮件/购买后续保存冲突)
        pItem->SetState(ITEM_REMOVED, this);
    }

    // AddItemDurations(pItem2); - pItem2 already have duration listed for player (目标堆叠物品已登记限时计时器)
    AddEnchantmentDurations(pItem2);

    pItem2->SetState(ITEM_CHANGED, this);

    return pItem2;
}

Item* Player::EquipNewItem(uint16 pos, uint32 item, bool update)
{
    if (Item* pItem = Item::CreateItem(item, 1, this))
    {
        ItemAddedQuestCheck(item, 1);
        return EquipItem(pos, pItem, update);
    }

    return nullptr;
}

/// 将物品装备到指定部位并应用属性、套装、外观及灵魂绑定
Item* Player::EquipItem(uint16 pos, Item* pItem, bool update)
{
    AddEnchantmentDurations(pItem);
    AddItemDurations(pItem);

    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;

    Item* pItem2 = GetItemByPos(bag, slot);
    if (!pItem2)
    {
        VisualizeItem(slot, pItem);

        if (IsAlive())
        {
            ItemPrototype const* pProto = pItem->GetProto();

            // item set bonuses applied only at equip and removed at unequip, and still active for broken items (套装效果仅在穿脱时增减，破损仍有效)
            if (pProto && pProto->ItemSet)
                AddItemsSetItem(this, pItem);

            _ApplyItemMods(pItem, slot, true);

            ApplyItemOnStoreSpell(pItem, true);

            // Weapons and also Totem/Relic/Sigil/etc (更新武器及圣物/图腾/魔印相关的攻击属性)
            if (pProto && IsInCombat() && (pProto->Class == ITEM_CLASS_WEAPON || pProto->InventoryType == INVTYPE_RELIC))
            {
                if (slot == SLOT_MAIN_HAND)
                    resetAttackTimer(BASE_ATTACK);
                else if (slot == SLOT_OFF_HAND)
                    resetAttackTimer(OFF_ATTACK);

                uint32 cooldownSpell = SPELL_ID_WEAPON_SWITCH_COOLDOWN_1_5s;

                if (getClass() == CLASS_ROGUE)
                    cooldownSpell = SPELL_ID_WEAPON_SWITCH_COOLDOWN_1_0s;

                SpellEntry const* spellProto = sSpellTemplate.LookupEntry<SpellEntry>(cooldownSpell);

                if (!spellProto)
                    sLog.outError("Weapon switch cooldown spell %u couldn't be found in Spell.dbc", cooldownSpell);
                else
                    AddGCD(*spellProto, 0, true);
            }
        }

        if (IsInWorld() && update)
        {
            pItem->AddToWorld();
            GetMap()->AddUpdateCreateObject(pItem);
        }

        ApplyEquipCooldown(pItem);

        if (slot == EQUIPMENT_SLOT_MAINHAND)
            UpdateWeaponDependantStats(BASE_ATTACK);
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
            UpdateWeaponDependantStats(OFF_ATTACK);
        else if (slot == EQUIPMENT_SLOT_RANGED)
            UpdateWeaponDependantStats(RANGED_ATTACK);
    }
    else
    {
        pItem2->SetCount(pItem2->GetCount() + pItem->GetCount());
        if (IsInWorld() && update)
        {
            GetMap()->AddUpdateObject(pItem2);
        }

        if (IsInWorld() && update)
        {
            pItem->RemoveFromWorld();
            GetMap()->AddUpdateRemoveObject({ GetObjectGuid() }, pItem->GetObjectGuid());
        }

        RemoveEnchantmentDurations(pItem);
        RemoveItemDurations(pItem);

        pItem->SetOwnerGuid(GetObjectGuid());               // prevent error at next SetState in case trade/mail/buy from vendor (重置物品保存状态)
        pItem->SetState(ITEM_REMOVED, this);
        pItem2->SetState(ITEM_CHANGED, this);

        ApplyEquipCooldown(pItem2);

        return pItem2;
    }

    return pItem;
}

void Player::QuickEquipItem(uint16 pos, Item* pItem)
{
    if (pItem)
    {
        AddEnchantmentDurations(pItem);
        AddItemDurations(pItem);
        ApplyItemOnStoreSpell(pItem, true);

        uint8 slot = pos & 255;
        VisualizeItem(slot, pItem);

        if (IsInWorld())
        {
            pItem->AddToWorld();
            GetMap()->AddUpdateCreateObject(pItem);
        }
    }
}

void Player::SetVisibleItemSlot(uint8 slot, Item* pItem)
{
    if (pItem)
    {
        SetGuidValue(PLAYER_VISIBLE_ITEM_1_CREATOR + (slot * MAX_VISIBLE_ITEM_OFFSET), pItem->GetGuidValue(ITEM_FIELD_CREATOR));

        int VisibleBase = PLAYER_VISIBLE_ITEM_1_0 + (slot * MAX_VISIBLE_ITEM_OFFSET);
        SetUInt32Value(VisibleBase + 0, pItem->GetEntry());

        for (int i = 0; i < MAX_INSPECTED_ENCHANTMENT_SLOT; ++i)
            SetUInt32Value(VisibleBase + 1 + i, pItem->GetEnchantmentId(EnchantmentSlot(i)));

        // Use SetInt16Value to prevent set high part to FFFF for negative value (使用 SetInt16Value 防止负数设置污染高位)
        SetInt16Value(PLAYER_VISIBLE_ITEM_1_PROPERTIES + (slot * MAX_VISIBLE_ITEM_OFFSET), 0, pItem->GetItemRandomPropertyId());
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_PROPERTIES + 1 + (slot * MAX_VISIBLE_ITEM_OFFSET), pItem->GetItemSuffixFactor());
    }
    else
    {
        SetGuidValue(PLAYER_VISIBLE_ITEM_1_CREATOR + (slot * MAX_VISIBLE_ITEM_OFFSET), ObjectGuid());

        int VisibleBase = PLAYER_VISIBLE_ITEM_1_0 + (slot * MAX_VISIBLE_ITEM_OFFSET);
        SetUInt32Value(VisibleBase + 0, 0);

        for (int i = 0; i < MAX_INSPECTED_ENCHANTMENT_SLOT; ++i)
            SetUInt32Value(VisibleBase + 1 + i, 0);

        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_PROPERTIES + 0 + (slot * MAX_VISIBLE_ITEM_OFFSET), 0);
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_PROPERTIES + 1 + (slot * MAX_VISIBLE_ITEM_OFFSET), 0);
    }
}

void Player::VisualizeItem(uint8 slot, Item* pItem)
{
    if (!pItem)
        return;

    // check also  BIND_WHEN_PICKED_UP and BIND_QUEST_ITEM for .additem or .additemset case by GM (not binded at adding to inventory) (装备时自动处理拾取绑定/装备绑定/任务物品绑定)
    ItemPrototype const* itemProto = pItem->GetProto();
    if (itemProto->Bonding == BIND_WHEN_EQUIPPED || itemProto->Bonding == BIND_WHEN_PICKED_UP || itemProto->Bonding == BIND_QUEST_ITEM)
        pItem->SetBinding(true);

    DEBUG_LOG("STORAGE: EquipItem slot = %u, item = %u", slot, pItem->GetEntry());

    m_items[slot] = pItem;
    SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), pItem->GetObjectGuid());
    pItem->SetGuidValue(ITEM_FIELD_CONTAINED, GetObjectGuid());
    pItem->SetGuidValue(ITEM_FIELD_OWNER, GetObjectGuid());
    pItem->SetSlot(slot);
    pItem->SetContainer(nullptr);

    if (slot < EQUIPMENT_SLOT_END)
        SetVisibleItemSlot(slot, pItem);

    pItem->SetState(ITEM_CHANGED, this);
}

/// 从指定背包或装备槽位摘除物品并撤销装备加成
void Player::RemoveItem(uint8 bag, uint8 slot, bool update)
{
    // note: removeitem does not actually change the item (注：RemoveItem 不会销毁或修改物品本身，)
    // it only takes the item out of storage temporarily (仅临时将其从存储槽位中移出)
    // note2: if removeitem is to be used for delinking (注2：若用于彻底移除，)
    // the item must be removed from the player's updatequeue (须随后将其从更新队列中移除)

    if (Item* pItem = GetItemByPos(bag, slot))
    {
        DEBUG_LOG("STORAGE: RemoveItem bag = %u, slot = %u, item = %u", bag, slot, pItem->GetEntry());

        RemoveEnchantmentDurations(pItem);
        RemoveItemDurations(pItem);

        if (bag == INVENTORY_SLOT_BAG_0)
        {
            if (slot < INVENTORY_SLOT_BAG_END)
            {
                ItemPrototype const* pProto = pItem->GetProto();
                // item set bonuses applied only at equip and removed at unequip, and still active for broken items (卸下时移除套装加成)

                if (pProto && pProto->ItemSet)
                    RemoveItemsSetItem(this, pProto);

                _ApplyItemMods(pItem, slot, false);

                // remove item dependent auras and casts (only weapon and armor slots) (移除依赖该装备的光环与施法)
                if (slot < EQUIPMENT_SLOT_END)
                {
                    RemoveItemDependentAurasAndCasts(pItem);

                    // remove held enchantments, update expertise (移除武器附魔并刷新精准等属性)
                    if (slot == EQUIPMENT_SLOT_MAINHAND)
                    {
                        for (uint32 i = PERM_ENCHANTMENT_SLOT; i <= TEMP_ENCHANTMENT_SLOT; ++i)
                            if (pItem->IsMainHandOnlyEnchant(EnchantmentSlot(i)))
                                pItem->ClearEnchantment(EnchantmentSlot(i));

                        if (pItem->GetItemSuffixFactor())
                        {
                            pItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_3);
                            pItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_4);
                        }
                        else
                        {
                            pItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_0);
                            pItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_1);
                        }

                        UpdateWeaponDependantStats(BASE_ATTACK);
                    }
                    else if (slot == EQUIPMENT_SLOT_OFFHAND)
                        UpdateWeaponDependantStats(OFF_ATTACK);
                }
            }

            m_items[slot] = nullptr;
            SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), ObjectGuid());

            if (slot < EQUIPMENT_SLOT_END)
                SetVisibleItemSlot(slot, nullptr);
        }
        else
        {
            Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
            if (pBag)
                pBag->RemoveItem(slot);
        }
        pItem->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
        // pItem->SetGuidValue(ITEM_FIELD_OWNER, ObjectGuid()); not clear owner at remove (it will be set at store). This used in mail and auction code (移出时不清除物主 GUID，供邮件与拍卖行逻辑复用)
        pItem->SetSlot(NULL_SLOT);

        // ApplyItemOnStoreSpell, for avoid re-apply will remove at _adding_ to not appropriate slot (入包法术不在此处移除，避免槽位移动时误重置)

        if (IsInWorld() && update)
        {
            GetMap()->AddUpdateCreateObject(pItem);
        }
    }
}

// Common operation need to remove item from inventory without delete in trade, auction, guild bank, mail.... (通用操作：从背包移出物品但不销毁，用于交易/拍卖/公会银行/邮件等)
void Player::MoveItemFromInventory(uint8 bag, uint8 slot, bool update)
{
    if (Item* it = GetItemByPos(bag, slot))
    {
        ItemRemovedQuestCheck(it->GetEntry(), it->GetCount());
        RemoveItem(bag, slot, update);

        // item atStore spell not removed in RemoveItem (for avoid reappaly in slots changes), so do it directly (离包后显式移除入包触发法术)
        if (IsEquipmentPos(bag, slot) || IsInventoryPos(bag, slot))
            ApplyItemOnStoreSpell(it, false);

        it->RemoveFromUpdateQueueOf(this);
        if (it->IsInWorld())
        {
            it->RemoveFromWorld();
            GetMap()->AddUpdateRemoveObject({ GetObjectGuid() }, it->GetObjectGuid());
        }
    }
}

// Common operation need to add item from inventory without delete in trade, guild bank, mail.... (通用操作：将外部已有物品存入背包，用于交易/公会银行/邮件等)
void Player::MoveItemToInventory(ItemPosCountVec const& dest, Item* pItem, bool update, bool in_characterInventoryDB)
{
    // update quest counters (更新任务物品收集计数)
    ItemAddedQuestCheck(pItem->GetEntry(), pItem->GetCount());

    // store item (存入指定槽位)
    Item* pLastItem = StoreItem(dest, pItem, update);

    // only set if not merged to existing stack (pItem can be deleted already but we can compare pointers any way) (仅当未完全并入旧堆叠时更新物品归属与状态)
    if (pLastItem == pItem)
    {
        // update owner for last item (this can be original item with wrong owner (更新物主 GUID)
        if (pLastItem->GetOwnerGuid() != GetObjectGuid())
            pLastItem->SetOwnerGuid(GetObjectGuid());

        // if this original item then it need create record in inventory (若首次入包则标记为新建并加入背包更新队列)
        // in case trade we already have item in other player inventory (交易转移的物品原在他人背包中，标记为 CHANGED)
        pLastItem->SetState(in_characterInventoryDB ? ITEM_CHANGED : ITEM_NEW, this);
    }
}

/// 彻底销毁指定槽位的物品（含非空背包内物品递归销毁）
void Player::DestroyItem(uint8 bag, uint8 slot, bool update)
{
    Item* pItem = GetItemByPos(bag, slot);
    if (pItem)
    {
        DEBUG_LOG("STORAGE: DestroyItem bag = %u, slot = %u, item = %u", bag, slot, pItem->GetEntry());

        // start from destroy contained items (only equipped bag can have its) (若销毁背包则先递归销毁其内部所有物品，)
        if (pItem->IsBag() && pItem->IsEquipped())          // this also prevent infinity loop if empty bag stored in bag==slot (同时防止空包自引导致死循环)
        {
            for (int i = 0; i < MAX_BAG_SIZE; ++i)
                DestroyItem(slot, i, update);
        }

        if (pItem->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
        {
            static SqlStatementID delGifts ;

            SqlStatement stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM character_gifts WHERE item_guid = ?");
            stmt.PExecute(pItem->GetGUIDLow());
        }

        RemoveEnchantmentDurations(pItem);
        RemoveItemDurations(pItem);

        sScriptDevAIMgr.OnItemLoot(this, pItem, false);

        if (IsEquipmentPos(bag, slot) || IsInventoryPos(bag, slot))
            ApplyItemOnStoreSpell(pItem, false);

        ItemRemovedQuestCheck(pItem->GetEntry(), pItem->GetCount());

        if (bag == INVENTORY_SLOT_BAG_0)
        {
            SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), ObjectGuid());

            // equipment and equipped bags can have applied bonuses (装备栏与背包栏物品销毁前移除属性加成)
            if (slot < INVENTORY_SLOT_BAG_END)
            {
                ItemPrototype const* pProto = pItem->GetProto();

                // item set bonuses applied only at equip and removed at unequip, and still active for broken items (移除套装加成)
                if (pProto && pProto->ItemSet)
                    RemoveItemsSetItem(this, pProto);

                _ApplyItemMods(pItem, slot, false);
            }

            if (slot < EQUIPMENT_SLOT_END)
            {
                // remove item dependent auras and casts (only weapon and armor slots) (移除依赖该物品的光环与施法)
                RemoveItemDependentAurasAndCasts(pItem);

                // update weapon dependant stats (刷新武器相关属性)
                if (slot == EQUIPMENT_SLOT_MAINHAND)
                    UpdateWeaponDependantStats(BASE_ATTACK);
                else if (slot == EQUIPMENT_SLOT_OFFHAND)
                    UpdateWeaponDependantStats(OFF_ATTACK);
                else if (slot == EQUIPMENT_SLOT_RANGED)
                    UpdateWeaponDependantStats(RANGED_ATTACK);

                // equipment visual show (更新角色外观显示)
                SetVisibleItemSlot(slot, nullptr);
            }

            m_items[slot] = nullptr;
        }
        else if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag))
            pBag->RemoveItem(slot);

        if (IsInWorld() && update)
        {
            pItem->RemoveFromWorld();
            GetMap()->AddUpdateRemoveObject({ GetObjectGuid() }, pItem->GetObjectGuid());
        }

        // pItem->SetOwnerGUID(0);
        pItem->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
        pItem->SetSlot(NULL_SLOT);
        pItem->SetState(ITEM_REMOVED, this);
    }
}

void Player::DestroyItemCount(uint32 itemEntry, uint32 count, bool update, bool unequip_check, bool inBankAlso)
{
    DEBUG_LOG("STORAGE: DestroyItemCount item = %u, count = %u", itemEntry, count);

    // in inventory (遍历主行囊)
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (item->GetEntry() == itemEntry && !item->IsInTrade())
            {
                DestroyItemCount(*item, count, update);
                if (count == 0)
                    return;
            }
        }
    }

    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (item->GetEntry() == itemEntry && !item->IsInTrade())
            {
                DestroyItemCount(*item, count, update);
                if (count == 0)
                    return;
            }
        }
    }

    // in inventory bags (遍历已装备背包)
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* bag = dynamic_cast<Bag*>(GetItemByPos(INVENTORY_SLOT_BAG_0, i)))
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                if (Item* item = bag->GetItemByPos(j))
                {
                    if (item->GetEntry() == itemEntry && !item->IsInTrade())
                    {
                        DestroyItemCount(*item, count, update);
                        if (count == 0)
                            return;
                    }
                }
            }
        }
    }

    // in equipment and bag list (遍历装备栏与背包栏自身)
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (item && item->GetEntry() == itemEntry && !item->IsInTrade())
            {
                if (!unequip_check || item->GetCount() > count || CanUnequipItem(INVENTORY_SLOT_BAG_0 << 8 | i, false) == EQUIP_ERR_OK)
                {
                    DestroyItemCount(*item, count, update);
                    if (count == 0)
                        return;
                }
            }
        }
    }

    if (inBankAlso) // Remove items from bank as well (按需同时从银行中移除)
    {
        for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetEntry() == itemEntry && !item->IsInTrade())
            {
                DestroyItemCount(*item, count, update);
                if (count == 0)
                    return;
            }
        }

        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    Item* item = pBag->GetItemByPos(j);
                    if (item && item->GetEntry() == itemEntry && !item->IsInTrade())
                    {
                        DestroyItemCount(*item, count, update);
                        if (count == 0)
                            return;
                    }
                }
            }
        }
    }
}

void Player::DestroyZoneLimitedItem(bool update, uint32 new_zone)
{
    DEBUG_LOG("STORAGE: DestroyZoneLimitedItem in map %u and area %u", GetMapId(), new_zone);

    // in inventory (主行囊内区域限定物品)
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

    // in inventory bags (背包内区域限定物品)
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* pItem = pBag->GetItemByPos(j))
                    if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
                        DestroyItem(i, j, update);

    // in equipment and bag list (装备栏与背包栏区域限定物品)
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
}

void Player::DestroyConjuredItems(bool update)
{
    // used when entering arena (进入竞技场时调用：)
    // destroys all conjured items (销毁所有魔法制造的临时物品)
    DEBUG_LOG("STORAGE: DestroyConjuredItems");

    // in inventory (主行囊内魔法制造物品)
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->IsConjuredConsumable())
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

    // in inventory bags (背包内魔法制造物品)
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* pItem = pBag->GetItemByPos(j))
                    if (pItem->IsConjuredConsumable())
                        DestroyItem(i, j, update);

    // in equipment and bag list (装备栏与背包栏魔法制造物品)
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->IsConjuredConsumable())
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
}

void Player::DestroyItemCount(Item& item, uint32& count, bool update)
{
    DEBUG_LOG("STORAGE: DestroyItemCount item (GUID: %u, Entry: %u) count = %u", item.GetGUIDLow(), item.GetEntry(), count);

    if (item.GetCount() <= count)
    {
        count -= item.GetCount();

        DestroyItem(item.GetBagSlot(), item.GetSlot(), update);
    }
    else
    {
        ItemRemovedQuestCheck(item.GetEntry(), count);
        item.SetCount(item.GetCount() - count);
        count = 0;
        if (IsInWorld() && update)
            GetMap()->AddUpdateObject(&item);
        item.SetState(ITEM_CHANGED, this);
    }
}

void Player::SplitItem(uint16 src, uint16 dst, uint32 count)
{
    uint8 srcbag = src >> 8;
    uint8 srcslot = src & 255;

    uint8 dstbag = dst >> 8;
    uint8 dstslot = dst & 255;

    Item* pSrcItem = GetItemByPos(srcbag, srcslot);
    if (!pSrcItem)
    {
        SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pSrcItem, nullptr);
        return;
    }

    if (pSrcItem->HasGeneratedLoot())                       // prevent split looting item (stackable items can has only temporary loot and this meaning that loot window open) (禁止拆分正在开启拾取的容器物品)
    {
        // best error message found for attempting to split while looting (返回最佳匹配的错误提示)
        SendEquipError(EQUIP_ERR_COULDNT_SPLIT_ITEMS, pSrcItem, nullptr);
        return;
    }

    // not let split all items (can be only at cheating) (禁止一次拆出全部数量，防作弊)
    if (pSrcItem->GetCount() == count)
    {
        SendEquipError(EQUIP_ERR_COULDNT_SPLIT_ITEMS, pSrcItem, nullptr);
        return;
    }

    // not let split more existing items (can be only at cheating) (禁止拆出超过现有数量，防作弊)
    if (pSrcItem->GetCount() < count)
    {
        SendEquipError(EQUIP_ERR_TRIED_TO_SPLIT_MORE_THAN_COUNT, pSrcItem, nullptr);
        return;
    }

    DEBUG_LOG("STORAGE: SplitItem bag = %u, slot = %u, item = %u, count = %u", dstbag, dstslot, pSrcItem->GetEntry(), count);
    Item* pNewItem = pSrcItem->CloneItem(count, this);
    if (!pNewItem)
    {
        SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pSrcItem, nullptr);
        return;
    }

    if (IsInventoryPos(dst))
    {
        // change item amount before check (for unique max count check) (检查前先临时扣减源堆叠数量以通过唯一数量校验)
        pSrcItem->SetCount(pSrcItem->GetCount() - count);

        ItemPosCountVec dest;
        uint8 bagSlot = 0;
        InventoryResult msg = CanStoreItem(dstbag, dstslot, dest, pNewItem, bagSlot, false);
        if (msg != EQUIP_ERR_OK)
        {
            delete pNewItem;
            pSrcItem->SetCount(pSrcItem->GetCount() + count);
            SendEquipError(msg, pSrcItem, nullptr, bagSlot);
            return;
        }

        if (IsInWorld())
            GetMap()->AddUpdateObject(pSrcItem);
        pSrcItem->SetState(ITEM_CHANGED, this);
        StoreItem(dest, pNewItem, true);
    }
    else if (IsBankPos(dst))
    {
        // change item amount before check (for unique max count check) (同上：放入目标空位前临时扣减数量)
        pSrcItem->SetCount(pSrcItem->GetCount() - count);

        ItemPosCountVec dest;
        uint8 bagSlot;
        InventoryResult msg = CanBankItem(dstbag, dstslot, dest, pNewItem, false, bagSlot);
        if (msg != EQUIP_ERR_OK)
        {
            delete pNewItem;
            pSrcItem->SetCount(pSrcItem->GetCount() + count);
            SendEquipError(msg, pSrcItem, nullptr, bagSlot);
            return;
        }

        if (IsInWorld())
            GetMap()->AddUpdateObject(pSrcItem);
        pSrcItem->SetState(ITEM_CHANGED, this);
        BankItem(dest, pNewItem, true);
    }
    else if (IsEquipmentPos(dst))
    {
        // change item amount before check (for unique max count check), provide space for splitted items (临时扣减源堆叠数量并为拆分出的物品腾出校验空间)
        pSrcItem->SetCount(pSrcItem->GetCount() - count);

        uint16 dest;
        InventoryResult msg = CanEquipItem(dstslot, dest, pNewItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            delete pNewItem;
            pSrcItem->SetCount(pSrcItem->GetCount() + count);
            SendEquipError(msg, pSrcItem, nullptr);
            return;
        }

        if (IsInWorld())
            GetMap()->AddUpdateObject(pSrcItem);
        pSrcItem->SetState(ITEM_CHANGED, this);
        EquipItem(dest, pNewItem, true);
        AutoUnequipOffhandIfNeed();
    }
}

/// 交换或移动两个槽位的物品（支持堆叠合并与背包整体互换）
void Player::SwapItem(uint16 src, uint16 dst)
{
    uint8 srcbag = src >> 8;
    uint8 srcslot = src & 255;

    uint8 dstbag = dst >> 8;
    uint8 dstslot = dst & 255;

    Item* pSrcItem = GetItemByPos(srcbag, srcslot);
    Item* pDstItem = GetItemByPos(dstbag, dstslot);

    if (!pSrcItem)
        return;

    DEBUG_LOG("STORAGE: SwapItem bag = %u, slot = %u, item = %u", dstbag, dstslot, pSrcItem->GetEntry());

    if (!IsAlive())
    {
        SendEquipError(EQUIP_ERR_YOU_ARE_DEAD, pSrcItem, pDstItem);
        return;
    }

    // SRC checks (源槽位校验)

    // check unequip potability for equipped items and bank bags (若源物品为已装备物品或银行背包，检查是否允许卸下)
    if (IsEquipmentPos(src) || IsBagPos(src))
    {
        // bags can be swapped with empty bag slots, or with empty bag (items move possibility checked later) (非空背包不可直接放入普通格子，背包间互换在后续校验)
        InventoryResult msg = CanUnequipItem(src, !IsBagPos(src) || IsBagPos(dst) || (pDstItem && pDstItem->IsBag() && ((Bag*)pDstItem)->IsEmpty()));
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, pSrcItem, pDstItem);
            return;
        }
    }

    // prevent put equipped/bank bag in self (禁止将背包放入其自身内部)
    if (IsBagPos(src) && srcslot == dstbag)
    {
        SendEquipError(EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, pSrcItem, pDstItem);
        return;
    }

    // prevent put equipped/bank bag in self (禁止将背包放入其自身内部)
    if (IsBagPos(dst) && dstslot == srcbag)
    {
        SendEquipError(EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, pDstItem, pSrcItem);
        return;
    }

    // DST checks (目标槽位校验)

    if (pDstItem)
    {
        // check unequip potability for equipped items and bank bags (若目标为已装备物品或银行背包，检查是否允许卸下)
        if (IsEquipmentPos(dst) || IsBagPos(dst))
        {
            // bags can be swapped with empty bag slots, or with empty bag (items move possibility checked later) (非空背包互换在后续处理)
            InventoryResult msg = CanUnequipItem(dst, !IsBagPos(dst) || IsBagPos(src) || (pSrcItem->IsBag() && ((Bag*)pSrcItem)->IsEmpty()));
            if (msg != EQUIP_ERR_OK)
            {
                SendEquipError(msg, pSrcItem, pDstItem);
                return;
            }
        }
    }

    // NOW this is or item move (swap with empty), or swap with another item (including bags in bag possitions) (执行移动到空位、与另一物品互换或背包互换)
    // or swap empty bag with another empty or not empty bag (with items exchange) (包括空包与非空包互换并迁移内部物品)

    // Move case (目标格为空的移动分支)
    if (!pDstItem)
    {
        if (IsInventoryPos(dst))
        {
            ItemPosCountVec dest;
            uint8 bagSlot = 0;
            InventoryResult msg = CanStoreItem(dstbag, dstslot, dest, pSrcItem, bagSlot, false);
            if (msg != EQUIP_ERR_OK)
            {
                SendEquipError(msg, pSrcItem, nullptr, bagSlot);
                return;
            }

            RemoveItem(srcbag, srcslot, true);
            StoreItem(dest, pSrcItem, true);
        }
        else if (IsBankPos(dst))
        {
            ItemPosCountVec dest;
            uint8 bagSlot;
            InventoryResult msg = CanBankItem(dstbag, dstslot, dest, pSrcItem, false, bagSlot);
            if (msg != EQUIP_ERR_OK)
            {
                SendEquipError(msg, pSrcItem, nullptr, bagSlot);
                return;
            }

            RemoveItem(srcbag, srcslot, true);
            BankItem(dest, pSrcItem, true);
        }
        else if (IsEquipmentPos(dst))
        {
            uint16 dest;
            InventoryResult msg = CanEquipItem(dstslot, dest, pSrcItem, false);
            if (msg != EQUIP_ERR_OK)
            {
                SendEquipError(msg, pSrcItem, nullptr);
                return;
            }

            RemoveItem(srcbag, srcslot, true);
            EquipItem(dest, pSrcItem, true);
            AutoUnequipOffhandIfNeed();
        }

        return;
    }

    // attempt merge to / fill target item (尝试将源物品合并/补满至目标同类堆叠)
    if (!pSrcItem->IsBag() && !pDstItem->IsBag())
    {
        ItemPrototype const* itemProto = pSrcItem->GetProto();
        InventoryResult msg;
        ItemPosCountVec sDest;
        uint8 bagSlot;
        // only fill the target stack: the rest stays in the source slot, so no free slot is needed elsewhere. An equipped stack (仅补满目标堆叠，多余数量留在源槽位；)
        // is always filled this way: it stays in its slot and only its count changes (已装备的堆叠如投掷武器始终原地增减数量)
        if (IsEquipmentPos(dst) || ((IsInventoryPos(dst) || IsBankPos(dst)) && pSrcItem->GetCount() + pDstItem->GetCount() > itemProto->GetMaxStackSize()))
        {
            if (pSrcItem->HasTemporaryLoot())
                msg = EQUIP_ERR_ALREADY_LOOTED;
            else if (pSrcItem->IsBindedNotWith(this))
                msg = EQUIP_ERR_DONT_OWN_THAT_ITEM;
            else
                msg = pDstItem->CanBeMergedPartlyWith(itemProto);

            // the whole source joins the equipped stack when it fits (当源数量可完全并入已装备堆叠时直接合并)
            if (IsEquipmentPos(dst))
                sDest.push_back(ItemPosCount(dst, pSrcItem->GetCount()));
        }
        else if (IsInventoryPos(dst))
            msg = CanStoreItem(dstbag, dstslot, sDest, pSrcItem, bagSlot, false);
        else if (IsBankPos(dst))
            msg = CanBankItem(dstbag, dstslot, sDest, pSrcItem, false, bagSlot);
        else
            return;

        // can be merge/fill (执行合并与数量更新)
        if (msg == EQUIP_ERR_OK)
        {
            if (pSrcItem->GetCount() + pDstItem->GetCount() <= itemProto->GetMaxStackSize())
            {
                RemoveItem(srcbag, srcslot, true);

                if (IsBankPos(dst))
                    BankItem(sDest, pSrcItem, true);
                else
                    StoreItem(sDest, pSrcItem, true);
            }
            else
            {
                pSrcItem->SetCount(pSrcItem->GetCount() + pDstItem->GetCount() - itemProto->GetMaxStackSize());
                pDstItem->SetCount(itemProto->GetMaxStackSize());
                pSrcItem->SetState(ITEM_CHANGED, this);
                pDstItem->SetState(ITEM_CHANGED, this);
                if (IsInWorld())
                {
                    GetMap()->AddUpdateObject(pSrcItem);
                    GetMap()->AddUpdateObject(pDstItem);
                }
            }
            return;
        }
    }

    // impossible merge/fill, do real swap (无法合并，执行两格真正互换)
    InventoryResult msg = EQUIP_ERR_CANT_DO_RIGHT_NOW;

    // check src->dest move possibility (校验源物品放入目标槽位的合法性)
    ItemPosCountVec sDest;
    uint16 eDest = 0;
    uint8 bagSlot = 0;
    if (IsInventoryPos(dst))
        msg = CanStoreItem(dstbag, dstslot, sDest, pSrcItem, bagSlot, true);
    else if (IsBankPos(dst))
        msg = CanBankItem(dstbag, dstslot, sDest, pSrcItem, true, bagSlot);
    else if (IsEquipmentPos(dst))
    {
        msg = CanEquipItem(dstslot, eDest, pSrcItem, true);
        if (msg == EQUIP_ERR_OK)
            msg = CanUnequipItem(eDest, true);
    }

    if (msg != EQUIP_ERR_OK)
    {
        SendEquipError(msg, pSrcItem, pDstItem, bagSlot);
        return;
    }

    // check dest->src move possibility (校验目标物品放入源槽位的合法性)
    ItemPosCountVec sDest2;
    uint16 eDest2 = 0;
    uint8 bagSlot2 = 0;
    if (IsInventoryPos(src))
        msg = CanStoreItem(srcbag, srcslot, sDest2, pDstItem, bagSlot2, true);
    else if (IsBankPos(src))
        msg = CanBankItem(srcbag, srcslot, sDest2, pDstItem, true, bagSlot2);
    else if (IsEquipmentPos(src))
    {
        msg = CanEquipItem(srcslot, eDest2, pDstItem, true);
        if (msg == EQUIP_ERR_OK)
            msg = CanUnequipItem(eDest2, true);
    }

    if (msg != EQUIP_ERR_OK)
    {
        SendEquipError(msg, pDstItem, pSrcItem, bagSlot2);
        return;
    }

    // Check bag swap with item exchange (one from empty in not bag possition (equipped (not possible in fact) or store) (非空背包互换与内部物品迁移空间校验)
    if (pSrcItem->IsBag() && pDstItem->IsBag())
    {
        Bag* emptyBag = nullptr;
        Bag* fullBag = nullptr;
        if (((Bag*)pSrcItem)->IsEmpty() && !IsBagPos(src))
        {
            emptyBag = (Bag*)pSrcItem;
            fullBag  = (Bag*)pDstItem;
        }
        else if (((Bag*)pDstItem)->IsEmpty() && !IsBagPos(dst))
        {
            emptyBag = (Bag*)pDstItem;
            fullBag  = (Bag*)pSrcItem;
        }

        // bag swap (with items exchange) case (执行两个背包槽位互换及内部物品转移)
        if (emptyBag && fullBag)
        {
            ItemPrototype const* emotyProto = emptyBag->GetProto();

            uint32 count = 0;

            for (uint32 i = 0; i < fullBag->GetBagSize(); ++i)
            {
                Item* bagItem = fullBag->GetItemByPos(i);
                if (!bagItem)
                    continue;

                ItemPrototype const* bagItemProto = bagItem->GetProto();
                if (!bagItemProto || !ItemCanGoIntoBag(bagItemProto, emotyProto))
                {
                    // one from items not go to empty target bag (存在不适配目标新背包类型的物品，如箭袋专属物品)
                    SendEquipError(EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, pSrcItem, pDstItem);
                    return;
                }

                ++count;
            }

            if (count > emptyBag->GetBagSize())
            {
                // too small targeted bag (目标背包容量不足以容纳原包内物品)
                SendEquipError(EQUIP_ERR_ITEMS_CANT_BE_SWAPPED, pSrcItem, pDstItem);
                return;
            }

            // Items swap (执行物品位置与包内物品交换)
            count = 0;                                      // will pos in new bag (迁入新背包的对应槽位)
            for (uint32 i = 0; i < fullBag->GetBagSize(); ++i)
            {
                Item* bagItem = fullBag->GetItemByPos(i);
                if (!bagItem)
                    continue;

                fullBag->RemoveItem(i);
                emptyBag->StoreItem(count, bagItem);
                bagItem->SetState(ITEM_CHANGED, this);

                ++count;
            }
        }
    }

    // now do moves, remove... (先从原槽位摘除两个物品)
    RemoveItem(dstbag, dstslot, false);
    RemoveItem(srcbag, srcslot, false);

    // add to dest (将源物品放入目标槽位)
    if (IsInventoryPos(dst))
        StoreItem(sDest, pSrcItem, true);
    else if (IsBankPos(dst))
        BankItem(sDest, pSrcItem, true);
    else if (IsEquipmentPos(dst))
        EquipItem(eDest, pSrcItem, true);

    // add to src (将目标物品放入源槽位)
    if (IsInventoryPos(src))
        StoreItem(sDest2, pDstItem, true);
    else if (IsBankPos(src))
        BankItem(sDest2, pDstItem, true);
    else if (IsEquipmentPos(src))
        EquipItem(eDest2, pDstItem, true);

    AutoUnequipOffhandIfNeed();
}

void Player::AddItemToBuyBackSlot(Item* pItem, uint32 money)
{
    MANGOS_ASSERT(pItem != nullptr);

    uint32 slot = m_currentBuybackSlot;
    // if current back slot non-empty search oldest or free (若当前回购指针已占用，查找空闲槽或最早条目覆盖)
    if (m_items[slot])
    {
        uint32 oldest_time = GetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1);
        uint32 oldest_slot = BUYBACK_SLOT_START;

        for (uint32 i = BUYBACK_SLOT_START + 1; i < BUYBACK_SLOT_END; ++i)
        {
            // found empty (找到空闲回购槽位)
            if (!m_items[i])
            {
                break;
            }

            uint32 i_time = GetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + i - BUYBACK_SLOT_START);

            if (oldest_time > i_time)
            {
                oldest_time = i_time;
                oldest_slot = i;
            }
        }

        // find oldest (未找到空位，淘汰最早出售的物品)
        slot = oldest_slot;
    }

    RemoveItemFromBuyBackSlot(slot, true);
    DEBUG_LOG("STORAGE: AddItemToBuyBackSlot item = %u, slot = %u", pItem->GetEntry(), slot);

    m_items[slot] = pItem;
    time_t base = time(nullptr);
    uint32 etime = uint32(base - m_logintime + (30 * 3600));
    uint32 eslot = slot - BUYBACK_SLOT_START;

    SetGuidValue(PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + (eslot * 2), pItem->GetObjectGuid());
    SetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + eslot, money);
    SetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + eslot, (uint32)etime);

    // move to next (for non filled list is move most optimized choice) (移动回购指针至下一格)
    if (m_currentBuybackSlot < BUYBACK_SLOT_END - 1)
        ++m_currentBuybackSlot;
}

Item* Player::GetItemFromBuyBackSlot(uint32 slot)
{
    DEBUG_LOG("STORAGE: GetItemFromBuyBackSlot slot = %u", slot);
    if (slot >= BUYBACK_SLOT_START && slot < BUYBACK_SLOT_END)
        return m_items[slot];
    return nullptr;
}

void Player::RemoveItemFromBuyBackSlot(uint32 slot, bool del)
{
    DEBUG_LOG("STORAGE: RemoveItemFromBuyBackSlot slot = %u", slot);
    if (slot >= BUYBACK_SLOT_START && slot < BUYBACK_SLOT_END)
    {
        Item* pItem = m_items[slot];
        if (pItem)
        {
            pItem->RemoveFromWorld();
            if (del)
            {
                GetMap()->AddUpdateRemoveObject({ GetObjectGuid() }, pItem->GetObjectGuid());
                pItem->SetState(ITEM_REMOVED, this);
            }
        }

        m_items[slot] = nullptr;

        uint32 eslot = slot - BUYBACK_SLOT_START;
        SetGuidValue(PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + (eslot * 2), ObjectGuid());
        SetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + eslot, 0);
        SetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + eslot, 0);

        // if current backslot is filled set to now free slot (更新当前回购指针至新腾出的槽位)
        if (m_items[m_currentBuybackSlot])
            m_currentBuybackSlot = slot;
    }
}

void Player::SendEquipError(InventoryResult msg, Item* pItem, Item* pItem2, uint8 slot, uint32 itemid /*= 0*/) const
{
    DEBUG_LOG("WORLD: Sent SMSG_INVENTORY_CHANGE_FAILURE (%u)", msg);
    WorldPacket data(SMSG_INVENTORY_CHANGE_FAILURE, (msg == EQUIP_ERR_CANT_EQUIP_LEVEL_I ? 22 : (msg == EQUIP_ERR_OK ? 1 : 18)));
    data << uint8(msg);

    if (msg != EQUIP_ERR_OK)
    {
        data << (pItem ? pItem->GetObjectGuid() : ObjectGuid()); // closes loot, cancels spell, unlocks item (发送装备错误后关闭拾取、取消施法并解锁物品)
        data << (pItem2 ? pItem2->GetObjectGuid() : ObjectGuid()); // only unlocks item (仅解锁物品状态)

        switch (msg)
        {
            case EQUIP_ERR_EVENT_AUTOEQUIP_BIND_CONFIRM:
                data << uint8(0); // unk purpose but specified nonetheless (未知用途字节，协议要求固定发送)
                break;
            case EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2: // specifies slot of target bag that has storing condition (指定目标背包的容器限制类型)
            default:
                data << uint8(slot);
                break;
        }

        switch (msg)
        {
            case EQUIP_ERR_CANT_EQUIP_LEVEL_I:
            {
                ItemPrototype const* proto = pItem ? pItem->GetProto() : ObjectMgr::GetItemPrototype(itemid);
                data << uint32(proto ? proto->RequiredLevel : 0);
                break;
            }
            case EQUIP_ERR_EVENT_AUTOEQUIP_BIND_CONFIRM: // no idea about this one... (未知状态字段，默认发 0)
            {
                data << uint64(0);
                data << uint32(0);
                data << uint64(0);
                break;
            }
        }
    }
    GetSession()->SendPacket(data);
}

void Player::SendBuyError(BuyResult msg, Creature* pCreature, uint32 vendorSlot) const
{
    DEBUG_LOG("WORLD: Sent SMSG_BUY_FAILED");
    WorldPacket data(SMSG_BUY_FAILED, (8 + 4 + 4 + 1));
    data << (pCreature ? pCreature->GetObjectGuid() : ObjectGuid());
    data << uint32(vendorSlot + 1);                 // numbered from 1 at client (客户端商人槽位从 1 开始编号)
    data << uint8(msg);
    GetSession()->SendPacket(data);
}

void Player::SendSellError(SellResult msg, Creature* pCreature, ObjectGuid itemGuid) const
{
    DEBUG_LOG("WORLD: Sent SMSG_SELL_ITEM");
    WorldPacket data(SMSG_SELL_ITEM, (8 + 8 + 1)); // last check 2.0.10 (协议版本校验 2.0.10)
    data << (pCreature ? pCreature->GetObjectGuid() : ObjectGuid());
    data << ObjectGuid(itemGuid);
    data << uint8(msg);
    GetSession()->SendPacket(data);
}

void Player::SendOpenContainer(ObjectGuid itemGuid)
{
    WorldPacket data(SMSG_OPEN_CONTAINER, 8);
    data << ObjectGuid(itemGuid);
    GetSession()->SendPacket(data);
}

void Player::TradeCancel(bool sendback, TradeStatus status /*= TRADE_STATUS_TRADE_CANCELED*/)
{
    if (m_trade)
    {
        Player* trader = m_trade->GetTrader();

        // send yellow "Trade canceled" message to both traders (向双方客户端发送交易取消提示)
        if (sendback)
            GetSession()->SendCancelTrade(status);

        trader->GetSession()->SendCancelTrade(status);

        // cleanup (销毁双方交易数据对象)
        delete m_trade;
        m_trade = nullptr;
        delete trader->m_trade;
        trader->m_trade = nullptr;
    }
}

void Player::UpdateItemDuration(uint32 time, bool realtimeonly)
{
    if (m_itemDuration.empty())
        return;

    DEBUG_LOG("Player::UpdateItemDuration(%u,%s)", time, realtimeonly ? "true" : "false");

    for (ItemDurationList::const_iterator itr = m_itemDuration.begin(); itr != m_itemDuration.end();)
    {
        Item* item = *itr;
        ++itr;                                              // current element can be erased in UpdateDuration (循环中先递增迭代器，以防限时物品到期被移除)

        if (!(realtimeonly) || (item->GetProto()->ExtraFlags & ITEM_EXTRA_REAL_TIME_DURATION))
            item->UpdateDuration(this, time);
    }
}

void Player::UpdateEnchantTime(uint32 time)
{
    for (EnchantDurationList::iterator itr = m_enchantDuration.begin(), next; itr != m_enchantDuration.end(); itr = next)
    {
        MANGOS_ASSERT(itr->item);
        next = itr;
        if (!itr->item->GetEnchantmentId(itr->slot))
        {
            next = m_enchantDuration.erase(itr);
        }
        else if (itr->leftduration <= time)
        {
            ApplyEnchantment(itr->item, itr->slot, false, false);
            itr->item->ClearEnchantment(itr->slot);
            next = m_enchantDuration.erase(itr);
        }
        else if (itr->leftduration > time)
        {
            itr->leftduration -= time;
            ++next;
        }
    }
}

void Player::AddEnchantmentDurations(Item* item)
{
    for (int x = 0; x < MAX_ENCHANTMENT_SLOT; ++x)
    {
        if (!item->GetEnchantmentId(EnchantmentSlot(x)))
            continue;

        uint32 duration = item->GetEnchantmentDuration(EnchantmentSlot(x));
        if (duration > 0)
            AddEnchantmentDuration(item, EnchantmentSlot(x), duration);
    }
}

void Player::RemoveEnchantmentDurations(Item* item)
{
    for (EnchantDurationList::iterator itr = m_enchantDuration.begin(); itr != m_enchantDuration.end();)
    {
        if (itr->item == item)
        {
            // save duration in item (将剩余附魔时间写回物品)
            item->SetEnchantmentDuration(EnchantmentSlot(itr->slot), itr->leftduration);
            itr = m_enchantDuration.erase(itr);
        }
        else
            ++itr;
    }
}

void Player::RemoveAllEnchantments(EnchantmentSlot slot, bool arena)
{
    // remove enchantments from equipped items first to clean up the m_enchantDuration list (先移除已装备物品的附魔以清理 m_enchantDuration 列表)
    for (EnchantDurationList::iterator itr = m_enchantDuration.begin(), next; itr != m_enchantDuration.end(); itr = next)
    {
        next = itr;
        if (itr->slot == slot)
        {
            if (itr->item && itr->item->GetEnchantmentId(slot))
            {
                if (arena)
                {
                    if (itr->item->CanEnterArenaEnchant(slot))
                    {
                        ++next;
                        continue;
                    }
                }

                // remove from stats (移除附魔属性加成)
                ApplyEnchantment(itr->item, slot, false, false);
                // remove visual (清除附魔外观槽位显示)
                itr->item->ClearEnchantment(slot);
            }
            // remove from update list (从附魔计时列表移除)
            next = m_enchantDuration.erase(itr);
        }
        else
            ++next;
    }

    // remove enchants from inventory items (移除背包物品上的附魔)
    // NOTE: no need to remove these from stats, since these aren't equipped (注：未装备物品不产生属性加成，仅清理附魔记录)
    // in inventory (遍历主行囊)
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
    {
        Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetEnchantmentId(slot) && (!arena || !item->CanEnterArenaEnchant(slot)))
            item->ClearEnchantment(slot);
    }

    // in inventory bags (遍历各背包)
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pBag)
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
            {
                Item* item = pBag->GetItemByPos(j);
                if (item && item->GetEnchantmentId(slot) && (!arena || !item->CanEnterArenaEnchant(slot)))
                    item->ClearEnchantment(slot);
            }
        }
    }
}

// duration == 0 will remove item enchant (持续时间为 0 表示移除附魔计时)
void Player::AddEnchantmentDuration(Item* item, EnchantmentSlot slot, uint32 duration)
{
    if (!item)
        return;

    if (slot >= MAX_ENCHANTMENT_SLOT)
        return;

    for (EnchantDurationList::iterator itr = m_enchantDuration.begin(); itr != m_enchantDuration.end(); ++itr)
    {
        if (itr->item == item && itr->slot == slot)
        {
            itr->item->SetEnchantmentDuration(itr->slot, itr->leftduration);
            m_enchantDuration.erase(itr);
            break;
        }
    }
    if (item && duration > 0)
    {
        GetSession()->SendItemEnchantTimeUpdate(GetObjectGuid(), item->GetObjectGuid(), slot, uint32(duration / 1000));
        m_enchantDuration.push_back(EnchantDuration(item, slot, duration));
    }
}

void Player::ApplyEnchantment(Item* item, bool apply)
{
    for (uint32 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
        ApplyEnchantment(item, EnchantmentSlot(slot), apply);
}

/// 应用或移除物品指定附魔槽位的属性、抗性、法术触发与外观效果
void Player::ApplyEnchantment(Item* item, EnchantmentSlot slot, bool apply, bool apply_dur, bool ignore_condition)
{
    if (!item)
        return;

    if (!item->IsEquipped())
        return;

    if (slot >= MAX_ENCHANTMENT_SLOT)
        return;

    uint32 enchant_id = item->GetEnchantmentId(slot);
    if (!enchant_id)
        return;

    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
    if (!pEnchant)
        return;

    if (!ignore_condition && pEnchant->EnchantmentCondition && !EnchantmentFitsRequirements(pEnchant->EnchantmentCondition, -1))
        return;

    if (!item->IsBroken())
    {
        for (int s = 0; s < 3; ++s)
        {
            uint32 enchant_display_type = pEnchant->type[s];
            uint32 enchant_amount = pEnchant->amount[s];
            uint32 enchant_spell_id = pEnchant->spellid[s];

            switch (enchant_display_type)
            {
                case ITEM_ENCHANTMENT_TYPE_NONE:
                    break;
                case ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL:
                    if (!apply) // clear modifier set by some spells (清除特定武器附魔修正)
                        item->SetEnchantmentModifier(0);
                    // processed in Player::CastItemCombatSpell (战斗触发类附魔在 CastItemCombatSpell 中处理)
                    break;
                case ITEM_ENCHANTMENT_TYPE_DAMAGE:
                    // processed in Player::_ApplyWeaponDependentAuraMods (武器限定光环附魔在 _ApplyWeaponDependentAuraMods 中处理)
                    if (item->GetSlot() == EQUIPMENT_SLOT_MAINHAND)
                    {
                        SetEnchantmentModifier(enchant_amount, BASE_ATTACK, apply);
                        UpdateDamagePhysical(BASE_ATTACK);
                    }
                    else if (item->GetSlot() == EQUIPMENT_SLOT_OFFHAND)
                    {
                        SetEnchantmentModifier(enchant_amount, OFF_ATTACK, apply);
                        UpdateDamagePhysical(OFF_ATTACK);
                    }
                    else if (item->GetSlot() == EQUIPMENT_SLOT_RANGED)
                    {
                        SetEnchantmentModifier(enchant_amount, RANGED_ATTACK, apply);
                        UpdateDamagePhysical(RANGED_ATTACK);
                    }
                    break;
                case ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL:
                {
                    if (enchant_spell_id)
                    {
                        if (apply)
                        {
                            int32 basepoints = 0;
                            // Random Property Exist - try found basepoints for spell (basepoints depends from item suffix factor) (存在随机附魔后缀：根据后缀因子计算法术基础点数)
                            if (item->GetItemRandomPropertyId())
                            {
                                ItemRandomSuffixEntry const* item_rand = sItemRandomSuffixStore.LookupEntry(abs(item->GetItemRandomPropertyId()));
                                if (item_rand)
                                {
                                    // Search enchant_amount (解析附魔实际加成数值)
                                    for (int k = 0; k < 3; ++k)
                                    {
                                        if (item_rand->enchant_id[k] == enchant_id)
                                        {
                                            basepoints = int32((item_rand->prefix[k] * item->GetItemSuffixFactor()) / 10000);
                                            break;
                                        }
                                    }
                                }
                            }
                            // Cast custom spell vs all equal basepoints got from enchant_amount (施放带自定义基础点数的附魔法术)
                            if (basepoints)
                                CastCustomSpell(this, enchant_spell_id, &basepoints, &basepoints, &basepoints, TRIGGERED_OLD_TRIGGERED, item);
                            else
                                CastSpell(this, enchant_spell_id, TRIGGERED_OLD_TRIGGERED, item);
                        }
                        else
                            RemoveAurasDueToItemSpell(item, enchant_spell_id);
                    }
                    break;
                }
                case ITEM_ENCHANTMENT_TYPE_RESISTANCE:
                    if (!enchant_amount)
                    {
                        ItemRandomSuffixEntry const* item_rand = sItemRandomSuffixStore.LookupEntry(abs(item->GetItemRandomPropertyId()));
                        if (item_rand)
                        {
                            for (int k = 0; k < 3; ++k)
                            {
                                if (item_rand->enchant_id[k] == enchant_id)
                                {
                                    enchant_amount = uint32((item_rand->prefix[k] * item->GetItemSuffixFactor()) / 10000);
                                    break;
                                }
                            }
                        }
                    }

                    HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + enchant_spell_id), TOTAL_VALUE, float(enchant_amount), apply);
                    break;
                case ITEM_ENCHANTMENT_TYPE_STAT:
                {
                    if (!enchant_amount)
                    {
                        ItemRandomSuffixEntry const* item_rand_suffix = sItemRandomSuffixStore.LookupEntry(abs(item->GetItemRandomPropertyId()));
                        if (item_rand_suffix)
                        {
                            for (int k = 0; k < 3; ++k)
                            {
                                if (item_rand_suffix->enchant_id[k] == enchant_id)
                                {
                                    enchant_amount = uint32((item_rand_suffix->prefix[k] * item->GetItemSuffixFactor()) / 10000);
                                    break;
                                }
                            }
                        }
                    }

                    DEBUG_LOG("Adding %u to stat nb %u", enchant_amount, enchant_spell_id);
                    switch (enchant_spell_id)
                    {
                        case ITEM_MOD_MANA:
                            DEBUG_LOG("+ %u MANA", enchant_amount);
                            HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(enchant_amount), apply);
                            break;
                        case ITEM_MOD_HEALTH:
                            DEBUG_LOG("+ %u HEALTH", enchant_amount);
                            HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(enchant_amount), apply);
                            break;
                        case ITEM_MOD_AGILITY:
                            DEBUG_LOG("+ %u AGILITY", enchant_amount);
                            HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, float(enchant_amount), apply);
                            ApplyStatBuffMod(STAT_AGILITY, float(enchant_amount), apply);
                            break;
                        case ITEM_MOD_STRENGTH:
                            DEBUG_LOG("+ %u STRENGTH", enchant_amount);
                            HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, float(enchant_amount), apply);
                            ApplyStatBuffMod(STAT_STRENGTH, float(enchant_amount), apply);
                            break;
                        case ITEM_MOD_INTELLECT:
                            DEBUG_LOG("+ %u INTELLECT", enchant_amount);
                            HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, float(enchant_amount), apply);
                            ApplyStatBuffMod(STAT_INTELLECT, float(enchant_amount), apply);
                            break;
                        case ITEM_MOD_SPIRIT:
                            DEBUG_LOG("+ %u SPIRIT", enchant_amount);
                            HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, float(enchant_amount), apply);
                            ApplyStatBuffMod(STAT_SPIRIT, float(enchant_amount), apply);
                            break;
                        case ITEM_MOD_STAMINA:
                            DEBUG_LOG("+ %u STAMINA", enchant_amount);
                            HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, float(enchant_amount), apply);
                            ApplyStatBuffMod(STAT_STAMINA, float(enchant_amount), apply);
                            break;
                        case ITEM_MOD_DEFENSE_SKILL_RATING:
                            ApplyRatingMod(CR_DEFENSE_SKILL, enchant_amount, apply);
                            DEBUG_LOG("+ %u DEFENCE", enchant_amount);
                            break;
                        case  ITEM_MOD_DODGE_RATING:
                            ApplyRatingMod(CR_DODGE, enchant_amount, apply);
                            DEBUG_LOG("+ %u DODGE", enchant_amount);
                            break;
                        case ITEM_MOD_PARRY_RATING:
                            ApplyRatingMod(CR_PARRY, enchant_amount, apply);
                            DEBUG_LOG("+ %u PARRY", enchant_amount);
                            break;
                        case ITEM_MOD_BLOCK_RATING:
                            ApplyRatingMod(CR_BLOCK, enchant_amount, apply);
                            DEBUG_LOG("+ %u SHIELD_BLOCK", enchant_amount);
                            break;
                        case ITEM_MOD_HIT_MELEE_RATING:
                            ApplyRatingMod(CR_HIT_MELEE, enchant_amount, apply);
                            DEBUG_LOG("+ %u MELEE_HIT", enchant_amount);
                            break;
                        case ITEM_MOD_HIT_RANGED_RATING:
                            ApplyRatingMod(CR_HIT_RANGED, enchant_amount, apply);
                            DEBUG_LOG("+ %u RANGED_HIT", enchant_amount);
                            break;
                        case ITEM_MOD_HIT_SPELL_RATING:
                            ApplyRatingMod(CR_HIT_SPELL, enchant_amount, apply);
                            DEBUG_LOG("+ %u SPELL_HIT", enchant_amount);
                            break;
                        case ITEM_MOD_CRIT_MELEE_RATING:
                            ApplyRatingMod(CR_CRIT_MELEE, enchant_amount, apply);
                            DEBUG_LOG("+ %u MELEE_CRIT", enchant_amount);
                            break;
                        case ITEM_MOD_CRIT_RANGED_RATING:
                            ApplyRatingMod(CR_CRIT_RANGED, enchant_amount, apply);
                            DEBUG_LOG("+ %u RANGED_CRIT", enchant_amount);
                            break;
                        case ITEM_MOD_CRIT_SPELL_RATING:
                            ApplyRatingMod(CR_CRIT_SPELL, enchant_amount, apply);
                            DEBUG_LOG("+ %u SPELL_CRIT", enchant_amount);
                            break;
//                        Values from ITEM_STAT_MELEE_HA_RATING to ITEM_MOD_HASTE_RANGED_RATING are never used (以下注释掉的近战/远程等级分支在附魔中从未使用)
//                        in Enchantments
//                        case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
//                            ApplyRatingMod(CR_HIT_TAKEN_MELEE, enchant_amount, apply);
//                            break;
//                        case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
//                            ApplyRatingMod(CR_HIT_TAKEN_RANGED, enchant_amount, apply);
//                            break;
//                        case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
//                            ApplyRatingMod(CR_HIT_TAKEN_SPELL, enchant_amount, apply);
//                            break;
//                        case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
//                            ApplyRatingMod(CR_CRIT_TAKEN_MELEE, enchant_amount, apply);
//                            break;
//                        case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
//                            ApplyRatingMod(CR_CRIT_TAKEN_RANGED, enchant_amount, apply);
//                            break;
//                        case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
//                            ApplyRatingMod(CR_CRIT_TAKEN_SPELL, enchant_amount, apply);
//                            break;
//                        case ITEM_MOD_HASTE_MELEE_RATING:
//                            ApplyRatingMod(CR_HASTE_MELEE, enchant_amount, apply);
//                            break;
//                        case ITEM_MOD_HASTE_RANGED_RATING:
//                            ApplyRatingMod(CR_HASTE_RANGED, enchant_amount, apply);
//                            break;
                        case ITEM_MOD_HASTE_SPELL_RATING:
                            ApplyRatingMod(CR_HASTE_SPELL, enchant_amount, apply);
                            break;
                        case ITEM_MOD_HIT_RATING:
                            ApplyRatingMod(CR_HIT_MELEE, enchant_amount, apply);
                            ApplyRatingMod(CR_HIT_RANGED, enchant_amount, apply);
                            DEBUG_LOG("+ %u HIT", enchant_amount);
                            break;
                        case ITEM_MOD_CRIT_RATING:
                            ApplyRatingMod(CR_CRIT_MELEE, enchant_amount, apply);
                            ApplyRatingMod(CR_CRIT_RANGED, enchant_amount, apply);
                            DEBUG_LOG("+ %u CRITICAL", enchant_amount);
                            break;
//                        Values ITEM_MOD_HIT_TAKEN_RATING and ITEM_MOD_CRIT_TAKEN_RATING are never used in Enchantment (被命中/被暴击等级在附魔中从未使用)
//                        case ITEM_MOD_HIT_TAKEN_RATING:
//                            ApplyRatingMod(CR_HIT_TAKEN_MELEE, enchant_amount, apply);
//                            ApplyRatingMod(CR_HIT_TAKEN_RANGED, enchant_amount, apply);
//                            ApplyRatingMod(CR_HIT_TAKEN_SPELL, enchant_amount, apply);
//                            break;
//                        case ITEM_MOD_CRIT_TAKEN_RATING:
//                            ApplyRatingMod(CR_CRIT_TAKEN_MELEE, enchant_amount, apply);
//                            ApplyRatingMod(CR_CRIT_TAKEN_RANGED, enchant_amount, apply);
//                            ApplyRatingMod(CR_CRIT_TAKEN_SPELL, enchant_amount, apply);
//                            break;
                        case ITEM_MOD_RESILIENCE_RATING:
                            ApplyRatingMod(CR_CRIT_TAKEN_MELEE, enchant_amount, apply);
                            ApplyRatingMod(CR_CRIT_TAKEN_RANGED, enchant_amount, apply);
                            ApplyRatingMod(CR_CRIT_TAKEN_SPELL, enchant_amount, apply);
                            DEBUG_LOG("+ %u RESILIENCE", enchant_amount);
                            break;
                        case ITEM_MOD_HASTE_RATING:
                            ApplyRatingMod(CR_HASTE_MELEE, enchant_amount, apply);
                            ApplyRatingMod(CR_HASTE_RANGED, enchant_amount, apply);
                            DEBUG_LOG("+ %u HASTE", enchant_amount);
                            break;
                        case ITEM_MOD_EXPERTISE_RATING:
                            ApplyRatingMod(CR_EXPERTISE, enchant_amount, apply);
                            DEBUG_LOG("+ %u EXPERTISE", enchant_amount);
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case ITEM_ENCHANTMENT_TYPE_TOTEM:           // Shaman Rockbiter Weapon (萨满石化武器附魔额外攻击强度)
                {
                    if (getClass() == CLASS_SHAMAN)
                    {
                        float addValue = 0.0f;
                        if (item->GetSlot() == EQUIPMENT_SLOT_MAINHAND)
                        {
                            addValue = float(enchant_amount * item->GetProto()->Delay / 1000.0f);
                            HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE, addValue, apply);
                        }
                        else if (item->GetSlot() == EQUIPMENT_SLOT_OFFHAND)
                        {
                            addValue = float(enchant_amount * item->GetProto()->Delay / 1000.0f);
                            HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_VALUE, addValue, apply);
                        }
                    }
                    break;
                }
                default:
                    sLog.outError("Unknown item enchantment (id = %d) display type: %d", enchant_id, enchant_display_type);
                    break;
            }                                               /*switch(enchant_display_type)*/
        }                                                   /*for*/
    }

    // visualize enchantment at player and equipped items (更新玩家可视装备的附魔外观光效)
    if (slot < MAX_INSPECTED_ENCHANTMENT_SLOT)
    {
        int VisibleBase = PLAYER_VISIBLE_ITEM_1_0 + (item->GetSlot() * MAX_VISIBLE_ITEM_OFFSET);
        SetUInt32Value(VisibleBase + 1 + slot, apply ? item->GetEnchantmentId(slot) : 0);
    }

    if (apply_dur)
    {
        if (apply)
        {
            // set duration (应用或刷新附魔剩余持续时间)
            uint32 duration = item->GetEnchantmentDuration(slot);
            if (duration > 0)
                AddEnchantmentDuration(item, slot, duration);
        }
        else
        {
            // duration == 0 will remove EnchantDuration (持续时间为 0 时移除附魔计时)
            AddEnchantmentDuration(item, slot, 0);
        }
    }
}

void Player::SendEnchantmentDurations()
{
    for (EnchantDurationList::const_iterator itr = m_enchantDuration.begin(); itr != m_enchantDuration.end(); ++itr)
    {
        GetSession()->SendItemEnchantTimeUpdate(GetObjectGuid(), itr->item->GetObjectGuid(), itr->slot, uint32(itr->leftduration) / 1000);
    }
}

void Player::SendItemDurations()
{
    for (ItemDurationList::const_iterator itr = m_itemDuration.begin(); itr != m_itemDuration.end(); ++itr)
    {
        (*itr)->SendTimeUpdate(this);
    }
}

void Player::SendNewItem(Item* item, uint32 count, bool received, bool created, bool broadcast /*=false*/, bool showInChat /*=true*/)
{
    if (!item)                                              // prevent crash (防止空物品指针导致崩溃)
        return;

    WorldPacket data(SMSG_ITEM_PUSH_RESULT, (8 + 4 + 4 + 4 + 1 + 4 + 4 + 4 + 4 + 4));
    data << GetObjectGuid();                                // player GUID (玩家 GUID)
    data << uint32(received);                               // 0=looted, 1=from npc (0=拾取获得，1=NPC 给予/购买)
    data << uint32(created);                                // 0=received, 1=created (0=获得，1=制造产出)
    data << uint32(showInChat);                             // showInChat (是否在聊天框显示获得提示)
    data << uint8(item->GetBagSlot());                      // bagslot (背包槽位编号)
    // item slot, but when added to stack: 0xFFFFFFFF (物品槽位；合并至现有堆叠时发 0xFFFFFFFF)
    data << uint32((item->GetCount() == count) ? item->GetSlot() : -1);
    data << uint32(item->GetEntry());                       // item id (物品模板 ID)
    data << uint32(item->GetItemSuffixFactor());            // SuffixFactor (随机后缀因子)
    data << uint32(item->GetItemRandomPropertyId());        // random item property id (随机属性 ID)
    data << uint32(count);                                  // count of items (本次获得数量)
    data << uint32(GetItemCount(item->GetEntry(), true));   // count of items in bags and bank (当前背包与银行总持有数量)

    if (broadcast && GetGroup())
        GetGroup()->BroadcastPacket(data, true);
    else
        GetSession()->SendPacket(data);
}

/*********************************************************/
/***             GOSSIP SYSTEM (NPC 对话系统)            ***/
/*********************************************************/

/// 为交互对象构建 NPC 对话菜单与关联任务选项
void Player::PrepareGossipMenu(WorldObject* pSource, uint32 menuId, bool forceQuests)
{
    m_playerMenu->ClearMenus();

    m_playerMenu->GetGossipMenu().SetMenuId(menuId);

    GossipMenuItemsMapBounds pMenuItemBounds = sObjectMgr.GetGossipMenuItemsMapBounds(menuId);

    Gender gender = GENDER_MALE;

    // prepares quest menu when true (为 true 时同时准备任务菜单)
    bool canSeeQuests = menuId == GetDefaultGossipMenuForSource(pSource) || forceQuests;

    // if canSeeQuests (the default, top level menu) and no menu options exist for this, use options from default options (若顶级菜单无自定义选项则回退读取默认对话选项)
    if (pMenuItemBounds.first == pMenuItemBounds.second && canSeeQuests)
        pMenuItemBounds = sObjectMgr.GetGossipMenuItemsMapBounds(0);

    for (GossipMenuItemsMap::const_iterator itr = pMenuItemBounds.first; itr != pMenuItemBounds.second; ++itr)
    {
        GossipMenuItems const& gossipMenu = itr->second;
        bool hasMenuItem = true;
        bool isGMSkipConditionCheck = false;

        if (gossipMenu.conditionId && !sObjectMgr.IsConditionSatisfied(gossipMenu.conditionId, this, GetMap(), pSource, CONDITION_FROM_GOSSIP_OPTION))
        {
            if (IsGameMaster())                             // Let GM always see menu items regardless of conditions (GM 模式下忽略条件限制显示所有对话选项)
                isGMSkipConditionCheck = true;
            else
            {
                if (gossipMenu.option_id == GOSSIP_OPTION_QUESTGIVER)
                    canSeeQuests = false;
                continue;                                   // Skip this option (不满足条件，跳过此选项)
            }
        }

        if (pSource->GetTypeId() == TYPEID_UNIT)
        {
            Creature* pCreature = (Creature*)pSource;

            gender = Gender(pCreature->getGender());

            uint32 npcflags = pCreature->GetUInt32Value(UNIT_NPC_FLAGS);

            if (!(gossipMenu.npc_option_npcflag & npcflags))
                continue;

            switch (gossipMenu.option_id)
            {
                case GOSSIP_OPTION_GOSSIP:
                    break;
                case GOSSIP_OPTION_QUESTGIVER:
                    hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_ARMORER:
                    hasMenuItem = false;                    // added in special mode (按特定模式添加对话选项，如训练师/商人/飞行管理员等)
                    break;
                case GOSSIP_OPTION_SPIRITHEALER:
                    if (!IsDead())
                        hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_VENDOR:
                {
                    VendorItemData const* vItems = pCreature->GetVendorItems();
                    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();
                    if ((!vItems || vItems->Empty()) && (!tItems || tItems->Empty()))
                    {
                        sLog.outErrorDb("Creature %u (Entry: %u) have UNIT_NPC_FLAG_VENDOR but have empty trading item list.", pCreature->GetGUIDLow(), pCreature->GetEntry());
                        hasMenuItem = false;
                    }
                    break;
                }
                case GOSSIP_OPTION_TRAINER:
                    if (!pCreature->IsTrainerOf(this, false))
                        hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_UNLEARNTALENTS:
                    if (!pCreature->CanTrainAndResetTalentsOf(this))
                        hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_UNLEARNPETSKILLS:
                    if (!GetPet() || GetPet()->getPetType() != HUNTER_PET || GetPet()->m_spells.size() <= 1 || pCreature->GetCreatureInfo()->TrainerType != TRAINER_TYPE_PETS || pCreature->GetCreatureInfo()->TrainerClass != CLASS_HUNTER)
                        hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_TAXIVENDOR:
                    if (GetSession()->SendLearnNewTaxiNode(pCreature))
                        m_playerMenu->GetGossipMenu().SetDiscoveredNode();
                    break;
                case GOSSIP_OPTION_BATTLEFIELD:
                    if (!pCreature->CanInteractWithBattleMaster(this, false))
                        hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_STABLEPET:
                    if (getClass() != CLASS_HUNTER)
                        hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_SPIRITGUIDE:
                case GOSSIP_OPTION_INNKEEPER:
                case GOSSIP_OPTION_BANKER:
                case GOSSIP_OPTION_PETITIONER:
                case GOSSIP_OPTION_TABARDDESIGNER:
                case GOSSIP_OPTION_AUCTIONEER:
                    break;                                  // no checks (无额外前置检查的菜单动作)
                case GOSSIP_OPTION_BOT:
                {
                    hasMenuItem = false;
                    break;
                }
                case GOSSIP_OPTION_SD2_1:
                case GOSSIP_OPTION_SD2_2:
                case GOSSIP_OPTION_SD2_3:
                case GOSSIP_OPTION_SD2_4:
                case GOSSIP_OPTION_SD2_5:
                    break;
                default:
                    sLog.outErrorDb("Creature entry %u have unknown gossip option %u for menu %u", pCreature->GetEntry(), gossipMenu.option_id, gossipMenu.menu_id);
                    hasMenuItem = false;
                    break;
            }
        }
        else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
        {
            GameObject* pGo = (GameObject*)pSource;

            switch (gossipMenu.option_id)
            {
                case GOSSIP_OPTION_QUESTGIVER:
                    hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_GOSSIP:
                    if (pGo->GetGoType() != GAMEOBJECT_TYPE_QUESTGIVER && pGo->GetGoType() != GAMEOBJECT_TYPE_GOOBER)
                        hasMenuItem = false;
                    break;
                default:
                    hasMenuItem = false;
                    break;
            }
        }

        if (hasMenuItem)
        {
            std::string strOptionText = gossipMenu.option_text;
            std::string strBoxText = gossipMenu.box_text;

            int loc_idx = GetSession()->GetSessionDbLocaleIndex();

            if (gossipMenu.option_broadcast_text || gossipMenu.box_broadcast_text)
            {
                if (gossipMenu.option_broadcast_text)
                {
                    BroadcastText const* bct = sObjectMgr.GetBroadcastText(gossipMenu.option_broadcast_text);
                    strOptionText = bct->GetText(loc_idx, gender);
                }
                if (gossipMenu.box_broadcast_text)
                {
                    BroadcastText const* bct = sObjectMgr.GetBroadcastText(gossipMenu.box_broadcast_text);
                    strBoxText = bct->GetText(loc_idx, gender);
                }
            }
            else
            {
                if (loc_idx >= 0)
                {
                    uint32 idxEntry = MAKE_PAIR32(menuId, gossipMenu.id);

                    if (GossipMenuItemsLocale const* no = sObjectMgr.GetGossipMenuItemsLocale(idxEntry))
                    {
                        if (no->OptionText.size() > (size_t)loc_idx && !no->OptionText[loc_idx].empty())
                            strOptionText = no->OptionText[loc_idx];

                        if (no->BoxText.size() > (size_t)loc_idx && !no->BoxText[loc_idx].empty())
                            strBoxText = no->BoxText[loc_idx];
                    }
                }
            }

            if (isGMSkipConditionCheck)
            {
                strOptionText.append(" (");
                strOptionText.append(GetSession()->GetMangosString(LANG_GM_ON));
                strOptionText.append(")");
            }

            m_playerMenu->GetGossipMenu().AddMenuItem(gossipMenu.option_icon, strOptionText, 0, gossipMenu.option_id, strBoxText, gossipMenu.box_money, gossipMenu.box_coded);
            m_playerMenu->GetGossipMenu().AddGossipMenuItemData(gossipMenu.action_menu_id, gossipMenu.action_poi_id, gossipMenu.action_script_id);
        }
    }

    if (pSource->IsCreature() && canSeeQuests && !static_cast<Unit*>(pSource)->isQuestGiver())
        canSeeQuests = false;

    if (canSeeQuests)
        PrepareQuestMenu(pSource->GetObjectGuid());

    // some gossips aren't handled in normal way ... so we need to do it this way .. TODO: handle it in normal way ;-) (部分 NPC 对话未走常规流程的遗留说明)
    /*if (pMenu->Empty())
    {
        if (pCreature->HasFlag(UNIT_NPC_FLAGS,UNIT_NPC_FLAG_TRAINER))
        {
            // output error message if need
            pCreature->IsTrainerOf(this, true);
        }

        if (pCreature->HasFlag(UNIT_NPC_FLAGS,UNIT_NPC_FLAG_BATTLEMASTER))
        {
            // output error message if need
            pCreature->CanInteractWithBattleMaster(this, true);
        }
    }*/
}

void Player::SendPreparedGossip(WorldObject* pSource)
{
    if (!pSource)
        return;

    GossipMenu gossipMenu = m_playerMenu->GetGossipMenu();
    QuestMenu questMenu = m_playerMenu->GetQuestMenu();

    if (pSource->GetTypeId() == TYPEID_UNIT)
    {

        if (gossipMenu.IsJustDiscoveredNode() && questMenu.Empty())
            return;

        // in case no gossip flag and quest menu not empty, open quest menu (client expect gossip menu with this flag) (若无对话标志且存在任务菜单，则直接打开任务菜单)
        if (!((Creature*)pSource)->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP) && !questMenu.Empty())
        {
            SendPreparedQuest(pSource->GetObjectGuid());
            return;
        }
    }
    else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        // probably need to find a better way here (待改进：统一处理无对话标志时的默认文本回退)
        if (!gossipMenu.GetMenuId() && !questMenu.Empty())
        {
            SendPreparedQuest(pSource->GetObjectGuid());
            return;
        }
    }

    // in case non empty gossip menu (that not included quests list size) show it (若对话菜单非空则发送完整对话菜单，)
    // (quest entries from quest menu will be included in list) (其中会包含关联任务列表)

    uint32 textId = GetGossipTextId(pSource);

    if (uint32 menuId = gossipMenu.GetMenuId())
        textId = GetGossipTextId(menuId, pSource);

    m_playerMenu->SendGossipMenu(textId, pSource->GetObjectGuid());
}

/// 处理玩家在 NPC 对话菜单中点击某个选项后的分支逻辑
void Player::OnGossipSelect(WorldObject* pSource, uint32 gossipListId, uint32 menuId)
{
    GossipMenu& gossipmenu = m_playerMenu->GetGossipMenu();

    if (gossipListId >= gossipmenu.MenuItemCount())
        return;

    // if not same, then something funky is going on (菜单 ID 不一致，忽略异常选择)
    if (menuId != gossipmenu.GetMenuId())
        return;

    GossipMenuItem const&  menu_item = gossipmenu.GetItem(gossipListId);

    uint32 gossipOptionId = menu_item.m_gOptionId;
    ObjectGuid guid = pSource->GetObjectGuid();
    uint32 moneyTake = menu_item.m_gBoxMoney;

    // if this function called and player have money for pay MoneyTake or cheating, proccess both cases (校验所需金币并处理扣费或异常)
    if (moneyTake > 0)
    {
        if (GetMoney() >= moneyTake)
            ModifyMoney(-int32(moneyTake));
        else
            return;                                         // cheating (金币不足，疑似客户端作弊)
    }

    if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        if (gossipOptionId > GOSSIP_OPTION_QUESTGIVER)
        {
            sLog.outError("Player guid %u request invalid gossip option for GameObject entry %u", GetGUIDLow(), pSource->GetEntry());
            return;
        }
    }

    GossipMenuItemData const* pMenuData = gossipmenu.GetItemData(gossipListId);
    GossipMenuItemData menuData = {};

    // if pMenuData exist we need to keep a copy of actual data for the following code to process (先拷贝菜单项数据，)
    // call like PrepareGossipMenu or SendPreparedGossip might change the value (以防后续调用 PrepareGossipMenu 改写原数据)
    if (pMenuData)
        menuData = *pMenuData;

    switch (gossipOptionId)
    {
        case GOSSIP_OPTION_GOSSIP:
        {
            if (!pMenuData)
                break;

            if (menuData.m_gAction_poi)
                m_playerMenu->SendPointOfInterest(menuData.m_gAction_poi);

            // send new menu || close gossip || stay at current menu (根据结果发送新菜单、关闭窗口或停留在当前菜单)
            if (menuData.m_gAction_menu > 0)
            {
                PrepareGossipMenu(pSource, uint32(menuData.m_gAction_menu));
                SendPreparedGossip(pSource);
            }
            else if (menuData.m_gAction_menu < 0)
            {
                m_playerMenu->CloseGossip();
                TalkedToCreature(pSource->GetEntry(), pSource->GetObjectGuid());
            }

            break;
        }
        case GOSSIP_OPTION_SPIRITHEALER:
            if (IsDead())
                ((Creature*)pSource)->CastSpell(((Creature*)pSource), 17251, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, GetObjectGuid());
            break;
        case GOSSIP_OPTION_QUESTGIVER:
            PrepareQuestMenu(guid);
            SendPreparedQuest(guid);
            break;
        case GOSSIP_OPTION_VENDOR:
        case GOSSIP_OPTION_ARMORER:
            GetSession()->SendListInventory(guid);
            break;
        case GOSSIP_OPTION_STABLEPET:
            GetSession()->SendStablePet(guid);
            break;
        case GOSSIP_OPTION_TRAINER:
            GetSession()->SendTrainerList(guid);
            break;
        case GOSSIP_OPTION_UNLEARNTALENTS:
            m_playerMenu->CloseGossip();
            SendTalentWipeConfirm(guid);
            break;
        case GOSSIP_OPTION_UNLEARNPETSKILLS:
            m_playerMenu->CloseGossip();
            SendPetSkillWipeConfirm();
            break;
        case GOSSIP_OPTION_TAXIVENDOR:
            GetSession()->SendTaxiMenu(((Creature*)pSource));
            break;
        case GOSSIP_OPTION_INNKEEPER:
            m_playerMenu->CloseGossip();
            SetBindPoint(guid);
            break;
        case GOSSIP_OPTION_BANKER:
            GetSession()->SendShowBank(guid);
            break;
        case GOSSIP_OPTION_PETITIONER:
            m_playerMenu->CloseGossip();
            GetSession()->SendPetitionShowList(guid);
            break;
        case GOSSIP_OPTION_TABARDDESIGNER:
            m_playerMenu->CloseGossip();
            GetSession()->SendTabardVendorActivate(guid);
            break;
        case GOSSIP_OPTION_AUCTIONEER:
            GetSession()->SendAuctionHello(((Creature*)pSource));
            break;
        case GOSSIP_OPTION_SPIRITGUIDE:
            PrepareGossipMenu(pSource);
            SendPreparedGossip(pSource);
            break;
        case GOSSIP_OPTION_BATTLEFIELD:
        {
            BattleGroundTypeId bgTypeId = sBattleGroundMgr.GetBattleMasterBG(pSource->GetEntry());

            if (bgTypeId == BATTLEGROUND_TYPE_NONE)
            {
                sLog.outError("a user (guid %u) requested battlegroundlist from a npc who is no battlemaster", GetGUIDLow());
                return;
            }

            GetSession()->SendBattleGroundList(guid, bgTypeId);
            break;
        }
    }

    if (pMenuData && menuData.m_gAction_script)
    {
        if (pSource->GetTypeId() == TYPEID_UNIT)
            GetMap()->ScriptsStart(SCRIPT_TYPE_GOSSIP, menuData.m_gAction_script, pSource, this, Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE);
        else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
            GetMap()->ScriptsStart(SCRIPT_TYPE_GOSSIP, menuData.m_gAction_script, this, pSource, Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET);
    }
}

uint32 Player::GetGossipTextId(WorldObject* pSource) const
{
    if (!pSource || pSource->GetTypeId() != TYPEID_UNIT)
        return DEFAULT_GOSSIP_MESSAGE;

    if (uint32 pos = sObjectMgr.GetNpcGossip(((Creature*)pSource)->GetGUIDLow()))
        return pos;

    return DEFAULT_GOSSIP_MESSAGE;
}

uint32 Player::GetGossipTextId(uint32 menuId, WorldObject* pSource)
{
    uint32 textId = DEFAULT_GOSSIP_MESSAGE;

    if (!menuId)
        return textId;

    uint32 scriptId = 0;
    uint32 lastConditionId = 0;

    GossipMenusMapBounds pMenuBounds = sObjectMgr.GetGossipMenusMapBounds(menuId);
    for (GossipMenusMap::const_iterator itr = pMenuBounds.first; itr != pMenuBounds.second; ++itr)
    {
        GossipMenus const& gossipMenu = itr->second;
        // Take the text that has the highest conditionId of all fitting (选取所有满足条件的对话文本中条件 ID 最高的一项)
        // No condition and no text with condition found OR higher and fitting condition found (无条件或找到更高优先级的匹配条件)
        if ((!gossipMenu.conditionId && !lastConditionId) ||
                (gossipMenu.conditionId > lastConditionId && sObjectMgr.IsConditionSatisfied(gossipMenu.conditionId, this, GetMap(), pSource, CONDITION_FROM_GOSSIP_MENU)))
        {
            lastConditionId = gossipMenu.conditionId;
            textId = gossipMenu.text_id;
            scriptId = gossipMenu.script_id;
        }
    }

    // Start related script (启动对应的数据库对话脚本)
    if (scriptId)
        GetMap()->ScriptsStart(SCRIPT_TYPE_GOSSIP, scriptId, this, pSource, Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET);

    return textId;
}

uint32 Player::GetDefaultGossipMenuForSource(WorldObject* pSource) const
{
    if (pSource->GetTypeId() == TYPEID_UNIT)
        return ((Creature*)pSource)->GetDefaultGossipMenuId();
    if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
        return ((GameObject*)pSource)->GetGOInfo()->GetGossipMenuId();

    return 0;
}

/*********************************************************/
/***             QUEST SYSTEM (任务系统)                 ***/
/*********************************************************/

void Player::PrepareQuestMenu(ObjectGuid guid) const
{
    QuestRelationsMapBounds rbounds;
    QuestRelationsMapBounds irbounds;

    // pets also can have quests (宠物或载具类目标也可提供任务)
    if (Creature* pCreature = GetMap()->GetAnyTypeCreature(guid))
    {
        rbounds = sObjectMgr.GetCreatureQuestRelationsMapBounds(pCreature->GetEntry());
        irbounds = sObjectMgr.GetCreatureQuestInvolvedRelationsMapBounds(pCreature->GetEntry());
    }
    else
    {
        // we should obtain map pointer from GetMap() in 99% of cases. Special case (通常应通过 GetMap() 获取地图指针；此处为传送法术任务的特例)
        // only for quests which cast teleport spells on player (仅用于对玩家施放传送法术的特殊任务)
        Map* _map = IsInWorld() ? GetMap() : sMapMgr.FindMap(GetMapId(), GetInstanceId());
        MANGOS_ASSERT(_map);

        if (GameObject* pGameObject = _map->GetGameObject(guid))
        {
            rbounds = sObjectMgr.GetGOQuestRelationsMapBounds(pGameObject->GetEntry());
            irbounds = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(pGameObject->GetEntry());
        }
        else
            return;
    }

    QuestMenu& qm = m_playerMenu->GetQuestMenu();
    qm.ClearMenu();

    for (QuestRelationsMap::const_iterator itr = irbounds.first; itr != irbounds.second; ++itr)
    {
        uint32 quest_id = itr->second;

        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
            continue;

        QuestStatus status = GetQuestStatus(quest_id);

        if (status == QUEST_STATUS_COMPLETE && !GetQuestRewardStatus(quest_id))
            qm.AddMenuItem(quest_id, DIALOG_STATUS_REWARD_REP);
        else if (status == QUEST_STATUS_INCOMPLETE)
            qm.AddMenuItem(quest_id, DIALOG_STATUS_INCOMPLETE);
        else if (status == QUEST_STATUS_AVAILABLE)
            qm.AddMenuItem(quest_id, DIALOG_STATUS_CHAT);
    }

    for (QuestRelationsMap::const_iterator itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        uint32 quest_id = itr->second;

        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
            continue;

        QuestStatus status = GetQuestStatus(quest_id);

        if (pQuest->IsAutoComplete() && CanTakeQuest(pQuest, false))
            qm.AddMenuItem(quest_id, DIALOG_STATUS_REWARD_REP);
        else if ((status == QUEST_STATUS_NONE || status == QUEST_STATUS_FAILED) && CanTakeQuest(pQuest, false))
            qm.AddMenuItem(quest_id, DIALOG_STATUS_AVAILABLE);
    }
}

void Player::SendPreparedQuest(ObjectGuid guid) const
{
    QuestMenu& questMenu = m_playerMenu->GetQuestMenu();

    if (questMenu.Empty())
        return;

    QuestMenuItem const& qmi0 = questMenu.GetItem(0);

    uint32 status = qmi0.m_qIcon;

    uint32 type;
    if (guid.IsCreature())
        type = QUESTGIVER_CREATURE;
    else
        type = QUESTGIVER_GAMEOBJECT;

    QuestgiverGreeting const* data = sObjectMgr.GetQuestgiverGreetingData(guid.GetEntry(), type);
    if (data && (questMenu.MenuItemCount() > 1 || sWorld.getConfig(CONFIG_BOOL_ALWAYS_SHOW_QUEST_GREETING)))
    {
        QEmote qe;
        qe._Delay = data->emoteDelay;
        qe._Emote = data->emoteId;
        std::string title = data->text;
        int loc_idx = GetSession()->GetSessionDbLocaleIndex();
        sObjectMgr.GetQuestgiverGreetingLocales(guid.GetEntry(), type, loc_idx, &title);
        m_playerMenu->SendQuestGiverQuestList(qe, title, guid);
    }
    else
    {
        // single element case (仅有单个可用任务的情况)
        if (questMenu.MenuItemCount() == 1)
        {
            // Auto open -- maybe also should verify there is no greeting (只有一个任务时自动打开接取或交任务界面)
            uint32 quest_id = qmi0.m_qId;
            Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

            if (pQuest)
            {
                if (status == DIALOG_STATUS_REWARD_REP && !GetQuestRewardStatus(quest_id))
                    m_playerMenu->SendQuestGiverRequestItems(pQuest, guid, CanRewardQuest(pQuest, false), true);
                else if (status == DIALOG_STATUS_INCOMPLETE)
                    m_playerMenu->SendQuestGiverRequestItems(pQuest, guid, false, true);
                // Send completable on repeatable and autoCompletable quest if player don't have quest (可重复或自动完成任务在未接取时直接发送可交付状态)
                // TODO: verify if check for !pQuest->IsDailyOrWeekly() is really correct (possibly not) (待办：核对 !pQuest->IsDailyOrWeekly() 判断是否准确)
                else if (pQuest->IsAutoComplete() && pQuest->IsRepeatable() && !pQuest->IsDailyOrWeekly())
                    m_playerMenu->SendQuestGiverRequestItems(pQuest, guid, CanCompleteRepeatableQuest(pQuest), true);
                else
                    m_playerMenu->SendQuestGiverQuestDetails(pQuest, guid, true);
            }
        }
        // multiply entries (存在多个任务，发送任务列表供选择)
        else
        {
            QEmote qe;
            qe._Delay = 0;
            qe._Emote = 0;
            std::string title;

            // need pet case for some quests (部分任务由宠物或特殊生物触发)
            if (Creature* pCreature = GetMap()->GetAnyTypeCreature(guid))
            {
                uint32 textid = GetGossipTextId(pCreature);

                GossipText const* gossiptext = sObjectMgr.GetGossipText(textid);
                if (!gossiptext)
                {
                    qe._Delay = 0;                              // TEXTEMOTE_MESSAGE;              // zyg: player emote (玩家表情)
                    qe._Emote = 0;                              // TEXTEMOTE_HELLO;                // zyg: NPC emote (NPC 问候表情)
                    title.clear();
                }
                else
                {
                    qe = gossiptext->Options[0].Emotes[0];

                    int loc_idx = GetSession()->GetSessionDbLocaleIndex();

                    if (gossiptext->Options[0].broadcastTextId)
                    {
                        BroadcastText const* bct = sObjectMgr.GetBroadcastText(gossiptext->Options[0].broadcastTextId);
                        title = bct->GetText(loc_idx, pCreature->getGender());
                    }
                    else
                    {
                        std::string title0 = gossiptext->Options[0].Text_0;
                        std::string title1 = gossiptext->Options[0].Text_1;
                        sObjectMgr.GetNpcTextLocaleStrings0(textid, loc_idx, &title0, &title1);
                        title = !title0.empty() ? title0 : title1;
                    }
                }
            }
            m_playerMenu->SendQuestGiverQuestList(qe, title, guid);
        }
    }
}

bool Player::IsActiveQuest(uint32 quest_id) const
{
    QuestStatusMap::const_iterator itr = mQuestStatus.find(quest_id);

    return itr != mQuestStatus.end() && itr->second.m_status != QUEST_STATUS_NONE;
}

bool Player::IsCurrentQuest(uint32 quest_id, uint8 completed_or_not) const
{
    QuestStatusMap::const_iterator itr = mQuestStatus.find(quest_id);
    if (itr == mQuestStatus.end())
        return false;

    QuestStatusData const& questStatus = itr->second;

    switch (completed_or_not)
    {
        case 1:
            return questStatus.m_status == QUEST_STATUS_INCOMPLETE;
        case 2:
            return questStatus.m_status == QUEST_STATUS_COMPLETE && !questStatus.m_rewarded;
        default:
            return questStatus.m_status == QUEST_STATUS_INCOMPLETE || (questStatus.m_status == QUEST_STATUS_COMPLETE && !questStatus.m_rewarded);
    }
}

Quest const* Player::GetNextQuest(ObjectGuid guid, Quest const* pQuest) const
{
    QuestRelationsMapBounds rbounds;

    if (Creature* pCreature = GetMap()->GetAnyTypeCreature(guid))
    {
        rbounds = sObjectMgr.GetCreatureQuestRelationsMapBounds(pCreature->GetEntry());
    }
    else
    {
        // we should obtain map pointer from GetMap() in 99% of cases. Special case (此处为传送法术任务特例)
        // only for quests which cast teleport spells on player (仅用于对玩家施放传送法术的任务)
        Map* _map = IsInWorld() ? GetMap() : sMapMgr.FindMap(GetMapId(), GetInstanceId());
        MANGOS_ASSERT(_map);

        if (GameObject* pGameObject = _map->GetGameObject(guid))
        {
            rbounds = sObjectMgr.GetGOQuestRelationsMapBounds(pGameObject->GetEntry());
        }
        else
            return nullptr;
    }

    uint32 nextQuestID = pQuest->GetNextQuestInChain();
    for (QuestRelationsMap::const_iterator itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        if (itr->second == nextQuestID)
            return sObjectMgr.GetQuestTemplate(nextQuestID);
    }

    return nullptr;
}

/**
 * Check if a player could see a start quest (检查玩家是否可见某个可接任务的感叹号)
 * Basic Quest-taking requirements: Class, Race, Skill, Quest-Line, ... (校验职业、种族、技能、任务链等基础接取条件)
 * Check if the quest-level is not too high (related config value CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF) (并检查任务等级是否高于玩家过大)
 */
bool Player::CanSeeStartQuest(Quest const* pQuest) const
{
    if (SatisfyQuestClass(pQuest, false) && SatisfyQuestRace(pQuest, false) && SatisfyQuestSkill(pQuest, false) && SatisfyQuestCondition(pQuest, false) &&
            SatisfyQuestExclusiveGroup(pQuest, false) && SatisfyQuestReputation(pQuest, false) &&
            SatisfyQuestPreviousQuest(pQuest, false) && SatisfyQuestBreadcrumbQuest(pQuest, false) && SatisfyQuestDependentBreadcrumbQuests(pQuest, false) &&
            SatisfyQuestNextChain(pQuest, false) && SatisfyQuestPrevChain(pQuest, false) &&
            SatisfyQuestDay(pQuest, false) && SatisfyQuestWeek(pQuest) &&
            SatisfyQuestMonth(pQuest) &&
            pQuest->IsActive())
    {
        int32 highLevelDiff = sWorld.getConfig(CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF);
        if (highLevelDiff < 0)
            return true;
        return (GetLevel() + uint32(highLevelDiff) >= pQuest->GetMinLevel()) && GetLevel() <= pQuest->GetMaxLevel();
    }

    return false;
}

bool Player::CanTakeQuest(Quest const* pQuest, bool msg) const
{
    return SatisfyQuestStatus(pQuest, msg) && SatisfyQuestExclusiveGroup(pQuest, msg) &&
           SatisfyQuestClass(pQuest, msg) && SatisfyQuestRace(pQuest, msg) && SatisfyQuestLevel(pQuest, msg) &&
           SatisfyQuestSkill(pQuest, msg) && SatisfyQuestCondition(pQuest, msg) && SatisfyQuestReputation(pQuest, msg) &&
           SatisfyQuestPreviousQuest(pQuest, msg) && SatisfyQuestTimed(pQuest, msg) &&
           SatisfyQuestNextChain(pQuest, msg) && SatisfyQuestPrevChain(pQuest, msg) &&
           SatisfyQuestBreadcrumbQuest(pQuest, msg) && SatisfyQuestDependentBreadcrumbQuests(pQuest, msg) &&
           SatisfyQuestDay(pQuest, msg) && SatisfyQuestWeek(pQuest) && SatisfyQuestMonth(pQuest) &&
           pQuest->IsActive();
}

bool Player::CanAddQuest(Quest const* pQuest, bool msg) const
{
    // a failed quest taken again keeps its quest log slot (重接已失败任务时保留其原有的任务日志槽位)
    if (FindQuestSlot(pQuest->GetQuestId()) >= MAX_QUEST_LOG_SIZE && !SatisfyQuestLog(msg))
        return false;

    if (!CanGiveQuestSourceItemIfNeed(pQuest))
        return false;

    return true;
}

bool Player::CanCompleteQuest(uint32 quest_id) const
{
    if (!quest_id)
        return false;

    QuestStatusMap::const_iterator q_itr = mQuestStatus.find(quest_id);

    // some quests can be auto taken and auto completed in one step (部分任务可一步自动接取并立即完成)
    QuestStatus status = q_itr != mQuestStatus.end() ? q_itr->second.m_status : QUEST_STATUS_NONE;

    if (status == QUEST_STATUS_COMPLETE)
        return false;                                       // not allow re-complete quest (禁止重复完成非可重复任务)

    Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id);

    if (!qInfo)
        return false;

    // only used for "flag" quests and not real in-game quests (仅用于内部标记型任务，非游戏内可见任务)
    if (qInfo->HasQuestFlag(QUEST_FLAGS_AUTO_REWARDED))
    {
        // a few checks, not all "satisfy" is needed (仅执行少量必要条件校验)
        return SatisfyQuestPreviousQuest(qInfo, false) && SatisfyQuestLevel(qInfo, false) && SatisfyQuestSkill(qInfo, false) && SatisfyQuestCondition(qInfo, false) && SatisfyQuestRace(qInfo, false) && SatisfyQuestClass(qInfo, false);
    }

    // auto complete quest (自动完成型任务直接判定可完成)
    if (qInfo->IsAutoComplete() && CanTakeQuest(qInfo, false))
        return true;

    if (status != QUEST_STATUS_INCOMPLETE)
        return false;

    // incomplete quest have status data (校验所有击杀、物品、声望、金币等任务目标是否达成)
    QuestStatusData const& q_status = q_itr->second;

    if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            if (qInfo->ReqItemCount[i] != 0 && q_status.m_itemcount[i] < qInfo->ReqItemCount[i])
                return false;
        }
    }

    if (qInfo->HasSpecialFlag(QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO)))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            if (qInfo->ReqCreatureOrGOId[i] == 0)
                continue;

            if (qInfo->ReqCreatureOrGOCount[i] != 0 && q_status.m_creatureOrGOcount[i] < qInfo->ReqCreatureOrGOCount[i])
                return false;
        }
    }

    if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT) && !q_status.m_explored)
        return false;

    if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED) && q_status.m_timer == 0)
        return false;

    if (qInfo->GetRewOrReqMoney() < 0)
    {
        if (GetMoney() < uint32(-qInfo->GetRewOrReqMoney()))
            return false;
    }

    uint32 repFacId = qInfo->GetRepObjectiveFaction();
    return !(repFacId && GetReputationMgr().GetReputation(repFacId) < qInfo->GetRepObjectiveValue());
}

bool Player::CanCompleteRepeatableQuest(Quest const* pQuest) const
{
    // Solve problem that player don't have the quest and try complete it. (处理玩家未接取直接交付的可重复任务)
    // if repeatable she must be able to complete event if player don't have it. (可重复交付任务无需先保存在任务日志中)
    // Seem that all repeatable quest are DELIVER Flag so, no need to add more. (所有可重复任务均带交付标记，校验交付物与条件即可)
    if (!CanTakeQuest(pQuest, false))
        return false;

    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (pQuest->ReqItemId[i] && pQuest->ReqItemCount[i] && !HasItemCount(pQuest->ReqItemId[i], pQuest->ReqItemCount[i]))
                return false;

    if (!CanRewardQuest(pQuest, false))
        return false;

    return true;
}

bool Player::CanRewardQuest(Quest const* pQuest, bool msg) const
{
    // not auto complete quest and not completed quest (only cheating case, then ignore without message) (既非自动完成又未完成的任务禁止领奖，防作弊)
    if (!pQuest->IsAutoComplete() && GetQuestStatus(pQuest->GetQuestId()) != QUEST_STATUS_COMPLETE)
        return false;

    // daily quest can't be rewarded (25 daily quest already completed) (今日已完成 25 个日常任务上限，不可继续领奖)
    if (!SatisfyQuestDay(pQuest, true) || !SatisfyQuestWeek(pQuest) || !SatisfyQuestMonth(pQuest))
        return false;

    // rewarded and not repeatable quest (only cheating case, then ignore without message) (非可重复任务已领奖过不可再领，防作弊)
    if (GetQuestRewardStatus(pQuest->GetQuestId()))
        return false;

    // prevent receive reward with quest items in bank (防止任务需求物品存放在银行时直接交任务领奖)
    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            if (pQuest->ReqItemCount[i] != 0 &&
                    GetItemCount(pQuest->ReqItemId[i]) < pQuest->ReqItemCount[i])
            {
                if (msg)
                    SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr, 0, pQuest->ReqItemId[i]);

                return false;
            }
        }
    }

    // prevent receive reward with low money and GetRewOrReqMoney() < 0 (任务需要扣除金币时校验玩家金币是否足够)
    if (pQuest->GetRewOrReqMoney() < 0 && GetMoney() < uint32(-pQuest->GetRewOrReqMoney()))
        return false;

    return true;
}

bool Player::CanRewardQuest(Quest const* pQuest, uint32 reward, bool msg) const
{
    // prevent receive reward with quest items in bank or for not completed quest (防作弊校验：物品在银行或任务未完成时禁止领奖)
    if (!CanRewardQuest(pQuest, msg))
        return false;

    ItemPosCountVec dest;
    if (pQuest->GetRewChoiceItemsCount() > 0)
    {
        if (pQuest->RewChoiceItemId[reward])
        {
            InventoryResult res = CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, pQuest->RewChoiceItemId[reward], pQuest->RewChoiceItemCount[reward]);
            if (res != EQUIP_ERR_OK)
            {
                SendEquipError(res, nullptr, nullptr, 0, pQuest->RewChoiceItemId[reward]);
                return false;
            }
        }
    }

    if (pQuest->GetRewItemsCount() > 0)
    {
        for (uint32 i = 0; i < pQuest->GetRewItemsCount(); ++i)
        {
            if (pQuest->RewItemId[i])
            {
                InventoryResult res = CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, pQuest->RewItemId[i], pQuest->RewItemCount[i]);
                if (res != EQUIP_ERR_OK)
                {
                    SendEquipError(res, nullptr, nullptr);
                    return false;
                }
            }
        }
    }

    return true;
}

void Player::SendPetTameFailure(PetTameFailureReason reason) const
{
    WorldPacket data(SMSG_PET_TAME_FAILURE, 1);
    data << uint8(reason);
    GetSession()->SendPacket(data);
}

/// 接取任务：分配任务日志槽位、发放起始物品并触发接取脚本
void Player::AddQuest(Quest const* pQuest, Object* questGiver)
{
    uint32 quest_id = pQuest->GetQuestId();

    // a failed quest taken again keeps its quest log slot, which SetQuestSlot below resets (重接失败任务时复用并重置原任务日志槽位)
    uint16 log_slot = FindQuestSlot(quest_id);
    if (log_slot >= MAX_QUEST_LOG_SIZE)
        log_slot = FindQuestSlot(0);
    MANGOS_ASSERT(log_slot < MAX_QUEST_LOG_SIZE);

    // if not exist then created with set uState==NEW and rewarded=false (若状态条目不存在则新建并初始化)
    QuestStatusData& questStatusData = mQuestStatus[quest_id];

    // check for repeatable quests status reset (若为可重复任务再次接取，重置探索与进度计数)
    questStatusData.m_status = QUEST_STATUS_INCOMPLETE;
    questStatusData.m_explored = false;

    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (unsigned int& i : questStatusData.m_itemcount)
            i = 0;
    }

    if (pQuest->HasSpecialFlag(QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO)))
    {
        for (unsigned int& i : questStatusData.m_creatureOrGOcount)
            i = 0;
    }

    if (pQuest->GetRepObjectiveFaction())
        if (FactionEntry const* factionEntry = sFactionStore.LookupEntry<FactionEntry>(pQuest->GetRepObjectiveFaction()))
            GetReputationMgr().SetVisible(factionEntry);

    uint32 qtime = 0;
    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
    {
        uint32 limittime = pQuest->GetLimitTime();

        // shared timed quest (队友共享的限时任务同步剩余时间)
        if (questGiver && questGiver->GetTypeId() == TYPEID_PLAYER)
            limittime = ((Player*)questGiver)->getQuestStatusMap()[quest_id].m_timer / IN_MILLISECONDS;

        AddTimedQuest(quest_id);
        questStatusData.m_timer = limittime * IN_MILLISECONDS;
        qtime = static_cast<uint32>(time(nullptr)) + limittime;
    }
    else
        questStatusData.m_timer = 0;

    SetQuestSlot(log_slot, quest_id, qtime);

    if (questStatusData.uState != QUEST_NEW)
        questStatusData.uState = QUEST_CHANGED;

    // quest accept scripts (通知区域脚本玩家已接取任务)
#ifdef ENABLE_PLAYERBOTS
    if (questGiver && this != questGiver)
#else
    if (questGiver)
#endif
    {
        switch (questGiver->GetTypeId())
        {
            case TYPEID_UNIT:
                sScriptDevAIMgr.OnQuestAccept(this, (Creature*)questGiver, pQuest);
                break;
            case TYPEID_ITEM:
            case TYPEID_CONTAINER:
                sScriptDevAIMgr.OnQuestAccept(this, (Item*)questGiver, pQuest);
                break;
            case TYPEID_GAMEOBJECT:
                sScriptDevAIMgr.OnQuestAccept(this, (GameObject*)questGiver, pQuest);
                break;
        }

        // starting initial DB quest script (执行数据库配置的任务接取起始脚本)
        if (pQuest->GetQuestStartScript() != 0)
            GetMap()->ScriptsStart(SCRIPT_TYPE_QUEST_START, pQuest->GetQuestStartScript(), questGiver, this, Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE);
    }

    // remove start item if not need (按需消耗或销毁任务起始触发物品)
    if (questGiver && questGiver->isType(TYPEMASK_ITEM))
    {
        // destroy not required for quest finish quest starting item (销毁非任务完成必需的任务起始物品)
        bool notRequiredItem = true;
        for (unsigned int i : pQuest->ReqItemId)
        {
            if (i == questGiver->GetEntry())
            {
                notRequiredItem = false;
                break;
            }
        }

        if (pQuest->GetSrcItemId() == questGiver->GetEntry())
            notRequiredItem = false;

        if (notRequiredItem)
            DestroyItem(((Item*)questGiver)->GetBagSlot(), ((Item*)questGiver)->GetSlot(), true);
    }

    GiveQuestSourceItemIfNeed(pQuest);

    AdjustQuestReqItemCount(pQuest, questStatusData);

    // Some spells applied at quest activation (刷新随任务接取状态生效的光环)
    uint32 zone, area;
    GetZoneAndAreaId(zone, area);
    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(zone);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, true);
    if (area != zone)
    {
        saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(area);
        for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
            itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, true);
    }
    saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(0);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, true);

    UpdateForQuestWorldObjects();
}

void Player::CompleteQuest(uint32 quest_id)
{
    if (quest_id)
    {
        SetQuestStatus(quest_id, QUEST_STATUS_COMPLETE);

        if (Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id))
        {
            // The client reads the complete state of a quest log slot as the event or area objective (客户端将任务日志槽位的完成标志位视为事件/探索目标已完成，)
            // of the quests flagged with one; the other objectives it counts itself. (其他击杀或收集目标由客户端自行计数)
            uint16 log_slot = FindQuestSlot(quest_id);
            if (log_slot < MAX_QUEST_LOG_SIZE && qInfo->HasQuestFlag(QuestFlags(QUEST_FLAGS_PARTY_ACCEPT | QUEST_FLAGS_EXPLORATION)))
                SetQuestSlotState(log_slot, QUEST_STATE_COMPLETE);

            if (qInfo->HasQuestFlag(QUEST_FLAGS_AUTO_REWARDED))
                RewardQuest(qInfo, 0, this, false);
        }
    }
}

void Player::IncompleteQuest(uint32 quest_id)
{
    if (quest_id)
    {
        SetQuestStatus(quest_id, QUEST_STATUS_INCOMPLETE);

        // The complete state of the quest log slot is the event objective, which stays done once the event happened. (任务日志槽位完成标记对应事件目标，触发后保持完成)
        QuestStatusMap::const_iterator itr = mQuestStatus.find(quest_id);
        bool eventDone = itr != mQuestStatus.end() && itr->second.m_explored;

        uint16 log_slot = FindQuestSlot(quest_id);
        if (log_slot < MAX_QUEST_LOG_SIZE && !eventDone)
            RemoveQuestSlotState(log_slot, QUEST_STATE_COMPLETE);
    }
}

/// 交付任务并领取经验、金币、物品、声望、荣誉与称号奖励
void Player::RewardQuest(Quest const* pQuest, uint32 reward, Object* questGiver, bool announce)
{
    uint32 quest_id = pQuest->GetQuestId();

    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        if (pQuest->ReqItemId[i])
            DestroyItemCount(pQuest->ReqItemId[i], pQuest->ReqItemCount[i], true);
    }

    for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
    {
        if (pQuest->ReqSourceId[i])
        {
            ItemPrototype const* iProto = ObjectMgr::GetItemPrototype(pQuest->ReqSourceId[i]);
            if (iProto && iProto->Bonding == BIND_QUEST_ITEM)
                DestroyItemCount(pQuest->ReqSourceId[i], pQuest->ReqSourceCount[i], true, false, true);
        }
    }

    RemoveTimedQuest(quest_id);

    if (pQuest->GetRewChoiceItemsCount() > 0)
    {
        if (uint32 itemId = pQuest->RewChoiceItemId[reward])
        {
            ItemPosCountVec dest;
            if (CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, pQuest->RewChoiceItemCount[reward]) == EQUIP_ERR_OK)
            {
                Item* item = StoreNewItem(dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
                SendNewItem(item, pQuest->RewChoiceItemCount[reward], true, false, false, false);
            }
        }
    }

    if (pQuest->GetRewItemsCount() > 0)
    {
        for (uint32 i = 0; i < pQuest->GetRewItemsCount(); ++i)
        {
            if (uint32 itemId = pQuest->RewItemId[i])
            {
                ItemPosCountVec dest;
                if (CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, pQuest->RewItemCount[i]) == EQUIP_ERR_OK)
                {
                    Item* item = StoreNewItem(dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
                    SendNewItem(item, pQuest->RewItemCount[i], true, false, false, false);
                }
            }
        }
    }

    RewardReputation(pQuest);

    uint16 log_slot = FindQuestSlot(quest_id);
    if (log_slot < MAX_QUEST_LOG_SIZE)
        SetQuestSlot(log_slot, 0);

    QuestStatusData& q_status = mQuestStatus[quest_id];

    // Used for client inform but rewarded only in case not max level (发送奖励经验值通知，满级后实际经验折算为金币)
    uint32 xp = uint32(pQuest->XPValue(this) * GetMap()->GetXPModRate(RateModType::QUEST));

    if (GetLevel() < GetMaxAttainableLevel())
        GiveXP(xp, nullptr);
    else
        ModifyMoney(int32(pQuest->GetRewMoneyMaxLevel() * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY)));

    // Give player extra money if GetRewOrReqMoney > 0 and get ReqMoney if negative (根据任务配置发放奖励金币或扣除所需金币)
    ModifyMoney(pQuest->GetRewOrReqMoney());

    // honor reward (发放任务奖励荣誉点数)
    uint32 honor = 0;
    if (pQuest->GetRewHonorableKills())
    {
        honor = MaNGOS::Honor::hk_honor_at_level(GetLevel(), pQuest->GetRewHonorableKills());
        RewardHonor(nullptr, 0, honor);
    }

    // title reward (授予任务奖励称号)
    if (pQuest->GetCharTitleId())
    {
        if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(pQuest->GetCharTitleId()))
            SetTitle(titleEntry);
    }

    // Send reward mail (按延迟配置发送任务后续奖励邮件)
    if (uint32 mail_template_id = pQuest->GetRewMailTemplateId())
        MailDraft(mail_template_id).SendMailTo(this, questGiver, MAIL_CHECK_MASK_HAS_BODY, pQuest->GetRewMailDelaySecs());

    if (pQuest->IsDaily())
        SetDailyQuestStatus(quest_id);

    if (pQuest->IsWeekly())
        SetWeeklyQuestStatus(quest_id);

    if (pQuest->IsMonthly())
        SetMonthlyQuestStatus(quest_id);

    if (!pQuest->IsRepeatable())
        SetQuestStatus(quest_id, QUEST_STATUS_COMPLETE);
    else
        SetQuestStatus(quest_id, QUEST_STATUS_NONE);

    q_status.m_rewarded = true;
    if (q_status.uState != QUEST_NEW)
        q_status.uState = QUEST_CHANGED;

    if (announce)
        SendQuestReward(pQuest, xp, honor);

    bool handled = false;

#ifdef ENABLE_PLAYERBOTS
    if (this != questGiver)
    {
#endif
    switch (questGiver->GetTypeId())
    {
        case TYPEID_UNIT:
            handled = sScriptDevAIMgr.OnQuestRewarded(this, (Creature*)questGiver, pQuest);
            break;
        case TYPEID_GAMEOBJECT:
            handled = sScriptDevAIMgr.OnQuestRewarded(this, (GameObject*)questGiver, pQuest);
            break;
    }
#ifdef ENABLE_PLAYERBOTS
    }

    if (this != questGiver)
    {
#endif
    if (!handled && pQuest->GetQuestCompleteScript() != 0)
        GetMap()->ScriptsStart(SCRIPT_TYPE_QUEST_END, pQuest->GetQuestCompleteScript(), questGiver, this, Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE);
#ifdef ENABLE_PLAYERBOTS
    }
#endif

    // Find spell cast on spell reward if any, then find the appropriate caster and cast it (若任务有奖励施法，查找合适施法者对玩家施放)
    uint32 spellId = pQuest->GetRewSpellCast();
    if (!spellId)
        spellId = pQuest->GetRewSpell();

    if (spellId)
    {
        if (SpellEntry const* spellProto = sSpellTemplate.LookupEntry<SpellEntry>(spellId))
        {
            Unit* caster = this;

            if (questGiver->GetTypeId() == TYPEID_UNIT)
            {
                for (uint8 i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    if (spellProto->Effect[i] == SPELL_EFFECT_LEARN_SPELL ||
                            spellProto->Effect[i] == SPELL_EFFECT_CREATE_ITEM ||
                            spellProto->EffectImplicitTargetA[i] == TARGET_UNIT ||
                            spellProto->EffectImplicitTargetA[i] == TARGET_UNIT_FRIEND)
                    {
                        caster = (Unit*)questGiver;
                        break;
                    }
                }
            }

            caster->CastSpell(this, spellProto, TRIGGERED_OLD_TRIGGERED);
        }
    }

    // remove auras from spells with quest reward state limitations (移除受任务未完成状态限制的光环)
    // Some spells applied at quest reward (更新依赖任务奖励状态的光环)
    uint32 zone, area;
    GetZoneAndAreaId(zone, area);
    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(zone);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, false);
    if (area != zone)
    {
        saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(area);
        for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
            itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, false);
    }
    saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(0);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, false);

    // resend quests status directly (立即刷新周围任务发布者头顶感叹号/问号状态)
    UpdateForQuestWorldObjects();
    SendQuestGiverStatusMultiple();
}

bool Player::IsQuestExplored(uint32 quest_id) const
{
    auto itr = mQuestStatus.find(quest_id);
    if (itr == mQuestStatus.end())
        return false;
    return (*itr).second.m_explored;
}

void Player::FailQuestForGroup(uint32 questId)
{
    if (Group* group = GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref != nullptr; ref = ref->next())
        {
            if (Player* member = ref->getSource())
            {
                if (member->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                    member->FailQuest(questId);
            }
        }
    }
    else
    {
        if (GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
            FailQuest(questId);
    }
}

void Player::FailQuest(uint32 questId)
{
    if (Quest const* quest = sObjectMgr.GetQuestTemplate(questId))
        FailQuest(quest);
}

/// 标记指定任务为失败状态并同步任务日志及终止相关定时器
void Player::FailQuest(Quest const* quest)
{
    uint32 questId = quest->GetQuestId();

    SetQuestStatus(questId, QUEST_STATUS_FAILED);

    uint16 log_slot = FindQuestSlot(questId);

    // only the state changes in the quest log: the client does not read the time of a failed quest (仅更新日志失败标志位，失败任务无需刷新计时字段)
    if (log_slot < MAX_QUEST_LOG_SIZE)
        SetQuestSlotState(log_slot, QUEST_STATE_FAIL);

    if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
    {
        QuestStatusData& q_status = mQuestStatus[questId];

        RemoveTimedQuest(questId);
        q_status.m_timer = 0;

        SendQuestTimerFailed(questId);
    }
    else
        SendQuestFailed(questId);
}

void Player::FailQuestsOnDeath()
{
    for (auto& data : mQuestStatus)
    {
        if (data.second.m_status != QUEST_STATUS_INCOMPLETE)
            continue;

        if (Quest const* quest = sObjectMgr.GetQuestTemplate(data.first))
            if (quest->HasQuestFlag(QUEST_FLAGS_STAY_ALIVE) && !CanRewardQuest(quest, false))
                FailQuest(quest);
    }
}

bool Player::SatisfyQuestSkill(Quest const* qInfo, bool msg) const
{
    uint32 skill = qInfo->GetRequiredSkill();

    // skip 0 case RequiredSkill (无需特定技能要求时跳过)
    if (skill == 0)
        return true;

    // check skill value (校验玩家是否具备该技能及足够点数)
    if (GetSkillValue(skill) < qInfo->GetRequiredSkillValue())
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestCondition(Quest const* qInfo, bool msg) const
{
    if (uint32 conditionId = qInfo->GetRequiredCondition())
    {
        bool result = sObjectMgr.IsConditionSatisfied(conditionId, this, GetMap(), nullptr, CONDITION_FROM_QUEST);

        if (!result && msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return result;
    }
    return true;
}

bool Player::SatisfyQuestLevel(Quest const* qInfo, bool msg) const
{
    uint32 level = GetLevel();
    if (level < qInfo->GetMinLevel() || level > qInfo->GetMaxLevel())
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestLog(bool msg) const
{
    // exist free slot (检查任务日志是否有空余槽位)
    if (FindQuestSlot(0) < MAX_QUEST_LOG_SIZE)
        return true;

    if (msg)
    {
        WorldPacket data(SMSG_QUESTLOG_FULL, 0);
        GetSession()->SendPacket(data);
        DEBUG_LOG("WORLD: Sent SMSG_QUESTLOG_FULL");
    }
    return false;
}

bool Player::SatisfyQuestPreviousQuest(Quest const* qInfo, bool msg) const
{
    // No previous quest (might be first quest in a series) (无前置任务要求，可能为任务线首环)
    if (qInfo->prevQuests.empty())
        return true;

    for (Quest::PrevQuests::const_iterator iter = qInfo->prevQuests.begin(); iter != qInfo->prevQuests.end(); ++iter)
    {
        uint32 prevId = abs(*iter);

        QuestStatusMap::const_iterator i_prevstatus = mQuestStatus.find(prevId);
        Quest const* qPrevInfo = sObjectMgr.GetQuestTemplate(prevId);

        if (qPrevInfo && i_prevstatus != mQuestStatus.end())
        {
            // If any of the positive previous quests completed, return true (正值前置任务任一已完成即可接取)
            if (*iter > 0 && i_prevstatus->second.m_rewarded)
            {
                // skip one-from-all exclusive group (正值互斥组表示“完成其一即可”，跳过全检)
                if (qPrevInfo->GetExclusiveGroup() >= 0)
                    return true;

                // each-from-all exclusive group ( < 0) (负值互斥组表示“组内全部前置均须完成”)
                // given a group with 2+ quests, and one of those has a branch that is not restricted by the group, return true (若当前查询任务有独立分支限制则校验同组完成情况)
                if (qInfo->GetPrevQuestId() != 0 && qPrevInfo->GetNextQuestId() != qInfo->GetPrevQuestId())
                    return true;

                // can be start if only all quests in prev quest exclusive group completed and rewarded (仅当互斥组内全部前置任务均已完成并领奖时才可接取)
                ExclusiveQuestGroupsMapBounds bounds = sObjectMgr.GetExclusiveQuestGroupsMapBounds(qPrevInfo->GetExclusiveGroup());

                MANGOS_ASSERT(bounds.first != bounds.second); // always must be found if qPrevInfo->ExclusiveGroup != 0 (互斥组映射条目必存在)

                for (ExclusiveQuestGroupsMap::const_iterator iter2 = bounds.first; iter2 != bounds.second; ++iter2)
                {
                    uint32 exclude_Id = iter2->second;

                    // skip checked quest id, only state of other quests in group is interesting (跳过已校验的前置任务自身，只检查同组其他任务)
                    if (exclude_Id == prevId)
                        continue;

                    QuestStatusMap::const_iterator i_exstatus = mQuestStatus.find(exclude_Id);

                    // alternative quest from group also must be completed and rewarded(reported) (同组其他前置任务也必须已完成并领奖)
                    if (i_exstatus == mQuestStatus.end() || !i_exstatus->second.m_rewarded)
                    {
                        if (msg)
                            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

                        return false;
                    }
                }
                return true;
            }
            // If any of the negative previous quests active, return true (负值前置任务表示目标任务激活期间可接取)
            if (*iter < 0 && IsCurrentQuest(prevId))
            {
                // skip one-from-all exclusive group (跳过正值互斥组)
                if (qPrevInfo->GetExclusiveGroup() >= 0)
                    return true;

                // each-from-all exclusive group ( < 0) (负值互斥组要求同组前置均处于激活状态)
                // given a group with 2+ quests, and one of those has a branch that is not restricted by the group, return true (校验负值互斥组分支限制)
                if (qInfo->GetPrevQuestId() != 0 && qPrevInfo->GetNextQuestId() != abs(qInfo->GetPrevQuestId()))
                    return true;

                // can be start if only all quests in prev quest exclusive group active (仅当互斥组内全部前置任务均已接取激活时才可接取)
                ExclusiveQuestGroupsMapBounds bounds = sObjectMgr.GetExclusiveQuestGroupsMapBounds(qPrevInfo->GetExclusiveGroup());

                MANGOS_ASSERT(bounds.first != bounds.second); // always must be found if qPrevInfo->ExclusiveGroup != 0 (互斥组映射条目必存在)

                for (ExclusiveQuestGroupsMap::const_iterator iter2 = bounds.first; iter2 != bounds.second; ++iter2)
                {
                    uint32 exclude_Id = iter2->second;

                    // skip checked quest id, only state of other quests in group is interesting (跳过已检查任务自身)
                    if (exclude_Id == prevId)
                        continue;

                    // alternative quest from group also must be active (同组其他任务也必须处于接取未交状态)
                    if (!IsCurrentQuest(exclude_Id))
                    {
                        if (msg)
                            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

                        return false;
                    }
                }
                return true;
            }
        }
    }

    // Has only positive prev. quests in non-rewarded state (正值前置任务均未完成领奖，)
    // and negative prev. quests in non-active state (且负值前置任务均未激活，不满足前置条件)
    if (msg)
        SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

    return false;
}

bool Player::SatisfyQuestBreadcrumbQuest(Quest const* qInfo, bool msg) const
{
    // If the target quest is not available, then we cannot accept the breadcrumb (若目标引导任务已不可接/已完成，则无法再接指引任务)
    uint32 breadcrumbTargetQuestId = qInfo->GetBreadcrumbForQuestId();
    if (!breadcrumbTargetQuestId)
        return true;

    if (!CanTakeQuest(sObjectMgr.GetQuestTemplate(breadcrumbTargetQuestId), false))
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestDependentBreadcrumbQuests(Quest const* qInfo, bool msg) const
{
    // If we have one of the breadcrumb quests, then we can't accept the target quest (若已接取或完成对应指引任务，则不再重复显示关联校验)
    for (uint32 breadcrumbQuestId : qInfo->DependentBreadcrumbQuests)
    {
        QuestStatus status = GetQuestStatus(breadcrumbQuestId);
        if ((status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_FAILED) && !GetQuestRewardStatus(breadcrumbQuestId))
        {
            if (msg)
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

            return false;
        }
    }

    return true;
}

bool Player::SatisfyQuestClass(Quest const* qInfo, bool msg) const
{
    uint32 reqClass = qInfo->GetRequiredClasses();

    if (reqClass == 0)
        return true;

    if ((reqClass & getClassMask()) == 0)
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestRace(Quest const* qInfo, bool msg) const
{
    uint32 reqraces = qInfo->GetRequiredRaces();

    if (reqraces == 0)
        return true;

    if ((reqraces & getRaceMask()) == 0)
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_FAILED_WRONG_RACE);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestReputation(Quest const* qInfo, bool msg) const
{
    uint32 fIdMin = qInfo->GetRequiredMinRepFaction();      // Min required rep (校验最低声望要求)
    if (fIdMin && GetReputationMgr().GetReputation(fIdMin) < qInfo->GetRequiredMinRepValue())
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    uint32 fIdMax = qInfo->GetRequiredMaxRepFaction();      // Max required rep (校验最高声望上限要求)
    if (fIdMax && GetReputationMgr().GetReputation(fIdMax) >= qInfo->GetRequiredMaxRepValue())
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestStatus(Quest const* qInfo, bool msg) const
{
    QuestStatusMap::const_iterator itr = mQuestStatus.find(qInfo->GetQuestId());

    // a failed quest can be taken again from its quest giver (已失败任务可从发布者处重新接取)
    if (itr != mQuestStatus.end() && itr->second.m_status != QUEST_STATUS_NONE && itr->second.m_status != QUEST_STATUS_FAILED)
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_ALREADY_ON);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestTimed(Quest const* qInfo, bool msg) const
{
    if (!m_timedquests.empty() && qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_ONLY_ONE_TIMED);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestExclusiveGroup(Quest const* qInfo, bool msg) const
{
    // non positive exclusive group, if > 0 then can be start if any other quest in exclusive group already started/completed (校验任务互斥组：正组互斥，同组中已有任务接取或完成则不可再接)
    if (qInfo->GetExclusiveGroup() <= 0)
        return true;

    ExclusiveQuestGroupsMapBounds bounds = sObjectMgr.GetExclusiveQuestGroupsMapBounds(qInfo->GetExclusiveGroup());

    MANGOS_ASSERT(bounds.first != bounds.second);           // must always be found if qInfo->ExclusiveGroup != 0 (互斥组条目必存在)

    for (ExclusiveQuestGroupsMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        uint32 exclude_Id = iter->second;

        // skip checked quest id, only state of other quests in group is interesting (跳过当前待查任务自身)
        if (exclude_Id == qInfo->GetQuestId())
            continue;

        // not allow have daily quest if daily quest from exclusive group already recently completed (同一互斥组内今天已完成其他日常，则不可接)
        Quest const* Nquest = sObjectMgr.GetQuestTemplate(exclude_Id);
        if (!SatisfyQuestDay(Nquest, false) || !SatisfyQuestWeek(Nquest))
        {
            if (msg)
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

            return false;
        }

        QuestStatusMap::const_iterator i_exstatus = mQuestStatus.find(exclude_Id);

        // alternative quest already started or completed (同互斥组内已有其他任务正在进行或已完成)
        if (i_exstatus != mQuestStatus.end() &&
                (i_exstatus->second.m_status == QUEST_STATUS_COMPLETE || i_exstatus->second.m_status == QUEST_STATUS_INCOMPLETE))
        {
            if (msg)
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

            return false;
        }
    }

    return true;
}

bool Player::SatisfyQuestNextChain(Quest const* qInfo, bool msg) const
{
    if (!qInfo->GetNextQuestInChain())
        return true;

    // next quest in chain already started or completed (后续链任务已接取或已完成，不可回头接前置)
    QuestStatusMap::const_iterator itr = mQuestStatus.find(qInfo->GetNextQuestInChain());
    if (itr != mQuestStatus.end() &&
            (itr->second.m_status == QUEST_STATUS_COMPLETE || itr->second.m_status == QUEST_STATUS_INCOMPLETE))
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    // check for all quests further up the chain (递归检查任务链更后续任务)
    // only necessary if there are quest chains with more than one quest that can be skipped (仅当任务链允许跳环时才需递归)
    // return SatisfyQuestNextChain( qInfo->GetNextQuestInChain(), msg ); (预留递归检查)
    return true;
}

bool Player::SatisfyQuestPrevChain(Quest const* qInfo, bool msg) const
{
    // No previous quest in chain (无链式前置任务)
    if (qInfo->prevChainQuests.empty())
        return true;

    for (uint32 prevId : qInfo->prevChainQuests)
    {
        // If any of the previous quests in chain active, return false (若任务链前置任务仍在进行中，则不可跳过接后续)
        if (IsCurrentQuest(prevId))
        {
            if (msg)
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

            return false;
        }

        // check for all quests further down the chain (递归检查任务链更前置任务)
        // only necessary if there are quest chains with more than one quest that can be skipped (仅当允许跳环时需递归)
        // if( !SatisfyQuestPrevChain( prevId, msg ) )
        //    return false;
    }

    // No previous quest in chain active (无正在进行中的链式前置任务)
    return true;
}

bool Player::SatisfyQuestDay(Quest const* qInfo, bool msg) const
{
    if (!qInfo->IsDaily())
        return true;

    bool have_slot = false;
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
    {
        uint32 id = GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx);
        if (qInfo->GetQuestId() == id)
            return false;

        if (!id)
            have_slot = true;
    }

    if (!have_slot)
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DAILY_QUESTS_REMAINING);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestWeek(Quest const* qInfo) const
{
    if (!qInfo->IsWeekly() || m_weeklyquests.empty())
        return true;

    // if not found in cooldown list (当前本周未完成过此周常任务)
    return m_weeklyquests.find(qInfo->GetQuestId()) == m_weeklyquests.end();
}

bool Player::SatisfyQuestMonth(Quest const* qInfo) const
{
    if (!qInfo->IsMonthly() || m_monthlyquests.empty())
        return true;

    // if not found in cooldown list (当前本月未完成过此月常任务)
    return m_monthlyquests.find(qInfo->GetQuestId()) == m_monthlyquests.end();
}

bool Player::CanGiveQuestSourceItemIfNeed(Quest const* pQuest, ItemPosCountVec* dest) const
{
    if (uint32 srcitem = pQuest->GetSrcItemId())
    {
        uint32 count = pQuest->GetSrcItemCount();

        // player already have max amount required item (including bank), just report success (玩家背包与银行中已有足量任务触发物品，跳过发放)
        uint32 has_count = GetItemCount(srcitem, true);
        if (has_count >= count)
            return true;

        count -= has_count;                                 // real need amount (计算实际需补充发放的数量)

        InventoryResult msg;
        if (!dest)
        {
            ItemPosCountVec destTemp;
            msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, destTemp, srcitem, count);
        }
        else
            msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, *dest, srcitem, count);

        if (msg == EQUIP_ERR_OK)
            return true;
        SendEquipError(msg, nullptr, nullptr, srcitem);
        return false;
    }

    return true;
}

void Player::GiveQuestSourceItemIfNeed(Quest const* pQuest)
{
    ItemPosCountVec dest;
    if (CanGiveQuestSourceItemIfNeed(pQuest, &dest) && !dest.empty())
    {
        uint32 count = 0;
        for (ItemPosCountVec::const_iterator c_itr = dest.begin(); c_itr != dest.end(); ++c_itr)
            count += c_itr->count;

        Item* item = StoreNewItem(dest, pQuest->GetSrcItemId(), true);
        SendNewItem(item, count, true, false);
    }
}


bool Player::TakeQuestSourceItem(uint32 quest_id, bool msg)
{
    Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id);
    if (qInfo)
    {
        uint32 srcitem = qInfo->GetSrcItemId();
        if (srcitem > 0)
        {
            uint32 count = qInfo->GetSrcItemCount();
            if (count <= 0)
                count = 1;

            // exist one case when destroy source quest item not possible: (放弃任务销毁起始物品时存在特例：)
            // non un-equippable item (equipped non-empty bag, for example) (例如已装备的非空背包无法直接销毁)
            InventoryResult res = CanUnequipItems(srcitem, count);
            if (res != EQUIP_ERR_OK)
            {
                if (msg)
                    SendEquipError(res, nullptr, nullptr, srcitem);
                return false;
            }

            DestroyItemCount(srcitem, count, true, true);
        }
    }
    return true;
}

bool Player::GetQuestRewardStatus(uint32 quest_id) const
{
    Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id);
    if (qInfo)
    {
        // for repeatable quests: rewarded field is set after first reward only to prevent getting XP more than once (可重复任务首次领奖后标记 rewarded=true 以防重复获取经验)
        QuestStatusMap::const_iterator itr = mQuestStatus.find(quest_id);
        if (itr != mQuestStatus.end() && itr->second.m_status != QUEST_STATUS_NONE
                && !qInfo->IsRepeatable())
            return itr->second.m_rewarded;

        return false;
    }
    return false;
}

QuestStatus Player::GetQuestStatus(uint32 quest_id) const
{
    if (quest_id)
    {
        QuestStatusMap::const_iterator itr = mQuestStatus.find(quest_id);
        if (itr != mQuestStatus.end())
            return itr->second.m_status;
    }
    return QUEST_STATUS_NONE;
}

bool Player::CanShareQuest(uint32 quest_id) const
{
    if (Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id))
        if (qInfo->HasQuestFlag(QUEST_FLAGS_SHARABLE))
            return IsCurrentQuest(quest_id);

    return false;
}

void Player::SetQuestStatus(uint32 quest_id, QuestStatus status)
{
    if (sObjectMgr.GetQuestTemplate(quest_id))
    {
        QuestStatusData& q_status = mQuestStatus[quest_id];

        q_status.m_status = status;

        if (q_status.uState != QUEST_NEW)
            q_status.uState = QUEST_CHANGED;
    }

    UpdateForQuestWorldObjects();
}

// not used in MaNGOS, but used in scripting code (核心未直接调用，预留给外部脚本修改任务状态)
uint32 Player::GetReqKillOrCastCurrentCount(uint32 quest_id, int32 entry) const
{
    Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id);
    if (!qInfo)
        return 0;

    for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
    {
        if (qInfo->ReqCreatureOrGOId[j] == entry)
        {
            auto itr = mQuestStatus.find(quest_id);
            if (itr != mQuestStatus.end())
                return itr->second.m_creatureOrGOcount[j];
        }
    }

    return 0;
}

void Player::AdjustQuestReqItemCount(Quest const* pQuest, QuestStatusData& questStatusData)
{
    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            uint32 reqitemcount = pQuest->ReqItemCount[i];
            if (reqitemcount != 0)
            {
                uint32 curitemcount = GetItemCount(pQuest->ReqItemId[i], true);

                questStatusData.m_itemcount[i] = std::min(curitemcount, reqitemcount);
                if (questStatusData.uState != QUEST_NEW) questStatusData.uState = QUEST_CHANGED;
            }
        }
    }
}

uint16 Player::FindQuestSlot(uint32 quest_id) const
{
    for (uint16 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
        if (GetQuestSlotQuestId(i) == quest_id)
            return i;

    return MAX_QUEST_LOG_SIZE;
}

void Player::AreaExploredOrEventHappens(uint32 questId)
{
    if (questId)
    {
        uint16 log_slot = FindQuestSlot(questId);
        if (log_slot < MAX_QUEST_LOG_SIZE)
        {
            QuestStatusData& q_status = mQuestStatus[questId];

            // a failed quest does not progress any more (已失败的任务不再推进探索/事件进度)
            if (!q_status.m_explored && q_status.m_status != QUEST_STATUS_FAILED)
            {
                SetQuestSlotState(log_slot, QUEST_STATE_COMPLETE);
                SendQuestCompleteEvent(questId);
                q_status.m_explored = true;

                if (q_status.uState != QUEST_NEW)
                    q_status.uState = QUEST_CHANGED;
            }
        }
        if (CanCompleteQuest(questId))
            CompleteQuest(questId);
    }
}

// not used in mangosd, function for external script library (供外部脚本库调用，为玩家及周围队友同步事件完成状态)
void Player::RewardPlayerAndGroupAtEventExplored(uint32 questId, WorldObject const* pEventObject)
{
    if (Group* pGroup = GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* pGroupGuy = itr->getSource();

            // for any leave or dead (with not released body) group member at appropriate distance (遍历符合距离的存活或未释放灵魂的队友)
            if (pGroupGuy && pGroupGuy->IsAtGroupRewardDistance(pEventObject) && !pGroupGuy->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                pGroupGuy->AreaExploredOrEventHappens(questId);
        }
    }
    else
        AreaExploredOrEventHappens(questId);
}

void Player::ItemAddedQuestCheck(uint32 entry, uint32 count)
{
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (questid == 0)
            continue;

        QuestStatusData& q_status = mQuestStatus[questid];

        if (q_status.m_status != QUEST_STATUS_INCOMPLETE)
            continue;

        Quest const* qInfo = sObjectMgr.GetQuestTemplate(questid);
        if (!qInfo || !qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
            continue;

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            uint32 reqitem = qInfo->ReqItemId[j];
            if (reqitem == entry)
            {
                uint32 reqitemcount = qInfo->ReqItemCount[j];
                uint32 curitemcount = q_status.m_itemcount[j];

                if (curitemcount < reqitemcount)
                {
                    uint32 additemcount = (curitemcount + count <= reqitemcount ? count : reqitemcount - curitemcount);
                    q_status.m_itemcount[j] += additemcount;
                    if (q_status.uState != QUEST_NEW)
                        q_status.uState = QUEST_CHANGED;

                    SendQuestUpdateAddItem(qInfo, j, curitemcount, additemcount);
                }

                if (CanCompleteQuest(questid))
                    CompleteQuest(questid); // will call UpdateForQuestWorldObjects to clean sparkles for this quest on client (更新任务物体闪光状态，清除已完成任务的提示光效)
                else if (q_status.m_itemcount[j] == reqitemcount)
                    UpdateForQuestWorldObjects(); // call UpdateForQuestWorldObjects to remove sparkles from finished objective on client (刷新任务物体闪光以移除已完成收集目标的光效)
            }
        }
    }
}

void Player::ItemRemovedQuestCheck(uint32 entry, uint32 count)
{
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;
        Quest const* qInfo = sObjectMgr.GetQuestTemplate(questid);
        if (!qInfo)
            continue;
        if (!qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
            continue;

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            uint32 reqitem = qInfo->ReqItemId[j];
            if (reqitem == entry)
            {
                QuestStatusData& q_status = mQuestStatus[questid];

                uint32 reqItemCount = qInfo->ReqItemCount[j];
                uint32 curItemCount = q_status.m_itemcount[j];

                // With items you can have more than the required by the quest. The bank counts as in (背包/银行可持有超过任务所需的物品，)
                // AdjustQuestReqItemCount; only the reward needs the items in the bags. (任务计数上限按需求量封顶，仅领奖时要求全部在背包中)
                if (curItemCount >= reqItemCount)
                    curItemCount = GetItemCount(entry, true);

                // The counter is capped at the required count, so it only drops once fewer items than that are left. (只有当剩余总量低于任务需求数量时，任务进度计数才减少)
                if (curItemCount >= count && q_status.m_itemcount[j] > curItemCount - count)
                {
                    q_status.m_itemcount[j] = curItemCount - count;
                    if (q_status.uState != QUEST_NEW)
                        q_status.uState = QUEST_CHANGED;

                    // Only a complete quest goes back to incomplete: a failed quest stays failed. (仅已完成的任务会退回未完成；已失败任务保持失败)
                    if (q_status.m_status == QUEST_STATUS_COMPLETE)
                        IncompleteQuest(questid);
                    UpdateForQuestWorldObjects();
                }
            }
        }
    }
}

void Player::KilledMonster(CreatureInfo const* cInfo, Creature const* creature)
{
    ObjectGuid guid = creature ? creature->GetObjectGuid() : ObjectGuid();
    if (cInfo->Entry)
        KilledMonsterCredit(cInfo->Entry, guid);

    if (creature && creature->GetEntry() && creature->GetEntry() != cInfo->Entry)
        KilledMonsterCredit(creature->GetEntry(), guid);

    for (unsigned int i : cInfo->KillCredit)
        if (i)
            KilledMonsterCredit(i, guid);
}

/// 击杀生物或触发击杀信用点时更新任务进度并通知客户端
void Player::KilledMonsterCredit(uint32 entry, ObjectGuid guid)
{
    uint32 addkillcount = 1;

    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr.GetQuestTemplate(questid);
        if (!qInfo)
            continue;
        // just if !ingroup || !noraidgroup || raidgroup (仅在单人、小队或允许团队完成的任务中共享击杀进度)
        QuestStatusData& q_status = mQuestStatus[questid];
        if (q_status.m_status == QUEST_STATUS_INCOMPLETE && (!GetGroup() || !GetGroup()->IsRaidGroup() || qInfo->IsAllowedInRaid()))
        {
            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_KILL_OR_CAST))
            {
                for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                {
                    // skip GO activate objective or none (跳过游戏对象交互目标或空目标)
                    if (qInfo->ReqCreatureOrGOId[j] <= 0)
                        continue;

                    // skip Cast at creature objective (跳过需特定法术施放的击杀目标)
                    if (qInfo->ReqSpell[j] != 0)
                        continue;

                    uint32 reqkill = qInfo->ReqCreatureOrGOId[j];

                    if (reqkill == entry)
                    {
                        uint32 reqkillcount = qInfo->ReqCreatureOrGOCount[j];
                        uint32 curkillcount = q_status.m_creatureOrGOcount[j];
                        if (curkillcount < reqkillcount)
                        {
                            q_status.m_creatureOrGOcount[j] = curkillcount + addkillcount;
                            if (q_status.uState != QUEST_NEW)
                                q_status.uState = QUEST_CHANGED;

                            SendQuestUpdateAddCreatureOrGo(qInfo, guid, j, q_status.m_creatureOrGOcount[j]);
                        }

                        if (CanCompleteQuest(questid))
                            CompleteQuest(questid);
                    }
                }
            }
        }
    }
}

void Player::CastedCreatureOrGO(uint32 entry, ObjectGuid guid, uint32 spell_id, bool original_caster)
{
    bool isCreature = guid.IsCreature();

    uint32 addCastCount = 1;
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr.GetQuestTemplate(questid);
        if (!qInfo)
            continue;

        if (!original_caster && !qInfo->HasQuestFlag(QUEST_FLAGS_SHARABLE))
            continue;

        if (!qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_KILL_OR_CAST))
            continue;

        QuestStatusData& q_status = mQuestStatus[questid];

        if (q_status.m_status != QUEST_STATUS_INCOMPLETE)
            continue;

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            // skip kill creature objective (0) or wrong spell casts (跳过纯击杀目标或法术 ID 不匹配的施法目标)
            if (qInfo->ReqSpell[j] != spell_id)
                continue;

            uint32 reqTarget = 0;

            if (isCreature)
            {
                // creature activate objectives (对生物施法交互目标)
                if (qInfo->ReqCreatureOrGOId[j] > 0)
                    // checked at quest_template loading (已在 quest_template 加载时校验合法性)
                    reqTarget = qInfo->ReqCreatureOrGOId[j];
            }
            else
            {
                // GO activate objective (对游戏对象施法交互目标)
                if (qInfo->ReqCreatureOrGOId[j] < 0)
                    // checked at quest_template loading (已在 quest_template 加载时校验合法性)
                    reqTarget = - qInfo->ReqCreatureOrGOId[j];
            }

            // other not this creature/GO related objectives (非当前目标生物/物体的条目跳过)
            if (reqTarget != entry)
                continue;

            uint32 reqCastCount = qInfo->ReqCreatureOrGOCount[j];
            uint32 curCastCount = q_status.m_creatureOrGOcount[j];
            if (curCastCount < reqCastCount)
            {
                q_status.m_creatureOrGOcount[j] = curCastCount + addCastCount;
                if (q_status.uState != QUEST_NEW)
                    q_status.uState = QUEST_CHANGED;

                SendQuestUpdateAddCreatureOrGo(qInfo, guid, j, q_status.m_creatureOrGOcount[j]);
            }

            if (CanCompleteQuest(questid))
                CompleteQuest(questid);

            // same objective target can be in many active quests, but not in 2 objectives for single quest (code optimization). (同一目标可属于多个任务，但在单个任务中不会重复出现，故可直接 break)
            break;
        }
    }
}

void Player::TalkedToCreature(uint32 entry, ObjectGuid guid)
{
    uint32 addTalkCount = 1;
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr.GetQuestTemplate(questid);
        if (!qInfo)
            continue;

        QuestStatusData& q_status = mQuestStatus[questid];

        if (q_status.m_status == QUEST_STATUS_INCOMPLETE)
        {
            if (qInfo->HasSpecialFlag(QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO)))
            {
                for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                {
                    // skip spell casts and Gameobject objectives (跳过法术施放类与游戏对象目标)
                    if (qInfo->ReqSpell[j] > 0 || qInfo->ReqCreatureOrGOId[j] < 0)
                        continue;

                    uint32 reqTarget = qInfo->ReqCreatureOrGOId[j];
                    if (reqTarget <= 0)    // creature activate objectives (与 NPC 对话目标计数)
                        continue;

                    if (reqTarget == entry)
                    {
                        uint32 reqTalkCount = qInfo->ReqCreatureOrGOCount[j];
                        uint32 curTalkCount = q_status.m_creatureOrGOcount[j];
                        if (curTalkCount < reqTalkCount)
                        {
                            q_status.m_creatureOrGOcount[j] = curTalkCount + addTalkCount;
                            if (q_status.uState != QUEST_NEW) q_status.uState = QUEST_CHANGED;

                            SendQuestUpdateAddCreatureOrGo(qInfo, guid, j, q_status.m_creatureOrGOcount[j]);
                        }
                        if (CanCompleteQuest(questid))
                            CompleteQuest(questid);
                    }
                }
            }
        }
    }
}

void Player::MoneyChanged(uint32 count)
{
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr.GetQuestTemplate(questid);
        if (qInfo && qInfo->GetRewOrReqMoney() < 0)
        {
            QuestStatusData& q_status = mQuestStatus[questid];

            if (q_status.m_status == QUEST_STATUS_INCOMPLETE)
            {
                if (int32(count) >= -qInfo->GetRewOrReqMoney())
                {
                    if (CanCompleteQuest(questid))
                        CompleteQuest(questid);
                }
            }
            else if (q_status.m_status == QUEST_STATUS_COMPLETE)
            {
                if (int32(count) < -qInfo->GetRewOrReqMoney())
                    IncompleteQuest(questid);
            }
        }
    }
}

enum TitleFactions
{
    FACTION_LEAGUE_OF_ARATHOR       = 509,
    FACTION_STORMPIKE_GUARD         = 730,
    FACTION_SILVERWING_SENTINELS    = 890,

    FACTION_DEFILERS                = 510,
    FACTION_FROSTWOLF_CLAN          = 729,
    FACTION_WARSONG_OUTRIDERS       = 889,

    TITLE_CONQUEROR                 = 47,
    TITLE_JUSTICAR                  = 48,
};

void Player::ReputationChanged(FactionEntry const* factionEntry)
{
    ReputationMgr const& repMgr = GetReputationMgr();
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        if (uint32 questid = GetQuestSlotQuestId(i))
        {
            if (Quest const* qInfo = sObjectMgr.GetQuestTemplate(questid))
            {
                if (qInfo->GetRepObjectiveFaction() == factionEntry->ID)
                {
                    QuestStatusData& q_status = mQuestStatus[questid];
                    if (q_status.m_status == QUEST_STATUS_INCOMPLETE)
                    {
                        if (repMgr.GetReputation(factionEntry) >= qInfo->GetRepObjectiveValue())
                            if (CanCompleteQuest(questid))
                                CompleteQuest(questid);
                    }
                    else if (q_status.m_status == QUEST_STATUS_COMPLETE)
                    {
                        if (repMgr.GetReputation(factionEntry) < qInfo->GetRepObjectiveValue())
                            IncompleteQuest(questid);
                    }
                }
            }
        }
    }

    switch (factionEntry->ID)
    {
        case FACTION_LEAGUE_OF_ARATHOR:
        case FACTION_STORMPIKE_GUARD:
        case FACTION_SILVERWING_SENTINELS:
        {
            FactionEntry const* factionEntryArathor = sFactionStore.LookupEntry<FactionEntry>(FACTION_LEAGUE_OF_ARATHOR);
            if (repMgr.GetRank(factionEntryArathor) < REP_EXALTED) break;
            FactionEntry const* factionEntryStormpike = sFactionStore.LookupEntry<FactionEntry>(FACTION_STORMPIKE_GUARD);
            if (repMgr.GetRank(factionEntryStormpike) < REP_EXALTED) break;
            FactionEntry const* factionEntrySentinels = sFactionStore.LookupEntry<FactionEntry>(FACTION_SILVERWING_SENTINELS);
            if (repMgr.GetRank(factionEntrySentinels) < REP_EXALTED) break;
            SetTitle(TITLE_JUSTICAR);
            break;
        }
        case FACTION_DEFILERS:
        case FACTION_FROSTWOLF_CLAN:
        case FACTION_WARSONG_OUTRIDERS:
        {
            FactionEntry const* factionEntryDefilers = sFactionStore.LookupEntry<FactionEntry>(FACTION_DEFILERS);
            if (repMgr.GetRank(factionEntryDefilers) < REP_EXALTED) break;
            FactionEntry const* factionEntryFrostwolf = sFactionStore.LookupEntry<FactionEntry>(FACTION_FROSTWOLF_CLAN);
            if (repMgr.GetRank(factionEntryFrostwolf) < REP_EXALTED) break;
            FactionEntry const* factionEntryWarsong = sFactionStore.LookupEntry<FactionEntry>(FACTION_WARSONG_OUTRIDERS);
            if (repMgr.GetRank(factionEntryWarsong) < REP_EXALTED) break;
            SetTitle(TITLE_CONQUEROR);
            break;
        }
    }
}

bool Player::HasQuestForItem(uint32 itemid) const
{
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (questid == 0)
            continue;

        QuestStatusMap::const_iterator qs_itr = mQuestStatus.find(questid);
        if (qs_itr == mQuestStatus.end())
            continue;

        QuestStatusData const& q_status = qs_itr->second;

        if (q_status.m_status == QUEST_STATUS_INCOMPLETE)
        {
            Quest const* qinfo = sObjectMgr.GetQuestTemplate(questid);
            if (!qinfo)
                continue;

            // hide quest if player is in raid-group and quest is no raid quest (在团队中时忽略非团队任务物品需求)
            if (GetGroup() && GetGroup()->IsRaidGroup() && !qinfo->IsAllowedInRaid() && !InBattleGround())
                continue;

            // There should be no mixed ReqItem/ReqSource drop (不混用需求物品与需求来源物品掉落判断)
            // This part for ReqItem drop (处理常规任务需求物品掉落判定)
            for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
            {
                if (itemid == qinfo->ReqItemId[j] && q_status.m_itemcount[j] < qinfo->ReqItemCount[j])
                    return true;
            }
            // This part - for ReqSource (处理任务衍生来源物品掉落判定)
            for (int j = 0; j < QUEST_SOURCE_ITEM_IDS_COUNT; ++j)
            {
                // examined item is a source item (确认当前检查物品为该任务的来源物品)
                if (qinfo->ReqSourceId[j] == itemid)
                {
                    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemid);

                    // 'unique' item (唯一物品上限检查)
                    if (pProto->MaxCount && GetItemCount(itemid, true) < pProto->MaxCount)
                        return true;

                    // allows custom amount drop when not 0 (支持自定义所需来源物品数量)
                    if (qinfo->ReqSourceCount[j])
                    {
                        if (GetItemCount(itemid, true) < qinfo->ReqSourceCount[j])
                            return true;
                    }
                    else if (GetItemCount(itemid, true) < pProto->Stackable)
                        return true;
                }
            }
        }
    }
    return false;
}

// Used for quests having some event (explore, escort, "external event") as quest objective. (用于探索、护送或外部事件类任务完成通知)
void Player::SendQuestCompleteEvent(uint32 quest_id) const
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTUPDATE_COMPLETE, 4);
        data << uint32(quest_id);
        GetSession()->SendPacket(data);
        DEBUG_LOG("WORLD: Sent SMSG_QUESTUPDATE_COMPLETE quest = %u", quest_id);
    }
}

void Player::SendQuestReward(Quest const* pQuest, uint32 XP, uint32 honor) const
{
    uint32 questid = pQuest->GetQuestId();
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_COMPLETE quest = %u", questid);
    WorldPacket data(SMSG_QUESTGIVER_QUEST_COMPLETE, (4 + 4 + 4 + 4 + 4 + 4 + pQuest->GetRewItemsCount() * 8));
    data << uint32(questid);
    data << uint32(0x03);

    if (GetLevel() < GetMaxAttainableLevel())
    {
        data << uint32(XP);
        data << uint32(pQuest->GetRewOrReqMoney());
    }
    else
    {
        data << uint32(0);
        data << uint32(pQuest->GetRewOrReqMoney() + int32(pQuest->GetRewMoneyMaxLevel() * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY)));
    }
    data << uint32(honor);                                  // new 2.3.0, HonorPoints (2.3.0 新增荣誉奖励字段)
    data << uint32(pQuest->GetRewItemsCount());             // max is 5 (最多包含 5 个可选物品)

    for (uint32 i = 0; i < pQuest->GetRewItemsCount(); ++i)
    {
        if (pQuest->RewItemId[i] > 0)
            data << pQuest->RewItemId[i] << pQuest->RewItemCount[i];
        else
            data << uint32(0) << uint32(0);
    }
    GetSession()->SendPacket(data);
}

void Player::SendQuestFailed(uint32 quest_id, uint32 reason) const
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTGIVER_QUEST_FAILED, 4);
        data << uint32(quest_id);
        data << uint32(reason);
        GetSession()->SendPacket(data);
        DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_FAILED");
    }
}

void Player::SendQuestTimerFailed(uint32 quest_id) const
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTUPDATE_FAILEDTIMER, 4);
        data << uint32(quest_id);
        GetSession()->SendPacket(data);
        DEBUG_LOG("WORLD: Sent SMSG_QUESTUPDATE_FAILEDTIMER");
    }
}

void Player::SendCanTakeQuestResponse(uint32 msg) const
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_INVALID, 4);
    data << uint32(msg);
    GetSession()->SendPacket(data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_INVALID");
}

void Player::SendQuestConfirmAccept(const Quest* pQuest, Player* pReceiver) const
{
    if (pReceiver)
    {
        int loc_idx = pReceiver->GetSession()->GetSessionDbLocaleIndex();
        std::string title = pQuest->GetTitle();
        sObjectMgr.GetQuestLocaleStrings(pQuest->GetQuestId(), loc_idx, &title);

        WorldPacket data(SMSG_QUEST_CONFIRM_ACCEPT, (4 + title.size() + 8));
        data << uint32(pQuest->GetQuestId());
        data << title;
        data << GetObjectGuid();
        pReceiver->GetSession()->SendPacket(data);

        DEBUG_LOG("WORLD: Sent SMSG_QUEST_CONFIRM_ACCEPT");
    }
}

void Player::SendPushToPartyResponse(Player* pPlayer, QuestShareMessages msg) const
{
    if (pPlayer)
    {
        WorldPacket data(MSG_QUEST_PUSH_RESULT, (8 + 1));
        data << pPlayer->GetObjectGuid();
        data << uint8(msg);                                 // valid values: 0-8 (任务共享反馈有效状态值 0-8)
        GetSession()->SendPacket(data);
        DEBUG_LOG("WORLD: Sent MSG_QUEST_PUSH_RESULT");
    }
}

void Player::SendQuestUpdateAddItem(Quest const* pQuest, uint32 item_idx, uint32 /*current*/, uint32 count)
{
    MANGOS_ASSERT(count < 256 && "Quest slot count store is limited to 8 bits 2^8 = 256 (0..255)");

    // Update quest watcher and fire QUEST_WATCH_UPDATE (更新任务追踪器并触发 QUEST_WATCH_UPDATE)
    DEBUG_LOG("WORLD: Sent SMSG_QUESTUPDATE_ADD_ITEM");
    WorldPacket data(SMSG_QUESTUPDATE_ADD_ITEM, (4 + 4));
    data << pQuest->ReqItemId[item_idx];
    data << count;
    GetSession()->SendPacket(data);
}

void Player::SendQuestUpdateAddCreatureOrGo(Quest const* pQuest, ObjectGuid guid, uint32 creatureOrGO_idx, uint32 count)
{
    MANGOS_ASSERT(count < 256 && "Quest slot count store is limited to 8 bits 2^8 = 256 (0..255)");

    int32 entry = pQuest->ReqCreatureOrGOId[ creatureOrGO_idx ];
    if (entry < 0)
        // client expected gameobject template id in form (id|0x80000000) (客户端约定：游戏对象模板 ID 需带最高位 0x80000000 标记)
        entry = (-entry) | 0x80000000;

    // Update quest watcher and fire QUEST_WATCH_UPDATE (更新任务追踪器并触发 QUEST_WATCH_UPDATE)
    WorldPacket data(SMSG_QUESTUPDATE_ADD_KILL, (4 * 4 + 8));
    DEBUG_LOG("WORLD: Sent SMSG_QUESTUPDATE_ADD_KILL");
    data << uint32(pQuest->GetQuestId());
    data << uint32(entry);
    data << uint32(count);
    data << uint32(pQuest->ReqCreatureOrGOCount[ creatureOrGO_idx ]);
    data << guid;
    GetSession()->SendPacket(data);

    // Update player field and fire UNIT_QUEST_LOG_CHANGED for self (通知客户端刷新任务日志变更)
    uint16 slot = FindQuestSlot(pQuest->GetQuestId());
    if (slot < MAX_QUEST_LOG_SIZE)
        SetQuestSlotCounter(slot, uint8(creatureOrGO_idx), uint8(count));
}

void Player::SendQuestGiverStatusMultiple() const
{
    uint32 count = 0;

    WorldPacket data(SMSG_QUESTGIVER_STATUS_MULTIPLE, 4);
    data << uint32(count);                                  // placeholder (任务发布者数量占位符)

    for (auto m_clientGUID : m_clientGUIDs)
    {
        if (m_clientGUID.IsAnyTypeCreature())
        {
            // need also pet quests case support (后续可扩展支持宠物提供任务状态)
            Creature* questgiver = GetMap()->GetAnyTypeCreature(m_clientGUID);

            if (!questgiver || !CanInteract(questgiver))
                continue;

            if (!questgiver->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER))
                continue;

            uint8 dialogStatus = sScriptDevAIMgr.GetDialogStatus(this, questgiver);

            if (dialogStatus == DIALOG_STATUS_UNDEFINED)
                dialogStatus = GetSession()->getDialogStatus(this, questgiver, DIALOG_STATUS_NONE);

            data << questgiver->GetObjectGuid();
            data << uint8(dialogStatus);
            ++count;
        }
        else if (m_clientGUID.IsGameObject())
        {
            GameObject* questgiver = GetMap()->GetGameObject(m_clientGUID);

            if (!questgiver)
                continue;

            if (questgiver->GetGoType() != GAMEOBJECT_TYPE_QUESTGIVER)
                continue;

            uint8 dialogStatus = sScriptDevAIMgr.GetDialogStatus(this, questgiver);

            if (dialogStatus == DIALOG_STATUS_UNDEFINED)
                dialogStatus = GetSession()->getDialogStatus(this, questgiver, DIALOG_STATUS_NONE);

            data << questgiver->GetObjectGuid();
            data << uint8(dialogStatus);
            ++count;
        }
    }

    data.put<uint32>(0, count);                             // write real count (回填实际查询的 NPC/物体数量)
    GetSession()->SendPacket(data);
}

/*********************************************************/
/***             LOAD SYSTEM (角色加载系统)              ***/
/*********************************************************/

void Player::_LoadDeclinedNames(std::unique_ptr<QueryResult> queryResult)
{
    if (!queryResult)
        return;

    delete m_declinedname;
    m_declinedname = new DeclinedName;

    Field* fields = queryResult->Fetch();
    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        m_declinedname->name[i] = fields[i].GetCppString();
}

void Player::_LoadArenaTeamInfo(std::unique_ptr<QueryResult> queryResult)
{
    // arenateamid, played_week, played_season, personal_rating (字段：战队ID、本周场次、本季场次、个人等级)
    memset((void*)&m_uint32Values[PLAYER_FIELD_ARENA_TEAM_INFO_1_1], 0, sizeof(uint32) * MAX_ARENA_SLOT * ARENA_TEAM_END);
    if (!queryResult)
        return;

    do
    {
        Field* fields = queryResult->Fetch();

        uint32 arenateamid     = fields[0].GetUInt32();
        uint32 played_week     = fields[1].GetUInt32();
        uint32 played_season   = fields[2].GetUInt32();
        uint32 personal_rating = fields[3].GetUInt32();

        ArenaTeam* aTeam = sObjectMgr.GetArenaTeamById(arenateamid);
        if (!aTeam)
        {
            sLog.outError("Player::_LoadArenaTeamInfo: couldn't load arenateam %u, week %u, season %u, rating %u", arenateamid, played_week, played_season, personal_rating);
            continue;
        }
        uint8  arenaSlot = aTeam->GetSlot();

        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_ID, arenateamid);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_MEMBER, (aTeam->GetCaptainGuid() == GetObjectGuid()) ? 0 : 1);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_GAMES_WEEK, played_week);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_GAMES_SEASON, played_season);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_WINS_SEASON, 0);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_PERSONAL_RATING, personal_rating);
    }
    while (queryResult->NextRow());
}

void Player::_LoadBGData(std::unique_ptr<QueryResult> queryResult)
{
    if (!queryResult)
        return;

    // Expecting only one row (预期最多返回一行战场数据)
    Field* fields = queryResult->Fetch();
    /* bgInstanceID, bgTeam, x, y, z, o, map (字段：战场实例ID、战场阵营、入场前坐标与地图) */
    m_bgData.bgInstanceID = fields[0].GetUInt32();
    m_bgData.bgTeam       = Team(fields[1].GetUInt32());
    m_bgData.joinPos      = WorldLocation(fields[6].GetUInt32(),    // Map (入场前地图 ID)
                                          fields[2].GetFloat(),     // X (入场前 X 坐标)
                                          fields[3].GetFloat(),     // Y (入场前 Y 坐标)
                                          fields[4].GetFloat(),     // Z (入场前 Z 坐标)
                                          fields[5].GetFloat());    // Orientation (入场前朝向)
}

bool Player::LoadPositionFromDB(ObjectGuid guid, uint32& mapid, float& x, float& y, float& z, float& o, bool& in_flight)
{
    auto queryResult = CharacterDatabase.PQuery("SELECT position_x,position_y,position_z,orientation,map,taxi_path FROM characters WHERE guid = '%u'", guid.GetCounter());
    if (!queryResult)
        return false;

    Field* fields = queryResult->Fetch();

    x = fields[0].GetFloat();
    y = fields[1].GetFloat();
    z = fields[2].GetFloat();
    o = fields[3].GetFloat();
    mapid = fields[4].GetUInt32();
    in_flight = !fields[5].GetCppString().empty();

    return true;
}

void Player::_LoadIntoDataField(const char* data, uint32 startOffset, uint32 count)
{
    if (!data)
        return;

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != count)
        return;

    Tokens::iterator iter;
    uint32 index;
    for (iter = tokens.begin(), index = 0; index < count; ++iter, ++index)
    {
        m_uint32Values[startOffset + index] = std::stoul(*iter);
    }

    // repair titles (根据任务完成情况补全应得称号)
    uint32 foundTitle = 0;
    for (uint32 i = 1; i <= 28; ++i)
    {
        if (HasTitle(i))
        {
            foundTitle = i;
            break;
        }
    }

    if (!foundTitle)
        return;

    uint32 raceMask = getRaceMask();
    if (foundTitle <= 14 && (raceMask & RACEMASK_ALLIANCE) != 0)
        return;
    if (foundTitle > 14 && (raceMask & RACEMASK_HORDE) != 0)
        return;

    if (CharTitlesEntry const* tEntry = sCharTitlesStore.LookupEntry(foundTitle))
        SetTitle(tEntry, true, false);
    if (foundTitle > 14)
        foundTitle -= 14;
    else
        foundTitle += 14;
    if (CharTitlesEntry const* tEntry = sCharTitlesStore.LookupEntry(foundTitle))
        SetTitle(tEntry, false, false);
}

/// 从数据库异步查询结果完整加载玩家角色数据、背包、法术、任务、光环与状态
bool Player::LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder)
{
    //       0     1        2     3     4      5       6      7   8      9            10            11
    // SELECT guid, account, name, race, class, gender, level, xp, money, playerBytes, playerBytes2, playerFlags,"
    // 12          13          14          15   16           17        18         19         20         21          22           23                 24
    //"position_x, position_y, position_z, map, orientation, taximask, cinematic, totaltime, leveltime, rest_bonus, logout_time, is_logout_resting, resettalents_cost,"
    // 25                 26       27       28       29       30         31           32            33        34    35      36                 37         38
    //"resettalents_time, trans_x, trans_y, trans_z, trans_o, transguid, extra_flags, stable_slots, at_login, zone, online, death_expire_time, taxi_path, dungeon_difficulty,"
    // 39           40                41                42                    43          44          45              46           47              48
    //"arenaPoints, totalHonorPoints, todayHonorPoints, yesterdayHonorPoints, totalKills, todayKills, yesterdayKills, chosenTitle, watchedFaction, drunk,"
    // 49      50      51      52      53      54      55             56              57      58           59          60               61
    //"health, power1, power2, power3, power4, power5, exploredZones, equipmentCache, ammoId, knownTitles, actionBars, grantableLevels, fishingSteps  FROM characters WHERE guid = '%u'", GUID_LOPART(m_guid));
    auto queryResult = holder->GetResult(PLAYER_LOGIN_QUERY_LOADFROM);

    Object::_Create(guid.GetCounter(), guid.GetCounter(), 0, HIGHGUID_PLAYER);

    if (!queryResult)
    {
        sLog.outError("%s not found in table `characters`, can't load. ", guid.GetString().c_str());
        return false;
    }

    Field* fields = queryResult->Fetch();

    uint32 dbAccountId = fields[1].GetUInt32();

    // check if the character's account in the db and the logged in account match. (校验数据库角色所属账号与当前登录会话账号是否一致)
    // player should be able to load/delete character only with correct account! (防止跨账号越权加载或操作角色)
    if (dbAccountId != GetSession()->GetAccountId())
    {
        sLog.outError("%s loading from wrong account (is: %u, should be: %u)",
                      guid.GetString().c_str(), GetSession()->GetAccountId(), dbAccountId);
        return false;
    }

    m_name = fields[2].GetCppString();

    // check name limitations (校验角色名合规性)
    if (ObjectMgr::CheckPlayerName(m_name) != CHAR_NAME_SUCCESS ||
            (GetSession()->GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(m_name)))
    {
        CharacterDatabase.PExecute("UPDATE characters SET at_login = at_login | '%u' WHERE guid ='%u'",
                                   uint32(AT_LOGIN_RENAME), guid.GetCounter());
        return false;
    }

    // overwrite possible wrong/corrupted guid (覆写可能损坏的 GUID 字段)
    SetGuidValue(OBJECT_FIELD_GUID, guid);

    // overwrite some data fields (恢复种族/职业/性别等核心字节字段)
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_RACE, fields[3].GetUInt8()); // race (种族)
    uint8 playerClass = fields[4].GetUInt8();
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_CLASS, fields[4].GetUInt8()); // class (职业)
    SetSpellClass(playerClass);

    uint8 gender = fields[5].GetUInt8() & 0x01;             // allowed only 1 bit values male/female cases (for fit drunk gender part) (性别仅占 1 位，以兼容醉酒度字节)
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_GENDER, gender); // gender (性别)

    SetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_DEBUFF_LIMIT, UNIT_BYTE2_PLAYER_CONTROLLED_DEBUFF_LIMIT);

    SetUInt32Value(UNIT_FIELD_LEVEL, fields[6].GetUInt8());
    SetUInt32Value(PLAYER_XP, fields[7].GetUInt32());

    _LoadIntoDataField(fields[55].GetString(), PLAYER_EXPLORED_ZONES_1, PLAYER_EXPLORED_ZONES_SIZE);
    _LoadIntoDataField(fields[58].GetString(), PLAYER__FIELD_KNOWN_TITLES, 2);
    // lifetime highest PvP rank, from the old PvP rank titles (旧版 PvP 生涯最高军衔)
    SetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_LIFETIME_MAX_PVP_RANK, GetHighestPvPRankIndex());

    InitDisplayIds();                                       // model, scale and model data (模型、缩放比例与声音数据)

    uint32 money = fields[8].GetUInt32();
    if (money > MAX_MONEY_AMOUNT)
        money = MAX_MONEY_AMOUNT;
    SetMoney(money);

    SetUInt32Value(PLAYER_BYTES, fields[9].GetUInt32());
    SetUInt32Value(PLAYER_BYTES_2, fields[10].GetUInt32());

    m_drunk = fields[48].GetUInt16();

    SetUInt16Value(PLAYER_BYTES_3, 0, (m_drunk & 0xFFFE) | gender);

    SetUInt32Value(PLAYER_FLAGS, fields[11].GetUInt32());
    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, fields[47].GetInt32());

    SetUInt32Value(PLAYER_AMMO_ID, fields[57].GetUInt32());

    // Action bars state (动作条启用掩码)
    SetByteValue(PLAYER_FIELD_BYTES, 2, fields[59].GetUInt8());

    // cleanup inventory related item value fields (its will be filled correctly in _LoadInventory) (清空装备栏位字段，将在 _LoadInventory 中重新填充)
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), ObjectGuid());
        SetVisibleItemSlot(slot, nullptr);

        delete m_items[slot];
        m_items[slot] = nullptr;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "Load Basic value of player %s is: ", m_name.c_str());
    outDebugStatsValues();

    // Need to call it to initialize m_team (m_team can be calculated from race) (初始化阵营 m_team，可根据种族推算)
    // Other way is to saves m_team into characters table. (否则需在 characters 表单独存储)
    setFactionForRace(getRace());
    SetCharm(nullptr);

    // load home bind and check in same time class/race pair, it used later for restore broken positions (加载炉石绑定点并校验职业/种族组合合法性)
    if (!_LoadHomeBind(holder->GetResult(PLAYER_LOGIN_QUERY_LOADHOMEBIND)))
    {
        return false;
    }

    InitPrimaryProfessions();                               // to max set before any spell loaded (加载法术前先重置最大可学等级等基础限制)

    // init saved position, and fix it later if problematic (读取保存坐标，若存在异常则在后续修复)
    uint32 transGUID = fields[30].GetUInt32();
    Relocate(fields[12].GetFloat(), fields[13].GetFloat(), fields[14].GetFloat(), fields[16].GetFloat());
    SetLocationMapId(fields[15].GetUInt32());

    uint32 difficulty = fields[38].GetUInt32();
    if (difficulty >= MAX_DIFFICULTY || GetLevel() < LEVELREQUIREMENT_HEROIC)
        difficulty = DUNGEON_DIFFICULTY_NORMAL;
    SetDifficulty(Difficulty(difficulty));                  // may be changed in _LoadGroup (副本难度可能会在 _LoadGroup 中被队长设置覆盖)

    _LoadGroup(holder->GetResult(PLAYER_LOGIN_QUERY_LOADGROUP));

    _LoadArenaTeamInfo(holder->GetResult(PLAYER_LOGIN_QUERY_LOADARENAINFO));

    SetArenaPoints(fields[39].GetUInt32());

    // check arena teams integrity (校验竞技场战队成员完整性)
    for (uint32 arena_slot = 0; arena_slot < MAX_ARENA_SLOT; ++arena_slot)
    {
        uint32 arena_team_id = GetArenaTeamId(arena_slot);
        if (!arena_team_id)
            continue;

        if (ArenaTeam* at = sObjectMgr.GetArenaTeamById(arena_team_id))
            if (at->HaveMember(GetObjectGuid()))
                continue;

        // arena team not exist or not member, cleanup fields (战队不存在或已非成员，清理对应竞技场字段)
        for (int j = 0; j < ARENA_TEAM_END; ++j)
            SetArenaTeamInfoField(arena_slot, ArenaTeamInfoType(j), 0);
    }

    SetHonorPoints(fields[40].GetUInt32());

    SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, fields[41].GetUInt32());
    SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, fields[42].GetUInt32());
    SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, fields[43].GetUInt32());
    SetUInt16Value(PLAYER_FIELD_KILLS, 0, fields[44].GetUInt16());
    SetUInt16Value(PLAYER_FIELD_KILLS, 1, fields[45].GetUInt16());

    _LoadBoundInstances(holder->GetResult(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES));

    if (!IsPositionValid())
    {
        sLog.outError("%s have invalid coordinates (X: %f Y: %f Z: %f O: %f). Teleport to default race/class locations.",
                      guid.GetString().c_str(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        RelocateToHomebind();

        transGUID = 0;

        m_movementInfo.ClearTransportData();
    }

    _LoadBGData(holder->GetResult(PLAYER_LOGIN_QUERY_LOADBGDATA));

    if (m_bgData.bgInstanceID)                              // saved in BattleGround (玩家上次离线时正处于战场/竞技场中)
    {
        BattleGround* currentBg = sBattleGroundMgr.GetBattleGround(m_bgData.bgInstanceID, BATTLEGROUND_TYPE_NONE);

        bool player_at_bg = currentBg && currentBg->IsPlayerInBattleGround(GetObjectGuid());

        if (player_at_bg && currentBg->GetStatus() != STATUS_WAIT_LEAVE)
        {
            BattleGroundQueueTypeId bgQueueTypeId = sBattleGroundMgr.BgQueueTypeId(currentBg->GetTypeId(), currentBg->GetArenaType());
            AddBattleGroundQueueId(bgQueueTypeId);

            m_bgData.bgTypeID = currentBg->GetTypeId();     // bg data not marked as modified (刚加载的战场数据未修改，无需重复标记存盘)

            // join player to battleground group (将玩家加入或恢复到战场团队)
            currentBg->EventPlayerLoggedIn(this);
            currentBg->AddOrSetPlayerToCorrectBgGroup(this, GetObjectGuid(), m_bgData.bgTeam);

            SetInviteForBattleGroundQueueType(bgQueueTypeId, currentBg->GetInstanceId());
        }
        else
        {
            // leave bg (战场已结束或无法重连，退出战场)
            if (player_at_bg)
                currentBg->RemovePlayerAtLeave(GetObjectGuid(), false, true);

            // move to bg enter point (将玩家坐标重置回战场入口前位置)
            const WorldLocation& _loc = GetBattleGroundEntryPoint();
            SetLocationMapId(_loc.mapid);
            Relocate(_loc.coord_x, _loc.coord_y, _loc.coord_z, _loc.orientation);

            // We are not in BG anymore (已不在战场中，重置战场状态)
            SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
            // remove outdated DB data in DB (清理数据库中过期的战场状态记录)
            _SaveBGData();
        }
    }
    else
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(GetMapId());
        // if server restart after player save in BG or area (若服务器重启后玩家坐标仍留在战场/竞技场地图，)
        // player can have current coordinates in to BG/Arena map, fix this (则修复其坐标至入口点或炉石点)
        if (!mapEntry || mapEntry->IsBattleGroundOrArena())
        {
            const WorldLocation& _loc = GetBattleGroundEntryPoint();
            SetLocationMapId(_loc.mapid);
            Relocate(_loc.coord_x, _loc.coord_y, _loc.coord_z, _loc.orientation);

            // We are not in BG anymore (清理战场数据)
            SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
            // remove outdated DB data in DB (立即删除数据库过期战场记录)
            _SaveBGData();
        }
    }

    if (transGUID != 0)
    {
        m_movementInfo.SetTransportData(ObjectGuid(HIGHGUID_MO_TRANSPORT, transGUID), fields[26].GetFloat(), fields[27].GetFloat(), fields[28].GetFloat(), fields[29].GetFloat(), 0);

        Position const& transportPosition = m_movementInfo.GetTransportPos();

        if (!MaNGOS::IsValidMapCoord(
                    GetPositionX() + transportPosition.x, GetPositionY() + transportPosition.y,
                    GetPositionZ() + transportPosition.z, GetOrientation() + transportPosition.o) ||
                // transport size limited (船只/飞艇静态路径载具检查)
                transportPosition.x > 50 || transportPosition.y > 50 || transportPosition.z > 50)
        {
            sLog.outError("%s have invalid transport coordinates (X: %f Y: %f Z: %f O: %f). Teleport to default race/class locations.",
                          guid.GetString().c_str(), GetPositionX() + transportPosition.x, GetPositionY() + transportPosition.y,
                          GetPositionZ() + transportPosition.z, GetOrientation() + transportPosition.o);

            RelocateToHomebind();

            m_movementInfo.ClearTransportData();

            transGUID = 0;
        }
    }

    // load the player's map here if it's not already loaded (提前加载玩家所在地图实例)
    SetMap(sMapMgr.CreateMap(GetMapId(), this));

    if (transGUID != 0)
    {
        ObjectGuid guid(HIGHGUID_MO_TRANSPORT, transGUID);
        if (GameObjectData const* data = sObjectMgr.GetGOData(transGUID))
        {
            GameObjectInfo const* transportInfo = sGOStorage.LookupEntry<GameObjectInfo>(data->id);
            if (transportInfo->type == GAMEOBJECT_TYPE_TRANSPORT)
            {
                guid = ObjectGuid(transportInfo->GetHighGuid(), data->id, transGUID);
                m_movementInfo.t_guid = guid;
                // an elevator only exists while its grid is loaded (电梯/可移动载具仅在所属网格加载后才存在于世界中)
                if (data->mapid == GetMapId())
                    GetMap()->LoadGrid(data->posX, data->posY);
            }
        }
        GenericTransport* transport = GetMap()->GetTransport(guid);
        Map* map = GetMap();
        if (!transport)
        {
            if (TransportTemplate const* transportTemplate = sTransportMgr.GetTransportTemplate(guid.GetCounter()))
            {
                uint32 mapId = *transportTemplate->mapsUsed.begin();
                if (mapId == map->GetId()) // only two maps max (最多允许 2 层关联地图，如主世界与副本)
                    mapId = *transportTemplate->mapsUsed.rbegin();
                map = sMapMgr.CreateMap(mapId, this);
                transport = map->GetTransport(guid);
            }
        }
        if (transport)
        {
            MapEntry const* transMapEntry = sMapStore.LookupEntry(transport->GetMapId());
            // client without expansion support (客户端不具备目标地图所需资料片权限)
            if (GetSession()->GetExpansion() < transMapEntry->Expansion())
                DEBUG_LOG("Player %s using client without required expansion tried login at transport at non accessible map %u", GetName(), transport->GetMapId());
            else
            {
                transport->AddPassenger(this);
                SetLocationMapId(transport->GetMapId());
                transport->UpdatePassengerPosition(this);
                SetMap(map);
            }
        }

        if (!m_transport && guid.GetHigh() == HIGHGUID_MO_TRANSPORT)
        {
            sLog.outError("%s have problems with transport guid (%u). Teleport to default race/class locations.",
                guid.GetString().c_str(), transGUID);

            RelocateToHomebind();

            m_movementInfo.ClearTransportData();

            SetMap(sMapMgr.CreateMap(GetMapId(), this));
            SaveRecallPosition();                           // save as recall also to prevent recall and fall from sky (同步更新回溯点以防回溯时从高空坠落)
        }
    }
    else                                                    // not transport case (普通非载具坐标校验)
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(GetMapId());
        // client without expansion support (客户端不具备资料片权限)
        if (GetSession()->GetExpansion() < mapEntry->Expansion())
        {
            DEBUG_LOG("Player %s using client without required expansion tried login at non accessible map %u", GetName(), GetMapId());
            RelocateToHomebind();

            SetMap(sMapMgr.CreateMap(GetMapId(), this));
            SaveRecallPosition();                           // save as recall also to prevent recall and fall from sky (同步更新回溯点)
        }
    }

    // player bounded instance saves loaded in _LoadBoundInstances, group versions at group loading (加载玩家自身及队伍的副本进度绑定)
    DungeonPersistentState* state = GetBoundInstanceSaveForSelfOrGroup(GetMapId());

    time_t now = time(nullptr);
    time_t logoutTime = time_t(fields[22].GetUInt64());

    // since last logout (in seconds) (计算自上次登出经过的离线秒数)
    uint32 time_diff = uint32(now - logoutTime);

    if (!sWorld.getConfig(CONFIG_BOOL_DISABLE_INSTANCE_RELOCATE))
    {
        // if the player is in an instance and it has been reset in the meantime teleport him to the entrance (若离线期间副本已重置，则将玩家传回副本入口)
        if (GetInstanceId() && (!state || time_diff > 15 * MINUTE))
        {
            AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(GetMapId());
            if (at)
                Relocate(at->target_X, at->target_Y, at->target_Z, at->target_Orientation);
            else
                sLog.outError("Player %s(GUID: %u) logged in to a reset instance (map: %u) and there is no area-trigger leading to this map. Thus he can't be ported back to the entrance. This _might_ be an exploit attempt.", GetName(), GetGUIDLow(), GetMapId());
        }
    }

    SaveRecallPosition();

    // set value, including drunk invisibility detection (设置醉酒度并更新醉酒侦测光环)
    // calculate sobering. after 15 minutes logged out, the player will be sober again (计算离线醒酒：离线超过 15 分钟即完全清醒)
    float soberFactor;
    if (time_diff > 15 * MINUTE)
        soberFactor = 0;
    else
        soberFactor = 1 - time_diff / (15 * MINUTE);
    uint16 newDrunkenValue = uint16(soberFactor * m_drunk);
    SetDrunkValue(newDrunkenValue);

    m_cinematic = fields[18].GetUInt32();
    m_Played_time[PLAYED_TIME_TOTAL] = fields[19].GetUInt32();
    m_Played_time[PLAYED_TIME_LEVEL] = fields[20].GetUInt32();

    m_resetTalentsCost = fields[24].GetUInt32();
    m_resetTalentsTime = time_t(fields[25].GetUInt64());

    // reserve some flags (保留部分离线期间不应丢失的玩家状态标志)
    uint32 old_safe_flags = GetUInt32Value(PLAYER_FLAGS) & (PLAYER_FLAGS_HIDE_CLOAK | PLAYER_FLAGS_HIDE_HELM);

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM))
        SetUInt32Value(PLAYER_FLAGS, 0 | old_safe_flags);

    m_taxi.LoadTaxiMask(fields[17].GetString());            // must be before InitTaxiNodesForLevel (必须在 InitTaxiNodesForLevel 之前加载飞行路线掩码)

    uint32 extraflags = fields[31].GetUInt32();

    m_stableSlots = fields[32].GetUInt32();
    if (m_stableSlots > MAX_PET_STABLES)
    {
        sLog.outError("Player can have not more %u stable slots, but have in DB %u", MAX_PET_STABLES, uint32(m_stableSlots));
        m_stableSlots = MAX_PET_STABLES;
    }

    m_atLoginFlags = fields[33].GetUInt32();

    // Honor system (荣誉系统：加载跨日时间戳并刷新昨日/今日统计)
    // Update Honor kills data (更新荣誉击杀字段)
    m_lastHonorUpdateTime = logoutTime;
    UpdateHonorFields();

    m_deathExpireTime = (time_t)fields[36].GetUInt64();
    if (m_deathExpireTime > now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP)
        m_deathExpireTime = now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP - 1;

    std::string taxi_nodes = fields[37].GetCppString();

    // clear channel spell data (if saved at channel spell casting) (清除引导法术残留字段)
    SetChannelObjectGuid(ObjectGuid());
    SetUInt32Value(UNIT_CHANNEL_SPELL, 0);

    // clear charm/summon related fields (清除魅惑/召唤控制残留字段)
    SetCharm(nullptr);
    SetPet(nullptr);
    SetTargetGuid(ObjectGuid());
    SetCharmerGuid(ObjectGuid());
    SetCreatorGuid(ObjectGuid());
    SetSummonerGuid(ObjectGuid());

    // reset some aura modifiers before aura apply (应用光环前重置光环基础修正数组)

    SetGuidValue(PLAYER_FARSIGHT, ObjectGuid());
    SetUInt32Value(PLAYER_TRACK_CREATURES, 0);
    SetUInt32Value(PLAYER_TRACK_RESOURCES, 0);

    // cleanup aura list explicitly before skill load where some spells can be applied (在加载技能前显式清空光环列表，因为技能加载可能触发法术)
    RemoveAllAuras();

    // make sure the unit is considered out of combat for proper loading (确保加载期间处于脱战状态)
    ClearInCombat();

    // make sure the unit is considered not in duel for proper loading (确保加载期间不处于决斗状态)
    SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid());
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    // reset stats before loading any modifiers (加载任何属性修正前先按等级初始化基础属性)
    InitStatsForLevel();
    InitTaxiNodesForLevel();

    // rest bonus can only be calculated after InitStatsForLevel() (休息双倍经验上限依赖等级，须在 InitStatsForLevel 后计算)
    m_rest_bonus = fields[21].GetFloat();

    if (time_diff > 0)
        SetRestBonus(GetRestBonus() + ComputeRest(time_diff, true, (fields[23].GetInt32() > 0)));

    // load skills after InitStatsForLevel because it triggering aura apply also (加载技能并应用被动技能效果)
    _LoadSkills(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSKILLS));

    // apply original stats mods before spell loading or item equipment that call before equip _RemoveStatsMods() (在加载法术与装备前先应用基础属性加成)

    // Mail (加载邮件与未收附件物品)
    _LoadMails(holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILS));
    _LoadMailedItems(holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS));
    UpdateNextMailTimeAndUnreads();

    _LoadSpells(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSPELLS));

    _LoadAuras(holder->GetResult(PLAYER_LOGIN_QUERY_LOADAURAS), time_diff);

    // add ghost flag (must be after aura load: PLAYER_FLAGS_GHOST set in aura) (光环加载后同步幽灵与存活/死亡状态)
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        m_deathState = DEAD;

    // after spell load (在法术加载后执行)
    InitTalentForLevel();
    LearnDefaultSkills();
    learnDefaultSpells();

    // after spell load, learn rewarded spell if need also (在加载任务后补学缺失的任务奖励法术)
    _LoadQuestStatus(holder->GetResult(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS));
    _LoadDailyQuestStatus(holder->GetResult(PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS));
    _LoadWeeklyQuestStatus(holder->GetResult(PLAYER_LOGIN_QUERY_LOADWEEKLYQUESTSTATUS));
    _LoadMonthlyQuestStatus(holder->GetResult(PLAYER_LOGIN_QUERY_LOADMONTHLYQUESTSTATUS));

    // must be before inventory (some items required reputation check) (必须在加载背包前加载声望，部分装备有声望穿戴要求)
    m_reputationMgr.LoadFromDB(holder->GetResult(PLAYER_LOGIN_QUERY_LOADREPUTATION));

    _LoadInventory(holder->GetResult(PLAYER_LOGIN_QUERY_LOADINVENTORY), time_diff);
    _LoadItemLoot(holder->GetResult(PLAYER_LOGIN_QUERY_LOADITEMLOOT));

    // update items with duration and realtime (扣减离线期间经过的限时物品存在时间)
    UpdateItemDuration(time_diff, true);

    // After quests and inventory loaded: force update on quest item counters (任务与背包加载完毕后强制重算任务物品计数，)
    // Fixes cases when quest status data was not correctly saved during last session (crash, db connection lost, etc) (修复异常掉线或宕机导致的任务物品计数不同步)
    for (auto& data : mQuestStatus)
    {
        if (Quest const* quest = sObjectMgr.GetQuestTemplate(data.first))
            AdjustQuestReqItemCount(quest, data.second);
    }

    _LoadActions(holder->GetResult(PLAYER_LOGIN_QUERY_LOADACTIONS));

    m_social = sSocialMgr.LoadFromDB(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSOCIALLIST), GetObjectGuid());

    // check PLAYER_CHOSEN_TITLE compatibility with PLAYER__FIELD_KNOWN_TITLES (校验当前佩戴称号是否在已知称号掩码中)
    // note: PLAYER__FIELD_KNOWN_TITLES updated at quest status loaded (注：任务奖励称号在任务加载阶段更新)
    uint32 curTitle = fields[46].GetUInt32();
    if (curTitle && !HasTitle(curTitle))
        curTitle = 0;

    SetUInt32Value(PLAYER_CHOSEN_TITLE, curTitle);

    Taxi::DestID destOrphan = 0;
    if (!m_taxiTracker.Load(taxi_nodes, destOrphan))
    {
        // problems with taxi path loading (飞行路线数据解析失败保护)
        TaxiNodesEntry const* nodeEntry = (destOrphan ? sTaxiNodesStore.LookupEntry(destOrphan) : nullptr);

        if (!nodeEntry)                                     // don't know taxi start node, to homebind (缺少飞行起点信息，传回炉石绑定点)
        {
            sLog.outError("Character %u have wrong data in taxi destination list, teleport to homebind.", GetGUIDLow());
            RelocateToHomebind();
        }
        else                                                // have start node, to it (回退至飞行起点节点位置)
        {
            sLog.outError("Character %u have too short taxi destination list, teleport to original node.", GetGUIDLow());
            SetLocationMapId(nodeEntry->map_id);
            Relocate(nodeEntry->x, nodeEntry->y, nodeEntry->z, 0.0f);
        }

        // we can be relocated from taxi and still have an outdated Map pointer! (若从飞行点重定位至其他地图，须同步刷新地图指针)
        // so we need to get a new Map pointer! (获取重定位后新坐标所属地图)
        SetMap(sMapMgr.CreateMap(GetMapId(), this));
        SaveRecallPosition();                           // save as recall also to prevent recall and fall from sky (同步更新回溯坐标)
    }
    else if (!m_taxiTracker.GetRoadmap().empty() && !m_taxiTracker.GetAtlas().empty())
    {
        if (size_t nodeResume = m_taxiTracker.GetResumeWaypointIndex())
        {
            const TaxiPathNodeEntry* entry = m_taxiTracker.GetMap().at(nodeResume);
            MANGOS_ASSERT(entry);
            Relocate(entry->x, entry->y, entry->z, GetOrientation());
        }

        // save source node as recall coord to prevent recall and fall from sky (将飞行起点存为回溯坐标)
        if (uint32 destSource = m_taxiTracker.GetRoute().destStart)
        {
            TaxiNodesEntry const* destination = sTaxiNodesStore.LookupEntry(destSource);
            MANGOS_ASSERT(destination);
            m_recallMap = destination->map_id;
            m_recallX = destination->x;
            m_recallY = destination->y;
            m_recallZ = destination->z;
        }
        // flight will start later (飞行将在加入地图后正式继续)
    }

    // has to be called after last Relocate() in Player::LoadFromDB (必须在 LoadFromDB 中最后一次 Relocate 后更新区域与地形状态)
    SetFallInformation(0, GetPositionZ());

    _LoadSpellCooldowns(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS));

    // Spell code allow apply any auras to dead character in load time in aura/spell/item loading (加载阶段可能为死亡角色挂上非死亡持久光环，)
    // Do now before stats re-calculation cleanup for ghost state unexpected auras (在重算属性前先按死亡/幽灵状态清理不该保留的光环)
    if (!IsAlive())
        RemoveAllAurasOnDeath();

    // apply all stat bonuses from items and auras (重新应用所有装备与光环属性加成)
    SetCanModifyStats(true);
    UpdateAllStats();

    // restore remembered power/health values (but not more max values) (恢复离线前记录的生命值与能量值，不超过当前上限)
    uint32 savedhealth = fields[49].GetUInt32();
    SetHealth(savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth);
    for (uint32 i = 0; i < MAX_POWERS; ++i)
    {
        uint32 savedpower = fields[50 + i].GetUInt32();
        SetPower(Powers(i), savedpower > GetMaxPower(Powers(i)) ? GetMaxPower(Powers(i)) : savedpower);
    }

    // RaF stuff. (初始化战友招募可赠送等级)
    m_grantableLevels = fields[60].GetUInt8();
    if ((GetSession()->GetRecruitingFriendId() != 0))
        SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_REFER_A_FRIEND);

    if (m_grantableLevels > 0)
        SetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_RAF_GRANTABLE_LEVEL, 0x01);

    m_fishingSteps = fields[61].GetUInt32();

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "The value of player %s after load item and aura is: ", m_name.c_str());
    outDebugStatsValues();

    // all fields read (所有数据库字段读取完成)

    // GM state (根据 extra_flags 恢复 GM 模式开关)
    if (GetSession()->GetSecurity() > SEC_PLAYER)
    {
        switch (sWorld.getConfig(CONFIG_UINT32_GM_LOGIN_STATE))
        {
            default:
            case 0:                      break;             // disable (禁用)
            case 1: SetGameMaster(true); break;             // enable (启用)
            case 2:                                         // save state (保留现有状态)
                if (extraflags & PLAYER_EXTRA_GM_ON)
                    SetGameMaster(true);
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_VISIBLE_STATE))
        {
            default:
            case 0: SetGMVisible(false); break;             // invisible (GM 隐身)
            case 1:                      break;             // visible (GM 可见)
            case 2:                                         // save state (保留现有状态)
                if (extraflags & PLAYER_EXTRA_GM_INVISIBLE)
                    SetGMVisible(false);
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_ACCEPT_TICKETS))
        {
            default:
            case 0:                        break;           // disable (禁用)
            case 1: SetAcceptTicket(true); break;           // enable (启用)
            case 2:                                         // save state (保留现有状态)
                if (extraflags & PLAYER_EXTRA_GM_ACCEPT_TICKETS)
                    SetAcceptTicket(true);
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_CHAT))
        {
            default:
            case 0:                  break;                 // disable (禁用)
            case 1: SetGMChat(true); break;                 // enable (启用)
            case 2:                                         // save state (保留现有状态)
                if (extraflags & PLAYER_EXTRA_GM_CHAT)
                    SetGMChat(true);
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_WISPERING_TO))
        {
            default:
            case 0:                          break;         // disable (禁用)
            case 1: SetAcceptWhispers(true); break;         // enable (启用)
            case 2:                                         // save state (保留现有状态)
                if (extraflags & PLAYER_EXTRA_ACCEPT_WHISPERS)
                    SetAcceptWhispers(true);
                break;
        }
    }

    _LoadDeclinedNames(holder->GetResult(PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES));

    _LoadCreatedInstanceTimers();

    return true;
}

void Player::_LoadActions(std::unique_ptr<QueryResult> queryResult)
{
    m_actionButtons.clear();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT button,action,type FROM character_action WHERE guid = '%u' ORDER BY button",GetGUIDLow());

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            uint8 button = fields[0].GetUInt8();
            uint32 action = fields[1].GetUInt32();
            uint8 type = fields[2].GetUInt8();

            if (ActionButton* ab = addActionButton(button, action, type))
                ab->uState = ACTIONBUTTON_UNCHANGED;
            else
            {
                sLog.outError("  ...at loading, and will deleted in DB also");

                // Will deleted in DB at next save (it can create data until save but marked as deleted) (非法动作条按钮标记为 DELETED，待下次存盘清理)
                m_actionButtons[button].uState = ACTIONBUTTON_DELETED;
            }
        }
        while (queryResult->NextRow());
    }
}

void Player::_LoadAuras(std::unique_ptr<QueryResult> queryResult, uint32 timediff)
{
    // RemoveAllAuras(); -- some spells casted before aura load, for example in LoadSkills, aura list explicitly cleaned early (光环列表已在加载技能前清空，此处不重复清理)

    // QueryResult *result = CharacterDatabase.PQuery("SELECT caster_guid,item_guid,spell,stackcount,remaincharges,basepoints0,basepoints1,basepoints2,periodictime0,periodictime1,periodictime2,maxduration,remaintime,effIndexMask FROM character_aura WHERE guid = '%u'",GetGUIDLow());

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();
            ObjectGuid caster_guid = ObjectGuid(fields[0].GetUInt64());
            uint32 item_lowguid = fields[1].GetUInt32();
            uint32 spellid = fields[2].GetUInt32();
            uint32 stackcount = fields[3].GetUInt32();
            uint32 remaincharges = fields[4].GetUInt32();
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = fields[i + 5].GetInt32();
                periodicTime[i] = fields[i + 8].GetUInt32();
            }

            int32 maxduration = fields[11].GetInt32();
            int32 remaintime = fields[12].GetInt32();
            uint32 effIndexMask = fields[13].GetUInt32();

            SpellEntry const* spellproto = sSpellTemplate.LookupEntry<SpellEntry>(spellid);
            if (!spellproto)
            {
                sLog.outError("Unknown spell (spellid %u), ignore.", spellid);
                continue;
            }

            if (remaintime != -1 && (spellproto->HasAttribute(SPELL_ATTR_EX4_AURA_EXPIRES_OFFLINE) || !IsPositiveSpell(spellproto)))
            {
                if (remaintime / IN_MILLISECONDS <= int32(timediff))
                    continue;

                remaintime -= timediff * IN_MILLISECONDS;
            }

            // prevent wrong values of remaincharges (修正异常的剩余充能次数)
            if (spellproto->procCharges == 0)
                remaincharges = 0;

            if (!spellproto->StackAmount)
                stackcount = 1;
            else if (spellproto->StackAmount < stackcount)
                stackcount = spellproto->StackAmount;
            else if (!stackcount)
                stackcount = 1;

            SpellAuraHolder* holder = CreateSpellAuraHolder(spellproto, this, nullptr);
            holder->SetLoadedState(caster_guid, ObjectGuid(HIGHGUID_ITEM, item_lowguid), stackcount, remaincharges, maxduration, remaintime);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((effIndexMask & (1 << i)) == 0)
                    continue;

                Aura* aura = CreateAura(spellproto, SpellEffectIndex(i), nullptr, nullptr, holder, this);
                if (!damage[i])
                    damage[i] = aura->GetModifier()->m_amount;

                aura->SetLoadedState(damage[i], periodicTime[i]);
                holder->AddAura(aura, SpellEffectIndex(i));
            }

            const bool empty = holder->IsEmptyHolder();
            if (!empty)
            {
                // reset stolen single target auras (清除偷取类的单目标光环，重连后不恢复)
                if (caster_guid != GetObjectGuid() && holder->GetTrackedAuraType() == TRACK_AURA_TYPE_SINGLE_TARGET)
                    holder->SetTrackedAuraType(TRACK_AURA_TYPE_NOT_TRACKED);

                holder->SetState(SPELLAURAHOLDER_STATE_DB_LOAD); // Safeguard mechanism against some actions (防止特定异常状态的保护机制)
            }

            if (!empty && AddSpellAuraHolder(holder))
            {
                holder->SetState(SPELLAURAHOLDER_STATE_READY);
                DETAIL_LOG("Added player auras from spellid %u", spellproto->Id);

                if (holder->GetSpellProto()->HasAttribute(SPELL_ATTR_COOLDOWN_ON_EVENT))
                    AddCooldown(*holder->GetSpellProto(), nullptr, true);
            }
            else
                delete holder;
        }
        while (queryResult->NextRow());
    }

    if (getClass() == CLASS_WARRIOR && !HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
        CastSpell(this, SPELL_ID_PASSIVE_BATTLE_STANCE, TRIGGERED_OLD_TRIGGERED);
}

void Player::LoadCorpse()
{
    if (IsAlive())
    {
        sObjectAccessor.ConvertCorpseForPlayer(GetObjectGuid());
    }
    else
    {
        if (Corpse* corpse = GetCorpse())
        {
            ApplyModByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_FLAGS, PLAYER_FIELD_BYTE_RELEASE_TIMER, corpse && !sMapStore.LookupEntry(corpse->GetMapId())->Instanceable());

            // [XFACTION]: Alter values update if detected crossfaction group interaction: (跨阵营组队交互时更新模型与阵营字段)
            if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP) && GetGroup())
                corpse->ForceValuesUpdateAtIndex(CORPSE_FIELD_BYTES_1);
        }
        else
        {
            // Prevent Dead Player login without corpse (防止死亡玩家在无尸体时异常登录，自动转为复活或灵魂状态)
            ResurrectPlayer(0.5f);
        }
    }
}

void Player::_LoadInventory(std::unique_ptr<QueryResult> queryResult, uint32 timediff)
{
    //        0          1            2                3      4         5        6      7             8                 9           10          11   12    13    14
    // SELECT itemEntry, creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, randomPropertyId, durability, itemTextId, bag, slot, item, item_template FROM character_inventory JOIN item_instance ON character_inventory.item = item_instance.guid WHERE character_inventory.guid = '%u' ORDER BY bag, slot
    std::map<uint32, Bag*> bagMap;                          // fast guid lookup for bags (背包 GUID 快速查找映射表)
    // NOTE: the "order by `bag`" is important because it makes sure (注：SQL 按 bag 排序确保主背包中的容器先于包内物品加载)
    // the bagMap is filled before items in the bags are loaded (保证填充 bagMap 后再读取包内物品)
    // NOTE2: the "order by `slot`" is needed because mainhand weapons are (wrongly?) (注2：按 slot 排序确保主手武器先于副手装备，)
    // expected to be equipped before offhand items (TODO: fixme) (以满足副手穿戴依赖判断)

    uint32 zone = GetZoneId();

    if (queryResult)
    {
        std::list<Item*> problematicItems;

        // prevent items from being added to the queue when stored (加载存入时暂不加入物品更新队列)
        m_itemUpdateQueueBlocked = true;
        do
        {
            Field* fields = queryResult->Fetch();
            uint32 bag_guid  = fields[11].GetUInt32();
            uint8  slot      = fields[12].GetUInt8();
            uint32 item_lowguid = fields[13].GetUInt32();
            uint32 item_id   = fields[14].GetUInt32();

            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);

            if (!proto)
            {
                CharacterDatabase.PExecute("DELETE FROM character_inventory WHERE item = '%u'", item_lowguid);
                CharacterDatabase.PExecute("DELETE FROM item_instance WHERE guid = '%u'", item_lowguid);
                sLog.outError("Player::_LoadInventory: Player %s has an unknown item (id: #%u) in inventory, deleted.", GetName(), item_id);
                continue;
            }

            Item* item = NewItemOrBag(proto);

            if (!item->LoadFromDB(item_lowguid, fields, GetObjectGuid()))
            {
                sLog.outError("Player::_LoadInventory: Player %s has broken item (id: #%u) in inventory, deleted.", GetName(), item_id);
                CharacterDatabase.PExecute("DELETE FROM character_inventory WHERE item = '%u'", item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();                           // it also deletes item object ! (创建异常则直接从数据库与内存销毁)
                continue;
            }

            // not allow have in alive state item limited to another map/zone (存活玩家不允许持有受限于其他地图/区域的物品)
            if (IsAlive() && item->IsLimitedToAnotherMapOrZone(GetMapId(), zone))
            {
                CharacterDatabase.PExecute("DELETE FROM character_inventory WHERE item = '%u'", item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();                           // it also deletes item object ! (销毁不合地图限制的物品)
                continue;
            }

            // "Conjured items disappear if you are logged out for more than 15 minutes" (魔法制造物品在离线超过 15 分钟后自动消失)
            if (timediff > 15 * MINUTE && (item->GetProto()->Flags & ITEM_FLAG_CONJURED))
            {
                CharacterDatabase.PExecute("DELETE FROM character_inventory WHERE item = '%u'", item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();                           // it also deletes item object ! (销毁过期的魔法制造物品)
                continue;
            }

            bool success = true;

            // the item/bag is not in a bag (位于主行囊/装备栏/银行主格/回购栏)
            if (!bag_guid)
            {
                item->SetContainer(nullptr);
                item->SetSlot(slot);

                if (IsInventoryPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    ItemPosCountVec dest;
                    uint8 bagSlot = 0;
                    if (CanStoreItem(INVENTORY_SLOT_BAG_0, slot, dest, item, bagSlot, false) == EQUIP_ERR_OK)
                        item = StoreItem(dest, item, true);
                    else
                        success = false;
                }
                else if (IsEquipmentPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    uint16 dest;
                    if (CanEquipItem(slot, dest, item, false, false) == EQUIP_ERR_OK)
                        QuickEquipItem(dest, item);
                    else
                        success = false;
                }
                else if (IsBankPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    ItemPosCountVec dest;
                    uint8 bagSlot;
                    if (CanBankItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false, bagSlot, false) == EQUIP_ERR_OK)
                        item = BankItem(dest, item, true);
                    else
                        success = false;
                }

                if (success)
                {
                    // store bags that may contain items in them (记录已加载的背包容器供包内物品索引)
                    if (item->IsBag() && IsBagPos(item->GetPos()))
                        bagMap[item_lowguid] = (Bag*)item;
                }
            }
            // the item/bag in a bag (位于某个背包容器内部的物品)
            else
            {
                item->SetSlot(NULL_SLOT);
                // the item is in a bag, find the bag (查找所属背包容器对象)
                std::map<uint32, Bag*>::const_iterator itr = bagMap.find(bag_guid);
                if (itr != bagMap.end() && slot < itr->second->GetBagSize())
                {
                    ItemPosCountVec dest;
                    uint8 bagSlot = 0;
                    if (CanStoreItem(itr->second->GetSlot(), slot, dest, item, bagSlot, false) == EQUIP_ERR_OK)
                        item = StoreItem(dest, item, true);
                    else
                        success = false;
                }
                else
                    success = false;
            }

            // item's state may have changed after stored (存入时可能改变状态，重设为未变更)
            if (success)
            {
                item->SetState(ITEM_UNCHANGED, this);

                // restore container unchanged state also (同步恢复容器的未变更状态)
                if (item->GetContainer())
                    item->GetContainer()->SetState(ITEM_UNCHANGED, this);
            }
            else
            {
                sLog.outError("Player::_LoadInventory: Player %s has item (GUID: %u Entry: %u) can't be loaded to inventory (Bag GUID: %u Slot: %u) by some reason, will send by mail.", GetName(), item_lowguid, item_id, bag_guid, slot);
                CharacterDatabase.PExecute("DELETE FROM character_inventory WHERE item = '%u'", item_lowguid);
                problematicItems.push_back(item);
            }
        }
        while (queryResult->NextRow());

        m_itemUpdateQueueBlocked = false;

        // send by mail problematic items (将无法正常放回原位的异常物品通过系统邮件退还玩家)
        while (!problematicItems.empty())
        {
            std::string subject = GetSession()->GetMangosString(LANG_NOT_EQUIPPED_ITEM);

            // fill mail (构建系统补偿邮件并附带问题物品)
            MailDraft draft(subject);

            for (int i = 0; !problematicItems.empty() && i < MAX_MAIL_ITEMS; ++i)
            {
                Item* item = problematicItems.front();
                problematicItems.pop_front();

                draft.AddItem(item);
            }

            draft.SendMailTo(this, MailSender(this, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_RETURNED);
        }
    }

    // if(IsAlive())
    _ApplyAllItemMods();
}

void Player::_LoadItemLoot(std::unique_ptr<QueryResult> queryResult)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT guid,itemid,amount,suffix,property FROM item_loot WHERE guid = '%u'", GetGUIDLow());

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();
            uint32 item_guid   = fields[0].GetUInt32();

            Item* item = GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, item_guid));

            if (!item)
            {
                CharacterDatabase.PExecute("DELETE FROM item_loot WHERE guid = '%u'", item_guid);
                sLog.outError("Player::_LoadItemLoot: Player %s has loot for nonexistent item (GUID: %u) in `item_loot`, deleted.", GetName(), item_guid);
                continue;
            }

            if (!item->m_loot)
                item->m_loot = new Loot(this, item);

            item->LoadLootFromDB(fields);
        }
        while (queryResult->NextRow());
    }
}

// load mailed item which should receive current player (加载当前玩家待收邮件中的附件物品)
void Player::_LoadMailedItems(std::unique_ptr<QueryResult> queryResult)
{
    //        0          1            2                3      4         5        6      7             8                 9           10          11       12         13
    // SELECT itemEntry, creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, randomPropertyId, durability, itemTextId, mail_id, item_guid, item_template FROM mail_items JOIN item_instance ON item_guid = guid WHERE receiver = '%u'", m_guid.GetCounter());
    if (!queryResult)
        return;

    do
    {
        Field* fields = queryResult->Fetch();
        uint32 mail_id       = fields[11].GetUInt32();
        uint32 item_guid_low = fields[12].GetUInt32();
        uint32 item_template = fields[13].GetUInt32();

        Mail* mail = GetMail(mail_id);
        if (!mail)
            continue;
        mail->AddItem(item_guid_low, item_template);

        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_template);

        if (!proto)
        {
            sLog.outError("Player %u has unknown item_template (ProtoType) in mailed items(GUID: %u template: %u) in mail (%u), deleted.", GetGUIDLow(), item_guid_low, item_template, mail->messageID);
            CharacterDatabase.PExecute("DELETE FROM mail_items WHERE item_guid = '%u'", item_guid_low);
            CharacterDatabase.PExecute("DELETE FROM item_instance WHERE guid = '%u'", item_guid_low);
            continue;
        }

        Item* item = NewItemOrBag(proto);

        if (!item->LoadFromDB(item_guid_low, fields, GetObjectGuid()))
        {
            sLog.outError("Player::_LoadMailedItems - Item in mail (%u) doesn't exist !!!! - item guid: %u, deleted from mail", mail->messageID, item_guid_low);
            CharacterDatabase.PExecute("DELETE FROM mail_items WHERE item_guid = '%u'", item_guid_low);
            item->FSetState(ITEM_REMOVED);
            item->SaveToDB();                               // it also deletes item object ! (非法邮件附件直接删除)
            continue;
        }

        AddMItem(item);
    }
    while (queryResult->NextRow());
}

void Player::_LoadMails(std::unique_ptr<QueryResult> queryResult)
{
    m_mail.clear();
    //        0  1           2      3        4       5          6           7            8     9   10      11         12             13
    //"SELECT id,messageType,sender,receiver,subject,itemTextId,expire_time,deliver_time,money,cod,checked,stationery,mailTemplateId,has_items FROM mail WHERE receiver = '%u' ORDER BY id DESC",GetGUIDLow()
    if (!queryResult)
        return;

    do
    {
        Field* fields = queryResult->Fetch();
        Mail* m = new Mail;
        m->messageID = fields[0].GetUInt32();
        m->messageType = fields[1].GetUInt8();
        m->sender = fields[2].GetUInt32();
        m->receiverGuid = ObjectGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
        m->subject = fields[4].GetCppString();
        m->itemTextId = fields[5].GetUInt32();
        m->expire_time = (time_t)fields[6].GetUInt64();
        m->deliver_time = (time_t)fields[7].GetUInt64();
        m->money = fields[8].GetUInt32();
        m->COD = fields[9].GetUInt32();
        m->checked = fields[10].GetUInt32();
        m->stationery = fields[11].GetUInt8();
        m->mailTemplateId = fields[12].GetInt16();
        m->has_items = fields[13].GetBool();                // true, if mail have items or mail have template and items generated (maybe none) (包含实体附件或已生成模板物品)

        if (m->mailTemplateId && !sMailTemplateStore.LookupEntry(m->mailTemplateId))
        {
            sLog.outError("Player::_LoadMail - Mail (%u) have nonexistent MailTemplateId (%u), remove at load", m->messageID, m->mailTemplateId);
            m->mailTemplateId = 0;
        }

        m->state = MAIL_STATE_UNCHANGED;

        m_mail.push_back(m);

        if (m->mailTemplateId && !m->has_items)
            m->prepareTemplateItems(this);
    }
    while (queryResult->NextRow());
}

void Player::LoadPet()
{
    // fixme: the pet should still be loaded if the player is not in world (待优化：即使玩家尚未进入世界也可加载宠物)
    // just not added to the map (仅延迟添加到地图)
    if (IsInWorld())
    {
        Pet* pet = new Pet;
        if (!pet->LoadPetFromDB(this, pet->GetPetSpawnPosition(this), 0, 0, true, 0, true))
        {
            delete pet;
            return;
        }
        if (IsMounted())
            pet->SetModeFlags(PET_MODE_DISABLE_ACTIONS);
    }
}

void Player::_LoadQuestStatus(std::unique_ptr<QueryResult> queryResult)
{
    mQuestStatus.clear();

    uint32 slot = 0;

    ////                                                     0      1       2         3         4      5          6          7          8          9           10          11          12
    // QueryResult *result = CharacterDatabase.PQuery("SELECT quest, status, rewarded, explored, timer, mobcount1, mobcount2, mobcount3, mobcount4, itemcount1, itemcount2, itemcount3, itemcount4 FROM character_queststatus WHERE guid = '%u'", GetGUIDLow());

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            uint32 quest_id = fields[0].GetUInt32();
            // used to be new, no delete? (新状态标记)
            Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
            if (pQuest)
            {
                // find or create (查找或创建任务状态条目)
                QuestStatusData& questStatusData = mQuestStatus[quest_id];

                uint32 qstatus = fields[1].GetUInt32();
                if (qstatus < MAX_QUEST_STATUS)
                    questStatusData.m_status = QuestStatus(qstatus);
                else
                {
                    questStatusData.m_status = QUEST_STATUS_NONE;
                    sLog.outError("Player %s have invalid quest %d status (%d), replaced by QUEST_STATUS_NONE(0).", GetName(), quest_id, qstatus);
                }

                questStatusData.m_rewarded = (fields[2].GetUInt8() > 0);
                questStatusData.m_explored = (fields[3].GetUInt8() > 0);

                time_t quest_time = time_t(fields[4].GetUInt64());

                if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED) && !GetQuestRewardStatus(quest_id) && questStatusData.m_status != QUEST_STATUS_NONE)
                {
                    // A failed quest keeps its time in the quest log, but its timer does not run any more (失败任务保留在任务日志中但不继续倒计时)
                    if (questStatusData.m_status != QUEST_STATUS_FAILED)
                    {
                        AddTimedQuest(quest_id);

                        if (quest_time <= sWorld.GetGameTime())
                            questStatusData.m_timer = 1;
                        else
                            questStatusData.m_timer = uint32(quest_time - sWorld.GetGameTime()) * IN_MILLISECONDS;
                    }
                }
                else
                    quest_time = 0;

                questStatusData.m_creatureOrGOcount[0] = fields[5].GetUInt32();
                questStatusData.m_creatureOrGOcount[1] = fields[6].GetUInt32();
                questStatusData.m_creatureOrGOcount[2] = fields[7].GetUInt32();
                questStatusData.m_creatureOrGOcount[3] = fields[8].GetUInt32();
                questStatusData.m_itemcount[0] = fields[9].GetUInt32();
                questStatusData.m_itemcount[1] = fields[10].GetUInt32();
                questStatusData.m_itemcount[2] = fields[11].GetUInt32();
                questStatusData.m_itemcount[3] = fields[12].GetUInt32();

                questStatusData.uState = QUEST_UNCHANGED;

                // add to quest log (将进行中任务写入玩家任务日志槽位)
                if (slot < MAX_QUEST_LOG_SIZE &&
                        ((questStatusData.m_status == QUEST_STATUS_INCOMPLETE ||
                          questStatusData.m_status == QUEST_STATUS_COMPLETE ||
                          questStatusData.m_status == QUEST_STATUS_FAILED) &&
                         (!questStatusData.m_rewarded || pQuest->IsRepeatable())))
                {
                    SetQuestSlot(slot, quest_id, uint32(quest_time));

                    if (questStatusData.m_explored)
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);

                    if (questStatusData.m_status == QUEST_STATUS_COMPLETE && pQuest->HasQuestFlag(QuestFlags(QUEST_FLAGS_PARTY_ACCEPT | QUEST_FLAGS_EXPLORATION)))
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);

                    if (questStatusData.m_status == QUEST_STATUS_FAILED)
                        SetQuestSlotState(slot, QUEST_STATE_FAIL);

                    for (uint8 idx = 0; idx < QUEST_OBJECTIVES_COUNT; ++idx)
                        if (questStatusData.m_creatureOrGOcount[idx])
                            SetQuestSlotCounter(slot, idx, questStatusData.m_creatureOrGOcount[idx]);

                    ++slot;
                }

                if (questStatusData.m_rewarded)
                {
                    // learn rewarded spell if unknown (补学已完成任务的奖励法术)
                    learnQuestRewardedSpells(pQuest);

                    // set rewarded title if any (恢复已完成任务的奖励称号)
                    if (pQuest->GetCharTitleId())
                    {
                        if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(pQuest->GetCharTitleId()))
                            SetTitle(titleEntry);
                    }
                }

                DEBUG_LOG("Quest status is {%u} for quest {%u} for player (GUID: %u)", questStatusData.m_status, quest_id, GetGUIDLow());
            }
        }
        while (queryResult->NextRow());
    }

    // clear quest log tail (清空任务日志末尾多余槽位)
    for (uint16 i = slot; i < MAX_QUEST_LOG_SIZE; ++i)
        SetQuestSlot(i, 0);
}

void Player::_LoadDailyQuestStatus(std::unique_ptr<QueryResult> queryResult)
{
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
        SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, 0);

    // QueryResult *result = CharacterDatabase.PQuery("SELECT quest FROM character_queststatus_daily WHERE guid = '%u'", GetGUIDLow());

    if (queryResult)
    {
        uint32 quest_daily_idx = 0;

        do
        {
            if (quest_daily_idx >= PLAYER_MAX_DAILY_QUESTS) // max amount with exist data in query (防止超出日常任务计数上限)
            {
                sLog.outError("Player (GUID: %u) have more 25 daily quest records in `charcter_queststatus_daily`", GetGUIDLow());
                break;
            }

            Field* fields = queryResult->Fetch();

            uint32 quest_id = fields[0].GetUInt32();

            Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
            if (!pQuest)
                continue;

            SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, quest_id);
            ++quest_daily_idx;

            DEBUG_LOG("Daily quest {%u} cooldown for player (GUID: %u)", quest_id, GetGUIDLow());
        }
        while (queryResult->NextRow());
    }

    m_DailyQuestChanged = false;
}

void Player::_LoadWeeklyQuestStatus(std::unique_ptr<QueryResult> queryResult)
{
    m_weeklyquests.clear();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT quest FROM character_queststatus_weekly WHERE guid = '%u'", GetGUIDLow());

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            uint32 quest_id = fields[0].GetUInt32();

            Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
            if (!pQuest)
                continue;

            m_weeklyquests.insert(quest_id);

            DEBUG_LOG("Weekly quest {%u} cooldown for player (GUID: %u)", quest_id, GetGUIDLow());
        }
        while (queryResult->NextRow());
    }
    m_WeeklyQuestChanged = false;
}

void Player::_LoadMonthlyQuestStatus(std::unique_ptr<QueryResult> queryResult)
{
    m_monthlyquests.clear();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT quest FROM character_queststatus_monthly WHERE guid = '%u'", GetGUIDLow());

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            uint32 quest_id = fields[0].GetUInt32();

            Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
            if (!pQuest)
                continue;

            m_monthlyquests.insert(quest_id);

            DEBUG_LOG("Monthly quest {%u} cooldown for player (GUID: %u)", quest_id, GetGUIDLow());
        }
        while (queryResult->NextRow());
    }

    m_MonthlyQuestChanged = false;
}

void Player::_LoadSpells(std::unique_ptr<QueryResult> queryResult)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT spell,active,disabled FROM character_spell WHERE guid = '%u'",GetGUIDLow());

    std::vector<std::tuple<uint32, bool, bool>> spells;
    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            uint32 spell_id = fields[0].GetUInt32();
            bool active = fields[1].GetBool();
            bool disabled = fields[2].GetBool();
            TalentSpellPos const* talentPos = GetTalentSpellPos(spell_id);
            if (!talentPos)
                spells.push_back(std::tuple<uint32, bool, bool>{ spell_id, active, disabled });
            else
                addSpell(spell_id, active, false, false, disabled);
        }
        while (queryResult->NextRow());
    }

    // train spells by loaded skills (根据已加载技能补全对应自动学习法术)
    for (auto itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        if (itr->second.uState != SKILL_DELETED)
            UpdateSkillTrainedSpells(uint16(itr->first), SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos))));
    }

    for (auto& data : spells)
        addSpell(std::get<0>(data), std::get<1>(data), false, false, std::get<2>(data));
}

void Player::_LoadGroup(std::unique_ptr<QueryResult> queryResult)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT groupId FROM group_member WHERE memberGuid='%u'", GetGUIDLow());
    if (queryResult)
    {
        uint32 groupId = (*queryResult)[0].GetUInt32();

        if (Group* group = sObjectMgr.GetGroupById(groupId))
        {
            uint8 subgroup = group->GetMemberGroup(GetObjectGuid());
            SetGroup(group, subgroup);
            if (GetLevel() >= LEVELREQUIREMENT_HEROIC)
            {
                // the group leader may change the instance difficulty while the player is offline (玩家离线期间队长可能更改了队伍副本难度，需同步刷新)
                SetDifficulty(group->GetDifficulty());
            }
        }
    }
    UpdateGroupLeaderFlag();
}

void Player::_LoadBoundInstances(std::unique_ptr<QueryResult> queryResult)
{
    for (auto& m_boundInstance : m_boundInstances)
        m_boundInstance.clear();

    Group* group = GetGroup();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT id, permanent, map, difficulty, resettime FROM character_instance LEFT JOIN instance ON instance = id WHERE guid = '%u'", GUID_LOPART(m_guid));
    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();
            bool perm = fields[1].GetBool();
            uint32 mapId = fields[2].GetUInt32();
            uint32 instanceId = fields[0].GetUInt32();
            uint8 difficulty = fields[3].GetUInt8();

            time_t resetTime = (time_t)fields[4].GetUInt64();
            // the resettime for normal instances is only saved when the InstanceSave is unloaded (普通副本重置时间仅在 InstanceSave 卸载时写入数据库，)
            // so the value read from the DB may be wrong here but only if the InstanceSave is loaded (若当前副本实例仍在内存中则内存值更准确，)
            // and in that case it is not used (该 DB 时间仅在实例未加载时使用)

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
            if (!mapEntry || !mapEntry->IsDungeon())
            {
                sLog.outError("_LoadBoundInstances: player %s(%d) has bind to nonexistent or not dungeon map %d", GetName(), GetGUIDLow(), mapId);
                CharacterDatabase.PExecute("DELETE FROM character_instance WHERE guid = '%u' AND instance = '%u'", GetGUIDLow(), instanceId);
                continue;
            }

            if (difficulty >= MAX_DIFFICULTY)
            {
                sLog.outError("_LoadBoundInstances: player %s(%d) has bind to nonexistent difficulty %d instance for map %u", GetName(), GetGUIDLow(), difficulty, mapId);
                CharacterDatabase.PExecute("DELETE FROM character_instance WHERE guid = '%u' AND instance = '%u'", GetGUIDLow(), instanceId);
                continue;
            }

            if (!perm && group)
            {
                sLog.outError("_LoadBoundInstances: %s is in group (Id: %d) but has a non-permanent character bind to map %d,%d,%d",
                              GetGuidStr().c_str(), group->GetId(), mapId, instanceId, difficulty);
                CharacterDatabase.PExecute("DELETE FROM character_instance WHERE guid = '%u' AND instance = '%u'",
                                           GetGUIDLow(), instanceId);
                continue;
            }

            // since non permanent binds are always solo bind, they can always be reset (非永久绑定均为单人普通副本绑定，总是允许重置)
            DungeonPersistentState* state = (DungeonPersistentState*)sMapPersistentStateMgr.AddPersistentState(mapEntry, instanceId, Difficulty(difficulty), resetTime, !perm, true);
            if (state) BindToInstance(state, perm, true);
        }
        while (queryResult->NextRow());
    }
}

InstancePlayerBind* Player::GetBoundInstance(uint32 mapid, Difficulty difficulty)
{
    // some instances only have one difficulty (部分副本仅存在单一难度)
    const MapEntry* entry = sMapStore.LookupEntry(mapid);
    if (!entry || !entry->SupportsHeroicMode())
        difficulty = DUNGEON_DIFFICULTY_NORMAL;

    BoundInstancesMap::iterator itr = m_boundInstances[difficulty].find(mapid);
    if (itr != m_boundInstances[difficulty].end())
        return &itr->second;
    return nullptr;
}

void Player::UnbindInstance(uint32 mapid, Difficulty difficulty, bool unload)
{
    BoundInstancesMap::iterator itr = m_boundInstances[difficulty].find(mapid);
    UnbindInstance(itr, difficulty, unload);
}

void Player::UnbindInstance(BoundInstancesMap::iterator& itr, Difficulty difficulty, bool unload)
{
    if (itr != m_boundInstances[difficulty].end())
    {
        if (!unload)
            CharacterDatabase.PExecute("DELETE FROM character_instance WHERE guid = '%u' AND instance = '%u'",
                                       GetGUIDLow(), itr->second.state->GetInstanceId());
        itr->second.state->RemovePlayer(this);              // state can become invalid (绑定状态可能失效)
        m_boundInstances[difficulty].erase(itr++);
    }
}

/// 将玩家绑定到指定的副本持久化状态（普通临时绑定或英雄/团队永久绑定）
InstancePlayerBind* Player::BindToInstance(DungeonPersistentState* state, bool permanent, bool load)
{
    if (state)
    {
        InstancePlayerBind& bind = m_boundInstances[state->GetDifficulty()][state->GetMapId()];
        if (bind.state)
        {
            // update the state when the group kills a boss (团队击杀首领时更新绑定状态)
            if (permanent != bind.perm || state != bind.state)
                if (!load)
                    CharacterDatabase.PExecute("UPDATE character_instance SET instance = '%u', permanent = '%u' WHERE guid = '%u' AND instance = '%u'",
                                               state->GetInstanceId(), permanent, GetGUIDLow(), bind.state->GetInstanceId());
        }
        else
        {
            if (!load)
                CharacterDatabase.PExecute("INSERT INTO character_instance (guid, instance, permanent) VALUES ('%u', '%u', '%u')",
                                           GetGUIDLow(), state->GetInstanceId(), permanent);
        }

        if (bind.state != state)
        {
            if (bind.state)
                bind.state->RemovePlayer(this);
            state->AddPlayer(this);
        }

        if (permanent)
            state->SetCanReset(false);

        bind.state = state;
        bind.perm = permanent;
        if (!load)
            DEBUG_LOG("Player::BindToInstance: %s(%d) is now bound to map %d, instance %d, difficulty %d",
                      GetName(), GetGUIDLow(), state->GetMapId(), state->GetInstanceId(), state->GetDifficulty());
        return &bind;
    }
    return nullptr;
}

DungeonPersistentState* Player::GetBoundInstanceSaveForSelfOrGroup(uint32 mapid)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry)
        return nullptr;

    InstancePlayerBind* pBind = GetBoundInstance(mapid, GetDifficulty());
    DungeonPersistentState* state = pBind ? pBind->state : nullptr;

    // the player's permanent player bind is taken into consideration first (优先检查玩家自身的永久副本绑定，)
    // then the player's group bind and finally the solo bind. (其次是队伍绑定，最后是单人临时绑定)
    if (!pBind || !pBind->perm)
    {
        // use the player's difficulty setting (it may not be the same as the group's) (使用玩家自身设定的副本难度)
        if (Group* group = GetGroup())
            if (InstanceGroupBind const* groupBind = group->GetBoundInstance(mapid))
                state = groupBind->state;
    }

    return state;
}

void Player::SendRaidInfo()
{
    uint32 counter = 0;

    WorldPacket data(SMSG_RAID_INSTANCE_INFO, 4);

    size_t p_counter = data.wpos();
    data << uint32(counter);                                // placeholder (副本数量占位符)

    time_t now = time(nullptr);

    for (auto& m_boundInstance : m_boundInstances)
    {
        for (BoundInstancesMap::const_iterator itr = m_boundInstance.begin(); itr != m_boundInstance.end(); ++itr)
        {
            if (itr->second.perm)
            {
                DungeonPersistentState* state = itr->second.state;
                data << uint32(state->GetMapId());          // map id (地图 ID)
                data << uint32(state->GetResetTime() - time(nullptr));
                data << uint32(state->GetInstanceId());     // instance id (副本实例 ID)
                data << uint32(counter);
                counter++;
            }
        }
    }
    data.put<uint32>(p_counter, counter);
    GetSession()->SendPacket(data);
}

/*
- called on every successful teleportation to a map (每次成功传送至新地图时调用，发送已绑定副本列表)
*/
void Player::SendSavedInstances()
{
    bool hasBeenSaved = false;
    WorldPacket data;

    for (auto& m_boundInstance : m_boundInstances)
    {
        for (BoundInstancesMap::const_iterator itr = m_boundInstance.begin(); itr != m_boundInstance.end(); ++itr)
        {
            if (itr->second.perm)                           // only permanent binds are sent (仅下发永久进度绑定记录)
            {
                hasBeenSaved = true;
                break;
            }
        }
    }

    // Send opcode 811. true or false means, whether you have current raid/heroic instances (发送是否有团队/英雄副本进度标志)
    data.Initialize(SMSG_UPDATE_INSTANCE_OWNERSHIP);
    data << uint32(hasBeenSaved);
    GetSession()->SendPacket(data);

    if (!hasBeenSaved)
        return;

    for (auto& m_boundInstance : m_boundInstances)
    {
        for (BoundInstancesMap::const_iterator itr = m_boundInstance.begin(); itr != m_boundInstance.end(); ++itr)
        {
            if (itr->second.perm)
            {
                data.Initialize(SMSG_UPDATE_LAST_INSTANCE);
                data << uint32(itr->second.state->GetMapId());
                GetSession()->SendPacket(data);
            }
        }
    }
}

/// convert the player's binds to the group (将玩家的副本进度绑定转换并合并到队伍)
void Player::ConvertInstancesToGroup(Player* player, Group* group, ObjectGuid player_guid)
{
    bool has_binds = false;
    bool has_solo = false;

    if (player)
    {
        player_guid = player->GetObjectGuid();
        if (!group)
            group = player->GetGroup();
    }

    MANGOS_ASSERT(player_guid);

    // copy all binds to the group, when changing leader it's assumed the character (移交队长或组队时拷贝绑定到队伍，)
    // will not have any solo binds (并清理个人单人绑定)

    if (player)
    {
        for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
        {
            for (BoundInstancesMap::iterator itr = player->m_boundInstances[i].begin(); itr != player->m_boundInstances[i].end();)
            {
                has_binds = true;

                if (group)
                    group->BindToInstance(itr->second.state, itr->second.perm, true);

                // permanent binds are not removed (永久副本绑定不会被移除)
                if (!itr->second.perm)
                {
                    // increments itr in call (UnbindInstance 内部会递增迭代器)
                    player->UnbindInstance(itr, Difficulty(i), true);
                    has_solo = true;
                }
                else
                    ++itr;
            }
        }
    }

    uint32 player_lowguid = player_guid.GetCounter();

    // if the player's not online we don't know what binds it has (玩家不在线时无法读取其个人绑定)
    if (!player || !group || has_binds)
        CharacterDatabase.PExecute("INSERT INTO group_instance SELECT guid, instance, permanent FROM character_instance WHERE guid = '%u'", player_lowguid);

    // the following should not get executed when changing leaders (更换队长时不执行常规入队绑定清理)
    if (!player || has_solo)
        CharacterDatabase.PExecute("DELETE FROM character_instance WHERE guid = '%u' AND permanent = 0", player_lowguid);
}

bool Player::_LoadHomeBind(std::unique_ptr<QueryResult> queryResult)
{
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        sLog.outError("Player have incorrect race/class pair. Can't be loaded.");
        return false;
    }

    bool ok = false;
    // QueryResult *result = CharacterDatabase.PQuery("SELECT map,zone,position_x,position_y,position_z FROM character_homebind WHERE guid = '%u'", GUID_LOPART(playerGuid));
    if (queryResult)
    {
        Field* fields = queryResult->Fetch();
        m_homebindMapId = fields[0].GetUInt32();
        m_homebindAreaId = fields[1].GetUInt16();
        m_homebindX = fields[2].GetFloat();
        m_homebindY = fields[3].GetFloat();
        m_homebindZ = fields[4].GetFloat();

        MapEntry const* bindMapEntry = sMapStore.LookupEntry(m_homebindMapId);

        // accept saved data only for valid position (and non instanceable), and accessable (仅接受合法非副本且可抵达的炉石绑定点)
        if (MapManager::IsValidMapCoord(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ) &&
                !bindMapEntry->Instanceable() && GetSession()->GetExpansion() >= bindMapEntry->Expansion())
        {
            ok = true;
        }
        else
            CharacterDatabase.PExecute("DELETE FROM character_homebind WHERE guid = '%u'", GetGUIDLow());
    }

    if (!ok)
    {
        m_homebindMapId = info->mapId;
        m_homebindAreaId = info->areaId;
        m_homebindX = info->positionX;
        m_homebindY = info->positionY;
        m_homebindZ = info->positionZ;

        CharacterDatabase.PExecute("INSERT INTO character_homebind (guid,map,zone,position_x,position_y,position_z) VALUES ('%u', '%u', '%u', '%f', '%f', '%f')", GetGUIDLow(), m_homebindMapId, (uint32)m_homebindAreaId, m_homebindX, m_homebindY, m_homebindZ);
    }

    DEBUG_LOG("Setting player home position: mapid is: %u, zoneid is %u, X is %f, Y is %f, Z is %f",
              m_homebindMapId, m_homebindAreaId, m_homebindX, m_homebindY, m_homebindZ);

    return true;
}

/*********************************************************/
/***                   SAVE SYSTEM (角色保存系统)              ***/
/*********************************************************/

/// 将玩家完整角色状态、背包、技能、法术、任务与冷却保存至数据库
void Player::SaveToDB()
{
    // we should assure this: ASSERT((m_nextSave != sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE))); (重置自动保存倒计时)
    // delay auto save at any saves (manual, in code, or autosave) (无论何种方式触发保存均顺延下次自动保存计时)
    m_nextSave = sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE);

    // lets allow only players in world to be saved (仅允许处于世界中的玩家执行存盘)
    if (IsBeingTeleportedFar())
    {
        ScheduleDelayedOperation(DELAYED_SAVE_PLAYER);
        return;
    }

    // first save/honor gain after midnight will also update the player's honor fields (跨午夜后的首次保存或获得荣誉时同步刷新荣誉字段)
    UpdateHonorFields();

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "The value of player %s at save: ", m_name.c_str());
    outDebugStatsValues();

    CharacterDatabase.BeginTransaction();

    static SqlStatementID delChar ;
    static SqlStatementID insChar ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delChar, "DELETE FROM characters WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    SqlStatement uberInsert = CharacterDatabase.CreateStatement(insChar, "INSERT INTO characters (guid,account,name,race,class,gender,level,xp,money,playerBytes,playerBytes2,playerFlags,"
                              "map, dungeon_difficulty, position_x, position_y, position_z, orientation, "
                              "taximask, online, cinematic, "
                              "totaltime, leveltime, rest_bonus, logout_time, is_logout_resting, resettalents_cost, resettalents_time, "
                              "trans_x, trans_y, trans_z, trans_o, transguid, extra_flags, stable_slots, at_login, zone, "
                              "death_expire_time, taxi_path, arenaPoints, totalHonorPoints, todayHonorPoints, yesterdayHonorPoints, totalKills, "
                              "todayKills, yesterdayKills, chosenTitle, watchedFaction, drunk, health, power1, power2, power3, "
                              "power4, power5, exploredZones, equipmentCache, ammoId, knownTitles, actionBars, grantableLevels, fishingSteps) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                              "?, ?, ?, ?, ?, ?, "
                              "?, ?, ?, "
                              "?, ?, ?, ?, ?, ?, ?, "
                              "?, ?, ?, ?, ?, ?, ?, ?, ?, "
                              "?, ?, ?, ?, ?, ?, ?, "
                              "?, ?, ?, ?, ?, ?, ?, ?, ?, "
                              "?, ?, ?, ?, ?, ?, ?, ?, ?) ");

    uberInsert.addUInt32(GetGUIDLow());
    uberInsert.addUInt32(GetSession()->GetAccountId());
    uberInsert.addString(m_name);
    uberInsert.addUInt8(getRace());
    uberInsert.addUInt8(getClass());
    uberInsert.addUInt8(getGender());
    uberInsert.addUInt32(GetLevel());
    uberInsert.addUInt32(GetUInt32Value(PLAYER_XP));
    uberInsert.addUInt32(GetMoney());
    uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES));
    uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES_2));
    uberInsert.addUInt32(GetUInt32Value(PLAYER_FLAGS));

    if (!IsBeingTeleported())
    {
        uberInsert.addUInt32(GetMapId());
        uberInsert.addUInt32(uint32(GetDifficulty()));
        uberInsert.addFloat(finiteAlways(GetPositionX()));
        uberInsert.addFloat(finiteAlways(GetPositionY()));
        uberInsert.addFloat(finiteAlways(GetPositionZ()));
        uberInsert.addFloat(finiteAlways(GetOrientation()));
    }
    else
    {
        uberInsert.addUInt32(GetTeleportDest().mapid);
        uberInsert.addUInt32(uint32(GetDifficulty()));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().coord_x));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().coord_y));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().coord_z));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().orientation));
    }

    std::ostringstream ss;
    ss << m_taxi;                                   // string with TaxiMaskSize numbers (拼接飞行点掩码字符串)
    uberInsert.addString(ss);

    uberInsert.addUInt32(IsInWorld() ? 1 : 0);

    uberInsert.addUInt32(m_cinematic);

    uberInsert.addUInt32(m_Played_time[PLAYED_TIME_TOTAL]);
    uberInsert.addUInt32(m_Played_time[PLAYED_TIME_LEVEL]);

    uberInsert.addFloat(finiteAlways(m_rest_bonus));
    uberInsert.addUInt64(uint64(time(nullptr)));
    uberInsert.addUInt32(HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) ? 1 : 0);
    // save, far from tavern/city (保存双倍精力：非旅店/主城区域)
    // save, but in tavern/city (保存双倍精力：位于旅店/主城休息区)
    uberInsert.addUInt32(m_resetTalentsCost);
    uberInsert.addUInt64(uint64(m_resetTalentsTime));

    Position const& transportPosition = m_movementInfo.GetTransportPos();
    uberInsert.addFloat(finiteAlways(transportPosition.x));
    uberInsert.addFloat(finiteAlways(transportPosition.y));
    uberInsert.addFloat(finiteAlways(transportPosition.z));
    uberInsert.addFloat(finiteAlways(transportPosition.o));

    if (m_transport)
        uberInsert.addUInt32(m_transport->GetGUIDLow());
    else
        uberInsert.addUInt32(0);

    uberInsert.addUInt32(m_ExtraFlags);

    uberInsert.addUInt32(uint32(m_stableSlots));            // to prevent save uint8 as char (强转为 uint32 防止按字符格式化)

    uberInsert.addUInt32(uint32(m_atLoginFlags));

    uberInsert.addUInt32(IsInWorld() ? GetZoneId() : GetCachedZoneId());

    uberInsert.addUInt64(uint64(m_deathExpireTime));

    ss << m_taxiTracker.Save();
    uberInsert.addString(ss);

    uberInsert.addUInt32(GetArenaPoints());

    uberInsert.addUInt32(GetHonorPoints());

    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));

    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION));

    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS));

    uberInsert.addUInt16(GetUInt16Value(PLAYER_FIELD_KILLS, 0));

    uberInsert.addUInt16(GetUInt16Value(PLAYER_FIELD_KILLS, 1));

    uberInsert.addUInt32(GetUInt32Value(PLAYER_CHOSEN_TITLE));

    // FIXME: at this moment send to DB as unsigned, including unit32(-1) (待修复：当前以无符号整数存入数据库，包括 -1)
    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX));

    uberInsert.addUInt16(uint16(GetUInt32Value(PLAYER_BYTES_3) & 0xFFFE));

    uberInsert.addUInt32(GetHealth());

    for (uint32 i = 0; i < MAX_POWERS; ++i)
        uberInsert.addUInt32(GetPower(Powers(i)));

    for (uint32 i = 0; i < PLAYER_EXPLORED_ZONES_SIZE; ++i) // string (序列化已探索区域掩码字符串)
    {
        ss << GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + i) << " ";
    }
    uberInsert.addString(ss);

    for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)         // string: item id, ench (perm/temp) (序列化装备栏外观缓存字符串)
    {
        ss << GetUInt32Value(PLAYER_VISIBLE_ITEM_1_0 + i * MAX_VISIBLE_ITEM_OFFSET) << " ";

        uint32 ench1 = GetUInt32Value(PLAYER_VISIBLE_ITEM_1_0 + i * MAX_VISIBLE_ITEM_OFFSET + 1 + PERM_ENCHANTMENT_SLOT);
        uint32 ench2 = GetUInt32Value(PLAYER_VISIBLE_ITEM_1_0 + i * MAX_VISIBLE_ITEM_OFFSET + 1 + TEMP_ENCHANTMENT_SLOT);
        ss << uint32(MAKE_PAIR32(ench1, ench2)) << " ";
    }
    // 1 in tbc - 4 in wotlk (TBC 仅记录 1 个附魔槽，WLK 为 4 个)
    for (uint32 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_START + 1; ++i) // string: item id, ench (perm/temp) (写入装备 ID 与附魔)
    {
        ss << (m_items[i] ? m_items[i]->GetEntry() : 0) << " ";
        ss << uint32(MAKE_PAIR32(0, 0)) << " ";
    }
    uberInsert.addString(ss);

    uberInsert.addUInt32(GetUInt32Value(PLAYER_AMMO_ID));

    for (uint32 i = 0; i < 2; ++i)
    {
        ss << GetUInt32Value(PLAYER__FIELD_KNOWN_TITLES + i) << " ";
    }
    uberInsert.addString(ss);

    uberInsert.addUInt32(uint32(GetByteValue(PLAYER_FIELD_BYTES, 2)));

    uberInsert.addUInt32(uint32(m_grantableLevels));

    uberInsert.addUInt8(m_fishingSteps);

    uberInsert.Execute();

    if (m_mailsUpdated)                                     // save mails only when needed (仅在邮件状态发生变更时写入数据库)
        _SaveMail();

    _SaveBGData();
    _SaveInventory();
    _SaveQuestStatus();
    _SaveDailyQuestStatus();
    _SaveWeeklyQuestStatus();
    _SaveMonthlyQuestStatus();
    _SaveSpells();
    _SaveSpellCooldowns();
    _SaveActions();
    _SaveAuras();
    _SaveSkills();
    _SaveNewInstanceIdTimer();
    m_reputationMgr.SaveToDB();
    GetSession()->SaveTutorialsData();                      // changed only while character in game (以下模块仅在玩家正常进入游戏世界后才保存)

    CharacterDatabase.CommitTransaction();

    // check if stats should only be saved on logout (检查属性快照是否配置为仅在登出时保存)
    // save stats can be out of transaction (属性快照保存可在事务外异步执行)
    if (m_session->isLogingOut() || !sWorld.getConfig(CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT))
        _SaveStats();

    // save pet (hunter pet level and experience and all type pets health/mana except priest pet). (保存当前出战宠物状态)
    if (Pet* pet = GetPet())
        pet->SavePetToDB(PET_SAVE_AS_CURRENT, this);
}

// fast save function for item/money cheating preventing - save only inventory and money state (防刷物品/刷金快速保存：仅保存背包物品与金币)
void Player::SaveInventoryAndGoldToDB()
{
    _SaveInventory();
    SaveGoldToDB();
}

void Player::SaveGoldToDB() const
{
    static SqlStatementID updateGold ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(updateGold, "UPDATE characters SET money = ? WHERE guid = ?");
    stmt.PExecute(GetMoney(), GetGUIDLow());
}

void Player::_SaveActions()
{
    static SqlStatementID insertAction ;
    static SqlStatementID updateAction ;
    static SqlStatementID deleteAction ;

    for (ActionButtonList::iterator itr = m_actionButtons.begin(); itr != m_actionButtons.end();)
    {
        switch (itr->second.uState)
        {
            case ACTIONBUTTON_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insertAction, "INSERT INTO character_action (guid,button,action,type) VALUES (?, ?, ?, ?)");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(uint32(itr->first));
                stmt.addUInt32(itr->second.GetAction());
                stmt.addUInt32(uint32(itr->second.GetType()));
                stmt.Execute();
                itr->second.uState = ACTIONBUTTON_UNCHANGED;
                ++itr;
            }
            break;
            case ACTIONBUTTON_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updateAction, "UPDATE character_action  SET action = ?, type = ? WHERE guid = ? AND button = ?");
                stmt.addUInt32(itr->second.GetAction());
                stmt.addUInt32(uint32(itr->second.GetType()));
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(uint32(itr->first));
                stmt.Execute();
                itr->second.uState = ACTIONBUTTON_UNCHANGED;
                ++itr;
            }
            break;
            case ACTIONBUTTON_DELETED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(deleteAction, "DELETE FROM character_action WHERE guid = ? AND button = ?");
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(uint32(itr->first));
                stmt.Execute();
                m_actionButtons.erase(itr++);
            }
            break;
            default:
                ++itr;
                break;
        }
    }
}

void Player::_SaveAuras()
{
    static SqlStatementID deleteAuras ;
    static SqlStatementID insertAuras ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(deleteAuras, "DELETE FROM character_aura WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    SpellAuraHolderMap const& auraHolders = GetSpellAuraHolderMap();

    if (auraHolders.empty())
        return;

    stmt = CharacterDatabase.CreateStatement(insertAuras, "INSERT INTO character_aura (guid, caster_guid, item_guid, spell, stackcount, remaincharges, "
            "basepoints0, basepoints1, basepoints2, periodictime0, periodictime1, periodictime2, maxduration, remaintime, effIndexMask) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (const auto& auraHolder : auraHolders)
    {
        SpellAuraHolder* holder = auraHolder.second;
        // skip all holders from spells that are passive or channeled (跳过被动光环与引导法术光环)
        // save singleTarget auras if self cast. (仅保存对自己施放的单目标光环)
        if (holder->IsSaveToDbHolder())
        {
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];
            uint32 effIndexMask = 0;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = 0;
                periodicTime[i] = 0;

                if (Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                {
                    // don't save not own area auras (不保存非自身施放的区域光环)
                    if (!aur->IsSaveToDbAura())
                        continue;

                    damage[i] = aur->GetModifier()->m_amount;
                    periodicTime[i] = aur->GetModifier()->periodictime;
                    effIndexMask |= (1 << i);
                }
            }

            if (!effIndexMask)
                continue;

            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt64(holder->GetCasterGuid().GetRawValue());
            stmt.addUInt32(holder->GetCastItemGuid().GetCounter());
            stmt.addUInt32(holder->GetId());
            stmt.addUInt32(holder->GetStackAmount());
            stmt.addUInt8(holder->GetAuraCharges());

            for (int i : damage)
                stmt.addInt32(i);

            for (unsigned int i : periodicTime)
                stmt.addUInt32(i);

            stmt.addInt32(holder->GetAuraMaxDuration());
            stmt.addInt32(holder->GetAuraDuration());
            stmt.addUInt32(effIndexMask);
            stmt.Execute();
        }
    }
}

void Player::_SaveInventory()
{
    // force items in buyback slots to new state (清理回购栏物品并重置未保存记录)
    // and remove those that aren't already (清理其余无效回购物品)
    for (uint8 i = BUYBACK_SLOT_START; i < BUYBACK_SLOT_END; ++i)
    {
        Item* item = m_items[i];
        if (!item || item->GetState() == ITEM_NEW) continue;

        static SqlStatementID delInv ;
        static SqlStatementID delItemInst ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delInv, "DELETE FROM character_inventory WHERE item = ?");
        stmt.PExecute(item->GetGUIDLow());

        stmt = CharacterDatabase.CreateStatement(delItemInst, "DELETE FROM item_instance WHERE guid = ?");
        stmt.PExecute(item->GetGUIDLow());

        m_items[i]->FSetState(ITEM_NEW);
    }

    // update enchantment durations (写入限时附魔剩余时间)
#ifdef ENABLE_PLAYERBOTS
    if (!GetPlayerbotAI())
    {
#endif
    for (EnchantDurationList::const_iterator itr = m_enchantDuration.begin(); itr != m_enchantDuration.end(); ++itr)
    {
        itr->item->SetEnchantmentDuration(itr->slot, itr->leftduration);
    }
#ifdef ENABLE_PLAYERBOTS
    }
#endif

    // if no changes (无背包物品变更则直接返回)
    if (m_itemUpdateQueue.empty()) return;

    // do not save if the update queue is corrupt (若物品更新队列异常则中止保存以防丢装备)
    bool error = false;
    for (auto item : m_itemUpdateQueue)
    {
        if (!item || item->GetState() == ITEM_REMOVED) continue;
        Item* test = GetItemByPos(item->GetBagSlot(), item->GetSlot());

        if (test == nullptr)
        {
            sLog.outError("Player(GUID: %u Name: %s)::_SaveInventory - the bag(%d) and slot(%d) values for the item with guid %d are incorrect, the player doesn't have an item at that position!", GetGUIDLow(), GetName(), item->GetBagSlot(), item->GetSlot(), item->GetGUIDLow());
            error = true;
        }
        else if (test != item)
        {
            sLog.outError("Player(GUID: %u Name: %s)::_SaveInventory - the bag(%d) and slot(%d) values for the item with guid %d are incorrect, the item with guid %d is there instead!", GetGUIDLow(), GetName(), item->GetBagSlot(), item->GetSlot(), item->GetGUIDLow(), test->GetGUIDLow());
            error = true;
        }
    }

    if (error)
    {
        sLog.outError("Player::_SaveInventory - one or more errors occurred save aborted!");
        ChatHandler(this).SendSysMessage(LANG_ITEM_SAVE_FAILED);
        return;
    }

    for (auto item : m_itemUpdateQueue)
    {
        if (!item) continue;

        SaveItemToInventory(item);
    }
    m_itemUpdateQueue.clear();
}

void Player::_SaveMail()
{
    static SqlStatementID updateMail ;
    static SqlStatementID deleteMailItems ;

    static SqlStatementID deleteItem ;
    static SqlStatementID deleteItemText;
    static SqlStatementID deleteMain ;
    static SqlStatementID deleteItems ;

    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        Mail* m = (*itr);
        if (m->state == MAIL_STATE_CHANGED)
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(updateMail, "UPDATE mail SET itemTextId = ?,has_items = ?, expire_time = ?, deliver_time = ?, money = ?, cod = ?, checked = ? WHERE id = ?");
            stmt.addUInt32(m->itemTextId);
            stmt.addUInt32(m->HasItems() ? 1 : 0);
            stmt.addUInt64(uint64(m->expire_time));
            stmt.addUInt64(uint64(m->deliver_time));
            stmt.addUInt32(m->money);
            stmt.addUInt32(m->COD);
            stmt.addUInt32(m->checked);
            stmt.addUInt32(m->messageID);
            stmt.Execute();

            if (!m->removedItems.empty())
            {
                stmt = CharacterDatabase.CreateStatement(deleteMailItems, "DELETE FROM mail_items WHERE item_guid = ?");

                for (std::vector<uint32>::const_iterator itr2 = m->removedItems.begin(); itr2 != m->removedItems.end(); ++itr2)
                    stmt.PExecute(*itr2);

                m->removedItems.clear();
            }
            m->state = MAIL_STATE_UNCHANGED;
        }
        else if (m->state == MAIL_STATE_DELETED)
        {
            if (m->HasItems())
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(deleteItem, "DELETE FROM item_instance WHERE guid = ?");
                for (MailItemInfoVec::const_iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                    stmt.PExecute(itr2->item_guid);
            }

            if (m->itemTextId)
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(deleteItemText, "DELETE FROM item_text WHERE id = ?");
                stmt.PExecute(m->itemTextId);
            }

            SqlStatement stmt = CharacterDatabase.CreateStatement(deleteMain, "DELETE FROM mail WHERE id = ?");
            stmt.PExecute(m->messageID);

            stmt = CharacterDatabase.CreateStatement(deleteItems, "DELETE FROM mail_items WHERE mail_id = ?");
            stmt.PExecute(m->messageID);
        }
    }

    // deallocate deleted mails... (释放已删除邮件对象的内存)
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end();)
    {
        if ((*itr)->state == MAIL_STATE_DELETED)
        {
            Mail* m = *itr;
            m_mail.erase(itr);
            delete m;
            itr = m_mail.begin();
        }
        else
            ++itr;
    }

    m_mailsUpdated = false;
}

void Player::_SaveQuestStatus()
{
    static SqlStatementID insertQuestStatus ;

    static SqlStatementID updateQuestStatus ;

    // we don't need transactions here. (任务状态保存无需额外事务)
    for (auto& mQuestStatu : mQuestStatus)
    {
        QuestStatusData& questStatus = mQuestStatu.second;
        switch (questStatus.uState)
        {
            case QUEST_NEW :
            {
                Quest const* quest = sObjectMgr.GetQuestTemplate(mQuestStatu.first);
                if (quest->IsAutoComplete() && !questStatus.m_rewarded)
                    continue;

                SqlStatement stmt = CharacterDatabase.CreateStatement(insertQuestStatus, "INSERT INTO character_queststatus (guid,quest,status,rewarded,explored,timer,mobcount1,mobcount2,mobcount3,mobcount4,itemcount1,itemcount2,itemcount3,itemcount4) "
                                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(mQuestStatu.first);
                stmt.addUInt8(questStatus.m_status);
                stmt.addUInt8(questStatus.m_rewarded);
                stmt.addUInt8(questStatus.m_explored);
                stmt.addUInt64(uint64(questStatus.m_timer / IN_MILLISECONDS + sWorld.GetGameTime()));
                for (unsigned int k : questStatus.m_creatureOrGOcount)
                    stmt.addUInt32(k);
                for (unsigned int k : questStatus.m_itemcount)
                    stmt.addUInt32(k);
                stmt.Execute();
            }
            break;
            case QUEST_CHANGED :
            {
                Quest const* quest = sObjectMgr.GetQuestTemplate(mQuestStatu.first);
                if (quest->IsAutoComplete() && !questStatus.m_rewarded)
                    continue;

                SqlStatement stmt = CharacterDatabase.CreateStatement(updateQuestStatus, "UPDATE character_queststatus SET status = ?,rewarded = ?,explored = ?,timer = ?,"
                                    "mobcount1 = ?,mobcount2 = ?,mobcount3 = ?,mobcount4 = ?,itemcount1 = ?,itemcount2 = ?,itemcount3 = ?,itemcount4 = ?  WHERE guid = ? AND quest = ?");

                stmt.addUInt8(questStatus.m_status);
                stmt.addUInt8(questStatus.m_rewarded);
                stmt.addUInt8(questStatus.m_explored);
                stmt.addUInt64(uint64(questStatus.m_timer / IN_MILLISECONDS + sWorld.GetGameTime()));
                for (unsigned int k : questStatus.m_creatureOrGOcount)
                    stmt.addUInt32(k);
                for (unsigned int k : questStatus.m_itemcount)
                    stmt.addUInt32(k);
                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(mQuestStatu.first);
                stmt.Execute();
            }
            break;
            case QUEST_UNCHANGED:
                break;
        }
        questStatus.uState = QUEST_UNCHANGED;
    }
}

void Player::_SaveDailyQuestStatus()
{
    if (!m_DailyQuestChanged)
        return;

    // we don't need transactions here. (日常任务保存无需额外事务)
    static SqlStatementID delQuestStatus ;
    static SqlStatementID insQuestStatus ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delQuestStatus, "DELETE FROM character_queststatus_daily WHERE guid = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insQuestStatus, "INSERT INTO character_queststatus_daily (guid,quest) VALUES (?, ?)");

    stmtDel.PExecute(GetGUIDLow());

    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
        if (GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx))
            stmtIns.PExecute(GetGUIDLow(), GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx));

    m_DailyQuestChanged = false;
}

void Player::_SaveWeeklyQuestStatus()
{
    if (!m_WeeklyQuestChanged || m_weeklyquests.empty())
        return;

    // we don't need transactions here. (周常任务保存无需额外事务)
    static SqlStatementID delQuestStatus;
    static SqlStatementID insQuestStatus;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delQuestStatus, "DELETE FROM character_queststatus_weekly WHERE guid = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insQuestStatus, "INSERT INTO character_queststatus_weekly (guid,quest) VALUES (?, ?)");

    stmtDel.PExecute(GetGUIDLow());

    for (uint32 quest_id : m_weeklyquests)
        stmtIns.PExecute(GetGUIDLow(), quest_id);

    m_WeeklyQuestChanged = false;
}

void Player::_SaveMonthlyQuestStatus()
{
    if (!m_MonthlyQuestChanged || m_monthlyquests.empty())
        return;

    // we don't need transactions here. (月常任务保存无需额外事务)
    static SqlStatementID deleteQuest;
    static SqlStatementID insertQuest;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(deleteQuest, "DELETE FROM character_queststatus_monthly WHERE guid = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insertQuest, "INSERT INTO character_queststatus_monthly (guid, quest) VALUES (?, ?)");

    stmtDel.PExecute(GetGUIDLow());

    for (uint32 quest_id : m_monthlyquests)
    {
        stmtIns.PExecute(GetGUIDLow(), quest_id);
    }

    m_MonthlyQuestChanged = false;
}

void Player::_SaveSkills()
{
    static SqlStatementID delSkills ;
    static SqlStatementID insSkills ;
    static SqlStatementID updSkills ;

    // we don't need transactions here. (技能保存无需额外事务)
    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end();)
    {
        if (itr->second.uState == SKILL_UNCHANGED)
        {
            ++itr;
            continue;
        }

        if (itr->second.uState == SKILL_DELETED)
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(delSkills, "DELETE FROM character_skills WHERE guid = ? AND skill = ?");
            stmt.PExecute(GetGUIDLow(), itr->first);
            mSkillStatus.erase(itr++);
            continue;
        }

        uint32 valueData = GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos));
        uint16 value = SKILL_VALUE(valueData);
        uint16 max = SKILL_MAX(valueData);

        switch (itr->second.uState)
        {
            case SKILL_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insSkills, "INSERT INTO character_skills (guid, skill, value, max) VALUES (?, ?, ?, ?)");
                stmt.PExecute(GetGUIDLow(), itr->first, value, max);
            }
            break;
            case SKILL_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updSkills, "UPDATE character_skills SET value = ?, max = ? WHERE guid = ? AND skill = ?");
                stmt.PExecute(value, max, GetGUIDLow(), itr->first);
            }
            break;
            case SKILL_UNCHANGED:
            case SKILL_DELETED:
                MANGOS_ASSERT(false);
                break;
        }
        itr->second.uState = SKILL_UNCHANGED;

        ++itr;
    }
}

void Player::_SaveSpells()
{
    static SqlStatementID delSpells ;
    static SqlStatementID insSpells ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delSpells, "DELETE FROM character_spell WHERE guid = ? and spell = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insSpells, "INSERT INTO character_spell (guid,spell,active,disabled) VALUES (?, ?, ?, ?)");

    for (PlayerSpellMap::iterator itr = m_spells.begin(); itr != m_spells.end();)
    {
        PlayerSpell& playerSpell = itr->second;

        if (playerSpell.state == PLAYERSPELL_REMOVED || playerSpell.state == PLAYERSPELL_CHANGED)
            stmtDel.PExecute(GetGUIDLow(), itr->first);

        // add only changed/new not dependent spells (仅保存新增或变更的非依赖派生法术)
        if (!playerSpell.dependent && (playerSpell.state == PLAYERSPELL_NEW || playerSpell.state == PLAYERSPELL_CHANGED))
            stmtIns.PExecute(GetGUIDLow(), itr->first, uint8(playerSpell.active ? 1 : 0), uint8(playerSpell.disabled ? 1 : 0));

        if (playerSpell.state == PLAYERSPELL_REMOVED)
            m_spells.erase(itr++);
        else
        {
            playerSpell.state = PLAYERSPELL_UNCHANGED;
            ++itr;
        }
    }
}

// save player stats -- only for external usage (保存角色属性快照供网页/外部查询使用，)
// real stats will be recalculated on player login (实际游戏属性在登录时重新计算)
void Player::_SaveStats()
{
    // check if stat saving is enabled and if char level is high enough (检查属性保存开关及最低等级门槛)
    if (!sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE) || GetLevel() < sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE))
        return;

    static SqlStatementID delStats ;
    static SqlStatementID insertStats ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delStats, "DELETE FROM character_stats WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    stmt = CharacterDatabase.CreateStatement(insertStats, "INSERT INTO character_stats (guid, maxhealth, maxpower1, maxpower2, maxpower3, maxpower4, maxpower5, "
            "strength, agility, stamina, intellect, spirit, armor, resHoly, resFire, resNature, resFrost, resShadow, resArcane, "
            "blockPct, dodgePct, parryPct, critPct, rangedCritPct, spellCritPct, attackPower, rangedAttackPower, spellPower) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    stmt.addUInt32(GetGUIDLow());
    stmt.addUInt32(GetMaxHealth());
    for (int i = 0; i < MAX_POWERS; ++i)
        stmt.addUInt32(GetMaxPower(Powers(i)));
    for (int i = 0; i < MAX_STATS; ++i)
        stmt.addFloat(GetStat(Stats(i)));
    // armor + school resistances (写入护甲与六系魔法抗性)
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        stmt.addInt32(GetResistance(SpellSchools(i)));
    stmt.addFloat(GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_DODGE_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_PARRY_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_CRIT_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1));
    stmt.addUInt32(GetUInt32Value(UNIT_FIELD_ATTACK_POWER));
    stmt.addUInt32(GetUInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER));
    stmt.addUInt32(GetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS));

    stmt.Execute();
}

void Player::outDebugStatsValues() const
{
    // optimize disabled debug output (若未开启调试日志级别则直接跳过)
    if (!sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG) || sLog.HasLogFilter(LOG_FILTER_PLAYER_STATS))
        return;

    sLog.outDebug("HP is: \t\t\t%u\t\tMP is: \t\t\t%u", GetMaxHealth(), GetMaxPower(POWER_MANA));
    sLog.outDebug("AGILITY is: \t\t%f\t\tSTRENGTH is: \t\t%f", GetStat(STAT_AGILITY), GetStat(STAT_STRENGTH));
    sLog.outDebug("INTELLECT is: \t\t%f\t\tSPIRIT is: \t\t%f", GetStat(STAT_INTELLECT), GetStat(STAT_SPIRIT));
    sLog.outDebug("STAMINA is: \t\t%f", GetStat(STAT_STAMINA));
    sLog.outDebug("Armor is: \t\t%i\t\tBlock is: \t\t%f", GetArmor(), GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    sLog.outDebug("HolyRes is: \t\t%i\t\tFireRes is: \t\t%i", GetResistance(SPELL_SCHOOL_HOLY), GetResistance(SPELL_SCHOOL_FIRE));
    sLog.outDebug("NatureRes is: \t\t%i\t\tFrostRes is: \t\t%i", GetResistance(SPELL_SCHOOL_NATURE), GetResistance(SPELL_SCHOOL_FROST));
    sLog.outDebug("ShadowRes is: \t\t%i\t\tArcaneRes is: \t\t%i", GetResistance(SPELL_SCHOOL_SHADOW), GetResistance(SPELL_SCHOOL_ARCANE));
    sLog.outDebug("MIN_DAMAGE is: \t\t%f\tMAX_DAMAGE is: \t\t%f", GetFloatValue(UNIT_FIELD_MINDAMAGE), GetFloatValue(UNIT_FIELD_MAXDAMAGE));
    sLog.outDebug("MIN_OFFHAND_DAMAGE is: \t%f\tMAX_OFFHAND_DAMAGE is: \t%f", GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE), GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE));
    sLog.outDebug("MIN_RANGED_DAMAGE is: \t%f\tMAX_RANGED_DAMAGE is: \t%f", GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE), GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE));
    sLog.outDebug("ATTACK_TIME is: \t%u\t\tRANGE_ATTACK_TIME is: \t%u", GetAttackTime(BASE_ATTACK), GetAttackTime(RANGED_ATTACK));
}

/*********************************************************/
/***             FLOOD FILTER SYSTEM (聊天防刷屏系统)    ***/
/*********************************************************/

/// 更新聊天防刷屏时间戳并在超限触发时实施短暂禁言
void Player::UpdateSpeakTime()
{
    // ignore chat spam protection for GMs in any mode (GM 账号不受聊天刷屏限制)
    if (GetSession()->GetSecurity() > SEC_PLAYER)
        return;

    time_t current = time(nullptr);
    if (m_speakTime > current)
    {
        uint32 max_count = sWorld.getConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT);
        if (!max_count)
            return;

        ++m_speakCount;
        if (m_speakCount >= max_count)
        {
            // prevent overwrite mute time, if message send just before mutes set, for example. (避免短时间消息覆盖已有更长的禁言时间)
            time_t new_mute = current + sWorld.getConfig(CONFIG_UINT32_CHATFLOOD_MUTE_TIME);
            if (GetSession()->m_muteTime < new_mute)
                GetSession()->m_muteTime = new_mute;

            m_speakCount = 0;
        }
    }
    else
        m_speakCount = 0;

    m_speakTime = current + sWorld.getConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY);
}

bool Player::CanSpeak() const
{
    return  GetSession()->m_muteTime <= time(nullptr);
}

/*********************************************************/
/***              LOW LEVEL FUNCTIONS:Notifiers (底层通知消息发送) ***/
/*********************************************************/

void Player::SendAttackSwingNotInRange() const
{
    WorldPacket data(SMSG_ATTACKSWING_NOTINRANGE, 0);
    GetSession()->SendPacket(data);
}

void Player::SavePositionInDB(ObjectGuid guid, uint32 mapid, float x, float y, float z, float o, uint32 zone)
{
    std::ostringstream ss;
    ss << "UPDATE characters SET position_x='" << x << "',position_y='" << y
       << "',position_z='" << z << "',orientation='" << o << "',map='" << mapid
       << "',zone='" << zone << "',trans_x='0',trans_y='0',trans_z='0',"
       << "transguid='0',taxi_path='' WHERE guid='" << guid.GetCounter() << "'";
    DEBUG_LOG("%s", ss.str().c_str());
    CharacterDatabase.Execute(ss.str().c_str());
}

void Player::SetUInt32ValueInArray(Tokens& tokens, uint16 index, uint32 value)
{
    char buf[11];
    snprintf(buf, 11, "%u", value);

    if (index >= tokens.size())
        return;

    tokens[index] = buf;
}

void Player::SendAttackSwingNotStanding() const
{
    WorldPacket data(SMSG_ATTACKSWING_NOTSTANDING, 0);
    GetSession()->SendPacket(data);
}

void Player::SendAttackSwingDeadTarget() const
{
    WorldPacket data(SMSG_ATTACKSWING_DEADTARGET, 0);
    GetSession()->SendPacket(data);
}

void Player::SendAttackSwingCantAttack() const
{
    WorldPacket data(SMSG_ATTACKSWING_CANT_ATTACK, 0);
    GetSession()->SendPacket(data);
}

void Player::SendAttackSwingCancelAttack() const
{
    WorldPacket data(SMSG_CANCEL_COMBAT, 0);
    GetSession()->SendPacket(data);
}

void Player::SendAttackSwingBadFacingAttack() const
{
    WorldPacket data(SMSG_ATTACKSWING_BADFACING, 0);
    GetSession()->SendPacket(data);
}

void Player::SendAutoRepeatCancel() const
{
    WorldPacket data(SMSG_CANCEL_AUTO_REPEAT, 0);
    GetSession()->SendPacket(data);
}

void Player::SendFeignDeathResisted() const
{
    WorldPacket data(SMSG_FEIGN_DEATH_RESISTED, 0);
    GetSession()->SendPacket(data);
}

void Player::SendExplorationExperience(uint32 Area, uint32 Experience) const
{
    WorldPacket data(SMSG_EXPLORATION_EXPERIENCE, 8);
    data << uint32(Area);
    data << uint32(Experience);
    GetSession()->SendPacket(data);
}

void Player::SendDungeonDifficulty(bool IsInGroup) const
{
    uint8 val = 0x00000001;
    WorldPacket data(MSG_SET_DUNGEON_DIFFICULTY, 12);
    data << (uint32)GetDifficulty();
    data << uint32(val);
    data << uint32(IsInGroup);
    GetSession()->SendPacket(data);
}

void Player::SendResetFailedNotify(uint32 mapid) const
{
    WorldPacket data(SMSG_RESET_FAILED_NOTIFY, 4);
    data << uint32(mapid);
    GetSession()->SendPacket(data);
}

/// Reset all solo instances and optionally send a message on success for each (重置所有单人副本并在需要时逐个发送成功通知)
/// 重置玩家所有可重置的单人普通副本实例
void Player::ResetInstances(InstanceResetMethod method)
{
    // method can be INSTANCE_RESET_ALL, INSTANCE_RESET_CHANGE_DIFFICULTY, INSTANCE_RESET_GROUP_JOIN (重置方式：手动全重置、切换难度或入队合并)

    // we assume that when the difficulty changes, all instances that can be reset will be (切换难度时假定所有可重置副本均需重置)
    Difficulty diff = GetDifficulty();

    for (BoundInstancesMap::iterator itr = m_boundInstances[diff].begin(); itr != m_boundInstances[diff].end();)
    {
        DungeonPersistentState* state = itr->second.state;
        const MapEntry* entry = sMapStore.LookupEntry(itr->first);
        if (!entry || !state->CanReset())
        {
            ++itr;
            continue;
        }

        if (method == INSTANCE_RESET_ALL)
        {
            // the "reset all instances" method can only reset normal maps (“重置所有副本”仅能重置普通难度地下城)
            if (entry->map_type == MAP_RAID || diff == DUNGEON_DIFFICULTY_HEROIC)
            {
                ++itr;
                continue;
            }
        }

        // if the map is loaded, reset it (若副本当前已加载在内存中，执行在线重置)
        if (Map* map = sMapMgr.FindMap(state->GetMapId(), state->GetInstanceId()))
            if (map->IsDungeon())
                ((DungeonMap*)map)->Reset(method);

        // since this is a solo instance there should not be any players inside (单人副本中不应仍有其他玩家停留)
        if (method == INSTANCE_RESET_ALL || method == INSTANCE_RESET_CHANGE_DIFFICULTY)
            SendResetInstanceSuccess(state->GetMapId());

        state->DeleteFromDB();
        m_boundInstances[diff].erase(itr++);

        // the following should remove the instance save from the manager and delete it as well (解除绑定后从管理器移除并销毁副本持久化状态)
        state->RemovePlayer(this);
    }
}

void Player::SendResetInstanceSuccess(uint32 MapId) const
{
    WorldPacket data(SMSG_INSTANCE_RESET, 4);
    data << uint32(MapId);
    GetSession()->SendPacket(data);
}

void Player::SendResetInstanceFailed(uint32 reason, uint32 MapId) const
{
    // TODO: find what other fail reasons there are besides players in the instance (待办：补充除副本内有玩家外的其他重置失败原因)
    // 0 - at least one player is in the instance (0 - 副本内仍有玩家)
    // 1 - at least one player is offline (1 - 至少有一名成员离线)
    // 2 - at least one player try to enter the instance (being teleported in) (2 - 有玩家正在传送进入副本)
    WorldPacket data(SMSG_INSTANCE_RESET_FAILED, 4);
    data << uint32(reason);
    data << uint32(MapId);
    GetSession()->SendPacket(data);
}

/*********************************************************/
/***              Update timers (状态计时器更新)               ***/
/*********************************************************/

/// checks the 15 afk reports per 5 minutes limit (检查每 5 分钟最多举报 15 名暂离玩家的频率限制)
void Player::UpdateAfkReport(time_t currTime)
{
    if (m_bgData.bgAfkReportedTimer <= currTime)
    {
        m_bgData.bgAfkReportedCount = 0;
        m_bgData.bgAfkReportedTimer = currTime + 5 * MINUTE;
    }
}

void Player::UpdatePvPFlagTimer(uint32 diff)
{
    // Freeze flag timer while participating in PvP combat, in pvp enforced zone, in capture points, when carrying flag or on player preference (参与 PvP 战斗、PvP 强制区、占点、持旗或手动开启 PvP 时冻结倒计时)
    if (!pvpInfo.inPvPCombat && !pvpInfo.inPvPEnforcedArea && !pvpInfo.inPvPCapturePoint && !pvpInfo.isPvPFlagCarrier && !HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_PVP_DESIRED))
        pvpInfo.timerPvPRemaining -= std::min(pvpInfo.timerPvPRemaining, diff);

    // Timer tries to drop flag if all conditions are met and time has passed (满足条件且计时归零时自动关闭 PvP 标志)
    UpdatePvP(false);
}

void Player::UpdatePvPContestedFlagTimer(uint32 diff)
{
    // Freeze flag timer while participating in PvP combat (在 PvP 战斗中冻结争夺区域 PvP 计时)
    if (!pvpInfo.inPvPCombat)
        pvpInfo.timerPvPContestedRemaining -= std::min(pvpInfo.timerPvPContestedRemaining, diff);

    // Timer tries to drop flag if all conditions are met and time has passed (计时结束后关闭争夺区域 PvP 标志)
    UpdatePvPContested(false);
}

void Player::UpdateDuelFlag(time_t currTime)
{
    if (!duel || duel->startTimer == 0 || currTime < duel->startTimer + 3)
        return;

    SetUInt32Value(PLAYER_DUEL_TEAM, 1);
    duel->opponent->SetUInt32Value(PLAYER_DUEL_TEAM, 2);

    duel->startTimer = 0;
    duel->startTime  = currTime;
    duel->opponent->duel->startTimer = 0;
    duel->opponent->duel->startTime  = currTime;
}

void Player::RemovePet(PetSaveMode mode)
{
    if (Pet* pet = GetPet())
        pet->Unsummon(mode, this);
    else if (m_temporaryUnsummonedPetNumber && mode == PET_SAVE_REAGENTS)
    {
        // TODO: Only edit pet in DB and reward reagent if necessary (待办：按需优化宠物存库与施法材料返还)
        Pet* NewPet = new Pet;
        if (!NewPet->LoadPetFromDB(this, NewPet->GetPetSpawnPosition(this), 0, m_temporaryUnsummonedPetNumber, true, 0, false, true))
        {
            delete NewPet;
            NewPet = nullptr;
        }

        m_temporaryUnsummonedPetNumber = 0;
        if (NewPet)
            NewPet->Unsummon(mode, this);
    }
}

void Player::Say(const std::string& text, const uint32 language) const
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_SAY, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    if (m_session->GetAnticheat()->IsSilenced())
        m_session->SendPacket(data);
    else
        SendMessageToSetInRange(data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY), true);
}

void Player::Yell(const std::string& text, const uint32 language) const
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_YELL, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    if (m_session->GetAnticheat()->IsSilenced())
        m_session->SendPacket(data);
    else
        SendMessageToSetInRange(data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL), true);
}

void Player::TextEmote(const std::string& text) const
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_EMOTE, text.c_str(), LANG_UNIVERSAL, GetChatTag(), GetObjectGuid(), GetName());
    if (m_session->GetAnticheat()->IsSilenced())
        m_session->SendPacket(data);
    else
        SendMessageToSetInRange(data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), true, !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT));
}

void Player::Whisper(const std::string& text, uint32 language, ObjectGuid receiver)
{
    if (language != LANG_ADDON)                             // if not addon data (非插件通信消息时，)
        language = LANG_UNIVERSAL;                          // whispers should always be readable (确保跨语言密语始终可正常阅读)

    Player* rPlayer = sObjectMgr.GetPlayer(receiver);

    WorldPacket data;

    if (!m_session->GetAnticheat()->IsSilenced())
    {
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
        rPlayer->GetSession()->SendPacket(data);
    }

    // do not send confirmations, afk, dnd or system notifications for addon messages (插件密语不回发确认包、AFK/DND 自动回复或系统提示)
    if (language == LANG_ADDON)
        return;

    data.clear();
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER_INFORM, text.c_str(), Language(language), CHAT_TAG_NONE, rPlayer->GetObjectGuid());
    GetSession()->SendPacket(data);

    if (!isAcceptWhispers())
    {
        SetAcceptWhispers(true);
        ChatHandler(this).SendSysMessage(LANG_COMMAND_WHISPERON);
    }

    // announce afk or dnd message (若对方处于暂离 AFK 或勿扰 DND 状态，自动回传状态提示)
    if (rPlayer->isAFK() || rPlayer->isDND())
    {
        const ChatMsg msgtype = rPlayer->isAFK() ? CHAT_MSG_AFK : CHAT_MSG_DND;
        data.clear();
        if (rPlayer->GetSession()->GetAnticheat()->IsSilenced())
        {
            const char* msgdefault = GetSession()->GetMangosString(rPlayer->isAFK() ? LANG_PLAYER_AFK_DEFAULT : LANG_PLAYER_DND_DEFAULT);
            ChatHandler::BuildChatPacket(data, msgtype, msgdefault, LANG_UNIVERSAL, CHAT_TAG_NONE, rPlayer->GetObjectGuid());
            return GetSession()->SendPacket(data);
        }

        ChatHandler::BuildChatPacket(data, msgtype, rPlayer->autoReplyMsg.c_str(), LANG_UNIVERSAL, CHAT_TAG_NONE, rPlayer->GetObjectGuid());
        GetSession()->SendPacket(data);
    }
}

void Player::PetSpellInitialize() const
{
    Pet* pet = GetPet();

    if (!pet)
        return;

    DEBUG_LOG("Pet Spells Groups");

    CharmInfo* charmInfo = pet->GetCharmInfo();

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 1 + 1 + 2 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    data << pet->GetObjectGuid();
    data << uint32(0);
    data << uint8(pet->AI()->GetReactState()) << uint8(charmInfo->GetCommandState()) << uint16(0);

    // action bar loop (写入宠物动作条 10 个槽位)
    charmInfo->BuildActionBar(data);

    size_t spellsCountPos = data.wpos();

    // spells count (写入宠物法术数量占位)
    uint8 addlist = 0;
    data << uint8(addlist);                                 // placeholder (数量占位符)

    if (pet->isControlled())
    {
        // spells loop (写入宠物可施放法术列表)
        for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
        {
            if (itr->second.state == PETSPELL_REMOVED)
                continue;

            data << uint32(MAKE_UNIT_ACTION_BUTTON(itr->first, itr->second.active));
            ++addlist;
        }
    }

    data.put<uint8>(spellsCountPos, addlist);

    pet->CharmCooldownInitialize(data);

    GetSession()->SendPacket(data);
}

void Player::PossessSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
        return;

    CharmInfo* charmInfo = charm->GetCharmInfo();

    if (!charmInfo)
    {
        sLog.outError("Player::PossessSpellInitialize(): charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 4 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    data << charm->GetObjectGuid();
    data << uint32(0);
    data << uint32(0);

    charmInfo->BuildActionBar(data);

    data << uint8(0);                                       // spells count (写入附身/控制生物法术数量)
    
    charm->CharmCooldownInitialize(data);

    GetSession()->SendPacket(data);
}

void Player::CharmSpellInitialize() const
{
    Unit* charm = GetCharm();

    if (!charm)
        return;

    CharmInfo* charmInfo = charm->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("Player::CharmSpellInitialize(): the player's charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    uint8 addlist = 0;

    if (charm->GetTypeId() != TYPEID_PLAYER)
    {
        CreatureInfo const* cinfo = ((Creature*)charm)->GetCreatureInfo();

        if (cinfo && cinfo->CreatureType == CREATURE_TYPE_DEMON && getClass() == CLASS_WARLOCK)
        {
            for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
            {
                if (charmInfo->GetCharmSpell(i)->GetAction())
                    ++addlist;
            }
        }
    }

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 1 + 1 + 2 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 4 * uint32(addlist) + 1);
    data << charm->GetObjectGuid();
    data << uint32(0x00000000);
    data << uint8(charm->AI()->GetReactState()) << uint8(charmInfo->GetCommandState()) << uint16(0);

    charmInfo->BuildActionBar(data);

    data << uint8(addlist);

    if (addlist)
    {
        for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        {
            CharmSpellEntry* cspell = charmInfo->GetCharmSpell(i);
            if (cspell->GetAction())
                data << uint32(cspell->packedData);
        }
    }

    charm->CharmCooldownInitialize(data);

    GetSession()->SendPacket(data);
}

void Player::RemovePetActionBar() const
{
    WorldPacket data(SMSG_PET_SPELLS, 8);
    data << ObjectGuid();
    SendDirectMessage(data);
}

std::pair<float, float> Player::RequestFollowData(ObjectGuid guid)
{
    uint32 slotId = 0;
    for (uint32 i = 0; i < 10; ++i)
    {
        if (m_followAngles.find(i) == m_followAngles.end())
        {
            slotId = i;
            break;
        }
    }
    m_followAngles.emplace(slotId, guid);
    switch (slotId)
    {
        case 0: return { 3 * M_PI_F / 6, 1.5f }; // left (左方向)
        case 1: return { 9 * M_PI_F / 6, 1.5f }; // right (右方向)
        case 2: return { 7 * M_PI_F / 6, 1.5f };
        case 3: return { 5 * M_PI_F / 6, 1.5f };
        case 4: return { 6 * M_PI_F / 6, 1.5f };
        case 5: return { 4 * M_PI_F / 6, 1.5f };
        case 6: return { 8 * M_PI_F / 6, 1.5f };
        case 7: return { 5.5f * M_PI_F / 6, 1.5f };
        case 8: return { 6.5f * M_PI_F / 6, 1.5f };
        case 9: return { 4.5f * M_PI_F / 6, 1.5f };
        default:
        case 10: return { 7.5f * M_PI_F / 6, 1.5f };
    }
}

void Player::RelinquishFollowData(ObjectGuid guid)
{
    for (auto itr = m_followAngles.begin(); itr != m_followAngles.end(); ++itr)
    {
        if (itr->second == guid)
        {
            m_followAngles.erase(itr);
            return;
        }
    }
}

bool Player::IsAffectedBySpellmod(SpellEntry const* spellInfo, SpellModifier* mod, std::set<SpellModifierPair>* consumedMods)
{
    if (!mod || !spellInfo)
        return false;

    if (mod->charges == -1)            // marked as expired but locked until spell casting finish (已耗尽但锁定至当前施法结束的修正)
    {
        // prevent apply to any spell except spell that trigger expire (禁止应用于非触发消耗的后续法术)
        if (consumedMods)
        {
            if (consumedMods->find(SpellModifierPair(mod->spellId, mod->modId)) == consumedMods->end())
                return false;
        }
    }

    return mod->isAffectedOnSpell(spellInfo);
}

void Player::AddSpellMod(SpellModifier* mod, bool apply)
{
    Opcodes opcode = (mod->type == SPELLMOD_FLAT) ? SMSG_SET_FLAT_SPELL_MODIFIER : SMSG_SET_PCT_SPELL_MODIFIER;

    for (int eff = 0; eff < 64; ++eff)
    {
        uint64 _mask = uint64(1) << eff;
        if (mod->mask.IsFitToFamilyMask(_mask))
        {
            int32 val = 0;
            for (SpellModifier* modifier : m_spellMods[mod->op])
            {
                if (modifier->type == mod->type && (modifier->mask.IsFitToFamilyMask(_mask)))
                    val += modifier->value;
            }
            val += apply ? mod->value : -(mod->value);
            WorldPacket data(opcode, (1 + 1 + 4));
            data << uint8(eff);
            data << uint8(mod->op);
            data << int32(val);
            SendDirectMessage(data);
        }
    }

    if (apply)
    {
        m_spellMods[mod->op].push_back(mod);
        m_spellMods[mod->op].sort(SpellModifierComparator());
    }
    else
    {
        m_spellMods[mod->op].remove(mod);
        delete mod;
    }
}

void Player::SendAllSpellMods(SpellModType modType)
{
    Opcodes opcode = (modType == SPELLMOD_FLAT) ? SMSG_SET_FLAT_SPELL_MODIFIER : SMSG_SET_PCT_SPELL_MODIFIER;
    for (uint32 eff = 0; eff < 64; ++eff)
    {
        for (uint32 op = 0; op < MAX_SPELLMOD; ++op)
        {
            uint64 _mask = uint64(1) << eff;
            int32 val = 0;
            for (SpellModifier* modifier : m_spellMods[op])
            {
                if (modifier->type == modType && (modifier->mask.IsFitToFamilyMask(_mask)))
                    val += modifier->value;
            }

            if (val == 0) // do not send zero payload (无该类型法术修正则不发空包)
                continue;

            WorldPacket data(opcode, (1 + 1 + 4));
            data << uint8(eff);
            data << uint8(op);
            data << int32(val);
            SendDirectMessage(data);
        }
    }
}

SpellModifier* Player::GetSpellMod(SpellModOp op, uint32 spellId) const
{
    for (SpellModifier* mod : m_spellMods[op])
        if (mod->spellId == spellId)
            return mod;

    return nullptr;
}

void Player::RemoveSpellMods(std::set<SpellModifierPair>& usedAuraCharges)
{
    if (!usedAuraCharges.size())
        return;

    for (const SpellModifierPair& spellModifierPair : usedAuraCharges)
    {
        if (SpellAuraHolder* holder = GetSpellAuraHolder(spellModifierPair.spellId))
        {
            if (holder->HasModifier(spellModifierPair.modId))
            {
                if (holder->DropAuraCharge())
                    RemoveAurasDueToSpell(spellModifierPair.spellId);
            }
        }
    }

    usedAuraCharges.clear();
}

void Player::ResetSpellModsDueToCanceledSpell(std::set<SpellModifierPair>& usedAuraCharges)
{
    if (!usedAuraCharges.size())
        return;

    for (const SpellModifierPair& spellModifierPair : usedAuraCharges)
        if (SpellAuraHolder* holder = GetSpellAuraHolder(spellModifierPair.spellId))
            if (holder->HasModifier(spellModifierPair.modId))
                holder->ResetSpellModCharges();
}

void Player::SetSpellClass(uint8 playerClass)
{
    SpellFamily name;
    switch (playerClass)
    {
        default: // compiler (编译器兜底返回值)
        case CLASS_WARRIOR: name = SPELLFAMILY_WARRIOR; break;
        case CLASS_PALADIN: name = SPELLFAMILY_PALADIN; break;
        case CLASS_HUNTER: name = SPELLFAMILY_HUNTER; break;
        case CLASS_ROGUE: name = SPELLFAMILY_ROGUE; break;
        case CLASS_PRIEST: name = SPELLFAMILY_PRIEST; break;
            // case CLASS_DEATH_KNIGHT: name = SPELLFAMILY_DEATH_KNIGHT; break; (死亡骑士分支保留，TBC 未启用)
        case CLASS_SHAMAN: name = SPELLFAMILY_SHAMAN; break;
        case CLASS_MAGE: name = SPELLFAMILY_MAGE; break;
        case CLASS_WARLOCK: name = SPELLFAMILY_WARLOCK; break;
        case CLASS_DRUID: name = SPELLFAMILY_DRUID; break;
    }
    m_spellClassName = name;
}

void Player::SendMessageToPlayer(std::string const& message) const
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, message.data(), LANG_UNIVERSAL, CHAT_TAG_NONE, GetObjectGuid());
    if (WorldSession* session = GetSession())
        session->SendPacket(data);
}

// send Proficiency (向客户端下发新获得的武器/护甲熟练度)
void Player::SendProficiency(ItemClass itemClass, uint32 itemSubclassMask) const
{
    WorldPacket data(SMSG_SET_PROFICIENCY, 1 + 4);
    data << uint8(itemClass) << uint32(itemSubclassMask);
    GetSession()->SendPacket(data);
}

void Player::RemovePetitionsAndSigns(ObjectGuid guid, uint32 type)
{
    uint32 lowguid = guid.GetCounter();

    std::unique_ptr<QueryResult> queryResult;
    if (type == 10)
        queryResult = CharacterDatabase.PQuery("SELECT ownerguid,petitionguid FROM petition_sign WHERE playerguid = '%u'", lowguid);
    else
        queryResult = CharacterDatabase.PQuery("SELECT ownerguid,petitionguid FROM petition_sign WHERE playerguid = '%u' AND type = '%u'", lowguid, type);

    if (queryResult)
    {
        do                                                  // this part effectively does nothing, since the deletion / modification only takes place _after_ the PetitionQuery. Though I don't know if the result remains intact if I execute the delete query beforehand. (清理角色待建公会/竞技场请愿书及其签名记录)
        {
            // and SendPetitionQueryOpcode reads data from the DB (需配合 SendPetitionQueryOpcode 读取最新状态)
            Field* fields = queryResult->Fetch();
            ObjectGuid ownerguid   = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            ObjectGuid petitionguid = ObjectGuid(HIGHGUID_ITEM, fields[1].GetUInt32());

            // send update if charter owner in game (若请愿书拥有者在线，向其发送更新通知)
            Player* owner = sObjectMgr.GetPlayer(ownerguid);
            if (owner)
                owner->GetSession()->SendPetitionQueryOpcode(petitionguid);
        }
        while (queryResult->NextRow());

        if (type == 10)
            CharacterDatabase.PExecute("DELETE FROM petition_sign WHERE playerguid = '%u'", lowguid);
        else
            CharacterDatabase.PExecute("DELETE FROM petition_sign WHERE playerguid = '%u' AND type = '%u'", lowguid, type);
    }

    CharacterDatabase.BeginTransaction();
    if (type == 10)
    {
        CharacterDatabase.PExecute("DELETE FROM petition WHERE ownerguid = '%u'", lowguid);
        CharacterDatabase.PExecute("DELETE FROM petition_sign WHERE ownerguid = '%u'", lowguid);
    }
    else
    {
        CharacterDatabase.PExecute("DELETE FROM petition WHERE ownerguid = '%u' AND type = '%u'", lowguid, type);
        CharacterDatabase.PExecute("DELETE FROM petition_sign WHERE ownerguid = '%u' AND type = '%u'", lowguid, type);
    }
    CharacterDatabase.CommitTransaction();
}

void Player::LeaveAllArenaTeams(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();
    auto queryResult = CharacterDatabase.PQuery("SELECT arena_team_member.arenateamid FROM arena_team_member JOIN arena_team ON arena_team_member.arenateamid = arena_team.arenateamid WHERE guid='%u'", lowguid);
    if (!queryResult)
        return;

    do
    {
        Field* fields = queryResult->Fetch();
        if (uint32 at_id = fields[0].GetUInt32())
            if (ArenaTeam* at = sObjectMgr.GetArenaTeamById(at_id))
                at->DelMember(guid);
    }
    while (queryResult->NextRow());
}

void Player::SetRestBonus(float rest_bonus_new)
{
    // Prevent resting on max level (满级角色不再累积双倍经验额度)
    if (GetLevel() >= GetMaxAttainableLevel())
        rest_bonus_new = 0;

    if (rest_bonus_new < 0)
        rest_bonus_new = 0;

    float rest_bonus_max = (float)GetUInt32Value(PLAYER_NEXT_LEVEL_XP) * 1.5f / 2.0f;

    if (rest_bonus_new > rest_bonus_max)
        m_rest_bonus = rest_bonus_max;
    else
        m_rest_bonus = rest_bonus_new;

    bool isRafResting = IsRafResting();

    // update data for client (更新客户端休息经验槽数值)
    if (m_rest_bonus > 10)
        SetByteValue(PLAYER_BYTES_2, 3, REST_STATE_RESTED | (isRafResting ? REST_STATE_RAF_LINKED : 0));
    else if (m_rest_bonus <= 1)
        SetByteValue(PLAYER_BYTES_2, 3, REST_STATE_NORMAL | (isRafResting ? REST_STATE_RAF_LINKED : 0));

    // RestTickUpdate (更新休息状态字节)
    SetUInt32Value(PLAYER_REST_STATE_EXPERIENCE, uint32(m_rest_bonus));
}

bool Player::IsRafResting() const
{
    return (GetByteValue(PLAYER_BYTES_2, 3) & REST_STATE_RAF_LINKED) != 0;
}

void Player::ResetMap()
{
    for (auto guid : m_clientGUIDs)
    {
        if (WorldObject* object = GetMap()->GetWorldObject(guid))
            object->RemoveClientIAmAt(this);
    }
    m_clientGUIDs.clear();
    Unit::ResetMap();
}

void Player::HandleStealthedUnitsDetection()
{
    UnitList stealthedUnits;

    MaNGOS::AnyStealthedCheck u_check(this);
    MaNGOS::UnitListSearcher<MaNGOS::AnyStealthedCheck > searcher(stealthedUnits, u_check);
    Cell::VisitAllObjects(this, searcher, MAX_PLAYER_STEALTH_DETECT_RANGE);

    WorldObject const* viewPoint = GetCamera().GetBody();

    UpdateData data;
    std::vector<Unit*> added;
    for (UnitList::const_iterator i = stealthedUnits.begin(); i != stealthedUnits.end(); ++i)
    {
        Unit* target = *i;
        if (target == this)
            continue;

        bool hasAtClient = HasAtClient(target);
        bool hasDetected = target->IsVisibleForOrDetect(this, viewPoint, true);

        if (hasDetected)
        {
            if (!hasAtClient)
            {
                GetMap()->AddCreateAtClientObject(this, target);
                AddAtClient((*i));
                ObjectGuid i_guid = (*i)->GetObjectGuid();
                DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "%s is detected in stealth by player %u. Distance = %f", i_guid.GetString().c_str(), GetGUIDLow(), GetDistance(*i));
            }
        }
        else
        {
            if (hasAtClient)
            {
                GetMap()->AddUpdateRemoveObject({ GetObjectGuid()}, target->GetObjectGuid());
                if (target->GetTypeId() == TYPEID_UNIT)
                    BeforeVisibilityDestroy(static_cast<Creature*>(target));
                RemoveAtClient(target);
            }
        }
    }
}

/// 激活飞行管理员提供的多节点飞行路线并起飞
bool Player::ActivateTaxiPathTo(std::vector<uint32> const& nodes, Creature* npc /*= nullptr*/, uint32 spellid /*= 0*/)
{
    if (nodes.size() < 2)
        return false;

    // not let cheating with start flight in time of logout process || if casting not finished || while in combat || if not use Spell's with EffectSendTaxi (防作弊校验：登出中、施法中、战斗中且非系统飞行法术时禁止起飞)
    if (GetSession()->isLogingOut() || IsInCombat())
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
        return false;
    }

    if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CLIENT_CONTROL_LOST))
        return false;

    // taximaster case (飞行管理员 NPC 发起的飞行校验)
    if (npc)
    {
        // not let cheating with start flight mounted (禁止在骑乘状态下直接起飞)
        if (GetMountID())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERALREADYMOUNTED);
            return false;
        }

        if (IsInDisallowedMountForm())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERSHAPESHIFTED);
            return false;
        }

        // not let cheating with start flight in time of logout process || if casting not finished || while in combat || if not use Spell's with EffectSendTaxi (防作弊校验：登出中、施法中或战斗中禁止起飞)
        if (IsNonMeleeSpellCasted(false))
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
            return false;
        }
    }
    // cast case or scripted call case (法术或脚本触发的单段飞行路径)
    else
    {
        RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

        if (IsInDisallowedMountForm())
            RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

        if (Spell* spell = GetCurrentSpell(CURRENT_GENERIC_SPELL))
            if (spell->m_spellInfo->Id != spellid)
                InterruptSpell(CURRENT_GENERIC_SPELL, false);

        InterruptSpell(CURRENT_AUTOREPEAT_SPELL, false);

        if (Spell* spell = GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            if (spell->m_spellInfo->Id != spellid)
                InterruptSpell(CURRENT_CHANNELED_SPELL, true);
    }

    uint32 sourcenode = nodes[0];

    // starting node too far away (cheat?) (起点距离飞行管理员过远，疑似作弊)
    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(sourcenode);
    if (!node)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOSUCHPATH);
        return false;
    }

    // check node starting pos data set case if provided (若节点提供坐标则校验玩家与起点距离)
    if (node->x != 0.0f || node->y != 0.0f || node->z != 0.0f)
    {
        if (node->map_id != GetMapId() ||
                (node->x - GetPositionX()) * (node->x - GetPositionX()) +
                (node->y - GetPositionY()) * (node->y - GetPositionY()) +
                (node->z - GetPositionZ()) * (node->z - GetPositionZ()) >
                (2 * INTERACTION_DISTANCE) * (2 * INTERACTION_DISTANCE) * (2 * INTERACTION_DISTANCE))
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXITOOFARAWAY);
            return false;
        }
    }
    // node must have pos if taxi master case (npc != nullptr) (通过 NPC 起飞时起点必须具备有效坐标)
    else if (npc)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);
        return false;
    }

    // Prepare to flight start now (准备正式进入飞行状态)

    // temporary remove pet (起飞前临时解散当前宠物)
    UnsummonPetTemporaryIfAny();

    // stop combat at start taxi flight if any (起飞时强制脱离当前战斗)
    CombatStop();

    // stop trade (client cancel trade at taxi map open but cheating tools can be used for reopen it) (强制取消可能残留的交易状态)
    TradeCancel(true);

    if (!m_taxiTracker.AddRoutes(nodes, (npc ? GetRepairPriceMultiplier(npc) : 0.0f), !spellid))
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);
        return false;
    }

    if (GetMoney() < m_taxiTracker.GetCostTotal())
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOTENOUGHMONEY);
        m_taxiTracker.Clear();
        return false;
    }

    if (!m_taxiTracker.Prepare())
        return false;

    // prevent stealth flight (移除潜行/隐形等移动限制状态以防飞行异常)
    // RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TALK); (注释：无需按对话打断标志移除光环)

    GetSession()->SendActivateTaxiReply(ERR_TAXIOK);

    GetMotionMaster()->MoveTaxi();

    return true;
}

bool Player::ActivateTaxiPathTo(uint32 path_id, uint32 spellid /*= 0*/)
{
    TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(path_id);
    if (!entry)
        return false;

    return ActivateTaxiPathTo({ entry->from, entry->to }, nullptr, spellid);
}

void Player::TaxiFlightResume(bool forceRenewMoveGen /*= false*/)
{
    if (m_taxiTracker.GetState() < Taxi::TRACKER_STANDBY)
        return;

    DEBUG_LOG("WORLD: Resuming taxi flight for character %u", GetGUIDLow());

    // Already in flight: just make sure client control is updated (已在飞行中：确保客户端控制权锁定状态正确)
    if (hasUnitState(UNIT_STAT_TAXI_FLIGHT))
    {
        UpdateClientControl(this, IsClientControlled(this));
        if (!forceRenewMoveGen)
            return;
    }

    GetMotionMaster()->MoveTaxi();
}

bool Player::TaxiFlightInterrupt(bool cancel /*= true*/)
{
    // Simply pauses if bool is not set (for example, storing the flight for one reason or another) (若未传取消标志则仅暂停飞行以便后续恢复)
    if (GetMotionMaster()->GetCurrentMovementGeneratorType() == TAXI_MOTION_TYPE)
    {
        OnTaxiFlightEject(cancel);
        GetMotionMaster()->MovementExpired();
        return true;
    }
    return false;
}

const Taxi::Map& Player::GetTaxiPathSpline() const
{
    // Bugcheck: container continuity error (一致性检查：飞行容器为空或节点不匹配)
    MANGOS_ASSERT(!m_taxiTracker.GetAtlas().empty());
    return m_taxiTracker.GetMap();
}

int32 Player::GetTaxiPathSplineOffset() const
{
    return int32(m_taxiTracker.GetResumeWaypointIndex());
}

void Player::OnTaxiFlightStart(const TaxiPathEntry* /*path*/)
{

}

void Player::OnTaxiFlightEnd(const TaxiPathEntry* path)
{
    // Final destination (已抵达最终飞行终点)
    if (const TaxiNodesEntry* destination = sTaxiNodesStore.LookupEntry(path->to))
        TeleportTo(GetMap()->GetId(), destination->x, destination->y, destination->z, GetOrientation());

    if (pvpInfo.inPvPEnforcedArea)
        CastSpell(this, 2479, TRIGGERED_OLD_TRIGGERED);

    /*
    NOTE: B clearly has some form of extra scripts on certain fly path ends. Nodes contain extra events, that can be done using dbscript_on_event,
    but these extra scripts have no EventID in the DBC. In future if this place fills up, need to consider moving it to a more generic way of scripting.
    注：部分特殊飞行路线终点无 DBC EventID，暂在此处硬编码触发后续脚本/法术。
    */
    if (path)
    {
        switch (path->to)
        {
            case 91:        // Susurrus (沙塔斯苏苏鲁斯对话飞行触发)
                RemoveAurasDueToSpell(32474);
                break;
            case 158:        // Vision Guide (沙塔斯幻象向导漫游触发)
                AreaExploredOrEventHappens(10525);
                RemoveAurasDueToSpell(36573);
                break;
            default:
                break;
        }
    }
}

void Player::OnTaxiFlightEject(bool clear /*= true*/)
{
    OnTaxiFlightSplineEnd();
    if (clear)
        m_taxiTracker.Clear(true);
}

bool Player::OnTaxiFlightUpdate(const size_t waypointIndex, const bool movement)
{
    switch (m_taxiTracker.GetState())
    {
        case Taxi::TRACKER_FLIGHT:
        {
            // Bugcheck: container continuity error (一致性检查：飞行容器不匹配)
            MANGOS_ASSERT(!m_taxiTracker.GetAtlas().empty());

            // Check for spline interference before updating the container (更新容器前检查是否有非飞行样条插值移动干扰)
            if (!movement)
            {
                float x, y, z;
                GetPosition(x, y, z);
                const TaxiPathNodeEntry* dest = m_taxiTracker.GetMap().back();
                if (int32(x * 100) != int32(dest->x * 100) || int32(y * 100) != int32(dest->y * 100) || int32(z * 100) != int32(dest->z * 100))
                    return false;
            }

            // Due to nature of update ticks, we have to check if we have queued up several nodes inbetween updates (因帧间隔可能一次跨越多个途经航点，需逐个推进)
            const size_t current = m_taxiTracker.GetCurrentWaypointIndex();
            const bool start = (!waypointIndex && !m_taxiTracker.GetCurrentWaypoint());
            if (waypointIndex > current || start)
            {
                Taxi::Map map = m_taxiTracker.GetMap();
                for (size_t next = (start ? current : current + 1); next <= waypointIndex; ++next)
                {
                    const TaxiPathNodeEntry* entry = map.at(next);
                    const bool last = (entry == map.back());
                    Taxi::PathID startID = 0;
                    Taxi::PathID endID = 0;
                    // Notify taxi container, check if we advanced to next route (推进航点容器并检查是否进入下一航段)
                    if (m_taxiTracker.UpdateRoute(entry, startID, endID))
                    {
                        if (endID)
                            OnTaxiFlightRouteEnd(endID, last);
                        if (startID)
                            OnTaxiFlightRouteStart(startID, (entry == map.front()));
                    }
                    OnTaxiFlightRouteProgress(entry, (last ? nullptr : map.at(next + 1)));
                }
            }
            return true;
        }
        case Taxi::TRACKER_STANDBY:
        case Taxi::TRACKER_TRANSFER:
            return true;
        default:
            break;
    }
    return false;
}

void Player::OnTaxiFlightSplineStart(const TaxiPathNodeEntry* node)
{
    if (m_taxiTracker.GetState() == Taxi::TRACKER_TRANSFER)
        UpdateClientControl(this, false);

    if (sTaxiPathStore.LookupEntry(node->path))
        Mount(m_taxiTracker.GetMountDisplayId());

    getHostileRefManager().updateOnlineOfflineState(false);

    // Bugcheck: container continuity error (一致性检查：当前路线段无效)
    MANGOS_ASSERT(m_taxiTracker.SetState(Taxi::TRACKER_FLIGHT));
}

void Player::OnTaxiFlightSplineEnd()
{
    Unmount();

    getHostileRefManager().updateOnlineOfflineState(true);

    // Note: only gets set by itself when container does not end up empty (仅当容器尚未清空时允许开启新样条段)
    m_taxiTracker.SetState(Taxi::TRACKER_TRANSFER);
}

bool Player::OnTaxiFlightSplineUpdate()
{
    switch (m_taxiTracker.GetState())
    {
        case Taxi::TRACKER_FLIGHT:
        case Taxi::TRACKER_STANDBY:
        case Taxi::TRACKER_TRANSFER:
        {
            // Bugcheck: container continuity error (一致性检查：当前无活动航段)
            MANGOS_ASSERT(!m_taxiTracker.GetAtlas().empty());

            const Taxi::Map& map = m_taxiTracker.GetMap();

            // Bugcheck: spline is too short for a taxi (异常检查：飞行路径样条点数过少)
            MANGOS_ASSERT(map.size() > 1);

            uint32 mapid = GetMapId();

            size_t nodeResume = m_taxiTracker.GetResumeWaypointIndex();
            const TaxiPathNodeEntry* first = (nodeResume ? map.at(nodeResume) : map.front());

            // Check if we are starting on the same game level with the spline (校验玩家是否与飞行样条同处于正常起点阶段)
            if (first->mapid == mapid)
            {
                OnTaxiFlightSplineStart(first);
                return true;
            }
            if (m_taxiTracker.GetState() == Taxi::TRACKER_TRANSFER)
            {
                OnTaxiFlightSplineEnd();
                TeleportTo(first->mapid, first->x, first->y, first->z, GetOrientation());
            }
            break;
        }
        default:
            // All done, simply finalize (整条飞行路径已结束，执行落地收尾)
            OnTaxiFlightSplineEnd();
            break;
    }
    return false;
}

void Player::OnTaxiFlightRouteStart(uint32 pathID, bool initial)
{
    if (IsTaxiDebug())
        ChatHandler(this).PSendSysMessage(LANG_TAXI_DEBUG_PATH, pathID);

    if (initial)
    {
        ModifyMoney(-int32(m_taxiTracker.GetCost()));

        if (const TaxiPathEntry* path = sTaxiPathStore.LookupEntry(pathID))
            OnTaxiFlightStart(path);
    }
}

void Player::OnTaxiFlightRouteEnd(uint32 pathID, bool final)
{
    if (final)
    {
        if (const TaxiPathEntry* path = sTaxiPathStore.LookupEntry(pathID))
            OnTaxiFlightEnd(path);
    }
    else
        ModifyMoney(-int32(m_taxiTracker.GetCost()));
}

void Player::OnTaxiFlightRouteProgress(const TaxiPathNodeEntry* node, const TaxiPathNodeEntry* next /*= nullptr*/)
{
    if (!node)
        return;

    if (IsTaxiDebug())
    {
        if (next)
            ChatHandler(this).PSendSysMessage(LANG_TAXI_DEBUG_NODE, node->path, node->index, next->path, next->index);
        else
            ChatHandler(this).PSendSysMessage(LANG_TAXI_DEBUG_NODE_FINAL, node->path, node->index);
    }

    // Needs to be confirmed: first one shouldn't fire arrival event, final one shouldnt fire departure event? (首节点不触发到达事件，末节点不触发出发事件)
    for (int arrival = 1; arrival > -int(next != nullptr); --arrival)
    {
        if (uint32 eventid = (arrival ? node->arrivalEventID : node->departureEventID))
        {
            const char* desc = (arrival ? "arrival" : "departure");
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Taxi %s event %u of node %u of path %u for player %s", desc, eventid, node->index, node->path, GetName());
            StartEvents_Event(GetMap(), eventid, this, this, !arrival);
        }
    }
}

void Player::InitDataForForm(bool reapplyMods)
{
    ShapeshiftForm form = GetShapeshiftForm();

    SpellShapeshiftFormEntry const* ssEntry = sSpellShapeshiftFormStore.LookupEntry(form);
    if (ssEntry && ssEntry->attackSpeed)
    {
        SetAttackTime(BASE_ATTACK, ssEntry->attackSpeed);
        SetAttackTime(OFF_ATTACK, ssEntry->attackSpeed);
        SetAttackTime(RANGED_ATTACK, BASE_ATTACK_TIME);
    }
    else
        SetRegularAttackTime();

    switch (form)
    {
        case FORM_CAT:
        {
            if (GetPowerType() != POWER_ENERGY)
                SetPowerType(POWER_ENERGY);
            break;
        }
        case FORM_BEAR:
        case FORM_DIREBEAR:
        {
            if (GetPowerType() != POWER_RAGE)
                SetPowerType(POWER_RAGE);
            break;
        }
        default:                                            // 0, for example (如无特殊形态或基础形态)
        {
            ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(getClass());
            if (cEntry && cEntry->powerType < MAX_POWERS && uint32(GetPowerType()) != cEntry->powerType)
                SetPowerType(Powers(cEntry->powerType));
            break;
        }
    }

    // update auras at form change, ignore this at mods reapply (.reset stats/etc) when form not change. (形态真正改变时刷新装备与光环，纯重算属性时跳过)
    if (!reapplyMods)
        UpdateEquipSpellsAtFormChange();

    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);
}

void Player::InitDisplayIds()
{
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        sLog.outError("Player %u has incorrect race/class pair. Can't init display ids.", GetGUIDLow());
        return;
    }

    // reset scale before reapply auras (重新应用光环前先重置模型缩放比例)
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    uint8 gender = getGender();
    switch (gender)
    {
        case GENDER_FEMALE:
            SetDisplayId(info->displayId_f);
            SetNativeDisplayId(info->displayId_f);
            break;
        case GENDER_MALE:
            SetDisplayId(info->displayId_m);
            SetNativeDisplayId(info->displayId_m);
            break;
        default:
            sLog.outError("Invalid gender %u for player", gender);
    }
}

void Player::TakeExtendedCost(uint32 extendedCostId, uint32 count)
{
    ItemExtendedCostEntry const* extendedCost = sItemExtendedCostStore.LookupEntry(extendedCostId);

    if (extendedCost->reqhonorpoints)
        ModifyHonorPoints(-int32(extendedCost->reqhonorpoints * count));
    if (extendedCost->reqarenapoints)
        ModifyArenaPoints(-int32(extendedCost->reqarenapoints * count));

    for (uint8 i = 0; i < MAX_EXTENDED_COST_ITEMS; ++i)
    {
        if (extendedCost->reqitem[i])
            DestroyItemCount(extendedCost->reqitem[i], extendedCost->reqitemcount[i] * count, true);
    }
}

bool Player::CanSeeVendorItem(Creature const* vendor, VendorItem const* vendorItem, ItemPrototype const* proto) const
{
    if (IsGameMaster())
        return true;

    // class wrong item skip only for bindable case (职业不符的物品仅在可绑定限制生效时隐藏)
    if ((proto->AllowableClass & getClassMask()) == 0 && proto->Bonding == BIND_WHEN_PICKED_UP)
        return false;

    // race wrong item skip always (种族不符的商人商品始终隐藏)
    if ((proto->AllowableRace & getRaceMask()) == 0)
        return false;

    if (vendorItem->conditionId && !sObjectMgr.IsConditionSatisfied(vendorItem->conditionId, this, vendor->GetMap(), vendor, CONDITION_FROM_VENDOR))
        return false;

    return true;
}

// Return true is the bought item has a max count to force refresh of window by caller (若购买限量物品则返回 true 以便调用方刷新商人列表)
/// 从商人处购买指定物品（校验库存、荣誉/竞技场点、声望折扣与扩展花费）
bool Player::BuyItemFromVendor(ObjectGuid vendorGuid, uint32 item, uint8 count, uint8 bag, uint8 slot)
{
    // cheating attempt (校验购买数量与背包目标槽位，防止作弊)
    if (count < 1) count = 1;

    if (!IsAlive())
        return false;

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (!pProto)
    {
        SendBuyError(BuyResult::CANT_FIND_ITEM, nullptr, 0);
        return false;
    }

    Creature* pCreature = GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: BuyItemFromVendor - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        SendBuyError(BuyResult::DISTANCE_TOO_FAR, nullptr, 0);
        return false;
    }

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();
    if ((!vItems || vItems->Empty()) && (!tItems || tItems->Empty()))
    {
        SendBuyError(BuyResult::CANT_FIND_ITEM, pCreature, 0);
        return false;
    }

    uint32 vCount = vItems ? vItems->GetItemCount() : 0;
    uint32 tCount = tItems ? tItems->GetItemCount() : 0;

    size_t vendorslot = vItems ? vItems->FindItemSlot(item) : vCount;
    if (vendorslot >= vCount)
        vendorslot = vCount + (tItems ? tItems->FindItemSlot(item) : tCount);

    if (vendorslot >= vCount + tCount)
    {
        SendBuyError(BuyResult::CANT_FIND_ITEM, pCreature, 0);
        return false;
    }

    VendorItem const* crItem = vendorslot < vCount ? vItems->GetItem(vendorslot) : tItems->GetItem(vendorslot - vCount);
    if (!crItem || crItem->item != item)                    // store diff item (cheating) (试图将不同物品堆叠到已有物品上，防作弊)
    {
        SendBuyError(BuyResult::CANT_FIND_ITEM, pCreature, vendorslot);
        return false;
    }

    // hidden from this player in the vendor list (cheating) (该玩家不可见此商品，防作弊)
    if (!CanSeeVendorItem(pCreature, crItem, pProto))
    {
        SendBuyError(BuyResult::CANT_FIND_ITEM, pCreature, vendorslot);
        return false;
    }

    uint32 totalCount = pProto->BuyCount * count;

    // check current item amount if it limited (检查商人限量物品的当前库存)
    if (crItem->maxcount != 0)
    {
        if (pCreature->GetVendorItemCurrentCount(crItem) < totalCount)
        {
            SendBuyError(BuyResult::ITEM_ALREADY_SOLD, pCreature, vendorslot);
            return false;
        }
    }

    if (uint32(GetReputationRank(pProto->RequiredReputationFaction)) < pProto->RequiredReputationRank)
    {
        SendBuyError(BuyResult::REPUTATION_REQUIRE, pCreature, vendorslot);
        return false;
    }

    if (uint32 extendedCostId = crItem->ExtendedCost)
    {
        ItemExtendedCostEntry const* iece = sItemExtendedCostStore.LookupEntry(extendedCostId);
        if (!iece)
        {
            sLog.outError("Item %u have wrong ExtendedCost field value %u", pProto->ItemId, extendedCostId);
            return false;
        }

        // honor points price (校验扩展花费中的荣誉点消耗)
        if (GetHonorPoints() < (iece->reqhonorpoints * count))
        {
            SendEquipError(EQUIP_ERR_NOT_ENOUGH_HONOR_POINTS, nullptr, nullptr);
            return false;
        }

        // arena points price (校验扩展花费中的竞技场点数消耗)
        if (GetArenaPoints() < (iece->reqarenapoints * count))
        {
            SendEquipError(EQUIP_ERR_NOT_ENOUGH_ARENA_POINTS, nullptr, nullptr);
            return false;
        }

        // item base price (计算基础金币价格与购买数量校验)
        for (uint8 i = 0; i < MAX_EXTENDED_COST_ITEMS; ++i)
        {
            if (iece->reqitem[i] && !HasItemCount(iece->reqitem[i], iece->reqitemcount[i] * count))
            {
                SendEquipError(EQUIP_ERR_VENDOR_MISSING_TURNINS, nullptr, nullptr);
                return false;
            }
        }

        // check for personal arena rating requirement (检查竞技场物品所需的个人/战队等级要求)
        if (GetMaxPersonalArenaRatingRequirement() < iece->reqpersonalarenarating)
        {
            SendEquipError(EQUIP_ERR_PERSONAL_ARENA_RATING_TOO_LOW, nullptr, nullptr);
            return false;
        }
    }

    uint32 price = pProto->BuyPrice * count;

    // reputation discount (应用阵营声望折扣倍率)
    price = uint32(floor(price * GetReputationPriceDiscount(pCreature)));

    if (GetMoney() < price)
    {
        SendBuyError(BuyResult::NOT_ENOUGHT_MONEY, pCreature, vendorslot);
        return false;
    }

    Item* pItem;

    if ((bag == NULL_BAG && slot == NULL_SLOT) || IsInventoryPos(bag, slot))
    {
        ItemPosCountVec dest;
        InventoryResult msg = CanStoreNewItem(bag, slot, dest, item, totalCount);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, nullptr, nullptr, 0, item);
            return false;
        }

        ModifyMoney(-int32(price));

        if (crItem->ExtendedCost)
            TakeExtendedCost(crItem->ExtendedCost, count);

        pItem = StoreNewItem(dest, item, true);
    }
    else if (IsEquipmentPos(bag, slot))
    {
        if (totalCount != 1)
        {
            SendEquipError(EQUIP_ERR_ITEM_CANT_BE_EQUIPPED, nullptr, nullptr);
            return false;
        }

        uint16 dest;
        InventoryResult msg = CanEquipNewItem(slot, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, nullptr, nullptr, 0, item);
            return false;
        }

        ModifyMoney(-int32(price));

        if (crItem->ExtendedCost)
            TakeExtendedCost(crItem->ExtendedCost, count);

        pItem = EquipNewItem(dest, item, true);

        if (pItem)
            AutoUnequipOffhandIfNeed();
    }
    else
    {
        SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, nullptr, nullptr);
        return false;
    }

    if (!pItem)
        return false;

    uint32 new_count = pCreature->UpdateVendorItemCurrentCount(crItem, totalCount);

    WorldPacket data(SMSG_BUY_ITEM, 8 + 4 + 4 + 4);
    data << pCreature->GetObjectGuid();
    data << uint32(vendorslot + 1);                 // numbered from 1 at client (客户端商人槽位从 1 开始计数)
    data << uint32(crItem->maxcount > 0 ? new_count : 0xFFFFFFFF);
    data << uint32(totalCount);
    GetSession()->SendPacket(data);

    SendNewItem(pItem, totalCount, true, false);

    return crItem->maxcount != 0;
}

uint32 Player::GetMaxPersonalArenaRatingRequirement()
{
    // returns the maximal personal arena rating that can be used to purchase items requiring this condition (返回可用于购买竞技场装备的最高有效个人等级：)
    // the personal rating of the arena team must match the required limit as well (个人等级与战队等级均须达标，)
    // so return max[in arenateams](min(personalrating[teamtype], teamrating[teamtype])) (故取各战队中 min(个人等级, 战队等级) 的最大值)
    uint32 max_personal_rating = 0;
    for (int i = 0; i < MAX_ARENA_SLOT; ++i)
    {
        if (ArenaTeam* at = sObjectMgr.GetArenaTeamById(GetArenaTeamId(i)))
        {
            uint32 p_rating = GetArenaPersonalRating(i);
            uint32 t_rating = at->GetRating();
            p_rating = p_rating < t_rating ? p_rating : t_rating;
            if (max_personal_rating < p_rating)
                max_personal_rating = p_rating;
        }
    }
    return max_personal_rating;
}

uint8 Player::GetHighestPvPRankIndex() const
{
    uint8 index = 0;
    for (uint8 rank = 1; rank <= 28; ++rank)
    {
        if (HasTitle(rank))
            // Old rank index starts at 5, values below 5 are discontinued negative ranks (旧版军衔索引从 5 起算，低于 5 为已废弃的负声望阶位)
            index = (rank <= 14) ? (rank + 4) : (rank - 10);
    }

    return index;
}

void Player::UpdateHomebindTime(uint32 time)
{
    // GMs never get homebind timer online (GM 玩家在无效副本中不触发炉石倒计时)
    if (m_InstanceValid || IsGameMaster())
    {
        if (m_HomebindTimer)                                // instance valid, but timer not reset (当前仍在等待重置，暂不清除计时)
        {
            // hide reminder (隐藏炉石传送倒计时提示)
            WorldPacket data(SMSG_RAID_GROUP_ONLY, 4 + 4);
            data << uint32(0);
            data << uint32(ERR_RAID_GROUP_NONE);            // error used only when timer = 0 (timer=0 时仅作占位)
            GetSession()->SendPacket(data);
        }
        // instance is valid, reset homebind timer (副本状态有效，重置炉石遣返计时器)
        m_HomebindTimer = 0;
    }
    else if (m_HomebindTimer > 0)
    {
        if (time >= m_HomebindTimer)
        {
            // teleport to homebind location (倒计时结束，将玩家传回炉石绑定点)
            TeleportTo(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, GetOrientation());
        }
        else
            m_HomebindTimer -= time;
    }
    else
    {
        // instance is invalid, start homebind timer (副本已失效或未在队伍中，启动 60 秒遣返倒计时)
        m_HomebindTimer = 60000;
        // send message to player (向客户端发送即将传回墓地/炉石点提示)
        WorldPacket data(SMSG_RAID_GROUP_ONLY, 4 + 4);
        data << uint32(m_HomebindTimer);
        data << uint32(ERR_RAID_GROUP_NONE);                // error used only when timer = 0 (无特殊错误码)
        GetSession()->SendPacket(data);
        DEBUG_LOG("PLAYER: Player '%s' (GUID: %u) will be teleported to homebind in 60 seconds", GetName(), GetGUIDLow());
    }
}

void Player::UpdatePvP(bool state, bool overriding)
{
    if (!state || overriding)
    {
        // Updating into unset state or overriding anything (关闭 PvP 状态或强制覆盖时停止立即刷新)
        if (!pvpInfo.timerPvPRemaining || overriding)
        {
            if (IsPvP() != state)
            {
                SetPvP(state);
                pvpInfo.timerPvPRemaining = 0;
            }
        }
    }
    else
    {
        // Updating into set state (开启 PvP 状态时设置标志)
        if (!IsPvP())
            SetPvP(state);

        // Refresh timer (刷新 PvP 5 分钟衰减计时器)
        pvpInfo.timerPvPRemaining = 300000;
    }
}

void Player::UpdatePvPContested(bool state, bool overriding)
{
    if (!state || overriding)
    {
        // Updating into unset state or overriding anything (关闭争夺区域 PvP 状态)
        if (!pvpInfo.timerPvPContestedRemaining || overriding)
        {
            if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP) != state)
            {
                SetPvPContested(state);
                pvpInfo.timerPvPContestedRemaining = 0;
            }
        }
    }
    else
    {
        // Updating into set state (开启争夺区域 PvP 状态)
        if (!HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP))
        {
            SetPvPContested(state);

            // Legacy way of calling MoveInLineOfSight for nearby contested guards (触发周围争夺区守卫视野检查，)
            // TODO: Find a better way to do this, needs marking for a delayed reaction update (待优化：改为标记延迟反应更新)
            UpdateVisibilityAndView();
        }

        // Refresh timer (刷新争夺区域 PvP 计时器)
        pvpInfo.timerPvPContestedRemaining = 30000;
    }
}

/// 检查多彩宝石或其他条件附魔的当前宝石颜色数量要求是否满足
bool Player::EnchantmentFitsRequirements(uint32 enchantmentcondition, int8 slot) const
{
    if (!enchantmentcondition)
        return true;

    SpellItemEnchantmentConditionEntry const* Condition = sSpellItemEnchantmentConditionStore.LookupEntry(enchantmentcondition);

    if (!Condition)
        return true;

    uint8 curcount[4] = {0, 0, 0, 0};

    // counting current equipped gem colors (统计当前已装备物品上的各色宝石数量)
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (i == slot)
            continue;
        Item* pItem2 = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem2 && !pItem2->IsBroken() && pItem2->GetProto()->Socket[0].Color)
        {
            for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + 3; ++enchant_slot)
            {
                uint32 enchant_id = pItem2->GetEnchantmentId(EnchantmentSlot(enchant_slot));
                if (!enchant_id)
                    continue;

                SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
                if (!enchantEntry)
                    continue;

                uint32 gemid = enchantEntry->GemID;
                if (!gemid)
                    continue;

                ItemPrototype const* gemProto = sItemStorage.LookupEntry<ItemPrototype>(gemid);
                if (!gemProto)
                    continue;

                GemPropertiesEntry const* gemProperty = sGemPropertiesStore.LookupEntry(gemProto->GemProperties);
                if (!gemProperty)
                    continue;

                uint8 GemColor = gemProperty->color;

                for (uint8 b = 0, tmpcolormask = 1; b < 4; ++b, tmpcolormask <<= 1)
                {
                    if (tmpcolormask & GemColor)
                        ++curcount[b];
                }
            }
        }
    }

    bool activate = true;

    for (int i = 0; i < 5; ++i)
    {
        if (!Condition->Color[i])
            continue;

        uint32 _cur_gem = curcount[Condition->Color[i] - 1];

        // if have <CompareColor> use them as count, else use <value> from Condition (若配置了对比颜色则使用其宝石数量作为阈值，否则使用条件常量值)
        uint32 _cmp_gem = Condition->CompareColor[i] ? curcount[Condition->CompareColor[i] - 1] : Condition->Value[i];

        switch (Condition->Comparator[i])
        {
            case 2:                                         // requires less <color> than (<value> || <comparecolor>) gems (要求某色宝石数量少于指定值或对比色数量)
                activate &= _cur_gem < _cmp_gem;
                break;
            case 3:                                         // requires more <color> than (<value> || <comparecolor>) gems (要求某色宝石数量多于指定值或对比色数量)
                activate &= _cur_gem > _cmp_gem;
                break;
            case 5:                                         // requires at least <color> than (<value> || <comparecolor>) gems (要求某色宝石数量不少于指定值)
                activate &= _cur_gem >= _cmp_gem;
                break;
        }
    }

    DEBUG_LOG("Checking Condition %u, there are %u Meta Gems, %u Red Gems, %u Yellow Gems and %u Blue Gems, Activate:%s", enchantmentcondition, curcount[0], curcount[1], curcount[2], curcount[3], activate ? "yes" : "no");

    return activate;
}

void Player::CorrectMetaGemEnchants(uint8 exceptslot, bool apply)
{
    // cycle all equipped items (遍历全部已装备槽位以校验多彩宝石激活状态)
    for (uint32 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        // enchants for the slot being socketed are handled by Player::ApplyItemMods (正在镶嵌的目标装备本身由 ApplyItemMods 处理)
        if (slot == exceptslot)
            continue;

        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, slot);

        if (!pItem || !pItem->GetProto()->Socket[0].Color)
            continue;

        for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + 3; ++enchant_slot)
        {
            uint32 enchant_id = pItem->GetEnchantmentId(EnchantmentSlot(enchant_slot));
            if (!enchant_id)
                continue;

            SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
            if (!enchantEntry)
                continue;

            uint32 condition = enchantEntry->EnchantmentCondition;
            if (condition)
            {
                // was enchant active with/without item? (判断该附魔在当前状态下是否已激活)
                bool wasactive = EnchantmentFitsRequirements(condition, apply ? exceptslot : -1);
                // should it now be? (判断变更后该附魔是否应激活)
                if (wasactive != EnchantmentFitsRequirements(condition, apply ? -1 : exceptslot))
                {
                    // ignore item gem conditions (在无视宝石条件模式或状态变化时应用/移除)
                    // if state changed, (dis)apply enchant (若激活状态变化则应用或移除多彩宝石属性)
                    ApplyEnchantment(pItem, EnchantmentSlot(enchant_slot), !wasactive, true, true);
                }
            }
        }
    }
}

// if false -> then toggled off if was on| if true -> toggled on if was off AND meets requirements (传入 false 时强制关闭已激活多彩；传入 true 时若满足条件则开启)
void Player::ToggleMetaGemsActive(uint8 exceptslot, bool apply)
{
    // cycle all equipped items (遍历所有已装备部位)
    for (int slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        // enchants for the slot being socketed are handled by WorldSession::HandleSocketOpcode(WorldPacket& recv_data) (排除正在操作的插槽自身)
        if (slot == exceptslot)
            continue;

        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, slot);

        if (!pItem || !pItem->GetProto()->Socket[0].Color)  // if item has no sockets or no item is equipped go to next item (无物品或无宝石插槽则跳过)
            continue;

        // cycle all (gem)enchants (遍历该装备的所有宝石附魔槽位)
        for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + 3; ++enchant_slot)
        {
            uint32 enchant_id = pItem->GetEnchantmentId(EnchantmentSlot(enchant_slot));
            if (!enchant_id)                                // if no enchant go to next enchant(slot) (该槽位无附魔则跳过)
                continue;

            SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
            if (!enchantEntry)
                continue;

            // only metagems to be (de)activated, so only enchants with condition (仅切换带激活条件的多彩宝石附魔)
            uint32 condition = enchantEntry->EnchantmentCondition;
            if (condition)
                ApplyEnchantment(pItem, EnchantmentSlot(enchant_slot), apply);
        }
    }
}

void Player::SetBattleGroundEntryPoint(Player* leader /*= nullptr*/)
{
    // chat command use case, or non-group join (GM 命令入场或非组队进入场景)
    if (!leader || !leader->IsInWorld() || leader->IsTaxiFlying() || leader->GetMap()->IsDungeon() || leader->GetMap()->IsBattleGroundOrArena())
        leader = this;

    if (leader->IsInWorld() && !leader->IsTaxiFlying())
    {
        // If map is dungeon find linked graveyard (若原位置在副本中，则查找关联的入口墓地作为返回点)
        if (leader->GetMap()->IsDungeon())
        {
            if (const WorldSafeLocsEntry* entry = leader->GetMap()->GetGraveyardManager().GetClosestGraveYard(leader->GetPositionX(), leader->GetPositionY(), leader->GetPositionZ(), leader->GetMapId(), leader->GetTeam()))
            {
                m_bgData.joinPos = WorldLocation(entry->map_id, entry->x, entry->y, entry->z, entry->o);
                m_bgData.m_needSave = true;
                return;
            }
            else
                sLog.outError("SetBattleGroundEntryPoint: Dungeon map %u has no linked graveyard, setting home location as entry point.", leader->GetMapId());
        }
        // If new entry point is not BG or arena set it (仅当源位置不是战场或竞技场时才记录为返回点)
        else if (!leader->GetMap()->IsBattleGroundOrArena())
        {
            m_bgData.joinPos = WorldLocation(leader->GetMapId(), leader->GetPositionX(), leader->GetPositionY(), leader->GetPositionZ(), leader->GetOrientation());
            m_bgData.m_needSave = true;
            return;
        }
    }

    // In error cases use homebind position (异常回退至炉石绑定点)
    m_bgData.joinPos = WorldLocation(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, 0.0f);
    m_bgData.m_needSave = true;
}

void Player::LeaveBattleground(bool teleportToEntryPoint)
{
    if (BattleGround* bg = GetBattleGround())
    {
        bg->RemovePlayerAtLeave(GetObjectGuid(), teleportToEntryPoint, true);

        // call after remove to be sure that player resurrected for correct cast (先移出战场团队并确保复活后再施法)
        if (bg->IsBattleGround() && !IsGameMaster() && sWorld.getConfig(CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER))
        {
            if (bg->GetStatus() == STATUS_IN_PROGRESS || bg->GetStatus() == STATUS_WAIT_JOIN)
            {
                // lets check if player was teleported from BG and schedule delayed Deserter spell cast (若玩家提前离开战场则安排延迟施加逃亡者减益)
                if (IsBeingTeleportedFar())
                {
                    ScheduleDelayedOperation(DELAYED_SPELL_CAST_DESERTER);
                    return;
                }

                CastSpell(this, SPELL_ID_BATTLEGROUND_DESERTER, TRIGGERED_OLD_TRIGGERED);               // Deserter (逃亡者法术 ID)
            }
        }
    }
}

bool Player::CanJoinToBattleground() const
{
    // check Deserter debuff (检查玩家是否带有逃亡者减益)
    return GetDummyAura(SPELL_ID_BATTLEGROUND_DESERTER) == nullptr;
}

bool Player::CanReportAfkDueToLimit()
{
    // a player can complain about 15 people per 5 minutes (每 5 分钟最多举报 15 名挂机玩家)
    return m_bgData.bgAfkReportedCount++ < 15;
}

/// This player has been blamed to be inactive in a battleground (记录该玩家被队友举报战场挂机)
void Player::ReportedAfkBy(Player* reporter)
{
    BattleGround* bg = GetBattleGround();
    if (!bg || bg != reporter->GetBattleGround() || GetTeam() != reporter->GetTeam())
        return;

    // check if player has 'Idle' or 'Inactive' debuff (若已有空闲或非活跃减益则跳过)
    if (m_bgData.bgAfkReporter.find(reporter->GetGUIDLow()) == m_bgData.bgAfkReporter.end() && !HasAura(43680, EFFECT_INDEX_0) && !HasAura(43681, EFFECT_INDEX_0) && reporter->CanReportAfkDueToLimit())
    {
        m_bgData.bgAfkReporter.insert(reporter->GetGUIDLow());
        // 3 players have to complain to apply debuff (满 3 名不同队友举报即触发挂机减益)
        if (m_bgData.bgAfkReporter.size() >= 3)
        {
            // cast 'Idle' spell (施加战场“空闲”法术)
            CastSpell(this, 43680, TRIGGERED_OLD_TRIGGERED);
            m_bgData.bgAfkReporter.clear();
        }
    }
}

void Player::AddAtClient(WorldObject* target)
{
    m_clientGUIDs.insert(target->GetObjectGuid());
    target->AddClientIAmAt(this);
}

void Player::RemoveAtClient(WorldObject* target, bool skipRemovalOfAt)
{
    m_clientGUIDs.erase(target->GetObjectGuid());
    if (!skipRemovalOfAt)
        target->RemoveClientIAmAt(this);
}

void Player::DestroyAtClient(WorldObject* target, bool skipRemovalOfAt)
{
    target->DestroyForPlayer(this);

    RemoveAtClient(target, skipRemovalOfAt);
}

bool Player::IsVisibleInGridForPlayer(Player* pl) const
{
    // gamemaster in GM mode see all, including ghosts (开启 GM 模式的管理员可见所有玩家，含幽灵)
    if (pl->IsGameMaster() && GetSession()->GetSecurity() <= pl->GetSession()->GetSecurity())
        return true;

    // player see dead player/ghost from own group/raid (同小队或团队成员始终可见彼此的灵魂)
    if (IsInGroup(pl))
        return true;

    // Live player see live player or dead player with not realized corpse (存活玩家可见存活玩家及尚未释放灵魂的尸体)
    if (pl->IsAlive() || pl->m_deathTimer > 0)
        return IsAlive() || m_deathTimer > 0;

    // Ghost see other friendly ghosts, that's for sure (幽灵互相可见同阵营幽灵)
    if (!(IsAlive() || m_deathTimer > 0) && CanCooperate(pl))
        return true;

    // Dead player see live players near own corpse (灵魂靠近自身尸体或可见范围内的存活玩家时可见)
    if (IsAlive())
    {
        if (Corpse* corpse = pl->GetCorpse())
        {
            // 20 - aggro distance for same level, 25 - max additional distance if player level less that creature level (仅可看见自身尸体附近一定范围内的单位)
            if (corpse->IsWithinDistInMap(this, (20 + 25) * sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO)))
                return true;
        }
    }

    // and not see any other (其余情况不可见)
    return false;
}

bool Player::IsVisibleGloballyFor(Player* u) const
{
    if (!u)
        return false;

    // Always can see self (全局列表始终可见自己)
    if (u == this)
        return true;

    // Visible units, always are visible for all players (处于可见状态的玩家对全局列表可见)
    if (GetVisibility() == VISIBILITY_ON)
        return true;

    // GMs are visible for higher gms (or players are visible for gms) (低权限 GM 对高权限 GM 可见)
    if (u->GetSession()->GetSecurity() > SEC_PLAYER)
        return GetSession()->GetSecurity() <= u->GetSession()->GetSecurity();

    // non faction visibility non-breakable for non-GMs (普通玩家非同阵营不可见，无跨阵营全局透视)
    if (GetVisibility() == VISIBILITY_OFF)
        return false;

    // non-gm stealth/invisibility not hide from global player lists (普通潜行/隐形不从全局在线列表中隐藏)
    return true;
}

void Player::BeforeVisibilityDestroy(Creature* creature)
{
    if (creature->IsInCombat() && IsInCombat())
    {
        if (!creature->GetMap()->IsDungeon() && !creature->IsCombatOnlyStealth() && creature->getThreatManager().HasThreat(this, true))
            getHostileRefManager().deleteReference(creature);
        if (Pet* pet = GetPet())
            if (pet->GetVictim() == creature)
                pet->AttackStop();
    }
    if (GetPetGuid() == creature->GetObjectGuid() && static_cast<Creature*>(creature)->IsPet())
        static_cast<Pet*>(creature)->Unsummon(PET_SAVE_REAGENTS);
}

/// 更新指定目标对象在玩家客户端的可见性（创建或销毁）
void Player::UpdateVisibilityOf(WorldObject const* viewPoint, WorldObject* target, UpdateData& updateData)
{
    if (HasAtClient(target))
    {
        if (!target->isVisibleForInState(this, viewPoint, true))
        {
            ObjectGuid t_guid = target->GetObjectGuid();

            if (target->IsCreature())
                BeforeVisibilityDestroy(static_cast<Creature*>(target));

            GetMap()->AddUpdateRemoveObject({ this->GetObjectGuid() }, target->GetObjectGuid());

            RemoveAtClient(target);

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf: %s out of range for player %u. Distance = %f", t_guid.GetString().c_str(), GetGUIDLow(), GetDistance(target));
        }
    }
    else
    {
        if (target->isVisibleForInState(this, viewPoint, false))
        {
            target->BuildCreateUpdateBlockForPlayer(updateData, this);
            AddAtClient(target);

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf: %s is visible now for player %u. Distance = %f", target->GetGuidStr().c_str(), GetGUIDLow(), GetDistance(target));
        }
    }
}

template<class T>
inline void UpdateVisibilityOf_helper(GuidSet& s64, T* target)
{
    if (!target->GetVisibilityData().IsInfiniteVisibility())
        s64.insert(target->GetObjectGuid());
}

template<class T>
void Player::UpdateVisibilityOf(WorldObject const* viewPoint, T* target, UpdateData& data, WorldObjectSet& visibleNow)
{
    if (HasAtClient(target))
    {
        if (!target->isVisibleForInState(this, viewPoint, true))
        {
            ObjectGuid t_guid = target->GetObjectGuid();

            if (target->GetTypeId() == TYPEID_UNIT)
                BeforeVisibilityDestroy(dynamic_cast<Creature*>(target));

            target->BuildOutOfRangeUpdateBlock(data);
            RemoveAtClient(target);

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(TemplateV): %s is out of range for %s. Distance = %f", t_guid.GetString().c_str(), GetGuidStr().c_str(), GetDistance(target));
        }
    }
    else
    {
        if (target->isVisibleForInState(this, viewPoint, false))
        {
            visibleNow.insert(target);
            target->BuildCreateUpdateBlockForPlayer(data, this);
            AddAtClient(target);

            DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "UpdateVisibilityOf(TemplateV): %s is visible now for %s. Distance = %f", target->GetGuidStr().c_str(), GetGuidStr().c_str(), GetDistance(target));
        }
    }
}

template void Player::UpdateVisibilityOf(WorldObject const* viewPoint, WorldObject*   target, UpdateData& data, WorldObjectSet& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint, Player*        target, UpdateData& data, WorldObjectSet& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint, Creature*      target, UpdateData& data, WorldObjectSet& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint, Corpse*        target, UpdateData& data, WorldObjectSet& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint, GameObject*    target, UpdateData& data, WorldObjectSet& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint, DynamicObject* target, UpdateData& data, WorldObjectSet& visibleNow);

void Player::SetPhaseMask(uint32 newPhaseMask)
{
    Unit::SetPhaseMask(newPhaseMask);

    m_pendingPhaseChange = true;
}

void Player::InitPrimaryProfessions()
{
    uint32 maxProfs = GetSession()->GetSecurity() < AccountTypes(sWorld.getConfig(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT))
                      ? sWorld.getConfig(CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL) : 10;
    SetFreePrimaryProfessions(maxProfs);
}

void Player::ClearSelectionGuid()
{
    if (m_curSelectionGuid)
    {
        WorldPacket data(SMSG_CLEAR_TARGET, 4);
        data << m_curSelectionGuid;
        GetSession()->SendPacket(data);
        m_curSelectionGuid = ObjectGuid();
    }
}

void Player::SendComboPoints() const
{
    Unit* combotarget = ObjectAccessor::GetUnit(*this, GetComboTargetGuid());
    if (combotarget)
    {
        WorldPacket data(SMSG_UPDATE_COMBO_POINTS, combotarget->GetPackGUID().size() + 1);
        data << combotarget->GetPackGUID();
        data << uint8(GetComboPoints());
        GetSession()->SendPacket(data);
    }
    /*else
    {
        // can be nullptr, and then points=0. Use unknown; to reset points of some sort?
        data << PackedGuid();
        data << uint8(0);
        GetSession()->SendPacket(data);
    }*/
}

void Player::SetGroup(Group* group, int8 subgroup)
{
    if (group == nullptr)
        m_group.unlink();
    else
    {
        // never use SetGroup without a subgroup unless you specify nullptr for group (设置有效队伍时必须指定合法子队编号)
        MANGOS_ASSERT(subgroup >= 0);
        m_group.link(group, this);
        m_group.setSubGroup((uint8)subgroup);
    }
}

/// 玩家加入地图前发送初始数据包（绑定点、教程、熟练度、动作条、初始法术等）
void Player::SendInitialPacketsBeforeAddToMap()
{
    WorldPacket data(SMSG_SET_REST_START, 4);
    data << uint32(0);                                      // unknown, may be rest state time or experience (未知字段，可能为休息状态计时)
    GetSession()->SendPacket(data);

    // Homebind (发送炉石绑定点位置)
    data.Initialize(SMSG_BINDPOINTUPDATE, 5 * 4);
    data << m_homebindX << m_homebindY << m_homebindZ;
    data << (uint32) m_homebindMapId;
    data << (uint32) m_homebindAreaId;
    GetSession()->SendPacket(data);

    // SMSG_SET_PROFICIENCY (发送初始护甲与武器熟练度)
    // SMSG_UPDATE_AURA_DURATION (通知自身初始光环剩余时间)

    // tutorial stuff (发送新手教程掩码)
    GetSession()->SendTutorialsData();

    data.Initialize(SMSG_INSTANCE_DIFFICULTY, 4 + 4);
    data << uint32(GetMap()->GetDifficulty());
    data << uint32(0);
    GetSession()->SendPacket(data);

    SendInitialSpells();

    SendUnlearnSpells();

    SendInitialActionButtons();
    m_reputationMgr.SendInitialReputations();

    // SMSG_SET_AURA_SINGLE (发送隐藏栏位光环时间)

    data.Initialize(SMSG_LOGIN_SETTIMESPEED, 4 + 4);
    data << uint32(secsToTimeBitFields(sWorld.GetGameTime()));
    data << (float)0.01666667f;                             // game speed (同步当前服务器游戏时间与流逝速率)
    GetSession()->SendPacket(data);

    if (!GetSession()->PlayerLoading())
        SetMover(this);
}

/// 玩家加入地图后发送区域状态、登录光效、移动速度及续飞数据
void Player::SendInitialPacketsAfterAddToMap(bool reconnect)
{
    // update zone (加入地图后刷新当前所在区域与子区域)
    uint32 newzone, newarea;
    GetZoneAndAreaId(newzone, newarea);
    UpdateZone(newzone, newarea);                           // also call SendInitWorldStates(); (同时内部会下发世界状态)

    GetSession()->ResetTimeSync();
    GetSession()->SendTimeSync();

    if (!reconnect)
    {
        // set some aura effects that send packet to player client after add player to map (玩家加入地图后重发部分需要地图上下文的自身光环包，)
        // SendMessageToSet not send it to player not it map, only for aura that not changed anything at re-apply (避免远距离传送后客户端丢失水下呼吸/跟踪/变身等状态)
        // same auras state lost at far teleport, send it one more time in this case also (跨地图传送后补发一次客户端光环状态)
        static const AuraType auratypes[] =
        {
            SPELL_AURA_GHOST,        SPELL_AURA_TRANSFORM,                 SPELL_AURA_WATER_WALK,
            SPELL_AURA_FEATHER_FALL, SPELL_AURA_HOVER,                     SPELL_AURA_SAFE_FALL,
            SPELL_AURA_MOD_STUN,     SPELL_AURA_MOD_ROOT,                  SPELL_AURA_MOD_FEAR,
            SPELL_AURA_FLY,          SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED,  SPELL_AURA_NONE
        };
        for (AuraType const* itr = &auratypes[0]; itr && itr[0] != SPELL_AURA_NONE; ++itr)
        {
            Unit::AuraList const& auraList = GetAurasByType(*itr);
            if (!auraList.empty())
                auraList.front()->ApplyModifier(true, true);
        }
    }

    SendAuraDurationsOnLogin();

    if (IsImmobilizedState()) // TODO: Figure out if this protocol is correct (待办：确认强制移动速度包协议细节)
        SendMoveRoot(true);

    SendEnchantmentDurations();                             // must be after add to map (必须在加入地图后发送登录事件)
    SendItemDurations();                                    // must be after add to map (必须在加入地图后激活飞行续飞)

    CastSpell(this, 836, TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_IGNORE_GCD); // LOGINEFFECT (发送登录光效通知)

    SendExtraAuraDurationsOnLogin(true);
    SendExtraAuraDurationsOnLogin(false);

    if (!sWorld.getConfig(CONFIG_BOOL_LFG_ENABLED))
    {
        WorldPacket data(SMSG_LFG_DISABLED);
        GetSession()->SendPacket(data);
    }
}

void Player::SendUpdateToOutOfRangeGroupMembers()
{
    if (m_groupUpdateMask == GROUP_UPDATE_FLAG_NONE)
        return;
    if (Group* group = GetGroup())
        group->UpdatePlayerOutOfRange(this);

    m_groupUpdateMask = GROUP_UPDATE_FLAG_NONE;
    ResetAuraUpdateMask();
    if (Unit* charm = GetCharm())
        charm->ResetAuraUpdateMask();
}

void Player::SendTransferAbortedByLockStatus(MapEntry const* mapEntry, AreaTrigger const* at, AreaLockStatus lockStatus, uint32 miscRequirement) const
{
    MANGOS_ASSERT(mapEntry);

    DEBUG_LOG("SendTransferAbortedByLockStatus: Called for %s on map %u, LockAreaStatus %u, miscRequirement %u)", GetGuidStr().c_str(), mapEntry->MapID, lockStatus, miscRequirement);

    if (!at->status_failed_text.empty())
    {
        switch (lockStatus)
        {
            case AREA_LOCKSTATUS_MISSING_ITEM:
                if (GetDifficulty() == DUNGEON_DIFFICULTY_HEROIC) // these have custom error in Map.dbc (此类跨地图错误使用 Map.dbc 中的自定义提示文本)
                    break;
            case AREA_LOCKSTATUS_TOO_LOW_LEVEL:
            case AREA_LOCKSTATUS_QUEST_NOT_COMPLETED:
            case AREA_LOCKSTATUS_RAID_LOCKED:
            {
                std::string message = at->status_failed_text;
                sObjectMgr.GetAreaTriggerLocales(at->entry, GetSession()->GetSessionDbLocaleIndex(), &message);
                GetSession()->SendAreaTriggerMessage(message.data(), miscRequirement);
                return;
            }
            default:
                break;
        }
    }

    switch (lockStatus)
    {
        case AREA_LOCKSTATUS_TOO_LOW_LEVEL:
            GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_LEVEL_MINREQUIRED), miscRequirement);
            break;
        case AREA_LOCKSTATUS_ZONE_IN_COMBAT:
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_ZONE_IN_COMBAT);
            break;
        case AREA_LOCKSTATUS_INSTANCE_IS_FULL:
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_MAX_PLAYERS);
            break;
        case AREA_LOCKSTATUS_QUEST_NOT_COMPLETED:
        {
            if (mapEntry->MapID == 269)                     // Exception for Black Morass (黑色沼泽副本传送特例提示)
            {
                GetSession()->SendAreaTriggerMessage("%s", GetSession()->GetMangosString(LANG_TELEREQ_QUEST_BLACK_MORASS));
                break;
            }
            if (mapEntry->IsContinent())               // do not report anything for quest areatrigge (任务类区域触发器不输出失败提示)
            {
                DEBUG_LOG("SendTransferAbortedByLockStatus: LockAreaStatus %u, do not teleport, no message sent (mapId %u)", lockStatus, mapEntry->MapID);
                break;
            }
        }
            // No break here! (此处有意穿透至默认失败消息分支)
        case AREA_LOCKSTATUS_MISSING_ITEM:
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_DIFFICULTY, GetDifficulty());
            break;
        case AREA_LOCKSTATUS_MISSING_DIFFICULTY:
        {
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_DIFFICULTY, GetDifficulty());
            break;
        }
        case AREA_LOCKSTATUS_INSUFFICIENT_EXPANSION:
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_INSUF_EXPAN_LVL, miscRequirement);
            break;
        case AREA_LOCKSTATUS_TOO_MANY_INSTANCE:
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_TOO_MANY_INSTANCES);
            break;
        case AREA_LOCKSTATUS_NOT_ALLOWED:
        case AREA_LOCKSTATUS_RAID_LOCKED:
        case AREA_LOCKSTATUS_UNKNOWN_ERROR:
            // ToDo: SendAreaTriggerMessage or Transfer Abort for these cases! (待办：为其余锁定状态补充区域或传送中止提示)
            break;
        case AREA_LOCKSTATUS_OK:
            sLog.outError("SendTransferAbortedByLockStatus: LockAreaStatus AREA_LOCKSTATUS_OK received for %s (mapId %u)", GetGuidStr().c_str(), mapEntry->MapID);
            MANGOS_ASSERT(false);
            break;
        default:
            sLog.outError("SendTransfertAbortedByLockstatus: unhandled LockAreaStatus %u, when %s attempts to enter in map %u", lockStatus, GetGuidStr().c_str(), mapEntry->MapID);
            break;
    }
}

void Player::SendInstanceResetWarning(uint32 mapid, uint32 time)
{
    // type of warning, based on the time remaining until reset (根据距离团队副本重置的剩余时间选择警告类型)
    uint32 type;
    if (time > 3600)
        type = RAID_INSTANCE_WELCOME;
    else if (time > 900 && time <= 3600)
        type = RAID_INSTANCE_WARNING_HOURS;
    else if (time > 300 && time <= 900)
        type = RAID_INSTANCE_WARNING_MIN;
    else
        type = RAID_INSTANCE_WARNING_MIN_SOON;

    WorldPacket data(SMSG_RAID_INSTANCE_MESSAGE, 4 + 4 + 4);
    data << uint32(type);
    data << uint32(mapid);
    data << uint32(time);
    GetSession()->SendPacket(data);
}

void Player::ApplyEquipCooldown(Item* pItem)
{
    if (pItem->GetProto()->Flags & ITEM_FLAG_NO_EQUIPCOOLDOWN)
        return;

    for (const auto& spellData : pItem->GetProto()->Spells)
    {
        // no spell (无绑定法术)
        if (!spellData.SpellId)
            continue;

        // wrong triggering type (note: ITEM_SPELLTRIGGER_ON_NO_DELAY_USE not have cooldown) (仅对常规使用型法术应用装备冷却)
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
            continue;

        SpellEntry const* spellentry = sSpellTemplate.LookupEntry<SpellEntry>(spellData.SpellId);
        if (!spellentry)
            continue;

        // 仅施加 30 秒换装基础冷却，忽略法术自带的长类别冷却（如祈福/咒逐 30 分钟），防止装备时误触发 (Issue #3477)
        AddCooldown(*spellentry, nullptr, false, 30 * IN_MILLISECONDS, true);

        WorldPacket data(SMSG_ITEM_COOLDOWN, 12);
        data << ObjectGuid(pItem->GetObjectGuid());
        data << uint32(spellData.SpellId);
        GetSession()->SendPacket(data);
    }
}

void Player::resetSpells()
{
    // not need after this call (遗忘全部法术前先重置天赋)
    if (HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
        RemoveAtLoginFlag(AT_LOGIN_RESET_SPELLS, true);

    // make full copy of map (spells removed and marked as deleted at another spell remove (先拷贝法术表，因为 removeSpell 过程中会修改或标记删除条目)
    // and we can't use original map for safe iterative with visit each spell at loop end (无法直接在原哈希表上安全迭代)
    PlayerSpellMap smap = GetSpellMap();

    for (PlayerSpellMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
        removeSpell(iter->first, false, false);             // only iter->first can be accessed, object by iter->second can be deleted already (迭代时仅读取法术 ID，指针可能已被销毁)

    LearnDefaultSkills();
    learnDefaultSpells();
    learnQuestRewardedSpells();
}

void Player::learnDefaultSpells()
{
    // learn default race/class spells (学习种族与职业默认初始法术)
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());

    for (uint32 tspell : info->spell)
    {
        DEBUG_LOG("PLAYER (Class: %u Race: %u): Adding initial spell, id = %u", uint32(getClass()), uint32(getRace()), tspell);
        if (!IsInWorld())                                   // will send in INITIAL_SPELLS in list anyway at map add (未在世界中时将在加入地图时随 INITIAL_SPELLS 统一下发)
            addSpell(tspell, true, true, true, false);
        else                                                // but send in normal spell in game learn case (在游戏中执行时逐个发送学习包)
            learnSpell(tspell, true);
    }
}

void Player::learnQuestRewardedSpells(Quest const* quest)
{
    uint32 spell_id = quest->GetRewSpellCast();

    // skip quests without rewarded spell (跳过无奖励法术的任务)
    if (!spell_id)
        return;

    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
    if (!spellInfo)
        return;

    // check learned spells state (校验法术是否已在任务完成时正常学会)
    bool found = false;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->Effect[i] == SPELL_EFFECT_LEARN_SPELL && !HasSpell(spellInfo->EffectTriggerSpell[i]))
        {
            found = true;
            break;
        }
    }

    // skip quests with not teaching spell or already known spell (跳过非教导型法术或已学会法术)
    if (!found)
        return;

    // Prevent learning profession specializations, because unlearning and re-learning a profession doesn't automatically re-add the specialization (防止遗忘专业后重学时自动补学专业专精法术)
    uint32 learned_0 = spellInfo->EffectTriggerSpell[EFFECT_INDEX_0];
    if (sSpellMgr.GetSpellRank(learned_0) > 1)
        return;

    CastSpell(this, spell_id, TRIGGERED_OLD_TRIGGERED);
}

void Player::learnQuestRewardedSpells()
{
    // learn spells received from quest completing (补学所有已完成任务的奖励法术)
    for (QuestStatusMap::const_iterator itr = mQuestStatus.begin(); itr != mQuestStatus.end(); ++itr)
    {
        // skip no rewarded quests (跳过尚未交付领奖的任务)
        if (!itr->second.m_rewarded)
            continue;

        Quest const* quest = sObjectMgr.GetQuestTemplate(itr->first);
        if (!quest)
            continue;

        learnQuestRewardedSpells(quest);
    }
}

void Player::SendAuraDurationsForTarget(Unit* target)
{
    SpellAuraHolderMap const& auraHolders = target->GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;

        if (holder->GetAuraSlot() >= MAX_AURAS || holder->IsPassive() || holder->GetCasterGuid() != GetObjectGuid())
            continue;

        holder->SendAuraDurationToCaster(this);
    }
}

void Player::SendAuraDurationsOnLogin()
{
    for (auto& data : GetSpellAuraHolderMap())
    {
        SpellAuraHolder* holder = data.second;
        if (holder->GetAuraSlot() >= MAX_AURAS)
            continue;

        holder->LoginAuraDuration();
    }
}

void Player::SendExtraAuraDurationsOnLogin(bool visible)
{
    std::vector<SpellAuraHolder*> holders;

    uint32 counter = MAX_AURAS;
    SpellAuraHolderMap const& auraHolders = GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;

        if (visible && holder->GetAuraSlot() >= MAX_AURAS) // passive auras are not visible (跳过不可见被动光环)
            continue;
        if (!visible)
        {
            if (holder->GetAuraSlot() < MAX_AURAS)
                continue;

            // TODO: Send racials - atm sending procs and quiver aura per sniff (待办：参照抓包下发种族与箭袋等触发光环)
            if (!holder->HasAuraType(SPELL_AURA_PROC_TRIGGER_SPELL) && !holder->HasAuraType(SPELL_AURA_MOD_RANGED_AMMO_HASTE))
                continue;
        }

        holders.push_back(holder);
        ++counter;
    }

    if (!visible) // slots >= 56 are sent as singles (槽位 >= 56 的隐藏光环使用单包下发)
    {
        std::sort(holders.begin(), holders.end(), [](SpellAuraHolder* left, SpellAuraHolder* right) { return left->GetAuraSlot() < right->GetAuraSlot(); });
        for (SpellAuraHolder* holder : holders)
            holder->SendAuraDurationToCaster(this);
    }
    else // visible slots (<= 56) are sent in a single packet (可见光环槽位 <= 56 打包汇总发送)
    {
        WorldPacket data(SMSG_INIT_EXTRA_AURA_INFO, (8 + (1 + 4 + 4 + 4) * holders.size()));
        data << GetPackGUID();
        for (SpellAuraHolder* holder : holders)
        {
            data << uint8(holder->GetAuraSlot());
            data << uint32(holder->GetId());
            data << int32(holder->GetAuraMaxDuration());
            data << uint32(holder->GetAuraMaxDuration() == -1 ? 0 : holder->GetAuraDuration());
        }
        SendDirectMessage(data);
    }
}

std::vector<WorldPacket> Player::BuildAurasForTarget(Player const& caster, Unit const& target)
{
    std::vector<WorldPacket> auras;

    SpellAuraHolderMap const& auraHolders = target.GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;

        if (holder->GetAuraSlot() >= MAX_AURAS || holder->IsPassive() || holder->GetCasterGuid() != caster.GetObjectGuid())
            continue;

        WorldPacket data = holder->BuildAuraDurationToCaster();
        auras.emplace_back(data);
    }

    return auras;
}

ItemSetEffect* Player::GetItemSetEffect(uint32 setId)
{
    auto itr = m_itemSetEffects.find(setId);
    if (itr == m_itemSetEffects.end())
        return nullptr;

    return &itr->second;
}

ItemSetEffect* Player::AddItemSetEffect(uint32 setId)
{
    return &m_itemSetEffects.emplace(setId, ItemSetEffect()).first->second;
}

void Player::RemoveItemSetEffect(uint32 setId)
{
    m_itemSetEffects.erase(setId);
}

void Player::SetDailyQuestStatus(uint32 quest_id)
{
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
    {
        if (!GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx))
        {
            SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, quest_id);
            m_DailyQuestChanged = true;
            break;
        }
    }
}

void Player::SetWeeklyQuestStatus(uint32 quest_id)
{
    m_weeklyquests.insert(quest_id);
    m_WeeklyQuestChanged = true;
}

void Player::SetMonthlyQuestStatus(uint32 quest_id)
{
    m_monthlyquests.insert(quest_id);
    m_MonthlyQuestChanged = true;
}

void Player::ResetDailyQuestStatus()
{
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
        SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, 0);

    // DB data deleted in caller (日常任务数据库记录由调用方统一删除)
    m_DailyQuestChanged = false;
}

void Player::ResetWeeklyQuestStatus()
{
    if (m_weeklyquests.empty())
        return;

    m_weeklyquests.clear();
    // DB data deleted in caller (周常任务数据库记录由调用方统一删除)
    m_WeeklyQuestChanged = false;
}

void Player::ResetMonthlyQuestStatus()
{
    if (m_monthlyquests.empty())
        return;

    m_monthlyquests.clear();
    // DB data deleted in caller (月常任务数据库记录由调用方统一删除)
    m_MonthlyQuestChanged = false;
}

BattleGround* Player::GetBattleGround() const
{
    if (GetBattleGroundId() == 0)
        return nullptr;

    return GetMap()->GetBG();
}

bool Player::InArena() const
{
    BattleGround* bg = GetBattleGround();
    return !(!bg || !bg->IsArena());
}

bool Player::GetBGAccessByLevel(BattleGroundTypeId bgTypeId) const
{
    // get a template bg instead of running one (获取战场模板以检查准入等级)
    BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    if (!bg)
        return false;

    if (GetLevel() < bg->GetMinLevel() || GetLevel() > bg->GetMaxLevel())
        return false;

    return true;
}

float Player::GetReputationPriceDiscount(Creature const* creature) const
{
    return GetReputationPriceDiscount(creature->GetFactionTemplateEntry());
}

float Player::GetReputationPriceDiscount(FactionTemplateEntry const* factionTemplate) const
{
    if (!factionTemplate || !factionTemplate->faction)
        return 1.0f;

    ReputationRank rank = GetReputationRank(factionTemplate->faction);
    if (rank <= REP_NEUTRAL)
        return 1.0f;

    return 1.0f - 0.05f * (rank - REP_NEUTRAL);
}

float Player::GetRepairPriceMultiplier(Creature const* vendor) const
{
    float discount = 0.0f;
    FactionTemplateEntry const* factionTemplate = vendor->GetFactionTemplateEntry();
    FactionEntry const* faction = factionTemplate ? sFactionStore.LookupEntry<FactionEntry>(factionTemplate->faction) : nullptr;
    if (faction && faction->HasReputation())
    {
        switch (vendor->GetReactionTo(this))
        {
            case REP_FRIENDLY: discount = 0.05f; break;
            case REP_HONORED:  discount = 0.1f;  break;
            case REP_REVERED:  discount = 0.15f; break;
            case REP_EXALTED:  discount = 0.2f;  break;
            default: break;
        }
    }
    return float(1.0 - double(discount));
}

/**
 * Check spell availability for training base at SkillLineAbility/SkillRaceClassInfo data.
 * 基于 SkillLineAbility 与 SkillRaceClassInfo 数据检查法术是否满足职业/种族/等级训练条件。
 * Checked allowed race/class and dependent from race/class allowed min level
 * 校验允许的种族、职业以及对应最低需求等级。
 *
 * @param spell_id  checked spell id (待检查的法术 ID)
 * @param pReqlevel if arg provided then function work in view mode (level check not applied but detected minlevel returned to var by arg pointer.
                    if arg not provided then considered train action mode and level checked
 *                  (若传入 pReqlevel 则为列表预览模式：不拦截等级并返回最低学习等级；否则为实际学习校验模式)
 * @return          true if spell available for show in trainer list (with skip level check) or training. (满足条件返回 true)
 */
bool Player::IsSpellFitByClassAndRace(uint32 spell_id, uint32* pReqlevel /*= nullptr*/) const
{
    uint32 racemask  = getRaceMask();
    uint32 classmask = getClassMask();

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(spell_id);
    if (bounds.first == bounds.second)
        return true;

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* abilityEntry = _spell_idx->second;
        // skip wrong race skills (跳过种族不匹配的技能)
        if (abilityEntry->racemask && (abilityEntry->racemask & racemask) == 0)
            continue;

        // skip wrong class skills (跳过职业不匹配的技能)
        if (abilityEntry->classmask && (abilityEntry->classmask & classmask) == 0)
            continue;

        SkillRaceClassInfoMapBounds raceBounds = sSpellMgr.GetSkillRaceClassInfoMapBounds(abilityEntry->skillId);
        for (SkillRaceClassInfoMap::const_iterator itr = raceBounds.first; itr != raceBounds.second; ++itr)
        {
            SkillRaceClassInfoEntry const* skillRCEntry = itr->second;
            if ((skillRCEntry->raceMask & racemask) && (skillRCEntry->classMask & classmask))
            {
                if (skillRCEntry->flags & SKILL_FLAG_NOT_TRAINABLE)
                    return false;

                if (pReqlevel)                              // show trainers list case (训练师列表预览模式：返回所需等级并放行)
                {
                    if (skillRCEntry->reqLevel)
                    {
                        *pReqlevel = skillRCEntry->reqLevel;
                        return true;
                    }
                }
                else                                        // check availble case at train (实际学习校验模式：检查玩家等级)
                {
                    if (skillRCEntry->reqLevel && GetLevel() < skillRCEntry->reqLevel)
                        return false;
                }
            }
        }

        return true;
    }

    return false;
}

bool Player::HasQuestForGO(int32 GOId) const
{
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (questid == 0)
            continue;

        QuestStatusMap::const_iterator qs_itr = mQuestStatus.find(questid);
        if (qs_itr == mQuestStatus.end())
            continue;

        QuestStatusData const& qs = qs_itr->second;

        if (qs.m_status == QUEST_STATUS_INCOMPLETE)
        {
            Quest const* qinfo = sObjectMgr.GetQuestTemplate(questid);
            if (!qinfo)
                continue;

            if (GetGroup() && GetGroup()->IsRaidGroup() && !qinfo->IsAllowedInRaid())
                continue;

            for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
            {
                if (qinfo->ReqCreatureOrGOId[j] >= 0)       // skip non GO case (跳过非游戏对象类任务目标)
                    continue;

                if ((-1)*GOId == qinfo->ReqCreatureOrGOId[j] && qs.m_creatureOrGOcount[j] < qinfo->ReqCreatureOrGOCount[j])
                    return true;
            }
        }
    }
    return false;
}

void Player::UpdateForQuestWorldObjects()
{
    if (m_clientGUIDs.empty())
        return;

    UpdateData updateData;
    for (auto m_clientGUID : m_clientGUIDs)
    {
        if (m_clientGUID.IsGameObject())
        {
            if (GameObject* obj = GetMap()->GetGameObject(m_clientGUID))
                if (sObjectMgr.IsGameObjectForQuests(obj->GetEntry()))
                    obj->BuildValuesUpdateBlockForPlayerWithFlags(updateData, this, UF_FLAG_DYNAMIC);
        }
    }
    updateData.SendData(*GetSession());
}

void Player::UpdateEverything()
{
    if (m_clientGUIDs.empty())
        return;

    UpdateData updateData;
    for (const auto guid : m_clientGUIDs)
        if (WorldObject* obj = GetMap()->GetWorldObject(guid))
            obj->BuildForcedValuesUpdateBlockForPlayer(updateData, this);

    updateData.SendData(*GetSession());
}

void Player::SummonIfPossible(bool agree, ObjectGuid guid)
{
    // old duplicate summon notification from client or hacking (忽略过期或重复的召唤响应，防止作弊)
    if (guid != m_summoner)
        return;

    if (!agree)
    {
        m_summon_expire = 0;
        return;
    }

    // expire and auto declined (召唤请求已过期或被玩家拒绝)
    if (m_summon_expire < time(nullptr))
        return;

    // stop taxi flight at summon (接受召唤时中断正在进行的飞行路线)
    TaxiFlightInterrupt();

    // drop flag at summon (接受召唤时强制丢弃战场旗帜)
    // this code can be reached only when GM is summoning player who carries flag, because player should be immune to summoning spells when he carries flag (仅当 GM 强召持旗玩家时才会触发此处丢旗)
    if (BattleGround* bg = GetBattleGround())
        bg->HandlePlayerDroppedFlag(this);

    m_summon_expire = 0;
    m_summoner.Clear();

    TeleportTo(m_summon_mapid, m_summon_x, m_summon_y, m_summon_z, GetOrientation());
}

void Player::RemoveItemDurations(Item* item)
{
    for (ItemDurationList::iterator itr = m_itemDuration.begin(); itr != m_itemDuration.end(); ++itr)
    {
        if (*itr == item)
        {
            m_itemDuration.erase(itr);
            break;
        }
    }
}

void Player::AddItemDurations(Item* item)
{
    if (item->GetUInt32Value(ITEM_FIELD_DURATION))
    {
        m_itemDuration.push_back(item);
        item->SendTimeUpdate(this);
    }
}

void Player::AutoUnequipOffhandIfNeed(uint8 bag)
{
    Item* offItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!offItem)
        return;

    ItemPrototype const* itemProto = offItem->GetProto();

    // need unequip offhand for 2h-weapon (装备双手武器时自动卸下副手物品)
    if ((CanDualWield() || itemProto->InventoryType == INVTYPE_SHIELD || itemProto->InventoryType == INVTYPE_HOLDABLE) &&
            !IsTwoHandUsed())
        return;

    ItemPosCountVec off_dest;
    uint8 bagSlot = 0;
    uint8 off_msg = CanStoreItem(bag, NULL_SLOT, off_dest, offItem, bagSlot, false);
    if (off_msg == EQUIP_ERR_OK)
    {
        RemoveItem(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND, true);
        StoreItem(off_dest, offItem, true);
    }
    else
    {
        MoveItemFromInventory(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND, true);
        CharacterDatabase.BeginTransaction();
        offItem->DeleteFromInventoryDB();                   // deletes item from character's inventory (从角色背包销毁物品并将数量归零)
        offItem->SaveToDB();                                // recursive and not have transaction guard into self, item not in inventory and can be save standalone (此时物品已不在背包中，可安全独立删除)
        CharacterDatabase.CommitTransaction();

        std::string subject = GetSession()->GetMangosString(LANG_NOT_EQUIPPED_ITEM);
        MailDraft(subject).AddItem(offItem).SendMailTo(this, MailSender(this, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
    }
}

/// 检查玩家是否装备了施放指定法术所需的特定类别武器或装备
bool Player::HasItemFitToSpellReqirements(SpellEntry const* spellInfo, Item const* ignoreItem, uint32* error) const
{
    if (spellInfo->EquippedItemClass < 0)
        return true;

    if (spellInfo->HasAttribute(SPELL_ATTR_EX3_REQUIRES_MAIN_HAND_WEAPON) && !hasMainhandWeaponForAttack())
    {
        if (error)
            *error = SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND;
        return false;
    }

    if (spellInfo->HasAttribute(SPELL_ATTR_EX3_REQUIRES_OFFHAND_WEAPON) && !hasOffhandWeaponForAttack())
    {
        if (error)
            *error = SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND;
        return false;
    }

    // scan other equipped items for same requirements (mostly 2 daggers/etc) (扫描其他已装备部位是否仍满足法术物品需求，如双持匕首)
    // for optimize check 2 used cases only (仅优化检查常用的主手/副手及指定槽位)
    switch (spellInfo->EquippedItemClass)
    {
        case ITEM_CLASS_WEAPON:
        {
            for (int i = EQUIPMENT_SLOT_MAINHAND; i < EQUIPMENT_SLOT_TABARD; ++i)
                if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                    if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                        return true;
            break;
        }
        case ITEM_CLASS_ARMOR:
        {
            // tabard not have dependent spells (战袍等无相关装备依赖施法)
            for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_MAINHAND; ++i)
                if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                    if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                        return true;

            // shields can be equipped to offhand slot (盾牌可装备在副手槽位)
            if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
                if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                    return true;

            // ranged slot can have some armor subclasses (远程槽位可装配圣物/神像/图腾等防具子类)
            if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
                if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                    return true;

            break;
        }
        default:
            sLog.outError("HasItemFitToSpellReqirements: Not handled spell requirement for item class %u", spellInfo->EquippedItemClass);
            break;
    }

    return false;
}

bool Player::CanNoReagentCast(SpellEntry const* spellInfo) const
{
    // don't take reagents for spells with SPELL_ATTR_EX5_NO_REAGENT_WHILE_PREP (带有准备阶段免材料属性的法术不消耗施法材料)
    if (spellInfo->HasAttribute(SPELL_ATTR_EX5_NO_REAGENT_COST_WITH_AURA) && HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PREPARATION))
        return true;

    return false;
}

void Player::RemoveItemDependentAurasAndCasts(Item* pItem)
{
    SpellAuraHolderMap& auras = GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end();)
    {
        SpellAuraHolder* holder = itr->second;

        // skip passive (passive item dependent spells work in another way) and not self applied auras (跳过被动光环及非自身施放的光环)
        SpellEntry const* spellInfo = holder->GetSpellProto();
        if (holder->IsPassive() ||  holder->GetCasterGuid() != GetObjectGuid())
        {
            ++itr;
            continue;
        }

        // skip if not item dependent or have alternative item (若不依赖物品或身上仍有替代装备则保留)
        if (HasItemFitToSpellReqirements(spellInfo, pItem))
        {
            ++itr;
            continue;
        }

        // no alt item, remove aura, restart check (无替代装备，移除该光环并重新迭代)
        RemoveAurasDueToSpell(holder->GetId());
        itr = auras.begin();
    }

    // currently casted spells can be dependent from item (打断当前正在施放但已失去装备依赖的法术)
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
        if (Spell* spell = GetCurrentSpell(CurrentSpellTypes(i)))
            if (!HasItemFitToSpellReqirements(spell->m_spellInfo, pItem))
                InterruptSpell(CurrentSpellTypes(i));
}

uint32 Player::GetResurrectionSpellId() const
{
    // search priceless resurrection possibilities (按优先级查找免费复活方式：灵魂石 > 扭曲虚空 > 复生)
    uint32 prio = 0;
    uint32 spell_id = 0;
    AuraList const& dummyAuras = GetAurasByType(SPELL_AURA_DUMMY);
    for (auto dummyAura : dummyAuras)
    {
        // Soulstone Resurrection                           // prio: 3 (max, non death persistent) (灵魂石复活，最高优先级 3)
        if (prio < 2 && dummyAura->GetSpellProto()->SpellVisual == 99 && dummyAura->GetSpellProto()->SpellIconID == 92)
        {
            switch (dummyAura->GetId())
            {
                case 20707: spell_id =  3026; break;        // rank 1 (1 阶)
                case 20762: spell_id = 20758; break;        // rank 2 (2 阶)
                case 20763: spell_id = 20759; break;        // rank 3 (3 阶)
                case 20764: spell_id = 20760; break;        // rank 4 (4 阶)
                case 20765: spell_id = 20761; break;        // rank 5 (5 阶)
                case 27239: spell_id = 27240; break;        // rank 6 (6 阶)
                default:
                    sLog.outError("Unhandled spell %u: S.Resurrection", dummyAura->GetId());
                    continue;
            }

            prio = 3;
        }
        // Twisting Nether                                  // prio: 2 (max) (扭曲虚空饰品效果，优先级 2)
        else if (dummyAura->GetId() == 23701 && roll_chance_i(10))
        {
            prio = 2;
            spell_id = 23700;
        }
    }

    // Reincarnation (passive spell)                        // prio: 1 (萨满复生被动，优先级 1)
    if (prio < 1 && HasSpell(20608) && IsSpellReady(21169) && HasItemCount(17030, EFFECT_INDEX_1))
        spell_id = 21169;

    return spell_id;
}

// Used in triggers for check "Only to targets that grant experience or honor" req (用于检查“仅对可提供经验或荣誉的目标生效”触发条件)
bool Player::isHonorOrXPTarget(Unit* pVictim) const
{
    // Victim level less gray level (目标等级低于灰名门槛则不提供经验/荣誉)
    if (MaNGOS::XP::IsTrivialLevelDifference(GetLevel(), pVictim->GetLevelForTarget(this)))
        return false;

    if (pVictim->GetTypeId() == TYPEID_UNIT)
    {
        Creature* npc = static_cast<Creature*>(pVictim);

        if (npc->IsTotem() || npc->IsPet() || npc->IsNoXp())
            return false;
    }
    return true;
}

void Player::RewardSinglePlayerAtKill(Unit* pVictim)
{
    // honor can be in PvP and !PvP (racial leader) cases (击杀玩家或敌对种族领袖均可获得荣誉)
    RewardHonor(pVictim, 1);

    // xp and reputation only in !PvP case (仅击杀非 PvP 生物才奖励经验与声望)
    if (!pVictim->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED) && pVictim->GetTypeId() == TYPEID_UNIT)
    {
        Creature* creatureVictim = static_cast<Creature*>(pVictim);
        RewardReputation(creatureVictim, 1);
        GiveXP(MaNGOS::XP::Gain(this, creatureVictim), creatureVictim);

        if (Pet* pet = GetPet())
            pet->GivePetXP(MaNGOS::XP::Gain(pet, creatureVictim));

        // normal creature (not pet/etc) can be only in !PvP case (仅常规非宠物生物触发击杀任务计数)
        if (CreatureInfo const* normalInfo = creatureVictim->GetCreatureInfo())
            KilledMonster(normalInfo, creatureVictim);
    }
}

void Player::RewardPlayerAndGroupAtEventCredit(uint32 creature_id, WorldObject* pRewardSource)
{
    MANGOS_ASSERT((!GetGroup() || pRewardSource) && "Player::RewardPlayerAndGroupAtEvent called for Group-Case but no source for range searching provided");

    ObjectGuid creature_guid = pRewardSource && pRewardSource->GetTypeId() == TYPEID_UNIT ? pRewardSource->GetObjectGuid() : ObjectGuid();

    // prepare data for near group iteration (遍历附近队伍/团队成员共享任务事件计数)
    if (Group* pGroup = GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* pGroupGuy = itr->getSource();
            if (!pGroupGuy)
                continue;

            if (!pGroupGuy->IsAtGroupRewardDistance(pRewardSource))
                continue;                                   // member (alive or dead) or his corpse at req. distance (成员存活或尸体在有效共享距离内)

            // quest objectives updated only for alive group member or dead but with not released body (仅存活或未释放灵魂的队友获得任务目标计数)
            if (pGroupGuy->IsAlive() || !pGroupGuy->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                pGroupGuy->KilledMonsterCredit(creature_id, creature_guid);
        }
    }
    else                                                    // if (!pGroup) (单人玩家情况)
        KilledMonsterCredit(creature_id, creature_guid);
}

void Player::RewardPlayerAndGroupAtCast(WorldObject* pRewardSource, uint32 spellid)
{
    // prepare data for near group iteration (遍历附近队伍/团队成员共享施法任务计数)
    if (Group* pGroup = GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* pGroupGuy = itr->getSource();
            if (!pGroupGuy)
                continue;

            if (!pGroupGuy->IsAtGroupRewardDistance(pRewardSource))
                continue;                               // member (alive or dead) or his corpse at req. distance (成员或其尸体在有效共享距离内)

            // quest objectives updated only for alive group member or dead but with not released body (仅存活或未释放灵魂的队友更新施法计数)
            if (pGroupGuy->IsAlive() || !pGroupGuy->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                pGroupGuy->CastedCreatureOrGO(pRewardSource->GetEntry(), pRewardSource->GetObjectGuid(), spellid, pGroupGuy == this);
        }
    }
    else                                                    // if (!pGroup) (单人玩家情况)
        CastedCreatureOrGO(pRewardSource->GetEntry(), pRewardSource->GetObjectGuid(), spellid);
}

bool Player::IsAtGroupRewardDistance(WorldObject const* pRewardSource) const
{
    if (IsInWorld() && pRewardSource->GetMap() == GetMap() && (GetMap()->IsDungeon() || pRewardSource->IsWithinDistInMap(this, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE))))
        return true;

    return false;
}

void Player::AddResurrectRequest(ObjectGuid casterGuid, SpellEntry const* spellInfo, Position position, uint32 mapId, uint32 health, uint32 mana, bool isSpiritHealer, const char* sentName)
{
    if (isRessurectRequested()) // already have one active request (已存在待响应的复活请求时忽略新请求)
        return;

    setResurrectRequestData(casterGuid, mapId, position.x, position.y, position.z, health, mana);
    SendResurrectRequest(spellInfo, isSpiritHealer, sentName);
}

void Player::SendResurrectRequest(SpellEntry const* spellInfo, bool isSpiritHealer, const char* sentName)
{
    WorldPacket data(SMSG_RESURRECT_REQUEST, (8 + 4 + strlen(sentName) + 1 + 1 + 1));
    data << m_resurrectGuid;
    data << uint32(strlen(sentName) + 1);

    data << sentName;
    data << uint8(isSpiritHealer);
    // override delay sent with SMSG_CORPSE_RECLAIM_DELAY, set instant resurrection for spells with this attribute (带有忽略复活延迟属性的法术允许立即复活)
    data << uint8(!spellInfo->HasAttribute(SPELL_ATTR_EX3_NO_RES_TIMER));
    GetSession()->SendPacket(data);
}

void Player::ResurrectUsingRequestDataInit()
{
    /// Teleport before resurrecting by player, otherwise the player might get attacked from creatures near his corpse (由他人复活前先传送至目标位置，避免复活后立即被尸体附近怪物攻击)
    if (m_resurrectGuid.IsPlayer())
        TeleportTo(m_resurrectMap, m_resurrectX, m_resurrectY, m_resurrectZ, GetOrientation());

    // we cannot resurrect player when we triggered far teleport (若触发跨地图远距离传送则延后至切图完成后复活)
    // player will be resurrected upon teleportation (传送完成后由 DELAYED_RESURRECT_PLAYER 执行复活)
    if (IsBeingTeleported())
    {
        ScheduleDelayedOperation(DELAYED_RESURRECT_PLAYER);
        return;
    }

    // Redundant unless at some point we dont teleport on rezzurect (传送完毕执行最终复活步骤)
    ResurrectUsingRequestDataFinish();
}

void Player::ResurrectUsingRequestDataFinish()
{
    ResurrectPlayer(0.0f, false);

    if (GetMaxHealth() > m_resurrectHealth)
        SetHealth(m_resurrectHealth);
    else
        SetHealth(GetMaxHealth());

    if (GetMaxPower(POWER_MANA) > m_resurrectMana)
        SetPower(POWER_MANA, m_resurrectMana);
    else
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));

    SetPower(POWER_RAGE, 0);

    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));

    SpawnCorpseBones();
}

void Player::UpdateClientControl(Unit const* target, bool enabled, bool forced) const
{
    if (target)
    {
        // Sending disabled control multiple times for the same unit is harmless (seen in data all the time) (重复下发禁用控制权无害)
        // Do a double-check if we should enable it only (仅在启用控制权时严格校验目标是否仍为当前控制对象)
        if (forced || !enabled || target->IsClientControlled(this))
        {
            const PackedGuid& packedGuid = target->GetPackGUID();
            WorldPacket data(SMSG_CLIENT_CONTROL_UPDATE, packedGuid.size() + 1);
            data << packedGuid;
            data << uint8(enabled);
            GetSession()->SendPacket(data);
        }
    }
}

void Player::UpdateZoneDependentAuras()
{
    // Some spells applied at enter into zone (with subzones), aura removed in UpdateAreaDependentAuras that called always at zone->area update (进入主区域时应用区域依赖光环)
    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(m_zoneUpdateId);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        itr->second->ApplyOrRemoveSpellIfCan(this, m_zoneUpdateId, 0, true);
}

void Player::UpdateAreaDependentAuras()
{
    // remove auras from spells with area limitations (移除不适用于当前子区域的光环)
    for (SpellAuraHolderMap::iterator iter = m_spellAuraHolders.begin(); iter != m_spellAuraHolders.end();)
    {
        // use m_zoneUpdateId for speed: UpdateArea called from UpdateZone or instead UpdateZone in both cases m_zoneUpdateId up-to-date (使用已刷新的 m_zoneUpdateId 加快判断)
        if (sSpellMgr.GetSpellAllowedInLocationError(iter->second->GetSpellProto(), GetMapId(), m_zoneUpdateId, m_areaUpdateId, this) != SPELL_CAST_OK)
        {
            RemoveSpellAuraHolder(iter->second);
            iter = m_spellAuraHolders.begin();
        }
        else
            ++iter;
    }

    // some auras applied at subzone enter (应用进入子区域触发的光环)
    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(m_areaUpdateId);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        itr->second->ApplyOrRemoveSpellIfCan(this, m_zoneUpdateId, m_areaUpdateId, true);
}

struct UpdateZoneDependentPetsHelper
{
    explicit UpdateZoneDependentPetsHelper(Player* _owner, uint32 zone, uint32 area) : owner(_owner), zone_id(zone), area_id(area) {}
    void operator()(Unit* unit) const
    {
        if (unit->GetTypeId() == TYPEID_UNIT && ((Creature*)unit)->IsPet() && !((Pet*)unit)->isControlled())
            if (uint32 spell_id = unit->GetUInt32Value(UNIT_CREATED_BY_SPELL))
                if (SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(spell_id))
                    if (sSpellMgr.GetSpellAllowedInLocationError(spellEntry, owner->GetMapId(), zone_id, area_id, owner) != SPELL_CAST_OK)
                        ((Pet*)unit)->Unsummon(PET_SAVE_AS_DELETED, owner);
    }
    Player* owner;
    uint32 zone_id;
    uint32 area_id;
};

void Player::UpdateZoneDependentPets()
{
    // check pet (permanent pets ignored), minipet, guardians (including protector) (检查非永久宠物、小宠物及守护者在当前区域是否允许存在)
    CallForAllControlledUnits(UpdateZoneDependentPetsHelper(this, m_zoneUpdateId, m_areaUpdateId), CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_MINIPET);
}

uint32 Player::GetCorpseReclaimDelay(bool pvp) const
{
    time_t now = time(nullptr);
    if (now > m_deathExpireTime)
        now = m_deathExpireTime;
    // 0..2 full period (按死亡累计次数计算 0..2 个完整复活惩罚周期)
    return getCorpseReclaimDelayHelper(m_deathExpireTime, now, pvp);
}

void Player::UpdateCorpseReclaimDelay()
{
    const bool pvp = (m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH) != 0;

    if ((pvp && !sWorld.getConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP)) ||
            (!pvp && !sWorld.getConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE)))
        return;

    time_t now = time(nullptr);
    if (now < m_deathExpireTime)
    {
        // full and partly periods 1..3 (计算已过时间并折算剩余复活延迟毫秒数)
        uint32 count = uint32((m_deathExpireTime - now) / DEATH_EXPIRE_STEP + 1);
        if (count < MAX_DEATH_COUNT)
            m_deathExpireTime = now + (count + 1) * DEATH_EXPIRE_STEP;
        else
            m_deathExpireTime = now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP;
    }
    else
        m_deathExpireTime = now + DEATH_EXPIRE_STEP;
}

void Player::SendCorpseReclaimDelay(bool load) const
{
    Corpse* corpse = GetCorpse();
    if (!corpse)
        return;

    uint32 delay;
    if (load)
    {
        if (corpse->GetGhostTime() > m_deathExpireTime)
            return;

        bool pvp = corpse->GetType() == CORPSE_RESURRECTABLE_PVP;
        time_t expected_time = corpse->GetGhostTime() + getCorpseReclaimDelayHelper(m_deathExpireTime, corpse->GetGhostTime(), pvp);

        time_t now = time(nullptr);
        if (now >= expected_time)
            return;

        delay = uint32(expected_time - now);
    }
    else
        delay = GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP);

    //! corpse reclaim delay 30 * 1000ms or longer at often deaths (频繁死亡时复活延迟最少 30 秒)
    WorldPacket data(SMSG_CORPSE_RECLAIM_DELAY, 4);
    data << uint32(delay * IN_MILLISECONDS);
    GetSession()->SendPacket(data);
}

Player* Player::GetNextRaidMemberWithLowestLifePercentage(float radius, AuraType noAuraType)
{
    Group* pGroup = GetGroup();
    if (!pGroup)
        return nullptr;

    Player* lowestPercentagePlayer = nullptr;
    uint32 lowestPercentage = 100;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* target = itr->getSource();

        if (target && target != this && target->IsAlive())
        {
            float x, y, z;
            GetPosition(x, y, z);
            // CanAssist check duel and controlled by enemy (过滤无法协助、决斗中或被敌方控制的队员)
            if (IsInMap(target) && target->IsWithinDist3d(x, y, z, radius) &&
                    !target->HasInvisibilityAura() && CanAssist(target) && !target->HasAuraType(noAuraType))
            {
                // First not picked (筛选生命百分比最低的团队成员)
                if (!lowestPercentagePlayer)
                {
                    lowestPercentagePlayer = target;
                    lowestPercentage = target->GetHealthPercent();
                }
                else if (target->GetHealthPercent() < lowestPercentage)
                {
                    lowestPercentagePlayer = target;
                    lowestPercentage = target->GetHealthPercent();
                }
            }
        }
    }

    return lowestPercentagePlayer;
}

PartyResult Player::CanUninviteFromGroup() const
{
    const Group* grp = GetGroup();
    if (!grp)
        return ERR_NOT_IN_GROUP;

    if (!grp->IsLeader(GetObjectGuid()) && !grp->IsAssistant(GetObjectGuid()))
        return ERR_NOT_LEADER;

    if (InBattleGround())
        return ERR_INVITE_RESTRICTED;

    return ERR_PARTY_RESULT_OK;
}

void Player::UpdateGroupLeaderFlag(const bool remove /*= false*/)
{
    const Group* group = GetGroup();
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GROUP_LEADER))
    {
        if (remove || !group || group->GetLeaderGuid() != GetObjectGuid())
            RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GROUP_LEADER);
    }
    else if (!remove && group && group->GetLeaderGuid() == GetObjectGuid())
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GROUP_LEADER);
}

void Player::SetBattleGroundRaid(Group* group, int8 subgroup)
{
    // we must move references from m_group to m_originalGroup (进入战场团队前将原野外队伍引用转移至 m_originalGroup)
    SetOriginalGroup(GetGroup(), GetSubGroup());

    m_group.unlink();
    m_group.link(group, this);
    m_group.setSubGroup((uint8)subgroup);
}

void Player::RemoveFromBattleGroundRaid()
{
    // remove existing reference (离开战场团队后移除战场队伍引用)
    m_group.unlink();
    if (Group* group = GetOriginalGroup())
    {
        m_group.link(group, this);
        m_group.setSubGroup(GetOriginalSubGroup());
    }
    SetOriginalGroup(nullptr);
}

void Player::SetOriginalGroup(Group* group, int8 subgroup)
{
    if (group == nullptr)
        m_originalGroup.unlink();
    else
    {
        // never use SetOriginalGroup without a subgroup unless you specify nullptr for group (设置原队伍时必须指定子队编号)
        MANGOS_ASSERT(subgroup >= 0);
        m_originalGroup.link(group, this);
        m_originalGroup.setSubGroup((uint8)subgroup);
    }
}

void Player::UpdateTerainEnvironmentFlags(Map* m, float x, float y, float z)
{
    float collisionHeight = GetCollisionHeight();
    GridMapLiquidData liquid_status;
    GridMapLiquidStatus res = m->GetTerrain()->getLiquidStatus(x, y, z, MAP_ALL_LIQUIDS, &liquid_status, collisionHeight);
    if (!res)
    {
        SetEnvironmentFlags(ENVIRONMENT_MASK_LIQUID_FLAGS, false);
        if (m_lastLiquid && m_lastLiquid->SpellId)
            RemoveAurasDueToSpell(m_lastLiquid->SpellId == 37025 ? 37284 : m_lastLiquid->SpellId);
        m_lastLiquid = nullptr;
        return;
    }

    // Environment has liquid information (地图地形返回了液体数据，判定深浅与液体类型)
    SetEnvironmentFlags(ENVIRONMENT_FLAG_LIQUID, true);

    if (uint32 liqEntry = liquid_status.entry)
    {
        LiquidTypeEntry const* liquid = sLiquidTypeStore.LookupEntry(liqEntry);
        if (m_lastLiquid && m_lastLiquid->SpellId && m_lastLiquid->Id != liqEntry)
            RemoveAurasDueToSpell(m_lastLiquid->SpellId);

        if (liquid && liquid->SpellId)
        {
            if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER))
            {
                if (!HasAura(liquid->SpellId))
                    CastSpell(this, liquid->SpellId, TRIGGERED_OLD_TRIGGERED);
            }
            else
                RemoveAurasDueToSpell(liquid->SpellId);
        }

        m_lastLiquid = liquid;
    }

    // All liquid types: check under surface level (任何液体：检测是否完全浸没于液面之下)
    if (liquid_status.type_flags & (MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_OCEAN | MAP_LIQUID_TYPE_MAGMA | MAP_LIQUID_TYPE_SLIME))
        SetEnvironmentFlags(ENVIRONMENT_FLAG_UNDERWATER, (res & LIQUID_MAP_UNDER_WATER));

    // In water: on or under surface level (处于水中：位于水面或水下)
    if (liquid_status.type_flags & (MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_OCEAN))
        SetEnvironmentFlags(ENVIRONMENT_FLAG_IN_WATER, (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER)));

    // In magma: on, under, or slightly above surface level (处于岩浆中)
    if (liquid_status.type_flags & MAP_LIQUID_TYPE_MAGMA)
        SetEnvironmentFlags(ENVIRONMENT_FLAG_IN_MAGMA, (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER | LIQUID_MAP_WATER_WALK)));

    // In slime: on, under, or slightly above surface level (处于软泥/毒水中)
    if (liquid_status.type_flags & MAP_LIQUID_TYPE_SLIME)
        SetEnvironmentFlags(ENVIRONMENT_FLAG_IN_SLIME, (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER | LIQUID_MAP_WATER_WALK)));

    // In deep water: on, under, above surface level (处于深海疲劳区域)
    SetEnvironmentFlags(ENVIRONMENT_FLAG_HIGH_SEA, (liquid_status.type_flags & MAP_LIQUID_TYPE_DEEP_WATER));

    // All liquid types: check if deep enough level for swimming (液体深度是否足以游泳)
    SetEnvironmentFlags(ENVIRONMENT_FLAG_HIGH_LIQUID, ((res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER)) && liquid_status.level > (liquid_status.depth_level + collisionHeight * 0.75)));
}

bool ItemPosCount::isContainedIn(ItemPosCountVec const& vec) const
{
    for (auto itr : vec)
        if (itr.pos == pos)
            return true;

    return false;
}

bool Player::CanUseBattleGroundObject() const
{
    // TODO : some spells gives player ForceReaction to one faction (ReputationMgr::ApplyForceReaction) (待办：部分法术会强制改变阵营声望反应，)
    // maybe gameobject code should handle that ForceReaction usage (需确认游戏对象交互是否需同步支持)
    return (IsAlive() &&                                    // living (必须存活)
            !isTotalImmune() &&                             // vulnerable (not immune) (未处于无敌/免疫状态)
            !HasAura(SPELL_RECENTLY_DROPPED_FLAG, EFFECT_INDEX_0));
}

bool Player::isTotalImmune() const
{
    AuraList const& immune = GetAurasByType(SPELL_AURA_SCHOOL_IMMUNITY);

    uint32 immuneMask = 0;
    for (auto itr : immune)
    {
        immuneMask |= itr->GetModifier()->m_miscvalue;
        if (immuneMask & SPELL_SCHOOL_MASK_ALL)             // total immunity (具备法术全免疫光环，如圣盾术/冰箱)
            return true;
    }
    return false;
}

bool Player::HasTitle(uint32 bitIndex) const
{
    if (bitIndex > MAX_TITLE_INDEX)
        return false;

    uint32 fieldIndexOffset = bitIndex / 32;
    uint32 flag = 1 << (bitIndex % 32);
    return HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag);
}

void Player::SetTitle(uint32 titleId, bool lost)
{
    if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(titleId))
        SetTitle(titleEntry, lost);
}

void Player::SetTitle(CharTitlesEntry const* title, bool lost, bool send)
{
    uint32 fieldIndexOffset = title->bit_index / 32;
    uint32 flag = 1 << (title->bit_index % 32);

    if (lost)
    {
        if (!HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag))
            return;

        RemoveFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag);
    }
    else
    {
        if (HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag))
            return;

        SetFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag);
    }

    SetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_LIFETIME_MAX_PVP_RANK, GetHighestPvPRankIndex());

    if (!send)
        return;

    WorldPacket data(SMSG_TITLE_EARNED, 4 + 4);
    data << uint32(title->bit_index);
    data << uint32(lost ? 0 : 1);                           // 1 - earned, 0 - lost (1 表示获得称号，0 表示失去称号)
    GetSession()->SendPacket(data);
}

void Player::SaveTitles()
{
    std::string playerTitles;
    for (uint32 i = 0; i < 2; ++i)
        playerTitles += std::to_string(GetUInt32Value(PLAYER__FIELD_KNOWN_TITLES + i)) + " ";
    CharacterDatabase.PExecute("UPDATE characters SET KnownTitles='%s' WHERE guid = '%u'", playerTitles.data(), GetGUIDLow());
}

void Player::SetQueuedSpell(Spell* spell)
{
    m_queuedSpell.reset(spell);
}

bool Player::HasQueuedSpell()
{
    return bool(m_queuedSpell);
}

void Player::ClearQueuedSpell()
{
    m_queuedSpell.reset();
}

void Player::CastQueuedSpell(SpellCastTargets& targets)
{
    Spell* spell = m_queuedSpell.release();
    targets.Update(this);
    spell->SpellStart(&targets);
}

void Player::BanPlayer(std::string const& reason)
{
    std::string reasonCopy = reason;
    sWorld.GetMessager().AddMessage([sessionId = GetSession()->GetAccountId(), reasonCopy](World* world)
    {
        sWorld.BanAccount(world->FindSession(sessionId), 0, reasonCopy, "Automatic script ban.");
    });
}

Item* Player::ConvertItem(Item* item, uint32 newItemId)
{
    uint16 pos = item->GetPos();

    Item* pNewItem = Item::CreateItem(newItemId, 1, this);
    if (!pNewItem)
        return nullptr;

    // copy enchantments (物品转换时复制原附魔)
    for (uint8 j = PERM_ENCHANTMENT_SLOT; j <= TEMP_ENCHANTMENT_SLOT; ++j)
    {
        if (item->GetEnchantmentId(EnchantmentSlot(j)))
            pNewItem->SetEnchantment(EnchantmentSlot(j), item->GetEnchantmentId(EnchantmentSlot(j)),
                                     item->GetEnchantmentDuration(EnchantmentSlot(j)), item->GetEnchantmentCharges(EnchantmentSlot(j)));
    }

    // copy durability (复制当前耐久度状态)
    if (item->GetUInt32Value(ITEM_FIELD_DURABILITY) < item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
    {
        double loosePercent = 1 - item->GetUInt32Value(ITEM_FIELD_DURABILITY) / double(item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY));
        DurabilityLoss(pNewItem, loosePercent);
    }

    if (IsInventoryPos(pos))
    {
        ItemPosCountVec dest;
        uint8 bagSlot = 0;
        InventoryResult msg = CanStoreItem(item->GetBagSlot(), item->GetSlot(), dest, pNewItem, bagSlot, true);
        // ignore cast/combat time restriction (非装备直接换装不校验战斗限制，仅转换形态)
        if (msg == EQUIP_ERR_OK)
        {
            DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            return StoreItem(dest, pNewItem, true);
        }
    }
    else if (IsBankPos(pos))
    {
        ItemPosCountVec dest;
        uint8 bagSlot;
        InventoryResult msg = CanBankItem(item->GetBagSlot(), item->GetSlot(), dest, pNewItem, true, bagSlot);
        // ignore cast/combat time restriction (先移除旧物品)
        if (msg == EQUIP_ERR_OK)
        {
            DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            return BankItem(dest, pNewItem, true);
        }
    }
    else if (IsEquipmentPos(pos))
    {
        uint16 dest;
        InventoryResult msg = CanEquipItem(item->GetSlot(), dest, pNewItem, true, false);
        // ignore cast/combat time restriction (存入或装备新物品)
        if (msg == EQUIP_ERR_OK)
        {
            DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            pNewItem = EquipItem(dest, pNewItem, true);
            AutoUnequipOffhandIfNeed();
            return pNewItem;
        }
    }

    // fail (目标位置校验失败，中止转换)
    delete pNewItem;
    return nullptr;
}

uint32 Player::CalculateTalentsPoints() const
{
    uint32 talentPointsForLevel = GetLevel() < 10 ? 0 : GetLevel() - 9;
    return uint32(talentPointsForLevel * sWorld.getConfig(CONFIG_FLOAT_RATE_TALENT));
}

struct DoPlayerLearnSpell
{
    DoPlayerLearnSpell(Player& _player) : player(_player) {}
    void operator()(uint32 spell_id) { player.learnSpell(spell_id, false); }
    Player& player;
};

void Player::learnSpellHighRank(uint32 spellid)
{
    learnSpell(spellid, false);

    DoPlayerLearnSpell worker(*this);
    sSpellMgr.doForHighRanks(spellid, worker);
}

#ifdef ENABLE_PLAYERBOTS
void Player::learnClassLevelSpells(bool includeHighLevelQuestRewards)
{
    ChrClassesEntry const* clsEntry = sChrClassesStore.LookupEntry(getClass());
    if (!clsEntry)
        return;

    uint32 family = clsEntry->spellfamily;

    // special cases which aren't sourced from trainers and normally require quests to obtain - added here for convenience (机器人：补充需通过任务获取的特殊职业核心法术)
    ObjectMgr::QuestMap const& qTemplates = sObjectMgr.GetQuestTemplates();
    for (const auto& qTemplate : qTemplates)
    {
        Quest const* quest = qTemplate.second.get();
        if (!quest)
            continue;

        // only class quests player could do (仅限该玩家职业可接取的职业任务奖励法术)
        if (quest->GetRequiredClasses() == 0 || !SatisfyQuestClass(quest, false) || !SatisfyQuestRace(quest, false) || !SatisfyQuestLevel(quest, false))
            continue;

        // custom filter for scripting purposes (若为自定义高阶任务奖励法术按参数过滤)
        if (!includeHighLevelQuestRewards && quest->GetMinLevel() >= 60)
            continue;

        learnQuestRewardedSpells(quest);
    }

    // learn trainer spells (从训练师模板中学习符合等级的职业法术)
    for (uint32 id = 0; id < sCreatureStorage.GetMaxEntry(); ++id)
    {
        CreatureInfo const* co = sCreatureStorage.LookupEntry<CreatureInfo>(id);
        if (!co)
            continue;

        if (co->TrainerType != TRAINER_TYPE_CLASS)
            continue;

        if (co->TrainerType == TRAINER_TYPE_CLASS && co->TrainerClass != getClass())
            continue;

        uint32 trainerId = co->TrainerTemplateId;
        if (!trainerId)
            trainerId = co->Entry;

        TrainerSpellData const* trainer_spells = sObjectMgr.GetNpcTrainerTemplateSpells(trainerId);
        if (!trainer_spells)
            trainer_spells = sObjectMgr.GetNpcTrainerSpells(trainerId);

        if (!trainer_spells)
            continue;

        for (TrainerSpellMap::const_iterator itr = trainer_spells->spellList.begin(); itr != trainer_spells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            if (!tSpell)
                continue;

            uint32 reqLevel = 0;

            if (!tSpell->learnedSpell.empty())
            {

                for (auto learnedSpell : tSpell->learnedSpell)
                {

                    // skip wrong class/race skills (跳过不符职业或种族的技能)
                    if (!IsSpellFitByClassAndRace(learnedSpell, &reqLevel))
                        continue;

                    if (tSpell->conditionId && !sObjectMgr.IsConditionSatisfied(tSpell->conditionId, this, GetMap(), this, CONDITION_FROM_TRAINER))
                        continue;

                    // skip spells with first rank learned as talent (and all talents then also) (跳过天赋树法术)
                    uint32 first_rank = sSpellMgr.GetFirstSpellInChain(learnedSpell);
                    reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);
                    bool isValidTalent = GetTalentSpellCost(first_rank) && HasSpell(first_rank) && reqLevel <= GetLevel();

                    TrainerSpellState state = GetTrainerSpellState(tSpell, reqLevel);
                    if (state != TRAINER_SPELL_GREEN && !isValidTalent)
                        continue;

                    SpellEntry const* proto = sSpellTemplate.LookupEntry<SpellEntry>(learnedSpell);
                    if (!proto)
                        continue;

                    // fix activate state for non-stackable low rank (and find next spell for !active case) (修正不可叠加低阶法术的激活状态)
                    if (uint32 nextId = sSpellMgr.GetSpellBookSuccessorSpellId(proto->Id))
                    {
                        if (HasSpell(nextId))
                        {
                            // high rank already known so this must !active (高阶已学，低阶标记为非激活)
                            continue;
                        }
                    }

                    // skip other spell families (minus a few exceptions) (跳过非本职业法术族条目)
                    if (proto->SpellFamilyName != family)
                    {
                        SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(learnedSpell);
                        if (bounds.first == bounds.second)
                            continue;

                        SkillLineAbilityEntry const* skillInfo = bounds.first->second;
                        if (!skillInfo)
                            continue;

                        switch (skillInfo->skillId)
                        {
                            case SKILL_SUBTLETY:
                            case SKILL_BEAST_MASTERY:
                            case SKILL_SURVIVAL:
                            case SKILL_DEFENSE:
                            case SKILL_DUAL_WIELD:
                            case SKILL_FERAL_COMBAT:
                            case SKILL_PROTECTION:
                            case SKILL_PLATE_MAIL:
                            case SKILL_DEMONOLOGY:
                            case SKILL_ENHANCEMENT:
                            case SKILL_MAIL:
                            case SKILL_HOLY2:
                            case SKILL_LOCKPICKING: break;

                            default: continue;
                        }
                    }

                    // skip wrong class/race skills (跳过不符职业或种族的技能)
                    if (!IsSpellFitByClassAndRace(learnedSpell))
                        continue;

                    // skip broken spells (跳过缺少有效等级记录的异常法术)
                    if (!SpellMgr::IsSpellValid(proto, this, false))
                        continue;

                    if (learnedSpell)
                    {
                        bool learned = false;
                        for (int j = 0; j < 3; ++j)
                        {
                            if (proto->Effect[j] == SPELL_EFFECT_LEARN_SPELL)
                            {
                                uint32 learnedSpell2 = proto->EffectTriggerSpell[j];
                                learnSpell(learnedSpell2, false);
                                learned = true;
                            }
                        }

                        if (!learned)
                        {
                            learnSpell(learnedSpell, false);
                        }
                    }
                }
            }
            else
            {
                CastSpell(this, tSpell->spell, TRIGGERED_OLD_TRIGGERED);
            }
        }
    }
}
#endif

void Player::_LoadSkills(std::unique_ptr<QueryResult> queryResult)
{
    //                                                           0      1      2
    // SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT skill, value, max FROM character_skills WHERE guid = '%u'", GUID_LOPART(m_guid));

    uint32 count = 0;
    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            uint16 skillId  = fields[0].GetUInt16();
            uint16 value    = fields[1].GetUInt16();
            uint16 max      = fields[2].GetUInt16();

            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skillId);
            if (!pSkill)
            {
                sLog.outError("Character %u has skill %u that does not exist.", GetGUIDLow(), skillId);
                continue;
            }

            // set fixed skill ranges (某些特殊技能拥有固定不变的上下限)
            switch (GetSkillRangeType(pSkill, false))
            {
                case SKILL_RANGE_LANGUAGE:                  // 300..300 (骑术/语言/武器大师等固定 300/300)
                    value = max = 300;
                    break;
                case SKILL_RANGE_MONO:                      // 1..1, grey monolite bar (专精或纯被动技能固定 1/1)
                {
                    SkillRaceClassInfoEntry const* entry = GetSkillInfo(skillId);
                    if (entry && entry->flags & SKILL_FLAG_MAXIMIZED)
                        value = max = GetSkillMaxForLevel();
                    else
                        value = max = 1;
                    break;
                }
                case SKILL_RANGE_LEVEL:
                    max = GetSkillMaxForLevel();
                    break;
                default:
                    break;
            }

            if (value == 0)
            {
                sLog.outError("Character %u has skill %u with value 0. Will be deleted.", GetGUIDLow(), skillId);
                CharacterDatabase.PExecute("DELETE FROM character_skills WHERE guid = '%u' AND skill = '%u' ", GetGUIDLow(), skillId);
                continue;
            }

            // Deduct current skill step from skill max value using data from dbc (根据 DBC 的 SkillTiers 反推当前技能所处阶位 step)
            // TODO: Maybe save step to db in the future? (待办：未来可考虑直接将技能阶位存入数据库)
            uint16 step = 0;

            uint32 raceMask = getRaceMask();
            uint32 classMask = getClassMask();
            auto bounds = sSpellMgr.GetSkillRaceClassInfoMapBounds(skillId);

            for (auto itr = bounds.first; (itr != bounds.second && !step); ++itr)
            {
                SkillRaceClassInfoEntry const* entry = itr->second;

                if (!(entry->raceMask & raceMask))
                    continue;

                if (!(entry->classMask & classMask))
                    continue;

                if (entry->flags & SKILL_FLAG_MAXIMIZED)
                {
                    if (entry->flags & SKILL_FLAG_DISPLAY_AS_MONO)
                        max = GetSkillMaxForLevel();
                    
                    value = max;
                }

                if (SkillTiersEntry const* steps = sSkillTiersStore.LookupEntry(entry->skillTierId))
                {
                    for (uint16 i = 0; (i < MAX_SKILL_STEP && !step && steps->maxSkillValue[i]); ++i)
                    {
                        if (steps->maxSkillValue[i] == max)
                            step = i + 1;
                    }
                }
            }

            SetUInt32Value(PLAYER_SKILL_INDEX(count), MAKE_PAIR32(skillId, step));
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), MAKE_SKILL_VALUE(value, max));
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);

            mSkillStatus.insert(SkillStatusMap::value_type(skillId, SkillStatusData(count, SKILL_UNCHANGED)));

            ++count;

            if (count >= PLAYER_MAX_SKILLS)                 // client limit (技能总数不得超过客户端上限 PLAYER_MAX_SKILLS)
            {
                sLog.outError("Character %u has more than %u skills.", GetGUIDLow(), PLAYER_MAX_SKILLS);
                break;
            }
        }
        while (queryResult->NextRow());
    }

    for (; count < PLAYER_MAX_SKILLS; ++count)
    {
        SetUInt32Value(PLAYER_SKILL_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);
    }
}

InventoryResult Player::CanEquipUniqueItem(Item* pItem, uint8 eslot) const
{
    ItemPrototype const* pProto = pItem->GetProto();

    // proto based limitations (按物品模板的唯一持有/唯一装备限制校验)
    if (InventoryResult res = CanEquipUniqueItem(pProto, eslot))
        return res;

    // check unique-equipped on gems (检查已镶嵌珠宝的唯一装备限制)
    for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + 3; ++enchant_slot)
    {
        uint32 enchant_id = pItem->GetEnchantmentId(EnchantmentSlot(enchant_slot));
        if (!enchant_id)
            continue;
        SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!enchantEntry)
            continue;

        ItemPrototype const* pGem = ObjectMgr::GetItemPrototype(enchantEntry->GemID);
        if (!pGem)
            continue;

        if (InventoryResult res = CanEquipUniqueItem(pGem, eslot))
            return res;
    }

    return EQUIP_ERR_OK;
}

InventoryResult Player::CanEquipUniqueItem(ItemPrototype const* itemProto, uint8 except_slot) const
{
    // check unique-equipped on item (检查物品本身的唯一装备限制)
    if (itemProto->Flags & ITEM_FLAG_UNIQUE_EQUIPPABLE)
    {
        // there is an equip limit on this item (校验装备栏及身上已装备数量是否超限)
        if (HasItemOrGemWithIdEquipped(itemProto->ItemId, 1, except_slot))
            return EQUIP_ERR_ITEM_UNIQUE_EQUIPABLE;
    }

    return EQUIP_ERR_OK;
}

/// 结算玩家坠落高度与安全坠落减免，计算并施加坠落伤害
void Player::HandleFall(MovementInfo const& movementInfo)
{
    // calculate total z distance of the fall (计算坠落总高度差)
    Position const& position = movementInfo.GetPos();
    float z_diff = m_lastFallZ - position.z;
    DEBUG_LOG("zDiff = %f", z_diff);

    // Players with low fall distance, Feather Fall or physical immunity (charges used) are ignored (短距离坠落、缓落或物理免疫时不受坠落伤害)
    // 14.57 can be calculated by resolving damageperc formula below to 0 (14.57 码为坠落伤害公式的零点阈值)
    if (z_diff >= 14.57f && !IsDead() && !IsGameMaster() && !HasMovementFlag(MOVEFLAG_ONTRANSPORT) &&
            !HasAuraType(SPELL_AURA_HOVER) && !HasAuraType(SPELL_AURA_FEATHER_FALL) &&
            !IsFreeFlying() && !IsImmuneToDamage(SPELL_SCHOOL_MASK_NORMAL))
    {
        // Safe fall, fall height reduction (应用安全坠落/羽落术减伤高度折抵)
        int32 safe_fall = GetTotalAuraModifier(SPELL_AURA_SAFE_FALL);

        float damageperc = 0.018f * (z_diff - safe_fall) - 0.2426f;

        if (damageperc > 0)
        {
            uint32 damage = (uint32)(damageperc * GetMaxHealth() * sWorld.getConfig(CONFIG_FLOAT_RATE_DAMAGE_FALL));

            float height = position.z;
            UpdateAllowedPositionZ(position.x, position.y, height);

            if (damage > 0)
            {
                // Prevent fall damage from being more than the player maximum health (限制单次坠落伤害不超过玩家最大生命值)
                if (damage > GetMaxHealth())
                    damage = GetMaxHealth();

                // Gust of Wind (防止特定击退法术等触发无谓记录)
                if (GetDummyAura(43621))
                    damage = GetMaxHealth() / 2;

                EnvironmentalDamage(DAMAGE_FALL, damage);
            }

            // Z given by moveinfo, LastZ, FallTime, WaterZ, MapZ, Damage, Safefall reduction (记录坠落伤害调试日志)
            DEBUG_LOG("FALLDAMAGE z=%f sz=%f pZ=%f FallTime=%d mZ=%f damage=%d SF=%d", position.z, height, GetPositionZ(), movementInfo.GetFallTime(), height, damage, safe_fall);
        }
    }
}

/// 学习指定天赋树节点的下一等级并校验前提天赋与已投入点数
void Player::LearnTalent(uint32 talentId, uint32 talentRank)
{
    uint32 CurTalentPoints = GetFreeTalentPoints();

    if (CurTalentPoints == 0)
        return;

    if (talentRank >= MAX_TALENT_RANK)
        return;

    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

    if (!talentInfo)
        return;

    TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);

    if (!talentTabInfo)
        return;

    // prevent learn talent for different class (cheating) (禁止学习其他职业的天赋，防作弊)
    if ((getClassMask() & talentTabInfo->ClassMask) == 0)
        return;

    // find current max talent rank (查找当前已学的该天赋最高等级)
    uint32 curtalent_maxrank = 0;
    for (int32 k = MAX_TALENT_RANK - 1; k > -1; --k)
    {
        if (talentInfo->RankID[k] && HasSpell(talentInfo->RankID[k]))
        {
            curtalent_maxrank = k + 1;
            break;
        }
    }

    // we already have same or higher talent rank learned (已学会同级或更高等级天赋则拒绝)
    if (curtalent_maxrank >= (talentRank + 1))
        return;

    // check if we have enough talent points (校验剩余可用天赋点数)
    if (CurTalentPoints < (talentRank - curtalent_maxrank + 1))
        return;

    // Check if it requires another talent (校验前置依赖天赋等级)
    if (talentInfo->DependsOn > 0)
    {
        if (TalentEntry const* depTalentInfo = sTalentStore.LookupEntry(talentInfo->DependsOn))
        {
            bool hasEnoughRank = false;
            for (int i = talentInfo->DependsOnRank; i < MAX_TALENT_RANK; ++i)
            {
                if (depTalentInfo->RankID[i] != 0)
                    if (HasSpell(depTalentInfo->RankID[i]))
                        hasEnoughRank = true;
            }

            if (!hasEnoughRank)
                return;
        }
    }

    // Check if it requires spell (校验是否已学会前置依赖法术)
    if (talentInfo->DependsOnSpell && !HasSpell(talentInfo->DependsOnSpell))
        return;

    // Find out how many points we have in this field (统计当前天赋页已投入的总点数)
    uint32 spentPoints = 0;

    uint32 tTab = talentInfo->TalentTab;
    if (talentInfo->Row > 0)
    {
        unsigned int numRows = sTalentStore.GetNumRows();
        for (unsigned int i = 0; i < numRows; ++i)          // Loop through all talents. (遍历该职业当前天赋树页的所有天赋)
        {
            // Someday, someone needs to revamp (注：此处按天赋树全部遍历计算已投点数，)
            const TalentEntry* tmpTalent = sTalentStore.LookupEntry(i);
            if (tmpTalent)                                  // the way talents are tracked (日后可优化天赋点数追踪方式)
            {
                if (tmpTalent->TalentTab == tTab)
                {
                    for (int j = 0; j < MAX_TALENT_RANK; ++j)
                    {
                        if (tmpTalent->RankID[j] != 0)
                        {
                            if (HasSpell(tmpTalent->RankID[j]))
                            {
                                spentPoints += j + 1;
                            }
                        }
                    }
                }
            }
        }
    }

    // not have required min points spent in talent tree (当前天赋页投入点数未达到该行所需门槛)
    if (spentPoints < (talentInfo->Row * MAX_TALENT_RANK))
        return;

    // spell not set in talent.dbc (Talent.dbc 中未配置对应法术)
    uint32 spellid = talentInfo->RankID[talentRank];
    if (spellid == 0)
    {
        sLog.outError("Talent.dbc have for talent: %u Rank: %u spell id = 0", talentId, talentRank);
        return;
    }

    // already known (目标天赋法术已学会)
    if (HasSpell(spellid))
        return;

    // learn! (other talent ranks will unlearned at learning) (学习新等级天赋，低等级天赋将自动遗忘)
    learnSpell(spellid, false, true);
    DETAIL_LOG("TalentID: %u Rank: %u Spell: %u\n", talentId, talentRank, spellid);
}

void Player::UpdateFallInformationIfNeed(MovementInfo const& minfo, uint16 opcode)
{
    if (m_lastFallTime >= minfo.GetFallTime() || m_lastFallZ <= minfo.GetPos().z || minfo.HasMovementFlag(MOVEFLAG_FLYING) || opcode == MSG_MOVE_FALL_LAND)
        SetFallInformation(minfo.GetFallTime(), minfo.GetPos().z);
}

void Player::UnsummonPetTemporaryIfAny()
{
    Pet* pet = GetPet();
    if (!pet)
        return;

    if (!m_temporaryUnsummonedPetNumber && pet->isControlled() && !pet->isTemporarySummoned() && pet->IsAlive())
        m_temporaryUnsummonedPetNumber = pet->GetCharmInfo()->GetPetNumber();

    pet->Unsummon(PET_SAVE_AS_CURRENT, this);
}

void Player::UnsummonPetIfAny()
{
    Pet* pet = GetPet();
    if (!pet)
        return;

    pet->Unsummon(PET_SAVE_NOT_IN_SLOT, this);
}

bool Player::IsPetNeedBeTemporaryUnsummoned(Pet* pet) const
{
    if (!IsInWorld() || !IsAlive())
        return true;

    if (IsMounted())
    {
        if (pet)
        {
            if (!pet->IsAlive())
                return false;

            if (pet->isControlled() && (!(pet->isTemporarySummoned() || InArena()) || sWorld.getConfig(CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT)))
                return true;
        }
        else if (!InArena())
            return true;

        if (HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED))
            return true;
    }

    if (HasAuraType(SPELL_AURA_FLY)) // if not IsMounted and has aura, means we are removing flying mount (若未上马但存在飞行光环，视为正在解除飞行坐骑)
        return true;

    if (IsTaxiFlying())
        return true;

    return false;
}

void Player::ResummonPetTemporaryUnSummonedIfAny()
{
    if (!m_temporaryUnsummonedPetNumber)
        return;

    // not resummon in not appropriate state (死亡、飞行、骑乘或战场准备期等不适合时暂不召回宠物)
    if (IsPetNeedBeTemporaryUnsummoned(nullptr))
        return;

    if (GetPetGuid())
        return;

    Pet* NewPet = new Pet;
    if (!NewPet->LoadPetFromDB(this, NewPet->GetPetSpawnPosition(this), 0, m_temporaryUnsummonedPetNumber, true))
        delete NewPet;

    m_temporaryUnsummonedPetNumber = 0;
}

void Player::_SaveBGData()
{
    // nothing save (战场数据无修改，跳过保存)
    if (!m_bgData.m_needSave)
        return;

    static SqlStatementID delBGData ;
    static SqlStatementID insBGData ;

    SqlStatement stmt =  CharacterDatabase.CreateStatement(delBGData, "DELETE FROM character_battleground_data WHERE guid = ?");

    stmt.PExecute(GetGUIDLow());

    if (m_bgData.bgInstanceID)
    {
        stmt = CharacterDatabase.CreateStatement(insBGData, "INSERT INTO character_battleground_data VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        /* guid, bgInstanceID, bgTeam, x, y, z, o, map (保存战场实例与入场前位置) */
        stmt.addUInt32(GetGUIDLow());
        stmt.addUInt32(m_bgData.bgInstanceID);
        stmt.addUInt32(uint32(m_bgData.bgTeam));
        stmt.addFloat(m_bgData.joinPos.coord_x);
        stmt.addFloat(m_bgData.joinPos.coord_y);
        stmt.addFloat(m_bgData.joinPos.coord_z);
        stmt.addFloat(m_bgData.joinPos.orientation);
        stmt.addUInt32(m_bgData.joinPos.mapid);

        stmt.Execute();
    }

    m_bgData.m_needSave = false;
}

void Player::RemoveAtLoginFlag(AtLoginFlags f, bool in_db_also /*= false*/)
{
    m_atLoginFlags &= ~f;

    if (in_db_also)
        CharacterDatabase.PExecute("UPDATE characters set at_login = at_login & ~ %u WHERE guid ='%u'", uint32(f), GetGUIDLow());
}

void Player::SendClearCooldown(uint32 spell_id, Unit* target) const
{
    WorldPacket data(SMSG_CLEAR_COOLDOWN, 4 + 8);
    data << uint32(spell_id);
    data << target->GetObjectGuid();
    SendDirectMessage(data);
}

bool Player::HasMovementFlag(MovementFlags f) const
{
    return m_movementInfo.HasMovementFlag(f);
}

void Player::SetHomebindToLocation(WorldLocation const& loc, uint32 area_id)
{
    m_homebindMapId = loc.mapid;
    m_homebindAreaId = area_id;
    m_homebindX = loc.coord_x;
    m_homebindY = loc.coord_y;
    m_homebindZ = loc.coord_z;

    // update sql homebind (将新炉石绑定点写入数据库)
    CharacterDatabase.PExecute("UPDATE character_homebind SET map = '%u', zone = '%u', position_x = '%f', position_y = '%f', position_z = '%f' WHERE guid = '%u'",
                               m_homebindMapId, m_homebindAreaId, m_homebindX, m_homebindY, m_homebindZ, GetGUIDLow());
}

Object* Player::GetObjectByTypeMask(ObjectGuid guid, TypeMask typemask)
{
    switch (guid.GetHigh())
    {
        case HIGHGUID_ITEM:
            if (typemask & TYPEMASK_ITEM)
                return GetItemByGuid(guid);
            break;
        case HIGHGUID_PLAYER:
            if (GetObjectGuid() == guid)
                return this;
            if ((typemask & TYPEMASK_PLAYER) && IsInWorld())
                return ObjectAccessor::FindPlayer(guid);
            break;
        case HIGHGUID_TRANSPORT:
        case HIGHGUID_GAMEOBJECT:
            if ((typemask & TYPEMASK_GAMEOBJECT) && IsInWorld())
                return GetMap()->GetGameObject(guid);
            break;
        case HIGHGUID_UNIT:
            if ((typemask & TYPEMASK_UNIT) && IsInWorld())
                return GetMap()->GetCreature(guid);
            break;
        case HIGHGUID_PET:
            if ((typemask & TYPEMASK_UNIT) && IsInWorld())
                return GetMap()->GetPet(guid);
            break;
        case HIGHGUID_DYNAMICOBJECT:
            if ((typemask & TYPEMASK_DYNAMICOBJECT) && IsInWorld())
                return GetMap()->GetDynamicObject(guid);
            break;
        case HIGHGUID_CORPSE:
        case HIGHGUID_MO_TRANSPORT:
        case HIGHGUID_GROUP:
        default:
            break;
    }

    return nullptr;
}

void Player::SetRestType(RestType n_r_type, uint32 areaTriggerId /*= 0*/)
{
    m_restType = n_r_type;
    if (m_restType == REST_TYPE_NO)
    {
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

        // Set player to FFA PVP when not in rested environment. (在 FFA 服务器中离开休息区时开启自由混战 PvP 标志)
        if (sWorld.IsFFAPvPRealm())
            SetPvPFreeForAll(true);
    }
    else
    {
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

        inn_trigger_id = areaTriggerId;
        time_inn_enter = time(nullptr);

        if (sWorld.IsFFAPvPRealm())
            SetPvPFreeForAll(false);
    }
}

void Player::SendDuelCountdown(uint32 counter) const
{
    WorldPacket data(SMSG_DUEL_COUNTDOWN, 4);
    data << uint32(counter);                                // seconds (倒计时秒数)
    GetSession()->SendPacket(data);
}

bool Player::IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const
{
    switch (spellInfo->Effect[index])
    {
        case SPELL_EFFECT_ATTACK_ME:
            return true;
        default:
            break;
    }
    switch (spellInfo->EffectApplyAuraName[index])
    {
        case SPELL_AURA_MOD_TAUNT:
            return true;
        default:
            break;
    }
    return Unit::IsImmuneToSpellEffect(spellInfo, index, castOnSelf);
}

/// 检查玩家进入区域触发器对应副本的准入条件（等级、钥匙、任务、次数与人数上限）
AreaLockStatus Player::GetAreaTriggerLockStatus(AreaTrigger const* at, uint32& miscRequirement, bool forceAllChecks)
{
    miscRequirement = 0;

    if (!at)
        return AREA_LOCKSTATUS_UNKNOWN_ERROR;

    MapEntry const* mapEntry = sMapStore.LookupEntry(at->target_mapId);
    if (!mapEntry)
        return AREA_LOCKSTATUS_UNKNOWN_ERROR;

    bool isRegularTargetMap = !mapEntry->IsNonRaidDungeon() || GetDifficulty() == REGULAR_DIFFICULTY;

    // Heroic allowed only on TBC instances (英雄难度仅适用于 TBC 资料片副本)
    if (!isRegularTargetMap && mapEntry->IsDungeon() && mapEntry->Expansion() != 1)
        return AREA_LOCKSTATUS_MISSING_DIFFICULTY;

    // Expansion requirement (检查玩家账号资料片权限)
    if (GetSession()->GetExpansion() < mapEntry->Expansion())
    {
        miscRequirement = mapEntry->Expansion();
        return AREA_LOCKSTATUS_INSUFFICIENT_EXPANSION;
    }

    // Gamemaster can always enter (GM 模式下忽略常规门禁条件直接放行)
    if (IsGameMaster())
        return AREA_LOCKSTATUS_OK;

    // Level Requirements (检查进入副本的最低/最高等级限制)
    if (GetLevel() < at->requiredLevel && !sWorld.getConfig(CONFIG_BOOL_INSTANCE_IGNORE_LEVEL))
    {
        miscRequirement = at->requiredLevel;
        return AREA_LOCKSTATUS_TOO_LOW_LEVEL;
    }
    if (!isRegularTargetMap && !sWorld.getConfig(CONFIG_BOOL_INSTANCE_IGNORE_LEVEL) && GetLevel() < uint32(maxLevelForExpansion[mapEntry->Expansion()]))
    {
        miscRequirement = maxLevelForExpansion[mapEntry->Expansion()];
        return AREA_LOCKSTATUS_TOO_LOW_LEVEL;
    }

    // Raid Requirements (检查是否必须处于团队中才能进入)
    if (mapEntry->IsRaid() && !sWorld.getConfig(CONFIG_BOOL_INSTANCE_IGNORE_RAID))
        if (!GetGroup() || !GetGroup()->IsRaidGroup())
            return AREA_LOCKSTATUS_RAID_LOCKED;

#ifdef ENABLE_PLAYERBOTS
    if (!isRealPlayer() && GetPlayerbotAI() && GetPlayerbotAI()->CanEnterArea(at))
        return AREA_LOCKSTATUS_OK;
#endif

    // Item Requirements: must have requiredItem OR requiredItem2, report the first one that's missing (校验钥匙/门任务物品需求)
    if (at->requiredItem)
    {
        if (!HasItemCount(at->requiredItem, 1) &&
                (!at->requiredItem2 || !HasItemCount(at->requiredItem2, 1)))
        {
            miscRequirement = at->requiredItem;
            return AREA_LOCKSTATUS_MISSING_ITEM;
        }
    }
    else if (at->requiredItem2 && !HasItemCount(at->requiredItem2, 1))
    {
        miscRequirement = at->requiredItem2;
        return AREA_LOCKSTATUS_MISSING_ITEM;
    }
    // Heroic item requirements (校验英雄副本所需的声望钥匙)
    if (!isRegularTargetMap && at->heroicKey)
    {
        if (!HasItemCount(at->heroicKey, 1) && (!at->heroicKey2 || !HasItemCount(at->heroicKey2, 1)))
        {
            miscRequirement = at->heroicKey;
            return AREA_LOCKSTATUS_MISSING_ITEM;
        }
    }
    else if (!isRegularTargetMap && at->heroicKey2 && !HasItemCount(at->heroicKey2, 1))
    {
        miscRequirement = at->heroicKey2;
        return AREA_LOCKSTATUS_MISSING_ITEM;
    }

    // Quest Requirements (校验前置开门任务完成状态)
    if (isRegularTargetMap && at->requiredQuest && !GetQuestRewardStatus(at->requiredQuest))
    {
        miscRequirement = at->requiredQuest;
        return AREA_LOCKSTATUS_QUEST_NOT_COMPLETED;
    }
    if (!isRegularTargetMap && at->requiredQuestHeroic && !GetQuestRewardStatus(at->requiredQuestHeroic))
    {
        miscRequirement = at->requiredQuestHeroic;
        return AREA_LOCKSTATUS_QUEST_NOT_COMPLETED;
    }

    // If the map is not created, assume it is possible to enter it. (若目标副本地图尚未创建，默认允许进入)
    DungeonPersistentState* state = GetBoundInstanceSaveForSelfOrGroup(at->target_mapId);
    Map* map = sMapMgr.FindMap(at->target_mapId, state ? state->GetInstanceId() : 0);

    // check if this account try to abuse reseting instance (校验每小时最多进入 5 个新副本的防刷机制)
    if (mapEntry->IsNonRaidDungeon() && ((map && map->GetDifficulty() == DUNGEON_DIFFICULTY_NORMAL) || (GetDifficulty() == DUNGEON_DIFFICULTY_NORMAL)))
    {
        if (!CanEnterNewInstance(state ? state->GetInstanceId() : 0))
            return AREA_LOCKSTATUS_TOO_MANY_INSTANCE;
    }

    // Map's state check (检查目标副本实例当前是否允许进入)
    if (map && map->IsDungeon())
    {
        // cannot enter if the instance is full (player cap), GMs don't count, must not check when teleporting around the same map (检查副本人数上限，GM 不计入人数且同图传送不校验)
        if (GetMapId() != at->target_mapId || forceAllChecks)
            if (((DungeonMap*)map)->GetPlayersCountExceptGMs() >= ((DungeonMap*)map)->GetMaxPlayers())
                return AREA_LOCKSTATUS_INSTANCE_IS_FULL;

        // In Combat check (首领战斗进行中禁止进入)
        if (map && map->GetInstanceData() && map->GetInstanceData()->IsEncounterInProgress())
            return AREA_LOCKSTATUS_ZONE_IN_COMBAT;

        // Bind Checks (副本进度绑定兼容性与重置校验)
        InstancePlayerBind* pBind = GetBoundInstance(at->target_mapId, GetDifficulty());
        if (pBind && pBind->perm && pBind->state != state)
            return AREA_LOCKSTATUS_HAS_BIND;
        if (pBind && pBind->perm && pBind->state != map->GetPersistentState())
            return AREA_LOCKSTATUS_HAS_BIND;
    }

    return AREA_LOCKSTATUS_OK;
};

float Player::ComputeRest(time_t timePassed, bool offline /*= false*/, bool inRestPlace /*= false*/) const
{
    // Every 8h in resting zone we gain a bubble (在休息区每 8 小时获得 1 格双倍精力，共 20 格)
    // A bubble is 5% of the total xp so there is 20 bubbles (每格为升级所需总经验的 5%)
    // So we gain (total XP/20 every 8h) (8h = 288800 sec) (即每 8 小时获得 总经验/20)
    // (TotalXP/20)/28800; simplified to (TotalXP/576000) per second (化简为每秒 TotalXP / 576000)
    // Client automatically double the value sent so we have to divide it by 2 (因客户端默认显示双倍值，服务端计算结果需除以 2)
    // So final formula (TotalXP/1152000) (最终公式为每秒 TotalXP / 1152000)
    float bonus = timePassed * (GetUInt32Value(PLAYER_NEXT_LEVEL_XP) / 1152000.0f); // Get the gained rest xp for given second (计算给定时间段内累积的休息经验)
    if (!offline)
        bonus *= sWorld.getConfig(CONFIG_FLOAT_RATE_REST_INGAME);                   // Apply the custom setting (乘以服务器自定义休息经验倍率)
    else
    {
        if (inRestPlace)
            bonus *= sWorld.getConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY);
        else
            bonus *= sWorld.getConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS) / 4.0f; // bonus is reduced by 4 when not in rest place (离线不在旅店或主城休息区时，获取速率降低为 1/4)
    }
    return bonus;
}

// player is interacting so we have to remove non authorized aura (玩家正在进行交互操作，移除不允许的隐蔽类光环)
void Player::DoInteraction()
{
    // remove some aura like stealth aura (交互时移除潜行等非授权光环)
    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_INTERACTING);
    SendForcedObjectUpdate();
}

void Player::DoLoot()
{
    // remove some aura like stealth aura (拾取时移除潜行等非授权光环)
    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_LOOTING);
    SendForcedObjectUpdate();
}

void Player::SendLootError(ObjectGuid guid, LootError error) const
{
    WorldPacket data(SMSG_LOOT_RESPONSE, 10);
    data << uint64(guid);
    data << uint8(0);
    data << uint8(error);
    SendDirectMessage(data);
}

void Player::SetDeathPrevention(bool enable)
{
    if (enable)
        m_ExtraFlags |= PLAYER_EXTRA_GM_UNKILLABLE;
    else
        m_ExtraFlags &= ~PLAYER_EXTRA_GM_UNKILLABLE;
}

bool Player::IsPreventingDeath() const
{
    return m_ExtraFlags & PLAYER_EXTRA_GM_UNKILLABLE;
}

/// 触发并发送玩家的全局公共冷却（GCD）
void Player::AddGCD(SpellEntry const& spellEntry, uint32 /*forcedDuration = 0*/, bool updateClient /*= false*/)
{
    int32 gcdDuration = spellEntry.StartRecoveryTime;
    if (!spellEntry.StartRecoveryCategory && !gcdDuration)
        return;

    // gcd modifier auras applied only to self spells and only player have mods for this (应用减少公共冷却的法术修正与光环)
    ApplySpellMod(spellEntry.Id, SPELLMOD_GLOBAL_COOLDOWN, gcdDuration);

    // apply haste rating (应用法术/近战急速等级对 GCD 的缩减)
    if (spellEntry.StartRecoveryCategory == 133 && gcdDuration == 1500 &&
        spellEntry.DmgClass != SPELL_DAMAGE_CLASS_MELEE && spellEntry.DmgClass != SPELL_DAMAGE_CLASS_RANGED &&
        !spellEntry.HasAttribute(SPELL_ATTR_USES_RANGED_SLOT) && !spellEntry.HasAttribute(SPELL_ATTR_IS_ABILITY))
    {
        gcdDuration = int32(float(gcdDuration) * GetFloatValue(UNIT_MOD_CAST_SPEED));
        gcdDuration = std::max(gcdDuration, 1000);
        gcdDuration = std::min(gcdDuration, 1500);
    }

    if (!gcdDuration)
        return;

    // TODO: Remove this once spells are queuable and GCD is checked on execute (待办：若实现法术队列并统一在执行时检查 GCD，可移除此偏移)
    if (uint32 latency = GetSession()->GetLatency())
    {
        if (latency > 300)
            gcdDuration -= 300;
        else
            gcdDuration -= latency;

        gcdDuration -= GetMap()->GetCurrentDiff() > 200 ? 200 : GetMap()->GetCurrentDiff();
    }

    WorldObject::AddGCD(spellEntry, gcdDuration);

    if (!updateClient)
        return;

    // send to client (向客户端发送全局公共冷却时间)
    WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 1 + 4);
    data << GetObjectGuid();
    data << uint8(SPELL_COOLDOWN_FLAG_INCLUDE_GCD);
    data << uint32(spellEntry.Id);
    data << uint32(0);
    GetSession()->SendPacket(data);
}

/// 为指定法术及关联类别添加冷却时间并通知客户端
void Player::AddCooldown(SpellEntry const& spellEntry, ItemPrototype const* itemProto /*= nullptr*/, bool permanent /*= false*/, uint32 forcedDuration /*= 0*/, bool ignoreCat /*= false*/)
{
    uint32 spellCategory = spellEntry.Category;
    uint32 recTime = spellEntry.RecoveryTime; // int because of spellmod calculations (使用有符号整型以便进行法术修正减免运算)
    uint32 categoryRecTime = ignoreCat ? 0 : spellEntry.CategoryRecoveryTime; // int because of spellmod calculations (使用有符号整型防止负向溢出)
    uint32 itemId = 0;

    auto pickCooldowns = [&](ItemPrototype const* itemProto)
    {
        for (const auto& Spell : itemProto->Spells)
        {
            if (Spell.SpellId == spellEntry.Id)
            {
                if (Spell.SpellCategory)
                    spellCategory = Spell.SpellCategory;
                if (Spell.SpellCooldown >= 0)
                    recTime = Spell.SpellCooldown;
                if (Spell.SpellCategoryCooldown >= 0)
                    categoryRecTime = Spell.SpellCategoryCooldown;
                itemId = itemProto->ItemId;
                break;
            }
        }
    };

    if (itemProto)
        pickCooldowns(itemProto);

    bool haveToSendEvent = false;
    bool wasPermanent = false;
    uint32 oldItemId = 0;
    auto cdDataItr = m_cooldownMap.FindBySpellId(spellEntry.Id);
    if (cdDataItr != m_cooldownMap.end())
    {
        auto& cdData = cdDataItr->second;
        if (!cdData->IsPermanent() && (!cdData->IsSpellCDExpired(GetMap()->GetCurrentClockTime()) || !cdData->IsCatCDExpired(GetMap()->GetCurrentClockTime())))
        {
            sLog.outError("Player::AddCooldown> Spell(%u) try to add and already existing cooldown %u?", spellEntry.Id, forcedDuration);
            return;
        }
        wasPermanent = cdData->IsPermanent();
        oldItemId = cdData->GetItemId();
        m_cooldownMap.erase(cdDataItr);
        haveToSendEvent = true;
    }

    if (permanent)
    {
        m_cooldownMap.AddCooldown(GetMap()->GetCurrentClockTime(), spellEntry.Id, recTime, spellCategory, categoryRecTime, itemId, true);
        return;
    }

    if (forcedDuration)
        recTime = forcedDuration;
    else
    {
        // shoot spells used equipped item cooldown values already assigned in GetAttackTime(RANGED_ATTACK) (魔杖射击等法术沿用远程武器攻击间隔，)
        // prevent 0 cooldowns set by another way (防止被其他途径覆盖为 0)
        if (spellEntry.HasAttribute(SPELL_ATTR_USES_RANGED_SLOT) && !spellEntry.HasAttribute(SPELL_ATTR_EX2_DO_NOT_RESET_COMBAT_TIMERS))
            recTime += GetFloatValue(UNIT_FIELD_RANGEDATTACKTIME);
    }

    // blizzlike code for choosing which is recTime > categoryRecTime after spellmod application (按官服规则在应用法术修正后比较单法术与类别冷却)
    // We use signed intermediates to prevent uint32 underflow when a flat reduction exceeds the CD value (使用有符号中间量防止固定减免超出原冷却导致无符号下溢)
    if (recTime)
    {
        int32 signedRecTime = static_cast<int32>(recTime);
        ApplySpellMod(spellEntry.Id, SPELLMOD_COOLDOWN, signedRecTime);
        recTime = signedRecTime > 0 ? static_cast<uint32>(signedRecTime) : 0;
    }
    if (spellCategory && categoryRecTime)
    {
        int32 signedCatRecTime = static_cast<int32>(categoryRecTime);
        ApplySpellMod(spellEntry.Id, SPELLMOD_COOLDOWN, signedCatRecTime);
        categoryRecTime = signedCatRecTime > 0 ? static_cast<uint32>(signedCatRecTime) : 0;
    }

    if (recTime || categoryRecTime || wasPermanent)
    {
        if (wasPermanent && oldItemId && !itemProto)
            if (ItemPrototype const* itemProto = sObjectMgr.GetItemPrototype(oldItemId))
                pickCooldowns(itemProto);

        // ready to add the cooldown (就绪，向冷却表写入单法术冷却与类别冷却)
        if (recTime || categoryRecTime) // only send event if was permanent but no cds (仅在需同步客户端的冷却场景发送事件)
            m_cooldownMap.AddCooldown(GetMap()->GetCurrentClockTime(), spellEntry.Id, recTime, spellCategory, categoryRecTime, itemId);

        // after some aura fade or potion activation we have to send cooldown event to start cd client side (药水生效或光环结束后通知客户端开始倒计时)
        if (haveToSendEvent)
        {
            // client keeps track of category cd by original spellId (客户端按原始法术 ID 追踪所属类别冷却)
            if (spellCategory && spellEntry.HasAttribute(SPELL_ATTR_COOLDOWN_ON_EVENT))
            {
                auto itr = m_cooldownMap.FindByCategory(spellCategory);
                if (itr != m_cooldownMap.end() && (*itr).second->GetSpellId() != spellEntry.Id)
                {
                    WorldPacket data(SMSG_COOLDOWN_EVENT, (4 + 8));
                    data << uint32((*itr).second->GetSpellId());
                    data << GetObjectGuid();
                    SendDirectMessage(data);
                }
            }

            // Send activate cooldown timer (possible 0) at client side (向客户端下发冷却计时包)
            WorldPacket data(SMSG_COOLDOWN_EVENT, (4 + 8));
            data << uint32(spellEntry.Id);
            data << GetObjectGuid();
            SendDirectMessage(data);
            sLog.outDebug("Sending SMSG_COOLDOWN_EVENT with spell id = %u", spellEntry.Id);
        }
    }
}

void Player::RemoveSpellCooldown(SpellEntry const& spellEntry, bool updateClient /*= true*/)
{
    m_cooldownMap.RemoveBySpellId(spellEntry.Id);

    if (updateClient)
        SendClearCooldown(spellEntry.Id, this);
}

void Player::RemoveSpellCategoryCooldown(uint32 category, bool updateClient /*= true*/)
{
    auto spellItr = m_cooldownMap.FindByCategory(category);
    if (spellItr == m_cooldownMap.end())
        return;

    auto& cdData = spellItr->second;
    if (updateClient)
        SendClearCooldown(cdData->GetSpellId(), this);

    m_cooldownMap.erase(spellItr);
}

void Player::RemoveAllCooldowns(bool sendOnly /*= false*/)
{
    std::set<uint32> spellsSent;
    // not reset gcd (usually small enough) (不重置极短的公共冷却)

    // reset normal cd (清除非永久常规法术冷却)
    for (auto& cdItr : m_cooldownMap)
    {
        auto& cdData = cdItr.second;
        if (!cdData->IsPermanent())
        {
            SendClearCooldown(cdData->GetSpellId(), this);
            spellsSent.emplace(cdData->GetSpellId());
        }
    }

    // reset lockout spell (清除法术反制/学派锁定冷却)
    for (auto& lockoutItr : m_lockoutMap)
    {
        SpellSchoolMask lockoutSchoolMask = SpellSchoolMask(1 << lockoutItr.first);
        RemoveSpellLockout(lockoutSchoolMask, &spellsSent);
    }

    if (!sendOnly)
    {
        for (auto itr = m_cooldownMap.begin(); itr != m_cooldownMap.end();)
        {
            if (spellsSent.find(itr->first) != spellsSent.end())
                itr = m_cooldownMap.erase(itr);
            else
                ++itr;
        }
        m_lockoutMap.clear();
    }
}

void Player::LockOutSpells(SpellSchoolMask schoolMask, uint32 duration)
{
    TimePoint lockoutExpireTime = std::chrono::milliseconds(duration) + GetMap()->GetCurrentClockTime();
    ByteBuffer cdData;
    uint32 spellCount = 0;

    // Send CD to clients (通知客户端各法术冷却状态)
    for (auto ownerSpellItr : GetSpellMap())
    {
        if (ownerSpellItr.second.state == PLAYERSPELL_REMOVED)
            continue;

        uint32 unSpellId = ownerSpellItr.first;
        SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(unSpellId);

        // Not send cooldown for this spells (跳过特殊被动/隐藏法术)
        if (spellEntry->HasAttribute(SPELL_ATTR_COOLDOWN_ON_EVENT))
            continue;

        TimePoint expireTime;
        bool isInfinite;
        bool spellCDFound = GetExpireTime(*spellEntry, expireTime, isInfinite);

        if ((schoolMask & GetSpellSchoolMask(spellEntry)) && (!spellCDFound || expireTime < lockoutExpireTime))
        {
            cdData << uint32(unSpellId);
            cdData << uint32(duration);                     // in m.secs (将持续时间转为毫秒)
            ++spellCount;
        }
    }

    WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 1 + spellCount * 8);
    data << GetObjectGuid();
    data << uint8(SPELL_COOLDOWN_FLAG_NONE);
    data.append(cdData);
    GetSession()->SendPacket(data);

    // store lockout (记录该法术的学派锁定冷却时间)
    WorldObject::LockOutSpells(schoolMask, duration);
}

void Player::RemoveSpellLockout(SpellSchoolMask spellSchoolMask, std::set<uint32>* spellAlreadySent /*= nullptr*/)
{
    for (auto ownerSpellItr : GetSpellMap())
    {
        if (ownerSpellItr.second.state == PLAYERSPELL_REMOVED)
            continue;

        uint32 unSpellId = ownerSpellItr.first;
        SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(unSpellId);

        // Not send cooldown for this spells (跳过无需通知客户端的法术)
        if (!spellEntry || !(GetSpellSchoolMask(spellEntry) & spellSchoolMask) || spellEntry->HasAttribute(SPELL_ATTR_COOLDOWN_ON_EVENT))
            continue;

        if (spellAlreadySent)
        {
            if (spellAlreadySent->find(spellEntry->Id) != spellAlreadySent->end())
                continue;

            spellAlreadySent->emplace(spellEntry->Id);
        }

        SendClearCooldown(spellEntry->Id, this);
    }
}

/* System for limiting instance entering to 5 per hour (限制每小时最多进入 5 个新副本的防刷机制) */
// check if player can enter new instance (检查玩家当前是否允许进入新的副本实例)
bool Player::CanEnterNewInstance(uint32 instanceId)
{
    if (IsGameMaster())
        return true;

    if (m_enteredInstances.find(instanceId) != m_enteredInstances.end())
        return true;

    return m_enteredInstances.size() < PLAYER_NEW_INSTANCE_LIMIT_PER_HOUR;
}

// when instance was entered (记录进入该副本实例的时间戳)
void Player::AddNewInstanceId(uint32 instanceId)
{
    if (m_enteredInstances.find(instanceId) == m_enteredInstances.end())
        m_enteredInstances.emplace(instanceId, std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::now() + std::chrono::hours(1)));
}

void Player::_LoadCreatedInstanceTimers()
{
    //                                                     0          1
    auto queryResult = CharacterDatabase.PQuery("SELECT ExpireTime, InstanceId FROM account_instances_entered WHERE AccountId = '%u'", m_session->GetAccountId());
    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();
            TimePoint expireTime = std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::from_time_t(fields[0].GetUInt64()));
            uint32 instanceId = fields[1].GetUInt32();

            if (expireTime > Clock::now())
                m_enteredInstances.emplace(instanceId, expireTime);

        }
        while (queryResult->NextRow());
    }
}

void Player::_SaveNewInstanceIdTimer()
{
    CharacterDatabase.PExecute("DELETE FROM account_instances_entered WHERE AccountId = '%u'", m_session->GetAccountId());

    if (m_enteredInstances.empty())
        return;

    static SqlStatementID insertInsertTimer;
    for (auto enterInstItr : m_enteredInstances)
    {
        SqlStatement stmt = CharacterDatabase.CreateStatement(insertInsertTimer,
                            "INSERT INTO account_instances_entered (AccountId, ExpireTime, InstanceId) VALUES( ?, ?, ?)");

        stmt.addUInt32(m_session->GetAccountId());
        stmt.addUInt64(Clock::to_time_t(enterInstItr.second));
        stmt.addUInt32(enterInstItr.first);
        stmt.Execute();
    }
}

// Clears timers that expired (清除已超过 1 小时滚动窗口的副本进入计时记录)
void Player::UpdateNewInstanceIdTimers(TimePoint const& now)
{
    for (auto iter = m_enteredInstances.begin(); iter != m_enteredInstances.end();)
    {
        if ((*iter).second < now)
            iter = m_enteredInstances.erase(iter);
        else
            ++iter;
    }
}

void Player::UpdateClientAuras()
{
    for (const auto& iter : m_spellAuraHolders)
        iter.second->UpdateAuraDuration();
}

void Player::SendPetBar()
{
    if (Pet* pet = GetPet())
        PetSpellInitialize();
    else if (Unit* charm = GetCharm())
    {
        if (charm->hasUnitState(UNIT_STAT_POSSESSED))
            PossessSpellInitialize();
        else
            CharmSpellInitialize();
    }
}

void Player::StartCinematic()
{
    // server sent cinematic start? (通知过场动画管理器开始播放)
    if (!m_cinematicMgr)
        return;

    m_cinematicMgr->BeginCinematic();
}

void Player::StopCinematic()
{
    // any cinematic started? (通知过场动画管理器停止播放)
    if (!m_cinematicMgr)
        return;

    m_cinematicMgr->EndCinematic();
    m_cinematicMgr.reset(nullptr);
}

bool Player::IsAtRecruitAFriendDistance(WorldObject const* other) const
{
    if (!other || !IsInMap(other))
        return false;

    WorldObject const* player = GetCorpse();
    if (!player || IsAlive())
        player = this;

    return other->GetDistance(player) <= sWorld.getConfig(CONFIG_FLOAT_MAX_RECRUIT_A_FRIEND_DISTANCE);
}

bool Player::GetsRecruitAFriendBonus()
{
    if (!GetSession()->GetRecruitingFriendId())
        return false;

    if (GetLevel() <= sWorld.getConfig(CONFIG_UINT32_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL))
    {
        if (Group* group = GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* player = itr->getSource();
                if (!player)
                    continue;

                if (!player->IsAtRecruitAFriendDistance(this))
                    continue;                               // member (alive or dead) or his corpse at req. distance (战友招募队友须在有效经验共享距离内)

                    // level must be allowed to get RaF bonus (受招募等级上限限制)
                if (player->GetLevel() > sWorld.getConfig(CONFIG_UINT32_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL))
                    continue;

                // level difference must be small enough to get RaF bonus, UNLESS we are lower level (双倍/三倍经验要求双方等级差不超过 3 级，低等级方例外)
                if (player->GetLevel() < GetLevel())
                    if (uint8(GetLevel() - player->GetLevel()) > sWorld.getConfig(CONFIG_UINT32_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL_DIFFERENCE))
                        continue;

                if (player->GetSession()->GetRecruitingFriendId() == GetSession()->GetAccountId())
                    return true;
            }
        }
    }
    return false;
}

void Player::SetSpellModSpell(Spell* spell)
{
    if (!spell)
    {
        m_modsSpell = nullptr;
        m_consumedMods = nullptr;
        return;
    }
    m_modsSpell = spell;
    m_consumedMods = &spell->m_usedAuraCharges;
}

uint32 Player::LookupHighestLearnedRank(uint32 spellId)
{
    uint32 ownedRank = 0;
    uint32 higherRank = spellId;
    do
    {
        if (HasSpell(higherRank))
            ownedRank = higherRank;
        else
            break;
    } while ((higherRank = sSpellMgr.GetNextSpellInChain(ownedRank)));
    return ownedRank;
}

void Player::UpdateRangedWeaponDependantAmmoHasteAura()
{
    int32 highest = 0;
    Item* weapon = GetWeaponForAttack(RANGED_ATTACK);
    if (weapon)
        highest = GetMaxPositiveAuraModifierByItemClass(SPELL_AURA_MOD_RANGED_AMMO_HASTE, weapon);

    if (highest != GetHighestAmmoMod())
    {
        if (GetHighestAmmoMod() > 0)
            ApplyAttackTimePercentMod(RANGED_ATTACK, float(GetHighestAmmoMod()), false);
        if (highest > 0)
            ApplyAttackTimePercentMod(RANGED_ATTACK, float(highest), true);
        SetHighestAmmoMod(highest);
    }
}

bool Player::SetStunnedByLogout(bool apply)
{
    if (SetStunned(apply, ObjectGuid(), 0, true))
    {
        // Sit down when eligible: (符合条件时在等待登出期间坐下)
        if (apply)
        {
            if (IsStandState())
            {
                if (!m_movementInfo.HasMovementFlag(MovementFlags(movementFlagsMask | MOVEFLAG_SWIMMING | MOVEFLAG_SPLINE_ENABLED)))
                    SetStandState(UNIT_STAND_STATE_SIT);
            }
        }
        // Stand up on cancel (取消登出时起立)
        else if (getStandState() == UNIT_STAND_STATE_SIT)
            SetStandState(UNIT_STAND_STATE_STAND);

        ApplyModByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_FLAGS, PLAYER_FIELD_BYTE_LOGGING_OUT, apply);
        return true;
    }
    return false;
}

void Player::SetPet(Unit* pet)
{
    Unit::SetPet(pet);
    ApplyModByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_FLAGS, PLAYER_FIELD_BYTE_CONTROLLING_PET, pet != nullptr);
}