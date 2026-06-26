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
specs/levels with no castable ability (`SelectSpell` returns 0).

**Condition-aware rotation ("don't-waste-casts").** The engine now evaluates
per-step conditions so it won't re-cast a DoT that's still active (it refreshes it
just before it expires) and uses a filler instead:
- `BotRotation{Policy}` — a pure, unit-tested decision
  (`ShouldCastForMissingOrExpiringAura`) plus an engine gate that reads
  `AssistedCombatRule` rows. Fork condition opcodes (e.g.
  `BOT_COND_TARGET_MISSING_OR_EXPIRING_MY_AURA = 1000`) are interpreted **only for
  our custom steps** (ID ≥ 1000000); Blizzard's undocumented stock-step opcodes are
  left **fail-open** (treated as eligible), so max-level rotations never regress.
- Priest (Initial spec 1452) Shadow Word: Pain is gated so the bot casts **Smite**
  as filler and refreshes SWP under 3s:
  `sql/updates/hotfixes/master/2026_06_24_00_hotfixes.sql`
  (`AssistedCombatRule` table hash `0xC1B4F680`). Hunter/Warrior unchanged.
- Spec/design: `docs/superpowers/specs/2026-06-24-condition-aware-bot-rotation-design.md`,
  plan `docs/superpowers/plans/2026-06-24-condition-aware-bot-rotation.md`.

**Headless bots cast *effective* spells now (root-cause fix).** Bots' spell casts
previously "succeeded" but applied **no** damage/auras to the enemy: the enemy was
dropped during effect-target selection because the bot's `IsValidAttackTarget` →
`CanSeeOrDetect` returned false. Root cause: `Player::CanNeverSee` gates on
`PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME`, which is normally set by the
client's login init-mover/time-sync handshake — a headless bot has no client, so it
was never set and the bot could never "see" anything. Fix: set that flag when the
bot's login completes (see *Code modifications to upstream files* →
`CharacterHandler.cpp`). This also makes bots see/validate/engage targets on their
own; the `TRIGGERED_IGNORE_TARGET_CHECK` on the bot's `CastSpell` (`BotMgr`) is now a
belt-and-suspenders safety net rather than a load-bearing workaround.

Key files: `src/server/scripts/Custom/Bots/BotRotation{,Policy}.{h,cpp}`; wired into
the combat loop in `BotMgr`. Earlier low-level data (Hunter / Priest / Warrior
Initial specs), auto-applied by the updater:
`sql/updates/hotfixes/master/2026_06_21_00_hotfixes.sql`.
Deeper docs: *Rotation engine* section of
`src/server/scripts/Custom/Bots/ROADMAP.md` and
`docs/superpowers/specs/2026-06-21-bot-lowlevel-rotations-design.md`.

## Player-bot positioning — ranged hold-at-range + formation spread

Status: **first pass — class-based ranged positioning + per-bot fan-out**

Two movement fixes now that bots cast effective spells:

- **Ranged-class bots hold at casting range instead of running into melee.**
  `BotMovementPolicy::IsRangedClass` (Hunter / Priest / Mage / Warlock / Evoker)
  picks a ranged stance: the bot chases to `BOT_RANGED_DIST` (~25 yd) via
  `MoveChase(target, ChaseRange(25), …)` and fights via the rotation, never closing
  to melee. ⚠️ It still calls `Attack(target, **false**)` — `ChaseMovementGenerator`
  halts unless `GetVictim()==target` (`HasLostTarget`), and `GetVictim()` is only set
  by `Attack()`; the `false` sets the victim **without** scheduling melee swings, and
  `ChaseRange` (not `Attack`) is what keeps the bot at range. Melee classes keep the
  prior close-and-swing behaviour.
- **Bots fan out instead of stacking.** Each bot gets a `BotEntry::formationSlot`
  at spawn; `BotMovementPolicy::FormationFollowAngle/FormationChaseAngle` turn it into
  distinct angles fed to `MoveFollow` (arc behind the master) and `MoveChase` (spread
  around the target), so bots are individually targetable.

