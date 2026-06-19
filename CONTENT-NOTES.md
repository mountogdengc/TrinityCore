# Content / DB Fix Notes

Hard-won knowledge from fixing world-content bugs on this server. Each entry is
a *pattern* (reusable next time) plus the *specific case* that taught it. Paired
SQL lives in `sql/updates/world/master/`.

---

## "Creature should carry / ride / sit on something, but doesn't"

**Pattern.** A creature carrying a crate, riding a mount, manning a turret, or
sitting in a chair is almost always the **vehicle + accessory** system, *not* a
model attachment or a spell:

- The **carrier is the vehicle** — `creature_template.VehicleId` points at a
  `Vehicle.db2` record that defines one or more **seats**.
- The **carried thing is the accessory** — a row in `vehicle_template_accessory`
  (`entry` = vehicle, `accessory_entry` = passenger, `seat_id`, `minion`). It is
  auto-seated when the vehicle spawns; no SmartAI mount step is needed.
- **The seat's position offset (over the head, on the back, in a chair) lives in
  client `Vehicle.db2` / `VehicleSeat.db2`.** It is *not* in the world DB and is
  *not* a spell — which is why a "carry ability" never shows up on Wowhead.

**Diagnostic — run against `world`:**
```sql
SELECT entry, name, VehicleId FROM creature_template WHERE entry IN (<carrier>, <passenger>);
SELECT * FROM vehicle_template_accessory WHERE entry IN (<carrier>) OR accessory_entry IN (<passenger>);
SELECT guid, id, MovementType FROM creature WHERE id IN (<carrier>);
```
- `VehicleId = 0` on the carrier  → not a vehicle, nothing can seat on it.
- `vehicle_template_accessory` empty → no carrier→passenger link.
- Passenger has its own `creature` ground spawns co-located with the carrier →
  the carrier renders *inside* the passenger (the classic "mob stuck in the
  crate" look).
- Carrier `MovementType = 0` → stands still (a seated passenger can't self-move,
  but a *vehicle* can — put movement on the carrier, never the passenger).

**Gotchas.**
- Killing a *vehicle* ejects passengers; it does not auto-kill them, and killing
  a passenger does not auto-kill the vehicle. "Destroy the crate and it kills the
  carrier" needs an explicit SmartAI rule (passenger `DEATH` →
  `KILL_UNIT` on `OWNER_OR_SUMMONER`).
- The TDB does **not** ship `Vehicle.db2`; if the world DB has `VehicleId = 0`
  for content that should carry, the carry was simply never wired. You supply the
  `VehicleId`/seat yourself — and the only reliable way to get the right one is
  to try a candidate in-game (`.npc add <carrier>`) and eyeball where the
  passenger lands.
- Enum numbers differ between cores. On this **`master`** fork the SmartAI values
  are e.g. `ENTER_VEHICLE = 155`, `EXIT_VEHICLE = 157`, `JUST_CREATED = 63`,
  `PASSENGER_REMOVED = 28` — **not** the 3.3.5/AzerothCore numbers (46/43/54/55)
  that AI assistants tend to hand you. Always confirm against
  `src/server/game/AI/SmartScripts/SmartScriptMgr.h`.

### Case: Northwatch Lugs not carrying Supply Crates (quest 25167)

- **Quest:** 25167 "Breaking the Chain" (Durotar) — destroy 3 crates, kill 10 Lugs.
- **Entries:** Lug `39245` (Human) / `39249` (Dwarf, KillCredit → 39245);
  Supply Crate `39251` (the destroy objective — must stay attackable).
- **Symptom:** crates on the ground, Lugs rendered inside them, nothing walked.
- **Cause:** carry never wired — all three had `VehicleId = 0`, no
  `vehicle_template_accessory`, Lug spawns `MovementType = 0`. Not in any in-tree
  TDB snapshot, so it's unimplemented content, not a regression.
- **Fix:** `sql/updates/world/master/2026_06_19_00_world.sql` — Lug becomes the
  vehicle, crate is an accessory on seat `@SEAT`, SmartAI "crate dies → kill the
  carrying Lug", Lugs get random roam.
- **Open knob:** the over-head seat offset isn't in the TDB, so `@VEH`/`@SEAT`
  default to `0` (safe no-op) and must be dialed in live, then baked into the
  migration. Step 5 of that file (removing the now-duplicate ground crates) stays
  commented until `@VEH` is confirmed.
