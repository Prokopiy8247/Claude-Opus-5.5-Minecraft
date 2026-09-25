class_name PlayerStats
extends RefCounted
## Health, hunger/saturation/exhaustion, air, fire, XP and damage handling following Java rules.

signal damaged(amount: float, cause: String)
signal changed

var p = null                 # Player
var health := 20.0
var max_health := 20.0
var absorption := 0.0
var food := 20
var saturation := 5.0
var exhaustion := 0.0
var air := 300
var fire_ticks := 0
var xp_level := 0
var xp_progress := 0.0       # 0..1
var xp_total := 0
var hurt_time := 0
var invuln := 0
var last_damage := 0.0
var regen_timer := 0
var starve_timer := 0
var last_cause := ""
var last_attacker = null
var score := 0


func _init(player) -> void:
	p = player


func reset() -> void:
	health = max_health
	absorption = 0.0
	food = 20
	saturation = 5.0
	exhaustion = 0.0
	air = 300
	fire_ticks = 0
	hurt_time = 0
	invuln = 0
	changed.emit()


func difficulty() -> int:
	return p.session.difficulty if p != null and p.session != null else 2


func hunger_enabled() -> bool:
	return p.gamemode == Player.SURVIVAL and difficulty() > 0


func vulnerable() -> bool:
	return p.gamemode == Player.SURVIVAL


func add_exhaustion(v: float) -> void:
	if not hunger_enabled():
		return
	exhaustion = minf(exhaustion + v, 40.0)


func tick(sprinting: bool, moved: float, jumped: bool) -> void:
	if p.dead:
		return
	if hurt_time > 0:
		hurt_time -= 1
	if invuln > 0:
		invuln -= 1
	var world: World = p.world
	# exhaustion from movement
	if p.in_water and p.eyes_in_water:
		add_exhaustion(0.01 * moved)
	elif sprinting:
		add_exhaustion(0.1 * moved)
	if jumped:
		add_exhaustion(0.2 if sprinting else 0.05)
	# hunger
	if exhaustion >= 4.0:
		exhaustion -= 4.0
		if saturation > 0.0:
			saturation = maxf(0.0, saturation - 1.0)
		elif difficulty() > 0:
			food = maxi(0, food - 1)
	# natural regeneration
	var diff := difficulty()
	if diff == 0 and health < max_health and p.session.tick % 20 == 0:
		heal(1.0)
	if health < max_health and food >= 18 and p.session.gamerule("naturalRegeneration", true):
		regen_timer += 1
		if saturation > 0.0 and food >= 20:
			if regen_timer >= 10:
				var amt := minf(saturation, 6.0)
				heal(amt / 6.0)
				add_exhaustion(amt)
				regen_timer = 0
		elif regen_timer >= 80:
			heal(1.0)
			add_exhaustion(6.0)
			regen_timer = 0
	else:
		regen_timer = 0
	if food <= 0 and vulnerable():
		starve_timer += 1
		if starve_timer >= 80:
			starve_timer = 0
			if diff == 3 or (diff == 2 and health > 1.0) or (diff == 1 and health > 10.0):
				damage(1.0, "starve", null, true)
	else:
		starve_timer = 0
	# air
	var resp := 0
	if p.eyes_in_water and not p.effects.has("water_breathing") and not p.effects.has("conduit_power") and vulnerable():
		var helmet: ItemStack = p.inventory.stack_at(Inventory.ARMOR + 3)
		if helmet != null:
			resp = helmet.enchant_level("respiration")
			if helmet.item_name() == "turtle_helmet":
				resp = 99
		if resp == 0 or randf() < 1.0 / (resp + 1):
			air -= 1
		if air <= -20:
			air = 0
			damage(2.0, "drown", null, true)
	else:
		air = mini(300, air + 4)
	# fire & lava
	var fire_res: bool = p.effects.has("fire_resistance")
	if p.in_lava:
		if not fire_res:
			if p.session.tick % 10 == 0:
				damage(4.0, "lava", null, true)
			fire_ticks = maxi(fire_ticks, 300)
	elif p.in_water:
		fire_ticks = 0
	if fire_ticks > 0:
		fire_ticks -= 1
		if fire_ticks % 20 == 0 and not fire_res and vulnerable():
			damage(1.0, "fire", null, true)
	# contact damage blocks (cactus, magma, fire, sweet berry bush, powder snow freezing)
	_contact_damage(world)
	changed.emit()


