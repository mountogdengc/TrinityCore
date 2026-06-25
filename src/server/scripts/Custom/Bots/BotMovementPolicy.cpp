/*
 * Player-bot movement policy helpers -- see BotMovementPolicy.h.
 */

#include "BotMovementPolicy.h"
#include "SharedDefines.h"

namespace
{
    constexpr float  BOT_PI                = 3.14159265358979f;
    constexpr float  BOT_FOLLOW_FAN_STEP   = 0.45f; // rad (~26 deg) between adjacent follow slots; ~5 bots stay in the rear arc
    constexpr uint32 BOT_CHASE_SECTORS     = 6;      // chase angles wrap every 6 slots (60 deg apart)
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
        case CLASS_EVOKER:
            return true;
        default:
            return false;
    }
}

float FormationFollowAngle(uint32 slot)
{
    // Symmetric fan behind the master: PI (center), then alternating right/left a step
    // per slot pair -- slot 0 -> PI, 1 -> PI+step, 2 -> PI-step, 3 -> PI+2*step, ...
    uint32 const pair = (slot + 1) / 2;
    float const side = (slot % 2 == 1) ? 1.0f : -1.0f;
    return BOT_PI + side * float(pair) * BOT_FOLLOW_FAN_STEP;
}

float FormationChaseAngle(uint32 slot)
{
    // Spread evenly around the target: BOT_CHASE_SECTORS distinct directions, 60 deg apart.
    return float(slot % BOT_CHASE_SECTORS) * (2.0f * BOT_PI / float(BOT_CHASE_SECTORS));
}
}
