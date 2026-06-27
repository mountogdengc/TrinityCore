# Bot formation engine + chat control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A server-side bot formation engine with named presets (Line / Wedge / Circle / Column), driven out-of-combat by `.bot formation <preset>`, so the squad arranges around the master in a chosen shape.

**Architecture:** A pure `BotFormationPolicy` (enum + per-slot polar offset + name parse, unit-tested) computes each bot's `(distance, angle)` relative to the master; `BotMgr` holds a per-master preset, computes each bot's squad index/size each follow pass, and feeds those into the existing `MoveFollow(master, dist, ChaseAngle(angle))`; a `.bot formation` chat command sets the preset. Combat positioning is untouched.

**Tech Stack:** C++ (TrinityCore master), Catch2 unit tests, Docker Compose build/run, in-game verification.

**Spec:** `docs/superpowers/specs/2026-06-26-bot-formation-engine-design.md`

**Verified facts:** the follow path is `BotMgr.cpp:454-457`
(`MoveFollow(master, BOT_FOLLOW_DIST, ChaseAngle(BotMovementPolicy::FormationFollowAngle(entry.formationSlot)))`);
`MoveFollow(Unit*, float dist, Optional<ChaseAngle>, ...)` takes per-bot distance+angle;
`ChaseAngle` angle is relative to the master's orientation (`PI` = behind). The `.bot`
command table is in `cs_bot.cpp:37-45`, with a `GetOwner(handler)` helper (cs_bot.cpp:56)
and printf-style `PSendSysMessage`.

---

## File structure

- **Create** `src/server/scripts/Custom/Bots/BotFormationPolicy.{h,cpp}` — enum + `Offset` + `Parse` (pure).
- **Create** `tests/game/BotFormationPolicy.cpp`; **modify** `tests/CMakeLists.txt`.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.h` — `_formations` map, `Get/SetFormation`, `formationKey` on `BotEntry`, include the policy header.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.cpp` — implement `Get/SetFormation`; squad-index pre-pass + formation-driven `MoveFollow` in `UpdateFollow`.
- **Modify** `src/server/scripts/Custom/Bots/cs_bot.cpp` — `.bot formation` command.
- **Modify** `CHANGELOG-custom.md`.

---

## Task 1: Pure formation policy (TDD)

**Files:**
- Create: `src/server/scripts/Custom/Bots/BotFormationPolicy.h`, `.cpp`
- Test: `tests/game/BotFormationPolicy.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Header** — create `src/server/scripts/Custom/Bots/BotFormationPolicy.h`:

```cpp
/*
 * Player-bot formation policy helpers (pure, unit-tested).
 */

#ifndef TRINITYCORE_BOTS_BOTFORMATIONPOLICY_H
#define TRINITYCORE_BOTS_BOTFORMATIONPOLICY_H

#include "Define.h"
#include "Optional.h"
#include <string_view>

enum class BotFormation : uint8 { Line, Wedge, Circle, Column };

// Polar offset in the master's frame: angle is relative to the master's orientation
// (0 = in front, PI = directly behind), as MoveFollow's ChaseAngle expects.
struct FormationOffset { float distance; float angle; };

namespace BotFormationPolicy
{
    // Each bot's spot for a preset, given its index within the squad (0..count-1) and the
    // squad size. Pure.
    FormationOffset Offset(BotFormation preset, uint32 slot, uint32 count);

    // Parse a command argument ("line"/"wedge"/"circle"/"column", case-insensitive).
    Optional<BotFormation> Parse(std::string_view name);
}

#endif // TRINITYCORE_BOTS_BOTFORMATIONPOLICY_H
```

- [ ] **Step 2: Failing test** — create `tests/game/BotFormationPolicy.cpp`. Copy the GPL license header from `tests/game/BotMovementPolicy.cpp`, then:

```cpp
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
```

- [ ] **Step 3: Register in `tests/CMakeLists.txt`** — add to the `target_sources(tests PRIVATE ...)` Bots block, alphabetically (after `BotDeathPolicy.cpp`, before `BotMovementPolicy.cpp`):

```cmake
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotFormationPolicy.cpp
```

- [ ] **Step 4: Implementation** — create `src/server/scripts/Custom/Bots/BotFormationPolicy.cpp`:

```cpp
/*
 * Player-bot formation policy helpers -- see BotFormationPolicy.h.
 */

#include "BotFormationPolicy.h"
#include <cctype>
#include <cmath>

