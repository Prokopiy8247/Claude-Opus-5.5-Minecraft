class_name Mob
extends Entity
## A living entity driven by MobDB stats and an AI archetype (MobAI). Renders a cuboid rig with
## procedural animation, handles look/swim/fly modes, burning, breeding, taming and riding.

var def: Dictionary = {}
var rig: Node3D = null
var parts: Dictionary = {}
var ai: MobAI = null
var home := Vector3.ZERO
var path := PackedVector3Array()
var path_i := 0
var repath := 0
var attack_cd := 0
var wander_cd := 0
var walk_phase := 0.0
var move_speed := 0.0
var last_move_dir := Vector3.ZERO
var look_target := Vector3.ZERO
var head_yaw := 0.0
var head_pitch := 0.0
var jump_brain := 0
var body_node: Node3D = null
var baby := false
var tamed := false
var sitting := false
var sheared := false
var wool_color := "white"
var variant := 0
var rider = null
var rider_input: Dictionary = {}
var pet_foe = null               # tamed wolves: the mob they fight for their owner
var carried = 0                # block value carried (enderman, sulfur cube)
var absorbed := 0
var fuse := -1
var charge := 0
var anger := 0
var special_timer := 0
var p_owned = null
var boss_bar_active := false
var invuln_phase := 0
var flying := false
var no_gravity_ticks := 0
var body_yaw := 0.0              # drawn body heading: turns toward `facing` over a few ticks
var prev_body_yaw := 0.0


func setup_mob(w: World, mob_name: String, pos: Vector3, extra := {}) -> void:
	def = MobDB.get_def(mob_name)
	mob = mob_name
	type_name = "mob"
	setup(w, pos)
	home = pos
	health = float(def.get("hp", 10))
	max_health = health
	var wp := float(def.get("width", 0.6))
	var hp2 := float(def.get("height", 1.8))
	body.width = maxf(wp, 0.2)
	body.height = maxf(hp2, 0.2)
	if def.get("fly", false):
		gravity = 0.0
		no_gravity_ticks = 1
	flying = bool(def.get("fly", false))
	if extra.has("baby"):
		baby = true
		body.width *= 0.5
		body.height *= 0.5
		health = maxf(1.0, health * 0.5)
		max_health = health
	if extra.has("variant"):
		variant = int(extra["variant"])
	if mob == "sheep":
		wool_color = ["white", "black", "gray", "light_gray", "brown", "pink", "lime", "yellow", "cyan", "purple", "blue",
			"green", "red", "orange", "magenta", "light_blue"][absi(hash(pos)) % 16]
	if extra.has("color"):
		wool_color = String(extra["color"])
	rig = MobRenderer.build(mob, String(def.get("model", "")))
	if baby and rig != null:
		rig.scale = Vector3(0.55, 0.55, 0.55)
	if rig != null:
		visual.add_child(rig)
		parts = MobRenderer.parts_of(rig)
		body_node = parts.get("body", parts.get("torso", null))
	if def.get("boss", false) and session != null:
		session.show_boss_bar(mob, MobDB.display(mob), health, max_health)
		boss_bar_active = true
	ai = MobAI.new(self)
	look_target = pos + Vector3(0, 1, 0)
	facing = randf() * TAU
	prev_facing = facing
	body_yaw = facing
	prev_body_yaw = facing
	if bool(def.get("despawn", true)):
		data["despawn"] = true


func is_mob() -> bool:
	return true


func entity_aabb() -> AABB:
	return body.aabb()


func eye_pos() -> Vector3:
	return body.pos + Vector3(0, float(def.get("eye", 1.5)), 0)


func is_undead() -> bool:
	return mob in ["zombie", "husk", "drowned", "zombie_villager", "skeleton", "stray", "bogged", "parched", "wither",
		"wither_skeleton", "zombie_horse", "skeleton_horse", "zombie_nautilus", "camel_husk", "phantom"]


func is_arthropod() -> bool:
	return mob in ["spider", "cave_spider", "silverfish", "endermite", "bee"]


func attack_damage() -> float:
	return float(def.get("dmg", 2.0))