Pure, unit-tested policy: `src/server/scripts/Custom/Bots/BotMovementPolicy.{h,cpp}`
(`tests/game/BotMovementPolicy.cpp`); wired into the combat/follow loop in
`BotMgr::UpdateFollow`. Out of scope (follow-ups in
`docs/bot-ux-future-milestones.md`): role-aware formation shaping, kiting, Hunter
pets, bot gear, formation presets/UI.
Spec/plan: `docs/superpowers/specs/2026-06-25-bot-ranged-positioning-formation-design.md`,
`docs/superpowers/plans/2026-06-25-bot-ranged-positioning-formation.md`.

**Ranged auto-attack between casts.** Ranged bots now keep their ranged auto-attack
running while in combat — Hunters fire **Auto Shot**, casters fire wand **Shoot** when
a wand is equipped (no wand → silent no-op; we don't hand out wands). `BotMgr` scans the
bot's known spells once for its autorepeat ranged spell (no hardcoded ids — there are
~12 Auto Shot variants) and starts it in the ranged combat branch; the engine loops it
on the `RANGED_ATTACK` timer and it stops on disengage. Pure trigger logic in
`BotRangedAttackPolicy::ShouldStartAutoRepeat` (`tests/game/BotRangedAttackPolicy.cpp`).
Spec/plan: `docs/superpowers/specs/2026-06-25-bot-ranged-auto-attack-design.md`,
`docs/superpowers/plans/2026-06-25-bot-ranged-auto-attack.md`.

## Hunter bot pets

Status: **first pass**

