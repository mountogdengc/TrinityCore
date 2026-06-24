# TrinityCore Repack Audit: Spell and AI Integrity

## Purpose

This document tracks database and runtime integrity issues in this fork that look like stale or broken TrinityCore-era content wiring, especially:

- SmartAI rows that reference missing spell IDs
- NPC templates that are still live but have partially dead caster kits
- Script/AI wiring mismatches where the DB no longer matches the intended runtime behavior

This is a standalone repack audit. The goal is not upstream conformity. The goal is to identify internal inconsistencies that are likely to surface as gameplay bugs, loader errors, or silent behavior gaps.

## Audit Rules

- Treat the running repack as the source of truth, not upstream.
- Prefer live-spawned NPCs over dead templates when prioritizing fixes.
- Prefer replacement candidates already used quietly by similar live NPCs in this repack.
- Do not assume a spell is valid just because it existed historically.
- Do not assume a spell is invalid just because `hotfixes` lookup is sparse in this repack.

## Status Legend

- `open`: identified but not yet classified
- `classified`: affected NPCs and likely candidates identified
- `ready to fix`: mapping chosen with enough confidence to write SQL
- `defer until encountered in play`: real issue, but low-priority enough to leave for live gameplay confirmation
- `fixed`: cleanup applied

## Global Findings

- The `hotfixes` database is too sparse to serve as a reliable spell-existence oracle in this fork.
- SOAP `lookup spell` is also not a reliable validator here; it can return `No spells found!` for spell IDs already wired on live NPCs.
- The most reliable signals so far are:
  - worldserver startup invalid-spell errors
  - whether a candidate spell is already used by similar live NPCs without appearing in the invalid-spell set

## Fix Readiness

### Ready To Fix

- `Alterac Valley Explorer Casters`
  - `15242 -> 15228`
  - `14145 -> 13339`
- `Scarshield Spellbinder`
  - `13748 -> 15979`
  - `15785 -> 15800`
- `Risen Sorcerer`
  - `15230 -> 37361`
  - `14145 -> 13339`
- `Arcane Bolt (15230)`
  - `10422 -> 37361`
  - `11484 -> 15979`
  - `9217 -> 15979`
  - `13197 -> 15979`
- `Shadow Word: Pain (14032)`
  - `18637 -> 17146`
  - `5648 -> 11639`
  - `8912 -> 11639`
  - `9045 -> 11639`
  - `32279 -> 11639`
  - `17309 -> 11639`
  - `29885 -> 11639`
- `Fireball (14034)`
  - `17771 -> 15228`
  - `18634 -> 15228`
  - `19016 -> 11921`
  - `8904 -> 9053`
  - `19413 -> 9053`
  - `18685 -> 9053`
  - `30409 -> 9053`
- `Cripple (20812)`
  - `28200 -> 52498`
  - `35454 -> 11443`
  - `36441 -> 11443`
- `Strike (34920)`
  - `18315 -> 15580`
  - `18309 -> 15580`
- `Shadow Bolt Volley (14887)`
  - `10477 -> 17228`
  - `9261 -> 17228`
  - `11383 -> 17228`
- `Immolate (37668)`
  - `18312 -> 17883`
  - `17938 -> 17883`
  - `4306 -> 11962`
- `Shadow Bolt (15232)`
  - `17771 -> 12471`
  - `17694 -> 12471`
  - `18640 -> 12471`
  - `10398 -> 9613`
  - `11448 -> 9613`
  - `22393 -> 9613`
  - `23022 -> 9613`
  - `24762 -> 9613`
  - `9034 -> 9613`
- `Scorch (15241)`
  - `17771 -> 36807`
  - `8910 -> 12466`
  - `17269 -> 17290`
- `Trample (15550)`
  - `24613 -> 51944`
  - `24614 -> 51944`
  - `25452 -> 51944`
  - `44317 -> 5568`
  - `15537 -> 5568`
  - `17723 -> 5568`
- `Shadow Word: Pain (34942)`
  - `18331 -> 34941`

### Decision-Needed

- `Rain of Fire (11990)`
  - broken family is confirmed, but the live replacements split by archetype rather than collapsing to one direct handoff
- `Shadow Bolt Volley (9081)`
  - good generic candidate exists, but not enough family-level certainty yet
- `Arcane Bolt (13748)`
  - map `229` and map `0` users are mostly explainable; map `230` remains a broken island
- `Mana Burn (15785)`
  - `9098` is clear; remaining users still need a substitute choice
- `Shadow Bolt Volley (15245)`
  - broken island with no cleaner same-map successor
- `Blast Wave (17145)`
  - `18315` is clear; remaining users are still open
- `Strike (18368)`
  - broad melee family with no single handoff yet
- `Immolate (20787)`
  - live broken family, but replacements still vary too much by archetype/map
- `Regrowth (22695)`
  - support family with no direct same-map handoff yet
- `Faerie Fire (25602)`
  - large broken island without a clean local successor

### Defer Until Encountered In Play

- `Mana Burn (22936)`
  - specialized triggered family with no handoff yet
- `Mana Burn (22947)`
  - small live family with no handoff yet
- `Mana Burn (26046)`
  - small live family with no handoff yet
- `Mana Burn (26049)`
  - small live family with no handoff yet
- `Mana Burn (28301)`
  - single-creature-family case with no handoff yet
- `Arcane Explosion (34933)`
  - likely separate triggered/on-death evaluation rather than a simple family-level substitute

## Families

### Family: Alterac Valley Explorer Casters

- Status: `ready to fix`
- Bad spell IDs:
  - `15242` (`Fireball`)
  - `14145` (`Fire Blast`)
- Affected live NPCs:
  - `13096` `Coldmine Explorer`
  - `13099` `Irondeep Explorer`
- Live presence:
  - `13096`: `76` spawns on map `30`, zone `2597`
  - `13099`: `64` spawns on map `30`, zone `2597`
- Current kit:
  - `12544` `Frost Armor`
  - `15242` dead `Fireball`
  - `14145` dead `Fire Blast`
  - `15244` `Cone of Cold`
- Candidate replacements considered:
  - `15242 -> 15228`
  - `14145 -> 13339`
- Reasoning:
  - `15228` is already used locally on the same AV map by `11604` `Whitewhisker Geomancer` as a `Fireball` replacement.
  - `13339` is used broadly in this repack by mage-like humanoid casters and fits this mixed frost/fire kit better than more specialized alternatives.
- Decision:
  - Replace `15242` with `15228`
  - Replace `14145` with `13339`

### Family: Scarshield Spellbinder

