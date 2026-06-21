# Low-level Hunter, Priest & Warrior bot rotations

**Date:** 2026-06-21
**Status:** Design (approved for planning) — revised to an auto-applied migration

## Summary

Give the headless player-bot an actual ability rotation for **low-level (sub-10,
pre-spec) Hunter, Priest, and Warrior** characters, so a bot fights with spells
instead of melee-only auto-attack.

The M4 rotation engine (`BotRotation::SelectSpell`, merged on `main`) is already
**fully data-driven**: it builds a per-spec ability priority list from the
`AssistedCombat` / `AssistedCombatStep` DB2 stores, keyed by `ChrSpecializationID`,
and each combat tick returns the highest-priority ability the bot can actually cast
(known + off cooldown/GCD + affordable + in range). It needs **no code change** to
support new classes — it just needs the *data* for the relevant spec.

Sub-10 characters report their class's **"Initial"** `ChrSpecialization`, for which
Blizzard ships no `AssistedCombat` data. So this work is **data-only**: custom
`AssistedCombat` Initial-spec rows for Hunter, Priest, and Warrior, shipped as an
**auto-applied hotfixes updater migration**
(`sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql`). The worldserver's
updater applies it to the hotfixes DB at startup, and the hotfix loader then puts
the rows into the in-memory `sAssistedCombatStore` — which is what the headless bot
reads. This **supersedes and replaces** the manual
`sql/custom/spike_assisted_combat_hunter_lowlevel.sql` spike (its Hunter rows move
into the migration; the standalone file is removed).

## Goals

- Sub-10 Hunter, Priest, and Warrior bots each cast a real damage rotation in
  combat, with melee carrying the gaps.
- **Zero manual DB steps**: the data ships as a normal hotfixes migration that the
  updater applies automatically on startup, and survives a hotfixes-DB rebuild.
- Zero C++ changes to the engine — prove the data-driven engine generalizes to new
  classes (a temporary-but-kept DEBUG log at the cast site is the only code touch,
  for verification).
- One migration with headroom to add more classes the same way.

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
- No client-facing intent. Unlike the original Hunter spike (a *client-rendering*
  experiment), this data exists to drive the **server-side** headless bot; the
  client push is incidental.

## Why this is not a C++ change (rejected alternative)

Hardcoding per-class fallback rotations in `BotRotation` was considered and
rejected: it would duplicate the data-driven engine we just built, split the
rotation source of truth in two, and not scale to other classes. The engine's whole
point is that a new class = new data rows. We stay on that path.

## Design

### The rotation data

One migration: **`sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql`**, holding
Hunter + Priest + Warrior Initial-spec rotations, with room to append more classes.
Per class it inserts:

- one `assisted_combat` row: `(ID, ChrSpecializationID = <class Initial spec>,
  VerifiedBuild = 0)` — the spec → rotation container. `VerifiedBuild = 0` marks a
  custom, all-builds row (the PK is `(ID, VerifiedBuild)`).
- N `assisted_combat_step` rows: `(ID, SpellID, AssistedCombatID, OrderIndex,
  VerifiedBuild = 0)` — the ordered ability list (lowest `OrderIndex` = highest
  priority).
- matching `hotfix_data` rows (`Status = 1` Valid) for each inserted record, by
  table hash, so the worldserver's hotfix loader applies them to the in-memory
  stores at startup (the mechanism the spike proved).

Row IDs use a high, collision-safe range, distinct per class. The file guards each
class with a `DELETE ... WHERE ID IN (...)` before `INSERT` so it is safe to re-run
by hand even though the updater applies it exactly once.

### Proposed rotations (priority order)

The engine gates every candidate on `bot->HasSpell(spellId)`, so listing a spell the
bot hasn't learned yet is harmless — it's skipped. Lists are unconditional damage
only (see Non-goals).

- **Hunter — Initial spec** (carried over from the spike, unchanged)
  1. Arcane Shot (`185358`)
  2. Steady Shot (`56641`)
- **Priest — Initial spec**
  1. Shadow Word: Pain (`589`) — DoT, apply first
  2. Smite (`585`) — direct-damage filler
- **Warrior — Initial spec**
  1. Slam (`1464`) — low-level damage ability; melee auto-attack carries the rest

### Values to verify against this build (67823)

Same VERIFY discipline the spike calls out — a wrong ID makes the data silently
inert (the spec's priority list stays empty and the bot just melees):

1. **Initial `ChrSpecializationID` per class.** Hunter's `1448` is **confirmed** by
   the existing spike. Candidates to confirm: **Priest `1452`**, **Warrior `1446`**
   (= `GetDefaultChrSpecializationForClass` → the class's `ChrSpecialization` row at
   the Initial `OrderIndex`).
2. **Spell IDs** are the correct, known-early spells for a sub-10 bot on this build.

Verification is at runtime via the cast-site DEBUG log (it prints the bot's actual
primary spec + the selected spell). **Correction is append-only:** because
`Updates.Redundancy=0` makes editing an already-applied migration unsafe, if a spec
id turns out wrong we add a *new* follow-up migration
(`2026_06_21_01_hotfixes.sql`) that `UPDATE`s the affected `assisted_combat` row —
we never edit the applied file. (In practice 1452/1446 are expected correct, so no
follow-up is likely.)

### Application & loading

- The migration lives in the auto-applied update tree
  (`sql/updates/hotfixes/master/`, `Updates.EnableDatabases` includes hotfixes).
  The worldserver entrypoint runs the updater **before** the server loads DB2
  stores, so on a single `docker compose up -d`:
  1. the updater applies `2026_06_21_00_hotfixes.sql` to the hotfixes DB, then
  2. `DB2Manager` loads `.db2` stores and applies the `Status = 1` hotfix rows into
     `sAssistedCombatStore` / `sAssistedCombatStepStore`, where the bot reads them.
- The tables already exist (created by `2026_06_19_00_hotfixes.sql`, which sorts
  earlier so it applies first).
- **Removed:** `sql/custom/spike_assisted_combat_hunter_lowlevel.sql` — its Hunter
  rows are now in the migration; the manual file is obsolete.

## Verification / success criteria

1. After building from this branch and `docker compose up -d`, the migration
   auto-applies; spawn sub-10 Hunter, Priest, and Warrior bots, group + follow a
   master, and engage a mob.
2. The cast-site DEBUG log prints, e.g., `Bot 'Alvaro' rotation: spec 1452, spell
   589.` — confirming the Initial spec id and that the rotation fired.
3. Each bot casts its listed abilities (observe the target's debuffs/health); a bot
   that hasn't learned a listed spell falls back to melee with no error spam (the
   `HasSpell` gate).
4. If a spec id in the log differs from the migration, add the append-only
   correction migration and restart.

## Files touched

- **New:** `sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql`
- **Removed:** `sql/custom/spike_assisted_combat_hunter_lowlevel.sql`
- **Modified:** `src/server/scripts/Custom/Bots/BotMgr.cpp` — one permanent DEBUG log
  at the `BotRotation::SelectSpell` cast site (diagnostics + spec-ID verification).
- **Modified:** `CHANGELOG-custom.md` — record the migration (Hunter/Priest/Warrior
  low-level bot rotation data, auto-applied) and the removal of the spike file.
