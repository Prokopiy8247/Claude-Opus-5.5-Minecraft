# FEATURE_MATRIX.md

Honest status of every feature area, checked against the code on disk and the automated runs of
2026-09-24 and the final pass of 2026-09-25 (build `MinecraftRecreation.exe` in the project folder, EditMode tests). Evidence paths are relative to `Assets/_Game/Scripts/` unless they
start with `Assets/` or `Tools/`. A row is **Full** only when the behaviour is wired into a reachable path
(player interaction, world tick or generation) — not merely because a class exists.

| Status | Meaning | Prompt scale |
|---|---|---|
| **Full** | implemented and reachable in normal play | WORKING |
| **Partial** | works, with real gaps listed in the notes | PARTIAL |
| **Stub** | code exists but a player cannot actually use it | NOT IMPLEMENTED |
| **Missing** | not implemented | NOT IMPLEMENTED |

Registry counts reported by the game itself (`AutoTest.WriteReport`, `Tools/_out/report_headless.json`,
`report_windowed.json`, `report_ui.json`): **896 blocks, 1268 items, 1552 texture layers (0 missing),
908 recipes (crafting + smelting), 89 mobs, 105 models**, 40 advancements defined, 0 errors in all runs.

---

## 1. Engine and core loop

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Chunk storage + streaming | Full | `World/Chunk.cs`, `World/ChunkManager.cs` | 16³ sections; terrain → decorate → light → mesh stages; unload + save beyond render distance +5 |
| Chunk meshing (opaque / cutout / translucent) | Full | `World/ChunkMesher.cs`, `World/MeshCtx.cs` | hidden-face culling, per-biome tints, localized remesh on block change |
| Lighting (sky + block, smooth lighting, AO) | Full | `World/Lighting.cs`, `World/MeshCtx.cs` | incremental relight on edits; smooth-lighting toggle in Options |
| Worker threads | Full | `World/JobSystem.cs` | custom pool, `max(2, cores-2)` threads, 6 ms/frame upload budget |
| Fixed 20 Hz tick | Full | `Game/GameManager.cs` `TickWorld` | only the dimension the player is in ticks |
| Day / night cycle | Full | `Game/GameSession.cs` | 24000-tick day, `/time`, sleeping at night/thunder |
| Weather | Partial | `Game/GameSession.cs` `TickWeather`, `Rendering/SkyRenderer.cs` | rain/thunder cycle, per-biome rain vs snow, fire extinguishing; **no natural lightning, no snow/ice build-up** |
| Sky | Full | `Rendering/SkyRenderer.cs` | square sun with glow and square moon on the correct east-west arc, 8 moon phases (one per day), 1500 stars, 3D cloud cells, orange dawn/dusk sky, Nether fog per biome, mottled End sky texture |
| Colour, light and fog pipeline | Full | `Shaders/MCRCommon.hlsl`, `Shaders/MCRChunk.shader`, `Shaders/MCREntity.shader`, `Rendering/WorldLighting.cs` | textures are tinted, shaded and lit in gamma space as in the original (the project is linear, which had washed colours out); cylindrical distance fog that blends into the horizon colour |
| Game modes | Partial | `Entities/Player.cs` `SetGameMode` | Survival, Creative (default for new worlds), Spectator Full; Adventure Partial (tools still till/strip); **Hardcore Missing** |

## 2. World generation

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Overworld terrain | Full | `Gen/OverworldGenerator.cs` | continentalness/erosion/ridge noise, height splines, 3D density overhangs, rivers, y −64..320, deepslate blend |
| Caves | Partial | `Gen/OverworldGenerator.cs` | cheese caverns + spaghetti tunnels; **no noodle caves, ravines or aquifers** (caves below sea level are dry); lava only below y −55 |
| Ores | Full | `Gen/OverworldDecorator.cs` `Ores` | 14 ore + 6 stone/gravel/dirt blob specs, deepslate variants, emerald in mountains only; no large veins |
| Biomes | Full | `Gen/Biome.cs`, `OverworldGenerator.PickBiome` | **56** registered: 44 overworld surface, 4 cave (3D), 5 Nether, 3 End; 9 vanilla biomes absent (deep cold/frozen/lukewarm ocean, old-growth birch/pine, windswept forest/savanna, end midlands, small end islands) |
| Trees and vegetation | Full | `Gen/TreeFeatures.cs`, `Gen/OverworldDecorator.cs` | 28 tree/feature types incl. mega trees, mangroves, cherry, pale oak, huge mushrooms, fungi; cave decoration for lush/dripstone/deep dark; bee nests, sea pickles, coral fans never generate |
| Lakes, geodes, fossils, desert wells | Missing | — | none generated |

