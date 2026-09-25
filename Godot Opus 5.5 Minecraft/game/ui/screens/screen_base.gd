class_name ScreenBase
extends Control
## Base class for all item screens: slot layout, Minecraft click semantics (pick/place/swap/split,
## right-click one, shift quick-move, number-key hotbar swap, Q drop, double-click collect,
## left/right drag distribution), tooltips and the carried cursor stack.

var ui = null            # UIRoot
var session = null
var player = null
var s := 2               # GUI scale
var win := Vector2i(176, 166)
var origin := Vector2.ZERO
var slots: Array = []    # Array of Dictionary {inv, idx, x, y, kind, section}
var hover := -1
var title := ""
var _drag := []
var _drag_button := 0
var _dragging := false
var _last_click_time := 0
var _last_click_slot := -1
var mouse := Vector2.ZERO
var show_player_inventory := true


func _init() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	focus_mode = Control.FOCUS_ALL


func setup(p_ui, ctx: Dictionary) -> void:
	ui = p_ui
	session = p_ui.session
	player = session.player
	_build(ctx)


## Subclasses create their slots here.
func _build(_ctx: Dictionary) -> void:
	pass


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	resized.connect(queue_redraw)


func _process(_delta: float) -> void:
	s = ui.scale() if ui != null else 2
	# centre the panel; screens with a tab strip above the window reserve room for it
	var pad_top := 30 if has_method("tab_strip_height") else 0
	var pad_bottom := 30 if _has_section("hotbar") and win.y <= 140 else 0
	var full := Vector2(win) * s + Vector2(0, (pad_top + pad_bottom) * s)
	origin = ((size - full) * 0.5).floor() + Vector2(0, pad_top * s)
	origin.x = maxf(origin.x, 4.0)
	queue_redraw()
	tick_screen()


## Per-frame hook for subclasses (progress arrows etc.).
func tick_screen() -> void:
	pass


# ------------------------------------------------------------------------------------------------
# slot helpers
func add_slot(inv: Inventory, idx: int, x: int, y: int, kind := "normal", section := "container") -> int:
	slots.append({"inv": inv, "idx": idx, "x": x, "y": y, "kind": kind, "section": section})
	return slots.size() - 1


func add_player_inventory(x: int, y: int) -> void:
	var inv: Inventory = player.inventory
	for r in 3:
		for c in 9:
			add_slot(inv, 9 + r * 9 + c, x + c * 18, y + r * 18, "normal", "main")
	for c in 9:
		add_slot(inv, c, x + c * 18, y + 58, "normal", "hotbar")


func slot_stack(i: int) -> ItemStack:
	var sl: Dictionary = slots[i]
	var inv: Inventory = sl.inv
	return inv.stack_at(int(sl.idx))


func set_slot_stack(i: int, st: ItemStack) -> void:
	var sl: Dictionary = slots[i]
	var inv: Inventory = sl.inv
	inv.set_stack(int(sl.idx), st)
	on_slot_changed(i)


## Override for filtering (armor slots, fuel, lapis, bottles...).
func accepts(i: int, st: ItemStack) -> bool:
	var kind: String = slots[i].kind
	if kind == "output" or kind == "catalog":
		return false
	if kind.begins_with("armor"):
		var a := int(kind.substr(5))
		var it := st.item()
		if it.props.get("elytra", false):
			return a == 2
		if it.armor_slot == a:
			return true
		return a == 3 and (it.name.ends_with("_head") or it.name.ends_with("_skull") or it.name == "carved_pumpkin")
	return true


func slot_limit(_i: int, st: ItemStack) -> int:
	return st.max_stack()


func on_slot_changed(_i: int) -> void:
	pass


## Output slot taken (crafting consumes ingredients etc.). Returns the stack actually taken.
func take_output(_i: int) -> ItemStack:
	var st := slot_stack(_i)
	if st == null:
		return null
	set_slot_stack(_i, null)
	return st


func slot_rect(i: int) -> Rect2:
	var sl: Dictionary = slots[i]
	return Rect2(origin + Vector2(int(sl.x) - 1, int(sl.y) - 1) * s, Vector2(18, 18) * s)


func slot_at(p: Vector2) -> int:
	for i in slots.size():
		if slot_rect(i).has_point(p):
			return i
	return -1


var carried: ItemStack:
	get:
		return ui.carried
	set(v):
		ui.carried = v if (v != null and not v.is_empty()) else null


