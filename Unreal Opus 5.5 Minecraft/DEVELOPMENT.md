# Opus 5.5 Minecraft Unreal: development guide

A single-player Unreal Engine 5 reinterpretation of Minecraft Java Edition (26.2 era), built by Claude Opus 5.5 as a
benchmark project. All code, textures, meshes, sounds and UI art are original: textures are synthesised procedurally at
runtime, hero meshes are modelled in Blender through the `blender_unreal` MCP bridge, and sounds are synthesised.
No Mojang code, assets, sounds, fonts or logos are used.

## Engine and project

| Item | Value |
|---|---|
| Engine | Unreal Engine **5.8.x**, `EngineAssociation: 5.8` |
| Project file | `Unreal_Minecraft.uproject` (the root of this project folder) |
| Modules | `Unreal_Minecraft` (Runtime, all gameplay), `Unreal_MinecraftEditor` (Editor, content build library) |
| Startup map | `/Game/Opus55Minecraft/Maps/L_Opus55World` (game and editor) |
| Game mode / instance | `AMCGameMode` (`/Script/Unreal_Minecraft.MCGameMode`), `UMCGameInstance` |
| Rendering | Lumen GI + reflections (hardware RT), virtual shadow maps, histogram auto-exposure (EV100 range), custom voxel vertex factory |
| Blender source file | `UnrealMinecraft.blend` (collections for mobs, entities and item props) |

## How the project bootstraps

1. `L_Opus55World` loads with `AMCGameMode`. `AMCGameMode::StartPlay` spawns `AMCGame`, the world manager.
2. `AMCGame::BeginPlay` initialises the registries (blocks, items, recipes, mobs, biomes, loot), procedural audio,
   texture synthesis (657 texture-array layers, cached after the first run), item icons and the voxel renderer materials.
3. Without arguments the Slate title screen appears (Singleplayer → world list / Create New World, Options, Quit).
   New worlds default to **Creative**. `-Opus55AutoStart` or `-Opus55Tour` skip the title screen and start/load a world directly.
4. `FMCWorld` per dimension (Overworld, Nether, End) owns chunks (16×16 columns, Y −64..320), lighting, block ticks,
   fluids, block entities and entities. Chunk generation and meshing run on worker threads; the game thread applies results.
5. `UMCVoxelRenderer` turns chunk sections into `UMCChunkMeshComponent`s (custom scene proxy, 7 material layers:
   opaque, cutout, translucent, water, lava, portal, end portal).
6. Mobs, the player body, vehicles and held/dropped items use `UMCRigComponent` / `UMCItemVisualComponent`, which load the
   Blender-authored static meshes (`/Game/Opus55Minecraft/Mobs|Entities|Items`) and animate them part by part.

## Architecture (Source/Unreal_Minecraft)

| Folder | Contents |
|---|---|
| `Core/` | Constants, coordinates, hashing, deterministic RNG, noise (Perlin/simplex/fBm) |
| `Blocks/` | Block registry (898 blocks, 5097 states), static block models (3318), texture definitions, block behaviours (placement, plants, fluids, redstone, functional blocks, copper ageing, TNT...) |
| `World/` | Chunks, palette storage, sky/block light propagation, block entities (chests, furnaces, hoppers, signs, beds...), entity management, region save files |
| `Gen/` | 65 biomes; Overworld generator (55 biomes, climate noise, caves incl. Lush/Dripstone/Deep Dark/Sulfur, aquifers, ores, features, trees), 19 structure types (villages, strongholds, mineshafts, temples, monuments, mansions, trial chambers, ancient cities...), Nether generator (5 biomes, fortress, bastion), End generator (main island, obsidian spikes, outer islands, gateways) |
| `Items/` | 1261 items, tool tiers, food, armour stats, enchantments, loot tables |
| `Crafting/` | 870 crafting, 186 smelting, 194 stonecutting, 30 brewing, 10 smithing recipes; recipe matching |
| `Game/` | `AMCGame` (world manager, dimensions, portals, commands, environment/day-night/weather, spawning, save/load), `AMCPlayer` (movement, flight, mining, interaction, survival stats), entities (items, XP orbs, projectiles, TNT, falling blocks, vehicles, end crystals), `AMCMob` + AI (89 mob types), bosses (Ender Dragon, Wither), menus/containers, the validation tour |
| `Render/` | Texture synthesis (procedural PBR albedo/normal/ORME arrays), mesher (greedy-free per-face mesher with smooth light and AO), chunk mesh component/proxy, particles, item icons and sprites, rigs |
| `UI/` | Slate UI: title/world/options screens, HUD (hotbar, hearts, hunger, armour, air, XP, boss bars), creative inventory with tabs and search, survival inventory, station screens (crafting, furnace family, chest, enchanting, brewing, anvil, grindstone, stonecutter, loom, smithing...), chat, debug overlay |
| `Audio/` | Procedural sound synthesis (blocks, steps, mobs, UI, ambience) with variant caching |

