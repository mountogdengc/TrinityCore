/*
 * Custom: Death QoL — pure decision helpers (no engine state, unit-testable).
 * See docs/superpowers/specs/2026-06-20-death-qol-design.md
 */

#ifndef TRINITYCORE_BOTS_BOTDEATHPOLICY_H
#define TRINITYCORE_BOTS_BOTDEATHPOLICY_H

#include <cstdint>

namespace Bots::DeathPolicy
{
// A dead bot should auto-revive only while its master is alive (so it doesn't run
// straight back into the same death while the owner is also down), once enabled
// and the post-death delay has elapsed.
bool ShouldBotAutoRevive(bool enabled, bool botIsDead, bool masterIsAlive,
    uint32_t msSinceDeath, uint32_t delayMs);

// A dead player should auto-revive at their corpse once enabled and the delay has
// elapsed — but never on battleground/arena maps, where death is a core mechanic.
bool ShouldPlayerAutoRevive(bool enabled, bool isInWorld, bool isDead,
    bool isArenaOrBattleground, uint32_t msSinceDeath, uint32_t delayMs);

// True when the `.revive` argument selects the "return to corpse" variant, i.e.
// the (trimmed, case-insensitive) argument is exactly "corpse".
bool IsReviveCorpseArg(char const* args);
}

#endif // TRINITYCORE_BOTS_BOTDEATHPOLICY_H
