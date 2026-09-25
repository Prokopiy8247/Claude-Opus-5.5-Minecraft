class_name ElytraFlight
extends RefCounted
## Elytra gliding physics per 20 TPS tick (lift from pitch, dive acceleration, pull-up, drag),
## with wall-collision damage like the original's "kinetic energy" rule.

static func step(world, body: VoxelBody, p) -> Dictionary:
	var env := body.sample_env(world, 1.62)
	var look: Vector3 = p.look_dir()
	var pitch: float = -p.pitch          # Minecraft pitch: positive = looking down
	var hlen := Vector2(look.x, look.z).length()
	var speed_h := Vector2(body.vel.x, body.vel.z).length()
	var cos_p := cos(pitch)
	var sq := cos_p * cos_p * minf(1.0, look.length() / 0.4)
	var grav := 0.01 if p.effects.has("slow_falling") else 0.08
	body.vel.y += -grav + sq * 0.06
	if body.vel.y < 0.0 and hlen > 0.0:
		var d := body.vel.y * -0.1 * sq
		body.vel.y += d
		body.vel.x += look.x * d / hlen
		body.vel.z += look.z * d / hlen
	if pitch < 0.0 and hlen > 0.0:
		var d2 := speed_h * -sin(pitch) * 0.04
		body.vel.y += d2 * 3.2
		body.vel.x -= look.x * d2 / hlen
		body.vel.z -= look.z * d2 / hlen
	if hlen > 0.0:
		body.vel.x += (look.x / hlen * speed_h - body.vel.x) * 0.1
		body.vel.z += (look.z / hlen * speed_h - body.vel.z) * 0.1
	body.vel.x *= 0.99
	body.vel.y *= 0.98
	body.vel.z *= 0.99
	var before := Vector2(body.vel.x, body.vel.z).length()
	body.move(world, body.vel)
	if body.collided_h:
		var after := Vector2(body.vel.x, body.vel.z).length()
		var dmg := (before - after) * 10.0 - 3.0
		if dmg > 0.0:
			p.stats.damage(dmg, "fly_into_wall", null, true)
	if body.on_ground or env.water or env.lava:
		p.elytra_flying = false
	body.fall_distance = maxf(0.0, body.fall_distance) if body.vel.y < -0.5 else 0.0
	return env


## Starts gliding when jumping in mid-air with an elytra equipped.
static func try_start(p) -> bool:
	var chest: ItemStack = p.inventory.stack_at(Inventory.ARMOR + 2)
	if chest == null or not chest.item().props.get("elytra", false):
		return false
	if chest.damage >= chest.item().durability - 1:
		return false
	if p.body.on_ground or p.flying or p.in_water:
		return false
	p.elytra_flying = true
	return true
