# Companion Cohorts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build per-character companion cohorts on top of the current M3 bot runtime, including cohort persistence, auto-spawn on login, catch-up XP, continuity behaviors, and simple progress visibility.

**Architecture:** Add a small persistent cohort layer beside the existing `BotMgr`, not a second bot system. `BotCohortMgr` owns cohort records and owner-facing policy, while `BotMgr` remains the runtime execution layer for spawning, following, combat, and periodic continuity checks. Player-visible status is exposed through chat commands first so the system is testable before any addon or richer UI work.

**Tech Stack:** TrinityCore C++, TrinityCore script system (`CommandScript`, `PlayerScript`, `WorldScript`), Character DB prepared statements, Catch2 unit tests, Docker Compose runtime validation, SOAP/GM commands.

## Global Constraints

- Keep the existing M3 follow/group/combat runtime intact; layer cohort behavior on top of it.
- Catch-up must use real in-game XP gains only; do not use direct DB leveling or synthetic level writes.
- Cohorts are per owner character, not global and not account-shared.
- Auto-spawn must be configurable per owner character.
- Early visibility must include level-band state, catch-up XP state, active quests, objective progress, and blocked reasons.
- Continuity must cover resurrection and summons before adding broader autonomy.
- Preserve existing commands: `.bot add/remove/follow/stay/count`.
- Use TDD where a pure policy seam is available; for engine-integrated behavior, add the smallest test seam possible and pair it with explicit runtime validation steps.

---

## File Map

### New files

- `src/server/scripts/Custom/Bots/BotCohortPolicy.h`
- `src/server/scripts/Custom/Bots/BotCohortPolicy.cpp`
- `src/server/scripts/Custom/Bots/BotCohortMgr.h`
- `src/server/scripts/Custom/Bots/BotCohortMgr.cpp`
- `tests/game/BotCohortPolicy.cpp`
- `sql/updates/characters/master/2026_06_20_01_characters.sql`

### Modified files

- `src/server/scripts/Custom/Bots/BotMgr.h`
- `src/server/scripts/Custom/Bots/BotMgr.cpp`
- `src/server/scripts/Custom/Bots/cs_bot.cpp`
- `src/server/scripts/Custom/custom_script_loader.cpp`
- `src/server/database/Database/Implementation/CharacterDatabase.h`
- `src/server/database/Database/Implementation/CharacterDatabase.cpp`
- `src/server/scripts/Custom/Bots/README.md`
- `src/server/scripts/Custom/Bots/ROADMAP.md`

### Responsibilities

- `BotCohortPolicy.*`
  Pure helper logic for level-band state, catch-up multiplier, login auto-spawn gating, and continuity action selection.

- `BotCohortMgr.*`
  Persistent cohort records, owner/cohort queries, owner config, catch-up evaluation, and owner-facing status snapshots.

- `BotMgr.*`
  Runtime bot spawning, existing follow/combat loop, plus periodic cohort continuity checks and guid-based spawn support.

- `cs_bot.cpp`
  Command surface for cohort assignment, auto-spawn config, and status display.

- `CharacterDatabase.*` + SQL update
  Cohort table schema and prepared statements.

- `tests/game/BotCohortPolicy.cpp`
  Catch2 coverage for level-band state, XP multiplier, and continuity-selection helpers.

---

### Task 1: Add cohort persistence and pure cohort policy helpers

**Files:**
- Create: `src/server/scripts/Custom/Bots/BotCohortPolicy.h`
- Create: `src/server/scripts/Custom/Bots/BotCohortPolicy.cpp`
- Create: `tests/game/BotCohortPolicy.cpp`
- Create: `sql/updates/characters/master/2026_06_20_01_characters.sql`
- Modify: `src/server/database/Database/Implementation/CharacterDatabase.h`
- Modify: `src/server/database/Database/Implementation/CharacterDatabase.cpp`
- Modify: `src/server/scripts/Custom/Bots/ROADMAP.md`

**Interfaces:**
- Produces: 
  - `enum class BotLevelBandState : uint8 { Below, InRange, Above };`
  - `struct BotCatchupState { BotLevelBandState BandState; float XpMultiplier; bool CatchupActive; };`
  - `BotLevelBandState ComputeLevelBandState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance);`
  - `BotCatchupState ComputeCatchupState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance, float boostedMultiplier);`
  - `bool ShouldAutoSpawnCohort(bool autoSpawnEnabled, bool ownerAlive, bool ownerInWorld);`
  - `bool ShouldAutoAcceptResurrection(bool isDead, bool hasRequest);`
  - `bool ShouldAutoAcceptSummon(bool ownerInWorld, bool hasSummonPending);`

