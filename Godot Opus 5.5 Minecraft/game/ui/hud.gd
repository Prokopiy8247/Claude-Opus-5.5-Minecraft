class_name Hud
extends CanvasLayer
## In-game heads-up display: crosshair, hotbar + offhand, hearts, hunger, armour, air bubbles, XP
## bar, status effects, boss bars, chat + action bar, sleep/travel covers, the block breaking
## overlay and the F3 debug screen. Everything is drawn with the original pixel toolkit.

const HEART := 9

var session = null
var visible_hud := true
var debug := false

var hotbar_sel := 0
var sel_anim := 0.0
var hearts_shake := 0.0
var chat_lines: Array = []           # [text, color, age]
var action_text := ""
var action_time := 0.0
var boss_bars := {}
var sleep_overlay := 0.0
var travel_alpha := 0.0
var travel_label := ""
var break_pos := Vector3i.ZERO
var break_stage := -1
var _sky_color := Color(0.47, 0.65, 1.0)
var _sky_dim := 0
var _sky_day := 1.0
var _sky_weather := 0
var _crack: Texture2D = null
var _drawn_stage := -1
var _fps_hist := PackedFloat32Array()
var _last_health := 20.0
var _hurt_flash := 0.0
var _chat_ticks := 0


func _ready() -> void:
	layer = 5
	var root := Control.new()
	root.name = "HudRoot"
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.draw.connect(_draw_hud)
	add_child(root)
	root.set_meta("hud", self)
	set_process(true)


func _process(delta: float) -> void:
	var root: Control = get_child(0) if get_child_count() > 0 else null
	if root == null:
		return
	root.modulate = Color(1, 1, 1, 1 if visible_hud else 0)
	# hotbar selection glow
	if session != null and session.player != null:
		hotbar_sel = session.player.inventory.selected
	sel_anim = lerpf(sel_anim, float(hotbar_sel), minf(1.0, delta * 18.0))
	if hearts_shake > 0.0:
		hearts_shake = maxf(0.0, hearts_shake - delta * 4.0)
	if _hurt_flash > 0.0:
		_hurt_flash = maxf(0.0, _hurt_flash - delta * 2.0)
	if action_time > 0.0:
		action_time = maxf(0.0, action_time - delta)
	_chat_ticks += 1
	for i in range(chat_lines.size() - 1, -1, -1):
		var c: Array = chat_lines[i]
		c[2] = int(c[2]) + 1
		if int(c[2]) > 200:
			chat_lines.remove_at(i)
	if debug:
		_fps_hist.append(Engine.get_frames_per_second())
		if _fps_hist.size() > 120:
			_fps_hist.remove_at(0)
	root.queue_redraw()


# ------------------------------------------------------------------------------------------------
# Called by the session
func update_sky(dim: int, day_factor: float, weather: int, _day_time: int) -> void:
	_sky_dim = dim
	_sky_day = day_factor
	_sky_weather = weather


func update_stats() -> void:
	return


func add_chat(text: String, color := Color(1, 1, 1)) -> void:
	chat_lines.append([text, color, 0])
	if chat_lines.size() > 10:
		chat_lines.remove_at(0)


func set_action_bar(text: String, seconds := 2.0) -> void:
	action_text = text
	action_time = seconds


func set_boss_bars(bars: Dictionary) -> void:
	boss_bars = bars.duplicate()


func set_sleep_overlay(on: bool) -> void:
	sleep_overlay = 1.0 if on else 0.0


func set_travel_overlay(stage: float, label: String) -> void:
	travel_alpha = clampf(stage, 0.0, 1.0)
	travel_label = label


func set_breaking(pos: Vector3i, stage: int) -> void:
	break_pos = pos
	break_stage = stage


func flash_hurt() -> void:
	_hurt_flash = 1.0
	hearts_shake = 1.0


func toggle_debug() -> void:
	debug = not debug


# ------------------------------------------------------------------------------------------------
func _scale() -> int:
	var vs := Vector2(1280, 720)
	if get_viewport() != null:
		vs = get_viewport().get_visible_rect().size
	var s := 2
	while (s + 1) * 320 <= vs.x and (s + 1) * 240 <= vs.y and s < 4:
		s += 1
	return s


