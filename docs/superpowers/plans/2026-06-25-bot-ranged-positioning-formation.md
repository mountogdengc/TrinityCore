# Bot ranged positioning + formation spread Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ranged-class bots hold at casting range (never run into melee), and all bots fan out by a per-bot angle while following and fighting (no more stacking).

**Architecture:** A new pure `BotMovementPolicy` (class→ranged test + per-slot formation angles, unit-tested like the other `*Policy` files) decides role/angles; `BotMgr` assigns each bot a formation slot and feeds range/angle into `MoveChase`/`MoveFollow`, skipping melee auto-attack for ranged bots.

**Tech Stack:** C++ (TrinityCore master, `-DSCRIPTS=static`), Catch2 unit tests, Docker Compose build/run, in-game verification.

**Spec:** `docs/superpowers/specs/2026-06-25-bot-ranged-positioning-formation-design.md`

**Verification reality:** the `tests` target links the full `game` lib, so unit tests build ≈ a full server build; the catch2 tests are enforced by CI. The practical local gate for movement behavior is **in-game** (visual). The Docker server build skips tests (`-DSCRIPTS=static`, no `BUILD_TESTING`).

---

## File structure

- **Create** `src/server/scripts/Custom/Bots/BotMovementPolicy.{h,cpp}` — pure helpers: `IsRangedClass`, `FormationFollowAngle`, `FormationChaseAngle`.
- **Create** `tests/game/BotMovementPolicy.cpp`; **modify** `tests/CMakeLists.txt` (add the policy `.cpp` to the `tests` target sources).
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.h` — add `formationSlot` + `combatTarget` to `BotEntry`, and a `_nextFormationSlot` member.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.cpp` — slot assignment in `AddBot`; ranged/melee branch (range + angle, skip `Attack` for ranged) and follow angle in `UpdateFollow`; include the policy header + a `BOT_RANGED_DIST` constant.

---

## Task 1: Pure movement policy (TDD)

**Files:**
- Create: `src/server/scripts/Custom/Bots/BotMovementPolicy.h`
- Create: `src/server/scripts/Custom/Bots/BotMovementPolicy.cpp`
- Test: `tests/game/BotMovementPolicy.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the header**

Create `src/server/scripts/Custom/Bots/BotMovementPolicy.h`:

```cpp
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
```

- [ ] **Step 2: Write the failing test**

Create `tests/game/BotMovementPolicy.cpp`:

```cpp
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

    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_WARRIOR));
    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_ROGUE));
    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_DEATH_KNIGHT));
    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_DRUID));   // hybrid -> melee for now
    REQUIRE_FALSE(BotMovementPolicy::IsRangedClass(CLASS_SHAMAN));  // hybrid -> melee for now
}

TEST_CASE("Formation angles are deterministic and distinct for adjacent slots", "[BotMovementPolicy]")
{
    REQUIRE(BotMovementPolicy::FormationFollowAngle(2) == BotMovementPolicy::FormationFollowAngle(2));
    REQUIRE(BotMovementPolicy::FormationChaseAngle(3) == BotMovementPolicy::FormationChaseAngle(3));

    REQUIRE(BotMovementPolicy::FormationFollowAngle(0) != BotMovementPolicy::FormationFollowAngle(1));
    REQUIRE(BotMovementPolicy::FormationFollowAngle(1) != BotMovementPolicy::FormationFollowAngle(2));
    REQUIRE(BotMovementPolicy::FormationChaseAngle(0) != BotMovementPolicy::FormationChaseAngle(1));
    REQUIRE(BotMovementPolicy::FormationChaseAngle(1) != BotMovementPolicy::FormationChaseAngle(2));
}
```

- [ ] **Step 3: Register the policy source with the tests target**

In `tests/CMakeLists.txt`, add the new file to the `target_sources(tests PRIVATE ...)` block so it reads:

```cmake
target_sources(tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotCombatPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotCohortPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotDeathPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotMovementPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotRotationPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/EasternKingdoms/tirisfal_recruitment.cpp)
```

Read the file first to confirm the current list (it already includes `BotRotationPolicy.cpp` from earlier work) and insert `BotMovementPolicy.cpp` alphabetically, preserving the rest.

- [ ] **Step 4: Write the implementation**

Create `src/server/scripts/Custom/Bots/BotMovementPolicy.cpp`:

```cpp
/*
 * Player-bot movement policy helpers -- see BotMovementPolicy.h.
 */

#include "BotMovementPolicy.h"
#include "SharedDefines.h"

namespace
{
    constexpr float BOT_PI = 3.14159265358979f;
}

