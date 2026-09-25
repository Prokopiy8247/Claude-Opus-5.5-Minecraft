# Claude Opus 5.5 Final Report — Godot Minecraft

## Overview

A single-player, first-person recreation of Minecraft Java 26.2 built from scratch in
**Godot 4.7.2 (GDScript, Forward+)** inside the existing project folder. Everything is original:
code, procedural pixel-art textures, synthesized audio, and 3D assets authored in **Blender via
the `blender_godot` MCP connection** (91 mob rigs, 55 held tools/weapons, 31 props incl. boats,
minecarts, chest, armour pieces, End crystal) and imported as GLB. No Mojang/Microsoft code,
textures, sounds, fonts or logos were used.

Session start: **2026-09-23 16:07:07 +0200** — end: **2026-09-25 05:09:39 +0200**.

## Major implemented features

- **Voxel engine**: 16×16 chunk columns, threaded generation/meshing, dedicated light thread
  (sky + block light, smooth lighting, AO), RenderingServer section meshes, texture-array shaders,
  Java daylight curve, per-dimension sky/fog.
- **Game modes**: Creative by default (flight, instant break, infinite blocks, 12-tab creative
  catalogue with search/scroll/tooltips/destroy slot — all 1346 creative items and 93 spawn eggs),
  Survival (health, hunger, saturation, timed mining with tool tiers, durability, XP, death drops,
  respawn), Spectator; F4 cycling; Peaceful–Hard.
- **World**: 41 surface biomes + cave biomes incl. **Sulfur Caves** (bands, spikes, pools,
  geysers), caves/ravines/ores/trees, 20 structure types with loot (village, mineshaft,
  stronghold with portal room, ruined portals, temples, witch hut, igloo, outposts, mansion,
  monument, shipwreck, treasure, ocean ruins, ancient city, fortress, bastion, End city).
- **Nether** (5 biomes, lava sea, fortress, bastion, portal travel 1:8 with linked portals) and
  **End** (island, obsidian spikes with crystals, Ender Dragon fight with healing beams and
  perching, exit portal + egg, gateways to outer islands with return gateways, End cities,
  shulkers, elytra). **Wither** with phases and skulls.
- **Mobs**: 90 mobs with archetype AI, pathfinding, natural spawning, loot, breeding and baby
  growth, taming (pets follow/sit/defend), riding (horses with saddles, donkeys, mules, camels,
  pigs/striders with sticks), villager professions with a trading screen, enderman stare/teleport,
  undead burning in daylight.
- **Systems**: crafting with recipe book, furnace/blast furnace/smoker/campfire, stonecutter,
  smithing, anvil, grindstone, enchanting table, brewing stand, beacon; redstone (dust, torches,
  repeaters, comparators, pistons, observers, lamps, doors, plates, buttons, levers, hoppers,
  droppers, dispensers, rails); TNT/explosions with VFX; fluids; fire; farming; boats and
  minecarts on rails (curves, slopes, powered rails); fishing; elytra; weather with rain/snow and
  lightning; beds; 23 commands and an F7 admin panel; save/load of all dimensions.
- **UI/Audio**: original pixel font and HUD, all container/station screens, pause/options/game
  rules/death screens, chat/console; runtime-synthesized block, step, mob, UI and ambient sounds
  and generative music.

## Validation / build result

| Check | Result |
|---|---|
| `tools/lint.sh` (all scripts compiled with autoloads) | **126 scripts, 0 failures**; registries 941 blocks, 1348 items, 90 mobs |
| `game/tests/test_runner.tscn` | **38 passed, 0 failed** |
| `game/tests/gameplay_check.tscn` (real session through the testing checklist) | **114 passed, 0 failed** |
| `game/tests/mob_check.tscn` | **90/90 mobs built from Blender GLB rigs** |
| `game/tests/travel_check.tscn` | **PASS** (Nether and End travel, dragon + 10 crystals) |
| `game/tests/motion_check.tscn` | **16 passed, 0 failed** (mob/projectile/vehicle render interpolation, path straightening) |
| Windowed screenshot autotests (world, mobs, armour, Nether, End, dragon, UI) | visually checked this session |
| Windows export | **Produced**: `GodotMinecraft.exe` in the project folder (single file, PCK embedded, ~112 MB), built with the official Godot 4.7.2 export templates from the "Windows Desktop" preset in `export_presets.cfg`; the exported build was launched (title screen and world checked visually). |

The gameplay test found and led to fixes for several real bugs this session: player death never
ran the death handler (no drops/death screen), creepers/spiders/slimes/ghasts/blazes never
targeted players, projectiles passed through the player, the daylight curve was a quarter-day
off, light-edit bursts starved chunk lighting, blast furnaces never smelted, entities near the
player were lost on save, End geometry/lighting and inactive dimensions rendering over the active
one, and the Ender Dragon's wings (rebuilt in Blender and re-inspected). Checking the exported
build found two more: the title-screen backdrop was drawn over the menu (dark screen, no
buttons), and the pixel font drew `.` `,` `:` `;` with a stray top pixel; both fixed and the exe
rebuilt.

