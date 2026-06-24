-- =====================================================================
-- Condition-aware low-level Priest rotation
-- =====================================================================
-- Gates the Priest "Initial" spec (1452) Shadow Word: Pain step on
-- "target is missing my SWP, or it is about to expire" so the bot casts
-- Smite (the rule-less filler step) while SWP is up, and refreshes SWP
-- under 3s remaining. Without this, SWP (cheap, no cooldown) is the top
-- castable spell every GCD and Smite never fires.
--
-- Fork opcode 1000 = BOT_COND_TARGET_MISSING_OR_EXPIRING_MY_AURA, evaluated
-- by BotRotation ONLY for custom steps (ID >= 1000000). ConditionValue1 = 0
-- => use the step's own spell (589, SWP); ConditionValue2 = 3000 => refresh
-- window in ms; ConditionValue3 unused. Targets step 1000010 (the SWP step
-- from 2026_06_21_00_hotfixes.sql). The Smite step (1000011) gets no rule.
--
-- Auto-applied by the worldserver updater (hotfixes DB); the hotfix loader
-- then applies the Status=1 row to the in-memory sAssistedCombatRuleStore.
-- Append-only (Updates.Redundancy=0): never edit this file once applied; add
-- 2026_06_24_01_hotfixes.sql to correct a value.
--
-- Table hash (db2 header table_hash):
--   AssistedCombatRule  0xC1B4F680 = 3249862272
-- =====================================================================

-- --- idempotent cleanup (safe re-run; the updater applies once) ----------
DELETE FROM `assisted_combat_rule` WHERE `ID` IN (1000010);
DELETE FROM `hotfix_data`          WHERE `Id` IN (9000030);

INSERT INTO `assisted_combat_rule`
    (`ID`, `OrderIndex`, `Field_11_1_7_60520_002`, `ConditionType`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `AssistedCombatStepID`, `VerifiedBuild`) VALUES
(1000010, 0, 0, 1000, 0, 3000, 0, 1000010, 0);

INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000030, 2000000030, 3249862272, 1000010, 1, 0);
