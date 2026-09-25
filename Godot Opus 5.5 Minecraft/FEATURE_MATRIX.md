# Feature Matrix

Status: `WORKING` = implemented and exercised by a test or a visual check; `PARTIAL` = present
but simplified or missing notable parts; `NOT IMPLEMENTED` = absent. "gameplay_check" refers to
`game/tests/gameplay_check.tscn` (a real session driven through ~120 checks), "test_runner" to
`game/tests/test_runner.tscn`, "shots" to the windowed `--autotest` screenshot scenarios.

## Core

| Feature | Status | Test notes |
|---|---|---|
| Project opens / all scripts compile | WORKING | `tools/lint.sh`: 126 scripts, 0 failures; registries build (941 blocks, 1348 items, 90 mobs) |
| Chunked voxel world, streaming, threaded gen/mesh/light | WORKING | gameplay_check: 81+ chunks lit in ~2.5 s; no per-block nodes (RenderingServer section meshes) |
| Player physics, no falling through terrain | WORKING | gameplay_check "player lands on terrain" |
| Smooth lighting, AO, day/night light curve | WORKING | shots (morning daylight fixed to the Java curve) |
| Save / load (level.json + per-dimension chunk files) | WORKING | gameplay_check: position, game mode, inventory, placed blocks, time, dragon state, live mobs restored |
| Autosave, save on quit | WORKING | 2-minute autosave; WorldScene saves on exit |
| Performance safeguards | WORKING | worker threads, upload budget, mob caps, light-edit bursts no longer starve chunk lighting |
| Entity render interpolation (20 TPS ticks drawn on every frame) | WORKING | `motion_check`: bodies drawn between their last two tick positions, drawn path length equals the real one (no back-and-forth sweeps), heading blended, smoothing keeps mobs from zig-zagging |
| Windows export build | WORKING | `GodotMinecraft.exe` (embedded PCK) exported with the official 4.7.2 templates; launched and verified |

## Game modes and player

| Feature | Status | Test notes |
|---|---|---|
| Default launch in Creative | WORKING | gameplay_check "default game mode is Creative" |
| Creative flight (double-tap space), instant break, infinite blocks | WORKING | gameplay_check (flight holds altitude, obsidian breaks instantly, stack not consumed) |
| Creative inventory: 12 tabs, search, scroll, tooltips, destroy slot | WORKING | gameplay_check (E opens it, search, all 12 tabs populated, every item in a tab), shots |
| All items obtainable | WORKING | all 1346 creative items in tabs (written book / filled map via crafting or /give, as in the original) |
| All mobs spawnable incl. bosses | WORKING | gameplay_check: all 90 mobs spawn; /summon wither, /summon ender_dragon; 93 spawn eggs |
| Hostile mobs ignore Creative players | WORKING | gameplay_check |
| Survival: health, hunger, saturation, exhaustion, regeneration, starvation | WORKING | gameplay_check (damage, hunger drain, eating bread) |
| Survival: timed mining, tool tiers, durability, drops collected | WORKING | gameplay_check (stone ~15 ticks with wooden pickaxe, iron ore needs stone pickaxe) |
| Death, inventory drop, respawn | WORKING | gameplay_check (a death-handler bug was found and fixed) |
| Spectator mode, F4 mode cycling, difficulty | WORKING | F4 / admin panel / /gamemode /difficulty |
| Third-person views, player model with worn armour | WORKING | shots (Blender player model and armour pieces) |
| Elytra gliding, firework boost | WORKING | gameplay_check (jump while falling opens wings, glides) |
| Status effects (speed, strength, regen, poison, wither, absorption, health boost…) | PARTIAL | periodic effects and HUD hearts work; blindness/nausea/darkness/invisibility have no visual/AI effect |

## Blocks, crafting, items

