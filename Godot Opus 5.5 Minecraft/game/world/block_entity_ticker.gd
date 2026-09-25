class_name BlockEntityTicker
extends RefCounted
## Runtime logic for block entities that need per-tick work: furnaces (burn + cook + XP),
## brewing stands (fuel, brew progress), hoppers (item transfer), campfires, beacons (pyramid
## effects), spawners, composter, plus the redstone-facing queries used by comparators.

static var _rng := RandomNumberGenerator.new()

const SMELT_TIME := 200
const BREW_TIME := 400


const TICKING := ["furnace", "brewing", "hopper", "campfire", "beacon", "spawner"]


static func tick_all(world: World, session) -> void:
	if world == null:
		return
	var set: Dictionary = session.ticking_bes.get(world.dim, {})
	if set.is_empty():
		return
	var stale := []
	for k in set.keys():
		var p: Vector3i = k
		if not world.is_loaded(p.x, p.z):
			continue
		var be := world.get_be(p.x, p.y, p.z)
		var kind := String(be.get("type", ""))
		if not (kind in TICKING):
			stale.append(k)
			continue
		match kind:
			"furnace":
				_tick_furnace(world, session, p, be)
			"brewing":
				_tick_brewing(world, p, be)
			"hopper":
				_tick_hopper(world, p, be)
			"campfire":
				_tick_campfire(world, session, p, be)
			"beacon":
				if session.tick_count % 80 == 0:
					_apply_beacon(world, session, p, be)
			"spawner":
				_tick_spawner(world, session, p, be)
			"trial_spawner":
				pass
	for k2 in stale:
		set.erase(k2)


static func valid_block(world: World, p: Vector3i, names: Array) -> bool:
	var n := BlockDB.name_of(world.get_blockv(p))
	return n in names


static func _tick_furnace(world: World, session, p: Vector3i, be: Dictionary) -> void:
	var items: Array = be.get("items", [])
	if items.size() < 3:
		return
	var kind: String = String(be.get("kind", "furnace"))
	var st_in := ItemStack.from_dict(items[0])
	var st_fuel := ItemStack.from_dict(items[1])
	var st_out := ItemStack.from_dict(items[2])
	var rec := RecipeDB.smelt_result(st_in.item_name(), kind) if st_in != null else {}
	var can_cook := not rec.is_empty() and (st_out == null or (st_out.item_name() == String(rec["out"])
			and st_out.count < st_out.max_stack()))
	var burn := int(be.get("burn", 0))
	var was_burning := burn > 0
	if burn > 0:
		be["burn"] = burn - 1
	# consume fuel
	if burn <= 0 and can_cook and st_fuel != null:
		var fticks := RecipeDB.fuel_ticks(st_fuel)
		if fticks > 0 or st_fuel.item_name() == "lava_bucket":
			be["burn"] = fticks if fticks > 0 else 20000
			if kind == "blast_furnace" or kind == "smoker":
				be["burn"] = int(be["burn"]) / 2
			be["burn_max"] = be["burn"]
			if st_fuel.item_name() == "lava_bucket":
				items[1] = ItemStack.of("bucket", 1).to_dict()
			else:
				st_fuel.count -= 1
				items[1] = st_fuel.to_dict() if st_fuel.count > 0 else {}
			burn = int(be["burn"])
	if burn > 0 and can_cook:
		be["cook"] = int(be.get("cook", 0)) + 1
		be["cook_total"] = SMELT_TIME / 2 if (kind == "blast_furnace" or kind == "smoker") else SMELT_TIME
		if int(be["cook"]) >= int(be["cook_total"]):
			be["cook"] = 0
			var out_name := String(rec["out"])
			if st_out == null:
				items[2] = ItemStack.of(out_name, 1).to_dict()
			else:
				st_out.count += 1
				items[2] = st_out.to_dict()
			st_in.count -= 1
			items[0] = st_in.to_dict() if st_in.count > 0 else {}
			be["xp"] = float(be.get("xp", 0.0)) + float(rec.get("xp", 0.1))
	else:
		be["cook"] = maxi(0, int(be.get("cook", 0)) - 2)
	be["items"] = items
	if (burn > 0) != was_burning:
		var v := world.get_blockv(p)
		var nm := BlockDB.name_of(v)
		if nm in ["furnace", "blast_furnace", "smoker"]:
			world.set_blockv(p, Vox.make(v & 0xFFF, (1 if burn > 0 else 0) | ((((v >> 12) & 3)) << 2)), World.F_URGENT)
		else:
			world.set_blockv(p, Vox.make(v & 0xFFF, 1 if burn > 0 else 0), World.F_URGENT)
	world.mark_modified(p.x, p.z)


