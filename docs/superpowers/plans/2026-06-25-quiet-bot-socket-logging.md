# Quiet bot null-socket logging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop `WorldSession::SendPacket` from logging the "non existent socket" error for headless bots (socketless by design), without changing behavior or silencing real-client socket errors.

**Architecture:** Add a first-class `IsBot()` flag to `WorldSession`, set it when `BotMgr` creates the bot session, and gate only that one `TC_LOG_ERROR` on `!IsBot()`.

**Tech Stack:** C++ (TrinityCore master), Docker Compose build/run, in-game log verification. No unit test (a logging gate on engine state; the integration gate is the in-game log count).

**Spec:** `docs/superpowers/specs/2026-06-25-quiet-bot-socket-logging-design.md`

---

## File structure

- **Modify** `src/server/game/Server/WorldSession.h` — `_isBot` member + `IsBot()`/`SetBot()` accessors.
- **Modify** `src/server/game/Server/WorldSession.cpp` — gate the null-socket log on `!IsBot()`.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.cpp` — `session->SetBot()` after the bot session is constructed.
- **Modify** `CHANGELOG-custom.md` — record the upstream-file edit.

---

## Task 1: Add the `IsBot()` marker, gate the log, set it on bot sessions

**Files:**
- Modify: `src/server/game/Server/WorldSession.h`
- Modify: `src/server/game/Server/WorldSession.cpp`
- Modify: `src/server/scripts/Custom/Bots/BotMgr.cpp`

- [ ] **Step 1: Add accessors + member to `WorldSession.h`**

Near the other small boolean accessors (around line 984, after `bool PlayerLoading() const ...`), add:

```cpp
        bool IsBot() const { return _isBot; }
        void SetBot() { _isBot = true; }   // headless player-bot: socketless by design
```

In the private data members (around line 2011-2016, next to `bool m_inQueue;` / `bool m_playerLogout;`), add an in-class-initialized member:

```cpp
        bool _isBot = false;                                // headless player-bot session (null socket by design)
```

(Read both regions first to confirm placement; the accessor goes in the public section near `PlayerLoading()`, the member in the private section near the other `bool m_player*` flags. In-class init avoids touching the constructor.)

- [ ] **Step 2: Gate the null-socket log in `WorldSession.cpp`**

The block at `WorldSession.cpp:254-258` currently reads:

```cpp
    if (!m_Socket[conIdx])
    {
        TC_LOG_ERROR("network.opcode", "Prevented sending of {} to non existent socket {} to {}", GetOpcodeNameForLogging(static_cast<OpcodeServer>(packet->GetOpcode())), uint32(conIdx), GetPlayerInfo());
        return;
    }
```

Change it to skip the log for bot sessions (the early `return` is unchanged):

```cpp
    if (!m_Socket[conIdx])
    {
        if (!IsBot())   // headless bots are socketless by design -- expected, not an error
            TC_LOG_ERROR("network.opcode", "Prevented sending of {} to non existent socket {} to {}", GetOpcodeNameForLogging(static_cast<OpcodeServer>(packet->GetOpcode())), uint32(conIdx), GetPlayerInfo());
        return;
    }
```

Leave the other `Prevented sending …` logs in this function (invalid opcode, missing handler, instance-only, disabled opcode) untouched — those are real errors even for a bot.

- [ ] **Step 3: Mark the bot session in `BotMgr.cpp`**

The bot session is constructed at `BotMgr.cpp:137-142`, ending with `0u /*recruiter*/, false);` at line 142. Immediately after that statement (before `_bots[key] = BotEntry{ session, master };`), add:

```cpp
    session->SetBot();   // socketless-by-design: suppress "non existent socket" log spam
```

So lines 142-144 become:

```cpp
        0u /*recruiter*/, false);

    session->SetBot();   // socketless-by-design: suppress "non existent socket" log spam

    _bots[key] = BotEntry{ session, master };
```

- [ ] **Step 4: Commit**

```bash
git add src/server/game/Server/WorldSession.h src/server/game/Server/WorldSession.cpp \
        src/server/scripts/Custom/Bots/BotMgr.cpp
git commit -m "fix(bots): silence expected null-socket log spam via WorldSession::IsBot()