- Status: `ready to fix`
- Bad spell IDs:
  - `13748` (`Arcane Bolt`)
  - `15785` (`Mana Burn`)
- Affected live NPCs:
  - `9098` `Scarshield Spellbinder`
- Live presence:
  - `20` spawns on map `229`
- Current kit:
  - `13748` dead `Arcane Bolt`
  - `15785` dead `Mana Burn`
  - `15122` `Counterspell`
  - `15123` `Resist Fire`
- Candidate replacements considered:
  - `13748 -> 15979`
  - `15785 -> 15800`
  - Other global candidates were reviewed but deprioritized because they were not as locally grounded.
- Reasoning:
  - `15979` is already used on the same map by `9201` `Spirestone Ogre Magus` as a live `Arcane Bolt`.
  - `15800` is already used on the same map by `9240` `Smolderthorn Shadow Priest` as a live `Mana Burn`.
  - Same-map replacements are preferred over generic replacements from unrelated zones.
- Decision:
  - Replace `13748` with `15979`
  - Replace `15785` with `15800`

### Family: Risen Sorcerer

- Status: `ready to fix`
- Bad spell IDs:
  - `15230` (`Arcane Bolt`)
  - `14145` (`Fire Blast`)
- Affected live NPCs:
  - `10422` `Risen Sorcerer`
- Live presence:
  - `3` spawns on map `329`
- Current kit:
  - `12544` `Frost Armor`
  - `17150` `Arcane Might`
  - `15230` dead `Arcane Bolt`
  - `13323` `Polymorph`
  - `14145` dead `Fire Blast`
- Candidate replacements considered:
  - `15230 -> 37361`
  - `14145 -> 13339`
- Reasoning:
  - `37361` is already used on the same map by `10390` `Skeletal Guardian` as a live `Arcane Bolt`.
  - `13339` remains the strongest generic `Fire Blast` replacement identified in this repack for mage-style humanoid casters.
- Decision:
  - Replace `15230` with `37361`
  - Replace `14145` with `13339`

### Family: Rain of Fire (`11990`)

- Status: `classified`
- Bad spell ID:
  - `11990` (`Rain of Fire`)
- Affected live NPCs:
  - `5862` `Twilight Geomancer`
  - `38926` `Twilight Flamecaller`
  - `4334` `Firemane Flamecaller`
  - `40882` `Twilight Infiltrator`
  - `17694` `Shadowmoon Darkcaster`
  - `35454` `Doomguard Invader`
  - `36441` `Doomguard Defender`
  - `2417` `Grel'borg the Miser`
  - `7606` `Oro Eyegouge`
  - `9860` `Salia`
- Live presence summary:
  - `5862`: `34` spawns on map `0`
  - `38926`: `32` spawns on map `1`
  - `4334`: `31` spawns on map `1`
  - `40882`: `3` spawns on map `1`
  - `17694`: `2` spawns on map `540`
  - `35454`: `2` spawns on map `1`
  - `36441`: `2` spawns on map `1`
  - `2417`: `1` spawn on map `0`
  - `7606`: `1` spawn on map `209`
  - `9860`: `1` spawn on map `1`
- Notes:
  - This family is not a single clean replacement case.
  - The live repack uses several different `Rain of Fire` spell IDs depending on archetype and era, including `79955`, `39273`, `20754`, `31598`, `33627`, and `33508`.
  - The safest confirmed mapping so far is `17694` `Shadowmoon Darkcaster`, where the same NPC already uses `33508` for heroic difficulty.
- Candidate mappings with strongest support so far:
  - `17694` `Shadowmoon Darkcaster`: `11990 -> 33508`
- Open subgroups needing more comparison before a final mapping:
  - twilight/fire caster group:
    - `5862` `Twilight Geomancer`
    - `38926` `Twilight Flamecaller`
    - `4334` `Firemane Flamecaller`
    - `40882` `Twilight Infiltrator`
  - demon/warlock group:
    - `35454` `Doomguard Invader`
    - `36441` `Doomguard Defender`
    - `7606` `Oro Eyegouge`
    - `2417` `Grel'borg the Miser`
    - `9860` `Salia`

### Family: Shadow Word: Pain (`14032`)

- Status: `ready to fix`
- Bad spell ID:
  - `14032` (`Shadow Word: Pain`)
- Affected live NPCs:
  - `5648` `Sandfury Shadowcaster`
  - `8912` `Twilight's Hammer Torturer`
  - `9045` `Scarshield Acolyte`
  - `32279` `Vile Torturer`
  - `18637` `Cabal Shadow Priest`
  - `17309` `Hellfire Watcher`
  - `29885` `Mildred the Cruel`
- Live presence summary:
  - `5648`: `26` spawns on map `209`
  - `8912`: `24` spawns on map `230`
  - `9045`: `11` spawns on map `229`
  - `32279`: `9` spawns on map `571`
  - `18637`: `4` spawns on map `555`
  - `17309`: `2` spawns on map `543`
  - `29885`: `1` spawn on map `571`
- Candidate replacements considered:
  - `11639`
  - `17146`
  - `34441`
- Reasoning:
  - `17146` is the strongest mapping for `18637` because the same NPC already uses it for heroic difficulty.
  - `11639` is the dominant general-purpose `Shadow Word: Pain` spell across live priest-like casters in this repack.
  - `34441` is a plausible Northrend-specific alternative for some map `571` shadow casters, but support is narrower than `11639`.
- Strongest current per-NPC mapping:
  - `18637` `Cabal Shadow Priest`: `14032 -> 17146`
- Decision:
  - `18637` `Cabal Shadow Priest`: `14032 -> 17146`
  - `5648` `Sandfury Shadowcaster`: `14032 -> 11639`
  - `8912` `Twilight's Hammer Torturer`: `14032 -> 11639`
  - `9045` `Scarshield Acolyte`: `14032 -> 11639`
  - `32279` `Vile Torturer`: `14032 -> 11639`
  - `17309` `Hellfire Watcher`: `14032 -> 11639`
  - `29885` `Mildred the Cruel`: `14032 -> 11639`

### Family: Fireball (`14034`)

- Status: `ready to fix`
- Bad spell ID:
  - `14034` (`Fireball`)
- Affected live NPCs:
  - `8904` `Shadowforge Senator`
  - `17771` `Murkblood Oracle`
  - `18634` `Cabal Summoner`
  - `19016` `Hellfire Familiar`
  - `19413` `Shattered Hand Mage`
  - `18685` `Okrek`
  - `30409` `Apprentice Osterkilgr`
