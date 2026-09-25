extends Node
## Headless gameplay test: drives a real WorldSession through the prompt's testing checklist
## (core, creative, survival, combat/explosions, UI, portals/Nether, End, save/load) and prints
## one PASS/FAIL line per check plus a summary. Mining, eating and the bow go through the real
## input pipeline (Input.action_press), the rest through the same calls the game makes.
##   godot --headless --path . res://game/tests/gameplay_check.tscn

const FOLDER := "gameplay_test"
const NONE := Vector3i(-99999, 0, 0)

var session: WorldSession = null
var passed := 0
var failed := 0
var base := Vector3i.ZERO
var initial_mode := -1


func _ready() -> void:
	Game.init_registries()
	SaveManager.delete_world(FOLDER)
	DragonFight.reset()
	await _open_session({"name": "GameplayTest", "folder": FOLDER, "seed": 20260924, "difficulty": 2,
		"gamerules": {}, "player": {}})
	initial_mode = P().gamemode
	await _run_all()
	print("GAMEPLAY: %d passed, %d failed" % [passed, failed])
	get_tree().quit(0 if failed == 0 else 1)


func _open_session(params: Dictionary) -> void:
	session = WorldSession.new()
	add_child(session)
	session.setup(params)
	await get_tree().process_frame
	session.start()
	await _wait_ready()


func check(what: String, ok: bool, detail := "") -> void:
	if ok:
		passed += 1
	else:
		failed += 1
	print("  %s %s%s" % ["PASS" if ok else "FAIL", what, ("  (" + detail + ")") if detail != "" else ""])


## Runs n simulation ticks, yielding a frame every few ticks (and always at the end) so the
## engine keeps streaming, meshing and resuming coroutines such as travel_to.
func ticks(n: int) -> void:
	for i in n:
		session.simulate()
		if i % 5 == 4 or i == n - 1:
			await get_tree().process_frame


func frames(n: int) -> void:
	for i in n:
		await get_tree().process_frame


func _wait_ready(limit_ms := 30000) -> bool:
	var t0 := Time.get_ticks_msec()
	while Time.get_ticks_msec() - t0 < limit_ms:
		var p := session.player.body.pos
		if session.world.is_ready_at(floori(p.x), floori(p.z)) and not session.traveling:
			return true
		await get_tree().process_frame
	return false


func _wait_dim(d: int, limit_ms := 40000) -> bool:
	var t0 := Time.get_ticks_msec()
	while session.dim != d and Time.get_ticks_msec() - t0 < limit_ms:
		await get_tree().process_frame
	return session.dim == d


func W() -> World:
	return session.world


func P() -> Player:
	return session.player


func bid(n: String) -> int:
	return BlockDB.id(n)


## A flat smooth-stone test pad around the player with open air above it.
func _build_pad(radius := 10) -> void:
	var p := P().body.pos
	base = Vector3i(floori(p.x), clampi(floori(p.y), 70, 200), floori(p.z))
	for x in range(-radius, radius + 1):
		for z in range(-radius, radius + 1):
			W().set_block(base.x + x, base.y - 1, base.z + z, bid("smooth_stone"), World.F_URGENT)
			for y in range(0, 10):
				W().set_block(base.x + x, base.y + y, base.z + z, 0, World.F_URGENT)
	P().teleport(Vector3(base.x + 0.5, base.y, base.z + 0.5))
	P().body.vel = Vector3.ZERO
	await ticks(4)


func _look_at(target: Vector3) -> void:
	var to := target - P().eye_position()
	P().yaw = atan2(-to.x, -to.z)
	P().pitch = clampf(atan2(to.y, Vector2(to.x, to.z).length()), -1.5, 1.5)


func _aim_block(pos: Vector3i) -> bool:
	_look_at(Vector3(pos) + Vector3(0.5, 0.5, 0.5))
	var inter: PlayerInteraction = P().interact
	inter.target = W().raycast(P().eye_position(), P().look_dir(), inter.reach())
	return not inter.target.is_empty() and inter.target.pos == pos


func _give(item: String, n := 1, slot := 0) -> ItemStack:
	var st := ItemStack.of(item, n)
	P().inventory.set_stack(slot, st)
	P().inventory.selected = slot
	return st


func _count(item: String) -> int:
	return P().inventory.count_item(ItemDB.id(item))


func _mobs(mob_name: String) -> Array:
	var out := []
	for e in session.entities.all():
		if e is Mob and (e as Mob).mob == mob_name and not (e as Mob).dead and not (e as Node).is_queued_for_deletion():
			out.append(e)
	return out


func _clear_mobs() -> void:
	for e in session.entities.all():
		if e is Mob and (e as Mob).mob != "ender_dragon":
			(e as Node).queue_free()
	await get_tree().process_frame


## Holds an input action for n ticks (the game reads Input in PlayerInteraction.tick).
func _hold(action: String, n: int, aim := NONE) -> void:
	Input.action_press(action)
	for i in n:
		if aim != NONE:
			_look_at(Vector3(aim) + Vector3(0.5, 0.5, 0.5))
		session.simulate()
		if i % 5 == 4:
			await get_tree().process_frame
	Input.action_release(action)
	session.simulate()


# ================================================================================================
func _run_all() -> void:
	print("== core")
	await _core()
	print("== creative")
	await _creative()
	print("== survival")
	await _survival()
	print("== combat / explosions")
	await _combat()
	print("== ui")
	await _ui()
	print("== portals / nether")
	await _portals()
	print("== end")
	await _end()
	print("== vehicles / animals / world")
	await _extras()
	print("== save / load")
	await _persistence()


func _lit_chunks() -> int:
	var lit := 0
	for k in W().cm.chunks:
		var c: Chunk = W().cm.chunks[k]
		if c.state >= Chunk.S_LIT:
			lit += 1
	return lit


func _core() -> void:
	var p := P().body.pos
	check("world loads and terrain is ready at spawn", W().is_ready_at(floori(p.x), floori(p.z)))
	var t0 := Time.get_ticks_msec()
	while _lit_chunks() < 81 and Time.get_ticks_msec() - t0 < 30000:
		await get_tree().process_frame
	check("chunks stream around the player", _lit_chunks() >= 81,
		"%d chunks lit after %.1f s" % [_lit_chunks(), float(Time.get_ticks_msec() - t0) / 1000.0])
	P().set_gamemode(Player.SURVIVAL)
	var top := W().top_solid_y(floori(p.x), floori(p.z))
	P().teleport(Vector3(floori(p.x) + 0.5, float(top) + 3.0, floori(p.z) + 0.5))
	await ticks(80)
	var y := P().body.pos.y
	check("player lands on terrain and does not fall through", P().body.on_ground and y > float(top) - 0.5,
		"top %d, y %.2f" % [top, y])
	P().stats.reset()
	P().set_gamemode(Player.CREATIVE)


