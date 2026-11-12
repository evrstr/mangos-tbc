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

#ifndef MANGOSSERVER_GOSSIP_H
#define MANGOSSERVER_GOSSIP_H

#include "Common.h"
#include "Quests/QuestDef.h"
#include "Entities/NPCHandler.h"
#include "Entities/ObjectGuid.h"
#include "Globals/SharedDefines.h"

class WorldSession;

#define GOSSIP_MAX_MENU_ITEMS       32                      // client supports showing max 32 items (客户端单页对话菜单最多支持显示 32 个选项)
#define DEFAULT_GOSSIP_MESSAGE      0xffffff                // 默认 NPC 问候文本 ID

// NPC 对话选项功能类型枚举
enum Gossip_Option
{
    GOSSIP_OPTION_NONE              = 0,                    // UNIT_NPC_FLAG_NONE               (0)
    GOSSIP_OPTION_GOSSIP            = 1,                    // UNIT_NPC_FLAG_GOSSIP             (1) (普通闲聊/分支对话)
    GOSSIP_OPTION_QUESTGIVER        = 2,                    // UNIT_NPC_FLAG_QUESTGIVER         (2) (任务发布)
    GOSSIP_OPTION_VENDOR            = 3,                    // UNIT_NPC_FLAG_VENDOR             (128) (商人交易)
    GOSSIP_OPTION_TAXIVENDOR        = 4,                    // UNIT_NPC_FLAG_TAXIVENDOR         (8192) (飞行管理员)
    GOSSIP_OPTION_TRAINER           = 5,                    // UNIT_NPC_FLAG_TRAINER            (16) (技能训练师)
    GOSSIP_OPTION_SPIRITHEALER      = 6,                    // UNIT_NPC_FLAG_SPIRITHEALER       (16384) (灵魂医者)
    GOSSIP_OPTION_SPIRITGUIDE       = 7,                    // UNIT_NPC_FLAG_SPIRITGUIDE        (32768) (战场灵魂向导)
    GOSSIP_OPTION_INNKEEPER         = 8,                    // UNIT_NPC_FLAG_INNKEEPER          (65536) (旅店老板绑定炉石)
    GOSSIP_OPTION_BANKER            = 9,                    // UNIT_NPC_FLAG_BANKER             (131072) (银行职员)
    GOSSIP_OPTION_PETITIONER        = 10,                   // UNIT_NPC_FLAG_PETITIONER         (262144) (公会/战队登记表)
    GOSSIP_OPTION_TABARDDESIGNER    = 11,                   // UNIT_NPC_FLAG_TABARDDESIGNER     (524288) (战袍设计师)
    GOSSIP_OPTION_BATTLEFIELD       = 12,                   // UNIT_NPC_FLAG_BATTLEFIELDPERSON  (1048576) (战场军官)
    GOSSIP_OPTION_AUCTIONEER        = 13,                   // UNIT_NPC_FLAG_AUCTIONEER         (2097152) (拍卖师)
    GOSSIP_OPTION_STABLEPET         = 14,                   // UNIT_NPC_FLAG_STABLE             (4194304) (兽栏管理员)
    GOSSIP_OPTION_ARMORER           = 15,                   // UNIT_NPC_FLAG_ARMORER            (4096) (护甲修理商)
    GOSSIP_OPTION_UNLEARNTALENTS    = 16,                   // UNIT_NPC_FLAG_TRAINER            (16) (bonus option for GOSSIP_OPTION_TRAINER) (遗忘天赋)
    GOSSIP_OPTION_UNLEARNPETSKILLS  = 17,                   // UNIT_NPC_FLAG_TRAINER            (16) (bonus option for GOSSIP_OPTION_TRAINER) (遗忘宠物技能)
    GOSSIP_OPTION_BOT               = 99,                   // UNIT_NPC_FLAG_GOSSIP             (1) UNUSED (just for bot system) (机器人系统专用选项)

    // Custom ones for SD2 system communication (ScriptDev2 自定义脚本通信选项)
    GOSSIP_OPTION_SD2_1             = 100,
    GOSSIP_OPTION_SD2_2             = 101,
    GOSSIP_OPTION_SD2_3             = 102,
    GOSSIP_OPTION_SD2_4             = 103,
    GOSSIP_OPTION_SD2_5             = 104,
    GOSSIP_OPTION_MAX
};

