# Bot formation engine + chat control

**Date:** 2026-06-26
**Status:** approved design, pre-implementation
**Area:** player-bots (`src/server/scripts/Custom/Bots/`)

## Problem

Bots currently fan out behind the master with a fixed per-slot angle
(`BotMovementPolicy::FormationFollowAngle`) — a single hardcoded arrangement. There's no
way to choose how the squad arranges around you (single-file through a cave, a defensive
ring, an arrowhead, a spread line). This is the foundation of the larger formation/UI
vision (`docs/bot-ux-future-milestones.md` M-F1): the server-side formation engine that a
future addon panel would remote-control.

## Goal

A server-side **formation engine** with named presets (Line / Wedge / Circle / Column),
each computing every bot's position relative to the master, driven out-of-combat via a
`.bot formation <preset>` chat command. Server-only, unit-tested, no client addon.

This is sub-project 1 of the formation/UI system. The drag-drop **addon UI panel** is a
separate later cycle; the **admin world-map / road routing** (M-F6) is a separate
milestone. Both are out of scope here.

## Key facts

- The follow path already calls `MoveFollow(master, dist, ChaseAngle(angle))`
  (`BotMgr.cpp:456`). `MoveFollow(Unit*, float dist, Optional<ChaseAngle> angle, ...)`
  takes a **per-bot distance and angle**, so a formation is just a per-bot polar offset
  `(distance, angle)` in the master's frame. `ChaseAngle`'s angle is relative to the
  target's orientation (`PI` = directly behind), so the formation **rotates with the
  master as they turn** — exactly what we want.
- `.bot` subcommands live in a clean table in `cs_bot.cpp` (`add`/`remove`/`follow`/
  `stay`/`count`), GM-gated; `formation` slots in beside them.
- Combat positioning is separate (`FormationChaseAngle` spreads bots around the *target*);
  formations govern the *follow/travel* arrangement around the *master* and leave combat
  untouched.

## Design

### 1. Pure formation math (`BotFormationPolicy`, unit-tested)

```cpp
enum class BotFormation : uint8 { Line, Wedge, Circle, Column };

struct FormationOffset { float distance; float angle; };   // polar, master's frame (PI = behind)

namespace BotFormationPolicy
{
    // Each bot's spot for a preset, given its index within the squad (0..count-1) and the
    // squad size. Computed as a local Cartesian offset (forward = master's facing, side =
    // perpendicular) then converted to polar for MoveFollow: distance = hypot(forward,side),
    // angle = atan2(side, forward)  (0 = front, PI = behind).
    FormationOffset Offset(BotFormation preset, uint32 slot, uint32 count);

    // Parse a command argument ("line"/"wedge"/"circle"/"column", case-insensitive).
    Optional<BotFormation> Parse(std::string_view name);
}
```

Preset geometry (constants `BASE`≈2.5 yd, `STEP`≈2.5 yd, `RADIUS`≈3.5 yd, tuned in-game):
- **Column:** `forward = -(BASE + slot·STEP)`, `side = 0` → file directly behind.
- **Line:** `forward = -BASE`, `side = (slot - (count-1)/2)·STEP` → a rank centered behind.
- **Wedge:** `pair = slot/2 + 1`, `sign = (slot even ? +1 : -1)`;
  `forward = -(pair·STEP)`, `side = sign·(pair·STEP)` → arrowhead, you at the point.
- **Circle:** `distance = RADIUS`, `angle = 2π·slot/max(count,1)` directly (rotationally
  symmetric).

All pure; Catch2 tests assert the expected geometry and that distinct slots give distinct
offsets.

### 2. Per-master state + wiring (`BotMgr`)

- `std::unordered_map<ObjectGuid /*master*/, BotFormation> _formations;` with a default
  (`BotFormation::Wedge`). Accessor `BotFormation GetFormation(ObjectGuid master) const`
  (returns default if unset) and `void SetFormation(ObjectGuid master, BotFormation)`.
- **Squad index/count:** a pre-pass at the top of `UpdateFollow` groups active bots by
  master into a `std::unordered_map<ObjectGuid, std::vector<...>>` (or equivalent), so each
  bot gets a stable `squadIndex` (0..N-1) and `squadCount` for its master. (Today's
  `BotEntry::formationSlot` is a global counter — formations need a per-squad index.)
- **Follow call:** replace the current
  `MoveFollow(master, BOT_FOLLOW_DIST, ChaseAngle(FormationFollowAngle(slot)))` with:
  ```cpp
  FormationOffset const f = BotFormationPolicy::Offset(GetFormation(master.GetGUID()), squadIndex, squadCount);
  bot->GetMotionMaster()->MoveFollow(master, f.distance, ChaseAngle(f.angle));
  ```

### 3. Control: `.bot formation <preset>` (`cs_bot.cpp`)

A new `formation` subcommand (GM, in-world): `.bot formation line|wedge|circle|column`.
Resolves the caller (`handler->GetPlayer()`), `BotFormationPolicy::Parse`s the argument
(usage message listing the four on failure), calls `sBotMgr->SetFormation(player->GetGUID(),
preset)`, and confirms (`"Bots now in <preset> formation."`). Bots rearrange on the next
follow pass.

### 4. Scope

- **Follow/travel only.** Combat keeps the existing target-spread (`FormationChaseAngle`),
  untouched.
- **In-memory per master** (resets on worldserver restart). DB persistence is a follow-up.
- **Fixed spacing constants** (no `spacing` command yet).
- Default preset for a master with no setting: **Wedge**.

## Data flow

1. `.bot formation circle` → `SetFormation(masterGuid, Circle)`.
2. Each follow pass: pre-pass computes `(squadIndex, squadCount)` per bot; per bot,
   `Offset(preset, squadIndex, squadCount)` → `MoveFollow(master, distance, angle)`.
3. As the master moves/turns, the body-relative angles keep the shape oriented behind them.
4. On combat, the existing chase logic takes over (formation paused until they disengage).

## Testing

- **Unit (CI):** `Offset` geometry per preset (Column all `angle≈PI`, increasing distance;
  Line symmetric lateral at fixed depth; Wedge distance+|side| grow per pair; Circle evenly
  spaced angles; distinct slots → distinct offsets); `Parse` round-trips the four names +
  rejects junk.
- **In-game:** 3–4 bots, `.bot follow`, then each `.bot formation <preset>` — the squad
  visibly forms the shape behind/around you and **rotates as you turn**; pulling a mob still
  spreads them around the target; an unknown preset name prints usage.

## Out of scope (follow-ups)

- The drag-drop **addon UI panel** (sub-project 2) + its addon-message protocol.
- `spacing`/distance adjustment command; DB persistence of the chosen preset.
- Per-bot manual placement; role-aware shaping (melee front / ranged back).
- Admin world-map + road routing (separate milestone).

## Risks / notes

- **Angle sign (left vs right):** `atan2(side, forward)` fixes the convention; if a preset
  mirrors the wrong way in-game, flip the `side` sign — verified during the in-game pass.
- **`ChaseAngle` tolerance** (default ±PI/4) can make tight formations loose; if shapes
  look sloppy, pass a tighter tolerance to `ChaseAngle`. Verify in-game.
- **Squad index stability:** the pre-pass derives index from the bot map each pass; order
  is stable within a session, so a bot keeps its slot unless the squad membership changes
  (then the shape re-packs, which is acceptable).
