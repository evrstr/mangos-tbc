#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""审计 original_data/Spell.sql 中的法术行：按 SpellEntryfmt 列序解析关键字段。
按 SpellEntryfmt（src/game/Server/SQLStorages.cpp）列序解析 spell_template dump 行，
用于核对 Reagent/AttributesEx6/procFlags/DurationIndex/StackAmount 等字段，
支撑 #4109 / #4022 / #4108 / #4101 / #4060 的根因定位。
"""

import re
import sys

FMT = ("iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiifiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiffffffiiiiiiiiiiiiiiiiiiiiifffiiiiiiiiiiiiiiifff"
       "iiiissssssssssssssssssssssssssssssssiiiiiLiiiifffiiiiiiiffffffii")

# 与 SpellEntry 结构体字段序对应（SQLStorage 按列序读取）
NAMES = (
    ["Id", "Category", "CastUI", "Dispel", "Mechanic", "Attributes", "AttributesEx", "AttributesEx2", "AttributesEx3",
     "AttributesEx4", "AttributesEx5", "AttributesEx6", "Stances", "StancesNot", "Targets", "TargetCreatureType",
     "RequiresSpellFocus", "FacingCasterFlags", "CasterAuraState", "TargetAuraState", "CasterAuraStateNot",
     "TargetAuraStateNot", "CastingTimeIndex", "RecoveryTime", "CategoryRecoveryTime", "InterruptFlags",
     "AuraInterruptFlags", "ChannelInterruptFlags", "procFlags", "procChance", "procCharges", "maxLevel", "baseLevel",
     "spellLevel", "DurationIndex", "powerType", "manaCost", "manaCostPerlevel", "manaPerSecond", "manaPerSecondPerLevel",
     "rangeIndex"]
    + ["speed"]
    + ["modalNextSpell", "StackAmount", "Totem1", "Totem2"]
    + ["Reagent%d" % (i + 1) for i in range(8)]
    + ["ReagentCount%d" % (i + 1) for i in range(8)]
    + ["EquippedItemClass", "EquippedItemSubClassMask", "EquippedItemInventoryTypeMask"]
    + ["Effect%d" % (i + 1) for i in range(3)]
    + ["EffectDieSides%d" % (i + 1) for i in range(3)]
    + ["EffectBaseDice%d" % (i + 1) for i in range(3)]
    + ["EffectDicePerLevel%d" % (i + 1) for i in range(3)]
    + ["EffectRealPointsPerLevel%d" % (i + 1) for i in range(3)]
    + ["EffectBasePoints%d" % (i + 1) for i in range(3)]
    + ["EffectMechanic%d" % (i + 1) for i in range(3)]
    + ["EffectImplicitTargetA%d" % (i + 1) for i in range(3)]
    + ["EffectImplicitTargetB%d" % (i + 1) for i in range(3)]
    + ["EffectRadiusIndex%d" % (i + 1) for i in range(3)]
    + ["EffectApplyAuraName%d" % (i + 1) for i in range(3)]
    + ["EffectAmplitude%d" % (i + 1) for i in range(3)]
    + ["EffectMultipleValue%d" % (i + 1) for i in range(3)]
    + ["EffectChainTarget%d" % (i + 1) for i in range(3)]
    + ["EffectItemType%d" % (i + 1) for i in range(3)]
    + ["EffectMiscValue%d" % (i + 1) for i in range(3)]
    + ["EffectMiscValueB%d" % (i + 1) for i in range(3)]
    + ["EffectTriggerSpell%d" % (i + 1) for i in range(3)]
    + ["EffectPointsPerComboPoint%d" % (i + 1) for i in range(3)]
    + ["SpellVisual", "SpellIconID", "activeIconID", "spellPriority"]
    + ["SpellName%d" % (i + 1) for i in range(16)]
    + ["Rank%d" % (i + 1) for i in range(16)]
    + ["ManaCostPercentage", "StartRecoveryCategory", "StartRecoveryTime", "MaxTargetLevel", "SpellFamilyName",
       "SpellFamilyFlags", "MaxAffectedTargets", "DmgClass", "PreventionType", "StanceBarOrder"]
    + ["DmgMultiplier%d" % (i + 1) for i in range(3)]
    + ["MinFactionId", "MinReputation", "RequiredAuraVision", "TotemCategory1", "TotemCategory2", "AreaId",
       "SchoolMask"]
    + ["effectBonusCoefficient%d" % (i + 1) for i in range(3)]
    + ["effectBonusCoefficientFromAP%d" % (i + 1) for i in range(3)]
    + ["IsServerSide", "AttributesServerside"]
)

def split_row(text):
    fields, cur, in_str, i = [], "", False, 0
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "'" and i + 1 < len(text) and text[i + 1] == "'":
                cur += "'"; i += 2; continue
            if c == "'":
                in_str = False; cur += "'"; i += 1; continue
            cur += c
        else:
            if c == "'":
                in_str = True; cur += "'"; i += 1; continue
            if c == ',':
                fields.append(cur.strip()); cur = ""; i += 1; continue
            cur += c
        i += 1
    if cur.strip():
        fields.append(cur.strip())
    return fields

def parse_row(line):
    m = re.match(r"\s*\((.*)\)\s*,?\s*$", line)
    if not m:
        return None
    return split_row(m.group(1))

def main():
    path = "sql/base/dbc/original_data/Spell.sql"
    wanted = set(int(x) for x in sys.argv[1:])
    types = list(FMT)
    print("fmt cols=%d names=%d" % (len(types), len(NAMES)))
    if len(types) != len(NAMES):
        print("!! fmt 与字段名数量不一致，中止")
        return 1
    hits = 0
    poisons = []
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            if not line.lstrip().startswith("("):
                continue
            fields = parse_row(line)
            if not fields or len(fields) != len(NAMES):
                continue
            try:
                sid = int(fields[0])
            except ValueError:
                continue
            if sid in wanted:
                hits += 1
                print("---- spell %d ----" % sid)
                for key in ("Attributes", "AttributesEx", "AttributesEx2", "AttributesEx3", "AttributesEx4",
                            "AttributesEx5", "AttributesEx6", "procFlags", "procChance", "DurationIndex",
                            "StackAmount", "Reagent1", "ReagentCount1", "Effect1", "EffectApplyAuraName1",
                            "EffectTriggerSpell1", "EffectImplicitTargetA1", "SpellFamilyName", "SpellFamilyFlags",
                            "EquippedItemClass", "EquippedItemSubClassMask", "EquippedItemInventoryTypeMask"):
                    print("  %-28s = %s" % (key, fields[NAMES.index(key)]))
            else:
                try:
                    fam = int(fields[NAMES.index("SpellFamilyName")])
                    eff1 = int(fields[NAMES.index("Effect1")])
                    eqclass = int(fields[NAMES.index("EquippedItemClass")])
                except (ValueError, IndexError):
                    continue
                if fam == 8 and eff1 == 53 and eqclass == 2:  # rogue enchant-weapon poisons
                    name = fields[NAMES.index("SpellName1")]
                    poisons.append((sid, name, fields[NAMES.index("AttributesEx")]))
    if wanted:
        print("matched %d/%d" % (hits, len(wanted)))
    if poisons:
        print("---- rogue weapon-enchants (Effect1=53, family=8) ----")
        for sid, name, ex in poisons:
            print("  %-8d AttributesEx=%-10s %s" % (sid, ex, name))
    return 0

if __name__ == "__main__":
    sys.exit(main())
