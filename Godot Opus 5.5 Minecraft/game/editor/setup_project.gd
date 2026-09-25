extends SceneTree
## Reproducible project configuration (input map, autoloads, rendering, shader globals, window).
##   godot --headless --path . --script res://game/editor/setup_project.gd

const ACTIONS := {
	"move_forward": [["key", KEY_W]],
	"move_back": [["key", KEY_S]],
	"move_left": [["key", KEY_A]],
	"move_right": [["key", KEY_D]],
	"jump": [["key", KEY_SPACE]],
	"sprint": [["key", KEY_CTRL]],
	"sneak": [["key", KEY_SHIFT]],
	"attack": [["mouse", MOUSE_BUTTON_LEFT]],
	"use": [["mouse", MOUSE_BUTTON_RIGHT]],
	"pick_block": [["mouse", MOUSE_BUTTON_MIDDLE]],
	"inventory": [["key", KEY_E]],
	"drop_item": [["key", KEY_Q]],
	"swap_hand": [["key", KEY_F]],
	"pause": [["key", KEY_ESCAPE]],
	"debug_overlay": [["key", KEY_F3]],
	"chat": [["key", KEY_T]],
	"command": [["key", KEY_SLASH]],
	"perspective": [["key", KEY_F5]],
	"hide_hud": [["key", KEY_F1]],
	"screenshot": [["key", KEY_F2]],
	"admin_panel": [["key", KEY_F7]],
	"hotbar_next": [["mouse", MOUSE_BUTTON_WHEEL_DOWN]],
	"hotbar_prev": [["mouse", MOUSE_BUTTON_WHEEL_UP]],
	"slot_1": [["key", KEY_1]], "slot_2": [["key", KEY_2]], "slot_3": [["key", KEY_3]],
	"slot_4": [["key", KEY_4]], "slot_5": [["key", KEY_5]], "slot_6": [["key", KEY_6]],
	"slot_7": [["key", KEY_7]], "slot_8": [["key", KEY_8]], "slot_9": [["key", KEY_9]],
	"player_list": [["key", KEY_TAB]],
	"fullscreen": [["key", KEY_F11]],
}

const SHADER_GLOBALS := {
	"daylight": ["float", 1.0],
	"sky_tint": ["vec3", Vector3(1, 1, 1)],
	"ambient_light": ["float", 0.0],
	"gamma_boost": ["float", 0.5],
	"night_vision": ["float", 0.0],
	"block_flicker": ["float", 1.0],
	"world_time": ["float", 0.0],
}


func _init() -> void:
	for a in ACTIONS:
		var events := []
		for e in ACTIONS[a]:
			if e[0] == "key":
				var k := InputEventKey.new()
				k.physical_keycode = e[1]
				events.append(k)
			else:
				var m := InputEventMouseButton.new()
				m.button_index = e[1]
				events.append(m)
		ProjectSettings.set_setting("input/" + a, {"deadzone": 0.2, "events": events})
	for g in SHADER_GLOBALS:
		ProjectSettings.set_setting("shader_globals/" + g, {"type": SHADER_GLOBALS[g][0], "value": SHADER_GLOBALS[g][1]})
	ProjectSettings.set_setting("application/config/name", "Godot Minecraft")
	ProjectSettings.set_setting("application/config/description", "Single-player block sandbox recreation built in Godot 4 with GDScript.")
	ProjectSettings.set_setting("application/run/main_scene", "res://game/main.tscn")
	ProjectSettings.set_setting("application/boot_splash/bg_color", Color(0.1, 0.09, 0.08))
	ProjectSettings.set_setting("application/boot_splash/show_image", false)
	ProjectSettings.set_setting("autoload/Game", "*res://game/core/game.gd")
	ProjectSettings.set_setting("autoload/Sfx", "*res://game/audio/sfx.gd")
	ProjectSettings.set_setting("display/window/size/viewport_width", 1600)
	ProjectSettings.set_setting("display/window/size/viewport_height", 900)
	ProjectSettings.set_setting("display/window/size/mode", 2)
	ProjectSettings.set_setting("display/window/stretch/mode", "disabled")
	ProjectSettings.set_setting("display/window/vsync/vsync_mode", 1)
	ProjectSettings.set_setting("rendering/textures/canvas_textures/default_texture_filter", 0)
	ProjectSettings.set_setting("rendering/anti_aliasing/quality/msaa_3d", 0)
	ProjectSettings.set_setting("rendering/environment/defaults/default_clear_color", Color(0.47, 0.65, 1.0))
	ProjectSettings.set_setting("rendering/lights_and_shadows/directional_shadow/size", 2048)
	ProjectSettings.set_setting("rendering/driver/threads/thread_model", 1)
	ProjectSettings.set_setting("physics/common/physics_ticks_per_second", 60)
	ProjectSettings.set_setting("gui/common/snap_controls_to_pixels", true)
	ProjectSettings.set_setting("audio/general/default_playback_type", 0)
	for w in ["integer_division", "narrowing_conversion", "unused_variable", "unused_local_constant", "unused_private_class_variable",
			"unused_parameter", "unused_signal", "shadowed_variable", "shadowed_variable_base_class", "shadowed_global_identifier",
			"return_value_discarded", "untyped_declaration", "unsafe_property_access", "unsafe_method_access", "unsafe_cast",
			"unsafe_call_argument", "confusable_local_declaration", "confusable_local_usage", "inference_on_variant",
			"static_called_on_instance", "redundant_await", "incompatible_ternary"]:
		ProjectSettings.set_setting("debug/gdscript/warnings/" + w, 0)
	ProjectSettings.set_setting("editor_plugins/enabled", PackedStringArray(["res://addons/opus55_tools/plugin.cfg"]))
	var err := ProjectSettings.save()
	print("project settings saved: ", error_string(err))
	quit(0 if err == OK else 1)
