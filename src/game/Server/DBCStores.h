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

#ifndef MANGOS_DBCSTORES_H
#define MANGOS_DBCSTORES_H

#include "Common.h"
#include "Database/DBCStore.h"
#include "Server/DBCStructure.h"

#include <list>
#include <vector>

/// 检查客户端构建版本号是否在服务器允许列表内
bool IsAcceptableClientBuild(uint32 build);
/// 返回服务器可接受客户端版本号的逗号分隔字符串
std::string AcceptableClientBuildsListStr();

/// 获取指定宠物家族与语言区域对应的默认宠物名称
char const* GetPetName(uint32 petfamily, uint32 dbclang);
/// 获取学习指定天赋法术所需的天赋点数消耗
uint32 GetTalentSpellCost(uint32 spellId);
uint32 GetTalentSpellCost(TalentSpellPos const* pos);
/// 根据法术 ID 查询其在天赋树中的位置信息
TalentSpellPos const* GetTalentSpellPos(uint32 spellId);

int32 GetAreaFlagByAreaID(uint32 area_id);                  // -1 if not found | 根据区域 ID 查询 AreaFlag，未找到返回 -1
/// 根据地图 ID 查询默认 AreaFlag
uint32 GetAreaFlagByMapId(uint32 mapid);

/// 根据三元组 (rootid, adtid, groupid) 查询对应的 WMO 室内区域表条目集合
std::vector<WMOAreaTableEntry const*>& GetWMOAreaTableEntriesByTripple(int32 rootid, int32 adtid, int32 groupid);

/// 根据区域 ID 获取 AreaTableEntry
AreaTableEntry const* GetAreaEntryByAreaID(uint32 area_id);
/// 根据 AreaFlag 与地图 ID 获取 AreaTableEntry
AreaTableEntry const* GetAreaEntryByAreaFlagAndMap(uint32 area_flag, uint32 map_id);

/// 获取指定地图与区域对应的虚拟显示地图 ID（如副本内显示外域主地图）
uint32 GetVirtualMapForMapAndZone(uint32 mapid, uint32 zoneId);

/// 游戏内容等级区间（用于区分旧大陆 1-60 与外域 61-70 内容）
enum ContentLevels
{
    CONTENT_1_60 = 0,
    CONTENT_61_70
};
/// 获取指定地图与区域所属的内容等级区间
ContentLevels GetContentLevelsForMapAndZone(uint32 mapid, uint32 zoneId);

/// 根据频道名称或 ID 获取聊天频道 DBC 条目
ChatChannelsEntry const* GetChatChannelsEntryFor(const std::string& name, uint32 channel_id = 0);

/// 判断物品图腾分类是否满足目标图腾分类要求
bool IsTotemCategoryCompatiableWith(uint32 itemTotemCategoryId, uint32 requiredTotemCategoryId);
/// 查询角色面部毛发样式 DBC 条目
CharacterFacialHairStylesEntry const* GetCharFacialHairEntry(uint8 race, uint8 gender, uint8 facialHairId);
/// 查询角色外观分段（皮肤/脸型/发型等）DBC 条目
CharSectionsEntry const* GetCharSectionEntry(uint8 race, CharSectionType genType, uint8 gender, uint8 type, uint8 color);

/// 检查世界坐标 (x, y) 是否位于指定地图与区域的边界范围内
bool MapCoordinateVsZoneCheck(float x, float y, uint32 mapid, uint32 zone);
/// 将区域地图归一化坐标 (0~100) 转换为世界坐标
bool Zone2MapCoordinates(float& x, float& y, uint32 zone);
/// 将世界坐标转换为区域地图归一化坐标 (0~100)
bool Map2ZoneCoordinates(float& x, float& y, uint32 zone);

/// 获取天赋在观察（Inspect）位掩码中的起始位偏移
uint32 GetTalentInspectBitPosInTab(uint32 talentId);
/// 获取指定天赋页在观察数据包中所占用的总位数
uint32 GetTalentTabInspectBitSize(uint32 talentTabId);
/// 获取指定职业对应的 3 个天赋分页 ID 数组
uint32 const* /*[3]*/ GetTalentTabPages(uint32 cls);

/// 判断三维点 (x, y, z) 是否落在 AreaTrigger 区域触发器（球形或盒形）内部
bool IsPointInAreaTriggerZone(AreaTriggerEntry const* atEntry, uint32 mapid, float x, float y, float z, float delta = 0.0f);

/// 根据生物模型 ID 查询所属种族
uint32 GetCreatureModelRace(uint32 model_id);

/// 获取地图默认光照环境 ID
uint32 GetDefaultMapLight(uint32 mapId);

