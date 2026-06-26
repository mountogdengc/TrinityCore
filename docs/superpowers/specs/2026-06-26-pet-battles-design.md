# Pet Battles — wild pet battle system (scoping / roadmap)

**Date:** 2026-06-26
**Status:** scoping / roadmap — pre-implementation. No code yet. This document
breaks the feature into shippable milestones; each milestone gets its own
`plans/` doc when it is picked up.
**Area:** new module `src/server/game/BattlePets/` (battle engine) on top of the
existing journal `BattlePetMgr`.

## Problem

Wild critters (`CREATURE_TYPE_CRITTER`) spawn correctly — ~32k of them across the
world — but the player cannot **pet-battle** them: no green paw icon, and clicking
one does nothing. This is **not a bug**. The TrinityCore master this fork tracks
**has never implemented the Pet Battle system**:

- Every client→server battle opcode is `STATUS_UNHANDLED → WorldSession::Handle_NULL`
  (`src/server/game/Server/Protocol/Opcodes.cpp:849–858`), including
  `CMSG_PET_BATTLE_REQUEST_WILD` (sent when you click a wild pet) and
  `CMSG_PET_BATTLE_INPUT` (the per-turn action).
- None of the 15 `SMSG_PET_BATTLE_*` flow packets have packet structs in
  `BattlePetPackets.{h,cpp}` — the server has no way to *drive* a battle.
- The core never sets `UNIT_NPC_FLAG_WILD_BATTLE_PET` (the green-paw flag); it only
  ever *removes* it (`TemporarySummon.cpp:258`). So critters show no paw unless the
  flag is in the DB `npcflag`, and even then the click goes nowhere.

What *does* exist is the **journal/collection** half only (`BattlePetMgr`): owning,
caging, leveling, healing, slotting, and summoning-as-companion pets you already have.

## Goal

A player can walk up to a wild battle pet, start a battle, fight a turn-based 3v3
(or fewer) match using their slotted pets' abilities, **capture** a defeated wild
pet into their journal, and earn pet XP — server-authoritative, persisted. Tamer
NPC (PvE trainer) battles fall out of the same engine. **PvP queued battles are out
of scope for the initial system** (matchmaking is a separable, larger lift).

This is one of the largest unimplemented WoW subsystems. It is intentionally staged
so each milestone is independently testable and useful.

## Why this is big (honest scope)

A pet battle is a self-contained turn-based RPG bolted onto the MMO:

- A **battle state machine** the server fully owns (the open-world units are frozen
  and a parallel "pet battle" context runs), driving the client through a fixed
  packet handshake.
- **Ability resolution**: each pet ability is a small script (damage/heal/aura/
  weather/swap/multi-turn) defined across `BattlePetAbility*` DB2 tables, with
  type-effectiveness (the 10-family strong/weak matrix), hit chance, priority/speed
  ordering, and state (`BattlePetState`) modifiers.
- **Breeds/quality/stats**: a pet's Health/Power/Speed derive from species base
  states × breed states × quality × level (`BattlePetBreedState`,
  `BattlePetSpeciesState`, `BattlePetBreedQuality`).
- **Capture**, **forfeit**, **swap**, **trap**, **death/round** rules.
- **Persistence** of captured pets, XP, and post-battle health into the journal DB.

## Key engine facts (validated against the tree)

### Already present (build on these)
- **Journal manager** `src/server/game/BattlePets/BattlePetMgr.{h,cpp}` — per-session
  owner of the player's pets and battle slots (`BattlePetSlot`, up to 3), with
  `AddPet`, `CageBattlePet`, `GrantBattlePetExperience/Level`, `HealBattlePetsPct`,
  `SummonPet`, `RollPetBreed`, `GetDefaultPetQuality`, `SelectPetDisplay`.
- **Species lookup** `BattlePetMgr::GetBattlePetSpeciesByCreature(creatureId)` maps a
  wild creature entry → `BattlePetSpeciesEntry`. This is what turns a clicked rabbit
  into a battle-pet species.
