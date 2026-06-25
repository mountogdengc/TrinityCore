# Condition-aware bot rotation — "don't-waste-casts"

**Date:** 2026-06-24
**Status:** approved design, pre-implementation
**Area:** player-bots M4 rotation engine (`src/server/scripts/Custom/Bots/`)

## Problem

`BotRotation::SelectSpell` (the M4 rotation engine) resolves the highest-priority
*castable* Assisted Combat ability and the bot casts it each combat tick. As of the
`TRIGGERED_IGNORE_TARGET_CHECK` cast fix, bots do cast. But the engine honors only
the priority **order** gated on raw cast mechanics (known / off-cooldown / affordable
/ in range) — it does **not** evaluate any per-step *conditions*.

For abilities that are always affordable and have no cooldown, this means the top
ability is re-selected every GCD forever. Observed: the low-level Priest (spec 1452)
re-applies **Shadow Word: Pain (589)** every GCD and never casts **Smite (585)**,
because SWP is cheap, has no cooldown, and sits first in the priority list. The
Hunter only avoids this by luck (Arcane Shot costs focus, so the engine falls through
to Steady Shot when focus is low).

## Goal

Add a minimal, server-side **condition layer** the fork controls, so a candidate can
be skipped when casting it is pointless — primarily: do not re-cast a DoT that is
already active, but *do* refresh it shortly before it expires. This fixes the Priest
SWP-spam and lets fillers fire, and establishes the mechanism to extend later.

This scope is intentionally "don't-waste-casts" only. Health/resource/cooldown/
interrupt/AoE primitives and any decoding of Blizzard's retail opcodes are **out of
scope** (see below).

## Background / constraints

- The Assisted Combat data has three stores (loaded from client `*.db2` plus hotfix
  overrides): `sAssistedCombatStore` (container per spec), `sAssistedCombatStepStore`
  (ordered ability list), `sAssistedCombatRuleStore` (per-step `ConditionType` +
  `ConditionValue1-3`). All three externs are already exposed in `DB2Stores.h`.
- Blizzard's retail `AssistedCombatRule.db2` (~47 KB) populates the rule store for
  **max-level** specs, but its `ConditionType` values are **undocumented opcodes**.
  TrinityCore never decodes them (retail resolves Assisted Combat client-side).
- The fork's low-level custom specs are hotfix rows that ship **steps with no rules**
  (`sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql`): containers
  `1000001` (Hunter 1448), `1000010` (Priest 1452), `1000020` (Warrior 1446); steps
  `1000001/1000002` (Hunter), `1000010/1000011` (Priest), `1000020` (Warrior). All
  custom rows use IDs `>= 1000000`.

## Design

### 1. Collision-proof opcode handling

The engine interprets `ConditionType` as a **fork opcode only for our custom steps**
(`AssistedCombatStepID >= 1000000`). For Blizzard steps (from db2) it evaluates
nothing and treats the step as eligible — exactly today's behavior. Because we only
ever author rules on our own steps, our opcode values can never be confused with
Blizzard's, and max-level rotations cannot regress.

Fork opcode (a small enum in `BotRotation.cpp`), distinct base to stay readable in
SQL:

- `BOT_COND_TARGET_MISSING_OR_EXPIRING_MY_AURA = 1000`
  - `ConditionValue1` = aura spell id; `0` ⇒ the step's own `SpellID`.
  - `ConditionValue2` = refresh window in milliseconds (e.g. `3000`); `0` ⇒ treat as
    "missing only" (no refresh-ahead).
  - `ConditionValue3` = unused (reserved, `0`).
  - **Passes (candidate stays eligible) when:** the target does **not** have that aura
    applied *by this bot*, **or** the bot-applied aura's remaining duration is
    `< ConditionValue2` ms. Otherwise the candidate is **skipped**.

Any `ConditionType` on a custom step that is not a known fork opcode → treated as
**pass** (fail-open; never silently blocks a bot from acting).

### 2. Engine changes (`BotRotation.{h,cpp}`)

- `EnsureIndex()` additionally builds, from `sAssistedCombatRuleStore`, a map
  `stepId -> [ (ConditionType, v1, v2, v3) ]` for custom steps only, and changes the
  per-spec priority list element from `spellId` to `(spellId, stepId)` so a candidate
  can find its rules. De-dup still keeps the first (highest-priority) occurrence.
