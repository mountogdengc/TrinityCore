/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tc_catch2.h"

#include "BotMovementPolicy.h"
#include "SharedDefines.h"

TEST_CASE("Ranged classes fight at range; melee/hybrids do not", "[BotMovementPolicy]")
{
    REQUIRE(BotMovementPolicy::IsRangedClass(CLASS_HUNTER));
    REQUIRE(BotMovementPolicy::IsRangedClass(CLASS_PRIEST));
    REQUIRE(BotMovementPolicy::IsRangedClass(CLASS_MAGE));
    REQUIRE(BotMovementPolicy::IsRangedClass(CLASS_WARLOCK));
    REQUIRE(BotMovementPolicy::IsRangedClass(CLASS_EVOKER));

    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_WARRIOR));
    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_ROGUE));
    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_DEATH_KNIGHT));
    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_DRUID));   // hybrid -> melee for now
    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_SHAMAN));  // hybrid -> melee for now
}

TEST_CASE("Formation angles are deterministic and distinct for adjacent slots", "[BotMovementPolicy]")
{
    // anchored geometry: slot 0 is directly behind; slot 1 fans one step right
    REQUIRE(BotMovementPolicy::FormationFollowAngle(0) == Approx(3.14159265358979f));
    REQUIRE(BotMovementPolicy::FormationFollowAngle(1) == Approx(3.14159265358979f + 0.45f));
    REQUIRE(BotMovementPolicy::FormationChaseAngle(0) == Approx(0.0f));
    REQUIRE(BotMovementPolicy::FormationChaseAngle(1) == Approx(2.0f * 3.14159265358979f / 6.0f));

    // distinct for adjacent slots
    REQUIRE(BotMovementPolicy::FormationFollowAngle(0) != BotMovementPolicy::FormationFollowAngle(1));
    REQUIRE(BotMovementPolicy::FormationFollowAngle(1) != BotMovementPolicy::FormationFollowAngle(2));
    REQUIRE(BotMovementPolicy::FormationChaseAngle(0) != BotMovementPolicy::FormationChaseAngle(1));
    REQUIRE(BotMovementPolicy::FormationChaseAngle(1) != BotMovementPolicy::FormationChaseAngle(2));
}