# ------------------------------------------------------------------------------------------------
# input
func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		mouse = event.position
		hover = slot_at(mouse)
		if _dragging and hover >= 0 and not _drag.has(hover) and carried != null:
			var st := slot_stack(hover)
			if (st == null or st.can_merge(carried)) and accepts(hover, carried):
				_drag.append(hover)
		return
	if event is InputEventMouseButton:
		var mb: InputEventMouseButton = event
		mouse = mb.position
		hover = slot_at(mouse)
		if mb.pressed:
			if handle_click(mb):
				accept_event()
				return
			if hover < 0:
				if not Rect2(origin, Vector2(win) * s).has_point(mouse) and carried != null:
					# click outside the window drops the carried stack
					if mb.button_index == MOUSE_BUTTON_LEFT:
						_drop(carried)
						carried = null
					elif mb.button_index == MOUSE_BUTTON_RIGHT:
						_drop(carried.split(1))
						if carried != null and carried.is_empty():
							carried = null
				return
			if mb.button_index == MOUSE_BUTTON_LEFT or mb.button_index == MOUSE_BUTTON_RIGHT:
				if carried != null and not Input.is_key_pressed(KEY_SHIFT) and slots[hover].kind != "output" and slots[hover].kind != "catalog":
					_dragging = true
					_drag = [hover]
					_drag_button = mb.button_index
					return
				_click(hover, mb.button_index, Input.is_key_pressed(KEY_SHIFT), mb.double_click)
			elif mb.button_index == MOUSE_BUTTON_MIDDLE:
				_middle(hover)
		else:
			if _dragging and (mb.button_index == _drag_button):
				_dragging = false
				if _drag.size() <= 1:
					if not _drag.is_empty():
						_click(_drag[0], _drag_button, false, false)
				else:
					_distribute(_drag_button)
				_drag = []
		accept_event()


## Subclasses (creative tabs, buttons) may consume clicks first.
func handle_click(_mb: InputEventMouseButton) -> bool:
	return false


func _unhandled_key_input(event: InputEvent) -> void:
	if not (event is InputEventKey) or not event.pressed:
		return
	var k: InputEventKey = event
	if hover >= 0:
		if k.keycode >= KEY_1 and k.keycode <= KEY_9:
			_hotbar_swap(hover, k.keycode - KEY_1)
			get_viewport().set_input_as_handled()
			return
		if k.keycode == KEY_Q:
			var st := slot_stack(hover)
			if st != null and slots[hover].kind != "catalog":
				if k.ctrl_pressed:
					_drop(st)
					set_slot_stack(hover, null)
				else:
					_drop(st.split(1))
					set_slot_stack(hover, st if not st.is_empty() else null)
			get_viewport().set_input_as_handled()
			return
		if k.keycode == KEY_F:
			_hotbar_swap(hover, -1)


func _drop(st: ItemStack) -> void:
	if st == null or st.is_empty():
		return
	session.drop_item_from_player(st)


func _click(i: int, button: int, shift: bool, dbl: bool) -> void:
	var kind: String = slots[i].kind
	var st := slot_stack(i)
	Sfx.play_ui("click", 0.3, 1.3)
	if kind == "output":
		if shift:
			var guard := 0
			while guard < 64:
				guard += 1
				var out := slot_stack(i)
				if out == null or not player.inventory.can_fit(out, 0, 36):
					break
				var got := take_output(i)
				if got == null:
					break
				var rem = player.inventory.add(got, 0, 36)
				if rem != null:
					_drop(rem)
					break
			return
		if st == null:
			return
		if carried == null:
			carried = take_output(i)
		elif carried.can_merge(st) and carried.count + st.count <= carried.max_stack():
			var got2 := take_output(i)
			if got2 != null:
				carried.count += got2.count
		return
	if shift:
		quick_move(i)
		return
	if dbl and carried != null and _last_click_slot == i:
		_collect_all(carried)
		return
	_last_click_slot = i
	if button == MOUSE_BUTTON_LEFT:
		if carried == null:
			if st != null:
				carried = st
				set_slot_stack(i, null)
		elif st == null:
			if accepts(i, carried):
				var lim := slot_limit(i, carried)
				if carried.count <= lim:
					set_slot_stack(i, carried)
					carried = null
				else:
					set_slot_stack(i, carried.split(lim))
		elif st.can_merge(carried):
			var room := mini(slot_limit(i, st), st.max_stack()) - st.count
			var n := mini(room, carried.count)
			if n > 0:
				st.count += n
				carried.count -= n
				if carried.count <= 0:
					carried = null
				set_slot_stack(i, st)
		elif accepts(i, carried) and carried.count <= slot_limit(i, carried):
			var tmp := st
			set_slot_stack(i, carried)
			carried = tmp
	elif button == MOUSE_BUTTON_RIGHT:
		if carried == null:
			if st != null:
				var half := int(ceil(st.count / 2.0))
				carried = st.split(half)
				set_slot_stack(i, st if not st.is_empty() else null)
		elif st == null:
			if accepts(i, carried):
				set_slot_stack(i, carried.split(1))
				if carried != null and carried.is_empty():
					carried = null
		elif st.can_merge(carried):
			if st.count < mini(slot_limit(i, st), st.max_stack()):
				st.count += 1
				carried.count -= 1
				if carried.count <= 0:
					carried = null
				set_slot_stack(i, st)
		elif accepts(i, carried):
			var tmp2 := st
			set_slot_stack(i, carried)
			carried = tmp2


