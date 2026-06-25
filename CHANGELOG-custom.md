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

Status: **M1 ✅ · M2 ✅ · M3 ✅ · M4 cohort foundation (started) · M5–M8 (planned) · rotation engine (separate track, first pass landed)**

A real `Player` driven by a `WorldSession` with a null socket, owned by `BotMgr`
and pumped each world tick by `bot_worldscript`.

- **M1** — spawn a static headless bot (no client, no AI).
- **M2** — follow + zone with the master across maps and into instances (the
  socketless cross-map teleport state machine; dungeon level-gate handling).
- **M3** — group with the master + melee assist combat (assist master's victim,
  defend self, retarget, post-combat hold). Melee remains the fallback whenever no
  ability is castable.
- **M4** — companion cohort foundation: persistence, owner binding, auto-spawn,
  level-band + catch-up XP evaluation (started; see below). The data-driven
  rotation engine is tracked as a **separate track** (first-pass resolver landed);
  that slice also wired the previously-dead `BotCombatPolicy::ShouldKeepCurrentVictim`
  into the combat loop so the bot **holds its current victim when the master
  retargets a friendly to heal** (validated via the master's `IsValidAttackTarget`,
  not the bot's unreliable own gate).

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

## Assisted-combat rotation — rotation engine (separate track)

Status: **castability-priority resolver (any spec with Assisted Combat data) —
tracked separately from the M4 cohort foundation**

`BotRotation::SelectSpell` is the server-side, headless replacement for the
client-side Single-Button Assistant. It builds a per-spec ability priority list
from Blizzard's Assisted Combat DB2 data (`AssistedCombat` → `AssistedCombatStep`,
ordered by `OrderIndex`) and, each combat tick, returns the highest-priority
ability the bot can actually cast — known (`HasSpell`), off cooldown/GCD
(`SpellHistory`), affordable (`CalcPowerCost` vs `GetPower`), and in range. The bot
casts it; melee auto-attack carries the rotation between casts and covers
specs/levels with no castable ability (`SelectSpell` returns 0). The rules'
`ConditionType` / `ConditionValueN` columns are intentionally **not** evaluated yet
— decoding Blizzard's condition opcodes is a follow-on slice. The low-level Hunter/Priest/Warrior
hotfix data (steps-only, no rules) is consumed by the same path.

Key files: `src/server/scripts/Custom/Bots/BotRotation.{h,cpp}`; wired into the
combat loop in `BotMgr::UpdateFollow`. Low-level data (Hunter / Priest / Warrior
Initial specs), auto-applied by the updater:
`sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql`.
Deeper docs: *Rotation engine* section of
`src/server/scripts/Custom/Bots/ROADMAP.md` and
`docs/superpowers/specs/2026-06-21-bot-lowlevel-rotations-design.md`.

## Custom secondary professions

Status: **done**

On a character's **first login**, grants the secondary professions (Cooking,
Fishing, Archaeology) by learning each one's base "Apprentice" spell. Primary
professions are intentionally *not* granted (the modern client only exposes two
primary slots).

Key files: `src/server/scripts/Custom/custom_secondary_professions.cpp`
(registered via `custom_script_loader.cpp`).

## Custom weapon skills (all weapons, any class)

Status: **done**

On a character's **first login**, grants **all 15 weapon skill lines** (axes/2H axes,
bows, guns, maces/2H maces, polearms, swords/2H swords, warglaives, staves, fist
weapons, daggers, crossbows, wands) so any class can equip and use any weapon type
from level 1 (e.g. a mage wielding a polearm).

Retail dropped weapon-skill *leveling* (combat uses `GetMaxSkillValueForLevel()`,
not the stored value), but the skill line still acts as the binary equip gate in
`Player::CanUseItem` (`GetSkillValue(itemSkill) == 0` → proficiency needed). The
script grants the skill line at value 1 (no combat effect) **and** re-asserts the
client-side weapon proficiency bitmask (`m_WeaponProficiency`) on **every** login,
since that bitmask is never persisted (rebuilt from known proficiency spells each
login). Skill lines persist, so they're only granted once; flip
`WEAPON_SKILLS_GRANT_ON_EVERY_LOGIN` to top up pre-existing characters. Fishing
poles are excluded (Fishing is handled by the secondary-professions script).

Key files: `src/server/scripts/Custom/custom_weapon_skills.cpp`
(registered via `custom_script_loader.cpp`).

## Tirisfal recruitment (Darnell escort)

Status: **done**

