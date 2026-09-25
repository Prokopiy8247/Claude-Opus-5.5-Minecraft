extends Node
## Audio autoload ("Sfx"): lazily synthesises and caches every sound, plays positional sounds from
## a player pool and UI sounds from a 2D pool. Also runs the generative ambient music.

const BLOCK_SOUNDS := {
	# group: [tone, decay, grit, pitch, extra]
	"stone": [0.5, 0.09, 0.35, 1.0, ""], "deepslate": [0.4, 0.1, 0.35, 0.85, ""], "tuff": [0.45, 0.09, 0.3, 0.9, ""],
	"wood": [0.22, 0.13, 0.1, 0.8, "thump"], "nether_wood": [0.25, 0.12, 0.1, 0.8, "thump"], "bamboo_wood": [0.3, 0.1, 0.1, 1.1, "thump"],
	"grass": [0.85, 0.11, 0.05, 1.2, ""], "crop": [0.9, 0.08, 0.05, 1.3, ""], "vine": [0.8, 0.1, 0.05, 1.2, ""],
	"moss": [0.6, 0.12, 0.0, 0.9, ""], "gravel": [0.55, 0.12, 0.9, 1.0, ""], "sand": [0.62, 0.07, 0.2, 1.1, ""],
	"soul_sand": [0.4, 0.14, 0.2, 0.8, ""], "glass": [1.0, 0.06, 0.6, 1.4, "ping"], "wool": [0.12, 0.09, 0.0, 0.8, ""],
	"snow": [0.72, 0.08, 0.35, 1.2, ""], "metal": [0.7, 0.08, 0.2, 1.0, "ring"], "copper": [0.65, 0.09, 0.2, 1.0, "ring"],
	"chain": [0.8, 0.07, 0.4, 1.2, "ring"], "lantern": [0.75, 0.08, 0.3, 1.1, "ring"], "anvil": [0.6, 0.1, 0.2, 0.8, "ring"],
	"slime": [0.3, 0.15, 0.0, 0.7, "squish"], "honey": [0.25, 0.18, 0.0, 0.7, "squish"], "mud": [0.3, 0.14, 0.1, 0.7, "squish"],
	"netherrack": [0.45, 0.07, 0.5, 0.9, ""], "nylium": [0.5, 0.08, 0.3, 0.9, ""], "basalt": [0.42, 0.09, 0.35, 0.8, ""],
	"amethyst": [0.9, 0.12, 0.1, 1.4, "chime"], "sculk": [0.35, 0.13, 0.1, 0.8, "squish"], "wart": [0.4, 0.1, 0.05, 0.9, ""],
	"coral": [0.55, 0.08, 0.3, 1.1, ""], "ladder": [0.25, 0.1, 0.1, 1.0, "thump"], "scaffold": [0.3, 0.08, 0.1, 1.2, "thump"],
	"bone": [0.6, 0.07, 0.4, 1.3, ""], "cobweb": [0.8, 0.1, 0.0, 1.0, ""],
}

var _cache: Dictionary = {}
var _pool3d: Array = []
var _pool2d: Array = []
var _next3d := 0
var _next2d := 0
var _music: AudioStreamPlayer
var _music_timer := 30.0
var _rng := RandomNumberGenerator.new()
var listener_pos := Vector3.ZERO
var enabled := true


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	if DisplayServer.get_name() == "headless":
		enabled = false
	for i in 24:
		var a := AudioStreamPlayer3D.new()
		a.max_distance = 32.0
		a.unit_size = 6.0
		a.attenuation_model = AudioStreamPlayer3D.ATTENUATION_INVERSE_DISTANCE
		add_child(a)
		_pool3d.append(a)
	for i in 8:
		var b := AudioStreamPlayer.new()
		add_child(b)
		_pool2d.append(b)
	_music = AudioStreamPlayer.new()
	_music.volume_db = -12.0
	add_child(_music)
	_rng.randomize()


func get_stream(key: String) -> AudioStream:
	if _cache.has(key):
		return _cache[key]
	var s := _make(key)
	_cache[key] = s
	return s