- New free function `bool EvaluateStepConditions(Player* bot, Unit* target,
  uint32 stepId, uint32 stepSpellId)`:
  - Returns `true` (eligible) if the step has no fork rules.
  - For each fork rule, evaluate the opcode; if any returns "skip", return `false`.
  - For `BOT_COND_TARGET_MISSING_OR_EXPIRING_MY_AURA`: resolve `auraId`
    (`v1 ? v1 : stepSpellId`), look up `target->GetAura(auraId, bot->GetGUID())`;
    eligible if the aura is absent, or `aura->GetDuration() >= 0 &&
    aura->GetDuration() < int32(v2)`.
- `SelectSpell()`: keep the existing castability gates, then add a final gate
  `if (!EvaluateStepConditions(...)) continue;` before `return spellId;`.

The cast site in `BotMgr` is unchanged (still casts with
`TRIGGERED_IGNORE_TARGET_CHECK`).

### 3. Rules we author (data)

New append-only hotfix migration `sql/updates/hotfixes/master/2026_06_24_00_hotfixes.sql`:

- Idempotent `DELETE` of the new rule IDs (same pattern as the steps migration).
- `assisted_combat_rule` row gating the Priest **SWP** step (`AssistedCombatStepID =
  1000010`): `ConditionType = 1000`, `ConditionValue1 = 0` (use step spell 589),
  `ConditionValue2 = 3000`, `ConditionValue3 = 0`, custom rule ID `>= 1000000`.
- `hotfix_data` rows (Status=1) for those rule rows, using the **AssistedCombatRule**
  `table_hash` (obtained from the db2 header / `DB2Manager` load, the same way the
  container hash `0xA4A21680` and step hash `0x790BCC4F` were obtained for the
  existing migration).
- Smite step (`1000011`) gets **no rule** → always-eligible filler.
- Hunter (1448) and Warrior (1446) get **no rules** in this slice — they already
  rotate correctly.

Result: the Priest casts SWP once, casts Smite as filler while SWP has `>= 3 s`
remaining, and refreshes SWP when it drops below 3 s.

## Data flow

1. Startup: db2 + hotfix rows populate the three Assisted Combat stores.
2. First bot tick: `EnsureIndex()` builds `spec -> [(spellId, stepId)]` and
   `customStepId -> [rules]`.
3. Each combat tick, `SelectSpell` walks the spec's priority list: castability gates →
   `EvaluateStepConditions` → first candidate passing all gates is returned.
4. `BotMgr` casts it with `TRIGGERED_IGNORE_TARGET_CHECK`; GCD pacing is unchanged.

## Files changed

- `src/server/scripts/Custom/Bots/BotRotation.cpp` — rule index, `EvaluateStepConditions`,
  opcode enum, `SelectSpell` gate. (`BotRotation.h` unchanged unless a helper is exposed.)
- `sql/updates/hotfixes/master/2026_06_24_00_hotfixes.sql` — new rule + hotfix_data rows.
- `CHANGELOG-custom.md` — document the condition layer + migration.

## Testing / verification

- Primary: in-game via the existing `bots` DEBUG rotation log. Expected Priest pattern:
  `spell 589` (SWP) → `spell 585` (Smite) repeated → `spell 589` again once SWP nears
  expiry. Hunter/Warrior patterns unchanged.
- The `EvaluateStepConditions` aura check is simple enough to leave to in-game
  verification; no new unit-test harness for bots exists and mocking aura/owner state
  is disproportionate for this slice. If a pure-logic seam is cheap to expose
  (opcode dispatch independent of `Player`/`Unit`), add a small unit test under
  `tests/`; otherwise rely on the debug log.
- The temporary `[CAST-DIAG]` rotation log added for the cast fix stays for the
  verification build, then is removed in a cleanup build (same pattern already used).

## Out of scope (deferred)

- Health-, resource-, cooldown-, interrupt-, and AoE-based primitives.
- Decoding Blizzard's retail `AssistedCombatRule` `ConditionType` opcodes / gating
  max-level rotations (kept as fail-open "pass").
- Rules for Hunter/Warrior (current behavior already acceptable).

## Risks / notes

- Append-only migrations (`Updates.Redundancy=0`): never edit an applied hotfix file;
  add a new `_NN` file to correct a value.
- Fail-open is deliberate: an unknown or malformed condition must never make a bot
  stop acting — worst case it behaves like today (no gating).
