#include "AI/BaseAI/PossessedAI.h"
#include "Entities/Creature.h"

// 构造函数：将 Creature 转为 Unit 基类传入 UnitAI
PossessedAI::PossessedAI(Creature* creature) : UnitAI(static_cast<Unit*>(creature))
{
}

// 返回静态空法术列表以满足基类纯虚接口
CreatureSpellList const& PossessedAI::GetSpellList() const
{
    static CreatureSpellList list; // will never be actually used (永不实际使用，仅满足接口契约)
    return list;
}
