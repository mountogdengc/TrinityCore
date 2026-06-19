-- =====================================================================
-- SPIKE: Assisted Combat (Single-Button Assistant) for low-level Hunters
-- =====================================================================
--
-- Goal: prove whether the 12.0 client will render an Assisted Combat
-- rotation for a PRE-SPEC (sub level 10) character. Sub-10 characters
-- report their class's "Initial" ChrSpecialization, for which Blizzard
-- ships no AssistedCombat data. This adds custom rows for the Hunter
-- Initial spec and pushes them to the client via the hotfix system.
--
-- Apply to the HOTFIXES database, e.g.:
--   mysql -u trinity -p hotfixes < spike_assisted_combat_hunter_lowlevel.sql
--
-- Requires the assisted_combat / assisted_combat_step tables to exist
-- (created by sql/updates/hotfixes/master/2026_06_19_00_hotfixes.sql).
--
-- !! ONE VALUE TO VERIFY for your build: @HUNTER_INITIAL_SPEC = 1448.
--    This must equal what a level-7 Hunter reports as its primary spec
--    (GetDefaultChrSpecializationForClass(HUNTER) = ChrSpecialization
--    row with ClassID=3, OrderIndex=4). If 1448 is wrong for your build,
--    the list stays empty and the spike is inconclusive -- change it here.
--
-- Table hashes (from WoWDBDefs manifest, db2 header table_hash):
--   AssistedCombat      0xA4A21680 = 2762086016
--   AssistedCombatStep  0x790BCC4F = 2030816335
-- =====================================================================

SET @HUNTER_INITIAL_SPEC := 1448;   -- Hunter "Initial" ChrSpecializationID (VERIFY)

SET @AC_ID    := 1000001;           -- custom AssistedCombat row id (high, collision-safe)
SET @STEP1_ID := 1000001;           -- custom AssistedCombatStep row ids
SET @STEP2_ID := 1000002;

SET @HASH_AC   := 2762086016;       -- 0xA4A21680
SET @HASH_STEP := 2030816335;       -- 0x790BCC4F

SET @PUSH_ID := 9000001;            -- hotfix push id (high, collision-safe)

-- Hunter low-level damage spells used as the rotation steps.
SET @SPELL_ARCANE_SHOT := 185358;   -- Arcane Shot (known very early)
SET @SPELL_STEADY_SHOT := 56641;    -- Steady Shot

-- --- clean any prior run -------------------------------------------------
DELETE FROM `assisted_combat`      WHERE `ID` = @AC_ID;
DELETE FROM `assisted_combat_step` WHERE `ID` IN (@STEP1_ID, @STEP2_ID);
DELETE FROM `hotfix_data`          WHERE `Id` = @PUSH_ID;

-- --- the spec -> rotation container -------------------------------------
INSERT INTO `assisted_combat` (`ID`, `ChrSpecializationID`, `VerifiedBuild`) VALUES
(@AC_ID, @HUNTER_INITIAL_SPEC, 0);

-- --- the ordered spell steps --------------------------------------------
INSERT INTO `assisted_combat_step` (`ID`, `SpellID`, `AssistedCombatID`, `OrderIndex`, `VerifiedBuild`) VALUES
(@STEP1_ID, @SPELL_ARCANE_SHOT, @AC_ID, 0, 0),
(@STEP2_ID, @SPELL_STEADY_SHOT, @AC_ID, 1, 0);

-- --- tell the client these records exist (Status 1 = Valid) -------------
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(@PUSH_ID, 2000000001, @HASH_AC,   @AC_ID,    1, 0),
(@PUSH_ID, 2000000002, @HASH_STEP, @STEP1_ID, 1, 0),
(@PUSH_ID, 2000000003, @HASH_STEP, @STEP2_ID, 1, 0);

-- =====================================================================
-- ROLLBACK (run these to remove the spike):
--   DELETE FROM `assisted_combat`      WHERE `ID` = 1000001;
--   DELETE FROM `assisted_combat_step` WHERE `ID` IN (1000001, 1000002);
--   DELETE FROM `hotfix_data`          WHERE `Id` = 9000001;
-- =====================================================================