`Source/Unreal_MinecraftEditor/MCEditorLibrary.cpp` builds the materials (HLSL custom nodes), the map and imports the FBX
meshes; it is driven by `Tools/content/build_content.py`.

## Content pipeline

1. **Blender (hero assets)** via the `blender_unreal` MCP server on port 9878, file `UnrealMinecraft.blend`:
   - `Tools/BlenderMCP/build_assets.py`: every mob rig part, the player arm, boat, minecart, item frame, end crystal
     (vertex-coloured, pivot-centred parts, exported to `Saved/Opus55Fbx/<Group>/SM_*.fbx`).
   - `Tools/BlenderMCP/build_items.py`: 12 held-item props (pickaxe, axe, shovel, hoe, sword, spear, mace, trident, bow,
     crossbow, shield, fishing rod) lofted from profiles, with a `MI_Metal` slot for tier materials.
   - `Tools/BlenderMCP/render_sheet.py`: preview contact sheets (`Saved/Opus55Preview/`).
   - Run a script: `uv run --with mcp python Tools/BlenderMCP/blender_unreal_mcp.py execute_blender_code --code-file <script>.py`
2. **Unreal import/build** (commandlet, ~1 minute):
   ```
   "<UE_ROOT>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "<project>/Unreal_Minecraft.uproject" -run=pythonscript -script="<project>/Tools/content/build_content.py" -unattended -nop4 -nosplash
   ```
   Rebuilds the 12 materials, the map, imports all FBX (731 static meshes) and prints a verification list.
3. **Runtime-generated content**: block textures, item sprites, UI glyphs, sounds and particles are generated at startup
   (textures cached in `Saved/Opus55Cache/`).

## Build and run

- **Recommended:** run `Launch-Windows.bat` on Windows or `Launch-macOS.command` on macOS. The launcher finds Unreal
  Engine 5.8, builds the C++ editor target and opens the project. See `README.md` for prerequisites and troubleshooting.
- **Manual editor launch:** double-click `Unreal_Minecraft.uproject`, allow Unreal to build the missing modules, then
  press Play (or Standalone Game).
- **Package a standalone build:** in Unreal Editor use **Platforms -> Windows/macOS -> Package Project**. Assets loaded
  by path at runtime are included through `DirectoriesToAlwaysCook` in `Config/DefaultGame.ini`.
- Run the game directly:
  ```
  "<UE_ROOT>/Engine/Binaries/Win64/UnrealEditor.exe" "<project>/Unreal_Minecraft.uproject" -game -windowed -ResX=1600 -ResY=900
  ```
- Useful arguments: `-Opus55AutoStart` (skip the title screen), `-Opus55World=<name>`, `-Opus55Seed=<n>`,
  `-Opus55Tour` (automated validation tour, see below), `-Opus55TourStay` (keep running after the tour),
  `-Opus55LoadCheck` (with `-Opus55Tour`: report the reloaded state of an existing world),
  `-Opus55TitleShot` (screenshot the title screen, create a world from it, screenshot the game, quit).

### Automated validation tour

```
UnrealEditor-Cmd.exe "<project>/Unreal_Minecraft.uproject" -game -Opus55Tour -Opus55World=Tour -Opus55Seed=12345 -windowed -ResX=1280 -ResY=720
```
Stages 38 scenes (terrain, mob line-ups, first/third person, all main screens, night lighting, rain, TNT, Nether portal
ignition, village, Sulfur/Lush caves, Nether, End with the dragon, portal round trip), writes screenshots and per-scene
render statistics to `Saved/Opus55Tour/`, saves the world and quits. Re-run with `-Opus55LoadCheck` to verify reloading.

