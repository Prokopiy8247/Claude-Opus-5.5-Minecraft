class_name Dispensers
extends RefCounted
## Dispenser / dropper behaviour when triggered by redstone.

static var _rng := RandomNumberGenerator.new()


static func fire(world: World, p: Vector3i, v: int) -> void:
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	var face := mini(((v >> 12) & 7), 5)
	var fv: Vector3i = Vox.DIR_VEC[face]
	var be := world.get_be(p.x, p.y, p.z, true)
	var items: Array = be.get("items", [])
	var filled := []
	for i in items.size():
		if items[i] is Dictionary and not (items[i] as Dictionary).is_empty():
			filled.append(i)
	var front: Vector3i = p + fv
	var out_pos := Vector3(p) + Vector3(0.5, 0.5, 0.5) + Vector3(fv) * 0.7
	if filled.is_empty():
		Sfx.play_at("click", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.5, 1.2)
		return
	var slot: int = filled[_rng.randi_range(0, filled.size() - 1)]
	var st := ItemStack.from_dict(items[slot])
	if st == null:
		return
	var session = world.session
	var dropper := d.name == "dropper" or d.name == "crafter"
	var used := false
	var consume := true
	if dropper:
		# insert into a container in front, else drop
		var fbe := world.get_be(front.x, front.y, front.z)
		if fbe.has("items"):
			var inv := Inventory.new((fbe["items"] as Array).size())
			inv.from_array(fbe["items"])
			var rem := inv.add(st.with_count(1), 0, inv.size())
			if rem == null:
				fbe["items"] = inv.to_array()
				world.mark_modified(front.x, front.z)
				used = true
			else:
				consume = false
		else:
			_drop(world, out_pos, Vector3(fv), st.with_count(1))
			used = true
	else:
		used = _dispense(world, p, front, fv, out_pos, st)
		if not used:
			_drop(world, out_pos, Vector3(fv), st.with_count(1))
			used = true
	if used and consume:
		var name := st.item_name()
		if name == "water_bucket" or name == "lava_bucket" or name == "powder_snow_bucket":
			st = ItemStack.of("bucket", 1)
		elif name == "bucket" and be.has("_filled"):
			st = ItemStack.of(String(be["_filled"]), 1)
			be.erase("_filled")
		elif st.item().is_damageable() and name == "flint_and_steel":
			st.damage += 1
			if st.damage >= st.item().durability:
				st = null
		else:
			st.count -= 1
		items[slot] = st.to_dict() if st != null and not st.is_empty() else {}
		be["items"] = items
		world.mark_modified(p.x, p.z)
	Sfx.play_at("dispense", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.6)
	if session != null:
		session.particles.smoke(out_pos, 4)


static func _drop(world: World, pos: Vector3, dir: Vector3, st: ItemStack) -> void:
	if world.session == null:
		return
	var vel := dir * (0.25 + _rng.randf() * 0.1) + Vector3(_rng.randf_range(-0.03, 0.03), 0.1, _rng.randf_range(-0.03, 0.03))
	world.session.entities.spawn_item(world, pos - Vector3(0, 0.15, 0), st, vel * 20.0)


static func _dispense(world: World, p: Vector3i, front: Vector3i, fv: Vector3i, out_pos: Vector3, st: ItemStack) -> bool:
	var session = world.session
	if session == null:
		return false
	var it := st.item()
	var n := it.name
	var dir := Vector3(fv)
	var ents = session.entities
	match n:
		"arrow", "spectral_arrow", "tipped_arrow":
			ents.spawn_projectile(world, n, out_pos, (dir + Vector3(0, 0.1, 0)).normalized() * 1.1 * 20.0, null, {"pickup": 1})
			Sfx.play_at("bow", out_pos)
			return true
		"snowball", "egg", "splash_potion", "lingering_potion", "experience_bottle", "wind_charge":
			ents.spawn_projectile(world, String(it.props.get("projectile", n)), out_pos, (dir + Vector3(0, 0.1, 0)).normalized() * 1.1 * 20.0,
				null, {"potion": st.data.get("potion", "")})
			return true
		"fire_charge":
			ents.spawn_projectile(world, "small_fireball", out_pos, dir * 1.2 * 20.0, null)
			return true
		"tnt":
			Explosions.prime_tnt(world, Vector3(front), 80)
			return true
		"water_bucket", "lava_bucket":
			return Fluids.place_source(world, front, 1 if n == "water_bucket" else 2)
		"bucket":
			var fvb := world.get_block(front.x, front.y, front.z)
			var kind := BlockDB.fluid[fvb & 0xFFF]
			if kind != 0 and ((fvb >> 12) & 15) == 0:
				world.set_block(front.x, front.y, front.z, 0)
				world.get_be(p.x, p.y, p.z, true)["_filled"] = "water_bucket" if kind == 1 else "lava_bucket"
				return true
			return false
		"bone_meal":
			return ItemUse.apply_bone_meal(world, front, null)
		"flint_and_steel":
			var tv := world.get_block(front.x, front.y, front.z)
			if BlockDB.defs[tv & 0xFFF].props.get("tnt", false):
				world.set_block(front.x, front.y, front.z, 0)
				Explosions.prime_tnt(world, Vector3(front), 80)
				return true
			return Fire.ignite(world, front)
		"shears":
			return ents.shear_at(world, front)
		"carved_pumpkin", "wither_skeleton_skull":
			if world.get_block(front.x, front.y, front.z) == 0:
				world.set_block(front.x, front.y, front.z, BlockDB.id(n))
				BlockBehaviors.on_placed(world, front, world.get_block(front.x, front.y, front.z), null)
				return true
			return false
	if it.kind == "spawn_egg":
		ents.spawn_mob(world, String(it.props["mob"]), Vector3(front) + Vector3(0.5, 0.0, 0.5))
		return true
	if it.kind == "armor":
		return ents.equip_nearby(world, front, st)
	if it.props.has("vehicle"):
		return ents.spawn_vehicle(world, String(it.props["vehicle"]), Vector3(front) + Vector3(0.5, 0.0, 0.5), it.props) != null
	if n.ends_with("shulker_box"):
		if world.get_block(front.x, front.y, front.z) == 0:
			world.set_block(front.x, front.y, front.z, Vox.make(BlockDB.id(n), (v_face(fv))))
			return true
	return false


static func v_face(fv: Vector3i) -> int:
	for d in 6:
		if Vox.DIR_VEC[d] == fv:
			return d
	return Vox.UP
