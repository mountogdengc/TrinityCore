-- Custom: give allied races the New Player Experience (Exile's Reach) start option.
--
-- Allied races ship with no NPE position in `playercreateinfo`, so even when the client
-- requests UseNPE the server falls back to their level-10 heritage start (Player::Create).
-- Populate the NPE columns with the faction-appropriate Exile's Reach spawn (the same
-- coordinates / transport / intro scene the core races use) so that choosing "Exile's Reach"
-- at character creation routes an allied race onto the starter ship at level 1. The level-1
-- part is handled in code (Player::GetStartLevel skips the allied-race level bump in NPE mode).
--
-- Notes:
--   * Hero classes are excluded (Death Knight = 6) so they keep their dedicated starting zone;
--     allied races cannot be Demon Hunter / Evoker, so those need no exclusion.
--   * Guarded by `npe_map IS NULL` so any existing NPE data is never overwritten.
--   * Their normal (heritage) start is unchanged: pick "normal start" and it behaves as before.

-- Alliance allied races (Void Elf, Lightforged Draenei, Kul Tiran, Dark Iron Dwarf,
-- Mechagnome, Earthen-A) -> Alliance Exile's Reach (transport 29, intro scene 2236).
UPDATE `playercreateinfo` SET
    `npe_map` = 2175, `npe_position_x` = 11.1301, `npe_position_y` = -0.417182,
    `npe_position_z` = 5.18741, `npe_orientation` = 3.14843,
    `npe_transport_guid` = 29, `npe_intro_scene_id` = 2236
WHERE `race` IN (29, 30, 32, 34, 37, 86) AND `class` <> 6 AND `npe_map` IS NULL;

-- Horde allied races (Nightborne, Highmountain Tauren, Zandalari, Vulpera, Mag'har Orc,
-- Earthen-H) -> Horde Exile's Reach (transport 30, intro scene 2486).
UPDATE `playercreateinfo` SET
    `npe_map` = 2175, `npe_position_x` = -10.7291, `npe_position_y` = -7.14635,
    `npe_position_z` = 8.73113, `npe_orientation` = 1.56321,
    `npe_transport_guid` = 30, `npe_intro_scene_id` = 2486
WHERE `race` IN (27, 28, 31, 35, 36, 91) AND `class` <> 6 AND `npe_map` IS NULL;
