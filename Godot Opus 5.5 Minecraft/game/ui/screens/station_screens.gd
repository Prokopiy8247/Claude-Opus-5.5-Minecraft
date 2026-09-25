class_name StationScreens
extends RefCounted
## Enchanting table, anvil, grindstone, stonecutter, smithing table and beacon screens.

static func _return_items(inv: Inventory, player, session, count: int) -> void:
	for i in count:
		var st := inv.stack_at(i)
		if st != null:
			var rem = player.inventory.add(st, 0, 36)
			if rem != null:
				session.drop_item_from_player(rem)
			inv.slots[i] = null


# ================================================================================================
class EnchantingScreen extends ScreenBase:
	var inv := Inventory.new(2)
	var pos := Vector3i.ZERO
	var offers: Array = []
	var shelves := 0
	var hover_opt := -1

	func _build(ctx: Dictionary) -> void:
		pos = ctx.get("pos", Vector3i.ZERO)
		title = "Enchant"
		win = Vector2i(176, 166)
		add_slot(inv, 0, 15, 47, "ench_item", "container")
		add_slot(inv, 1, 35, 47, "lapis", "container")
		add_player_inventory(8, 84)
		shelves = _count_shelves()

	func _count_shelves() -> int:
		var w: World = player.world
		var n := 0
		for dx in range(-2, 3):
			for dz in range(-2, 3):
				if absi(dx) < 2 and absi(dz) < 2:
					continue
				for dy in range(0, 2):
					if BlockDB.name_of(w.get_blockv(pos + Vector3i(dx, dy, dz))) == "bookshelf":
						# the space between must be air
						var mid := pos + Vector3i(signi(dx), dy, signi(dz))
						if w.get_blockv(mid) == 0:
							n += 1
		return mini(n, 15)

	func accepts(i: int, st: ItemStack) -> bool:
		if slots[i].kind == "lapis":
			return st.item_name() == "lapis_lazuli"
		if slots[i].kind == "ench_item":
			return st.item().enchantability > 0 or st.item_name() == "book"
		return super(i, st)

	func slot_limit(i: int, st: ItemStack) -> int:
		if slots[i].kind == "ench_item":
			return 1
		return super(i, st)

	func on_slot_changed(i: int) -> void:
		if slots[i].kind == "ench_item":
			_refresh()

	func _refresh() -> void:
		offers = []
		var st := inv.stack_at(0)
		if st == null or not st.enchantments().is_empty():
			return
		var it := st.item()
		offers = EnchantDB.table_offers(it, shelves, session.enchant_seed + st.id * 31)

	func _opt_rect(k: int) -> Rect2:
		return Rect2(origin + Vector2(60, 14 + k * 19) * s, Vector2(108, 19) * s)

	func handle_click(mb: InputEventMouseButton) -> bool:
		if mb.button_index != MOUSE_BUTTON_LEFT:
			return false
		for k in 3:
			if _opt_rect(k).has_point(mb.position):
				_enchant(k)
				return true
		return false

	func _enchant(k: int) -> void:
		if k >= offers.size() or (offers[k] as Dictionary).is_empty():
			return
		var o: Dictionary = offers[k]
		var st := inv.stack_at(0)
		var lapis := inv.stack_at(1)
		var need := int(o.lapis)
		var creative: bool = player.is_creative()
		if not creative:
			if player.stats.xp_level < int(o.cost) or lapis == null or lapis.count < need:
				return
			player.stats.add_levels(-need)
			lapis.count -= need
			inv.set_stack(1, lapis if lapis.count > 0 else null)
		EnchantDB.apply(st, o.list)
		inv.set_stack(0, st)
		session.enchant_seed = randi()
		Sfx.play_at("enchant", Vector3(pos) + Vector3(0.5, 1.0, 0.5), 0.8)
		_refresh()

	func _process(delta: float) -> void:
		super(delta)
		hover_opt = -1
		for k in 3:
			if _opt_rect(k).has_point(mouse):
				hover_opt = k

	func draw_background() -> void:
		super()
		for k in 3:
			var r := _opt_rect(k)
			var has: bool = k < offers.size() and not (offers[k] as Dictionary).is_empty()
			var col := Color8(141, 118, 84) if not has else (Color8(152, 116, 196) if hover_opt == k else Color8(106, 85, 128))
			draw_rect(r, Color8(0, 0, 0))
			draw_rect(r.grow(-s), col)
			if has:
				var o: Dictionary = offers[k]
				var p: Array = o.preview
				var name := EnchantDB.display(String(p[0]), int(p[1])) + " . . . ?"
				var ok: bool = player.is_creative() or player.stats.xp_level >= int(o.cost)
				PixelUI.text(self, r.position + Vector2(20, 3) * s, name, Color8(104, 90, 70) if not ok else Color8(240, 230, 190), s, false)
				PixelUI.text(self, r.position + Vector2(108 - 4 - PixelUI.text_width(str(o.cost)), 10) * s, str(o.cost),
					PixelUI.TEXT_GREEN if ok else Color8(64, 128, 32), s)
				PixelUI.text(self, r.position + Vector2(4, 5) * s, str(o.lapis), PixelUI.TEXT_AQUA, s)
		PixelUI.text(self, origin + Vector2(12, 67) * s, "Shelves: %d" % shelves, Color8(64, 64, 64), s, false)

	func on_close() -> void:
		StationScreens._return_items(inv, player, session, 2)


