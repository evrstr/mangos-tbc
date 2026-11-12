/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
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

#include "AI/ScriptDevAI/include/sc_common.h"

// 春节烟花发射器触发脚本
struct LunarFestivalFirework : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            if (GameObject* launcher = spell->GetGOTarget())
                launcher->Use(spell->GetCaster());
        }
    }
};

// 春节单发烟花目标高度调整脚本 (+3.0f)
struct LunarFestivalFireworkSingle : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.z += 3.f;
    }
};

// 春节小型烟花束 (下方) 坐标偏移脚本
struct LunarFestivalClusterSmallD : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.z += 2.f;
    }
};

// 春节小型烟花束 (北方) 坐标偏移脚本
struct LunarFestivalClusterSmallN : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.y += 2.f;
        spell->m_targets.m_destPos.z += 5.f;
    }
};

// 春节小型烟花束 (东南) 坐标偏移脚本
struct LunarFestivalClusterSmallSE : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.x += 3.5f;
        spell->m_targets.m_destPos.y -= 1.f;
        spell->m_targets.m_destPos.z += 5.f;
    }
};

// 春节小型烟花束 (西南) 坐标偏移脚本
struct LunarFestivalClusterSmallSW : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.x -= 3.5f;
        spell->m_targets.m_destPos.y -= 1.f;
        spell->m_targets.m_destPos.z += 5.f;
    }
};

// 春节小型烟花束 (上方) 坐标偏移脚本
struct LunarFestivalClusterSmallU : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.z += 8.f;
    }
};

// 春节大型烟花束 (下方) 坐标偏移脚本
struct LunarFestivalClusterBigD : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.z += 3.f;
    }
};

// 春节大型烟花束 (北方) 坐标偏移脚本
struct LunarFestivalClusterBigN : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.y += 3.f;
        spell->m_targets.m_destPos.z += 7.5f;
    }
};

// 春节大型烟花束 (东南) 坐标偏移脚本
struct LunarFestivalClusterBigSE : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.x += 5.5f;
        spell->m_targets.m_destPos.y -= 1.5f;
        spell->m_targets.m_destPos.z += 7.5f;
    }
};

// 春节大型烟花束 (西南) 坐标偏移脚本
struct LunarFestivalClusterBigSW : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.x -= 5.5f;
        spell->m_targets.m_destPos.y -= 1.5f;
        spell->m_targets.m_destPos.z += 7.5f;
    }
};

// 春节大型烟花束 (上方) 坐标偏移脚本
struct LunarFestivalClusterBigU : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.z += 12.f;
    }
};

// 注册春节烟花法术脚本
void AddSC_lunar_festival()
{
    RegisterSpellScript<LunarFestivalFirework>("spell_lunar_festival_firework");
    RegisterSpellScript<LunarFestivalFireworkSingle>("spell_lunar_festival_firework_single");
    RegisterSpellScript<LunarFestivalClusterSmallD>("spell_lunar_festival_cluster_small_d");
    RegisterSpellScript<LunarFestivalClusterSmallN>("spell_lunar_festival_cluster_small_n");
    RegisterSpellScript<LunarFestivalClusterSmallSE>("spell_lunar_festival_cluster_small_se");
    RegisterSpellScript<LunarFestivalClusterSmallSW>("spell_lunar_festival_cluster_small_sw");
    RegisterSpellScript<LunarFestivalClusterSmallU>("spell_lunar_festival_cluster_small_u");
    RegisterSpellScript<LunarFestivalClusterBigD>("spell_lunar_festival_cluster_big_d");
    RegisterSpellScript<LunarFestivalClusterBigN>("spell_lunar_festival_cluster_big_n");
    RegisterSpellScript<LunarFestivalClusterBigSE>("spell_lunar_festival_cluster_big_se");
    RegisterSpellScript<LunarFestivalClusterBigSW>("spell_lunar_festival_cluster_big_sw");
    RegisterSpellScript<LunarFestivalClusterBigU>("spell_lunar_festival_cluster_big_u");
}
