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

#ifndef MANGOSSERVER_PET_H
#define MANGOSSERVER_PET_H

#include "Common.h"
#include "Entities/ObjectGuid.h"
#include "Entities/Creature.h"
#include "Entities/Unit.h"
#include "Maps/InstanceData.h"

// 宠物类型枚举
enum PetType
{
    SUMMON_PET              = 0,                            // 召唤宠物（术士恶魔、法师水元素等受控召唤物）
    HUNTER_PET              = 1,                            // 猎人驯服的野兽宠物
    GUARDIAN_PET            = 2,                            // 守护者宠物（非直接操控的护卫召唤物）
    MINI_PET                = 3,                            // 小宠物（非战斗观赏性伴侣宠物）
    MAX_PET_TYPE            = 4                             // 宠物类型总数上限
};

#define MAX_PET_STABLES         2                           // 最大兽栏槽位数

// stored in character_pet.slot (存储在 character_pet.slot 中的宠物保存槽位模式)
enum PetSaveMode
{
    PET_SAVE_AS_DELETED        = -1,                        // not saved in fact (实际不保存，视为删除)
    PET_SAVE_AS_CURRENT        =  0,                        // in current slot (with player) (当前随身携带槽位)
    PET_SAVE_FIRST_STABLE_SLOT =  1,                        // 首个兽栏槽位索引
    PET_SAVE_LAST_STABLE_SLOT  =  MAX_PET_STABLES,          // last in DB stable slot index (including), all higher have same meaning as PET_SAVE_NOT_IN_SLOT (最后一个数据库兽栏槽位索引)
    PET_SAVE_NOT_IN_SLOT       =  100,                      // for avoid conflict with stable size grow will use 100 (未占用槽位：避免与兽栏扩容冲突使用 100)
    PET_SAVE_REAGENTS          =  101                       // PET_SAVE_NOT_IN_SLOT with reagents return (未占用槽位并返还施法材料)
};

// There might be a lot more (宠物行为与反应状态标志位，实际可能还有更多)
enum PetModeFlags
{
    PET_MODE_STAY              = 0x0000000,                 // 原地停留
    PET_MODE_FOLLOW            = 0x0000001,                 // 跟随主人
    PET_MODE_ATTACK            = 0x0000002,                 // 主动攻击目标
    PET_MODE_PASSIVE           = 0x0000000,                 // 被动姿态
    PET_MODE_DEFENSIVE         = 0x0000100,                 // 防御姿态
    PET_MODE_AGGRESSIVE        = 0x0000200,                 // 主动姿态

    PET_MODE_DISABLE_ACTIONS   = 0x8000000,                 // 禁用宠物动作（如主人骑乘时）

    // autoset in client at summon (召唤时客户端默认自动设置的模式)
    PET_MODE_DEFAULT           = PET_MODE_FOLLOW | PET_MODE_DEFENSIVE,
};

// 猎人宠物快乐度状态枚举
enum HappinessState
{
    UNHAPPY = 1,                                            // 不高兴（伤害 75%）
    CONTENT = 2,                                            // 满足（伤害 100%）
    HAPPY   = 3                                             // 快乐（伤害 125%）
};

// 猎人宠物忠诚度等级枚举
enum LoyaltyLevel
{
    REBELLIOUS  = 1,                                        // 叛逆（1 级）
    UNRULY      = 2,                                        // 桀骜（2 级）
    SUBMISSIVE  = 3,                                        // 顺从（3 级）
    DEPENDABLE  = 4,                                        // 可靠（4 级）
    FAITHFUL    = 5,                                        // 忠实（5 级）
    BEST_FRIEND = 6                                         // 挚友（6 级）
};

// 宠物法术同步状态（用于数据库增量保存）
enum PetSpellState
{
    PETSPELL_UNCHANGED = 0,                                 // 未变更
    PETSPELL_CHANGED   = 1,                                 // 已修改（如激活状态变化）
    PETSPELL_NEW       = 2,                                 // 新增法术
    PETSPELL_REMOVED   = 3                                  // 已移除法术
};

// 宠物法术来源类型
enum PetSpellType
{
    PETSPELL_NORMAL = 0,                                    // 普通宠物技能
    PETSPELL_FAMILY = 1,                                    // 宠物家族自带技能
};

