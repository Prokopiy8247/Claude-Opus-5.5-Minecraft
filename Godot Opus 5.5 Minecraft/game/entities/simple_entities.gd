class_name SimpleEntities
extends RefCounted
## Dropped items, XP orbs, primed TNT, falling blocks, projectiles (arrows, snowballs, potions,
## fireballs, ender pearls, wind charges), boats and minecarts. All are Entity subclasses defined
## here so the whole lightweight-entity family lives in one file.

# ------------------------------------------------------------------------------------------------
# Block mesh materials for dropped blocks / falling blocks
static var _drop_mats := {}

static func _drop_material(render_layer: int) -> ShaderMaterial:
	if _drop_mats.has(render_layer):
		return _drop_mats[render_layer]
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/voxel_entity.gdshader")
	m.set_shader_parameter("blocks", TextureLibrary.block_array)
	m.set_shader_parameter("alpha_cut", 0.5 if render_layer != 2 else 0.05)
	m.set_shader_parameter("use_vertex_light", true)
	_drop_mats[render_layer] = m
	return m


static func _tinted_material(render_layer: int, tint: Color) -> ShaderMaterial:
	var key := "t%d_%d" % [render_layer, tint.to_rgba32()]
	if _drop_mats.has(key):
		return _drop_mats[key]
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/voxel_entity.gdshader")
	m.set_shader_parameter("blocks", TextureLibrary.block_array)
	m.set_shader_parameter("alpha_cut", 0.05)
	m.set_shader_parameter("use_vertex_light", true)
	m.set_shader_parameter("ent_flash", Vector4(tint.r, tint.g, tint.b, 0.0))
	_drop_mats[key] = m
	return m


# ================================================================================================
## Dropped item stack: renders the block geometry or a flat item sprite that bobs and spins.
class ItemEntity extends Entity:
	var stack: ItemStack = null
	var sprite: Sprite3D = null
	var spin := 0.0
	var bob := 0.0
	var ground_ticks := 0
	var throw_vel := Vector3.ZERO

	func _init() -> void:
		type_name = "item"
		body.width = 0.25
		body.height = 0.25

	func setup_item(w: World, pos: Vector3, p_stack: ItemStack, vel := Vector3.ZERO) -> void:
		setup(w, pos)
		stack = p_stack
		body.vel = vel
		pickup_delay = 10
		gravity = 0.04
		_build()

	func _build() -> void:
		if stack == null:
			return
		var it := stack.item()
		if it == null:
			return
		if it.block != "":
			var v := Vox.make(BlockDB.id(it.block), 0)
			var d: BlockDef = BlockDB.defs[v & 0xFFF]
			if d.model == BlockDB.M_CUBE or d.model == BlockDB.M_LEAVES or d.model == BlockDB.M_PATH:
				var mesh := BlockMesh.build(v, Color(-1, 0, 0), true)
				var mi := MeshInstance3D.new()
				mi.mesh = mesh
				mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
				visual.add_child(mi)
			else:
				var sprite2 := ItemIcons.sprite_mesh(it)
				if sprite2 != null:
					visual.add_child(sprite2)
				else:
					var mesh2 := BlockMesh.build(v, Color(-1, 0, 0), true)
					var mi2 := MeshInstance3D.new()
					mi2.mesh = mesh2
					visual.add_child(mi2)
		else:
			var n := ItemIcons.sprite_mesh(it)
			if n != null:
				visual.add_child(n)
		visual.position = Vector3(0, 0.2, 0)

	func tick() -> void:
		if stack != null and stack.data.get("despawn", false) and age > 6000:
			queue_free()
			return
		if pickup_delay > 0:
			pickup_delay -= 1
		super()
		if on_ground:
			ground_ticks += 1
			body.vel.x *= 0.6
			body.vel.z *= 0.6
		if stack != null and stack.data.get("owner") != null:
			pickup_delay = maxi(pickup_delay, 20)
		if age > 6000:
			queue_free()

	func frame(alpha: float) -> void:
		super(alpha)
		global_position.y += 0.25 + sin(float(age + alpha) * 0.12) * 0.06
		if visual != null:
			visual.rotation.y = float(age + alpha) * 0.05

	func try_pickup(player) -> bool:
		if stack == null or pickup_delay > 0 or player.dead or player.gamemode == Player.SPECTATOR:
			return false
		if body.pos.distance_squared_to(player.body.pos + Vector3(0, 0.8, 0)) > 2.6:
			return false
		if player.gamemode == Player.CREATIVE:
			queue_free()
			Sfx.play_at("pop", body.pos, 0.3)
			return true
		var rem: ItemStack = player.inventory.add(stack, 0, 36)
		if rem == null:
			Sfx.play_at("pop", body.pos, 0.3)
			queue_free()
			return true
		if rem.count < stack.count:
			stack = rem
			player.inventory.changed.emit()
			return true
		return false

	func on_death(_c: String, _k) -> void:
		queue_free()


