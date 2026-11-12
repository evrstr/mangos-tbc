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

#include "Entities/GossipDef.h"
#include "Quests/QuestDef.h"
#include "Globals/ObjectMgr.h"
#include "Server/Opcodes.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "Tools/Formulas.h"

// 构造对话菜单并预留 16 个选项容量
GossipMenu::GossipMenu(WorldSession* session) : m_session(session)
{
    m_gItems.reserve(16);                                   // can be set for max from most often sizes to speedup push_back and less memory use (按常见尺寸预留容量以减少扩容开销)
    m_gMenuId = 0;
    m_discoveredNode = false;
}

GossipMenu::~GossipMenu()
{
    ClearMenu();
}

// 向对话菜单追加一条选项（含图标、文本、Sender/Action、确认弹窗与金额）
void GossipMenu::AddMenuItem(uint8 Icon, const std::string& Message, uint32 dtSender, uint32 dtAction, const std::string& BoxMessage, uint32 BoxMoney, bool Coded)
{
    MANGOS_ASSERT(m_gItems.size() <= GOSSIP_MAX_MENU_ITEMS);

    GossipMenuItem gItem;

    gItem.m_gIcon       = Icon;
    gItem.m_gMessage    = Message;
    gItem.m_gCoded      = Coded;
    gItem.m_gSender     = dtSender;
    gItem.m_gOptionId   = dtAction;
    gItem.m_gBoxMessage = BoxMessage;
    gItem.m_gBoxMoney   = BoxMoney;

    m_gItems.push_back(gItem);
}

// 追加对话选项对应的数据库后续动作（子菜单跳转、POI、脚本）
void GossipMenu::AddGossipMenuItemData(int32 action_menu, uint32 action_poi, uint32 action_script)
{
    GossipMenuItemData pItemData;

    pItemData.m_gAction_menu    = action_menu;
    pItemData.m_gAction_poi     = action_poi;
    pItemData.m_gAction_script  = action_script;

    m_gItemsData.push_back(pItemData);
}

// 简化版添加选项重载（无 Sender/Action 与确认弹窗）
void GossipMenu::AddMenuItem(uint8 Icon, const std::string& Message, bool Coded)
{
    AddMenuItem(Icon, Message, 0, 0, "", 0, Coded);
}

void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), Coded);
}

void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, char const* BoxMessage, uint32 BoxMoney, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), dtSender, dtAction, std::string(BoxMessage ? BoxMessage : ""), BoxMoney, Coded);
}

// 通过 mangos_string 文本 ID 添加本地化对话选项
void GossipMenu::AddMenuItem(uint8 Icon, int32 itemText, uint32 dtSender, uint32 dtAction, int32 boxText, uint32 BoxMoney, bool Coded)
{
    uint32 loc_idx = m_session->GetSessionDbLocaleIndex();

    char const* item_text = itemText ? sObjectMgr.GetMangosString(itemText, loc_idx) : "";
    char const* box_text = boxText ? sObjectMgr.GetMangosString(boxText, loc_idx) : "";

    AddMenuItem(Icon, std::string(item_text), dtSender, dtAction, std::string(box_text), BoxMoney, Coded);
}

// 通过 broadcast_text 文本 ID 添加支持性别称谓与本地化的对话选项
void GossipMenu::AddMenuItemBct(uint8 Icon, int32 itemText, Gender gender, uint32 dtSender, uint32 dtAction, int32 boxText, uint32 BoxMoney, bool Coded)
{
    uint32 loc_idx = m_session->GetSessionDbLocaleIndex();

    BroadcastText const* bctOption = sObjectMgr.GetBroadcastText(itemText);
    MANGOS_ASSERT(bctOption); // should never be null
    std::string const& strOptionText = bctOption->GetText(loc_idx, gender);

    BroadcastText const* bctBox = sObjectMgr.GetBroadcastText(boxText);

    AddMenuItem(Icon, strOptionText, dtSender, dtAction, bctBox != nullptr ? bctBox->GetText(loc_idx, gender) : "", BoxMoney, Coded);
}