# ================================================================================================
class AnvilScreen extends ScreenBase:
	var inv := Inventory.new(3)
	var pos := Vector3i.ZERO
	var cost := 0
	var repair_units := 0
	var name_text := ""
	var name_edit: LineEdit = null

	func _build(ctx: Dictionary) -> void:
		pos = ctx.get("pos", Vector3i.ZERO)
		title = "Repair & Name"
		win = Vector2i(176, 166)
		add_slot(inv, 0, 27, 47, "normal", "container")
		add_slot(inv, 1, 76, 47, "normal", "container")
		add_slot(inv, 2, 134, 47, "output", "container")
		add_player_inventory(8, 84)
		name_edit = LineEdit.new()
		name_edit.flat = true
		name_edit.add_theme_font_override("font", PixelUI.get_font())
		name_edit.text_changed.connect(_on_name)
		add_child(name_edit)

	func _on_name(t: String) -> void:
		name_text = t
		_update()

	func tick_screen() -> void:
		if name_edit != null:
			name_edit.position = origin + Vector2(60, 20) * s
			name_edit.size = Vector2(106, 16) * s
			name_edit.add_theme_font_size_override("font_size", 8 * s)

	func on_slot_changed(i: int) -> void:
		if i <= 1:
			var st := inv.stack_at(0)
			if st != null and name_text == "":
				name_edit.text = st.display_name()
				name_text = name_edit.text
			_update()

	func _update() -> void:
		var a := inv.stack_at(0)
		var b := inv.stack_at(1)
		cost = 0
		repair_units = 0
		inv.slots[2] = null
		if a == null:
			return
		var out := a.copy()
		var changed := false
		if b != null:
			var it := a.item()
			var repair_item := String(it.props.get("repair", ""))
			if b.id == a.id and it.is_damageable():
				var rem := (it.durability - a.damage) + (it.durability - b.damage) + it.durability * 12 / 100
				out.damage = maxi(0, it.durability - rem)
				cost += 2
				changed = true
				for e in b.enchantments():
					var lvl := maxi(out.enchant_level(String(e)), int(b.enchantments()[e]))
					if out.enchant_level(String(e)) == int(b.enchantments()[e]):
						lvl = mini(lvl + 1, int(EnchantDB.ENCH.get(String(e), [lvl])[0]))
					var ench: Dictionary = out.data.get("ench", {}).duplicate()
					ench[String(e)] = lvl
					out.data["ench"] = ench
					cost += lvl
			elif repair_item != "" and b.item_name() == repair_item and a.damage > 0:
				var per := maxi(1, it.durability / 4)
				var n := mini(b.count, int(ceil(float(a.damage) / per)))
				out.damage = maxi(0, a.damage - per * n)
				cost += n
				repair_units = n
				changed = true
			elif b.item_name() == "enchanted_book":
				for e in b.enchantments():
					if EnchantDB.applies(String(e), it):
						var ench2: Dictionary = out.data.get("ench", {}).duplicate()
						ench2[String(e)] = maxi(out.enchant_level(String(e)), int(b.enchantments()[e]))
						out.data["ench"] = ench2
						cost += int(b.enchantments()[e]) * 2
						changed = true
		if name_text != "" and name_text != a.display_name():
			out.data["name"] = name_text
			cost += 1
			changed = true
		if changed:
			inv.slots[2] = out
		inv.changed.emit()

	func take_output(i: int) -> ItemStack:
		var out := inv.stack_at(2)
		if out == null:
			return null
		if not player.is_creative():
			if player.stats.xp_level < cost:
				return null
			player.stats.add_levels(-cost)
		inv.slots[0] = null
		var b := inv.stack_at(1)
		if b != null:
			if repair_units > 0:
				b.count -= repair_units
			else:
				b.count -= 1 if b.item_name() != out.item_name() else b.count
			inv.slots[1] = b if b.count > 0 else null
		repair_units = 0
		inv.slots[2] = null
		Sfx.play_at("anvil_use", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.7)
		_update()
		return out

	func draw_background() -> void:
		super()
		if cost > 0:
			var ok: bool = player.is_creative() or player.stats.xp_level >= cost
			PixelUI.text(self, origin + Vector2(60, 69) * s, "Enchantment Cost: %d" % cost, PixelUI.TEXT_GREEN if ok else PixelUI.TEXT_RED, s)
		draw_rect(Rect2(origin + Vector2(59, 19) * s, Vector2(110, 18) * s), Color8(0, 0, 0))

	func on_close() -> void:
		inv.slots[2] = null
		StationScreens._return_items(inv, player, session, 2)


