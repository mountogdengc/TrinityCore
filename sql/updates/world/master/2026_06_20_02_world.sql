-- Fix Deathknell Recruitment: Darnell should defend the player and walk close enough
-- to Scarlet Corpses for the proximity-based pickup script to fire.

UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'npc_scarlet_corpse_recruitment'
WHERE `entry` = 49340;

UPDATE `smart_scripts`
SET `action_param1` = 1
WHERE `entryorguid` IN (4933700, 4933701)
  AND `source_type` = 9
  AND `action_type` = 8
  AND `action_param1` = 0;

UPDATE `smart_scripts`
SET `event_param3` = 8
WHERE `entryorguid` = 49340
  AND `source_type` = 0
  AND `id` = 0
  AND `event_type` = 75
  AND `event_param2` = 49337;

DELETE FROM `waypoint_path_node`
WHERE `PathId` = 394697
  AND `NodeId` >= 7;

INSERT INTO `waypoint_path_node` (`PathId`, `NodeId`, `PositionX`, `PositionY`, `PositionZ`, `Orientation`)
VALUES
    (394697, 7, 1897.90, 1596.55, 87.7583, NULL),
    (394697, 8, 1908.07, 1620.89, 90.3874, NULL),
    (394697, 9, 1936.35, 1551.70, 88.1211, NULL),
    (394697, 10, 1936.95, 1536.34, 90.2483, NULL),
    (394697, 11, 1970.03, 1588.29, 82.3629, NULL),
    (394697, 12, 1968.81, 1603.26, 88.2857, NULL);
