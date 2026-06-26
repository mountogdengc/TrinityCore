# Hunter bot pets

**Date:** 2026-06-26
**Status:** approved design, pre-implementation
**Area:** player-bots (`src/server/scripts/Custom/Bots/`)

## Problem

Hunter bots fight with no pet. A real Hunter's pet is a large fraction of their damage
(and their melee presence), so a petless Hunter bot is doing roughly half a Hunter's
job. Freshly-created Hunter bots have **no tamed pet at all** (no `character_pet` row),
so there is nothing to "call" — we must give them one.

## Goal

Every **Hunter** bot summons and maintains a real `HUNTER_PET` that:
- fights the bot's target (assist),
- levels with the bot,
- comes back after it dies, after the bot zones, or if it's otherwise missing,
- despawns cleanly when the bot is removed.

Server-side only (the null-socket bot needs no client packets). Minimal and contained to
`BotMgr` + one new pure policy file. Hunters only this pass.

## Key engine facts (validated)

- **Create a real hunter pet** with `Unit::CreateTamedPetFrom(entry, spellId=0)` (the
  tame path → `HUNTER_PET`), then the `EffectTameCreature` sequence: `GivePetLevel(owner
  level)` → `pet->GetMap()->AddToMap(pet)` → `SetMinion(pet, true)` →
  `SetReactState(REACT_DEFENSIVE)` → `SavePetToDB(PET_SAVE_AS_CURRENT)`.
  `Player::SummonPet` is the *warlock* path (creates a temporary `SUMMON_PET`) — not what
  we want.
- **Pet entry must be a tameable beast:** `creature_template.type == CREATURE_TYPE_BEAST`,
  a real `family` (not `CREATURE_FAMILY_NONE`), and the `CREATURE_TYPE_FLAG_TAMEABLE` flag.
- **Combat is automatic:** a `REACT_DEFENSIVE` pet's `PetAI::SelectNextTarget` attacks the
  owner's victim / attackers, and follows the owner out of combat (`PET_FOLLOW_DIST`).
  Our bot already sets `GetVictim()` in combat (ranged bots via `Attack(target,false)`),
  so the pet engages the bot's target with no extra wiring.
- **Hunter pets do NOT auto-sync level** — must call `Pet::GivePetLevel(ownerLevel)`.
- **Cross-map teleport unsummons the pet** (`UnsummonPetTemporaryIfAny`); it must be
  resummoned after the bot zones.
- **No auto-revive on pet death** — a dead `HUNTER_PET` corpse persists; we must revive it.
- The bot session is a real `WorldSession` with a null socket, so `PetSpellInitialize()`
  (UI packet) is harmless but pointless — we skip it.

## Design

### 1. Pure decisions (`BotPetPolicy`, unit-tested)

```cpp
namespace BotPetPolicy
{
    // A living, in-world Hunter bot with no pet should summon one.
    bool ShouldSummonPet(bool isHunter, bool inWorld, bool botAlive, bool petExists);
    // A dead pet should be revived once the dead timer passes the revive delay.
    bool ShouldRevivePet(bool petExists, bool petAlive, int32 deadMs, int32 reviveDelayMs);
    // Hunter pets don't auto-sync; resync when levels differ.
    bool NeedsLevelSync(uint8 petLevel, uint8 ownerLevel);
}
```

### 2. `BotMgr::EnsureHunterPet` — one reconciliation routine

Called each follow tick for every Hunter bot, throttled by the existing follow interval.
It reconciles pet state using the policy:

- **No pet** (`ShouldSummonPet`): summon one (initial spawn / post-teleport / post-death
  cleanup). Reset `petDeadTimer = 0`.
- **Dead pet** (`ShouldRevivePet`, accruing `petDeadTimer` by the tick interval): remove
  the corpse (`RemovePet(pet, PET_SAVE_NOT_IN_SLOT)`) and summon a fresh one at full
  health (the "Revive Pet"). Uses the existing `Custom.BotAutoReviveDelayMs` (default
  5000 ms) so it matches the bot's own auto-revive timing.
- **Living pet** (`NeedsLevelSync`): `pet->GivePetLevel(bot->GetLevel())`.

### 3. Summon = nearest tameable beast (engine glue in `BotMgr`)

`BotMgr::SummonBotPet(bot)`:
1. Pick the entry: `BotMgr::PickTameableBeastEntry(bot)` scans the bot's grid for the
   nearest `Creature` whose template is a tameable beast (type BEAST + tameable flag +
   real family). If none found, use a fixed default beast entry (`BOT_DEFAULT_PET_ENTRY`,
   a known low-level tameable beast). Cache the result on `BotEntry::petEntry` so revives
   reuse the same beast.
2. Run the tame sequence above with that entry, at `bot->GetLevel()`.
3. Skip `PetSpellInitialize()` (UI only).

### 4. Cleanup on `.bot remove`

In `BotMgr::RemoveBot` (before the session/player is torn down), if the bot has a pet,
`bot->RemovePet(pet, PET_SAVE_NOT_IN_SLOT)` so it despawns and isn't persisted. (Player
logout also handles pets, but being explicit avoids a stray pet and a surprising save.)

### 5. State on `BotEntry`

- `uint32 petEntry = 0;` — cached chosen beast entry (0 = not yet picked).
- `uint32 petDeadTimer = 0;` — ms the pet has been dead (drives the revive delay).

## Data flow

1. Hunter bot spawns → first follow tick → `EnsureHunterPet` → no pet → `SummonBotPet`
   (nearest tameable beast at bot level) → `PetAI` follows.
2. Combat: bot acquires a victim → pet (`REACT_DEFENSIVE`) assists automatically.
3. Pet dies → `petDeadTimer` accrues → after `BotAutoReviveDelayMs` → fresh pet summoned.
4. Bot zones cross-map → core unsummons pet → next tick `EnsureHunterPet` resummons it.
5. Bot levels up → `NeedsLevelSync` → `GivePetLevel`.
6. `.bot remove` → pet despawned.

## Testing

- **Unit (CI):** `ShouldSummonPet`, `ShouldRevivePet`, `NeedsLevelSync` truth tables.
- **In-game:**
  - A Hunter bot summons a zone-appropriate pet shortly after spawning.
  - The pet attacks the bot's target and follows out of combat.
  - Kill the pet → it revives after ~5 s.
  - Zone the bot cross-map → the pet reappears at the destination.
  - Level the bot → pet level follows.
  - `.bot remove` despawns the pet with **no worldserver crash** (verify the pet save
    path is clean — distinct from the known `.character level` talent-state crash).

## Out of scope (follow-ups)

- Pet special abilities / a pet ability rotation (pet melees + whatever `PetAI`
  auto-casts by default).
- Choosing or taming a *specific* pet via command; pet families/talents/specs.
- Pet happiness/loyalty (retail removed it); the stable.
- Non-Hunter pets (warlock/DK/mage/etc.).

## Risks / notes

- **Default fallback entry** must be a verified tameable beast that exists in the 12.0.5
  world DB; pick and confirm during implementation (e.g. a common low-level wolf/boar).
- **Save-path crash:** adding a pet exercises the bot's character save on `.bot remove`;
  must be tested. If it crashes, despawn the pet earlier in the removal sequence.
- **Grid scan cost:** `PickTameableBeastEntry` runs only when summoning (rare, cached),
  not per tick.
- **Revive cadence:** reusing `BotAutoReviveDelayMs` keeps the pet from perma-dying mid
  fight without instant-reviving; acceptable for a first pass.
