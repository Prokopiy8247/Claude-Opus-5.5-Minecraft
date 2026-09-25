class_name ItemUse
extends RefCounted
## Right-click item behaviour: eating, drinking, buckets, flint and steel, bone meal, hoe/shovel
## path making, shears, spawn eggs (items), bows, crossbows, tridents, shields, ender pearls,
## end crystals, potions, boats/minecarts, music discs, leads and the spyglass.

static var _rng := RandomNumberGenerator.new()


## Called once when the use action starts. Returns true when the action was consumed.
static func use(player, inter, st: ItemStack, slot: int, target: Dictionary, _pressed: bool) -> bool:
	if st == null:
		return false
	var it := st.item()
	var world: World = player.world
	var session = player.session
	var n := it.name
	match it.use:
		"hoe":
			return _hoe(player, world, target, slot, st)
		"shovel":
			return _shovel(player, world, target, slot, st)
		"axe":
			return _axe(player, world, target, slot, st)
		"spawn_egg":
			n = "spawn_egg"
		"boat":
			return _boat(player, world, target, slot, st)
		"equip":
			return _equip_armor(player, st, slot)
		"drink":
			inter.start_using()
			return true
	match n:
		"bow":
			if player.is_creative() or _has_ammo(player, "arrow"):
				inter.start_using()
				inter.using_bow = true
				return true
			return false
		"crossbow":
			if _has_ammo(player, "arrow") or _has_ammo(player, "spectral_arrow"):
				inter.start_using()
				inter.using_bow = true
				return true
			return false
		"trident":
			if st.enchant_level("riptide") > 0 and (player.in_water or session.is_raining_at(world, floori(player.body.pos.x), floori(player.body.pos.y), floori(player.body.pos.z))):
				var dir: Vector3 = player.look_dir()
				player.body.vel = dir * 1.6
				Sfx.play_at("riptide", player.body.pos, 1.0)
				return true
			if _pickup_trident(player, st):
				return true
			return false
		"shield":
			inter.blocking = true
			inter.start_using()
			return true
		"ender_pearl":
			session.entities.spawn_projectile(world, "ender_pearl", player.eye_position(), player.look_dir() * 1.5 * 20.0, player, {})
			spend(player, slot, st)
			return true
		"ender_eye":
			if world.dim != 0:
				return false
			var goal: Vector3 = session.locate_structure("stronghold", player.body.pos)
			if goal == Vector3.INF:
				session.action_bar("The eye finds no stronghold")
				return false
			var eye := SpecialEntities.EyeOfEnder.new()
			session.entities.add_entity(eye)
			eye.setup_eye(world, player.eye_position() + player.look_dir() * 0.6, goal)
			Sfx.play_at("eye_place", player.body.pos, 0.6)
			spend(player, slot, st)
			return true
		"snowball", "egg", "splash_potion", "lingering_potion", "experience_bottle", "wind_charge":
			session.entities.spawn_projectile(world, String(it.props.get("projectile", n)), player.eye_position(),
				player.look_dir() * 1.2 * 20.0, player, {"potion": st.data.get("potion", "")})
			Sfx.play_at("throw", player.body.pos, 0.6)
			spend(player, slot, st)
			return true
		"fire_charge":
			session.entities.spawn_projectile(world, "small_fireball", player.eye_position(), player.look_dir() * 1.2 * 20.0, player, {})
			spend(player, slot, st)
			return true
		"firework_rocket":
			if player.elytra_flying:
				var dir: Vector3 = player.look_dir()
				player.body.vel += dir * 0.9
				player.body.vel.y = maxf(player.body.vel.y, 0.2)
				session.particles.burst(player.body.pos + Vector3(0, 1, 0), Color(1, 0.9, 0.6), 20, 4.0, "spark", 0.2)
				Sfx.play_at("firework", player.body.pos, 0.8)
				spend(player, slot, st)
				return true
			session.entities.spawn_projectile(world, "firework_rocket", player.eye_position(), player.look_dir() * 0.6 * 20.0, player, {})
			spend(player, slot, st)
			return true
		"flint_and_steel":
			if target.is_empty():
				return false
			var p: Vector3i = target.pos + Vox.DIR_VEC[target.face]
			if Fire.ignite(world, p):
				Sfx.play_at("flint_and_steel", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.8)
				if not player.is_creative():
					inter.damage_item(slot, 1)
				return true
			return false
		"bucket":
			return _bucket(player, world, target)
		"water_bucket", "lava_bucket", "powder_snow_bucket", "cod_bucket", "salmon_bucket", "tropical_fish_bucket", "pufferfish_bucket", "axolotl_bucket", "tadpole_bucket":
			return _place_fluid_bucket(player, world, slot, st, target)
		"glass_bottle":
			return _fill_bottle(player, world, slot, st, target)
		"bone_meal":
			if target.is_empty():
				return false
			var bp: Vector3i = target.pos
			var grown := apply_bone_meal(world, bp, player)
			if not grown:
				grown = apply_bone_meal(world, bp + Vox.DIR_VEC[target.face], player)
			if grown:
				if not player.is_creative():
					spend(player, slot, st)
				Sfx.play_at("bone_meal", Vector3(bp) + Vector3(0.5, 0.5, 0.5), 0.7)
			return grown
		"hoe":
			return _hoe(player, world, target, slot, st)
		"shovel":
			return _shovel(player, world, target, slot, st)
		"shears":
			return false   # block interaction handles leaves / wool / cobwebs
		"spawn_egg":
			var mob := String(it.props.get("mob", ""))
			if mob == "" or player.gamemode != Player.CREATIVE and player.gamemode != Player.SURVIVAL:
				return false
			var dir: Vector3 = player.look_dir()
			var spawn_pos: Vector3 = player.eye_position() + dir * 1.5
			if not target.is_empty():
				spawn_pos = Vector3(target.pos + Vox.DIR_VEC[target.face]) + Vector3(0.5, 0.0, 0.5)
			session.entities.spawn_mob(world, mob, spawn_pos, {"persistent": true})
			session.particles.poof(spawn_pos + Vector3(0, 0.5, 0), 12)
			Sfx.play_at("pop", spawn_pos, 0.7, 0.7)
			spend(player, slot, st)
			return true
		"end_crystal":
			if target.is_empty():
				return false
			var cp: Vector3i = target.pos + Vox.DIR_VEC[target.face]
			var names := ["obsidian", "bedrock"]
			var support := BlockDB.name_of(world.get_blockv(cp + Vector3i(0, -1, 0)))
			if not (support in names) or world.get_blockv(cp) != 0:
				return false
			session.spawn_end_crystal(Vector3(cp) + Vector3(0.5, 0.0, 0.5))
			spend(player, slot, st)
			return true
		"music_disc_13", "music_disc_cat", "music_disc_blocks", "music_disc_chirp", "music_disc_far", "music_disc_mall", "music_disc_mellohi", "music_disc_stal", "music_disc_strad", "music_disc_ward", "music_disc_11", "music_disc_wait":
			return false   # jukebox interaction
		"minecart", "chest_minecart", "furnace_minecart", "tnt_minecart", "hopper_minecart":
			# carts go onto the rail that is looked at (or on top of the targeted block)
			if target.is_empty():
				return false
			var tp: Vector3i = target.pos
			var cart_pos := Vector3(tp) + Vector3(0.5, 0.0, 0.5)
			if BlockDB.model[world.get_id(tp.x, tp.y, tp.z)] != BlockDB.M_RAIL:
				var nrm: Vector3i = Vox.DIR_VEC[int(target.face)]
				cart_pos = Vector3(tp + nrm) + Vector3(0.5, 0.0, 0.5)
			session.entities.spawn_vehicle(world, String(it.props.get("vehicle", "minecart")), cart_pos, it.props)
			spend(player, slot, st)
			return true
		"armor_stand", "item_frame", "glow_item_frame", "painting":
			session.decor_item(player, n, target, st, slot)
			return true
		"spyglass":
			inter.start_using()
			return true
		"fishing_rod":
			return ItemExtras.use_rod(player, st, slot)
		"goat_horn":
			Sfx.play_at("goat_horn", player.body.pos, 1.0)
			inter.start_using()
			return true
		"name_tag", "lead", "saddle":
			return false
		"milk_bucket", "honey_bottle", "potion", "ominous_bottle":
			inter.start_using()
			return true
	if it.kind == "food":
		if player.gamemode == Player.CREATIVE or player.stats.food < 20 or bool(it.props.get("always", false)):
			inter.start_using()
			return true
		return false
	if it.kind == "armor" and st != null and _equip_armor(player, st, slot):
		return true
	if it.props.has("plants") and not target.is_empty():
		return _plant_seed(player, world, st, slot, target, String(it.props["plants"]))
	return false