Zone script + helpers for the Tirisfal Glades "Darnell" recruitment quest flow:
ensures/cleans up the Darnell companion based on quest status and grants
recruitment credit under the right conditions.

Key files: `src/server/scripts/EasternKingdoms/tirisfal_recruitment.{h,cpp}`,
hooked from `zone_tirisfal_glades.cpp`.
Tests: `tests/game/TirisfalRecruitment.cpp`.

## Death QoL — chat while dead, no-run revive, bot auto-revive

Status: **done**

Config-gated relaxations of the dead state (all **default off** — vanilla unless
opted in). Flags toggle on a worldserver restart (no rebuild) via entrypoint env.

- **Chat while dead** (`Custom.AllowChatWhileDead`) — bypasses the server-side
  `/say` `/yell` `/emote` dead block. Mainly for headless bots (no client).
- **Player auto-revive at corpse** (`Custom.PlayerAutoReviveAtCorpse` +
  `Custom.PlayerAutoReviveDelayMs`) — teleports a dead player to their corpse and
  resurrects there (full HP, no sickness) after a delay; all players;
  battlegrounds/arenas excluded.
- **`.revive corpse`** — manual self-revive at corpse (no config; existing revive
  RBAC). Plain `.revive` unchanged.
- **Bot auto-revive** (`Custom.BotAutoRevive` + `Custom.BotAutoReviveDelayMs`) —
  resurrects a dead bot at its master, only while the master is alive.

Key files: `src/server/scripts/Custom/DeathQoL/DeathQoL.{h,cpp}` (player path +
shared corpse-return helper), `src/server/scripts/Custom/Bots/BotDeathPolicy.{h,cpp}`
(pure decision helpers), `BotMgr.cpp` (bot path), `cs_misc.cpp` (`.revive corpse`),
`ChatHandler.cpp` (chat gate). Config: `World.{h,cpp}`, `worldserver.conf.dist`,
`docker/worldserver/entrypoint.sh`.
Tests: `tests/game/DeathRevivePolicy.cpp`.
Deeper docs: `docs/superpowers/specs/2026-06-20-death-qol-design.md`.

## Retail base-Stamina fix (player_classlevelstats)

Status: **done** (2026-06-22)

TC master ships base **Stamina** in `player_classlevelstats` that is ~4.7x too low
at low levels and far too high at high levels (e.g. L1=62 vs retail ~292; L80=86452
vs retail ~2638 → ~1.7M HP). Since this core computes health as
`stamina x HpPerStamina` (base health 0, and `HpPerSta.txt` is correct), bad stamina
was the sole cause of non-retail health (the "grindy classic" feel). Fix sets `sta`
to a retail-accurate, **class-independent** curve (retail homogenises low-level base
stats) for **L1-80, all classes**.

- Values from live retail captures via a custom **StatCapture** addon (records
  `eff_sta`/`pos_sta` so base = `eff_sta - pos_sta`); **L1-70 measured**, L71-80
  extrapolated. Curve is non-linear (~+29/lvl to L20, ~+12 L20-35, rising to ~+50 by
  L70). Class-independence verified across Priest/Hunter/Warrior/etc. at overlapping
  levels; model verified by `maxhp = eff_sta x HpPerStamina` (exact, e.g. Evoker L70
  2504x13=32552).
- Migration: `sql/custom/world_player_classlevelstats_stamina_retail.sql` (80 UPDATEs).
  **Reversible:** backup table `player_classlevelstats_backup_prestaminafix`.
  Requires a worldserver restart to take effect (stats cached at startup).
- Not touched: str/agi/inte (within a couple points of retail already). Minor
  follow-ups: `HpPerSta.txt` integer-rounding causes small high-level health error;
  L71-80 stamina is extrapolated (refine if leveling past 70 on retail).

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

### Worldserver crash-backtrace flow

Status: **done**