func _contact_damage(world: World) -> void:
	if world == null or not vulnerable():
		return
	var box: AABB = p.body.aabb().grow(0.01)
	var feet := world.get_block(floori(p.body.pos.x), floori(p.body.pos.y - 0.05), floori(p.body.pos.z))
	if feet != 0:
		var fd: BlockDef = BlockDB.defs[feet & 0xFFF]
		if fd.props.get("magma", false) and not p.sneaking and not p.effects.has("fire_resistance"):
			if p.session.tick % 10 == 0:
				damage(1.0, "hot_floor", null, true)
	for x in range(floori(box.position.x), floori(box.end.x) + 1):
		for y in range(floori(box.position.y), floori(box.end.y) + 1):
			for z in range(floori(box.position.z), floori(box.end.z) + 1):
				var v := world.get_block(x, y, z)
				if v == 0:
					continue
				var d: BlockDef = BlockDB.defs[v & 0xFFF]
				if d.damage > 0.0 and d.fluid == 0 and not d.props.get("magma", false):
					if d.props.get("fire", false):
						if not p.effects.has("fire_resistance"):
							fire_ticks = maxi(fire_ticks, 160)
							if p.session.tick % 10 == 0:
								damage(d.damage, "in_fire", null, true)
					elif d.props.get("campfire", false):
						if ((v >> 12) & 4) == 0 and not p.effects.has("fire_resistance") and p.session.tick % 10 == 0:
							damage(d.damage, "campfire", null, true)
					elif p.session.tick % 10 == 0:
						damage(d.damage, "cactus" if d.name == "cactus" else "block", null, true)
				elif d.props.get("thorns", false) and p.session.tick % 10 == 0 and ((v >> 12) & 15) > 0:
					damage(1.0, "sweet_berry_bush", null, true)


func on_land(fall: float, landed_on: int) -> void:
	var id := landed_on & 0xFFF
	var bd: BlockDef = BlockDB.defs[id] if id > 0 else null
	if bd != null and bd.props.get("bouncy", false) and not p.sneaking:
		p.body.vel.y = absf(p.body.vel.y) * 0.8 + fall * 0.04
		return
	var mul := 1.0
	if bd != null and (bd.name.ends_with("_bed") or bd.name == "hay_block"):
		mul = 0.5 if bd.name.ends_with("_bed") else 0.2
	if bd != null and (bd.name == "slime_block" or bd.name == "powder_snow" or bd.name == "honey_block"):
		mul = 0.2 if bd.name == "honey_block" else 0.0
	var jump_boost := 0
	if p.effects.has("jump_boost"):
		jump_boost = int(p.effects["jump_boost"].amp) + 1
	var dmg := ceilf((fall - 3.0 - jump_boost) * mul)
	if dmg > 0.0 and not p.effects.has("slow_falling"):
		var boots: ItemStack = p.inventory.stack_at(Inventory.ARMOR)
		var ff := 0
		if boots != null:
			ff = boots.enchant_level("feather_falling")
		dmg = maxf(0.0, dmg * (1.0 - minf(0.8, ff * 0.12)))
		damage(dmg, "fall", null, true)
		Sfx.play_at("hurt_fall" if dmg < 5 else "hurt_fall_big", p.body.pos)


func armor_points() -> int:
	var a := 0
	for s in 4:
		var st: ItemStack = p.inventory.stack_at(Inventory.ARMOR + s)
		if st != null:
			a += st.item().armor
	return a


func armor_toughness() -> float:
	var t := 0.0
	for s in 4:
		var st: ItemStack = p.inventory.stack_at(Inventory.ARMOR + s)
		if st != null:
			t += st.item().toughness
	return t