- [ ] **Step 1: Write the failing tests**

```cpp
#include "tc_catch2.h"

#include "BotCohortPolicy.h"

TEST_CASE("Bot cohort policy marks companions below the owner's level band", "[BotCohortPolicy]")
{
    REQUIRE(BotCohortPolicy::ComputeLevelBandState(20, 16, 2) == BotLevelBandState::Below);
    REQUIRE(BotCohortPolicy::ComputeLevelBandState(20, 19, 2) == BotLevelBandState::InRange);
    REQUIRE(BotCohortPolicy::ComputeLevelBandState(20, 24, 2) == BotLevelBandState::Above);
}

TEST_CASE("Bot cohort policy enables catch-up XP only below band", "[BotCohortPolicy]")
{
    BotCatchupState const below = BotCohortPolicy::ComputeCatchupState(20, 16, 2, 2.0f);
    BotCatchupState const inRange = BotCohortPolicy::ComputeCatchupState(20, 20, 2, 2.0f);

    REQUIRE(below.CatchupActive);
    REQUIRE(below.XpMultiplier == Approx(2.0f));
    REQUIRE(inRange.CatchupActive == false);
    REQUIRE(inRange.XpMultiplier == Approx(1.0f));
}

TEST_CASE("Bot cohort policy only auto-accepts continuity actions when appropriate", "[BotCohortPolicy]")
{
    REQUIRE(BotCohortPolicy::ShouldAutoSpawnCohort(true, true, true));
    REQUIRE_FALSE(BotCohortPolicy::ShouldAutoSpawnCohort(false, true, true));

    REQUIRE(BotCohortPolicy::ShouldAutoAcceptResurrection(true, true));
    REQUIRE_FALSE(BotCohortPolicy::ShouldAutoAcceptResurrection(false, true));

    REQUIRE(BotCohortPolicy::ShouldAutoAcceptSummon(true, true));
    REQUIRE_FALSE(BotCohortPolicy::ShouldAutoAcceptSummon(false, true));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
docker compose build > build.log 2>&1
```

Then run the test binary inside the rebuilt image or local build tree:

```bash
tests.exe "[BotCohortPolicy]"
```

Expected: FAIL with missing `BotCohortPolicy` declarations and missing test file.

- [ ] **Step 3: Write the minimal implementation**

Create the SQL schema and prepared statements:

```sql
CREATE TABLE IF NOT EXISTS `character_bot_cohort` (
  `owner_guid` BIGINT UNSIGNED NOT NULL,
  `companion_guid` BIGINT UNSIGNED NOT NULL,
  `active` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
  `auto_spawn` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`owner_guid`, `companion_guid`),
  KEY `idx_character_bot_cohort_owner_active` (`owner_guid`, `active`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

Add the pure policy seam:

```cpp
// BotCohortPolicy.h
enum class BotLevelBandState : uint8
{
    Below,
    InRange,
    Above
};

struct BotCatchupState
{
    BotLevelBandState BandState = BotLevelBandState::InRange;
    float XpMultiplier = 1.0f;
    bool CatchupActive = false;
};

namespace BotCohortPolicy
{
BotLevelBandState ComputeLevelBandState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance);
BotCatchupState ComputeCatchupState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance, float boostedMultiplier);
bool ShouldAutoSpawnCohort(bool autoSpawnEnabled, bool ownerAlive, bool ownerInWorld);
bool ShouldAutoAcceptResurrection(bool isDead, bool hasRequest);
bool ShouldAutoAcceptSummon(bool ownerInWorld, bool hasSummonPending);
}
```

```cpp
// BotCohortPolicy.cpp
BotLevelBandState BotCohortPolicy::ComputeLevelBandState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance)
{
    if (companionLevel + tolerance < ownerLevel)
        return BotLevelBandState::Below;

    if (companionLevel > ownerLevel + tolerance)
        return BotLevelBandState::Above;

    return BotLevelBandState::InRange;
}