func _creative() -> void:
	check("default game mode is Creative", initial_mode == Player.CREATIVE, "session created without a gamemode key")
	await _build_pad()
	check("creative player may fly", P().may_fly)
	P().flying = true
	P().teleport(Vector3(base.x + 0.5, base.y + 4.0, base.z + 0.5))
	await ticks(40)
	check("flight holds altitude", absf(P().body.pos.y - (base.y + 4.0)) < 0.6, "y %.2f" % P().body.pos.y)
	P().flying = false
	P().teleport(Vector3(base.x + 0.5, base.y, base.z + 0.5))
	await ticks(10)
	# instant break
	var bp := base + Vector3i(0, 0, -2)
	W().set_block(bp.x, bp.y, bp.z, bid("obsidian"), World.F_URGENT)
	_give("diamond_pickaxe")
	var aimed := _aim_block(bp)
	P().interact.break_cooldown = 0
	P().interact._mine()
	check("creative breaks blocks instantly (even obsidian)", aimed and W().get_block(bp.x, bp.y, bp.z) == 0)
	# unlimited placement: a column of stone from a single-item stack
	_give("stone", 1)
	var placed := 0
	for i in 3:
		var tp := bp + Vector3i(0, i - 1, 0)
		P().interact.target = W().raycast(Vector3(tp) + Vector3(0.5, 2.6, 0.5), Vector3.DOWN, 3.0)
		if P().interact.place_block(bid("stone"), 0):
			placed += 1
	var left: ItemStack = P().inventory.stack_at(0)
	check("creative placement does not consume the stack", placed == 3 and left != null and left.count == 1,
		"%d placed, stack %d" % [placed, left.count if left != null else 0])
	# creative inventory
	var ev := InputEventKey.new()
	ev.keycode = KEY_E
	ev.pressed = true
	session.ui._unhandled_key_input(ev)
	await frames(2)
	var scr = session.ui.screen
	check("E opens the creative inventory in Creative", scr is CreativeScreen)
	if scr is CreativeScreen:
		var cs: CreativeScreen = scr
		cs._select("search")
		cs.search.text = "diamond"
		cs._on_search("diamond")
		check("creative search finds items", cs.items.size() >= 10, "%d results for 'diamond'" % cs.items.size())
		cs._select("spawn_eggs")
		check("spawn egg tab lists every mob", cs.items.size() >= MobDB.order.size(),
			"%d eggs / %d mobs" % [cs.items.size(), MobDB.order.size()])
		var tabs_ok := 0
		for t in CreativeScreen.TOP + CreativeScreen.BOTTOM:
			if t == "search" or t == "inventory" or (ItemDB.tab_items.get(t, PackedInt32Array()) as PackedInt32Array).size() > 0:
				tabs_ok += 1
		check("all 12 creative tabs are populated", tabs_ok == 12, "%d/12" % tabs_ok)
	session.ui.close_screen()
	# every registered item reachable from some creative tab
	var reach := {}
	for t in ItemDB.tab_items:
		for i in (ItemDB.tab_items[t] as PackedInt32Array):
			reach[i] = true
	var missing := []
	for it in ItemDB.defs:
		var d: ItemDef = it
		# like the original, written books and filled maps only come from gameplay / commands
		if d == null or d.name in ["air", "written_book", "filled_map"]:
			continue
		if not reach.has(d.id):
			missing.append(d.name)
	check("every registered item is in a creative tab", missing.size() == 0,
		"%d items in tabs, %d missing %s" % [reach.size(), missing.size(), str(missing.slice(0, 8))])
	# every mob spawnable
	var ok_mobs := 0
	var bad := []
	for mn in MobDB.order:
		var m: Mob = session.entities.spawn_mob(W(), String(mn), Vector3(base.x + 6.5, base.y + 1.0, base.z + 6.5), {"persistent": true})
		if m != null and is_instance_valid(m):
			ok_mobs += 1
			m.queue_free()
		else:
			bad.append(mn)
	await frames(2)
	check("every mob can be spawned", bad.is_empty(), "%d/%d %s" % [ok_mobs, MobDB.order.size(), str(bad)])
	# bosses through /summon
	Commands.run(session, "summon wither %d %d %d" % [base.x + 8, base.y + 2, base.z])
	await ticks(5)
	var withers := _mobs("wither")
	check("/summon wither", withers.size() == 1)
	for wt in withers:
		(wt as Node).queue_free()
	Commands.run(session, "summon ender_dragon %d %d %d" % [base.x, base.y + 20, base.z])
	await ticks(5)
	var dragons := _mobs("ender_dragon")
	check("/summon ender_dragon", dragons.size() >= 1)
	for d in dragons:
		(d as Node).queue_free()
	DragonFight.dragon_ref = null
	await frames(2)
	# hostile mobs ignore a creative player
	var z: Mob = session.entities.spawn_mob(W(), "zombie", Vector3(base.x + 2.5, base.y, base.z + 0.5), {"persistent": true})
	var hp0 := P().stats.health
	await ticks(100)
	check("hostile mobs ignore a Creative player", is_instance_valid(z) and z.target == null and P().stats.health >= hp0)
	await _clear_mobs()


