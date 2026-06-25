# Condition-aware bot rotation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the M4 bot rotation engine skip a candidate spell when a fork-defined condition says it's pointless — chiefly, don't re-cast a DoT that's still active; refresh it just before it expires — fixing the low-level Priest re-casting Shadow Word: Pain every GCD instead of using Smite as filler.

**Architecture:** Pure decision logic goes in a new `BotRotationPolicy.{h,cpp}` (no engine deps, unit-tested in CI like the other `*Policy` files). `BotRotation.cpp` reads `sAssistedCombatRuleStore`, looks up the live aura state, and calls the policy. Conditions are authored as `assisted_combat_rule` hotfix rows and interpreted as fork opcodes **only for our custom steps** (ID ≥ 1000000); Blizzard's undocumented opcodes stay fail-open.

**Tech Stack:** C++ (TrinityCore `master`, `-DSCRIPTS=static`), Catch2 unit tests (`tests/`), MySQL hotfix migration (auto-applied by the worldserver updater), Docker Compose build/run.

**Spec:** `docs/superpowers/specs/2026-06-24-condition-aware-bot-rotation-design.md`

**Verification reality:** The `tests` target links the full `game` lib, so building it locally ≈ a full server build; the catch2 test is enforced by CI (`.github/workflows/linux-build.yml`). The practical local gate for engine + data behavior is **in-game**, read from the `bots` DEBUG log (the `Logger.bots` Debug toggle in `docker/worldserver/entrypoint.sh` is already on).

---

## File structure

- **Create** `src/server/scripts/Custom/Bots/BotRotationPolicy.h` — fork opcode enum + pure decision declaration.
- **Create** `src/server/scripts/Custom/Bots/BotRotationPolicy.cpp` — pure decision implementation.
- **Create** `tests/game/BotRotationPolicy.cpp` — Catch2 unit tests for the pure decision.
- **Modify** `tests/CMakeLists.txt` — add `BotRotationPolicy.cpp` to the `tests` target sources.
- **Modify** `src/server/scripts/Custom/Bots/BotRotation.cpp` — build a custom-step rule index, evaluate conditions in `SelectSpell`.
- **Create** `sql/updates/hotfixes/master/2026_06_24_00_hotfixes.sql` — Priest SWP rule + `hotfix_data`.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.cpp` — remove the temporary `[CAST-DIAG]` logging (keep the `TRIGGERED_IGNORE_TARGET_CHECK` cast).
- **Modify** `CHANGELOG-custom.md` — document the condition layer + migration + the (already-applied) cast fix.

---

## Task 1: Pure condition-decision policy (TDD)

**Files:**
- Create: `src/server/scripts/Custom/Bots/BotRotationPolicy.h`
- Create: `src/server/scripts/Custom/Bots/BotRotationPolicy.cpp`
- Test: `tests/game/BotRotationPolicy.cpp`
- Modify: `tests/CMakeLists.txt:24` (append to `target_sources`)

- [ ] **Step 1: Write the policy header**

Create `src/server/scripts/Custom/Bots/BotRotationPolicy.h`:

```cpp
/*
 * Player-bot rotation policy helpers (pure, unit-tested).
 */

#ifndef TRINITYCORE_BOTS_BOTROTATIONPOLICY_H
#define TRINITYCORE_BOTS_BOTROTATIONPOLICY_H

#include "Define.h"

namespace BotRotationPolicy
{
    // Fork condition opcodes. Interpreted by BotRotation ONLY for custom Assisted
    // Combat steps (ID >= 1000000); Blizzard's opcodes on stock steps are ignored
    // (fail-open). Values sit well outside Blizzard's small opcode range.
    enum BotConditionType : int32
    {
        BOT_COND_TARGET_MISSING_OR_EXPIRING_MY_AURA = 1000,
    };

    // True when a DoT-style candidate should still be cast: the aura is absent, or
    // its remaining duration is below the refresh window. A permanent aura
    // (remainingMs < 0) that is present returns false (never re-cast). refreshMs == 0
    // collapses to "cast only when absent".
    bool ShouldCastForMissingOrExpiringAura(bool botAuraPresent, int32 remainingMs, int32 refreshMs);
}