// 对话选项左侧图标枚举
enum GossipOptionIcon
{
    GOSSIP_ICON_CHAT                = 0,                    // white chat bubble (白色聊天气泡)
    GOSSIP_ICON_VENDOR              = 1,                    // brown bag (棕色钱袋/商人)
    GOSSIP_ICON_TAXI                = 2,                    // flight (双翼/飞行点)
    GOSSIP_ICON_TRAINER             = 3,                    // book (书本/训练师)
    GOSSIP_ICON_INTERACT_1          = 4,                    // interaction wheel (交互齿轮 1)
    GOSSIP_ICON_INTERACT_2          = 5,                    // interaction wheel (交互齿轮 2)
    GOSSIP_ICON_MONEY_BAG           = 6,                    // brown bag with yellow dot (带黄点的钱袋)
    GOSSIP_ICON_TALK                = 7,                    // white chat bubble with black dots (带黑点的聊天气泡)
    GOSSIP_ICON_TABARD              = 8,                    // tabard (战袍图标)
    GOSSIP_ICON_BATTLE              = 9,                    // two swords (交叉双剑/战场)
    GOSSIP_ICON_DOT                 = 10,                   // yellow dot (黄色圆点)
    GOSSIP_ICON_CHAT_11             = 11,                   // This and below are most the same visual as GOSSIP_ICON_CHAT (以下大多视觉效果与聊天气泡相同)
    GOSSIP_ICON_CHAT_12             = 12,                   // but are still used for unknown reasons.
    GOSSIP_ICON_CHAT_13             = 13,
    GOSSIP_ICON_CHAT_14             = 14,                   // probably invalid
    GOSSIP_ICON_CHAT_15             = 15,                   // probably invalid
    GOSSIP_ICON_CHAT_16             = 16,
    GOSSIP_ICON_CHAT_17             = 17,
    GOSSIP_ICON_CHAT_18             = 18,
    GOSSIP_ICON_CHAT_19             = 19,
    GOSSIP_ICON_CHAT_20             = 20,
    GOSSIP_ICON_MAX
};

// POI icons. Many more exist, list not complete. (小地图兴趣点 POI 图标枚举)
enum Poi_Icon
{
    ICON_POI_GREY_AV_MINE       =   0,                      // Grey mine lorry (灰色矿车)
    ICON_POI_RED_AV_MINE        =   1,                      // Red mine lorry (红色矿车)
    ICON_POI_BLUE_AV_MINE       =   2,                      // Blue mine lorry (蓝色矿车)
    ICON_POI_BWTOMB             =   3,                      // Blue and White Tomb Stone (蓝白墓碑)
    ICON_POI_SMALL_HOUSE        =   4,                      // Small house (小屋)
    ICON_POI_GREYTOWER          =   5,                      // Grey Tower (灰色哨塔)
    ICON_POI_REDFLAG            =   6,                      // Red Flag w/Yellow ! (红底黄叹号旗帜)
    ICON_POI_TOMBSTONE          =   7,                      // Normal tomb stone (brown) (普通棕色墓碑)
    ICON_POI_BWTOWER            =   8,                      // Blue and White Tower (蓝白哨塔)
    ICON_POI_REDTOWER           =   9,                      // Red Tower (红色哨塔)
    ICON_POI_BLUETOWER          =   10,                     // Blue Tower (蓝色哨塔)
    ICON_POI_RWTOWER            =   11,                     // Red and White Tower (红白哨塔)
    ICON_POI_REDTOMB            =   12,                     // Red Tomb Stone (红色墓碑)
    ICON_POI_RWTOMB             =   13,                     // Red and White Tomb Stone (红白墓碑)
    ICON_POI_BLUETOMB           =   14,                     // Blue Tomb Stone (蓝色墓碑)
    ICON_POI_BLANK              =   15,                     // Blank (not visible) (空白不可见)
    ICON_POI_16                 =   16,                     // Grey ?
    ICON_POI_17                 =   17,                     // Blue/White ?
    ICON_POI_18                 =   18,                     // Blue ?
    ICON_POI_19                 =   19,                     // Red and White ?
    ICON_POI_20                 =   20,                     // Red ?
    ICON_POI_GREYLOGS           =   21,                     // Grey Wood Logs (灰色原木)
    ICON_POI_BWLOGS             =   22,                     // Blue and White Wood Logs (蓝白原木)
    ICON_POI_BLUELOGS           =   23,                     // Blue Wood Logs (蓝色原木)
    ICON_POI_RWLOGS             =   24,                     // Red and White Wood Logs (红白原木)
    ICON_POI_REDLOGS            =   25,                     // Red Wood Logs (红色原木)
    ICON_POI_26                 =   26,                     // Grey ?
    ICON_POI_27                 =   27,                     // Blue and White ?
    ICON_POI_28                 =   28,                     // Blue ?
    ICON_POI_29                 =   29,                     // Red and White ?
    ICON_POI_30                 =   30,                     // Red ?
    ICON_POI_GREYHOUSE          =   31,                     // Grey House (灰色房屋)
    ICON_POI_BWHOUSE            =   32,                     // Blue and White House (蓝白房屋)
    ICON_POI_BLUEHOUSE          =   33,                     // Blue House (蓝色房屋)
    ICON_POI_RWHOUSE            =   34,                     // Red and White House (红白房屋)
    ICON_POI_REDHOUSE           =   35,                     // Red House (红色房屋)
    ICON_POI_GREYHORSE          =   36,                     // Grey Horse (灰色马匹)
    ICON_POI_BWHORSE            =   37,                     // Blue and White Horse (蓝白马匹)
    ICON_POI_BLUEHORSE          =   38,                     // Blue Horse (蓝色马匹)
    ICON_POI_RWHORSE            =   39,                     // Red and White Horse (红白马匹)
    ICON_POI_REDHORSE           =   40                      // Red Horse (红色马匹)
};