func _survival() -> void:
	P().set_gamemode(Player.SURVIVAL)
	P().stats.reset()
	await _build_pad()
	P().inventory.clear()
	check("survival: flight disabled", not P().may_fly and not P().flying)
	var ev := InputEventKey.new()
	ev.keycode = KEY_E
	ev.pressed = true
	session.ui._unhandled_key_input(ev)
	await frames(2)
	check("survival: E opens the survival inventory (no creative catalog)",
		session.ui.screen != null and not (session.ui.screen is CreativeScreen))
	session.ui.close_screen()
	# damage and hunger
	P().stats.invuln = 0
	P().stats.damage(5.0, "generic")
	check("survival: health drops on damage", P().stats.health <= 15.5, "health %.1f" % P().stats.health)
	var food0 := P().stats.food
	P().stats.saturation = 0.0
	P().stats.add_exhaustion(8.5)
	await ticks(2)
	check("survival: exhaustion drains hunger", P().stats.food < food0, "food %d -> %d" % [food0, P().stats.food])
	# eating through the real use pipeline (hold right click)
	P().stats.food = 10
	_give("bread", 4)
	_look_at(P().eye_position() + Vector3(0, 0, -5))
	await _hold("use", 40)
	check("survival: holding use with bread eats it", P().stats.food > 10 and _count("bread") == 3,
		"food %d, bread left %d" % [P().stats.food, _count("bread")])
	P().stats.reset()
	# timed mining (hold left click), drop pickup, durability
	var sp := base + Vector3i(0, 0, -2)
	W().set_block(sp.x, sp.y, sp.z, bid("stone"), World.F_URGENT)
	_give("wooden_pickaxe")
	await ticks(4)
	var n_ticks := 0
	Input.action_press("attack")
	while W().get_block(sp.x, sp.y, sp.z) != 0 and n_ticks < 200:
		_look_at(Vector3(sp) + Vector3(0.5, 0.5, 0.5))
		session.simulate()
		n_ticks += 1
		if n_ticks % 5 == 0:
			await get_tree().process_frame
	Input.action_release("attack")
	check("survival: blocks take time to mine", n_ticks > 5 and W().get_block(sp.x, sp.y, sp.z) == 0,
		"stone with a wooden pickaxe: %d ticks" % n_ticks)
	P().teleport(Vector3(sp) + Vector3(0.5, 0.0, 0.5))
	var picked := false
	for i in 60:
		await ticks(1)
		if _count("cobblestone") > 0:
			picked = true
			break
	check("survival: drops are collected into the inventory", picked)
	var cur: ItemStack = P().inventory.stack_at(0)
	check("survival: tools lose durability", cur != null and cur.damage >= 1, "damage %d" % (cur.damage if cur != null else -1))
	# instant hand-mining of stone takes much longer than with the pickaxe
	check("survival: mining speed depends on the tool", P().interact.dig_speed(bid("stone")) > 0.0)
	# tool tiers: iron ore drops raw iron only with a stone+ pickaxe
	var op := base + Vector3i(1, 0, -2)
	W().set_block(op.x, op.y, op.z, bid("iron_ore"), World.F_URGENT)
	_give("wooden_pickaxe")
	P().interact.break_block(op, W().get_block(op.x, op.y, op.z), true)
	P().teleport(Vector3(op) + Vector3(0.5, 0.0, 0.5))
	await ticks(30)
	var raw0 := _count("raw_iron")
	W().set_block(op.x, op.y + 2, op.z, 0, World.F_URGENT)
	P().teleport(Vector3(sp) + Vector3(0.5, 0.0, 0.5))
	W().set_block(op.x, op.y, op.z, bid("iron_ore"), World.F_URGENT)
	_give("stone_pickaxe")
	P().interact.break_block(op, W().get_block(op.x, op.y, op.z), true)
	P().teleport(Vector3(op) + Vector3(0.5, 0.0, 0.5))
	await ticks(40)
	check("survival: tool tiers (iron ore needs a stone pickaxe)", raw0 == 0 and _count("raw_iron") >= 1,
		"wooden %d, stone %d" % [raw0, _count("raw_iron")])
	# crafting (2x2 inventory grid)
	var grid := [ItemStack.of("oak_planks", 1), null, ItemStack.of("oak_planks", 1), null]
	var r := RecipeDB.find(grid, 2)
	check("survival: 2x2 crafting (planks -> sticks)", not r.is_empty() and (r["out"] as ItemStack).item_name() == "stick")
	# mobs attack in survival
	P().stats.reset()
	P().stats.invuln = 0
	P().teleport(Vector3(base.x + 0.5, base.y, base.z + 0.5))
	var z: Mob = session.entities.spawn_mob(W(), "zombie", Vector3(base.x + 3.5, base.y, base.z + 0.5), {"persistent": true})
	var hurt := false
	for i in 300:
		await ticks(1)
		if P().stats.health < 20.0:
			hurt = true
			break
	check("survival: hostile mobs attack the player", hurt, "health %.1f" % P().stats.health)
	if is_instance_valid(z):
		z.queue_free()
	# death and respawn
	P().inventory.set_stack(5, ItemStack.of("diamond", 7))
	P().stats.invuln = 0
	P().stats.damage(1000.0, "generic", null, true)
	await ticks(25)
	check("survival: the player can die", P().dead)
	var dropped := 0
	for e in session.entities.all():
		if e is SimpleEntities.ItemEntity and (e as SimpleEntities.ItemEntity).stack != null and (e as SimpleEntities.ItemEntity).stack.item_name() == "diamond":
			dropped += (e as SimpleEntities.ItemEntity).stack.count
	check("survival: death drops the inventory", dropped == 7 and _count("diamond") == 0, "%d dropped" % dropped)
	session.respawn_player()
	await _wait_ready()
	await ticks(5)
	check("survival: respawn restores the player", not P().dead and P().stats.health >= 20.0)
	await _clear_mobs()


