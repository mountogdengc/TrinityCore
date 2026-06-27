/*
 * Player-bot formation policy helpers (pure, unit-tested).
 */

#ifndef TRINITYCORE_BOTS_BOTFORMATIONPOLICY_H
#define TRINITYCORE_BOTS_BOTFORMATIONPOLICY_H

#include "Define.h"
#include "Optional.h"
#include <string_view>

enum class BotFormation : uint8 { Line, Wedge, Circle, Column };

// Polar offset in the master's frame: angle is relative to the master's orientation
// (0 = in front, PI = directly behind), as MoveFollow's ChaseAngle expects.
struct FormationOffset { float distance; float angle; };

namespace BotFormationPolicy
{
    // Each bot's spot for a preset, given its index within the squad (0..count-1) and the
    // squad size. Pure.
    FormationOffset Offset(BotFormation preset, uint32 slot, uint32 count);

    // Parse a command argument ("line"/"wedge"/"circle"/"column", case-insensitive).
    Optional<BotFormation> Parse(std::string_view name);
}

#endif // TRINITYCORE_BOTS_BOTFORMATIONPOLICY_H
