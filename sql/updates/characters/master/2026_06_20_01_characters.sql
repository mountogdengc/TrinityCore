CREATE TABLE IF NOT EXISTS `character_bot_cohort` (
  `owner_guid` BIGINT UNSIGNED NOT NULL,
  `companion_guid` BIGINT UNSIGNED NOT NULL,
  `active` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
  `auto_spawn` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`owner_guid`, `companion_guid`),
  KEY `idx_character_bot_cohort_owner_active` (`owner_guid`, `active`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
