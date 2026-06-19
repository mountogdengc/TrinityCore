--
-- Table structure for table `assisted_combat`
--
DROP TABLE IF EXISTS `assisted_combat`;
CREATE TABLE `assisted_combat` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ChrSpecializationID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- Table structure for table `assisted_combat_rule`
--
DROP TABLE IF EXISTS `assisted_combat_rule`;
CREATE TABLE `assisted_combat_rule` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `OrderIndex` int NOT NULL DEFAULT '0',
  `Field_11_1_7_60520_002` int NOT NULL DEFAULT '0',
  `ConditionType` int NOT NULL DEFAULT '0',
  `ConditionValue1` int NOT NULL DEFAULT '0',
  `ConditionValue2` int NOT NULL DEFAULT '0',
  `ConditionValue3` int NOT NULL DEFAULT '0',
  `AssistedCombatStepID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- Table structure for table `assisted_combat_step`
--
DROP TABLE IF EXISTS `assisted_combat_step`;
CREATE TABLE `assisted_combat_step` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `AssistedCombatID` int unsigned NOT NULL DEFAULT '0',
  `OrderIndex` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
