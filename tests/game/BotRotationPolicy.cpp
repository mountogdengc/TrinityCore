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

#include "BotRotationPolicy.h"

TEST_CASE("Bot rotation casts a DoT when the target lacks it", "[BotRotationPolicy]")
{
    REQUIRE(BotRotationPolicy::ShouldCastForMissingOrExpiringAura(false, 0, 3000));
}

TEST_CASE("Bot rotation refreshes a DoT only when about to expire", "[BotRotationPolicy]")
{
    // Plenty of time left -> do NOT re-cast (filler should run instead).
    REQUIRE_FALSE(BotRotationPolicy::ShouldCastForMissingOrExpiringAura(true, 8000, 3000));
    // Inside the refresh window -> re-cast.
    REQUIRE(BotRotationPolicy::ShouldCastForMissingOrExpiringAura(true, 2000, 3000));
    // Exactly at the boundary -> not yet (strictly less-than).
    REQUIRE_FALSE(BotRotationPolicy::ShouldCastForMissingOrExpiringAura(true, 3000, 3000));
}

TEST_CASE("Bot rotation never re-casts a permanent aura", "[BotRotationPolicy]")
{
    REQUIRE_FALSE(BotRotationPolicy::ShouldCastForMissingOrExpiringAura(true, -1, 3000));
}

TEST_CASE("Bot rotation with no refresh window casts only when the aura is absent", "[BotRotationPolicy]")
{
    REQUIRE(BotRotationPolicy::ShouldCastForMissingOrExpiringAura(false, 0, 0));
    REQUIRE_FALSE(BotRotationPolicy::ShouldCastForMissingOrExpiringAura(true, 5000, 0));
}
