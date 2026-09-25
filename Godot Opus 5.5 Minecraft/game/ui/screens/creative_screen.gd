class_name CreativeScreen
extends ScreenBase
## Creative inventory: 12 tabs (Building, Colored, Natural, Functional, Redstone, Search |
## Tools & Utilities, Combat, Food & Drinks, Ingredients, Spawn Eggs, Survival Inventory),
## searchable scrolling catalog with tooltips, hotbar row, destroy-item slot.

const TOP := ["building", "colored", "natural", "functional", "redstone", "search"]
const BOTTOM := ["tools", "combat", "food", "ingredients", "spawn_eggs", "inventory"]
const COLS := 9
const ROWS := 5

static var last_tab := "building"
static var last_search := ""

var tab := "building"
var items := PackedInt32Array()
var scroll := 0
var catalog := Inventory.new(COLS * ROWS)
var trash := Inventory.new(1)
var search: LineEdit = null
var _drag_scroll := false
var _cat_first := 0


func tab_strip_height() -> int:
	return 30


func _build(_ctx: Dictionary) -> void:
	win = Vector2i(195, 136)
	tab = last_tab
	search = LineEdit.new()
	search.flat = true
	search.placeholder_text = ""
	search.add_theme_font_override("font", PixelUI.get_font())
	search.add_theme_color_override("font_color", Color(1, 1, 1))
	search.add_theme_color_override("caret_color", Color(1, 1, 1))
	var sb := StyleBoxEmpty.new()
	search.add_theme_stylebox_override("normal", sb)
	search.add_theme_stylebox_override("focus", sb)
	search.text_changed.connect(_on_search)
	search.text = last_search
	add_child(search)
	_select(tab)


func _on_search(t: String) -> void:
	last_search = t
	if tab == "search":
		items = ItemDB.search(t)
		scroll = 0
		_fill()


func _select(t: String) -> void:
	tab = t
	last_tab = t
	scroll = 0
	slots.clear()
	search.visible = t == "search"
	if t == "search":
		items = ItemDB.search(search.text)
		search.grab_focus()
	elif t != "inventory":
		items = ItemDB.tab_items.get(t, PackedInt32Array())
	if t == "inventory":
		var inv: Inventory = player.inventory
		for a in 4:
			add_slot(inv, Inventory.ARMOR + 3 - a, 54 + (a % 2) * 54, 6 + (a / 2) * 27, "armor%d" % (3 - a), "armor")
		add_slot(inv, Inventory.OFFHAND, 35, 20, "normal", "offhand")
		for r in 3:
			for c in 9:
				add_slot(inv, 9 + r * 9 + c, 9 + c * 18, 54 + r * 18, "normal", "main")
		for c in 9:
			add_slot(inv, c, 9 + c * 18, 112, "normal", "hotbar")
		add_slot(trash, 0, 173, 112, "destroy", "trash")
	else:
		for r in ROWS:
			for c in COLS:
				add_slot(catalog, r * COLS + c, 9 + c * 18, 18 + r * 18, "catalog", "catalog")
		for c in 9:
			add_slot(player.inventory, c, 9 + c * 18, 112, "normal", "hotbar")
		_fill()
	Sfx.play_ui("click", 0.4, 1.0)


func _fill() -> void:
	var start := scroll * COLS
	for i in COLS * ROWS:
		var k := start + i
		catalog.slots[i] = ItemStack.new(items[k], 1) if k < items.size() else null


func _max_scroll() -> int:
	return maxi(0, int(ceil(items.size() / float(COLS))) - ROWS)


func tick_screen() -> void:
	if search != null:
		search.position = origin + Vector2(82, 5) * s
		search.size = Vector2(89, 10) * s
		search.add_theme_font_size_override("font_size", 8 * s)