## Applies damage with armour, enchantments and invulnerability frames. Returns the damage dealt.
func damage(amount: float, cause: String, attacker = null, bypass_armor := false) -> float:
	if p.dead or amount <= 0.0:
		return 0.0
	if not vulnerable() and cause != "void" and cause != "kill":
		return 0.0
	if cause == "kill":
		health = 0.0
		_check_death(cause)
		return amount
	if invuln > 10 and amount <= last_damage:
		return 0.0
	var dealt := amount
	if invuln > 10:
		dealt = amount - last_damage
	last_damage = amount
	if p.interact != null and p.interact.blocking and attacker != null and cause in ["mob", "player", "arrow", "projectile", "explosion"]:
		var to_att: Vector3 = (attacker.global_position - p.body.pos)
		to_att.y = 0.0
		if to_att.length() > 0.01 and to_att.normalized().dot(Vector3(-sin(p.yaw), 0, -cos(p.yaw))) > 0.0:
			p.interact.on_shield_block(amount, attacker)
			return 0.0
	if not bypass_armor or cause in ["explosion", "fall"]:
		if cause != "fall" and cause != "drown" and cause != "starve" and cause != "void" and cause != "magic":
			var a := float(armor_points())
			var t := armor_toughness()
			var red := clampf(maxf(a / 5.0, a - dealt / (2.0 + t / 4.0)), 0.0, 20.0) / 25.0
			dealt *= 1.0 - red
			p.interact.damage_armor(amount)
	# protection enchantments
	var epf := 0
	for s in 4:
		var st: ItemStack = p.inventory.stack_at(Inventory.ARMOR + s)
		if st == null:
			continue
		epf += st.enchant_level("protection")
		if cause in ["fire", "lava", "in_fire", "hot_floor"]:
			epf += st.enchant_level("fire_protection") * 2
		if cause == "explosion":
			epf += st.enchant_level("blast_protection") * 2
		if cause in ["arrow", "projectile"]:
			epf += st.enchant_level("projectile_protection") * 2
	if epf > 0:
		dealt *= 1.0 - minf(20, epf) * 0.04
	if p.effects.has("resistance") and cause != "void":
		dealt *= maxf(0.0, 1.0 - 0.2 * (int(p.effects["resistance"].amp) + 1))
	if attacker != null and is_instance_valid(attacker) and attacker is Mob:
		if cause == "mob":
			ItemExtras.thorns(p, attacker)
		if p.session != null:
			ItemExtras.alert_pets(p.session, attacker)
	# knockback from mob hits and projectiles
	if attacker != null and is_instance_valid(attacker) and attacker is Node3D and cause in ["mob", "arrow", "projectile", "player"]:
		var kd: Vector3 = p.body.pos - (attacker as Node3D).global_position
		kd.y = 0.0
		if kd.length() > 0.01:
			kd = kd.normalized()
			p.body.vel.x += kd.x * 0.4
			p.body.vel.z += kd.z * 0.4
			p.body.vel.y = maxf(p.body.vel.y, 0.36)
	if absorption > 0.0:
		var ab := minf(absorption, dealt)
		absorption -= ab
		dealt -= ab
	health -= dealt
	hurt_time = 10
	invuln = 20
	last_cause = cause
	last_attacker = attacker
	add_exhaustion(0.1)
	damaged.emit(dealt, cause)
	if dealt > 0.0:
		Sfx.play_at("player_hurt", p.body.pos)
	_check_death(cause)
	changed.emit()
	return dealt


func _check_death(cause: String) -> void:
	if health > 0.0:
		return
	# totem of undying
	for slot in [p.inventory.selected, Inventory.OFFHAND]:
		var st: ItemStack = p.inventory.stack_at(slot)
		if st != null and st.item_name() == "totem_of_undying" and cause != "void" and cause != "kill":
			p.inventory.set_stack(slot, null)
			health = 1.0
			p.clear_effects()
			p.add_effect("regeneration", 900, 1)
			p.add_effect("absorption", 100, 1)
			p.add_effect("fire_resistance", 800, 0)
			if p.session != null:
				p.session.on_totem()
			return
	health = 0.0
	p.die(cause)


func heal(v: float) -> void:
	health = minf(max_health, health + v)
	changed.emit()


func eat(hunger: int, sat: float) -> void:
	food = mini(20, food + hunger)
	saturation = minf(float(food), saturation + sat)
	changed.emit()


# ---- experience --------------------------------------------------------------------------------
static func xp_for_level(level: int) -> int:
	if level >= 30:
		return 9 * level - 158
	if level >= 15:
		return 5 * level - 38
	return 2 * level + 7


func add_xp(amount: int) -> void:
	score += amount
	xp_total += amount
	var need := xp_for_level(xp_level)
	xp_progress += float(amount) / need
	while xp_progress >= 1.0:
		xp_progress = (xp_progress - 1.0) * need
		xp_level += 1
		need = xp_for_level(xp_level)
		xp_progress /= need
		if xp_level % 5 == 0:
			Sfx.play_ui("levelup")
	changed.emit()


func add_levels(n: int) -> void:
	xp_level = maxi(0, xp_level + n)
	if n < 0:
		xp_progress = 0.0
	changed.emit()


func to_dict() -> Dictionary:
	return {"health": health, "food": food, "sat": saturation, "exh": exhaustion, "air": air, "fire": fire_ticks,
		"lvl": xp_level, "prog": xp_progress, "total": xp_total, "abs": absorption}


func from_dict(d: Dictionary) -> void:
	health = float(d.get("health", 20.0))
	food = int(d.get("food", 20))
	saturation = float(d.get("sat", 5.0))
	exhaustion = float(d.get("exh", 0.0))
	air = int(d.get("air", 300))
	fire_ticks = int(d.get("fire", 0))
	xp_level = int(d.get("lvl", 0))
	xp_progress = float(d.get("prog", 0.0))
	xp_total = int(d.get("total", 0))
	absorption = float(d.get("abs", 0.0))
