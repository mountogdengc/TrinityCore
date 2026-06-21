# Low-level Hunter, Priest & Warrior Bot Rotations — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give sub-10 (pre-spec) Hunter, Priest, and Warrior bots a real ability rotation via an auto-applied hotfixes migration of custom `AssistedCombat` Initial-spec data — no engine code change.

**Architecture:** The merged M4 `BotRotation::SelectSpell` already builds a per-spec ability priority list from the `AssistedCombat`/`AssistedCombatStep` DB2 stores and casts the top castable ability each combat tick. Sub-10 characters report their class's "Initial" `ChrSpecialization`, for which Blizzard ships no data, so we add custom rows as an **auto-applied hotfixes updater migration**. On `docker compose up -d` the worldserver updater applies the migration to the hotfixes DB, then `DB2Manager` loads the `Status=1` hotfix rows into the in-memory `sAssistedCombatStore`, where the headless bot reads them. A small permanent DEBUG log at the cast site is the spec-ID verification tool.

**Tech Stack:** TrinityCore master (C++), MySQL hotfixes DB, TC SQL updater (`Updates.EnableDatabases` includes hotfixes; `Updates.Redundancy=0`), DB2 hotfix system, Docker Compose.

**Key assumption to validate (Task 5):** TC's hotfix loader *adds* new `AssistedCombat`/`AssistedCombatStep` records into the server-side in-memory stores. This is how custom DB2 content normally works in TC, but it is load-bearing — if a bot's priority list stays empty after the data applies (DEBUG log shows `spell 0` with a *correct* spec id and a known spell), the rows aren't reaching the store and we'd revisit. The existing `assisted_combat` tables are currently empty, so this migration is the first data in them.

**`Updates.Redundancy=0` discipline:** the updater applies each migration **once** and editing an already-applied file is unsafe on this fork. So this migration must be correct on first apply; any spec-ID correction is an **append-only** follow-up migration (`2026_06_21_01_hotfixes.sql`), never an edit to `..._00_...`.

**No automated test:** `SelectSpell` reads live `Player` + DB2 stores and the index builder is in an anonymous namespace, so there is no unit-test seam in the existing `tests/` harness. Verification is **manual in-game** with concrete observable outputs (Task 5). There is genuinely nothing meaningful to assert in isolation for a data-only change.

**Branch:** `claude/bot-lowlevel-rotations` (already created off `main`; design spec already committed here).

---

## File Structure

- **Create:** `sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql` — Hunter + Priest + Warrior Initial-spec rotation data + `hotfix_data` rows; auto-applied by the updater.
- **Remove:** `sql/custom/spike_assisted_combat_hunter_lowlevel.sql` — superseded (its Hunter rows fold into the migration).
- **Modify:** `src/server/scripts/Custom/Bots/BotMgr.cpp` — one permanent `TC_LOG_DEBUG` line at the `BotRotation::SelectSpell` cast site (diagnostics + spec-ID verification).
- **Modify:** `CHANGELOG-custom.md` and `src/server/scripts/Custom/Bots/ROADMAP.md` — repoint the removed spike file to the migration; record the auto-applied Hunter/Priest/Warrior data.

---

### Task 1: Create the auto-applied rotation migration & remove the spike file

**Files:**
- Create: `sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql`
- Remove: `sql/custom/spike_assisted_combat_hunter_lowlevel.sql`

- [ ] **Step 1: Create the migration with this exact content**