func _tab_rect(t: String) -> Rect2:
	var i := TOP.find(t)
	if i >= 0:
		var x := 0 if i < 5 else 195 - 28
		if i < 5:
			x = i * 29
		return Rect2(origin + Vector2(x, -28) * s, Vector2(28, 30) * s)
	var j := BOTTOM.find(t)
	var x2 := j * 29 if j < 5 else 195 - 28
	return Rect2(origin + Vector2(x2, 134) * s, Vector2(28, 30) * s)


func handle_click(mb: InputEventMouseButton) -> bool:
	if mb.button_index == MOUSE_BUTTON_WHEEL_DOWN and tab != "inventory":
		scroll = mini(scroll + 1, _max_scroll())
		_fill()
		return true
	if mb.button_index == MOUSE_BUTTON_WHEEL_UP and tab != "inventory":
		scroll = maxi(scroll - 1, 0)
		_fill()
		return true
	if mb.button_index != MOUSE_BUTTON_LEFT:
		return false
	for t in TOP + BOTTOM:
		if _tab_rect(String(t)).has_point(mb.position):
			if String(t) != tab:
				_select(String(t))
			return true
	var bar := Rect2(origin + Vector2(175, 18) * s, Vector2(12, 90) * s)
	if tab != "inventory" and bar.has_point(mb.position):
		_drag_scroll = true
		_scroll_to(mb.position.y)
		return true
	return false


func _scroll_to(y: float) -> void:
	var top := origin.y + 18 * s
	var frac := clampf((y - top - 7.5 * s) / ((90 - 15) * s), 0.0, 1.0)
	scroll = roundi(frac * _max_scroll())
	_fill()


func _gui_input(event: InputEvent) -> void:
	if _drag_scroll:
		if event is InputEventMouseMotion:
			_scroll_to(event.position.y)
			return
		if event is InputEventMouseButton and not event.pressed:
			_drag_scroll = false
			return
	super(event)


func accepts(i: int, st: ItemStack) -> bool:
	if slots[i].kind == "destroy":
		return true
	return super(i, st)


func _click(i: int, button: int, shift: bool, dbl: bool) -> void:
	var kind: String = slots[i].kind
	if kind == "catalog":
		var st := slot_stack(i)
		if carried != null:
			# clicking the catalog with a stack deletes it (or adds one of the same item)
			if st != null and carried.id == st.id and carried.count < carried.max_stack() and button == MOUSE_BUTTON_LEFT:
				carried.count += 1
			else:
				carried = null
			return
		if st == null:
			return
		if shift:
			var full := st.with_count(st.max_stack())
			var rem = player.inventory.add(full, 0, 36)
			if rem != null:
				pass
			Sfx.play_ui("pop", 0.3, 1.2)
			return
		carried = st.with_count(st.max_stack() if button == MOUSE_BUTTON_RIGHT else 1)
		return
	if kind == "destroy":
		if shift:
			for k in 46:
				player.inventory.slots[k] = null
			player.inventory.changed.emit()
		carried = null
		trash.slots[0] = null
		Sfx.play_ui("click", 0.3, 0.6)
		return
	super(i, button, shift, dbl)


func _middle(i: int) -> void:
	var st := slot_stack(i)
	if st != null and carried == null:
		carried = st.with_count(st.max_stack())


func _hotbar_swap(i: int, hot: int) -> void:
	if slots[i].kind == "catalog":
		var st := slot_stack(i)
		if st != null and hot >= 0:
			player.inventory.set_stack(hot, st.with_count(st.max_stack()))
		return
	super(i, hot)


