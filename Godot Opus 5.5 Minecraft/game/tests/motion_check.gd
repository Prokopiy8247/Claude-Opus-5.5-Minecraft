extends Node
## Render interpolation check: mobs, projectiles and vehicles must be drawn between their last two
## simulated positions (no back-and-forth sweeps between ticks), yaw must blend between ticks and
## rendering must not change gameplay state.
##   godot --headless --path . res://game/tests/motion_check.tscn

const FOLDER := "motion_test"
const MOBS := ["pig", "cow", "sheep", "chicken", "zombie", "skeleton", "creeper", "spider", "wolf",
	"horse", "villager", "iron_golem", "fox", "rabbit"]
const ALPHAS := [0.0, 0.25, 0.5, 0.75, 1.0]

var session: WorldSession = null
var passed := 0
var failed := 0
var base := Vector3i.ZERO


func _ready() -> void:
	Game.init_registries()
	SaveManager.delete_world(FOLDER)
	session = WorldSession.new()
	add_child(session)
	session.setup({"name": "MotionTest", "folder": FOLDER, "seed": 20260925, "difficulty": 2,
		"gamerules": {"doMobSpawning": false}, "player": {}})
	await get_tree().process_frame
	session.start()
	var t0 := Time.get_ticks_msec()
	while Time.get_ticks_msec() - t0 < 30000:
		var p := session.player.body.pos
		if session.world.is_ready_at(floori(p.x), floori(p.z)) and not session.traveling:
			break
		await get_tree().process_frame
	await _pad(14)
	# the test drives the ticks itself: no real-time ticks between a snapshot and its evaluation
	session.paused = true
	_paths()
	await _mobs_walk()
	await _mobs_wander()
	await _projectile()
	_vehicle()
	print("MOTION: %d passed, %d failed" % [passed, failed])
	session.paused = false
	SaveManager.delete_world(FOLDER)
	get_tree().quit(0 if failed == 0 else 1)


func check(what: String, ok: bool, detail := "") -> void:
	if ok:
		passed += 1
	else:
		failed += 1
	print("  %s %s%s" % ["PASS" if ok else "FAIL", what, ("  (" + detail + ")") if detail != "" else ""])


func _pad(r: int) -> void:
	var w := session.world
	var p := session.player.body.pos
	base = Vector3i(floori(p.x), clampi(floori(p.y), 70, 200), floori(p.z))
	# edits to chunks that are not generated yet are dropped: wait for the whole area first
	var t0 := Time.get_ticks_msec()
	var all_ready := false
	while not all_ready and Time.get_ticks_msec() - t0 < 30000:
		all_ready = true
		for x in range(-r - 16, r + 17, 8):
			for z in range(-r - 16, r + 17, 8):
				if not w.is_ready_at(base.x + x, base.z + z):
					all_ready = false
		await get_tree().process_frame
	check("test area loaded", all_ready)
	for x in range(-r, r + 1):
		for z in range(-r, r + 1):
			w.set_block(base.x + x, base.y - 1, base.z + z, BlockDB.id("smooth_stone"), World.F_URGENT)
			for y in range(0, 8):
				w.set_block(base.x + x, base.y + y, base.z + z, 0, World.F_URGENT)
	session.player.teleport(Vector3(base.x + 0.5, base.y, base.z + 0.5))
	for i in 10:
		session.simulate()
		await get_tree().process_frame


static func _seg_dist(p: Vector3, a: Vector3, b: Vector3) -> float:
	var ab := b - a
	var l2 := ab.length_squared()
	if l2 < 1e-12:
		return p.distance_to(a)
	var t := clampf((p - a).dot(ab) / l2, 0.0, 1.0)
	return p.distance_to(a + ab * t)


