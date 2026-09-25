class_name TradeScreen
extends Control
## Villager trading: every villager gets a profession (from its spawn position) with a fixed set
## of emerald trades; each offer can be used 8 times and restocks at the start of each day.
## Click an offer to trade with items from the inventory.

const PROFESSIONS := {
	"farmer": [["wheat", 20, "emerald", 1], ["emerald", 1, "bread", 6], ["potato", 26, "emerald", 1],
		["emerald", 1, "pumpkin_pie", 4], ["emerald", 3, "golden_carrot", 3]],
	"librarian": [["paper", 24, "emerald", 1], ["emerald", 9, "bookshelf", 1], ["book", 4, "emerald", 1],
		["emerald", 12, "enchanted_book", 1], ["emerald", 5, "name_tag", 1]],
	"armorer": [["coal", 15, "emerald", 1], ["emerald", 5, "iron_helmet", 1], ["emerald", 9, "iron_chestplate", 1],
		["iron_ingot", 4, "emerald", 1], ["emerald", 19, "diamond_chestplate", 1]],
	"weaponsmith": [["coal", 15, "emerald", 1], ["emerald", 3, "iron_axe", 1], ["emerald", 7, "iron_sword", 1],
		["flint", 24, "emerald", 1], ["emerald", 17, "diamond_sword", 1]],
	"toolsmith": [["coal", 15, "emerald", 1], ["emerald", 1, "stone_pickaxe", 1], ["emerald", 4, "iron_shovel", 1],
		["iron_ingot", 4, "emerald", 1], ["emerald", 18, "diamond_pickaxe", 1]],
	"cleric": [["rotten_flesh", 32, "emerald", 1], ["emerald", 1, "redstone", 2], ["emerald", 1, "lapis_lazuli", 1],
		["gold_ingot", 3, "emerald", 1], ["emerald", 5, "ender_pearl", 1]],
	"butcher": [["chicken", 14, "emerald", 1], ["emerald", 1, "cooked_porkchop", 6], ["porkchop", 7, "emerald", 1],
		["emerald", 1, "cooked_chicken", 8]],
	"fletcher": [["stick", 32, "emerald", 1], ["emerald", 1, "arrow", 16], ["flint", 26, "emerald", 1],
		["emerald", 2, "bow", 1], ["emerald", 3, "crossbow", 1]],
	"cartographer": [["paper", 24, "emerald", 1], ["emerald", 7, "map", 1], ["glass_pane", 11, "emerald", 1],
		["emerald", 5, "compass", 1]],
	"shepherd": [["white_wool", 18, "emerald", 1], ["emerald", 2, "shears", 1], ["emerald", 1, "red_wool", 1],
		["emerald", 3, "white_bed", 1]],
	"fisherman": [["string", 20, "emerald", 1], ["emerald", 1, "cooked_cod", 6], ["cod", 15, "emerald", 1],
		["emerald", 3, "fishing_rod", 1]],
	"leatherworker": [["leather", 6, "emerald", 1], ["emerald", 3, "leather_chestplate", 1], ["emerald", 2, "leather_helmet", 1],
		["emerald", 6, "saddle", 1]],
	"mason": [["clay_ball", 10, "emerald", 1], ["emerald", 1, "bricks", 10], ["stone", 20, "emerald", 1],
		["emerald", 1, "polished_andesite", 4], ["emerald", 1, "quartz_block", 1]],
}
const TRADER := [["emerald", 1, "oak_sapling", 1], ["emerald", 1, "cherry_sapling", 1], ["emerald", 2, "cactus", 1],
	["emerald", 1, "sugar_cane", 1], ["emerald", 3, "glowstone", 1], ["emerald", 5, "blue_ice", 1], ["emerald", 1, "pointed_dripstone", 2]]
const MAX_USES := 8

var ui = null
var session = null
var player = null
var villager: Mob = null
var offers: Array = []
var s := 2
var origin := Vector2.ZERO
var win := Vector2i(220, 170)
var _hover := -1


static func profession_of(m: Mob) -> String:
	if m.mob == "wandering_trader":
		return "wandering_trader"
	if not m.data.has("profession"):
		var keys := PROFESSIONS.keys()
		m.data["profession"] = String(keys[absi(hash(Vector3i(m.home.round()))) % keys.size()])
	return String(m.data["profession"])


static func offers_of(m: Mob) -> Array:
	var p := profession_of(m)
	return TRADER if p == "wandering_trader" else PROFESSIONS.get(p, [])


func _init() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	focus_mode = Control.FOCUS_ALL


