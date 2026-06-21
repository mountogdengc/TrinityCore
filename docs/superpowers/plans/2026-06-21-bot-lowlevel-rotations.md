# Low-level Priest & Warrior Bot Rotations — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give sub-10 (pre-spec) Priest and Warrior bots a real ability rotation by adding custom `AssistedCombat` Initial-spec data — no engine code change.

**Architecture:** The merged M4 `BotRotation::SelectSpell` already builds a per-spec priority list from the `AssistedCombat`/`AssistedCombatStep` DB2 stores and casts the top castable ability each combat tick. Sub-10 characters report their class's "Initial" `ChrSpecialization`, for which Blizzard ships no data, so we insert custom rows into the **hotfixes DB**; the worldserver's hotfix loader applies them to the in-memory `sAssistedCombatStore` at startup, where the headless bot reads them. A small, permanent debug log at the cast site doubles as the spec-ID verification tool.

**Tech Stack:** TrinityCore master (C++), MySQL hotfixes DB, DB2 hotfix system, Docker Compose.

**Key assumption to validate first (Task 5):** TrinityCore's hotfix loader *adds* new `AssistedCombat`/`AssistedCombatStep` records (not just overrides existing ones) into the server-side in-memory stores. This is how custom DB2 content normally works in TC, but it is the load-bearing assumption — if the bot's priority list stays empty after the data is applied (debug log shows `spell 0` with a correct spec id), the data isn't reaching the store and we'd need to revisit (e.g. ship the rows in a `.db2` patch instead).

**No automated test:** `SelectSpell` reads live `Player` + DB2 stores and the index builder is in an anonymous namespace, so there is no unit-test seam in the existing `tests/` harness. Verification is **manual in-game** with concrete observable outputs (Task 5). This is honest, not a shortcut — there is genuinely nothing meaningful to assert in isolation for a data-only change.

**Branch:** `claude/bot-lowlevel-rotations` (already created off `main`; the design spec is already committed here).

---

## File Structure

- **Create:** `sql/custom/bot_lowlevel_rotations.sql` — Priest + Warrior Initial-spec rotation data (idempotent, with rollback), modeled on `sql/custom/spike_assisted_combat_hunter_lowlevel.sql`.
- **Modify:** `src/server/scripts/Custom/Bots/BotMgr.cpp` — one permanent `TC_LOG_DEBUG` line at the `BotRotation::SelectSpell` cast site (rotation diagnostics + spec-ID verification).
- **Modify:** `CHANGELOG-custom.md` — record the low-level Priest/Warrior bot rotation data and how to apply it.

---

### Task 1: Author the rotation data SQL file

**Files:**
- Create: `sql/custom/bot_lowlevel_rotations.sql`

- [ ] **Step 1: Create the file with this exact content**

