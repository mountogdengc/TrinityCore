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

#include "BotDeathPolicy.h"

using namespace Bots::DeathPolicy;

TEST_CASE("Bot auto-revive only fires when enabled, dead, master alive, past delay", "[DeathRevivePolicy]")
{
    // The one true case: everything satisfied.
    REQUIRE(ShouldBotAutoRevive(true, true, true, 5000, 5000));
    REQUIRE(ShouldBotAutoRevive(true, true, true, 6000, 5000));

    // Each precondition failing independently.
    REQUIRE_FALSE(ShouldBotAutoRevive(false, true, true, 5000, 5000)); // disabled
    REQUIRE_FALSE(ShouldBotAutoRevive(true, false, true, 5000, 5000)); // bot not dead
    REQUIRE_FALSE(ShouldBotAutoRevive(true, true, false, 5000, 5000)); // master dead
    REQUIRE_FALSE(ShouldBotAutoRevive(true, true, true, 4999, 5000));  // before delay
}

TEST_CASE("Player auto-revive excludes battlegrounds/arenas and respects the delay", "[DeathRevivePolicy]")
{
    REQUIRE(ShouldPlayerAutoRevive(true, true, true, false, 5000, 5000));
    REQUIRE(ShouldPlayerAutoRevive(true, true, true, false, 8000, 5000));

    REQUIRE_FALSE(ShouldPlayerAutoRevive(false, true, true, false, 5000, 5000)); // disabled
    REQUIRE_FALSE(ShouldPlayerAutoRevive(true, false, true, false, 5000, 5000)); // not in world
    REQUIRE_FALSE(ShouldPlayerAutoRevive(true, true, false, false, 5000, 5000)); // alive
    REQUIRE_FALSE(ShouldPlayerAutoRevive(true, true, true, true, 5000, 5000));   // bg/arena excluded
    REQUIRE_FALSE(ShouldPlayerAutoRevive(true, true, true, false, 4000, 5000));  // before delay
}

TEST_CASE("`.revive corpse` argument parsing", "[DeathRevivePolicy]")
{
    REQUIRE(IsReviveCorpseArg("corpse"));
    REQUIRE(IsReviveCorpseArg("CORPSE"));
    REQUIRE(IsReviveCorpseArg("  corpse  "));
    REQUIRE(IsReviveCorpseArg("Corpse"));

    REQUIRE_FALSE(IsReviveCorpseArg(nullptr));
    REQUIRE_FALSE(IsReviveCorpseArg(""));
    REQUIRE_FALSE(IsReviveCorpseArg("corpses"));     // trailing non-space
    REQUIRE_FALSE(IsReviveCorpseArg("corpse extra")); // trailing word
    REQUIRE_FALSE(IsReviveCorpseArg("corp"));         // partial
    REQUIRE_FALSE(IsReviveCorpseArg("PlayerName"));
}
