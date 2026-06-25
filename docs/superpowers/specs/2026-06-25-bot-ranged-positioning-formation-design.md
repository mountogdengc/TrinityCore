# Bot ranged positioning + formation spread

**Date:** 2026-06-25
**Status:** approved design, pre-implementation
**Area:** player-bots (`src/server/scripts/Custom/Bots/`)

## Problem

Two movement issues now that bots cast effective spells:

1. **Ranged bots run into melee.** `BotMgr` drives combat with `MoveChase(target)` (default = melee contact) for *every* class, and sets melee auto-attack via `Attack(target)`. So Hunters/Priests close to melee and stand in the mob's face — they take melee damage they shouldn't and die a lot. Ranged classes should hold at casting range.
2. **Bots stack on the same spot.** Following uses `MoveFollow(master, 2.0f)` with no angle, and combat uses `MoveChase(target)` with no angle, so every bot piles onto one point — hard to see/target individually and visually wrong.

## Goal

- Ranged-class bots **hold at casting range and never run into melee**; they fight via the rotation and simply wait at range when nothing's castable (e.g. OOM).
- Bots **fan out** (distinct per-bot angle) both while following and in combat, so they don't stack and are individually targetable.

Class-based, minimal, and contained to `BotMgr` + one new pure policy file. Role-aware formation, ranged auto-attack/wand, kiting, Hunter pets, and bot gear are **out of scope** (see below).

## Design

### 1. Role classification (pure, by class)

New `BotMovementPolicy` (pure helpers, unit-tested like `BotCombatPolicy`/`BotRotationPolicy`):

```cpp
namespace BotMovementPolicy
{
    // Ranged-DPS / caster classes that should fight at range. Hybrids
    // (Druid/Shaman/Monk/Paladin/DK/DH/Evoker) default to melee for now.
    bool IsRangedClass(uint8 cls);   // true for HUNTER, PRIEST, MAGE, WARLOCK

    // Distinct, stable formation angles per bot "slot" (0..N), relative to the
    // target/master orientation (0 = front, PI = behind). Follow fans behind the
    // master; combat fans around the target.
    float FormationFollowAngle(uint32 slot);   // around PI (behind master)
    float FormationChaseAngle(uint32 slot);    // spread around the target
}
```

- `IsRangedClass`: `cls == CLASS_HUNTER || CLASS_PRIEST || CLASS_MAGE || CLASS_WARLOCK`. Everything else (Warrior/Rogue/DK/Paladin + the remaining hybrids) → melee.
- Angle helpers: deterministic per slot; a small fan (e.g. base ± a step per slot) so consecutive slots get visibly different angles. Exact formula nailed in the plan; both are pure and unit-tested.

### 2. Combat positioning (`BotMgr`)

When the bot has a `target`, branch on `IsRangedClass(bot->GetClass())`:

- **Melee bot** (today's behavior + spread): `Attack(target, true)` then
  `MoveChase(target, {} /*default melee range*/, ChaseAngle(FormationChaseAngle(slot)))`.
- **Ranged bot**: **do not** call `Attack()` (no melee swings, no closing to contact).
  Instead `MoveChase(target, ChaseRange(BOT_RANGED_DIST), ChaseAngle(FormationChaseAngle(slot)))`
  with `BOT_RANGED_DIST ≈ 25.f` (single-arg `ChaseRange` → `MaxRange`, so the bot
  approaches only until within ~25 yd and then holds; it does **not** kite/back off,
  matching scope). The rotation (`BotRotation::SelectSpell` → `CastSpell`) runs every
  tick as today; when nothing is castable the bot just holds at range. Ranged bots
  still enter/stay in combat via spell threat.

The existing chase-refresh logic (re-issue `MoveChase` when the generator was dropped
or the target changed) is preserved; it just passes the range/angle now.

### 3. Formation slot assignment (`BotMgr`)

- Add a `uint8 formationSlot` to `BotEntry`, assigned when the bot is added
  (incrementing per active bot, wrapped to a small max so angles stay sane).
- Following: `MoveFollow(master, BOT_FOLLOW_DIST, ChaseAngle(FormationFollowAngle(slot)))`
  instead of the angle-less call — bots fan in an arc behind the master.
- Combat: the same slot feeds `FormationChaseAngle(slot)` in #2.

### 4. Files

- **Create** `src/server/scripts/Custom/Bots/BotMovementPolicy.{h,cpp}` — pure helpers.
- **Create** `tests/game/BotMovementPolicy.cpp`; **modify** `tests/CMakeLists.txt`
  (add `BotMovementPolicy.cpp` to the `tests` target sources).
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.{h,cpp}` — `formationSlot` on
  `BotEntry`, slot assignment in `AddBot`, ranged/melee branch + angles in the
  combat path, follow angle in `UpdateFollow`.

## Data flow

1. `AddBot` assigns the bot a `formationSlot`.
2. `UpdateFollow` (out of combat): `MoveFollow(master, dist, FormationFollowAngle(slot))`.
3. Combat tick: classify by class → melee (`Attack` + melee `MoveChase`+angle) or
   ranged (no `Attack`; `MoveChase` at `BOT_RANGED_DIST`+angle) → rotation casts.

## Testing

- **Unit (CI):** `IsRangedClass` for each class id; `FormationFollowAngle`/`ChaseAngle`
  return distinct values for distinct slots and are deterministic.
- **In-game:** Hunter/Priest hold ~25 yd and don't run into melee (and stop dying to
  it); Warrior still closes and melees; multiple bots fan out while following and in
  combat; each bot is individually clickable.

## Out of scope (follow-ups)

- Role-aware formation (melee-front / ranged-back shaping).
- Ranged auto-attack / wand between casts (headless ranged-attack mode).
- Kiting / actively maintaining distance when a mob closes.
- Hunter pet summon/management; bot self-gearing.

## Risks / notes

- **Facing:** casts may require facing the target; `MoveChase` keeps the bot oriented
  toward the target while positioned, so this should hold — verify in-game; if a
  stationary at-range bot ever faces away, add an explicit `SetFacingToObject` before
  the cast.
- **Ranged bot meleed when a mob reaches it:** acceptable (no kiting this pass); the
  mob is normally held by the master / a melee bot via threat.
- **Slot churn:** removing/re-adding bots may reuse/advance slots; angles only need to
  be *distinct among currently-active* bots, so a simple wrapped counter is fine.