func _middle(i: int) -> void:
	if not player.is_creative() or carried != null:
		return
	var st := slot_stack(i)
	if st != null:
		carried = st.with_count(st.max_stack())


func _distribute(button: int) -> void:
	if carried == null:
		return
	var targets := []
	for i in _drag:
		var st := slot_stack(i)
		if (st == null or st.can_merge(carried)) and accepts(i, carried):
			targets.append(i)
	if targets.is_empty():
		return
	var per := 1 if button == MOUSE_BUTTON_RIGHT else maxi(1, carried.count / targets.size())
	for i in targets:
		if carried == null or carried.count <= 0:
			break
		var st2 := slot_stack(i)
		var room := carried.max_stack() - (st2.count if st2 != null else 0)
		var n := mini(mini(per, room), carried.count)
		if n <= 0:
			continue
		if st2 == null:
			set_slot_stack(i, carried.split(n))
		else:
			st2.count += n
			carried.count -= n
			set_slot_stack(i, st2)
	if carried != null and carried.count <= 0:
		carried = null


func _collect_all(target: ItemStack) -> void:
	for i in slots.size():
		if carried == null or carried.count >= carried.max_stack():
			return
		if slots[i].kind == "output" or slots[i].kind == "catalog":
			continue
		var st := slot_stack(i)
		if st != null and st.can_merge(target):
			var n := mini(st.count, carried.max_stack() - carried.count)
			carried.count += n
			st.count -= n
			set_slot_stack(i, st if st.count > 0 else null)


func _hotbar_swap(i: int, hot: int) -> void:
	var kind: String = slots[i].kind
	if kind == "catalog":
		var st0 := slot_stack(i)
		if st0 != null and hot >= 0:
			player.inventory.set_stack(hot, st0.with_count(st0.max_stack()))
		return
	var target := hot if hot >= 0 else Inventory.OFFHAND
	var a := slot_stack(i)
	var b: ItemStack = player.inventory.stack_at(target)
	if kind == "output":
		if b == null and a != null:
			player.inventory.set_stack(target, take_output(i))
		return
	if b != null and not accepts(i, b):
		return
	set_slot_stack(i, b)
	player.inventory.set_stack(target, a)


## Default shift-click: container <-> player inventory, hotbar <-> main, armour auto-equip.
func quick_move(i: int) -> void:
	var st := slot_stack(i)
	if st == null:
		return
	var sec: String = slots[i].section
	var dest_sections: Array
	if sec == "container" or sec == "craft" or sec == "armor" or sec == "offhand":
		dest_sections = ["hotbar", "main"] if sec == "container" else ["main", "hotbar"]
	elif sec == "main" or sec == "hotbar":
		dest_sections = ["container"]
		if not _has_section("container"):
			if st.item().kind == "armor" and _has_section("armor"):
				dest_sections = ["armor", ("hotbar" if sec == "main" else "main")]
			else:
				dest_sections = ["hotbar"] if sec == "main" else ["main"]
		else:
			dest_sections.append("hotbar" if sec == "main" else "main")
	var moving := st
	set_slot_stack(i, null)
	for ds in dest_sections:
		moving = _insert_into_section(moving, String(ds))
		if moving == null:
			break
	if moving != null:
		set_slot_stack(i, moving)


func _has_section(sec: String) -> bool:
	for sl in slots:
		if sl.section == sec:
			return true
	return false


func _insert_into_section(st: ItemStack, sec: String) -> ItemStack:
	# merge first
	for i in slots.size():
		if slots[i].section != sec:
			continue
		var cur := slot_stack(i)
		if cur != null and cur.can_merge(st) and accepts(i, st):
			var room := mini(slot_limit(i, cur), cur.max_stack()) - cur.count
			var n := mini(room, st.count)
			if n > 0:
				cur.count += n
				st.count -= n
				set_slot_stack(i, cur)
				if st.count <= 0:
					return null
	for i in slots.size():
		if slots[i].section != sec or slots[i].kind == "output":
			continue
		if slot_stack(i) == null and accepts(i, st):
			var lim := slot_limit(i, st)
			if st.count <= lim:
				set_slot_stack(i, st)
				return null
			set_slot_stack(i, st.split(lim))
	return st


