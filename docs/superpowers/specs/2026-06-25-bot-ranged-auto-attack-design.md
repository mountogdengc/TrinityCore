# Bot ranged auto-attack (Auto Shot / wand Shoot) between casts

**Date:** 2026-06-25
**Status:** approved design, pre-implementation
**Area:** player-bots (`src/server/scripts/Custom/Bots/`)

## Problem

Ranged bots now hold at ~25 yd and fight via the rotation, but between rotation
casts — and at low level, where a class has very few castable abilities — they just
stand there. A real ranged character's auto-attack (Hunter **Auto Shot**, caster
**wand "Shoot"**) plinks continuously and is a meaningful chunk of low-level DPS.
The bot does nothing in those gaps.

## Goal

A ranged bot in combat keeps its **ranged auto-attack** running, dealing steady
damage alongside the rotation. Hunters always (they carry a bow/gun); casters
(Priest / Mage / Warlock / Evoker) only when a **wand is equipped** — a caster with
no wand is a silent no-op (we do **not** hand out wands). No kiting.

Minimal and contained to `BotMgr` + one new pure policy file, mirroring the
ranged-positioning feature it builds on.

## Key engine facts (validated)

- **Auto Shot / wand Shoot are autorepeat spells.** Cast one once and the server
  loops it every ranged-swing tick via `Unit::update` →
  `m_currentSpells[CURRENT_AUTOREPEAT_SPELL]` (gated by the `RANGED_ATTACK` timer).
  Detected by `SpellInfo::IsAutoRepeatRangedSpell()` (`SPELL_ATTR2_AUTO_REPEAT`).
- **Auto Shot is spell 75 — but there are ~12 "Auto Shot" variants** (145759,
  219676, …) keyed by spec/level. A given Hunter knows exactly one. **So we do not
  hardcode an id** — we scan the bot's known spells for its autorepeat ranged spell.
- **A ranged weapon must be equipped** (slot 15: bow/gun/wand) or the autorepeat
  can't start. `Player::GetWeaponForAttack(RANGED_ATTACK, true)` returns it (or null).
  This null is exactly the caster-without-wand no-op gate.
- Wands are fully functional in this 12.0.5 build (not vestigial).

## Design

### 1. Find the bot's auto-attack spell (engine helper, `BotMgr`)

`uint32 BotMgr::FindRangedAutoAttackSpell(Player* bot)`:

1. If `bot->GetWeaponForAttack(RANGED_ATTACK, /*useable*/true) == nullptr` → return 0
   (no bow/gun/wand → nothing to do; this is the caster-no-wand no-op).
2. Iterate `bot->GetSpellMap()`; for each known **active, non-disabled** spell, fetch
   its `SpellInfo` and return the first whose `IsAutoRepeatRangedSpell()` is true.
   (Auto Shot for a Hunter; Shoot for a wand caster.) → return 0 if none.

The result is cached per bot so we don't scan the spell map every tick:
`BotEntry::rangedAutoSpellId` (the found id, or 0 = none) plus
`BotEntry::rangedAutoChecked` (set true after the first scan). The scan runs once per
bot; thereafter we read the cache. Staleness (a caster equips a wand *after* the first
scan) is acceptable — re-adding the bot refreshes it — and is not worth per-tick
rescans for v1.

### 2. Start / stop in the ranged combat branch (`BotMgr::UpdateFollow`)

This slots into the existing ranged branch (the one that already does
`Attack(target, false)` + `MoveChase(... ChaseRange(BOT_RANGED_DIST) ...)` + runs the
rotation). After positioning:

- Compute `bool repeating = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) != nullptr;`
- If `BotRangedAttackPolicy::ShouldStartAutoRepeat(autoSpellId != 0, repeating, targetChanged)`
  is true, start it: `bot->CastSpell(target, autoSpellId, …)` with **untriggered /
  minimal flags** (NOT `TRIGGERED_FULL_MASK`) so the engine routes it into the
  autorepeat slot. `targetChanged` reuses the `entry.combatTarget != target->GetGUID()`
  signal already maintained for the chase.
- The rotation cast and `Attack(target, false)` are unchanged; the ranged auto-attack
  runs on the independent `RANGED_ATTACK` timer, so all three coexist.

On disengage (the non-combat / follow path, where `entry.combatTarget` is already
reset): `bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL)` so the bot stops shooting when
it leaves combat. Melee bots are untouched.

### 3. Pure decision (`BotRangedAttackPolicy`, unit-tested)

```cpp
namespace BotRangedAttackPolicy
{
    // (Re)start the ranged autorepeat when the bot has an auto-attack spell and is
    // either not currently repeating or just switched targets.
    bool ShouldStartAutoRepeat(bool hasAutoSpell, bool alreadyRepeating, bool targetChanged);
    // => hasAutoSpell && (!alreadyRepeating || targetChanged)
}
```

Mirrors the existing pure `*Policy` modules; keeps the trigger logic out of the engine
glue and testable in CI.

### 4. Files

- **Create** `src/server/scripts/Custom/Bots/BotRangedAttackPolicy.{h,cpp}` — pure
  `ShouldStartAutoRepeat`.
- **Create** `tests/game/BotRangedAttackPolicy.cpp`; **modify** `tests/CMakeLists.txt`.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.{h,cpp}` — `rangedAutoSpellId` +
  `rangedAutoChecked` on `BotEntry`; `FindRangedAutoAttackSpell`; start/stop wiring in
  `UpdateFollow`.

## Data flow

1. Ranged bot engages a target → positions at range, runs rotation (as today).
2. Same tick: if not already auto-repeating (or target changed) and it has an
   auto-attack spell (cached/looked up via `FindRangedAutoAttackSpell`) → cast it once;
   engine loops it on the ranged timer.
3. Bot disengages → `InterruptSpell(CURRENT_AUTOREPEAT_SPELL)`; resumes following.

## Testing

- **Unit (CI):** `ShouldStartAutoRepeat` truth table (has/!has spell × repeating/not ×
  target-changed/not).
- **In-game:**
  - Hunter bot visibly auto-shoots between/over abilities.
  - A caster **with** a wand equipped shoots; a caster **without** a wand does nothing
    (no errors / no log spam).
  - The bot stops shooting when it disengages (target dies / leaves combat).
  - Auto-attack re-targets correctly across a target switch.
  - Verify `GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)` actually engages after the start
    cast (the untriggered-flags requirement).

## Out of scope (follow-ups)

- Kiting / maintaining distance when a mob closes.
- Auto-granting wands to caster bots (a caster with no wand simply doesn't wand-weave).
- Hunter pet summon/maintenance (the next bot feature — separate spec).
- Ammo / reagents (retail has none).

## Risks / notes

- **Repeated start attempts when the autorepeat can't begin** (target momentarily out
  of LoS/range): we only attempt when not already repeating, and the bot holds in
  range, so this is rare; acceptable for v1 (no throttle, no logging). Revisit only if
  it proves noisy in-game.
- **Cast flags:** must preserve autorepeat routing — verified in-game by confirming the
  autorepeat slot populates. If untriggered casting trips an unwanted check, fall back
  to the minimal trigger flags that still set `CURRENT_AUTOREPEAT_SPELL`.
- **Spell-scan cost:** bounded and infrequent (only when starting, with a per-bot
  cache), so no per-tick overhead.
