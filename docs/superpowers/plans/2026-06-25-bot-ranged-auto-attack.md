# Bot ranged auto-attack Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A ranged bot in combat keeps its ranged auto-attack running (Hunter Auto Shot; caster wand Shoot when a wand is equipped), dealing steady damage alongside the rotation instead of standing idle between casts.

**Architecture:** A new pure `BotRangedAttackPolicy` decides when to (re)start the autorepeat; `BotMgr` scans the bot's known spells once for its autorepeat ranged spell (no hardcoded ids), caches it on `BotEntry`, and starts/stops the autorepeat in the ranged branch of `UpdateFollow`. Auto-attack runs on the independent `RANGED_ATTACK` timer, so it coexists with the rotation and the existing `Attack(target, false)` chase-keeper.

**Tech Stack:** C++ (TrinityCore master), Catch2 unit tests, Docker Compose build/run, in-game verification.

**Spec:** `docs/superpowers/specs/2026-06-25-bot-ranged-auto-attack-design.md`

**Verification reality:** the `tests` target links the full `game` lib, so the catch2 tests build ≈ a full server build and are enforced by CI. The practical local gate for the behavior is **in-game**. The Docker server build skips tests.

---

## File structure

- **Create** `src/server/scripts/Custom/Bots/BotRangedAttackPolicy.{h,cpp}` — pure `ShouldStartAutoRepeat`.
- **Create** `tests/game/BotRangedAttackPolicy.cpp`; **modify** `tests/CMakeLists.txt`.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.h` — `rangedAutoSpellId` + `rangedAutoChecked` on `BotEntry`; declare `FindRangedAutoAttackSpell`.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.cpp` — implement `FindRangedAutoAttackSpell`; start/stop wiring in `UpdateFollow`; includes.

---

## Task 1: Pure ranged-attack policy (TDD)

**Files:**
- Create: `src/server/scripts/Custom/Bots/BotRangedAttackPolicy.h`
- Create: `src/server/scripts/Custom/Bots/BotRangedAttackPolicy.cpp`
- Test: `tests/game/BotRangedAttackPolicy.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the header**

Create `src/server/scripts/Custom/Bots/BotRangedAttackPolicy.h`:

```cpp
/*
 * Player-bot ranged auto-attack policy (pure, unit-tested).
 */

#ifndef TRINITYCORE_BOTS_BOTRANGEDATTACKPOLICY_H
#define TRINITYCORE_BOTS_BOTRANGEDATTACKPOLICY_H

#include "Define.h"

namespace BotRangedAttackPolicy
{
    // (Re)start the ranged autorepeat (Auto Shot / wand Shoot) when the bot has an
    // auto-attack spell AND is either not currently repeating or just switched targets.
    bool ShouldStartAutoRepeat(bool hasAutoSpell, bool alreadyRepeating, bool targetChanged);
}

#endif // TRINITYCORE_BOTS_BOTRANGEDATTACKPOLICY_H
```

- [ ] **Step 2: Write the failing test**

Create `tests/game/BotRangedAttackPolicy.cpp` — first copy the exact GPL license header block from an existing file in `tests/game/` (e.g. `tests/game/BotMovementPolicy.cpp`), then:

```cpp
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
```

- [ ] **Step 3: Register the test source with the tests target**

In `tests/CMakeLists.txt`, add `BotRangedAttackPolicy.cpp` to the `target_sources(tests PRIVATE ...)` block alongside the other `src/server/scripts/Custom/Bots/*Policy.cpp` entries, in alphabetical position (it sorts after `BotMovementPolicy.cpp`, before `BotRotationPolicy.cpp`). Read the file first to match the exact `${CMAKE_SOURCE_DIR}/...` path style; do not reorder other entries. The added line:

```cmake
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotRangedAttackPolicy.cpp
```

- [ ] **Step 4: Write the implementation**

Create `src/server/scripts/Custom/Bots/BotRangedAttackPolicy.cpp`:

```cpp
/*
 * Player-bot ranged auto-attack policy -- see BotRangedAttackPolicy.h.
 */

#include "BotRangedAttackPolicy.h"

