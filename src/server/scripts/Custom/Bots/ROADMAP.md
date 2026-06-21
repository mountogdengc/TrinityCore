# Player-bot roadmap

Headless player-bots: a real `Player` driven by a `WorldSession` with a **null
socket**, owned by `BotMgr` and pumped each world tick by `bot_worldscript`. See
`README.md` for the M1 internals and `CLAUDE.md` (repo root) for the build/run and
the cross-cutting gotchas.

Status: **M1 ✅ · M2 ✅ · M3 ✅ · M4 cohort foundation (started) · M5–M8 (planned) · rotation engine (deferred, separate track)**

GM commands: `.bot add/remove/follow/stay/count`. `add`/`remove`/`stay`/`count`
run from the console/SOAP; `follow` needs an in-world player.

---

## M1 — Spawn a static headless bot ✅

Load a named character into the world as a bot with no client and no AI.

- `BotMgr::AddBot` builds a `WorldSession` with a null `WorldSocket` and reuses the
  real async login machinery (`LoadBotCharacter` → `LoginQueryHolder` →
  `HandlePlayerLogin`); the holder callback fires from the session's `Update()`.
- `SendPacket`/`SendDirectMessage` null-check the socket, so the whole packet flow
  is a harmless no-op for a bot.
- The bot is **not** registered in `World::m_sessions`, so it's "offline" to
  `/who`, `pinfo`, `tele name`, etc.

## M2 — Follow + zone with the master ✅

Chase the master and travel with them anywhere, including across maps and into
instances. `BotMgr::UpdateFollow`, every `BOT_FOLLOW_INTERVAL_MS` (500 ms):

- Same-map close → `MoveFollow`; fell behind (mount/taxi/teleport) → blink via
  `TeleportTo` to the master's position **and instance id**.
- **Cross-map port is a 3-step state machine the socketless bot drives itself**
  (no client to ack): `WaitingForSuspendTokenResponse` → `HandleSuspendTokenResponse`,
  `WaitingForWorldPortAck` → `HandleMoveWorldportAck`; near teleports use
  `HandleMoveTeleportAck`. This runs **before** the `IsInWorld()` guard — a port
  removes the bot from the old map before adding it to the new one, so guarding
  first strands it mid-port.
- **Dungeon entry:** a low-level bot must pass the instance LEVEL gate the GM
  master bypasses → the worldserver runs with `Instance.IgnoreLevel = 1`.

## M3 — Group + assist combat ✅

Join the master's party and fight alongside them.

- `BotMgr::EnsureGrouped` puts the bot in the master's party (forming one if the
  master is solo) so they share the same dungeon/raid instance.
- `BotMgr::SelectAssistTarget` + `BotCombatPolicy` pick the target: master's melee
  victim → master's selected target **while the master is in combat** → whatever is
  attacking the bot (`getAttackers` + combat refs). `Attack()` + `MoveChase()`
  drive the melee; a valid victim is kept through brief target gaps
  (`BOT_STALE_COMBAT_MS`).
- ⚠️ Eligibility uses the **master's** `IsValidAttackTarget`, not the bot's: a
  headless bot's own `CanSeeOrDetect` gate is unreliable (see Open issues).
- Scope: **melee auto-attack only**, no spells yet — that's M4.

---

## M4 — Cohort foundation (started)

Introduce the per-character companion-cohort model on top of the existing M3
runtime. This is the M4 scope **of record** (see the spec/plan below). The
data-driven rotation engine that earlier drafts folded into M4 is now a
**separate, deferred track** (see *Rotation engine* below); the **M3 melee loop
stays the combat baseline** until that track lands.

- **Started in this slice:** per-owner cohort persistence in the character DB,
  owner-scoped auto-spawn configuration, and pure policy helpers for level-band,
  catch-up XP gating, and continuity auto-accept decisions.
- **Scope:** persistent per-character cohort assignment; active vs benched
  membership; per-character auto-spawn config; cohort spawn-on-login flow; target
  level-band evaluation; catch-up XP state evaluation.
- **Success criteria:** logging into a character can auto-bring that character's
  active cohort (when enabled); different characters own distinct cohorts; the
  system can classify each companion as below / in / above band.
