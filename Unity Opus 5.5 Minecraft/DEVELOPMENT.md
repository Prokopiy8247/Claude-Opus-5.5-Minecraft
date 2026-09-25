# DEVELOPMENT.md

How the MCR ("Minecraft Recreation") Unity project is built, run, tested and extended.
Everything in this document was read off the project as it exists on disk. Run the commands from the
Unity project root (`Unity Opus 5.5 Minecraft`).

---

## 1. Toolchain

| Piece | Version / path |
|---|---|
| Unity Editor | **6000.6.0f1** (revision `f7f8ed4d1e24`) — `ProjectSettings/ProjectVersion.txt` |
| Editor executable | `C:/Program Files/Unity/Hub/Editor/6000.6.0f1/Editor/Unity.exe` |
| Render pipeline | URP **17.6.0** (`com.unity.render-pipelines.universal`) |
| Input | Unity **Input System 1.20.0** (`activeInputHandler: 1`, "Input System Package only") |
| Other packages of note | test-framework 1.8.0, ugui 2.6.0, ai.navigation 2.0.14, visualscripting 1.9.12, timeline 6.6.0 |
| Product | company `Opus 5.5`, product `Minecraft Recreation`, version 0.1.0 |
| Assemblies | `Assets/_Game/Scripts/MCR.Runtime.asmdef`, `Assets/_Game/Editor/MCR.Editor.asmdef` |
| Namespace | `MCR` (editor code: `MCR.EditorTools`) |

The project deliberately uses **no** UGUI widgets, TextMeshPro, prefab wiring or manual Inspector setup.
`Tools > Opus 5.5 Minecraft > Configure Render Pipeline` strips the URP template's SSAO and decal renderer
features and disables HDR, MSAA and shadows, because the game draws its own sky, fog and lighting.

## 2. Folder layout under `Assets/_Game`

```
Assets/_Game/
  Scripts/            113 C# files, ~50.9k lines (MCR.Runtime.asmdef)
    Core/             math, noise, RNG, Int3/Dir/AABB helpers
    Blocks/           block registry, block types, functional blocks, block entities
    Items/            item registry, item types, procedural item sprites
    Inventory/        IContainer/Slot/Menu model, every workstation menu, villager trading
    Crafting/         recipe registry (crafting / smelting / stonecutting / smithing)
    Gen/              overworld generator, decorators, tree features, biomes
    Dimensions/       Nether + End generators (NetherGenerator.cs holds both)
    Structures/       one file per structure family (village, stronghold, nether, end, ...)
    World/            chunk storage, streaming, meshing, lighting, job system, entities
    Entities/         Entity base, Player, projectiles, vehicles, simple entities
    Mobs/             mob registry/AI, per-category mobs, bosses, mob models
    Combat/           damage, effects, enchantments, attributes
    Redstone/         wire/torch/repeater/comparator/piston machinery and block behaviours
    Rendering/        chunk/entity/UI shaders bridge, skin painter, particles, sky, model renderer
    Textures/         procedural pixel-art generation (~4.7k lines) for every block/item/particle
    Audio/            procedural sound synthesis (no audio files)
    UI/               HUD, screen stack, creative/survival inventory, container widgets, font, atlases
    SaveSystem/       (empty) — saving lives in Game/SaveManager.cs and World/World.cs
    Game/             bootstrap, GameManager (input/settings/tick), commands, achievements,
                      save/load, portals, explosions, alchemy, loot, AutoTest harness
    Debug/            (empty) — the debug overlay is UI/Hud.cs, F3 combos are Game/GameInput.cs
    Player/           (empty) — the player is Entities/Player.cs
  Editor/             BatchTools.cs + ModelImporter.cs (MCR.Editor.asmdef, 2 files / ~680 lines)
  Shaders/            MCRChunk, MCREntity, MCRUI, MCRUIComposite, MCRUnlit + MCRCommon.hlsl
  Data/Models/        105 ModelDef JSON files + _index.json (the Blender pipeline's input)
  Generated/          Models/*.fbx + Models/Textures/*.png + Prefabs/ + Resources/Models/ (mesh+mat)
                      + Resources/Generated/BlockTextures.asset (baked 1552-layer texture array)
  Scenes/Main.unity   the only scene: a Bootstrap component and a camera; everything else is code
  Tests/              (empty — no NUnit tests are written yet)
```