## 3. Structures (20 types registered in `Structures/StructuresCore.cs`)

| Structure | Status | Evidence | Notes |
|---|---|---|---|
| Village | Full | `Structures/StructuresVillage.cs` | 5 styles, 15 house kinds with job blocks, farms, bell, golem, villagers, loot |
| Stronghold + End portal room | Full | `Structures/StructuresStronghold.cs` | 128 on 8 rings, 10 piece types; portal room with 12 frames (10% pre-filled eyes), lava, silverfish spawner |
| Mineshaft, dungeon | Full | `Structures/StructuresUnderground.cs` | rails, cobwebs, cave-spider spawners, chests |
| Desert pyramid, jungle temple, swamp hut, igloo, pillager outpost | Full | `Structures/StructuresSurface.cs` | TNT trap, witch + cat, igloo basement, outpost tower |
| Shipwreck, ocean ruin, buried treasure | Full | `Structures/StructuresWater.cs` | warm/cold ruins, beach treasure |
| Ruined portal, trail ruins | Full | `Structures/StructuresRuins.cs` | overworld-land ruined portals only (no Nether/ocean variants) |
| Woodland mansion, ancient city | Partial | `Structures/StructuresMansionCity.cs` | compact layouts (37×21 mansion, 64×64 city) |
| Trial chambers | Partial | `Structures/StructuresTrial.cs` | atrium + 4 wings + trial spawners; no vaults or decorated pots |
| Nether fortress | Full | `Structures/StructuresNether.cs` | bridges, blaze spawners, nether wart, loot; fixed in the final pass (the structure never set `dim = DimensionId.Nether`, so it never generated) with a regression test; fortresses and bastions share a 27-chunk grid (`NetherComplex`) like the original; verified in-game by screenshot |
| Bastion remnant | Full | `Structures/StructuresNether.cs` | same fix; skipped in basalt deltas; verified in-game by screenshot |
| End city + end ship | Full | `Structures/StructuresEnd.cs` | outer islands 1000+ blocks out; elytra (in a chest), shulkers, loot |
| Ocean monument | Missing | — | guardians exist but only via egg/command |
| `/locate` | Full | `Game/Commands.cs`, `StructureManager.Locate` | every generated structure id, including `fortress` and `bastion_remnant` in the Nether |
| Loot tables | Full | `Game/Loot.cs`, `Blocks/BlockEntities.cs` `UnpackLoot` | 32 tables, seeded per chest, filled on first open |

