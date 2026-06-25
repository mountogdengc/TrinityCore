# Player-bot UX — future milestones (vision / parking lot)

Aspirational bot features captured from a mature WotLK player-bot server
(mod-playerbots + a custom client addon). **None of this is committed work** — it's a
backlog so we don't lose the ideas. Reference screenshots live in `doc/` (dated
2026-06-25). Current shipped/active bot work is in
`src/server/scripts/Custom/Bots/ROADMAP.md` and `CHANGELOG-custom.md`.

## Retail (12.0.5) feasibility note — read first

The **server-side** of every item below is feasible in this fork (it's all
`BotMgr`/policy logic + DB state). The **rich client UIs** are the catch:

- WotLK private servers often drive bespoke UI with **custom server opcodes** — that
  **does not port** to retail (the 12.0.5 client only speaks Blizzard's protocol).
- On retail, custom UI is a **client AddOn** talking to the server via **addon
  messages** (`C_ChatInfo.SendAddonMessage` ↔ server `CHAT_MSG_ADDON`). Config-style
  UIs need no protected/secure actions, so combat lockdown isn't a blocker. Cost:
  write a retail addon, define the message protocol both ways, and **players must
  install the addon** (not server-pushed).
- **No-addon fallback** that works today: **gossip menus** (stock client window with
  clickable options) and `.bot` **chat commands**.

## M-F1 — Formation & Orders (the next big UX milestone)

Server-side **formation engine**: named presets (Arrow / Line / Circle / Chevron),
each bot's offset relative to an anchor, per-bot "follows X" and "takes orders from Y",
quick-place Behind/Front/Left/Right. Generalizes the current per-slot-angle work
(`BotMovementPolicy`) into computed offsets feeding `MoveFollow`/`MoveChase`.
- **UI options:** gossip menu / chat first; the drag-and-drop Formation/Orders panel
  (ref `doc/2026-06-25_12-32-21.webp`) as a later custom addon.
- Builds directly on `docs/superpowers/specs/2026-06-25-bot-ranged-positioning-formation-design.md`.

## M-F2 — Bot personality / traits

Per-bot trait vector — Experience, Bravery, Altruism, Sociability, Greed, Diligence,
Curiosity, Aggression (refs `doc/image7.png`, Persona tab in
`doc/2026-06-25_12-53-39.webp`). Traits would bias behavior (e.g. Bravery → leash/flee
thresholds, Altruism → assist/heal priority, Aggression → pull willingness). Pure,
DB-persisted, testable; no client needed to *have* traits (a viewer/editor is UI later).

## M-F3 — Bot Inspector (debug/control panel)

A diagnostic + control surface (refs `doc/2026-06-25_12-53-39.webp` and the
`Waloria_*.mp4` capture): tabs Brain / Combat / Kit / Quests / Bags / Persona /
State / Move / Mind / LLM (the video shows a collapsed-category variant: Brain /
Combat / Etc / Quests). Live telemetry per bot:
- **Doing** — high-level state (resting / wandering / idle).
- **Casting & Moving** — granular movement logic ("moving to do → wandering",
  "moving on path").
- **State** — combat status (COMBAT / …); **Target** (none / unit).
- **Pos & Pathing** — exact coordinates + number of nodes in the current path.

Server already owns most of this state; exposable first via a `.bot inspect <name>`
command dumping the same fields, addon panel later.

## M-F6 — Admin world-map: bot tracking + road routing (avoid-mobs pathing)

A top-down GM/admin map (the `20260620-*.mp4` capture) showing every bot as an icon,
with **white path lines drawn along roads** — bots route along roads to **minimize
random mob encounters** rather than beelining. The admin **hovers** a bot for
name/status and **clicks** to view/set its destination and route.

Two layers:
- **Server-side routing** — a road/path graph the bots prefer when traveling
  (waypoint network with mob-density-aware cost), feeding the same MotionMaster path
  the bot already walks. This is the substantive, testable piece and is independent
  of any UI.
- **Admin map UI** — bot positions + routes + click-to-retarget. On retail this is
  the hardest UI to reproduce (the WotLK build likely drove it with custom
  opcodes/world-map hooks); candidates are a minimap/world-map **addon** fed by addon
  messages, or an **out-of-game web/SOAP dashboard** reading bot positions (no client
  changes — likely the most practical first cut).

Related autonomy seen in the videos (idle/town behavior): bots **wander/rest** in
town, **earn/use mounts**, and even **cast and use a Mage portal**. Track that as its
own "bot idle/autonomy" follow-up under this milestone.

## M-F4 — Dialogue & recruitment flavor (optionally LLM-backed)

Ambient/recruitment/dismissal chatter (refs `doc/image2.png` tavern banter,
`image3.png` "Good. I could use the company.", `image4.png` "Another time, perhaps.").
Baseline: canned line tables keyed by event + personality. Stretch: an **LLM** backend
(the reference server has an "LLM" tab) generating lines — would be an external service
call from the server, gated/off by default; treat as a separate, opt-in subsystem.

## M-F5 — Bot transmog / outfits

Per-slot appearance builder and saved outfit presets applied to bots (refs
`doc/image5.png` builder, `doc/image6.png` outfits). Server applies transmog to the
bot's items; cosmetic only. UI is addon-tier; a `.bot outfit <name>` command could
drive saved presets without UI.

## Source media

Reference captures in `doc/` (2026-06-25): screenshots `image.png`–`image7.png`,
`2026-06-25_12-32-21.webp` (formation panel), `2026-06-25_12-53-39.webp` (inspector),
and two videos — `Waloria_gC8m1Jg6pC.mp4` (tavern chatter + bot inspector in motion)
and `20260620-1842-29.7426435.mp4` (admin world-map with road routing). Video content
is folded into M-F3, M-F4, and M-F6 above.