The game object graph is created at runtime: `Bootstrap.AutoCreate()` (a
`[RuntimeInitializeOnLoadMethod(AfterSceneLoad)]`) adds a `GameManager` + `AutoTest` GameObject, so the
project runs from any scene, including an empty one. `GameManager.Awake` initialises settings, the HUD,
biomes, blocks, items, recipes, textures and sounds, then pushes `TitleScreen`.

## 3. Fast compile check (no editor launch)

```bash
python Tools/fastcompile.py runtime 20      # MCR.Runtime  — every .cs under Assets/_Game/Scripts
python Tools/fastcompile.py editor 20       # MCR.Editor   — Assets/_Game/Editor, links Tools/_out/MCR.Runtime.dll
```

It reuses the reference list from Unity's last generated
`Library/Bee/artifacts/1900b0aE.dag/Assembly-CSharp*.rsp` and invokes the Roslyn `csc.dll` bundled with
the editor (`Editor/Data/DotNetSdk`), taking ~10-20 s. Extra flags:
`--extra DIR` (compile staging folders outside `Assets`), `--exclude FILE`, `--only SUBSTR` (filter the
error list). Output is `MCR.Runtime: N errors (M shown), W warnings`; exit code is non-zero on errors.

Two gotchas worth knowing:

* Without `--extra`/`--exclude` it writes **`Tools/_out/MCR.Runtime.dll`** — the same path the Unity
  editor's own `Assembly-CSharp.rsp` references. Prefer
  `python Tools/fastcompile.py runtime 20 --exclude Tools/_none_.cs`, which redirects the output to a
  private DLL and deletes it afterwards, so a compile check can never disturb the editor.
* If it reports `error CS0006: Metadata file '.../MCR.Runtime.ref.dll' could not be found`, Unity is
  open and currently regenerating `Library/Bee`; wait for it to finish and rerun (see §5).

## 4. Batch builds (Windows and macOS players)

```text
<UNITY_EDITOR> -batchmode -quit -projectPath "<PROJECT_PATH>" \
  -executeMethod MCR.EditorTools.BatchTools.BuildWindows -logFile "<PROJECT_PATH>/Tools/_out/build-windows.log"

<UNITY_EDITOR> -batchmode -quit -projectPath "<PROJECT_PATH>" \
  -executeMethod MCR.EditorTools.BatchTools.BuildMacOS -logFile "<PROJECT_PATH>/Tools/_out/build-macos.log"
```

`BuildWindows` runs the whole regenerable pipeline every time (`RebuildGeneratedAssets`), so one command
rebuilds textures, model FBX imports and the player. `BuildMacOS` uses the tracked **macOS Universal**
Build Profile, producing a player for both Intel and Apple Silicon Macs. Results are written to
`Tools/_out/build-windows.txt` and `Tools/_out/build-macos.txt`.

```
[Build:Windows] Succeeded ..., output=<PROJECT_PATH>/Builds/Windows/MinecraftRecreation.exe
[Build:macOS] Succeeded ..., output=<PROJECT_PATH>/Builds/macOS/Minecraft Recreation.app
```

Build output lives under `Builds/Windows` and `Builds/macOS` (both git-ignored). Double-clicking the Windows
executable starts the game; on macOS open the `.app` bundle. `fastcompile.py` needs
`Library/Bee/artifacts/1900b0aE.dag/Assembly-CSharp.rsp`, which exists once Unity has compiled the
project at least once.

Related one-shot batch methods (all in `Assets/_Game/Editor/BatchTools.cs`, all callable with
`-executeMethod MCR.EditorTools.BatchTools.<Name>`): `Validate` (registry audit → `Tools/_out/validate.txt`),
`RebuildGeneratedAssets`, `ExportModelData`, `SetupScene` (rewrites `Assets/_Game/Scenes/Main.unity`),
`All` (scene + build). `ModelImporter.ImportBlenderModels` / `ValidateModels` come from
`Assets/_Game/Editor/ModelImporter.cs`.

