# Player-bot overlay (Milestone 1)

This overlay adds an experimental **server-side player-bot** capability to the
TrinityCore (`master`) build produced by this Docker wrapper. It is applied onto
the upstream source at image-build time, so the core source stays an unmodified
clone of `TC_REPO`/`TC_BRANCH`.

## What M1 does

A real character is loaded into the world as a "headless" bot — a genuine
`Player` driven by a `WorldSession` that has **no client socket**. To the rest of
the server it is an ordinary in-world player (visible, targetable, takes damage).
There is **no AI yet**: the bot just stands where the character logged out.

Commands (require GM RBAC, `RBAC_PERM_COMMAND_GM`):

| Command | Effect |
| --- | --- |
| `.bot add <CharacterName>` | Load that character into the world as a bot. |
| `.bot remove <CharacterName>` | Save and despawn the bot. |
| `.bot count` | Report the number of active bots (console-allowed). |

Constraints in M1:
- The character's **owning account must have no active session** (don't bot a
  character whose account is logged in).
- Bots are intentionally **not** registered in `World::m_sessions`, so they won't
  appear in `/who`, can't be whispered, and can't be grouped yet. That changes in
  a later milestone (which also needs the idle-kick / RBAC handling already
  prepared here).

## How it works

`BotMgr` constructs a `WorldSession` with a null `std::shared_ptr<WorldSocket>`.
`WorldSession::SendPacket()` null-checks the socket, so the entire login packet
flow becomes a harmless no-op. The session reuses the **real** asynchronous login
path (`LoginQueryHolder` → `WorldSession::HandlePlayerLogin`) via a small added
entry point, `WorldSession::LoadBotCharacter`. `BotMgr::Update()` pumps each bot
session every world tick (with a `MapSessionFilter`) so the async login callback
fires and the session stays alive.

## Files

```
overlay/
  apply-overlay.sh        # copies scripts + runs the core patcher
  apply_core_patches.py   # idempotent, anchored edits to 4 core files
  src/server/scripts/Custom/Bots/
    BotMgr.h / BotMgr.cpp  # headless-session manager
    cs_bot.cpp             # .bot command + per-tick world script
```

### Core edits made by `apply_core_patches.py`

1. `WorldSession.h` — declare `void LoadBotCharacter(ObjectGuid)`.
2. `Handlers/CharacterHandler.cpp` — define it (where `LoginQueryHolder` is in
   scope), reusing the real async login.
3. `Server/WorldSession.cpp` — guard the idle-connection kick against a null
   socket (`m_Socket[CONNECTION_TYPE_REALM] && IsConnectionIdle() && ...`).
4. `scripts/Custom/custom_script_loader.cpp` — register `AddSC_bots()`.

The patcher is **idempotent** (re-running is a no-op) and **fails loudly** if an
upstream anchor moves, so a future `master` bump surfaces the breakage at build
time instead of miscompiling. `CMake`'s `CollectAndAddSourceFiles` picks up the
`Custom/Bots` sources automatically — no CMake changes needed.

## Building with / without the overlay

The overlay is on by default. Disable it with a build arg:

```sh
docker compose build --build-arg BOTS_OVERLAY=0 trinitycore
```

## Pinning note

Patches are anchored to source text, not line numbers, so they tolerate normal
upstream churn. For fully reproducible builds, pin `TC_BRANCH` to a specific
commit; if a future bump moves an anchor, the build fails with a clear
`[FAIL] ... anchor not found` message naming the file to re-anchor.
