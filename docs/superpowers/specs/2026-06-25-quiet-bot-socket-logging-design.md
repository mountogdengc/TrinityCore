# Silence expected null-socket log noise for bot sessions

**Date:** 2026-06-25
**Status:** approved design, pre-implementation
**Area:** core (`WorldSession`) + player-bots (`BotMgr`)

## Problem

`WorldSession::SendPacket` (`src/server/game/Server/WorldSession.cpp:254-258`) logs a
`TC_LOG_ERROR("network.opcode", "Prevented sending of {} to non existent socket {} to {}")`
whenever a session has no socket on the target connection. Headless player-bots run on a
**WorldSession with a null socket by design**, so this fires for every world packet
routed to a bot — ~19k `SMSG_ON_MONSTER_MOVE` plus thousands of other movement/object
packets per session (23k+ lines observed in one session). It drowns `Server.log` and
hides real issues.

For a *real* client, a missing socket genuinely is an error worth logging. The fix must
keep that, and only silence the expected bot case.

## Goal

Stop logging the "non existent socket" error for bot sessions, with **no behavior
change** otherwise (the packet is still dropped; real-client socket errors still log).

## Design

### 1. An explicit `IsBot()` marker on `WorldSession`

Today a bot session is only distinguishable by its stringly-typed account name `"BOT"`
(`BotMgr.cpp:137`). Add a first-class flag:

- `WorldSession.h`: `bool _isBot = false;` member; public `bool IsBot() const { return _isBot; }`
  and `void SetBot() { _isBot = true; }`.
- `BotMgr` calls `session->SetBot()` immediately after `new WorldSession(...)` (line ~137).

This is a clean, reusable marker (more honest than matching the `"BOT"` name) that other
bot logic can use later.

### 2. Gate only the null-socket log

In `WorldSession::SendPacket`, the block:

```cpp
if (!m_Socket[conIdx])
{
    TC_LOG_ERROR("network.opcode", "Prevented sending of {} to non existent socket {} to {}", ...);
    return;
}
```

becomes:

```cpp
if (!m_Socket[conIdx])
{
    if (!IsBot())   // bots are socketless by design -- expected, not an error
        TC_LOG_ERROR("network.opcode", "Prevented sending of {} to non existent socket {} to {}", ...);
    return;
}
```

The early `return` is unchanged for everyone, so the only observable difference is the
absence of the log line for bots.

### Scope guard

- **Only** the "non existent socket" log is gated. The other `Prevented sending …` logs
  in the same function (invalid opcode, missing handler, instance-only opcode, disabled
  opcode) signal real bugs even for a bot and are left intact.
- **No log-config change.** Raising `Logger.network.opcode` to silent would also hide
  real opcode errors for actual players — rejected.
- **Not** suppressing packet generation for bots (a much larger change across every
  `SendDirectMessage` path). We only remove the log noise.

## Files

- **Modify** `src/server/game/Server/WorldSession.h` — `_isBot` + `IsBot()`/`SetBot()`.
- **Modify** `src/server/game/Server/WorldSession.cpp` — gate the null-socket log on `!IsBot()`.
- **Modify** `src/server/scripts/Custom/Bots/BotMgr.cpp` — `session->SetBot()` after session creation.

## Testing

- **In-game:** after a bot fight, `docker logs tc-worldserver 2>&1 | grep -c "Prevented sending"`
  should be ~0 (down from 23k). A non-bot opcode error (if induced) would still log.
- No unit test: this is a one-branch logging gate on engine state; the integration gate
  is the in-game log count.

## Out of scope

- Eliminating the wasted packet construction for bots (broad optimization).
- Any other bot-session log noise beyond the null-socket line.