- **Wild-pet unit plumbing**: `UNIT_NPC_FLAG_WILD_BATTLE_PET` (0x40000000),
  `Unit::IsWildBattlePet()`, the `WildBattlePetLevel` unit field, and
  `Creature::SelectWildBattlePetLevel()` (rolls a level from
  `AreaTable.WildBattlePetLevelMin/Max`). The wild pet's *identity and level* are
  already representable on the open-world creature.
- **DB2 stores loaded today**: `sBattlePetSpeciesStore`, `sBattlePetAbilityStore`,
  `sBattlePetBreedQualityStore`, `sBattlePetBreedStateStore`,
  `sBattlePetSpeciesStateStore` (`DataStores/DB2Stores.cpp`).
- **DB2-extern precedent**: the fork already added `extern` decls + loads for the
  AssistedCombat stores (`DB2Stores.h`, see `CHANGELOG-custom.md` → *Code
  modifications to upstream files*). The same pattern adds the missing ability-effect
  stores below.

### Missing (must be built)
- **Ability-resolution DB2 stores are NOT loaded**: `BattlePetAbilityEffect`,
  `BattlePetAbilityState`, `BattlePetAbilityTurn`, `BattlePetState`. Without these the
  server cannot know what an ability *does* per turn. Add them to `DB2Stores.{h,cpp}`
  (declare, load, index) — mechanical but a prerequisite for M3+.
- **All 15 `SMSG_PET_BATTLE_*` packets** (no structs exist): `FINALIZE_LOCATION`,
  `INITIAL_UPDATE`, `FIRST_ROUND`, `ROUND_RESULT`, `REPLACEMENTS_MADE`, `FINAL_ROUND`,
  `FINISHED`, `REQUEST_FAILED`, `SLOT_UPDATES`, plus the queue/PvP ones (deferred).
- **All 10 `CMSG_PET_BATTLE_*` handlers** (today `Handle_NULL`): `REQUEST_WILD`,
  `REQUEST_UPDATE`, `INPUT`, `REPLACE_FRONT_PET`, `FINAL_NOTIFY`, `QUIT_NOTIFY`,
  `SCRIPT_ERROR_NOTIFY`, `WILD_LOCATION_FAIL` (+ PvP, deferred).
- **The battle engine itself** — no `PetBattle` class/state machine exists.

## Architecture

```
src/server/game/BattlePets/
  BattlePetMgr.{h,cpp}        (exists — journal; gains capture/XP write-back hooks)
  PetBattle.{h,cpp}           (new — one live battle: teams, rounds, turn resolution)
  PetBattleData.{h,cpp}       (new — combatant snapshot: stats from species×breed×quality×level)
  PetBattleAbility.{h,cpp}    (new — ability effect interpreter over the *Ability* DB2s)
  PetBattleSystem.{h,cpp}     (new — global registry of active battles + location/finalize)
  (pure helpers)              (new — type-effectiveness, hit chance, stat calc; unit-tested)
```

- A **`PetBattle`** is created by `CMSG_PET_BATTLE_REQUEST_WILD`. It snapshots the
  player's slotted pets (from `BattlePetMgr`) and the wild creature's species/level
  into `PetBattleData` combatants, freezes the participants in the world, and drives
  the client via the `SMSG_PET_BATTLE_*` handshake.
- **Server-authoritative**: the client only sends `CMSG_PET_BATTLE_INPUT` (chosen
  action for the round); the server resolves both sides and replies with
  `SMSG_PET_BATTLE_ROUND_RESULT`. The client never decides outcomes.
- Pure decision/calc helpers (type chart, hit roll, damage, stat derivation) live in
  free functions so they are unit-tested in `tests/game/` like the bot policies.

## Protocol handshake (the happy path to implement first)

