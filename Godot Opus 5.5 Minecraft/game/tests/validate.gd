extends SceneTree
## Validation pass: registries, recipes, loot, portals, item icons, game rules for every block.
##   godot --headless --path . --script res://game/tests/validate.gd

var problems: Array = []
var info: Array = []


func _init() -> void:
	var t0 := Time.get_ticks_msec()
	BlockDB.init_registry()
	BiomeDB.init()
	TextureLibrary.load_blocks()
	ItemDB.init()
	RecipeDB.init()
	LootDB.init()
	_check_block_defs()
	_check_recipe_books()
	info.append("registries ready in %d ms" % (Time.get_ticks_msec() - t0))
	for s in info:
		print("[ok] ", s)
	for p in problems:
		print("[PROBLEM] ", p)
	if RecipeDB.skipped > 0:
		print("[info] skipped recipes: ", ", ".join(RecipeDB.skipped_names))
	print("validate: %d blocks, %d items, %d shaped + %d shapeless recipes, %d problems" % [
		BlockDB.count, ItemDB.count, RecipeDB.shaped.size(), RecipeDB.shapeless.size(), problems.size()])
	quit(0 if problems.is_empty() else 1)


func _check_block_defs() -> void:
	var seen := {}
	for d in BlockDB.defs:
		var bd: BlockDef = d
		if seen.has(bd.name):
			problems.append("duplicate block " + bd.name)
		seen[bd.name] = true
		if bd.id == 0 and bd.name != "air":
			problems.append("id 0 is not air: " + bd.name)
		if bd.has_item and bd.tabs.is_empty() and not bd.name.begins_with("moving"):
			problems.append("block without creative tab: " + bd.name)
		if bd.model == BlockDB.M_AIR and bd.name != "air" and bd.name != "moving_piston":
			problems.append("M_AIR block: " + bd.name)
		if bd.opacity < 0 or bd.opacity > 15:
			problems.append("bad opacity on " + bd.name)
		if bd.light < 0 or bd.light > 15:
			problems.append("bad light on " + bd.name)
	info.append("%d blocks" % BlockDB.count)
	var layers := BlockDB.tex_layer.size()
	var missing := 0
	for t in BlockDB.texture_names:
		if not BlockDB.tex_layer.has(String(t)):
			missing += 1
	if missing > 0:
		problems.append("%d textures missing from atlas" % missing)
	info.append("%d texture layers, %d block textures referenced" % [layers, BlockDB.texture_names.size()])
	info.append("%d non-block items" % ItemDB.defs.size())
	# creative tabs
	for tab in ItemDB.TABS:
		var n: int = (ItemDB.tab_items.get(tab, PackedInt32Array()) as PackedInt32Array).size()
		info.append("tab %s: %d items" % [tab, n])
		if n == 0:
			problems.append("empty creative tab " + tab)
	# items must have an icon description
	for it in ItemDB.defs:
		var d: ItemDef = it
		if d.icon == "":
			problems.append("item without icon: " + d.name)


func _check_recipe_books() -> void:
	info.append("%d shaped / %d shapeless recipes (%d skipped: unknown items)" % [RecipeDB.shaped.size(), RecipeDB.shapeless.size(), RecipeDB.skipped])
	if RecipeDB.shaped.size() + RecipeDB.shapeless.size() < 200:
		problems.append("suspiciously few recipes: %d" % (RecipeDB.shaped.size() + RecipeDB.shapeless.size()))
	info.append("%d smelting recipes, %d stonecutting inputs, %d smithing recipes" % [RecipeDB.smelting.size(), RecipeDB.stonecut.size(), RecipeDB.smithing.size()])
	# craft a few known recipes through the real matcher
	_craft_check(["oak_log", "", "", ""], 2, "oak_planks", 4)
	_craft_check(["oak_planks", "oak_planks", "oak_planks", "oak_planks"], 2, "crafting_table", 1)
	_craft_check(["oak_planks", "", "oak_planks", ""], 2, "stick", 4)
	_craft_check(["oak_planks", "oak_planks", "oak_planks", "oak_planks", "", "oak_planks", "oak_planks", "oak_planks", "oak_planks"], 3, "chest", 1)
	_craft_check(["cobblestone", "cobblestone", "cobblestone", "", "stick", "", "", "stick", ""], 3, "stone_pickaxe", 1)
	_craft_check(["iron_ingot", "", "iron_ingot", "iron_ingot", "iron_ingot", "iron_ingot", "iron_ingot", "iron_ingot", "iron_ingot"], 3, "iron_chestplate", 1)
	_craft_check(["", "", "", "", "oak_planks", "oak_planks", "", "oak_planks", "oak_planks"], 3, "crafting_table", 1)
	# loot tables must only reference existing items
	for t in LootDB.chest_tables:
		for e in LootDB.chest_tables[t]["items"]:
			if not ItemDB.has(String(e[0])) and not String(e[0]).begins_with("@"):
				problems.append("loot table %s references missing item %s" % [t, e[0]])
	for m in LootDB.mob_tables:
		for e in LootDB.mob_tables[m]:
			if not String(e[0]).begins_with("@") and not ItemDB.has(String(e[0])):
				problems.append("mob loot %s references missing item %s" % [m, e[0]])
	info.append("%d chest loot tables, %d mob loot tables" % [LootDB.chest_tables.size(), LootDB.mob_tables.size()])


func _craft_check(names: Array, size: int, expect: String, count: int) -> void:
	var grid := []
	for n in names:
		var s := String(n)
		grid.append(null if s == "" else ItemStack.of(s, 1))
	var r := RecipeDB.find(grid, size)
	if r.is_empty():
		problems.append("recipe not found: %s" % expect)
		return
	var out: ItemStack = r.out
	if out.item_name() != expect or out.count != count:
		problems.append("recipe mismatch: %s expected %s x%d got %d x%d" % [names, expect, count, out.count, out.item_name()])
	if RecipeDB.recipes_for(expect).is_empty():
		problems.append("recipes_for missing output " + expect)