## 4. Dimensions and portals

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Nether terrain, biomes, ores | Full | `Dimensions/NetherGenerator.cs` | 128-high 3D density, lava sea at y 31, 5 biomes, glowstone, quartz/gold/ancient debris |
| Nether structures | Full | see fortress/bastion above | |
| Nether portal frame + ignition + travel | Full | `Game/Portals.cs`, `Blocks/BlockTypes2.cs` `PortalBlock` | obsidian frames up to 21 interior, flint & steel / fire / dispenser; ÷8 / ×8 coordinates |
| Nether portal linking | Full | `Game/Portals.cs`, `Game/SaveManager.cs` | known portals per dimension are saved in `level.txt` (`portals0..2`), so links survive a reload; arrival picks a safe spot (never lava) and builds the return portal; the purple screen overlay never appears |
| End portal (eyes of ender, activation, travel) | Full | `Blocks/FunctionalBlocks.cs` `EndPortalFrameBlock`, `Game/Portals.cs`, `Items/ItemTypes2.cs` `EnderEyeItem` | thrown eye flies toward the nearest stronghold |
| End island, pillars, crystals, exit fountain, 20 gateways, outer islands, chorus | Full | `Dimensions/NetherGenerator.cs` `EndGenerator`, `Mobs/Bosses.cs` `DragonFight` | |
| Exit portal → credits → home | Full | `Blocks/FunctionalBlocks.cs` `EndPortalBlock.OnEntityInside` → `DragonFight.OnExitPortalUsed` → `Portals.TravelEnd` | routed through the fight (only once the dragon is dead) as of this session; credits roll on every exit, not only the first |
| End gateways | Full | `Game/Portals.cs` `UseGateway` | players only (thrown ender pearls don't trigger them) |
| Per-dimension persistence | Full | `Game/SaveManager.cs` | separate region folders `dim0/1/2`; inactive dimensions do not tick |

## 5. Mining and building

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Mining | Full | `Game/PlayerInteraction.cs`, `Blocks/Block.cs`, `Items/Item.cs` | hardness, 6 tool tiers, correct-tool drops, Efficiency/Haste/Fatigue, silk touch, fortune, ore XP, durability, crack overlay |
| Creative instant break / infinite placement | Full | `Game/PlayerInteraction.cs` | swords can't break blocks in creative |
| Placement | Full | `Items/ItemTypes.cs` `BlockItem.Place` | stairs shapes, slab merging, doors/beds (2 blocks), 6-way facing, chest joining |
| Waterlogging | Missing | — | |
| Signs, banners, cake, item frames, paintings, armour stands, leads | Missing | — | referenced by recipes/trades/loot but not implemented (entries are skipped) |

## 6. Crafting and processing

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| 2×2 and 3×3 crafting | Full | `Inventory/Containers.cs` `InventoryMenu`, `CraftingTableMenu`; `Crafting/Recipes.cs` | shaped (any offset, mirrored), shapeless, tags, remainders, shift-craft up to 64 |
| Recipe volume | Full | `Crafting/RecipesCore.cs`, `Recipes.cs`, `Tools/_out/validate.txt` | 908 = 783 crafting + 125 smelting (the number the game reports), plus 175 stonecutting and 10 smithing |
| Recipe book | Missing | `UI/UiAtlas.cs` ("recipe book toggle omitted") | known recipes are tracked and saved but have no UI |
| Furnace, blast furnace, smoker | Full | `Blocks/BlockEntities.cs` `FurnaceEntity` | fuel table, 2× speed variants, stored XP paid on output |
| Campfire cooking | Full | `Blocks/BlockEntities.cs` `CampfireEntity` | |
| Brewing | Full | `Game/Alchemy.cs`, `Inventory/Menus.cs` `BrewingMenu` | 44 potion types, 56 mixes, blaze-powder fuel |
| Splash / lingering / tipped | Partial | `Entities/Projectiles.cs` `ThrownPotion`, `AreaEffectCloud` | bottles can't be filled from water; dragon's breath can't be collected (no lingering in survival) |
| Enchanting table | Full | `Inventory/Menus.cs` `EnchantMenu` | bookshelf power (≤15), 3 seeded offers, lapis + levels |
| Enchantment effects | Partial | `Combat/CombatData.cs` + call sites | 38 of 43 enchantments do something; Swift Sneak, Soul Speed, Breach, Wind Burst, Binding do nothing; Thorns only one way |
| Anvil | Full | `Inventory/Menus.cs` `AnvilMenu` | rename, repair, combine, prior-work cost, "Too Expensive", damage states |
| Grindstone / stonecutter | Full | `Inventory/Menus.cs` | |
| Smithing table | Partial | `Inventory/Menus.cs` `SmithingMenu` | netherite upgrade only; the template has no survival source; no trims |
| Loom | Stub | `Inventory/Menus.cs` `LoomMenu` | logic works but banners don't exist |
| Cartography table | Missing | `Blocks/FunctionalBlocks.cs` | registered with no station |
| Beacon | Full | `Blocks/BlockEntities.cs` `BeaconEntity`, `Inventory/Menus.cs`, `Rendering/BlockEntityRenderer.cs` | pyramid levels, powers, range, animated light beam while the beacon sees the sky |
| Villager trading | Full | `Inventory/Trading.cs`, `Mobs/MobsPassive.cs` | 13 professions, 5 levels, restock on open; no reputation/discounts |

## 7. Redstone

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Dust, torches, redstone block | Full | `Redstone/Redstone.cs`, `Blocks/BlockTypes2.cs` | network search, strong/weak power, 2-tick torch delay |
| Repeater | Partial | `Redstone/Redstone.cs` `RepeaterBlock` | delays 1-4; **no locking** |
| Comparator | Full | `Redstone/Redstone.cs` `ComparatorBlock` | compare/subtract, container reading |
| Levers, buttons, pressure plates | Full | `Redstone/Redstone.cs` | arrows press wooden buttons |
| Pistons (normal + sticky) | Partial | `Redstone/Redstone.cs` `Pistons` | push 12 / pull 1; **no slime/honey sticking, no animation** |
| Observer, hopper, dispenser/dropper, lamp, copper bulb, doors/trapdoors, note block, 4 rail types, daylight detector, target, trapped chest, bell | Full | `Redstone/Redstone.cs`, `Blocks/FunctionalBlocks.cs`, `Game/Alchemy.cs` `Dispensing`/`Rails` | |
| Sculk sensor | Partial | `Redstone/Redstone.cs` | triggered only by stepping on it |
| Lightning rod, crafter | Stub | `Entities/SimpleEntities.cs`, `Blocks/FunctionalBlocks.cs` | decorative only |
| Tripwire | Missing | — | |
| Scheduled block ticks | Full | `World/World.cs` `ScheduleTick` | not saved across reloads |

## 8. TNT and explosions

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| TNT priming and blast | Full | `Entities/SimpleEntities.cs` `PrimedTnt`, `Game/Explosion.cs` | 80-tick fuse, 1352-ray blast, blast resistance, chain priming, knockback + exposure damage |
| Creeper, bed/anchor, end crystal, TNT cart, ghast fireball explosions | Full | `Mobs/MobsHostile.cs`, `Blocks/FunctionalBlocks.cs`, `Entities/Vehicles.cs` | |
| Block drops from player-lit TNT | Partial | `Game/Explosion.cs:92` | 100% drops only when the source is the TNT entity itself; player-lit TNT passes the igniter, so only ~1/power of blocks drop |

## 9. Vehicles

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Boats, chest boats, bamboo raft | Full | `Entities/Vehicles.cs` `Boat` | 9 woods + raft, 2 seats, mobs board |
| Minecarts (plain, chest, TNT, furnace, hopper) | Partial | `Entities/Vehicles.cs` `Minecart` | rails, slopes, powered/activator rails; hopper cart only picks up items |
| Pig / strider riding | Full | `Mobs/MobsPassive.cs` | saddle + carrot / warped fungus on a stick |
| Horse, donkey, mule | Partial | `Mobs/MobsPassive.cs` `HorseMob` | taming, saddle, charged jump; **no chest inventory UI, no horse armour** |
| Llama, camel, happy ghast | Partial | `Mobs/MobsPassive.cs`, `Mobs/MobsOther.cs` | llamas not rideable; camel dash unused |
| Leads | Missing | — | |

## 10. Farming and food

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Crops, farmland, hoe, hydration, trampling | Full | `Blocks/BlockTypes.cs` `CropBlock`, `FarmlandBlock` | |
| Bone meal | Full | `Items/ItemTypes.cs` `BoneMealItem` | not on cocoa, sea pickles, flowers, cave vines |
| Breeding | Full | `Mobs/Mob.cs` `BreedGoal` | love mode, babies, XP |
| Hunger, saturation, exhaustion, regeneration, starvation | Full | `Entities/Player.cs` `HungerData` | vanilla exhaustion values |
| Foods | Full | `Items/ItemCatalog.cs` | 40 foods + milk; cake missing |

## 11. Combat

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Melee | Full | `Entities/Player.cs` `Attack` | attack cooldown, crits, sweeping, sprint knockback, mace smash |
| Bow, crossbow, trident, spear | Full | `Items/ItemTypes2.cs`, `Entities/Projectiles.cs` | charge, multishot, loyalty/riptide/channeling |
| Shields and armour | Partial | `Entities/Entity.cs` `Hurt`, `ApplyArmor` | vanilla armour formula; axes don't disable shields; netherite knockback resistance unused |
| Status effects | Partial | `Combat/CombatData.cs`, `Entities/Entity.cs` | 26 of 36 effects do something (Nausea, Glowing, Darkness, Bad Omen, ... do nothing) |
| XP orbs and levels, Totem of Undying | Full | `Entities/SimpleEntities.cs` `XpOrb`, `Entities/Player.cs` | vanilla XP curve, Mending |

## 12. Mobs (89 registered in `Mobs/MobRegistry.cs`; every one has its own articulated model)

| Category | Status | Registered ids | Notes |
|---|---|---|---|
| Passive (25) | Full | pig, cow, mooshroom, sheep, chicken, rabbit, horse, donkey, mule, skeleton_horse, zombie_horse, camel, cat, ocelot, fox, goat, turtle, frog, armadillo, sniffer, allay, bat, parrot, happy_ghast, ghastling | breeding, taming (wolf/cat/horse), tempting |
| Neutral (7) | Full | wolf, llama, trader_llama, polar_bear, panda, bee, enderman | enderman stare-aggro, teleports, moves blocks — **carried block not drawn** |
| Hostile, overworld (21) | Full | zombie, husk, drowned, zombie_villager, skeleton, stray, bogged, parched, creeper, spider, cave_spider, slime, witch, silverfish, endermite, phantom, breeze, creaking, warden, sulfur_cube, camel_husk | A* pathfinding, bows, creeper swell, witch potions, phantom insomnia spawns |
| Aquatic (13) | Partial | cod, salmon, tropical_fish, pufferfish, squid, glow_squid, dolphin, axolotl, tadpole, nautilus, zombie_nautilus, guardian, elder_guardian | guardian laser beam is not drawn; guardians/tadpole/zombie_nautilus never spawn naturally (no monument) |
| Nether (10) | Partial | ghast, blaze, piglin, piglin_brute, zombified_piglin, hoglin, zoglin, magma_cube, wither_skeleton, strider | behaviours Full (fireballs, bartering, zombification); blazes and wither skeletons now come from fortress spawners / fortress spawning; blaze rods render as one column |
| End (1) | Partial | shulker | homing bullets use a white-concrete cube placeholder |
| Illagers (5) | Partial | pillager, vindicator, evoker, vex, ravager | patrols yes; **raids missing**; evoker fangs are a dripstone placeholder; ravager has no roar/stun |
| Golems and villagers (5) | Partial | villager, wandering_trader, iron_golem, snow_golem, copper_golem | iron/snow golems can't be built from blocks; iron golem limbs don't animate |
| Spawning | Full | `Mobs/MobSpawner.cs` | per-category caps, light rules, biome tables, despawning, patrols, wandering trader |

## 13. Bosses

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Ender Dragon | Full | `Mobs/Bosses.cs` `EnderDragonMob`, `DragonFight` | 8 phases, perching + breath, crystal healing (beams), death climb, 12000/500 XP, egg, exit portal, gateways, respawn ritual; jaw never opens |
| Wither | Partial | `Mobs/Bosses.cs` `WitherSummon`, `WitherBoss` | soul sand/skull summoning, 3-head skull fire, half-health armour phase — **no armour visual**; 20-tick charge instead of 220 |
| Elder Guardian | Partial | `Mobs/MobsOther.cs`, `Game/GameManager.cs` | Mining Fatigue curse as a title only; no natural spawn |
| Warden | Full | `Mobs/MobsHostile.cs` `WardenMob` | emerge/dig, vibration + sniff anger, darkness pulse, sonic boom |
| Boss bar | Partial | `UI/Hud.cs` `RenderBossBar` | dragon and wither; plain bars, atlas sprites unused |
| Raids | Missing | — | |

## 14. Inventory and UI screens

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Survival HUD | Full | `UI/Hud.cs` `RenderHud` | hotbar, offhand, XP bar and level, hearts with dark containers, half hearts, hurt blink, low-health jitter, poison/wither/frozen/absorption variants, hunger with containers, armour row with empty slots, air, crosshair, attack indicator, fire overlay, advancement toasts |
| Survival inventory (2×2 crafting, armour, offhand) | Full | `Inventory/Containers.cs` `InventoryMenu`, `UI/ContainerScreen.cs` | armour/shield silhouettes fixed this session (`UI/GuiScreen.cs` `DrawEmptyIcon`) |
| Creative inventory | Full | `UI/CreativeScreen.cs` | 13 tabs in top/bottom strips with item icons, all reachable (previously only 4 of 12 were drawn); 1266 catalogue items (Building 360, Natural 191, Colored 166, Tools 104, Spawn Eggs 91, Functional 88, Ingredients 88, Redstone 75, Combat 60, Food 43 — `Tools/_out/validate.txt`); search with live count, scroll bar, clear-inventory button; the Operator tab has no items |
| Creative "Survival Inv." tab | Full | `UI/CreativeScreen.cs` `RenderSurvival` | the real `InventoryMenu` (2×2 crafting, armour, offhand, hotbar selection frame, preview box) with ContainerScreen's click protocol: pick up/place, right-click split, shift-click, double-click gather, drag distribution, 1-9/F/Q/middle-click; the cursor stack follows between tabs |
| Container screens (16 menus) | Full | `UI/ContainerScreen.cs`, `UI/MenuWidgets.cs` | furnace/brewing progress, anvil name field, enchanting offers, stonecutter/loom pickers, beacon powers, merchant list |
| Mouse clicks in every screen | Full | `UI/Hud.cs` `Input`/`Render`/`RenderUnderneath` | fixed this session: click edges were cleared before any screen drew, so buttons, tabs and slots never reacted to a real mouse; only the top screen now receives clicks, and the key that opens a screen (E) no longer closes it in the same frame |
| Title, world list, pause, options, death, chat, loading, credits, game mode switcher, help | Full | `UI/Screens.cs`, `UI/MiscScreens.cs`, `UI/GameModeScreen.cs` | |
| Create world screen | Partial | `UI/Screens.cs` `CreateWorldScreen` | name and seed boxes (click or Tab to switch); numeric seeds are used as typed, text seeds hashed like Java's `String.hashCode` (EditMode test); only Survival/Creative offered, no world types |
| Advancements screen | Partial | `UI/GameModeScreen.cs`, `UI/Hud.cs` `RenderToast` | flat list, no tree; "Advancement Made!" toasts slide in at the top right |
| Inventory player preview | Full | `UI/ContainerScreen.cs`, `UI/PlayerPreview.cs` | the player model (with armour and held items) filmed by its own camera into a texture; body and head turn toward the mouse pointer |
| Title screen | Full | `UI/Screens.cs` `TitleScreen`, `UI/TitleArt.cs` | rotating panorama cube (six faces captured from a generated world, `Assets/_Game/Generated/Resources/Panorama`), stone-block "MINECRAFT" lettering built from our own glyphs (not the official logo), pulsing splash text, menu music |
| Recipe book | Missing | — | |
| Bitmap font, UI atlas, item icons | Full | `UI/FontData.cs`, `UI/UiAtlas.cs`, `UI/ItemIcons.cs` | original 5×7 font and code-painted sprites; no UGUI widgets, no TextMeshPro, no OnGUI |

## 15. Commands

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Command table | Full | `Game/Commands.cs` | 26 commands (help, gamemode, difficulty, time, weather, tp, give, enchant, effect, summon, kill, clear, seed, gamerule, locate, setblock, fill, particle, spawnpoint, dimension, save, advancement, title, killall, debug, reload) + 9 aliases (gm, ?, day, night, fly, heal, god, xp, experience) |
| Tab completion | Full | `Game/Commands.cs` `Complete`/`ArgumentPool`, `UI/Screens.cs` `ChatScreen` | added this session: command names, then per-argument ids (items for /give, mobs for /summon and /killall, blocks for /setblock and /fill, effects, enchantments, structures, advancements, game rules) and keywords; live suggestion box; Tab / Shift+Tab cycle; caret editing with Left/Right/Backspace/Delete |
| Permission level | Stub | `Game/Commands.cs` | `opOnly` is never enforced; "Allow Cheats" is ignored |
| Command gaps | Partial | `Game/Commands.cs` | `~` / `~N` relative coordinates work (block coordinates for setblock/fill/spawnpoint/clone); `/dimension` arrives on a safe spot / the End platform; `/kill` has no effect on invulnerable (creative) players; `/advancement revoke` does nothing; `/weather` duration is ticks |

## 16. Debug tools

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| F3 overlay | Full | `UI/Hud.cs` `RenderDebug` | fps, position, chunk, facing, biome, light, time, dimension, seed, chunk/entity/particle counts, target |
| F3 combos | Full | `Game/GameInput.cs` | F3+B hitboxes, F3+G chunk borders, F3+A reload, F3+D clear chat, F3+N spectator, F3+T icon cache, F3+Q help, F3+F4 switcher; F3+H flag unused |
| Frame-time chart | Stub | `UI/Hud.cs` `RenderDebugCharts` | `debugCharts` is never set |
| AutoTest harness | Full | `Game/AutoTest.cs` | `-mcrNew/-mcrLoad/-mcrMode/-mcrTicks/-mcrScript/-mcrShot/-mcrReport/-mcrQuit` |
| Editor tooling | Full | `Assets/_Game/Editor/BatchTools.cs`, `Assets/_Game/Editor/ModelImporter.cs`, `Assets/_Game/Editor/ItemModelImporter.cs` | `Tools > Opus 5.5 Minecraft` menu (Rebuild Generated Assets imports the mob, prop and item FBX), batch build, registry validation |
| EditMode tests | Full | `Assets/_Game/Tests/EditMode/RegistryTests.cs` | 9 tests (registries, spawn eggs for every mob/boss, planks/crafting table/furnace/smelting recipes, item serialization, structure dimensions, fortress/bastion alternation, seed parsing, model definitions); 9/9 pass in batch mode |

## 17. Audio

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Sound effects | Full | `Audio/Sounds.cs` | all synthesized from the event name (no audio files): 38 block sound types, 74 creature voice profiles, 16 note-block instruments, ~190 literal event names |
| Music | Full | `Audio/Sounds.cs` `MusicGen`, `Game/GameManager.cs` | generated piano pieces for overworld/Nether/End/menu (the menu piece starts on the title screen), 17 discs |
| Volume settings | Full | `UI/Screens.cs` `OptionsScreen`, `Game/GameManager.cs` | master + 8 category sliders, saved in PlayerPrefs with the other options |

## 18. Save / load

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| World + player save | Full | `Game/SaveManager.cs`, `Entities/Player.cs` `SaveFull` | seed, time, weather, rules, spawn, dragon fight, player (position, mode, health, food, XP, inventory, ender chest, effects, recipes, advancements) |
| Chunk deltas, block entities, entities | Full | `Game/SaveManager.cs` | only changed blocks are stored; mobs, items, vehicles, crystals saved per chunk |
| Autosave | Full | `Game/GameManager.cs` | every 60 s of play, on dimension change, pause menu, Ctrl+S, `/save` |
| Save on quit, portal links | Full | `Game/GameManager.cs` `OnApplicationQuit`, `Game/SaveManager.cs` | closing the window saves the running world; portal links are saved |
| Gaps | Partial | — | scheduled block ticks and in-flight projectiles aren't saved |

## 19. Achievements (advancements)

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| Advancement set | Partial | `Game/Achievements.cs` | 40 defined; 27 can be earned in play, 13 only via `/advancement grant` (e.g. smelt_iron, enchant, brew, trade, raid, iron_golem); toasts shown in the HUD; earned set not reset between worlds in one run |

---

## 20. Asset completion gate

Required by the prompt ("Asset completion gate"): inspect the visible result and verify each item.
Checked against the screenshots in `Tools/_out/shots/` (this session: `ui_creative.png`,
`ui_survival.png`, `ui_chat*.png`, `final_windowed.png`; earlier sets `t1/`-`t8/`) and the code.

| Gate check | Status | Evidence |
|---|---|---|
| High-priority mobs are not primitive placeholders | **Pass (Partial)** | 89/89 mobs have dedicated articulated cuboid models (`Mobs/MobModels.cs`), rebuilt in Blender, with painted skins (`Rendering/SkinPainter.cs`, faces and overlays fixed in the final pass), animated procedurally (`Rendering/MobVisual.cs`); worn armour is drawn on players and biped mobs. Defects: blaze rods and guardian spikes collapse into one column, iron golem limbs don't move, enderman's carried block not drawn |
| Held tools are not flat 2D placeholder sprites | **Pass** | 45 held items (every sword, pickaxe, axe, shovel and hoe of all 7 tiers, bow, crossbow, mace, shears, spyglass, brush, fishing rod, flint and steel, carrot on a stick, stick) use Blender-built voxel meshes (`Tools/BlenderBridge/item_pipeline.py` → `Assets/_Game/Generated/Models/Items/*.fbx` → `Rendering/BlenderItems.cs`), torch/lantern/campfire (+ soul variants) use Blender props; everything else is extruded 1/16 thick by `Rendering/ItemRender.cs`; shield, trident and spear use the Blender cuboid models. Screenshots `Tools/_out/shots/held_fbx_*.png`, `cmp_props.png` |
| Visible blocks have real pixel-art textures | **Pass** | 1552 procedural 16×16 layers, 0 missing (`Textures/*`); chests (normal, trapped, ender, copper, double) are drawn with the Blender chest models and an opening lid by `Rendering/BlockEntityRenderer.cs`, which also draws the spinning mob in spawners |
| Creative inventory has usable icons | **Pass** | `UI/ItemIcons.cs` isometric block icons and 2× item sprites; all 13 tabs now reachable with item-icon tabs (`ui_creative.png`) |
| Nether and End have dimension-specific materials | **Pass** | netherrack, soul sand/soil, basalt, blackstone, nylium, wart blocks, fungi, end stone, purpur, chorus, end rod, portal frame textures; per-biome Nether fog; textured End sky |
| Ender Dragon has a recognizable articulated model | **Pass** | 28 bones (5-part neck, jaw, 12-part tail, 2-segment wings, 4 legs), glowing eyes, wing/neck/tail animation (jaw never opens) |
| Wither has a recognizable model | **Pass (Partial)** | 3 heads, shoulder bar, ribbed spine, tail, glowing eyes; no half-health armour overlay |
| TNT has recognizable visuals and VFX | **Pass** | red/white banded TNT texture, flashing + swelling primed TNT, smoke, 8-frame explosion, screen flash (no camera shake) |
| Portals have animated/distinct materials | **Pass** | 16-frame animated nether portal (purple swirl) and end portal/gateway (dark teal sparkle), distinct from each other; flat, no parallax |
| HUD is not default Unity UI styling | **Pass** | custom immediate-mode renderer (meshes rendered into a texture), code-painted atlas and original font; half hearts and containers as in the original |

### 3D asset production: procedural vs Blender

The spec's list of important 3D assets (player, mobs, dragon, wither, weapons, bows, shields,
tridents/spears, armour, boats, minecarts, chests, crystals, props, held items) maps onto the **105 model
definitions** in `Mobs/MobModels.cs` (89 mobs + player + 15 props: minecart, boat, chest_boat, raft,
end_crystal, chest, ender_chest, trapped_chest, copper_chest, shield, trident, spear, bow, crossbow, armor)
plus the procedural block/item pipelines.

