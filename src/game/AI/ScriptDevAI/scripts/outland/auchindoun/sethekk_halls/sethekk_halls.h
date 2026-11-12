/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_SETHEKK_HALLS_H
#define DEF_SETHEKK_HALLS_H

// 塞泰克大厅首领与对象枚举
enum
{
    MAX_ENCOUNTER               = 3,                            // 遭遇战总数

    TYPE_SYTH                   = 0,                            // 暗织者斯瑟
    TYPE_ANZU                   = 1,                            // 安苏（乌鸦之神）
    TYPE_IKISS                  = 2,                            // 利爪之王艾吉斯

    NPC_ANZU                    = 23035,                        // 安苏

    NPC_BROOD_OF_ANZU           = 23132,                        // 安苏之子

    NPC_INVIS_RAVEN_GOD_PORTAL  = 23046,                        // 隐形乌鸦之神传送门
    NPC_INVIS_RAVEN_GOD_TARGET  = 23057,                        // 隐形乌鸦之神目标
    NPC_INVIS_RAVEN_GOD_CASTER  = 23058,                        // 隐形乌鸦之神施法者

    GO_IKISS_SWING_DOOR         = 183398,                       // 艾吉斯侧门
    GO_IKISS_CHEST              = 187372,                       // 艾吉斯的宝箱
    GO_RAVENS_CLAW              = 185554,                       // 乌鸦之爪
    GO_MOONSTONE                = 185590,                       // 灌魔月亮石
    GO_TEST_RIFT                = 185595,                       // 裂隙测试对象

    // possible spells used for Anzu summoning event (安苏召唤事件法术)
    SPELL_PORTAL                = 39952,                        // 异界传送门
    SPELL_SUMMONING_BEAMS       = 39978,                        // 召唤光束
    SPELL_RED_LIGHTNING         = 39990,                        // 红色闪电
};

// 塞泰克大厅副本数据管理类
class instance_sethekk_halls : public ScriptedInstance
{
    public:
        instance_sethekk_halls(Map* pMap);
        ~instance_sethekk_halls() {}

        void Initialize() override;                             // 初始化副本数据与遭遇战状态

        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建登记
        void OnObjectCreate(GameObject* pGo) override;          // 游戏对象创建登记

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置遭遇战状态数据
        uint32 GetData(uint32 uiType) const override;           // 获取遭遇战状态数据

        const char* Save() const override { return m_strInstData.c_str(); } // 存盘数据字符串
        void Load(const char* chrIn) override;                  // 读取副本存盘数据

        void Update(const uint32 diff) override;                // 副本全局帧更新
        void StartAnzuIntro(Player* player);                    // 启动安苏召唤过场动画
        void FinishAnzuIntro();                                 // 完成安苏召唤过场动画
    private:

        uint32 m_auiEncounter[MAX_ENCOUNTER];                   // 遭遇战进度数组
        std::string m_strInstData;                              // 副本数据存储字符串

        uint32 m_anzuTimer;                                     // 安苏过场计时器
        uint32 m_anzuStage;                                     // 安苏过场阶段计数
};

#endif