func hostile() -> bool:
	return bool(def.get("hostile", false))


func can_be_pushed() -> bool:
	return not (mob == "warden" or mobile_boss())


func mobile_boss() -> bool:
	return mob == "ender_dragon" or mob == "wither"


func add_effect(effect: String, ticks: int, amp := 0) -> void:
	ai.effects[effect] = {"amp": amp, "ticks": ticks}


func effect_amp(effect: String) -> int:
	var e: Dictionary = ai.effects.get(effect, {})
	return int(e.get("amp", -1))


func speed_mult() -> float:
	var m := 1.0
	if effect_amp("speed") >= 0:
		m *= 1.0 + 0.2 * (effect_amp("speed") + 1)
	if effect_amp("slowness") >= 0:
		m *= maxf(0.2, 1.0 - 0.15 * (effect_amp("slowness") + 1))
	return m


# ------------------------------------------------------------------------------------------------
func tick() -> void:
	if world == null or dead:
		return
	# start-of-tick state for render interpolation (AI, steering and physics below move and turn)
	prev_pos = body.pos
	prev_facing = facing
	prev_body_yaw = body_yaw
	age += 1
	if invuln > 0:
		invuln -= 1
	if hurt_time > 0:
		hurt_time -= 1
	if fire_ticks > 0:
		fire_ticks -= 1
		if age % 20 == 0 and not bool(def.get("fireproof", false)):
			hurt(1.0, "fire")
	if age % 20 == 7:
		_sun_burn()
	var steered := false
	if rider != null:
		if is_instance_valid(rider) and rider.riding == self:
			steered = Riding.steer_mob(self)
		else:
			rider = null
	if not steered and Breeding.tick(self):
		steered = true
	if not steered and ItemExtras.leash_tick(self):
		steered = true
	if not steered and ItemExtras.pet_tick(self):
		steered = true
	if not steered:
		ai.tick()
	_physics()
	# the body swings round to the new heading like the original's body rotation instead of
	# snapping; a ridden mount keeps its rider's heading
	if rider != null:
		body_yaw = facing
	else:
		body_yaw = wrapf(body_yaw + angle_difference(body_yaw, facing) * 0.35, -PI, PI)
	if move_speed > 0.02 and body.on_ground:
		walk_phase += move_speed * 4.0
	if carried != 0 and body_node != null:
		pass
	if boss_bar_active and session != null and age % 5 == 0:
		session.update_boss_bar(mob, health, max_health)
	# despawning
	if bool(def.get("despawn", true)) and session != null and session.player != null:
		var pd := body.pos.distance_to(session.player.body.pos)
		if pd > 44.0 and age > 600 and randf() < 0.01:
			queue_free()
		if pd > 128.0:
			queue_free()
	if mob == "wither" and health < max_health * 0.5 and invuln_phase < 2:
		invuln_phase = 2
		data["armor"] = 4.0


func _physics() -> void:
	var env := body.sample_env(world, body.height * 0.6)
	in_water = env.water
	in_lava = env.lava
	if bool(def.get("lava_walk", false)) and in_lava:
		body.vel.y = maxf(body.vel.y, 0.1)
	var fly := bool(def.get("fly", false)) or flying
	if fly:
		body.vel *= 0.91
		body.move(world, body.vel)
		if body.collided_v:
			body.vel.y = 0.0
		on_ground = body.on_ground
		return
	if in_water and not bool(def.get("water", false)) and mob != "drowned" and mob != "guardian":
		body.vel.y *= 0.8
		body.vel.y += 0.04
		body.vel *= 0.9
	elif in_water:
		body.vel.y *= 0.85
		body.vel.y += 0.005
		if ai.wants_up and randf() < 0.3:
			body.vel.y = 0.12
	body.vel.y -= gravity
	var slip := 0.546
	body.vel.x *= slip
	body.vel.z *= slip
	body.vel.y *= 0.98
	if body.vel.y < -1.5:
		body.vel.y = -1.5
	body.move(world, body.vel)
	on_ground = body.on_ground
	if on_ground and body.vel.y < 0.0:
		body.vel.y = 0.0
	# fall damage (not for fliers, swimmers or mobs landing in water)
	if on_ground:
		if body.fall_distance > 3.0 and not in_water and not bool(def.get("fly", false)) and mob != "cat" and mob != "chicken":
			hurt(ceilf(body.fall_distance - 3.0), "fall")
		body.fall_distance = 0.0
	elif body.vel.y < 0.0:
		body.fall_distance = maxf(0.0, body.fall_distance)
	if bool(def.get("climb", false)) and body.collided_h and not body.on_ground:
		body.vel.y = 0.12
	if in_lava and not bool(def.get("fireproof", false)):
		hurt(4.0, "lava")
	if env.web:
		body.vel *= 0.25
	if not on_ground and in_water:
		body.fall_distance = 0.0