func draw_background() -> void:
	# unselected tabs behind the panel
	for t in TOP + BOTTOM:
		if String(t) != tab:
			_draw_tab(String(t), false)
	PixelUI.panel(self, Rect2(origin, Vector2(win) * s), s)
	var title_s: String = ItemDB.TAB_TITLES.get(tab, tab)
	PixelUI.text(self, origin + Vector2(8, 6) * s, title_s if tab != "search" else "Search Items", Color8(64, 64, 64), s, false)
	if tab == "search":
		draw_rect(Rect2(origin + Vector2(80, 4) * s, Vector2(90, 12) * s), Color8(0, 0, 0))
		draw_rect(Rect2(origin + Vector2(81, 5) * s, Vector2(88, 10) * s), Color8(32, 32, 32))
	for i in slots.size():
		var r := slot_rect(i)
		if slots[i].kind == "destroy":
			PixelUI.slot(self, r.position, s)
			_draw_x(r)
		else:
			PixelUI.slot(self, r.position, s)
	if tab != "inventory":
		# scrollbar
		var bar := Rect2(origin + Vector2(175, 18) * s, Vector2(12, 90) * s)
		draw_rect(bar, Color8(0, 0, 0))
		draw_rect(bar.grow(-s), Color8(60, 60, 60))
		var ms := _max_scroll()
		var frac := 0.0 if ms == 0 else float(scroll) / ms
		var knob := Rect2(bar.position + Vector2(s, s + frac * (90 - 17) * s), Vector2(10, 15) * s)
		draw_rect(knob, Color8(198, 198, 198) if ms > 0 else Color8(120, 120, 120))
		draw_rect(Rect2(knob.position, Vector2(10, 1) * s), Color8(255, 255, 255))
		draw_rect(Rect2(knob.position + Vector2(0, 14) * s, Vector2(10, 1) * s), Color8(85, 85, 85))
		if tab != "search":
			PixelUI.text(self, origin + Vector2(122, 6) * s, "%d items" % items.size(), Color8(96, 96, 96), s, false)
	else:
		# player preview box
		draw_rect(Rect2(origin + Vector2(73, 6) * s, Vector2(32, 43) * s), Color8(0, 0, 0))
		draw_rect(Rect2(origin + Vector2(74, 7) * s, Vector2(30, 41) * s), Color8(40, 40, 40))
		var st2 := ItemStack.of("player_head", 1)
		if st2 != null:
			PixelUI.item(self, origin + Vector2(81, 19) * s, st2, s)
	_draw_tab(tab, true)


func _draw_x(r: Rect2) -> void:
	var c := Color8(200, 40, 40)
	for k in 10:
		draw_rect(Rect2(r.position + Vector2(4 + k, 4 + k) * s, Vector2(s, s)), c)
		draw_rect(Rect2(r.position + Vector2(13 - k, 4 + k) * s, Vector2(s, s)), c)


func _draw_tab(t: String, selected: bool) -> void:
	var r := _tab_rect(t)
	var top := TOP.has(t)
	var body := r
	if not selected:
		body = Rect2(r.position + Vector2(0, 2 * s if top else 0), r.size - Vector2(0, 2 * s))
	draw_rect(body, Color8(0, 0, 0))
	draw_rect(body.grow(-s), PixelUI.PANEL if selected else Color8(160, 160, 160))
	draw_rect(Rect2(body.position + Vector2(s, s), Vector2(body.size.x - 2 * s, s)), PixelUI.PANEL_LIGHT)
	var icon_name: String = ItemDB.TAB_ICONS.get(t, "stone")
	var st := ItemStack.of(icon_name, 1)
	if st != null:
		PixelUI.item(self, r.position + Vector2(6, 8) * s, st, s, false)
	if r.has_point(mouse) and not selected:
		draw_rect(body.grow(-s), Color(1, 1, 1, 0.15))


func draw_foreground() -> void:
	for t in TOP + BOTTOM:
		if _tab_rect(String(t)).has_point(mouse):
			var label: String = ItemDB.TAB_TITLES.get(String(t), String(t))
			var w := PixelUI.text_width(label)
			var box := Rect2(mouse + Vector2(12, -12) * s, Vector2(w + 8, 16) * s)
			PixelUI.tooltip_box(self, box, s)
			PixelUI.text(self, box.position + Vector2(4, 4) * s, label, PixelUI.TEXT, s)


func quick_move(i: int) -> void:
	var sec: String = slots[i].section
	if sec == "hotbar" and tab != "inventory":
		# shift-click in the hotbar row of a catalog tab clears the slot (creative convenience)
		set_slot_stack(i, null)
		return
	super(i)
