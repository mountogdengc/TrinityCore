CREATE TABLE IF NOT EXISTS `character_bot_cohort_owner` (
  `owner_guid` BIGINT UNSIGNED NOT NULL,
  `auto_spawn` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`owner_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