func frame(alpha: float) -> void:
	if world == null:
		return
	global_position = prev_pos.lerp(body.pos, alpha)
	if visual != null:
		visual.rotation.y = lerp_angle(prev_body_yaw, body_yaw, alpha)
	MobAnimator.animate(self, alpha)
	if flash > 0.0:
		flash = maxf(0.0, flash - get_process_delta_time() * 3.5)
	if hurt_time > 0:
		flash = 1.0
	_update_flash()


var _last_flash := 0.0


func _update_flash() -> void:
	if rig == null:
		return
	var amount := flash * 0.55
	var col := Color(1, 0.35, 0.35) if hurt_time > 0 else Color(1, 1, 1)
	if amount <= 0.0 and _last_flash <= 0.0:
		return
	_last_flash = amount
	for mi in _meshes(rig):
		(mi as GeometryInstance3D).set_instance_shader_parameter("ent_flash", Vector4(col.r, col.g, col.b, amount))


func _meshes(n: Node) -> Array:
	var out := []
	for c in n.get_children():
		if c is MeshInstance3D:
			out.append(c)
		out.append_array(_meshes(c))
	return out


## Lighting for all parts (called by the entity manager when the mob's chunk light changes).
func update_light() -> void:
	if world == null or rig == null:
		return
	var l := world.get_light(floori(body.pos.x), floori(body.pos.y + 0.5), floori(body.pos.z))
	var v := Vector2(float(l.x), float(l.y))
	for mi in _meshes(rig):
		(mi as GeometryInstance3D).set_instance_shader_parameter("ent_light", v)


func hurt(amount: float, cause: String, attacker = null, dir := Vector2.ZERO, knockback := 0.0) -> float:
	if dead or amount <= 0.0:
		return 0.0
	if invuln > 0 and cause != "void":
		return 0.0
	if def.get("armor", 0.0) > 0.0 and cause != "void" and cause != "magic":
		amount = maxf(0.0, amount - float(def["armor"]))
	if amount <= 0.0:
		return 0.0
	health -= amount
	hurt_time = 10
	invuln = 10
	flash = 1.0
	Sfx.play_mob(mob, "hurt", body.pos, 0.7)
	if knockback > 0.0:
		body.vel.x += dir.x * knockback * (0.1 if mob == "ravager" else 1.0)
		body.vel.z += dir.y * knockback * (0.1 if mob == "ravager" else 1.0)
		body.vel.y = maxf(body.vel.y, knockback * 0.4)
	ai.on_hurt(attacker)
	if health <= 0.0:
		die(cause, attacker)
	return amount


