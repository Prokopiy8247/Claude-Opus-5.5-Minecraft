class_name UIRoot
extends CanvasLayer
## UI root: routes every screen (inventory, creative catalog, containers, stations, recipe book,
## pause menu, options, death screen, console + commands, admin panel), owns the carried stack and
## the GUI scale, and handles keyboard shortcuts and mouse capture.

var session = null
var player = null
var carried: ItemStack = null
var screen: ScreenBase = null
var modal = null                       # Control for non-slot screens (menus, console, admin panel)
var item_frames: Inventory = null      # carried placeholders (unused, kept for slot parity)
var _scale_cache := 0
var _last_size := Vector2.ZERO
var console: LineEdit = null


func _ready() -> void:
	layer = 10


func refresh() -> void:
	player = session.player
	_scale_cache = Game.gui_scale()
	_last_size = get_viewport().get_visible_rect().size
	if DisplayServer.get_name() != "headless" and screen == null and modal == null:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func scale() -> int:
	var vs := get_viewport().get_visible_rect().size
	if vs != _last_size:
		_last_size = vs
		_scale_cache = Game.gui_scale()
	return maxi(1, _scale_cache)


func is_open() -> bool:
	return screen != null or modal != null


# ------------------------------------------------------------------------------------------------
# Screen management
func open_screen(kind: String, ctx: Dictionary = {}) -> void:
	close_screen()
	var cls = _screen_class(kind)
	if cls == null:
		push_warning("unknown screen: " + kind)
		return
	var s: ScreenBase = cls.new()
	s.setup(self, ctx)
	add_child(s)
	screen = s
	_capture(false)


func _screen_class(kind: String):
	match kind:
		"creative":
			return CreativeScreen
		"inventory":
			return CraftingScreens.InventoryScreen
		"crafting":
			return CraftingScreens.CraftingTableScreen
		"container", "endere_chest":
			return ContainerScreens.ChestScreen
		"dispenser":
			return ContainerScreens.DispenserScreen
		"hopper":
			return ContainerScreens.HopperScreen
		"furnace":
			return ContainerScreens.FurnaceScreen
		"brewing":
			return ContainerScreens.BrewingScreen
		"enchanting":
			return StationScreens.EnchantingScreen
		"anvil":
			return StationScreens.AnvilScreen
		"grindstone":
			return StationScreens.GrindstoneScreen
		"stonecutter":
			return StationScreens.StonecutterScreen
		"smithing":
			return StationScreens.SmithingScreen
		"beacon":
			return StationScreens.BeaconScreen
	return null


func close_screen() -> void:
	if screen != null and is_instance_valid(screen):
		screen.on_close()
		screen.queue_free()
	screen = null
	if carried != null:
		# put the carried stack back or drop it
		if player != null:
			var rem = player.inventory.add(carried, 0, 36)
			if rem != null:
				session.drop_item_from_player(rem)
		carried = null
	if modal == null:
		_capture(true)


func open_modal(node: Control) -> void:
	close_screen()
	if modal != null and is_instance_valid(modal):
		modal.queue_free()
	modal = node
	add_child(node)
	_capture(false)


func close_modal() -> void:
	if modal != null and is_instance_valid(modal):
		modal.queue_free()
	modal = null
	if session != null:
		session.paused = false
	if screen == null:
		_capture(true)


func _capture(on: bool) -> void:
	if DisplayServer.get_name() == "headless":
		return
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED if on else Input.MOUSE_MODE_VISIBLE
	if player != null:
		player.input_locked = not on