#ifdef ENABLE_PLAYERBOTS
EmotesTextSoundEntry const* FindTextSoundEmoteFor(uint32 emote, uint32 race, uint32 gender);
CharSectionsEntry const* GetCharSectionEntry(uint8 race, CharSectionType genType, uint8 gender, uint8 type, uint8 color);
typedef std::multimap<uint32, CharSectionsEntry const*> CharSectionsMap;
extern CharSectionsMap sCharSectionMap;
#endif

extern DBCStorage <AreaTableEntry>               sAreaStore;// recommend access using functions | 区域表（建议通过辅助函数访问）
extern DBCStorage <AreaTriggerEntry>             sAreaTriggerStore;
extern DBCStorage <AuctionHouseEntry>            sAuctionHouseStore;
extern DBCStorage <BankBagSlotPricesEntry>       sBankBagSlotPricesStore;
extern DBCStorage <BattlemasterListEntry>        sBattlemasterListStore;
#ifdef ENABLE_PLAYERBOTS
extern DBCStorage <ChatChannelsEntry>            sChatChannelsStore; //has function for access aswell
#else
// extern DBCStorage <ChatChannelsEntry>            sChatChannelsStore; //has function for access aswell, no usable index
#endif
extern DBCStorage <CharStartOutfitEntry>         sCharStartOutfitStore;
extern DBCStorage <CharTitlesEntry>              sCharTitlesStore;
extern DBCStorage <ChatChannelsEntry>            sChatChannelsStore;
extern DBCStorage <CharacterFacialHairStylesEntry>  sCharacterFacialHairStylesStore;
extern DBCStorage <CharSectionsEntry>            sCharSectionsStore;
extern DBCStorage <ChrClassesEntry>              sChrClassesStore;
extern DBCStorage <ChrRacesEntry>                sChrRacesStore;
extern DBCStorage <CinematicCameraEntry>         sCinematicCameraStore;
extern DBCStorage <CinematicSequencesEntry>      sCinematicSequencesStore;
extern DBCStorage <CreatureDisplayInfoEntry>     sCreatureDisplayInfoStore;
extern DBCStorage <CreatureDisplayInfoExtraEntry>sCreatureDisplayInfoExtraStore;
extern DBCStorage <CreatureFamilyEntry>          sCreatureFamilyStore;
extern DBCStorage <CreatureModelDataEntry>       sCreatureModelDataStore;
extern DBCStorage <CreatureSpellDataEntry>       sCreatureSpellDataStore;
extern DBCStorage <CreatureTypeEntry>            sCreatureTypeStore;
extern DBCStorage <DurabilityCostsEntry>         sDurabilityCostsStore;
extern DBCStorage <DurabilityQualityEntry>       sDurabilityQualityStore;
extern DBCStorage <EmotesEntry>                  sEmotesStore;
extern DBCStorage <EmotesTextEntry>              sEmotesTextStore;
//extern DBCStorage <FactionEntry>                 sFactionStore;
extern DBCStorage <FactionTemplateEntry>         sFactionTemplateStore;
extern DBCStorage <GameObjectArtKitEntry>   sGameObjectArtKitStore;
extern DBCStorage <GameObjectDisplayInfoEntry>   sGameObjectDisplayInfoStore;
extern DBCStorage <GemPropertiesEntry>           sGemPropertiesStore;

extern DBCStorage <GMSurveyCurrentSurveyEntry>   sGMSurveyCurrentSurveyStore;
extern DBCStorage <GMSurveyQuestionsEntry>       sGMSurveyQuestionsStore;
extern DBCStorage <GMSurveyEntry>                sGMSurveySurveysStore;
extern DBCStorage <GMTicketCategoryEntry>        sGMTicketCategoryStore;

