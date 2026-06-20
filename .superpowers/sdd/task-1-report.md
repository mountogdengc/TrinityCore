## Task 1 Report: Cohort persistence and pure cohort policy helpers

### Status

DONE_WITH_CONCERNS

### What I implemented

This slice is present in the workspace and aligns with the task brief's requested scope:

- Added pure cohort policy helpers in `src/server/scripts/Custom/Bots/BotCohortPolicy.h` and `src/server/scripts/Custom/Bots/BotCohortPolicy.cpp`.
- Added the focused Catch2 policy tests in `tests/game/BotCohortPolicy.cpp`.
- Added cohort persistence schema in `sql/updates/characters/master/2026_06_20_01_characters.sql`.
- Added character DB prepared statement ids in `src/server/database/Database/Implementation/CharacterDatabase.h`.
- Added prepared statement SQL wiring in `src/server/database/Database/Implementation/CharacterDatabase.cpp`.
- Updated `src/server/scripts/Custom/Bots/ROADMAP.md` to reflect that M4 starts with cohort persistence and owner binding before rotation work.

### Functional details

#### Pure cohort policy seam

Implemented:

- `enum class BotLevelBandState : uint8 { Below, InRange, Above };`
- `struct BotCatchupState`
- `BotCohortPolicy::ComputeLevelBandState`
- `BotCohortPolicy::ComputeCatchupState`
- `BotCohortPolicy::ShouldAutoSpawnCohort`
- `BotCohortPolicy::ShouldAutoAcceptResurrection`
- `BotCohortPolicy::ShouldAutoAcceptSummon`

Behavior in the current implementation:

- A companion is `Below` when `companionLevel + tolerance < ownerLevel`.
- A companion is `Above` when `companionLevel > ownerLevel + tolerance`.
- Otherwise it is `InRange`.
- Catch-up XP is active only while the companion is `Below`.
- Catch-up XP uses `boostedMultiplier` while active and `1.0f` otherwise.
- Auto-spawn requires all of `autoSpawnEnabled`, `ownerAlive`, and `ownerInWorld`.
- Resurrection auto-accept requires both `isDead` and `hasRequest`.
- Summon auto-accept requires both `ownerInWorld` and `hasSummonPending`.

#### Persistence

Added table:

`character_bot_cohort`

Columns:

- `owner_guid`
- `companion_guid`
- `active`
- `auto_spawn`

Indexing:

- primary key on `(owner_guid, companion_guid)`
- secondary key on `(owner_guid, active)`

Prepared statements added:

- `CHAR_SEL_BOT_COHORT_BY_OWNER`
- `CHAR_REP_BOT_COHORT_MEMBER`
- `CHAR_DEL_BOT_COHORT_MEMBER`
- `CHAR_UPD_BOT_COHORT_AUTO_SPAWN`

### Files changed

- `E:/TrinityCore/src/server/scripts/Custom/Bots/BotCohortPolicy.h`
- `E:/TrinityCore/src/server/scripts/Custom/Bots/BotCohortPolicy.cpp`
- `E:/TrinityCore/tests/game/BotCohortPolicy.cpp`
- `E:/TrinityCore/sql/updates/characters/master/2026_06_20_01_characters.sql`
- `E:/TrinityCore/src/server/database/Database/Implementation/CharacterDatabase.h`
- `E:/TrinityCore/src/server/database/Database/Implementation/CharacterDatabase.cpp`
- `E:/TrinityCore/src/server/scripts/Custom/Bots/ROADMAP.md`

### Validation attempted

The environment note in the prompt was accurate: this shell did not have local build/test tooling available for the exact planned validation path.

#### Commands run

Read/inspection commands:

```powershell
Get-Content E:/TrinityCore/.superpowers/sdd/task-1-brief.md
Get-Content src/server/scripts/Custom/Bots/BotCohortPolicy.h
Get-Content src/server/scripts/Custom/Bots/BotCohortPolicy.cpp
Get-Content tests/game/BotCohortPolicy.cpp
git status --short
Get-Content src/server/database/Database/Implementation/CharacterDatabase.h
Get-Content src/server/database/Database/Implementation/CharacterDatabase.cpp
Get-Content src/server/scripts/Custom/Bots/ROADMAP.md
Get-Content sql/updates/characters/master/2026_06_20_01_characters.sql
rg -n "BotCohortPolicy|character_bot_cohort|2026_06_20_01_characters|BotCohortPolicy.cpp" -S tests src sql CMakeLists.txt
Get-Content build-red.log
Get-ChildItem -Recurse -Filter tests.exe | Select-Object -ExpandProperty FullName
where.exe cmake
where.exe cl
```

Build/test attempt:

```powershell
docker compose build
```

#### Results

- The file inspections confirmed that the requested cohort policy seam, tests, SQL migration, DB prepared statement ids, prepared statement SQL, and roadmap updates are present in the workspace.
- `where.exe cmake` returned no result.
- `where.exe cl` returned no result.
- `Get-ChildItem -Recurse -Filter tests.exe | Select-Object -ExpandProperty FullName` did not produce a usable test binary during the attempt window.
- `rg` returned the expected references but hit the command timeout after emitting matches.

#### Build failure / limits

The available Docker build evidence came from the existing `build-red.log`, which showed:

- Docker config access warnings:
  - `open C:\Users\david\.docker\config.json: Access is denied.`
- Buildx instance access failure:
  - `CreateFile C:\Users\david\.docker\buildx\instances: Access is denied.`

I then requested an escalated `docker compose build` to get a real compile signal outside the sandbox restrictions, but that attempt was interrupted before completion, so I do not have a successful or failing compile result attributable to the current code.

### TDD note

The workspace already contained the policy test file and implementation files when I inspected it. I verified the presence and alignment of the tests with the task brief, but I was not able to complete a fresh red/green test cycle in this environment because the available build/test path was blocked by missing local toolchain binaries and Docker access limitations.

### Concerns

1. I could not complete compile or test execution, so I cannot claim build-verified completion.
2. The current report reflects the workspace state I found and validated by inspection; I did not independently prove in this session that the new test file is wired into the test target or that these files compile together.
3. The escalated Docker retry was interrupted, so there is still no definitive runtime/build signal for this slice.

### Commit status

- No commit created in this session.

### Overall assessment

The requested Task 1 slice appears to be implemented in the permitted files and matches the task brief at the source level, but completion remains `DONE_WITH_CONCERNS` because validation could not be completed in this environment.