# ================================================================================================
class GrindstoneScreen extends ScreenBase:
	var inv := Inventory.new(3)

	func _build(_ctx: Dictionary) -> void:
		title = "Repair & Disenchant"
		win = Vector2i(176, 166)
		add_slot(inv, 0, 49, 19, "normal", "container")
		add_slot(inv, 1, 49, 40, "normal", "container")
		add_slot(inv, 2, 129, 34, "output", "container")
		add_player_inventory(8, 84)

	func on_slot_changed(i: int) -> void:
		if i <= 1:
			var a := inv.stack_at(0)
			var b := inv.stack_at(1)
			inv.slots[2] = null
			var src := a if a != null else b
			if src != null and (a == null or b == null or a.id == b.id):
				var out := src.copy()
				var ench: Dictionary = out.data.get("ench", {}).duplicate()
				for e in ench.keys():
					if not EnchantDB.is_curse(String(e)):
						ench.erase(e)
				out.data["ench"] = ench
				if ench.is_empty():
					out.data.erase("ench")
				if out.item_name() == "enchanted_book" and ench.is_empty():
					out = ItemStack.of("book", 1)
				if a != null and b != null and out.item().is_damageable():
					var d := out.item().durability
					out.damage = maxi(0, d - ((d - a.damage) + (d - b.damage) + d * 5 / 100))
				inv.slots[2] = out
			inv.changed.emit()

	func take_output(_i: int) -> ItemStack:
		var out := inv.stack_at(2)
		if out == null:
			return null
		var xp := 0
		for st in [inv.stack_at(0), inv.stack_at(1)]:
			if st != null:
				for e in (st as ItemStack).enchantments():
					if not EnchantDB.is_curse(String(e)):
						xp += int((st as ItemStack).enchantments()[e]) * 2
		if xp > 0:
			session.entities.spawn_xp(player.world, player.body.pos + Vector3(0, 1, 0), xp)
		inv.slots[0] = null
		inv.slots[1] = null
		inv.slots[2] = null
		inv.changed.emit()
		return out

	func on_close() -> void:
		inv.slots[2] = null
		StationScreens._return_items(inv, player, session, 2)