func _draw_hud() -> void:
	if not visible_hud or session == null or session.player == null:
		return
	var root: Control = get_child(0)
	var size := root.size
	var s := _scale()
	_draw_breaking(root, s)
	_draw_sleep(root, size)
	if travel_alpha > 0.02:
		root.draw_rect(Rect2(Vector2.ZERO, size), Color(0.12, 0.02, 0.18, travel_alpha * 0.95))
		if travel_label != "":
			PixelUI.text_centered(root, size.x * 0.5, size.y * 0.5, travel_label, Color(1, 0.85, 1.0), s)
	if _hurt_flash > 0.0:
		root.draw_rect(Rect2(Vector2.ZERO, size), Color(0.6, 0.0, 0.0, _hurt_flash * 0.35))
	var pl = session.player
	_draw_crosshair(root, size, s, pl)
	_draw_hotbar(root, size, s, pl)
	_draw_vitals(root, size, s, pl)
	_draw_boss_bars(root, size, s)
	_draw_chat(root, size, s)
	if action_text != "" and action_time > 0.0:
		PixelUI.text_centered(root, size.x * 0.5, size.y - 68 * s * 0.5 - 6 * s, action_text, PixelUI.TEXT, s)
	if debug:
		_draw_debug(root, size, s, pl)


# ------------------------------------------------------------------------------------------------
func _draw_crosshair(root: Control, size: Vector2, s: int, pl) -> void:
	if pl.interact != null and pl.interact.using_bow:
		return
	var c := size * 0.5
	var tex := PixelUI.sprite("crosshair", Color(1, 1, 1, 0.85), Color(1, 1, 1, 0.5), Color(0, 0, 0, 0.4), Color(1, 1, 1, 0.95))
	# the attack cooldown widens the crosshair like the modern client
	var spread := 0.0
	if pl.interact != null:
		spread = (1.0 - clampf(pl.interact.attack_strength(), 0.0, 1.0)) * 3.0
	var w := 4.5 * s
	root.draw_texture_rect(tex, Rect2(c - Vector2(w, w) - Vector2(spread * s, 0), Vector2(4.5, 4.5) * s), false)
	root.draw_texture_rect(tex, Rect2(c - Vector2(w, w) + Vector2(spread * s, 0), Vector2(4.5, 4.5) * s), false)


func _slot_frame(p: Vector2, sel: bool, s: int) -> void:
	var ci: Control = get_child(0)
	PixelUI.slot(ci, p, s)
	if sel:
		PixelUI.slot(ci, p, s)


func _draw_hotbar(root: Control, size: Vector2, s: int, pl) -> void:
	var inv = pl.inventory
	var w := 182 * s
	var x := roundf(size.x * 0.5 - w * 0.5)
	var y := size.y - 22 * s
	var sel: int = pl.inventory.selected
	# background strip
	root.draw_rect(Rect2(x, y, float(w), 22.0 * s), Color(0, 0, 0, 0.35))
	root.draw_rect(Rect2(x, y, float(w), float(s)), Color(1, 1, 1, 0.18))
	for i in 9:
		var r := Rect2(Vector2(x + (1 + i * 20) * s, y + s), Vector2(20, 20) * s)
		root.draw_rect(r, Color(0.55, 0.55, 0.55, 0.65) if i == sel else Color(0.3, 0.3, 0.3, 0.6))
		root.draw_rect(Rect2(r.position, Vector2(r.size.x, s)), Color(0, 0, 0, 0.4))
		root.draw_rect(Rect2(r.position, Vector2(s, r.size.y)), Color(0, 0, 0, 0.4))
		var st: ItemStack = inv.stack_at(i)
		PixelUI.item(root, r.position + Vector2(2, 2) * s, st, s)
	# the selected slot pops out
	var sx := x + (1 + sel_anim * 20.0) * s - 2 * s
	var sr := Rect2(roundf(sx), y - s, 24 * s, 24 * s)
	root.draw_rect(sr, Color(0, 0, 0, 0.9), false, s)
	root.draw_rect(sr.grow(-s), Color(1, 1, 1, 0.95), false, s)
	# offhand slot on the left
	var off: ItemStack = inv.stack_at(Inventory.OFFHAND)
	var or_ := Rect2(Vector2(x - 26 * s, y + 1 * s), Vector2(20, 20) * s)
	root.draw_rect(or_, Color(0.3, 0.3, 0.3, 0.6))
	root.draw_rect(Rect2(or_.position, Vector2(or_.size.x, s)), Color(0, 0, 0, 0.4))
	PixelUI.item(root, or_.position + Vector2(2, 2) * s, off, s)


