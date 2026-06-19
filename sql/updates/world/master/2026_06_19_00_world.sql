--
-- Quest 25167 "Breaking the Chain" (Durotar): make Northwatch Lugs carry the
-- Northwatch Supply Crate and walk, instead of standing idle with the crate
-- dumped on the ground and the Lug rendered inside it.
--
-- Entries:
--   39245 Northwatch Lug   (Human variant, kill credit)
--   39249 Northwatch Lug   (Dwarf variant -> KillCredit1 = 39245)
--   39251 Northwatch Supply Crate (quest objective, must stay attackable)
--
-- The carry is the vehicle/accessory system: the Lug is the vehicle, the crate
-- is an accessory auto-seated onto it. There is NO carry "spell" -- the seat
-- offset lives in client Vehicle.db2, which is the one value not present in the
-- TDB. Dial @VEH / @SEAT in live (./tc sql) with `.npc add 39245` until the
-- crate sits over the Lug's head, then bake the final numbers in here.
--
-- @VEH = 0 leaves the Lug a non-vehicle: the accessory link, the crate AI and
-- the ground-crate cleanup all stay inert, so this file is a safe no-op for the
-- carry until you set a real vehicle id. Movement is applied regardless.
--

SET @VEH  := 0;   -- <<< Lug Vehicle.db2 id with an over-head seat (TUNE THIS)
SET @SEAT := 0;   -- <<< seat index on @VEH that holds the crate (TUNE THIS)

-- (1) Make both Lug variants vehicles.
UPDATE `creature_template` SET `VehicleId` = @VEH WHERE `entry` IN (39245, 39249);

-- (2) Auto-seat the crate onto each Lug on spawn. minion=1 ties the crate's life
--     to its Lug; summontype 8 = manual despawn (managed by the vehicle).
DELETE FROM `vehicle_template_accessory` WHERE `entry` IN (39245, 39249);
INSERT INTO `vehicle_template_accessory`
  (`entry`, `accessory_entry`, `seat_id`, `minion`, `description`, `summontype`, `summontimer`) VALUES
  (39245, 39251, @SEAT, 1, 'Northwatch Lug - Northwatch Supply Crate', 8, 0),
  (39249, 39251, @SEAT, 1, 'Northwatch Lug - Northwatch Supply Crate', 8, 0);

-- (3) "Destroy the crate and it falls on the Lug": when the carried crate dies,
--     kill the Lug carrying it (the crate's summoner). Harmless no-op while the
--     crate is a plain ground spawn (no summoner) i.e. when @VEH = 0.
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 39251;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 39251;
INSERT INTO `smart_scripts`
  (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`,
   `event_param1`, `event_param2`, `event_param3`, `event_param4`,
   `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
   `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
  (39251, 0, 0, 0, 6, 0, 100, 0,   0, 0, 0, 0,   51, 0, 0, 0, 0, 0, 0,   23, 0, 0, 0, 0, 0, 0, 0,
   'Northwatch Supply Crate - On Death - Kill carrying Northwatch Lug');

-- (4) Make the idle Lugs move. Random roam around the spawn point; a sniffed
--     waypoint patrol (waypoint_path / waypoint_path_node) would be more Blizzlike.
UPDATE `creature` SET `MovementType` = 1, `wander_distance` = 8
WHERE `id` IN (39245, 39249) AND `MovementType` = 0;

-- (5) AFTER @VEH is confirmed working in-game (each Lug visibly carries a crate),
--     remove the now-duplicate standalone ground crates. Verify the count first:
--       SELECT COUNT(*) FROM `creature` WHERE `id` = 39251 AND `map` = 1;
--     then uncomment:
-- DELETE FROM `creature` WHERE `id` = 39251 AND `map` = 1;
