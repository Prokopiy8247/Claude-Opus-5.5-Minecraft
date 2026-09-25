class_name ItemExtras
extends RefCounted
## Smaller item mechanics: fishing, name tags, leads, compass/clock read-outs, chorus fruit,
## suspicious stew, mending, thorns, trident loyalty/channeling, and tamed pets (wolves, cats,
## parrots) that follow their owner, sit on command and defend them.

const PETS := ["wolf", "cat", "parrot"]
const STEW_EFFECTS := [["night_vision", 100], ["jump_boost", 120], ["weakness", 180], ["blindness", 160],
	["poison", 240], ["saturation", 7], ["fire_resistance", 80], ["regeneration", 160], ["wither", 160]]


# ------------------------------------------------------------------------------------------------
# Fishing
## Right-click with a rod: cast a bobber, or reel in (catching whatever bit).
static func use_rod(player, st: ItemStack, slot: int) -> bool:
	var session = player.session
	var bob = _bobber_of(player)
	if bob != null:
		var caught: bool = bob.data.get("bite", 0) > 0
		if caught:
			var loot := fish_loot(st)
			var drop = session.entities.spawn_item(player.world, bob.body.pos + Vector3(0, 0.3, 0), loot)
			if drop != null:
				var to: Vector3 = player.eye_position() - bob.body.pos
				drop.body.vel = to * 0.1 + Vector3(0, sqrt(to.length()) * 0.08, 0)
			session.entities.spawn_xp(player.world, player.body.pos, randi_range(1, 6))
			Sfx.play_at("splash", bob.body.pos, 0.6)
		bob.queue_free()
		player.interact.damage_item(slot, 1)
		return true
	var b := SimpleEntities.Projectile.new()
	session.entities.add_entity(b)
	b.setup_proj(player.world, "fishing_bobber", player.eye_position() + player.look_dir() * 0.4,
		player.look_dir() * 0.9 + Vector3(0, 0.15, 0), player, {"lure": st.enchant_level("lure")})
	Sfx.play_at("throw", player.body.pos, 0.5)
	return true


static func _bobber_of(player):
	for e in player.session.entities.all():
		if e is SimpleEntities.Projectile and (e as SimpleEntities.Projectile).kind == "fishing_bobber" \
				and (e as SimpleEntities.Projectile).shooter == player and not (e as Node).is_queued_for_deletion():
			return e
	return null


## Fish 85 %, junk 10 %, treasure 5 % (luck of the sea shifts towards treasure).
static func fish_loot(rod: ItemStack) -> ItemStack:
	var luck := rod.enchant_level("luck_of_the_sea") if rod != null else 0
	var r := randf()
	var treasure := 0.05 + 0.02 * luck
	var junk := 0.10 - 0.025 * luck
	if r < treasure:
		var t: Array = ["name_tag", "saddle", "nautilus_shell", "bow", "fishing_rod", "enchanted_book"]
		return ItemStack.of(String(t[randi() % t.size()]), 1)
	if r < treasure + junk:
		var j: Array = ["leather_boots", "stick", "string", "bowl", "rotten_flesh", "lily_pad", "bone", "ink_sac", "tripwire_hook"]
		return ItemStack.of(String(j[randi() % j.size()]), 1)
	var f := randf()
	if f < 0.6:
		return ItemStack.of("cod", 1)
	if f < 0.85:
		return ItemStack.of("salmon", 1)
	if f < 0.87:
		return ItemStack.of("tropical_fish", 1)
	return ItemStack.of("pufferfish", 1)


## Bobber tick: floats on water, waits 5-30 s (lure: shorter), then a fish bites for 1 s.
static func bobber_tick(b) -> void:
	var pl = b.shooter
	if pl == null or not is_instance_valid(pl) or pl.dead or b.body.pos.distance_to(pl.body.pos) > 32.0:
		b.queue_free()
		return
	var held: ItemStack = pl.inventory.selected_stack()
	if held == null or held.item_name() != "fishing_rod":
		b.queue_free()
		return
	var w: World = b.world
	var id := w.get_id(floori(b.body.pos.x), floori(b.body.pos.y), floori(b.body.pos.z))
	var in_water := BlockDB.name_of(id) == "water"
	if in_water:
		b.body.vel.y = minf(b.body.vel.y + 0.04, 0.05)
		b.body.vel.x *= 0.85
		b.body.vel.z *= 0.85
		var wait := int(b.data.get("wait", 0))
		if wait <= 0 and int(b.data.get("bite", 0)) <= 0:
			var lure := int(b.data.get("lure", 0))
			b.data["wait"] = randi_range(100, 600) - lure * 100
		elif wait > 0:
			b.data["wait"] = wait - 1
			if wait == 1:
				b.data["bite"] = 20
				b.body.vel.y = -0.2
				Sfx.play_at("splash", b.body.pos, 0.5)
				if b.session != null:
					b.session.particles.splash(b.body.pos + Vector3(0, 0.2, 0), 10)
		var bite := int(b.data.get("bite", 0))
		if bite > 0:
			b.data["bite"] = bite - 1
	else:
		b.body.vel.y -= 0.03
		b.body.vel *= 0.98
	b.body.move(w, b.body.vel)
	if b.body.on_ground and not in_water:
		b.body.vel = Vector3.ZERO


