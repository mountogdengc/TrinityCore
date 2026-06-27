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

#include "BotFormationPolicy.h"

#include <cmath>

using BotFormationPolicy::Offset;
using BotFormationPolicy::Parse;

TEST_CASE("Column: all directly behind, increasing distance", "[BotFormationPolicy]")
{
    FormationOffset const a = Offset(BotFormation::Column, 0, 4);
    FormationOffset const b = Offset(BotFormation::Column, 1, 4);
    FormationOffset const c = Offset(BotFormation::Column, 2, 4);
    REQUIRE(a.angle == Approx(3.14159265f));   // behind
    REQUIRE(b.angle == Approx(3.14159265f));
    REQUIRE(a.distance < b.distance);
    REQUIRE(b.distance < c.distance);
}

TEST_CASE("Line: fixed depth, symmetric lateral spread", "[BotFormationPolicy]")
{
    FormationOffset const s0 = Offset(BotFormation::Line, 0, 4);
    FormationOffset const s3 = Offset(BotFormation::Line, 3, 4);
    REQUIRE(s0.distance == Approx(s3.distance));    // outermost pair is symmetric
    REQUIRE(s0.angle != Approx(s3.angle));          // opposite sides
    REQUIRE(std::abs(s0.angle) > 1.0f);             // behind-ish (not in front)
}

TEST_CASE("Wedge: distance grows per pair, alternating sides", "[BotFormationPolicy]")
{
    FormationOffset const s0 = Offset(BotFormation::Wedge, 0, 4);  // pair 1, one side
    FormationOffset const s1 = Offset(BotFormation::Wedge, 1, 4);  // pair 1, other side
    FormationOffset const s2 = Offset(BotFormation::Wedge, 2, 4);  // pair 2, deeper
    REQUIRE(s0.distance == Approx(s1.distance));    // same pair mirrors
    REQUIRE(s2.distance > s0.distance);             // deeper pair is farther
    REQUIRE(s0.angle != Approx(s1.angle));          // opposite sides
}

TEST_CASE("Circle: fixed radius, evenly spaced angles", "[BotFormationPolicy]")
{
    FormationOffset const s0 = Offset(BotFormation::Circle, 0, 4);
    FormationOffset const s1 = Offset(BotFormation::Circle, 1, 4);
    REQUIRE(s0.distance == Approx(s1.distance));
    REQUIRE((s1.angle - s0.angle) == Approx(2.0f * 3.14159265358979f / 4.0f));
}

TEST_CASE("Distinct slots give distinct offsets (line)", "[BotFormationPolicy]")
{
    REQUIRE(Offset(BotFormation::Line, 0, 4).angle != Offset(BotFormation::Line, 1, 4).angle);
}

TEST_CASE("Parse round-trips names, rejects junk", "[BotFormationPolicy]")
{
    REQUIRE(Parse("line").value()   == BotFormation::Line);
    REQUIRE(Parse("WEDGE").value()  == BotFormation::Wedge);
    REQUIRE(Parse("Circle").value() == BotFormation::Circle);
    REQUIRE(Parse("column").value() == BotFormation::Column);
    REQUIRE_FALSE(Parse("box").has_value());
    REQUIRE_FALSE(Parse("").has_value());
}