func setup(p_ui, m: Mob) -> void:
	ui = p_ui
	session = p_ui.session
	player = session.player
	villager = m
	offers = offers_of(m)
	# restock once per in-game day
	var day := int(session.tick / 24000) if session.tick >= 0 else 0
	if int(m.data.get("restock_day", -1)) != day:
		m.data["restock_day"] = day
		m.data["uses"] = []
	var uses: Array = m.data.get("uses", [])
	while uses.size() < offers.size():
		uses.append(0)
	m.data["uses"] = uses
	Sfx.play_mob(m.mob, "ambient", m.body.pos, 0.6)


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)


func _process(_delta: float) -> void:
	s = ui.scale()
	origin = ((size - Vector2(win) * s) * 0.5).floor()
	if villager == null or not is_instance_valid(villager) or villager.dead \
			or villager.body.pos.distance_to(player.body.pos) > 8.0:
		ui.close_modal()
		return
	queue_redraw()


func _row(i: int) -> Rect2:
	return Rect2(origin + Vector2(8, 22 + i * 24) * s, Vector2(win.x - 16, 22) * s)


func _count(item: String) -> int:
	var id := ItemDB.id(item)
	return 0 if id < 0 else player.inventory.count_item(id)


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		_hover = -1
		for i in offers.size():
			if _row(i).has_point((event as InputEventMouseMotion).position):
				_hover = i
	if event is InputEventMouseButton and event.pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT:
		for i in offers.size():
			if _row(i).has_point((event as InputEventMouseButton).position):
				trade(i)
				accept_event()
				return


## Executes offer i if the player can pay and the offer is in stock. Returns true on success.
func trade(i: int) -> bool:
	var o: Array = offers[i]
	var uses: Array = villager.data.get("uses", [])
	if int(uses[i]) >= MAX_USES:
		session.action_bar("This trade is out of stock until tomorrow")
		return false
	if _count(String(o[0])) < int(o[1]):
		session.action_bar("You need %d x %s" % [int(o[1]), ItemDB.get_by_name(String(o[0])).display])
		return false
	player.inventory.remove_item(ItemDB.id(String(o[0])), int(o[1]), 36)
	var out := ItemStack.of(String(o[2]), int(o[3]))
	if String(o[2]) == "enchanted_book":
		var choices := ["sharpness", "efficiency", "protection", "unbreaking", "mending", "fortune", "power", "feather_falling"]
		var e := String(choices[randi() % choices.size()])
		out.data["ench"] = {e: randi_range(1, int(EnchantDB.ENCH.get(e, [3])[0]))}
	var rem = player.inventory.add(out, 0, 36)
	if rem != null:
		session.drop_item_from_player(rem)
	uses[i] = int(uses[i]) + 1
	villager.data["uses"] = uses
	session.entities.spawn_xp(villager.world, villager.body.pos + Vector3(0, 1, 0), randi_range(1, 3))
	session.particles.happy(villager.body.pos + Vector3(0, villager.body.height, 0), 6)
	Sfx.play_ui("pop", 0.5, 1.2)
	return true


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.06, 0.06, 0.08, 0.6))
	PixelUI.panel(self, Rect2(origin, Vector2(win) * s), s)
	var prof := profession_of(villager)
	var title := "Wandering Trader" if prof == "wandering_trader" else "Villager - " + prof.capitalize()
	PixelUI.text(self, origin + Vector2(8, 7) * s, title, Color8(64, 64, 64), s, false)
	var uses: Array = villager.data.get("uses", [])
	for i in offers.size():
		var o: Array = offers[i]
		var r := _row(i)
		var out_of_stock := i < uses.size() and int(uses[i]) >= MAX_USES
		var affordable := _count(String(o[0])) >= int(o[1])
		draw_rect(r, Color(1, 1, 1, 0.12) if i == _hover else Color(0, 0, 0, 0.06))
		PixelUI.slot(self, r.position + Vector2(2, 2) * s, s)
		PixelUI.item(self, r.position + Vector2(3, 3) * s, ItemStack.of(String(o[0]), int(o[1])), s)
		PixelUI.text(self, r.position + Vector2(26, 7) * s, "->", Color8(64, 64, 64), s, false)
		PixelUI.slot(self, r.position + Vector2(40, 2) * s, s)
		PixelUI.item(self, r.position + Vector2(41, 3) * s, ItemStack.of(String(o[2]), int(o[3])), s)
		var label := ItemDB.get_by_name(String(o[2])).display if ItemDB.has(String(o[2])) else String(o[2])
		var col := Color8(64, 64, 64) if affordable else Color8(140, 60, 60)
		if out_of_stock:
			label = "Out of stock"
			col = Color8(160, 40, 40)
		PixelUI.text(self, r.position + Vector2(64, 7) * s, label, col, s, false)
	PixelUI.text(self, origin + Vector2(8, win.y - 12) * s, "Click a trade   Esc: close", Color8(90, 90, 90), s, false)