// 获取指定选项索引的 Sender 标识
uint32 GossipMenu::MenuItemSender(unsigned int ItemId)
{
    if (ItemId >= m_gItems.size())
        return 0;

    return m_gItems[ ItemId ].m_gSender;
}

// 获取指定选项索引的 Action 标识
uint32 GossipMenu::MenuItemAction(unsigned int ItemId)
{
    if (ItemId >= m_gItems.size())
        return 0;

    return m_gItems[ ItemId ].m_gOptionId;
}

// 获取指定选项是否需要口令/确认框输入
bool GossipMenu::MenuItemCoded(unsigned int ItemId)
{
    if (ItemId >= m_gItems.size())
        return false;

    return m_gItems[ ItemId ].m_gCoded;
}

// 清空对话菜单及其元数据
void GossipMenu::ClearMenu()
{
    m_gItems.clear();
    m_gItemsData.clear();
    m_gMenuId = 0;
    m_discoveredNode = false;
}

PlayerMenu::PlayerMenu(WorldSession* session) : mGossipMenu(session)
{
}

PlayerMenu::~PlayerMenu()
{
    ClearMenus();
}

// 清空玩家当前对话与任务菜单
void PlayerMenu::ClearMenus()
{
    mGossipMenu.ClearMenu();
    mQuestMenu.ClearMenu();
}

uint32 PlayerMenu::GossipOptionSender(unsigned int Selection)
{
    return mGossipMenu.MenuItemSender(Selection);
}

uint32 PlayerMenu::GossipOptionAction(unsigned int Selection)
{
    return mGossipMenu.MenuItemAction(Selection);
}

bool PlayerMenu::GossipOptionCoded(unsigned int Selection)
{
    return mGossipMenu.MenuItemCoded(Selection);
}

// 组装并发送 SMSG_GOSSIP_MESSAGE 数据包（包含闲聊选项与可接/可交任务列表）
void PlayerMenu::SendGossipMenu(uint32 TitleTextId, ObjectGuid objectGuid)
{
    WorldPacket data(SMSG_GOSSIP_MESSAGE, (100));           // guess size
    data << ObjectGuid(objectGuid);
    data << uint32(mGossipMenu.GetMenuId());                // new 2.4.0 (2.4.0 新增菜单 ID 字段)
    data << uint32(TitleTextId);
    data << uint32(mGossipMenu.MenuItemCount());            // max count 0x20 (最多 32 个选项)

    for (uint32 iI = 0; iI < mGossipMenu.MenuItemCount(); ++iI)
    {
        GossipMenuItem const& gItem = mGossipMenu.GetItem(iI);
        data << uint32(iI);
        data << uint8(gItem.m_gIcon);
        data << uint8(gItem.m_gCoded);                      // makes pop up box password (设为 true 时客户端弹出文本输入框)
        data << uint32(gItem.m_gBoxMoney);                  // money required to open menu, 2.0.3 (点击确认框提示的费用)
        data << gItem.m_gMessage;                           // text for gossip item, max 0x800
        data << gItem.m_gBoxMessage;                        // accept text (related to money) pop up box, 2.0.3, max 0x800
    }

    data << uint32(mQuestMenu.MenuItemCount());             // max count 0x20 (附带任务列表最多 32 项)

    for (uint32 iI = 0; iI < mQuestMenu.MenuItemCount(); ++iI)
    {
        QuestMenuItem const& qItem = mQuestMenu.GetItem(iI);
        uint32 questID = qItem.m_qId;
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(questID);

        data << uint32(questID);
        data << uint32(qItem.m_qIcon);
        data << int32(pQuest->GetQuestLevel());

        int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
        std::string title = pQuest->GetTitle();
        sObjectMgr.GetQuestLocaleStrings(questID, loc_idx, &title);

        data << title;                                      // max 0x200
    }

    GetMenuSession()->SendPacket(data);
    DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_MESSAGE from %s", objectGuid.GetString().c_str());
}