func _combat() -> void:
	P().set_gamemode(Player.SURVIVAL)
	P().stats.reset()
	await _build_pad(12)
	# melee
	var z: Mob = session.entities.spawn_mob(W(), "zombie", Vector3(base.x + 0.5, base.y, base.z - 2.0), {"persistent": true})
	await ticks(2)
	_give("iron_sword")
	P().interact.attack_strength_ticks = 100
	var hp0 := z.health
	P().interact._attack_entity(z)
	check("melee: a sword hit damages a mob", z.health < hp0, "%.1f -> %.1f" % [hp0, z.health])
	if is_instance_valid(z):
		z.queue_free()
	# bow: draw (hold use) and release
	_give("bow")
	P().inventory.set_stack(9, ItemStack.of("arrow", 16))
	_look_at(Vector3(base.x + 0.5, base.y + 1.6, base.z - 10.0))
	var before := _count_kind("arrow")
	Input.action_press("use")
	for i in 25:
		session.simulate()
	Input.action_release("use")
	session.simulate()
	check("bow: drawing and releasing fires an arrow", _count_kind("arrow") > before and _count("arrow") == 15,
		"arrows in flight %d, quiver %d" % [_count_kind("arrow"), _count("arrow")])
	P().add_effect("resistance", 2000, 4)
	# TNT: terrain and entities
	var tp := base + Vector3i(5, -1, 5)
	var victim: Mob = session.entities.spawn_mob(W(), "pig", Vector3(tp.x + 1.5, base.y, tp.z + 0.5), {"persistent": true})
	var solid_before := _solid_around(tp, 2)
	Explosions.prime_tnt(W(), Vector3(tp.x, tp.y + 1, tp.z), 20)
	var tnt_seen := _count_class_tnt() > 0
	await ticks(40)
	var solid_after := _solid_around(tp, 2)
	check("TNT: primed TNT entity exists", tnt_seen)
	check("TNT: explosion destroys terrain", solid_after < solid_before, "%d -> %d solid blocks" % [solid_before, solid_after])
	check("TNT: explosion damages entities", not is_instance_valid(victim) or victim.dead or victim.health < victim.max_health)
	# creeper: approaches, swells, explodes
	P().teleport(Vector3(base.x - 6.5, base.y, base.z - 5.5))
	var cp := base + Vector3i(-6, -1, -8)
	var c: Mob = session.entities.spawn_mob(W(), "creeper", Vector3(cp.x + 0.5, base.y, cp.z + 0.5), {"persistent": true})
	var c_before := _solid_around(cp, 3)
	var exploded := false
	for i in 300:
		await ticks(1)
		if not is_instance_valid(c) or c.is_queued_for_deletion():
			exploded = true
			break
	await ticks(2)
	var c_after := _solid_around(cp, 3)
	check("creeper: approaches, swells and explodes", exploded and c_after < c_before, "%d -> %d solid" % [c_before, c_after])
	P().stats.reset()
	# Wither: attacks a survival player with skulls
	P().teleport(Vector3(base.x + 0.5, base.y, base.z + 0.5))
	var wt: Mob = session.entities.spawn_mob(W(), "wither", Vector3(base.x + 0.5, base.y + 6.0, base.z - 10.5), {"persistent": true})
	var shots := 0
	for i in 300:
		await ticks(1)
		shots = maxi(shots, _count_shooter(wt))
		if shots > 0:
			break
	check("Wither: attacks with skulls", shots > 0)
	if is_instance_valid(wt):
		wt.queue_free()
	P().clear_effects()
	P().stats.reset()
	await _clear_mobs()
	P().set_gamemode(Player.CREATIVE)


func _count_class_tnt() -> int:
	var n := 0
	for e in session.entities.all():
		if e is SimpleEntities.TntEntity:
			n += 1
	return n


func _count_kind(kind: String) -> int:
	var n := 0
	for e in session.entities.all():
		if e is SimpleEntities.Projectile and String((e as SimpleEntities.Projectile).kind) == kind:
			n += 1
	return n


func _count_shooter(shooter) -> int:
	var n := 0
	for e in session.entities.all():
		if e is SimpleEntities.Projectile and (e as SimpleEntities.Projectile).shooter == shooter:
			n += 1
	return n


func _solid_around(c: Vector3i, r: int) -> int:
	var n := 0
	for x in range(-r, r + 1):
		for y in range(-r, r + 1):
			for z in range(-r, r + 1):
				if W().get_block(c.x + x, c.y + y, c.z + z) != 0:
					n += 1
	return n


func _ui() -> void:
	check("HUD exists", session.hud != null and is_instance_valid(session.hud))
	P().inventory.selected = 4
	check("hotbar selection", P().inventory.selected == 4)
	session.ui.open_screen("inventory", {})
	await frames(2)
	var scr = session.ui.screen
	var lines: Array = (scr as ScreenBase).tooltip_lines(ItemStack.of("diamond_sword")) if scr is ScreenBase else []
	check("tooltips describe items", lines.size() >= 2, str(lines.slice(0, 3)))
	session.ui.close_screen()
	session.ui.open_pause_menu()
	await frames(2)
	check("pause menu opens and pauses the game", session.ui.modal != null and session.paused)
	session.ui.close_modal()
	check("closing the pause menu resumes", not session.paused)