namespace BotRangedAttackPolicy
{
bool ShouldStartAutoRepeat(bool hasAutoSpell, bool alreadyRepeating, bool targetChanged)
{
    return hasAutoSpell && (!alreadyRepeating || targetChanged);
}
}
```

- [ ] **Step 5: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotRangedAttackPolicy.h \
        src/server/scripts/Custom/Bots/BotRangedAttackPolicy.cpp \
        tests/game/BotRangedAttackPolicy.cpp tests/CMakeLists.txt
git commit -m "feat(bots): pure ranged auto-attack policy (when to start autorepeat)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

(Do not run a Docker build; the `tests` target is verified in CI.)

---

## Task 2: BotEntry cache + spell-scan helper

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.h`
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`

- [ ] **Step 1: Add cache fields to `BotEntry` and declare the helper**

In `BotMgr.h`, the `BotEntry` struct currently ends with `combatTarget`. Add two fields:

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
        uint32        rangedAutoSpellId = 0; // cached autorepeat ranged spell (Auto Shot / wand Shoot); 0 = none
        bool          rangedAutoChecked = false; // true once we've scanned this bot's spells for the above
    };
```

Then add a private method declaration near the other private helpers (e.g. next to `SelectAssistTarget` / `EnsureGrouped` declarations):

```cpp
    // Find this bot's autorepeat ranged spell (Auto Shot for a Hunter, wand Shoot for a
    // wand-equipped caster), or 0 if it has no usable ranged weapon / no such spell.
    uint32 FindRangedAutoAttackSpell(Player* bot);
```

- [ ] **Step 2: Add the includes needed by the helper**

In `BotMgr.cpp`, ensure these engine headers are included (add any that are missing, in the existing include order/style): `"SpellMgr.h"`, `"SpellInfo.h"`, `"Item.h"`. (`Player.h`, `Map.h`, `SharedDefines.h`, `Unit.h` are already pulled in transitively or directly — verify `RANGED_ATTACK`, `PLAYERSPELL_REMOVED`, `CURRENT_AUTOREPEAT_SPELL`, and `sSpellMgr` all resolve; add the owning header only if one does not.)

- [ ] **Step 3: Implement `FindRangedAutoAttackSpell`**

Add this method to `BotMgr.cpp` (e.g. just above `BotMgr::SelectAssistTarget`):

```cpp
uint32 BotMgr::FindRangedAutoAttackSpell(Player* bot)
{
    // No usable bow/gun/wand in the ranged slot -> nothing to auto-attack with. This is
    // exactly the caster-without-a-wand no-op (we never hand out wands).
    if (!bot->GetWeaponForAttack(RANGED_ATTACK, /*useable*/ true))
        return 0;

    Difficulty const difficulty = bot->GetMap()->GetDifficultyID();
    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (playerSpell.state == PLAYERSPELL_REMOVED || !playerSpell.active || playerSpell.disabled)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, difficulty);
        if (spellInfo && spellInfo->IsAutoRepeatRangedSpell())
            return spellId;   // Auto Shot (Hunter) / Shoot (wand caster)
    }
    return 0;
}
```