# ------------------------------------------------------------------------------------------------
# Name tags, leads
static func name_tag(player, m: Mob, st: ItemStack) -> bool:
	var nm := String(st.data.get("name", ""))
	if nm == "":
		player.session.action_bar("Rename the name tag in an anvil first")
		return false
	m.data["custom_name"] = nm
	m.data.erase("despawn")
	show_name(m)
	if not player.is_creative():
		st.count -= 1
		if st.count <= 0:
			player.inventory.set_stack(player.inventory.selected, null)
	return true


static func show_name(m: Mob) -> void:
	var nm := String(m.data.get("custom_name", ""))
	if nm == "" or m.visual == null:
		return
	var lbl: Label3D = m.visual.get_node_or_null("NameTag")
	if lbl == null:
		lbl = Label3D.new()
		lbl.name = "NameTag"
		lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		lbl.no_depth_test = true
		lbl.fixed_size = true
		lbl.pixel_size = 0.0022
		lbl.font = PixelUI.get_font()
		lbl.font_size = 16
		lbl.outline_size = 4
		lbl.modulate = Color(1, 1, 1)
		m.visual.add_child(lbl)
	lbl.text = nm
	lbl.position = Vector3(0, m.body.height + 0.45, 0)


static func lead(player, m: Mob, st: ItemStack) -> bool:
	if not bool(m.def.get("leadable", true)) or m.def.get("hostile", false):
		return false
	if bool(m.data.get("leashed", false)):
		m.data.erase("leashed")
		player.session.entities.spawn_item(player.world, m.body.pos + Vector3(0, 0.5, 0), ItemStack.of("lead", 1))
		return true
	m.data["leashed"] = true
	m.data.erase("despawn")
	if not player.is_creative():
		st.count -= 1
		if st.count <= 0:
			player.inventory.set_stack(player.inventory.selected, null)
	return true


## A leashed mob is pulled towards the player; the lead snaps beyond 10 blocks.
static func leash_tick(m: Mob) -> bool:
	if not bool(m.data.get("leashed", false)) or m.session == null or m.session.player == null:
		return false
	var pl = m.session.player
	var d: float = m.body.pos.distance_to(pl.body.pos)
	if d > 10.0 or pl.world != m.world:
		m.data.erase("leashed")
		m.session.entities.spawn_item(m.world, m.body.pos + Vector3(0, 0.5, 0), ItemStack.of("lead", 1))
		return false
	if d > 4.0:
		m.ai._walk_to(pl.body.pos, 1.4 + (d - 4.0) * 0.2)
		return true
	return false


# ------------------------------------------------------------------------------------------------
# Compass / clock read-outs (action bar while held)
static func held_readout(player) -> String:
	var st: ItemStack = player.inventory.selected_stack()
	if st == null:
		return ""
	var nm := st.item_name()
	if nm == "clock":
		if player.world.dim != 0:
			return "The clock spins wildly"
		var t: int = (player.session.day_time + 6000) % 24000
		var h := t / 1000
		var mi := int(float(t % 1000) * 0.06)
		return "Time: %02d:%02d  (%s)" % [h, mi, "night" if player.session.is_night() else "day"]
	if nm == "compass" or nm == "recovery_compass":
		var target: Vector3 = player.spawn_point if player.spawn_point != Vector3.ZERO else Vector3.ZERO
		if nm == "recovery_compass":
			if player.session.last_death_pos == Vector3.INF:
				return "The needle spins: no death point yet"
			target = player.session.last_death_pos
		if player.world.dim != 0 and nm == "compass":
			return "The needle spins wildly"
		var to: Vector3 = target - player.body.pos
		var dist := Vector2(to.x, to.z).length()
		var ang: float = atan2(-to.x, -to.z) - float(player.yaw)
		var dirs := ["ahead", "ahead-left", "left", "behind-left", "behind", "behind-right", "right", "ahead-right"]
		var idx := int(round(fposmod(ang, TAU) / (TAU / 8.0))) % 8
		return "%s: %d blocks %s" % ["Spawn" if nm == "compass" else "Last death", int(dist), dirs[idx]]
	return ""