# ================================================================================================
class StonecutterScreen extends ScreenBase:
	var inv := Inventory.new(2)
	var options: Array = []
	var selected := -1
	var scroll := 0

	func _build(_ctx: Dictionary) -> void:
		title = "Stonecutter"
		win = Vector2i(176, 166)
		add_slot(inv, 0, 20, 33, "normal", "container")
		add_slot(inv, 1, 143, 33, "output", "container")
		add_player_inventory(8, 84)

	func on_slot_changed(i: int) -> void:
		if i == 0:
			var st := inv.stack_at(0)
			options = RecipeDB.stonecut.get(st.item_name(), []) if st != null else []
			selected = -1
			inv.slots[1] = null
			inv.changed.emit()

	func _opt_rect(k: int) -> Rect2:
		var kk := k - scroll * 4
		return Rect2(origin + Vector2(52 + (kk % 4) * 16, 15 + (kk / 4) * 18) * s, Vector2(16, 18) * s)

	func handle_click(mb: InputEventMouseButton) -> bool:
		if mb.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			scroll = mini(scroll + 1, maxi(0, int(ceil(options.size() / 4.0)) - 3))
			return true
		if mb.button_index == MOUSE_BUTTON_WHEEL_UP:
			scroll = maxi(0, scroll - 1)
			return true
		if mb.button_index != MOUSE_BUTTON_LEFT:
			return false
		for k in range(scroll * 4, mini(options.size(), scroll * 4 + 12)):
			if _opt_rect(k).has_point(mb.position):
				selected = k
				var o: Array = options[k]
				inv.slots[1] = ItemStack.of(String(o[0]), int(o[1]))
				inv.changed.emit()
				Sfx.play_ui("click", 0.4, 1.2)
				return true
		return false

	func take_output(_i: int) -> ItemStack:
		var out := inv.stack_at(1)
		var src := inv.stack_at(0)
		if out == null or src == null:
			return null
		src.count -= 1
		inv.slots[0] = src if src.count > 0 else null
		if src.count <= 0:
			options = []
			selected = -1
			inv.slots[1] = null
		else:
			inv.slots[1] = out.copy()
		inv.changed.emit()
		Sfx.play_ui("click", 0.4, 0.8)
		return out

	func draw_background() -> void:
		super()
		draw_rect(Rect2(origin + Vector2(51, 14) * s, Vector2(66, 56) * s), Color8(139, 139, 139))
		for k in range(scroll * 4, mini(options.size(), scroll * 4 + 12)):
			var r := _opt_rect(k)
			draw_rect(r, Color8(198, 198, 198) if k != selected else Color8(120, 160, 230))
			var o: Array = options[k]
			var st := ItemStack.of(String(o[0]), int(o[1]))
			PixelUI.item(self, r.position + Vector2(0, 1) * s, st, s)

	func on_close() -> void:
		inv.slots[1] = null
		StationScreens._return_items(inv, player, session, 1)