- [ ] **Step 4: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.h src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): scan + cache a bot's autorepeat ranged spell (Auto Shot / wand Shoot)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Wire start/stop into the combat loop

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`

- [ ] **Step 1: Include the policy header**

In `BotMgr.cpp`, add alongside the other `Bot*.h` includes (alphabetical: after `BotMovementPolicy.h`, before `BotRotation.h`):

```cpp
#include "BotRangedAttackPolicy.h"
```

- [ ] **Step 2: Capture `targetChanged` and start the autorepeat in the ranged branch**

In `UpdateFollow`, the ranged branch currently reads:

```cpp
            if (BotMovementPolicy::IsRangedClass(bot->GetClass()))
            {
                // Ranged: hold at casting range; the rotation does the damage. Attack(target,
                // false) sets the victim WITHOUT starting melee swings -- the chase generator
                // halts unless GetVictim()==target (ChaseMovementGenerator::HasLostTarget), and
                // ChaseRange(BOT_RANGED_DIST) -- not Attack() -- is what stops the bot closing to
                // melee. Re-issue on a target switch or if the generator was dropped.
                if (entry.combatTarget != target->GetGUID() || mm->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
                {
                    bot->Attack(target, false);   // set m_attacking (chase needs it); false => no melee swings
                    mm->MoveChase(target, ChaseRange(BOT_RANGED_DIST), chaseAngle);
                    entry.combatTarget = target->GetGUID();
                }
            }
```

Replace it with (capture `targetChanged` before `combatTarget` is updated, then start/refresh the autorepeat):

```cpp
            if (BotMovementPolicy::IsRangedClass(bot->GetClass()))
            {
                // Ranged: hold at casting range; the rotation does the damage. Attack(target,
                // false) sets the victim WITHOUT starting melee swings -- the chase generator
                // halts unless GetVictim()==target (ChaseMovementGenerator::HasLostTarget), and
                // ChaseRange(BOT_RANGED_DIST) -- not Attack() -- is what stops the bot closing to
                // melee. Re-issue on a target switch or if the generator was dropped.
                bool const targetChanged = entry.combatTarget != target->GetGUID();
                if (targetChanged || mm->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
                {
                    bot->Attack(target, false);   // set m_attacking (chase needs it); false => no melee swings
                    mm->MoveChase(target, ChaseRange(BOT_RANGED_DIST), chaseAngle);
                    entry.combatTarget = target->GetGUID();
                }

                // Ranged auto-attack: keep Auto Shot / wand Shoot running alongside the
                // rotation (it loops itself on the RANGED_ATTACK timer once started). Scan
                // once for this bot's autorepeat spell and cache it. Start it when not
                // already repeating, or re-point it after a target switch.
                if (!entry.rangedAutoChecked)
                {
                    entry.rangedAutoSpellId = FindRangedAutoAttackSpell(bot);
                    entry.rangedAutoChecked = true;
                }
                bool const repeating = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) != nullptr;
                if (BotRangedAttackPolicy::ShouldStartAutoRepeat(entry.rangedAutoSpellId != 0, repeating, targetChanged))
                    bot->CastSpell(target, entry.rangedAutoSpellId, false);   // untriggered => routes into the autorepeat slot
            }
```

(The melee `else` branch and the rotation/`CastSpell` block below are unchanged.)

- [ ] **Step 3: Stop the autorepeat on disengage**

In `UpdateFollow`, the "nothing left to fight" block currently reads:

```cpp
        // Nothing left to fight (victim dead / invalid / out of leash, and the
        // master isn't engaged): drop the now-stale victim and fall through to
        // follow. The post-combat hold below keeps us from snapping back instantly.
        if (bot->GetVictim())
            bot->AttackStop();
```

Replace it with (also stop any ranged autorepeat so the bot doesn't keep shooting after combat):

```cpp
        // Nothing left to fight (victim dead / invalid / out of leash, and the
        // master isn't engaged): drop the now-stale victim and fall through to
        // follow. The post-combat hold below keeps us from snapping back instantly.
        if (bot->GetVictim())
            bot->AttackStop();
        bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);   // stop Auto Shot / wand Shoot when disengaging
```

- [ ] **Step 4: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): ranged bots weave Auto Shot / wand Shoot between casts

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Changelog + build + in-game verify

**Files:**
- Modify: `CHANGELOG-custom.md`

- [ ] **Step 1: Update the changelog**

In `CHANGELOG-custom.md`, under the existing `## Player-bot positioning — ranged hold-at-range + formation spread` section, append a short paragraph (in the same style as the surrounding entries):

```markdown
**Ranged auto-attack between casts.** Ranged bots now keep their ranged auto-attack
running while in combat — Hunters fire **Auto Shot**, casters fire wand **Shoot** when
a wand is equipped (no wand → silent no-op; we don't hand out wands). `BotMgr` scans the
bot's known spells once for its autorepeat ranged spell (no hardcoded ids — there are
~12 Auto Shot variants) and starts it in the ranged combat branch; it loops itself on
the `RANGED_ATTACK` timer and stops on disengage. Pure trigger logic in
`BotRangedAttackPolicy::ShouldStartAutoRepeat`. Spec/plan:
`docs/superpowers/specs/2026-06-25-bot-ranged-auto-attack-design.md`,
`docs/superpowers/plans/2026-06-25-bot-ranged-auto-attack.md`.
```