func _draw_vitals(root: Control, size: Vector2, s: int, pl) -> void:
	var stats = pl.stats
	var x0 := roundf(size.x * 0.5 - 91 * s)
	var y := size.y - 39 * s
	var creative: bool = pl.gamemode == Player.CREATIVE
	var shake := int(round(sin(Time.get_ticks_msec() * 0.03) * hearts_shake * 2.0))
	var extra_rows := 0
	if not creative:
		# hearts: red (green when poisoned, black when withering), a second row for health boost
		# and golden absorption hearts
		var hc := Color(0.85, 0.1, 0.12)
		var hd := Color(0.35, 0.05, 0.05)
		if pl.effects.has("poison"):
			hc = Color(0.55, 0.62, 0.12)
			hd = Color(0.25, 0.3, 0.05)
		elif pl.effects.has("wither"):
			hc = Color(0.18, 0.15, 0.15)
			hd = Color(0.05, 0.04, 0.04)
		var hearts_n := int(ceil(stats.max_health / 2.0))
		var abs_n := int(ceil(stats.absorption / 2.0))
		for i in hearts_n + abs_n:
			var row := i / 10
			var col := i % 10
			var p := Vector2(x0 + col * 8 * s + shake * s, y - row * 10 * s)
			if i < hearts_n:
				var v: float = stats.health - i * 2.0
				if v >= 2.0:
					root.draw_texture_rect(PixelUI.sprite("heart", hc, hd), Rect2(p, Vector2(9, 9) * s), false)
				elif v >= 1.0:
					root.draw_texture_rect(PixelUI.sprite("heart_half", hc, hd), Rect2(p, Vector2(9, 9) * s), false)
				else:
					root.draw_texture_rect(PixelUI.sprite("heart_empty", Color(0.25, 0.25, 0.25), Color(0.12, 0.12, 0.12)), Rect2(p, Vector2(9, 9) * s), false)
			else:
				var av2: float = stats.absorption - (i - hearts_n) * 2.0
				root.draw_texture_rect(PixelUI.sprite("heart" if av2 >= 2.0 else "heart_half", Color(0.95, 0.75, 0.15), Color(0.55, 0.38, 0.05)),
					Rect2(p, Vector2(9, 9) * s), false)
		extra_rows = (hearts_n + abs_n - 1) / 10
		# hunger (right aligned, right to left)
		var hx := roundf(size.x * 0.5 + 91 * s - 9 * s)
		for i in 10:
			var fv: int = stats.food - i * 2
			var p2 := Vector2(hx - i * 8 * s, y)
			if fv >= 2:
				root.draw_texture_rect(PixelUI.sprite("food", Color(0.72, 0.42, 0.12), Color(0.4, 0.2, 0.05)), Rect2(p2, Vector2(9, 9) * s), false)
			elif fv >= 1:
				root.draw_texture_rect(PixelUI.sprite("food_half", Color(0.72, 0.42, 0.12), Color(0.4, 0.2, 0.05)), Rect2(p2, Vector2(9, 9) * s), false)
			else:
				root.draw_texture_rect(PixelUI.sprite("food_empty", Color(0.3, 0.3, 0.3), Color(0.14, 0.14, 0.14)), Rect2(p2, Vector2(9, 9) * s), false)
	# armour row above the hearts
	var ap: int = stats.armor_points()
	if ap > 0 and not creative:
		for i in 10:
			var av: int = ap - i * 2
			var p3 := Vector2(x0 + i * 8 * s, y - (10 + extra_rows * 10) * s)
			if av >= 2:
				root.draw_texture_rect(PixelUI.sprite("armor", Color(0.85, 0.8, 0.85), Color(0.35, 0.32, 0.4)), Rect2(p3, Vector2(9, 9) * s), false)
			elif av == 1:
				root.draw_texture_rect(PixelUI.sprite("armor_half", Color(0.85, 0.8, 0.85), Color(0.35, 0.32, 0.4)), Rect2(p3, Vector2(9, 9) * s), false)
			elif i < 10 and av <= -1:
				root.draw_texture_rect(PixelUI.sprite("armor_empty", Color(0.25, 0.25, 0.28), Color(0.12, 0.12, 0.14)), Rect2(p3, Vector2(9, 9) * s), false)
	# air bubbles when underwater
	if stats.air < 300:
		var bubbles := int(ceil(float(stats.air) / 30.0))
		var bx := roundf(size.x * 0.5 + 91 * s - 9 * s)
		for i in 10:
			var p4 := Vector2(bx - i * 8 * s, y - 10 * s)
			if i < bubbles:
				root.draw_texture_rect(PixelUI.sprite("bubble", Color(0.85, 0.9, 1.0), Color(0.3, 0.4, 0.6)), Rect2(p4, Vector2(9, 9) * s), false)
			elif i == bubbles:
				root.draw_texture_rect(PixelUI.sprite("bubble_pop", Color(0.9, 0.95, 1.0), Color(0.3, 0.4, 0.6)), Rect2(p4, Vector2(9, 9) * s), false)
	# XP bar
	if not creative:
		var bw := 182 * s
		var bx2 := roundf(size.x * 0.5 - bw * 0.5)
		var by := size.y - 29 * s
		root.draw_rect(Rect2(bx2, by, bw, 5 * s), Color(0, 0, 0, 0.75))
		root.draw_rect(Rect2(bx2 + s, by + s, (bw - 2 * s) * float(stats.xp_progress), 3 * s), Color(0.55, 0.95, 0.2))
		if stats.xp_level > 0:
			PixelUI.text_centered(root, size.x * 0.5, by - 8 * s, str(stats.xp_level), Color(0.55, 1.0, 0.2), s)
	# status effects (top right)
	var y2 := 4 * s
	for e in pl.effects:
		var d: Dictionary = pl.effects[e]
		var label := _effect_label(String(e), int(d.amp))
		var w := PixelUI.text_width(label) * s + 28 * s
		root.draw_rect(Rect2(size.x - w - 4 * s, y2, w, 14 * s), Color(0, 0, 0, 0.5))
		var icon := ItemStack.of(_effect_icon(String(e)), 1)
		PixelUI.item(root, Vector2(size.x - w - 2 * s, y2 + s * 0.5), icon, s, false)
		PixelUI.text(root, Vector2(size.x - w + 16 * s, y2 + 2 * s), label, PixelUI.TEXT, s)
		y2 += 16 * s


