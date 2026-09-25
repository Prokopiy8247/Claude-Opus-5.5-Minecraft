# Feature matrix

Status legend: **WORKING** = exercised and observed in the running game (mostly through the automated validation
tour, `-Opus55Tour`, final run after the Opus 5.5 rename + `-Opus55LoadCheck` + `-Opus55TitleShot`; key screenshots and both reports are copied to
`Docs/Screenshots/`); **PARTIAL** =
implemented but incomplete, simplified, or not verified in play this session; **NOT IMPLEMENTED** = absent.
A feature is not marked WORKING only because code for it exists.

## Core / engine

| Feature | Status | Test notes |
|---|---|---|
| Project builds (UE 5.8.2, editor + runtime modules) | WORKING | Editor and standalone Game targets build (`Tools/build.sh Editor|Game`); content commandlet rebuild: 0 errors |
| Standalone packaged game (Shipping, `Unreal_Minecraft.exe` in the project folder) | WORKING | UAT BuildCookRun: 1,292 packages cooked, 0 errors; started from the project folder it shows the title menu and creates a world; the full tour passes on the packaged build (38/38, average 180 fps) |
| Content pipeline (materials, map, 731 FBX meshes via commandlet) | WORKING | `Tools/content/build_content.py` rebuilds 12 materials, `L_Opus55World`, imports all Blender FBX |
| World loads, chunk streaming, render distance 12 | WORKING | Tour: 841 chunks loaded, pending meshes drains to 0, ~110–140 fps at 1280×720 on the development PC |
| Voxel meshing, smooth light, AO, 7 material layers | WORKING | Terrain, water, lava, portal and end-portal layers seen in the tour |
| Sky light / block light propagation | WORKING | Caves dark, torch/glowstone/lantern/sea-lantern/campfire light in the night scene |
| Player collision (no falling through terrain) | WORKING | Tour stands, walks into portal, lands in the Nether; no fall-through observed |
| Save / load | WORKING | Tour saves 844 files; `-Opus55LoadCheck` reload restores dimension (Nether), exact position, Creative mode and held stack |
| Performance | WORKING | Final tour average 119 fps (109–142 over the last runs); off-screen mob animation budget, rig parts excluded from ray tracing/DF |
| No console-error spam | WORKING | Game logs show no gameplay errors during tours (only engine/editor warnings from running via the editor binary) |

## Creative

| Feature | Status | Test notes |
|---|---|---|
| Default mode is Creative | WORKING | New/auto-started worlds start in Creative (`mode 1` in the log, load check `mode=1`) |
| Flight (double-tap Space) | PARTIAL | Implemented; tour enables flight programmatically, the double-tap itself was not automated |
| Instant break, unlimited placement, pick block | PARTIAL | Implemented; not exercised by the automated tour |
| Creative inventory: tabs, icons, survival-inventory tab, destroy slot | WORKING | Screenshot 17 (Java-style tabs, 1261 item icons) |
| Creative search | PARTIAL | Search tab and filter implemented; typing not automated |
| Any block/item obtainable | WORKING | 898 blocks / 1261 items in the catalogue and via `/give` |
| Any mob spawnable (`/summon`, spawn eggs) | WORKING | Tour summons 36 mob types in six line-ups (hostile, passive, new, Nether, large, small); 89 types registered |
| Bosses test-spawnable | PARTIAL | Ender Dragon spawns with the End (boss bar seen); `/summon wither` supported but not captured this session |
| Game-mode switcher F3+F4 | WORKING | Cycles Creative → Survival → Adventure → Spectator through the `/gamemode` path (that path is exercised by the tour) |

## Survival

| Feature | Status | Test notes |
|---|---|---|
| Creative catalogue replaced by survival inventory | WORKING | Screenshot 18 (2×2 crafting, armour, offhand) |
| Survival HUD (hearts, hunger, armour, air, XP, hotbar) | WORKING | Screenshot 19, Minecraft GUI-scale layout |
| Mining times, tool tiers, durability, drops | PARTIAL | Implemented (hardness × tool table, tier gating, durability); not exercised in an automated survival session |
| Hunger / saturation / regeneration / starvation | PARTIAL | Implemented; not verified in a long play session |
| Death and respawn | PARTIAL | Death screen and respawn implemented; not automated |
| Crafting (870 crafting, 186 smelting, 194 stonecutting, 30 brewing, 10 smithing) | PARTIAL | Recipes registered and station screens open; recipe execution not automated this session |
| Mobs attack the survival player | PARTIAL | Hostile AI implemented; tour line-ups use NoAI mobs |

## Portals and dimensions

