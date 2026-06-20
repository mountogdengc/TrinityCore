# Death QoL — chat while dead, no-run revive, bot auto-revive

**Date:** 2026-06-20
**Status:** Design (approved for planning)

## Summary

Quality-of-life relaxations of the dead state, so a player (and bots) aren't
punished by the corpse run and the dead-chat block:

1. **Chat while dead** — config-gated bypass of the server-side `/say` `/yell`
   `/emote` dead blocks. Primarily matters for **bots** (no client) and for
   silencing the "you can't talk while dead" warning on command channels.
2. **Player auto-revive at corpse** (primary player path, **no chat needed**) —
   when enabled, a dead player is automatically teleported to their corpse and
   resurrected after a short delay. Zero input: you die at the murlocs, and a few
   seconds later you're back on them. Applies to **all** players (behind the
   default-off flag).
3. **Manual `.revive corpse`** — the same teleport-to-corpse + resurrect, on
   demand, for when auto-revive is off or you don't want to wait the delay. Plain
   `.revive` stays stock.
4. **Bot auto-revive** — a dead bot automatically resurrects at the master, but
   **only while the master is alive** (so it doesn't walk back into the same death
   while you're also down).

Mounting while dead was considered and **dropped** (the retail client blocks ghost
mounting client-side, and the no-run revive removes the need to travel anyway).
Item- and spirit-healer-based revive paths were considered and dropped in favor of
the zero-input auto-revive.

All three are **off by default** — vanilla behavior is unchanged unless a server
opts in.

## Goals

- Let a player get back to their death spot with **no input and no chat** when
  auto-revive is on (the murloc-grind use case).
- Provide a manual `.revive corpse` for the same return, on demand.
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
| `Custom.PlayerAutoReviveAtCorpse` | `TC_PLAYER_AUTO_REVIVE_AT_CORPSE` | `0` | Auto teleport-to-corpse + resurrect a dead player. Applies to all players. |
| `Custom.PlayerAutoReviveDelayMs` | `TC_PLAYER_AUTO_REVIVE_DELAY_MS` | `5000` | Delay after death before a player auto-revives. |
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

## Shared mechanism — "return to corpse and resurrect"

Components 2 and 3 share one operation, factored into a single reusable helper on
`Player` (or a free function in the custom layer):

```
ReturnToCorpseAndResurrect(Player* p)
  - if !p->HasCorpse() / GetCorpseLocation() invalid -> false (no-op)
  - TeleportTo(p->GetCorpseLocation())
  - p->ResurrectPlayer(1.0f)        // full HP, no resurrection sickness
  - p->SpawnCorpseBones()
  - return true
```

Resurrection sickness is intentionally **not** applied — this mirrors normal
corpse-reclaim (running to your body), not spirit-healer revival.

## Component 2 — Player auto-revive at corpse (primary, no chat)

**Where:** a `PlayerScript` death hook in the custom layer (e.g.
`OnPlayerJustDied` / death-state transition), scheduling a delayed action; the
action runs `ReturnToCorpseAndResurrect`.

**Flow:**
1. Player dies. If `Custom.PlayerAutoReviveAtCorpse` is off → do nothing (vanilla).
2. On, schedule a revive `Custom.PlayerAutoReviveDelayMs` after death.
3. When the timer fires and the player is still dead → `ReturnToCorpseAndResurrect`.
4. The delay lets the death area's mobs leash/reset before you reappear on them.

**Scope:** all players (the flag is the gate). No per-character opt-out in this
iteration — turning the flag off restores vanilla for everyone.

**Edge cases:**
- Player already resurrected (spirit healer, soulstone, release+reclaim) before the
  timer fires → skip (no-op).
- Player logged out / corpse gone before the timer → skip.
- **Battlegrounds / arenas:** death is a core mechanic there — **exclude** these
  maps from auto-revive. (Open: whether to also exclude dungeons/raids; default is
  to leave them in, since this is a solo+bot server. Flagged for plan review.)
- Corpse teleport fails (instance no longer enterable) → fall back to in-place
  resurrect so the player isn't stuck as a ghost.

## Component 3 — Manual `.revive corpse`

**Where:** `HandleReviveCommand` in `src/server/scripts/Commands/cs_misc.cpp:625`.

**Change:** parse an optional `corpse` argument.
- `.revive [target]` — unchanged stock behavior (resurrect in place / offline res).
- `.revive corpse` — operate on **self** via `ReturnToCorpseAndResurrect`.

**Flow:**
1. Caster types `.revive corpse` (from `/party` while dead, or any time).
2. No corpse → clear message, no-op (no teleport).
3. Otherwise teleport to corpse and resurrect there.

**Edge cases:**
- Caster is alive → "you are not dead", no-op.
- `corpse` arg only ever targets self (ignore any trailing player name to avoid
  ambiguous "teleport someone else to their corpse" semantics).
- Needs no config — gated by the existing `RBAC_PERM_COMMAND_REVIVE`. Works
  regardless of the auto-revive flag.

## Component 4 — Bot auto-revive

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
- Player auto-revive eligibility (pure helper): disabled; alive; before delay; on a
  battleground/arena map (excluded); eligible (the true case).
- `.revive corpse` argument parsing: `""` vs `"corpse"` vs `"corpse extra"` vs a
  player name → correct branch selection.

**Manual:**
- `Custom.PlayerAutoReviveAtCorpse=1`: die at the murlocs → a few seconds later
  you're alive back at the murlocs, no run, no sickness.
- Die at a known spot, release to graveyard, `.revive corpse` from `/party` →
  appear alive at the death spot (works with auto-revive off).
- `Custom.AllowChatWhileDead=1`: dead bot can chat/command server-side; warning
  gone.
- `Custom.BotAutoRevive=1`: kill a grouped bot while you're alive → it revives at
  you after the delay; kill it while you're also dead → it stays down until you're
  up.

## Documentation

- Add a **Death QoL** entry to `CHANGELOG-custom.md` (features + the new config
  flags + the `.revive corpse` command + player/bot auto-revive), per the upkeep
  rule in `CLAUDE.md`.
- Note bot auto-revive in `src/server/scripts/Custom/Bots/ROADMAP.md`.
- Document the five config flags / env vars alongside `Instance.IgnoreLevel` in the
  worldserver entrypoint and `CLAUDE.md`.

## Open questions

- **Auto-revive on dungeon/raid maps:** battlegrounds and arenas are excluded
  (death is a core mechanic). Should dungeons/raids also be excluded, or left in
  (auto-reviving at corpse on a wipe)? Default for this iteration: **left in**;
  revisit in the plan if it causes problems.