## Controls (Minecraft Java defaults)

| Input | Action |
|---|---|
| Mouse | Look (vertical FOV 70 by default, invert option) |
| W A S D | Move |
| Space | Jump; double-tap toggles Creative flight; ascend while flying |
| Left Shift | Sneak; descend while flying |
| Left Ctrl | Sprint |
| Left mouse | Attack / mine (instant in Creative) |
| Right mouse | Use / place / eat / interact / open containers |
| Middle mouse | Pick block (Creative) |
| 1–9, mouse wheel | Hotbar slot |
| E | Inventory (Creative catalogue in Creative) |
| Q | Drop item (Ctrl+Q drops the stack) |
| F | Swap main hand / offhand |
| T | Chat |
| / | Command line |
| Tab | Player list |
| Esc | Pause menu (options, save & quit to title) |
| F1 | Hide HUD |
| F2 | Screenshot |
| F3 | Debug overlay |
| F3 + F4 | Cycle game mode Creative → Survival → Adventure → Spectator |
| F3 + G | Debug frame charts |
| F5 | Camera: first person → third person back → third person front |
| F11 | Fullscreen |

Inventory screens: left click pick/place, right click split/place one, shift-click quick move, number keys swap with
the hotbar, Q over a slot drops.

Commands: `gamemode`, `defaultgamemode`, `difficulty`, `give`, `clear`, `summon` (with `{NoAI:1b,Silent:1b}`),
`kill`, `tp`/`teleport`, `setblock`, `fill` (replace/destroy/hollow/outline/keep), `time`, `weather`, `gamerule`,
`effect`, `enchant`, `xp`/`experience`, `locate`, `particle`, `seed`, `spawnpoint`, `setworldspawn`, `say`, `help`.

## Save location

`<Project>/Saved/Opus55Worlds/<World name>/`:
- `level.dat`: seed, time, weather, game rules, dragon-fight state, active dimension;
- `player.dat`: position, dimension, game mode, inventory, health/hunger/XP, effects, spawn point;
- `DIM0/`, `DIM1/`, `DIM2/` (Overworld, Nether, End): one `c.<x>.<z>.mcc` file per chunk with blocks, light,
  block entities and entities.

Worlds autosave every 5 minutes and on "Save and Quit to Title" / exit. Options (sensitivity, FOV, GUI scale, render
and simulation distance, volumes...) are stored in `Saved/Opus55Options.json`; the texture cache in `Saved/Opus55Cache/`.

## Major systems (summary)

Voxel engine with smooth lighting and AO; three dimensions with portals; Creative (catalogue, flight, instant break,
any item/mob/boss) and Survival (mining times, tool tiers and durability, drops, hunger/saturation, health, air, XP,
death/respawn); 870+ recipes and all main station screens; 89 mob types with AI, spawning and Blender-authored models;
Ender Dragon and Wither bosses; fluids; redstone components; TNT and explosions; farming; enchanting and brewing;
boats and minecarts; day/night, weather and lighting; procedural audio; save/load. The honest state of each is in
`FEATURE_MATRIX.md`.

## Known limitations

- Texture art is procedural: each block family is recognisable but it is not pixel-identical to Minecraft.
- Most blocks with special shapes (chests, torches, lanterns, campfires, beds...) are voxel box models with procedural
  textures, not Blender meshes; armour has no worn 3D visual.
- Only the player travels between dimensions (other entities are stopped at portals).
- End cities (and therefore naturally generated elytra/shulker loot) are not generated.
- After a material/shader change, the first launch compiles shaders; when a compile finishes UE recreates the affected
  chunk proxies and the renderer re-meshes them, so terrain can blink out for a few frames once.
- Right after a large `/time set` jump the view was observed washed out (terrain lost in haze, mobs dark) for about
  two seconds while sky capture, Lumen and auto-exposure re-converge; it recovers by itself (the tour now sets the
  clock during its warm-up).
- Many systems are implemented but were only exercised through the automated tour screenshots, not through long manual
  play sessions; see `FEATURE_MATRIX.md` for what was actually verified.
