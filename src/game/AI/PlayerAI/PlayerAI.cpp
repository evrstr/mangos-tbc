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

#include "PlayerAI.h"
#include "Entities/Player.h"
#include "Spells/SpellMgr.h"
#include <limits>

// 通用玩家 AI 内部定时动作 ID
enum GenericPlayerAIActions
{
    GENERIC_ACTION_RESET = 1000,            // 重置施法冷却标记
    GENERIC_THREAT_CHANGE = 1001,           // 触发周期性随机切换攻击目标
};

// 构造函数：注册施法重置与目标切换定时动作
PlayerAI::PlayerAI(Player* player) : UnitAI(player), m_player(player), m_spellsDisabled(false)
{
    AddCustomAction(GENERIC_ACTION_RESET, true, [&]() { m_spellsDisabled = false; });
    AddCustomAction(GENERIC_THREAT_CHANGE, true, [&]() { m_executeTargetChange = true; });
}

// 每帧执行玩家 AI 战斗逻辑
void PlayerAI::ExecuteActions()
{
    ExecuteSpells();
}

// 为玩家 AI 添加一个可用法术；未提供 selector 时按法术目标类型自动推导目标选择函数
void PlayerAI::AddPlayerSpellAction(uint32 spellId, std::function<Unit*()> selector)
{
    bool failureOnNoTarget = false;
    if (!selector)
    {
        SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
        if (spellInfo)
        {
            if (IsNextMeleeSwingSpell(spellInfo))
                selector = [&]()->Unit* { return m_player->GetVictim(); };
            else if (HasSpellTarget(spellInfo, TARGET_UNIT_ENEMY) || (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION)) // always random (敌对目标或地面目标法术总是随机选敌)
                selector = [&, spellId = spellInfo->Id]()->Unit* { return m_player->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, spellId, SELECT_FLAG_IN_LOS); };
            if (HasSpellTarget(spellInfo, TARGET_UNIT_FRIEND) || HasSpellTarget(spellInfo, TARGET_UNIT_FRIEND_CHAIN_HEAL)) // heals only target self (治疗/友方增益法术仅以自身为目标)
                selector = [&]()->Unit* { return m_player; };

            failureOnNoTarget = (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION) != 0;
        }
        if (!selector) // fallback (兜底返回空目标)
            selector = [&]()->Unit* { return nullptr; };
    }
    m_playerSpellActions.emplace_back(spellId, failureOnNoTarget, selector);
}

// 执行法术施放与定期随机目标切换
void PlayerAI::ExecuteSpells()
{
    if (m_executeTargetChange && CanExecuteCombatAction())
    {
        if (Unit* target = m_player->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
            AttackStart(target);

        ResetTimer(GENERIC_THREAT_CHANGE, urand(10000, 20000));
        m_executeTargetChange = false;
    }

    if (m_spellsDisabled)
        return;

    std::shuffle(m_playerSpellActions.begin(), m_playerSpellActions.end(), *GetRandomGenerator());

    bool success = false;
    for (auto& data : m_playerSpellActions)
    {
        Unit* target = data.targetFinder();
        if (data.failureOnNoTarget && target == nullptr)
            continue;
        if (DoCastSpellIfCan(target, data.spellId) == CAST_OK)
            success = true;  
    }          

    // 施法成功后大概率进入 6~12 秒的施法间隔，避免被魅惑玩家无休止放技能
    if (success)
    {
        uint32 randomInterval = urand(0, 5);
        if (randomInterval != 0)
        {
            m_spellsDisabled = true;
            ResetTimer(GENERIC_ACTION_RESET, urand(6000, 12000));
        }
    }
}

// 玩家刚被魅惑时回调：加入魅惑者阵型或跟随魅惑者，并立即攻击最近敌人
void PlayerAI::JustGotCharmed(Unit* charmer)
{
    if (charmer->GetFormationSlot())
    {
        charmer->GetFormationSlot()->GetFormationData()->Add(m_player);
    }
    else
        m_player->GetMotionMaster()->MoveFollow(charmer, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE, true);
    AttackClosestEnemy();
}

// 脱离战斗时停止自身及宠物攻击，并恢复跟随魅惑者
void PlayerAI::EnterEvadeMode()
{
    m_player->CombatStopWithPets(true);
    if (Unit* charmer = m_player->GetCharmer())
    {
        if (charmer->GetFormationSlot())
        {
            charmer->GetFormationSlot()->GetFormationData()->Add(m_player);
        }
        else
            m_player->GetMotionMaster()->MoveFollow(charmer, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE, true);
    }
}

// 搜寻并攻击最近敌人（优先选择玩家目标，其次按距离最近）
void PlayerAI::AttackClosestEnemy()
{
    float distance = std::numeric_limits<float>::max();
    AttackSpecificEnemy([&](Unit* enemy, Unit*& closestEnemy) mutable
    {
        float curDistance = enemy->GetDistance(m_unit, true, DIST_CALC_NONE);
        if (!closestEnemy || (!closestEnemy->IsPlayer() && enemy->IsPlayer()) || curDistance < distance)
        {
            closestEnemy = enemy;
            distance = curDistance;
        }
    });
}

// 命令玩家移动跟随指定单位
void PlayerAI::RequestFollow(Unit* followee)
{
    m_unit->GetMotionMaster()->MoveFollow(followee, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE, false, !m_unit->IsPlayerControlled());
}
