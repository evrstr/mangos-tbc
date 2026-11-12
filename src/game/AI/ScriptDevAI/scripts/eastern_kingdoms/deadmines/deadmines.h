/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 死亡矿井（Deadmines）副本全局定义头文件

#ifndef DEF_DEADMINES_H                                     // 防止头文件重复包含
#define DEF_DEADMINES_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER           = 4,                            // encounter 标识 ID

    TYPE_RHAHKZOR           = 0,                            // 拉克佐 遭遇战进度数据标识 ID
    TYPE_SNEED              = 1,                            // 斯尼德 遭遇战进度数据标识 ID
    TYPE_GILNID             = 2,                            // 基尔尼格 遭遇战进度数据标识 ID
    TYPE_IRON_CLAD_DOOR     = 3,                            // 黑铁包铁门 遭遇战进度数据标识 ID

    INST_SAY_ALARM1         = -1036000,                     // 台词alarm1 标识 ID
    INST_SAY_ALARM2         = -1036001,                     // 台词alarm2 标识 ID

    GO_FACTORY_DOOR         = 13965,                        // rhahk'zor (拉克佐)
    GO_MAST_ROOM_DOOR       = 16400,                        // sneed (斯尼德)
    GO_FOUNDRY_DOOR         = 16399,                        // gilnid (基尔尼格)
    GO_HEAVY_DOOR_1         = 17153,                        // to sneed (通往 sneed)
    GO_HEAVY_DOOR_2         = 17154,                        // to gilnid (通往 gilnid)
    GO_IRON_CLAD_DOOR       = 16397,                        // 黑铁包铁门 游戏对象 Entry ID
    GO_DEFIAS_CANNON        = 16398,                        // defias大炮 游戏对象 Entry ID
    GO_SMITE_CHEST          = 144111,                       // use to get correct location of mr.smites equipment changes
    GO_MYSTERIOUS_CHEST     = 180024,                       // used for quest 7938; spawns in the instance only if one of the players has the quest (用于任务 7938)

    NPC_RHAHKZOR            = 644,                          // 拉克佐 Entry ID
    NPC_SNEED               = 643,                          // 斯尼德 Entry ID
    NPC_GILNID              = 1763,                         // 基尔尼格 Entry ID
    NPC_MR_SMITE            = 646,                          // mr重拳先生 Entry ID
    NPC_PIRATE              = 657,                          // 海盗 Entry ID
    NPC_SQUALLSHAPER        = 1732,                         // 唤风者 Entry ID

    QUEST_FORTUNE_AWAITS    = 7938,                         // 命运等待 任务 ID

    GUID_PREFIX = 3600000,                                  // prefix 标识 ID
};

// 副本进度与状态机管理类：标识 ID（instance_deadmines）
class instance_deadmines : public ScriptedInstance
{
    public:
        instance_deadmines(Map* pMap);

        // 初始化副本数据结构
        void Initialize() override;

        // 玩家进入副本区域回调
        void OnPlayerEnter(Player* pPlayer) override;

        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;

        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        void SpawnFirstDeadminesPatrol();
        void SpawnSecondDeadminesPatrol();
        void SpawnThirdDeadminesPatrol();

    private:
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        uint32 m_uiIronDoorTimer;                           // IronDoor 技能计时器
        uint32 m_uiDoorStep;                                // 大门剧情推进步骤
        bool m_firstEnter;                                  // 是否首次进入副本标记
};

#endif                                                      // 结束头文件防重复包含宏