## Called every tick while the use action is held.
static func using_tick(player, inter, ticks: int) -> void:
	var st: ItemStack = player.inventory.selected_stack()
	if inter.blocking:
		return
	if st == null:
		inter._stop_using()
		return
	var it := st.item()
	if it.kind == "food":
		if ticks >= 32:
			eat(player, st, inter, player.inventory.selected)
		elif ticks % 4 == 0:
			Sfx.play_at("eat", player.body.pos, 0.4)
			player.session.particles.item_break(st, player.eye_position() + player.look_dir() * 0.4 + Vector3(0, -0.15, 0))
		return
	match it.name:
		"bow":
			if ticks % 20 == 0:
				Sfx.play_at("bow_draw", player.body.pos, 0.4)
			if ticks > 72000:
				inter._stop_using()
		"crossbow":
			if ticks % 25 == 0:
				Sfx.play_at("crossbow_load", player.body.pos, 0.2)
		"spyglass":
			pass
		"potion", "milk_bucket", "honey_bottle", "ominous_bottle":
			if ticks >= 32:
				inter._stop_using()
				match it.name:
					"milk_bucket":
						player.clear_effects()
						Sfx.play_at("drink", player.body.pos, 0.7)
						returns_bucket(player, player.inventory.selected, st)
					"honey_bottle":
						player.effects.erase("poison")
						player.stats.eat(6, 1.2)
						returns_bucket(player, player.inventory.selected, st, "glass_bottle")
					"ominous_bottle":
						player.add_effect("bad_omen", 12000, 0)
						spend(player, player.inventory.selected, st)
					_:
						drink_potion(player, st, player.inventory.selected)
			elif ticks % 4 == 0:
				Sfx.play_at("drink", player.body.pos, 0.4)