static func _tick_brewing(world: World, p: Vector3i, be: Dictionary) -> void:
	var items: Array = be.get("items", [])
	if items.size() < 5:
		return
	var fuel := int(be.get("fuel", 0))
	var brew := int(be.get("brew", 0))
	var ing := ItemStack.from_dict(items[3])
	if brew > 0:
		be["brew"] = brew - 1
		if int(be["brew"]) % 6 == 0:
			world.session.particles.burst(Vector3(p) + Vector3(0.5, 1.3, 0.5), Color(0.6, 0.9, 1.0), 3, 0.6, "bubble", 0.08)
		if int(be["brew"]) <= 0:
			_do_brew(world, items, ing)
			be["items"] = items
		world.mark_modified(p.x, p.z)
		return
	if fuel <= 0:
		var st_fuel := ItemStack.from_dict(items[4])
		if st_fuel != null and st_fuel.item_name() == "blaze_powder":
			st_fuel.count -= 1
			items[4] = st_fuel.to_dict() if st_fuel.count > 0 else {}
			be["fuel"] = 20
			be["items"] = items
			world.mark_modified(p.x, p.z)
			return
	if ing == null:
		return
	var any := false
	for i in 3:
		var st := ItemStack.from_dict(items[i])
		if st == null:
			continue
		if not EffectDB.brew(st.item_name(), String(st.data.get("potion", "water")), ing.item_name()).is_empty():
			any = true
			break
	if any:
		be["brew"] = BREW_TIME
		world.mark_modified(p.x, p.z)


static func _do_brew(world: World, items: Array, ing: ItemStack) -> void:
	if ing == null:
		return
	for i in 3:
		var st := ItemStack.from_dict(items[i])
		if st == null:
			continue
		var r := EffectDB.brew(st.item_name(), String(st.data.get("potion", "water")), ing.item_name())
		if r.is_empty():
			continue
		var out := ItemStack.new(ItemDB.id(String(r["item"])), 1)
		out.data = st.data.duplicate()
		out.data["potion"] = String(r["potion"])
		items[i] = out.to_dict()
	ing.count -= 1
	items[3] = ing.to_dict() if ing.count > 0 else {}


static func _tick_hopper(world: World, p: Vector3i, be: Dictionary) -> void:
	if bool(be.get("locked", false)):
		return
	var items: Array = be.get("items", [])
	if items.is_empty():
		return
	# 1) push into the container the hopper faces
	var meta := (world.get_blockv(p) >> 12) & 15
	var dir := mini(meta & 7, 5)
	var fv: Vector3i = Vox.DIR_VEC[dir]
	var target: Vector3i = p + fv
	var tbe := world.get_be(target.x, target.y, target.z)
	if tbe.has("items"):
		var tinv := Inventory.new((tbe["items"] as Array).size())
		tinv.from_array(tbe["items"])
		for i in items.size():
			var st := ItemStack.from_dict(items[i])
			if st == null:
				continue
			var rem := tinv.add(st.with_count(1), 0, tinv.size())
			if rem == null:
				st.count -= 1
				items[i] = st.to_dict() if st.count > 0 else {}
				tbe["items"] = tinv.to_array()
				world.mark_modified(p.x, p.z)
				world.mark_modified(target.x, target.z)
				be["cooldown"] = 8
				break
	# 2) pull one item from the container above, or suck up dropped items lying on top
	if int(be.get("cooldown", 0)) <= 0:
		var above: Vector3i = p + Vector3i(0, 1, 0)
		var abe := world.get_be(above.x, above.y, above.z)
		var own := Inventory.new(items.size())
		own.from_array(items)
		if abe.has("items"):
			var ainv := Inventory.new((abe["items"] as Array).size())
			ainv.from_array(abe["items"])
			for i in ainv.size():
				var st2 := ainv.stack_at(i)
				if st2 == null:
					continue
				if own.add(st2.with_count(1), 0, own.size()) == null:
					st2.count -= 1
					ainv.set_stack(i, st2 if st2.count > 0 else null)
					abe["items"] = ainv.to_array()
					items = own.to_array()
					be["cooldown"] = 8
					world.mark_modified(p.x, p.z)
					world.mark_modified(above.x, above.z)
					break
		elif world.session != null and BlockDB.full[world.get_id(above.x, above.y, above.z)] == 0:
			var box := AABB(Vector3(p) + Vector3(0, 0.6, 0), Vector3(1, 1.4, 1))
			for e in world.session.entities.all():
				if not (e is SimpleEntities.ItemEntity):
					continue
				var ie: SimpleEntities.ItemEntity = e
				if ie.stack == null or ie.is_queued_for_deletion() or not ie.body.aabb().intersects(box):
					continue
				var rem3 := own.add(ie.stack, 0, own.size())
				if rem3 == null:
					ie.stack = null
					ie.queue_free()
				elif rem3.count < ie.stack.count:
					ie.stack = rem3
				else:
					continue
				items = own.to_array()
				world.mark_modified(p.x, p.z)
				break
	be["items"] = items
	be["cooldown"] = maxi(0, int(be.get("cooldown", 0)) - 1)