func _effect_label(e: String, amp: int) -> String:
	var n := e.replace("_", " ").capitalize()
	return "%s%s" % [n, "" if amp <= 0 else " " + str(amp + 1)]


func _effect_icon(e: String) -> String:
	match e:
		"speed":
			return "sugar"
		"slowness":
			return "soul_lantern"
		"haste", "mining_fatigue":
			return "golden_pickaxe"
		"strength":
			return "blaze_powder"
		"jump_boost":
			return "rabbit_foot"
		"regeneration":
			return "ghast_tear"
		"fire_resistance":
			return "magma_cream"
		"water_breathing":
			return "pufferfish"
		"night_vision":
			return "golden_carrot"
		"invisibility":
			return "glass"
		"poison":
			return "spider_eye"
		"wither":
			return "wither_skeleton_skull"
		"absorption":
			return "golden_apple"
		"levitation":
			return "shulker_shell"
		"slow_falling":
			return "phantom_membrane"
		"conduit_power":
			return "nautilus_shell"
		"dolphins_grace":
			return "cod"
	return "potion"


func _draw_boss_bars(root: Control, size: Vector2, s: int) -> void:
	var y := 6 * s
	for k in boss_bars:
		var b: Dictionary = boss_bars[k]
		var w := 182 * s
		var x := roundf(size.x * 0.5 - w * 0.5)
		PixelUI.text_centered(root, size.x * 0.5, y, String(b["name"]), PixelUI.TEXT, s)
		y += 10 * s
		root.draw_rect(Rect2(x - s, y - s, w + 2 * s, 7 * s), Color(0, 0, 0, 0.9))
		var frac := clampf(float(b["hp"]) / maxf(1.0, float(b["max"])), 0.0, 1.0)
		var col := Color(0.75, 0.15, 0.65)
		if String(b["name"]).to_lower().begins_with("wither"):
			col = Color(0.42, 0.42, 0.45)
		root.draw_rect(Rect2(x, y, w * frac, 5 * s), col)
		y += 14 * s


