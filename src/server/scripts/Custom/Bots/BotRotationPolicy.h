/*
 * Player-bot rotation policy helpers (pure, unit-tested).
 */

#ifndef TRINITYCORE_BOTS_BOTROTATIONPOLICY_H
#define TRINITYCORE_BOTS_BOTROTATIONPOLICY_H

#include "Define.h"

namespace BotRotationPolicy
{
    // Fork condition opcodes. Interpreted by BotRotation ONLY for custom Assisted
    // Combat steps (ID >= 1000000); Blizzard's opcodes on stock steps are ignored
    // (fail-open). Values sit well outside Blizzard's small opcode range.
    enum BotConditionType : int32
    {
        BOT_COND_TARGET_MISSING_OR_EXPIRING_MY_AURA = 1000,
    };

    // True when a DoT-style candidate should still be cast: the aura is absent, or
    // its remaining duration is below the refresh window. A permanent aura
    // (remainingMs < 0) that is present returns false (never re-cast). refreshMs == 0
    // collapses to "cast only when absent".
    bool ShouldCastForMissingOrExpiringAura(bool botAuraPresent, int32 remainingMs, int32 refreshMs);
}

#endif // TRINITYCORE_BOTS_BOTROTATIONPOLICY_H