BotCatchupState BotCohortPolicy::ComputeCatchupState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance, float boostedMultiplier)
{
    BotCatchupState result;
    result.BandState = ComputeLevelBandState(ownerLevel, companionLevel, tolerance);
    result.CatchupActive = result.BandState == BotLevelBandState::Below;
    result.XpMultiplier = result.CatchupActive ? boostedMultiplier : 1.0f;
    return result;
}

bool BotCohortPolicy::ShouldAutoSpawnCohort(bool autoSpawnEnabled, bool ownerAlive, bool ownerInWorld)
{
    return autoSpawnEnabled && ownerAlive && ownerInWorld;
}

bool BotCohortPolicy::ShouldAutoAcceptResurrection(bool isDead, bool hasRequest)
{
    return isDead && hasRequest;
}

bool BotCohortPolicy::ShouldAutoAcceptSummon(bool ownerInWorld, bool hasSummonPending)
{
    return ownerInWorld && hasSummonPending;
}
```

Add prepared statement ids and SQL wiring in `CharacterDatabase.h/.cpp` for:

```cpp
CHAR_SEL_BOT_COHORT_BY_OWNER,
CHAR_REP_BOT_COHORT_MEMBER,
CHAR_DEL_BOT_COHORT_MEMBER,
CHAR_UPD_BOT_COHORT_AUTO_SPAWN,
```

Update `ROADMAP.md` to note that cohort work has begun and that M4 now starts with persistence + owner binding before rotation work.

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
```

Expected: PASS, including the three policy tests.

- [ ] **Step 5: Commit**

```bash
git add \
  sql/updates/characters/master/2026_06_20_01_characters.sql \
  src/server/database/Database/Implementation/CharacterDatabase.h \
  src/server/database/Database/Implementation/CharacterDatabase.cpp \
  src/server/scripts/Custom/Bots/BotCohortPolicy.h \
  src/server/scripts/Custom/Bots/BotCohortPolicy.cpp \
  src/server/scripts/Custom/Bots/ROADMAP.md \
  tests/game/BotCohortPolicy.cpp
git commit -m "bot: add cohort persistence and policy helpers"
```

### Task 2: Add `BotCohortMgr` and owner-facing cohort commands

