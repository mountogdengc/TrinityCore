# Low-level Priest & Warrior bot rotations

**Date:** 2026-06-21
**Status:** Design (approved for planning)

## Summary

Give the headless player-bot an actual ability rotation for **low-level (sub-10,
pre-spec) Priest and Warrior** characters, so a bot fights with spells instead of
melee-only auto-attack.

The M4 rotation engine (`BotRotation::SelectSpell`, merged on `main`) is already
**fully data-driven**: it builds a per-spec ability priority list from the
`AssistedCombat` / `AssistedCombatStep` DB2 stores, keyed by `ChrSpecializationID`,
and each combat tick returns the highest-priority ability the bot can actually cast
(known + off cooldown/GCD + affordable + in range). It needs **no code change** to
support new classes — it just needs the *data* for the relevant spec.

Sub-10 characters report their class's **"Initial"** `ChrSpecialization`, for which
Blizzard ships no `AssistedCombat` data. So this work is **data-only**: author
custom `AssistedCombat` Initial-spec rows for Priest and Warrior — the same row
pattern as the existing `sql/custom/spike_assisted_combat_hunter_lowlevel.sql`,
loaded into the hotfixes DB so the worldserver's in-memory DB2 stores (and thus the
headless bot) see them.

## Goals

- A sub-10 Priest bot casts a real damage rotation in combat (DoT + filler).
- A sub-10 Warrior bot casts its low-level damage ability, with melee carrying the
  rest.
- Zero C++ changes — prove the data-driven engine generalizes to new classes.
- Reusable file with headroom to add more classes the same way.

## Non-goals

- No spec'd (level 10+) rotations — those specs already have Blizzard
  `AssistedCombat` data; if/when bots are leveled past 10 the engine should pick it
  up automatically (verify separately).
- No condition/proc-aware abilities. The engine does **not** yet evaluate the
  `AssistedCombatRule` `ConditionType` / `ConditionValueN` columns, so abilities
  with special usability gates (e.g. Victory Rush, usable only after a kill) would
  mis-fire (`SPELL_FAILED`) every off-cooldown tick. We restrict the lists to
  **unconditional, always-castable** damage abilities until the condition-decoding
  slice lands.
- No client-facing intent. Unlike the Hunter file (a *client-rendering*
  experiment), this data exists to drive the **server-side** headless bot; the
  client push is incidental.

## Why this is not a C++ change (rejected alternative)

Hardcoding per-class fallback rotations in `BotRotation` was considered and
rejected: it would duplicate the data-driven engine we just built, split the
rotation source of truth in two, and not scale to other classes. The engine's whole
point is that a new class = new data rows. We stay on that path.

## Design

### The rotation data

One file: **`sql/custom/bot_lowlevel_rotations.sql`**, holding Priest + Warrior
Initial-spec rotations, with room to append more classes. Per class it inserts:

- one `assisted_combat` row: `(ID, ChrSpecializationID = <class Initial spec>)` —
  the spec → rotation container.
- N `assisted_combat_step` rows: `(ID, SpellID, AssistedCombatID, OrderIndex)` — the
  ordered ability list (lowest `OrderIndex` = highest priority).
- matching `hotfix_data` rows (`Status = 1` Valid) for each inserted record, by
  table hash, so the worldserver's hotfix loader applies them to the in-memory
  stores at startup.

Row IDs use a high, collision-safe range (continuing the Hunter file's
`1000001+` / push-id `9000001+` convention, with distinct values per class so the
two files and the two classes never collide).

### Proposed rotations (priority order)

The engine gates every candidate on `bot->HasSpell(spellId)`, so listing a spell the
bot hasn't learned yet is harmless — it's skipped. Lists are unconditional damage
only (see Non-goals).

- **Priest — Initial spec**
  1. Shadow Word: Pain (`589`) — DoT, apply first
  2. Smite (`585`) — direct-damage filler
- **Warrior — Initial spec**
  1. Slam (`1464`) — low-level damage ability; melee auto-attack carries the rest

### Values to verify against this build (67823)

Same VERIFY discipline the Hunter file calls out — a wrong ID makes the data
silently inert (the spec's priority list stays empty and the bot just melees):

1. **Initial `ChrSpecializationID` per class.** Hunter's `1448` is confirmed by the
   existing spike. Candidates to confirm: **Priest `1452`**, **Warrior `1446`**
   (= `GetDefaultChrSpecializationForClass` → the class's `ChrSpecialization` row at
   the Initial `OrderIndex`). Confirm before finalizing the SQL.
2. **Spell IDs** `589` / `585` / `1464` are the correct, known-early spells for a
   sub-10 bot on this build.

Verification method (resolved in the plan): confirm a spawned sub-10 Priest/Warrior
bot's `GetPrimarySpecialization()` equals the value we used (e.g. a one-off
`TC_LOG_DEBUG` of the bot's primary spec on spawn, or inspecting the loaded
`ChrSpecialization` data), and that `bot->HasSpell()` is true for the listed spells.

### Application & loading

- Apply the file to the **hotfixes** DB (the Hunter file's pattern):
  `mysql -u trinity -p hotfixes < sql/custom/bot_lowlevel_rotations.sql`.
- The tables already exist (created by
  `sql/updates/hotfixes/master/2026_06_19_00_hotfixes.sql`).
- On startup the worldserver's `DB2Manager` hotfix loader applies the `Status = 1`
  rows to `sAssistedCombatStore` / `sAssistedCombatStepStore`, so the headless bot
  (server-side) sees them — no client required.
- The file is idempotent: it `DELETE`s its own row IDs before re-`INSERT`ing, and
  carries a rollback block, matching the Hunter file.

(Staying in `sql/custom/` keeps parity with the Hunter file and keeps this opt-in
data out of the auto-applied `sql/updates` tree. Promoting it to a hotfixes
*updater migration* — so it survives a hotfixes-DB rebuild without a manual step —
is a possible follow-up, noted but not done here.)

## Verification / success criteria

1. After applying the file and restarting the worldserver, spawn a sub-10 Priest
   bot and a sub-10 Warrior bot, group + follow a master, and engage a mob.
2. The Priest bot casts Shadow Word: Pain then Smite (observe target debuff +
   health drop, and/or a debug log at the `BotMgr::UpdateFollow` cast site).
3. The Warrior bot casts Slam between melee swings.
4. A bot that hasn't yet learned a listed spell falls back to melee with no error
   spam (the `HasSpell` gate).

## Files touched

- **New:** `sql/custom/bot_lowlevel_rotations.sql`
- **Updated:** `CHANGELOG-custom.md` (extend the Assisted-combat rotation entry, or
  add a sibling, to record the low-level Priest/Warrior bot data and how to apply
  it).
- Possibly a temporary debug log line in `src/server/scripts/Custom/Bots/BotMgr.cpp`
  for verification, reverted before finishing (decide in the plan).
