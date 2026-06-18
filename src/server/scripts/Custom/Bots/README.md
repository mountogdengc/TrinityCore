# Player bots (Milestone 1)

Experimental server-side player bots for this TrinityCore fork.

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

M1 constraints:
- The character's **owning account must have no active session**.
- Bots are intentionally **not** registered in `World::m_sessions`, so they won't
  appear in `/who`, can't be whispered, and can't be grouped yet. That changes in
  a later milestone.

## How it works

`BotMgr` constructs a `WorldSession` with a null `std::shared_ptr<WorldSocket>`.
`WorldSession::SendPacket()` null-checks the socket, so the entire login packet
flow becomes a harmless no-op. The session reuses the **real** asynchronous login
path (`LoginQueryHolder` → `WorldSession::HandlePlayerLogin`) via a small added
entry point, `WorldSession::LoadBotCharacter`. `BotMgr::Update()` pumps each bot
session every world tick (with a `MapSessionFilter`) so the async login callback
fires and the session stays alive.

## Core engine touch-points

Beyond the files in this directory, M1 adds three minimal edits to core:

1. `game/Server/WorldSession.h` — declares `void LoadBotCharacter(ObjectGuid)`.
2. `game/Handlers/CharacterHandler.cpp` — defines it (where `LoginQueryHolder` is
   in scope), reusing the real async login.
3. `game/Server/WorldSession.cpp` — guards the idle-connection kick against a
   null socket (`m_Socket[CONNECTION_TYPE_REALM] && IsConnectionIdle() && ...`).

Registration is wired in `scripts/Custom/custom_script_loader.cpp`
(`AddSC_bots()`); CMake collects `Custom/Bots/*.cpp` automatically.