## 5. Unity project lock

Only one Unity instance may hold the project. Before launching Unity or a long editor run:

```bash
tasklist | grep -i Unity.exe
```

If anything shows up, wait (poll every ~30 s) — a second editor or a batch build holding `Library/` will
make the build stall or produce misleading compile errors.

## 6. Automated self-test harness

`Assets/_Game/Scripts/Game/AutoTest.cs` is added by the bootstrap and (when one of its switches is
present) drives a full run without a human: create/load a world, simulate N ticks, run a scripted list of
steps, write screenshots and a JSON report, then quit.

| Switch | Meaning |
|---|---|
| `-mcrNew <seed>` | create and enter a fresh world with that seed (non-numeric seeds hash) |
| `-mcrLoad <folder>` | load an existing save folder instead |
| `-mcrMode creative\|survival\|adventure\|spectator` | starting game mode (default creative) |
| `-mcrTicks N` | simulate N ticks before the script starts (default 400) |
| `-mcrScript "a;b;c"` | semicolon-separated steps, one per frame (see below) |
| `-mcrShot <png>` | one screenshot once ticks are done |
| `-mcrReport <json>` | write the metrics JSON and quit |
| `-mcrQuit` | quit when finished |

Script steps: `give ITEM [count]`, `slot N`, `screen creative\|inventory\|pause\|options\|advancements\|
gamemode\|chat\|help\|death\|credits`, `tab N` (reopen the creative screen on tab N), `close`,
`cmd CHAT-COMMAND`, `look PITCH YAW`, `tp X Y Z`, `wait FRAMES`, `ticks N`, `shot PNG`, `f3`, `f1`,
`view 0-2`, `swing`, `fly`, `mouse X Y` (GUI units), `place BLOCK` (two blocks ahead), `open BLOCK`
(place it and use it), `goto STRUCTURE [inside|PieceName]` (fly to the nearest structure of that id),
`probe` (log player/mob visual state), `log TEXT`, `hold attack|use N` (hold a mouse button for N ticks
through the normal interaction rules), `walk N` (walk forward N ticks), `click SLOT [0|1] [shift]` (menu
slot click), `inv` (log the inventory), `stat` (log position, dimension, mode, health, food, XP),
`hurt MOB [amount]` (damage the nearest mob of that id, e.g. to kill the dragon), `count MOB`,
`block [x y z]` (log the targeted block), `find`, `respawn`, `wear`, `panorama DIR [size]` (capture the six
title panorama faces), `totitle` (save, leave for the title screen; later steps can screenshot it).
Steps run one per frame once the requested ticks are done; `wait N` pauses N frames and `ticks N` waits for
N world ticks.

Final-pass checks used for the report (title screen after leaving a world, Nether trip + portal links in
`level.txt`, save on quit):

```bash
./MinecraftRecreation.exe -screen-width 1600 -screen-height 900 -screen-fullscreen 0 \
  -logFile Tools/_out/run_final2.log -mcrNew 4242 -mcrMode creative -mcrTicks 300 \
  -mcrScript "ticks 100;give diamond_sword;wait 5;cmd time set 12800;ticks 10;shot $ROOT/Tools/_out/shots/final/dusk.png;totitle;wait 30;shot $ROOT/Tools/_out/shots/final/title.png" \
  -mcrReport Tools/_out/report_final2.json -mcrQuit
./MinecraftRecreation.exe -batchmode -nographics -logFile Tools/_out/run_final3.log \
  -mcrNew 777 -mcrMode survival -mcrTicks 500 \
  -mcrScript "ticks 60;stat;cmd dimension the_nether;ticks 150;stat;cmd dimension overworld;ticks 150;stat" \
  -mcrReport Tools/_out/report_final3.json -mcrQuit
```

### EditMode tests