## Called when the use action is released (bow shot, trident throw...).
static func release(player, inter, ticks: int) -> void:
	var st: ItemStack = player.inventory.selected_stack()
	if st == null:
		return
	var it := st.item()
	if it.name == "bow" or it.name == "crossbow":
		var t := clampf(ticks / 20.0, 0.0, 1.0)
		var power := t * t * 3.0
		if power < 0.03:
			return
		var ammo_name := "arrow"
		var ammo_slot := -1
		if not player.is_creative():
			ammo_slot = _find_ammo(player, "arrow")
			if ammo_slot < 0:
				ammo_slot = _find_ammo(player, "spectral_arrow")
			if ammo_slot < 0:
				return
			ammo_name = player.inventory.stack_at(ammo_slot).item_name()
		var st_ammo: ItemStack = player.inventory.stack_at(ammo_slot) if ammo_slot >= 0 else null
		var dmg := 2.0
		var infinity := st.enchant_level("infinity") > 0
		if st_ammo != null:
			dmg = 2.0 * power
			if st.item_name() == "crossbow":
				dmg = 7.0
		if it.name == "crossbow":
			power = 1.15
		var arrow = player.session.entities.spawn_projectile(player.world, ammo_name, player.eye_position(),
			player.look_dir() * power * 20.0, player, {"damage": dmg, "potion": st.data.get("potion", ""), "power": st.enchant_level("power") * 0.5})
		if arrow != null:
			if st.enchant_level("flame") > 0:
				arrow.set_on_fire(200)
			arrow.invuln = 0
			arrow.data["crit"] = t >= 1.0
		Sfx.play_at("bow", player.body.pos, 0.8)
		if not player.is_creative() and not infinity:
			_consume_ammo(player, ammo_slot)
		if not player.is_creative():
			inter.damage_item(player.inventory.selected, 1)
	elif it.name == "trident":
		var power2 := clampf(ticks / 20.0, 0.0, 1.0) * 2.5
		if power2 > 0.1:
			var tr = player.session.entities.spawn_projectile(player.world, "item_trident", player.eye_position(),
				player.look_dir() * power2 * 20.0, player, {"damage": 8.0 * power2, "item": st.to_dict()})
			if tr != null:
				player.inventory.set_stack(player.inventory.selected, null)
				Sfx.play_at("trident_throw", player.body.pos, 1.0)
	inter.using_bow = false
	inter.blocking = false


# ------------------------------------------------------------------------------------------------
static func _has_ammo(player, kind: String) -> bool:
	return player.is_creative() or _find_ammo(player, kind) >= 0


static func _find_ammo(player, kind: String) -> int:
	var id := ItemDB.id(kind)
	if id < 0:
		return -1
	return player.inventory.find_item(id, 36)