```sql
-- =====================================================================
-- Low-level (sub-10, pre-spec) bot rotations: Priest & Warrior
-- =====================================================================
--
-- Server-side rotation data for the M4 player-bot rotation engine
-- (BotRotation::SelectSpell). Sub-10 characters report their class's
-- "Initial" ChrSpecialization, for which Blizzard ships no AssistedCombat
-- data; these custom rows give the engine an ability priority list so a
-- headless bot casts spells instead of melee-only.
--
-- This is BOT data (server-side). Unlike the Hunter file it is not about
-- client rendering; the worldserver's hotfix loader applies these Status=1
-- rows to the in-memory sAssistedCombatStore / sAssistedCombatStepStore at
-- startup, which is what the bot reads.
--
-- Apply to the HOTFIXES database, then restart the worldserver:
--   docker exec -i tc-db mysql -utrinity -ptrinity hotfixes < sql/custom/bot_lowlevel_rotations.sql
--   docker compose up -d
--
-- Requires the assisted_combat / assisted_combat_step tables (created by
-- sql/updates/hotfixes/master/2026_06_19_00_hotfixes.sql).
--
-- !! VERIFY for this build (67823): the Initial ChrSpecializationID per
--    class. Hunter's 1448 is confirmed (spike). If a value below is wrong,
--    that class's priority list stays empty and the bot just melees --
--    confirm via the BotMgr rotation debug log (Task 5 of the plan).
--
-- Table hashes (db2 header table_hash, from the Hunter spike):
--   AssistedCombat      0xA4A21680 = 2762086016
--   AssistedCombatStep  0x790BCC4F = 2030816335
-- =====================================================================

SET @PRIEST_INITIAL_SPEC  := 1452;   -- Priest  "Initial" ChrSpecializationID (VERIFY)
SET @WARRIOR_INITIAL_SPEC := 1446;   -- Warrior "Initial" ChrSpecializationID (VERIFY)

SET @HASH_AC   := 2762086016;        -- 0xA4A21680
SET @HASH_STEP := 2030816335;        -- 0x790BCC4F

-- Unconditional, always-castable low-level damage spells only (the engine
-- does not yet evaluate AssistedCombatRule condition columns, so proc-gated
-- abilities like Victory Rush would mis-fire). The engine also gates every
-- candidate on HasSpell(), so listing a not-yet-learned spell is harmless.
SET @SPELL_SW_PAIN := 589;           -- Shadow Word: Pain (Priest DoT)
SET @SPELL_SMITE   := 585;           -- Smite (Priest filler)
SET @SPELL_SLAM    := 1464;          -- Slam (Warrior)

-- Row-id ranges: high + collision-safe, distinct from the Hunter spike
-- (1000001.., push 9000001..) and between classes.
SET @AC_PRIEST   := 1000010;
SET @AC_WARRIOR  := 1000020;

-- --- clean any prior run -------------------------------------------------
DELETE FROM `assisted_combat`      WHERE `ID` IN (@AC_PRIEST, @AC_WARRIOR);
DELETE FROM `assisted_combat_step` WHERE `ID` IN (1000010, 1000011, 1000020);
DELETE FROM `hotfix_data`          WHERE `Id` IN (9000010, 9000020);

-- =========================== PRIEST =====================================
INSERT INTO `assisted_combat` (`ID`, `ChrSpecializationID`, `VerifiedBuild`) VALUES
(@AC_PRIEST, @PRIEST_INITIAL_SPEC, 0);

INSERT INTO `assisted_combat_step` (`ID`, `SpellID`, `AssistedCombatID`, `OrderIndex`, `VerifiedBuild`) VALUES
(1000010, @SPELL_SW_PAIN, @AC_PRIEST, 0, 0),   -- DoT first
(1000011, @SPELL_SMITE,   @AC_PRIEST, 1, 0);   -- filler

INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000010, 2000000010, @HASH_AC,   @AC_PRIEST, 1, 0),
(9000010, 2000000011, @HASH_STEP, 1000010,    1, 0),
(9000010, 2000000012, @HASH_STEP, 1000011,    1, 0);

-- =========================== WARRIOR ====================================
INSERT INTO `assisted_combat` (`ID`, `ChrSpecializationID`, `VerifiedBuild`) VALUES
(@AC_WARRIOR, @WARRIOR_INITIAL_SPEC, 0);

INSERT INTO `assisted_combat_step` (`ID`, `SpellID`, `AssistedCombatID`, `OrderIndex`, `VerifiedBuild`) VALUES
(1000020, @SPELL_SLAM, @AC_WARRIOR, 0, 0);

INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(9000020, 2000000020, @HASH_AC,   @AC_WARRIOR, 1, 0),
(9000020, 2000000021, @HASH_STEP, 1000020,     1, 0);

-- =====================================================================
-- ROLLBACK (run these to remove this data):
--   DELETE FROM `assisted_combat`      WHERE `ID` IN (1000010, 1000020);
--   DELETE FROM `assisted_combat_step` WHERE `ID` IN (1000010, 1000011, 1000020);
--   DELETE FROM `hotfix_data`          WHERE `Id` IN (9000010, 9000020);
-- =====================================================================
```

- [ ] **Step 2: Apply the file for real and confirm it parses + inserts (it is idempotent; Task 5 re-applies it)**

Run (Bash tool):
```bash
docker exec -i tc-db mysql -utrinity -ptrinity hotfixes < sql/custom/bot_lowlevel_rotations.sql 2>&1 | grep -vi warning
```
Expected: no `ERROR` output. Then confirm the rows landed:
```bash
docker exec tc-db mysql -utrinity -ptrinity -N -e \
  "SELECT ID,ChrSpecializationID FROM hotfixes.assisted_combat WHERE ID IN (1000010,1000020); \
   SELECT ID,SpellID,OrderIndex FROM hotfixes.assisted_combat_step WHERE ID IN (1000010,1000011,1000020) ORDER BY ID;" 2>&1 | grep -vi warning
```
Expected: AC rows `1000010 1452` and `1000020 1446`; step rows `1000010 589 0`, `1000011 585 1`, `1000020 1464 0`.