# ------------------------------------------------------------------------------------------------
# Unified key handling (screens consume keys first)
func _unhandled_key_input(event: InputEvent) -> void:
	if not (event is InputEventKey) or not event.pressed or event.echo:
		return
	var k: InputEventKey = event
	if screen != null:
		if k.keycode == KEY_ESCAPE or k.keycode == KEY_E:
			close_screen()
			get_viewport().set_input_as_handled()
		return
	if modal != null:
		if k.keycode == KEY_ESCAPE:
			close_modal()
			get_viewport().set_input_as_handled()
		return
	match k.keycode:
		KEY_E:
			open_screen("creative" if player.is_creative() else "inventory", {})
			get_viewport().set_input_as_handled()
		KEY_ESCAPE:
			open_pause_menu()
			get_viewport().set_input_as_handled()
		KEY_F3:
			if session.hud != null:
				session.hud.toggle_debug()
		KEY_F7:
			open_admin_panel()
			get_viewport().set_input_as_handled()
		KEY_T:
			open_console(false)
			get_viewport().set_input_as_handled()
		KEY_SLASH:
			open_console(true)
			get_viewport().set_input_as_handled()
		KEY_F4:
			cycle_gamemode()
		KEY_F2:
			_screenshot()


func _screenshot() -> void:
	var dir := "user://screenshots"
	DirAccess.make_dir_recursive_absolute(dir)
	var name := "%s/shot_%d.png" % [dir, Time.get_ticks_msec()]
	var err := get_viewport().get_texture().get_image().save_png(name)
	if err == OK:
		session.action_bar("Saved screenshot to %s" % ProjectSettings.globalize_path(name))


