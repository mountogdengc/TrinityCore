-- Fix Deathknell Recruitment companion behavior.
-- Darnell should follow the player, assist in combat, and collect nearby corpses
-- directly instead of relying on corpse-side LOS against a private summon.

UPDATE `creature_template`
SET `AIName` = '',
    `ScriptName` = 'npc_darnell_recruitment'
WHERE `entry` = 49337;

UPDATE `creature_template`
SET `AIName` = '',
    `ScriptName` = ''
WHERE `entry` = 49340;
