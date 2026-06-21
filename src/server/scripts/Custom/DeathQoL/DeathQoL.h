/*
 * Custom: Death QoL — engine helper shared by the auto-revive worldscript and the
 * `.revive corpse` command. See docs/superpowers/specs/2026-06-20-death-qol-design.md
 */

#ifndef TRINITYCORE_CUSTOM_DEATHQOL_H
#define TRINITYCORE_CUSTOM_DEATHQOL_H

class Player;

namespace Custom::DeathQoL
{
// Teleport a dead player to their corpse and resurrect them there: full HP, no
// resurrection sickness (mirrors normal corpse reclaim, not spirit-healer revival).
// Returns false and does nothing if the player is null, already alive, or has no
// corpse. Used by both player auto-revive and `.revive corpse`.
bool ReturnToCorpseAndResurrect(Player* player);
}

#endif // TRINITYCORE_CUSTOM_DEATHQOL_H
