-- =========================================================================
-- 批次 4：社区 Fork 分支与 Upstream Issue/PR 关键修复汇编
-- 包含：重击长戟斩首触发、通灵学院召唤侍僧引怪、萨满闪电之盾死循环、
--       玛瑟里顿落石目标过滤、术士 T3 2件套吸血触发几率
-- =========================================================================

-- 1. 修复 #3891 / PR #890：重击长戟（Halberd of Smiting，物品 19874）斩首效果无限叠层
-- 原始配置中 spelltrigger_1 = 2（击中时可能触发）导致每次挥砍重复施加被动光环 25669 并无限叠加；
-- 修正为 spelltrigger_1 = 1（装备时生效），使被动光环仅在装备时施加一次并通过法术触发事件正常判定
UPDATE `item_template` SET `spelltrigger_1` = 1 WHERE `entry` = 19874;

-- 2. 修复 #4305：通灵学院黑暗召唤师（11582）召唤复生的仆从（法术 17618）触发全场连锁引怪
-- 原始法术使用 TARGET_LOCATION_SCRIPT_NEAR_CASTER 并在全图搜索脆弱的骷髅（11258），
-- 导致跨群落寻怪并在远处怪堆旁召唤仆从引发 Chamber of Summoning 全场灭顶连锁引仇恨；
-- 修正为 TARGET_LOCATION_CASTER_FRONT（47）与 3 码半径（15），使仆从始终在召唤者正前方生成
UPDATE `spell_template` SET `EffectImplicitTargetA1` = 47, `EffectRadiusIndex1` = 15 WHERE `Id` = 17618;

-- 3. 修复 #4306：萨满闪电之盾（Lightning Shield，法术 324）面对带闪电盾的目标触发死循环瞬秒
-- 原始配置中 procEx 设置了 PROC_EX_EX_TRIGGER_ALWAYS（65536），无视所有内置冷却与触发间隔，
-- 且内置冷却 Cooldown 错误配置为 3000ms；
-- 修正为 procEx = 0 并设置官方标准的 3500ms 内置冷却，杜绝受击弹幕死循环
UPDATE `spell_proc_event` SET `procEx` = 0, `Cooldown` = 3500 WHERE `entry` = 324;

-- 4. 修复 PR #876：玛瑟里顿碎石落石点名法术（Debris，法术 30629）误命中图腾、宠物与隐形触发器
-- 为该法术附加 SPELL_ATTR_EX3_ONLY_ON_PLAYER（256），确保仅对玩家角色施放
UPDATE `spell_template` SET `AttributesEx3` = `AttributesEx3` | 256 WHERE `Id` = 30629;

-- 5. 修复 #3941 / PR #894：术士 T3 疫病之心 2 件套吸血效果（Vampirism，法术 28831）暗影箭 100% 触发
-- 原始配置中 EffectBasePoints1 误设为 100% 几率；
-- 修正为 EffectBasePoints1 = 9（基础点数+1=10%几率）且限定 EffectItemType1 = 0x1（暗影箭掩码）
UPDATE `spell_template` SET `EffectBasePoints1` = 9, `EffectItemType1` = 0x1 WHERE `Id` = 28831;