`Assets/_Game/Tests/EditMode/RegistryTests.cs` (assembly `MCR.Tests.EditMode`, 9 tests: registries, spawn
eggs, recipes, item serialization, structure dimensions, fortress/bastion alternation, seed parsing, model
definitions). Batch run (the editor must not be open on the project):

```bash
<UNITY_EDITOR> -batchmode -projectPath "<PROJECT_PATH>" \
  -runTests -testPlatform EditMode -testResults "<PROJECT_PATH>/Tools/_out/editmode-results.xml" \
  -logFile "<PROJECT_PATH>/Tools/_out/editmode.log"
```

Last result: `total="9" passed="9" failed="0"`.

### Token accounting

`python Tools/token_usage.py --until 2026-09-25T03:33:00Z --json Tools/_out/token_usage.json` sums the API
usage recorded in this project's Claude Code session transcripts (main session + subagents), deduplicated per
message id, prices it at the Claude Opus 5.5 API rates ($4 input, $5 5-minute / $8 1-hour cache writes, $0.20
cache reads, $20 output per 1M tokens; no long-context premium) and measures the active working time (pauses
longer than 30 minutes and stalls around API errors excluded). `--until` stops the count at the end of the
project work.

Worked example (windowed, creative, screenshots of the creative inventory, its Survival Inv. tab and chat;
`-batchmode -nographics` instead of the screen flags for a headless run):

```bash
cd "<PROJECT_PATH>"
ROOT="<PROJECT_PATH>"
./MinecraftRecreation.exe -screen-width 1600 -screen-height 900 -screen-fullscreen 0 \
  -logFile Tools/_out/run_ui.log -mcrNew 777 -mcrMode creative -mcrTicks 200 \
  -mcrScript "screen creative;wait 3;shot $ROOT/Tools/_out/shots/ui_creative.png;tab 11;wait 3;shot $ROOT/Tools/_out/shots/ui_survival.png;close;screen chat;wait 3;shot $ROOT/Tools/_out/shots/ui_chat.png" \
  -mcrReport Tools/_out/report_ui.json -mcrQuit
```

The report contains ticks, fps, seed, dimension, chunks loaded, entities, block entities, registered
blocks/items/texture layers/missing textures/recipes/mobs/models, achievements earned, player and spawn
state, a position trace every 25 ticks, and every logged error. Headless form:

```bash
./MinecraftRecreation.exe -batchmode -nographics -logFile Tools/_out/run_headless.log \
  -mcrNew 4242 -mcrMode survival -mcrTicks 600 -mcrReport Tools/_out/report_headless.json -mcrQuit
```

## 7. Blender bridge

Blender is the secondary authoring path for the 3D models; it talks to the dedicated `blender_unity` MCP
addon socket at **127.0.0.1:9876** (never 9877/9878, which belong to the Godot/Unreal instances), with
`UnityMinecraft.blend` in the project root as the master file.

```bash
python Tools/BlenderBridge/blender_unity.py info                # scene, version, export capabilities
python Tools/BlenderBridge/blender_unity.py exec script.py k=v  # run a script (validated first)
python Tools/BlenderBridge/blender_unity.py shot out.png 1000   # viewport screenshot
```

`exec` runs every script through the MCP's own **Safe Mode validator** before sending it, so scripts must
stay inside *bpy / bmesh / mathutils / pure stdlib* and use only native save/export operators. Safe Mode
also blocks file reads *inside* Blender, which is why the pipeline embeds its data in the script text
instead of having Blender open JSON from disk (`{{KEY}}` placeholders are substituted by the driver).

Model pipeline (all models are built into the `MCR_Models` collection; no manual Blender editing):

```
ModelDef.ToJson()                                  code-defined cuboid model + box UVs
  → Editor: ExportModelData   → Assets/_Game/Data/Models/<name>.json + Tools/BlenderBridge/export/skins/<name>.png
  → python Tools/BlenderBridge/fbx_pipeline.py build [model ...]
                              → blender_unity.py exec scripts/build_models.py  (one merged rest-pose mesh)
                              → Assets/_Game/Generated/Models/<name>.fbx + Models/Textures/<name>.png
  → Editor: Tools > Opus 5.5 Minecraft > Import Blender Models  (importer settings, prefab per model,
                              copies mesh+material into Generated/Resources/Models)
  → runtime: MCR.BlenderModels cuts the merged mesh back into per-bone meshes;
             ModelRenderer.Build prefers it and falls back to the procedural ModelMesher mesh
```