- Live presence summary:
  - `8904`: `34` spawns on map `230`
  - `17771`: `8` spawns on map `546`
  - `18634`: `8` spawns on map `555`
  - `19016`: `7` spawns on map `542`
  - `19413`: `7` spawns on map `530`
  - `18685`: `4` spawns on map `530`
  - `30409`: `1` spawn on map `571`
- Candidate replacements considered:
  - `15228`
  - `11921`
  - `9053`
  - `20793`
- Reasoning:
  - `17771` `Murkblood Oracle` already uses `15228` for heroic difficulty on the same phase/action slot.
  - `18634` `Cabal Summoner` already uses `15228` for heroic difficulty on the same action.
  - `19016` `Hellfire Familiar` already uses `11921` for normal difficulty while the dead `14034` appears on the heroic row.
  - For the remaining fire casters, `9053` is the strongest generic live `Fireball` in this repack, with broad usage on the same or adjacent maps.
- Strongest current per-NPC mappings:
  - `17771` `Murkblood Oracle`: `14034 -> 15228`
  - `18634` `Cabal Summoner`: `14034 -> 15228`
  - `19016` `Hellfire Familiar`: `14034 -> 11921`
- Decision:
  - `17771` `Murkblood Oracle`: `14034 -> 15228`
  - `18634` `Cabal Summoner`: `14034 -> 15228`
  - `19016` `Hellfire Familiar`: `14034 -> 11921`
  - `8904` `Shadowforge Senator`: `14034 -> 9053`
  - `19413` `Shattered Hand Mage`: `14034 -> 9053`
  - `18685` `Okrek`: `14034 -> 9053`
  - `30409` `Apprentice Osterkilgr`: `14034 -> 9053`

### Family: Cripple (`20812`)

- Status: `ready to fix`
- Bad spell ID:
  - `20812` (`Cripple`)
- Affected live NPCs:
  - `28200` `Dark Necromancer`
  - `35454` `Doomguard Invader`
  - `36441` `Doomguard Defender`
- Live presence summary:
  - `28200`: `5` spawns on map `595`
  - `35454`: `2` spawns on map `1`
  - `36441`: `2` spawns on map `1`
- Candidate replacements considered:
  - `52498`
  - `11443`
  - `33787`
- Reasoning:
  - `28200` `Dark Necromancer` already uses `52498` for heroic difficulty on the same spell slot, making that the strongest direct replacement.
  - `11443` is the dominant live `Cripple` variant across the repack and is the strongest generic candidate for the doomguard pair.
  - `33787` is a live dungeon-specific `Cripple`, but support is much narrower than `11443`.
- Decision:
  - `28200` `Dark Necromancer`: `20812 -> 52498`
  - `35454` `Doomguard Invader`: `20812 -> 11443`
  - `36441` `Doomguard Defender`: `20812 -> 11443`
- Notes:
  - Additional `20812` SmartAI users exist on unspawned templates such as `12396`, `36412`, and `36442`, but the live-spawned fix set above is now clear enough to implement.

### Family: Shadow Bolt Volley (`9081`)

- Status: `classified`
- Bad spell ID:
  - `9081` (`Shadow Bolt Volley`)
- Affected live NPCs:
  - `4848` `Shadowforge Darkcaster`
  - `2605` `Zalas Witherbark`
  - `9517` `Shadow Lord Fel'dan`
  - `49148` `Dark Ritualist Zakahn`
- Live presence summary:
  - `4848`: `2` spawns on map `70`
  - `2605`: `1` spawn on map `0`
  - `9517`: `1` spawn on map `1`
  - `49148`: `1` spawn on map `1`
- Candidate replacements considered:
  - `14887`
  - `17228`
  - `15245`
  - `39175`
- Reasoning:
  - `14887` is the strongest broad live `Shadow Bolt Volley` variant in active dungeon-style caster kits and already appears on `9261` `Firebrand Darkweaver`.
  - `17228` is another viable live volley spell, but it trends more toward different caster families and map clusters than `14887`.
  - `15245` and `39175` are both live but narrower in scope and appear less suitable as generic replacements for these four entries.
- Current best generic candidate:
  - `9081 -> 14887`
- Notes:
  - This family still needs one more pass before being marked `ready to fix`, but `14887` currently has the strongest support.

### Family: Arcane Bolt (`15230`)

- Status: `ready to fix`
- Bad spell ID:
  - `15230` (`Arcane Bolt`)
- Affected live NPCs:
  - `13197` `Fel Lash`
  - `10422` `Risen Sorcerer`
  - `11484` `Residual Monstrosity`
  - `9217` `Spirestone Lord Magus`
- Live presence summary:
  - `13197`: `17` spawns on map `429`
  - `10422`: `3` spawns on map `329`
  - `11484`: `3` spawns on map `429`
  - `9217`: `1` spawn on map `229`
- Strongest local replacement signals:
  - map `229`: `9201` `Spirestone Ogre Magus` uses `15979` `Arcane Bolt`
  - map `429`: `11480` `Arcane Aberration` uses `15979` `Arcane Bolt`
  - map `329`: `10390` `Skeletal Guardian` uses `37361` `Arcane Bolt`
- Strongest current per-NPC mappings:
  - `10422` `Risen Sorcerer`: `15230 -> 37361`
  - `11484` `Residual Monstrosity`: `15230 -> 15979`
  - `9217` `Spirestone Lord Magus`: `15230 -> 15979`
- Decision:
  - `10422` `Risen Sorcerer`: `15230 -> 37361`
  - `11484` `Residual Monstrosity`: `15230 -> 15979`
  - `9217` `Spirestone Lord Magus`: `15230 -> 15979`
  - `13197` `Fel Lash`: `15230 -> 15979`

### Family: Arcane Bolt (`13748`)

- Status: `classified`
- Bad spell ID:
  - `13748` (`Arcane Bolt`)
- Affected live NPCs:
  - `8900` `Doomforge Arcanasmith`
  - `9098` `Scarshield Spellbinder`
  - `8913` `Twilight Emissary`
  - `12378` `Damned Soul`
  - `9693` `Bloodaxe Evoker`
- Live presence summary:
  - `8900`: `24` spawns on map `230`
  - `9098`: `20` spawns on map `229`
  - `8913`: `18` spawns on map `230`
  - `12378`: `14` spawns on map `0`
  - `9693`: `7` spawns on map `229`
- Strongest current mapping:
  - `9098` `Scarshield Spellbinder`: `13748 -> 15979`
- Additional strong local mappings:
  - `9693` `Bloodaxe Evoker`: `13748 -> 15979`
  - `12378` `Damned Soul`: `13748 -> 13901`