| Feature | Status | Test notes |
|---|---|---|
| Block registry with original pixel-art textures | WORKING | 941 blocks, procedural atlas; shots |
| Placement rules (stairs, slabs, doors, torches, rails, redstone…) | WORKING | used throughout the tests |
| Crafting 2x2 / 3x3 + recipe book | WORKING | gameplay_check / test_runner recipes; recipe book UI |
| Smelting: furnace, blast furnace (fixed), smoker, campfire | WORKING | test_runner iron smelting; blast furnace kind bug fixed |
| Stonecutter, smithing (netherite upgrade), anvil, grindstone | PARTIAL | work; no armour trims, no anvil prior-work penalty / "too expensive" |
| Tools, weapons, armour, shields, maces, tridents, bows, crossbows | WORKING | melee, bow draw/release, trident sticks and returns with Loyalty |
| Enchanting table + effects | PARTIAL | table/lapis/bookshelves work; sharpness/efficiency/protection/fortune/silk touch/unbreaking/looting/fire aspect/knockback/power/infinity/mending/thorns/loyalty/channeling apply; depth strider, frost walker, soul speed, punch, multishot, piercing, impaling, binding have no effect |
| Brewing and potions (drink, splash) | PARTIAL | all standard recipes; lingering potions behave like splash (no cloud) |
| Fishing | WORKING | gameplay_check (cast into water, reel in on a bite) |
| Name tags, leads, compass, clock, spyglass, chorus fruit, XP bottles, eggs, snowballs, wind charges | WORKING | gameplay_check for name tag / lead / compass / chorus fruit |
| Maps, bundles, books & quills, item frames, paintings, armour stands, signs | NOT IMPLEMENTED | items exist; decorative placeables drop as items |

## World generation

| Feature | Status | Test notes |
|---|---|---|
| Overworld terrain, biomes (41 surface + cave biomes), rivers, oceans | WORKING | test_runner terrain per dimension; shots |
| Caves (cheese/spaghetti/noodle), ravines, ores, trees per biome | WORKING | shots |
| Sulfur Caves (26.2 biome: bands, spikes, pools, geysers, sulfur cubes) | PARTIAL | biome and geysers work; sulfur cube AI is minimal (no block absorption) |
| Aquifers, dripstone behaviour, sculk spreading | NOT IMPLEMENTED | caves are dry |
| Structures: village, mineshaft, stronghold (portal room), ruined portal | WORKING | test_runner: every type builds and is locatable |
| Desert/jungle temple, witch hut, igloo, outpost, mansion, monument, shipwreck, buried treasure, ocean ruin, ancient city | WORKING | test_runner (simplified hand-coded layouts with loot chests) |
| Trail ruins, trial chambers, dungeons, desert wells, fossils | NOT IMPLEMENTED | |
| Loot tables (chests, mob drops, looting) | WORKING | chest loot rolled on first open; blaze rods drop (gameplay_check) |

## Mobs

| Feature | Status | Test notes |
|---|---|---|
| 90 mobs with Blender-authored articulated models | WORKING | mob_check: 90/90 from GLB rigs; shots |
| Priority set: pig, cow, sheep, chicken, horse, wolf, iron golem, zombie, skeleton, creeper, spider, slime, witch, drowned, Nether and End mobs | WORKING | gameplay_check (zombies attack, skeleton arrows hit, creeper swells and explodes, blazes, piglin family, shulkers) |
| Villagers with professions and trading UI | PARTIAL | 13 professions + wandering trader, fixed emerald trades with daily restock; no levels, no workstation claiming, no gossip |
| Natural spawning (light, biome, caps, despawn) | WORKING | hostile archetypes now correctly target players (creepers/spiders/slimes/ghasts/blazes were passive before) |
| Pathfinding, panic, enderman stare/teleport, undead burning in daylight, fall damage | WORKING | gameplay_check (zombies burn at noon); paths are straightened with the mob's footprint (motion_check) |
| Taming, pets follow/sit/defend | WORKING | gameplay_check (tamed wolf follows, sits) |
| Breeding and baby growth | WORKING | gameplay_check (two fed cows make a calf, cooldown) |
| Raids, piglin bartering, sniffer digging, axolotl play-dead | NOT IMPLEMENTED | |

## Dimensions and progression

| Feature | Status | Test notes |
|---|---|---|
| Nether portal: build, ignite, travel, return, 1:8 mapping, linked portals | WORKING | gameplay_check round trip, coordinates, inventory preserved |
| Nether: 5 biomes, lava sea, fortress, bastion, blazes, piglins, hoglins, ghasts | WORKING | gameplay_check; shots |
| Dimension state persists | WORKING | gameplay_check (block placed in the Nether still there on return) |
| Eyes of ender, stronghold locating, End portal activation | WORKING | gameplay_check (eye thrown, 12 filled frames open the portal) |
| The End: island, obsidian spikes, crystals, dragon fight, exit portal, egg | WORKING | gameplay_check (crystals heal, crystals destroyed, dragon defeated, portal opens); shots |
| End gateways to outer islands, return gateway | WORKING | gameplay_check (lands on an outer island >600 blocks out) |
| End cities, shulkers, elytra | WORKING | test_runner / gameplay_check (end city locatable, shulkers spawn, elytra wearable & glides) |
| Wither: summoning, phases, skull attacks | WORKING | gameplay_check (Wither attacks with skulls) |
| Boss state across reloads | PARTIAL | defeat is saved; a dragon fight interrupted by a reload restarts at full health |

