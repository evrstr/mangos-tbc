/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
* This program is free software licensed under GPL version 2
* Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_SYSTEM_H
#define SC_SYSTEM_H

extern std::string  strSD2Version;                          // version info: database entry and revision (数据库版本与修订信息)

#define TEXT_SOURCE_RANGE -1000000                          // the amount of entries each text source has available (每个文本源保留的 Entry ID 范围区间)

// 核心脚本文本 ID 起止区间 (script_texts)
#define TEXT_SOURCE_TEXT_START      TEXT_SOURCE_RANGE
#define TEXT_SOURCE_TEXT_END        TEXT_SOURCE_RANGE*2 + 1

// 自定义脚本文本 ID 起止区间 (custom_texts)
#define TEXT_SOURCE_CUSTOM_START    TEXT_SOURCE_RANGE*2
#define TEXT_SOURCE_CUSTOM_END      TEXT_SOURCE_RANGE*3 + 1

// 对话菜单文本 ID 起止区间 (gossip_texts)
#define TEXT_SOURCE_GOSSIP_START    TEXT_SOURCE_RANGE*3
#define TEXT_SOURCE_GOSSIP_END      TEXT_SOURCE_RANGE*4 + 1

#define pSystemMgr SystemMgr::Instance()                    // SystemMgr 单例访问宏

// 路径信息结构体
struct PathInformation
{
    uint32 lastWaypoint;                                    // 最终路径点编号
};

// 系统数据管理器：负责从数据库加载版本、各类脚本文本和路径点数据
class SystemMgr
{
    public:
        SystemMgr();
        ~SystemMgr() {}

        static SystemMgr& Instance();                       // 获取单例实例

        typedef std::map < uint32 /*entry*/, std::map < int32 /*pathId*/, PathInformation > > EntryPathInfo;

        // Database (数据库加载函数)
        void LoadVersion();                                 // 加载 SD2 数据库版本
        void LoadScriptTexts();                             // 从 script_texts 表加载通用脚本文本
        void LoadScriptTextsCustom();                       // 从 custom_texts 表加载自定义文本
        void LoadScriptGossipTexts();                       // 从 gossip_texts 表加载对话文本
        void LoadScriptWaypoints();                         // 从 script_waypoint 表加载脚本路径点数据
};

#endif
