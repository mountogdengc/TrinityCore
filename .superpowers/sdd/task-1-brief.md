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

