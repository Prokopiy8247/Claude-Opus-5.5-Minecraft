class_name TitleScreen
extends Control
## Title screen: logo-free original wordmark, singleplayer world list (create / play / delete),
## settings, controls and quit. Drawn with the project's pixel toolkit on a dimmed block backdrop.

enum View { MAIN, WORLD_LIST, CREATE, SETTINGS, CONTROLS }

var view := View.MAIN
var worlds: Array = []
var sel := 0
var name_edit: LineEdit = null
var seed_edit: LineEdit = null
var gamemode := Player.CREATIVE
var difficulty := 2
var splash := ""
var s := 2
var _scroll := 0
var _anim := 0.0

const SPLASHES := [
	"Also try a pickaxe!", "100% blocky!", "Now with Sulfur Caves!", "Written in GDScript!",
	"Diamonds are forever", "Creepers gonna creep", "Not affiliated with Mojang", "Handmade textures!",
	"20 ticks per second", "Bring a torch", "Watch out for the Warden",
]


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_STOP
	splash = SPLASHES[randi() % SPLASHES.size()]
	_refresh_worlds()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	set_process(true)
	if Game.autotest != "" or Game.cmdline.has("quickstart"):
		_quickstart.call_deferred()


## Command-line entry (--autotest=<scenario> or --quickstart): fresh deterministic world.
func _quickstart() -> void:
	var tag := Game.autotest if Game.autotest != "" else "quick"
	var folder := "autotest_" + tag
	SaveManager.delete_world(folder)
	var gm := Player.SURVIVAL if Game.cmdline.get("mode", "creative") == "survival" else Player.CREATIVE
	Game.start_world({
		"name": "Autotest " + tag, "folder": folder, "seed": int(Game.cmdline.get("seed", "20260924")),
		"gamemode": gm, "difficulty": 2, "gamerules": {}, "player": {},
	})


func _refresh_worlds() -> void:
	worlds = SaveManager.list_worlds()
	sel = clampi(sel, 0, maxi(0, worlds.size() - 1))


func _process(delta: float) -> void:
	_anim += delta
	s = Game.gui_scale()
	queue_redraw()


# ------------------------------------------------------------------------------------------------
func _draw() -> void:
	# dimmed dirt backdrop drawn first (a background child node would cover this draw list)
	PixelUI.tile_background(self, Rect2(Vector2.ZERO, size), s)
	match view:
		View.MAIN:
			_draw_main()
		View.WORLD_LIST:
			_draw_world_list()
		View.CREATE:
			_draw_create()
		View.SETTINGS:
			_draw_settings()
		View.CONTROLS:
			_draw_controls()
	# footer
	PixelUI.text(self, Vector2(4 * s, size.y - 10 * s), "Godot Minecraft %s  -  an original Minecraft-style recreation, not affiliated with Mojang or Microsoft" % Game.VERSION,
		Color(0.75, 0.75, 0.8), maxi(1, s - 1), false)


func _center_button(y: int, label: String, w := 200) -> Rect2:
	return Rect2(Vector2(roundf(size.x * 0.5 - w * s * 0.5), float(y * s)), Vector2(w, 20) * s)


func _draw_main() -> void:
	var title := "GODOT MINECRAFT"
	var tw := PixelUI.text_width(title) * (s + 1)
	PixelUI.text(self, Vector2(roundf(size.x * 0.5 - tw * 0.5) + 3 * s, 30 * s + 3 * s), title, Color(0.1, 0.1, 0.1), s + 1, false)
	PixelUI.text(self, Vector2(roundf(size.x * 0.5 - tw * 0.5), 30 * s), title, Color(0.9, 0.9, 0.95), s + 1, false)
	var sw := PixelUI.text_width(splash) * s
	var pulse := 1.0 + sin(_anim * 4.0) * 0.06
	PixelUI.text(self, Vector2(roundf(size.x * 0.5 - sw * pulse * 0.5), 46 * s), splash, Color(1.0, 1.0, 0.35), s, false)
	var labels := ["Singleplayer", "Settings...", "Controls...", "Quit Game"]
	for i in labels.size():
		var r := _center_button(64 + i * 24, String(labels[i]))
		var hover := r.has_point(get_local_mouse_position())
		PixelUI.button(self, r, String(labels[i]), s, hover)


