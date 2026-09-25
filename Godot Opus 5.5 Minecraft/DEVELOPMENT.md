# Godot Minecraft — Development Notes

A single-player, first-person block sandbox modelled on Minecraft Java 26.2, written from scratch
in GDScript for Godot. All code, textures (procedural pixel art), models (Blender, driven by
script) and sounds (synthesized at runtime) are original; no Mojang/Microsoft code or assets are
used or shipped.

## Engine and tools

| | |
|---|---|
| Engine | Godot **4.7.2.stable.official** (ed1daf0bf), GDScript only |
| Renderer | Forward+ (D3D12/Vulkan on Windows) |
| Main scene | `res://game/main.tscn` (title screen) |
| Autoloads | `Game` (`game/core/game.gd`), `Sfx` (`game/audio/sfx.gd`) |
| Editor plugin | `addons/opus55_tools` — Tools menu: *Rebuild Generated Assets*, *Validate Registries* |
| Blender | 5.2 through the `blender_godot` MCP addon socket (port 9877); master file `GodotMinecraft.blend` |

## Running

1. Open the folder containing `project.godot` in Godot 4.7.2 and press **F5** (or run
   `Godot_v4.7.2-stable_win64.exe --path .`).
2. Title screen → **Singleplayer** → **Create New World** (Creative by default; the Game Mode and
   Difficulty rows are clickable) → **Create**. Existing worlds are listed with Play / Delete.
3. Quick start without the menus: `godot --path . -- --quickstart [--mode=survival] [--seed=N]`.
4. Or just run the exported build `GodotMinecraft.exe` in the project folder.

### Building the Windows executable

