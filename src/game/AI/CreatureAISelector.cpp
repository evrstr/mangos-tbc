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

#include "CreatureAISelector.h"
#include "Entities/Creature.h"
#include "BaseAI/CreatureAIImpl.h"
#include "BaseAI/NullCreatureAI.h"
#include "Policies/Singleton.h"
#include "MotionGenerators/MovementGenerator.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Entities/Pet.h"
#include "Entities/Player.h"
#include "Log/Log.h"
#include "BaseAI/PetAI.h"
#include "BaseAI/PossessedAI.h"
#include "AI/PlayerAI/PlayerAI.h"

INSTANTIATE_SINGLETON_1(CreatureAIRegistry);
INSTANTIATE_SINGLETON_1(MovementGeneratorRegistry);

// 工厂选择器：按优先级为生物/单位挑选 AI 与默认移动生成器
namespace FactorySelector
{
    // 按优先级为生物选择 AI：脚本 AI -> 宠物/守卫/图腾 -> 指定 AIName -> Permit 评分最高者 -> NullCreatureAI
    UnitAI* selectAI(Creature* creature)
    {
        // Allow scripting AI for normal creatures and not controlled pets (guardians and mini-pets)
        // 允许普通生物及非玩家直接控制的宠物（守护者与小宠物）使用 ScriptDevAI 脚本
        if ((!creature->IsPet() || !static_cast<Pet*>(creature)->isControlled()) || creature->HasCharmer())
            if (UnitAI* scriptedAI = sScriptDevAIMgr.GetCreatureAI(creature))
                return scriptedAI;

        CreatureAIRegistry& ai_registry(CreatureAIRepository::Instance());

        const CreatureAICreator* ai_factory = nullptr;

        std::string ainame = creature->GetAIName();

        // select by NPC flags (按 NPC 类型与标志位选择对应 AI)
        if (creature->IsPet())
        {
            if (creature->IsPlayerControlled() && static_cast<Pet*>(creature)->isControlled())
                ai_factory = ai_registry.GetRegistryItem("PetAI");
            else                            // For guardians and creature pets in general (守护者及普通生物召唤宠)
                ai_factory = ai_registry.GetRegistryItem("GuardianAI");
        }
        else if(creature->HasCharmer() && !creature->IsTemporarySummon()) // for charmed creatures but not possessed tempspawns (被魅惑的生物，排除被附身的临时召唤物)
            ai_factory = ai_registry.GetRegistryItem("PetAI");
        else if (creature->IsTotem())
            ai_factory = ai_registry.GetRegistryItem("TotemAI");
        else if (!ainame.empty())           // select by script name (按数据库配置的 AIName 选择)
            ai_factory = ai_registry.GetRegistryItem(ainame);
        else if (creature->IsGuard())
            ai_factory = ai_registry.GetRegistryItem("GuardAI");
        else                                // select by permit check (遍历注册项，通过 Permit 许可度评分选择最佳 AI)
        {
            int best_val = PERMIT_BASE_NO;
            typedef CreatureAIRegistry::RegistryMapType RMT;
            RMT const& l = ai_registry.GetRegisteredItems();
            for (const auto& iter : l)
            {
                const CreatureAICreator* factory = iter.second;
                const SelectableAI* p = dynamic_cast<const SelectableAI*>(factory);
                MANGOS_ASSERT(p != nullptr);
                int val = p->Permit(creature);
                if (val > best_val)
                {
                    best_val = val;
                    ai_factory = p;
                }
            }
        }

        // select NullCreatureAI if not another cases (若无任何匹配项则回退至空 AI NullCreatureAI)
        ainame = (ai_factory == nullptr) ? "NullCreatureAI" : ai_factory->key();

        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature %u used AI is %s.", creature->GetGUIDLow(), ainame.c_str());
        return (ai_factory == nullptr ? new NullCreatureAI(creature) : ai_factory->Create(creature));
    }

    // 根据名称为单位创建指定 AI（支持生物工厂创建及玩家的 PetAI/PossessedAI）
    UnitAI* GetSpecificAI(Unit* unit, std::string const& ainame)
    {
        CreatureAIRegistry& ai_registry(CreatureAIRepository::Instance());
        const CreatureAICreator* ai_factory = ai_registry.GetRegistryItem(ainame);
        if (unit->IsCreature())
            return  ai_factory->Create(static_cast<Creature*>(unit));
        if (ainame == "PetAI")
            return GetClassAI(Classes(unit->getClass()), static_cast<Player*>(unit));
        if (ainame == "PossessedAI")
            return (new PossessedAI(unit));

        sLog.outError("FactorySelector::GetSpecificAI> Cannot get %s AI for %s", ainame.c_str(), unit->GetGuidStr().c_str());
        MANGOS_ASSERT(false);
        return nullptr;
    }

    // 为生物选择默认移动生成器（玩家拥有的生物强制使用跟随移动，否则按数据库配置）
    MovementGenerator* selectMovementGenerator(Creature* creature)
    {
        MovementGeneratorRegistry& mv_registry(MovementGeneratorRepository::Instance());
        MANGOS_ASSERT(creature->GetCreatureInfo() != nullptr);
        MovementGeneratorCreator const* mv_factory = mv_registry.GetRegistryItem(
                    creature->GetOwnerGuid().IsPlayer() ? FOLLOW_MOTION_TYPE : creature->GetDefaultMovementType());

        /* if( mv_factory == nullptr  )
        {
            int best_val = -1;
            std::vector<std::string> l;
            mv_registry.GetRegisteredItems(l);
            for( std::vector<std::string>::iterator iter = l.begin(); iter != l.end(); ++iter)
            {
            const MovementGeneratorCreator *factory = mv_registry.GetRegistryItem((*iter).c_str());
            const SelectableMovement *p = dynamic_cast<const SelectableMovement *>(factory);
            ASSERT( p != nullptr );
            int val = p->Permit(creature);
            if( val > best_val )
            {
                best_val = val;
                mv_factory = p;
            }
            }
        }*/

        return (mv_factory == nullptr ? nullptr : mv_factory->Create(creature));
    }
}