`fbx_pipeline.py layout [--save] [model ...]` shows models in a grid in Blender for visual inspection;
`--save` writes `UnityMinecraft.blend`. `Tools/BlenderBridge/scripts/whoami.py` is the environment probe.
The FBX round trip must keep Unity's importer default `bakeAxisConversion = false` —
`ModelImporter.ValidateModels` checks this and reports any model whose FBX no longer matches its ModelDef
(`Tools/_out/models.txt`); mismatched models are skipped, and the game keeps the procedural mesh for them.

Item / prop pipeline (held and dropped items; `MCR_Items` and `MCR_Props` collections, same axis convention):

```
Editor: BatchTools.ExportItemSprites   → Tools/BlenderBridge/export/items/<id>.png (tool sprites of every tier, bow,
                                          crossbow, rods, shears ...) + export/props/<texture>.png (torch, lantern,
                                          campfire_log_lit, fire ...), each with _index.json
  → python Tools/BlenderBridge/item_pipeline.py all      (build | props | view | restore)
        → scripts/build_items.py: item_<id> = the sprite extruded pixel by pixel, face for face ItemRender.ExtrudedMesh;
          prop_<id> = torch / lantern / campfire (+ soul variants) from block pixels with the placed block's uvs
        → Assets/_Game/Generated/Models/Items/<id>.fbx, Models/Props/<id>.fbx (+ Textures/)
  → Editor: ItemModelImporter.ImportItemModels (run by Import Blender Models) → Resources/Models/Items|Props/<id>_mesh
        + Props/<id>_tex.txt; every item is checked vertex for vertex against ItemRender.ExtrudedMesh
        (Tools/_out/item_models.txt), a mismatch is left out
  → runtime: MCR.BlenderItems copies each mesh into the ChunkVertex layout (texture-array layer, shade, full light),
             ItemRender.CreateExtrudedVisual prefers it; -mcrNoBlenderItems forces the procedural meshes
```

## 8. Editor menus — `Tools > Opus 5.5 Minecraft`

| Menu item | Implementation | What it does |
|---|---|---|
| Rebuild Generated Assets | `BatchTools.RebuildGeneratedAssets` | pipeline: render settings, shader refs, baked texture array, model JSON+skins, item sprites, Blender mob/prop FBX import, Blender item FBX import, panorama import settings |
| Configure Render Pipeline | `BatchTools.ConfigureRenderPipeline` | strips SSAO/decals, disables HDR/MSAA/shadows |
| Setup Main Scene | `BatchTools.SetupScene` | writes `Scenes/Main.unity` (Bootstrap + camera) and the build scene list |
| Build Windows | `BatchTools.BuildWindows` | regenerates assets and builds the 64-bit Windows player |
| Import Blender Models | `ModelImporter.ImportBlenderModels` | FBX importer settings, prefabs, `Resources/Models`, `models.txt` report |
| Validate Models | `ModelImporter.ValidateModels` | FBX vs ModelDef audit (axis conversion, bone/cube fit) |
| Export Item Sprites | `BatchTools.ExportItemSprites` | item sprites + prop block textures as PNG for `item_pipeline.py` |
| Import Blender Items | `ItemModelImporter.ImportItemModels` | item/prop FBX import, `Resources/Models/Items` and `/Props`, `item_models.txt` report |

Batch-only entry points (no menu item): `BatchTools.Validate`, `BatchTools.ExportModelData`, `BatchTools.All`.

## 9. Save location

```
%USERPROFILE%\AppData\LocalLow\Opus 5.5\Minecraft Recreation\
  saves\<World>\level.txt              time, weather, game rules, spawn, dragon fight, player + inventory
  saves\<World>\dim0|dim1|dim2\r.<rx>.<rz>.bin    32x32-chunk regions, only changed blocks + block entities + entities
  screenshots\                        F2 screenshots
```