## Systems

| Feature | Status | Test notes |
|---|---|---|
| Redstone: dust, torches, repeaters, comparators, pistons (push entities), observers, lamps, doors, levers, buttons, pressure plates, daylight detectors, target blocks, hoppers, droppers, dispensers | PARTIAL | core logic faithful; plates/daylight/target now trigger; repeater locking, slime-block piston contraptions, detector rails, tripwire, sculk sensors not implemented |
| TNT and explosions | WORKING | gameplay_check (terrain destroyed, entities damaged, creeper explosion); particles, shake, sound |
| Fluids: flow, sources, lava/water interactions, buckets, swimming/drowning | PARTIAL | works; no waterlogging for regular blocks, no bubble columns, no currents pushing entities |
| Farming: crops, stems, cane, cactus, bamboo, berries, farmland, bone meal, composter | PARTIAL | works; cocoa, torchflower/pitcher crops, farmland trampling missing |
| Day/night, sun/moon/stars sky | WORKING | shots |
| Weather: rain/snow visuals, thunder, lightning | WORKING | gameplay_check (lightning converts pigs); rain/snow respect roofs and cold biomes |
| Beds: sleep, skip night, spawn point, explode in Nether/End | WORKING | |
| Vehicles: boats (paddling, floating), minecarts on rails (curves, slopes, powered rails), TNT minecart | WORKING | gameplay_check (boat mount/paddle/dismount/break, L-track curve followed) |
| Riding: horses (taming by riding, saddles), donkeys, mules, camels, pigs/striders with sticks | WORKING | gameplay_check (saddled horse steered) |
| XP orbs, levels | WORKING | |
| Commands: gamemode, give, summon, time, weather, tp, locate, kill, difficulty, effect, gamerule, setblock, fill, clear, xp, enchant, spawnpoint, seed, say, heal, feed, dimension | WORKING | test_runner command checks; used by the autotests |
| F7 admin panel | WORKING | game mode, time, weather, difficulty, flight, re-mesh |

## UI and presentation

| Feature | Status | Test notes |
|---|---|---|
| Title screen, world list, create world (name, seed, mode, difficulty) | WORKING | shots |
| HUD: crosshair + attack cooldown, hotbar, offhand, hearts (poison/wither/absorption), hunger, armour, air, XP, effects, boss bars, chat | WORKING | gameplay_check HUD/hotbar; shots |
| Inventory, containers, furnace, brewing, enchanting, anvil, grindstone, stonecutter, smithing, beacon, trading screens | WORKING | gameplay_check (tooltips, E screens) |
| Pause menu pauses the game; options; game rules; death screen | WORKING | gameplay_check (pause) |
| Original pixel font and UI sprites (no Godot default styling) | WORKING | shots |
| Audio: synthesized block sounds per material, footsteps, mobs, UI, explosions, rain, music | PARTIAL | everything synthesized at runtime (no audio files); no cave/biome ambience, mob idle chatter limited |

## Asset completion gate

| Check | Status | Notes |
|---|---|---|
| High-priority mobs are not primitive placeholders | WORKING | articulated, textured Blender models for all 90 mobs (mob_check 90/90) |
| Held tools are not flat 2D sprites | WORKING | 55 Blender-extruded pixel models for tools/weapons in first person |
| Visible blocks have pixel-art textures | WORKING | procedural 16x16 original atlas |
| Creative inventory has usable icons | WORKING | block models and item sprites for every entry |
| Nether and End have dimension-specific materials/assets | WORKING | netherrack/nylium/basalt/blackstone/soul soil, end stone/obsidian spikes/purpur; per-dimension fog and sky |
| Ender Dragon has a recognizable articulated model | WORKING | Blender rig with neck, head, jaw, spiked back, tail, legs and two-segment membrane wings (rebuilt and inspected in Blender this session) |
| Wither has a recognizable model | WORKING | three heads, spine and ribs (Blender GLB) |
| TNT has recognizable visuals and VFX | WORKING | textured primed TNT flashing on its fuse, explosion particles, camera shake |
| Portals have animated/distinct materials | WORKING | animated Nether portal texture with particles; End portal/gateway starfield |
| HUD is not default Godot UI styling | WORKING | custom pixel font, sprites and panels |