func _draw_world_list() -> void:
	PixelUI.text_centered(self, size.x * 0.5, 10 * s, "Select World", PixelUI.TEXT, s)
	var list := Rect2(Vector2(roundf(size.x * 0.5 - 150 * s), 24 * s), Vector2(300, 120) * s)
	draw_rect(list, Color(0, 0, 0, 0.65))
	draw_rect(list.grow(s), Color(0.45, 0.45, 0.5), false, s)
	var shown := maxi(1, int(list.size.y / (22.0 * s)))
	_scroll = clampi(_scroll, 0, maxi(0, worlds.size() - shown))
	if worlds.is_empty():
		PixelUI.text_centered(self, size.x * 0.5, list.position.y + 50 * s, "No worlds yet - create one below", Color(0.8, 0.8, 0.85), s)
	else:
		for i in mini(shown, worlds.size() - _scroll):
			var idx := _scroll + i
			var w: Dictionary = worlds[idx]
			var r := Rect2(list.position + Vector2(0, i * 22 * s), Vector2(list.size.x, 22 * s))
			var hover := r.has_point(get_local_mouse_position())
			if idx == sel:
				draw_rect(r, Color(0.4, 0.5, 0.8, 0.5))
			elif hover:
				draw_rect(r, Color(1, 1, 1, 0.12))
			PixelUI.text(self, r.position + Vector2(6, 7) * s, "%s" % String(w.get("name", w.get("folder", "?"))), PixelUI.TEXT, s, false)
			var sub := "%s  seed %d  %s" % [String(w.get("folder", "")), int(w.get("seed", 0)), _time_ago(int(w.get("last_played", 0)))]
			PixelUI.text(self, r.position + Vector2(150, 7) * s, sub, Color(0.72, 0.72, 0.78), maxi(1, s - 1), false)
	# buttons
	var names := ["Play", "Create New World", "Delete", "Back"]
	for i in names.size():
		var r2 := _center_button(152 + i * 24, String(names[i]), 200)
		PixelUI.button(self, r2, String(names[i]), s, r2.has_point(get_local_mouse_position()))


func _draw_create() -> void:
	PixelUI.text_centered(self, size.x * 0.5, 12 * s, "Create New World", PixelUI.TEXT, s)
	var box := Rect2(Vector2(roundf(size.x * 0.5 - 130 * s), 26 * s), Vector2(260, 108) * s)
	draw_rect(box, Color(0, 0, 0, 0.65))
	draw_rect(box.grow(s), Color(0.45, 0.45, 0.5), false, s)
	PixelUI.text(self, Vector2(box.position.x + 8 * s, 30 * s), "World Name", PixelUI.TEXT_GRAY, s, false)
	PixelUI.text(self, Vector2(box.position.x + 8 * s, 58 * s), "Seed (blank = random)", PixelUI.TEXT_GRAY, s, false)
	for i in 2:
		var r := _create_row(i)
		var hover := r.has_point(get_local_mouse_position())
		draw_rect(r, Color(0.35, 0.35, 0.45, 0.8) if hover else Color(0.2, 0.2, 0.25, 0.7))
	PixelUI.text(self, _create_row(0).position + Vector2(4, 2) * s, "Game Mode: %s" % Commands.mode_name(gamemode), PixelUI.TEXT, s, false)
	PixelUI.text(self, _create_row(1).position + Vector2(4, 2) * s, "Difficulty: %s" % Commands.difficulty_name(difficulty), PixelUI.TEXT, s, false)
	PixelUI.text(self, Vector2(box.position.x + 8 * s, 120 * s), "Click a setting to change it", PixelUI.TEXT_GRAY, maxi(1, s - 1), false)
	var names := ["Create", "Back"]
	for i in names.size():
		var r := _center_button(142 + i * 24, String(names[i]), 200)
		PixelUI.button(self, r, String(names[i]), s, r.has_point(get_local_mouse_position()))