func _portals() -> void:
	P().set_gamemode(Player.CREATIVE)
	await _build_pad(8)
	var f := base + Vector3i(-1, 0, -4)
	for i in 4:
		W().set_block(f.x + i, f.y, f.z, bid("obsidian"), World.F_URGENT)
		W().set_block(f.x + i, f.y + 4, f.z, bid("obsidian"), World.F_URGENT)
	for j in 5:
		W().set_block(f.x, f.y + j, f.z, bid("obsidian"), World.F_URGENT)
		W().set_block(f.x + 3, f.y + j, f.z, bid("obsidian"), World.F_URGENT)
	var lit := Portals.try_light(W(), f + Vector3i(1, 1, 0))
	var inside := BlockDB.name_of(W().get_block(f.x + 1, f.y + 1, f.z))
	check("Nether portal: obsidian frame ignites", lit and inside == "nether_portal", inside)
	P().inventory.set_stack(20, ItemStack.of("heart_of_the_sea", 1))
	var from := Vector3(f.x + 1.5, f.y + 1.0, f.z + 0.5)
	P().teleport(from)
	var t0 := Time.get_ticks_msec()
	while session.dim != 1 and Time.get_ticks_msec() - t0 < 40000:
		if session.dim == 0 and not session.traveling:
			P().teleport(from)
		await ticks(1)
	check("portal: Overworld -> Nether by standing in the portal", session.dim == 1)
	await _wait_ready()
	await ticks(20)
	var np := P().body.pos
	var expect := Vector2(from.x / 8.0, from.z / 8.0)
	check("portal: coordinates map 8:1", Vector2(np.x, np.z).distance_to(expect) < 24.0,
		"overworld %s -> nether %s" % [str(Vector2(from.x, from.z).round()), str(Vector2(np.x, np.z).round())])
	check("portal: inventory preserved", _count("heart_of_the_sea") == 1)
	var nether_mat := 0
	var lava := false
	for gx in range(-6, 7):
		for gz in range(-6, 7):
			var x := floori(np.x) + gx * 5
			var zz := floori(np.z) + gz * 5
			if not W().is_loaded(x, zz):
				continue
			for y in range(10, 40):
				if BlockDB.name_of(W().get_block(x, y, zz)) == "lava":
					lava = true
			for y in range(40, 90, 3):
				var bn := BlockDB.name_of(W().get_block(x, y, zz))
				if bn in ["netherrack", "soul_sand", "soul_soil", "basalt", "blackstone", "crimson_nylium", "warped_nylium"]:
					nether_mat += 1
	check("Nether: dimension-specific terrain", nether_mat > 20, "%d nether blocks sampled" % nether_mat)
	check("Nether: lava sea", lava)
	var ng := W().gen as NetherGen
	var biomes := {}
	for i in 64:
		var ang := TAU * float(i) / 64.0
		for rr in [150.0, 400.0, 900.0, 1600.0]:
			biomes[BiomeDB.name_of(ng.biome_at(int(cos(ang) * float(rr)), int(sin(ang) * float(rr))))] = true
	check("Nether: all five biomes occur", biomes.size() >= 5, str(biomes.keys()))
	var fort: Vector3 = session.locate_structure("nether_fortress", np)
	check("Nether: fortress locatable", fort != Vector3.INF, str(fort))
	var bastion: Vector3 = session.locate_structure("bastion", np)
	check("Nether: bastion locatable", bastion != Vector3.INF, str(bastion))
	var rods := 0
	for i in 12:
		var b: Mob = session.entities.spawn_mob(W(), "blaze", np + Vector3(2, 1, 0), {"persistent": true})
		b.die("player", P())
		await ticks(1)
	await ticks(10)
	for e in session.entities.all():
		if e is SimpleEntities.ItemEntity and (e as SimpleEntities.ItemEntity).stack != null and (e as SimpleEntities.ItemEntity).stack.item_name() == "blaze_rod":
			rods += (e as SimpleEntities.ItemEntity).stack.count
	rods += _count("blaze_rod")
	check("Nether: blazes drop blaze rods", rods > 0, "%d rods from 12 blazes" % rods)
	var ok := 0
	for mn in ["piglin", "hoglin", "ghast", "zombified_piglin", "magma_cube", "wither_skeleton", "strider"]:
		var m: Mob = session.entities.spawn_mob(W(), mn, np + Vector3(0, 3, 3), {"persistent": true})
		if m != null:
			ok += 1
			m.queue_free()
	check("Nether: piglin / hoglin / ghast family spawn", ok == 7, "%d/7" % ok)
	var mark := Vector3i(floori(np.x) + 2, floori(np.y), floori(np.z) + 2)
	W().set_block(mark.x, mark.y, mark.z, bid("gold_block"), World.F_URGENT)
	var back := _find_block_near(P().body.pos, "nether_portal", 6)
	check("portal: a linked portal was built on the Nether side", back != NONE)
	if back != NONE:
		P().portal_cooldown = 0
		var bpos := Vector3(back) + Vector3(0.5, 0.0, 0.5)
		t0 = Time.get_ticks_msec()
		while session.dim != 0 and Time.get_ticks_msec() - t0 < 40000:
			if session.dim == 1 and not session.traveling:
				P().teleport(bpos)
			await ticks(1)
	check("portal: Nether -> Overworld return", session.dim == 0)
	await _wait_ready()
	var home := P().body.pos
	check("portal: returns to the original portal", Vector2(home.x, home.z).distance_to(Vector2(from.x, from.z)) < 8.0,
		"%s vs %s" % [str(Vector2(home.x, home.z).round()), str(Vector2(from.x, from.z).round())])
	check("portal: inventory preserved on the round trip", _count("heart_of_the_sea") == 1)
	session.travel_to(1, Vector3(mark) + Vector3(0.5, 1.0, 0.5), false)
	await _wait_dim(1)
	await _wait_ready()
	check("dimension state persists between visits", BlockDB.name_of(W().get_block(mark.x, mark.y, mark.z)) == "gold_block")
	session.travel_to(0, home, false)
	await _wait_dim(0)
	await _wait_ready()
	await _clear_mobs()


func _find_block_near(p: Vector3, block_name: String, r: int) -> Vector3i:
	var c := Vector3i(floori(p.x), floori(p.y), floori(p.z))
	var id := bid(block_name)
	for y in range(-r, r + 1):
		for x in range(-r, r + 1):
			for z in range(-r, r + 1):
				if (W().get_block(c.x + x, c.y + y, c.z + z) & 0xFFF) == id:
					return Vector3i(c.x + x, c.y + y, c.z + z)
	return NONE