func _make(key: String) -> AudioStream:
	var h := hash(key)
	var parts := key.split(":")
	if parts.size() >= 3 and parts[0] == "block":
		var g: String = parts[1]
		var kind: String = parts[2]
		var variant := int(parts[3]) if parts.size() > 3 else 0
		var p: Array = BLOCK_SOUNDS.get(g, BLOCK_SOUNDS["stone"])
		var dur := 0.18
		var dec: float = p[1]
		match kind:
			"break":
				dur = 0.32
				dec *= 1.8
			"place":
				dur = 0.22
			"hit":
				dur = 0.12
				dec *= 0.6
			"step":
				dur = 0.1
				dec *= 0.5
		var s := SoundSynth.noise_burst(dur, p[0], dec, h + variant * 7919, p[2], p[3] * (1.0 + variant * 0.06))
		return s
	match key:
		"player_hurt": return SoundSynth.voice(0.28, 210.0, 130.0, 650.0, 0.3, h, 0.9)
		"hurt_fall": return SoundSynth.noise_burst(0.18, 0.3, 0.07, h, 0.2, 0.8)
		"hurt_fall_big": return SoundSynth.noise_burst(0.3, 0.25, 0.12, h, 0.3, 0.7)
		"pop": return SoundSynth.tone(0.09, 900.0, 1700.0, 0, 0.35, 0.05)
		"click": return SoundSynth.tone(0.06, 1400.0, 1000.0, 2, 0.3, 0.03)
		"levelup": return SoundSynth.chord([523.0, 659.0, 784.0, 1047.0], 1.2, 0.5)
		"xp": return SoundSynth.tone(0.12, 1800.0 + (h % 400), 2400.0, 0, 0.18, 0.07)
		"explode": return SoundSynth.explosion(h)
		"fuse": return SoundSynth.fuse(h)
		"bow": return SoundSynth.pluck(180.0, 0.35, 0.5)
		"arrow_hit": return SoundSynth.noise_burst(0.12, 0.35, 0.04, h, 0.2, 0.9)
		"eat": return SoundSynth.noise_burst(0.14, 0.6, 0.05, h, 0.6, 1.0)
		"burp": return SoundSynth.voice(0.35, 110.0, 80.0, 400.0, 0.4, h, 0.7)
		"drink": return SoundSynth.tone(0.18, 300.0, 180.0, 0, 0.35, 0.1)
		"splash": return SoundSynth.noise_burst(0.5, 0.9, 0.2, h, 0.4, 1.0)
		"swim": return SoundSynth.noise_burst(0.25, 0.7, 0.12, h, 0.1, 0.8)
		"door_open": return SoundSynth.tone(0.3, 180.0, 260.0, 3, 0.4, 0.2, 0.05)
		"door_close": return SoundSynth.noise_burst(0.2, 0.25, 0.08, h, 0.1, 0.8)
		"chest_open": return SoundSynth.tone(0.35, 160.0, 220.0, 3, 0.35, 0.25, 0.08)
		"chest_close": return SoundSynth.noise_burst(0.2, 0.3, 0.07, h, 0.1, 0.8)
		"item_break": return SoundSynth.noise_burst(0.2, 0.9, 0.08, h, 0.8, 1.3)
		"shield_block": return SoundSynth.noise_burst(0.2, 0.3, 0.08, h, 0.2, 0.7)
		"crit": return SoundSynth.noise_burst(0.15, 0.7, 0.05, h, 0.5, 1.4)
		"swing": return SoundSynth.noise_burst(0.12, 0.8, 0.04, h, 0.0, 1.5)
		"attack": return SoundSynth.noise_burst(0.12, 0.45, 0.05, h, 0.3, 1.0)
		"portal_ambient": return SoundSynth.tone(2.0, 90.0, 110.0, 3, 0.25, 1.5, 0.08)
		"portal_travel": return SoundSynth.tone(2.5, 60.0, 400.0, 3, 0.35, 2.0, 0.2)
		"portal_trigger": return SoundSynth.tone(3.0, 120.0, 70.0, 3, 0.3, 2.5, 0.15)
		"end_portal_open": return SoundSynth.chord([196.0, 247.0, 294.0, 392.0], 3.0, 0.6)
		"eye_place": return SoundSynth.tone(0.5, 600.0, 900.0, 0, 0.35, 0.3)
		"ignite": return SoundSynth.noise_burst(0.3, 0.9, 0.1, h, 0.7, 1.2)
		"fire": return SoundSynth.noise_burst(0.8, 0.6, 0.3, h, 0.9, 0.7)
		"lava_pop": return SoundSynth.tone(0.12, 300.0, 120.0, 0, 0.3, 0.06)
		"thunder": return SoundSynth.explosion(h + 99)
		"bell": return SoundSynth.chord([880.0, 1320.0, 1760.0], 2.5, 0.6)
		"teleport": return SoundSynth.tone(0.45, 800.0, 200.0, 3, 0.4, 0.3, 0.3)
		"totem": return SoundSynth.chord([659.0, 880.0, 1175.0, 1568.0], 1.5, 0.6)
		"mace_smash": return SoundSynth.explosion(h + 5)
		"anvil_use": return SoundSynth.tone(0.4, 1200.0, 1100.0, 3, 0.35, 0.25)
		"anvil_land": return SoundSynth.tone(0.6, 700.0, 650.0, 3, 0.5, 0.3)
		"enchant": return SoundSynth.chord([784.0, 988.0, 1175.0], 1.2, 0.4)
		"brew": return SoundSynth.tone(0.8, 400.0, 600.0, 0, 0.2, 0.5, 0.2)
		"note": return SoundSynth.pluck(440.0, 0.8, 0.5)
		"piston_out": return SoundSynth.noise_burst(0.25, 0.4, 0.07, h, 0.3, 0.8)
		"piston_in": return SoundSynth.noise_burst(0.25, 0.35, 0.07, h + 1, 0.3, 0.7)
		"lever": return SoundSynth.tone(0.06, 900.0, 700.0, 2, 0.35, 0.03)
		"button": return SoundSynth.tone(0.05, 1200.0, 900.0, 2, 0.3, 0.03)
		"dispense": return SoundSynth.tone(0.08, 1000.0, 1200.0, 2, 0.3, 0.05)
		"minecart": return SoundSynth.noise_burst(0.6, 0.4, 0.4, h, 0.5, 0.7)
		"boat_paddle": return SoundSynth.noise_burst(0.2, 0.7, 0.08, h, 0.1, 0.9)
		"dragon_roar": return SoundSynth.voice(2.0, 70.0, 50.0, 300.0, 0.8, h, 1.0)
		"dragon_flap": return SoundSynth.noise_burst(0.35, 0.2, 0.15, h, 0.1, 0.5)
		"dragon_death": return SoundSynth.voice(5.0, 90.0, 40.0, 250.0, 0.9, h, 1.0)
		"wither_spawn": return SoundSynth.voice(3.0, 60.0, 90.0, 280.0, 0.9, h, 1.0)
		"wither_shoot": return SoundSynth.voice(0.6, 120.0, 80.0, 500.0, 0.8, h, 0.8)
		"ghast_shoot": return SoundSynth.voice(0.7, 500.0, 250.0, 1200.0, 0.3, h, 0.8)
		"blaze_shoot": return SoundSynth.noise_burst(0.5, 0.6, 0.2, h, 0.6, 0.8)
		"sonic_boom": return SoundSynth.explosion(h + 11)
		"geyser": return SoundSynth.noise_burst(1.2, 0.8, 0.5, h, 0.6, 0.9)
		"glass_break": return SoundSynth.noise_burst(0.35, 1.0, 0.1, h, 0.9, 1.6)
		"fizz", "extinguish": return SoundSynth.noise_burst(0.45, 0.95, 0.18, h, 0.2, 1.3)
		"explode_small": return SoundSynth.noise_burst(0.6, 0.35, 0.22, h, 0.8, 0.6)
		"bucket_empty": return SoundSynth.noise_burst(0.4, 0.75, 0.16, h, 0.1, 0.9)
		"bucket_fill", "bottle_fill": return SoundSynth.tone(0.3, 260.0, 520.0, 0, 0.3, 0.18, 0.1)
		"wood_break": return SoundSynth.noise_burst(0.3, 0.35, 0.1, h, 0.4, 0.8)
		"metal_break": return SoundSynth.tone(0.35, 900.0, 600.0, 3, 0.3, 0.15)
		"wax_off", "axe_strip": return SoundSynth.noise_burst(0.25, 0.6, 0.09, h, 0.6, 1.1)
		"warden_attack", "iron_golem_attack": return SoundSynth.noise_burst(0.35, 0.2, 0.12, h, 0.9, 0.5)
		"vault_open": return SoundSynth.chord([392.0, 523.0, 659.0], 0.8, 0.35)
		"trident_throw", "throw": return SoundSynth.noise_burst(0.2, 0.75, 0.06, h, 0.0, 1.3)
		"trident_pickup": return SoundSynth.tone(0.12, 1200.0, 1600.0, 0, 0.25, 0.06)
		"sweep": return SoundSynth.noise_burst(0.22, 0.85, 0.07, h, 0.0, 1.6)
		"riptide": return SoundSynth.noise_burst(0.8, 0.8, 0.35, h, 0.3, 0.8)
		"respawn_anchor_set", "respawn_anchor_charge": return SoundSynth.tone(0.6, 220.0, 440.0, 3, 0.35, 0.4, 0.1)
		"rain": return SoundSynth.noise_burst(2.4, 0.9, 1.6, h, 0.35, 1.0)
		"plant", "bone_meal", "berry_pick": return SoundSynth.noise_burst(0.14, 0.5, 0.05, h, 0.4, 1.2)
		"goat_horn": return SoundSynth.voice(1.6, 330.0, 300.0, 900.0, 0.2, h, 0.6)
		"flint_and_steel": return SoundSynth.noise_burst(0.2, 1.0, 0.05, h, 0.9, 1.4)
		"firework": return SoundSynth.noise_burst(0.7, 0.8, 0.3, h, 0.7, 1.1)
		"equip_leather", "armor_equip": return SoundSynth.noise_burst(0.2, 0.45, 0.07, h, 0.3, 1.0)
		"end_portal_frame_fill": return SoundSynth.tone(0.5, 440.0, 660.0, 0, 0.35, 0.3, 0.2)
		"dragon_shoot": return SoundSynth.voice(0.8, 150.0, 100.0, 600.0, 0.7, h, 0.8)
		"dragon_breath": return SoundSynth.noise_burst(1.2, 0.5, 0.6, h, 0.5, 0.6)
		"crossbow_load": return SoundSynth.tone(0.25, 400.0, 700.0, 2, 0.3, 0.12)
		"creeper_hiss": return SoundSynth.fuse(h + 3)
		"composter_fill", "composter_fill_success", "composter_empty": return SoundSynth.noise_burst(0.2, 0.4, 0.08, h, 0.5, 0.9)
		"bow_hit": return SoundSynth.tone(0.1, 1600.0, 1400.0, 0, 0.3, 0.05)
		"bow_draw": return SoundSynth.tone(0.5, 200.0, 320.0, 2, 0.15, 0.4)
		"beacon_power": return SoundSynth.chord([262.0, 330.0, 392.0, 523.0], 2.0, 0.4)
		"door_open", "fence_gate_open", "trapdoor_open", "iron_door_open", "iron_trapdoor_open":
			return SoundSynth.tone(0.3, 180.0, 260.0, 3, 0.4, 0.2, 0.05)
		"door_close", "fence_gate_close", "trapdoor_close", "iron_door_close", "iron_trapdoor_close":
			return SoundSynth.noise_burst(0.2, 0.25, 0.08, h, 0.1, 0.8)
	if key.begins_with("note_"):
		# note block instruments: plucked strings, bells, flutes and drums at F#3 (pitch shifts it)
		match key.substr(5):
			"basedrum": return SoundSynth.noise_burst(0.25, 0.1, 0.08, h, 0.2, 0.5)
			"snare": return SoundSynth.noise_burst(0.18, 0.9, 0.05, h, 0.2, 1.2)
			"hat": return SoundSynth.noise_burst(0.08, 1.0, 0.02, h, 0.0, 1.8)
			"bass": return SoundSynth.pluck(92.5, 0.8, 0.5)
			"bell", "chime": return SoundSynth.chord([740.0, 1480.0, 2220.0], 1.4, 0.35)
			"flute": return SoundSynth.tone(0.8, 740.0, 740.0, 0, 0.3, 0.5, 0.15)
			"guitar": return SoundSynth.pluck(185.0, 0.9, 0.45)
			"xylophone", "iron_xylophone": return SoundSynth.tone(0.5, 1480.0, 1480.0, 0, 0.3, 0.12)
			"cow_bell": return SoundSynth.tone(0.4, 740.0, 720.0, 3, 0.3, 0.15)
			"didgeridoo": return SoundSynth.voice(0.9, 92.5, 92.5, 400.0, 0.3, h, 0.6)
			"bit": return SoundSynth.tone(0.3, 370.0, 370.0, 1, 0.25, 0.2)
			"banjo": return SoundSynth.pluck(370.0, 0.6, 0.45)
			"pling": return SoundSynth.chord([370.0, 740.0], 1.0, 0.35)
		return SoundSynth.pluck(370.0, 0.8, 0.45)
	if key.begins_with("mob:"):
		return _mob_sound(key.substr(4), h)
	return SoundSynth.noise_burst(0.1, 0.5, 0.05, h)


