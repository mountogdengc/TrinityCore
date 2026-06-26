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

#include "BotPetPolicy.h"

TEST_CASE("ShouldSummonPet only for a live in-world hunter with no pet", "[BotPetPolicy]")
{
    using BotPetPolicy::ShouldSummonPet;
    REQUIRE(ShouldSummonPet(true, true, true, false));
    REQUIRE_FALSE(ShouldSummonPet(true, true, true, true));    // already has a pet
    REQUIRE_FALSE(ShouldSummonPet(false, true, true, false));  // not a hunter
    REQUIRE_FALSE(ShouldSummonPet(true, false, true, false));  // not in world
    REQUIRE_FALSE(ShouldSummonPet(true, true, false, false));  // dead bot
}

TEST_CASE("ShouldRevivePet once a dead pet passes the revive delay", "[BotPetPolicy]")
{
    using BotPetPolicy::ShouldRevivePet;
    REQUIRE_FALSE(ShouldRevivePet(false, false, 9999, 5000));  // no pet
    REQUIRE_FALSE(ShouldRevivePet(true, true, 9999, 5000));    // pet alive
    REQUIRE_FALSE(ShouldRevivePet(true, false, 4999, 5000));   // not yet
    REQUIRE(ShouldRevivePet(true, false, 5000, 5000));         // exactly at delay
    REQUIRE(ShouldRevivePet(true, false, 8000, 5000));         // past delay
}

TEST_CASE("NeedsLevelSync when levels differ", "[BotPetPolicy]")
{
    using BotPetPolicy::NeedsLevelSync;
    REQUIRE_FALSE(NeedsLevelSync(10, 10));
    REQUIRE(NeedsLevelSync(9, 10));
    REQUIRE(NeedsLevelSync(11, 10));
}
