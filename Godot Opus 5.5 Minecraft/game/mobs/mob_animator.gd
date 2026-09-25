class_name MobAnimator
extends RefCounted
## Procedural animation for cuboid rigs: leg/arm swing, idle sway, head look, wing flap, tail wave,
## slime squash, ghast tentacle drift, blaze rod orbit, dragon wing beats, creeper swell.
## Every part keeps its rest pose in the "base_rot"/"base_pos" metadata; animation adds offsets.

static func _rot(n: Node3D, add: Vector3) -> void:
	var b: Vector3 = n.get_meta("base_rot", Vector3.ZERO)
	n.rotation = b + add


static func animate(m: Mob, alpha: float) -> void:
	var rig := m.rig
	if rig == null:
		return
	var arch: String = m.def.get("arch", "passive")
	var t := float(m.age) + alpha
	var moving := m.move_speed
	var walk := m.walk_phase + alpha * moving * 4.0
	var parts := m.parts
	var amt := minf(1.0, moving * 9.0)
	var body: Node3D = m.body_node
	if body != null:
		var bp: Vector3 = body.get_meta("base_pos", body.position)
		body.position = bp + Vector3(0, absf(sin(walk)) * 0.02 * amt, 0)
	# four legs
	var keys := ["leg_fl", "leg_fr", "leg_bl", "leg_br"]
	for i in 4:
		var l: Node3D = parts.get(keys[i], null)
		if l == null:
			continue
		var phase: float = [0.0, PI, PI, 0.0][i]
		_rot(l, Vector3(sin(walk + phase) * 0.9 * amt, 0, 0))
	# numbered legs (spider, sniffer)
	for i in 8:
		var sl: Node3D = parts.get("leg_%d" % (i + 1), null)
		if sl == null:
			continue
		var ph := i * 0.8 + (PI if i % 2 == 0 else 0.0)
		_rot(sl, Vector3(0, sin(walk * 1.3 + ph) * 0.35 * amt, cos(walk * 1.3 + ph) * 0.15 * amt))
	# arms
	for side in ["arm_right", "arm_left"]:
		var a: Node3D = parts.get(side, null)
		if a == null:
			continue
		var sgn := 1.0 if side.ends_with("left") else -1.0
		var swing := sin(walk) * 0.8 * amt * sgn
		var idle := sin(t * 0.067) * 0.05 * sgn
		var attack := 0.0
		if m.hurt_time > 0 or m.ai.shoot_cd > 30:
			attack = 0.0
		_rot(a, Vector3(swing + attack, 0, idle))
	# head look
	var head: Node3D = parts.get("head", null)
	if head != null and arch != "slime" and arch != "sulfur_cube":
		var eye := m.body.pos + Vector3(0, float(m.def.get("eye", 1.5)), 0)
		var to := m.look_target - eye
		# relative to the drawn body, which can still be turning toward m.facing
		var body_rot: float = m.visual.rotation.y if m.visual != null else m.facing
		var yaw := wrapf(atan2(-to.x, -to.z) - body_rot, -PI, PI)
		var pitch := atan2(to.y, maxf(0.01, Vector2(to.x, to.z).length()))
		var cur_y: float = head.get_meta("look_y", 0.0)
		var cur_p: float = head.get_meta("look_p", 0.0)
		cur_y = lerp_angle(cur_y, clampf(yaw, -1.2, 1.2), 0.15)
		cur_p = lerpf(cur_p, clampf(pitch, -0.7, 0.7), 0.12)
		head.set_meta("look_y", cur_y)
		head.set_meta("look_p", cur_p)
		_rot(head, Vector3(cur_p, cur_y, 0))
	match arch:
		"slime", "sulfur_cube":
			_slime(m, rig, t)
		"ghast":
			_tentacles(m, t, 9)
		"blaze":
			_rods(m, t)
		"boss_dragon":
			_dragon(m, t, walk, amt)
		"boss_wither":
			_wither(m, t)
		"fish":
			_fish(m, t)
		"creeper":
			if m.fuse >= 0:
				var s := 1.0 + (30 - m.fuse) / 30.0 * 0.25 + sin(t * 1.3) * 0.02
				rig.scale = Vector3(s, s * 0.95, s)
			elif rig.scale != Vector3.ONE and not m.baby:
				rig.scale = Vector3.ONE
	_wings(m, t)
	_tentacles(m, t, 8, "tentacle_")
	# tails
	for i in 6:
		var tl: Node3D = parts.get("tail_%d" % (i + 1), null)
		if tl != null:
			_rot(tl, Vector3(sin(t * 0.1 - i * 0.4) * 0.08, sin(t * 0.12 - i * 0.5) * (0.3 if arch != "boss_dragon" else 0.15), 0))
	var tail: Node3D = parts.get("tail", null)
	if tail != null and arch != "fish":
		_rot(tail, Vector3(0, sin(t * 0.2) * 0.3, 0))
	for i in 6:
		var nk: Node3D = parts.get("neck_%d" % (i + 1), null)
		if nk != null:
			_rot(nk, Vector3(sin(t * 0.08 - i * 0.3) * 0.05, sin(t * 0.05 - i * 0.3) * 0.06, 0))
	var jaw: Node3D = parts.get("jaw", null)
	if jaw != null:
		_rot(jaw, Vector3(absf(sin(t * 0.05)) * 0.3, 0, 0))
	for i in 2:
		var ear: Node3D = parts.get("ear_%d" % (i + 1), null)
		if ear != null:
			_rot(ear, Vector3(0, 0, sin(t * 0.2 + i) * 0.1))
	# shulker lid opening
	var lid: Node3D = parts.get("lid", null)
	if lid != null:
		var open: float = 0.25 if m.data.get("peek", false) else 0.0
		var bp2: Vector3 = lid.get_meta("base_pos", lid.position)
		lid.position = lid.position.lerp(bp2 + Vector3(0, open, 0), 0.1)