static func _consume_ammo(player, slot: int) -> void:
	var st: ItemStack = player.inventory.stack_at(slot)
	if st == null:
		return
	st.count -= 1
	if st.count <= 0:
		player.inventory.set_stack(slot, null)
	player.inventory.changed.emit()


static func _pickup_trident(player, st: ItemStack) -> bool:
	var world: World = player.world
	var hit := world.raycast(player.eye_position(), player.look_dir(), 3.0, true, true)
	if hit.is_empty():
		return false
	var p: Vector3i = hit.pos
	var v := world.get_blockv(p)
	if BlockDB.name_of(v) != "trident_placed":
		return false
	world.set_blockv(p, 0)
	player.inventory.set_stack(player.inventory.selected, st.with_count(1))
	Sfx.play_at("trident_pickup", Vector3(p) + Vector3(0.5, 0.5, 0.5), 1.0)
	return true


static func eat(player, st: ItemStack, inter, slot: int) -> void:
	var it := st.item()
	player.stats.eat(it.food, it.saturation)
	if bool(it.props.get("teleport", false)):
		ItemExtras.chorus_teleport(player)
	if it.name == "suspicious_stew":
		ItemExtras.stew_effect(player, st)
	var eff: Array = it.props.get("effects", [])
	for e in eff:
		var chance := float(e[3]) if e.size() > 3 else 1.0
		if randf() <= chance:
			player.add_effect(String(e[0]), int(e[1]), int(e[2]))
	Sfx.play_at("burp", player.body.pos, 0.7)
	inter._stop_using()
	if it.props.get("returns", "") != "":
		returns_bucket(player, slot, st, String(it.props["returns"]))
	else:
		spend(player, slot, st)


static func drink_potion(player, st: ItemStack, slot: int) -> void:
	var potion := String(st.data.get("potion", "water"))
	for e in EffectDB.potion_effects(potion):
		player.add_effect(String(e[0]), int(e[1]), int(e[2]))
	Sfx.play_at("drink", player.body.pos, 0.8)
	returns_bucket(player, slot, st, "glass_bottle")


static func returns_bucket(player, slot: int, st: ItemStack, item := "bucket") -> void:
	if player.is_creative():
		return
	if st.count > 1:
		st.count -= 1
		var rem = player.inventory.add(ItemStack.of(item, 1), 0, 36)
		if rem != null:
			player.session.drop_item_from_player(rem)
	else:
		player.inventory.set_stack(slot, ItemStack.of(item, 1))
	player.inventory.changed.emit()


static func spend(player, slot: int, st: ItemStack) -> void:
	if player.is_creative():
		return
	st.count -= 1
	if st.count <= 0:
		player.inventory.set_stack(slot, null)
	player.inventory.changed.emit()


static func _equip_armor(player, st: ItemStack, slot: int) -> bool:
	var it := st.item()
	var target_slot: int = Inventory.ARMOR + it.armor_slot
	if it.props.get("elytra", false):
		target_slot = Inventory.ARMOR + 2
	var cur: ItemStack = player.inventory.stack_at(target_slot)
	player.inventory.set_stack(target_slot, st.with_count(1))
	if player.is_creative():
		player.inventory.set_stack(slot, st.with_count(1))
	else:
		player.inventory.set_stack(slot, cur)
	Sfx.play_at("armor_equip", player.body.pos, 0.6)
	return true


static func _bucket(player, world: World, target: Dictionary) -> bool:
	if target.is_empty():
		return false
	var p: Vector3i = target.pos
	var v := world.get_blockv(p)
	var id := v & 0xFFF
	if BlockDB.fluid[id] == 0:
		return false
	if ((v >> 12) & 15) != 0:
		return false
	var kind := BlockDB.fluid[id]
	world.set_blockv(p, 0)
	var item := "water_bucket" if kind == 1 else "lava_bucket"
	if player.is_creative():
		return true
	player.inventory.set_stack(player.inventory.selected, ItemStack.of(item, 1))
	Sfx.play_at("bucket_fill", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.7)
	return true


