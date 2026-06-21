# AGENTS.md — TrinityCore retail fork + player-bots

This is a **full TrinityCore `master` (retail, client 12.0.5 / build 67823) fork**
run via a Docker Compose wrapper, plus an in-development **player-bot** system.
For the full bring-up story and runbook see **`SETUP-NOTES.md`**.

## Build & run (read this first)

- The image `trinitycore:local` is built by the **`bnetserver`** service (it owns
  the `build:` block; `worldserver` reuses the image). So:
  - **Rebuild the binaries:** `docker compose build` (a full ~30-min recompile).
    `docker compose build worldserver` or `--build worldserver` does **nothing**
    (worldserver has no build context) — a common trap.
  - **Bring up:** `docker compose up -d` (db + bnetserver + worldserver).
- **Entrypoints are bind-mounted** (`docker/*/entrypoint.sh`, `entrypoint-lib.sh`),
  so entrypoint/config-logic changes apply on a **restart** with no rebuild.
  **C++/source changes require a full `docker compose build`.**
- ⚠️ **Always verify a build actually compiled:** check the `trinitycore:local`
  image **ID changed** after building (`docker images trinitycore:local`). A failed
  build leaves the old image in place. **Never pipe `docker compose build` to
  `tail`/`head`** — the pipe's exit code masks the build's. For real errors use
  `docker compose build --progress=plain > log 2>&1; echo $?` and grep the log for
  `error:`. (BuildKit's normal output is just a dashboard link; the compile log
  isn't on stdout.)
- Map data (`/data`) lives in the external named volume **`trinitycore_data`**
  (WSL2 fs). **Never** bind-mount it from a Windows drive — it's ~100-500x slower
  for the 259k small map/vmap/mmap files. Backup copy is in `./data`.

## Retail-specific facts (vs legacy 3.3.5)

- Master builds **`bnetserver`** (Battle.net login on 1119 + login REST 8081),
  not `authserver`. There are **4 databases**: auth, characters, world,
  **hotfixes** (updater bitmask `Updates.EnableDatabases=15`).
- `Updates.Redundancy=0` — the fork's SQL tree triggers re-apply failures
  otherwise (duplicate-column crash loop).
- The bundled bnetserver cert has `CN=*.*`, which forces the login REST into
  plain-HTTP dev mode; modern clients need HTTPS, so the entrypoint generates a
  `CN=127.0.0.1` cert. (Was the cause of `WOW51900317`.)

## Accounts / testing

- Login: **`david@local` / `Password1`** (GM/Administrator). Game account `1#1`.
- **SOAP console** is enabled (port 7878). Run console commands without the game:
  ```bash
  curl -s -u "1#1:Password1" -H 'Content-Type: application/xml' \
    -d '<?xml version="1.0"?><SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ns1="urn:TC"><SOAP-ENV:Body><ns1:executeCommand><command>bot count</command></ns1:executeCommand></SOAP-ENV:Body></SOAP-ENV:Envelope>' \
    http://127.0.0.1:7878/
  ```
  (Bnet email creds get 401; use the `1#1` game account.) The console can't run
  player-context commands (e.g. `.bot follow`).
- The interactive worldserver console needs a real TTY: `docker attach tc-worldserver`
  (detach with Ctrl-P, Ctrl-Q — never Ctrl-C).

## Player-bots (`src/server/scripts/Custom/Bots/`)

Headless bots: a real `Player` driven by a `WorldSession` with a **null socket**.
`BotMgr` owns the sessions and is pumped each tick by `bot_worldscript`.

- **Roadmap:** M1 spawn static headless bot ✅ · M2 follow + zone with master,
  incl. dungeons ✅ · M3 group ✅ + assist combat ✅ · M4 data-driven rotation
  engine (next) · M5+ questing/looting/gear/parties.
- Commands: `.bot add/remove/follow/stay/count` (GM). `add`/`remove`/`stay`/`count`
  are console-allowed; `follow` needs an in-world player.
- **M3 combat** (`BotCombatPolicy` + `BotMgr::SelectAssistTarget`): the bot
  assists the master's melee victim / in-combat selected target, and defends
  itself (its attackers + combat refs). ⚠️ Eligibility is checked with the
  **master's** `IsValidAttackTarget`, not the bot's — a headless bot's own
  `CanSeeOrDetect` gate is unreliable (it returns false for mobs that can see and
  aggro *it*, so `bot->IsValidAttackTarget` wrongly rejects them). Root cause of
  the bot-side visibility gate is still open; the master-validity routing is the
  working M3 workaround.
- ⚠️ **Don't `.character level` a spawned bot** (console `GiveLevel`): it leaves
  the headless bot's talent/spec state inconsistent and crashes the worldserver on
  the next save/`.bot remove`. Level bots by playing/escorting instead. A
  naturally-leveled bot (e.g. a level-10 with no talents chosen) saves fine.
- ⚠️ **`PSendSysMessage` here is printf-style** (`fmt::make_printf_args`): use
  `%s`/`%zu`, **not** `{}`. `{}` prints literally and drops the argument. (Use
  `{}` only for `TC_LOG_*`.)
- **Cross-map teleport is a 3-step state machine the socketless bot drives itself**
  (no client to send acks): after `TeleportTo`, feed `HandleSuspendTokenResponse`
  (state `WaitingForSuspendTokenResponse`) then `HandleMoveWorldportAck`
  (`WaitingForWorldPortAck`); same-map/near teleports use `HandleMoveTeleportAck`
  (`WaitingForTeleportAck`). ⚠️ Drive this **before** any `IsInWorld()` guard — a
  cross-map port removes the bot from the old map (`IsInWorld()==false`) *before*
  adding it to the new one, so bailing on `!IsInWorld()` first strands the bot
  mid-port (gone from A, never arrives at B). This was the RFC "never arrives" bug.
- **Dungeon entry:** a bot following a (usually GM) master into an instance must
  itself pass the LEVEL gate the GM bypasses (`MapDifficultyConditions` +
  `access_requirement`, see `Player::Satisfy`) — a low-level bot is silently turned
  away with `SMSG_TRANSFER_ABORTED`. The worldserver entrypoint sets
  `Instance.IgnoreLevel = 1` (env `TC_INSTANCE_IGNORE_LEVEL`) so any-level bots can
  zone in. Instance *sharing* also needs the bot in the master's group
  (`BotMgr::EnsureGrouped`, which forms a party if the master is solo).

## Git / pushing

- `origin` is **SSH** (`git@github.com:mountogdengc/TrinityCore.git`) and is a
  **GitHub fork of TrinityCore/TrinityCore** — required so old upstream commits
  with malformed author lines are grandfathered (a non-fork repo gets fsck-rejected
  on push, surfacing as an opaque HTTP 500 over HTTPS). `http.sslBackend=openssl`.
- The lightweight overlay variant lives at `mountogdengc/TrinityCore-overlay`.

## Gotchas

- A separate **`solo-wow`** stack (AzerothCore / WotLK 3.3.5 + mod-playerbots —
  *incompatible* with this retail fork) auto-restarts and steals ports
  3306/3724/8085. Stop it: `docker stop solo-wow-mysql solo-wow-authserver solo-wow-worldserver`.
- After a Docker/WSL restart a container can drop off the network ("Unknown MySQL
  server host 'db'"): `docker compose down && docker compose up -d`.
- WSL2 memory is capped to 16 GB in `C:\Users\david\.wslconfig`.