(The running worldserver won't pick these up until the Task 5 restart — hotfixes load at startup. That's fine.)

- [ ] **Step 3: Commit**

```bash
git add sql/custom/bot_lowlevel_rotations.sql
git commit -m "feat(bots): low-level Priest & Warrior rotation data (AssistedCombat Initial spec)"
```

---

### Task 2: Add the rotation cast debug log (diagnostics + spec-ID verification)

**Files:**
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp` (the `BotRotation::SelectSpell` cast site, ~line 368, inside the `if (target)` block)

This logs the bot's primary spec and the selected spell every combat tick a target is engaged. At `DEBUG` level it is off in production; enabling `Logger.bots=4` (Debug) during Task 5 prints the actual Initial spec id (confirming/correcting the SQL `@*_INITIAL_SPEC` values) and proves the rotation fires.

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

- [ ] **Step 2: Confirm it compiles in isolation (no full build yet)**

This is part of the Task 4 build; no standalone compile here. Just verify by eye that:
- `TC_LOG_DEBUG` uses `{}` placeholders (correct for `TC_LOG_*`, unlike `PSendSysMessage` which is printf-style).
- `int32(bot->GetPrimarySpecialization())` mirrors the existing cast in `BotRotation.cpp:85`.
- `<cstdint>`/`int32` and `Log.h` are already included in `BotMgr.cpp` (they are — the file already uses `TC_LOG_DEBUG` and `uint32`).

- [ ] **Step 3: Commit**

```bash
git add src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "feat(bots): log rotation spec + selected spell (DEBUG) for verification"
```

---

### Task 3: Update CHANGELOG-custom.md

**Files:**
- Modify: `CHANGELOG-custom.md` (the "Assisted-combat rotation — rotation engine (separate track)" section)

- [ ] **Step 1: Append a paragraph to that section's prose** (after the existing "Key files / Deeper docs" lines, before the next `##` heading)

```markdown

**Low-level data (Priest & Warrior):** `sql/custom/bot_lowlevel_rotations.sql`
adds custom `AssistedCombat` Initial-spec rows so sub-10 (pre-spec) Priest and
Warrior bots get a rotation (Priest: Shadow Word: Pain → Smite; Warrior: Slam).
Apply to the **hotfixes** DB and restart the worldserver
(`docker exec -i tc-db mysql -utrinity -ptrinity hotfixes < sql/custom/bot_lowlevel_rotations.sql`).
Unconditional damage only until the rule-condition slice lands. Design:
`docs/superpowers/specs/2026-06-21-bot-lowlevel-rotations-design.md`.
```

- [ ] **Step 2: Commit**

```bash
git add CHANGELOG-custom.md
git commit -m "docs: changelog entry for low-level Priest/Warrior bot rotation data"
```

---

### Task 4: Build the worldserver (carries the rotation engine + debug log)

**Files:** none (build only)

The currently-running image predates the M4 rotation engine, so a build from this branch is required regardless of this feature.

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

### Task 5: Apply data, restart, and verify in-game (the real test)

**Files:** none (runtime verification)

- [ ] **Step 1: Apply the rotation data to the hotfixes DB (idempotent)**

```bash
docker exec -i tc-db mysql -utrinity -ptrinity hotfixes < sql/custom/bot_lowlevel_rotations.sql 2>&1 | grep -vi warning
docker exec tc-db mysql -utrinity -ptrinity -N -e \
  "SELECT t.AssistedCombatID, t.SpellID, t.OrderIndex FROM hotfixes.assisted_combat_step t WHERE t.ID IN (1000010,1000011,1000020) ORDER BY t.AssistedCombatID,t.OrderIndex;" 2>&1 | grep -vi warning
```
Expected: three step rows (Priest container → 589 then 585; Warrior container → 1464).

- [ ] **Step 2: Recreate the containers onto the new image (loads the binary AND the hotfix rows)**

