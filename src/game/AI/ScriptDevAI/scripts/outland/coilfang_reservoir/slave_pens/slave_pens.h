/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
* This program is free software licensed under GPL version 2
* Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SLAVE_PENS_H

// 奴隶围栏副本数据头文件
/* 遭遇战概述：
 * 定义奴隶围栏首领、对象与门禁数据。
 */
#define SLAVE_PENS_H

// 相关常量、法术与文本条目枚举
enum
{
    NPC_NATURALIST_BITE     = 17893,

    NPC_QUAGMIRRAN = 17942,

    DATA_NATURALIST = 0,
};

// instance_slave_pens 副本数据管理类
class instance_slave_pens : public ScriptedInstance
{
    public:
        instance_slave_pens(Map* map);
        // 初始化副本数据与状态
        void Initialize() override;

        // 设置遭遇战状态数据
        void SetData(uint32 type, uint32 data) override;
        // 获取遭遇战状态数据
        uint32 GetData(uint32 type) const override;

        // 副本生物创建登记
        void OnCreatureCreate(Creature* creature) override;

        void Update(const uint32 diff) override;

    private:
        bool m_naturalistYelled;

        uint32 m_quagmirranTimer;
};

#endif