static func _place_fluid_bucket(player, world: World, slot: int, st: ItemStack, target: Dictionary) -> bool:
	if target.is_empty():
		return false
	var it := st.item()
	var p: Vector3i = target.pos + Vox.DIR_VEC[target.face]
	var kind := 1 if it.name in ["water_bucket", "cod_bucket", "salmon_bucket", "tropical_fish_bucket", "pufferfish_bucket",
		"axolotl_bucket", "tadpole_bucket"] else (2 if it.name == "lava_bucket" else 3)
	if kind == 3:
		if world.get_blockv(p) == 0:
			world.set_blockv(p, BlockDB.id("powder_snow"))
			spend(player, slot, st)
			return true
		return false
	if not Fluids.place_source(world, p, kind):
		return false
	var mob := String(it.props.get("mob", ""))
	if mob != "" and player.session != null:
		player.session.entities.spawn_mob(world, mob, Vector3(p) + Vector3(0.5, 0, 0.5))
	Sfx.play_at("bucket_empty", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.7)
	spend(player, slot, st)
	if not player.is_creative():
		player.inventory.add(ItemStack.of("bucket", 1), 9, 36)
		player.inventory.changed.emit()
	return true


static func _fill_bottle(player, world: World, slot: int, st: ItemStack, target: Dictionary) -> bool:
	if target.is_empty():
		return false
	var p: Vector3i = target.pos
	if BlockDB.fluid[world.get_id(p.x, p.y, p.z)] != 1:
		return false
	if player.is_creative():
		return true
	player.inventory.set_stack(slot, ItemStack.new(ItemDB.id("potion"), 1, 0, {"potion": "water"}))
	player.inventory.changed.emit()
	Sfx.play_at("bottle_fill", player.body.pos, 0.6)
	return true


## Bone meal: grows crops, saplings and grass. Returns true when something grew.
static func apply_bone_meal(world: World, p: Vector3i, player) -> bool:
	var v := world.get_blockv(p)
	if v == 0:
		# grass on dirt
		var below := world.get_blockv(p + Vector3i(0, -1, 0))
		if below == 0:
			return false
		var bid := below & 0xFFF
		var name := BlockDB.name_of(below)
		if (bid == BlockDB.DIRT or name == "grass_block") and world.get_blockv(p) == 0:
			for i in 24:
				var dx := _rng.randi_range(-3, 3)
				var dz := _rng.randi_range(-3, 3)
				var dy := _rng.randi_range(-1, 1)
				var q := p + Vector3i(dx, dy, dz)
				if world.get_blockv(q) != 0:
					continue
				if world.get_id(q.x, q.y - 1, q.z) != BlockDB.GRASS:
					continue
				world.set_blockv(q, BlockDB.id("short_grass"))
			for i in 4:
				var dx2 := _rng.randi_range(-2, 2)
				var dz2 := _rng.randi_range(-2, 2)
				var q2 := p + Vector3i(dx2, 0, dz2)
				if world.get_blockv(q2) == 0 and world.get_id(q2.x, q2.y - 1, q2.z) == BlockDB.GRASS:
					world.set_blockv(q2, BlockDB.id(["dandelion", "poppy", "short_grass", "cornflower"][_rng.randi_range(0, 3)]))
			return true
		return false
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	var meta := (v >> 12) & 15
	if d.tick == "crop" or d.tick == "stem":
		var max_age: int = d.props.get("max_age", 7)
		if meta >= max_age:
			return false
		for i in 2 + _rng.randi_range(0, 2):
			var m := mini(max_age, ((v >> 12) & 15) + 1)
			if m == (v >> 12) & 15:
				break
			world.set_blockv(p, Vox.make(v & 0xFFF, m), 0)
			v = world.get_blockv(p)
		world.session.particles.burst(Vector3(p) + Vector3(0.5, 0.2, 0.5), Color(0.5, 0.9, 0.3), 10, 1.5, "happy", 0.1)
		return true
	match d.tick:
		"sapling":
			BlockBehaviors.grow_tree(world, p.x, p.y, p.z, v)
			world.session.particles.burst(Vector3(p) + Vector3(0.5, 0.5, 0.5), Color(0.5, 0.9, 0.3), 12, 2.0, "happy", 0.1)
			return true
		"grass", "mycelium":
			return false
		"cave_vines", "weeping_vines", "vine":
			for i in 4:
				world.schedule_tick(p.x, p.y, p.z, 1)
			return true
	return false


static func _plant_seed(player, world: World, st: ItemStack, slot: int, target: Dictionary, plant: String) -> bool:
	var p: Vector3i = target.pos + Vox.DIR_VEC[target.face]
	if BlockDB.name_of(world.get_blockv(p)) != "farmland":
		var below := world.get_blockv(p + Vector3i(0, -1, 0))
		if BlockDB.name_of(below) != "farmland" and plant != "sweet_berry_bush" and plant != "cave_vines":
			return false
		p = p + Vector3i(0, -1, 0)
		if world.get_blockv(p) != 0:
			return false
	var bid := BlockDB.id(plant)
	if bid <= 0:
		return false
	world.set_blockv(p, bid)
	spend(player, slot, st)
	Sfx.play_at("plant", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.6)
	return true