# ================================================================================================
## Experience orb.
class XpOrb extends Entity:
	var amount := 1
	var life_total := 6000

	func _init() -> void:
		type_name = "xp_orb"
		body.width = 0.25
		body.height = 0.25
		gravity = 0.03

	func setup_xp(w: World, pos: Vector3, n: int) -> void:
		setup(w, pos)
		amount = n
		body.vel = Vector3(randf_range(-0.05, 0.05), 0.1, randf_range(-0.05, 0.05))
		var mesh := ItemIcons.orb_mesh(amount)
		var mi := MeshInstance3D.new()
		mi.mesh = mesh
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		visual.add_child(mi)
		visual.position = Vector3(0, 0.1, 0)

	func tick() -> void:
		super()
		if age > life_total:
			queue_free()
		if session != null and session.player != null and not session.player.dead:
			var pl = session.player
			var d := body.pos.distance_squared_to(pl.body.pos + Vector3(0, 0.9, 0))
			if d < 64.0:
				var dir: Vector3 = (pl.body.pos + Vector3(0, 0.9, 0) - body.pos).normalized()
				var pull := 0.08 if d > 4.0 else 0.2
				body.vel.x += dir.x * pull
				body.vel.y += dir.y * pull
				body.vel.z += dir.z * pull
				body.vel *= 0.92
			if d < 1.8:
				var rest := ItemExtras.mending(pl, amount)
				if rest > 0:
					pl.stats.add_xp(rest)
				Sfx.play_at("xp", body.pos, 0.4, 1.0 + randf() * 0.4)
				queue_free()

	func frame(alpha: float) -> void:
		super(alpha)
		global_position.y += 0.05 + sin(float(age + alpha) * 0.3) * 0.03

	func on_death(_c: String, _k) -> void:
		queue_free()


# ================================================================================================
## Primed TNT.
class TntEntity extends Entity:
	var fuse := 80
	var power := 4.0
	var igniter = null
	var mesh_node: MeshInstance3D = null

	func _init() -> void:
		type_name = "tnt"
		body.width = 0.98
		body.height = 0.98
		gravity = 0.04

	func setup_tnt(w: World, pos: Vector3, p_fuse: int, p_igniter = null) -> void:
		setup(w, pos)
		fuse = p_fuse
		igniter = p_igniter
		body.vel = Vector3(0, 0.2, 0)
		_build()

	func _build() -> void:
		var v := Vox.make(BlockDB.id("tnt"), 0)
		mesh_node = MeshInstance3D.new()
		mesh_node.mesh = BlockMesh.build(v, Color(-1, 0, 0), true)
		mesh_node.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		visual.add_child(mesh_node)
		visual.position = Vector3(0, 0.02, 0)

	func tick() -> void:
		super()
		fuse -= 1
		if mesh_node != null:
			var white := (fuse / 6) % 2 == 0
			mesh_node.material_override = _white_material(white)
		if fuse <= 0:
			Explosions.explode(world, body.pos + Vector3(0, 0.5, 0), power, false, self)
			queue_free()

	func _white_material(white: bool) -> ShaderMaterial:
		var key := "tnt_white" if white else "tnt_normal"
		if SimpleEntities._drop_mats.has(key):
			return SimpleEntities._drop_mats[key]
		var m := ShaderMaterial.new()
		m.shader = load("res://game/world/shaders/voxel_entity.gdshader")
		m.set_shader_parameter("blocks", TextureLibrary.block_array)
		m.set_shader_parameter("alpha_cut", 0.5)
		if white:
			m.set_shader_parameter("ent_flash", Vector4(1, 1, 1, 0.75))
		SimpleEntities._drop_mats[key] = m
		return m

	func on_death(_c: String, _k) -> void:
		Explosions.explode(world, body.pos + Vector3(0, 0.5, 0), power, false, self)
		queue_free()


