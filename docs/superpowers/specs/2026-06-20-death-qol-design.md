# Death QoL — chat while dead, no-run revive, bot auto-revive

**Date:** 2026-06-20
**Status:** Design (approved for planning)

## Summary

Three quality-of-life relaxations of the dead state, so a GM/player (and bots)
aren't punished by the corpse run and the dead-chat block:

1. **Chat while dead** — config-gated bypass of the server-side `/say` `/yell`
   `/emote` dead blocks. Primarily matters for **bots** (no client) and for
   silencing the "you can't talk while dead" warning on command channels.
2. **No-run self-revive** — `.revive corpse` teleports the caster to their corpse
   and resurrects in place, so you return to where you died with no graveyard run.
3. **Bot auto-revive** — a dead bot automatically resurrects at the master, but
   **only while the master is alive** (so it doesn't walk back into the same death
   while you're also down).

Mounting while dead was considered and **dropped** (the retail client blocks ghost
mounting client-side, and the no-run revive removes the need to travel anyway).

All three are **off by default** — vanilla behavior is unchanged unless a server
opts in.

## Goals

- Let a GM resurrect at their death spot in one command, issuable while dead.
- Let bots resurrect themselves when their master is alive, with no manual step.
- Remove the server-side dead-chat block for servers/bots that want it.
- Keep every change config-gated and default-off (vanilla-safe).

## Non-goals

- Changing the **retail client's** own dead-state restrictions (we can't; we only
  control the server). `/say` from a living-only client UI stays client-blocked.
- Mounting while dead.
- Reworking graveyard placement or resurrection sickness.
- General bot rotation/combat behavior (that's M4, separate).

## Configuration

New worldserver config keys (also surfaced as entrypoint env, so they toggle on a
**restart with no rebuild**, matching the existing `Instance.IgnoreLevel` pattern):

| Config key | Env | Default | Meaning |
|------------|-----|---------|---------|
| `Custom.AllowChatWhileDead` | `TC_ALLOW_CHAT_WHILE_DEAD` | `0` | Bypass server dead-checks for `/say` `/yell` `/emote`. |
| `Custom.BotAutoRevive` | `TC_BOT_AUTO_REVIVE` | `0` | Enable bot auto-revive. |
| `Custom.BotAutoReviveDelayMs` | `TC_BOT_AUTO_REVIVE_DELAY_MS` | `5000` | Delay after death before a bot auto-revives. |

`.revive corpse` needs no config — it's a GM command gated by the existing
`RBAC_PERM_COMMAND_REVIVE`.

## Component 1 — Chat while dead

**Where:** `src/server/game/Handlers/ChatHandler.cpp`, the `CHAT_MSG_SAY` /
`CHAT_MSG_EMOTE` / `CHAT_MSG_YELL` cases (currently `if (!sender->IsAlive()) return
ChatMessageResult::PlayerDead;` at ~lines 249 / 264 / 279).

**Change:** guard each dead-return with the config:
`if (!sender->IsAlive() && !sWorld->getBoolConfig(CONFIG_CUSTOM_ALLOW_CHAT_WHILE_DEAD)) return ChatMessageResult::PlayerDead;`

**Notes / boundary of effect:**
- `ParseCommands` (`ChatHandler.cpp:233`) already runs *before* these checks, so GM
  commands were never blocked here. This change removes the *warning* and lets
  dead players actually `/say`/`/yell`/`/emote` server-side.
- The retail **client** still greys out `/say` for a ghost, so the visible win on a
  real client is via channels that already work while dead (`/party`, `/guild`,
  whisper). The decisive beneficiary is the **headless bot**, whose only gate is
  this server check.

## Component 2 — No-run self-revive (`.revive corpse`)

**Where:** `HandleReviveCommand` in `src/server/scripts/Commands/cs_misc.cpp:625`.

**Change:** parse an optional `corpse` argument.
- `.revive [target]` — unchanged stock behavior (resurrect in place / offline res).
- `.revive corpse` — operate on **self**: read `GetCorpseLocation()`, teleport
  there, then `ResurrectPlayer(...)` + `SpawnCorpseBones()` + `SaveToDB()`.

**Flow:**
1. Caster types `.revive corpse` (from `/party` while dead, or any time).
2. If the caster has no corpse (`GetCorpseLocation()` invalid / already alive with
   no bones), send a clear message and no-op — do not teleport.
3. Otherwise `TeleportTo(corpseLocation)`, then resurrect at that spot.
4. Result: alive at the death location, corpse bones cleaned up.

**Edge cases:**
- Corpse in an instance the caster can no longer enter → attempt teleport; if it
  fails, fall back to in-place resurrect and warn.
- Caster is alive → message "you are not dead", no-op.
- `corpse` arg only ever targets self (ignore any trailing player name to avoid
  ambiguous "teleport someone else to their corpse" semantics).

## Component 3 — Bot auto-revive

**Where:** bot policy + `BotMgr` update loop (`src/server/scripts/Custom/Bots/`).
A pure helper holds the decision; `BotMgr` performs the side effects.

**Pure policy helper** (unit-testable, no engine state):
```
ShouldBotAutoRevive(enabled, botIsDead, masterIsAlive, msSinceBotDeath, delayMs)
  -> bool
```
Returns true only when: `enabled && botIsDead && masterIsAlive &&
msSinceBotDeath >= delayMs`. The **master-alive** gate is the key requirement: a
bot will *not* auto-revive while the master is also dead.

**BotMgr side:**
- Track each bot's time-of-death (set when the bot transitions to dead).
- Each tick, for dead bots whose master is alive, when the helper returns true:
  `ResurrectPlayer(1.0f)`, `SpawnCorpseBones()`, teleport to the master's position
  (reusing the existing follow/teleport machinery), and resume follow/assist.
- Clear the death timestamp on revive.

**Edge cases:**
- Master offline / no master → no auto-revive (treated as master-not-alive).
- Bot already resurrecting / in a cross-map port → skip this tick, retry next.
- Config disabled → never auto-revive (bot stays dead; manual handling only).

## Testing

**Unit tests** (`tests/game/`):
- `ShouldBotAutoRevive` truth table: disabled; bot alive; master dead; before
  delay; at/after delay with master alive (the only true case).
- `.revive corpse` argument parsing: `""` vs `"corpse"` vs `"corpse extra"` vs a
  player name → correct branch selection.

**Manual:**
- Die at a known spot, release to graveyard, `.revive corpse` from `/party` →
  appear alive at the death spot.
- `Custom.AllowChatWhileDead=1`: dead bot can chat/command server-side; warning
  gone.
- `Custom.BotAutoRevive=1`: kill a grouped bot while you're alive → it revives at
  you after the delay; kill it while you're also dead → it stays down until you're
  up.

## Documentation

- Add a **Death QoL** entry to `CHANGELOG-custom.md` (features + the new config
  flags + the `.revive corpse` command), per the upkeep rule in `CLAUDE.md`.
- Note bot auto-revive in `src/server/scripts/Custom/Bots/ROADMAP.md`.
- Document the three config flags / env vars alongside `Instance.IgnoreLevel` in
  the worldserver entrypoint and `CLAUDE.md`.

## Open questions

None — scope is settled for a single implementation plan.
