class_name PlayerMotion
extends RefCounted
## Per-tick Minecraft Java movement rules (walking, sprinting, sneaking, jumping, creative flight,
## swimming, lava, ladders, cobwebs) applied to a VoxelBody.

const GRAVITY := 0.08
const DRAG_Y := 0.98
const JUMP := 0.42
const WALK_SPEED := 0.1
const SPRINT_MUL := 1.3
const FLY_SPEED := 0.05
const AIR_ACCEL := 0.02


static func slipperiness(world, body: VoxelBody) -> float:
	var v: int = world.get_block(floori(body.pos.x), floori(body.pos.y - 0.5), floori(body.pos.z))
	if v == 0:
		return 0.6
	return BlockDB.defs[v & 0xFFF].slip


static func speed_factor(world, body: VoxelBody) -> float:
	var v: int = world.get_block(floori(body.pos.x), floori(body.pos.y - 0.2), floori(body.pos.z))
	if v == 0:
		v = world.get_block(floori(body.pos.x), floori(body.pos.y + 0.01), floori(body.pos.z))
	if v == 0:
		return 1.0
	return BlockDB.defs[v & 0xFFF].speed


static func jump_factor(world, body: VoxelBody) -> float:
	var v: int = world.get_block(floori(body.pos.x), floori(body.pos.y - 0.2), floori(body.pos.z))
	if v == 0:
		return 1.0
	return BlockDB.defs[v & 0xFFF].jump


## Adds acceleration in the facing frame. input.x = strafe (right +), input.y = forward (+).
static func move_relative(body: VoxelBody, accel: float, input: Vector2, yaw: float) -> void:
	var l := input.length_squared()
	if l < 1e-7:
		return
	var inp := input
	if l > 1.0:
		inp = input.normalized()
	inp *= accel
	var s := sin(yaw)
	var c := cos(yaw)
	# Godot: yaw 0 looks toward -Z; forward = (-sin, -cos), right = (cos, -sin)
	body.vel.x += -s * inp.y + c * inp.x
	body.vel.z += -c * inp.y - s * inp.x


## One 20 TPS step. state keys: input(Vector2), jump, sneak, sprint, flying, yaw, speed_mul, jump_mul, levitation.
## Returns environment info from the step.
static func step(world, body: VoxelBody, st: Dictionary) -> Dictionary:
	var env := body.sample_env(world, 1.62)
	var input: Vector2 = st.input
	var yaw: float = st.yaw
	var sneak: bool = st.sneak
	var sprint: bool = st.sprint
	var speed_mul: float = st.get("speed_mul", 1.0)
	if sneak and not st.flying:
		input *= 0.3
	if st.flying:
		var fs := FLY_SPEED * (2.0 if sprint else 1.0) * float(st.get("fly_mul", 1.0))
		if st.jump:
			body.vel.y += FLY_SPEED * 3.0
		if sneak:
			body.vel.y -= FLY_SPEED * 3.0
		move_relative(body, fs, input, yaw)
		body.move(world, body.vel, false)
		body.vel.y *= 0.6
		body.vel.x *= 0.91
		body.vel.z *= 0.91
		body.fall_distance = 0.0
	elif env.water and not st.get("spectator", false):
		var accel := 0.02
		if sprint and st.get("swimming", false):
			accel = 0.04
		move_relative(body, accel * speed_mul, input, yaw)
		if st.jump:
			body.vel.y += 0.04
		if sneak:
			body.vel.y -= 0.04
		body.move(world, body.vel, false)
		body.vel *= 0.8
		body.vel.y -= 0.02 * (0.25 if st.get("swimming", false) else 1.0)
		if body.collided_h and body.is_free(world, body.aabb_at(body.pos + Vector3(body.vel.x, 0.6 - body.pos.y + floorf(body.pos.y) + 0.0, body.vel.z))):
			body.vel.y = 0.3
		body.fall_distance = 0.0
	elif env.lava:
		move_relative(body, 0.02, input, yaw)
		if st.jump:
			body.vel.y += 0.04
		body.move(world, body.vel, false)
		body.vel *= 0.5
		body.vel.y -= 0.02
		if body.collided_h:
			body.vel.y = 0.3
		body.fall_distance *= 0.5
	else:
		var slip := slipperiness(world, body) if body.on_ground else 1.0
		var f := slip * 0.91
		var accel: float
		if body.on_ground:
			var spd := WALK_SPEED * (SPRINT_MUL if sprint else 1.0) * speed_mul
			accel = spd * (0.16277136 / (f * f * f))
		else:
			accel = AIR_ACCEL * (1.3 if sprint else 1.0)
		if st.jump and body.on_ground:
			body.vel.y = JUMP * jump_factor(world, body) * float(st.get("jump_mul", 1.0))
			if sprint:
				body.vel.x += -sin(yaw) * 0.2
				body.vel.z += -cos(yaw) * 0.2
		move_relative(body, accel, input, yaw)
		if env.climb:
			body.vel.x = clampf(body.vel.x, -0.15, 0.15)
			body.vel.z = clampf(body.vel.z, -0.15, 0.15)
			body.vel.y = maxf(body.vel.y, -0.15)
			if sneak and body.vel.y < 0.0:
				body.vel.y = 0.0
			body.fall_distance = 0.0
		if env.web:
			body.vel *= Vector3(0.25, 0.05, 0.25)
		var sf := speed_factor(world, body)
		var mv := body.vel * Vector3(sf, 1.0, sf)
		body.move(world, mv, sneak)
		if env.climb and (body.collided_h or st.jump):
			body.vel.y = 0.2
		var lev: int = st.get("levitation", 0)
		if lev > 0:
			body.vel.y += (0.05 * lev - body.vel.y) * 0.2
		elif st.get("slow_fall", false) and body.vel.y < 0.0:
			body.vel.y -= 0.01
		else:
			body.vel.y -= GRAVITY
		body.vel.y *= DRAG_Y
		body.vel.x *= f
		body.vel.z *= f
		if env.web:
			body.vel = Vector3.ZERO
	return env
