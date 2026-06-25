/*
 * Player-bot movement policy helpers (pure, unit-tested).
 */

#ifndef TRINITYCORE_BOTS_BOTMOVEMENTPOLICY_H
#define TRINITYCORE_BOTS_BOTMOVEMENTPOLICY_H

#include "Define.h"

namespace BotMovementPolicy
{
    // Classes that should fight at range (caster DPS + healer). Everyone else, and
    // every hybrid for now, is treated as melee.
    bool IsRangedClass(uint8 cls);

    // Per-bot formation angle in radians, relative to the anchor's orientation
    // (0 = directly in front, PI = directly behind). Deterministic and distinct for
    // adjacent slots. Follow fans symmetrically behind the master; chase spreads
    // evenly around the target.
    float FormationFollowAngle(uint32 slot);
    float FormationChaseAngle(uint32 slot);
}

#endif // TRINITYCORE_BOTS_BOTMOVEMENTPOLICY_H