# ------------------------------------------------------------------------------------------------
# drawing
func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.06, 0.06, 0.08, 0.72))
	draw_background()
	for i in slots.size():
		if slots[i].kind == "hidden":
			continue
		var r := slot_rect(i)
		var st := slot_stack(i)
		if _dragging and _drag.has(i):
			draw_rect(r.grow(-s), Color(1, 1, 1, 0.35))
		PixelUI.item(self, r.position + Vector2(s, s), st, s)
		if i == hover:
			draw_rect(Rect2(r.position + Vector2(s, s), Vector2(16, 16) * s), Color(1, 1, 1, 0.45))
	draw_foreground()
	if carried != null:
		PixelUI.item(self, mouse - Vector2(8, 8) * s, carried, s)
	elif hover >= 0:
		var hs := slot_stack(hover)
		if hs != null:
			draw_tooltip(hs, mouse)


func draw_background() -> void:
	PixelUI.panel(self, Rect2(origin, Vector2(win) * s), s)
	for i in slots.size():
		if slots[i].kind == "catalog" or slots[i].kind == "hidden":
			continue
		var r := slot_rect(i)
		PixelUI.slot(self, r.position, s)
	if title != "":
		PixelUI.text(self, origin + Vector2(8, 6) * s, title, Color8(64, 64, 64), s, false)
	if show_player_inventory and _has_section("main"):
		var y := 0
		for sl in slots:
			if sl.section == "main":
				y = int(sl.y)
				break
		PixelUI.text(self, origin + Vector2(8, y - 11) * s, "Inventory", Color8(64, 64, 64), s, false)


func draw_foreground() -> void:
	pass


func tooltip_lines(st: ItemStack) -> Array:
	var it := st.item()
	var lines := []
	var name_col := ItemDB.rarity_color(it.rarity)
	if st.has_glint() and it.rarity < 1:
		name_col = PixelUI.TEXT_AQUA
	if st.data.has("name"):
		name_col = PixelUI.TEXT_AQUA
	lines.append([st.display_name(), name_col])
	for e in st.enchantments():
		var col := PixelUI.TEXT_RED if EnchantDB.is_curse(String(e)) else PixelUI.TEXT_GRAY
		lines.append([EnchantDB.display(String(e), int(st.enchantments()[e])), col])
	if st.data.has("potion"):
		for l in EffectDB.potion_tooltip(String(st.data["potion"])):
			lines.append([l, Color(0.35, 0.35, 1.0)])
	if it.kind == "spawn_egg" and player.is_creative():
		lines.append(["Right-click to spawn", PixelUI.TEXT_GRAY])
	if it.armor > 0:
		lines.append(["", PixelUI.TEXT])
		lines.append(["When on body:", PixelUI.TEXT_GRAY])
		lines.append(["+%d Armor" % it.armor, Color(0.33, 0.33, 1.0)])
		if it.toughness > 0.0:
			lines.append(["+%d Armor Toughness" % int(it.toughness), Color(0.33, 0.33, 1.0)])
	elif it.damage > 1.0 and (it.kind == "weapon" or it.kind == "tool"):
		lines.append(["", PixelUI.TEXT])
		lines.append(["When in Main Hand:", PixelUI.TEXT_GRAY])
		lines.append([" %s Attack Damage" % _num(it.damage), Color(0.0, 0.66, 0.0)])
		lines.append([" %s Attack Speed" % _num(it.attack_speed), Color(0.0, 0.66, 0.0)])
	if it.is_damageable() and st.damage > 0:
		lines.append(["Durability: %d / %d" % [it.durability - st.damage, it.durability], PixelUI.TEXT])
	if it.food > 0:
		lines.append(["Restores %d hunger" % it.food, PixelUI.TEXT_GRAY])
	return lines


func _num(v: float) -> String:
	if absf(v - roundf(v)) < 0.01:
		return str(int(roundf(v)))
	return "%.1f" % v


func draw_tooltip(st: ItemStack, at: Vector2) -> void:
	var lines := tooltip_lines(st)
	var w := 0
	for l in lines:
		w = maxi(w, PixelUI.text_width(String(l[0])))
	var h := lines.size() * 10 + 2
	if lines.size() > 1:
		h += 2
	var p := at + Vector2(12, -12) * s
	var box := Rect2(p - Vector2(4, 4) * s, Vector2(w + 8, h + 6) * s)
	if box.end.x > size.x:
		box.position.x = at.x - box.size.x - 12 * s
	if box.position.y < 0:
		box.position.y = 0
	PixelUI.tooltip_box(self, box, s)
	var y := box.position.y + 4 * s
	for li in lines.size():
		var l: Array = lines[li]
		PixelUI.text(self, Vector2(box.position.x + 4 * s, y), String(l[0]), l[1], s)
		y += (12 if li == 0 else 10) * s


## Called by the UI root when the screen closes (return grid items etc.).
func on_close() -> void:
	pass