Hunter bots now summon and maintain a real pet. On the follow tick,
`BotMgr::EnsureHunterPet` reconciles pet state: a petless live Hunter gets one — the
**nearest tameable beast** in range (`PickTameableBeastEntry`, skipping other players'
pets/summons), or Young Wolf #299 as fallback — summoned via the `EffectTameCreature`
sequence (`CreateTamedPetFrom` → level → `AddToMap` → `SetMinion` → `REACT_DEFENSIVE` →
`SavePetToDB`); a dead pet is revived after `Custom.BotAutoReviveDelayMs` (the delay
value only — pet revive isn't gated by the `BotAutoRevive` enable flag); a living pet's
level is kept in sync (hunter pets don't auto-sync). Combat/follow come free from
`PetAI` (a defensive pet assists the owner's victim). The pet is despawned on
`.bot remove` (before logout/save). Pure decisions in `BotPetPolicy`
(`tests/game/BotPetPolicy.cpp`). Key files:
`src/server/scripts/Custom/Bots/BotPetPolicy.{h,cpp}`, `BotMgr.{h,cpp}`.
**Limitation:** pet upkeep runs on the master-follow path, so a master-less / parked
Hunter bot (or one whose master is offline) won't summon/revive a pet until it has an
online master. Out of scope: pet ability rotation, specific-pet taming,
families/talents, non-hunter pets. Spec/plan:
`docs/superpowers/specs/2026-06-26-bot-hunter-pets-design.md`,
`docs/superpowers/plans/2026-06-26-bot-hunter-pets.md`.

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

## Cross-faction play (two-side interaction)

Status: **done**

Lets Horde and Alliance **group, chat in channels, share calendars/guilds, and
trade** with each other (this fork is PvE-only, so the usual faction separation
isn't wanted). Driven by a single entrypoint env flag `TC_ALLOW_CROSS_FACTION`
(default **1**), which sets the core's `AllowTwoSide.Interaction.{Calendar,Channel,
Group,Guild}` and `AllowTwoSide.Trade` configs. Config-only — applies on a
worldserver **restart, no rebuild**. Set `TC_ALLOW_CROSS_FACTION=0` to restore
vanilla same-faction-only behavior.

`AllowTwoSide.Interaction.Auction` is intentionally **not** wired (kept at the core
default 1): its own config docs warn that flipping it in production strands
already-placed faction-AH auctions.

Key files: `docker/worldserver/entrypoint.sh` (env flag + `set_conf` calls).

## Allied races: Exile's Reach start option (level 1)

Status: **server-side done — needs in-client verification of the toggle** (2026-06-25)

Lets allied races choose **Exile's Reach (the New Player Experience) and start at
level 1**, instead of only their level-10 heritage start. Picking their **normal
start** is unchanged (still the level-10 heritage intro).

The NPE machinery already exists in the core (`CharacterCreateInfo::UseNPE` +
`playercreateinfo.createPositionNPE`); allied races just lacked the data + a level
rule:
- **Code** — `Player::GetStartLevel` gains a `useNewPlayerExperience` arg; in NPE
  mode it skips the allied-race level bump so the character starts at level 1 like a
  core race. `Player::Create` passes `m_createMode == PlayerCreateMode::NPE`.
  (Tracked under *Code modifications to upstream files*.)
- **Data** — `sql/updates/world/master/2026_06_25_00_world.sql` populates the NPE
  columns (faction-appropriate Exile's Reach spawn) for the allied races, excluding
  Death Knights (kept on their dedicated start). Guarded by `npe_map IS NULL`.

⚠️ **Open item:** whether the live 12.0.5 client actually *offers* the "Exile's
Reach" toggle for an allied race is **not verifiable headless** — it may be a
client-side gate needing a `CharBaseInfo`/race-flag hotfix. The server side is
correct and ready; this needs a quick in-client check (create an allied race → is
the Exile's Reach option shown? → does it land you on the ship at level 1?). Needs a
`docker compose build` (C++) + the auto-applied SQL.

Hero-class / Dracthyr exceptions are intentionally **left as-is** (their dedicated
starts via `StartDeathKnightPlayerLevel` / `StartDemonHunterPlayerLevel` /
`StartEvokerPlayerLevel`).

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

## Low-level crafted-gear item-level boost

Status: **done**

Crafted (tradeskill) equippable gear tends to lose to same-level vendor/quest
greens while leveling, so professions feel pointless. At craft time the produced
item gets a **flat item-level bump** so the extra effort actually pays off.

- **Mechanism:** `Spell::DoCreateItem` appends a **client-known item-level-delta
  bonus list** (`ItemBonusMgr::GetItemBonusListForItemLevelDelta`, the retail
  upgrade system) to the crafted item. Because these delta bonus lists ship in the
  client's `ItemBonusListLevelDelta.db2`, the boost renders correctly (stats,
  item level, tooltip) with **no custom DB2 rows and no hotfix** — server and
  client compute the same result. It only adds item level, not required level, so
  a low-level character can still equip the result.
- **Scope:** only weapons/armor (equippable; class WEAPON or ARMOR) whose base
  required level is `<= Custom.CraftedGearBoostMaxRequiredLevel` — "low levels
  only"; end-game crafting is already competitive. If the exact `+N` delta has no
  upgrade bonus list, the largest available delta below it is used.
- **On by default** (the rest of the fork's custom flags default off; this one is
  requested on). Toggle/tune on a worldserver restart (no rebuild) via config or
  entrypoint env — only the C++ hook itself needs a rebuild.

Config keys (`World.{h,cpp}`, `worldserver.conf.dist`,
`docker/worldserver/entrypoint.sh` — envs `TC_CRAFTED_GEAR_BOOST`,
`TC_CRAFTED_GEAR_BOOST_ITEM_LEVELS`, `TC_CRAFTED_GEAR_BOOST_MAX_REQ_LEVEL`):

- `Custom.CraftedGearBoost` (bool, default `1`) — enable.
- `Custom.CraftedGearBoostItemLevels` (int, default `7`) — item levels to add.
- `Custom.CraftedGearBoostMaxRequiredLevel` (int, default `60`) — only boost gear
  at/below this required level (`0` disables).

Key files: `src/server/game/Spells/SpellEffects.cpp` (`Spell::DoCreateItem` hook).
Config: `World.{h,cpp}`, `worldserver.conf.dist`, `docker/worldserver/entrypoint.sh`.

## Auction house simulator (AuctionHouseBot)

Status: **enabled by default**

Turns on upstream's **AuctionHouseBot** (`src/server/game/AuctionHouseBot/`, already
compiled into the core) so the auction house feels alive on a solo/bot server:
the **seller** populates the AH with listings and the **buyer** purchases player
listings (so you can actually sell things). This is **config only — no rebuild**;
`sAuctionBot->Initialize()`/`Update()` already run unconditionally, gated purely on
the config flags.

Enabled via the worldserver entrypoint (restart to apply), default ON, env-toggleable:

- `AuctionHouseBot.Seller.Enabled = 1` — env `TC_AHBOT_SELLER` (default `1`).
- `AuctionHouseBot.Buyer.Enabled = 1` + the three per-faction
  `AuctionHouseBot.Buyer.{Alliance,Horde,Neutral}.Enabled = 1` (the core gates the
  buyer on both the master flag and the per-faction flag) — env `TC_AHBOT_BUYER`
  (default `1`); set to `0` for a populate-only AH that doesn't buy your listings.

Notes:
- The seller's faction item-amount ratios keep their `conf.dist` defaults (100 each),
  so all auction houses get stocked. No dedicated bot account is needed — with
  `AuctionHouseBot.Account = 0` the seller creates ownerless ("system") auctions,
  which the buyer recognises via `IsBotChar()` and won't re-buy.
- Volume/prices/quality mix are tunable through the large `AuctionHouseBot.*` block
  in `worldserver.conf.dist` (e.g. `AuctionHouseBot.Items.Amount.{White,Green,Blue,Purple}`).
  `.ahbot` GM commands inspect/rebuild the populated AH.
- ⚠️ This fork runs `AllowTwoSide.Interaction.Auction = 1`; the core notes
  faction-specific AHBot settings "might not work as expected" with cross-faction
  auctions on. Worth an in-game check that listings show up in the retail 12.0.5
  client's AH; if a faction AH looks empty, tune the per-faction ratios. AHBot does
  use the modern commodity auction API (`AuctionPosting` / `IsCommodity`), so it is
  built for the retail AH, not legacy-only.

Config: `docker/worldserver/entrypoint.sh` (the `AHBOT_*` env + `set_conf` lines);
all knobs live in the upstream `AuctionHouseBot.*` block of `worldserver.conf.dist`.

## Server rate overrides

Status: **done**

Non-default `Rate.*` / `SkillGain.*` values for this fork (everything else is at the
`worldserver.conf.dist` default of 1):

- **Loot:** `Rate.Drop.Item.* = 2`, `Rate.Drop.Money = 2`,
  `Rate.Drop.Item.ReferencedAmount = 2`. *Set directly in the persisted
  `worldserver.conf`.* ⚠️ These scale the drop **chance** of items
  (`LootStoreItem::Roll` → `roll_chance(chance * qualityModifier)`), **not** stack
  counts. So they boost *low-chance* drops (e.g. gems/extra items in a vein) but do
  **not** increase the base ore/herb count of a guaranteed gather (already ~100%).
- **Gathering skill:** `SkillGain.Gathering = 2` (2x mining/herb skill-ups). *Conf.*
- XP and crafting skill-gain are left at 1x.

**No mining-yield multiplier exists in retail master.** The legacy 3.3.5
`Rate.Mining.Amount` / `Rate.Mining.Next` config keys were removed from this codebase
(no `RATE_MINING_*` anywhere in `src/`). Ore/herb count per node is fixed by
`gameobject_loot_template.mincount/maxcount` (a DB value) and isn't scaled by any
`Rate.*`. To increase yield you must edit those loot-template counts (world DB) or add a
custom gather-amount multiplier in the loot-count code.

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

## Food / drink restoration floor

Status: **done** (2026-06-25)

Classic-style food and drink restore health/mana through flat `SPELL_AURA_MOD_REGEN`
(food) and `SPELL_AURA_MOD_POWER_REGEN` (drink) values, fed per-tick through
`Player::RegenerateHealth` and `Player::UpdatePowerRegen`. Those flat values were
tuned for vanilla-sized pools; after the **base-Stamina fix** raised low-level health
~4.7x to retail-accurate values, eating/drinking restores a negligible fraction of the
bar (the "food restores very little" report). Fix floors the food/drink contribution at
a configurable **percent of max health/mana per 5s** — only for genuine food/drink auras
(detected via `SpellInfo::GetSpellSpecific` → `SPELL_SPECIFIC_FOOD`/`DRINK`/
`FOOD_AND_DRINK`), so other `MOD_REGEN`/`MOD_POWER_REGEN` sources are untouched and any
food already restoring more than the floor keeps its larger value. Percent-based retail
food (which ticks via `OBS_MOD_HEALTH`/`OBS_MOD_POWER`, a different path) is unaffected.

- **Config** (`World.{h,cpp}`, `worldserver.conf.dist`,
  `docker/worldserver/entrypoint.sh` — envs `TC_FOOD_DRINK_RESTORE`,
  `TC_FOOD_DRINK_RESTORE_PCT_PER_5SEC`):
  - `Custom.FoodDrinkRestore` (bool, default `1`) — enable the floor.
  - `Custom.FoodDrinkRestorePctPer5Sec` (float, default `12.0`) — min % of max
    health/mana restored per 5s while the food/drink aura is active.
- **Mechanism:** health floor applied in `Player::RegenerateHealth` (ticks every 2s, so
  the per-5s percent is scaled by `2/5`); mana floor applied in
  `Player::UpdatePowerRegen` (per-second regen field, so the per-5s percent is `/5`).
  Takes effect immediately — no rebuild needed beyond the C++ change itself.

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

- **`src/server/game/Entities/Player/Player.{h,cpp}`** — allied-race Exile's Reach
  start level. `Player::GetStartLevel` gains a `useNewPlayerExperience` parameter
  (default false); when set it skips the `IsAlliedRace` level bump so an allied race
  created through the NPE starts at level 1. `Player::Create` passes
  `m_createMode == PlayerCreateMode::NPE`. Pairs with the allied-race NPE position data
  in `sql/updates/world/master/2026_06_25_00_world.sql`. (See *Allied races: Exile's
  Reach start option* above.)
- **`src/server/game/Handlers/CharacterHandler.cpp`** — headless-bot visibility fix.
  In `WorldSession::LoadBotCharacter`, after `HandlePlayerLogin`, set
  `PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME` on the loaded bot. A real client
  gets this flag from its login init-mover/time-sync handshake; a headless bot has no
  client, so without it `Player::CanNeverSee` returns true forever and the bot can't
  "see" any object — `IsValidAttackTarget` fails and every enemy-targeted spell effect
  is silently dropped (casts "succeed" but deal no damage / apply no auras). Setting
  it makes the bot a first-class member of the server visibility system. See the
  *Assisted-combat rotation* feature section.
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
  Death QoL config keys, the three `CONFIG_CUSTOM_CRAFTED_GEAR_BOOST*` keys, and the
  two food/drink-restoration keys (`CONFIG_CUSTOM_FOOD_DRINK_RESTORE` bool +
  `CONFIG_CUSTOM_FOOD_DRINK_RESTORE_PCT_PER_5SEC` float) (enum + loader).
- **`src/server/game/Entities/Player/Player.cpp`** — `Player::RegenerateHealth` floors
  food (`SPELL_AURA_MOD_REGEN`) restoration at `Custom.FoodDrinkRestorePctPer5Sec`% of
  max health per 5s (food/drink restoration floor, above).
- **`src/server/game/Entities/Unit/StatSystem.cpp`** — `Player::UpdatePowerRegen` floors
  drink (`SPELL_AURA_MOD_POWER_REGEN`) restoration at the same percent of max power per
  5s (food/drink restoration floor, above).
- **`src/server/game/Spells/SpellEffects.cpp`** — `Spell::DoCreateItem` appends a
  client-known item-level-delta bonus list to low-level crafted equippable gear
  (low-level crafted-gear item-level boost, above).
- **`src/server/game/Handlers/ChatHandler.cpp`** — guards the `/say` `/yell`
  `/emote` dead-checks behind `Custom.AllowChatWhileDead`.
- **`src/server/scripts/Commands/cs_misc.cpp`** — adds the `.revive corpse`
  variant to `HandleReviveCommand`.
- **`src/server/game/Server/WorldSession.{h,cpp}`** — adds a `WorldSession::IsBot()`
  flag (set by `BotMgr` on the headless bot session via `SetBot()`) and gates the
  "non existent socket" `network.opcode` ERROR in `SendPacket` on `!IsBot()`. Headless
  bots are socketless by design, so that log fired for every packet routed to a bot
  (~23k lines/session); the packet is still dropped and real-client socket errors still
  log. (Player-bots, above.)
  _(The temporary `[ABSORB-DIAG]` / `[DMG-DIAG]` `TC_LOG_ERROR` instrumentation that
  was added to `Unit::CalcAbsorbResist` and `Unit::AttackerStateUpdate` to diagnose
  the PW:Shield issue has been removed now that the absorb-vs-scaling fix above is
  confirmed.)_

---

## Removed

_(none yet — when a custom feature is removed, move its entry here with a date and
reason.)_
