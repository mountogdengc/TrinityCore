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

#include "BotCombatPolicy.h"

TEST_CASE("Bot combat policy assists the selected target only once the master is in combat", "[BotCombatPolicy]")
{
    // In combat, no active victim, selected target eligible -> assist.
    REQUIRE(BotCombatPolicy::ShouldUseMasterSelectedTarget(false, true, true));
    // Merely selecting a hostile out of combat must NOT trigger an attack.
    REQUIRE_FALSE(BotCombatPolicy::ShouldUseMasterSelectedTarget(false, false, true));
    // An active master victim takes precedence over the selected target.
    REQUIRE_FALSE(BotCombatPolicy::ShouldUseMasterSelectedTarget(true, true, true));
}

TEST_CASE("Bot combat policy keeps a valid current victim during target gaps", "[BotCombatPolicy]")
{
    REQUIRE(BotCombatPolicy::ShouldKeepCurrentVictim(false, true, true, true, 0, 1500));
}
