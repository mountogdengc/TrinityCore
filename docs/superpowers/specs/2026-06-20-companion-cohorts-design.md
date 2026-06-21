# Companion Cohorts Design

## Goal

Design the next phase of the player-bot system around a companion-RPG experience.
Bots are primarily adventuring companions for a specific player character, not
general world-simulation actors. The system should keep each player character
supplied with a usable cohort that can adventure with them, stay near their
level, and remain compatible with their quest and zone progression.

## Product Direction

The intended experience is:

- each player character owns a cohort of companions
- companions are long-term party members and part of player progression
- companions should stay within a target level band of their owner, roughly
  `owner level +/- 2-3`
- companions catch up through real in-game progression, not direct DB leveling
- companions remain usable during ordinary party play through login, death,
  resurrection, summons, and travel
- the player can see enough companion state to understand progress and
  incompatibilities without turning the system into a heavy management UI

This design is explicitly optimized for reliability, visibility, and party play
with the owner character. It does not aim to build fully autonomous open-world
simulation in the early milestones.

## Non-Goals For Early Milestones

The following are intentionally deferred:

- independent roaming or background progression while the owner is offline
- a global shared bot pool across characters
- auction house autonomy
- independent quest pickup/turn-in away from the owner
- social simulation, bot chatter, or bot memory systems
- LLM-backed reasoning or dialogue
- advanced dungeon and raid coordination

These remain valid future directions and are captured later in this document.

## Current Foundation

The existing bot system already provides:

- headless bot sessions backed by real `Player` characters
- follow behavior across maps and instances
- party grouping with the owner
- assist / defend combat with a melee baseline
- basic command surface: `.bot add/remove/follow/stay/count`

M4 and later work should layer on top of this foundation rather than replace it.

## Core Model

### Per-Character Cohorts

Each player character owns their own companion cohort. Cohorts are not shared
globally. This is the primary boundary for:

- faction separation
- target level banding
- quest and zone compatibility
- player-facing team identity

The system should support multiple player characters on the same account, each
with a different cohort and progression lane.

### Catch-Up Through Real Play

Companions should not be leveled by direct DB edits or synthetic level grants in
the early design. Catch-up must happen through ordinary in-game XP gains while
the owner is actively playing that character.

To reduce grind and keep cohorts usable, companions below the owner's target
level band receive a temporary XP multiplier, such as `1.5x` or `2x`, until they
return to range. Once in range, the multiplier turns off automatically.

### Companion Purpose

Questing exists in this design to keep companions adventure-compatible with
their owner character. Companions do not need an independent narrative arc in
the early system. They need enough progression state to keep joining the owner
in gated zones and quest chains.

## Persistent Data Model

The initial stored model should stay small and explicit.

### Required persistent data

- `owner_character_guid`
- `companion_character_guid`
- `active_or_benched`
- `auto_spawn_enabled`

### Derived runtime state

The following should be derived from live game state rather than heavily stored:

- whether the companion is below / in / above target level band
- whether catch-up XP is active
- quest and zone compatibility state
- blocked reasons
- active quest progress

This keeps the system honest and reduces drift between persistent metadata and
real in-world character state.

## Runtime Behavior

### Cohort Activation

Each player character should have configurable cohort spawning behavior.

- default behavior: active cohort auto-spawns on owner login
- this behavior is configurable per owner character
- the system should allow an owner to disable auto-spawn and use manual summon /
  spawn control instead

Auto-spawn should include reasonable safety checks so the system does not create
invalid or disruptive spawns in obviously bad situations.

### Continuity Behaviors

Companions must survive normal party lifecycle events without constant manual
repair from the player.

Early continuity support includes:

- accepting resurrection
- recovering and rejoining after death when no resurrection is available
- accepting summons
- accepting normal party travel flows required to stay with the owner

These are foundational behaviors, not optional polish. Without them, the cohort
will constantly fracture during normal play.

### Combat Baseline

The current M3 melee combat loop remains the baseline execution model until a
real rotation engine is added. Later ability usage should layer on top of:

- current target selection
- chase and movement behavior
- current stale-combat handling
- melee fallback when no spell is usable