func _create_row(i: int) -> Rect2:
	var x := roundf(size.x * 0.5 - 130 * s) + 8 * s
	return Rect2(Vector2(x, (86 + i * 14) * s), Vector2(244, 12) * s)


func _draw_settings() -> void:
	PixelUI.text_centered(self, size.x * 0.5, 10 * s, "Settings", PixelUI.TEXT, s)
	var rows := [
		["Render distance", str(Game.settings.render_distance)],
		["Field of view", str(int(Game.settings.fov))],
		["GUI scale", "Auto (%d)" % Game.gui_scale()],
		["Mouse sensitivity", "%.1f" % (float(Game.settings.mouse_sensitivity) * 10.0)],
		["Master volume", "%d%%" % int(float(Game.settings.master_volume) * 100.0)],
		["Music volume", "%d%%" % int(float(Game.settings.music_volume) * 100.0)],
		["Brightness", "%d%%" % int(float(Game.settings.brightness) * 100.0)],
		["View bobbing", "ON" if Game.settings.view_bobbing else "OFF"],
		["Fancy leaves", "ON" if Game.settings.fancy_leaves else "OFF"],
		["Fullscreen", "ON" if Game.settings.fullscreen else "OFF"],
	]
	var x0 := roundf(size.x * 0.5 - 150 * s)
	for i in rows.size():
		var r := Rect2(Vector2(x0, (24 + i * 14) * s), Vector2(300, 13) * s)
		var hover := r.has_point(get_local_mouse_position())
		draw_rect(r, Color(0.35, 0.35, 0.4, 0.75) if hover else Color(0.18, 0.18, 0.22, 0.7))
		PixelUI.text(self, r.position + Vector2(4, 3) * s, String(rows[i][0]), PixelUI.TEXT, s, false)
		var v: String = rows[i][1]
		PixelUI.text(self, Vector2(r.position.x + r.size.x - (PixelUI.text_width(v) + 6) * s, r.position.y + 3 * s), v, PixelUI.TEXT_YELLOW, s, false)
	var back := _center_button(24 + rows.size() * 14 + 6, "Done", 200)
	PixelUI.button(self, back, "Done", s, back.has_point(get_local_mouse_position()))


func _draw_controls() -> void:
	PixelUI.text_centered(self, size.x * 0.5, 10 * s, "Controls", PixelUI.TEXT, s)
	var rows := [
		["Move", "W A S D"], ["Jump", "Space"], ["Sneak", "Shift"], ["Sprint", "Ctrl"], ["Attack / Mine", "Left Mouse"],
		["Use / Place", "Right Mouse"], ["Pick block", "Middle Mouse"], ["Drop item", "Q"], ["Swap offhand", "F"],
		["Inventory", "E"], ["Chat", "T"], ["Command", "/"], ["Debug screen", "F3"], ["Hide HUD", "F1"],
		["Gamemode", "F4"], ["Admin panel", "F7"], ["Perspective", "F5"], ["Screenshot", "F2"], ["Pause", "Esc"],
	]
	var x0 := roundf(size.x * 0.5 - 150 * s)
	for i in rows.size():
		var y := (24 + i * 11) * s
		PixelUI.text(self, Vector2(x0, y), String(rows[i][0]), PixelUI.TEXT, s, false)
		PixelUI.text(self, Vector2(x0 + 120 * s, y), String(rows[i][1]), PixelUI.TEXT_YELLOW, s, false)
	var back := _center_button(24 + rows.size() * 11 + 8, "Done", 200)
	PixelUI.button(self, back, "Done", s, back.has_point(get_local_mouse_position()))


