# Hunter bot pets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hunter bots summon and maintain a real `HUNTER_PET` (nearest tameable beast) that fights the bot's target, levels with the bot, revives after death, resummons after zoning, and despawns on `.bot remove`.

**Architecture:** A pure `BotPetPolicy` (summon/revive/level-sync decisions, unit-tested) + engine glue in `BotMgr` (pick nearest tameable beast, run the proven tame sequence, one `EnsureHunterPet` reconciliation routine driven from the follow tick, cleanup in `RemoveBot`). Combat/follow come free from `PetAI` (a `REACT_DEFENSIVE` pet assists the owner's victim).

**Tech Stack:** C++ (TrinityCore master), Catch2 unit tests, Docker Compose build/run, in-game verification.

**Spec:** `docs/superpowers/specs/2026-06-26-bot-hunter-pets-design.md`

**Verified facts:**
- `Pet* Unit::CreateTamedPetFrom(uint32 creatureEntry, uint32 spell_id = 0)` (Unit.h:1262) — the tame path → `HUNTER_PET`.
- Working reference sequence: `Spell::EffectTameCreature` (SpellEffects.cpp:2637-2690).
- `CreatureTemplate::IsTameable(bool canTameExotic, CreatureDifficulty const*)` (CreatureData.h:552); on a live creature use `c->GetCreatureTemplate()->IsTameable(false, c->GetCreatureDifficulty())` (`Creature::GetCreatureDifficulty()` Creature.h:268).
- `WorldObject::GetCreatureListWithOptionsInGrid(Container&, float range, FindCreatureOptions const&)` (Object.h:503); `FindCreatureOptions` (Object.h:231) has `IsAlive` (`FindCreatureAliveState`).
- `Pet::GivePetLevel(uint8)` (Pet.h:83); `Player::GetPet()` (Player.h:1365); `Player::RemovePet(Pet*, PetSaveMode, bool=false)` (Player.h:1367); `Pet::isDead()`/`IsAlive()`.
- Revive delay config: `CONFIG_CUSTOM_BOT_AUTO_REVIVE_DELAY_MS` (World.h:453, default 5000). Used for the *delay value only* — pet revive is NOT gated by the `Custom.BotAutoRevive` enable flag.
- Default fallback beast: **entry 299 "Young Wolf"** (verified `CREATURE_TYPE_BEAST`, family Wolf, TAMEABLE).

---

## File structure

- **Create** `src/server/scripts/Custom/Bots/BotPetPolicy.{h,cpp}` — pure decisions.
- **Create** `tests/game/BotPetPolicy.cpp`; **modify** `tests/CMakeLists.txt`.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.h` — `petEntry`/`petDeadTimer` on `BotEntry`; declare `EnsureHunterPet`, `SummonBotPet`, `PickTameableBeastEntry`.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.cpp` — constants, the three methods, call `EnsureHunterPet` from `UpdateFollow`, pet cleanup in `RemoveBot`.
- **Modify** `CHANGELOG-custom.md` — feature entry.

---

## Task 1: Pure pet policy (TDD)

**Files:**
- Create: `src/server/scripts/Custom/Bots/BotPetPolicy.h`, `.cpp`
- Test: `tests/game/BotPetPolicy.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Header** — create `src/server/scripts/Custom/Bots/BotPetPolicy.h`:

```cpp
/*
 * Player-bot pet policy helpers (pure, unit-tested).
 */

#ifndef TRINITYCORE_BOTS_BOTPETPOLICY_H
#define TRINITYCORE_BOTS_BOTPETPOLICY_H

#include "Define.h"

namespace BotPetPolicy
{
    // A living, in-world Hunter bot with no pet should summon one.
    bool ShouldSummonPet(bool isHunter, bool inWorld, bool botAlive, bool petExists);

    // A dead pet should be revived once it has been dead at least reviveDelayMs.
    bool ShouldRevivePet(bool petExists, bool petAlive, int32 deadMs, int32 reviveDelayMs);

    // Hunter pets don't auto-sync level; resync when the pet's level differs from the owner.
    bool NeedsLevelSync(uint8 petLevel, uint8 ownerLevel);
}

#endif // TRINITYCORE_BOTS_BOTPETPOLICY_H
```

- [ ] **Step 2: Failing test** — create `tests/game/BotPetPolicy.cpp`. Copy the GPL license header from `tests/game/BotMovementPolicy.cpp`, then:

```cpp
#include "tc_catch2.h"

#include "BotPetPolicy.h"

TEST_CASE("ShouldSummonPet only for a live in-world hunter with no pet", "[BotPetPolicy]")
{
    using BotPetPolicy::ShouldSummonPet;
    REQUIRE(ShouldSummonPet(true, true, true, false));
    REQUIRE_FALSE(ShouldSummonPet(true, true, true, true));    // already has a pet
    REQUIRE_FALSE(ShouldSummonPet(false, true, true, false));  // not a hunter
    REQUIRE_FALSE(ShouldSummonPet(true, false, true, false));  // not in world
    REQUIRE_FALSE(ShouldSummonPet(true, true, false, false));  // dead bot
}

TEST_CASE("ShouldRevivePet once a dead pet passes the revive delay", "[BotPetPolicy]")
{
    using BotPetPolicy::ShouldRevivePet;
    REQUIRE_FALSE(ShouldRevivePet(false, false, 9999, 5000));  // no pet
    REQUIRE_FALSE(ShouldRevivePet(true, true, 9999, 5000));    // pet alive
    REQUIRE_FALSE(ShouldRevivePet(true, false, 4999, 5000));   // not yet
    REQUIRE(ShouldRevivePet(true, false, 5000, 5000));         // exactly at delay
    REQUIRE(ShouldRevivePet(true, false, 8000, 5000));         // past delay
}

TEST_CASE("NeedsLevelSync when levels differ", "[BotPetPolicy]")
{
    using BotPetPolicy::NeedsLevelSync;
    REQUIRE_FALSE(NeedsLevelSync(10, 10));
    REQUIRE(NeedsLevelSync(9, 10));
    REQUIRE(NeedsLevelSync(11, 10));
}
```

- [ ] **Step 3: Register in `tests/CMakeLists.txt`** — add to the `target_sources(tests PRIVATE ...)` Bots block, alphabetically (after `BotMovementPolicy.cpp`, before `BotRangedAttackPolicy.cpp`):

```cmake
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotPetPolicy.cpp
```

- [ ] **Step 4: Implementation** — create `src/server/scripts/Custom/Bots/BotPetPolicy.cpp`:

```cpp
/*
 * Player-bot pet policy helpers -- see BotPetPolicy.h.
 */

#include "BotPetPolicy.h"

namespace BotPetPolicy
{
bool ShouldSummonPet(bool isHunter, bool inWorld, bool botAlive, bool petExists)
{
    return isHunter && inWorld && botAlive && !petExists;
}

bool ShouldRevivePet(bool petExists, bool petAlive, int32 deadMs, int32 reviveDelayMs)
{
    return petExists && !petAlive && deadMs >= reviveDelayMs;
}

bool NeedsLevelSync(uint8 petLevel, uint8 ownerLevel)
{
    return petLevel != ownerLevel;
}
}
```

- [ ] **Step 5: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotPetPolicy.h src/server/scripts/Custom/Bots/BotPetPolicy.cpp \
        tests/game/BotPetPolicy.cpp tests/CMakeLists.txt
git commit -m "feat(bots): pure pet policy (summon/revive/level-sync decisions)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

(No Docker build; the `tests` target is CI-verified.)

---

## Task 2: BotEntry state + method declarations

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.h`

- [ ] **Step 1: Add fields to `BotEntry`** — after the existing `rangedAutoChecked` field:

```cpp
        uint32        rangedAutoSpellId = 0; // cached autorepeat ranged spell (Auto Shot / wand Shoot); 0 = none
        bool          rangedAutoChecked = false; // true once we've scanned this bot's spells for the above
        uint32        petEntry = 0;          // cached chosen tameable-beast entry (0 = not yet picked)
        uint32        petDeadTimer = 0;       // ms the hunter pet has been dead (drives revive delay)
```

(Read the current struct first; append the two new fields after `rangedAutoChecked`.)

- [ ] **Step 2: Declare the methods** — in the private section near `SelectAssistTarget` / `FindRangedAutoAttackSpell` declarations:

```cpp
    // Hunter pets: reconcile pet state (summon / revive / level-sync) each follow tick.
    void EnsureHunterPet(Player* bot, BotEntry& entry);
    // Summon a HUNTER_PET of `entry` for the bot at the bot's level (the tame sequence).
    void SummonBotPet(Player* bot, uint32 entry);
    // Nearest tameable beast to the bot, or BOT_DEFAULT_PET_ENTRY if none in range.
    uint32 PickTameableBeastEntry(Player* bot);
```

- [ ] **Step 3: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.h
git commit -m "feat(bots): BotEntry pet state + pet method declarations

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Pick beast + summon pet (engine glue)

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`

- [ ] **Step 1: Includes + constants**

Add includes needed for the pet/grid work (place with the existing includes; confirm each resolves, add only if missing): `"Pet.h"`, `"Creature.h"`, `"CreatureData.h"`, `"GridNotifiers.h"`, `"GridNotifiersImpl.h"`, `"CellImpl.h"`. To get the include set right for `GetCreatureListWithOptionsInGrid`, open an existing caller (grep the tree for `GetCreatureListWithOptionsInGrid`) and match its includes.

In the anonymous-namespace constants block (with `BOT_FOLLOW_DIST`, `BOT_RANGED_DIST`), add:

```cpp
    constexpr uint32 BOT_DEFAULT_PET_ENTRY  = 299;   // Young Wolf -- verified tameable beast fallback
    constexpr float  BOT_PET_SCAN_RANGE     = 40.0f; // how far to look for a tameable beast to "tame"
```

- [ ] **Step 2: `PickTameableBeastEntry`** — add to `BotMgr.cpp`:

```cpp
uint32 BotMgr::PickTameableBeastEntry(Player* bot)
{
    std::vector<Creature*> nearby;
    FindCreatureOptions opts;
    opts.IsAlive = FindCreatureAliveState::Alive;
    bot->GetCreatureListWithOptionsInGrid(nearby, BOT_PET_SCAN_RANGE, opts);

    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* c : nearby)
    {
        CreatureTemplate const* tmpl = c->GetCreatureTemplate();
        if (!tmpl || !tmpl->IsTameable(false, c->GetCreatureDifficulty()))
            continue;
        float const d = bot->GetExactDist(c);
        if (!best || d < bestDist)
        {
            best = c;
            bestDist = d;
        }
    }
    return best ? best->GetEntry() : BOT_DEFAULT_PET_ENTRY;
}
```

- [ ] **Step 3: `SummonBotPet`** — mirror `Spell::EffectTameCreature` (SpellEffects.cpp:2637-2690), entry overload, skipping the target-despawn and `PetSpellInitialize` (UI-only). Add:

```cpp
void BotMgr::SummonBotPet(Player* bot, uint32 entry)
{
    if (!entry || bot->GetPet())
        return;

    Pet* pet = bot->CreateTamedPetFrom(entry, 0);
    if (!pet)
        return;

    uint8 const level = bot->GetLevel();
    pet->SetLevel(level > 1 ? level - 1 : level);   // matches the tame flow's pre-add level
    pet->GetMap()->AddToMap(pet->ToCreature());
    pet->SetLevel(level);
    bot->SetMinion(pet, true);
    pet->GivePetLevel(level);                        // init stats/spells for the bot's level
    pet->SetReactState(REACT_DEFENSIVE);             // assist the owner's target, don't auto-aggro
    pet->SavePetToDB(PET_SAVE_AS_CURRENT);
    // (Skip PetSpellInitialize -- that only sends the pet action bar to a real client.)
    TC_LOG_DEBUG("bots", "Bot '{}' summoned pet entry {} at level {}.", bot->GetName(), entry, uint32(level));
}
```

Read `EffectTameCreature` first and confirm this ordering matches it (especially that `CreateTamedPetFrom`/`SetLevel`/`GivePetLevel` leave the pet at the bot's level with initialized stats). If the entry overload differs from the `Creature*` overload in a way that breaks this, report it (status DONE_WITH_CONCERNS) rather than guessing.

- [ ] **Step 4: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): pick nearest tameable beast + summon a hunter pet for bots

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Reconcile per tick + wire into the loop + cleanup

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`