namespace BotMovementPolicy
{
bool IsRangedClass(uint8 cls)
{
    switch (cls)
    {
        case CLASS_HUNTER:
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return true;
        default:
            return false;
    }
}

float FormationFollowAngle(uint32 slot)
{
    // Symmetric fan behind the master: PI, PI+step, PI-step, PI+2*step, PI-2*step, ...
    uint32 const pair = (slot + 1) / 2;
    float const sign = (slot % 2 == 1) ? 1.0f : -1.0f;
    return BOT_PI + sign * float(pair) * 0.45f;
}

float FormationChaseAngle(uint32 slot)
{
    // Spread evenly around the target: 6 distinct directions, 60 deg apart.
    return float(slot % 6) * (2.0f * BOT_PI / 6.0f);
}
}
```

- [ ] **Step 5: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMovementPolicy.h \
        src/server/scripts/Custom/Bots/BotMovementPolicy.cpp \
        tests/game/BotMovementPolicy.cpp tests/CMakeLists.txt
git commit -m "feat(bots): pure movement policy -- ranged class + formation angles

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

(Do not run a Docker build for this task; the `tests` target builds with the full game lib in CI.)

---

## Task 2: BotEntry formation slot + combat-target tracking

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.h`
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp` (slot assignment in `AddBot`)

- [ ] **Step 1: Extend `BotEntry` and add the slot counter**

In `BotMgr.h`, the `BotEntry` struct (around line 58-65) currently ends with `staleCombatTimer`. Add two fields:

```cpp
    struct BotEntry
    {
        WorldSession* session;
        ObjectGuid    master;                // empty => idle (M1 behaviour: hold position)
        uint32        holdTimer = 0;         // M3: ms left to linger after a fight before re-following
        uint32        deadTimer = 0;         // M4: ms the bot has been dead (drives auto-revive)
        uint32        staleCombatTimer = 0;  // M4: ms the current victim has looked invalid (kept until BOT_STALE_COMBAT_MS)
        uint8         formationSlot = 0;     // per-bot index -> distinct follow/chase angle (anti-stacking)
        ObjectGuid    combatTarget;          // current chase target; drives re-issue of MoveChase on a switch
    };
```

Then add a private counter member next to `_followTimer` (around line 82):

```cpp
    uint32 _followTimer = 0;
    uint8  _nextFormationSlot = 0;   // hands out BotEntry::formationSlot on AddBot
```

- [ ] **Step 2: Assign the slot in `AddBot`**

In `BotMgr.cpp` `AddBot`, the line that inserts the entry is currently:

```cpp
    _bots[key] = BotEntry{ session, master };
```

Replace it with (assign a distinct slot per added bot):

```cpp
    _bots[key] = BotEntry{ session, master };
    _bots[key].formationSlot = _nextFormationSlot++;
```

(`BotEntry{ session, master }` aggregate-inits `session`/`master`; the new `formationSlot`/`combatTarget` take their defaults, then we set the slot.)

- [ ] **Step 3: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.h src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): per-bot formation slot + combat-target tracking on BotEntry

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Ranged/melee positioning + formation angles in BotMgr

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`

- [ ] **Step 1: Include the policy header and add the ranged distance constant**

In `BotMgr.cpp`, add the include alongside the other bot includes (after `#include "BotDeathPolicy.h"`):

```cpp
#include "BotMovementPolicy.h"
```

In the anonymous-namespace constants block (the one with `BOT_FOLLOW_DIST`, around line 37-42), add:

```cpp
    constexpr float  BOT_RANGED_DIST        = 25.0f; // ranged bots hold at this range and cast (no melee)
```

- [ ] **Step 2: Replace the combat chase block with ranged/melee branch + angle**

In `UpdateFollow`, the combat block currently reads:

```cpp
            MotionMaster* mm = bot->GetMotionMaster();
            if (bot->GetVictim() != target)
            {
                bot->Attack(target, true);
                mm->MoveChase(target);
            }
            else if (mm->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
                mm->MoveChase(target);
```

Replace it with:

```cpp
            MotionMaster* mm = bot->GetMotionMaster();
            ChaseAngle const chaseAngle(BotMovementPolicy::FormationChaseAngle(entry.formationSlot));
            if (BotMovementPolicy::IsRangedClass(bot->GetClass()))
            {
                // Ranged: hold at casting range and never melee -- the rotation does the
                // damage. Re-issue the chase on a target switch or if the generator was
                // dropped (teleport / idle-follow). No Attack() => no running to contact.
                if (entry.combatTarget != target->GetGUID() || mm->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
                {
                    bot->AttackStop();   // defensive: ensure no stale melee auto-attack
                    mm->MoveChase(target, ChaseRange(BOT_RANGED_DIST), chaseAngle);
                    entry.combatTarget = target->GetGUID();
                }
            }
            else
            {
                // Melee: close to contact and auto-attack, fanned by formation angle.
                if (bot->GetVictim() != target)
                {
                    bot->Attack(target, true);
                    mm->MoveChase(target, {}, chaseAngle);
                    entry.combatTarget = target->GetGUID();
                }
                else if (mm->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
                    mm->MoveChase(target, {}, chaseAngle);
            }
```

(`MoveChase(target, {}, chaseAngle)` keeps the default melee range but applies the angle. `ChaseRange`/`ChaseAngle` come from `MovementDefines.h`, already included.)

- [ ] **Step 3: Reset combat target + add follow angle in the non-combat path**

In `UpdateFollow`, the non-combat/follow path currently ends with:

```cpp
        // isn't a follow (initial, post-teleport, post-combat).
        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
            bot->GetMotionMaster()->MoveFollow(master, BOT_FOLLOW_DIST);
```

Replace those two lines with (clear the stored combat target so re-entering combat re-issues the chase, and fan the follow by formation angle):

```cpp
        // isn't a follow (initial, post-teleport, post-combat).
        entry.combatTarget = ObjectGuid::Empty;
        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
            bot->GetMotionMaster()->MoveFollow(master, BOT_FOLLOW_DIST,
                ChaseAngle(BotMovementPolicy::FormationFollowAngle(entry.formationSlot)));
```

Note: the `UpdateFollow` loop must iterate by reference (`for (auto& [name, entry] : _bots)`) so `entry.combatTarget` writes persist — it already does. If the combat block sits inside a path that `continue`s before reaching here, the reset only runs on the non-combat path, which is correct.

- [ ] **Step 4: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): ranged bots hold at range; fan formation for follow + combat

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Build, deploy (with merged PRs), verify in-game

**Files:** none (build/run only)

- [ ] **Step 1: Full rebuild from current branch**

This branch is based on current `main`, so the build also includes the merged crafted-gear (#26), weapon-skills (#27), and AHBot (#28) work.

```bash
cd /e/TrinityCore
docker images trinitycore:local --format 'pre-build: {{.ID}}'
docker compose --progress plain build > build-botmove.log 2>&1; echo "BUILD_EXIT=$?" >> build-botmove.log
```

- [ ] **Step 2: Verify the build compiled**

```bash
grep BUILD_EXIT build-botmove.log                         # expect BUILD_EXIT=0
grep -niE "error:|failed to solve|lease does not exist" build-botmove.log | head  # expect empty
docker images trinitycore:local --format '{{.ID}} {{.CreatedAt}}'   # expect a NEW id
```
If `BUILD_EXIT` is non-zero or the image id is unchanged, fix the error and re-run before proceeding. (If `lease does not exist` appears, run `docker builder prune -f` then rebuild.)

- [ ] **Step 3: Deploy**

```bash
docker compose up -d
# wait for readiness (startup can be slow on this disk):
docker logs tc-worldserver 2>&1 | grep -iE "World initialized|ready\.\.\." | tail -2
```
Expect `ready...`. Confirm the running container is on the new image: `docker inspect tc-worldserver --format '{{.Image}}'`.

- [ ] **Step 4: In-game verification (the integration gate)**

As the GM master: `.bot add Zebgoro` (Hunter), `.bot add Followerone` (Priest), `.bot add Lithilia` (Warrior). Follow around, then pull a mob.
- **Ranged (Hunter/Priest):** stay back at ~25 yd and cast — do **not** run into melee; far fewer deaths.
- **Melee (Warrior):** still closes and melees.
- **Following + combat:** the three bots **fan out** (distinct positions) instead of stacking; each is individually clickable.

If a ranged bot still runs to melee, confirm `IsRangedClass` matches its class and that the ranged branch issued `MoveChase(..., ChaseRange(25), ...)`. If bots still stack, confirm distinct `formationSlot`s were assigned.

- [ ] **Step 5: Clean up build log + commit nothing (build artifacts only)**

```bash
rm -f build-botmove.log
```

---

## Task 5: PR + merge

- [ ] **Step 1: Push and open PR**

```bash
git push -u origin claude/bot-ranged-positioning
gh pr create --repo mountogdengc/TrinityCore --base main --head claude/bot-ranged-positioning \
  --title "feat(bots): ranged hold-at-range positioning + formation spread" --body "$(cat <<'EOF'
Ranged-class bots (Hunter/Priest/Mage/Warlock) now hold at ~25 yd and fight via the rotation instead of running into melee (the main cause of bot deaths), and all bots fan out by a per-bot formation angle so they stop stacking (and are individually targetable).

- New pure `BotMovementPolicy` (`IsRangedClass`, `FormationFollowAngle`, `FormationChaseAngle`) + unit tests.
- `BotMgr`: per-bot formation slot; ranged branch (`MoveChase` at `ChaseRange(25)`, no melee `Attack`) vs melee branch (close + auto-attack), both fanned by angle; follow uses a per-slot angle.

Out of scope (see `docs/bot-ux-future-milestones.md`): named formation presets + UI, ranged auto-attack/wand, kiting, Hunter pets, bot gear.

Spec: `docs/superpowers/specs/2026-06-25-bot-ranged-positioning-formation-design.md`

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 2: Merge (per user) and sync**

```bash
gh pr merge <PR#> --repo mountogdengc/TrinityCore --merge --delete-branch
git checkout main && git pull --ff-only origin main
```

---

## Notes / guardrails

- `ChaseRange(25.f)` → `MinRange=0` (never "too close" → no kiting), `MaxRange≈25.5` (move in only if farther): the bot approaches to ~25 yd and holds. Matches scope (no kiting).
- Keep the `entry.combatTarget` reset on the non-combat path so re-entering combat re-issues the chase.
- The `docker/worldserver/entrypoint.sh` `Logger.bots` toggle remains uncommitted in the working tree; don't stage it.