func _draw_chat(root: Control, size: Vector2, s: int) -> void:
	if chat_lines.is_empty():
		return
	var y := size.y - 62 * s
	var start := maxi(0, chat_lines.size() - 10)
	var shown := []
	for i in range(start, chat_lines.size()):
		shown.append(chat_lines[i])
	var total := shown.size() * 10 * s
	var yy := size.y - 48 * s - total
	for c in shown:
		var age := int(c[2])
		var alpha := 1.0
		if chat_lines.size() >= 10 and age > 160:
			alpha = clampf(1.0 - float(age - 160) / 40.0, 0.0, 1.0)
		var col: Color = c[1]
		var text := String(c[0])
		var w := PixelUI.text_width(text) * s
		root.draw_rect(Rect2(4 * s, yy - s, w + 4 * s, 10 * s), Color(0, 0, 0, 0.42 * alpha))
		PixelUI.text(root, Vector2(6 * s, yy), text, Color(col.r, col.g, col.b, alpha), s)
		yy += 10 * s


# ------------------------------------------------------------------------------------------------
# Block breaking overlay: project the block's faces into screen space and draw the crack texture.
func _crack_texture(stage: int) -> Texture2D:
	if _crack != null and _drawn_stage == stage:
		return _crack
	var img := TextureLibrary.tile("destroy_stage_%d" % clampi(stage, 0, 9))
	if img == null:
		return null
	_crack = ImageTexture.create_from_image(img)
	_drawn_stage = stage
	return _crack


func _draw_breaking(root: Control, s: int) -> void:
	if break_stage < 0 or session == null or session.world == null:
		return
	var cam: Camera3D = session.player.camera
	if cam == null:
		return
	var stage := clampi(int(float(break_stage) / 10.0 * 10.0), 0, 9)
	var tex := _crack_texture(stage)
	if tex == null:
		return
	var b := break_pos
	var w: World = session.world
	var shape: Array = BlockShapes.collision(w.get_blockv(b), w, b.x, b.y, b.z)
	if shape.is_empty():
		shape = [AABB(Vector3.ZERO, Vector3.ONE)]
	# draw each collision box as its 6 faces projected through the camera
	for bx in shape:
		var ab: AABB = bx
		_draw_box_faces(root, cam, ab.position + Vector3(b), ab.position + ab.size + Vector3(b), tex)


func _draw_box_faces(root: Control, cam: Camera3D, lo: Vector3, hi: Vector3, tex: Texture2D) -> void:
	var vp := cam.get_viewport()
	var size := Vector2(1280, 720)
	if vp != null:
		size = vp.get_visible_rect().size
	var faces := [
		[[hi.x, lo.y, lo.z], [hi.x, hi.y, lo.z], [hi.x, hi.y, hi.z], [hi.x, lo.y, hi.z]],   # +X
		[[lo.x, lo.y, hi.z], [lo.x, hi.y, hi.z], [lo.x, hi.y, lo.z], [lo.x, lo.y, lo.z]],   # -X
		[[lo.x, hi.y, lo.z], [lo.x, hi.y, hi.z], [hi.x, hi.y, hi.z], [hi.x, hi.y, lo.z]],   # +Y
		[[lo.x, lo.y, hi.z], [lo.x, lo.y, lo.z], [hi.x, lo.y, lo.z], [hi.x, lo.y, hi.z]],   # -Y
		[[hi.x, lo.y, hi.z], [hi.x, hi.y, hi.z], [lo.x, hi.y, hi.z], [lo.x, lo.y, hi.z]],   # +Z
		[[lo.x, lo.y, lo.z], [lo.x, hi.y, lo.z], [hi.x, hi.y, lo.z], [hi.x, lo.y, lo.z]],   # -Z
	]
	for f in faces:
		var pts := PackedVector2Array()
		var ok := true
		var behind := false
		for p in f:
			var v := Vector3(float(p[0]), float(p[1]), float(p[2]))
			var clip := cam.unproject_position(v)
			if not cam.is_position_behind(v):
				pts.append(clip)
			else:
				behind = true
		if behind or pts.size() != 4:
			ok = false
		if not ok:
			continue
		# two triangles with the crack texture
		root.draw_polygon(PackedVector2Array([pts[0], pts[1], pts[2]]), PackedColorArray([Color(1, 1, 1, 0.82), Color(1, 1, 1, 0.82), Color(1, 1, 1, 0.82)]), PackedVector2Array([Vector2(0, 1), Vector2(0, 0), Vector2(1, 0)]), tex)
		root.draw_polygon(PackedVector2Array([pts[0], pts[2], pts[3]]), PackedColorArray([Color(1, 1, 1, 0.82), Color(1, 1, 1, 0.82), Color(1, 1, 1, 0.82)]), PackedVector2Array([Vector2(0, 1), Vector2(1, 0), Vector2(1, 1)]), tex)