Commit:

```bash
git add CHANGELOG-custom.md
git commit -m "docs(bots): changelog entry for ranged auto-attack

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

- [ ] **Step 2: Full rebuild**

```bash
cd /e/TrinityCore
docker images trinitycore:local --format 'pre-build: {{.ID}}'
docker compose --progress plain build > build-autoattack.log 2>&1; echo "BUILD_EXIT=$?" >> build-autoattack.log
```

- [ ] **Step 3: Verify the build compiled**

```bash
grep BUILD_EXIT build-autoattack.log                          # expect BUILD_EXIT=0
grep -niE "error:|failed to solve|lease does not exist" build-autoattack.log | head   # expect empty
docker images trinitycore:local --format '{{.ID}} {{.CreatedAt}}'   # expect a NEW id
```
If `BUILD_EXIT` is non-zero or the image id is unchanged, fix the error and rebuild. (The link will fail with an undefined reference to `BotRangedAttackPolicy::ShouldStartAutoRepeat` or `BotMgr::FindRangedAutoAttackSpell` if a file was missed — that itself confirms the new code must be present to link.)

- [ ] **Step 4: Deploy**

```bash
docker compose up -d
docker logs tc-worldserver 2>&1 | grep -iE "World initialized|ready\.\.\." | tail -2
```

- [ ] **Step 5: In-game verification (the integration gate)**

As the GM master, bring a Hunter and a caster bot and pull a mob:
- **Hunter** visibly fires **Auto Shot** repeatedly between/over its abilities (steady ranged hits in the combat log).
- A **caster with a wand equipped** fires wand Shoot; a **caster with no wand** does nothing extra (no errors, no log spam).
- The bot **stops** auto-attacking when the fight ends / it disengages and returns to following.
- Auto-attack **re-points** correctly when the master switches the bot to a new target.

If a Hunter doesn't auto-shoot, confirm `FindRangedAutoAttackSpell` returned non-zero (it has a ranged weapon + a known Auto Shot variant) and that the start cast populated `CURRENT_AUTOREPEAT_SPELL` (if not, the cast flags need adjusting — try the minimal trigger flags that still set the autorepeat slot).

- [ ] **Step 6: Clean up the build log**

```bash
rm -f build-autoattack.log
```

---

## Task 5: PR + merge

- [ ] **Step 1: Push and open PR**

```bash
git push -u origin claude/bot-ranged-auto-attack
gh pr create --repo mountogdengc/TrinityCore --base main --head claude/bot-ranged-auto-attack \
  --title "feat(bots): ranged bots weave Auto Shot / wand Shoot between casts" --body "$(cat <<'EOF'
Ranged bots now keep their ranged auto-attack running in combat instead of standing idle between rotation casts: Hunters fire Auto Shot, casters fire wand Shoot when a wand is equipped (no wand → silent no-op).

- New pure `BotRangedAttackPolicy::ShouldStartAutoRepeat` + unit tests.
- `BotMgr` scans the bot's known spells once for its autorepeat ranged spell (no hardcoded ids — there are ~12 Auto Shot variants), caches it, and starts it in the ranged combat branch; the engine loops it on the RANGED_ATTACK timer; stops on disengage.

Coexists with the rotation and the `Attack(target, false)` chase-keeper (separate timer). Verified in-game.

Out of scope (later): kiting, auto-granting wands, Hunter pets (next feature).

Spec: `docs/superpowers/specs/2026-06-25-bot-ranged-auto-attack-design.md`

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

- `CastSpell(target, spellId, false)` is untriggered so the engine routes the autorepeat into `CURRENT_AUTOREPEAT_SPELL`. If in-game testing shows the autorepeat slot doesn't populate, switch to the minimal trigger flags that preserve autorepeat routing (verify `GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)` becomes non-null).
- `targetChanged` MUST be captured before the re-issue block updates `entry.combatTarget`, or the auto-attack will never see a switch.
- The cache (`rangedAutoChecked`) means a caster who equips a wand mid-session won't wand-weave until re-added — acceptable per spec.