- [ ] **Step 1: Include the policy header** — add (alphabetically among `Bot*.h`): `#include "BotPetPolicy.h"`. Also ensure `"World.h"` is included for `sWorld->getIntConfig` and `CONFIG_CUSTOM_BOT_AUTO_REVIVE_DELAY_MS` (it is already used elsewhere in this file via the death-policy code — confirm).

- [ ] **Step 2: `EnsureHunterPet`** — add to `BotMgr.cpp`:

```cpp
void BotMgr::EnsureHunterPet(Player* bot, BotEntry& entry)
{
    bool const isHunter = bot->GetClass() == CLASS_HUNTER;
    bool const inWorld  = bot->IsInWorld();
    bool const alive    = bot->IsAlive();
    if (!isHunter || !inWorld || !alive)
        return;   // engine guard: pet actions only for a live, in-world hunter

    Pet* pet = bot->GetPet();

    // Dead pet -> revive after the configured delay (recreate fresh at full health).
    if (pet && pet->isDead())
    {
        entry.petDeadTimer += BOT_FOLLOW_INTERVAL_MS;
        if (BotPetPolicy::ShouldRevivePet(true, false, int32(entry.petDeadTimer),
                sWorld->getIntConfig(CONFIG_CUSTOM_BOT_AUTO_REVIVE_DELAY_MS)))
        {
            bot->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
            entry.petDeadTimer = 0;
            if (!entry.petEntry)
                entry.petEntry = PickTameableBeastEntry(bot);
            SummonBotPet(bot, entry.petEntry);
        }
        return;
    }
    entry.petDeadTimer = 0;

    // No pet -> summon (initial spawn / after a cross-map teleport unsummoned it).
    if (BotPetPolicy::ShouldSummonPet(isHunter, inWorld, alive, pet != nullptr))
    {
        if (!entry.petEntry)
            entry.petEntry = PickTameableBeastEntry(bot);
        SummonBotPet(bot, entry.petEntry);
        return;
    }

    // Living pet -> keep its level in sync with the bot.
    if (pet && BotPetPolicy::NeedsLevelSync(pet->GetLevel(), bot->GetLevel()))
        pet->GivePetLevel(bot->GetLevel());
}
```

