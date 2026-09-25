class_name ItemDB
extends RefCounted
## Item registry: every block with an item form plus all ItemCatalog items. Lookups by id / name,
## creative tab ordering and helper queries used by crafting, combat and UI.

static var defs: Array = []          # Array[ItemDef]
static var by_name: Dictionary = {}
static var count := 0
static var block_item: PackedInt32Array   # block id -> item id (-1)
static var tab_items: Dictionary = {}     # tab -> PackedInt32Array
static var inited := false

const TABS := ["building", "colored", "natural", "functional", "redstone", "tools", "combat", "food", "ingredients", "spawn_eggs"]
const TAB_TITLES := {"building": "Building Blocks", "colored": "Colored Blocks", "natural": "Natural Blocks",
	"functional": "Functional Blocks", "redstone": "Redstone Blocks", "tools": "Tools & Utilities", "combat": "Combat",
	"food": "Food & Drinks", "ingredients": "Ingredients", "spawn_eggs": "Spawn Eggs", "search": "Search Items",
	"inventory": "Survival Inventory"}
const TAB_ICONS := {"building": "bricks", "colored": "cyan_wool", "natural": "grass_block", "functional": "crafting_table",
	"redstone": "redstone", "tools": "iron_pickaxe", "combat": "golden_sword", "food": "apple", "ingredients": "iron_ingot",
	"spawn_eggs": "creeper_spawn_egg", "search": "compass", "inventory": "chest"}


static func init() -> void:
	if inited:
		return
	defs.clear()
	by_name.clear()
	block_item = PackedInt32Array()
	block_item.resize(BlockDB.count)
	block_item.fill(-1)
	var cat := ItemCatalog.new()
	cat.build()
	var catalog_names := {}
	for e in cat.items:
		catalog_names[e.name] = e
	# block items first (block registry order -> creative ordering follows families)
	for d in BlockDB.defs:
		var bd: BlockDef = d
		if not bd.has_item or bd.id == 0:
			continue
		if catalog_names.has(bd.name):
			continue
		var it := ItemDef.new()
		it.name = bd.name
		it.display = bd.display
		it.kind = "block"
		it.block = bd.name
		it.tabs = bd.tabs
		if bd.tags.has("logs") or bd.tags.has("planks") or (bd.flammable and bd.tool == "axe"):
			it.fuel = 300
		if bd.name.ends_with("_slab") and bd.flammable:
			it.fuel = 150
		if bd.name == "coal_block":
			it.fuel = 16000
		if bd.name == "dried_kelp_block":
			it.fuel = 4000
		if bd.tags.has("saplings"):
			it.fuel = 100
		if bd.name.ends_with("_bed") or bd.name.ends_with("_shulker_box") or bd.name == "shulker_box":
			it.max_stack = 1
		if bd.name in ["dragon_egg", "beacon", "heavy_core"]:
			it.rarity = 2 if bd.name != "heavy_core" else 3
		if bd.name in ["spawner", "trial_spawner", "vault", "reinforced_deepslate", "barrier", "end_portal_frame"]:
			it.rarity = 2
		var ic: String = bd.icon
		if ic.begins_with("texture:"):
			it.icon = "blocktex:" + ic.substr(8)
		elif ic.begins_with("item:"):
			it.icon = "sprite:" + ic.substr(5)
		elif ic == "model":
			it.icon = "model:" + bd.name
		else:
			it.icon = "block:" + bd.name
		_register(it)
	for e in cat.items:
		var it2 := ItemDef.new()
		for k in e:
			if k == "tabs":
				it2.tabs = PackedStringArray(e[k])
			else:
				it2.set(k, e[k])
		it2.display = BlockCatalog.pretty(it2.name)
		_register(it2)
	count = defs.size()
	_rebuild_tabs()
	inited = true


static func _register(it: ItemDef) -> void:
	if by_name.has(it.name):
		return
	it.id = defs.size()
	defs.append(it)
	by_name[it.name] = it.id
	if it.block != "":
		var bid := BlockDB.id(it.block)
		if bid > 0 and block_item[bid] < 0:
			block_item[bid] = it.id
	count = defs.size()


static func register_spawn_egg(mob: String, c1: Color, c2: Color, display: String = "") -> void:
	var it := ItemDef.new()
	it.name = mob + "_spawn_egg"
	it.display = (display if display != "" else BlockCatalog.pretty(mob)) + " Spawn Egg"
	it.kind = "spawn_egg"
	it.tabs = PackedStringArray(["spawn_eggs"])
	it.use = "spawn_egg"
	it.icon = "egg:" + mob
	it.color = c1
	it.color2 = c2
	it.props = {"mob": mob}
	_register(it)


static func rebuild_tabs() -> void:
	_rebuild_tabs()


static func _rebuild_tabs() -> void:
	tab_items.clear()
	var lists := {}
	for t in TABS:
		lists[t] = []
	for it in defs:
		for t in (it as ItemDef).tabs:
			if lists.has(t):
				(lists[t] as Array).append((it as ItemDef).id)
	for t in TABS:
		tab_items[t] = PackedInt32Array(lists[t])


static func id(n: String) -> int:
	return by_name.get(n, -1)


static func has(n: String) -> bool:
	return by_name.has(n)


static func def(i: int) -> ItemDef:
	if i < 0 or i >= defs.size():
		return null
	return defs[i]


static func get_by_name(n: String) -> ItemDef:
	var i: int = by_name.get(n, -1)
	return defs[i] if i >= 0 else null


static func for_block(block_value: int) -> int:
	var bid := block_value & 0xFFF
	if bid < 0 or bid >= block_item.size():
		return -1
	var d: BlockDef = BlockDB.defs[bid]
	if block_item[bid] < 0 and d.props.has("item"):
		return id(String(d.props["item"]))
	return block_item[bid]


static func block_of(item_id: int) -> int:
	var d := def(item_id)
	if d == null or d.block == "":
		return -1
	return BlockDB.id(d.block)


## Items matching a search string (case-insensitive substring over display names and ids).
static func search(q: String) -> PackedInt32Array:
	var out := PackedInt32Array()
	var ql := q.strip_edges().to_lower()
	for it in defs:
		var d: ItemDef = it
		if d.tabs.is_empty():
			continue
		if ql == "" or d.display.to_lower().contains(ql) or d.name.contains(ql.replace(" ", "_")):
			out.append(d.id)
	return out


static func rarity_color(r: int) -> Color:
	match r:
		1: return Color(1.0, 1.0, 0.33)
		2: return Color(0.33, 1.0, 1.0)
		3: return Color(1.0, 0.33, 1.0)
	return Color(1, 1, 1)
