/*
 * Player-bot movement policy helpers -- see BotMovementPolicy.h.
 */

#include "BotMovementPolicy.h"
#include "SharedDefines.h"

namespace
{
    constexpr float BOT_PI = 3.14159265358979f;
}

namespace BotMovementPolicy
{
bool IsRangedClass(uint8 cls)
{
    switch (cls)
    {
        case CLASS_HUNTER:
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return true;
        default:
            return false;
    }
}

float FormationFollowAngle(uint32 slot)
{
    // Symmetric fan behind the master: PI, PI+step, PI-step, PI+2*step, PI-2*step, ...
    uint32 const pair = (slot + 1) / 2;
    float const sign = (slot % 2 == 1) ? 1.0f : -1.0f;
    return BOT_PI + sign * float(pair) * 0.45f;
}

float FormationChaseAngle(uint32 slot)
{
    // Spread evenly around the target: 6 distinct directions, 60 deg apart.
    return float(slot % 6) * (2.0f * BOT_PI / 6.0f);
}
}
