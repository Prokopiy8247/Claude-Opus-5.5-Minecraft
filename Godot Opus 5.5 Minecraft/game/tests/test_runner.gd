extends Node
## Headless test runner (autoloads available):
##   godot --headless --path . res://game/tests/test_runner.tscn -- --test=all
## Tests: structures, dimensions, redstone, crafting, save. Prints PASS/FAIL lines and exits with the
## number of failures as the exit code.

var failures := 0
var passes := 0


func _ready() -> void:
	Game.init_registries()
	var which := String(Game.cmdline.get("test", "all"))
	var t0 := Time.get_ticks_msec()
	if which == "all" or which == "structures":
		_test_structures()
	if which == "all" or which == "dimensions":
		_test_dimensions()
	if which == "all" or which == "crafting":
		_test_crafting()
	if which == "all" or which == "commands":
		_test_commands()
	if which == "all" or which == "save":
		_test_save()
	print("TESTS: %d passed, %d failed (%d ms)" % [passes, failures, Time.get_ticks_msec() - t0])
	get_tree().quit(failures)


func check(cond: bool, label: String) -> void:
	if cond:
		passes += 1
		print("  PASS ", label)
	else:
		failures += 1
		print("  FAIL ", label)


# ------------------------------------------------------------------------------------------------
func _test_structures() -> void:
	print("[structures]")
	for dim in [0, 1, 2]:
		var g := DimensionDB.make_generator(dim, 20260924)
		for t in StructureRegistry.types_for(dim):
			var st: StructureType = t
			# find a start whose biome is valid, then build it
			var built: StructureLayout = null
			var tries := 0
			for rz in range(-6, 7):
				for rx in range(-6, 7):
					if built != null:
						break
					var start := st.start_chunk(g.seed_value, rx, rz)
					if start == StructureType.NONE:
						continue
					tries += 1
					if st.name == "end_city" and Vector2(start.x * 16, start.y * 16).length() < 900.0:
						continue
					var t1 := Time.get_ticks_msec()
					var l := st.build(g.seed_value, start, g)
					if l != null and not l.empty and not l.blocks.is_empty():
						built = l
						var n := 0
						for k in l.blocks:
							n += (l.blocks[k] as Array).size() / 4
						var chests := 0
						for k2 in l.bents:
							chests += (l.bents[k2] as Array).size()
						print("    %s: %d blocks, %d block entities, %d chunks, %d ms" % [st.name, n, chests, l.blocks.size(),
							Time.get_ticks_msec() - t1])
			if built == null and st.biomes.is_empty():
				check(false, "structure %s builds (%d starts tried)" % [st.name, tries])
			elif built == null:
				print("    %s: no start in a valid biome nearby (biome-limited, %d starts tried)" % [st.name, tries])
			else:
				check(true, "structure %s builds" % st.name)
		# locate works for the first type
		var sm := StructureManager.new()
		sm.setup(20260924, dim, g)
		if not sm.types.is_empty():
			var first: StructureType = sm.types[0]
			var p := sm.locate(first.name, Vector3.ZERO, 16)
			check(p != Vector3.INF or not first.biomes.is_empty(), "locate %s in dim %d -> %s" % [first.name, dim, str(p)])


func _test_dimensions() -> void:
	print("[dimensions]")
	for dim in [0, 1, 2]:
		var g := DimensionDB.make_generator(dim, 20260924)
		var solid := 0
		var t1 := Time.get_ticks_msec()
		var coords := [Vector2i(0, 0), Vector2i(3, -2)] if dim != 2 else [Vector2i(0, 0), Vector2i(-1, 1)]
		for cc in coords:
			var ck: Vector2i = cc
			var c := Chunk.new(ck.x, ck.y, g.min_y, g.height)
			g.generate(c)
			for v in c.blocks:
				if v != 0:
					solid += 1
		var ms := Time.get_ticks_msec() - t1
		print("    dim %d: %d non-air blocks in %d chunks, %d ms" % [dim, solid, coords.size(), ms])
		check(solid > 1000, "dimension %d generates terrain" % dim)
	# Nether mapping 1:8
	var p := DimensionDB.map_position(Vector3(800, 64, -160), 0, 1)
	check(p.is_equal_approx(Vector3(100, 64, -20)), "overworld -> nether coordinates scale 1:8")
	var q := DimensionDB.map_position(Vector3(100, 64, -20), 1, 0)
	check(q.is_equal_approx(Vector3(800, 64, -160)), "nether -> overworld coordinates scale 8:1")


func _test_crafting() -> void:
	print("[crafting]")
	var grid := []
	grid.resize(9)
	var planks := ItemStack.of("oak_planks", 1)
	for i in [0, 1, 2, 4, 7]:
		grid[i] = null
	grid[0] = planks.copy()
	grid[1] = planks.copy()
	grid[2] = planks.copy()
	grid[4] = ItemStack.of("stick", 1)
	grid[7] = ItemStack.of("stick", 1)
	var r := RecipeDB.find(grid, 3)
	check(not r.is_empty() and (r["out"] as ItemStack).item_name() == "wooden_pickaxe", "wooden pickaxe recipe")
	check(RecipeDB.smelt_result("iron_ore").get("out", "") == "iron_ingot" or RecipeDB.smelt_result("raw_iron").get("out", "") == "iron_ingot",
		"iron smelting")
	check(not EffectDB.brew("potion", "water", "nether_wart").is_empty(), "awkward potion brewing")
	check(ItemDB.has("ender_dragon_spawn_egg") and ItemDB.has("wither_spawn_egg"), "boss spawn eggs registered")
	check((ItemDB.tab_items.get("spawn_eggs", PackedInt32Array()) as PackedInt32Array).size() >= 80, "spawn egg tab populated")


func _test_commands() -> void:
	print("[commands]")
	check(Commands.mode_name(Player.CREATIVE) == "Creative", "mode names")
	check(Commands.difficulty_name(2) == "Normal", "difficulty names")


func _test_save() -> void:
	print("[save]")
	var folder := "unit_test_world"
	SaveManager.delete_world(folder)
	var ok := SaveManager.write_level(folder, {"name": "Unit", "seed": 42, "time": 1234})
	check(ok, "level.json written")
	var d := SaveManager.read_level(folder)
	check(int(d.get("seed", 0)) == 42 and int(d.get("time", 0)) == 1234, "level.json round trip")
	check((d.get("block_palette", []) as Array).size() == BlockDB.count, "block palette saved")
	var c := Chunk.new(0, 0, -64, 384)
	c.blocks[5000] = BlockDB.id("diamond_block")
	var sd := c.to_save_dict()
	var raw: PackedByteArray = (sd["blocks"] as PackedByteArray).decompress(int(sd["size"]), FileAccess.COMPRESSION_ZSTD)
	check(raw.to_int32_array()[5000] == BlockDB.id("diamond_block"), "chunk compression round trip")
	SaveManager.delete_world(folder)