- [ ] **Step 3: Call it from `UpdateFollow`** — in the `UpdateFollow` per-bot loop, after the teleport state machine + the `!bot->IsInWorld()` / dead / `!ms` guards (i.e. where the bot is confirmed live and in-world, right before/after `EnsureGrouped(bot, master)`), add:

```cpp
        EnsureHunterPet(bot, entry);
```

Read `UpdateFollow` to place this where `bot` is known live and in-world. It is safe to run every follow tick (it's throttled by the follow interval and early-returns for non-hunters). It must run on the follow path regardless of combat (so the pet is maintained in and out of fights) — place it before the combat `if (target)` block so it runs each tick.

- [ ] **Step 4: Cleanup in `RemoveBot`** — `BotMgr::RemoveBot` (BotMgr.cpp:170) resolves the bot via `session->GetPlayer()` and then `delete session;` (which logs out + saves). Despawn the pet before that. The existing block:

```cpp
    // Leave the master's party so it isn't left with a stale member.
    if (Player* bot = session->GetPlayer())
        if (Group* group = bot->GetGroup())
            group->RemoveMember(bot->GetGUID());
```

becomes:

```cpp
    // Despawn any hunter pet and leave the master's party before logout/save.
    if (Player* bot = session->GetPlayer())
    {
        if (Pet* pet = bot->GetPet())
            bot->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);   // despawn pet before ~WorldSession saves the char
        if (Group* group = bot->GetGroup())
            group->RemoveMember(bot->GetGUID());
    }
```

- [ ] **Step 5: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): maintain hunter pets each tick (summon/revive/level) + despawn on remove

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: Changelog + build + in-game verify

