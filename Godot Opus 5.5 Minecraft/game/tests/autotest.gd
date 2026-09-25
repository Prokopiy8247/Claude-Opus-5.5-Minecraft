extends RefCounted
## Command-line autotest scenarios (--autotest=<name>). Used for headless validation and for the
## visual screenshot pass:  godot --path . -- --autotest=mobs --shots=res://shots

static func run(session, name: String) -> void:
	match name:
		"mobs":
			_spawn_showcase(session)
		"blocks":
			_place_showcase(session)


## Builds a flat stage in the sky and lines up one of every category of mob on it, so the model
## pass can be inspected without terrain in the way.
static func _spawn_showcase(session) -> void:
	var pl = session.player
	var w = session.world
	var cx := floori(pl.body.pos.x)
	var cz := floori(pl.body.pos.z)
	var y := mini(w.max_y - 40, maxi(w.top_solid_y(cx, cz) + 30, 150))
	var stone := BlockDB.id("smooth_stone")
	var glass := BlockDB.id("glass")
	for dx in range(-9, 10):
		for dz in range(-20, 4):
			w.set_block(cx + dx, y, cz + dz, stone, World.F_URGENT)
			if dx == -9 or dx == 9 or dz == -20 or dz == 3:
				w.set_block(cx + dx, y + 1, cz + dz, glass, World.F_URGENT)
	var cols := 7
	var i := 0
	for n in SHOWCASE_MOBS:
		var mob := String(n)
		if not MobDB.has(mob):
			continue
		var col := i % cols
		var row := i / cols
		var x := cx - 7 + col * 2 + (row % 2)
		var z := cz - 6 - row * 2
		session.entities.spawn_mob(w, mob, Vector3(x + 0.5, float(y + 1), z + 0.5), {"persistent": true, "summoned": true})
		i += 1
	pl.set_gamemode(Player.CREATIVE)
	pl.flying = true
	pl.teleport(Vector3(cx + 0.5, float(y + 2), cz - 1.5))
	pl.yaw = 0.0
	pl.pitch = -0.38
	session.day_time = 6000
	session.set_meta("showcase", Vector3(cx + 0.5, float(y + 2), cz - 1.5))


const SHOWCASE_MOBS := ["pig", "cow", "sheep", "chicken", "villager", "zombie", "skeleton", "creeper", "spider",
	"enderman", "zombie_villager", "iron_golem", "wolf", "cat", "fox", "panda", "bee", "parrot", "bat", "slime",
	"magma_cube", "blaze", "ghast", "piglin", "hoglin", "strider", "warden", "shulker", "phantom", "guardian",
	"dolphin", "turtle", "frog", "axolotl", "allay", "vex", "pillager", "vindicator", "evoker", "ravager",
	"creaking", "breeze", "armadillo", "sniffer", "goat", "llama", "camel", "horse", "donkey", "mooshroom",
	"wither_skeleton", "witch", "rabbit", "trader_llama", "camel_husk"]


## Places one of every interesting block in rows in front of the player.
static func _place_showcase(session) -> void:
	var pl = session.player
	var w = session.world
	var names := ["oak_log", "oak_planks", "stone_bricks", "cobblestone", "deepslate_bricks", "bricks", "glass",
		"oak_leaves", "grass_block", "dirt_path", "furnace", "crafting_table", "chest", "torch", "lantern",
		"oak_stairs", "oak_slab", "oak_fence", "redstone_lamp", "tnt", "gold_block", "diamond_block", "obsidian",
		"netherrack", "end_stone", "purpur_block", "prismarine", "sea_lantern", "hay_block", "bookshelf",
		"water", "lava", "ice", "snow_block", "amethyst_block", "sculk", "sulfur_bricks", "cinnabar"]
	var fwd: Vector3 = Vector3(-sin(pl.yaw), 0.0, -cos(pl.yaw))
	var right := Vector3(fwd.z, 0.0, -fwd.x)
	var i := 0
	for n in names:
		var col := i % 8
		var row := i / 8
		var off := right * (float(col) - 3.5) + fwd * (5.0 + float(row) * 1.5)
		var p: Vector3 = pl.body.pos + off
		var y: int = w.top_solid_y(floori(p.x), floori(p.z))
		w.set_block(floori(p.x), y + 1, floori(p.z), BlockDB.id(String(n)), World.F_URGENT)
		i += 1
	pl.pitch = -0.25
