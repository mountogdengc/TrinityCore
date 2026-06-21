-- =====================================================================
-- Low-level (sub-10, pre-spec) bot rotations: Hunter, Priest, Warrior
-- =====================================================================
--
-- Server-side rotation data for the M4 player-bot rotation engine
-- (BotRotation::SelectSpell). Sub-10 characters report their class's
-- "Initial" ChrSpecialization, for which Blizzard ships no AssistedCombat
-- data; these custom rows give the engine an ability priority list so a
-- headless bot casts spells instead of melee-only.
--
-- Auto-applied by the worldserver updater (hotfixes DB). At startup the
-- hotfix loader then applies the Status=1 rows to the in-memory
-- sAssistedCombatStore / sAssistedCombatStepStore, which is what the bot
-- reads. Supersedes the old manual
-- sql/custom/spike_assisted_combat_hunter_lowlevel.sql (Hunter rows folded in).
--
-- Initial ChrSpecializationID per class (a wrong value = empty list = bot
-- melees; verify via the BotMgr rotation DEBUG log, plan Task 5):
--   Hunter  (class 3) -> 1448  (confirmed by the prior spike)
--   Priest  (class 5) -> 1452  (verify build 67823)
--   Warrior (class 1) -> 1446  (verify build 67823)
-- Correction is APPEND-ONLY: never edit this applied file (Updates.Redundancy=0);
-- add 2026_06_21_01_hotfixes.sql to UPDATE a wrong ChrSpecializationID.
--
-- Unconditional, always-castable damage spells only (the engine does not yet
-- evaluate AssistedCombatRule conditions; HasSpell() gates unknown spells).
--
-- Table hashes (db2 header table_hash):
--   AssistedCombat      0xA4A21680 = 2762086016
--   AssistedCombatStep  0x790BCC4F = 2030816335
-- =====================================================================

-- --- idempotent cleanup (safe re-run; the updater applies once) ----------
DELETE FROM `assisted_combat`      WHERE `ID` IN (1000001, 1000010, 1000020);
DELETE FROM `assisted_combat_step` WHERE `ID` IN (1000001, 1000002, 1000010, 1000011, 1000020);
DELETE FROM `hotfix_data`          WHERE `Id` IN (9000001, 9000010, 9000020);

-- =========================== HUNTER (1448) ==============================
INSERT INTO `assisted_combat` (`ID`, `ChrSpecializationID`, `VerifiedBuild`) VALUES
(1000001, 1448, 0);
INSERT INTO `assisted_combat_step` (`ID`, `SpellID`, `AssistedCombatID`, `OrderIndex`, `VerifiedBuild`) VALUES
(1000001, 185358, 1000001, 0, 0),   -- Arcane Shot
(1000002,  56641, 1000001, 1, 0);   -- Steady Shot
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000001, 2000000001, 2762086016, 1000001, 1, 0),
(9000001, 2000000002, 2030816335, 1000001, 1, 0),
(9000001, 2000000003, 2030816335, 1000002, 1, 0);

-- =========================== PRIEST (1452) ==============================
INSERT INTO `assisted_combat` (`ID`, `ChrSpecializationID`, `VerifiedBuild`) VALUES
(1000010, 1452, 0);
INSERT INTO `assisted_combat_step` (`ID`, `SpellID`, `AssistedCombatID`, `OrderIndex`, `VerifiedBuild`) VALUES
(1000010, 589, 1000010, 0, 0),   -- Shadow Word: Pain (DoT first)
(1000011, 585, 1000010, 1, 0);   -- Smite (filler)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000010, 2000000010, 2762086016, 1000010, 1, 0),
(9000010, 2000000011, 2030816335, 1000010, 1, 0),
(9000010, 2000000012, 2030816335, 1000011, 1, 0);

-- =========================== WARRIOR (1446) =============================
INSERT INTO `assisted_combat` (`ID`, `ChrSpecializationID`, `VerifiedBuild`) VALUES
(1000020, 1446, 0);
INSERT INTO `assisted_combat_step` (`ID`, `SpellID`, `AssistedCombatID`, `OrderIndex`, `VerifiedBuild`) VALUES
(1000020, 1464, 1000020, 0, 0);   -- Slam
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000020, 2000000020, 2762086016, 1000020, 1, 0),
(9000020, 2000000021, 2030816335, 1000020, 1, 0);