Requires the Godot 4.7.2 export templates (Editor → Manage Export Templates, or extract the
Windows templates into `%APPDATA%\Godot\export_templates\4.7.2.stable\`). Then:

```
Godot_v4.7.2-stable_win64_console.exe --headless --path . --export-release "Windows Desktop" GodotMinecraft.exe
```

The "Windows Desktop" preset (`export_presets.cfg`) embeds the PCK into the executable, includes
the atlas index (`*.json`) and leaves out the test scenes. The build is git-ignored.

The first launch generates the block texture atlas and item sprites procedurally (cached under
`game/generated/textures`), which takes a few seconds.

## Controls

| Key | Action |
|---|---|
| W A S D | move |
| Mouse | look |
| Space | jump; **double-tap** toggles flight in Creative/Spectator; while falling with an elytra: glide |
| Left Shift | sneak (in flight: descend); **dismount** boats, carts and animals |
| Left Ctrl | sprint (in flight: faster) |
| Left mouse | attack / break blocks (hold in Survival) |
| Right mouse | use item / place block / interact (doors, chests, beds, mount boats and animals…) |
| Middle mouse | pick block |
| Mouse wheel, 1–9 | select hotbar slot |
| E | inventory (Creative: creative catalogue) |
| Q / Ctrl+Q | drop one item / drop the stack |
| F | swap main hand / offhand |
| T | chat |
| / | command console (with leading slash) |
| Esc | pause menu (pauses the game) / close screens |
| F1 | hide HUD |
| F2 | screenshot (`user://screenshots`) |
| F3 | debug overlay (Shift+F3: print seed/position to chat) |
| F4 | cycle game mode Survival → Creative → Spectator |
| F5 | camera: first person → third person back → third person front |
| F7 | admin panel (game mode, time, weather, difficulty, flight, re-mesh) |
| F11 | fullscreen |

Inventory screens use Minecraft click rules: left/right click, shift-click quick move,
double-click collect, number keys swap with hotbar, drag to distribute, destroy slot in Creative.

Commands (`/help` lists them): `gamemode, give, summon, time, weather, tp, locate, kill,
difficulty, effect, gamerule, setblock, fill, clear, xp, enchant, spawnpoint, seed, say, heal,
feed, dimension`.

## Save location

`user://saves/<world folder>/` — on Windows
`%APPDATA%\Godot\app_userdata\Godot Minecraft\saves\<world>\`:

- `level.json` — seed, time, weather, game rules, difficulty, player (position, dimension, game
  mode, inventory, stats, effects, spawn point), ender chest, known portals, dragon state and a
  block-id palette (ids are remapped on load, so saves survive registry changes). Written
  atomically (temp file + rename).
- `dim0/`, `dim1/`, `dim2/` — Overworld, Nether, End: one `c.<cx>.<cz>.bin` per modified chunk
  (ZSTD-compressed block array, block entities such as chest contents, biomes, entities).

The game autosaves every 2 minutes, on *Save* in the pause menu and when leaving the world.
Settings are stored in `user://settings.cfg`.

## Architecture

```
game/
  core/        Game autoload (registries, settings, world start), Vox (block value packing), ModelLibrary
  blocks/      BlockDB + block_catalog (941 blocks), placement rules, shapes, interaction, behaviours
  items/       ItemDB + item_catalog (1348 items), item use, enchantments, potion effects, icons
  crafting/    RecipeDB: shaped/shapeless (tags, mirroring), smelting, stonecutting, smithing
  world/       World (one per dimension), ChunkManager (streaming, worker threads), LightEngine
               (light thread), Mesher, shaders, fluids, fire, particles, weather FX, block entities
  world/gen/   OverworldGen (multi-noise terrain, caves, ores, trees, Sulfur Caves), NetherGen, EndGen
  biomes/      BiomeDB (41 surface + cave biomes, colours, spawn lists)
  structures/  StructureRegistry/Manager/Layout + StructureGen builders (20 structure types)
  entities/    Entity base + VoxelBody physics, EntityManager (spawning, saving), items/XP/TNT/
               falling blocks/projectiles/vehicles, End crystals, eyes of ender, Riding, Rails, Breeding
  mobs/        MobDB (90 mobs), Mob, MobRenderer (Blender GLB rigs), ModelSpecs, MobAnimator, textures
  ai/          MobAI archetypes (melee, ranged, creeper, spider, slime, flyer, ghast, blaze, enderman,
               golems, shulker, warden, creaking, dragon, wither…) + A* Pathfinder
  player/      Player, PlayerMotion (Java movement constants), PlayerInteraction, PlayerStats,
               ElytraFlight, HandView (first-person hand/items), PlayerSkin, ArmorRenderer
  combat/      Explosions (Java ray model, resistance, exposure, knockback)
  redstone/    RedstoneSystem (dust networks, torches, repeaters, comparators, pistons, observers,
               lamps, doors, rails, plates, buttons, levers…), Dispensers
  portals/     Nether portal frames (2x3..21x21), End portal ring, destination search/build
  bosses/      DragonFight (crystals, podium, exit portal, gateways)
  inventory/   Inventory (46 player slots: hotbar, main, armour, offhand, 2x2 craft)
  loot/        LootDB (mob drops with looting, chest tables for every structure)
  ui/          UIRoot (screens, pause/options/gamerules/death/console/admin), Hud, PixelUI (original
               pixel font + sprites), Commands, screens/ (creative, inventory, crafting, containers,
               furnace, brewing, enchanting, anvil, grindstone, stonecutter, smithing, beacon)
  audio/       Sfx autoload + SoundSynth (all sounds synthesized), MusicGen
  save_system/ SaveManager
  main/        TitleScreen, WorldScene (environment, sky, fog, weather audio)
  editor/      asset exporters (Blender source data, textures), project setup helpers
  generated/   textures (block atlas), blender_src (inputs for Blender), models (GLB outputs)
  tests/       headless test scenes (see below)
tools/
  gd.sh, lint.sh             Godot CLI wrappers (import, run, compile check)
  blender_bridge/            MCP socket client + build scripts for all Blender assets
```

### Bootstrap

`Game` (autoload) parses user command-line arguments, loads settings, registers the shader
globals and, on first use, builds every registry (`Game.init_registries()`: blocks → biomes →
texture atlas → mesher tables → models → items → mobs → spawn eggs → recipes → loot → effects →
enchantments → behaviours → redstone). `main.tscn` shows the `TitleScreen`; picking a world calls
`Game.start_world(params)`, which switches to `world_scene.tscn`. `WorldScene` builds the sky
shader environment and a `WorldSession`, which owns the worlds (one `World` node per dimension,
created on demand), the player, the `EntityManager`, particles, HUD and UI, and runs the 20 TPS
simulation (`simulate()`) with render interpolation.

### Voxel engine

- Chunks are 16×16 columns (Overworld y −64..319, Nether 0..127, End 0..255) stored as flat
  `PackedInt32Array`s; a block value is `id | meta << 12`.
- `ChunkManager` streams chunks around the player (render distance 2–32): generation and meshing
  run as `WorkerThreadPool` tasks, lighting on a dedicated `LightEngine` thread (sky + block light
  flood fill, incremental updates on edits); finished section meshes are uploaded as
  `RenderingServer` instances (no per-block nodes) within a per-frame time budget.
- `Pathfinder` runs A* over the voxel grid with the mob's head clearance and then straightens the
  path (`_smooth`): waypoints are merged while the mob's footprint stays on walkable ground, which
  is what stops mobs from zig-zagging along 4-connected block paths.
- `Mesher` builds three surfaces per 16³ section (opaque, cutout, translucent) with face culling,
  Minecraft-style smooth lighting and ambient occlusion, biome tints and ~40 special block models
  (stairs, slabs, fences, panes, rails, redstone, plants, fluids, doors…).
- Voxel shaders (`game/world/shaders/`) read a `Texture2DArray` atlas with animated layers and
  apply the light map (daylight curve, dimension ambient, brightness setting, night vision).
- Block edits re-mesh only the touched sections (plus neighbours on borders).

### Worlds, dimensions and travel

Entities other than the player are simulated at 20 TPS and drawn by `Entity.frame(alpha)` between
the position from the start of the last tick (`prev_pos`, `prev_body_yaw`) and the current one.
`tick()` therefore never writes render state — `frame()` is the only place the visual is touched.
`Mob` keeps a separate smoothed `body_yaw` for the drawn heading (an attack still uses `facing`);
`MobAnimator` subtracts the drawn yaw, not `facing`, for the head look.

`WorldSession.travel_to()` streams the destination around the target (up to 20 s, travel overlay
shown), then moves the player and entities. Inactive dimensions are hidden and frozen. Nether
portals map coordinates 1:8 and link to existing portals or build a new one; the End is entered
through a stronghold portal and left through the exit portal that opens when the dragon dies;
End gateways jump to the outer islands (and build a return gateway there).

### Blender asset pipeline

All important 3D assets are authored in Blender by script through the `blender_godot` MCP socket
and imported as GLB:

1. **Export source data from Godot** (model specs, mob texture atlases, held-item sprites, prop
   tiles): `godot --headless --path . res://game/editor/blender_export.tscn` →
   `game/generated/blender_src/` (`.gdignore`d).
2. **Build in Blender** (Blender running with the addon listening on port 9877):
   `cd tools/blender_bridge && python build_assets.py all` (or `mobs|items|props [names…]`).
   Each asset is created in its own collection (`Mobs/…`, `Items/…`, `Props/…`), saved into
   `GodotMinecraft.blend` and exported as GLB to `game/generated/models/{mobs,items,entities}/`.
   Scripts are checked with the MCP server's Safe Mode validator before execution.
3. **Import**: `bash tools/gd.sh import` (or focus the editor). The game loads the GLBs through
   `ModelLibrary` / `MobRenderer`: 91 mob rigs (named part holders are animated procedurally),
   55 held tools/weapons (extruded pixel models), 31 props (10 boats + 10 chest boats, 5
   minecarts, chest with hinged lid, 4 armour pieces, End crystal base). The procedural builders
   remain as fallbacks if a GLB is missing.

### Tests and validation

| Command | What it checks |
|---|---|
| `bash tools/lint.sh` | every script compiles with the autoloads loaded; registries build |
| `bash tools/gd.sh run res://game/tests/test_runner.tscn` | structures build and are locatable in all dimensions, terrain per dimension, 1:8 mapping, recipes, smelting, brewing, spawn eggs, save round trip (38 checks) |
| `godot --headless --path . res://game/tests/gameplay_check.tscn` | a real session driven through the gameplay checklist: creative, survival (timed mining, drops, tools, food, mobs, death/respawn), combat and explosions, UI, Nether portal round trip, Nether content, End portal, dragon fight, gateways, vehicles, riding, breeding, elytra, weather, save/load |
| `bash tools/gd.sh run res://game/tests/mob_check.tscn` | all 90 mobs build from their Blender GLB rigs |
| `godot --headless --path . res://game/tests/motion_check.tscn` | entity render interpolation: drawn positions and heading between ticks, no render-state feedback, straightened paths (16 checks) |
| `bash tools/gd.sh run res://game/tests/travel_check.tscn` | Nether and End travel, End arrival and dragon/crystals |
| `godot --path . -- --autotest=<walk|menu|mobs|blocks|armor|nether|flight> --shots=res://shots` | windowed screenshot scenarios |

## Major systems (summary)

- **Game modes**: Creative (default; flight, instant break, infinite blocks, creative catalogue
  with 12 tabs + search + destroy slot, all 1348 items and 90 spawn eggs), Survival (health,
  hunger/saturation/exhaustion, timed mining with tool tiers, durability, XP, death drops and
  respawn), Spectator; difficulties Peaceful–Hard.
- **Crafting**: 2x2/3x3 grids with the recipe book, furnace/blast furnace/smoker/campfire,
  stonecutter, smithing (netherite upgrade), anvil, grindstone, enchanting table (bookshelves),
  brewing stand (all standard potions, splash/lingering), beacon.
- **World**: biomes incl. Sulfur Caves (banded walls, sulfur spikes, pools, geysers), caves,
  ores, trees, structures with loot chests, fluids (flow, sources, obsidian/cobblestone/basalt),
  fire spread, day/night with the Java daylight curve, rain/snow/thunder with lightning, beds.
- **Mobs**: 90 mobs with spawning rules, pathfinding, loot, taming (pets follow/sit/defend), breeding, riding, villager trading,
  bosses (Ender Dragon with crystals and perching, Wither with phases).
- **Redstone**: dust, torches, repeaters, comparators, pistons, observers, lamps, doors, plates,
  buttons, levers, hoppers, dispensers/droppers, rails, TNT, note blocks.
- **Vehicles**: boats (paddling, floating, ice), minecarts on rails (curves, slopes, powered
  rails, TNT minecarts on activator rails), rideable horses/donkeys/mules/camels/pigs/striders.

## Known limitations

See `FEATURE_MATRIX.md` for the per-feature status. The main gaps: villager trading is a fixed
per-profession offer list (no levels), no raids or piglin bartering; several enchantments (frost
walker, depth strider, punch, multishot…) and items (maps, bundles, books, signs, item frames)
have no effect yet; no waterlogging for regular blocks, no bubble columns; detector rails, tripwire,
sculk sensors and some decorative functional blocks (loom, cartography table, lectern) are inert;
the End dragon fight resets to full health when a save is reloaded mid-fight; a rare
worker-thread race can log an out-of-bounds error during very large block edits (the affected
section re-meshes on the next edit).
