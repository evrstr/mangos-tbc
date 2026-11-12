/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_BREWFEST_H
#define DEF_BREWFEST_H

#include "AI/ScriptDevAI/base/TimerAI.h"

// 美酒节相关 NPC 编号枚举
enum
{
    // Kalimdor (卡利姆多美酒节 NPC)
    NPC_BLIX_FIXWIDGET                  = 24495,            // 布利克斯·菲克斯维吉特
    NPC_DROHNS_DISTILLERY_BARKER        = 24492,            // 德罗恩酿酒厂宣传员
    NPC_TCHALIS_VOODOO_BREWERY_BARKER   = 24493,            // 查利的巫毒酿酒厂宣传员
    NPC_GORDOK_BREW_BARKER_H            = 23685,            // 戈多克美酒宣传员 (部落)
    NPC_TAPPER_SWINDLEKEG               = 24711,            // 塔珀·诈桶
    NPC_VOLJIN                          = 10540,            // 沃金

    // EK (东部王国美酒节 NPC)
    NPC_BELBI_QUIKSWITCH        = 23710,                    // 贝尔比·快捷开关
    NPC_ITA_THUNDERBREW         = 23684,                    // 伊塔·雷酒
    NPC_MAEVE_BARLEYBREW        = 23683,                    // 梅芙·麦芽酒
    NPC_GORDOK_BREW_BARKER      = 23685,                    // 戈多克美酒宣传员 (联盟)
    NPC_IPFELKOFER_IRONKEG      = 24710,                    // 伊普费尔科弗·铁桶
    NPC_MEKKATORQUE             = 7937,                     // 格尔宾·梅卡托克
    
    // Both (双阵营共有)
    NPC_DARK_IRON_HERALD = 24536,                           // 黑铁传令官
};

class ScriptedInstance;

// 美酒节事件管理器：负责敲酒桶与黑铁矮人进攻事件的计时与调度
class BrewfestEvent : public TimerManager
{
    public:
        BrewfestEvent(ScriptedInstance* instance);

        void Update(const uint32 diff);                     // 周期更新事件计时器
        void StartKegTappingEvent();                        // 开启敲酒桶仪式事件
        void StartDarkIronAttackEvent();                    // 开启黑铁矮人进攻事件

    private:
        void HandleKegTappingEvent();                       // 处理敲酒桶仪式各阶段
        
        ScriptedInstance* m_instance;                       // 对应副本地图实例
        uint32 m_kegTappingStage;                           // 敲酒桶阶段
        bool m_kalimdor;                                    // 是否为卡利姆多场地
};

#endif