# ================================================================================================
class SmithingScreen extends ScreenBase:
	var inv := Inventory.new(4)

	func _build(_ctx: Dictionary) -> void:
		title = "Upgrade Gear"
		win = Vector2i(176, 166)
		add_slot(inv, 0, 8, 48, "normal", "container")
		add_slot(inv, 1, 26, 48, "normal", "container")
		add_slot(inv, 2, 44, 48, "normal", "container")
		add_slot(inv, 3, 98, 48, "output", "container")
		add_player_inventory(8, 84)

	func on_slot_changed(i: int) -> void:
		if i <= 2:
			inv.slots[3] = null
			var t := inv.stack_at(0)
			var b := inv.stack_at(1)
			var a := inv.stack_at(2)
			if t != null and b != null and a != null:
				for r in RecipeDB.smithing:
					if r[0] == t.item_name() and r[1] == b.item_name() and r[2] == a.item_name():
						var out := ItemStack.of(String(r[3]), 1)
						out.data = b.data.duplicate(true)
						out.damage = b.damage
						inv.slots[3] = out
			inv.changed.emit()

	func take_output(_i: int) -> ItemStack:
		var out := inv.stack_at(3)
		if out == null:
			return null
		for k in 3:
			var st := inv.stack_at(k)
			if st != null:
				st.count -= 1
				inv.slots[k] = st if st.count > 0 else null
		inv.slots[3] = null
		Sfx.play_ui("anvil_use", 0.6, 1.3)
		on_slot_changed(0)
		return out

	func draw_background() -> void:
		super()
		var p := origin + Vector2(66, 48) * s
		draw_rect(Rect2(p + Vector2(0, 5) * s, Vector2(16, 6) * s), Color8(139, 139, 139))
		for k in 8:
			draw_rect(Rect2(p + Vector2(15 + k, k) * s, Vector2(1, 16 - 2 * k) * s), Color8(139, 139, 139))

	func on_close() -> void:
		inv.slots[3] = null
		StationScreens._return_items(inv, player, session, 3)


# ================================================================================================
class BeaconScreen extends ScreenBase:
	var inv := Inventory.new(1)
	var pos := Vector3i.ZERO
	var levels := 0
	const POWERS := [["speed", "haste"], ["resistance", "jump_boost"], ["strength"], ["regeneration"]]

	func _build(ctx: Dictionary) -> void:
		pos = ctx.get("pos", Vector3i.ZERO)
		title = "Beacon"
		win = Vector2i(230, 219)
		add_slot(inv, 0, 136, 110, "payment", "container")
		add_player_inventory(36, 137)
		levels = BlockEntityTicker.beacon_levels(player.world, pos)

	func accepts(i: int, st: ItemStack) -> bool:
		if slots[i].kind == "payment":
			return st.item_name() in ["iron_ingot", "gold_ingot", "diamond", "emerald", "netherite_ingot"]
		return super(i, st)

	func slot_limit(i: int, st: ItemStack) -> int:
		return 1 if slots[i].kind == "payment" else super(i, st)

	func _btn(tier: int, k: int) -> Rect2:
		return Rect2(origin + Vector2(30 + k * 25, 22 + tier * 25) * s, Vector2(22, 22) * s)

	func handle_click(mb: InputEventMouseButton) -> bool:
		if mb.button_index != MOUSE_BUTTON_LEFT:
			return false
		for tier in 4:
			for k in (POWERS[tier] as Array).size():
				if _btn(tier, k).has_point(mb.position):
					if tier < levels and inv.stack_at(0) != null:
						var be: Dictionary = player.world.get_be(pos.x, pos.y, pos.z, true)
						be["effect"] = POWERS[tier][k]
						be["type"] = "beacon"
						session.register_ticking_be(player.world, pos)
						inv.slots[0] = null
						inv.changed.emit()
						Sfx.play_at("beacon_power", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.8)
					return true
		return false

	func draw_background() -> void:
		super()
		PixelUI.text(self, origin + Vector2(30, 8) * s, "Primary Power (pyramid %d)" % levels, Color8(64, 64, 64), s, false)
		var cur := String(player.world.get_be(pos.x, pos.y, pos.z).get("effect", ""))
		for tier in 4:
			for k in (POWERS[tier] as Array).size():
				var r := _btn(tier, k)
				var en := tier < levels
				var eff: String = POWERS[tier][k]
				PixelUI.button(self, r, "", s, eff == cur, not en)
				PixelUI.text_centered(self, r.position.x + r.size.x * 0.5, r.position.y + 7 * s, eff.substr(0, 3).to_upper(),
					EffectDB.color_of(eff), s)
		PixelUI.text(self, origin + Vector2(30, 115) * s, "Pay with an ingot or gem:", Color8(64, 64, 64), s, false)

	func on_close() -> void:
		StationScreens._return_items(inv, player, session, 1)