static func _tick_campfire(world: World, session, p: Vector3i, be: Dictionary) -> void:
	var food: Array = be.get("food", [])
	if food.is_empty():
		return
	var e: Dictionary = food[0]
	e["t"] = int(e.get("t", 0)) + 1
	if int(e["t"]) >= 600:
		var name := String(e.get("item", ""))
		var rec := RecipeDB.smelt_result(name, "campfire")
		if not rec.is_empty():
			var st := ItemStack.of(String(rec["out"]), 1)
			if st != null:
				session.entities.spawn_item(world, Vector3(p) + Vector3(0.5, 0.7, 0.5), st)
		food.remove_at(0)
		be["food"] = food
		world.mark_modified(p.x, p.z)
	if session.tick_count % 4 == 0:
		session.particles.flame(Vector3(p) + Vector3(0.5, 0.7, 0.5))
		session.particles.smoke(Vector3(p) + Vector3(0.5, 1.0, 0.5), 1)


static func _tick_spawner(world: World, session, p: Vector3i, be: Dictionary) -> void:
	var mob := String(be.get("mob", ""))
	if mob == "" or not MobDB.has(mob):
		return
	if session.difficulty <= 0 or not bool(session.gamerule("doMobSpawning", true)):
		return
	var delay := int(be.get("delay", 0))
	if delay > 0:
		be["delay"] = delay - 1
		world.mark_modified(p.x, p.z)
		return
	var radius := 8.0
	var count := 0
	var pl = session.player
	var dist: float = pl.body.pos.distance_to(Vector3(p))
	if dist > 16.0:
		return
	for e in session.entities.all():
		if e is Mob and (e as Mob).body.pos.distance_to(Vector3(p)) < radius:
			count += 1
	if count >= 6:
		be["delay"] = 40
		return
	var d := MobDB.get_def(mob)
	for attempt in 4:
		var off := Vector3(_rng.randf_range(-3, 3), _rng.randf_range(-1, 3), _rng.randf_range(-3, 3))
		var sp := Vector3(p) + off + Vector3(0.5, 0.5, 0.5)
		if world.get_block(floori(sp.x), floori(sp.y), floori(sp.z)) != 0:
			continue
		if BlockDB.solid[world.get_id(floori(sp.x), floori(sp.y) - 1, floori(sp.z))] == 0:
			continue
		session.entities.spawn_mob(world, mob, sp, {"persistent": false, "force_hostile": true})
		session.particles.burst(Vector3(p) + Vector3(0.5, 0.5, 0.5), Color(0.3, 0.3, 0.4), 12, 1.5, "large_smoke", 0.15)
		Sfx.play_mob(mob, "notice", Vector3(p), 0.5)
		be["delay"] = 200 + _rng.randi_range(0, 200)
		break


## Pyramid level of the beacon at p (0..4).
static func beacon_levels(world: World, p: Vector3i) -> int:
	var base_names := ["iron_block", "gold_block", "diamond_block", "emerald_block", "netherite_block"]
	var levels := 0
	for lvl in range(1, 6):
		var ok := true
		for dx in range(-lvl, lvl + 1):
			for dz in range(-lvl, lvl + 1):
				if maxi(absi(dx), absi(dz)) != lvl:
					continue
				var n := BlockDB.name_of(world.get_blockv(p + Vector3i(dx, -lvl, dz)))
				if not (n in base_names):
					ok = false
					break
			if not ok:
				break
		if not ok:
			break
		levels += 1
	return levels


static func _apply_beacon(world: World, session, p: Vector3i, be: Dictionary) -> void:
	var levels := beacon_levels(world, p)
	if levels <= 0:
		return
	var eff := String(be.get("effect", ""))
	if eff == "":
		return
	var range_blocks := 10 + levels * 10
	var amp := 0 if levels < 4 else 1
	var pl = session.player
	if pl.body.pos.distance_to(Vector3(p)) > range_blocks:
		return
	if "speed" in eff or "haste" in eff or "strength" in eff or "regeneration" in eff:
		pl.add_effect(eff, 200, amp)
		for e in session.entities.all():
			if e is Mob and (e as Mob).body.pos.distance_to(Vector3(p)) < range_blocks:
				(e as Mob).add_effect(eff, 200, amp)


# ------------------------------------------------------------------------------------------------
## Comparator measurement helper (block entity contents are read here so the redstone module can
## stay free of container knowledge).
static func measure(world: World, p: Vector3i) -> int:
	var be := world.get_be(p.x, p.y, p.z)
	if be.has("items"):
		var items: Array = be["items"]
		if items.is_empty():
			return 0
		var frac := 0.0
		for it in items:
			var st := ItemStack.from_dict(it) if it is Dictionary else null
			if st != null:
				frac += float(st.count) / float(st.max_stack())
		frac /= float(items.size())
		return floori(frac * 14.0) + (1 if frac > 0.0 else 0)
	var food: Array = be.get("food", [])
	if not food.is_empty():
		return 4 + food.size()
	return -1