// 发送 SMSG_GOSSIP_COMPLETE 关闭客户端对话窗口
void PlayerMenu::CloseGossip() const
{
    WorldPacket data(SMSG_GOSSIP_COMPLETE, 0);
    GetMenuSession()->SendPacket(data);

    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_COMPLETE");
}

// Outdated (旧版直接按坐标与图标发送兴趣点标记)
void PlayerMenu::SendPointOfInterest(float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, char const* locName) const
{
    WorldPacket data(SMSG_GOSSIP_POI, (4 + 4 + 4 + 4 + 4 + 10)); // guess size
    data << uint32(Flags);
    data << float(X);
    data << float(Y);
    data << uint32(Icon);
    data << uint32(Data);
    data << locName;

    GetMenuSession()->SendPacket(data);
    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_POI");
}

// 根据 points_of_interest 表 ID 发送本地化小地图兴趣点标记 (SMSG_GOSSIP_POI)
void PlayerMenu::SendPointOfInterest(uint32 poi_id) const
{
    PointOfInterest const* poi = sObjectMgr.GetPointOfInterest(poi_id);
    if (!poi)
    {
        sLog.outErrorDb("Requested send nonexistent POI (Id: %u), ignore.", poi_id);
        return;
    }

    std::string icon_name = poi->icon_name;

    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
        if (PointOfInterestLocale const* pl = sObjectMgr.GetPointOfInterestLocale(poi_id))
            if (pl->IconName.size() > size_t(loc_idx) && !pl->IconName[loc_idx].empty())
                icon_name = pl->IconName[loc_idx];

    WorldPacket data(SMSG_GOSSIP_POI, (4 + 4 + 4 + 4 + 4 + 10)); // guess size
    data << uint32(poi->flags);
    data << float(poi->x);
    data << float(poi->y);
    data << uint32(poi->icon);
    data << uint32(poi->data);
    data << icon_name;

    GetMenuSession()->SendPacket(data);
    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_POI");
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/***                    任务系统界面封包               ***/
/*********************************************************/

QuestMenu::QuestMenu()
{
    m_qItems.reserve(16);                                   // can be set for max from most often sizes to speedup push_back and less memory use
}

QuestMenu::~QuestMenu()
{
    ClearMenu();
}

// 向任务菜单添加一条任务项（过滤不存在的任务模板）
void QuestMenu::AddMenuItem(uint32 QuestId, uint8 Icon)
{
    Quest const* qinfo = sObjectMgr.GetQuestTemplate(QuestId);
    if (!qinfo)
        return;

    MANGOS_ASSERT(m_qItems.size() <= GOSSIP_MAX_MENU_ITEMS);

    QuestMenuItem qItem;

    qItem.m_qId        = QuestId;
    qItem.m_qIcon      = Icon;

    m_qItems.push_back(qItem);
}

// 检查任务菜单中是否已包含指定任务 ID
bool QuestMenu::HasItem(uint32 questid)
{
    for (QuestMenuItemList::const_iterator i = m_qItems.begin(); i != m_qItems.end(); ++i)
        if (i->m_qId == questid)
            return true;
    return false;
}

void QuestMenu::ClearMenu()
{
    m_qItems.clear();
}

// 发送 NPC 任务列表窗口 (SMSG_QUESTGIVER_QUEST_LIST)
void PlayerMenu::SendQuestGiverQuestList(QEmote eEmote, const std::string& Title, ObjectGuid npcGUID)
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_LIST, 100);      // guess size
    data << ObjectGuid(npcGUID);
    data << Title;
    data << uint32(eEmote._Delay);                          // player emote (问候表情延迟)
    data << uint32(eEmote._Emote);                          // NPC emote (NPC 动作表情)

    size_t count_pos = data.wpos();
    data << uint8(mQuestMenu.MenuItemCount());
    uint32 count = 0;
    for (; count < mQuestMenu.MenuItemCount(); ++count)
    {
        QuestMenuItem const& qmi = mQuestMenu.GetItem(count);

        uint32 questID = qmi.m_qId;

        if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(questID))
        {
            int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
            std::string title = pQuest->GetTitle();
            sObjectMgr.GetQuestLocaleStrings(questID, loc_idx, &title);

            data << uint32(questID);
            data << uint32(qmi.m_qIcon);
            data << int32(pQuest->GetQuestLevel());
            data << title;
        }
    }
    data.put<uint8>(count_pos, count);
    GetMenuSession()->SendPacket(data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_LIST NPC Guid = %s", npcGUID.GetString().c_str());
}