- Notes:
  - map `229` has a direct local live `Arcane Bolt` candidate in `15979` via `9201` `Spirestone Ogre Magus`
  - map `230` does not yet show the same clean local handoff; current live map `230` `Arcane Bolt` usage is still the broken `13748` family itself, so `8900` and `8913` remain open
  - map `0` has live `Arcane Bolt` alternatives in `13901` and `20829`; `12378` currently leans toward `13901` because it is the cleaner live caster-style `Arcane Bolt` on that map
  - The remaining unresolved map `230` users now look like a self-contained broken cluster rather than a missed local handoff. If we fix them, it will likely require choosing the least-bad neighboring spell rather than reusing a same-map live successor.

### Family: Mana Burn (`15785`)

- Status: `classified`
- Bad spell ID:
  - `15785` (`Mana Burn`)
- Affected live NPCs:
  - `19306` `Mana Leech`
  - `9098` `Scarshield Spellbinder`
  - `10426` `Risen Inquisitor`
  - `4515` `Death's Head Acolyte`
- Live presence summary:
  - `19306`: `24` spawns on map `557`
  - `9098`: `20` spawns on map `229`
  - `10426`: `3` spawns on map `329`
  - `4515`: `2` spawns on map `47`
- Strongest current mapping:
  - `9098` `Scarshield Spellbinder`: `15785 -> 15800`
- Notes:
  - The remaining users do not yet have as clean a same-map replacement path as `9098`
  - map `557` has live `Mana Burn` usage on `18331` `Ethereal Darkcaster` via `34930` normal / `34931` heroic
  - map `530` has a strong separate `Mana Burn` family centered on `11981`, but that does not directly resolve the current live `15785` users
  - `10426` and `4515` remain open pending better local comparison
  - `19306` `Mana Leech` also remains open; its kit is closer to a mana-creature/drain family than to priest-like or caster-humanoid `Mana Burn` users, so forcing it onto `15800`, `34930/34931`, or `11981` would be too speculative in this pass

### Family: Shadow Bolt Volley (`14887`)

- Status: `ready to fix`
- Bad spell ID:
  - `14887` (`Shadow Bolt Volley`)
- Affected live NPCs:
  - `10477` `Scholomance Necromancer`
  - `9261` `Firebrand Darkweaver`
  - `11383` `High Priestess Hai'watna`
- Live presence summary:
  - `10477`: `15` spawns on map `289`
  - `9261`: `12` spawns on map `229`
  - `11383`: `1` spawn on map `0`
- Strongest local replacement signal:
  - map `289` already uses `17228` `Shadow Bolt Volley` on:
    - `10433` `Marduk Blackpool`
    - `10472` `Scholomance Occultist`
- Broader live fallback signal:
  - `17228` is the broadest classic-era live `Shadow Bolt Volley` still in SmartAI use in this repack, with active users on maps `0`, `30`, `289`, and `429`.
  - The alternative live families `15245` and `39175` exist, but they are tied to smaller or more specialized clusters than `17228`.
- Decision:
  - `10477` `Scholomance Necromancer`: `14887 -> 17228`
  - `9261` `Firebrand Darkweaver`: `14887 -> 17228`
  - `11383` `High Priestess Hai'watna`: `14887 -> 17228`
- Notes:
  - `10477` has the strongest direct local evidence.
  - `9261` and `11383` do not expose a same-map successor, so these two mappings are promoted as the least-risk generic live-repack handoff rather than a same-NPC heroic/normal conversion.

### Family: Shadow Bolt Volley (`15245`)

- Status: `classified`
- Bad spell ID:
  - `15245` (`Shadow Bolt Volley`)
- Affected live NPCs:
  - `19191` `Arazzius the Cruel`
  - `19354` `Arzeth the Merciless`
  - `22006` `Shadowlord Deathwail`
  - `48312` `High Warlock Xi'lun`
- Live presence summary:
  - `19191`: `1` spawn on map `530`
  - `19354`: `1` spawn on map `530`
  - `22006`: `1` spawn on map `530`
  - `48312`: `1` spawn on map `0`
- Notes:
  - This is a distinct live `Shadow Bolt Volley` cluster from `14887`
  - map `530` currently shows this family self-contained, without a stronger same-map live replacement than the dead ID itself
  - the broader map `530` spell ecosystem does not currently expose another live `Shadow Bolt Volley` variant on the same map that cleanly supersedes this one
  - the family is confirmed live and broken, but no replacement is locked yet
  - At this point the map `530` entries also look like a self-contained broken cluster rather than a case of a missed obvious local successor.

### Family: Trample (`15550`)

- Status: `ready to fix`
- Bad spell ID:
  - `15550` (`Trample`)
- Affected live NPCs:
  - `24613` `Mammoth Calf`
  - `24614` `Wooly Mammoth`
  - `25452` `Scourged Mammoth`
  - `17723` `Bog Giant`
  - `15537` `Anubisath Warrior`
  - `44317` `The Ravenian`
- Live presence summary:
  - `24613`: `99` spawns on map `571`
  - `24614`: `67` spawns on map `571`
  - `25452`: `55` spawns on map `571`
  - `17723`: `9` spawns on map `546`
  - `15537`: `3` spawns on map `531`
  - `44317`: `1` spawn on map `0`
- Strongest local replacement signals:
  - map `571` already uses `51944` `Trample` on multiple mammoth- and ancient-type creatures:
    - `26271` `Emaciated Mammoth Bull`
    - `26272` `Emaciated Mammoth`
    - `26711` `Injured Mammoth`
    - `30445` `Ice Steppe Bull`
    - `31229` `Ancient Watcher`
  - map `0` broadly uses `5568` `Trample` as the dominant old-world generic `Trample`
- Strongest current per-NPC mappings:
  - `24613` `Mammoth Calf`: `15550 -> 51944`
  - `24614` `Wooly Mammoth`: `15550 -> 51944`
  - `25452` `Scourged Mammoth`: `15550 -> 51944`
  - `44317` `The Ravenian`: `15550 -> 5568`
- Additional supporting signals:
  - `15324` `Qiraji Gladiator` already uses `5568` together with `10966` `Uppercut`, which closely matches the `15537` `Anubisath Warrior` kit.
  - The broad old-world/live generic `Trample` family around `5568` includes large melee beasts, elementals, protectors, and trampler-type creatures that fit `17723` better than the Northrend-specific `51944` family.