func _end() -> void:
	P().set_gamemode(Player.CREATIVE)
	var sh: Vector3 = session.locate_structure("stronghold", P().body.pos)
	check("End: stronghold locatable (eyes of ender have a target)", sh != Vector3.INF, str(sh))
	# thrown eye of ender flies towards it
	_give("ender_eye", 2)
	var eyes_before := _count_eyes()
	ItemUse.use(P(), P().interact, P().inventory.stack_at(0), 0, {}, true)
	await ticks(2)
	check("End: eye of ender is thrown towards the stronghold", _count_eyes() > eyes_before)
	await _build_pad(8)
	var c := base + Vector3i(0, -1, -5)
	var fid := bid("end_portal_frame")
	for i in range(-1, 2):
		for d in [[i, -2], [i, 2], [-2, i], [2, i]]:
			W().set_block(c.x + int(d[0]), c.y, c.z + int(d[1]), Vox.make(fid, 4), World.F_URGENT)
	var opened := Portals.check_end_portal(W(), c + Vector3i(1, 0, -2))
	check("End portal: 12 filled frames activate the portal", opened and BlockDB.name_of(W().get_block(c.x, c.y, c.z)) == "end_portal")
	P().teleport(Vector3(c.x + 0.5, c.y + 0.1, c.z + 0.5))
	var t0 := Time.get_ticks_msec()
	var last_dump := 0
	while session.dim != 2 and Time.get_ticks_msec() - t0 < 40000:
		await ticks(1)
		if Time.get_ticks_msec() - last_dump > 4000:
			last_dump = Time.get_ticks_msec()
			var pp := P().body.pos
			var fb := Vector3i(floori(pp.x), floori(pp.y + 0.5), floori(pp.z))
			var ew = session.worlds.get(2, null)
			print("    [end-portal] dim=%d traveling=%s pos=%s feet=%s cooldown=%d dead=%s end_ready=%s" % [session.dim,
				str(session.traveling), str(pp.round()), BlockDB.name_of(W().get_blockv(fb)), P().portal_cooldown, str(P().dead),
				str((ew as World).is_ready_at(100, 0)) if ew != null else "no world"])
	check("End portal: travel to the End", session.dim == 2)
	await _wait_ready()
	await ticks(40)
	var dragon = DragonFight.dragon_entity(session)
	check("End: Ender Dragon exists", dragon != null)
	check("End: ten crystals on the obsidian spikes", DragonFight.crystals().size() == 10, "%d" % DragonFight.crystals().size())
	check("End: arrival on the obsidian platform", BlockDB.name_of(W().get_block(100, 48, 0)) == "obsidian")
	# walk over to the main island so the whole arena streams in
	P().teleport(DragonFight.arrival_point(W()))
	await _wait_ready()
	var t1 := Time.get_ticks_msec()
	var all_ready := false
	while not all_ready and Time.get_ticks_msec() - t1 < 30000:
		all_ready = true
		for sp0 in EndGen.spikes(W().seed_value):
			if not W().is_ready_at(int(sp0[0]), int(sp0[1])):
				all_ready = false
		await get_tree().process_frame
	P().teleport(DragonFight.arrival_point(W()))
	await ticks(10)
	var spikes := 0
	for sp in EndGen.spikes(W().seed_value):
		if BlockDB.name_of(W().get_block(int(sp[0]), int(sp[3]) - 2, int(sp[1]))) == "obsidian":
			spikes += 1
	check("End: obsidian spikes generated", spikes == 10, "%d/10" % spikes)
	if dragon == null:
		return
	var dm: Mob = dragon
	dm.health = dm.max_health - 40.0
	var h0 := dm.health
	var cr = DragonFight.crystals()[0]
	var healed := false
	for i in 80:
		dm.body.pos = (cr as Node3D).global_position + Vector3(6, 4, 0)
		await ticks(1)
		if dm.health > h0:
			healed = true
			break
	check("End: crystals heal the dragon", healed, "%.0f -> %.0f" % [h0, dm.health])
	for k in DragonFight.crystals():
		(k as Entity).hurt(1.0, "player", P())
	await ticks(5)
	check("End: crystals can be destroyed", DragonFight.crystals().size() == 0)
	dm.invuln = 0
	dm.hurt(10000.0, "player", P())
	await ticks(60)
	check("End: the dragon can be defeated", DragonFight.defeated)
	var portal_blocks := 0
	for x in range(-3, 4):
		for z in range(-3, 4):
			if BlockDB.name_of(W().get_block(x, EndGen.PODIUM_Y, z)) == "end_portal":
				portal_blocks += 1
	check("End: exit portal activates", portal_blocks >= 12, "%d portal blocks" % portal_blocks)
	check("End: dragon egg on the podium", BlockDB.name_of(W().get_block(0, EndGen.PODIUM_Y + 4, 0)) == "dragon_egg")
	var gw := NONE
	for i in 20:
		var ang := TAU * float(i) / 20.0 + 0.07
		for rr in range(78, 118):
			var x := int(round(cos(ang) * rr))
			var z := int(round(sin(ang) * rr))
			if not W().is_loaded(x, z):
				continue
			for y in range(40, 90):
				if BlockDB.name_of(W().get_block(x, y, z)) == "end_gateway":
					gw = Vector3i(x, y, z)
					break
			if gw != NONE:
				break
		if gw != NONE:
			break
	check("End: exit gateway generated after the fight", gw != NONE, str(gw))
	if gw != NONE:
		P().portal_cooldown = 0
		P().teleport(Vector3(gw) + Vector3(0.5, 0.0, 0.5))
		t0 = Time.get_ticks_msec()
		while Time.get_ticks_msec() - t0 < 40000:
			await ticks(1)
			if Vector2(P().body.pos.x, P().body.pos.z).length() > 600.0 and P().body.on_ground:
				break
		var dd := Vector2(P().body.pos.x, P().body.pos.z).length()
		check("End: gateway reaches the outer islands and lands on ground", dd > 600.0 and P().body.on_ground,
			"distance %.0f, pos %s" % [dd, str(P().body.pos.round())])
		var ret := NONE
		for i in 100:
			await ticks(1)
			ret = _find_block_near(P().body.pos, "end_gateway", 6)
			if ret != NONE:
				break
		check("End: a return gateway is built on the outer island", ret != NONE)
	var city: Vector3 = session.locate_structure("end_city", P().body.pos)
	check("End: End city locatable", city != Vector3.INF, str(city))
	var shulker: Mob = session.entities.spawn_mob(W(), "shulker", P().body.pos + Vector3(2, 0, 0), {"persistent": true})
	check("End: shulkers spawn", shulker != null)
	var ely := ItemDB.get_by_name("elytra")
	check("End: elytra exists and is worn in the chest slot", ely != null and ely.armor_slot == 2)
	session.travel_to(0, Vector3.INF, false)
	await _wait_dim(0)
	await _wait_ready()
	await _clear_mobs()


func _count_eyes() -> int:
	var n := 0
	for e in session.entities.all():
		if e is SpecialEntities.EyeOfEnder:
			n += 1
	return n


