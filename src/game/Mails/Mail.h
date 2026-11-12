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

/**
 * @addtogroup mailing The mail system
 * The mailing system in MaNGOS consists of mostly 4 files:
 * - Mail.h
 * - Mail.cpp
 * - MassMailMgr.h
 * - MassMailMgr.cpp
 *
 * @{
 *
 * @file Mail.h
 * This file contains the the headers needed for MaNGOS to handle mails.
 *
 */

#ifndef MANGOS_MAIL_H
#define MANGOS_MAIL_H

#include "Common.h"
#include "Entities/ObjectGuid.h"
#include <map>
#include <utility>

struct AuctionEntry;
class Item;
class Object;
class Player;

#define MAIL_BODY_ITEM_TEMPLATE 8383                        ///< - plain letter, A Dusty Unsent Letter: 889 (普通信件物品模板 ID)
/// The maximal amount of items a mail can contain. (单封邮件最多可附带 12 件物品)
#define MAX_MAIL_ITEMS 12
/**
 * The type of the mail. (邮件发送者来源类型，共有 5 种)
 * A mail can have 5 Different Types.
 */
enum MailMessageType
{
    MAIL_NORMAL         = 0,                                // 普通玩家邮件
    MAIL_AUCTION        = 2,                                // 拍卖行邮件
    MAIL_CREATURE       = 3,                                /// client send CMSG_CREATURE_QUERY on this mailmessagetype (生物/NPC 邮件)
    MAIL_GAMEOBJECT     = 4,                                /// client send CMSG_GAMEOBJECT_QUERY on this mailmessagetype (游戏对象邮件)
    MAIL_ITEM           = 5,                                /// client send CMSG_ITEM_QUERY on this mailmessagetype (物品触发邮件)
};
/**
 * A Mask representing the status of the mail. (邮件状态检查标志位掩码)
 */
enum MailCheckMask
{
    MAIL_CHECK_MASK_NONE        = 0x00,                     /// Nothing. (未读/无特殊标记)
    MAIL_CHECK_MASK_READ        = 0x01,                     /// This mail was read. (已阅读)
    MAIL_CHECK_MASK_RETURNED    = 0x02,                     /// This mail was returned. No allow return mail. (退信，不允许再次退回)
    MAIL_CHECK_MASK_COPIED      = 0x04,                     /// This mail was copied. No allow make item copy from mail text. (已抄录正文，不允许再生成信件物品)
    MAIL_CHECK_MASK_COD_PAYMENT = 0x08,                     /// This mail is payable on delivery. (货到付款邮件)
    MAIL_CHECK_MASK_HAS_BODY    = 0x10,                     /// This mail has body text. (包含正文文本)
};

/**
 * The different types of Stationaries that exist for mails. (邮件信纸背景样式，源自 Stationery.dbc)
 * They have been gathered from Stationery.dbc
 */
enum MailStationery
{
    MAIL_STATIONERY_UNKNOWN =  1,
    MAIL_STATIONERY_DEFAULT = 41,
    MAIL_STATIONERY_GM      = 61,
    MAIL_STATIONERY_AUCTION = 62,
    MAIL_STATIONERY_VAL     = 64,
    MAIL_STATIONERY_CHR     = 65,
};
/**
 * Representation of the State of a mail. (邮件内存同步状态)
 */
enum MailState
{
    MAIL_STATE_UNCHANGED = 1,                               // 未修改
    MAIL_STATE_CHANGED   = 2,                               // 已修改待入库
    MAIL_STATE_DELETED   = 3                                // 已删除待清理
};
/**
 * Answers contained in mails from auctionhouses. (拍卖行系统邮件子类型)
 */
enum MailAuctionAnswers
{
    AUCTION_OUTBIDDED           = 0,                        // 竞价被超越退款
    AUCTION_WON                 = 1,                        // 竞拍中标得物
    AUCTION_SUCCESSFUL          = 2,                        // 拍卖售出得款
    AUCTION_EXPIRED             = 3,                        // 拍卖流拍退货
    AUCTION_CANCELLED_TO_BIDDER = 4,                        // 拍卖被卖家取消退款给买家
    AUCTION_CANCELED            = 5,                        // 拍卖被取消退货给卖家
    AUCTION_SALE_PENDING        = 6                         // 拍卖成交待到账通知
};
/**
 * A class to represent the sender of a mail. (邮件发件人描述类：封装发件类型、GUID/模板 ID 与信纸样式)
 */
class MailSender
{
    public:                                                 // Constructors
        MailSender() : m_messageType(MAIL_NORMAL), m_senderId(0), m_stationery(MAIL_STATIONERY_DEFAULT) {}