# ------------------------------------------------------------------------------------------------
## Block paths are straightened on open ground but still go around walls.
func _paths() -> void:
	var w := session.world
	var a := Vector3(base.x - 7.5, base.y, base.z - 5.5)
	var b := Vector3(base.x + 6.5, base.y, base.z + 4.5)
	var p := Pathfinder.find(w, a, b, 300, 3, 2, 0.6)
	var ok := p.size() > 0 and p[p.size() - 1].distance_to(b) < 0.8
	check("diagonal path on open ground is one straight line", ok and p.size() <= 2, "%d waypoints" % p.size())
	# a wall across the way (with a gap at one end): the path must not cut through it
	var wz := base.z
	for x in range(-9, 8):
		w.set_block(base.x + x, base.y, wz, BlockDB.id("stone"), World.F_URGENT)
		w.set_block(base.x + x, base.y + 1, wz, BlockDB.id("stone"), World.F_URGENT)
	var q := Pathfinder.find(w, a, b, 600, 3, 2, 0.6)
	var through := false
	var prev := a
	for wp in q:
		var n := ceili(prev.distance_to(wp) * 8.0)
		for s in n + 1:
			var t := float(s) / float(maxi(n, 1))
			var pt := prev.lerp(wp, t)
			var cell := Vector3i(floori(pt.x), base.y, floori(pt.z))
			if cell.z == wz and cell.x >= base.x - 9 and cell.x <= base.x + 7:
				if not through:
					print("    crossing at %s on %s -> %s" % [str(pt - Vector3(base)), str(prev - Vector3(base)), str(wp - Vector3(base))])
				through = true
		prev = wp
	if through or q.size() > 6:
		var rel := []
		for wp in q:
			rel.append(str(wp - Vector3(base)))
		print("    path: ", ", ".join(PackedStringArray(rel)))
	check("straightened path still goes around a wall", q.size() > 0 and not through
		and q[q.size() - 1].distance_to(b) < 0.8, "%d waypoints, crosses wall: %s" % [q.size(), through])
	for x in range(-9, 8):
		w.set_block(base.x + x, base.y, wz, 0, World.F_URGENT)
		w.set_block(base.x + x, base.y + 1, wz, 0, World.F_URGENT)


# ------------------------------------------------------------------------------------------------
func _mobs_walk() -> void:
	var w := session.world
	var mobs: Array = []
	for i in MOBS.size():
		var ang := TAU * float(i) / MOBS.size()
		var pos := Vector3(base.x + 0.5 + cos(ang) * 5.0, base.y, base.z + 0.5 + sin(ang) * 5.0)
		var m := session.entities.spawn_mob(w, String(MOBS[i]), pos)
		if m != null:
			mobs.append(m)
	check("test mobs spawned", mobs.size() == MOBS.size(), "%d/%d" % [mobs.size(), MOBS.size()])
	# settle on the pad, then send every mob on a walk (straight out, then back past its start)
	for i in 10:
		session.simulate()
	for m in mobs:
		var mb: Mob = m
		var out := Vector3(mb.body.pos.x - base.x - 0.5, 0, mb.body.pos.z - base.z - 0.5).normalized()
		var side := out.rotated(Vector3.UP, PI * 0.5)
		var pts := PackedVector3Array()
		for k in range(1, 7):
			pts.append(mb.body.pos + out * k)
		for k in range(1, 7):
			pts.append(mb.body.pos + out * 6.0 + side * k)
		mb.path = pts
		mb.path_i = 0
		mb.wander_cd = 1000
	var worst := 0.0
	var worst_mob := ""
	var render_len := 0.0
	var real_len := 0.0
	var yaw_events := 0
	var yaw_bad := 0
	var last_rp := {}
	var drawn_end := {}
	for t in 120:
		var before := {}
		for m in mobs:
			if is_instance_valid(m):
				before[m] = (m as Mob).body.pos
		session.simulate()
		for m in mobs:
			if not is_instance_valid(m) or not before.has(m) or (m as Mob).visual == null:
				continue
			var mb: Mob = m
			var a: Vector3 = before[m]
			var b: Vector3 = mb.body.pos
			real_len += a.distance_to(b)
			var yaw0 := 0.0
			for al in ALPHAS:
				mb.frame(float(al))
				if float(al) == 0.0:
					yaw0 = mb.visual.rotation.y
				var rp := mb.global_position
				var e := _seg_dist(rp, a, b)
				if e > worst:
					worst = e
					worst_mob = mb.mob
				if last_rp.has(m):
					render_len += rp.distance_to(last_rp[m])
				last_rp[m] = rp
			# the drawn yaw starts each tick where the previous one ended (turns blend, no jumps)
			var yaw1 := mb.visual.rotation.y
			if drawn_end.has(m) and absf(angle_difference(yaw0, yaw1)) > 0.02:
				yaw_events += 1
			if drawn_end.has(m) and absf(angle_difference(yaw0, float(drawn_end[m]))) > 0.001:
				yaw_bad += 1
			drawn_end[m] = yaw1
		if t % 5 == 4:
			await get_tree().process_frame
	check("mobs actually walked", real_len > 20.0, "%.1f blocks in total" % real_len)
	check("mobs are drawn between their last two tick positions", worst < 0.01,
		"worst off-segment distance %.3f blocks (%s)" % [worst, worst_mob])
	var ratio := render_len / maxf(real_len, 0.001)
	check("drawn path length equals real path length (no back-and-forth)", ratio < 1.02 and ratio > 0.98,
		"drawn %.1f vs real %.1f blocks, ratio %.2f" % [render_len, real_len, ratio])
	check("mob turns are blended between ticks", yaw_events > 0 and yaw_bad == 0,
		"%d turning ticks, %d yaw jumps at tick boundaries" % [yaw_events, yaw_bad])
	for m in mobs:
		if is_instance_valid(m):
			(m as Node).queue_free()
	await get_tree().process_frame