## Player Visibility

The first player-facing visibility layer should stay simple and practical.

For each companion in the current owner character's cohort, expose:

- name, class, level
- whether they are below / in / above target band
- whether catch-up XP is active
- active quests
- per-objective progress such as `3/12 Northwatch Lugs`
- simple blocked reasons, for example:
  - wrong faction
  - level mismatch
  - missing prerequisite quest chain
  - zone progression incompatibility

This is enough for the player to understand what needs help without requiring a
full management game.

## Milestone Plan

### M4 Foundation

Introduce the cohort model on top of the current M3 bot runtime.

Scope:

- persistent per-character cohort assignment
- active vs benched companion membership
- per-character auto-spawn configuration
- cohort spawn-on-login flow
- target level band evaluation
- catch-up XP state evaluation

Success criteria:

- logging into a character can automatically bring in that character’s active
  cohort when enabled
- different player characters can own distinct cohorts
- the system can determine whether each companion is below, in, or above band

### M5 Continuity

Add lifecycle behaviors that keep the cohort together during real play.

Scope:

- accept resurrection
- death recovery and rejoin behavior
- accept summons
- basic travel-continuity handling needed for party cohesion

Success criteria:

- ordinary deaths and summons do not permanently break the cohort
- companions can recover and rejoin the owner without manual despawn / respawn

### M6 Visibility

Expose companion progression state to the owner in a simple readable way.

Scope:

- current cohort status view
- quest list visibility
- objective progress visibility
- compatibility and blocked-state explanations
- catch-up XP visibility

Success criteria:

- the owner can understand why a companion is behind or blocked
- the owner can see enough quest progress to help companions catch up

### M7 Party Progression

Add real grouped progression behavior.

Scope:

- real XP gain while adventuring with owner
- catch-up XP multiplier when below target band
- grouped quest advancement
- maintaining rough level parity with the owner over time

Success criteria:

- companions can stay within the target band through play
- progression comes from normal gameplay, not DB edits

### M8 Adventure Actions

Add the first practical progression-maintenance actions while the owner is
present.

Scope:

- loot evaluation
- equipping obvious upgrades
- basic repair behavior at vendors
- selling junk while with the owner

Success criteria:

- companions can keep themselves reasonably usable during ordinary play
- equipment and inventory maintenance no longer require manual intervention for
  every small step

## Future Milestones Parking Lot

These are intentionally recorded so they are not forgotten, but they are not
deep-planned yet.

### Progression and World Autonomy

- autonomous roaming and open-world questing
- independent quest pickup and turn-in
- reserve rosters and larger cohort pools per owner
- mounts, flight paths, portals, and long-distance travel logic
- zone preference by level and survival heuristics

### Economy and Maintenance

- vendor purchasing beyond simple repairs
- auction house buying and selling
- more sophisticated gear evaluation
- inventory housekeeping and consumable management

### Tactical Party Systems

- stance-style commands such as follow, stay, assist, passive, defensive,
  aggressive
- live formation editing
- role coordination for tank / healer / dps
- AoE avoidance and pull logic
- dungeon and raid support

### Social and Personality Systems

- bot social behaviors and chatter
- experience traits visible per bot
- memory of real events and places
- group-level coordination memory

### LLM and Conversational Systems

- real LLM integration
- conversational plans that map to in-game behavior
- budget controls and token management

## Open Risks and Follow-Ups

### Bot-Side Visibility

Current assist logic still works around an unreliable bot-side visibility gate.
Any future spell-targeting or self-directed combat logic must avoid reintroducing
that failure mode by blindly routing all eligibility checks through the bot's own
`CanSeeOrDetect` path.

### Direct Level Manipulation

Console- or DB-driven bot leveling remains unsafe today. The cohort system
assumes that catch-up is handled by real gameplay progression. If a safe
socketless-bot leveling path is discovered later, it can be treated as a
separate improvement rather than a requirement for this design.

### Scope Discipline

The long-term vision is much broader than the early companion-cohort system. The
main implementation risk is collapsing multiple future systems into early
milestones and losing a stable foundation. Early work should stay centered on
party usefulness and visible progression.