#endif // TRINITYCORE_BOTS_BOTROTATIONPOLICY_H
```

- [ ] **Step 2: Write the failing test**

Create `tests/game/BotRotationPolicy.cpp`:

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
```

- [ ] **Step 3: Register the policy source with the tests target**

In `tests/CMakeLists.txt`, change the `target_sources(tests PRIVATE ...)` block (lines 19-24) to add the new file:

```cmake
target_sources(tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotCombatPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotCohortPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotDeathPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotRotationPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/EasternKingdoms/tirisfal_recruitment.cpp)
```

(The test file `tests/game/BotRotationPolicy.cpp` is picked up automatically by `CollectAndAddSourceFiles`.)

- [ ] **Step 4: Confirm it would fail (no implementation yet)**

The implementation `.cpp` does not exist yet, so a `-DBUILD_TESTING=1` build links with an undefined-reference to `BotRotationPolicy::ShouldCastForMissingOrExpiringAura`. (CI builds with `BUILD_TESTING`; locally this links the full `game` lib, so don't build it just for this — Step 6 implements it before any build happens.)

- [ ] **Step 5: Write the implementation**

Create `src/server/scripts/Custom/Bots/BotRotationPolicy.cpp`:

```cpp
/*
 * Player-bot rotation policy helpers -- see BotRotationPolicy.h.
 */

#include "BotRotationPolicy.h"

namespace BotRotationPolicy
{
bool ShouldCastForMissingOrExpiringAura(bool botAuraPresent, int32 remainingMs, int32 refreshMs)
{
    if (!botAuraPresent)
        return true;
    return remainingMs >= 0 && remainingMs < refreshMs;
}
}
```

- [ ] **Step 6: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotRotationPolicy.h \
        src/server/scripts/Custom/Bots/BotRotationPolicy.cpp \
        tests/game/BotRotationPolicy.cpp tests/CMakeLists.txt
git commit -m "feat(bots): pure refresh-aware DoT condition policy + unit tests"
```

---

## Task 2: Evaluate conditions in the rotation engine

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotRotation.cpp`

- [ ] **Step 1: Add includes**

In `BotRotation.cpp`, add to the include block (after `#include "BotRotation.h"`):

```cpp
#include "BotRotationPolicy.h"
#include "SpellAuras.h"
```

- [ ] **Step 2: Add a rule-index + RuleData to the anonymous namespace**

In the anonymous `namespace { ... }`, change the priority map to carry the step id and add a custom-step rule index. Replace the declaration:

```cpp
    std::unordered_map<int32, std::vector<uint32>> g_specPriority;
    bool g_indexBuilt = false;
```

with:

```cpp
    struct RuleData { int32 conditionType; int32 value1; int32 value2; int32 value3; };

    // spec -> ordered [(spellId, stepId)]; stepId lets a candidate find its rules.
    std::unordered_map<int32, std::vector<std::pair<uint32, int32>>> g_specPriority;
    // custom stepId (>= 1000000) -> fork rules gating that step.
    std::unordered_map<int32, std::vector<RuleData>> g_stepRules;
    bool g_indexBuilt = false;

    // Custom Assisted Combat rows (our hotfix data) use IDs >= this base.
    constexpr int32 BOT_CUSTOM_ID_BASE = 1000000;
```

- [ ] **Step 3: Build the rule index and carry stepId in EnsureIndex**

In `EnsureIndex()`, change the `specSteps` element type to include the step id and populate `g_stepRules`. Replace the step-collection + sort/dedup block so it reads:

```cpp
        // spec -> [(order, spellId, stepId)]
        std::unordered_map<int32, std::vector<std::tuple<int32, uint32, int32>>> specSteps;
        for (AssistedCombatStepEntry const* step : sAssistedCombatStepStore)
        {
            if (step->SpellID <= 0)
                continue;

            auto specItr = containerSpec.find(uint32(step->AssistedCombatID));
            if (specItr == containerSpec.end())
                continue;

            specSteps[specItr->second].emplace_back(step->OrderIndex, uint32(step->SpellID), int32(step->ID));
        }

        // Fork condition rules, indexed by step -- only for our custom steps.
        for (AssistedCombatRuleEntry const* rule : sAssistedCombatRuleStore)
        {
            if (rule->AssistedCombatStepID < BOT_CUSTOM_ID_BASE)
                continue;   // Blizzard step -> fail-open (no fork interpretation).
            g_stepRules[rule->AssistedCombatStepID].push_back(
                { rule->ConditionType, rule->ConditionValue1, rule->ConditionValue2, rule->ConditionValue3 });
        }

        for (auto& [specId, steps] : specSteps)
        {
            std::stable_sort(steps.begin(), steps.end(),
                [](std::tuple<int32, uint32, int32> const& a, std::tuple<int32, uint32, int32> const& b)
                { return std::get<0>(a) < std::get<0>(b); });

            std::vector<std::pair<uint32, int32>>& priority = g_specPriority[specId];
            for (auto const& [order, spellId, stepId] : steps)
            {
                bool seen = false;
                for (auto const& existing : priority)
                    if (existing.first == spellId) { seen = true; break; }
                if (!seen)
                    priority.emplace_back(spellId, stepId);   // keep highest-priority occurrence
            }
        }
```

(Remove the old `specSteps`/sort/dedup block this replaces. Keep everything else in `EnsureIndex` — the `containerSpec` build and the function's structure — unchanged.)

- [ ] **Step 4: Add the condition evaluator (anonymous namespace, after EnsureIndex)**

```cpp
    // True if `target` still warrants casting `stepSpellId` from this step. No fork
    // rules -> always true. Unknown opcodes -> true (fail-open). Drives BotRotation's
    // "don't re-apply an active DoT" gate.
    bool EvaluateStepConditions(Player* bot, Unit* target, int32 stepId, uint32 stepSpellId)
    {
        auto itr = g_stepRules.find(stepId);
        if (itr == g_stepRules.end())
            return true;

        for (RuleData const& rule : itr->second)
        {
            switch (rule.conditionType)
            {
                case BotRotationPolicy::BOT_COND_TARGET_MISSING_OR_EXPIRING_MY_AURA:
                {
                    uint32 const auraId = rule.value1 ? uint32(rule.value1) : stepSpellId;
                    Aura const* aura = target->GetAura(auraId, bot->GetGUID());
                    int32 const remainingMs = aura ? aura->GetDuration() : 0;
                    if (!BotRotationPolicy::ShouldCastForMissingOrExpiringAura(aura != nullptr, remainingMs, rule.value2))
                        return false;
                    break;
                }
                default:
                    break;   // unknown fork opcode -> fail-open
            }
        }
        return true;
    }
```

- [ ] **Step 5: Apply the condition gate in SelectSpell**

In `SelectSpell`, the loop currently iterates `for (uint32 spellId : specItr->second)`. Change it to unpack the pair and add the condition gate as the final check before returning. Replace the loop header:

```cpp
    for (uint32 spellId : specItr->second)
    {
```

with:

```cpp
    for (auto const& [spellId, stepId] : specItr->second)
    {
```

and **replace** the loop's final `return spellId;` (the last statement inside the loop, after all the existing gates) with:

```cpp
        if (!EvaluateStepConditions(bot, target, stepId, spellId))
            continue;

        return spellId;
```

(Leave the existing `HasSpell` / cooldown / cost / range gates above it untouched. There must be exactly one `return spellId;` in the loop after this change.)

- [ ] **Step 6: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotRotation.cpp
git commit -m "feat(bots): gate rotation candidates on fork conditions (custom steps)"
```

---

## Task 3: Author the Priest SWP condition rule (data)

**Files:**
- Create: `sql/updates/hotfixes/master/2026_06_24_00_hotfixes.sql`

- [ ] **Step 1: Write the migration**

Create `sql/updates/hotfixes/master/2026_06_24_00_hotfixes.sql`:

```sql
-- =====================================================================
-- Condition-aware low-level Priest rotation
-- =====================================================================
-- Gates the Priest "Initial" spec (1452) Shadow Word: Pain step on
-- "target is missing my SWP, or it is about to expire" so the bot casts
-- Smite (the rule-less filler step) while SWP is up, and refreshes SWP
-- under 3s remaining. Without this, SWP (cheap, no cooldown) is the top
-- castable spell every GCD and Smite never fires.
--
-- Fork opcode 1000 = BOT_COND_TARGET_MISSING_OR_EXPIRING_MY_AURA, evaluated
-- by BotRotation ONLY for custom steps (ID >= 1000000). ConditionValue1 = 0
-- => use the step's own spell (589, SWP); ConditionValue2 = 3000 => refresh
-- window in ms; ConditionValue3 unused. Targets step 1000010 (the SWP step
-- from 2026_06_21_00_hotfixes.sql). The Smite step (1000011) gets no rule.
--
-- Auto-applied by the worldserver updater (hotfixes DB); the hotfix loader
-- then applies the Status=1 row to the in-memory sAssistedCombatRuleStore.
-- Append-only (Updates.Redundancy=0): never edit this file once applied; add
-- 2026_06_24_01_hotfixes.sql to correct a value.
--
-- Table hash (db2 header table_hash):
--   AssistedCombatRule  0xC1B4F680 = 3249862272
-- =====================================================================

-- --- idempotent cleanup (safe re-run; the updater applies once) ----------
DELETE FROM `assisted_combat_rule` WHERE `ID` IN (1000010);
DELETE FROM `hotfix_data`          WHERE `Id` IN (9000030);

INSERT INTO `assisted_combat_rule`
    (`ID`, `OrderIndex`, `Field_11_1_7_60520_002`, `ConditionType`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `AssistedCombatStepID`, `VerifiedBuild`) VALUES
(1000010, 0, 0, 1000, 0, 3000, 0, 1000010, 0);

INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000030, 2000000030, 3249862272, 1000010, 1, 0);
```

- [ ] **Step 2: Commit**

```bash
git add sql/updates/hotfixes/master/2026_06_24_00_hotfixes.sql
git commit -m "feat(bots): condition rule -- Priest casts Smite as SWP filler"
```

---

## Task 4: Build, deploy, verify in-game, then clean up diagnostics

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp` (remove `[CAST-DIAG]`)
- Modify: `CHANGELOG-custom.md`

- [ ] **Step 1: Full rebuild (C++ + scripts changed)**

```bash
cd /e/TrinityCore
docker images trinitycore:local --format 'pre-build: {{.ID}}'
docker compose --progress plain build > build-botcond.log 2>&1; echo "BUILD_EXIT=$?" >> build-botcond.log
```

- [ ] **Step 2: Verify the build actually compiled**

```bash
grep BUILD_EXIT build-botcond.log              # expect BUILD_EXIT=0
grep -niE "error:|failed to solve" build-botcond.log | head   # expect empty
docker images trinitycore:local --format '{{.ID}} {{.CreatedAt}}'  # expect a NEW id
```
Expected: `BUILD_EXIT=0`, no errors, image id changed. If the build failed, fix and re-run before proceeding.

- [ ] **Step 3: Deploy**

```bash
docker compose up -d
# wait for readiness, then confirm:
docker logs tc-worldserver 2>&1 | grep -iE "World initialized|ready\.\.\." | tail -2
```
Expected: `World initialized ...` and `... ready...`. (The migration applies during startup; check it ran: `docker exec tc-worldserver sh -lc 'grep -i "2026_06_24_00_hotfixes" /opt/trinitycore/logs/Server.log | tail -2'`.)

- [ ] **Step 4: In-game verification (the integration gate)**

In game, as the GM master: `.bot add Followerone` (Priest), pull a mob, fight ~10 seconds. Then read the rotation log:

```bash
docker exec tc-worldserver sh -lc 'grep "rotation: spec 1452" /opt/trinitycore/logs/Server.log | tail -20'
```
Expected pattern: `spell 589` (SWP) once, then `spell 585` (Smite) repeated for several GCDs, then `spell 589` again when SWP drops under 3 s — i.e. SWP is no longer cast every GCD. Also confirm Hunter (`.bot add Zebgoro`) and Warrior (`.bot add Lithilia`) still rotate (`grep "rotation: spec 1448"` / `1446`) — unchanged.

If the Priest still spams 589: confirm the rule loaded — `docker exec tc-db sh -lc 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" hotfixes -e "SELECT * FROM assisted_combat_rule WHERE ID=1000010;"'` — and that `hotfix_data` row 9000030 has `Status=1`. Do not proceed to cleanup until the pattern is correct.

- [ ] **Step 5: Remove the temporary `[CAST-DIAG]` logging**

In `BotMgr.cpp`, replace the diagnostic cast block (the `if (spellId) { ... SpellCastResult ... TC_LOG_DEBUG ... } else TC_LOG_DEBUG ...`) with the clean cast:

```cpp
            // M4: data-driven rotation -- cast the top castable Assisted Combat
            // ability at the victim (target check bypassed: BotMgr already validated
            // the target via the master in SelectAssistTarget; the bot's own gate is
            // unreliable for headless sessions). Melee fills the gaps between casts.
            if (uint32 const spellId = BotRotation::SelectSpell(bot, target))
                bot->CastSpell(target, spellId, TRIGGERED_IGNORE_TARGET_CHECK);
            continue;
```

- [ ] **Step 6: Update CHANGELOG-custom.md**

Under *Custom features* (bots) and *Code modifications to upstream files*, add an entry describing: (a) the `TRIGGERED_IGNORE_TARGET_CHECK` cast fix in `BotMgr.cpp` (headless bots couldn't cast — `SPELL_FAILED_BAD_TARGETS` from the bot's own attack-validity gate); (b) the new `BotRotationPolicy` + `BotRotation` condition layer (fork opcodes on custom steps, fail-open for Blizzard rules); (c) the `2026_06_24_00_hotfixes.sql` Priest SWP rule. Note the `[CAST-DIAG]` diagnostic was added and removed.

- [ ] **Step 7: Rebuild + redeploy with the diagnostic removed, confirm still casting**

```bash
cd /e/TrinityCore
docker compose --progress plain build > build-botcond2.log 2>&1; echo "BUILD_EXIT=$?" >> build-botcond2.log
grep BUILD_EXIT build-botcond2.log; grep -niE "error:" build-botcond2.log | head
docker compose up -d
```
Then a quick in-game re-check that bots still cast (the `[CAST-DIAG]` line is gone; bots visibly cast). Delete the build logs (`rm -f build-botcond*.log`).

- [ ] **Step 8: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.cpp CHANGELOG-custom.md
git commit -m "feat(bots): cast rotation via combat assist (ignore bot target gate) + cleanup"
```

---

## Task 5: Finish the branch

- [ ] **Step 1: Push and open a PR against the fork**

```bash
git push -u origin claude/bot-rotation-conditions
gh pr create --repo mountogdengc/TrinityCore --base main --head claude/bot-rotation-conditions \
  --title "feat(bots): condition-aware rotation + headless cast fix" --body "$(cat <<'EOF'
Headless bots now cast their Assisted Combat rotation instead of melee-only, and
the engine skips pointless casts.

- Cast fix: bots cast with TRIGGERED_IGNORE_TARGET_CHECK (the bot's own
  IsValidAttackTarget gate is unreliable for headless sessions -> SPELL_FAILED_BAD_TARGETS;
  the target is already validated via the master in SelectAssistTarget).
- Condition layer: new pure BotRotationPolicy (unit-tested) + BotRotation evaluates
  fork condition opcodes for our custom steps only; Blizzard's undocumented opcodes
  stay fail-open. First primitive: don't re-apply a DoT that's still active.
- Data: 2026_06_24_00_hotfixes.sql gates Priest Shadow Word: Pain so Smite is used
  as filler and SWP is refreshed under 3s.

Spec: docs/superpowers/specs/2026-06-24-condition-aware-bot-rotation-design.md
EOF
)"
```

- [ ] **Step 2: Merge (per user)**

```bash
gh pr merge <N> --repo mountogdengc/TrinityCore --merge --delete-branch
git checkout main && git pull --ff-only origin main
```

---

## Notes / guardrails

- Append-only hotfix migrations (`Updates.Redundancy=0`): never edit an applied file; add a new `_NN` file.
- Fail-open is intentional: an unknown/malformed condition must never stop a bot from acting (worst case = today's behavior).
- The `docker/worldserver/entrypoint.sh` `Logger.bots` Debug toggle is required for Step 4/7 verification; it's a temp change marked "revert before committing" and stays out of these commits.
- The cast fix (`TRIGGERED_IGNORE_TARGET_CHECK`) and `SpellDefines.h` include are already in the working tree from the diagnosis session; Task 4 Step 5 finalizes that file.
```