func _extras() -> void:
	P().set_gamemode(Player.SURVIVAL)
	P().stats.reset()
	P().add_effect("resistance", 6000, 4)
	await _build_pad(12)
	# --- boat on a pool: W paddles forward
	var pool := base + Vector3i(4, -1, -6)
	for x in range(-3, 4):
		for z in range(-3, 4):
			W().set_block(pool.x + x, pool.y, pool.z + z, bid("water"), World.F_URGENT)
			W().set_block(pool.x + x, pool.y - 1, pool.z + z, bid("stone"), World.F_URGENT)
	await ticks(5)
	var boat = session.entities.spawn_vehicle(W(), "boat", Vector3(pool.x + 0.5, pool.y + 0.6, pool.z + 0.5), {"wood": "oak"})
	await ticks(10)
	var mounted: bool = boat.interact(P(), null)
	check("boat: right-click mounts the boat", mounted and P().riding == boat)
	var start: Vector3 = boat.body.pos
	boat.facing = 0.0
	Input.action_press("move_forward")
	await ticks(30)
	Input.action_release("move_forward")
	var moved: float = Vector2(boat.body.pos.x - start.x, boat.body.pos.z - start.z).length()
	check("boat: paddles forward on water and floats", moved > 1.0 and absf(boat.body.pos.y - start.y) < 0.8,
		"moved %.2f, dy %.2f" % [moved, boat.body.pos.y - start.y])
	check("boat: the rider is carried on the seat", P().body.pos.distance_to(boat.body.pos) < 1.0)
	Input.action_press("sneak")
	await ticks(2)
	Input.action_release("sneak")
	await ticks(2)
	check("boat: sneak dismounts", P().riding == null and boat.rider == null)
	boat.hurt(10.0, "player", P())
	await ticks(2)
	check("boat: breaking it drops the boat item", not is_instance_valid(boat) or boat.is_queued_for_deletion())
	# --- minecart on an L-shaped track: rails auto-curve, the cart follows the curve
	var r0 := base + Vector3i(-8, 0, 4)
	P().teleport(Vector3(base.x + 0.5, base.y, base.z + 0.5))
	var rail := bid("rail")
	var track := []
	for i in 6:
		track.append(r0 + Vector3i(i, 0, 0))
	for j in range(1, 6):
		track.append(r0 + Vector3i(5, 0, -j))
	for t in track:
		var tp: Vector3i = t
		W().set_block(tp.x, tp.y, tp.z, rail, World.F_URGENT)
		BlockBehaviors.on_placed(W(), tp, W().get_block(tp.x, tp.y, tp.z), P())
	var corner := r0 + Vector3i(5, 0, 0)
	var cshape := Rails.shape_of(W().get_block(corner.x, corner.y, corner.z))
	check("rails: placing an L-shaped track makes a curve", cshape >= 6 and cshape <= 9, "corner shape %d" % cshape)
	var cart = session.entities.spawn_vehicle(W(), "minecart", Vector3(r0.x + 0.5, r0.y, r0.z + 0.5), {})
	cart.body.vel = Vector3(0.3, 0, 0)
	for i in 80:
		await ticks(1)
	var cp: Vector3 = cart.body.pos
	check("minecart: follows the rails round the curve", cp.z < float(corner.z) - 1.0 and absf(cp.x - (corner.x + 0.5)) < 0.3,
		"cart at %s, corner %s" % [str(cp.snapped(Vector3(0.1, 0.1, 0.1))), str(corner)])
	cart.queue_free()
	# --- horse: saddle, mount, steer once tamed
	var horse: Mob = session.entities.spawn_mob(W(), "horse", Vector3(base.x - 4.5, base.y, base.z + 0.5), {"persistent": true})
	await ticks(2)
	horse.tamed = true
	var saddle := _give("saddle")
	horse.interact(P(), saddle)
	check("horse: a saddle can be put on a tamed horse", bool(horse.data.get("saddled", false)))
	P().inventory.set_stack(0, null)
	check("horse: empty hand mounts it", horse.interact(P(), null) and P().riding == horse)
	var hs := horse.body.pos
	P().yaw = 0.0
	Input.action_press("move_forward")
	await ticks(30)
	Input.action_release("move_forward")
	check("horse: the rider steers it", horse.body.pos.distance_to(hs) > 2.0, "moved %.2f" % horse.body.pos.distance_to(hs))
	Riding.dismount(P())
	horse.queue_free()
	# --- breeding: two cows fed wheat make a calf
	var c1: Mob = session.entities.spawn_mob(W(), "cow", Vector3(base.x + 2.5, base.y, base.z + 2.5), {"persistent": true})
	var c2: Mob = session.entities.spawn_mob(W(), "cow", Vector3(base.x + 4.5, base.y, base.z + 2.5), {"persistent": true})
	await ticks(2)
	var wheat := _give("wheat", 8)
	c1.interact(P(), wheat)
	c2.interact(P(), P().inventory.stack_at(0))
	var babies := 0
	for i in 200:
		await ticks(1)
		babies = 0
		for e in _mobs("cow"):
			if (e as Mob).baby:
				babies += 1
		if babies > 0:
			break
	check("breeding: two fed cows make a calf", babies == 1, "%d calves" % babies)
	check("breeding: parents get a cooldown", int(c1.data.get("breed_cd", 0)) > 0)
	# --- elytra: jump while falling opens the wings
	P().inventory.set_stack(Inventory.ARMOR + 2, ItemStack.of("elytra", 1))
	P().teleport(Vector3(base.x + 0.5, base.y + 30.0, base.z + 0.5))
	await ticks(8)
	Input.action_press("jump")
	await ticks(2)
	Input.action_release("jump")
	await ticks(2)
	check("elytra: jumping while falling starts gliding", P().elytra_flying)
	var gy := P().body.pos.y
	P().pitch = -0.2
	await ticks(20)
	var hor := Vector2(P().body.vel.x, P().body.vel.z).length()
	check("elytra: glides forward", hor > 0.2, "horizontal speed %.2f b/t" % hor)
	P().elytra_flying = false
	P().inventory.set_stack(Inventory.ARMOR + 2, null)
	P().teleport(Vector3(base.x + 0.5, base.y, base.z + 0.5))
	await ticks(10)
	P().stats.reset()
	# --- skeleton arrows hit the player
	P().clear_effects()
	P().stats.reset()
	var sk: Mob = session.entities.spawn_mob(W(), "skeleton", Vector3(base.x + 0.5, base.y, base.z - 8.5), {"persistent": true})
	var hit := false
	for i in 300:
		await ticks(1)
		if P().stats.health < 20.0:
			hit = true
			break
	check("skeleton arrows hit the player", hit, "health %.1f" % P().stats.health)
	if is_instance_valid(sk):
		sk.queue_free()
	P().stats.reset()
	P().add_effect("resistance", 6000, 4)
	# --- undead burn in daylight
	session.day_time = 6000
	session.weather = 0
	var zb: Mob = session.entities.spawn_mob(W(), "zombie", Vector3(base.x - 6.5, base.y, base.z - 6.5), {"persistent": true})
	var burned := false
	for i in 200:
		await ticks(1)
		if not is_instance_valid(zb) or zb.fire_ticks > 0:
			burned = true
			break
	check("zombies burn in daylight", burned)
	if is_instance_valid(zb):
		zb.queue_free()
	# --- pressure plate powers when stepped on
	var pp := base + Vector3i(-2, 0, 2)
	W().set_block(pp.x, pp.y, pp.z, bid("stone_pressure_plate"), World.F_URGENT)
	P().teleport(Vector3(pp) + Vector3(0.5, 0.0, 0.5))
	await ticks(6)
	check("pressure plate: powers when stood on", (W().get_block(pp.x, pp.y, pp.z) >> 12) > 0)
	P().teleport(Vector3(base.x + 0.5, base.y, base.z + 0.5))
	# --- lightning turns a pig into a zombified piglin
	var pig: Mob = session.entities.spawn_mob(W(), "pig", Vector3(base.x + 6.5, base.y, base.z + 6.5), {"persistent": true})
	await ticks(1)
	session.strike_lightning(pig.body.pos)
	await ticks(2)
	check("lightning: strikes convert pigs to zombified piglins", _mobs("zombified_piglin").size() >= 1)
	await _clear_mobs()
	# --- fishing in the pool
	P().teleport(Vector3(pool.x + 0.5, pool.y + 1.0, pool.z + 4.5))
	await ticks(3)
	_give("fishing_rod")
	_look_at(Vector3(pool.x + 0.5, pool.y + 0.5, pool.z + 0.5))
	ItemUse.use(P(), P().interact, P().inventory.stack_at(0), 0, {}, true)
	var bob = null
	for i in 40:
		await ticks(1)
		for e in session.entities.all():
			if e is SimpleEntities.Projectile and (e as SimpleEntities.Projectile).kind == "fishing_bobber":
				bob = e
		if bob != null and BlockDB.name_of(W().get_block(floori(bob.body.pos.x), floori(bob.body.pos.y), floori(bob.body.pos.z))) == "water":
			break
	check("fishing: the rod casts a bobber into the water", bob != null)
	var fish_before := 0
	for e in session.entities.all():
		if e is SimpleEntities.ItemEntity:
			fish_before += 1
	if bob != null:
		bob.data["wait"] = 2
		await ticks(3)
		ItemUse.use(P(), P().interact, P().inventory.stack_at(0), 0, {}, true)
		await ticks(2)
	var fish_after := 0
	for e in session.entities.all():
		if e is SimpleEntities.ItemEntity:
			fish_after += 1
	check("fishing: reeling in on a bite catches something", fish_after > fish_before)
	# --- name tag, lead, pets
	P().teleport(Vector3(base.x + 0.5, base.y, base.z + 0.5))
	await ticks(3)
	var tagged: Mob = session.entities.spawn_mob(W(), "pig", Vector3(base.x + 2.5, base.y, base.z + 0.5), {"persistent": true})
	var tag := _give("name_tag")
	tag.data["name"] = "Rex"
	tagged.interact(P(), tag)
	check("name tag: names a mob (label above it)", String(tagged.data.get("custom_name", "")) == "Rex" and tagged.visual.get_node_or_null("NameTag") != null)
	var cow2: Mob = session.entities.spawn_mob(W(), "cow", Vector3(base.x - 2.5, base.y, base.z + 0.5), {"persistent": true})
	var lead_st := _give("lead")
	cow2.interact(P(), lead_st)
	P().teleport(Vector3(base.x + 7.5, base.y, base.z + 0.5))
	await ticks(60)
	check("lead: a leashed animal follows the player", bool(cow2.data.get("leashed", false)) and cow2.body.pos.distance_to(P().body.pos) < 6.5,
		"distance %.1f" % cow2.body.pos.distance_to(P().body.pos))
	var wolf: Mob = session.entities.spawn_mob(W(), "wolf", Vector3(base.x - 6.5, base.y, base.z + 0.5), {"persistent": true})
	wolf.tamed = true
	wolf.sitting = false
	await ticks(80)
	check("pets: a tamed wolf follows its owner", wolf.body.pos.distance_to(P().body.pos) < 5.0, "distance %.1f" % wolf.body.pos.distance_to(P().body.pos))
	P().inventory.set_stack(0, null)
	wolf.interact(P(), null)
	check("pets: right-click makes it sit", wolf.sitting)
	# --- mending repairs with XP; compass read-out; chorus fruit
	var pick := ItemStack.of("diamond_pickaxe", 1)
	pick.damage = 100
	pick.data["ench"] = {"mending": 1}
	P().inventory.set_stack(0, pick)
	P().inventory.selected = 0
	session.entities.spawn_xp(W(), P().body.pos + Vector3(0, 0.5, 0), 20)
	await ticks(30)
	check("mending: XP repairs the held tool", pick.damage < 100, "damage %d" % pick.damage)
	_give("compass")
	check("compass: shows the way to spawn", ItemExtras.held_readout(P()).begins_with("Spawn"), ItemExtras.held_readout(P()))
	var before_tp := P().body.pos
	var chorus := _give("chorus_fruit", 2)
	ItemUse.eat(P(), chorus, P().interact, 0)
	check("chorus fruit: teleports the player", P().body.pos.distance_to(before_tp) > 0.5)
	P().clear_effects()
	await _clear_mobs()