WorldSession::SendPacket logged a network.opcode ERROR for every packet routed
to a headless bot (socketless by design) -- 23k+ lines/session. Add an IsBot()
flag set by BotMgr and gate only the non-existent-socket log on it; the packet
is still dropped and real-client socket errors still log.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Changelog (upstream-file edit)

**Files:**
- Modify: `CHANGELOG-custom.md`

- [ ] **Step 1: Record the upstream edit**

In `CHANGELOG-custom.md`, under the **Code modifications to upstream files** section (the bulleted list that already has entries like `**src/server/game/Handlers/CharacterHandler.cpp**`), add a bullet:

```markdown
- **`src/server/game/Server/WorldSession.{h,cpp}`** — add a `WorldSession::IsBot()`
  flag (set by `BotMgr` on the headless bot session) and gate the "non existent socket"
  `network.opcode` ERROR in `SendPacket` on `!IsBot()`. Headless bots are socketless by
  design, so that log fired for every packet routed to a bot (~23k lines/session); the
  packet is still dropped and real-client socket errors still log. See the bot section.
```

Read the section first to match the existing bullet style/placement.

- [ ] **Step 2: Commit**

```bash
git add CHANGELOG-custom.md
git commit -m "docs: changelog for bot null-socket log suppression (WorldSession::IsBot)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Build, deploy, verify

**Files:** none (build/run only)

- [ ] **Step 1: Full rebuild**

```bash
cd /e/TrinityCore
docker images trinitycore:local --format 'pre-build: {{.ID}}'
docker compose --progress plain build > build-quietlog.log 2>&1; echo "BUILD_EXIT=$?" >> build-quietlog.log
```

- [ ] **Step 2: Verify the build compiled**

```bash
grep BUILD_EXIT build-quietlog.log                          # expect BUILD_EXIT=0
grep -niE "error:|failed to solve|lease does not exist" build-quietlog.log | head   # expect empty
docker images trinitycore:local --format '{{.ID}} {{.CreatedAt}}'   # expect a NEW id
```
If `BUILD_EXIT` is non-zero or the image id is unchanged, fix and rebuild before proceeding.

- [ ] **Step 3: Deploy**

```bash
docker compose up -d
docker logs tc-worldserver 2>&1 | grep -iE "World initialized|ready\.\.\." | tail -2
```

- [ ] **Step 4: Verify the log is quiet (integration gate)**

Add a couple of bots (`.bot add …`), have them follow/fight for a bit, then:

```bash
docker logs tc-worldserver 2>&1 | grep -c "Prevented sending"
```
Expect **~0** (down from ~23k). It should NOT grow as bots move/fight. (Real-client opcode
errors, if any, would still appear — those are not gated.)

- [ ] **Step 5: Clean up the build log**

```bash
rm -f build-quietlog.log
```

---

## Task 4: PR + merge

- [ ] **Step 1: Push and open PR**

```bash
git push -u origin claude/quiet-bot-socket-logging
gh pr create --repo mountogdengc/TrinityCore --base main --head claude/quiet-bot-socket-logging \
  --title "fix(bots): silence expected null-socket log spam (WorldSession::IsBot)" --body "$(cat <<'EOF'
Headless bots run on a socketless `WorldSession` by design, so `SendPacket` logged a `network.opcode` ERROR ("Prevented sending of … to non existent socket …") for every packet routed to a bot — ~23k lines per session, drowning `Server.log`.

- Add a first-class `WorldSession::IsBot()` flag, set by `BotMgr` on the bot session.
- Gate only the non-existent-socket log on `!IsBot()`. The packet is still dropped; real-client socket errors and all other `Prevented sending …` logs are untouched.

Verified in-game: `grep -c "Prevented sending"` drops from ~23k to ~0 during bot combat.

Spec: `docs/superpowers/specs/2026-06-25-quiet-bot-socket-logging-design.md`

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 2: Merge (per user) and sync**

```bash
gh pr merge <PR#> --repo mountogdengc/TrinityCore --merge --delete-branch
git checkout main && git pull --ff-only origin main
```

---

## Notes / guardrails

- `IsBot()` is intentionally general (not tied to the `"BOT"` account name) so other bot
  logic can reuse it.
- Do not gate the other `Prevented sending …` logs — they indicate real bugs even for bots.
- The `docker/worldserver/entrypoint.sh` working tree should be clean before building
  (no stray debug toggles).
