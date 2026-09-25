extends Node
## Global game state autoload ("Game"): registries bootstrap, settings, current world session,
## scene switching between title screen and world.

signal settings_changed

const SETTINGS_PATH := "user://settings.cfg"
const VERSION := "1.0 (Minecraft Java 26.2-style recreation)"

var registries_ready := false
var settings := {
	"render_distance": 8,
	"fov": 70.0,
	"mouse_sensitivity": 0.5,
	"gui_scale": 0,          # 0 = auto
	"master_volume": 0.8,
	"music_volume": 0.5,
	"brightness": 0.5,
	"fancy_leaves": true,
	"view_bobbing": true,
	"fullscreen": false,
	"clouds": true,
	"invert_mouse": false,
}

# current session
var world_name := ""
var world_seed := 0
var pending_world: Dictionary = {}   # parameters for the world scene to create/load
var session = null                   # WorldSession (set by the world scene)
var autotest := ""                   # command-line automation scenario
var cmdline := {}


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_parse_cmdline()
	load_settings()
	_ensure_shader_globals()
	init_registries()
	apply_window_settings()


func _parse_cmdline() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--"):
			var kv := a.substr(2).split("=", true, 1)
			cmdline[kv[0]] = kv[1] if kv.size() > 1 else "true"
	autotest = cmdline.get("autotest", "")


func init_registries() -> void:
	if registries_ready:
		return
	var t0 := Time.get_ticks_msec()
	BlockDB.init_registry()
	BiomeDB.init()
	TextureLibrary.load_blocks()
	Mesher.init_tables()
	MeshModels.init()
	ItemDB.init()
	MobDB.init()
	_register_spawn_eggs()
	RecipeDB.init()
	LootDB.init()
	EffectDB.init()
	EnchantDB.init()
	BlockBehaviors.init()
	RedstoneSystem.init()
	Trees.init()
	registries_ready = true
	print("[Game] registries ready in %d ms (%d blocks)" % [Time.get_ticks_msec() - t0, BlockDB.count])


## One spawn egg per mob (bosses included, for test spawning from the creative catalogue).
func _register_spawn_eggs() -> void:
	for mob in MobDB.order:
		var cols: Array = MobDB.EGG_COLORS.get(mob, ["#888888", "#444444"])
		ItemDB.register_spawn_egg(mob, Color(String(cols[0])), Color(String(cols[1])), MobDB.display(mob))
	ItemDB.rebuild_tabs()


func _class_script(cls: String) -> Script:
	for e in ProjectSettings.get_global_class_list():
		if e["class"] == cls:
			return load(e["path"])
	return null


func _ensure_shader_globals() -> void:
	var defaults := {
		"daylight": [RenderingServer.GLOBAL_VAR_TYPE_FLOAT, 1.0],
		"sky_tint": [RenderingServer.GLOBAL_VAR_TYPE_VEC3, Vector3.ONE],
		"ambient_light": [RenderingServer.GLOBAL_VAR_TYPE_FLOAT, 0.0],
		"gamma_boost": [RenderingServer.GLOBAL_VAR_TYPE_FLOAT, 0.5],
		"night_vision": [RenderingServer.GLOBAL_VAR_TYPE_FLOAT, 0.0],
		"block_flicker": [RenderingServer.GLOBAL_VAR_TYPE_FLOAT, 1.0],
		"world_time": [RenderingServer.GLOBAL_VAR_TYPE_FLOAT, 0.0],
	}
	# project.godot declares them (see game/editor/setup_project.gd); only add when missing
	for k in defaults:
		if not ProjectSettings.has_setting("shader_globals/" + k):
			RenderingServer.global_shader_parameter_add(k, defaults[k][0], defaults[k][1])
	RenderingServer.global_shader_parameter_set("gamma_boost", float(settings.brightness))


func load_settings() -> void:
	var cf := ConfigFile.new()
	if cf.load(SETTINGS_PATH) == OK:
		for k in settings:
			settings[k] = cf.get_value("settings", k, settings[k])


func save_settings() -> void:
	var cf := ConfigFile.new()
	for k in settings:
		cf.set_value("settings", k, settings[k])
	cf.save(SETTINGS_PATH)
	settings_changed.emit()
	RenderingServer.global_shader_parameter_set("gamma_boost", float(settings.brightness))
	apply_window_settings()


func apply_window_settings() -> void:
	if DisplayServer.get_name() == "headless":
		return
	var want_fs: bool = settings.fullscreen
	var mode := DisplayServer.window_get_mode()
	var is_fs := mode == DisplayServer.WINDOW_MODE_FULLSCREEN or mode == DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN
	if want_fs and not is_fs:
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
	elif not want_fs and is_fs:
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
	var bus := AudioServer.get_bus_index("Master")
	AudioServer.set_bus_volume_db(bus, linear_to_db(maxf(float(settings.master_volume), 0.0001)))


func gui_scale() -> int:
	var s: int = settings.gui_scale
	if s > 0:
		return s
	var vs := get_viewport().get_visible_rect().size if get_viewport() != null else Vector2(1600, 900)
	var auto := 1
	while (auto + 1) * 320 <= vs.x and (auto + 1) * 240 <= vs.y and auto < 4:
		auto += 1
	return auto


func start_world(params: Dictionary) -> void:
	pending_world = params
	get_tree().change_scene_to_file("res://game/main/world_scene.tscn")


func to_title() -> void:
	session = null
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	get_tree().change_scene_to_file("res://game/main.tscn")


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("fullscreen"):
		settings.fullscreen = not settings.fullscreen
		save_settings()