```bash
docker compose up -d
```
Expected: `tc-worldserver` and `tc-bnetserver` recreated; `docker ps` shows them on the new image id from Task 4.

- [ ] **Step 3: Enable bot debug logging so the rotation log prints**

The worldserver console (attach with `docker attach tc-worldserver`, detach Ctrl-P Ctrl-Q), or via SOAP:
```bash
curl -s -u "1#1:Password1" -H 'Content-Type: application/xml' \
  -d '<?xml version="1.0"?><SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ns1="urn:TC"><SOAP-ENV:Body><ns1:executeCommand><command>account set log Logger.bots Debug</command></ns1:executeCommand></SOAP-ENV:Body></SOAP-ENV:Envelope>' \
  http://127.0.0.1:7878/
```
If that logger command isn't available, set `Logger.bots=4,Console Server` in `worldserver.conf` via the entrypoint env and restart, or just read the worldserver stdout where `TC_LOG_DEBUG("bots", ...)` already routes. The goal is only to see the `Bot '<name>' rotation: spec N, spell M.` lines.

- [ ] **Step 4: Spawn the existing sub-10 bots, group/follow, and engage a mob**

In-world as the GM master (`david@local`), near a low-level mob:
```
.bot add Alvaro       (Priest, L7)
.bot add Lithilia     (Warrior, L7)
.bot follow Alvaro
.bot follow Lithilia
```
Pull a mob so the bots assist.

- [ ] **Step 5: Read the rotation log and confirm the spec IDs + casts**

Watch the worldserver log for lines like:
```
Bot 'Alvaro' rotation: spec 1452, spell 589.
Bot 'Lithilia' rotation: spec 1446, spell 1464.
```
Interpret:
- **`spec` matches the SQL `@*_INITIAL_SPEC`** (Priest 1452 / Warrior 1446) → the IDs are correct. If `spec` shows a *different* number, that is the real Initial spec id for this build: update the matching `SET @*_INITIAL_SPEC` in `sql/custom/bot_lowlevel_rotations.sql`, re-run Step 1 (no rebuild needed), and re-test.
- **`spell` is non-zero** (589/585 for the priest, 1464 for the warrior) → the rotation fired. Cross-check by watching the target: a Shadow Word: Pain debuff + Smite hits on the priest's target; Slam hits from the warrior.
- **`spell 0` with a correct `spec`** → either the bot hasn't learned that spell (try `Alvaro` L7 rather than `Followerone` L1; the `HasSpell` gate is expected), or — if no class casts anything despite correct specs and known spells — the load-bearing assumption failed (hotfix rows didn't populate the server store); stop and report (see plan header).

- [ ] **Step 6: Record the verified spec IDs**

If Step 5 showed different spec IDs than 1452/1446, ensure the SQL file now has the corrected values committed:
```bash
git add sql/custom/bot_lowlevel_rotations.sql
git commit -m "fix(bots): correct Priest/Warrior Initial spec ids per build 67823"
```
(If 1452/1446 were already correct, skip — nothing changed.)

- [ ] **Step 7: Update the SQL file's VERIFY comment to a confirmed note**

Once verified, change the two `(VERIFY)` comments in `sql/custom/bot_lowlevel_rotations.sql` to `(confirmed build 67823)` and commit:
```bash
git add sql/custom/bot_lowlevel_rotations.sql
git commit -m "docs(bots): mark Priest/Warrior Initial spec ids confirmed for build 67823"
```

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
  --title "Low-level Priest & Warrior bot rotations (AssistedCombat data)" \
  --body "Adds sql/custom/bot_lowlevel_rotations.sql (Priest: SW:Pain → Smite; Warrior: Slam) so sub-10 pre-spec bots use the M4 rotation engine. Data-only + a DEBUG cast log. Verified in-game on build 67823. See docs/superpowers/specs/2026-06-21-bot-lowlevel-rotations-design.md.

🤖 Generated with [Claude Code](https://claude.com/claude-code)"
```

- [ ] **Step 3: Confirm mergeable**

```bash
gh pr view --repo mountogdengc/TrinityCore --json mergeable,mergeStateStatus -q '.mergeable + " / " + .mergeStateStatus'
```
Expected: `MERGEABLE / CLEAN`.
```
```