extern DBCStorage <GtCombatRatingsEntry>         sGtCombatRatingsStore;
extern DBCStorage <GtChanceToMeleeCritBaseEntry> sGtChanceToMeleeCritBaseStore;
extern DBCStorage <GtChanceToMeleeCritEntry>     sGtChanceToMeleeCritStore;
extern DBCStorage <GtChanceToSpellCritBaseEntry> sGtChanceToSpellCritBaseStore;
extern DBCStorage <GtChanceToSpellCritEntry>     sGtChanceToSpellCritStore;
extern DBCStorage <GtOCTRegenHPEntry>            sGtOCTRegenHPStore;
extern DBCStorage <GtNPCManaCostScalerEntry>     sGtNPCManaCostScalerStore;
// extern DBCStorage <GtOCTRegenMPEntry>            sGtOCTRegenMPStore; -- not used currently
extern DBCStorage <GtRegenHPPerSptEntry>         sGtRegenHPPerSptStore;
extern DBCStorage <GtRegenMPPerSptEntry>         sGtRegenMPPerSptStore;
extern DBCStorage <ItemEntry>                    sItemStore;
extern DBCStorage <ItemBagFamilyEntry>           sItemBagFamilyStore;
extern DBCStorage <ItemClassEntry>               sItemClassStore;
// extern DBCStorage <ItemDisplayInfoEntry>      sItemDisplayInfoStore; -- not used currently
extern DBCStorage <ItemExtendedCostEntry>        sItemExtendedCostStore;
extern DBCStorage <ItemRandomPropertiesEntry>    sItemRandomPropertiesStore;
extern DBCStorage <ItemRandomSuffixEntry>        sItemRandomSuffixStore;
extern DBCStorage <ItemSetEntry>                 sItemSetStore;
#ifdef ENABLE_PLAYERBOTS
extern DBCStorage <LFGDungeonEntry>              sLFGDungeonStore;
#endif
extern DBCStorage <LiquidTypeEntry>              sLiquidTypeStore;
extern DBCStorage <LockEntry>                    sLockStore;
extern DBCStorage <MailTemplateEntry>            sMailTemplateStore;
extern DBCStorage <MapEntry>                     sMapStore;
extern DBCStorage <QuestSortEntry>               sQuestSortStore;
extern DBCStorage <RandomPropertiesPointsEntry>  sRandomPropertiesPointsStore;
extern DBCStorage <SkillLineEntry>               sSkillLineStore;
extern DBCStorage <SkillLineAbilityEntry>        sSkillLineAbilityStore;
extern DBCStorage <SkillRaceClassInfoEntry>      sSkillRaceClassInfoStore;
extern DBCStorage <SkillTiersEntry>              sSkillTiersStore;
extern DBCStorage <SoundEntriesEntry>            sSoundEntriesStore;
extern DBCStorage <SpellCastTimesEntry>          sSpellCastTimesStore;
extern DBCStorage <SpellCategoryEntry>           sSpellCategory;
extern DBCStorage <SpellDurationEntry>           sSpellDurationStore;
extern DBCStorage <SpellFocusObjectEntry>        sSpellFocusObjectStore;
extern DBCStorage <SpellItemEnchantmentEntry>    sSpellItemEnchantmentStore;
extern DBCStorage <SpellItemEnchantmentConditionEntry> sSpellItemEnchantmentConditionStore;
extern SpellCategoryStore                        sSpellCategoryStore;
extern ItemSpellCategoryStore                    sItemSpellCategoryStore;
extern PetFamilySpellsStore                      sPetFamilySpellsStore;
extern DBCStorage <SpellRadiusEntry>             sSpellRadiusStore;
extern DBCStorage <SpellRangeEntry>              sSpellRangeStore;
extern DBCStorage <SpellShapeshiftFormEntry>     sSpellShapeshiftFormStore;
extern DBCStorage <SpellVisualEntry>             sSpellVisualStore;
extern DBCStorage <StableSlotPricesEntry>        sStableSlotPricesStore;
extern DBCStorage <SummonPropertiesEntry>        sSummonPropertiesStore;
extern DBCStorage <TalentEntry>                  sTalentStore;
extern DBCStorage <TalentTabEntry>               sTalentTabStore;
extern DBCStorage <TaxiNodesEntry>               sTaxiNodesStore;
extern DBCStorage <TaxiPathEntry>                sTaxiPathStore;
extern TaxiMask                                  sTaxiNodesMask;
extern TaxiPathSetBySource                       sTaxiPathSetBySource;
extern TaxiPathNodesByPath                       sTaxiPathNodesByPath;
extern DBCStorage <TransportAnimationEntry>      sTransportAnimationStore;
extern DBCStorage <TotemCategoryEntry>           sTotemCategoryStore;
extern DBCStorage <WMOAreaTableEntry>            sWMOAreaTableStore;
// extern DBCStorage <WorldMapAreaEntry>           sWorldMapAreaStore; -- use Zone2MapCoordinates and Map2ZoneCoordinates
// extern DBCStorage <WorldMapOverlayEntry>         sWorldMapOverlayStore; -- not used currently

/// 从指定数据目录加载全部 DBC 文件并构建派生索引缓存
void LoadDBCStores(const std::string& dataPath);

// script support functions | 供脚本系统调用的只读 DBC 存储访问接口
DBCStorage <SoundEntriesEntry>          const* GetSoundEntriesStore();
DBCStorage <SpellRangeEntry>            const* GetSpellRangeStore();
DBCStorage <FactionEntry>               const* GetFactionStore();
DBCStorage <ItemEntry>                  const* GetItemDisplayStore();
DBCStorage <CreatureDisplayInfoEntry>   const* GetCreatureDisplayStore();
DBCStorage <EmotesEntry>                const* GetEmotesStore();
DBCStorage <EmotesTextEntry>            const* GetEmotesTextStore();
#endif