# ------------------------------------------------------------------------------------------------
# Recipe book
class RecipeBook extends Control:
	var ui = null
	var session = null
	var player = null
	var s := 2
	var origin := Vector2.ZERO
	var win := Vector2i(147, 166)
	var page := 0
	var page_size := 20
	var entries: Array = []
	var result: ItemStack = null
	var _dragging := false
	var _scroll := 0

	func _init() -> void:
		mouse_filter = Control.MOUSE_FILTER_STOP
		focus_mode = Control.FOCUS_ALL

	var craftable := PackedByteArray()

	## Recipes for the held item, or every recipe the player can currently craft.
	func setup(p_ui, out: ItemStack) -> void:
		ui = p_ui
		session = p_ui.session
		player = session.player
		result = out
		if out != null and not RecipeDB.recipes_for(out.item_name()).is_empty():
			entries = RecipeDB.recipes_for(out.item_name()).duplicate()
		else:
			entries = []
			for r in RecipeDB.shaped + RecipeDB.shapeless:
				if _has_ingredients(r):
					entries.append(r)
		_refresh()

	func _refresh() -> void:
		craftable.resize(entries.size())
		for i in entries.size():
			craftable[i] = 1 if _has_ingredients(entries[i]) else 0

	func _ready() -> void:
		set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

	func _process(_delta: float) -> void:
		s = ui.scale()
		origin = ((size - Vector2(win) * s) * 0.5).floor()
		queue_redraw()

	func _item_rect(i: int) -> Rect2:
		var col := i % 5
		var row := i / 5
		return Rect2(origin + Vector2(9 + col * 26, 18 + row * 26) * s, Vector2(20, 20) * s)

	func _gui_input(event: InputEvent) -> void:
		if event is InputEventMouseButton and event.pressed:
			var mb: InputEventMouseButton = event
			if mb.button_index == MOUSE_BUTTON_WHEEL_DOWN:
				_scroll = mini(_scroll + 1, maxi(0, _max_page() - 1))
				accept_event()
				return
			if mb.button_index == MOUSE_BUTTON_WHEEL_UP:
				_scroll = maxi(_scroll - 1, 0)
				accept_event()
				return
			if mb.button_index == MOUSE_BUTTON_LEFT:
				for i in page_size:
					var idx := _scroll * page_size + i
					if idx < entries.size() and _item_rect(i).has_point(mb.position):
						_craft(entries[idx])
						accept_event()
						return

	func _max_page() -> int:
		return maxi(1, int(ceil(entries.size() / float(page_size))))

	## Ingredients of one recipe as {item or #tag: count}.
	func _needs(rec: Dictionary) -> Dictionary:
		var need := {}
		if rec.has("grid"):
			for ing in (rec["grid"] as Array):
				var n := String(ing)
				if n != "":
					need[n] = int(need.get(n, 0)) + 1
		else:
			for ing2 in (rec.get("ing", []) as Array):
				var n2 := String(ing2)
				need[n2] = int(need.get(n2, 0)) + 1
		return need

	func _count_of(ing: String) -> int:
		if ing.begins_with("#"):
			var total := 0
			for n in (RecipeDB.tags.get(ing, {}) as Dictionary):
				var iid := ItemDB.id(String(n))
				if iid >= 0:
					total += player.inventory.count_item(iid)
			return total
		var id := ItemDB.id(ing)
		return 0 if id < 0 else player.inventory.count_item(id)

	func _has_ingredients(rec: Dictionary) -> bool:
		for ing in _needs(rec):
			if _count_of(String(ing)) < int(_needs(rec)[ing]):
				return false
		return true

	func _consume(rec: Dictionary) -> bool:
		var need := _needs(rec)
		for ing in need:
			if _count_of(String(ing)) < int(need[ing]):
				return false
		for ing in need:
			var left := int(need[ing])
			if String(ing).begins_with("#"):
				for n in (RecipeDB.tags.get(String(ing), {}) as Dictionary):
					if left <= 0:
						break
					var iid := ItemDB.id(String(n))
					if iid < 0:
						continue
					left -= player.inventory.remove_item(iid, left, 36)
			else:
				player.inventory.remove_item(ItemDB.id(String(ing)), left, 36)
		return true

	func _craft(rec: Dictionary) -> void:
		if not _consume(rec):
			session.action_bar("Missing ingredients")
			return
		var out := ItemStack.of(String(rec["out"]), int(rec.get("count", 1)))
		if out == null:
			return
		var rem = player.inventory.add(out, 0, 36)
		if rem != null:
			session.drop_item_from_player(rem)
		Sfx.play_ui("pop", 0.4, 1.1)
		_refresh()

	func _draw() -> void:
		draw_rect(Rect2(Vector2.ZERO, size), Color(0.06, 0.06, 0.08, 0.72))
		PixelUI.panel(self, Rect2(origin, Vector2(win) * s), s)
		PixelUI.text(self, origin + Vector2(8, 6) * s, "Recipe Book", Color8(64, 64, 64), s, false)
		for i in page_size:
			var idx := _scroll * page_size + i
			var r := _item_rect(i)
			PixelUI.slot(self, r.position, s)
			if idx < entries.size():
				var rec: Dictionary = entries[idx]
				var st: ItemStack = ItemStack.of(String(rec["out"]), int(rec.get("count", 1)))
				PixelUI.item(self, r.position + Vector2(2, 2) * s, st, s)
				if idx < craftable.size() and craftable[idx] == 0:
					draw_rect(r.grow(-s), Color(0, 0, 0, 0.55))
		PixelUI.text(self, origin + Vector2(8, 152) * s, "%d recipes  (wheel to scroll)" % entries.size(), Color8(64, 64, 64), s, false)
		if _max_page() > 1:
			PixelUI.text(self, origin + Vector2(118, 152) * s, "%d/%d" % [_scroll + 1, _max_page()], Color8(64, 64, 64), s, false)


## Recipe book for the stack the player is holding (or the whole catalogue when holding nothing).
func open_recipe_book(_from_screen) -> void:
	if carried != null or player == null:
		return
	var sel: ItemStack = player.inventory.selected_stack()
	var rb := RecipeBook.new()
	rb.setup(self, sel)
	if rb.entries.is_empty():
		session.action_bar("Nothing craftable with your inventory")
		return
	open_modal(rb)


# ------------------------------------------------------------------------------------------------
# Pause menu / options / death / admin
## Villager trading window.
func open_trade(m: Mob) -> void:
	var t := TradeScreen.new()
	t.setup(self, m)
	open_modal(t)


func open_pause_menu() -> void:
	var p := _pause_panel()
	open_modal(p)
	if session != null:
		session.paused = true