// 单条对话菜单显示项数据
struct GossipMenuItem
{
    uint8       m_gIcon;                                    // 选项图标 ID
    bool        m_gCoded;                                   // 是否弹出输入密码/口令框
    std::string m_gMessage;                                 // 选项显示文本
    uint32      m_gSender;                                  // 回调发送者标识（通常对应 Gossip_Option）
    uint32      m_gOptionId;                                // 回调动作/选项 ID
    std::string m_gBoxMessage;                              // 确认弹出框提示文本
    uint32      m_gBoxMoney;                                // 点击该选项所需消耗的金币（铜币单位）
};

typedef std::vector<GossipMenuItem> GossipMenuItemList;

// 数据库驱动的对话菜单附加动作数据
struct GossipMenuItemData
{
    int32  m_gAction_menu;                                  // negative for close gossip (跳转的目标子菜单 ID，负数表示关闭对话)
    uint32 m_gAction_poi;                                   // 触发显示的小地图兴趣点 POI ID
    uint32 m_gAction_script;                                // 触发的数据库脚本 ID
};

typedef std::vector<GossipMenuItemData> GossipMenuItemDataList;

// 任务菜单项数据
struct QuestMenuItem
{
    uint32      m_qId;                                      // 任务 ID
    uint8       m_qIcon;                                    // 任务状态图标（感叹号/问号等）
};

typedef std::vector<QuestMenuItem> QuestMenuItemList;

// NPC 对话菜单容器（存储当前打开的 Gossip 选项与数据库附加动作）
class GossipMenu
{
    public:
        explicit GossipMenu(WorldSession* session);
        ~GossipMenu();

        // 添加纯文本或带交互参数/确认框的对话选项
        void AddMenuItem(uint8 Icon, const std::string& Message, bool Coded = false);
        void AddMenuItem(uint8 Icon, const std::string& Message, uint32 dtSender, uint32 dtAction, const std::string& BoxMessage, uint32 BoxMoney, bool Coded = false);