func _mob_sound(k: String, h: int) -> AudioStream:
	var parts := k.split(".")
	var mob: String = parts[0]
	var kind: String = parts[1] if parts.size() > 1 else "idle"
	var hurt := kind == "hurt"
	var death := kind == "death"
	var m := 0.8 if death else 1.0
	match mob:
		"pig": return SoundSynth.voice(0.3, 320.0 * (1.2 if hurt else 1.0), 260.0 * m, 900.0, 0.5, h, 0.8)
		"cow", "mooshroom": return SoundSynth.voice(0.9 if not hurt else 0.35, 120.0, 95.0 * m, 450.0, 0.25, h, 0.9)
		"sheep", "goat", "llama": return SoundSynth.voice(0.55, 300.0, 280.0 * m, 800.0, 0.35, h, 0.8)
		"chicken", "parrot": return SoundSynth.tone(0.12, 1500.0, 1100.0 * m, 1, 0.15, 0.06, 0.2)
		"zombie", "husk", "drowned", "zombie_villager", "zombified_piglin":
			return SoundSynth.voice(0.8, 95.0 * (1.2 if hurt else 1.0), 70.0 * m, 380.0, 0.6, h, 0.9)
		"skeleton", "stray", "wither_skeleton", "bogged":
			return SoundSynth.noise_burst(0.3, 0.9, 0.05, h, 0.9, 1.4)
		"creeper": return SoundSynth.fuse(h) if kind == "fuse" else SoundSynth.noise_burst(0.3, 0.5, 0.1, h, 0.2, 0.8)
		"spider", "cave_spider": return SoundSynth.noise_burst(0.3, 0.8, 0.1, h, 0.6, 1.6)
		"enderman": return SoundSynth.tone(0.5, 300.0, 120.0, 3, 0.4, 0.3, 0.4)
		"villager", "wandering_trader", "pillager", "vindicator", "evoker", "witch":
			return SoundSynth.voice(0.35, 190.0, 160.0 * m, 700.0, 0.2, h, 0.8)
		"iron_golem", "copper_golem": return SoundSynth.noise_burst(0.3, 0.2, 0.15, h, 0.3, 0.6)
		"wolf": return SoundSynth.voice(0.18, 420.0, 350.0, 1100.0, 0.5, h, 0.9)
		"cat", "ocelot": return SoundSynth.voice(0.5, 500.0, 700.0, 1400.0, 0.1, h, 0.7)
		"horse", "donkey", "mule", "camel": return SoundSynth.voice(0.6, 260.0, 380.0, 900.0, 0.4, h, 0.8)
		"ghast", "happy_ghast", "ghastling": return SoundSynth.voice(1.0, 600.0, 420.0, 1500.0, 0.2, h, 0.6)
		"blaze": return SoundSynth.noise_burst(0.6, 0.5, 0.3, h, 0.7, 0.7)
		"slime", "magma_cube", "sulfur_cube": return SoundSynth.noise_burst(0.2, 0.25, 0.1, h, 0.0, 0.6)
		"ender_dragon": return get_stream("dragon_roar")
		"wither": return get_stream("wither_shoot")
		"warden": return SoundSynth.voice(1.2, 60.0, 45.0, 250.0, 0.9, h, 1.0)
		"piglin", "piglin_brute", "hoglin", "zoglin": return SoundSynth.voice(0.4, 180.0, 140.0, 600.0, 0.7, h, 0.8)
	return SoundSynth.voice(0.3, 250.0, 200.0 * m, 700.0, 0.3, h, 0.6)