```sql
-- =====================================================================
-- Low-level (sub-10, pre-spec) bot rotations: Hunter, Priest, Warrior
-- =====================================================================
--
-- Server-side rotation data for the M4 player-bot rotation engine
-- (BotRotation::SelectSpell). Sub-10 characters report their class's
-- "Initial" ChrSpecialization, for which Blizzard ships no AssistedCombat
-- data; these custom rows give the engine an ability priority list so a
-- headless bot casts spells instead of melee-only.
--
-- Auto-applied by the worldserver updater (hotfixes DB). At startup the
-- hotfix loader then applies the Status=1 rows to the in-memory
-- sAssistedCombatStore / sAssistedCombatStepStore, which is what the bot
-- reads. Supersedes the old manual
-- sql/custom/spike_assisted_combat_hunter_lowlevel.sql (Hunter rows folded in).
--
-- Initial ChrSpecializationID per class (a wrong value = empty list = bot
-- melees; verify via the BotMgr rotation DEBUG log, plan Task 5):
--   Hunter  (class 3) -> 1448  (confirmed by the prior spike)
--   Priest  (class 5) -> 1452  (verify build 67823)
--   Warrior (class 1) -> 1446  (verify build 67823)
-- Correction is APPEND-ONLY: never edit this applied file (Updates.Redundancy=0);
-- add 2026_06_21_01_hotfixes.sql to UPDATE a wrong ChrSpecializationID.
--
-- Unconditional, always-castable damage spells only (the engine does not yet
-- evaluate AssistedCombatRule conditions; HasSpell() gates unknown spells).
--
-- Table hashes (db2 header table_hash):
--   AssistedCombat      0xA4A21680 = 2762086016
--   AssistedCombatStep  0x790BCC4F = 2030816335
-- =====================================================================

-- --- idempotent cleanup (safe re-run; the updater applies once) ----------
DELETE FROM `assisted_combat`      WHERE `ID` IN (1000001, 1000010, 1000020);
DELETE FROM `assisted_combat_step` WHERE `ID` IN (1000001, 1000002, 1000010, 1000011, 1000020);
DELETE FROM `hotfix_data`          WHERE `Id` IN (9000001, 9000010, 9000020);

-- =========================== HUNTER (1448) ==============================
INSERT INTO `assisted_combat` (`ID`, `ChrSpecializationID`, `VerifiedBuild`) VALUES
(1000001, 1448, 0);
INSERT INTO `assisted_combat_step` (`ID`, `SpellID`, `AssistedCombatID`, `OrderIndex`, `VerifiedBuild`) VALUES
(1000001, 185358, 1000001, 0, 0),   -- Arcane Shot
(1000002,  56641, 1000001, 1, 0);   -- Steady Shot
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000001, 2000000001, 2762086016, 1000001, 1, 0),
(9000001, 2000000002, 2030816335, 1000001, 1, 0),
(9000001, 2000000003, 2030816335, 1000002, 1, 0);

-- =========================== PRIEST (1452) ==============================
INSERT INTO `assisted_combat` (`ID`, `ChrSpecializationID`, `VerifiedBuild`) VALUES
(1000010, 1452, 0);
INSERT INTO `assisted_combat_step` (`ID`, `SpellID`, `AssistedCombatID`, `OrderIndex`, `VerifiedBuild`) VALUES
(1000010, 589, 1000010, 0, 0),   -- Shadow Word: Pain (DoT first)
(1000011, 585, 1000010, 1, 0);   -- Smite (filler)
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000010, 2000000010, 2762086016, 1000010, 1, 0),
(9000010, 2000000011, 2030816335, 1000010, 1, 0),
(9000010, 2000000012, 2030816335, 1000011, 1, 0);

-- =========================== WARRIOR (1446) =============================
INSERT INTO `assisted_combat` (`ID`, `ChrSpecializationID`, `VerifiedBuild`) VALUES
(1000020, 1446, 0);
INSERT INTO `assisted_combat_step` (`ID`, `SpellID`, `AssistedCombatID`, `OrderIndex`, `VerifiedBuild`) VALUES
(1000020, 1464, 1000020, 0, 0);   -- Slam
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000020, 2000000020, 2762086016, 1000020, 1, 0),
(9000020, 2000000021, 2030816335, 1000020, 1, 0);
```

- [ ] **Step 2: Remove the superseded spike file**

```bash
git rm sql/custom/spike_assisted_combat_hunter_lowlevel.sql
```
Expected: the file is staged for deletion.

- [ ] **Step 3: Confirm the migration applies cleanly against the live hotfixes DB (it is idempotent; the updater re-applies on restart anyway)**

Run (Bash tool):
```bash
docker exec -i tc-db mysql -utrinity -ptrinity hotfixes < sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql 2>&1 | grep -vi warning
```
Expected: no `ERROR` output. Then confirm the rows landed:
```bash
docker exec tc-db mysql -utrinity -ptrinity -N -e \
  "SELECT ID,ChrSpecializationID FROM hotfixes.assisted_combat WHERE ID IN (1000001,1000010,1000020) ORDER BY ID; \
   SELECT ID,SpellID,AssistedCombatID,OrderIndex FROM hotfixes.assisted_combat_step WHERE AssistedCombatID IN (1000001,1000010,1000020) ORDER BY AssistedCombatID,OrderIndex;" 2>&1 | grep -vi warning
```
Expected: AC rows `1000001 1448`, `1000010 1452`, `1000020 1446`; step rows for Hunter (185358 then 56641), Priest (589 then 585), Warrior (1464).