func _pause_panel() -> Control:
	var panel := _panel_control("Game Menu", 220, 190)
	var list := VBoxContainer.new()
	list.position = Vector2(20, 40)
	list.size = Vector2(180, 130)
	panel.add_child(list)
	_add_button(list, "Back to Game", func(): close_modal())
	_add_button(list, "Options...", func(): open_options())
	_add_button(list, "Gamerules...", func(): open_gamerules())
	_add_button(list, "Save World", func():
		session.save_all()
		session.chat("World saved", Color(0.7, 1, 0.7))
		close_modal())
	_add_button(list, "Quit to Title", func():
		session.save_all()
		Game.to_title())
	return panel


func _add_button(parent: Node, label: String, cb: Callable) -> Button:
	var b := Button.new()
	b.text = label
	b.custom_minimum_size = Vector2(180, 20)
	b.add_theme_font_override("font", PixelUI.get_font())
	b.add_theme_font_size_override("font_size", 8 * scale())
	b.pressed.connect(cb)
	parent.add_child(b)
	return b


func _panel_control(title: String, w: int, h: int) -> Control:
	var root := Control.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_STOP
	var bg := ColorRect.new()
	bg.color = Color(0.05, 0.05, 0.07, 0.72)
	bg.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_child(bg)
	var frame := Panel.new()
	var vs := get_viewport().get_visible_rect().size
	frame.position = ((vs - Vector2(w * scale(), h * scale())) * 0.5).floor()
	frame.size = Vector2(w * scale(), h * scale())
	frame.add_theme_stylebox_override("panel", _box())
	root.add_child(frame)
	var lbl := Label.new()
	lbl.text = title
	lbl.position = Vector2(12 * scale(), 8 * scale())
	lbl.add_theme_font_override("font", PixelUI.get_font())
	lbl.add_theme_font_size_override("font_size", 8 * scale())
	lbl.add_theme_color_override("font_color", PixelUI.TEXT)
	frame.add_child(lbl)
	root.set_meta("frame", frame)
	return root


func _box() -> StyleBoxFlat:
	var b := StyleBoxFlat.new()
	b.bg_color = PixelUI.PANEL
	b.border_color = Color(0, 0, 0)
	b.set_border_width_all(maxi(1, scale()))
	return b


func open_options() -> void:
	close_modal()
	var p := _panel_control("Options", 300, 260)
	var frame: Control = p.get_meta("frame")
	var list := VBoxContainer.new()
	list.position = Vector2(16 * scale(), 34 * scale())
	list.size = Vector2(268 * scale(), 210 * scale())
	frame.add_child(list)
	_add_slider(list, "Render distance", 2, 16, float(Game.settings.render_distance), func(v: float):
		Game.settings.render_distance = int(v)
		for k in session.worlds:
			var w = session.worlds[k]
			if w != null and is_instance_valid(w):
				(w as World).cm.render_distance = int(v))
	_add_slider(list, "Field of view", 30, 110, float(Game.settings.fov), func(v: float):
		Game.settings.fov = v
		if session.player.camera != null:
			session.player.camera.fov = v)
	_add_slider(list, "Mouse sensitivity", 1, 10, float(Game.settings.mouse_sensitivity) * 10.0, func(v: float):
		Game.settings.mouse_sensitivity = v / 10.0)
	_add_slider(list, "Brightness", 0, 10, float(Game.settings.brightness) * 10.0, func(v: float):
		Game.settings.brightness = v / 10.0
		RenderingServer.global_shader_parameter_set("gamma_boost", v / 10.0))
	_add_slider(list, "Master volume", 0, 10, float(Game.settings.master_volume) * 10.0, func(v: float):
		Game.settings.master_volume = v / 10.0
		AudioServer.set_bus_volume_db(AudioServer.get_bus_index("Master"), linear_to_db(maxf(v / 10.0, 0.0001))))
	_add_slider(list, "Music volume", 0, 10, float(Game.settings.music_volume) * 10.0, func(v: float):
		Game.settings.music_volume = v / 10.0)
	_add_check(list, "View bobbing", bool(Game.settings.view_bobbing), func(v: bool): Game.settings.view_bobbing = v)
	_add_check(list, "Fancy leaves", bool(Game.settings.fancy_leaves), func(v: bool):
		Game.settings.fancy_leaves = v
		for k in session.worlds:
			var w = session.worlds[k]
			if w != null and is_instance_valid(w):
				(w as World).cm.fancy_leaves = v
				(w as World).cm.remesh_all())
	_add_check(list, "Fullscreen", bool(Game.settings.fullscreen), func(v: bool):
		Game.settings.fullscreen = v
		Game.apply_window_settings())
	_add_button(list, "Back", func():
		Game.save_settings()
		close_modal()
		open_pause_menu())
	open_modal(p)