# ------------------------------------------------------------------------------------------------
## Free wandering with the real AI and pathfinder: the drawn body must not snap around or zig-zag
## (4-connected block paths turned into left-right-left heading flips every block).
func _mobs_wander() -> void:
	var w := session.world
	var mobs: Array = []
	for i in MOBS.size():
		var ang := TAU * float(i) / MOBS.size()
		var pos := Vector3(base.x + 0.5 + cos(ang) * 3.0, base.y, base.z + 0.5 + sin(ang) * 3.0)
		var m := session.entities.spawn_mob(w, String(MOBS[i]), pos)
		if m != null:
			m.wander_cd = 0
			mobs.append(m)
	var walked := 0.0
	var turned := 0.0
	var snaps := 0
	var max_step := 0.0
	var last_yaw := {}
	var steps := 0
	for t in 400:
		var before := {}
		for m in mobs:
			if is_instance_valid(m):
				before[m] = (m as Mob).body.pos
		session.simulate()
		for m in mobs:
			if not is_instance_valid(m) or not before.has(m) or (m as Mob).visual == null:
				continue
			var mb: Mob = m
			var a: Vector3 = before[m]
			var moved := Vector2(mb.body.pos.x - a.x, mb.body.pos.z - a.z).length()
			mb.frame(1.0)
			var yaw := mb.visual.rotation.y
			if last_yaw.has(m) and moved > 0.01:
				var dy := absf(angle_difference(float(last_yaw[m]), yaw))
				walked += moved
				turned += dy
				max_step = maxf(max_step, dy)
				steps += 1
				if dy > 1.2:
					snaps += 1
			last_yaw[m] = yaw
		if t % 5 == 4:
			await get_tree().process_frame
	var per_block := turned / maxf(walked, 0.001)
	check("wandering mobs walked", walked > 15.0, "%.1f blocks, %d moving ticks" % [walked, steps])
	check("wandering mobs do not snap around (drawn yaw)", snaps == 0,
		"%d snaps over 1.2 rad in one tick, largest %.2f rad" % [snaps, max_step])
	check("wandering mobs walk straight, no zig-zag", per_block < 0.35,
		"heading change %.2f rad per block walked" % per_block)
	for m in mobs:
		if is_instance_valid(m):
			(m as Node).queue_free()
	await get_tree().process_frame


# ------------------------------------------------------------------------------------------------
func _projectile() -> void:
	var w := session.world
	var start := Vector3(base.x + 0.5, base.y + 2.5, base.z + 0.5)
	var arrow = session.entities.spawn_projectile(w, "arrow", start, Vector3(14, 6, 3), null)
	var worst := 0.0
	var flew := 0.0
	var stuck_ticks := 0
	var stuck_jitter := 0.0
	for t in 80:
		if not is_instance_valid(arrow):
			break
		var a: Vector3 = arrow.body.pos
		var was_stuck: bool = arrow.stuck
		session.simulate()
		if not is_instance_valid(arrow):
			break
		var b: Vector3 = arrow.body.pos
		flew += a.distance_to(b)
		for al in ALPHAS:
			arrow.frame(float(al))
			worst = maxf(worst, _seg_dist(arrow.global_position, a, b))
		if was_stuck and arrow.stuck:
			stuck_ticks += 1
			arrow.frame(0.0)
			var p0: Vector3 = arrow.global_position
			arrow.frame(1.0)
			stuck_jitter = maxf(stuck_jitter, p0.distance_to(arrow.global_position))
	check("arrow flew", flew > 3.0, "%.1f blocks" % flew)
	check("arrow is drawn between its last two tick positions", worst < 0.01, "worst %.3f blocks" % worst)
	check("stuck arrow stays still between ticks", stuck_ticks > 0 and stuck_jitter < 0.001,
		"%d stuck ticks, jitter %.3f" % [stuck_ticks, stuck_jitter])
	if is_instance_valid(arrow):
		arrow.queue_free()
	await get_tree().process_frame


func _vehicle() -> void:
	var boat = session.entities.spawn_vehicle(session.world, "boat", Vector3(base.x + 3.5, base.y, base.z + 0.5))
	boat.prev_facing = 0.0
	boat.facing = 1.0
	boat.frame(0.5)
	var drawn: float = boat.visual.rotation.y
	boat.frame(0.9)
	check("rendering a vehicle does not change its heading", is_equal_approx(boat.facing, 1.0),
		"facing %.3f after two frames" % boat.facing)
	check("vehicle yaw is blended between ticks", absf(drawn - 0.5) < 0.01, "drawn %.3f at alpha 0.5" % drawn)
	boat.queue_free()
