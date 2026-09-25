class_name Commands
extends RefCounted
## Chat command interpreter (Java-edition syntax subset): /gamemode /give /summon /time /weather /tp
## /locate /kill /difficulty /effect /gamerule /setblock /fill /clear /xp /enchant /spawnpoint /seed
## /help /say /heal /feed /dimension. Relative coordinates (~) are supported.

const HELP := [
	"/gamemode <survival|creative|spectator>", "/give <item> [count]", "/summon <mob> [x y z]",
	"/time set <day|noon|night|midnight|ticks> | /time add <ticks>", "/weather <clear|rain|thunder>",
	"/tp <x y z>", "/locate <structure>", "/kill [@e|@e[type=mob]|@s]", "/difficulty <peaceful|easy|normal|hard>",
	"/effect give <effect> [seconds] [amplifier] | /effect clear", "/gamerule <rule> <true|false>",
	"/setblock <x y z> <block>", "/fill <x1 y1 z1> <x2 y2 z2> <block>", "/clear", "/xp add <n> [levels]",
	"/enchant <enchantment> [level]", "/spawnpoint", "/seed", "/dimension <overworld|nether|end>", "/heal", "/feed",
]


static func mode_name(m: int) -> String:
	match m:
		Player.CREATIVE:
			return "Creative"
		Player.SPECTATOR:
			return "Spectator"
	return "Survival"


static func difficulty_name(d: int) -> String:
	return ["Peaceful", "Easy", "Normal", "Hard"][clampi(d, 0, 3)]


static func _say(session, text: String, ok := true) -> void:
	session.chat(text, Color(0.75, 0.9, 1.0) if ok else Color(1.0, 0.45, 0.45))


static func _coord(tok: String, base: float) -> float:
	if tok.begins_with("~"):
		var rest := tok.substr(1)
		return base + (float(rest) if rest != "" else 0.0)
	return float(tok)


static func _pos3(session, args: PackedStringArray, i: int) -> Vector3:
	var p: Vector3 = session.player.body.pos
	if args.size() < i + 3:
		return Vector3.INF
	return Vector3(_coord(args[i], p.x), _coord(args[i + 1], p.y), _coord(args[i + 2], p.z))