| Stage | Count | Evidence |
|---|---|---|
| Geometry defined procedurally in code (cuboid ModelDefs) | **105 / 105 (100%)** | `Mobs/MobModels.cs`, exported as `Assets/_Game/Data/Models/*.json` |
| Skins painted procedurally in code | **105 / 105 (100%)** | `Rendering/SkinPainter.cs`, `Rendering/PropSkins.cs` |
| Rebuilt in Blender (`UnityMinecraft.blend`, via `blender_unity` on port 9876, Safe Mode) and exported as FBX | **105 / 105 (100%)** | `Tools/_out/blender_build_all.txt` ("exported 105/105 FBX"), `Assets/_Game/Generated/Models/*.fbx`; `.blend` saved 2026-09-24 22:01 |
| FBX imported into Unity (prefab + Resources mesh/material, matches its ModelDef) | **105 / 105 (100%), 0 mismatches** | `Tools/_out/models.txt` (`BLENDER_MODELS defs=105 fbx=105 imported=105 match=105`) |
| FBX meshes actually rendered at runtime | **105 / 105** where a model is drawn | `Rendering/ModelRenderer.cs` prefers `BlenderModels.BoneMeshes`; every run log of this session prints `[BlenderModels] Using the Blender FBX meshes from Resources/Models`. The first-person arm is still cut from the procedural mesh (`UI/PlayerVisual.cs`) |
| Held items and props built in Blender (voxel extrusion of the item sprites, props from their block textures), exported as FBX, imported and rendered | **45 items + 6 props** | `Tools/BlenderBridge/item_pipeline.py`, `Tools/BlenderBridge/scripts/build_items.py`, `Assets/_Game/Generated/Models/Items`, `.../Props`, `Assets/_Game/Editor/ItemModelImporter.cs`; run logs print `[BlenderItems] Using the Blender item meshes from Resources/Models/Items and Props`; `-mcrNoBlenderItems` switches back to the procedural meshes for comparison |
| Hand-modelled / sculpted in Blender | **0 / 105 (0%)** | the FBX files are a faithful round-trip of the code geometry (same vertices and box UVs, validated by `ModelImporter.ValidateModels`), not independent Blender authoring |

Everything outside those models is procedural only, with no Blender involvement: terrain and every
block shape (`World/MeshCtx.cs`, block `Emit` methods), the remaining held/dropped items
(`Rendering/ItemRender.cs`), particles, sky, UI, textures and sound.

**Still open (honest gaps):**
* model defects: blaze rods and guardian spikes overlap, iron golem has no limb animation, the enderman's
  carried block and the wither's armour phase are not visualised;
* placeholder visuals: shulker bullet (white concrete cube), evoker fangs (pointed dripstone), no guardian
  laser beam;
* no model has been hand-authored in Blender; Blender is used as a build/export stage.
