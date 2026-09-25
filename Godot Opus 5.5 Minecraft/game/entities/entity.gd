class_name Entity
extends Node3D
## Base class for every non-player entity: 20 TPS AABB voxel physics, health, damage, fire,
## interpolation for rendering and free-form data. Subclasses add visuals and AI.

var world: World = null
var session = null
var body := VoxelBody.new()
var prev_pos := Vector3.ZERO
var type_name := "entity"
var mob := ""
var health := 10.0
var max_health := 10.0
var dead := false
var age := 0
var fire_ticks := 0
var hurt_time := 0
var invuln := 0
var on_ground := false
var gravity := 0.08
var drag := 0.98
var data: Dictionary = {}
var visual: Node3D = null
var in_water := false
var in_lava := false
var no_physics := false
var pickup_delay := 0
var owner_player = null
var target = null
var facing := 0.0            # yaw for mobs
var prev_facing := 0.0
var flash := 0.0


func setup(p_world: World, pos: Vector3) -> void:
	world = p_world
	session = p_world.session if p_world != null else null
	body.pos = pos
	prev_pos = pos
	global_position = pos
	visual = Node3D.new()
	visual.name = "Visual"
	add_child(visual)


func entity_aabb() -> AABB:
	return body.aabb()


func is_mob() -> bool:
	return false


func eye_pos() -> Vector3:
	return body.pos + Vector3(0, body.height * 0.85, 0)


func feet() -> Vector3:
	return body.pos


# ------------------------------------------------------------------------------------------------
func tick() -> void:
	age += 1
	if invuln > 0:
		invuln -= 1
	if hurt_time > 0:
		hurt_time -= 1
	if fire_ticks > 0:
		fire_ticks -= 1
		if age % 20 == 0:
			hurt(1.0, "fire")
			if world != null and BlockDB.fluid[world.get_id(floori(body.pos.x), floori(body.pos.y), floori(body.pos.z))] == 1:
				fire_ticks = 0
	prev_pos = body.pos
	prev_facing = facing
	physics()
	if dead:
		queue_free()


func physics() -> void:
	if world == null or no_physics:
		return
	var env := body.sample_env(world, body.height * 0.6)
	in_water = env.water
	in_lava = env.lava
	body.vel.y -= gravity
	if in_water:
		body.vel.y *= 0.8
		body.vel.y -= 0.02
	if in_lava:
		body.vel.y *= 0.5
	var slip := 0.6
	body.vel.x *= slip
	body.vel.z *= slip
	body.vel.y *= drag
	if body.vel.length_squared() > 100.0:
		body.vel = body.vel.normalized() * 10.0
	body.move(world, body.vel)
	on_ground = body.on_ground
	if on_ground and body.vel.y < 0.0:
		body.vel.y = 0.0
	if in_lava:
		hurt(4.0, "lava")


## Render interpolation (called every frame) from the state at the start of the last tick
## (prev_pos / prev_facing) to the state at its end. Only the visual changes here.
func frame(alpha: float) -> void:
	if world == null:
		return
	global_position = prev_pos.lerp(body.pos, alpha)
	if visual != null:
		visual.rotation.y = lerp_angle(prev_facing, facing, alpha)
	if flash > 0.0:
		flash = maxf(0.0, flash - get_process_delta_time() * 3.0)
	if hurt_time > 0:
		flash = 1.0


## Damage. Returns the amount actually dealt.
func hurt(amount: float, cause: String, attacker = null, dir := Vector2.ZERO, knockback := 0.0) -> float:
	if dead or amount <= 0.0:
		return 0.0
	if invuln > 0 and cause != "void":
		return 0.0
	health -= amount
	hurt_time = 10
	invuln = 10
	flash = 1.0
	if knockback > 0.0:
		body.vel.x += dir.x * knockback
		body.vel.z += dir.y * knockback
		body.vel.y = maxf(body.vel.y, knockback * 0.4)
	if health <= 0.0:
		die(cause, attacker)
	return amount


func set_on_fire(ticks: int) -> void:
	fire_ticks = maxi(fire_ticks, ticks)


func die(cause: String, killer = null) -> void:
	if dead:
		return
	dead = true
	on_death(cause, killer)


func on_death(_cause: String, _killer) -> void:
	queue_free()


func knockback(dir: Vector2, power: float) -> void:
	body.vel.x += dir.x * power
	body.vel.z += dir.y * power
	body.vel.y = maxf(body.vel.y, 0.35)


func can_be_pushed() -> bool:
	return true


func to_dict() -> Dictionary:
	return {"type": type_name, "mob": mob, "pos": body.pos, "data": data}


func from_dict(_d: Dictionary) -> void:
	pass