func _time_ago(t: int) -> String:
	if t <= 0:
		return "never"
	var d := int(Time.get_unix_time_from_system()) - t
	if d < 60:
		return "just now"
	if d < 3600:
		return "%d min ago" % (d / 60)
	if d < 86400:
		return "%d h ago" % (d / 3600)
	return "%d days ago" % (d / 86400)


# ------------------------------------------------------------------------------------------------
func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		var mb: InputEventMouseButton = event
		if mb.button_index == MOUSE_BUTTON_WHEEL_DOWN and view == View.WORLD_LIST:
			_scroll += 1
			return
		if mb.button_index == MOUSE_BUTTON_WHEEL_UP and view == View.WORLD_LIST:
			_scroll = maxi(0, _scroll - 1)
			return
		if mb.button_index != MOUSE_BUTTON_LEFT:
			return
		_click(mb.position)


func _click(p: Vector2) -> void:
	match view:
		View.MAIN:
			if _center_button(64, "").has_point(p):
				view = View.WORLD_LIST
				_refresh_worlds()
			elif _center_button(88, "").has_point(p):
				view = View.SETTINGS
			elif _center_button(112, "").has_point(p):
				view = View.CONTROLS
			elif _center_button(136, "").has_point(p):
				Game.save_settings()
				get_tree().quit()
		View.WORLD_LIST:
			var list := Rect2(Vector2(roundf(size.x * 0.5 - 150 * s), 24 * s), Vector2(300, 120) * s)
			if list.has_point(p):
				var idx := _scroll + int((p.y - list.position.y) / (22.0 * s))
				if idx >= 0 and idx < worlds.size():
					sel = idx
					if Input.is_key_pressed(KEY_SHIFT):
						_play(idx)
				return
			if _center_button(152, "").has_point(p):
				if not worlds.is_empty():
					_play(sel)
			elif _center_button(176, "").has_point(p):
				view = View.CREATE
				_make_fields()
			elif _center_button(200, "").has_point(p):
				if not worlds.is_empty():
					SaveManager.delete_world(String((worlds[sel] as Dictionary).get("folder", "")))
					_refresh_worlds()
			elif _center_button(224, "").has_point(p):
				view = View.MAIN
		View.CREATE:
			if _create_row(0).has_point(p):
				gamemode = Player.SURVIVAL if gamemode == Player.CREATIVE else Player.CREATIVE
			elif _create_row(1).has_point(p):
				difficulty = (difficulty + 1) % 4
			elif _center_button(142, "").has_point(p):
				_create_world()
			elif _center_button(166, "").has_point(p):
				_clear_fields()
				view = View.MAIN
		View.SETTINGS:
			var rows := 10
			var x0 := roundf(size.x * 0.5 - 150 * s)
			for i in rows:
				var r := Rect2(Vector2(x0, (24 + i * 14) * s), Vector2(300, 13) * s)
				if not r.has_point(p):
					continue
				var right := p.x > r.position.x + r.size.x * 0.5
				_step_setting(i, right)
			var back := _center_button(24 + rows * 14 + 6, "", 200)
			if back.has_point(p):
				Game.save_settings()
				view = View.MAIN
		View.CONTROLS:
			var back2 := _center_button(24 + 19 * 11 + 8, "", 200)
			if back2.has_point(p):
				view = View.MAIN


func _step_setting(i: int, right: bool) -> void:
	var d := 1 if right else -1
	match i:
		0:
			Game.settings.render_distance = clampi(int(Game.settings.render_distance) + d, 2, 32)
		1:
			Game.settings.fov = clampf(float(Game.settings.fov) + d * 5.0, 30.0, 110.0)
		2:
			var gs := int(Game.settings.gui_scale) + d
			Game.settings.gui_scale = 0 if gs > 4 else (3 if gs < 0 else gs)
		3:
			Game.settings.mouse_sensitivity = clampf(float(Game.settings.mouse_sensitivity) + d * 0.1, 0.1, 2.0)
		4:
			Game.settings.master_volume = clampf(float(Game.settings.master_volume) + d * 0.1, 0.0, 1.0)
		5:
			Game.settings.music_volume = clampf(float(Game.settings.music_volume) + d * 0.1, 0.0, 1.0)
		6:
			Game.settings.brightness = clampf(float(Game.settings.brightness) + d * 0.1, 0.0, 1.0)
		7:
			Game.settings.view_bobbing = not bool(Game.settings.view_bobbing)
		8:
			Game.settings.fancy_leaves = not bool(Game.settings.fancy_leaves)
		9:
			Game.settings.fullscreen = not bool(Game.settings.fullscreen)
	Game.save_settings()