func play_at(key: String, pos: Vector3, vol := 1.0, pitch := 1.0) -> void:
	if not enabled:
		return
	if pos.distance_squared_to(listener_pos) > 48.0 * 48.0:
		return
	var a: AudioStreamPlayer3D = _pool3d[_next3d]
	_next3d = (_next3d + 1) % _pool3d.size()
	a.stream = get_stream(key)
	a.volume_db = linear_to_db(maxf(vol, 0.001)) - 2.0
	a.pitch_scale = pitch * _rng.randf_range(0.92, 1.08)
	a.global_position = pos
	a.play()


func play_ui(key: String, vol := 1.0, pitch := 1.0) -> void:
	if not enabled:
		return
	var b: AudioStreamPlayer = _pool2d[_next2d]
	_next2d = (_next2d + 1) % _pool2d.size()
	b.stream = get_stream(key)
	b.volume_db = linear_to_db(maxf(vol, 0.001)) - 4.0
	b.pitch_scale = pitch
	b.play()


func play_block(v: int, kind: String, pos: Vector3, vol := 1.0) -> void:
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	var g := d.sound if BLOCK_SOUNDS.has(d.sound) else "stone"
	play_at("block:%s:%s:%d" % [g, kind, _rng.randi_range(0, 2)], pos, vol * (0.45 if kind == "step" else 0.9),
		0.8 if kind == "break" else 1.0)