**Files:**
- Modify: `CHANGELOG-custom.md`

- [ ] **Step 1: Changelog** — under the existing player-bot positioning/auto-attack area (after the "Ranged auto-attack between casts" paragraph), add a new subsection:

```markdown
## Hunter bot pets

Status: **first pass**

Hunter bots now summon and maintain a real pet. On the follow tick, `BotMgr::EnsureHunterPet`
reconciles pet state: a petless live Hunter gets one (the **nearest tameable beast** in
range via `PickTameableBeastEntry`, or Young Wolf #299 as fallback), summoned with the
`EffectTameCreature` sequence (`CreateTamedPetFrom` → level → `AddToMap` → `SetMinion` →
`REACT_DEFENSIVE` → `SavePetToDB`); a dead pet is revived after `Custom.BotAutoReviveDelayMs`;
a living pet's level is kept in sync (hunter pets don't auto-sync). Combat/follow are free
from `PetAI` (a defensive pet assists the owner's victim). The pet is despawned on
`.bot remove`. Pure decisions in `BotPetPolicy` (`tests/game/BotPetPolicy.cpp`).
Out of scope: pet ability rotation, specific-pet taming, families/talents, non-hunter pets.
Spec/plan: `docs/superpowers/specs/2026-06-26-bot-hunter-pets-design.md`,
`docs/superpowers/plans/2026-06-26-bot-hunter-pets.md`.
```