func _make_fields() -> void:
	if name_edit == null:
		name_edit = LineEdit.new()
		name_edit.add_theme_font_override("font", PixelUI.get_font())
		name_edit.add_theme_color_override("font_color", Color(1, 1, 1))
		name_edit.text = "New World"
		add_child(name_edit)
	if seed_edit == null:
		seed_edit = LineEdit.new()
		seed_edit.add_theme_font_override("font", PixelUI.get_font())
		seed_edit.add_theme_color_override("font_color", Color(1, 1, 1))
		add_child(seed_edit)
	var box := Vector2(roundf(size.x * 0.5 - 130 * s), 0)
	name_edit.position = box + Vector2(8, 40) * s
	name_edit.size = Vector2(240, 14) * s
	name_edit.add_theme_font_size_override("font_size", 8 * s)
	seed_edit.position = box + Vector2(8, 68) * s
	seed_edit.size = Vector2(240, 14) * s
	seed_edit.add_theme_font_size_override("font_size", 8 * s)
	name_edit.visible = true
	seed_edit.visible = true


func _clear_fields() -> void:
	if name_edit != null:
		name_edit.visible = false
	if seed_edit != null:
		seed_edit.visible = false


func _create_world() -> void:
	var nm := "New World"
	if name_edit != null and name_edit.text.strip_edges() != "":
		nm = name_edit.text.strip_edges()
	var sd := 0
	if seed_edit != null and seed_edit.text.strip_edges() != "":
		var t := seed_edit.text.strip_edges()
		sd = int(t) if t.is_valid_int() else hash(t)
	if sd == 0:
		sd = randi()
	_clear_fields()
	Game.start_world({
		"name": nm, "folder": SaveManager.unique_folder(nm), "seed": sd,
		"gamemode": gamemode, "difficulty": difficulty, "gamerules": {}, "player": {},
	})


func _play(idx: int) -> void:
	# the whole level.json goes through (time, weather, portals, dragon state, block palette...)
	var params: Dictionary = (worlds[idx] as Dictionary).duplicate(true)
	var folder := String(params.get("folder", ""))
	var pl: Dictionary = params.get("player", {})
	params["name"] = String(params.get("name", folder))
	params["folder"] = folder
	params["seed"] = int(params.get("seed", 0))
	params["gamemode"] = int(pl.get("gamemode", Player.CREATIVE))
	params["difficulty"] = int(params.get("difficulty", 2))
	params["gamerules"] = params.get("gamerules", {})
	params["player"] = pl
	params["dim"] = int(params.get("dim", 0))
	Game.start_world(params)


func _unhandled_key_input(event: InputEvent) -> void:
	if not (event is InputEventKey) or not event.pressed:
		return
	var k: InputEventKey = event
	if k.keycode == KEY_ESCAPE:
		if view == View.MAIN:
			Game.save_settings()
			get_tree().quit()
		else:
			_clear_fields()
			view = View.MAIN
	elif view == View.CREATE and k.keycode == KEY_ENTER:
		_create_world()
	elif view == View.CREATE and k.keycode == KEY_TAB:
		gamemode = Player.SURVIVAL if gamemode == Player.CREATIVE else Player.CREATIVE
	elif view == View.CREATE and k.keycode == KEY_F6:
		difficulty = (difficulty + 1) % 4