func play_mob(mob: String, kind: String, pos: Vector3, vol := 1.0, pitch := 1.0) -> void:
	play_at("mob:%s.%s" % [mob, kind], pos, vol, pitch)


func _process(delta: float) -> void:
	if not enabled:
		return
	_music_timer -= delta
	if _music_timer <= 0.0 and not _music.playing:
		_music_timer = _rng.randf_range(90.0, 200.0)
		_music.stream = MusicGen.piece(_rng.randi())
		_music.volume_db = linear_to_db(maxf(float(Game.settings.music_volume) * 0.35, 0.0001))
		_music.play()


# ------------------------------------------------------------------------------------------------
# Jukebox: plays a generated piece for a disc at a block position.
var _jukeboxes: Dictionary = {}


func play_jukebox(pos: Vector3i, disc: String) -> void:
	stop_jukebox(pos)
	if not enabled:
		return
	var a := AudioStreamPlayer3D.new()
	a.max_distance = 64.0
	a.unit_size = 10.0
	add_child(a)
	a.global_position = Vector3(pos) + Vector3(0.5, 0.5, 0.5)
	a.stream = MusicGen.piece(hash(disc))
	a.volume_db = linear_to_db(maxf(float(Game.settings.music_volume), 0.0001))
	a.play()
	_jukeboxes[pos] = a
	a.finished.connect(func(): stop_jukebox(pos))


func stop_jukebox(pos: Vector3i) -> void:
	var a = _jukeboxes.get(pos)
	if a != null and is_instance_valid(a):
		a.queue_free()
	_jukeboxes.erase(pos)


func stop_all_jukeboxes() -> void:
	for p in _jukeboxes.keys():
		stop_jukebox(p)
