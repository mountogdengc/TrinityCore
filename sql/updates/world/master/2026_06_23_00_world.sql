--
-- Quest 24974 "Ever So Lonely" (Tirisfal Glades):
-- the captured murloc receives credit and cosmetic chains, but never gets the
-- follow order that should bring it back to Sedrick Calston.
--

DELETE FROM `smart_scripts`
WHERE `entryorguid` = 3892300
  AND `source_type` = 9
  AND `id` = 4;

INSERT INTO `smart_scripts`
    (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`,
     `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`,
     `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`,
     `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`,
     `target_x`, `target_y`, `target_z`, `target_o`, `comment`)
VALUES
    (3892300, 9, 4, 0, '', 0, 0, 100, 0,
     0, 0, 0, 0, 0, '',
     29, 1, 0, 0, 0, 0, 0, 0, '',
     7, 0, 0, 0, 0, '',
     0, 0, 0, 0, 'PuddlejumperC - Actionlist - Follow invoker back to Sedrick');