- Decision:
  - `24613` `Mammoth Calf`: `15550 -> 51944`
  - `24614` `Wooly Mammoth`: `15550 -> 51944`
  - `25452` `Scourged Mammoth`: `15550 -> 51944`
  - `44317` `The Ravenian`: `15550 -> 5568`
  - `15537` `Anubisath Warrior`: `15550 -> 5568`
  - `17723` `Bog Giant`: `15550 -> 5568`
- Notes:
  - This is a non-caster family and should stay separate from the SmartAI caster-kit cleanup groups.
  - `15550` is still actively used by a large live Northrend mammoth cluster, so this is real gameplay debt rather than dead data noise.
  - The mammoth mappings are strongly local and high-confidence.
  - `15537` has the strongest generic fallback evidence because of the `Qiraji Gladiator` kit overlap.
  - `17723` is promoted on broader family-fit evidence rather than a same-map or same-NPC successor.

### Family: Blast Wave (`17145`)

- Status: `classified`
- Bad spell ID:
  - `17145` (`Blast Wave`)
- Affected live NPCs:
  - `11257` `Scholomance Handler`
  - `10425` `Risen Battle Mage`
  - `18315` `Ethereal Theurgist`
  - `30475` `Thane Illskar`
- Live presence summary:
  - `11257`: `5` spawns on map `289`
  - `10425`: `4` spawns on map `329`
  - `18315`: `4` spawns on map `557`
  - `30475`: `1` spawn on map `571`
- Strongest current mapping:
  - `18315` `Ethereal Theurgist`: `17145 -> 38064`
- Alternative live `Blast Wave` families reviewed:
  - `38064`
    - strong direct fit only for `18315`, where it already exists as the heroic-difficulty counterpart
  - `22424`
    - active on `Pusillin`, `Death Talon Wyrmkin`, `Phasing Sorcerer`, and `Slag`
    - does not line up well with the remaining `17145` users by map, archetype, or surrounding spell kit
  - `15744`
    - active on `Bloodaxe Evoker` and `Phasing Sorcerer`
    - confirms that other live `Blast Wave` families exist, but does not provide a cleaner handoff for `11257`, `10425`, or `30475`
- Notes:
  - `18315` already uses `38064` for heroic difficulty on the same spell role, making that a direct handoff.
  - `11257` `Scholomance Handler` is an arcane/frost hybrid kit (`Arcane Blast`, `Cone of Cold`, `Blast Wave`) with no matching live `Blast Wave` successor in the same dungeon cluster.
  - `10425` `Risen Battle Mage` is a fire/arcane undead-caster kit (`Immolate`, `Arcane Explosion`, `Blast Wave`) that also lacks a convincing live local handoff.
  - `30475` `Thane Illskar` mixes `Fireball`, `Frostfire Bolt`, `Rain of Fire`, and `Blast Wave`, and none of the active `Blast Wave` families match that mixed Northrend event-boss profile closely enough.
  - This family remains `decision-needed` because the non-ethereal users do not collapse toward one trustworthy replacement family; promoting a generic fallback here would be more speculative than the recent `ready to fix` families.

### Family: Strike (`18368`)

- Status: `classified`
- Bad spell ID:
  - `18368` (`Strike`)
- Affected live NPCs:
  - `11462` `Warpwood Treant`
  - `7428` `Frostmaul Giant`
  - `11622` `Rattlegore`
- Live presence summary:
  - `11462`: `26` spawns on map `429`
  - `7428`: `6` spawns on map `1`
  - `11622`: `1` spawn on map `289`
- Candidate replacements considered:
  - `15580`
  - `11976`
  - `12057`
  - `13446`
- Notes:
  - This is a broad melee `Strike` family rather than a caster issue.
  - `11462` `Warpwood Treant`, `7428` `Frostmaul Giant`, and `11622` `Rattlegore` do not collapse toward one convincing live `Strike` family.
  - The live alternatives split across generic melee (`11976`), heavier bruiser variants (`13446`, `12057`), and narrower modernized variants (`15580`), with no one substitute standing out across all three kits.
  - This family remains active but blocked unless one of the three archetypes is split out and fixed independently.

### Family: Immolate (`20787`)

- Status: `classified`
- Bad spell ID:
  - `20787` (`Immolate`)
- Affected live NPCs:
  - `8910` `Blazing Fireguard`
  - `7372` `Deadwind Warlock`
  - `5771` `Jugkar Grim'rod`
  - `32263` `Overseer Veraj`
- Live presence summary:
  - `8910`: `19` spawns on map `230`
  - `7372`: `6` spawns on map `0`
  - `5771`: `1` spawn on map `1`
  - `32263`: `1` spawn on map `571`
- Candidate replacements considered:
  - `11962`
  - `20826`
  - `17883`
  - `12742`
- Strongest current subgroup:
  - `7372` `Deadwind Warlock`: `20787 -> 20826`
  - `32263` `Overseer Veraj`: `20787 -> 20826`
- Additional likely mapping under review:
  - `5771` `Jugkar Grim'rod`: currently leans `20826`
- Notes:
  - This is a live `Immolate` family, but current replacements vary substantially by map and archetype.
  - `7372` and `32263` both share a tight warlock-style kit with `Shadow Bolt` plus `Curse of Weakness`, which aligns well with the live `20826` pattern seen on `Blackrock Sorcerer` and `Blackrock Warlock`.
  - `5771` also looks closer to that older warlock/fire-caster `20826` pattern than to the later TBC `11962` or dungeon-paired `17883` family, but the support is weaker because it is a one-off NPC.
  - `8910` `Blazing Fireguard` is the blocker: its `Fire Blast` plus `Scorch` plus `Immolate` kit does not line up cleanly with the warlock-style `20826` family or the later warlock-heavy `11962` family.
  - This family stays `decision-needed` unless we either split `8910` out from the warlock-pattern subgroup or find a better fire-caster local `Immolate` handoff for it.

### Family: Regrowth (`22695`)

- Status: `decision-needed`
- Bad spell ID:
  - `22695` (`Regrowth`)
- Affected live NPCs:
  - `96545` `Growing Squashling`
  - `14303` `Petrified Guardian`
- Live presence summary:
  - `96545`: `27` spawns on map `1116`
  - `14303`: `6` spawns on map `429`
- Candidate replacements considered:
  - `39125`
  - `27637`
- Notes:
  - This is a healing/non-caster support family.
  - There is not yet a direct same-map handoff for either live user.

### Family: Strike (`34920`)

- Status: `ready to fix`
- Bad spell ID:
  - `34920` (`Strike`)
- Affected live NPCs:
  - `18309` `Ethereal Scavenger`
  - `18315` `Ethereal Theurgist`