# ================================================================================================
## Falling block (sand, gravel, concrete powder...).
class FallingBlockEntity extends Entity:
	var block_value := 0
	var mesh_node: MeshInstance3D = null

	func _init() -> void:
		type_name = "falling_block"
		body.width = 0.98
		body.height = 0.98
		gravity = 0.04

	func setup_falling(w: World, pos: Vector3, v: int) -> void:
		setup(w, pos)
		block_value = v
		body.vel = Vector3.ZERO
		mesh_node = MeshInstance3D.new()
		mesh_node.mesh = BlockMesh.build(v, Color(-1, 0, 0), true)
		mesh_node.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		visual.add_child(mesh_node)
		visual.position = Vector3(0, 0.5, 0)

	func tick() -> void:
		super()
		if on_ground or age > 400:
			_land()

	func _land() -> void:
		var p := Vector3i(floori(body.pos.x), floori(body.pos.y + 0.5), floori(body.pos.z))
		var cur := world.get_block(p.x, p.y, p.z)
		if cur == 0 or BlockDB.replaceable[cur & 0xFFF] == 1 or BlockDB.fluid[cur & 0xFFF] != 0:
			world.set_block(p.x, p.y, p.z, block_value, World.F_URGENT)
			BlockBehaviors.on_placed(world, p, block_value, null)
		else:
			if session != null:
				session.entities.spawn_item(world, body.pos, ItemStack.new(ItemDB.for_block(block_value), 1))
		queue_free()

	func on_death(_c: String, _k) -> void:
		_land()