static func _hoe(player, world: World, target: Dictionary, slot: int, st: ItemStack) -> bool:
	if target.is_empty():
		return false
	var p: Vector3i = target.pos
	var name := BlockDB.name_of(world.get_blockv(p))
	var above := world.get_blockv(p + Vector3i(0, 1, 0))
	if above != 0:
		return false
	if name in ["grass_block", "dirt", "coarse_dirt", "podzol", "rooted_dirt", "mycelium"]:
		world.set_blockv(p, BlockDB.id("farmland"))
	elif name == "dirt_path" or name == "gravel":
		return false
	elif name in ["moss_block", "pale_moss_block"]:
		world.set_blockv(p, BlockDB.id("dirt_path"))
	else:
		return false
	_use_tool(player, slot, st, 1)
	Sfx.play_block(BlockDB.id("dirt"), "place", Vector3(p) + Vector3(0.5, 1.0, 0.5), 0.8)
	return true


static func _shovel(player, world: World, target: Dictionary, slot: int, st: ItemStack) -> bool:
	if target.is_empty():
		return false
	var p: Vector3i = target.pos
	var name := BlockDB.name_of(world.get_blockv(p))
	if name in ["grass_block", "dirt", "coarse_dirt", "podzol", "mycelium", "rooted_dirt"] and world.get_blockv(p + Vector3i(0, 1, 0)) == 0:
		world.set_blockv(p, BlockDB.id("dirt_path"))
		_use_tool(player, slot, st, 1)
		Sfx.play_block(BlockDB.id("dirt"), "place", Vector3(p) + Vector3(0.5, 1.0, 0.5), 0.8)
		return true
	if name == "campfire" or name == "soul_campfire":
		world.set_blockv(p, 0)
		player.session.entities.spawn_item(world, Vector3(p) + Vector3(0.5, 0.3, 0.5), ItemStack.of("campfire", 1))
		return true
	return false


static func _use_tool(player, slot: int, st: ItemStack, amount := 1) -> void:
	if player.is_creative() or player.interact == null:
		return
	if st.item().is_damageable():
		player.interact.damage_item(slot, amount)


static func _axe(player, world: World, target: Dictionary, slot: int, st: ItemStack) -> bool:
	if target.is_empty():
		return false
	var p: Vector3i = target.pos
	var v := world.get_blockv(p)
	var name := BlockDB.name_of(v)
	var meta := (v >> 12) & 15
	var out := ""
	if (name.ends_with("_log") or name.ends_with("_wood") or name.ends_with("_stem") or name.ends_with("_hyphae") or name == "bamboo_block") and not name.begins_with("stripped_"):
		out = "stripped_" + name
	elif name.begins_with("waxed_"):
		out = name.substr(6)
		Sfx.play_at("wax_off", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.7)
	elif name.begins_with("exposed_") or name.begins_with("weathered_") or name.begins_with("oxidized_"):
		var rest := name.substr(name.find("_") + 1)
		if name.begins_with("exposed_"):
			out = "copper_block" if rest == "copper" else rest
		elif name.begins_with("weathered_"):
			out = "exposed_" + rest
		else:
			out = "weathered_" + rest
		world.session.particles.burst(Vector3(p) + Vector3(0.5, 0.5, 0.5), Color(0.5, 0.9, 0.8), 10, 1.5, "spark", 0.1)
	if out == "" or not BlockDB.has(out):
		return false
	world.set_blockv(p, Vox.make(BlockDB.id(out), meta))
	_use_tool(player, slot, st, 1)
	Sfx.play_at("axe_strip", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.8)
	return true


static func _boat(player, world: World, target: Dictionary, slot: int, st: ItemStack) -> bool:
	var it := st.item()
	var pos: Vector3
	var hit := world.raycast(player.eye_position(), player.look_dir(), 5.0, true, false)
	if hit.is_empty():
		return false
	pos = Vector3(hit.point) + Vector3(0, 0.05, 0)
	player.session.entities.spawn_vehicle(world, String(it.props.get("vehicle", "boat")), pos, it.props)
	spend(player, slot, st)
	return true