- Live presence summary:
  - `18309`: `7` spawns on map `557`
  - `18315`: `4` spawns on map `557`
- Reasoning:
  - `18315` already uses `15580` for the normal-difficulty `Strike` slot, making that a direct same-NPC handoff.
  - `18309` shares the same map `557` ethereal cluster and is a melee-adjacent kit (`Shield Bash`, `Singe`, `Strike`), so reusing the same live `Strike` variant is the cleanest local substitute.
- Decision:
  - `18315` `Ethereal Theurgist`: `34920 -> 15580`
  - `18309` `Ethereal Scavenger`: `34920 -> 15580`
- Notes:
  - `18994` `Infinite Executioner` also has `34920` on its SmartAI, but no live spawns were identified in this pass, so it is not driving the immediate fix set.

### Family: Shadow Word: Pain (`34942`)

- Status: `ready to fix`
- Bad spell ID:
  - `34942` (`Shadow Word: Pain`)
- Affected live NPCs:
  - `18331` `Ethereal Darkcaster`
- Live presence summary:
  - `18331`: `13` spawns on map `557`
- Candidate replacements considered:
  - `11639`
  - `17146`
  - `34941`
- Reasoning:
  - `34941` is the only live `Shadow Word: Pain` family currently tied to the same broader ethereal/Ethereum content cluster, via `21702` `Ethereum Life-Binder`.
  - `11639` is the dominant generic priest-style fallback in the repack, but it is less locally grounded for this specific Outland/TBC-era ethereal case.
  - `17146` appears on several caster-priest kits, but it is also less locally targeted than the small `34941` ethereal/Ethereum pattern.
- Decision:
  - `18331` `Ethereal Darkcaster`: `34942 -> 34941`
- Notes:
  - No same-NPC direct normal/heroic handoff is present for this spell.
  - This is a narrower readiness call than the broad `14032 -> 11639` family, and it is based on local thematic/cluster fit rather than raw frequency.

### Family: Immolate (`37668`)

- Status: `ready to fix`
- Bad spell ID:
  - `37668` (`Immolate`)
- Affected live NPCs:
  - `18312` `Ethereal Spellbinder`
  - `4306` `Scarlet Torturer`
  - `17938` `Coilfang Observer`
- Live presence summary:
  - `18312`: `13` spawns on map `557`
  - `4306`: `12` spawns on map `189`
  - `17938`: `5` spawns on map `547`
- Strongest current mappings:
  - `18312` `Ethereal Spellbinder`: `37668 -> 17883`
  - `17938` `Coilfang Observer`: `37668 -> 17883`
- Broader live fallback signal:
  - `11962` is the dominant live SmartAI `Immolate` variant in this repack, with broad usage across warlock-, darkweaver-, and summoner-style caster kits on maps `0`, `1`, `209`, `530`, and `571`.
  - `12742` and `15654` are live but materially narrower families than `11962`.
- Decision:
  - `18312` `Ethereal Spellbinder`: `37668 -> 17883`
  - `17938` `Coilfang Observer`: `37668 -> 17883`
  - `4306` `Scarlet Torturer`: `37668 -> 11962`
- Notes:
  - `18312` and `17938` already use `17883` for the normal-difficulty `Immolate` slot, making those direct same-family handoffs.
  - `4306` does not expose a same-map local successor, so `11962` is chosen as the strongest broad live `Immolate` fallback rather than as a dungeon-paired difficulty conversion.

### Family: Faerie Fire (`25602`)

- Status: `decision-needed`
- Bad spell ID:
  - `25602` (`Faerie Fire`)
- Affected live NPCs:
  - `15274` `Mana Wyrm`
  - `15966` `Mana Serpent`
  - `19306` `Mana Leech`
  - `16521` `Blood Elf Scout`
  - `15648` `Manawraith`
  - `16331` `Darnassian Druid`
- Live presence summary:
  - `15274`: `52` spawns on map `530`
  - `15966`: `25` spawns on map `530`
  - `19306`: `24` spawns on map `557`
  - `16521`: `23` spawns on map `530`
  - `15648`: `13` spawns on map `530`
  - `16331`: `9` spawns on map `530`
- Notes:
  - This is a large live non-caster / utility-debuff family.
  - The repack does have other live `Faerie Fire` IDs (`6950`, `16498`, `32129`), but they are not present as obvious same-map successors for the current live `25602` users.
  - At the moment this family behaves like a broken island: confirmed live and broken, but without a clear direct local handoff.

## Lower-Priority Dead Data

- `2052`
  - currently no spawned live SmartAI users identified
- `1243`
  - currently no spawned live SmartAI users identified

## Live Reference Families

These are active live spell families in the repack that are useful as replacement anchors or neighborhood context, but are not currently being tracked as broken targets in this audit.

### Shadow Bolt (`12471`)

- Live and heavily used across spawned warlock/shadow caster kits.
- Important confirmed live users include:
  - `8904` `Shadowforge Senator`
  - `5648` `Sandfury Shadowcaster`
  - `9261` `Firebrand Darkweaver`
  - `17771` `Murkblood Oracle`
  - `17694` `Shadowmoon Darkcaster`
  - `18640` `Cabal Warlock`
  - `22006` `Shadowlord Deathwail`
- This family serves as the strongest current handoff target for broken `15232` users where a direct normal/heroic mapping exists.

### Scorch (`36807`)

- Live active family.
- Confirmed spawned users:
  - `17771` `Murkblood Oracle`
  - `19168` `Sunseeker Astromage`
- This is the strongest current handoff target for `17771`'s broken `15241` `Scorch`.

### Strike (`15580`)

- Live active melee `Strike` family.
- Used by a broad set of spawned melee/NPC guard kits including:
  - `9096` `Rage Talon Dragonspawn`
  - `10317` `Blackhand Elite`
  - `11451` `Wildspawn Satyr`
  - `18315` `Ethereal Theurgist`
  - `14321` `Guard Fengus`
  - `14323` `Guard Slip'kik`
  - `14326` `Guard Mol'dar`
- This is the strongest current handoff target for `18315`'s broken `34920` and a candidate family for some `18368` `Strike` cases.

### Shadow Bolt (`20825`)

- Live active warlock/shadow-bolt family.
- Used by a wide range of spawned casters including:
  - `35452` `Burning Blade Warlock`
  - `7026` `Blackrock Sorcerer`
  - `1564` `Bloodsail Warlock`
  - `12378` `Damned Soul`
  - `19994` `Bloodmaul Warlock`
  - `19732` `Ango'rosh Warlock`
  - `22384` `Bloodmaul Soothsayer`
  - `32263` `Overseer Veraj`