A reported bug — every mob rapidly moving back and forth — was reproduced with a new test
(`motion_check`: models travelled 86x the distance the mobs actually walked). Cause: `Mob.tick()`
overrides `Entity.tick()` and never stored `prev_pos`, so each frame drew the mob at
`lerp(spawn point, current position, alpha)` and it swept between the two 20 times a second.
Related defects fixed with it: `prev_facing` was stored after the AI had turned the mob (turns
never blended); projectiles stored `prev_pos` only every 20 ticks (arrows swung, stuck arrows
jittered); `Entity.frame()` overwrote the real heading of boats and minecarts every frame; mob
bodies snapped up to 180 degrees in one tick on the 4-connected block paths (paths are now
straightened with the mob's footprint and the drawn body turns over a few ticks); path head
clearance was truncated (`int(1.95)` = 1 block for zombies, 0 = no obstacle check for chickens);
chasing mobs never re-planned once a path was used up; and no world ever stopped its light thread
and worker jobs, so leaving a world leaked its chunks and quitting spammed thousands of renderer
errors.

## Known bugs / limitations

- Villager trading uses fixed offers per profession (no levels, workstations or gossip); no raids,
  piglin bartering or sniffer/axolotl special behaviours.
- Some enchantments have no effect (frost walker, depth strider, soul speed, punch, multishot,
  piercing, impaling, curse of binding); lingering potions act like splash potions; blindness,
  nausea, darkness and invisibility have no visual/AI effect.
- Missing items/blocks behaviour: maps, bundles, books & quills, signs, item frames, paintings,
  armour stands (placeables drop as items), loom, cartography table, lectern, sculk sensors,
  tripwire, detector rails, repeater locking, slime-block contraptions.
- No waterlogging for regular blocks, bubble columns, water currents, aquifers; no trial
  chambers, trail ruins, dungeons.
- A dragon fight interrupted by reloading the save restarts at full health (defeat is saved).
- A rare worker-thread race during very large block edits can log an out-of-bounds error in the
  light engine/mesher; reads are now bounds-checked and the section re-meshes on the next edit.

## Controls

| Key | Action |
|---|---|
| W A S D / Mouse | move / look |
| Space | jump; double-tap: toggle flight (Creative/Spectator); while falling with an elytra: glide |
| Left Shift | sneak / descend; dismount boats, carts and animals |
| Left Ctrl | sprint |
| Left mouse | attack / break (hold in Survival) |
| Right mouse | use / place / interact (doors, chests, beds, trade, mount, saddle, fish…) |
| Middle mouse | pick block |
| Wheel, 1–9 | hotbar |
| E | inventory (creative catalogue in Creative) |
| Q / Ctrl+Q | drop item / stack |
| F | swap offhand |
| T, / | chat, command console |
| Esc | pause (pauses the game) / close screen |
| F1 / F2 / F3 | hide HUD / screenshot / debug overlay |
| F4 | cycle game mode |
| F5 | camera perspective |
| F7 | admin panel |
| F11 | fullscreen |

Launch: double-click `GodotMinecraft.exe` in the project folder (Singleplayer → Create New World),
or open the project in Godot 4.7.2 and press F5.

## Size of the work

- Files added/changed versus the empty baseline commit: **1244 files, +67,910 / −62 lines**
  (includes generated GLBs, textures and import metadata).
- GDScript: **126 files, ~42,100 lines**; shaders ~450 lines; Python/shell tooling (Blender
  bridge, build scripts) ~3,700 lines.
- Generated assets: 177 GLB models (91 mobs, 55 items, 31 props), procedural block atlas,
  `GodotMinecraft.blend` with all Blender collections.

## Claude Opus 5.5 Session Metrics

```text
## Claude Opus 5.5 Session Metrics

Start: 2026-09-23 16:07:07 +0200
End: 2026-09-25 05:09:39 +0200 (project completed)
Active work time (without pauses): 10:52:51 (652.9 minutes)
Idle pauses (waiting for the user to resume the session): 28:27:52
Wall-clock span: 39:20:43

Model: Claude Opus 5.5 (claude-opus-5-5, 1M context) in Claude Code
Reasoning effort: max

API requests: 979 (main session + 4 subagents)
Input tokens (total): 382,780,462
  Cache reads: 348,857,046
  Cache writes: 33,921,480 (all 5-minute TTL)
  Uncached input: 1,936
Output tokens: 2,168,175 (of which reasoning: 938,888)
Total tokens: 384,948,637

Claude Opus 5.5 API-equivalent cost: $282.75
  Cache reads    348.86M x $0.20/M = $69.77
  Cache writes    33.92M x $5.00/M = $169.61
  Output           2.17M x $20.00/M = $43.36
  Uncached input   0.002M x $4.00/M = $0.01
Rates: Claude Opus 5.5 $4 input / $20 output / $0.20 cache read / $5 5-minute cache write per
million tokens (the cache-write rate is derived from the standard 1.25x multiplier); no separate
long-context surcharge. Token-only figure; tool fees: none.

Data source: per-request usage records in the Claude Code session transcripts, de-duplicated by
message id (`python .opus55-run/session_metrics.py`). Active time counts model generation and
running tools/tests/Blender; gaps after a finished turn until the next user message are pauses.
Not included: auxiliary requests the transcript does not log individually (context-compaction
summaries, session-title generation), so the true figure is slightly higher.
```