func die(cause: String, killer = null) -> void:
	if dead:
		return
	dead = true
	if rider != null and is_instance_valid(rider):
		Riding.dismount(rider)
	if bool(data.get("saddled", false)) and session != null:
		session.entities.spawn_item(world, body.pos + Vector3(0, 0.4, 0), ItemStack.of("saddle", 1))
	if session != null:
		session.particles.poof(body.pos + Vector3(0, body.height * 0.5, 0), 12)
		var by_player: bool = killer != null and killer == session.player
		var looting := 0
		if by_player:
			var st: ItemStack = session.player.inventory.selected_stack()
			if st != null:
				looting = st.enchant_level("looting")
		var drops := LootDB.mob_drops(mob, _rng(), looting, by_player, fire_ticks > 0,
			{"size": data.get("size", 1), "sheared": sheared, "color": wool_color})
		for st2 in drops:
			session.entities.spawn_item(world, body.pos + Vector3(0, 0.4, 0), st2)
		if mob == "ender_dragon":
			DragonFight.on_dragon_defeated(session)
		elif float(def.get("xp", 0)) > 0 and by_player:
			session.entities.spawn_xp(world, body.pos, int(def["xp"]))
		if boss_bar_active:
			session.remove_boss_bar(mob)
		# slime family splits
		var split: String = String(def.get("split", ""))
		if split != "" and int(data.get("size", 1)) > 1:
			var n: int = int(data["size"]) / 2
			for i in maxi(2, n):
				session.entities.spawn_mob(world, split, body.pos + Vector3(randf_range(-0.6, 0.6), 0.2, randf_range(-0.6, 0.6)),
					{"size": maxi(1, n)})
		Sfx.play_mob(mob, "death", body.pos, 0.8)
	queue_free()


func _rng() -> RandomNumberGenerator:
	var r := RandomNumberGenerator.new()
	r.randomize()
	return r


## Undead (day_burn) catch fire under open daylight sky unless wet or wearing nothing to hide under.
func _sun_burn() -> void:
	if not bool(def.get("day_burn", false)) or session == null or world == null or not world.gen.has_sky:
		return
	if in_water or fire_ticks > 0 or session.sun_angle_factor() < 0.6 or session.weather != 0:
		return
	var x := floori(body.pos.x)
	var z := floori(body.pos.z)
	var y := floori(body.pos.y + body.height * 0.9)
	if world.get_light(x, y, z).x >= 15 and randf() < 0.4:
		set_on_fire(160)


func interact(player, held: ItemStack) -> bool:
	var hn := held.item_name() if held != null else ""
	if hn == "name_tag":
		return ItemExtras.name_tag(player, self, held)
	if hn == "lead":
		return ItemExtras.lead(player, self, held)
	# feeding / breeding
	if hn != "" and Breeding.feed(self, player, held):
		return true
	# villagers trade (not babies, not while sneaking)
	if (mob == "villager" or mob == "wandering_trader") and not baby and not player.sneaking and session != null and session.ui != null:
		session.ui.open_trade(self)
		return true
	# tamed pets sit / stand up on an empty-hand click
	if tamed and ItemExtras.PETS.has(mob) and held == null and not player.sneaking:
		sitting = not sitting
		return true
	# saddles and mounting
	if hn == "saddle" and Riding.can_saddle(self) and (tamed or not Riding.HORSES.has(mob)):
		data["saddled"] = true
		data.erase("despawn")
		Sfx.play_at("equip_leather", body.pos, 0.7)
		if not player.is_creative():
			held.count -= 1
			if held.count <= 0:
				player.inventory.set_stack(player.inventory.selected, null)
		return true
	if not player.sneaking and rider == null and Riding.mob_mountable(self) and (held == null or Riding.STICKS.values().has(hn)):
		return Riding.mount(player, self)
	if Riding.HORSES.has(mob):
		return false
	if not tamed and String(def.get("tame_item", "")) != "" and held != null and held.item_name() == String(def["tame_item"]):
		var chance := 0.33
		if randf() < chance:
			tamed = true
			sitting = true
			data.erase("despawn")
			session.particles.hearts(body.pos + Vector3(0, body.height, 0), 7)
			player.stats.add_xp(1)
			if not player.is_creative():
				held.count -= 1
				if held.count <= 0:
					player.inventory.set_stack(player.inventory.selected, null)
			return true
		session.particles.smoke(body.pos + Vector3(0, body.height, 0), 4)
		return false
	if sheared or mob != "sheep" or held == null:
		return false
	if held.item_name() == "shears" and not sheared:
		sheared = true
		for i in 1 + absi(hash(body.pos)) % 3:
			session.entities.spawn_item(world, body.pos + Vector3(0, 0.6, 0), ItemStack.of(wool_color + "_wool", 1))
		if player.interact != null:
			player.interact.damage_item(player.inventory.selected, 1)
		return true
	return false


func on_death(_c: String, _k) -> void:
	queue_free()
