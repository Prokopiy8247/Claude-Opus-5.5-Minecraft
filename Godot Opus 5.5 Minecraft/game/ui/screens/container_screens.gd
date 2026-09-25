class_name ContainerScreens
extends RefCounted
## Block-entity backed screens: chests (single/double), barrels, shulker boxes, ender chest,
## dispensers/droppers, hoppers, furnaces (+ blast furnace / smoker) and brewing stands.

## Inventory view over one or more block-entity item arrays, written back on every change.
class BEInventory extends RefCounted:
	var world: World
	var positions: Array = []
	var sizes: Array = []
	var inv: Inventory

	func _init(w: World, p_positions: Array, per := 27) -> void:
		world = w
		positions = p_positions
		var total := 0
		for p in positions:
			var pv: Vector3i = p
			var be: Dictionary = world.get_be(pv.x, pv.y, pv.z, true)
			if String(be.get("loot", "")) != "":
				# structure loot is rolled the first time the container is opened
				var rng := RandomNumberGenerator.new()
				rng.seed = hash(pv) ^ world.seed_value
				be["items"] = LootDB.roll_chest(String(be["loot"]), rng, per)
				be.erase("loot")
				world.mark_modified(pv.x, pv.z)
			if not be.has("items") or (be["items"] as Array).is_empty():
				var arr := []
				arr.resize(per)
				for i in per:
					arr[i] = {}
				be["items"] = arr
			var n: int = (be["items"] as Array).size()
			sizes.append(n)
			total += n
		inv = Inventory.new(total)
		var off := 0
		for pi in positions.size():
			var pv2: Vector3i = positions[pi]
			var items: Array = world.get_be(pv2.x, pv2.y, pv2.z)["items"]
			for i in items.size():
				inv.slots[off + i] = ItemStack.from_dict(items[i]) if items[i] is Dictionary else null
			off += int(sizes[pi])
		inv.changed.connect(write_back)

	func write_back() -> void:
		var off := 0
		for pi in positions.size():
			var pv: Vector3i = positions[pi]
			var be: Dictionary = world.get_be(pv.x, pv.y, pv.z, true)
			var arr := []
			for i in int(sizes[pi]):
				var st := inv.stack_at(off + i)
				arr.append(st.to_dict() if st != null else {})
			be["items"] = arr
			world.mark_modified(pv.x, pv.z)
			off += int(sizes[pi])


# ================================================================================================
class ChestScreen extends ScreenBase:
	var be_inv: BEInventory = null
	var rows := 3
	var positions := []
	var ender := false

	func _build(ctx: Dictionary) -> void:
		ender = bool(ctx.get("ender", false))
		title = String(ctx.get("title", "Chest"))
		var inv: Inventory
		if ender:
			inv = session.ender_inventory
			positions = [ctx.get("pos", Vector3i.ZERO)]
		else:
			positions = ctx.get("positions", [ctx.get("pos", Vector3i.ZERO)])
			be_inv = BEInventory.new(player.world, positions)
			inv = be_inv.inv
		rows = int(ceil(inv.size() / 9.0))
		win = Vector2i(176, 114 + rows * 18)
		for r in rows:
			for c in 9:
				var idx := r * 9 + c
				if idx < inv.size():
					add_slot(inv, idx, 8 + c * 18, 18 + r * 18, "normal", "container")
		add_player_inventory(8, 32 + rows * 18)
		Sfx.play_at("chest_open", Vector3(positions[0]) + Vector3(0.5, 0.5, 0.5), 0.6)

	func accepts(i: int, st: ItemStack) -> bool:
		if slots[i].section == "container" and title.to_lower().contains("shulker") and st.item_name().ends_with("shulker_box"):
			return false
		return super(i, st)

	func on_close() -> void:
		if be_inv != null:
			be_inv.write_back()
		session.chest_opened(player.world, positions, false)
		Sfx.play_at("chest_close", Vector3(positions[0]) + Vector3(0.5, 0.5, 0.5), 0.6)
		var w: World = player.world
		for p in positions:
			var pv: Vector3i = p
			var v := w.get_blockv(pv)
			if BlockDB.name_of(v) == "barrel":
				w.set_blockv(pv, Vox.make(v & 0xFFF, ((v >> 12) & 7)), World.F_URGENT)