## Runs one command line (without the leading slash). Returns true on success.
static func run(session, line: String) -> bool:
	var args := line.strip_edges().split(" ", false)
	if args.is_empty():
		return false
	var cmd := args[0].to_lower()
	var pl: Player = session.player
	match cmd:
		"help", "?":
			for h in HELP:
				_say(session, String(h))
			return true
		"gamemode", "gm":
			if args.size() < 2:
				_say(session, "Usage: /gamemode <survival|creative|spectator>", false)
				return false
			var m := -1
			match args[1].to_lower():
				"survival", "s", "0", "adventure", "a", "2":
					m = Player.SURVIVAL
				"creative", "c", "1":
					m = Player.CREATIVE
				"spectator", "sp", "3":
					m = Player.SPECTATOR
			if m < 0:
				_say(session, "Unknown game mode: %s" % args[1], false)
				return false
			pl.set_gamemode(m)
			_say(session, "Set own game mode to %s Mode" % mode_name(m))
			return true
		"give":
			if args.size() < 2:
				_say(session, "Usage: /give <item> [count]", false)
				return false
			var iname := args[1].to_lower().trim_prefix("minecraft:")
			if args[1] == "@s" and args.size() >= 3:
				iname = args[2].to_lower().trim_prefix("minecraft:")
				args.remove_at(1)
			if not ItemDB.has(iname):
				_say(session, "Unknown item '%s'" % iname, false)
				return false
			var count := int(args[2]) if args.size() > 2 else 1
			count = clampi(count, 1, 6400)
			var left := count
			var mx := ItemDB.get_by_name(iname).max_stack
			while left > 0:
				var n := mini(left, mx)
				var rem = pl.inventory.add(ItemStack.of(iname, n), 0, 36)
				if rem != null:
					session.drop_item_from_player(rem)
				left -= n
			_say(session, "Gave %d [%s] to Player" % [count, BlockCatalog.pretty(iname)])
			return true
		"summon":
			if args.size() < 2:
				_say(session, "Usage: /summon <mob> [x y z]", false)
				return false
			var mob := args[1].to_lower().trim_prefix("minecraft:")
			if not MobDB.has(mob):
				_say(session, "Unknown entity '%s'" % mob, false)
				return false
			var p := _pos3(session, args, 2)
			if p == Vector3.INF:
				p = pl.body.pos + pl.look_dir() * Vector3(3, 0, 3) + Vector3(0, 0.5, 0)
			if mob == "ender_dragon":
				DragonFight.spawn_dragon(session.world, p + Vector3(0, 10, 0))
			else:
				session.entities.spawn_mob(session.world, mob, p, {"persistent": true, "summoned": true})
			_say(session, "Summoned new %s" % MobDB.display(mob))
			return true
		"time":
			if args.size() < 3:
				_say(session, "Usage: /time set <day|noon|night|midnight|n> or /time add <n>", false)
				return false
			var t := 0
			match args[2].to_lower():
				"day":
					t = 1000
				"noon":
					t = 6000
				"sunset":
					t = 12000
				"night":
					t = 13000
				"midnight":
					t = 18000
				"sunrise":
					t = 23000
				_:
					t = int(args[2])
			if args[1] == "add":
				session.day_time = (session.day_time + t) % WorldSession.DAY_TICKS
			else:
				session.day_time = posmod(t, WorldSession.DAY_TICKS)
			_say(session, "Set the time to %d" % session.day_time)
			return true
		"weather":
			if args.size() < 2:
				_say(session, "Usage: /weather <clear|rain|thunder>", false)
				return false
			var wmap := {"clear": 0, "rain": 1, "thunder": 2}
			if not wmap.has(args[1].to_lower()):
				_say(session, "Unknown weather", false)
				return false
			session.weather = int(wmap[args[1].to_lower()])
			session.weather_timer = int(args[2]) * 20 if args.size() > 2 else 6000
			_say(session, "Changing to %s" % ["clear weather", "rain", "rain and thunder"][session.weather])
			return true
		"tp", "teleport":
			var p2 := _pos3(session, args, 1)
			if p2 == Vector3.INF:
				_say(session, "Usage: /tp <x y z>", false)
				return false
			pl.teleport(p2)
			_say(session, "Teleported Player to %.1f, %.1f, %.1f" % [p2.x, p2.y, p2.z])
			return true
		"locate":
			if args.size() < 2:
				_say(session, "Usage: /locate <structure>  (%s)" % ", ".join(StructureRegistry.names_for(session.dim)), false)
				return false
			var sname := args[args.size() - 1].to_lower().trim_prefix("minecraft:")
			var w: World = session.world
			var found: Vector3 = w.gen.structures.locate(sname, pl.body.pos) if w.gen.structures != null else Vector3.INF
			if found == Vector3.INF:
				_say(session, "Could not find a structure of type \"%s\" nearby" % sname, false)
				return false
			var dist := Vector2(found.x - pl.body.pos.x, found.z - pl.body.pos.z).length()
			_say(session, "The nearest %s is at [%d, ~, %d] (%d blocks away)" % [sname, int(found.x), int(found.z), int(dist)])
			return true
		"kill":
			var target := args[1] if args.size() > 1 else "@s"
			if target == "@s" or target == "@p":
				pl.stats.damage(1000.0, "kill", null, true)
				return true
			var n := 0
			for e in session.entities.all().duplicate():
				var ent: Entity = e
				if target.begins_with("@e[type="):
					var ty := target.substr(8).trim_suffix("]").trim_prefix("minecraft:")
					if not (ent is Mob and (ent as Mob).mob == ty) and not (ty == "item" and ent is SimpleEntities.ItemEntity):
						continue
				if ent is Mob:
					(ent as Mob).hurt(10000.0, "kill", null)
				else:
					ent.queue_free()
				n += 1
			_say(session, "Killed %d entities" % n)
			return true
		"difficulty":
			if args.size() < 2:
				_say(session, "The difficulty is %s" % difficulty_name(session.difficulty))
				return true
			var dmap := {"peaceful": 0, "easy": 1, "normal": 2, "hard": 3, "0": 0, "1": 1, "2": 2, "3": 3}
			if not dmap.has(args[1].to_lower()):
				_say(session, "Unknown difficulty", false)
				return false
			session.difficulty = int(dmap[args[1].to_lower()])
			if session.difficulty == 0:
				for e in session.entities.all().duplicate():
					if e is Mob and (e as Mob).hostile() and not bool((e as Mob).def.get("boss", false)):
						(e as Node).queue_free()
			_say(session, "The difficulty has been set to %s" % difficulty_name(session.difficulty))
			return true
		"effect":
			if args.size() >= 2 and args[1] == "clear":
				pl.clear_effects()
				_say(session, "Removed every effect from Player")
				return true
			if args.size() < 3:
				_say(session, "Usage: /effect give <effect> [seconds] [amplifier]", false)
				return false
			var ename := args[2].to_lower().trim_prefix("minecraft:")
			if args[1] != "give":
				ename = args[1].to_lower().trim_prefix("minecraft:")
			if not EffectDB.EFFECTS.has(ename):
				_say(session, "Unknown effect '%s'" % ename, false)
				return false
			var secs := int(args[3]) if args.size() > 3 else 30
			var amp := int(args[4]) if args.size() > 4 else 0
			pl.add_effect(ename, secs * 20, amp)
			_say(session, "Applied effect %s to Player" % ename.replace("_", " "))
			return true
		"gamerule":
			if args.size() < 2:
				_say(session, ", ".join(PackedStringArray(session.gamerules.keys())))
				return true
			if args.size() < 3:
				_say(session, "Gamerule %s is currently set to: %s" % [args[1], str(session.gamerules.get(args[1], "?"))])
				return true
			var v: Variant = args[2].to_lower() == "true"
			if args[2].is_valid_int():
				v = int(args[2])
			session.gamerules[args[1]] = v
			if args[1] == "randomTickSpeed":
				for k in session.worlds:
					var ww = session.worlds[k]
					if ww != null and is_instance_valid(ww):
						(ww as World).random_tick_speed = int(v)
			_say(session, "Gamerule %s is now set to: %s" % [args[1], str(v)])
			return true
		"setblock":
			var p3 := _pos3(session, args, 1)
			if p3 == Vector3.INF or args.size() < 5:
				_say(session, "Usage: /setblock <x y z> <block>", false)
				return false
			var bname := args[4].to_lower().trim_prefix("minecraft:")
			var bid := BlockDB.id(bname)
			if not BlockDB.has(bname):
				_say(session, "Unknown block", false)
				return false
			session.world.set_block(floori(p3.x), floori(p3.y), floori(p3.z), bid)
			_say(session, "Changed the block at %d, %d, %d" % [floori(p3.x), floori(p3.y), floori(p3.z)])
			return true
		"fill":
			var a := _pos3(session, args, 1)
			var b := _pos3(session, args, 4)
			if a == Vector3.INF or b == Vector3.INF or args.size() < 8:
				_say(session, "Usage: /fill <x1 y1 z1> <x2 y2 z2> <block>", false)
				return false
			var fname := args[7].to_lower().trim_prefix("minecraft:")
			var fid := BlockDB.id(fname)
			if not BlockDB.has(fname):
				_say(session, "Unknown block", false)
				return false
			var lo := Vector3i(floori(minf(a.x, b.x)), floori(minf(a.y, b.y)), floori(minf(a.z, b.z)))
			var hi := Vector3i(floori(maxf(a.x, b.x)), floori(maxf(a.y, b.y)), floori(maxf(a.z, b.z)))
			var vol := (hi.x - lo.x + 1) * (hi.y - lo.y + 1) * (hi.z - lo.z + 1)
			if vol > 32768:
				_say(session, "Too many blocks in the specified area (maximum 32768, specified %d)" % vol, false)
				return false
			for y in range(lo.y, hi.y + 1):
				for z in range(lo.z, hi.z + 1):
					for x in range(lo.x, hi.x + 1):
						session.world.set_block(x, y, z, fid, World.F_NOTIFY | World.F_DROP_BE)
			_say(session, "Successfully filled %d block(s)" % vol)
			return true
		"clear":
			for i in 46:
				pl.inventory.slots[i] = null
			pl.inventory.changed.emit()
			_say(session, "Removed all items from Player")
			return true
		"xp", "experience":
			if args.size() < 3:
				_say(session, "Usage: /xp add <amount> [levels|points]", false)
				return false
			var amt := int(args[2])
			if args.size() > 3 and args[3].begins_with("level"):
				pl.stats.add_levels(amt)
			else:
				pl.stats.add_xp(amt)
			_say(session, "Gave %d experience to Player" % amt)
			return true
		"enchant":
			if args.size() < 2:
				_say(session, "Usage: /enchant <enchantment> [level]", false)
				return false
			var st: ItemStack = pl.inventory.selected_stack()
			if st == null:
				_say(session, "You are not holding an item", false)
				return false
			var en := args[args.size() - 2 if args.size() > 2 and args[args.size() - 1].is_valid_int() else args.size() - 1].to_lower().trim_prefix("minecraft:")
			var lvl := int(args[args.size() - 1]) if args[args.size() - 1].is_valid_int() else 1
			if not EnchantDB.ENCH.has(en):
				_say(session, "Unknown enchantment '%s'" % en, false)
				return false
			EnchantDB.apply(st, [[en, lvl]])
			pl.inventory.changed.emit()
			_say(session, "Applied enchantment %s to Player's item" % EnchantDB.display(en, lvl))
			return true
		"spawnpoint":
			pl.spawn_point = pl.body.pos
			pl.spawn_dim = session.dim
			_say(session, "Set spawn point to %d, %d, %d" % [int(pl.body.pos.x), int(pl.body.pos.y), int(pl.body.pos.z)])
			return true
		"seed":
			_say(session, "Seed: [%d]" % session.seed_value)
			return true
		"say":
			session.chat("[Player] %s" % " ".join(args.slice(1)), Color(1, 1, 1))
			return true
		"heal":
			pl.stats.health = pl.stats.max_health
			pl.stats.changed.emit()
			return true
		"feed":
			pl.stats.food = 20
			pl.stats.saturation = 20.0
			return true
		"dimension", "dim", "execute":
			var target_dim := -1
			var tok := args[args.size() - 1].to_lower()
			if "nether" in tok:
				target_dim = 1
			elif "end" in tok:
				target_dim = 2
			elif "overworld" in tok:
				target_dim = 0
			if target_dim < 0:
				_say(session, "Usage: /dimension <overworld|nether|end>", false)
				return false
			if target_dim == session.dim:
				return true
			var dest := Vector3.INF
			if target_dim == 2:
				dest = Vector3(100.5, 60.0, 0.5)
			session.travel_to(target_dim, dest)
			if target_dim == 2:
				session.dimension_changed.connect(func(_d): session.player.teleport(DragonFight.arrival_point(session.world)), CONNECT_ONE_SHOT)
			return true
	_say(session, "Unknown command: /%s  (type /help)" % cmd, false)
	return false