namespace
{
    constexpr float BOT_FORM_PI     = 3.14159265358979f;
    constexpr float BOT_FORM_BASE   = 2.5f;   // base follow depth behind the master (yd)
    constexpr float BOT_FORM_STEP   = 2.5f;   // spacing between adjacent slots (yd)
    constexpr float BOT_FORM_RADIUS = 3.5f;   // circle radius (yd)

    // forward = along the master's facing (+ = in front), side = perpendicular.
    FormationOffset FromCartesian(float forward, float side)
    {
        return FormationOffset{ std::sqrt(forward * forward + side * side),
                                std::atan2(side, forward) };
    }
}

namespace BotFormationPolicy
{
FormationOffset Offset(BotFormation preset, uint32 slot, uint32 count)
{
    switch (preset)
    {
        case BotFormation::Column:
            return FromCartesian(-(BOT_FORM_BASE + float(slot) * BOT_FORM_STEP), 0.0f);

        case BotFormation::Line:
        {
            float const center = float(count > 0 ? count - 1 : 0) * 0.5f;
            return FromCartesian(-BOT_FORM_BASE, (float(slot) - center) * BOT_FORM_STEP);
        }

        case BotFormation::Wedge:
        {
            uint32 const pair = slot / 2 + 1;
            float const sign = (slot % 2 == 0) ? 1.0f : -1.0f;
            return FromCartesian(-(float(pair) * BOT_FORM_STEP), sign * float(pair) * BOT_FORM_STEP);
        }

        case BotFormation::Circle:
        default:
        {
            uint32 const n = count > 0 ? count : 1;
            return FormationOffset{ BOT_FORM_RADIUS, (2.0f * BOT_FORM_PI * float(slot)) / float(n) };
        }
    }
}

Optional<BotFormation> Parse(std::string_view name)
{
    auto eq = [](std::string_view a, char const* b)
    {
        std::string_view const bv(b);
        if (a.size() != bv.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) != static_cast<unsigned char>(bv[i]))
                return false;
        return true;
    };

    if (eq(name, "line"))   return BotFormation::Line;
    if (eq(name, "wedge"))  return BotFormation::Wedge;
    if (eq(name, "circle")) return BotFormation::Circle;
    if (eq(name, "column")) return BotFormation::Column;
    return {};
}
}
```

- [ ] **Step 5: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotFormationPolicy.h src/server/scripts/Custom/Bots/BotFormationPolicy.cpp \
        tests/game/BotFormationPolicy.cpp tests/CMakeLists.txt
git commit -m "feat(bots): pure formation policy (preset offsets + name parse)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

(No Docker build; the `tests` target is CI-verified.)

---

## Task 2: Per-master formation state (BotMgr header + accessors)

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.h`
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`

- [ ] **Step 1: Header** — in `BotMgr.h`:

(a) Add the include near the other `Bot*Policy.h` includes (it's a pure header):
```cpp
#include "BotFormationPolicy.h"
```
(If `BotMgr.h` includes its policy headers from the `.cpp` instead, add it wherever `BotFormation` will be visible to the class body — it's needed for the `_formations` map member and the `Get/SetFormation` signatures. Read the top of `BotMgr.h` first; add the include there.)

(b) Add a field to `BotEntry` after `petDeadTimer`:
```cpp
        uint32        formationKey = 0xFFFFFFFF; // last-applied (preset|index|count) packing; re-issue follow on change
```

(c) Declare the accessors in the public section (near `AddBot`/`RemoveBot`):
```cpp
    // Per-master follow formation (in-memory; defaults to Wedge).
    BotFormation GetFormation(ObjectGuid master) const;
    void SetFormation(ObjectGuid master, BotFormation preset);
```

(d) Add the state member in the private section (near `_bots`):
```cpp
    std::unordered_map<ObjectGuid, BotFormation> _formations;   // master guid -> chosen follow formation
```

- [ ] **Step 2: Accessors** — in `BotMgr.cpp`, add (e.g. just above `BotMgr::UpdateFollow`):

```cpp
BotFormation BotMgr::GetFormation(ObjectGuid master) const
{
    auto itr = _formations.find(master);
    return itr != _formations.end() ? itr->second : BotFormation::Wedge;
}