// 更新客户端头顶任务状态标记 (SMSG_QUESTGIVER_STATUS)
void PlayerMenu::SendQuestGiverStatus(uint8 questStatus, ObjectGuid npcGUID) const
{
    WorldPacket data(SMSG_QUESTGIVER_STATUS, 9);
    data << npcGUID;
    data << uint8(questStatus);

    GetMenuSession()->SendPacket(data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_STATUS for %s", npcGUID.GetString().c_str());
}

// 发送任务详情与接取界面 (SMSG_QUESTGIVER_QUEST_DETAILS)
void PlayerMenu::SendQuestGiverQuestDetails(Quest const* pQuest, ObjectGuid guid, bool ActivateAccept) const
{
    std::string Title      = pQuest->GetTitle();
    std::string Details    = pQuest->GetDetails();
    std::string Objectives = pQuest->GetObjectives();

    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
                Title = ql->Title[loc_idx];
            if (ql->Details.size() > (size_t)loc_idx && !ql->Details[loc_idx].empty())
                Details = ql->Details[loc_idx];
            if (ql->Objectives.size() > (size_t)loc_idx && !ql->Objectives[loc_idx].empty())
                Objectives = ql->Objectives[loc_idx];
        }
    }

    WorldPacket data(SMSG_QUESTGIVER_QUEST_DETAILS, 100);   // guess size
    data << guid;
    data << uint32(pQuest->GetQuestId());
    data << Title;
    data << Details;
    data << Objectives;
    data << uint32(ActivateAccept ? 1 : 0);                 // auto finish (是否激活“接受”按钮)
    data << uint32(pQuest->GetSuggestedPlayers());

    if (pQuest->HasQuestFlag(QUEST_FLAGS_HIDDEN_REWARDS))
    {
        data << uint32(0);                                  // Rewarded chosen items hidden (隐藏奖励时下发 0)
        data << uint32(0);                                  // Rewarded items hidden
        data << uint32(0);                                  // Rewarded money hidden
    }
    else
    {
        ItemPrototype const* IProto;

        uint32 rewChocieItemCount = pQuest->GetRewChoiceItemsCount();
        data << uint32(rewChocieItemCount);

        for (uint32 i = 0; i < rewChocieItemCount; ++i)
        {
            if (!pQuest->RewChoiceItemId[i])
                continue;

            data << uint32(pQuest->RewChoiceItemId[i]);
            data << uint32(pQuest->RewChoiceItemCount[i]);

            IProto = ObjectMgr::GetItemPrototype(pQuest->RewChoiceItemId[i]);

            if (IProto)
                data << uint32(IProto->DisplayInfoID);
            else
                data << uint32(0x00);
        }

        uint32 rewItemCount = pQuest->GetRewItemsCount();
        data << uint32(rewItemCount);

        for (uint32 i = 0; i < rewItemCount; ++i)
        {
            if (!pQuest->RewItemId[i])
                continue;

            data << uint32(pQuest->RewItemId[i]);
            data << uint32(pQuest->RewItemCount[i]);

            IProto = ObjectMgr::GetItemPrototype(pQuest->RewItemId[i]);

            if (IProto)
                data << uint32(IProto->DisplayInfoID);
            else
                data << uint32(0);
        }

        data << uint32(pQuest->GetRewOrReqMoney());
    }

    // rewarded honor points. Multiply with 10 to satisfy client (荣誉奖励乘以 10 以匹配客户端显示格式)
    data << uint32(10 * MaNGOS::Honor::hk_honor_at_level(GetMenuSession()->GetPlayer()->GetLevel(), pQuest->GetRewHonorableKills()));
    data << uint32(pQuest->GetRewSpell());                  // reward spell, this spell will display (icon) (casted if RewSpellCast==0) (界面展示的奖励法术)
    data << uint32(pQuest->GetRewSpellCast());              // casted spell (实际触发施放的法术)
    data << uint32(pQuest->GetCharTitleBitIndex());         // CharTitle, new 2.4.0, player gets this title (bit index from CharTitles) (2.4.0 新增奖励头衔位索引)

    uint32 detailsEmotesCount = pQuest->GetDetailsEmoteCount();
    data << uint32(detailsEmotesCount);

    for (uint32 i = 0; i < detailsEmotesCount; ++i)
    {
        data << uint32(pQuest->DetailsEmote[i]);
        data << uint32(pQuest->DetailsEmoteDelay[i]);       // DetailsEmoteDelay (in ms)
    }

    GetMenuSession()->SendPacket(data);

    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_DETAILS - for %s of %s, questid = %u", GetMenuSession()->GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str(), pQuest->GetQuestId());
}

