# CHANGELOG-custom.md — fork deltas from upstream TrinityCore

The single authoritative index of **everything this fork adds, removes, or changes
on top of upstream `TrinityCore/TrinityCore` master** (client 12.0.5 / build 67823).
If a feature isn't listed here, treat it as stock TrinityCore.

This file is a *map*, not the full docs: each entry links to the deeper doc
(`CLAUDE.md`, `SETUP-NOTES.md`, `CONTENT-NOTES.md`, the `ROADMAP.md`/spec files)
that owns the detail. Keep entries short; put the depth in the linked doc.

## Maintenance rule (read this before changing features)

**Whenever you add, remove, or change a custom feature, update this file in the
same commit.** Concretely:

- **Adding** a feature → add an entry under the right section with: one-line
  summary, status, key files, and a link to its deeper doc (create one if the
  feature warrants it).
- **Changing** a feature → update its entry (and bump status if a milestone moved).
- **Removing** a feature → delete its entry (or move it to *Removed* with a date
  and reason) and confirm the code/docs it pointed at are gone too.
- **Modifying an upstream file in place** → it MUST appear in
  *Code modifications to upstream files* below, so in-place edits aren't silently
  lost in a future upstream merge.

Read this file before starting feature work so you don't duplicate or contradict
something that already exists.

---

## Player-bots — headless companions

Status: **M1 ✅ · M2 ✅ · M3 ✅ · M4 (started) · M5+ (planned)**

A real `Player` driven by a `WorldSession` with a null socket, owned by `BotMgr`
and pumped each world tick by `bot_worldscript`.

- **M1** — spawn a static headless bot (no client, no AI).
- **M2** — follow + zone with the master across maps and into instances (the
  socketless cross-map teleport state machine; dungeon level-gate handling).
- **M3** — group with the master + melee assist combat (assist master's victim,
  defend self, retarget, post-combat hold).
- **M4** — cohorts + data-driven rotation engine (started; see below).

GM commands: `.bot add/remove/follow/stay/count` (`add`/`remove`/`stay`/`count`
work from console/SOAP; `follow` needs an in-world player).

Key files: `src/server/scripts/Custom/Bots/` (`BotMgr`, `BotCombatPolicy`,
`cs_bot.cpp`).
Deeper docs: `src/server/scripts/Custom/Bots/ROADMAP.md`,
`src/server/scripts/Custom/Bots/README.md`, and the player-bot section of
`CLAUDE.md` (build/run + cross-cutting gotchas).

## Companion cohorts — M4 foundation

Status: **started (persistence + owner binding + policy helpers in place)**

Per-owner cohort persistence in the character DB, owner-scoped auto-spawn
configuration, and pure policy helpers (level-band, catch-up XP gating,
continuity auto-accept). Lays the runtime groundwork the M4 rotation engine builds
on.

Key files: `src/server/scripts/Custom/Bots/BotCohortMgr.{h,cpp}`,
`BotCohortPolicy.{h,cpp}`; cohort commands in `cs_bot.cpp`.
Deeper docs: `docs/superpowers/specs/2026-06-20-companion-cohorts-design.md`,
`docs/superpowers/plans/2026-06-20-companion-cohorts.md`.

## Assisted-combat rotation — M4 spike

Status: **spike (Hunter low-level only)**

Server-side resolver experiment that walks Blizzard's Assisted Combat DB2 priority
lists (`assisted_combat` / `_rule` / `_step`) for the bot's spec and casts the top
castable ability — the headless replacement for the client-side Single-Button
Assistant. Currently a low-level Hunter proof of concept; generalization is the
core of M4.

Key files: `sql/custom/spike_assisted_combat_hunter_lowlevel.sql`.
Deeper docs: M4 section of `src/server/scripts/Custom/Bots/ROADMAP.md`.

## Custom secondary professions

Status: **done**

On a character's **first login**, grants the secondary professions (Cooking,
Fishing, Archaeology) by learning each one's base "Apprentice" spell. Primary
professions are intentionally *not* granted (the modern client only exposes two
primary slots).

Key files: `src/server/scripts/Custom/custom_secondary_professions.cpp`
(registered via `custom_script_loader.cpp`).

## Tirisfal recruitment (Darnell escort)

Status: **done**

Zone script + helpers for the Tirisfal Glades "Darnell" recruitment quest flow:
ensures/cleans up the Darnell companion based on quest status and grants
recruitment credit under the right conditions.

Key files: `src/server/scripts/EasternKingdoms/tirisfal_recruitment.{h,cpp}`,
hooked from `zone_tirisfal_glades.cpp`.
Tests: `tests/game/TirisfalRecruitment.cpp`.

## World / DB content fixes

Status: **ongoing**

Hand-authored world-content fixes (e.g. Northwatch Lugs carrying a Supply Crate
via the vehicle+accessory system; Westfall copper-vein pooling) shipped as SQL in
`sql/updates/world/master/` and `sql/custom/world/`.

Deeper docs: **`CONTENT-NOTES.md`** — the reusable *patterns* behind these fixes
plus the specific cases that taught them. Add new content fixes there.

## Infrastructure — Docker Compose wrapper

Status: **done / ongoing**

Docker Compose bring-up of the retail stack (bnetserver + worldserver + 4 DBs),
bind-mounted entrypoints, the `CN=127.0.0.1` login-REST cert workaround, map data
on a WSL2 named volume, SOAP console, and the bot-enabling config
(`Instance.IgnoreLevel=1`, etc.).

Deeper docs: **`SETUP-NOTES.md`** (full runbook) and the build/run section of
`CLAUDE.md`.

---

## Code modifications to upstream files

In-place edits to stock TrinityCore source (track these so an upstream merge
doesn't silently revert them):

- **`src/server/game/Entities/Player/CollectionMgr.{h,cpp}`** — adds
  `enum AppearanceGrantSource { Normal, BulkLoginGrant }` and threads it through
  `AddItemAppearance`, so appearance-**set** rewards are skipped on the bulk
  login-grant path (`ShouldGrantAppearanceSetRewards`). Prevents the
  collection-grant login crash. Tests: `tests/game/CollectionMgr.cpp`. See the
  `collection-grants-crash-login` memory.
- **`src/server/scripts/EasternKingdoms/zone_tirisfal_glades.cpp`** — hooks the
  Tirisfal recruitment script (above).

---

## Removed

_(none yet — when a custom feature is removed, move its entry here with a date and
reason.)_
