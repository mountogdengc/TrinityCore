/*
 * Player-bot rotation policy helpers -- see BotRotationPolicy.h.
 */

#include "BotRotationPolicy.h"

namespace BotRotationPolicy
{
bool ShouldCastForMissingOrExpiringAura(bool botAuraPresent, int32 remainingMs, int32 refreshMs)
{
    if (!botAuraPresent)
        return true;
    return remainingMs >= 0 && remainingMs < refreshMs;
}
}