void BotMgr::SetFormation(ObjectGuid master, BotFormation preset)
{
    _formations[master] = preset;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.h src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): per-master formation state + accessors

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Drive the follow path with the formation

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`

- [ ] **Step 1: Squad index/count pre-pass** — at the very top of `BotMgr::UpdateFollow` (before the `for (auto& [name, entry] : _bots)` loop), add:

```cpp
    // Per-master squad index/size for formation positioning. Computed over ALL bots up
    // front so the per-bot early-continues below don't shift anyone's slot.
    std::unordered_map<ObjectGuid, uint32> squadCount;
    std::unordered_map<std::string, uint32> squadIndex;
    for (auto const& [n, e] : _bots)
        if (!e.master.IsEmpty())
            squadIndex[n] = squadCount[e.master]++;
```

- [ ] **Step 2: Replace the follow call** — the block at `BotMgr.cpp:454-457` currently reads:

```cpp
        entry.combatTarget = ObjectGuid::Empty;
        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
            bot->GetMotionMaster()->MoveFollow(master, BOT_FOLLOW_DIST,
                ChaseAngle(BotMovementPolicy::FormationFollowAngle(entry.formationSlot)));
```

Replace it with:

```cpp
        entry.combatTarget = ObjectGuid::Empty;
        {
            uint32 const idx = squadIndex[name];
            uint32 const cnt = squadCount[entry.master];
            BotFormation const preset = GetFormation(entry.master);
            FormationOffset const f = BotFormationPolicy::Offset(preset, idx, cnt);
            // Re-issue the follow when it isn't active OR the formation/slot/size changed,
            // so `.bot formation` and squad-membership changes take effect promptly (but not
            // every tick, which would stutter the path).
            uint32 const key = (uint32(preset) << 16) | ((idx & 0xFF) << 8) | (cnt & 0xFF);
            if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE
                || entry.formationKey != key)
            {
                bot->GetMotionMaster()->MoveFollow(master, f.distance, ChaseAngle(f.angle));
                entry.formationKey = key;
            }
        }
```

(`BotFormationPolicy.h` is already included via `BotMgr.h` from Task 2. `BotMovementPolicy::FormationFollowAngle` is no longer used by this call but stays in the codebase — leave it. `<unordered_map>`/`<string>` are already included in `BotMgr.cpp`; confirm and add only if missing.)

- [ ] **Step 3: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): position the squad by formation preset on the follow path

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: `.bot formation` command

**Files:**
- Modify: `src/server/scripts/Custom/Bots/cs_bot.cpp`

- [ ] **Step 1: Include the policy header** — add near the top of `cs_bot.cpp` with the other includes:

```cpp
#include "BotFormationPolicy.h"
```

- [ ] **Step 2: Register the subcommand** — in the `botCommandTable` (cs_bot.cpp:37-45), add a `formation` row (after `stay`):

```cpp
            { "formation", HandleBotFormationCommand, rbac::RBAC_PERM_COMMAND_GM, Console::No  },