(This manual apply just proves the SQL is valid. The running worldserver won't *use* it until the Task 5 restart, where the updater applies it for real and the store loads it.)

- [ ] **Step 4: Commit**

```bash
git add sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql
git commit -m "feat(bots): auto-applied hotfixes migration for low-level Hunter/Priest/Warrior rotations

Folds the manual Hunter spike into an updater migration and adds Priest
(SW:Pain -> Smite) and Warrior (Slam) Initial-spec rotation data."
```

---

### Task 2: Add the rotation cast DEBUG log (diagnostics + spec-ID verification)

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp` (the `BotRotation::SelectSpell` cast site, inside the `if (target)` block, currently ~lines 364-369)

This logs the bot's primary spec and the selected spell each combat tick a target is engaged. At `DEBUG` level it is off in production; enabling `bots` logging during Task 5 prints the actual Initial spec id (confirming/correcting the migration's `ChrSpecializationID`s) and proves the rotation fires.

- [ ] **Step 1: Replace the existing cast block**

Find this block (currently ~lines 364-369):
```cpp
            // M4: data-driven rotation -- cast the top castable Assisted Combat
            // ability at the victim. The melee auto-attack set up above carries the
            // rotation between casts and covers specs/levels with no castable
            // ability (SelectSpell returns 0, so we just keep meleeing).
            if (uint32 spellId = BotRotation::SelectSpell(bot, target))
                bot->CastSpell(target, spellId);
            continue;
```

Replace it with:
```cpp
            // M4: data-driven rotation -- cast the top castable Assisted Combat
            // ability at the victim. The melee auto-attack set up above carries the
            // rotation between casts and covers specs/levels with no castable
            // ability (SelectSpell returns 0, so we just keep meleeing).
            uint32 const spellId = BotRotation::SelectSpell(bot, target);
            TC_LOG_DEBUG("bots", "Bot '{}' rotation: spec {}, spell {}.",
                bot->GetName(), int32(bot->GetPrimarySpecialization()), spellId);
            if (spellId)
                bot->CastSpell(target, spellId);
            continue;
```

- [ ] **Step 2: Verify by eye (compiles as part of Task 4, not standalone)**

Confirm:
- `TC_LOG_DEBUG` uses `{}` placeholders (correct for `TC_LOG_*`, unlike printf-style `PSendSysMessage`).
- `int32(bot->GetPrimarySpecialization())` mirrors `BotRotation.cpp:85`.
- `Log.h`, `int32`, and `Player.h` are already included in `BotMgr.cpp` (they are — the file already uses `TC_LOG_DEBUG`, `uint32`, and `Player`).

- [ ] **Step 3: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): log rotation spec + selected spell (DEBUG) for verification"
```

---

### Task 3: Repoint the docs off the removed spike file

**Files:**
- Modify: `CHANGELOG-custom.md`
- Modify: `src/server/scripts/Custom/Bots/ROADMAP.md`

- [ ] **Step 1: CHANGELOG — repoint the "Low-level data" line to the migration**

In `CHANGELOG-custom.md`, in the "Assisted-combat rotation — rotation engine (separate track)" section, replace:
```markdown
Key files: `src/server/scripts/Custom/Bots/BotRotation.{h,cpp}`; wired into the
combat loop in `BotMgr::UpdateFollow`. Low-level data:
`sql/custom/spike_assisted_combat_hunter_lowlevel.sql`.
Deeper docs: *Rotation engine* section of
`src/server/scripts/Custom/Bots/ROADMAP.md`.
```
with:
```markdown
Key files: `src/server/scripts/Custom/Bots/BotRotation.{h,cpp}`; wired into the
combat loop in `BotMgr::UpdateFollow`. Low-level data (Hunter / Priest / Warrior
Initial specs), auto-applied by the updater:
`sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql`.
Deeper docs: *Rotation engine* section of
`src/server/scripts/Custom/Bots/ROADMAP.md` and
`docs/superpowers/specs/2026-06-21-bot-lowlevel-rotations-design.md`.
```

- [ ] **Step 2: CHANGELOG — soften the now-fileless "spike" mention**

In the same section, replace:
```markdown
condition opcodes is a follow-on slice. The low-level Hunter
hotfix spike (steps-only, no rules) is consumed by the same path.
```
with:
```markdown
condition opcodes is a follow-on slice. The low-level Hunter/Priest/Warrior
hotfix data (steps-only, no rules) is consumed by the same path.
```

- [ ] **Step 3: ROADMAP — rewrite the "Prior art (exploratory spike)" bullet**

In `src/server/scripts/Custom/Bots/ROADMAP.md`, replace this bullet:
```markdown
- **Prior art (exploratory spike):**
  `sql/custom/spike_assisted_combat_hunter_lowlevel.sql` is a **client-side**
  hotfix experiment — it pushes custom `assisted_combat` rows for the Hunter
  "Initial" (sub-10, pre-spec) spec to a *real client* to test whether the 12.0
  client renders a rotation before level 10. The steps-only data it ships is
  consumed by the same resolver path.
```
with:
```markdown
- **Low-level data (Hunter / Priest / Warrior):**
  `sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql` ships custom
  `assisted_combat` Initial-spec steps for sub-10 Hunter, Priest, and Warrior bots
  (auto-applied by the updater; loaded server-side into `sAssistedCombatStore`,
  not the client). Unconditional damage only until the rule-condition slice lands.
  Supersedes the earlier manual Hunter spike file.
```

- [ ] **Step 4: Confirm no dangling reference to the removed file remains**

Run (Bash tool):
```bash
grep -rn "spike_assisted_combat_hunter_lowlevel" --include=*.md --include=*.sql . | grep -v docs/superpowers/specs/2026-06-21
```
Expected: no output (the only remaining mentions are historical, inside this feature's own spec/plan).

- [ ] **Step 5: Commit**

```bash
git add CHANGELOG-custom.md src/server/scripts/Custom/Bots/ROADMAP.md
git commit -m "docs: repoint rotation docs to the auto-applied migration; drop spike file refs"
```

---

### Task 4: Build the worldserver (carries the rotation engine + DEBUG log)

**Files:** none (build only)

The currently-running image predates the M4 rotation engine, so a build from this branch is required regardless.

- [ ] **Step 1: Build (full ~30-min recompile; never pipe to tail/head)**

```bash
docker compose build --progress=plain > /tmp/tc-build.log 2>&1; echo "exit=$?"
```
Expected: `exit=0`.

- [ ] **Step 2: Verify the build actually compiled (image ID changed)**

```bash
docker images trinitycore:local --format "{{.ID}}  {{.CreatedAt}}"
grep -iE "error:" /tmp/tc-build.log | head
```
Expected: a **new** image ID with a fresh timestamp, and no `error:` lines. (A failed build leaves the old image in place — per CLAUDE.md.)

---

### Task 5: Restart (migration auto-applies) and verify in-game (the real test)

**Files:** none (runtime verification)

- [ ] **Step 1: Recreate the containers onto the new image — the updater applies the migration, then the store loads it**

```bash
docker compose up -d
```
Expected: `tc-worldserver` + `tc-bnetserver` recreated on the new image id from Task 4.

- [ ] **Step 2: Confirm the migration applied cleanly (no updater error, row recorded)**

```bash
docker logs tc-worldserver 2>&1 | grep -iE "2026_06_21_00_hotfixes|Applied .* file|updates|error" | tail -20
docker exec tc-db mysql -utrinity -ptrinity -N -e \
  "SELECT name, state FROM hotfixes.updates WHERE name LIKE '2026_06_21_00%';" 2>&1 | grep -vi warning
```
Expected: the worldserver log shows the migration applied with no updater error; the `updates` row shows the migration name with state `RELEASED`. If the worldserver is crash-looping on an updater error, read the error, fix the migration, and (since it was never recorded as applied) restart.

- [ ] **Step 3: Enable bot DEBUG logging so the rotation log prints**

Attach the console (`docker attach tc-worldserver`; detach Ctrl-P Ctrl-Q) and run, or via SOAP:
```bash
curl -s -u "1#1:Password1" -H 'Content-Type: application/xml' \
  -d '<?xml version="1.0"?><SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ns1="urn:TC"><SOAP-ENV:Body><ns1:executeCommand><command>account set log Logger.bots Debug</command></ns1:executeCommand></SOAP-ENV:Body></SOAP-ENV:Envelope>' \
  http://127.0.0.1:7878/
```
If that command isn't available, set `Logger.bots=4,Console Server` in the worldserver config via the entrypoint env and restart, or just read `docker logs -f tc-worldserver` where `TC_LOG_DEBUG("bots", ...)` routes. The goal is only to see `Bot '<name>' rotation: spec N, spell M.` lines.

- [ ] **Step 4: Spawn the existing sub-10 bots, follow, and engage a mob**

In-world as the GM master (`david@local`), near a low-level mob. Available sub-10 test characters: `Alvaro` (Priest L7), `Lithilia` (Warrior L7), plus a sub-10 Hunter if one exists (else create/use any). For each:
```
.bot add Alvaro
.bot add Lithilia
.bot follow Alvaro
.bot follow Lithilia
```
Pull a mob so the bots assist.

- [ ] **Step 5: Read the rotation log and confirm spec IDs + casts**

Watch for lines like:
```
Bot 'Alvaro' rotation: spec 1452, spell 589.
Bot 'Lithilia' rotation: spec 1446, spell 1464.
```
Interpret:
- **`spec` matches the migration value** (Hunter 1448 / Priest 1452 / Warrior 1446) → ids correct.
- **`spell` non-zero** → rotation fired; cross-check on the target (Priest: SW:Pain debuff + Smite hits; Warrior: Slam hits; Hunter: Arcane/Steady Shot).
- **`spec` differs from the migration** → that is the real Initial spec id for this build. Go to Step 6 (append-only correction).
- **`spell 0` with a correct `spec` and a spell the bot knows** → if *no* class casts despite correct specs + known spells, the load-bearing store-load assumption failed; stop and report (see plan header).
- **`spell 0` because the bot hasn't learned the listed spell** → expected for very low levels (the `HasSpell` gate); confirm with a slightly higher sub-10 bot.

- [ ] **Step 6: (Only if a spec id was wrong) add an append-only correction migration**

Do **not** edit `2026_06_21_00_hotfixes.sql` (already applied; `Updates.Redundancy=0`). Create `sql/updates/hotfixes/master/2026_06_21_01_hotfixes.sql` with the corrected id, e.g. for a wrong Warrior id `<REAL_ID>`:
```sql
-- Correct the Warrior Initial ChrSpecializationID for build 67823 (verified
-- via the BotMgr rotation DEBUG log: the bot reported spec <REAL_ID>, not 1446).
UPDATE `assisted_combat` SET `ChrSpecializationID` = <REAL_ID> WHERE `ID` = 1000020;
```
Then `docker compose up -d` to apply, re-verify Step 5, and commit:
```bash
git add sql/updates/hotfixes/master/2026_06_21_01_hotfixes.sql
git commit -m "fix(bots): correct <class> Initial spec id for build 67823 (append-only)"
```
(If 1448/1452/1446 were all correct, skip this step entirely.)

---

### Task 6: Finish the branch

**Files:** none (git)

- [ ] **Step 1: Push the branch**

```bash
git push -u origin claude/bot-lowlevel-rotations
```

- [ ] **Step 2: Open the PR to main**

```bash
gh pr create --repo mountogdengc/TrinityCore --base main --head claude/bot-lowlevel-rotations \
  --title "Low-level Hunter/Priest/Warrior bot rotations (auto-applied AssistedCombat migration)" \
  --body "Adds sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql (Hunter: Arcane/Steady Shot; Priest: SW:Pain -> Smite; Warrior: Slam) so sub-10 pre-spec bots use the M4 rotation engine. Auto-applied by the updater; folds in + removes the manual Hunter spike file. Adds a DEBUG cast log. Verified in-game on build 67823. See docs/superpowers/specs/2026-06-21-bot-lowlevel-rotations-design.md.

🤖 Generated with [Claude Code](https://claude.com/claude-code)"
```

- [ ] **Step 3: Confirm mergeable**

```bash
gh pr view --repo mountogdengc/TrinityCore --json mergeable,mergeStateStatus -q '.mergeable + " / " + .mergeStateStatus'
```
Expected: `MERGEABLE / CLEAN`.
