class_name CraftingScreens
extends RefCounted
## Survival inventory (2x2 crafting, armour, offhand, player preview) and crafting table (3x3).

class CraftLogic:
	var inv: Inventory
	var first := 0         # first grid slot index in inv
	var size := 2
	var result_idx := 4
	var recipe: Dictionary = {}

	func _init(p_inv: Inventory, p_first: int, p_size: int, p_result: int) -> void:
		inv = p_inv
		first = p_first
		size = p_size
		result_idx = p_result

	func grid() -> Array:
		var g := []
		for i in size * size:
			g.append(inv.stack_at(first + i))
		return g

	func update() -> void:
		var r := RecipeDB.find(grid(), size)
		recipe = r
		inv.slots[result_idx] = r.get("out", null)
		inv.changed.emit()

	## Consumes one set of ingredients; returns the crafted stack.
	func take() -> ItemStack:
		var out: ItemStack = inv.stack_at(result_idx)
		if out == null:
			return null
		for i in size * size:
			var st := inv.stack_at(first + i)
			if st == null:
				continue
			var rem := String(st.item().props.get("returns", ""))
			st.count -= 1
			if st.count <= 0:
				inv.slots[first + i] = null
				if rem != "" and st.item().kind == "bucket":
					inv.slots[first + i] = ItemStack.of(rem, 1)
			if st.item_name() == "milk_bucket" or st.item_name() == "water_bucket" or st.item_name() == "lava_bucket":
				inv.slots[first + i] = ItemStack.of("bucket", 1)
			if st.item_name() == "honey_bottle":
				inv.slots[first + i] = ItemStack.of("glass_bottle", 1)
		var got := out.copy()
		update()
		return got

	func return_items(player, session) -> void:
		for i in size * size:
			var st := inv.stack_at(first + i)
			if st != null:
				var rem = player.inventory.add(st, 0, 36)
				if rem != null:
					session.drop_item_from_player(rem)
				inv.slots[first + i] = null
		inv.slots[result_idx] = null


# ================================================================================================
class InventoryScreen extends ScreenBase:
	var craft: CraftLogic
	var preview: SubViewportContainer = null

	func _build(_ctx: Dictionary) -> void:
		win = Vector2i(176, 166)
		show_player_inventory = false
		var inv: Inventory = player.inventory
		craft = CraftLogic.new(inv, Inventory.CRAFT, 2, 45)
		for a in 4:
			add_slot(inv, Inventory.ARMOR + 3 - a, 8, 8 + a * 18, "armor%d" % (3 - a), "armor")
		add_slot(inv, Inventory.OFFHAND, 77, 62, "normal", "offhand")
		for r in 2:
			for c in 2:
				add_slot(inv, Inventory.CRAFT + r * 2 + c, 98 + c * 18, 18 + r * 18, "normal", "craft")
		add_slot(inv, 45, 154, 28, "output", "craft_out")
		add_player_inventory(8, 84)
		craft.update()
		preview = PlayerPreview.make(player)
		add_child(preview)

	func tick_screen() -> void:
		if preview != null:
			preview.position = origin + Vector2(26, 8) * s
			preview.size = Vector2(49, 70) * s
			PlayerPreview.look(preview, mouse - (preview.position + preview.size * 0.5))

	func on_slot_changed(i: int) -> void:
		if slots[i].section == "craft":
			craft.update()

	func take_output(_i: int) -> ItemStack:
		return craft.take()

	func draw_background() -> void:
		super()
		PixelUI.text(self, origin + Vector2(97, 6) * s, "Crafting", Color8(64, 64, 64), s, false)
		# player preview frame
		draw_rect(Rect2(origin + Vector2(25, 7) * s, Vector2(51, 72) * s), Color8(0, 0, 0))
		draw_rect(Rect2(origin + Vector2(26, 8) * s, Vector2(49, 70) * s), Color8(40, 40, 40))
		# crafting arrow
		_arrow(origin + Vector2(135, 29) * s, 1.0)
		# empty armour slot hints
		for a in 4:
			if player.inventory.stack_at(Inventory.ARMOR + 3 - a) == null:
				var hint: String = ["helmet", "chestplate", "leggings", "boots"][a]
				var tex := ItemIcons.icon(ItemDB.id("iron_" + hint))
				if tex != null:
					draw_texture_rect(tex, Rect2(origin + Vector2(8, 8 + a * 18) * s, Vector2(16, 16) * s), false, Color(0.2, 0.2, 0.2, 0.5))
		if player.inventory.stack_at(Inventory.OFFHAND) == null:
			var st := ItemIcons.icon(ItemDB.id("shield"))
			if st != null:
				draw_texture_rect(st, Rect2(origin + Vector2(77, 62) * s, Vector2(16, 16) * s), false, Color(0.2, 0.2, 0.2, 0.5))

	func _arrow(p: Vector2, frac: float) -> void:
		var c := Color8(139, 139, 139)
		draw_rect(Rect2(p + Vector2(0, 5) * s, Vector2(16, 6) * s), c)
		for k in 8:
			draw_rect(Rect2(p + Vector2(15 + k, k) * s, Vector2(1, 16 - 2 * k) * s), c)

	func on_close() -> void:
		craft.return_items(player, session)
		player.inventory.changed.emit()