func _persistence() -> void:
	P().set_gamemode(Player.SURVIVAL)
	await ticks(10)
	var p := P().body.pos
	var mark := Vector3i(floori(p.x) + 3, floori(p.y) + 1, floori(p.z))
	W().set_block(mark.x, mark.y, mark.z, bid("emerald_block"), World.F_URGENT)
	P().inventory.set_stack(30, ItemStack.of("nether_star", 3))
	var cow: Mob = session.entities.spawn_mob(W(), "cow", p + Vector3(2, 0, 2), {"persistent": true})
	cow.data["tag"] = "saved_cow"
	session.day_time = 13000
	session.save_all()
	await frames(2)
	var saved_pos := P().body.pos
	session.queue_free()
	await frames(3)
	DragonFight.reset()
	var params := SaveManager.read_level(FOLDER)
	check("save/load: level.json readable", not params.is_empty())
	if params.is_empty():
		return
	# exactly what the title screen passes when a world is picked
	var pl: Dictionary = params.get("player", {})
	params["folder"] = FOLDER
	params["gamemode"] = int(pl.get("gamemode", Player.CREATIVE))
	await _open_session(params)
	await ticks(10)
	check("save/load: player position restored", P().body.pos.distance_to(saved_pos) < 2.0,
		"%s vs %s" % [str(P().body.pos.round()), str(saved_pos.round())])
	check("save/load: game mode restored", P().gamemode == Player.SURVIVAL)
	check("save/load: inventory restored", _count("nether_star") == 3)
	check("save/load: placed block restored", BlockDB.name_of(W().get_block(mark.x, mark.y, mark.z)) == "emerald_block")
	check("save/load: time of day restored", absi(session.day_time - 13000) < 100, "%d" % session.day_time)
	check("save/load: dragon defeat remembered", DragonFight.defeated)
	var found_cow := false
	for i in 150:
		for e in _mobs("cow"):
			if String((e as Mob).data.get("tag", "")) == "saved_cow":
				found_cow = true
		if found_cow:
			break
		await ticks(1)
	check("save/load: live mobs near the player are saved", found_cow)