```
client clicks wild pet
  → CMSG_PET_BATTLE_REQUEST_WILD            (M2: handle; validate range/level/slots)
  ← SMSG_PET_BATTLE_FINALIZE_LOCATION       (M2: position the battlefield)
  ← SMSG_PET_BATTLE_INITIAL_UPDATE          (M2: both teams, pets, abilities)
  ← SMSG_PET_BATTLE_FIRST_ROUND             (M3: round 0 state)
loop:
  → CMSG_PET_BATTLE_INPUT                    (M3: ability / swap / trap / pass / forfeit)
  ← SMSG_PET_BATTLE_ROUND_RESULT            (M3: resolved effects, deaths, ordering)
  [→ CMSG_PET_BATTLE_REPLACE_FRONT_PET / ← SMSG_PET_BATTLE_REPLACEMENTS_MADE on death]
  ← SMSG_PET_BATTLE_FINAL_ROUND             (M3/M4: terminal round)
  → CMSG_PET_BATTLE_FINAL_NOTIFY            (M4: client ack)
  ← SMSG_PET_BATTLE_FINISHED                (M4: results)
  [capture/XP/heal write-back to journal]   (M4)
  → CMSG_PET_BATTLE_QUIT_NOTIFY             (M4: teardown, unfreeze world)
```

`SMSG_PET_BATTLE_REQUEST_FAILED` covers every reject (out of range, no slotted pets,
already in battle, pet on cooldown, wild pet already engaged).

## Milestones

- **M1 — Make wild pets *appear* battle-ready (data + flag, no gameplay).**
  Set `UNIT_NPC_FLAG_WILD_BATTLE_PET` on the creature entries that map to a
  `BattlePetSpecies` (cross-reference `GetBattlePetSpeciesByCreature`), and ensure
  `SelectWildBattlePetLevel()` runs for them so they carry a level. Result: the green
  paw shows and pets display a level. **Clicking still fails gracefully** (M2 gates it
  with `REQUEST_FAILED` instead of a dropped packet). *Shippable, low-risk, visible.*

- **M2 — Battle setup handshake (no combat yet).**
  Add `BattlePetSystem` + `PetBattle` skeleton, the `PetBattleData` stat snapshot
  (species×breed×quality×level → Health/Power/Speed), the missing ability-effect DB2
  loads, and the structs/handlers for `REQUEST_WILD → FINALIZE_LOCATION →
  INITIAL_UPDATE`. The battle opens, both teams render, then we cleanly forfeit
  (`FINISHED`). Proves the freeze/teardown and the slot snapshot. *Testable: a battle
  UI opens and closes without desync or crash.*

- **M3 — Turn resolution (the actual battle).**
  Implement `PetBattleAbility` over `BattlePetAbility{,Effect,State,Turn}`: a round =
  collect both inputs, order by speed/priority, roll hit, apply effects (damage/heal/
  auras), apply the **family type-effectiveness** matrix and `BattlePetState`
  modifiers, process deaths and front-pet replacement, emit `ROUND_RESULT`. Loop to
  `FINAL_ROUND` on a team wipe. *Testable: a winnable/losable PvE fight end-to-end.*

- **M4 — Outcomes: capture, XP, persistence, heal.**
  `FINAL_NOTIFY → FINISHED`: grant pet XP (`GrantBattlePetExperience`), apply trap
  **capture** of a weakened wild pet into the journal (`AddPet`), persist post-battle
  pet health, and heal/forfeit paths. Hook the journal write-backs and DB saves.
  *Testable: capture a rabbit; it's in your journal after relog; XP sticks.*

- **M5 — PvE tamer battles + polish.**
  Reuse the engine for static **Pet Tamer** NPCs (scripted 3-pet teams), weather/
  field effects, multi-turn/charge abilities, ability cooldowns, the
  max-game-length warning, and edge cases (both-die, swap-on-death timing).

- **M6 (deferred) — PvP queued battles.**
  `REQUEST_PVP` + the `QUEUE_*` matchmaking opcodes. Separable; only after PvE is
  solid.

