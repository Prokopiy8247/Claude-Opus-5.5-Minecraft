class_name Riding
extends RefCounted
## Mounts: boats, minecarts and rideable mobs (horses and relatives, camels, saddled pigs and
## striders, happy ghasts). While riding, Player.tick hands its input to control(): the mount
## stores it in `rider_input` and moves on its own tick; the player is carried on the seat.
## Sneak dismounts. The rider never targets its own mount.

const HORSES := ["horse", "donkey", "mule", "skeleton_horse", "zombie_horse"]
const STICKS := {"pig": "carrot_on_a_stick", "strider": "warped_fungus_on_a_stick"}


static func seat_offset(m) -> Vector3:
	if m is SimpleEntities.Vehicle:
		var v: SimpleEntities.Vehicle = m
		return Vector3(0, 0.1 if v.kind.ends_with("boat") else 0.35, 0)
	if m is Mob:
		var mb: Mob = m
		if mb.mob == "happy_ghast":
			return Vector3(0, mb.body.height, 0)
		return Vector3(0, mb.body.height * 0.72, 0)
	return Vector3(0, 0.5, 0)


## True when the mob can carry the player right now (saddle / taming rules).
static func mob_mountable(m: Mob) -> bool:
	if m.baby or m.dead:
		return false
	if HORSES.has(m.mob):
		return true             # untamed horses are tamed by riding them (they may buck the rider off)
	if m.mob in ["pig", "strider", "camel", "happy_ghast"]:
		return bool(m.data.get("saddled", false))
	return bool(m.def.get("rideable", false)) and bool(m.data.get("saddled", false))


static func can_saddle(m: Mob) -> bool:
	return not m.baby and (HORSES.has(m.mob) or m.mob in ["pig", "strider", "camel", "happy_ghast"]) \
		and not bool(m.data.get("saddled", false))


static func mount(player, m) -> bool:
	if player.riding != null or m == null or not is_instance_valid(m) or m.rider != null:
		return false
	player.riding = m
	m.rider = player
	player.flying = false
	player.elytra_flying = false
	player.body.vel = Vector3.ZERO
	player.body.fall_distance = 0.0
	player.teleport(m.body.pos + seat_offset(m))
	Sfx.play_at("click", player.body.pos, 0.3)
	if player.session != null:
		player.session.action_bar("Press Shift to dismount")
	return true


static func dismount(player) -> void:
	var m = player.riding
	player.riding = null
	if m == null or not is_instance_valid(m):
		return
	m.rider = null
	m.rider_input = {}
	# step off beside the mount onto free ground; on top of it when boxed in
	var base: Vector3 = m.body.pos
	var out := base + Vector3(0, seat_offset(m).y + 0.6, 0)
	var found := false
	for d in [Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(0, 0, 1), Vector3(0, 0, -1)]:
		if found:
			break
		var q: Vector3 = base + d * (m.body.width * 0.5 + 0.8)
		var fy := floori(q.y)
		for dy in [0, 1, -1]:
			if _free(player.world, floori(q.x), fy + dy, floori(q.z)):
				out = Vector3(floori(q.x) + 0.5, float(fy + dy), floori(q.z) + 0.5)
				found = true
				break
	player.teleport(out)
	player.body.fall_distance = 0.0


static func _free(w: World, x: int, y: int, z: int) -> bool:
	return BlockDB.solid[w.get_id(x, y, z)] == 0 and BlockDB.solid[w.get_id(x, y + 1, z)] == 0 \
		and BlockDB.solid[w.get_id(x, y - 1, z)] == 1


## Player.tick while riding: hand the input to the mount and sit on it.
static func control(player, ri: Dictionary) -> void:
	var m = player.riding
	if m == null or not is_instance_valid(m) or m.dead or m.world != player.world or m.is_queued_for_deletion():
		player.riding = null
		return
	if ri.sneak:
		dismount(player)
		return
	m.rider_input = {"input": ri.input, "jump": ri.jump, "yaw": player.yaw, "pitch": player.pitch}
	player.body.pos = m.body.pos + seat_offset(m)
	player.body.vel = m.body.vel
	player.body.fall_distance = 0.0
	player.sprinting = false


## A ridden mob's movement for this tick (instead of its AI). Returns false when the mob ignores
## the rider (no steering item for pigs/striders, unsaddled horse wandering...).
static func steer_mob(m: Mob) -> bool:
	var ri: Dictionary = m.rider_input
	m.rider_input = {}
	var rider = m.rider
	if rider == null or ri.is_empty():
		return false
	var inp: Vector2 = ri.get("input", Vector2.ZERO)
	var ryaw: float = float(ri.get("yaw", m.facing))
	# horses: taming by riding - hearts or a buck-off every second until tamed
	if HORSES.has(m.mob) and not m.tamed:
		if m.age % 20 == 0:
			if randf() < 0.18:
				m.tamed = true
				if m.session != null:
					m.session.particles.hearts(m.body.pos + Vector3(0, m.body.height, 0), 7)
			elif randf() < 0.25:
				if m.session != null:
					m.session.particles.smoke(m.body.pos + Vector3(0, m.body.height, 0), 6)
				dismount(rider)
				return false
		m.move_speed = 0.0
		return true
	if HORSES.has(m.mob) and not bool(m.data.get("saddled", false)):
		return false           # tamed but unsaddled: can sit on it, cannot steer
	if STICKS.has(m.mob):
		var held: ItemStack = rider.inventory.selected_stack()
		if held == null or held.item_name() != String(STICKS[m.mob]):
			m.move_speed = 0.0
			return true
		inp = Vector2(0, 1)
	m.facing = ryaw
	var fwd := Vector3(-sin(ryaw), 0, -cos(ryaw))
	var right := Vector3(cos(ryaw), 0, -sin(ryaw))
	var base_speed := float(m.def.get("speed", 0.08))
	var sp := base_speed * (2.6 if HORSES.has(m.mob) else (1.8 if m.mob == "camel" else 1.4))
	var want := (fwd * inp.y + right * inp.x * 0.4) * sp * 2.0 / 0.546
	if m.mob == "happy_ghast":
		var pitch: float = float(ri.get("pitch", 0.0))
		var look := Vector3(-sin(ryaw) * cos(pitch), sin(pitch), -cos(ryaw) * cos(pitch))
		m.body.vel = m.body.vel.lerp(look * inp.y * 0.18 + Vector3(0, 0.12 if bool(ri.get("jump", false)) else 0.0, 0), 0.2)
		m.move_speed = m.body.vel.length()
		return true
	var k := 0.6 if m.body.on_ground else 0.1
	m.body.vel.x = lerpf(m.body.vel.x, want.x, k)
	m.body.vel.z = lerpf(m.body.vel.z, want.z, k)
	if bool(ri.get("jump", false)) and m.body.on_ground and (HORSES.has(m.mob) or m.mob == "camel"):
		m.body.vel.y = 0.75 if HORSES.has(m.mob) else 0.55
	elif m.body.on_ground and m.body.collided_h and inp.length() > 0.1:
		m.body.vel.y = 0.42     # step up single blocks like a player would
	m.move_speed = Vector2(m.body.vel.x, m.body.vel.z).length() * 0.546
	return true
