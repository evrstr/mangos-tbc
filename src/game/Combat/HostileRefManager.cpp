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

#include "Combat/HostileRefManager.h"
#include "Combat/ThreatManager.h"
#include "Entities/Unit.h"
#include "Server/DBCStructure.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Maps/Map.h"

// 构造函数
HostileRefManager::HostileRefManager(Unit* owner) : iOwner(owner)
{
}

// 析构时移除所有反向仇恨引用
HostileRefManager::~HostileRefManager()
{
    deleteReferences();
}

//=================================================
// send threat to all my haters for the pVictim
// The pVictim is hated than by them as well
// use for buffs and healing threat functionality
// 向所有仇恨本单位的敌人广播协助仇恨（常用于治疗/增益法术对施法者产生仇恨）

void HostileRefManager::threatAssist(Unit* victim, float threat, SpellEntry const* threatSpell, bool singleTarget, bool ignoreTimer)
{
    if (threatSpell->HasAttribute(SPELL_ATTR_EX2_NO_INITIAL_THREAT) || threatSpell->HasAttribute(SPELL_ATTR_EX_NO_THREAT) || !getOwner()->CanEnterCombat() || !victim->CanEnterCombat())
        return;

    if (threatSpell->HasAttribute(SPELL_ATTR_EX4_NO_HELPFUL_THREAT))
        return;

    HostileReference* ref = getFirst();
    if (!ref)
        return;

    // 收集当前未受控且可攻击 victim 的敌对来源
    std::vector<HostileReference*> validRefs;
    while (ref)
    {
        Unit* owner = ref->getSource()->getOwner();
        if (!owner->IsCrowdControlled() && owner != victim && owner->CanAttack(victim))
            validRefs.push_back(ref);
        ref = ref->next();
    }

    uint32 size = singleTarget ? 1 : validRefs.size();            // if singleTarget do not devide threat (单体直接仇恨不均分，否则按有效仇恨源数量均分)
    float threatPerTarget = threat / size;
    for (HostileReference* validReference : validRefs)
    {
        validReference->getSource()->addThreat(victim, threatPerTarget, false, (threatSpell ? GetSpellSchoolMask(threatSpell) : SPELL_SCHOOL_MASK_NORMAL), threatSpell, true);
        if (!ignoreTimer)
            victim->GetCombatManager().TriggerCombatTimer(validReference->getSource()->getOwner());
    }
}

//=================================================
// 对所有仇恨本单位的敌对列表按百分比增减仇恨
void HostileRefManager::addThreatPercent(int32 threatPercent)
{
    HostileReference* ref = getFirst();
    while (ref != nullptr)
    {
        ref->addThreatPercent(threatPercent);
        ref = ref->next();
    }
}

// 临时仇恨消退处理（apply=true 时暂扣仇恨，false 时恢复）
void HostileRefManager::threatTemporaryFade(Unit* /*victim*/, float threat, bool apply)
{
    HostileReference* ref = getFirst();

    while (ref)
    {
        if (apply)
        {
            float curThreat = ref->getThreat();
            float reducedThreat = std::max(-curThreat, threat);
            ref->setFadeoutThreatReduction(reducedThreat);
        }
        else
            ref->resetFadeoutThreatReduction();

        ref = ref->next();
    }
}

//=================================================
// The online / offline status is given to the method. The calculation has to be done before
// 批量设置所有敌对引用的在线/离线状态

void HostileRefManager::setOnlineOfflineState(bool isOnline)
{
    HostileReference* ref = getFirst();
    while (ref != nullptr)
    {
        ref->setOnlineOfflineState(isOnline);
        ref = ref->next();
    }
}

// 更新在线/离线状态（战斗中假死成功时阻止恢复在线状态）
void HostileRefManager::updateOnlineOfflineState(bool pIsOnline)
{
    if (pIsOnline && iOwner)
    {
        // Do not set online while feigning death in combat (战斗中处于假死成功状态时保持离线仇恨状态)
        if (iOwner->IsFeigningDeathSuccessfully() && iOwner->IsInCombat())
            return;
    }
    setOnlineOfflineState(pIsOnline);
}

//=================================================
// The online / offline status is calculated and set
// 遍历并重新计算所有仇恨表引用的在线状态

void HostileRefManager::updateThreatTables()
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        ref->updateOnlineStatus();
        ref = ref->next();
    }
}

//=================================================
// The references are not needed anymore
// tell the source to remove them from the list and free the mem
// 移除并释放全部敌对引用

void HostileRefManager::deleteReferences()
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        ref->removeReference();
        delete ref;
        ref = nextRef;
    }
}

//=================================================
// delete one reference, defined by faction
// 移除来自指定阵营的所有敌对引用

void HostileRefManager::deleteReferencesForFaction(uint32 faction)
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        if (ref->getSource()->getOwner()->GetFactionTemplateEntry()->faction == faction)
        {
            ref->removeReference();
            delete ref;
        }
        ref = nextRef;
    }
}

//=================================================
// delete one reference, defined by Unit
// 移除指定敌对单位的引用

void HostileRefManager::deleteReference(Unit* victim)
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        if (ref->getSource()->getOwner() == victim)
        {
            ref->removeReference();
            delete ref;
            break;
        }
        ref = nextRef;
    }
}

//=================================================
// set state for one reference, defined by Unit
// 更新指定敌对单位引用的在线/离线状态

void HostileRefManager::setOnlineOfflineState(Unit* victim, bool isOnline)
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        if (ref->getSource()->getOwner() == victim)
        {
            ref->setOnlineOfflineState(isOnline);
            break;
        }
        ref = nextRef;
    }
}

// 解析当前仇恨误导目标单位
Unit* HostileRefManager::GetThreatRedirectionTarget() const
{
    return m_redirectionTargetGuid ? iOwner->GetMap()->GetUnit(m_redirectionTargetGuid) : nullptr;
}

// 获取链表中首个敌对引用
HostileReference* HostileRefManager::getFirst()
{
    return static_cast<HostileReference*>(RefManager<Unit, ThreatManager>::getFirst());
}

// 更新目标压制状态（例如本单位无敌或对敌方主要攻击学派免疫时降低仇恨优先级）
void HostileRefManager::HandleSuppressed(bool apply, bool immunity)
{
    if (apply)
    {
        for (auto& data : *this)
        {
            HostileReference& ref = static_cast<HostileReference&>(data);
            if (immunity)
            {
                Unit* source = ref.getSource()->getOwner();
                Unit* target = ref.getTarget();
                if (!target->IsImmuneToDamage(source->GetMainAttackSchoolMask()))
                    continue;
            }
            ref.SetHostileState(STATE_SUPPRESSED);
        }
    }
    else
    {
        for (auto& data : *this)
        {
            HostileReference& ref = static_cast<HostileReference&>(data);
            Unit* source = ref.getSource()->getOwner();
            Unit* target = ref.getTarget();
            if (!source->IsSuppressedTarget(target))
            {
                ref.SetSuppressabilityToggle();
                continue;
            }
            ref.SetHostileState(STATE_SUPPRESSED);
        }
    }
}


//=================================================
