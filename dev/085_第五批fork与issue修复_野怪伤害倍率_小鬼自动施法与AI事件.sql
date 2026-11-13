-- =========================================================================
-- 批次 5：社区 Fork 分支与 Upstream Issue/PR 关键修复汇编
-- 包含：24-25级野兽普通伤害超标10倍修复、小鬼相位移动下血之契印自动施法、
--       暮光复仇者与冬泉霜刃豹随从死亡激怒事件修复、泥链镇捕获图腾任务推进
-- =========================================================================

-- 1. 修复 #4301：湿地/暮色森林/希尔斯布莱德 24-25 级普通野怪伤害倍率被错写为 22
-- 斑点夜行蛛（1021）、黑色破坏者（628）、深海巨齿鲨（2188）、巨型苔原爬行者（2349）
-- 模板中 DamageMultiplier 误设为 22（比同级普通怪高出约 10 倍，单击超过 300 点物理伤害秒杀同级玩家）；
-- 修正为标准的 2.08，使其普攻伤害回落到正常的 35~48 点区间
UPDATE `creature_template` SET `DamageMultiplier` = 2.08 WHERE `Entry` IN (1021, 628, 2188, 2349);

-- 2. 修复 #4088：术士小鬼在处于相位变换（Phase Shift，免疫状态）时无法自动施放血之契印（Blood Pact）
-- 原核心在宠物自动施法判定中通过 CanAssist 检查免疫，因相位变换处于免疫状态而返回失败，导致小鬼召唤后挂机；
-- 为血之契印全等级法术附加 SPELL_ATTR_EX6_CAN_ASSIST_IMMUNE_PC（0x8），允许其为处于免疫状态的自身/主人提供辅助增益
UPDATE `spell_template` SET `AttributesEx6` = `AttributesEx6` | 8 WHERE `Id` IN (6307, 7804, 7805, 11766, 11767, 27268);

-- 3. 修复 #4050：暮光复仇者（11880）与蓟皮复仇者（3925）在 30% 血量错误施放复仇技能，以及霜刃豹幼崽死亡事件
-- 移除错误的 30% 血量无条件释放配置，改为标准的同伴死亡触发机制
DELETE FROM `creature_ai_scripts` WHERE `id` IN (1188003, 392502);

-- 暮光地灵师（11881）死亡时向周围发送 AIEvent，暮光复仇者（11880）接收到事件后施放复仇（8602）
DELETE FROM `creature_ai_scripts` WHERE `id` IN (1188002, 1188105);
INSERT INTO `creature_ai_scripts` (id,creature_id,event_type,event_inverse_phase_mask,event_chance,event_flags,event_param1,event_param2,event_param3,event_param4,event_param5,event_param6,action1_type,action1_param1,action1_param2,action1_param3,action2_type,action2_param1,action2_param2,action2_param3,action3_type,action3_param1,action3_param2,action3_param3,comment) VALUES
(1188002, 11880, 30, 0, 100, 1, 5, 11881, 0, 0, 0, 0, 11, 8602, 0, 0, 1, 1151, 0, 0, 0, 0, 0, 0, 'Twilight Avenger - Cast Vengeance on ReceiveEventA'),
(1188105, 11881, 6, 0, 100, 0, 0, 0, 0, 0, 0, 0, 45, 5, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'Twilight Geolord - Throw AIEventA on Death');

-- 霜刃豹幼崽（7430）死亡时向周围发送 AIEvent，霜刃豹守卫者（7434）接收到事件后施放激怒（15716）
DELETE FROM `creature_ai_scripts` WHERE `id` IN (743002, 743401);
INSERT INTO `creature_ai_scripts` (id,creature_id,event_type,event_inverse_phase_mask,event_chance,event_flags,event_param1,event_param2,event_param3,event_param4,event_param5,event_param6,action1_type,action1_param1,action1_param2,action1_param3,action2_type,action2_param1,action2_param2,action2_param3,action3_type,action3_param1,action3_param2,action3_param3,comment) VALUES
(743002, 7430, 6, 0, 100, 0, 0, 0, 0, 0, 0, 0, 45, 5, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'Frostsaber Cub - Send AIEventA on Death'),
(743401, 7434, 30, 0, 100, 1, 5, 7430, 0, 0, 0, 0, 11, 15716, 0, 0, 1, 1151, 0, 0, 0, 0, 0, 0, 'Frostsaber Pride Watcher - Cast Enrage on Receiving Frostsaber Cub Death AIEventA');

-- 4. 修复 #4048：尘泥沼泽任务物品 33101 召唤的捕获的图腾（23811）生成后不施法导致任务卡死
-- 捕获的图腾生成时（Event 11）立即对自身施放被动诱捕法术 42464
DELETE FROM `creature_ai_scripts` WHERE `creature_id` = 23811;
INSERT INTO `creature_ai_scripts` (id, creature_id, event_type, action1_type, action1_param1, action1_param2, comment) VALUES
(2381100, 23811, 11, 11, 42464, 15, 'Captured Totem - Cast Passive 42464 on Spawn');