Commit:

```bash
git add CHANGELOG-custom.md
git commit -m "docs(bots): changelog entry for hunter bot pets

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

- [ ] **Step 2: Build**

```bash
cd /e/TrinityCore
docker images trinitycore:local --format 'pre-build: {{.ID}}'
docker compose --progress plain build > build-hunterpets.log 2>&1; echo "BUILD_EXIT=$?" >> build-hunterpets.log
```

- [ ] **Step 3: Verify build** — `BUILD_EXIT=0`, no `error:`/`failed to solve`, and the image ID changed. (Link fails with undefined `BotPetPolicy::*` / `BotMgr::EnsureHunterPet` if a file was missed — confirms the code is present.) If only the final `rpc … EOF`/`unpacking` line failed but `exporting … naming to docker.io/library/trinitycore:local` completed and the image ID changed, the build is good (daemon status blip).

- [ ] **Step 4: Deploy**

```bash
docker compose up -d
docker logs tc-worldserver 2>&1 | grep -iE "World initialized|ready\.\.\." | tail -2
```

- [ ] **Step 5: In-game verification (the integration gate)** — with a Hunter bot:
  - It summons a **zone-appropriate** pet shortly after spawning (a nearby beast type), at the bot's level.
  - The pet **attacks the bot's target** and **follows** out of combat.
  - **Kill the pet** → it revives ~5 s later.
  - **Zone the bot cross-map** (follow you through a portal/dungeon) → the pet reappears at the destination.
  - **`.bot remove`** the Hunter → the pet despawns and the **worldserver does not crash** (watch `docker logs tc-worldserver` for a crash around removal; this exercises the pet save path).
  - If `.bot remove` crashes, the fix is to despawn the pet earlier / use `PET_SAVE_NOT_IN_SLOT` (already specified) — re-check ordering in `RemoveBot`.

- [ ] **Step 6: Clean up build log** — `rm -f build-hunterpets.log`

---

## Task 6: PR + merge

- [ ] **Step 1: Push + PR**

```bash
git push -u origin claude/bot-hunter-pets
gh pr create --repo mountogdengc/TrinityCore --base main --head claude/bot-hunter-pets \
  --title "feat(bots): hunter bot pets (summon nearest tameable beast, revive, level, assist)" --body "$(cat <<'EOF'
Hunter bots now summon and maintain a real `HUNTER_PET`, so they stop doing half a Hunter's damage.

- New pure `BotPetPolicy` (ShouldSummonPet / ShouldRevivePet / NeedsLevelSync) + unit tests.
- `BotMgr::EnsureHunterPet` reconciles pet state each follow tick: summon the nearest tameable beast (Young Wolf #299 fallback) via the `EffectTameCreature` sequence; revive a dead pet after `Custom.BotAutoReviveDelayMs`; keep its level synced; resummon after a cross-map teleport; despawn on `.bot remove`.
- Combat/follow come free from `PetAI` (a `REACT_DEFENSIVE` pet assists the bot's target).

Verified in-game: pet summons zone-appropriate, fights the bot's target, revives after death, reappears after zoning, levels with the bot, and `.bot remove` despawns it with no crash.

Out of scope: pet ability rotation, specific-pet taming, families/talents, non-hunter pets.

Spec: `docs/superpowers/specs/2026-06-26-bot-hunter-pets-design.md`

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

- Pet revive uses the *value* of `Custom.BotAutoReviveDelayMs` but is NOT gated by the `Custom.BotAutoRevive` enable flag — hunter pets always come back.
- `EnsureHunterPet` early-returns for non-hunters, so it's cheap to call every tick for every bot.
- `PickTameableBeastEntry` runs only when summoning (entry cached on `BotEntry`), not per tick.
- Watch the `.bot remove` save path (the only real crash risk); the known `.character level` bot crash is unrelated (talent/spec state), but pets touch character save, so test it.