- **Spec prerequisite (carried forward for the rotation track):** specs unlock at
  level 10. ⚠️ Don't `.character level` a spawned bot to reach that — the console
  boost crashes the worldserver on save; level by playing/escorting (see Open
  issues).

Design of record: `docs/superpowers/specs/2026-06-20-companion-cohorts-design.md`,
`docs/superpowers/plans/2026-06-20-companion-cohorts.md`.

## M5 — Continuity (planned)

Lifecycle behaviors that keep the cohort together during real play: accept
resurrection; death recovery + rejoin when no res is available; accept summons;
basic travel-continuity handling for party cohesion. Success: ordinary deaths and
summons don't permanently fracture the cohort, and companions rejoin without a
manual despawn/respawn.

## M6 — Visibility (planned)

Expose companion progression to the owner simply: cohort status view; quest list
with per-objective progress (e.g. `3/12 Northwatch Lugs`); compatibility /
blocked reasons (wrong faction, level mismatch, missing prerequisite chain, zone
incompatibility); catch-up XP state. Success: the owner can see why a companion is
behind or blocked, and enough quest progress to help it catch up.

## M7 — Party progression (planned)

Real grouped progression: XP gain while adventuring with the owner; catch-up XP
multiplier when below band (auto-off once back in band); grouped quest
advancement; rough level parity over time — all from normal play, not DB edits.

## M8 — Adventure actions (planned)

First practical self-maintenance while the owner is present: loot evaluation;
equipping obvious upgrades; basic vendor repair; selling junk. Success: companions
stay reasonably usable without per-step manual intervention.

---

## Rotation engine — real ability usage (deferred, separate track)

**Not part of the M4 cohort milestone.** Parked as its own track that layers on
top of the M3 baseline (target selection, chase/movement, stale-combat handling,
melee fallback) once the cohort foundation is stable. Sequencing relative to
M5–M8 is open. Until it lands, melee auto-attack remains the combat baseline.

- **Intended data source:** the Assisted Combat DB2 tables (`assisted_combat`,
  `assisted_combat_rule`, `assisted_combat_step`) — Blizzard's per-spec ability
  priority lists, already loaded in-core. Retail resolves the Single-Button
  Assistant **client-side**, which a headless bot can't use — so the plan is a
  **server-side resolver** that, per combat tick, walks the rule/step lists for
  the bot's spec, casts the top castable ability at the current victim, and falls
  back to melee when nothing is castable.
- **Prior art (exploratory spike, not the plan):**
  `sql/custom/spike_assisted_combat_hunter_lowlevel.sql` is a **client-side**
  hotfix experiment — it pushes custom `assisted_combat` rows for the Hunter
  "Initial" (sub-10, pre-spec) spec to a *real client* to test whether the 12.0
  client renders a rotation before level 10. It does **not** drive a bot (no
  server-side resolver, no client) and is tracked in `CHANGELOG-custom.md` as a
  spike, separate from these milestones.
- **Open questions to design:** how to evaluate each rule's condition columns;
  cooldown/GCD/resource gating; AoE vs single-target step targeting; a fallback
  rotation for specs/levels with no Assisted Combat data; and how a no-spec
  low-level bot behaves (likely melee until it has a spec).

## Later — multi-bot parties & tactics (parking lot)

Beyond the cohort milestones, recorded so they aren't lost (see the spec's
parking lot for the full list): multi-bot groups with role awareness
(tank/heal/dps) enough to **fill a dungeon group** behind one player;
stance-style commands; AoE avoidance / pull logic; dungeon & raid support; and
further out, economy/AH, social/personality, and LLM-backed systems.

---

## Open issues (carried forward)

- **Bot-side `CanSeeOrDetect` gate is unreliable.** It returns false for mobs that
  can see and aggro the *bot* (asymmetric visibility), so `bot->IsValidAttackTarget`
  wrongly rejects valid targets. M3 works around it by routing assist through the
  **master's** validity; self-defense still depends on the bot's own check. Root
  cause unresolved — fixing it removes the workaround and unblocks clean
  self-defense / independent targeting.
- **Console level-boost crash.** `.character level <bot> <n>` (`GiveLevel`) leaves
  a spawned bot's talent/spec state inconsistent and crashes the worldserver on the
  next save/remove. Level bots by playing; fixing the `GiveLevel` path for
  socketless bots is a follow-up.