        /**
         * Creates a new MailSender object.
         *
         * @param messageType the type of the mail.
         * @param sender_guidlow_or_entry The lower part of the GUID of the player sending
         *                                                                this mail, or the Entry of the non-player object.
         * @param stationery The stationary associated with this MailSender.
         *
         */
        MailSender(MailMessageType messageType, uint32 sender_guidlow_or_entry, MailStationery stationery = MAIL_STATIONERY_DEFAULT)
            : m_messageType(messageType), m_senderId(sender_guidlow_or_entry), m_stationery(stationery)
        {
        }
        MailSender(Object* sender, MailStationery stationery = MAIL_STATIONERY_DEFAULT);
        MailSender(AuctionEntry* sender);
    public:                                                 // Accessors
        /// The Messagetype of this MailSender.
        MailMessageType GetMailMessageType() const { return m_messageType; }
        /// The GUID of the player represented by this MailSender, or the Entry of the non-player object.
        uint32 GetSenderId() const { return m_senderId; }
        /// The stationary associated with this MailSender
        MailStationery GetStationery() const { return m_stationery; }
    private:
        // Trap for wrong used guid as low guid, no body
        MailSender(MailMessageType messageType, uint64 wrong_guid, MailStationery stationery = MAIL_STATIONERY_DEFAULT);

        MailMessageType m_messageType;
        uint32 m_senderId;                                  // player low guid or other object entry
        MailStationery m_stationery;
};
/**
 * A class to represent the receiver of a mail. (邮件收件人描述类：兼容在线玩家指针与离线玩家 GUID)
 */
class MailReceiver
{
    public:                                                 // Constructors
        explicit MailReceiver(ObjectGuid receiver_guid) : m_receiver(nullptr), m_receiver_guid(receiver_guid) {}
        MailReceiver(Player* receiver);
        MailReceiver(Player* receiver, ObjectGuid receiver_guid);
    public:                                                 // Accessors
        /**
         * Gets the player associated with this MailReciever
         *
         * @see Player
         *
         * @returns a pointer to the Player this mail is for.
         *
         */
        Player* GetPlayer() const { return m_receiver; }
        /**
         * Gets the low part of the recievers GUID.
         *
         * @returns the low part of the GUID of the player associated with this MailReciever
         */
        ObjectGuid const& GetPlayerGuid() const { return m_receiver_guid; }
    private:
        Player* m_receiver;
        ObjectGuid m_receiver_guid;
};
/**
 * The class to represent the draft of a mail. (邮件草稿构造器：用于组装主题、正文、模板、金币、COD 与附件物品并投递或退信)
 */
class MailDraft
{
        /**
         * Holds a Map of GUIDs of items and pointers to the items.
         */
        typedef std::map<uint32, Item*> MailItemMap;

    public:                                                 // Constructors
        /**
         * Creates a new blank MailDraft object
         *
         */
        MailDraft()
            : m_mailTemplateId(0), m_mailTemplateItemsNeed(false), m_bodyId(0), m_money(0), m_COD(0) {}
        /**
         * Creates a new MailDraft object using mail template id.
         *
         * @param mailTemplateId The ID of the Template to be used.
         * @param a boolean specifying whether the mail needs items or not.
         *
         */
        explicit MailDraft(uint16 mailTemplateId, bool need_items = true)
            : m_mailTemplateId(mailTemplateId), m_mailTemplateItemsNeed(need_items), m_bodyId(0), m_money(0), m_COD(0)
        {}
        /**
         * Creates a new MailDraft object using subject text and content text id.
         *
         * @param subject The subject of the mail.
         * @param itemTextId The id of the body of the mail.
         */
        MailDraft(std::string subject, uint32 itemTextId = 0)
            : m_mailTemplateId(0), m_mailTemplateItemsNeed(false), m_subject(subject), m_bodyId(itemTextId), m_money(0), m_COD(0) {}
        /**
         * Creates a new MailDraft object using subject and contect texts.
         *
         * @param subject The subject of the mail.
         * @param itemText The text of the body of the mail.
         */
        MailDraft(std::string subject, std::string text);
    public:                                                 // Accessors
        /// Returns the template ID used for this MailDraft.
        uint16 GetMailTemplateId() const { return m_mailTemplateId; }
        /// Returns the subject of this MailDraft.
        std::string const& GetSubject() const { return m_subject; }
        /// Returns the ID of the text of this MailDraft.
        uint32 GetBodyId() const { return m_bodyId; }
        /// Returns the amount of money in this MailDraft.
        uint32 GetMoney() const { return m_money; }
        /// Returns the Cost of delivery of this MailDraft.
        uint32 GetCOD() const { return m_COD; }
    public:                                                 // modifiers

        // this two modifiers expected to be applied in normal case to blank draft and exclusively, It DON'T must overwrite already set itemTextId, in other cases it will work and with mixed cases but this will be not normal way use.
        MailDraft& SetSubjectAndBodyId(const std::string& subject, uint32 itemTextId) { m_subject = subject; MANGOS_ASSERT(!m_bodyId); m_bodyId = itemTextId; return *this; }
        MailDraft& SetSubjectAndBody(const std::string& subject, const std::string& text);
        MailDraft& SetMailTemplate(uint16 mailTemplateId, bool need_items = true) { m_mailTemplateId = mailTemplateId, m_mailTemplateItemsNeed = need_items; return *this; }