| Feature | Status | Test notes |
|---|---|---|
| Build and ignite a Nether portal | WORKING | Tour 27b: obsidian frame + fire → 6/6 animated portal blocks |
| Travel Overworld → Nether through the portal | WORKING | Tour 36: player enters portal, arrives in the Nether |
| Coordinates map sensibly (÷8) | WORKING | Portal at (−222, −326) → arrival (−21.5, −51.5) near (−27.8, −40.8) (linked/created portal) |
| Inventory preserved across dimensions | WORKING | Held stack `grass_block x64` before and after travel |
| Return Nether → Overworld, portal persists | WORKING | Tour 35: back in the Overworld, test portal still lit 6/6 |
| Dimension state persists across save/load | WORKING | Reload puts the player back in the Nether |
| End portal (frames + eyes of ender, activation, travel both ways) | WORKING | Tour 37b/38/39: 12 frames on flat ground + eyes -> 3x3 portal at frame height; stepping onto an edge block -> End; the exit portal -> Overworld. Fixed this session: activation needed an empty layer under the ring, travel needed the exact centre block, and portals were ignored inside the End (no way home). Stronghold portal rooms generate but were not walked |
| Only the player changes dimension | PARTIAL | Other entities are stopped at portals (by design in this build) |

## Overworld

| Feature | Status | Test notes |
|---|---|---|
| Terrain generation (climate noise, 55 Overworld biomes incl. 26.2 Sulfur Caves) | WORKING | Plains/forest/village terrain, aerial view, Lush and Sulfur caves in screenshots |
| Caves, aquifers, ores, springs | WORKING | Cave scenes; aquifer barriers stop springs from flooding caves |
| Trees and vegetation (sway) | WORKING | Forest and flowers in screenshots |
| Villages | WORKING | Tour 28 (houses, farm, well, library, church, lamps) |
| Other structures (19 types: stronghold, mineshaft, temples, monument, mansion, trial chambers, ancient city, outposts, shipwrecks, ruins...) | PARTIAL | Generated and `/locate`-able; only villages were visually checked this session; simplified layouts |
| Day/night cycle, sun, moon phases, stars | WORKING | Night scene with stars and moonlight |
| Weather (rain, thunder) | WORKING | Rain scene (tour 27) |
| Fluids (water/lava flow, sources, infinite water rule) | WORKING | Minecraft source rule; lava ocean in the Nether |
| Farming (crops, growth, bonemeal, farmland) | PARTIAL | Implemented; not exercised by the tour |
| Redstone (power, wires, torches, repeaters, comparators, pistons, lamps, TNT) | PARTIAL | Power → TNT priming verified (tour 27a); other components not exercised |

## Nether

| Feature | Status | Test notes |
|---|---|---|
| Nether terrain and biomes (wastes, crimson, warped, soul sand valley, basalt deltas) | WORKING | Screens 31/32: netherrack, lava ocean, glowstone, own material language |
| Lava | WORKING | Lava ocean with emissive/animated material |
| Nether mobs (piglin, hoglin, ghast, blaze, wither skeleton, zombified piglin, magma cube, strider...) | WORKING | Natural spawns in the reload census (piglin, blaze, wither skeleton, hoglin, ghast, magma cube) |
| Fortress / bastion | PARTIAL | Generated (blazes spawn); not visually inspected this session |
| Blaze rod obtainable | PARTIAL | Blaze loot table implemented; kill not automated |

## End

| Feature | Status | Test notes |
|---|---|---|
| End dimension, main island, obsidian pillars, crystal cages | WORKING | Screens 33/34 (own material language, 10 end crystals) |
| Ender Dragon exists (model, flight, boss bar) | WORKING | "dragon present" in the tour, boss bar visible |
| Dragon can be defeated, crystals heal it, exit portal activates, egg | PARTIAL | Fight manager, crystal healing, death sequence and exit portal implemented; full fight not automated |
| Outer islands and gateways | PARTIAL | Outer island generation and gateway spawning implemented; not travelled this session |
| End cities, shulkers in cities, elytra loot | NOT IMPLEMENTED | Shulker mob and elytra item exist, but End cities are not generated |
| Elytra flight | PARTIAL | Player elytra flight state and pose exist; not tested |

## Combat, explosions, bosses

| Feature | Status | Test notes |
|---|---|---|
| Melee combat (cooldown, crits, knockback) | PARTIAL | Implemented; not automated |
| Bow / crossbow / trident / spear / mace | PARTIAL | Projectiles and weapon logic implemented; held models authored in Blender; firing not automated |
| TNT damages terrain and entities | WORKING | Tour 27a: primed by redstone, detonated, crater 93/147 blocks, drops |
| Creeper explosion | PARTIAL | Uses the same explosion code as TNT; creeper swell/explode not automated |
| Wither (summon, boss bar, skull attacks) | PARTIAL | Boss implemented with authored model; fight not automated |
| Status effects | WORKING | Fire resistance and effects applied in the tour (`/effect`, API) |