// 宠物已学法术条目
struct PetSpell
{
    uint8 active;                                           // use instead enum (not good use *uint8* limited enum in case when value in enum not positive in *int8*) (使用 uint8 替代枚举存储激活状态)

    PetSpellState state : 8;                                // 持久化状态
    PetSpellType type   : 8;                                // 法术类型
};

// 宠物动作反馈错误码
enum ActionFeedback
{
    FEEDBACK_NONE            = 0,                           // 无反馈
    FEEDBACK_PET_DEAD        = 1,                           // 宠物已死亡
    FEEDBACK_NOTHING_TO_ATT  = 2,                           // 没有可攻击的目标
    FEEDBACK_CANT_ATT_TARGET = 3,                           // 无法攻击该目标
    FEEDBACK_NO_PATH         = 4,                           // 无法寻路到达目标
};

// 宠物语音触发类型
enum PetTalk
{
    PET_TALK_SPECIAL_SPELL  = 0,                            // 施放特殊法术时发声
    PET_TALK_ATTACK         = 1                             // 发起攻击时发声
};

// 宠物命名校验失败原因（与客户端 SMSG_PET_NAME_INVALID 对应）
enum PetNameInvalidReason
{
    // custom, not send (服务端内部使用，不发送给客户端)
    PET_NAME_SUCCESS                                        = 0, // 名称合法

    PET_NAME_INVALID                                        = 1, // 名称无效
    PET_NAME_NO_NAME                                        = 2, // 名称为空
    PET_NAME_TOO_SHORT                                      = 3, // 名称过短
    PET_NAME_TOO_LONG                                       = 4, // 名称过长
    PET_NAME_MIXED_LANGUAGES                                = 6, // 混用多种语言字符
    PET_NAME_PROFANE                                        = 7, // 包含违禁/脏话词汇
    PET_NAME_RESERVED                                       = 8, // 包含保留名称
    PET_NAME_THREE_CONSECUTIVE                              = 11, // 连续三个相同字符
    PET_NAME_INVALID_SPACE                                  = 12, // 非法空格位置
    PET_NAME_CONSECUTIVE_SPACES                             = 13, // 连续多个空格
    PET_NAME_RUSSIAN_CONSECUTIVE_SILENT_CHARACTERS          = 14, // 俄语连续静音符号
    PET_NAME_RUSSIAN_SILENT_CHARACTER_AT_BEGINNING_OR_END   = 15, // 俄语首尾静音符号
    PET_NAME_DECLENSION_DOESNT_MATCH_BASE_NAME              = 16  // 词形变化与基础名称不匹配
};

typedef std::unordered_map<uint32, PetSpell> PetSpellMap;   // 宠物法术映射表（法术 ID -> PetSpell）
typedef std::map<uint32, uint32> TeachSpellMap;             // 可教导给玩家的法术映射（学会的法术 ID -> 来源法术 ID）
typedef std::vector<uint32> AutoSpellList;                  // 开启自动施法的法术 ID 列表

#define HAPPINESS_LEVEL_SIZE        333000                  // 每一级快乐度区间大小（总上限约 1000000）

extern const uint32 LevelUpLoyalty[6];                      // 升级忠诚度所需经验表
extern const uint32 LevelStartLoyalty[6];                   // 各忠诚度等级起始点数表

#define ACTIVE_SPELLS_MAX           4                       // 宠物动作栏最大可激活自动施法的技能数量

class Player;

// 宠物实体类：继承自 Creature，统一表示召唤宠物、猎人宠物、守护者及小宠物
class Pet : public Creature
{
    public:
        explicit Pet(PetType type = MAX_PET_TYPE);
        virtual ~Pet();

        // 世界注册与注销
        void AddToWorld() override;
        void RemoveFromWorld() override;

        // 宠物类型与状态判定
        PetType getPetType() const { return m_petType; }
        void setPetType(PetType type) { m_petType = type; }
        bool isControlled() const { return getPetType() == SUMMON_PET || getPetType() == HUNTER_PET; }
        bool isTemporarySummoned() const { return m_duration > 0; }
        bool IsGuardian() const { return getPetType() == GUARDIAN_PET; }
        bool HasActionsDisabled() const { return GetModeFlags() & PET_MODE_DISABLE_ACTIONS; }

