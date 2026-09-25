extends Node
## Headless travel test: enters the Nether and the End, reports what it finds there.
##   godot --headless --path . res://game/tests/travel_check.tscn

var session: WorldSession = null
var stage := 0
var frames := 0


func _ready() -> void:
	Game.init_registries()
	var params := {"name": "TravelTest", "folder": "travel_test", "seed": 20260924, "gamemode": Player.CREATIVE,
		"difficulty": 2, "gamerules": {}, "player": {}}
	SaveManager.delete_world("travel_test")
	session = WorldSession.new()
	add_child(session)
	session.setup(params)
	await get_tree().process_frame
	session.start()


func _process(_d: float) -> void:
	frames += 1
	if stage == 0 and frames > 120:
		stage = 1
		_go(1, Vector3(8.5, 80.0, 8.5))
	elif stage == 1:
		if session.dim == 1:
			stage = 2
			frames = 0
		elif frames > 1800:
			print("FAIL could not enter the nether (dim=%d, traveling=%s)" % [session.dim, str(session.traveling)])
			get_tree().quit(1)
	elif stage == 2 and frames > 240:
		_report("nether")
		stage = 3
		_go(2, Vector3(100.5, 50.0, 0.5))
	elif stage == 3:
		if session.dim == 2:
			stage = 4
			frames = 0
		elif frames > 1800:
			print("FAIL could not enter the end")
			get_tree().quit(1)
	elif stage == 4 and frames > 400:
		_report("end")
		print("  travel: PASS")
		get_tree().quit(0)
	elif stage == 4 and frames % 120 == 0:
		print("  end: dragon=%s crystals=%d player=%s" % [str(DragonFight.dragon_entity(session) != null),
			DragonFight.crystals().size(), str(session.player.body.pos.round())])


func _go(dim: int, pos: Vector3) -> void:
	print("travelling to dim %d -> %s" % [dim, str(pos)])
	session.travel_to(dim, pos, false)
	session.player.teleport(pos)


func _report(what: String) -> void:
	var w: World = session.world
	var p := session.player.body.pos
	var x := floori(p.x)
	var z := floori(p.z)
	print("  %s: dim=%d pos=%s ready=%s surface=%d biome=%s blocks=%s" % [what, session.dim, str(p.round()),
		str(w.is_ready_at(x, z)), w.top_solid_y(x, z), BiomeDB.name_of(w.biome_at(x, floori(p.y), z)),
		str(w.get_block(x, floori(p.y) - 1, z) != 0)])
	var names := []
	for y in range(floori(p.y) - 3, floori(p.y) + 3):
		var n := BlockDB.name_of(w.get_block(x, y, z))
		if n != "air":
			names.append("%d:%s" % [y, n])
	print("    column: %s" % ", ".join(PackedStringArray(names)))