func _add_slider(parent: Node, label: String, mn: float, mx: float, value: float, cb: Callable) -> void:
	var row := HBoxContainer.new()
	var lbl := Label.new()
	lbl.text = label
	lbl.custom_minimum_size = Vector2(150 * scale(), 16 * scale())
	lbl.add_theme_font_override("font", PixelUI.get_font())
	lbl.add_theme_font_size_override("font_size", 8 * scale())
	lbl.add_theme_color_override("font_color", PixelUI.TEXT)
	row.add_child(lbl)
	var sl := HSlider.new()
	sl.min_value = mn
	sl.max_value = mx
	sl.step = 1.0
	sl.value = value
	sl.custom_minimum_size = Vector2(120 * scale(), 16 * scale())
	sl.value_changed.connect(cb)
	row.add_child(sl)
	parent.add_child(row)


func _add_check(parent: Node, label: String, value: bool, cb: Callable) -> void:
	var c := CheckBox.new()
	c.text = label
	c.button_pressed = value
	c.add_theme_font_override("font", PixelUI.get_font())
	c.add_theme_font_size_override("font_size", 8 * scale())
	c.add_theme_color_override("font_color", PixelUI.TEXT)
	c.toggled.connect(cb)
	parent.add_child(c)


var _rules := ["doMobSpawning", "doTileDrops", "doFireTick", "mobGriefing", "keepInventory", "doDaylightCycle",
	"doWeatherCycle", "naturalRegeneration", "showDeathMessages", "doMobLoot", "doImmediateRespawn", "doInsomnia"]


func open_gamerules() -> void:
	close_modal()
	var p := _panel_control("Gamerules", 300, 320)
	var frame: Control = p.get_meta("frame")
	var list := VBoxContainer.new()
	list.position = Vector2(16 * scale(), 34 * scale())
	list.size = Vector2(268 * scale(), 270 * scale())
	frame.add_child(list)
	for r in _rules:
		var name := String(r)
		_add_check(list, name, bool(session.gamerules.get(name, false)), func(v: bool):
			session.set_gamerule(name, v)
			session.chat("Gamerule %s = %s" % [name, str(v)], Color(0.8, 0.9, 1.0)))
	_add_button(list, "Back", func():
		close_modal()
		open_pause_menu())
	open_modal(p)


func show_death_screen(cause: String) -> void:
	if modal != null and modal.has_meta("death"):
		return
	var p := _panel_control("You Died!", 220, 140)
	p.set_meta("death", true)
	var frame: Control = p.get_meta("frame")
	_add_button(frame, "Respawn", func():
		close_modal()
		session.respawn_player())
	var b := _add_button(frame, "Title Screen", func():
		close_modal()
		session.save_all()
		Game.to_title())
	b.position = Vector2(0, 24 * scale())
	var msg := Label.new()
	msg.text = cause.capitalize() if cause != "" else "unknown"
	msg.position = Vector2(12 * scale(), 8 * scale())
	frame.add_child(msg)
	open_modal(p)