## Combat resolution model (M3 detail)

- **Stats**: `Health = f(speciesState, breedState, quality, level)`;
  `Power`/`Speed` likewise. Pure function over the four DB2 inputs; unit-tested.
- **Round**: both sides commit one action; resolve in **speed order** (ties broken by
  a deterministic rule), with ability **priority** overriding speed. Each ability =
  ordered `BattlePetAbilityTurn` → `BattlePetAbilityEffect` rows interpreted by
  `PetBattleAbility`.
- **Type chart**: 10 pet families, the canonical strong/weak ±50% matrix → a pure
  `PetBattleTypeMultiplier(attackType, defendType)`.
- **Hit**: accuracy from the ability minus dodge/state; one RNG roll per effect.
  (RNG is server-side; `Math.random` is fine in core, unlike the workflow sandbox.)
- **Death/replace**: on front-pet death, server requests a swap
  (`REPLACEMENTS_MADE`); if a side has no pets left → terminal.

## Files to add / change

- **New**: the `PetBattle*` module files above; new `SMSG/CMSG PetBattle*` structs in
  `BattlePetPackets.{h,cpp}`; pure-helper headers + `tests/game/PetBattle*.cpp`.
- **Change (track in CHANGELOG → *Code modifications to upstream files*)**:
  - `Server/Protocol/Opcodes.cpp` — route the 10 `CMSG_PET_BATTLE_*` from
    `Handle_NULL` to real handlers.
  - `Server/WorldSession.{h,cpp}` — handler declarations + a `_petBattle` pointer.
  - `DataStores/DB2Stores.{h,cpp}` — load `BattlePetAbilityEffect/State/Turn`,
    `BattlePetState` (AssistedCombat extern pattern).
  - `BattlePets/BattlePetMgr.{h,cpp}` — capture/XP/health write-back entry points.
  - Wild-pet `npcflag`/level: `sql/updates/world/master/` data update (M1) +
    possibly `Creature.cpp` to auto-apply the paw flag to mapped species at spawn.

## Testing / verification

- **Unit (CI, `tests/game/`)**: stat derivation, type-effectiveness matrix, hit/damage
  rolls (seeded), speed/priority ordering, capture eligibility, XP curve — all pure
  functions, mirroring the bot-policy test style.
- **In-game, per milestone**: M1 paw icon + level; M2 battle opens/closes clean; M3 a
  full PvE fight resolves correctly with type bonuses; M4 capture persists across
  relog and XP sticks; M5 a tamer NPC fight.
- **Stability**: battle entry/exit must freeze and restore world units with no leak or
  crash (lesson from the bot teleport/save-path work — exercise teardown hard).

## Out of scope (initial system)

- PvP queued/challenge battles (M6) and the whole `QUEUE_*`/`REQUEST_PVP` matchmaker.
- Pet leveling *bands*, rare spawn variants, and battle-pet world quests.
- Account-wide vs per-character pet sharing changes (journal already exists as-is).
- Mobile/armory parity.

## Risks / notes

- **DB2 completeness**: the client (12.0.5 / build 67823) extract must actually contain
  the `BattlePetAbility{Effect,State,Turn}` and `BattlePetState` tables; confirm during
  M2 before building M3 on them. If absent, abilities can't be data-driven and the
  scope balloons (hand-authoring ability scripts).
- **Packet shapes**: the `SMSG_PET_BATTLE_*` layouts for this build must be reversed
  from the client / referenced from a core that implemented them; a wrong layout
  desyncs the client silently. Budget real time for M2/M3 packet bring-up.
- **World freeze**: a battle suspends the open-world participants; interaction with
  combat, movement, phasing, and logout-mid-battle all need explicit teardown.
- **Effort honesty**: M1 is hours; M2 days; M3 is the bulk (1–2+ weeks); M4–M5 another
  large chunk. This is a milestone program, not a single PR.
```