        // for using from scripts, don't must be inlined (供脚本模块调用的 C 字符串重载，不内联以保持 ABI 边界)
        void AddMenuItem(uint8 Icon, char const* Message, bool Coded = false);
        void AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, char const* BoxMessage, uint32 BoxMoney, bool Coded = false);

        // 通过 mangos_string 表或 broadcast_text 表本地化文本添加对话选项
        void AddMenuItem(uint8 Icon, int32 itemText, uint32 dtSender, uint32 dtAction, int32 boxText, uint32 BoxMoney, bool Coded = false);
        void AddMenuItemBct(uint8 Icon, int32 itemText, Gender gender, uint32 dtSender, uint32 dtAction, int32 boxText, uint32 BoxMoney, bool Coded = false);

        void SetMenuId(uint32 menu_id) { m_gMenuId = menu_id; }
        uint32 GetMenuId() const { return m_gMenuId; }

        // used to avoid opening gossip menu at node discover (标记刚发现飞行点，避免同时弹出闲聊菜单)
        void SetDiscoveredNode() { m_discoveredNode = true; }
        bool IsJustDiscoveredNode() const { return m_discoveredNode; }

        // 记录数据库菜单项的跳转/POI/脚本元数据
        void AddGossipMenuItemData(int32 action_menu, uint32 action_poi, uint32 action_script);

        unsigned int MenuItemCount() const
        {
            return m_gItems.size();
        }

        bool Empty() const
        {
            return m_gItems.empty();
        }

        GossipMenuItem const& GetItem(unsigned int Id)
        {
            return m_gItems[ Id ];
        }

        bool IsItemsDataEmpty() const { return m_gItemsData.empty(); }

        GossipMenuItemData const* GetItemData(unsigned int indexId)
        {
            if (indexId >= m_gItemsData.size())
            {
                sLog.outError("GossipMenu::GetItemData> indexId is out of bounds!");
                return nullptr;
            }
            return &m_gItemsData[indexId];
        }

        // 查询指定索引菜单项的 Sender、Action 与 Coded 属性
        uint32 MenuItemSender(unsigned int ItemId);
        uint32 MenuItemAction(unsigned int ItemId);
        bool MenuItemCoded(unsigned int ItemId);

        void ClearMenu();

        WorldSession* GetMenuSession() const { return m_session; }

    protected:
        GossipMenuItemList      m_gItems;                   // 对话菜单选项列表
        GossipMenuItemDataList  m_gItemsData;               // 对应的数据库跳转/POI/脚本列表

        uint32 m_gMenuId;                                   // 当前菜单 ID
        bool m_discoveredNode;                              // 是否刚发现新飞行点

    private:
        WorldSession* m_session;                            // 所属玩家会话
};

// 任务交互菜单容器（存储当前 NPC/物品可接或可交的任务条目）
class QuestMenu
{
    public:
        QuestMenu();
        ~QuestMenu();

        void AddMenuItem(uint32 QuestId, uint8 Icon);
        void ClearMenu();

        uint8 MenuItemCount() const
        {
            return m_qItems.size();
        }

        bool Empty() const
        {
            return m_qItems.empty();
        }

        bool HasItem(uint32 questid);

        QuestMenuItem const& GetItem(uint16 Id)
        {
            return m_qItems[ Id ];
        }

    protected:
        QuestMenuItemList m_qItems;                         // 任务菜单列表
};

// 玩家交互菜单聚合类（同时持有 GossipMenu 与 QuestMenu，并负责向客户端组包发送对话及任务界面）
class PlayerMenu
{
    private:
        GossipMenu mGossipMenu;                             // 对话子菜单
        QuestMenu  mQuestMenu;                              // 任务子菜单

    public:
        explicit PlayerMenu(WorldSession* session);
        ~PlayerMenu();

        GossipMenu& GetGossipMenu() { return mGossipMenu; }
        QuestMenu& GetQuestMenu() { return mQuestMenu; }

        WorldSession* GetMenuSession() const { return mGossipMenu.GetMenuSession(); }

        bool Empty() const { return mGossipMenu.Empty() && mQuestMenu.Empty(); }

        void ClearMenus();
        uint32 GossipOptionSender(unsigned int Selection);
        uint32 GossipOptionAction(unsigned int Selection);
        bool GossipOptionCoded(unsigned int Selection);

        // 发送/关闭 Gossip 对话窗口，以及发送小地图兴趣点标记
        void SendGossipMenu(uint32 TitleTextId, ObjectGuid objectGuid);
        void CloseGossip() const;
        void SendPointOfInterest(float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, const char* locName) const;
        void SendPointOfInterest(uint32 poi_id) const;

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /***                    任务系统界面封包               ***/
        /*********************************************************/
        void SendQuestGiverStatus(uint8 questStatus, ObjectGuid npcGUID) const;

        void SendQuestGiverQuestList(QEmote eEmote, const std::string& Title, ObjectGuid npcGUID);

        void SendQuestGiverQuestDetails(Quest const* pQuest, ObjectGuid guid, bool ActivateAccept) const;

        void SendQuestGiverOfferReward(Quest const* pQuest, ObjectGuid npcGUID, bool EnableNext) const;
        void SendQuestGiverRequestItems(Quest const* pQuest, ObjectGuid npcGUID, bool Completable, bool CloseOnCancel) const;
};
#endif