Autosave every 1200 ticks (60 s of unpaused play), plus on quit-to-title, on dimension arrival,
Pause → Save World, Ctrl+S, `/save` and when the window is closed (`GameManager.OnApplicationQuit`).
`level.txt` also stores the known Nether portals per dimension (`portals0..2`). Options, key bindings and
volume sliders live in PlayerPrefs.

## 10. Controls

In world (hard-coded in `Game/GameInput.cs`; the Options → Controls list is display-only):

| Input | Action |
|---|---|
| Mouse | Look (sensitivity + invert-Y; spyglass zoom ×0.25) |
| Left mouse | Attack / mine (hold to keep mining) |
| Right mouse | Use / place / eat / draw bow (hold repeats every 4 ticks) |
| Middle mouse | Pick block |
| Wheel, 1-9 | Hotbar selection |
| W A S D | Move |
| Space | Jump / fly up; double-tap toggles flight; while falling with an elytra, starts gliding |
| Left Shift | Sneak (×0.3 speed), dismount, fly down |
| Left Ctrl (or double-tap W) | Sprint |
| E | Inventory (creative screen in creative mode) |
| Q / Ctrl+Q | Drop one / drop the stack |
| F | Swap main hand and offhand |
| T or `/` | Chat, `/` opens chat pre-filled |
| L | Advancements |
| Esc | Pause menu |
| F1 / F2 / F3 / F5 / F11 | Hide GUI / screenshot / debug overlay / camera perspective / fullscreen |
| Ctrl+S | Save (also walks backwards — it presses S) |
| F3+B / F3+G / F3+A / F3+D / F3+N / F3+T / F3+Q / F3+F4 | hitboxes / chunk borders / reload chunks / clear chat / spectator toggle / clear icon cache / help / game mode switcher (F3+H toggles a flag nothing reads yet) |

In container screens: Esc or E closes; left click picks up a stack, right click splits it in half or
places one; Shift+click quick-moves; double-click gathers; dragging distributes (left = even split,
right = one each, middle = full stacks in creative); 1-9 swap with the hotbar, F swaps with the offhand,
Q throws one (Ctrl+Q throws the stack), middle-click clones in creative.
In chat: Enter sends, Up/Down browse history, Left/Right move the caret, Backspace/Delete edit at the
caret, Tab cycles the completion candidates (Shift+Tab backwards), Esc closes.

## 11. Performance notes

The render distance defaults to 10 chunks (slider 2-32), simulation distance 8 (2-16). Chunk work is
staged (terrain → decorate → light → mesh) across a custom thread pool sized `max(2, cores-2)`; mesh
uploads are budgeted to ~6 ms/frame and chunks remeshed next to the player are done on the main thread.
Entities tick within 8 chunks of the player, mob spawning runs on 20-tick (animals: 400-tick) cycles with
per-category caps, and only the dimension the player is standing in streams and ticks — the other two
stay frozen in memory.

## 12. Known limitations

Gameplay and content gaps are listed honestly in `FEATURE_MATRIX.md`; the ones most likely to bite a
developer are:

* **Maintainer workflow** — the EditMode tests cover the data layer only; gameplay is verified with the
  AutoTest harness plus screenshots; `Debug/`, `Player/`, `SaveSystem/` are reserved empty folders.
* **Code-level gaps** — `opOnly` on commands is never enforced; earned achievements and the
  redstone "opened by power" memory are static and never reset, so they carry into a second world created
  in the same run.
* **Blender pipeline** — the 105 mob/prop FBX meshes are a round-trip of the code-defined cuboid
  geometry, and the 45 item meshes are voxel extrusions of the item sprites built in Blender
  (`Tools/BlenderBridge/item_pipeline.py`), plus 6 props (torches, lanterns, campfires) built from their
  block textures; nothing is sculpted by hand. See the asset gate in `FEATURE_MATRIX.md`.
* **Concurrency** — this project has been edited by more than one session; run
  `python Tools/fastcompile.py runtime 20 --exclude Tools/_none_.cs` before and after any batch build.
