# Player-bot roadmap

Headless player-bots: a real `Player` driven by a `WorldSession` with a **null
socket**, owned by `BotMgr` and pumped each world tick by `bot_worldscript`. See
`README.md` for the M1 internals and `CLAUDE.md` (repo root) for the build/run and
the cross-cutting gotchas.

Status: **M1 ✅ · M2 ✅ · M3 ✅ · M4 (started) · M5+ (planned)**

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

## M4 — Cohorts + rotation foundation (started)

M4 now starts by adding companion cohort persistence and owner binding on top of
the existing M3 runtime, then expands into the data-driven rotation engine that
replaces the M3 melee baseline with real ability usage instead of hardcoded
spells.

- **Started in this slice:** per-owner cohort persistence in the character DB,
  owner-scoped auto-spawn configuration, and pure policy helpers for level-band,
  catch-up XP gating, and continuity auto-accept decisions.

- **Data source:** the Assisted Combat DB2 tables (`assisted_combat`,
  `assisted_combat_rule`, `assisted_combat_step`) are Blizzard's per-spec ability
  priority lists, already loaded in-core. Retail resolves the Single-Button
  Assistant **client-side**, which a headless bot can't use — so M4 is a
  **server-side resolver** that walks the spec's step list and casts the top
  castable ability. (Stores reached via `extern` decls added to `DB2Stores.h`.)
- **Resolver — DONE (castability-priority first pass):** `BotRotation::SelectSpell`
  builds a per-spec priority list from `AssistedCombat` → `AssistedCombatStep`
  (ordered by `OrderIndex`), cached on first use. Each combat tick it returns the
  highest-priority ability the bot can actually cast — known (`HasSpell`), off
  cooldown/GCD (`SpellHistory`), affordable (`CalcPowerCost` vs `GetPower`), in
  range — and `BotMgr::UpdateFollow` casts it at the victim; melee carries the
  rotation between casts and when nothing is castable. Sourced from **steps, not
  rules**, which is also what the low-level Hunter hotfix spike ships.
  - ⚠️ The rule `ConditionType` / `ConditionValueN` columns are **not evaluated
    yet** — this is the next slice (decode the condition opcodes). Until then the
    resolver is priority + castability only, so it may fire an ability whose retail
    condition wouldn't be satisfied.
- **Target retention fix — DONE:** the bot now holds its current victim when the
  master retargets a *friendly* (e.g. a healer clicking a party member to heal).
  `BotMgr::UpdateFollow` validates the held mob with the *master*'s
  `IsValidAttackTarget` (alive + not-friendly + within leash) instead of the
  unreliable bot-side gate, routed through `BotCombatPolicy::ShouldKeepCurrentVictim`
  with a `BOT_STALE_COMBAT_MS` (2 s) grace window for transient LoS/phase blips.
- **Open questions still to design:** how to evaluate each rule's condition columns
  (the next slice); targeting for AoE vs single-target steps; cast-time abilities vs
  the melee chase (movement interrupts casts); self-heal / ally-heal targeting;
  a fallback rotation for specs with no Assisted Combat data. Owner-bound cohort
  continuity beyond resurrection/summon remains follow-on work within M4.
- **Spec prerequisite:** specs unlock at level 10. ⚠️ Don't `.character level` a
  spawned bot to reach that — the console boost crashes the worldserver on save;
  level by playing/escorting (see Open issues).

## M5+ — Questing / looting / gear / parties (planned)

Rough scope, to be split into its own milestones and specced when reached:

- **Looting:** auto-loot kills (and respect the master's loot rules in a group).
- **Gear:** equip upgrades from loot/quest rewards; basic stat weighting.
- **Questing:** accept/turn-in and complete simple quests alongside the master.
- **Multi-bot parties:** several bots in one group with role awareness
  (tank/heal/dps), enough to **fill a dungeon group** behind a single player.

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