# ================================================================================================
class CraftingTableScreen extends ScreenBase:
	var grid_inv := Inventory.new(10)
	var craft: CraftLogic

	func _build(_ctx: Dictionary) -> void:
		win = Vector2i(176, 166)
		title = "Crafting"
		craft = CraftLogic.new(grid_inv, 0, 3, 9)
		for r in 3:
			for c in 3:
				add_slot(grid_inv, r * 3 + c, 30 + c * 18, 17 + r * 18, "normal", "craft")
		add_slot(grid_inv, 9, 124, 35, "output", "craft_out")
		add_player_inventory(8, 84)

	func on_slot_changed(i: int) -> void:
		if slots[i].section == "craft":
			craft.update()

	func take_output(_i: int) -> ItemStack:
		return craft.take()

	func draw_background() -> void:
		super()
		var p := origin + Vector2(90, 35) * s
		var c := Color8(139, 139, 139)
		draw_rect(Rect2(p + Vector2(0, 5) * s, Vector2(16, 6) * s), c)
		for k in 8:
			draw_rect(Rect2(p + Vector2(15 + k, k) * s, Vector2(1, 16 - 2 * k) * s), c)
		# recipe book button
		PixelUI.button(self, Rect2(origin + Vector2(5, 34) * s, Vector2(20, 18) * s), "?", s, false)

	func handle_click(mb: InputEventMouseButton) -> bool:
		if mb.button_index == MOUSE_BUTTON_LEFT and Rect2(origin + Vector2(5, 34) * s, Vector2(20, 18) * s).has_point(mb.position):
			ui.open_recipe_book(self)
			return true
		return false

	func on_close() -> void:
		craft.return_items(player, session)


# ================================================================================================
## 3D player model preview inside the inventory (SubViewport with its own camera and light).
class PlayerPreview extends RefCounted:
	static func make(player) -> SubViewportContainer:
		var c := SubViewportContainer.new()
		c.stretch = true
		c.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var vp := SubViewport.new()
		vp.transparent_bg = true
		vp.own_world_3d = true
		vp.size = Vector2i(98, 140)
		vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		c.add_child(vp)
		var cam := Camera3D.new()
		cam.projection = Camera3D.PROJECTION_ORTHOGONAL
		cam.size = 2.4
		cam.position = Vector3(0, 1.0, 4)
		vp.add_child(cam)
		var rig := PlayerSkin.build_model()
		rig.name = "Rig"
		rig.rotation.y = PI
		vp.add_child(rig)
		var env := WorldEnvironment.new()
		var e := Environment.new()
		e.background_mode = Environment.BG_CLEAR_COLOR
		env.environment = e
		vp.add_child(env)
		c.set_meta("rig", rig)
		return c

	static func look(c: SubViewportContainer, rel: Vector2) -> void:
		var rig: Node3D = c.get_meta("rig", null)
		if rig == null:
			return
		rig.rotation.y = PI - clampf(rel.x / 200.0, -1.0, 1.0) * 0.8
		var parts := MobRenderer.parts_of(rig)
		var head: Node3D = parts.get("head", null)
		if head != null:
			head.rotation = Vector3(clampf(rel.y / 200.0, -1.0, 1.0) * 0.5, -clampf(rel.x / 200.0, -1.0, 1.0) * 0.6, 0)