## Items, stations, progression

| Feature | Status | Test notes |
|---|---|---|
| Inventory model (hotbar, main, armour, offhand, stacking, drag distribution) | WORKING | Inventory screens and hotbar in screenshots |
| Tooltips | PARTIAL | Implemented; not captured in the tour |
| Crafting table, furnace, chest, enchanting table, brewing stand, anvil screens | WORKING | Screenshots 20–25 open and render correctly |
| Other stations (blast furnace, smoker, grindstone, stonecutter, smithing, loom, cartography, beacon, hopper, dispenser, crafter, shulker box, villager trading) | PARTIAL | Menus implemented; not captured this session |
| Enchanting / XP / brewing logic | PARTIAL | Implemented; not exercised end-to-end |
| Tool / armour tiers (wood, stone, copper, iron, gold, diamond, netherite) | PARTIAL | Item stats and tiered held models; armour has no worn visual |
| Transportation (boats, minecarts, rails, horses) | PARTIAL | Boat and minecart Blender models spawn and render; riding not automated |

## UI, audio, VFX

| Feature | Status | Test notes |
|---|---|---|
| Title screen and starting a world from it | WORKING | `-Opus55TitleShot`: title menu captured (Docs/Screenshots/00_title_screen), a world created from the title switches to the HUD with game input (00b_world_from_title); fixed this session: the menu was missing because the UI root waited for a player |
| Options and pause menu (Save and Quit to Title) | PARTIAL | Implemented; not captured by the automated tour |
| HUD styling (Minecraft GUI scale, pixel-accurate hotbar/status layout) | WORKING | Screenshots 16/19 |
| Chat and commands | WORKING | Tour drives the game through 20+ commands; chat shown bottom-left |
| Debug overlay (F3) | WORKING | Screenshot 04 |
| Cursor handling (no lock traps) | PARTIAL | Input mode switches per screen (UI vs game); only exercised through programmatic screen changes in the tour |
| Procedural audio (blocks, mobs, UI, ambience) | WORKING | Clips synthesised on demand (log); original, not Mojang sounds |
| Particles (block break, explosion, portal, flames, weather) | WORKING | Explosion debris/smoke and portal particles in screenshots |
| First-person held items (authored 3D tools, extruded sprites, block items) | WORKING | Screenshots 15/16 |
| Third-person camera (F5) | WORKING | Screenshots 13/14 |

## Asset completion gate

| Check | Status | Test notes |
|---|---|---|
| Important mobs are not placeholder capsules/primitives | WORKING | All 89 rigs use Blender-authored part meshes (731 meshes); line-up screenshots 06–11 |
| Held tools are not flat 2D placeholders | WORKING | 12 Blender props (pickaxe, axe, shovel, hoe, sword, spear, mace, trident, bow, crossbow, shield, fishing rod) with tier materials; other items are per-pixel extruded 3D sprites |
| Important interactive blocks have authored visuals | PARTIAL | Chests, furnaces, stations, torches, lanterns, campfires, beds... are detailed voxel box models with procedural textures, not Blender meshes |
| Terrain material families are visually distinct | WORKING | 657 procedural PBR layers (albedo/normal/roughness-AO-metal-emissive) |
| Nether has its own material language | WORKING | Netherrack, nylium, basalt, soul sand, glowstone, lava ocean; red fog |
| End has its own material language | WORKING | End stone, obsidian pillars, purple void sky, end crystals |
| Portals have strong animated visuals | WORKING | Animated swirl Nether portal (screenshot 27b), parallax star-field End portal/gateway |
| Ender Dragon is a convincing detailed boss model | PARTIAL | 23-part Blender dragon (head, jaw, glowing eyes, 5 neck and 8 tail segments, wings, legs) animated in flight; blocky style, not a high-detail sculpt |
| Wither is a convincing boss model | PARTIAL | Three-headed Blender model (body, 3 heads, tail); not visually re-checked in the final tour |
| TNT has convincing visuals/VFX | WORKING | Primed TNT entity (white flash, swell), explosion particles, crater (screenshot 27a) |
| HUD and inventory are styled, not raw default widgets | WORKING | Minecraft-style bevelled panels, slots, glyphs, GUI scale |
| No default checker materials in shipping gameplay | WORKING | None observed in 38 tour screenshots |
| Armour visuals on the player/mobs | NOT IMPLEMENTED | Armour works as items/stats only |