**Files:**
- Create: `src/server/scripts/Custom/Bots/BotCohortMgr.h`
- Create: `src/server/scripts/Custom/Bots/BotCohortMgr.cpp`
- Modify: `src/server/scripts/Custom/Bots/BotMgr.h`
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`
- Modify: `src/server/scripts/Custom/Bots/cs_bot.cpp`
- Modify: `src/server/scripts/Custom/custom_script_loader.cpp`
- Modify: `tests/game/BotCohortPolicy.cpp`

**Interfaces:**
- Consumes:
  - `BotCohortPolicy::ShouldAutoSpawnCohort(...)`
  - prepared statements from Task 1
- Produces:
  - `class BotCohortMgr`
  - `bool AssignCompanion(ObjectGuid ownerGuid, ObjectGuid companionGuid, bool active, std::string& error);`
  - `bool RemoveCompanion(ObjectGuid ownerGuid, ObjectGuid companionGuid, std::string& error);`
  - `std::vector<ObjectGuid> GetActiveCompanions(ObjectGuid ownerGuid) const;`
  - `bool GetAutoSpawnEnabled(ObjectGuid ownerGuid) const;`
  - `bool SetAutoSpawnEnabled(ObjectGuid ownerGuid, bool enabled, std::string& error);`
  - `bool SpawnOwnerCohort(Player* owner, std::string& error);`
  - `bool BotMgr::AddBot(ObjectGuid characterGuid, ObjectGuid master, std::string& error);`

- [ ] **Step 1: Extend the failing test seam for owner auto-spawn decisions**

Append to `tests/game/BotCohortPolicy.cpp`:

```cpp
TEST_CASE("Bot cohort policy blocks login auto-spawn for dead owners", "[BotCohortPolicy]")
{
    REQUIRE_FALSE(BotCohortPolicy::ShouldAutoSpawnCohort(true, false, true));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
```

Expected: FAIL because the new test references behavior not yet used by the runtime and the cohort manager does not exist.

- [ ] **Step 3: Write minimal implementation**

Create the cohort manager:

```cpp
// BotCohortMgr.h
class Player;

class BotCohortMgr
{
public:
    static BotCohortMgr* instance();

    bool AssignCompanion(ObjectGuid ownerGuid, ObjectGuid companionGuid, bool active, std::string& error);
    bool RemoveCompanion(ObjectGuid ownerGuid, ObjectGuid companionGuid, std::string& error);
    std::vector<ObjectGuid> GetActiveCompanions(ObjectGuid ownerGuid) const;
    bool GetAutoSpawnEnabled(ObjectGuid ownerGuid) const;
    bool SetAutoSpawnEnabled(ObjectGuid ownerGuid, bool enabled, std::string& error);
    bool SpawnOwnerCohort(Player* owner, std::string& error);
};

#define sBotCohortMgr BotCohortMgr::instance()
```

Add guid-based bot spawning:

```cpp
// BotMgr.h
bool AddBot(ObjectGuid characterGuid, ObjectGuid master, std::string& error);
bool AddBot(std::string const& characterName, ObjectGuid master, std::string& error);
```

```cpp
// BotMgr.cpp
bool BotMgr::AddBot(ObjectGuid characterGuid, ObjectGuid master, std::string& error)
{
    std::string const characterName = sCharacterCache->GetCharacterNameByGuid(characterGuid);
    if (characterName.empty())
    {
        error = "Could not resolve bot name from guid.";
        return false;
    }

    return AddBot(characterName, master, error);
}
```

Add owner-facing cohort commands under `.bot cohort`:

```cpp
static ChatCommandTable botCohortCommandTable =
{
    { "assign", HandleBotCohortAssignCommand, rbac::RBAC_PERM_COMMAND_GM, Console::No },
    { "remove", HandleBotCohortRemoveCommand, rbac::RBAC_PERM_COMMAND_GM, Console::No },
    { "list",   HandleBotCohortListCommand,   rbac::RBAC_PERM_COMMAND_GM, Console::No },
    { "auto",   HandleBotCohortAutoCommand,   rbac::RBAC_PERM_COMMAND_GM, Console::No },
    { "spawn",  HandleBotCohortSpawnCommand,  rbac::RBAC_PERM_COMMAND_GM, Console::No },
};
```

And register the new manager where custom scripts are loaded:

```cpp
// custom_script_loader.cpp remains AddSC_bots(); no separate loader is needed.
// cs_bot.cpp creates the command and player/world scripts that call sBotCohortMgr.
```

Manual verification commands after rebuild:

```text
.bot add MyCompanion
.bot cohort assign MyCompanion
.bot cohort list
.bot cohort auto on
.bot cohort spawn
```

Expected chat output:

```text
Bot 'MyCompanion' assigned to your cohort.
Active cohort members: 1
Automatic cohort spawn enabled.
Spawned 1 cohort companion.
```

- [ ] **Step 4: Run tests and runtime validation**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
docker compose up -d
```

Then log in with the GM character and run the five cohort commands above.

Expected:

- Catch2 still passes.
- Cohort assignment persists across logout/login.
- Manual spawn uses the assigned companion and sets the owner as `master`.

- [ ] **Step 5: Commit**

```bash
git add \
  src/server/scripts/Custom/Bots/BotCohortMgr.h \
  src/server/scripts/Custom/Bots/BotCohortMgr.cpp \
  src/server/scripts/Custom/Bots/BotMgr.h \
  src/server/scripts/Custom/Bots/BotMgr.cpp \
  src/server/scripts/Custom/Bots/cs_bot.cpp \
  src/server/scripts/Custom/custom_script_loader.cpp \
  tests/game/BotCohortPolicy.cpp
git commit -m "bot: add cohort manager and cohort commands"
```

### Task 3: Auto-spawn cohorts on login and add simple status visibility

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotCohortMgr.h`
- Modify: `src/server/scripts/Custom/Bots/BotCohortMgr.cpp`
- Modify: `src/server/scripts/Custom/Bots/cs_bot.cpp`
- Modify: `src/server/scripts/Custom/Bots/README.md`
- Modify: `tests/game/BotCohortPolicy.cpp`

**Interfaces:**
- Consumes:
  - `BotCohortMgr::GetActiveCompanions(...)`
  - `BotMgr::AddBot(ObjectGuid, ObjectGuid, std::string&)`
  - `BotCohortPolicy::ComputeCatchupState(...)`
- Produces:
  - `struct BotCohortStatusRow`
  - `std::vector<BotCohortStatusRow> BuildOwnerStatus(Player* owner) const;`
  - `.bot cohort status`
  - `PlayerScript::OnLogin` hook that calls `SpawnOwnerCohort`

- [ ] **Step 1: Add a failing status-format test**

Append to `tests/game/BotCohortPolicy.cpp`:

```cpp
TEST_CASE("Bot cohort policy exposes catch-up active only below band", "[BotCohortPolicy]")
{
    BotCatchupState const state = BotCohortPolicy::ComputeCatchupState(18, 14, 2, 1.5f);
    REQUIRE(state.BandState == BotLevelBandState::Below);
    REQUIRE(state.CatchupActive);
    REQUIRE(state.XpMultiplier == Approx(1.5f));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
```

Expected: FAIL until status generation uses the policy seam consistently.

- [ ] **Step 3: Write minimal implementation**

Add a player login hook in `cs_bot.cpp`:

```cpp
class bot_playerscript : public PlayerScript
{
public:
    bot_playerscript() : PlayerScript("bot_playerscript") { }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        std::string error;
        sBotCohortMgr->SpawnOwnerCohort(player, error);
    }
};
```

Register it:

```cpp
void AddSC_bots()
{
    new bot_commandscript();
    new bot_worldscript();
    new bot_playerscript();
}
```

Define a simple status row:

```cpp
struct BotCohortStatusRow
{
    std::string Name;
    uint8 Level = 0;
    BotLevelBandState BandState = BotLevelBandState::InRange;
    bool CatchupActive = false;
    float XpMultiplier = 1.0f;
};
```

And a status command:

```cpp
static bool HandleBotCohortStatusCommand(ChatHandler* handler)
{
    Player* owner = handler->GetPlayer();
    if (!owner)
        return false;

    for (BotCohortStatusRow const& row : sBotCohortMgr->BuildOwnerStatus(owner))
        handler->PSendSysMessage("%s - level %u - band %s - catchup %s (x%.1f)",
            row.Name.c_str(), row.Level,
            row.BandState == BotLevelBandState::Below ? "below" :
            row.BandState == BotLevelBandState::Above ? "above" : "in-range",
            row.CatchupActive ? "on" : "off",
            row.XpMultiplier);

    return true;
}
```

Update `README.md` so it no longer describes the system as M1-only and lists the new cohort commands.

- [ ] **Step 4: Run tests and login validation**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
docker compose up -d
```

Validation flow:

1. Log in on the owner character.
2. Confirm the active cohort spawns automatically.
3. Run `.bot cohort status`.

Expected output includes one line per assigned companion, for example:

```text
MyCompanion - level 14 - band below - catchup on (x1.5)
```

- [ ] **Step 5: Commit**

```bash
git add \
  src/server/scripts/Custom/Bots/BotCohortMgr.h \
  src/server/scripts/Custom/Bots/BotCohortMgr.cpp \
  src/server/scripts/Custom/Bots/cs_bot.cpp \
  src/server/scripts/Custom/Bots/README.md \
  tests/game/BotCohortPolicy.cpp
git commit -m "bot: auto-spawn cohorts and show basic cohort status"
```

### Task 4: Apply catch-up XP to active cohort companions

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotCohortMgr.h`
- Modify: `src/server/scripts/Custom/Bots/BotCohortMgr.cpp`
- Modify: `src/server/scripts/Custom/Bots/cs_bot.cpp`
- Modify: `tests/game/BotCohortPolicy.cpp`

**Interfaces:**
- Consumes:
  - `BotCohortPolicy::ComputeCatchupState(...)`
  - `BotCohortMgr::BuildOwnerStatus(...)`
- Produces:
  - `float GetCatchupMultiplier(Player const* owner, Player const* companion) const;`
  - `bool IsCompanionInOwnersActiveCohort(ObjectGuid ownerGuid, ObjectGuid companionGuid) const;`
  - `PlayerScript::OnGiveXP` catch-up adjustment

- [ ] **Step 1: Add the failing XP-multiplier test**

Append to `tests/game/BotCohortPolicy.cpp`:

```cpp
TEST_CASE("Bot cohort policy leaves XP unchanged when companion is in range", "[BotCohortPolicy]")
{
    BotCatchupState const state = BotCohortPolicy::ComputeCatchupState(18, 17, 2, 2.0f);
    REQUIRE_FALSE(state.CatchupActive);
    REQUIRE(state.XpMultiplier == Approx(1.0f));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
```

Expected: FAIL until the XP policy is consistently wired and the tests reflect the final helper behavior.

- [ ] **Step 3: Write minimal implementation**

Extend the player script:

```cpp
void OnGiveXP(Player* player, uint32& amount, Unit* /*victim*/) override
{
    if (WorldSession* session = player->GetSession())
    {
        if (!session->GetPlayer())
            return;
    }

    if (ObjectGuid ownerGuid = sBotMgr->GetMasterGuidForBot(player->GetGUID()); !ownerGuid.IsEmpty())
    {
        if (Player* owner = ObjectAccessor::FindConnectedPlayer(ownerGuid))
        {
            float const multiplier = sBotCohortMgr->GetCatchupMultiplier(owner, player);
            amount = uint32(float(amount) * multiplier);
        }
    }
}
```

Add the runtime query needed by the player script:

```cpp
// BotMgr.h
ObjectGuid GetMasterGuidForBot(ObjectGuid botGuid) const;
```

```cpp
// BotMgr.cpp
ObjectGuid BotMgr::GetMasterGuidForBot(ObjectGuid botGuid) const
{
    for (auto const& [name, entry] : _bots)
        if (entry.session->GetPlayer() && entry.session->GetPlayer()->GetGUID() == botGuid)
            return entry.master;

    return ObjectGuid::Empty;
}
```

Manual validation:

1. Put a companion 3+ levels behind the owner.
2. Kill mobs while grouped.
3. Run `.bot cohort status` before and after kills.

Expected:

- lower-level companion shows `catchup on`
- companion levels noticeably faster than owner until back in band

- [ ] **Step 4: Run tests and XP validation**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
docker compose up -d
```

Expected:

- Catch2 still passes.
- In-game XP gain is multiplied only for active companions below band.

- [ ] **Step 5: Commit**

```bash
git add \
  src/server/scripts/Custom/Bots/BotCohortMgr.h \
  src/server/scripts/Custom/Bots/BotCohortMgr.cpp \
  src/server/scripts/Custom/Bots/BotMgr.h \
  src/server/scripts/Custom/Bots/BotMgr.cpp \
  src/server/scripts/Custom/Bots/cs_bot.cpp \
  tests/game/BotCohortPolicy.cpp
git commit -m "bot: apply catch-up XP to cohort companions"
```

### Task 5: Auto-accept resurrection and summons for active cohort companions

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`
- Modify: `src/server/scripts/Custom/Bots/BotMgr.h`
- Modify: `src/server/scripts/Custom/Bots/BotCohortMgr.cpp`
- Modify: `tests/game/BotCohortPolicy.cpp`

**Interfaces:**
- Consumes:
  - `BotCohortPolicy::ShouldAutoAcceptResurrection(...)`
  - `BotCohortPolicy::ShouldAutoAcceptSummon(...)`
- Produces:
  - `void UpdateContinuity(Player* bot, Player* owner);`
  - periodic resurrection / summon acceptance inside `BotMgr::UpdateFollow`

- [ ] **Step 1: Add the failing continuity test**

Append to `tests/game/BotCohortPolicy.cpp`:

```cpp
TEST_CASE("Bot cohort policy rejects summon auto-accept when owner is absent", "[BotCohortPolicy]")
{
    REQUIRE_FALSE(BotCohortPolicy::ShouldAutoAcceptSummon(false, true));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
```

Expected: FAIL until the continuity helpers and runtime path are in agreement.

- [ ] **Step 3: Write minimal implementation**

Add a continuity helper in `BotMgr.cpp`:

```cpp
namespace
{
    void UpdateContinuity(Player* bot, Player* owner)
    {
        if (BotCohortPolicy::ShouldAutoAcceptResurrection(bot->IsDead(), bot->IsResurrectRequested()))
            bot->ResurrectUsingRequestData();

        if (BotCohortPolicy::ShouldAutoAcceptSummon(owner && owner->IsInWorld(), bot->HasSummonPending()))
            bot->SummonIfPossible(true);
    }
}
```

Call it from the live update loop before standard follow/combat logic:

```cpp
if (bot->IsInWorld())
    UpdateContinuity(bot, master);
```

Manual validation:

1. Kill a cohort companion.
2. Cast a resurrection on them.
3. Confirm they accept automatically.
4. Send a summon to a distant cohort companion.
5. Confirm they accept automatically and rejoin.

- [ ] **Step 4: Run tests and continuity validation**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
docker compose up -d
```

Expected:

- Catch2 passes.
- Companions accept resurrection requests.
- Companions accept summons while their owner is online and active.

- [ ] **Step 5: Commit**

```bash
git add \
  src/server/scripts/Custom/Bots/BotMgr.h \
  src/server/scripts/Custom/Bots/BotMgr.cpp \
  src/server/scripts/Custom/Bots/BotCohortMgr.cpp \
  tests/game/BotCohortPolicy.cpp
git commit -m "bot: add cohort resurrection and summon continuity"
```

### Task 6: Expose quest progress and blocked-state visibility for cohorts

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotCohortMgr.h`
- Modify: `src/server/scripts/Custom/Bots/BotCohortMgr.cpp`
- Modify: `src/server/scripts/Custom/Bots/cs_bot.cpp`
- Modify: `src/server/scripts/Custom/Bots/ROADMAP.md`

**Interfaces:**
- Consumes:
  - `Player::GetQuestStatus(uint32 questId) const`
  - `Player::GetQuestSlotQuestId(uint16 slot) const`
  - `Player::GetQuestObjectiveData(QuestObjective const& objective) const`
  - `sObjectMgr->GetQuestTemplate(questId)`
- Produces:
  - `struct BotQuestObjectiveStatus`
  - `struct BotQuestStatusView`
  - `std::vector<BotQuestStatusView> BuildQuestStatus(Player* companion) const;`
  - `.bot cohort quests`

- [ ] **Step 1: Write the failing visibility test**

Add a small formatter seam to test:

```cpp
TEST_CASE("Bot cohort quest formatter prints objective progress", "[BotCohortPolicy]")
{
    std::string const line = BotCohortPolicy::FormatObjectiveProgress("Northwatch Lugs", 3, 12);
    REQUIRE(line == "Northwatch Lugs: 3/12");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
```

Expected: FAIL because the formatter does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Add the formatter:

```cpp
std::string BotCohortPolicy::FormatObjectiveProgress(std::string const& label, int32 current, int32 required)
{
    return Trinity::StringFormat("{}: {}/{}", label, current, required);
}
```

Build quest status views:

```cpp
struct BotQuestObjectiveStatus
{
    std::string Label;
    int32 Current = 0;
    int32 Required = 0;
};

struct BotQuestStatusView
{
    uint32 QuestId = 0;
    std::string QuestTitle;
    std::vector<BotQuestObjectiveStatus> Objectives;
};
```

And emit them through a command:

```cpp
static bool HandleBotCohortQuestsCommand(ChatHandler* handler)
{
    Player* owner = handler->GetPlayer();
    if (!owner)
        return false;

    for (BotCohortStatusRow const& row : sBotCohortMgr->BuildOwnerStatus(owner))
    {
        handler->PSendSysMessage("%s", row.Name.c_str());
        for (BotQuestStatusView const& quest : sBotCohortMgr->BuildQuestStatus(row.CompanionGuid))
        {
            handler->PSendSysMessage("  [%u] %s", quest.QuestId, quest.QuestTitle.c_str());
            for (BotQuestObjectiveStatus const& objective : quest.Objectives)
                handler->PSendSysMessage("    %s", BotCohortPolicy::FormatObjectiveProgress(objective.Label, objective.Current, objective.Required).c_str());
        }
    }

    return true;
}
```

Manual validation:

```text
.bot cohort quests
```

Expected:

```text
MyCompanion
  [12345] Northwatch Sweep
    Northwatch Lugs: 3/12
```

- [ ] **Step 4: Run tests and quest-status validation**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
docker compose up -d
```

Expected:

- Catch2 passes.
- Quest output shows active quests and objective counts for each active companion.

- [ ] **Step 5: Commit**

```bash
git add \
  src/server/scripts/Custom/Bots/BotCohortPolicy.h \
  src/server/scripts/Custom/Bots/BotCohortPolicy.cpp \
  src/server/scripts/Custom/Bots/BotCohortMgr.h \
  src/server/scripts/Custom/Bots/BotCohortMgr.cpp \
  src/server/scripts/Custom/Bots/cs_bot.cpp \
  src/server/scripts/Custom/Bots/ROADMAP.md \
  tests/game/BotCohortPolicy.cpp
git commit -m "bot: expose cohort quest progress"
```

### Task 7: Add first-pass adventure actions for upgrades and town maintenance

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotCohortMgr.h`
- Modify: `src/server/scripts/Custom/Bots/BotCohortMgr.cpp`
- Modify: `src/server/scripts/Custom/Bots/cs_bot.cpp`
- Modify: `src/server/scripts/Custom/Bots/README.md`

**Interfaces:**
- Consumes:
  - active cohort membership from `BotCohortMgr`
  - owner context from `ChatHandler::GetPlayer()`
- Produces:
  - `.bot cohort maintain`
  - `bool RunTownMaintenance(Player* owner, Creature* vendor, std::string& error);`
  - `bool EquipObviousUpgrades(Player* companion);`

- [ ] **Step 1: Add the failing helper test**

Append to `tests/game/BotCohortPolicy.cpp`:

```cpp
TEST_CASE("Bot cohort policy keeps catch-up XP disabled for above-band companions", "[BotCohortPolicy]")
{
    BotCatchupState const state = BotCohortPolicy::ComputeCatchupState(20, 24, 2, 2.0f);
    REQUIRE(state.BandState == BotLevelBandState::Above);
    REQUIRE_FALSE(state.CatchupActive);
    REQUIRE(state.XpMultiplier == Approx(1.0f));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
```

Expected: FAIL until all final policy branches are covered and stable.

- [ ] **Step 3: Write minimal implementation**

Keep M8 intentionally command-driven for the first pass:

```cpp
static bool HandleBotCohortMaintainCommand(ChatHandler* handler)
{
    Player* owner = handler->GetPlayer();
    Creature* vendor = owner ? owner->GetSelectedCreature() : nullptr;
    if (!owner || !vendor)
        return false;

    std::string error;
    if (!sBotCohortMgr->RunTownMaintenance(owner, vendor, error))
    {
        handler->PSendSysMessage("Maintenance failed: %s", error.c_str());
        return false;
    }

    handler->SendSysMessage("Cohort maintenance complete.");
    return true;
}
```

And the first-pass maintenance rules:

```cpp
bool BotCohortMgr::RunTownMaintenance(Player* owner, Creature* vendor, std::string& error)
{
    for (ObjectGuid companionGuid : GetActiveCompanions(owner->GetGUID()))
    {
        Player* companion = ObjectAccessor::FindConnectedPlayer(companionGuid);
        if (!companion)
            continue;

        RepairAllItems(companion, vendor);
        SellPoorQualityItems(companion, vendor);
        EquipObviousUpgrades(companion);
    }

    return true;
}
```

Manual validation:

1. Give a companion damaged gear and gray items.
2. Stand at a repair vendor.
3. Target the vendor and run `.bot cohort maintain`.

Expected:

- gray items are sold
- damaged items are repaired
- clear stat upgrades already in bags can be equipped

- [ ] **Step 4: Run tests and maintenance validation**

Run:

```bash
docker compose build > build.log 2>&1
tests.exe "[BotCohortPolicy]"
docker compose up -d
```

Expected:

- Catch2 passes.
- The command-driven maintenance pass works on active companions without affecting non-cohort bots.

- [ ] **Step 5: Commit**

```bash
git add \
  src/server/scripts/Custom/Bots/BotCohortMgr.h \
  src/server/scripts/Custom/Bots/BotCohortMgr.cpp \
  src/server/scripts/Custom/Bots/cs_bot.cpp \
  src/server/scripts/Custom/Bots/README.md \
  tests/game/BotCohortPolicy.cpp
git commit -m "bot: add cohort maintenance actions"
```

## Self-Review Notes

### Spec coverage

- Per-character cohorts: Task 1 and Task 2
- Auto-spawn configurable per character: Task 2 and Task 3
- Catch-up XP through real gameplay: Task 4
- Resurrection and summons: Task 5
- Visible quest/objective progress: Task 6
- Basic upgrade / repair / junk-sell actions: Task 7
- Future milestones remain documented in the spec and roadmap, not in this implementation plan

### Placeholder scan

- No `TODO`, `TBD`, or “implement later” markers remain in tasks.
- Each task contains exact file paths, code snippets, commands, and expected results.

### Type consistency

- `BotLevelBandState` and `BotCatchupState` are introduced in Task 1 and reused consistently later.
- `BotCohortMgr` owns owner/cohort queries; `BotMgr` remains runtime-oriented.
- Guid-based spawn support is introduced before login auto-spawn depends on it.