When the dockerised worldserver segfaults, WSL's crash handler
(`core_pattern = |/wsl-capture-crash`) drops a multi-GB ELF core into
`%LOCALAPPDATA%\Temp\wsl-crashes\`. These pile up fast (6–10 GB each) and are only
useful as a gdb backtrace against the matching binary. Files:

- `docker/debug/Dockerfile` — thin `trinitycore-debug:local` image (`FROM
  trinitycore:local` + `gdb`) so the binary/symbols match the crashing process.
  **Rebuild it whenever you rebuild `trinitycore:local`.**
- `scripts/process-worldserver-crashes.ps1` — runs gdb (`thread apply all bt`)
  against each pending dump, writes a small `.bt.txt` to `crash-backtraces/`
  (gitignored), and only then deletes the giant `.dmp`. Fail-soft (no daemon /
  missing image / locked dump → logged, dump kept).
- `scripts/register-crash-task.ps1` — registers the Scheduled Task
  `TrinityCore-WorldserverCrashBacktraces` via `schtasks` (every 15 min, current
  user, while logged on — no elevation needed); `-Remove` to delete.
- `scripts/run-crash-hidden.vbs` — the task launches the processor through this
  (`WScript.Shell.Run … 0`) instead of `powershell.exe` directly, so it runs with
  no window (avoids a console flashing on screen every 15 min).

---

## Code modifications to upstream files

In-place edits to stock TrinityCore source (track these so an upstream merge
doesn't silently revert them):

- **Absorb-vs-creature-scaling fix (PW:Shield & all absorbs while leveling)** —
  upstream computes melee/spell absorbs on the *pre-scaling* damage and only applies
  the creature level-scaling multiplier (`Creature::GetDamageMultiplierForTarget`)
  afterwards in `Unit::DealDamageMods`. For a low-level character fighting a
  down-scaled creature (internal level = the zone ContentTuning `ScalingLevelMax`,
  e.g. 30, vs a level-8 player → multiplier ≈ 0.068) the shield is consumed against
  the unscaled ~18× damage and pops in one hit while only the tiny scaled amount
  hits health — making PW:Shield (and every absorb) ~15× too weak. Fix moves the
  scaling to *before* the absorb:
  - **`src/server/game/Entities/Unit/Unit.{h,cpp}`** — `DealDamageMods` gains a
    `bool applyDamageMultiplier = true` param; `CalculateMeleeDamage` and
    `CalculateSpellDamageTaken` now apply `GetDamageMultiplierForTarget` before
    armor/absorb.
  - The four `DealDamageMods` calls that immediately follow those two functions pass
    `applyDamageMultiplier=false` so the multiplier isn't applied twice:
    `Unit::AttackerStateUpdate` (melee), `Spell.cpp` (direct spell hit), and
    `SpellAuraEffects.cpp` ×2 (periodic/proc damage). The other `DealDamageMods`
    callers (share/thorns/split redirects) keep the default and are unchanged.
  - Net effect: damage that *isn't* absorbed is identical to before (scaling is
    commutative there); only the absorbed case is corrected. Player-cast damage is
    unaffected (a player's `GetDamageMultiplierForTarget` is 1.0).
- **`src/server/game/Entities/Player/CollectionMgr.{h,cpp}`** — adds
  `enum AppearanceGrantSource { Normal, BulkLoginGrant }` and threads it through
  `AddItemAppearance`, so appearance-**set** rewards are skipped on the bulk
  login-grant path (`ShouldGrantAppearanceSetRewards`). Prevents the
  collection-grant login crash. Tests: `tests/game/CollectionMgr.cpp`. See the
  `collection-grants-crash-login` memory.
- **`src/server/scripts/EasternKingdoms/zone_tirisfal_glades.cpp`** — hooks the
  Tirisfal recruitment script (above).
- **`src/server/game/DataStores/DB2Stores.h`** — adds `extern` declarations for the
  three Assisted Combat DB2 stores (`sAssistedCombatStore`,
  `sAssistedCombatRuleStore`, `sAssistedCombatStepStore`). Upstream defines them in
  `DB2Stores.cpp` but exposes them to no other translation unit; the declarations let
  the M4 bot rotation engine (`BotRotation`) read them.
- **`src/server/game/World/World.{h,cpp}`** — adds the five `CONFIG_CUSTOM_*`
  Death QoL config keys (enum + loader).
- **`src/server/game/Handlers/ChatHandler.cpp`** — guards the `/say` `/yell`
  `/emote` dead-checks behind `Custom.AllowChatWhileDead`.
- **`src/server/scripts/Commands/cs_misc.cpp`** — adds the `.revive corpse`
  variant to `HandleReviveCommand`.
  _(The temporary `[ABSORB-DIAG]` / `[DMG-DIAG]` `TC_LOG_ERROR` instrumentation that
  was added to `Unit::CalcAbsorbResist` and `Unit::AttackerStateUpdate` to diagnose
  the PW:Shield issue has been removed now that the absorb-vs-scaling fix above is
  confirmed.)_

---

## Removed

_(none yet — when a custom feature is removed, move its entry here with a date and
reason.)_