# ------------------------------------------------------------------------------------------------
# Food extras
static func chorus_teleport(player) -> void:
	var w: World = player.world
	for i in 16:
		var q: Vector3 = player.body.pos + Vector3(randf_range(-8, 8), randf_range(-8, 8), randf_range(-8, 8))
		var x := floori(q.x)
		var z := floori(q.z)
		for dy in range(0, 16):
			var y := floori(q.y) - dy
			if BlockDB.solid[w.get_id(x, y - 1, z)] == 1 and BlockDB.solid[w.get_id(x, y, z)] == 0 and BlockDB.solid[w.get_id(x, y + 1, z)] == 0:
				player.session.particles.portal(player.body.pos + Vector3(0, 1, 0))
				player.teleport(Vector3(x + 0.5, y, z + 0.5))
				Sfx.play_at("teleport", player.body.pos, 0.6)
				return


static func stew_effect(player, st: ItemStack) -> void:
	var e: Array = STEW_EFFECTS[absi(int(st.data.get("stew", randi()))) % STEW_EFFECTS.size()]
	player.add_effect(String(e[0]), int(e[1]), 0)


# ------------------------------------------------------------------------------------------------
# Enchantments
## XP picked up first repairs mending items (2 durability per point).
static func mending(player, xp: int) -> int:
	var slots := [player.inventory.selected, Inventory.OFFHAND, Inventory.ARMOR, Inventory.ARMOR + 1, Inventory.ARMOR + 2, Inventory.ARMOR + 3]
	for s in slots:
		if xp <= 0:
			break
		var st: ItemStack = player.inventory.stack_at(s)
		if st == null or st.damage <= 0 or st.enchant_level("mending") <= 0:
			continue
		var fix := mini(st.damage, xp * 2)
		st.damage -= fix
		xp -= int(ceil(fix / 2.0))
		player.inventory.changed.emit()
	return maxi(0, xp)


## Thorns on worn armour hurts melee attackers.
static func thorns(player, attacker) -> void:
	if attacker == null or not is_instance_valid(attacker) or not attacker.has_method("hurt"):
		return
	for i in 4:
		var st: ItemStack = player.inventory.stack_at(Inventory.ARMOR + i)
		if st == null:
			continue
		var lvl := st.enchant_level("thorns")
		if lvl > 0 and randf() < 0.15 * lvl:
			attacker.hurt(float(randi_range(1, 4)), "thorns", player)
			st.damage += 2
			return


# ------------------------------------------------------------------------------------------------
# Tamed pets
## Owner-following AI for tamed wolves/cats/parrots. Returns true when it handled the tick.
static func pet_tick(m: Mob) -> bool:
	if not m.tamed or not PETS.has(m.mob) or m.session == null:
		return false
	var pl = m.session.player
	if pl == null or pl.dead or pl.world != m.world:
		return false
	if m.sitting:
		m.move_speed = 0.0
		m.body.vel.x *= 0.5
		m.body.vel.z *= 0.5
		return true
	# wolves fight whatever hurt their owner or what the owner attacks
	var foe = m.pet_foe
	if m.mob == "wolf" and foe != null and is_instance_valid(foe) and not foe.dead and foe != m and foe.world == m.world:
		var fd: float = m.body.pos.distance_to(foe.body.pos)
		if fd < 20.0:
			if fd > 1.8:
				m.ai._steer_to(foe.body.pos, 1.3)
			elif m.attack_cd <= 0:
				m.attack_cd = 20
				foe.hurt(4.0, "mob", m, Vector2(foe.body.pos.x - m.body.pos.x, foe.body.pos.z - m.body.pos.z).normalized(), 0.3)
			if m.attack_cd > 0:
				m.attack_cd -= 1
			return true
	m.pet_foe = null
	var d: float = m.body.pos.distance_to(pl.body.pos)
	if d > 12.0:
		m.ai.teleport_toward(pl.body.pos)
		return true
	if d > 3.5:
		m.ai._steer_to(pl.body.pos, 1.3)
		return true
	return false


## The owner was hurt by / attacked `foe`: every tamed wolf nearby joins in.
static func alert_pets(session, foe) -> void:
	if foe == null or not (foe is Mob) or (foe as Mob).tamed:
		return
	for e in session.entities.all():
		if e is Mob and (e as Mob).tamed and (e as Mob).mob == "wolf" and not (e as Mob).sitting:
			(e as Mob).pet_foe = foe