// 发送任务完成领奖选择界面 (SMSG_QUESTGIVER_OFFER_REWARD)
void PlayerMenu::SendQuestGiverOfferReward(Quest const* pQuest, ObjectGuid npcGUID, bool EnableNext) const
{
    std::string Title = pQuest->GetTitle();
    std::string OfferRewardText = pQuest->GetOfferRewardText();

    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
                Title = ql->Title[loc_idx];
            if (ql->OfferRewardText.size() > (size_t)loc_idx && !ql->OfferRewardText[loc_idx].empty())
                OfferRewardText = ql->OfferRewardText[loc_idx];
        }
    }

    WorldPacket data(SMSG_QUESTGIVER_OFFER_REWARD, 50);     // guess size

    data << ObjectGuid(npcGUID);
    data << uint32(pQuest->GetQuestId());
    data << Title;
    data << OfferRewardText;

    data << uint32(EnableNext ? 1 : 0);                     // Auto Finish
    data << uint32(pQuest->GetSuggestedPlayers());          // SuggestedGroupNum

    uint32 EmoteCount = 0;
    for (unsigned int i : pQuest->OfferRewardEmote)
    {
        if (i == 0)
            break;
        ++EmoteCount;
    }

    data << EmoteCount;                                     // Emote Count
    for (uint32 i = 0; i < EmoteCount; ++i)
    {
        data << uint32(pQuest->OfferRewardEmoteDelay[i]);   // Delay Emote
        data << uint32(pQuest->OfferRewardEmote[i]);
    }

    ItemPrototype const* pItem;

    data << uint32(pQuest->GetRewChoiceItemsCount());
    for (uint32 i = 0; i < pQuest->GetRewChoiceItemsCount(); ++i)
    {
        pItem = ObjectMgr::GetItemPrototype(pQuest->RewChoiceItemId[i]);

        data << uint32(pQuest->RewChoiceItemId[i]);
        data << uint32(pQuest->RewChoiceItemCount[i]);

        if (pItem)
            data << uint32(pItem->DisplayInfoID);
        else
            data << uint32(0);
    }

    data << uint32(pQuest->GetRewItemsCount());
    for (uint32 i = 0; i < pQuest->GetRewItemsCount(); ++i)
    {
        pItem = ObjectMgr::GetItemPrototype(pQuest->RewItemId[i]);
        data << uint32(pQuest->RewItemId[i]);
        data << uint32(pQuest->RewItemCount[i]);

        if (pItem)
            data << uint32(pItem->DisplayInfoID);
        else
            data << uint32(0);
    }

    data << uint32(pQuest->GetRewOrReqMoney());

    // rewarded honor points. Multiply with 10 to satisfy client
    data << uint32(10 * MaNGOS::Honor::hk_honor_at_level(GetMenuSession()->GetPlayer()->GetLevel(), pQuest->GetRewHonorableKills()));
    data << uint32(0x08);                                   // unused by client?
    data << uint32(pQuest->GetRewSpell());                  // reward spell, this spell will display (icon) (casted if RewSpellCast==0)
    data << uint32(pQuest->GetRewSpellCast());              // casted spell
    data << uint32(pQuest->GetCharTitleBitIndex());         // character title
    GetMenuSession()->SendPacket(data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_OFFER_REWARD NPCGuid = %s, questid = %u", npcGUID.GetString().c_str(), pQuest->GetQuestId());
}