# ================================================================================================
## Projectiles: arrow, spectral arrow, snowball, egg, ender pearl, potion, fireballs, wind charge.
class Projectile extends Entity:
	var kind := "arrow"
	var shooter = null
	var damage := 0.0
	var potion := ""
	var stuck := false
	var hit_pickup := false
	var mesh_node: Node3D = null
	var stick_timer := 0

	func _init() -> void:
		type_name = "projectile"
		body.width = 0.25
		body.height = 0.25
		gravity = 0.03
		drag = 0.99
		no_physics = true

	func setup_proj(w: World, p_kind: String, pos: Vector3, vel: Vector3, p_shooter, extra := {}) -> void:
		setup(w, pos)
		kind = p_kind
		shooter = p_shooter
		body.vel = vel
		potion = String(extra.get("potion", ""))
		damage = float(extra.get("damage", 0.0))
		data = extra
		no_physics = false
		_build()

	func _build() -> void:
		match kind:
			"arrow", "spectral_arrow", "tipped_arrow":
				mesh_node = ItemIcons.arrow_mesh(kind == "spectral_arrow")
			"snowball":
				mesh_node = ItemIcons.cube_mesh(Color(0.95, 0.97, 1.0), 0.2)
			"egg":
				mesh_node = ItemIcons.cube_mesh(Color(0.9, 0.88, 0.83), 0.2)
			"ender_pearl":
				mesh_node = ItemIcons.cube_mesh(Color(0.13, 0.5, 0.44), 0.2)
			"splash_potion", "lingering_potion":
				var c := EffectDB.potion_color(potion) if potion != "" else Color(0.4, 0.5, 0.9)
				mesh_node = ItemIcons.cube_mesh(c, 0.22)
			"small_fireball":
				mesh_node = ItemIcons.fireball_mesh(0.3, true)
			"fishing_bobber":
				mesh_node = ItemIcons.cube_mesh(Color(0.85, 0.15, 0.12), 0.14)
			"fireball":
				mesh_node = ItemIcons.fireball_mesh(0.9, false)
			"wind_charge":
				mesh_node = ItemIcons.cube_mesh(Color(0.8, 0.85, 1.0), 0.25)
			"experience_bottle":
				mesh_node = ItemIcons.cube_mesh(Color(0.7, 0.95, 0.3), 0.2)
			_:
				mesh_node = ItemIcons.cube_mesh(Color.WHITE, 0.2)
		if mesh_node != null:
			visual.add_child(mesh_node)

	func physics() -> void:
		if kind in ["small_fireball", "fireball"]:
			body.vel.y -= 0.0
			body.vel *= 0.99
		else:
			body.vel.y -= gravity
			body.vel.x *= 0.99
			body.vel.z *= 0.99
			body.vel.y *= 0.99
		if in_water:
			body.vel *= 0.6
		body.move(world, body.vel)

	func tick() -> void:
		prev_pos = body.pos
		if kind == "fishing_bobber":
			age += 1
			ItemExtras.bobber_tick(self)
			return
		if kind == "item_trident" and (stuck or age > 30) and _loyalty() > 0 and shooter != null and is_instance_valid(shooter):
			# loyalty: fly back to the thrower
			stuck = false
			var to: Vector3 = shooter.eye_position() - body.pos
			body.vel = to.normalized() * minf(to.length(), 0.3 + 0.2 * _loyalty())
			body.pos += body.vel
			if to.length() < 1.2:
				try_pickup(shooter)
			return
		if stuck:
			stick_timer += 1
			if kind == "item_trident":
				return
			if stick_timer > 1200 or (kind in ["snowball", "egg", "splash_potion", "lingering_potion", "wind_charge", "small_fireball", "fireball"]):
				queue_free()
			return
		if kind == "firework_rocket" and age > 30:
			_firework_burst()
			queue_free()
			return
		age += 1
		physics()
		if world != null:
			_block_hit()
			_entity_hit()
			if body.collided_h or (body.on_ground and kind not in ["small_fireball", "fireball"]):
				_on_hit_block()
		if age > 1200:
			queue_free()

	func _block_hit() -> void:
		var p := Vector3i(floori(body.pos.x), floori(body.pos.y), floori(body.pos.z))
		var v := world.get_block(p.x, p.y, p.z)
		if v == 0:
			return
		var id := v & 0xFFF
		if BlockDB.fluid[id] != 0:
			if kind == "small_fireball" or kind == "fireball":
				_on_hit_block()
			return
		if BlockDB.full[id] == 1 or BlockDB.solid[id] == 1:
			_on_hit_block()

	func _on_hit_block() -> void:
		var tb := Vector3i(floori(body.pos.x + body.vel.x * 0.5), floori(body.pos.y + body.vel.y * 0.5), floori(body.pos.z + body.vel.z * 0.5))
		if BlockDB.name_of(world.get_block(tb.x, tb.y, tb.z)) == "target":
			RedstoneSystem.target_hit(world, tb, body.pos, kind.ends_with("arrow") or kind == "item_trident")
		match kind:
			"item_trident":
				stuck = true
				body.vel = Vector3.ZERO
				Sfx.play_at("arrow_hit", body.pos, 0.6)
			"experience_bottle":
				if session != null:
					session.entities.spawn_xp(world, body.pos, randi_range(3, 11))
					session.particles.burst(body.pos, Color(0.4, 0.9, 1.0), 16, 2.5, "spark", 0.12)
				Sfx.play_at("glass_break", body.pos, 0.6)
				queue_free()
			"egg":
				if session != null:
					session.particles.poof(body.pos, 6, 0.5)
					if randf() < 0.125:
						var chicks := 4 if randf() < 0.03125 else 1
						for i in chicks:
							var c: Mob = session.entities.spawn_mob(world, "chicken", body.pos + Vector3(0, 0.2, 0), {"baby": true})
							if c != null:
								c.data["grow"] = -Breeding.GROW_TICKS
				queue_free()
			"snowball":
				if session != null:
					session.particles.burst(body.pos, Color(0.95, 0.97, 1.0), 8, 1.5, "dust", 0.1)
				queue_free()
			"wind_charge":
				_wind_burst()
				queue_free()
			"firework_rocket":
				_firework_burst()
				queue_free()
			"arrow", "spectral_arrow", "tipped_arrow":
				stuck = true
				body.vel = Vector3.ZERO
				Sfx.play_at("arrow_hit", body.pos, 0.5)
				if kind == "spectral_arrow" and session != null:
					var hit_ent = session.entities.entity_at(body.pos, 1.0, self)
					if hit_ent != null and hit_ent.has_method("add_effect"):
						hit_ent.add_effect("glowing", 200, 0)
			"ender_pearl":
				var pl = session.player if session != null else null
				if pl != null and shooter == pl:
					var dest := _safe_teleport(body.pos)
					pl.teleport(dest)
					pl.stats.damage(5.0, "ender_pearl")
					session.particles.portal(dest + Vector3(0, 1, 0))
					Sfx.play_at("teleport", dest)
				queue_free()
			"small_fireball", "fireball":
				if session != null:
					if kind == "fireball":
						Explosions.explode(world, body.pos, 1.0, true, self, true)
					else:
						session.particles.burst(body.pos, Color(1, 0.6, 0.2), 12, 2.0, "flame", 0.2)
						Sfx.play_at("explode_small", body.pos, 0.5)
						_ignite_area()
				queue_free()
			"splash_potion", "lingering_potion":
				if session != null:
					session.particles.burst(body.pos, EffectDB.potion_color(potion), 24, 3.0, "spark", 0.15)
					Sfx.play_at("glass_break", body.pos, 0.6)
					_apply_potion(3.0 if kind == "splash_potion" else 4.0, kind == "lingering_potion")
				queue_free()
			_:
				if session != null:
					Sfx.play_at("bow_hit", body.pos, 0.3)
				queue_free()

	func _ignite_area() -> void:
		var p := Vector3i(floori(body.pos.x), floori(body.pos.y), floori(body.pos.z))
		if world.get_block(p.x, p.y, p.z) == 0:
			Fire.ignite(world, p)

	func _apply_potion(radius: float, lingering: bool) -> void:
		if session == null:
			return
		var eff := EffectDB.potion_effects(potion)
		if eff.is_empty():
			return
		for e in session.entities.all():
			var ent = e
			if ent is Entity and (ent as Entity).body.pos.distance_to(body.pos) <= radius:
				var ent2 = ent
				if ent2.has_method("add_effect"):
					for pair in eff:
						var ticks: int = int(pair[1])
						if lingering:
							ticks = maxi(1, ticks / 4)
						ent2.add_effect(String(pair[0]), ticks, int(pair[2]))
		var pl = session.player
		if pl != null and pl.body.pos.distance_to(body.pos) <= radius:
			for pair in eff:
				var ticks2: int = int(pair[1])
				if lingering:
					ticks2 = maxi(1, ticks2 / 4)
				pl.add_effect(String(pair[0]), ticks2, int(pair[2]))

	func _safe_teleport(p: Vector3) -> Vector3:
		var q := p
		var g := 0
		while g < 32:
			var below := world.get_id(floori(q.x), floori(q.y) - 1, floori(q.z))
			if BlockDB.full[below] == 1 and world.get_block(floori(q.x), floori(q.y), floori(q.z)) == 0 \
					and world.get_block(floori(q.x), floori(q.y) + 1, floori(q.z)) == 0:
				return q
			q.y += 1.0
			g += 1
		return p

	## Arrow damage from the bow draw (release passes 2 x power), Power enchant, random crit bonus.
	func _arrow_damage() -> float:
		var dmg := damage if damage > 0.0 else 2.0
		dmg += float(data.get("power", 0.0)) * 2.0
		if bool(data.get("crit", false)):
			dmg += float(randi_range(0, int(dmg / 2.0) + 1))
		return maxf(1.0, round(dmg))

	func _entity_hit() -> void:
		if session == null or stuck:
			return
		var pl = session.player
		var near_player: bool = pl != null and pl.world == world and (pl.body.aabb() as AABB).grow(0.3).has_point(body.pos)
		if near_player and shooter != pl and not pl.dead and pl.gamemode != Player.SPECTATOR and kind != "ender_pearl":
			_hit_player(pl)
			return
		var hit = session.entities.entity_at(body.pos, 0.35, self)
		if hit == null:
			return
		if hit == shooter:
			return
		if kind == "fireball":
			Explosions.explode(world, body.pos, 1.0, true, self, true)
			queue_free()
			return
		if hit.has_method("hurt"):
			var dmg := damage if damage > 0.0 else 2.0
			if kind.ends_with("arrow"):
				dmg = _arrow_damage()
			elif kind == "item_trident":
				dmg = 8.0
			elif kind == "snowball":
				dmg = 3.0 if (hit is Mob and (hit as Mob).mob == "blaze") else 0.01
			var dir := Vector2(body.vel.x, body.vel.z).normalized()
			hit.hurt(dmg, "arrow", shooter, dir, 0.3)
			if kind == "item_trident":
				_channel(hit.body.pos)
				body.vel *= -0.1
				stuck = true
				return
			if kind == "tipped_arrow" or kind == "arrow":
				var eff2 := EffectDB.potion_effects(potion)
				for pair in eff2:
					if hit.has_method("add_effect"):
						hit.add_effect(String(pair[0]), int(pair[1]), int(pair[2]))
		if kind in ["snowball", "egg", "ender_pearl"]:
			queue_free()

	func frame(alpha: float) -> void:
		global_position = prev_pos.lerp(body.pos, alpha)
		if mesh_node != null and not stuck:
			var d := body.vel
			if d.length_squared() > 1e-6:
				var dn := d.normalized()
				var up := Vector3.UP if absf(dn.y) < 0.999 else Vector3.FORWARD
				mesh_node.look_at(mesh_node.global_position + dn, up)

	func _loyalty() -> int:
		var st := ItemStack.from_dict(data.get("item", {}))
		return st.enchant_level("loyalty") if st != null else 0

	func _channel(at: Vector3) -> void:
		var st := ItemStack.from_dict(data.get("item", {}))
		if st != null and st.enchant_level("channeling") > 0 and session != null and session.weather == 2 \
				and world.get_light(floori(at.x), floori(at.y + 1.0), floori(at.z)).x >= 15:
			session.strike_lightning(at)

	## A projectile hitting the player.
	func _hit_player(pl) -> void:
		var diff: int = session.difficulty
		var special := String(data.get("special", ""))
		match kind:
			"fireball":
				Explosions.explode(world, body.pos, 1.0, true, self, true)
			"small_fireball":
				if bool(data.get("wither", false)):
					pl.stats.damage(8.0 if diff >= 2 else 5.0, "wither_skull", shooter)
					if diff >= 2:
						pl.add_effect("wither", 200 if diff == 2 else 800, 1)
				elif special == "shulker_bullet":
					pl.stats.damage(4.0, "mob", shooter)
					pl.add_effect("levitation", 200, 0)
				elif special == "guardian_beam":
					pl.stats.damage(6.0, "magic", shooter)
				else:
					pl.stats.damage(5.0, "fire", shooter)
					pl.stats.fire_ticks = maxi(pl.stats.fire_ticks, 100)
			"arrow", "spectral_arrow", "tipped_arrow":
				pl.stats.damage(_arrow_damage(), "arrow", shooter)
				for pair in EffectDB.potion_effects(potion):
					pl.add_effect(String(pair[0]), int(pair[1]), int(pair[2]))
				if kind == "spectral_arrow":
					pl.add_effect("glowing", 200, 0)
			"item_trident":
				pl.stats.damage(8.0, "arrow", shooter)
			"snowball", "egg":
				pl.stats.damage(0.01, "projectile", shooter)
			"splash_potion", "lingering_potion", "experience_bottle", "wind_charge":
				_on_hit_block()
				return
			_:
				pl.stats.damage(maxf(damage, 2.0), "projectile", shooter)
		if kind == "item_trident":
			body.vel *= -0.1
			stuck = true
			return
		queue_free()

	func _wind_burst() -> void:
		if session == null:
			return
		session.particles.burst(body.pos, Color(0.85, 0.9, 1.0), 20, 4.0, "dust", 0.2)
		Sfx.play_at("riptide", body.pos, 0.5)
		for e in session.entities.all():
			var ent: Entity = e
			var d := ent.body.pos - body.pos
			if d.length() < 2.5:
				ent.body.vel += d.normalized() * 0.6 + Vector3(0, 0.5, 0)
		var pl = session.player
		if pl != null and pl.body.pos.distance_to(body.pos) < 2.5:
			var d2: Vector3 = pl.body.pos - body.pos
			pl.body.vel += d2.normalized() * 0.6 + Vector3(0, 0.7, 0)
			pl.body.fall_distance = 0.0

	func _firework_burst() -> void:
		if session == null:
			return
		var cols := [Color(1, 0.3, 0.3), Color(0.3, 1, 0.4), Color(0.4, 0.6, 1), Color(1, 0.9, 0.3), Color(1, 0.4, 1)]
		session.particles.spark(body.pos, cols[randi() % cols.size()], 40, 7.0)
		Sfx.play_at("firework", body.pos, 0.8)

	func try_pickup(player) -> bool:
		if (stuck or _loyalty() > 0) and kind == "item_trident" and body.pos.distance_squared_to(player.eye_position() - Vector3(0, 0.8, 0)) < 3.0:
			var st := ItemStack.from_dict(data.get("item", {}))
			if st == null:
				st = ItemStack.of("trident", 1)
			if player.inventory.add(st, 0, 36) == null:
				Sfx.play_at("trident_pickup", body.pos, 0.6)
				queue_free()
				return true
			return false
		if stuck and kind in ["arrow", "spectral_arrow", "tipped_arrow"] and player.gamemode != Player.CREATIVE:
			if body.pos.distance_squared_to(player.body.pos) < 2.0:
				var ground := world.get_id(floori(body.pos.x), floori(body.pos.y) - 1, floori(body.pos.z))
				if player.inventory.add(ItemStack.of(kind, 1), 0, 36) == null:
					queue_free()
					return true
		return false

	func on_death(_c: String, _k) -> void:
		queue_free()


