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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef WORLD_PVP_SI
#define WORLD_PVP_SI

#include "Common.h"
#include "OutdoorPvP.h"
#include "Tools/Language.h"
#include "World/WorldStateDefines.h"

// 希利苏斯（Silithus）水晶尘收集野外 PvP 常量定义
enum
{
    // npcs (任务完成计数用虚拟生物)
    NPC_SILITHUS_DUST_QUEST_ALLIANCE    = 17090,        // dummy npcs for quest credit (联盟任务计数虚拟 NPC)
    NPC_SILITHUS_DUST_QUEST_HORDE       = 18199,        // 部落任务计数虚拟 NPC

    // game objects (水晶尘土堆与水晶喷泉)
    GO_SILITHYST_MOUND                  = 181597,       // created when a player drops the flag (玩家掉落水晶尘后生成的土堆)
    GO_SILITHYST_GEYSER                 = 181598,       // spawn on the map by default (地图默认刷新的水晶喷泉)

    // spells (水晶尘携带有益/减益光环与塞纳里奥青睐区域增益)
    //SPELL_SILITHYST_OBJECT            = 29518,        // unknown, related to the GO (未知，与水晶对象相关)
    SPELL_SILITHYST                     = 29519,        // buff received when you are carrying a silithyst (携带水晶尘时获得的光环)
    SPELL_TRACES_OF_SILITHYST           = 29534,        // individual buff received when successfully delivered a silithyst (成功上交水晶尘后获得的个人增益)
    SPELL_CENARION_FAVOR                = 30754,        // zone buff received when a team gathers 200 silithyst (某阵营集满 200 份水晶尘后获得的区域增益)
    SPELL_SILITHYST_FLAG_DROP           = 29533,        // drop the flag (丢弃水晶尘触发法术)

    // quests (水晶尘上交任务)
    QUEST_SCOURING_DESERT_ALLIANCE      = 9419,
    QUEST_SCOURING_DESERT_HORDE         = 9422,

    // area triggers (双方营地上交水晶尘的区域触发器)
    AREATRIGGER_SILITHUS_ALLIANCE       = 4162,
    AREATRIGGER_SILITHUS_HORDE          = 4168,

    // misc (塞纳里奥议会声望、荣誉奖励与获胜上限)
    FACTION_CENARION_CIRCLE             = 609,
    HONOR_REWARD_SILITHYST              = 19,
    REPUTATION_REWARD_SILITHYST         = 20,
    MAX_SILITHYST                       = 200,
};

// 希利苏斯野外 PvP 控制器：管理水晶尘采集、上交、掉落土堆及 200 点满额后的区域增益
class OutdoorPvPSI : public OutdoorPvP
{
    public:
        OutdoorPvPSI();

        void HandlePlayerEnterZone(Player* player, bool isMainZone) override;
        void HandlePlayerLeaveZone(Player* player, bool isMainZone) override;
        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;

        bool HandleAreaTrigger(Player* player, uint32 triggerId) override;
        bool HandleGameObjectUse(Player* player, GameObject* go) override;
        bool HandleDropFlag(Player* player, uint32 spellId) override;

    private:
        uint8 m_resourcesAlliance;
        uint8 m_resourcesHorde;
        Team m_zoneOwner;
};

#endif