```

- [ ] **Step 3: Handler** — add the handler alongside the other `HandleBot*` statics (e.g. after `HandleBotStayCommand`):

```cpp
    static bool HandleBotFormationCommand(ChatHandler* handler, std::string const& preset)
    {
        Player* owner = GetOwner(handler);
        if (!owner)
            return false;

        Optional<BotFormation> const parsed = BotFormationPolicy::Parse(preset);
        if (!parsed)
        {
            handler->SendSysMessage("Usage: .bot formation line|wedge|circle|column");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sBotMgr->SetFormation(owner->GetGUID(), *parsed);
        handler->PSendSysMessage("Bots now in %s formation.", preset.c_str());
        return true;
    }
```

(`GetOwner` is the existing helper at cs_bot.cpp:56; `sBotMgr` is the singleton used elsewhere in this file; `PSendSysMessage` is printf-style — `%s`, not `{}`.)

- [ ] **Step 4: Commit**

```bash
git add src/server/scripts/Custom/Bots/cs_bot.cpp
git commit -m "feat(bots): .bot formation <line|wedge|circle|column> command

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: Changelog + build + in-game verify

**Files:**
- Modify: `CHANGELOG-custom.md`

- [ ] **Step 1: Changelog** — after the `## Hunter bot pets` section, add:

```markdown
## Bot formations

Status: **first pass — engine + chat control**

The squad's follow arrangement is now a chosen preset instead of a fixed fan. Pure
`BotFormationPolicy` maps `(preset, slot, count)` to a polar offset `(distance, angle)` in
the master's frame — **Line / Wedge / Circle / Column** — and `BotMgr` feeds it into the
existing `MoveFollow(master, dist, ChaseAngle(angle))` (so the shape rotates with the
master as they turn). Each follow pass derives every bot's index within its master's squad.
Set it with `.bot formation line|wedge|circle|column` (GM, in-world); default is Wedge.
In-memory per master (resets on restart); combat positioning is unchanged.
Key files: `src/server/scripts/Custom/Bots/BotFormationPolicy.{h,cpp}`
(`tests/game/BotFormationPolicy.cpp`), `BotMgr.{h,cpp}`, `cs_bot.cpp`.
Out of scope (later): the drag-drop addon UI panel, spacing/persistence commands, the
admin map. Spec/plan: `docs/superpowers/specs/2026-06-26-bot-formation-engine-design.md`,
`docs/superpowers/plans/2026-06-26-bot-formation-engine.md`.
```

Commit:
```bash
git add CHANGELOG-custom.md
git commit -m "docs(bots): changelog entry for bot formations

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

- [ ] **Step 2: Build**

```bash
cd /e/TrinityCore
docker images trinitycore:local --format 'pre-build: {{.ID}}'
docker compose --progress plain build > build-formation.log 2>&1; echo "BUILD_EXIT=$?" >> build-formation.log
```

- [ ] **Step 3: Verify build** — `BUILD_EXIT=0`, no `error:`/`failed to solve`, and the image ID changed. (Link fails with undefined `BotFormationPolicy::*` / `BotMgr::SetFormation` if a file was missed.) If only the final `unpacking … rpc … EOF` failed but `naming to docker.io/library/trinitycore:local` completed and the image ID changed, the build is good (daemon status blip).

- [ ] **Step 4: Deploy**

```bash
docker compose up -d
docker logs tc-worldserver 2>&1 | grep -iE "World initialized|ready\.\.\." | tail -2
```

- [ ] **Step 5: In-game verification (the integration gate)** — add 3–4 bots and `.bot follow`, then:
  - `.bot formation column` → bots line up **single-file directly behind** you.
  - `.bot formation line` → bots spread into a **rank behind** you.
  - `.bot formation wedge` → bots form an **arrowhead** with you at the point.
  - `.bot formation circle` → bots **surround** you.
  - **Turn in place** → the shape **rotates to stay oriented** behind/around you.
  - Pull a mob → bots still **spread around the target** (combat unchanged); after the fight they reform.
  - `.bot formation box` (invalid) → prints the usage line.
  - If a shape mirrors the wrong way (left/right swapped) or looks loose, note it — the fix is flipping the `side` sign in `BotFormationPolicy` or tightening the `ChaseAngle` tolerance.

- [ ] **Step 6: Clean up** — `rm -f build-formation.log`

---

## Task 6: PR + merge

- [ ] **Step 1: Push + PR**

```bash
git push -u origin claude/bot-formation-engine
gh pr create --repo mountogdengc/TrinityCore --base main --head claude/bot-formation-engine \
  --title "feat(bots): formation engine + .bot formation command (line/wedge/circle/column)" --body "$(cat <<'EOF'
The squad's follow arrangement is now a chosen formation preset instead of a fixed fan.

- New pure `BotFormationPolicy` (`Offset(preset, slot, count)` → polar offset; `Parse`) + unit tests. Presets: Line / Wedge / Circle / Column.
- `BotMgr` keeps a per-master preset (default Wedge), derives each bot's index within its squad each follow pass, and feeds the offset into the existing `MoveFollow(master, dist, ChaseAngle(angle))` — so the shape rotates with the master. A per-bot "formation key" re-issues the follow when the preset/slot/size changes so `.bot formation` takes effect promptly.
- `.bot formation line|wedge|circle|column` (GM, in-world).

Combat positioning is unchanged. In-memory per master (resets on restart). First sub-project of the formation/UI vision — the drag-drop addon panel and admin map are separate follow-ups.

Verified in-game: each preset forms correctly, rotates as the master turns, and combat still spreads around the target.

Spec: `docs/superpowers/specs/2026-06-26-bot-formation-engine-design.md`

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 2: Merge (per user) + sync**

```bash
gh pr merge <PR#> --repo mountogdengc/TrinityCore --merge --delete-branch
git checkout main && git pull --ff-only origin main
```

---

## Notes / guardrails

- `MoveFollow`'s `ChaseAngle` is body-relative, so formations rotate with the master automatically — no per-tick recompute of world positions needed.
- The `formationKey` packs `(preset<<16 | index<<8 | count)`; `index`/`count` are small (≤ a party/raid), so 8 bits each is plenty.
- Don't touch the combat `MoveChase` path — formations are follow-only this pass.
- `BotMovementPolicy::FormationFollowAngle` becomes unused by the follow call but stays (still unit-tested); removing it is out of scope.