- This is useful context for warlock/shadow-caster neighborhoods but is not currently itself a broken target.

### Curse of Weakness (`11980`)

- Live active debuff family.
- Important spawned users include:
  - `35452` `Burning Blade Warlock`
  - `1564` `Bloodsail Warlock`
  - `21065` `Tormented Citizen`
  - `8915` `Twilight's Hammer Ambassador`
  - `7372` `Deadwind Warlock`
  - `32263` `Overseer Veraj`
- This remains useful context alongside `13338` and `20825` for warlock-type kits.

### Dark Mending (`16588`)

- Live active healing family.
- Confirmed spawned users:
  - `19826` `Dark Conclave Shadowmancer`
  - `21200` `Screeching Spirit`
  - `17084` `Avruu`
  - `21384` `Dark Conclave Harbinger`
- This is a working live family, not a broken target.

### Mind Blast (`17287`)

- Live active priest/shadow-caster family.
- Confirmed spawned users:
  - `19633` `Bloodwarder Mender`
  - `11487` `Magister Kalendris`
- This is a working live family, not a broken target.

### Shadowform (`22917`) and Mind Flay (`22919`)

- Both are live working single-user reference families on `11487` `Magister Kalendris`.
- They help describe the surrounding shadow-priest style kit but are not broken targets.

### Ethereal Support Families

- `32316`
  - live working family on `18312` `Ethereal Spellbinder`
  - used for `Summon Ethereal Wraith`
- `37470`
  - live working family on `18312` `Ethereal Spellbinder`
  - used for `Counterspell`
- `34928`
  - live working normal-difficulty `Vampiric Aura` family on `18331` `Ethereal Darkcaster`
- `38060`
  - live working heroic-difficulty `Vampiric Aura` family on `18331` `Ethereal Darkcaster`
- `33865`
  - live working `Singe` family on `18309` `Ethereal Scavenger`
- `33871`
  - live working `Shield Bash` family on `18309` `Ethereal Scavenger`
- `32191`
  - live working `Heavy Dynamite` family used by `17938` `Coilfang Observer` and other spawned users
- `37666`
  - live working `Heavy Dynamite` family used by `17938` `Coilfang Observer` and other spawned users

### Shadow Bolt Volley Reference Families

- `28407`
  - live working `Shadow Bolt Volley` family on `16164` `Shade of Naxxramas` (Normal Dungeon)
- `55323`
  - live working `Shadow Bolt Volley` family on `16164` `Shade of Naxxramas` (Normal Dungeon)
- `56064`
  - live working `Shadow Bolt Volley` family on `15981` `Naxxramas Acolyte`
- `56065`
  - live working `Shadow Bolt Volley` family on `15981` `Naxxramas Acolyte`
- `59255`
  - live working heroic `Shadow Bolt Volley` family on `28368` `Ymirjar Necromancer`

### Mana Burn Reference Families

- `37159`
  - live working `Mana Burn` family on `20039` `Phoenix-Hawk`
- `37176`
  - live working `Mana Burn` family on `18883` `Mana Snapper`
- `48054`
  - live working `Mana Burn` family on `26737` `Crazed Mana-Surge` (Dungeon)
- `57047`
  - live working `Mana Burn` family on `26737` `Crazed Mana-Surge` (Dungeon)

### Other Low-Volume References

- `34941`
  - live working `Shadow Word: Pain` family on:
    - `17833` `Durnholde Warden`
    - `21702` `Ethereum Life-Binder`
- `34940`
  - live working `Gouge` family on `18314` `Nexus Stalker`

### Family: Shadow Bolt (`15232`)

- Status: `ready to fix`
- Bad spell ID:
  - `15232` (`Shadow Bolt`)
- Affected live NPCs:
  - `11448` `Gordok Warlock`
  - `10398` `Thuzadin Shadowcaster`
  - `17771` `Murkblood Oracle`
  - `22393` `Deathshadow Overlord`
  - `18640` `Cabal Warlock`
  - `9034` `Hate'rel`
  - `17694` `Shadowmoon Darkcaster`
  - `23022` `Gordunni Soulreaper`
  - `24762` `Sunblade Keeper`
- Live presence summary:
  - `11448`: `15` spawns on map `429`
  - `10398`: `14` spawns on map `329`
  - `17771`: `8` spawns on map `546`
  - `22393`: `5` spawns on map `530`
  - `18640`: `4` spawns on map `555`
  - `9034`: `3` spawns on map `230`
  - `17694`: `2` spawns on map `540`
  - `23022`: `1` spawn on map `530`
  - `24762`: `1` spawn on map `585`
- Strongest current per-NPC mappings:
  - `17771` `Murkblood Oracle`: `15232 -> 12471`
  - `17694` `Shadowmoon Darkcaster`: `15232 -> 12471`
  - `18640` `Cabal Warlock`: `15232 -> 12471`
- Broader live fallback signal:
  - `9613` is the dominant live SmartAI `Shadow Bolt` family in this repack by a wide margin, with heavy usage across classic shadowcasters, necromancers, warlocks, and Outland-era dark casters.
  - `12471` is a smaller but still well-established live `Shadow Bolt` family used by several caster NPCs, including direct normal-difficulty counterparts for some of the broken `15232` users.
  - `20825` and `12739` are both live alternatives, but they are materially narrower than `9613` and do not fit the remaining unresolved set as broadly.
- Decision:
  - `17771` `Murkblood Oracle`: `15232 -> 12471`
  - `17694` `Shadowmoon Darkcaster`: `15232 -> 12471`
  - `18640` `Cabal Warlock`: `15232 -> 12471`
  - `10398` `Thuzadin Shadowcaster`: `15232 -> 9613`
  - `11448` `Gordok Warlock`: `15232 -> 9613`
  - `22393` `Deathshadow Overlord`: `15232 -> 9613`
  - `23022` `Gordunni Soulreaper`: `15232 -> 9613`
  - `24762` `Sunblade Keeper`: `15232 -> 9613`
  - `9034` `Hate'rel`: `15232 -> 9613`
- Notes:
  - `17771`, `17694`, and `18640` already expose direct `12471` normal-difficulty counterparts on the same NPC.
  - `22393` has especially strong support for `9613` because the same map `530` cluster already uses `9613` on `22363` `Deathshadow Warlock` and many adjacent warlock/shadowcaster templates.
  - `10398`, `11448`, `23022`, `24762`, and `9034` do not expose same-NPC difficulty-paired successors, so they are promoted on broad live-repack fallback evidence rather than direct normal/heroic handoffs.