# ================================================================================================
## Boat (wooden), chest boat and minecart variants.
class Vehicle extends Entity:
	var kind := "boat"
	var wood := "oak"
	var rider = null
	var rider_input: Dictionary = {}
	var water_only := false
	var chest := false
	var damage_taken := 0.0

	func _init() -> void:
		type_name = "vehicle"
		body.width = 1.375
		body.height = 0.5625
		gravity = 0.04

	func setup_vehicle(w: World, p_kind: String, pos: Vector3, props: Dictionary) -> void:
		setup(w, pos)
		kind = p_kind
		wood = String(props.get("wood", "oak"))
		chest = p_kind == "chest_boat" or p_kind == "chest_minecart"
		water_only = p_kind.ends_with("minecart")
		if is_cart():
			body.width = 0.98
			body.height = 0.7
		var mesh := MobRenderer.vehicle_mesh(kind, wood, chest)
		if mesh != null:
			visual.add_child(mesh)
		visual.position = Vector3(0, 0, 0)

	func is_cart() -> bool:
		return kind.ends_with("minecart")

	## Item dropped when the vehicle is broken.
	func item_name() -> String:
		if is_cart():
			return kind
		if wood == "bamboo":
			return "bamboo_chest_raft" if chest else "bamboo_raft"
		return wood + ("_chest_boat" if chest else "_boat")

	func tick() -> void:
		age += 1
		if invuln > 0:
			invuln -= 1
		if hurt_time > 0:
			hurt_time -= 1
		damage_taken = maxf(0.0, damage_taken - 0.05)
		prev_pos = body.pos
		prev_facing = facing
		var inp: Vector2 = rider_input.get("input", Vector2.ZERO) if rider != null else Vector2.ZERO
		var ryaw: float = float(rider_input.get("yaw", facing))
		rider_input = {}
		if is_cart():
			_cart_tick(inp, ryaw)
		else:
			_boat_tick(inp)
		if body.pos.y < world.min_y - 64:
			if rider != null:
				Riding.dismount(rider)
			queue_free()

	## Boats: float on the water surface, paddle forward/back, turn with A/D; crawl on land,
	## glide on ice.
	func _boat_tick(inp: Vector2) -> void:
		var top := _water_top()
		var on_water := top > -1e8 and body.pos.y < top + 0.05
		if rider != null:
			facing += -inp.x * 0.065
			var fwd := Vector3(-sin(facing), 0, -cos(facing))
			var acc := 0.0
			if inp.y > 0.1:
				acc = 0.04
			elif inp.y < -0.1:
				acc = -0.005
			elif absf(inp.x) > 0.1:
				acc = 0.005
			body.vel += fwd * acc
		var fr := 0.9
		if not on_water and body.on_ground:
			var below := BlockDB.name_of(world.get_block(floori(body.pos.x), floori(body.pos.y - 0.2), floori(body.pos.z)))
			fr = 0.98 if below.contains("ice") else 0.45
		body.vel.x *= fr
		body.vel.z *= fr
		if on_water:
			var target := top - 0.35
			if body.pos.y < target:
				body.vel.y = minf(body.vel.y + 0.05, 0.12)
			else:
				body.vel.y -= 0.02
			body.vel.y *= 0.7
		else:
			body.vel.y -= 0.04
			body.vel.y *= 0.98
		body.move(world, body.vel)
		on_ground = body.on_ground
		if on_ground and body.vel.y < 0.0:
			body.vel.y = 0.0

	## Surface height of the water the hull is in (-INF when none).
	func _water_top() -> float:
		var x := floori(body.pos.x)
		var z := floori(body.pos.z)
		var y0 := floori(body.pos.y)
		for dy in [1, 0, -1]:
			var y: int = y0 + dy
			var id := world.get_id(x, y, z)
			if BlockDB.fluid[id] == 1 and BlockDB.name_of(id).contains("water") and BlockDB.name_of(world.get_id(x, y + 1, z)) != "water":
				return float(y) + 0.9
		return -INF

	## Minecarts: follow rails (see Rails), otherwise roll to a stop like a heavy block.
	func _cart_tick(inp: Vector2, ryaw: float) -> void:
		var rp := Rails.rail_under(world, body.pos)
		if rp != Rails.NONE:
			Rails.move_cart(self, rp, inp, ryaw)
			on_ground = true
			if kind == "tnt_minecart":
				var v := world.get_block(rp.x, rp.y, rp.z)
				if String((BlockDB.defs[v & 0xFFF] as BlockDef).props.get("rail", "")) == "activator" and ((v >> 12) & 8) != 0:
					explode_cart()
			return
		body.vel.y -= 0.04
		var fr := 0.5 if body.on_ground else 0.95
		body.vel.x *= fr
		body.vel.z *= fr
		body.move(world, body.vel)
		on_ground = body.on_ground
		if on_ground and body.vel.y < 0.0:
			body.vel.y = 0.0

	func explode_cart() -> void:
		if dead:
			return
		dead = true
		if rider != null:
			Riding.dismount(rider)
		Explosions.explode(world, body.pos + Vector3(0, 0.5, 0), 4.0, false, self)
		queue_free()

	func can_ride() -> bool:
		return rider == null

	func interact(player, held: ItemStack) -> bool:
		if player.sneaking or rider != null or player.riding != null:
			return false
		if kind in ["furnace_minecart", "tnt_minecart", "hopper_minecart"]:
			return false
		return Riding.mount(player, self)

	func ride(player) -> void:
		Riding.mount(player, self)

	func dismount() -> void:
		if rider != null:
			Riding.dismount(rider)

	## Hits break the vehicle (creative: at once, no drop).
	func hurt(amount: float, cause: String, attacker = null, _dir := Vector2.ZERO, _kb := 0.0) -> float:
		if dead:
			return 0.0
		if cause == "explosion" and kind == "tnt_minecart":
			explode_cart()
			return amount
		hurt_time = 10
		flash = 1.0
		damage_taken += maxf(amount, 1.0) * 10.0
		var creative: bool = attacker != null and attacker is Player and (attacker as Player).is_creative()
		if damage_taken > 40.0 or creative or cause in ["explosion", "lava", "void"]:
			dead = true
			if rider != null:
				Riding.dismount(rider)
			if not creative and session != null and cause != "void":
				session.entities.spawn_item(world, body.pos + Vector3(0, 0.3, 0), ItemStack.of(item_name(), 1))
			Sfx.play_at("wood_break" if not is_cart() else "metal_break", body.pos, 0.6)
			queue_free()
		return amount

	func on_death(_c: String, _k) -> void:
		dismount()
		queue_free()