// 发送任务提交所需物品检查界面 (SMSG_QUESTGIVER_REQUEST_ITEMS)
void PlayerMenu::SendQuestGiverRequestItems(Quest const* pQuest, ObjectGuid npcGUID, bool Completable, bool CloseOnCancel) const
{
    // We can always call to RequestItems, but this packet only goes out if there are actually
    // items.  Otherwise, we'll skip straight to the OfferReward
    // 若任务无所需物品且已完成（或缺少请求物品说明文本），直接跳转至领奖界面

    std::string Title = pQuest->GetTitle();
    std::string RequestItemsText = pQuest->GetRequestItemsText();

    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
                Title = ql->Title[loc_idx];
            if (ql->RequestItemsText.size() > (size_t)loc_idx && !ql->RequestItemsText[loc_idx].empty())
                RequestItemsText = ql->RequestItemsText[loc_idx];
        }
    }

    // Only shown for incomplete quests, or ones that require items. (仅在任务未完成或需要提交物品时显示物品请求页)
    if (RequestItemsText.empty() || ((pQuest->GetReqItemsCount() == 0) && Completable))
    {
        SendQuestGiverOfferReward(pQuest, npcGUID, true);
        return;
    }

    WorldPacket data(SMSG_QUESTGIVER_REQUEST_ITEMS, 50);    // guess size
    data << ObjectGuid(npcGUID);
    data << uint32(pQuest->GetQuestId());
    data << Title;
    data << RequestItemsText;

    if (Completable)
    {
        data << pQuest->GetCompleteEmoteDelay();
        data << pQuest->GetCompleteEmote();
    }
    else
    {
        data << pQuest->GetIncompleteEmoteDelay();
        data << pQuest->GetIncompleteEmote();
    }

    // Close Window after cancel (取消时是否关闭窗口)
    data << uint32(CloseOnCancel);                          // auto finish

    data << uint32(pQuest->GetSuggestedPlayers());          // SuggestedGroupNum

    // Required Money (任务所需缴纳的金币)
    data << uint32(pQuest->GetRewOrReqMoney() < 0 ? -pQuest->GetRewOrReqMoney() : 0);

    data << uint32(pQuest->GetReqItemsCount());
    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        if (!pQuest->ReqItemId[i])
            continue;
        ItemPrototype const* pItem = ObjectMgr::GetItemPrototype(pQuest->ReqItemId[i]);
        data << uint32(pQuest->ReqItemId[i]);
        data << uint32(pQuest->ReqItemCount[i]);

        if (pItem)
            data << uint32(pItem->DisplayInfoID);
        else
            data << uint32(0);
    }

    if (!Completable)                                       // Completable = flags1 && flags2 && flags3 && flags4 (四个标志均非零客户端才激活“继续”按钮)
        data << uint32(0x00);                               // flags1
    else
        data << uint32(0x03);

    data << uint32(0x04);                                   // flags2
    data << uint32(0x08);                                   // flags3
    data << uint32(0x10);                                   // flags4

    GetMenuSession()->SendPacket(data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_REQUEST_ITEMS NPCGuid = %s, questid = %u", npcGUID.GetString().c_str(), pQuest->GetQuestId());
}