static func _slime(m: Mob, rig: Node3D, t: float) -> void:
	var sq := 1.0 + sin(t * 0.25) * 0.06
	if not m.on_ground:
		sq = 1.12
	var base := 1.0
	if m.data.has("size"):
		base = 0.5 * float(m.data["size"])
	rig.scale = Vector3(base * (2.0 - sq), base * sq, base * (2.0 - sq))


static func _tentacles(m: Mob, t: float, count: int, prefix := "tentacle_") -> void:
	for i in count:
		var tc: Node3D = m.parts.get(prefix + "%d" % (i + 1), null)
		if tc != null:
			_rot(tc, Vector3(sin(t * 0.06 + i * 0.7) * 0.25, 0, cos(t * 0.05 + i * 1.1) * 0.25))


static func _rods(m: Mob, t: float) -> void:
	for i in 12:
		var rod: Node3D = m.parts.get("rod_%d" % (i + 1), null)
		if rod == null:
			continue
		var bp: Vector3 = rod.get_meta("base_pos", rod.position)
		var r := Vector2(bp.x, bp.z).length()
		var a0 := atan2(bp.z, bp.x)
		var ring := i / 4
		var a := a0 + t * (0.05 if ring % 2 == 0 else -0.05)
		rod.position = Vector3(cos(a) * r, bp.y + sin(t * 0.1 + i) * 0.03, sin(a) * r)


static func _dragon(m: Mob, t: float, walk: float, amt: float) -> void:
	var flap := sin(t * 0.22)
	for i in 2:
		var side := "left" if i == 0 else "right"
		var sgn := 1.0 if i == 0 else -1.0
		var w1: Node3D = m.parts.get("wing_%s_1" % side, null)
		var w2: Node3D = m.parts.get("wing_%s_2" % side, null)
		if w1 != null:
			_rot(w1, Vector3(0, 0, sgn * flap * 0.55))
		if w2 != null:
			_rot(w2, Vector3(0, 0, sgn * sin(t * 0.22 - 0.6) * 0.35))


static func _wither(m: Mob, t: float) -> void:
	var names := ["head_centre", "head_left", "head_right"]
	for i in 3:
		var h: Node3D = m.parts.get(names[i], null)
		if h != null:
			_rot(h, Vector3(sin(t * 0.08 + i) * 0.12, sin(t * 0.05 + i * 2.0) * 0.25, 0))
	for i in 3:
		var rib: Node3D = m.parts.get("rib_%d" % (i + 1), null)
		if rib != null:
			_rot(rib, Vector3(sin(t * 0.1 - i * 0.6) * 0.08, 0, 0))


static func _wings(m: Mob, t: float) -> void:
	var fast: bool = m.flying or not m.on_ground
	for side in ["wing_left", "wing_right", "wing_left_1", "wing_right_1", "wing_left_2", "wing_right_2"]:
		var w: Node3D = m.parts.get(side, null)
		if w == null or m.def.get("arch", "") == "boss_dragon":
			continue
		var sgn := 1.0 if side.contains("left") else -1.0
		var speed := 0.9 if fast else 0.12
		var amp := 0.8 if fast else 0.08
		_rot(w, Vector3(0, 0, sgn * sin(t * speed) * amp))
	for side in ["fin_left", "fin_right"]:
		var f: Node3D = m.parts.get(side, null)
		if f != null:
			_rot(f, Vector3(0, 0, sin(t * 0.3 + (1.0 if side.contains("left") else 0.0)) * 0.4))


static func _fish(m: Mob, t: float) -> void:
	var tail: Node3D = m.parts.get("tail", null)
	if tail != null:
		_rot(tail, Vector3(0, sin(t * 0.35) * 0.5, 0))