func _draw_sleep(root: Control, size: Vector2) -> void:
	if sleep_overlay <= 0.0:
		return
	root.draw_rect(Rect2(Vector2.ZERO, size), Color(0, 0, 0, 0.86))
	PixelUI.text_centered(root, size.x * 0.5, size.y * 0.5 - 8 * _scale(), "Sleeping...", Color(0.8, 0.8, 0.85), _scale())


# ------------------------------------------------------------------------------------------------
func _draw_debug(root: Control, size: Vector2, s: int, pl) -> void:
	var lines := []
	var w: World = session.world
	var pos: Vector3 = pl.body.pos
	var fps := Engine.get_frames_per_second()
	var avg := 0.0
	for f in _fps_hist:
		avg += f
	if not _fps_hist.is_empty():
		avg /= float(_fps_hist.size())
	var biome := "?"
	if w != null and w.is_ready_at(floori(pos.x), floori(pos.z)):
		biome = BiomeDB.name_of(w.biome_at(floori(pos.x), floori(pos.y), floori(pos.z))).replace("_", " ")
	lines.append("Godot Minecraft  %s" % Game.VERSION)
	lines.append("%d fps (avg %.1f)  draw calls %d" % [fps, avg, Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)])
	lines.append("XYZ: %.3f / %.5f / %.3f" % [pos.x, pos.y, pos.z])
	lines.append("Block: %d %d %d   Chunk: %d %d" % [floori(pos.x), floori(pos.y), floori(pos.z), floori(pos.x) / 16, floori(pos.z) / 16])
	lines.append("Facing: %s  (%.1f / %.1f)" % [_facing(pl.yaw), fposmod(-pl.yaw * 180.0 / PI, 360.0), pl.pitch * 180.0 / PI])
	lines.append("Biome: %s   Dimension: %s" % [biome, String(DimensionDB.get_def(session.dim).display)])
	if w != null:
		var l := w.get_light(floori(pos.x), floori(pos.y), floori(pos.z))
		var sky := w.light_level(floori(pos.x), floori(pos.y), floori(pos.z), session.sky_darken())
		lines.append("Light: %d (sky %d, block %d)  Day: %d  Weather: %s" % [sky, l.x, l.y, session.day_time, ["clear", "rain", "thunder"][clampi(session.weather, 0, 2)]])
		lines.append("Chunks: %d loaded, %d pending  Entities: %d  Particles: %d" % [w.cm.chunks.size(), w.cm.pending_work(), session.stats.entities, 0])
	lines.append("Velocity: %.3f %.3f %.3f  Ground: %s" % [pl.body.vel.x, pl.body.vel.y, pl.body.vel.z, str(pl.body.on_ground)])
	lines.append("Mode: %s   Seed: %d   Targeted: %s" % [_mode_name(pl.gamemode), session.seed_value, session.structures_at(w, Vector3i(floori(pos.x), floori(pos.y) - 1, floori(pos.z)))])
	var y := 6 * s
	for l2 in lines:
		var txt := String(l2)
		root.draw_rect(Rect2(4 * s, y - 2, PixelUI.text_width(txt) * s + 4 * s, 11 * s), Color(0, 0, 0, 0.4))
		PixelUI.text(root, Vector2(6 * s, y), txt, PixelUI.TEXT, s, false)
		y += 10 * s


func _facing(yaw: float) -> String:
	var d := fposmod(-yaw * 180.0 / PI, 360.0)
	if d < 45.0 or d >= 315.0:
		return "south"
	if d < 135.0:
		return "west"
	if d < 225.0:
		return "north"
	return "east"


func _mode_name(m: int) -> String:
	match m:
		Player.CREATIVE:
			return "Creative"
		Player.SPECTATOR:
			return "Spectator"
		_:
			return "Survival"
