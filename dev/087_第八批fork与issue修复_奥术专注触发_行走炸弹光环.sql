-- =========================================================================
-- 批次 8：社区 Fork 分支与 Upstream Issue/PR 关键修复汇编
-- 包含：奥术专注（清晰预兆）不触发、诺莫瑞根行走炸弹无爆炸伤害
-- =========================================================================

-- 1. 修复 #4022：法师天赋"奥术专注"（12574~12577）不触发"清晰预兆"（12536）
-- 旧库 spell_proc_event 对 12574~12577 配置了 procEx = 131072 (0x20000, PROC_EX_EX_ONE_TIME_TRIGGER)，
-- 该扩展位从未被命中结果携带（procExtra 永不含此位），
-- 导致 SpellMgr::IsSpellProcEventCanTriggeredBy 走自定义扩展掩码分支且永不匹配 → 触发事件被整体挡死；
-- 清零 procEx 使触发回退到 spell_template 的 procFlags（87376 含 DEAL_HARMFUL_SPELL 等）+ 10%/8%/6%/4% 概率正常判定。
-- 行不存在时 UPDATE 无操作，脚本幂等。
UPDATE `spell_proc_event` SET `procEx` = 0 WHERE `entry` IN (12574, 12575, 12576, 12577) AND `procEx` <> 0;

-- 2. 修复 #4108：诺莫瑞根"行走炸弹"（7915）没有爆炸伤害
-- 爆炸伤害链：被动光环 11830（100% 近战命中触发 11504 AoE 火焰伤害）已入库，
-- 但 NPC 7915 未挂载该被动光环 → 近战命中永不触发爆炸；
-- 向 creature_template_addon 追加 auras=11830（已含 11830 则不重复追加，幂等）。
INSERT INTO `creature_template_addon` (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `auras`)
VALUES (7915, 0, 0, 0, 0, 0, '11830')
ON DUPLICATE KEY UPDATE `auras` = IF(`auras` IS NULL OR `auras` = '', '11830',
                                     IF(`auras` LIKE '%11830%', `auras`, CONCAT(`auras`, ' 11830')));
