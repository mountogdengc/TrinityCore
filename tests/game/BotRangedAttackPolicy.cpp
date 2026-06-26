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

#include "BotRangedAttackPolicy.h"

TEST_CASE("Ranged autorepeat starts only with a spell, and (re)starts on target switch", "[BotRangedAttackPolicy]")
{
    using BotRangedAttackPolicy::ShouldStartAutoRepeat;

    // No auto-attack spell -> never start, regardless of other state.
    REQUIRE_FALSE(ShouldStartAutoRepeat(false, false, false));
    REQUIRE_FALSE(ShouldStartAutoRepeat(false, false, true));
    REQUIRE_FALSE(ShouldStartAutoRepeat(false, true, true));

    // Has a spell, not yet repeating -> start.
    REQUIRE(ShouldStartAutoRepeat(true, false, false));

    // Has a spell, already repeating, same target -> do not restart.
    REQUIRE_FALSE(ShouldStartAutoRepeat(true, true, false));

    // Has a spell, already repeating, target switched -> restart at the new target.
    REQUIRE(ShouldStartAutoRepeat(true, true, true));
}