class DispenserScreen extends ScreenBase:
	var be_inv: BEInventory

	func _build(ctx: Dictionary) -> void:
		title = String(ctx.get("title", "Dispenser"))
		win = Vector2i(176, 166)
		be_inv = BEInventory.new(player.world, [ctx.get("pos", Vector3i.ZERO)], 9)
		for r in 3:
			for c in 3:
				add_slot(be_inv.inv, r * 3 + c, 62 + c * 18, 17 + r * 18, "normal", "container")
		add_player_inventory(8, 84)

	func on_close() -> void:
		be_inv.write_back()


class HopperScreen extends ScreenBase:
	var be_inv: BEInventory

	func _build(ctx: Dictionary) -> void:
		title = "Item Hopper"
		win = Vector2i(176, 133)
		be_inv = BEInventory.new(player.world, [ctx.get("pos", Vector3i.ZERO)], 5)
		for c in 5:
			add_slot(be_inv.inv, c, 44 + c * 18, 20, "normal", "container")
		add_player_inventory(8, 51)

	func on_close() -> void:
		be_inv.write_back()


# ================================================================================================
class FurnaceScreen extends ScreenBase:
	var be_inv: BEInventory
	var pos := Vector3i.ZERO
	var kind := "furnace"

	func _build(ctx: Dictionary) -> void:
		pos = ctx.get("pos", Vector3i.ZERO)
		kind = String(ctx.get("kind", "furnace"))
		title = BlockCatalog.pretty(kind)
		win = Vector2i(176, 166)
		be_inv = BEInventory.new(player.world, [pos], 3)
		var be: Dictionary = player.world.get_be(pos.x, pos.y, pos.z, true)
		be["type"] = "furnace"
		be["kind"] = kind
		session.register_ticking_be(player.world, pos)
		add_slot(be_inv.inv, 0, 56, 17, "normal", "container")
		add_slot(be_inv.inv, 1, 56, 53, "fuel", "container")
		add_slot(be_inv.inv, 2, 116, 35, "furnace_out", "container")
		add_player_inventory(8, 84)

	func accepts(i: int, st: ItemStack) -> bool:
		var k: String = slots[i].kind
		if k == "fuel":
			return RecipeDB.fuel_ticks(st) > 0 or st.item_name() == "bucket"
		if k == "furnace_out":
			return false
		return super(i, st)

	func _click(i: int, button: int, shift: bool, dbl: bool) -> void:
		if slots[i].kind == "furnace_out":
			var st := slot_stack(i)
			if st != null and (carried == null or (carried.can_merge(st) and carried.count + st.count <= carried.max_stack())):
				var be: Dictionary = player.world.get_be(pos.x, pos.y, pos.z, true)
				var xp: float = float(be.get("xp", 0.0))
				if xp > 0.0:
					session.entities.spawn_xp(player.world, player.body.pos + Vector3(0, 1, 0), maxi(1, int(round(xp))))
					be["xp"] = 0.0
				if shift:
					var rem = player.inventory.add(st, 0, 36)
					set_slot_stack(i, rem)
				elif carried == null:
					carried = st
					set_slot_stack(i, null)
				else:
					carried.count += st.count
					set_slot_stack(i, null)
			return
		super(i, button, shift, dbl)

	func quick_move(i: int) -> void:
		var st := slot_stack(i)
		if st == null:
			return
		var sec: String = slots[i].section
		if sec == "main" or sec == "hotbar":
			var target := -1
			if RecipeDB.smelt_result(st.item_name(), _smelt_kind()).size() > 0:
				target = 0
			elif RecipeDB.fuel_ticks(st) > 0:
				target = 1
			if target >= 0:
				var cur := slot_stack(target)
				if cur == null:
					set_slot_stack(target, st)
					set_slot_stack(i, null)
					return
				elif cur.can_merge(st):
					var n := mini(cur.max_stack() - cur.count, st.count)
					cur.count += n
					st.count -= n
					set_slot_stack(target, cur)
					set_slot_stack(i, st if st.count > 0 else null)
					return
		super(i)

	func _smelt_kind() -> String:
		match kind:
			"blast_furnace":
				return "blast"
			"smoker":
				return "smoker"
		return "furnace"

	func draw_background() -> void:
		super()
		var be: Dictionary = player.world.get_be(pos.x, pos.y, pos.z)
		var burn := float(be.get("burn", 0))
		var burn_max := maxf(1.0, float(be.get("burn_max", 1)))
		var cook := float(be.get("cook", 0))
		var cook_total := maxf(1.0, float(be.get("cook_total", 200)))
		# flame
		var fp := origin + Vector2(57, 37) * s
		draw_rect(Rect2(fp, Vector2(14, 14) * s), Color8(139, 139, 139))
		if burn > 0:
			var h := ceili(14.0 * burn / burn_max)
			draw_rect(Rect2(fp + Vector2(0, 14 - h) * s, Vector2(14, h) * s), Color8(255, 150, 20))
			draw_rect(Rect2(fp + Vector2(4, 14 - h) * s, Vector2(6, maxi(0, h - 2)) * s), Color8(255, 220, 60))
		# arrow
		var ap := origin + Vector2(79, 34) * s
		draw_rect(Rect2(ap + Vector2(0, 5) * s, Vector2(16, 7) * s), Color8(139, 139, 139))
		for k in 8:
			draw_rect(Rect2(ap + Vector2(15 + k, k) * s, Vector2(1, 17 - 2 * k) * s), Color8(139, 139, 139))
		var w := int(24.0 * cook / cook_total)
		if w > 0:
			draw_rect(Rect2(ap + Vector2(0, 5) * s, Vector2(mini(w, 16), 7) * s), Color8(255, 255, 255))
			for k in maxi(0, w - 15):
				draw_rect(Rect2(ap + Vector2(15 + k, k) * s, Vector2(1, 17 - 2 * k) * s), Color8(255, 255, 255))

	func on_close() -> void:
		be_inv.write_back()