        MailDraft& AddItem(Item* item);
        /**
         * Modifies the amount of money in a MailDraft.
         *
         * @param money The amount of money included in this MailDraft.
         */
        MailDraft& SetMoney(uint32 money) { m_money = money; return *this; }
        /**
         * Modifies the cost of delivery of the MailDraft.
         *
         * @param COD the amount to which the cod should be set.
         */
        MailDraft& SetCOD(uint32 COD) { m_COD = COD; return *this; }

        void CloneFrom(MailDraft const& draft);
    public:                                                 // finishers
        void SendReturnToSender(uint32 sender_acc, ObjectGuid sender_guid, ObjectGuid receiver_guid);
        void SendMailTo(MailReceiver const& receiver, MailSender const& sender, MailCheckMask checked = MAIL_CHECK_MASK_NONE, uint32 deliver_delay = 0);
    private:
        MailDraft(MailDraft const&);                        // trap decl, no body, mail draft must cloned only explicitly...
        MailDraft& operator=(MailDraft const&);             // trap decl, no body, ...because items clone is high price operation

        void deleteIncludedItems(bool inDB = false);
        bool prepareItems(Player* receiver);                ///< called from SendMailTo for generate mailTemplateBase items

        /// The ID of the template associated with this MailDraft.
        uint16      m_mailTemplateId;
        /// Boolean specifying whether items are required or not.
        bool        m_mailTemplateItemsNeed;
        /// The subject of the MailDraft.
        std::string m_subject;
        /// The ID of the body of the MailDraft.
        uint32      m_bodyId;
        /// A map of items in this MailDraft.
        MailItemMap m_items;                                ///< Keep the items in a map to avoid duplicate guids (which can happen), store only low part of guid

        /// The amount of money in this MailDraft.
        uint32 m_money;
        /// The cod amount of this MailDraft.
        uint32 m_COD;
};
/**
 * Structure holding information about an item in the mail.
 */
struct MailItemInfo
{
    uint32 item_guid;                                       ///< the GUID of the item.
    uint32 item_template;                                   ///< the ID of the template of the item.
};

typedef std::vector<MailItemInfo> MailItemInfoVec;
/**
 * Structure that holds an actual mail. (邮箱中已投递的单封邮件运行时实体)
 */
struct Mail
{
    /// the ID of the message contained in the mail.
    uint32 messageID;
    /// the type of the message
    uint8 messageType;
    /// the stationary used in this mail.
    uint8 stationery;
    /// the ID of the template this mail is based on.
    uint16 mailTemplateId;
    /// the LowGUID of the player that sent this mail, or creature low guid, or other id
    uint32 sender;
    /// the GUID of the player that this mail is sent to.
    ObjectGuid receiverGuid;
    /// the subject of the mail
    std::string subject;
    /// The ID of the itemtext.
    uint32 itemTextId;
    /// flag mark mail that already has items, or already generate none items for template
    bool has_items;
    /// A vector containing Information about the items in this mail.
    MailItemInfoVec items;
    /// A vector containing Information about the items that where already take from this mail.
    std::vector<uint32> removedItems;
    /// The time at which this mail will expire
    time_t expire_time;
    /// The time at which this mail (was/will be) delivered
    time_t deliver_time;
    /// The amount of money contained in this mail.
    uint32 money;
    /// The amount of money the receiver has to pay to get this mail.
    uint32 COD;
    /// The time at which this mail was read.
    uint32 checked;
    /// The state of this mail.
    MailState state;

    /**
     * Adds an item to the mail.
     * This method adds an item to mail represented by this structure.
     * There is no checking done whether this is a legal action or not; it is up
     * to the caller to make sure there is still room for more items in the mail.
     *
     * @param itemGuidLow      the GUID(low) of the item added to the mail.
     * @param item_template the ID of the template of the item.
     *
     */
    void AddItem(uint32 itemGuidLow, uint32 item_template)
    {
        MailItemInfo mii;
        mii.item_guid = itemGuidLow;
        mii.item_template = item_template;
        items.push_back(mii);
        has_items = true;
    }

    /**
     * Removes an item from the mail.
     * This method removes an item from the mail.
     *
     * @see MailItemInfo
     *
     * @param item_guid        The GUID of the item to be removed.
     * @returns true if the item was removed, or false if no item with that GUID was found.
     *
     */
    bool RemoveItem(uint32 item_guid)
    {
        for (MailItemInfoVec::iterator itr = items.begin(); itr != items.end(); ++itr)
        {
            if (itr->item_guid == item_guid)
            {
                items.erase(itr);
                return true;
            }
        }
        return false;
    }

    /*
     * Checks whether a mail contains items (including case none items generated from template already) or not.
     * HasItems() checks whether the mail contains items or not.
     *
     * @returns true if the mail contains items or template items already generated possible none, false otherwise.
     *
     */
    bool HasItems() const { return has_items; }

    /*
     * Generate items for template if items not genereated before (receiver has been offline, has_items == false)
     *
     */
    void prepareTemplateItems(Player* receiver);            ///< called from _LoadMails for generate mailTemplateBase items not generated for offline player
};

#endif
/*! @} */