func hide_death_screen() -> void:
	if modal != null and modal.has_meta("death"):
		close_modal()


func on_dimension_changed() -> void:
	if screen != null:
		close_screen()
	if session.hud != null and session.hud.has_method("add_chat"):
		return


# ------------------------------------------------------------------------------------------------
# Console + commands
func open_console(prefill_slash: bool) -> void:
	var p := Control.new()
	p.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	p.mouse_filter = Control.MOUSE_FILTER_STOP
	console = LineEdit.new()
	console.placeholder_text = "Message or /command"
	console.add_theme_font_override("font", PixelUI.get_font())
	console.add_theme_font_size_override("font_size", 8 * maxi(1, scale()))
	console.add_theme_color_override("font_color", Color(1, 1, 1))
	var vs := get_viewport().get_visible_rect().size
	console.position = Vector2(8 * scale(), vs.y - 46 * scale())
	console.size = Vector2(vs.x - 16 * scale(), 14 * scale())
	console.text = "/" if prefill_slash else ""
	console.text_submitted.connect(_on_console_submit)
	console.focus_exited.connect(func(): if modal == p: close_modal())
	p.add_child(console)
	open_modal(p)
	console.grab_focus()
	console.caret_column = console.text.length()


func _on_console_submit(text: String) -> void:
	var t := text.strip_edges()
	close_modal()
	console = null
	if t == "":
		return
	if t.begins_with("/"):
		Commands.run(session, t.substr(1))
	else:
		session.chat("<Player> %s" % t, Color(1, 1, 1))


# ------------------------------------------------------------------------------------------------
# Admin panel
func open_admin_panel() -> void:
	if modal != null:
		close_modal()
		return
	var p := _panel_control("Admin Panel (F7)", 320, 300)
	var frame: Control = p.get_meta("frame")
	var list := VBoxContainer.new()
	list.position = Vector2(16 * scale(), 34 * scale())
	list.size = Vector2(288 * scale(), 250 * scale())
	frame.add_child(list)
	_add_button(list, "Gamemode: Creative", func(): _set_mode(Player.CREATIVE))
	_add_button(list, "Gamemode: Survival", func(): _set_mode(Player.SURVIVAL))
	_add_button(list, "Gamemode: Spectator", func(): _set_mode(Player.SPECTATOR))
	_add_button(list, "Time: Day", func(): _command("time set day"))
	_add_button(list, "Time: Night", func(): _command("time set night"))
	_add_button(list, "Weather: Clear", func(): _command("weather clear"))
	_add_button(list, "Weather: Rain", func(): _command("weather rain"))
	_add_button(list, "Weather: Thunder", func(): _command("weather thunder"))
	_add_button(list, "Difficulty up", func():
		session.difficulty = clampi(session.difficulty + 1, 0, 3)
		session.chat("Difficulty: %s" % Commands.difficulty_name(session.difficulty)))
	_add_button(list, "Difficulty down", func():
		session.difficulty = clampi(session.difficulty - 1, 0, 3)
		session.chat("Difficulty: %s" % Commands.difficulty_name(session.difficulty)))
	_add_button(list, "Toggle flying", func():
		player.flying = not player.flying
		player.may_fly = true)
	_add_button(list, "Reload chunks", func():
		session.world.cm.remesh_all())
	_add_button(list, "Close", func(): close_modal())
	open_modal(p)


func _set_mode(m: int) -> void:
	player.set_gamemode(m)
	session.chat("Game mode set to %s" % Commands.mode_name(m), Color(0.8, 0.9, 1.0))


func _command(cmd: String) -> void:
	Commands.run(session, cmd)


func cycle_gamemode() -> void:
	var order := [Player.SURVIVAL, Player.CREATIVE, Player.SPECTATOR]
	var i := order.find(player.gamemode)
	if i < 0:
		i = 0
	var m: int = order[(i + 1) % order.size()]
	_set_mode(m)
	session.action_bar("Game mode: %s" % Commands.mode_name(m))