# ================================================================================================
class BrewingScreen extends ScreenBase:
	var be_inv: BEInventory
	var pos := Vector3i.ZERO

	func _build(ctx: Dictionary) -> void:
		pos = ctx.get("pos", Vector3i.ZERO)
		title = "Brewing Stand"
		win = Vector2i(176, 166)
		be_inv = BEInventory.new(player.world, [pos], 5)
		var be: Dictionary = player.world.get_be(pos.x, pos.y, pos.z, true)
		be["type"] = "brewing"
		session.register_ticking_be(player.world, pos)
		add_slot(be_inv.inv, 0, 56, 51, "bottle", "container")
		add_slot(be_inv.inv, 1, 79, 58, "bottle", "container")
		add_slot(be_inv.inv, 2, 102, 51, "bottle", "container")
		add_slot(be_inv.inv, 3, 79, 17, "ingredient", "container")
		add_slot(be_inv.inv, 4, 17, 17, "blaze", "container")
		add_player_inventory(8, 84)

	func accepts(i: int, st: ItemStack) -> bool:
		match String(slots[i].kind):
			"bottle":
				return st.item_name() in ["potion", "splash_potion", "lingering_potion", "glass_bottle"]
			"ingredient":
				return EffectDB.is_brewing_ingredient(st.item_name())
			"blaze":
				return st.item_name() == "blaze_powder"
		return super(i, st)

	func slot_limit(i: int, st: ItemStack) -> int:
		if slots[i].kind == "bottle":
			return 1
		return super(i, st)

	func draw_background() -> void:
		super()
		var be: Dictionary = player.world.get_be(pos.x, pos.y, pos.z)
		var t := int(be.get("brew", 0))
		var fuel := int(be.get("fuel", 0))
		# fuel bar
		draw_rect(Rect2(origin + Vector2(60, 44) * s, Vector2(18, 4) * s), Color8(60, 60, 60))
		draw_rect(Rect2(origin + Vector2(60, 44) * s, Vector2(int(18.0 * fuel / 20.0), 4) * s), Color8(240, 160, 40))
		# progress arrow (down)
		draw_rect(Rect2(origin + Vector2(97, 16) * s, Vector2(9, 28) * s), Color8(139, 139, 139))
		if t > 0:
			var h := int(28.0 * (1.0 - t / 400.0))
			draw_rect(Rect2(origin + Vector2(97, 16) * s, Vector2(9, h) * s), Color8(255, 255, 255))
		# bubbles
		if t > 0:
			var bh := int(fmod(t / 2.0, 29.0))
			draw_rect(Rect2(origin + Vector2(65, 14 + 29 - bh) * s, Vector2(12, bh) * s), Color8(180, 220, 255, 200))

	func on_close() -> void:
		be_inv.write_back()
