/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_MANA_TOMBS_H
#define DEF_MANA_TOMBS_H

// 法力陵墓副本数据管理类
class instance_mana_tombs : public ScriptedInstance
{
    public:
        instance_mana_tombs(Map* pMap);
        ~instance_mana_tombs() {}

        void Initialize() override;                         // 初始化副本状态与世界状态变量
};

#endif
