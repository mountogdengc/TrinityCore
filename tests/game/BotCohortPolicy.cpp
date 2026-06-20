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

#include "BotCohortPolicy.h"

TEST_CASE("Bot cohort policy marks companions below the owner's level band", "[BotCohortPolicy]")
{
    REQUIRE(BotCohortPolicy::ComputeLevelBandState(20, 16, 2) == BotLevelBandState::Below);
    REQUIRE(BotCohortPolicy::ComputeLevelBandState(20, 19, 2) == BotLevelBandState::InRange);
    REQUIRE(BotCohortPolicy::ComputeLevelBandState(20, 24, 2) == BotLevelBandState::Above);
}

TEST_CASE("Bot cohort policy enables catch-up XP only below band", "[BotCohortPolicy]")
{
    BotCatchupState const below = BotCohortPolicy::ComputeCatchupState(20, 16, 2, 2.0f);
    BotCatchupState const inRange = BotCohortPolicy::ComputeCatchupState(20, 20, 2, 2.0f);

    REQUIRE(below.CatchupActive);
    REQUIRE(below.XpMultiplier == Approx(2.0f));
    REQUIRE(inRange.CatchupActive == false);
    REQUIRE(inRange.XpMultiplier == Approx(1.0f));
}

TEST_CASE("Bot cohort policy only auto-accepts continuity actions when appropriate", "[BotCohortPolicy]")
{
    REQUIRE(BotCohortPolicy::ShouldAutoSpawnCohort(true, true, true));
    REQUIRE_FALSE(BotCohortPolicy::ShouldAutoSpawnCohort(false, true, true));

    REQUIRE(BotCohortPolicy::ShouldAutoAcceptResurrection(true, true));
    REQUIRE_FALSE(BotCohortPolicy::ShouldAutoAcceptResurrection(false, true));

    REQUIRE(BotCohortPolicy::ShouldAutoAcceptSummon(true, true));
    REQUIRE_FALSE(BotCohortPolicy::ShouldAutoAcceptSummon(false, true));
}

TEST_CASE("Bot cohort policy blocks login auto-spawn for dead owners", "[BotCohortPolicy]")
{
    REQUIRE_FALSE(BotCohortPolicy::ShouldAutoSpawnCohort(true, false, true));
}