### Family: Scorch (`15241`)

- Status: `ready to fix`
- Bad spell ID:
  - `15241` (`Scorch`)
- Affected live NPCs:
  - `8910` `Blazing Fireguard`
  - `17269` `Bleeding Hollow Darkcaster`
  - `17771` `Murkblood Oracle`
- Live presence summary:
  - `8910`: `19` spawns on map `230`
  - `17269`: `13` spawns on map `543`
  - `17771`: `8` spawns on map `546`
- Strongest current mapping:
  - `17771` `Murkblood Oracle`: `15241 -> 36807`
- Additional local replacement signals:
  - `8910` `Blazing Fireguard` shares a compact fire-caster kit with `11466` `Highborne Summoner`, which already uses `12466` alongside the same `13341` `Fire Blast` family.
  - `17269` `Bleeding Hollow Darkcaster` sits next to an Outland fire-caster cluster where `17395` `Shadowmoon Summoner` uses `17290` as the live upgraded fire-cast slot.
- Decision:
  - `17771` `Murkblood Oracle`: `15241 -> 36807`
  - `8910` `Blazing Fireguard`: `15241 -> 12466`
  - `17269` `Bleeding Hollow Darkcaster`: `15241 -> 17290`
- Notes:
  - `17771` already uses `36807` as the heroic `Scorch` slot, making that a direct handoff.
  - `8910` and `17269` are promoted on local live-repack family similarity rather than same-NPC normal/heroic pairings.

### Family: Blast Wave (`15744`)

- Status: `classified`
- Bad spell ID:
  - `15744` (`Blast Wave`)
- Affected live NPCs:
  - `9693` `Bloodaxe Evoker`
  - `18558` `Phasing Sorcerer`
- Live presence summary:
  - `9693`: `7` spawns on map `229`
  - `18558`: `7` spawns on map `558`
- Strongest current mapping:
  - `9693` `Bloodaxe Evoker`: `15744 -> 15744` is itself the live family marker, but no broken replacement path is needed here beyond confirming that this effect family exists and is active in the repack.
- Notes:
  - This family appears in the live repack and is active, which helps validate `Blast Wave` replacement candidates elsewhere.
  - No new broken live mapping is required from this family itself.

### Family: Flamecrack (`15743`)

- Status: `classified`
- Bad spell ID:
  - `15743` (`Flamecrack`)
- Affected live NPCs:
  - `9693` `Bloodaxe Evoker`
- Live presence summary:
  - `9693`: `7` spawns on map `229`
- Notes:
  - Small live family with no immediate broader replacement implications beyond the local `Bloodaxe Evoker` kit.

### Family: Mana Burn (`22936`)

- Status: `defer until encountered in play`
- Bad spell ID:
  - `22936` (`Mana Burn`)
- Affected live NPCs:
  - `11480` `Arcane Aberration`
- Live presence summary:
  - `11480`: `10` spawns on map `429`
- Notes:
  - This is a special-case on-death / triggered `Mana Burn` family on a single live dungeon creature.
  - No direct handoff has been identified yet.
  - This family is currently deferred unless it starts surfacing in live gameplay reports.

### Family: Mana Burn (`22947`)

- Status: `defer until encountered in play`
- Bad spell ID:
  - `22947` (`Mana Burn`)
- Affected live NPCs:
  - `14398` `Eldreth Darter`
- Live presence summary:
  - `14398`: `7` spawns on map `429`
- Notes:
  - Small live `Mana Burn` family.
  - No direct same-map successor identified yet.
  - This family is currently deferred unless it starts surfacing in live gameplay reports.

### Family: Mana Burn (`26046`)

- Status: `defer until encountered in play`
- Bad spell ID:
  - `26046` (`Mana Burn`)
- Affected live NPCs:
  - `15247` `Qiraji Brainwasher`
- Live presence summary:
  - `15247`: `6` spawns on map `531`
- Notes:
  - Small live `Mana Burn` family.
  - No direct same-map successor identified yet.
  - This family is currently deferred unless it starts surfacing in live gameplay reports.

### Family: Mana Burn (`26049`)

- Status: `defer until encountered in play`
- Bad spell ID:
  - `26049` (`Mana Burn`)
- Affected live NPCs:
  - `15246` `Qiraji Mindslayer`
- Live presence summary:
  - `15246`: `4` spawns on map `531`
- Notes:
  - Small live `Mana Burn` family.
  - No direct same-map successor identified yet.
  - This family is currently deferred unless it starts surfacing in live gameplay reports.

### Family: Mana Burn (`28301`)

- Status: `defer until encountered in play`
- Bad spell ID:
  - `28301` (`Mana Burn`)
- Affected live NPCs:
  - `16020` `Mad Scientist`
- Live presence summary:
  - `16020`: `24` spawns on map `533`
- Notes:
  - Single-creature-family live `Mana Burn` case.
  - No direct replacement path identified yet.
  - This family is currently deferred unless it starts surfacing in live gameplay reports.

### Family: Arcane Explosion (`34933`)

- Status: `defer until encountered in play`
- Bad spell ID:
  - `34933` (`Arcane Explosion`)
- Affected live NPCs:
  - `19306` `Mana Leech`
  - `26727` `Mage Hunter Ascendant`
  - `27107` `Injured Warsong Mage`
- Live presence summary:
  - `19306`: `24` spawns on map `557`
  - `26727`: `8` spawns on map `576`
  - `27107`: `4` spawns on map `571`
- Notes:
  - This is a live `Arcane Explosion` family rather than another bolt/curse family.
  - `19306` uses it as an on-death effect, so this family likely needs to be evaluated separately from the normal combat-cast spell cleanup.
  - This family is currently deferred unless it starts surfacing in live gameplay reports.

## Other Confirmed Audit Findings

### SmartAI rows on non-SmartAI creatures

- Status: `open`
- Notes:
  - `49340` was moved to `ScriptName='npc_scarlet_corpse_recruitment'` by `sql/updates/world/master/2026_06_20_02_world.sql`
  - the live DB still has `2` `smart_scripts` rows for `49340`
  - this appears to be a stale DB cleanup issue that causes noisy but likely benign loader warnings

### Invalid persisted player/account spell data

- Status: `open`
- Notes:
  - spell `371571` exists in `characters.character_spell` for guids `1` and `2`
  - spell `371571` also exists in `auth.battlenet_account_mounts` for battlenet accounts `1` and `2`
  - logs show `Player::AddSpell: Spell (ID: 371571) does not exist`