        // 创建、数据库加载/保存、解散与删除
        bool Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, uint32 pet_number);
        bool CreateBaseAtCreature(Creature* creature);
        bool LoadPetFromDB(Player* owner, Position const& spawnPos, uint32 petentry = 0, uint32 petnumber = 0, bool current = false, uint32 healthPercentage = 0, bool permanentOnly = false, bool forced = false);
        void SavePetToDB(PetSaveMode mode, Player* owner);
        Position GetPetSpawnPosition(Player* owner);
        bool isLoading() const { return m_loading; }
        void SetLoading(bool state) { m_loading = state; }
        void Unsummon(PetSaveMode mode, Unit* owner = nullptr, bool removeList = true);
        static void DeleteFromDB(uint32 guidlow, bool separate_transaction = true);
        static void DeleteFromDB(Unit* owner, PetSaveMode slot);
        static SpellCastResult TryLoadFromDB(Unit* owner, uint32 petentry = 0, uint32 petnumber = 0, bool current = false, PetType mandatoryPetType = MAX_PET_TYPE);
        void PlayDismissSound();

        ObjectGuid const GetSpawnerGuid() const override { return GetOwnerGuid(); }

        void SetOwnerGuid(ObjectGuid owner) override;

        Player* GetSpellModOwner() const override;

        void SetDeathState(DeathState s) override;          // overwrite virtual Creature::SetDeathState and Unit::SetDeathState (重写死亡状态切换逻辑)
        void Update(const uint32 diff) override;  // overwrite virtual Creature::Update and Unit::Update (重写宠物每帧更新逻辑)

        uint8 GetPetAutoSpellSize() const override { return m_autospells.size(); }
        uint32 GetPetAutoSpellOnPos(uint8 pos) const override
        {
            if (pos >= m_autospells.size())
                return 0;
            return m_autospells[pos];
        }

        bool CanSwim() const override
        {
            if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
                return true;
            if (Unit const* owner = GetOwner())
                return static_cast<Creature const*>(owner)->CanSwim();
            return Creature::CanSwim();
        }

        // 恢复、快乐度、忠诚度、经验升级与属性成长
        void RegenerateAll(uint32 update_diff) override;    // overwrite Creature::RegenerateAll (重写资源回复逻辑)
        void LooseHappiness();
        void TickLoyaltyChange();
        void ModifyLoyalty(int32 addvalue);
        HappinessState GetHappinessState() const;
        uint32 GetMaxLoyaltyPoints(uint32 level) const;
        uint32 GetStartLoyaltyPoints(uint32 level) const;
        void KillLoyaltyBonus(uint32 level);
        uint32 GetLoyaltyLevel() { return GetByteValue(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_PET_LOYALTY); }
        void SetLoyaltyLevel(LoyaltyLevel level);
        void GivePetXP(uint32 xp);
        void GivePetLevel(uint32 level);
        void SynchronizeLevelWithOwner();
        void InitStatsForLevel(uint32 petlevel);
        void InitPetScalingAuras();
        bool HaveInDiet(ItemPrototype const* item) const;
        uint32 GetCurrentFoodBenefitLevel(uint32 itemlevel) const;
        void SetDuration(int32 dur) { m_duration = dur; }
        int32 GetDuration() const { return m_duration; }

        bool UpdateStats(Stats stat) override;
        void UpdateMaxHealth() override;
        void UpdateMaxPower(Powers power) override;
        void UpdateAttackPowerAndDamage(bool ranged = false) override;
        float GetConditionalTotalPhysicalDamageModifier(WeaponAttackType type) const override;

        bool   CanTakeMoreActiveSpells(uint32 spellid) const;
        void   ToggleAutocast(uint32 spellid, bool apply);
        bool   HasTPForSpell(uint32 spellid) const;
        int32  GetTPForSpell(uint32 spellid) const;

        void SetModeFlags(PetModeFlags mode);
        PetModeFlags GetModeFlags() const { return m_petModeFlags; }

        bool HasSpell(uint32 spell) const override;
        void AddTeachSpell(uint32 learned_id, uint32 source_id) { m_teachspells[learned_id] = source_id; }

        void LearnPetPassives();
        void CastPetAuras(bool current);
        void CastOwnerTalentAuras();
        void CastPetAura(PetAura const* aura);

        // 宠物冷却、光环与法术的数据库读写及学习/遗忘
        void _LoadSpellCooldowns();
        void _SaveSpellCooldowns();
        void _LoadAuras(uint32 timediff);
        void _SaveAuras();
        void _LoadSpells();
        void _SaveSpells();

        bool addSpell(uint32 spell_id, ActiveStates active = ACT_DECIDE, PetSpellState state = PETSPELL_NEW, PetSpellType type = PETSPELL_NORMAL);
        bool learnSpell(uint32 spell_id);
        bool unlearnSpell(uint32 spell_id, bool learn_prev, bool clear_ab = true);
        bool removeSpell(uint32 spell_id, bool learn_prev, bool clear_ab = true);
        void CleanupActionBar();

        PetSpellMap     m_spells;
        TeachSpellMap   m_teachspells;
        AutoSpellList   m_autospells;

        void InitPetCreateSpells();
        void CheckLearning(uint32 spellid);
        uint32 resetTalentsCost() const;

        virtual CharmInfo* InitCharmInfo(Unit* charm) override;
        virtual void DeleteCharmInfo() override;

        void  SetTP(int32 TP);
        int32 GetDispTP() const;

        int32   m_TrainingPoints;
        uint32  m_resetTalentsCost;
        time_t  m_resetTalentsTime;

        // overwrite Creature function for name localization back to WorldObject version without localization (宠物支持自定义命名，绕过生物本地化名称表)
        const char* GetNameForLocaleIdx(int32 locale_idx) const override { return WorldObject::GetNameForLocaleIdx(locale_idx); }

        DeclinedName const* GetDeclinedNames() const { return m_declinedname; }

        void SetRequiredXpForNextLoyaltyLevel();
        void UpdateRequireXpForNextLoyaltyLevel(uint32 xp);

        bool    m_removed;                                  // prevent overwrite pet state in DB at next Pet::Update if pet already removed(saved) (防止已移除或已保存的宠物在下次 Update 时覆盖数据库状态)

        // return charminfo ai only when this pet is possessed. (eye of the beast case for ex.) (仅当被附身控制如野兽之眼时返回 CharmInfo 的 AI)
        virtual UnitAI* AI() override
        {
            if (hasUnitState(UNIT_STAT_POSSESSED) && m_charmInfo->GetAI()) return m_charmInfo->GetAI();
            return m_ai.get();
        }

        virtual CombatData* GetCombatData() override { return m_combatData; }
        virtual CombatData const* GetCombatData() const override { return m_combatData; }

        virtual void RegenerateHealth() override;

        void ResetCorpseRespawn();

        void ForcedDespawn(uint32 timeMSToDespawn = 0, bool onlyAlive = false) override;

        bool IgnoresOwnersDeath() const;
    protected:
        uint32  m_happinessTimer;                           // 快乐度衰减计时器（毫秒）
        uint32  m_loyaltyTimer;                             // 忠诚度周期检测计时器（毫秒）
        PetType m_petType;                                  // 当前宠物类型
        int32   m_duration;                                 // time until unsummon (used mostly for summoned guardians and not used for controlled pets) (剩余存在时间，多用于临时召唤守护者)
        int32   m_loyaltyPoints;                            // 当前忠诚度点数
        bool    m_loading;                                  // 是否正处于数据库加载过程中
        uint32  m_xpRequiredForNextLoyaltyLevel;            // 升至下一忠诚度等级所需经验值
        DeclinedName* m_declinedname;                       // 俄语等屈折语词形变化名称

    private:
        PetModeFlags m_petModeFlags;                        // 宠物模式标志（停留/跟随/被动/防御/主动等）
        CharmInfo*   m_originalCharminfo;                   // 附身状态前保存的原始控制信息
        bool m_inStatsUpdate;                               // 防止属性更新重入标记
        bool m_scaleWithCls;                                // 是否启用职业/等级缩放光环

        // 禁用 Creature 基类的地图静态刷怪保存与删除接口
        void SaveToDB(uint32, uint8) override               // overwrited of Creature::SaveToDB     - don't must be called
        {
            MANGOS_ASSERT(false);
        }
        void DeleteFromDB() override                        // overwrite of Creature::DeleteFromDB - don't must be called
        {
            MANGOS_ASSERT(false);
        }
};
#endif
