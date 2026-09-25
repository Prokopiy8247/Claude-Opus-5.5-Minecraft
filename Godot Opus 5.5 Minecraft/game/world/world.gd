class_name World
extends Node3D
## One loaded dimension: chunk streaming, block access/modification API, voxel ray casts,
## scheduled + random block ticks and the entity container.

signal block_changed(pos: Vector3i, old_v: int, new_v: int)

const F_NOTIFY := 1        # notify neighbours (physics, redstone, support checks)
const F_DROP_BE := 2       # remove block entity data
const F_URGENT := 4        # re-mesh with priority (player edits)
const F_DEFAULT := 7

var dim := 0
var dim_def: Dictionary
var gen: WorldGen
var cm: ChunkManager
var min_y := 0
var height := 384
var max_y := 319
var seed_value := 0
var session = null
var entities: Node3D
var be_renderers: Node3D
var materials: Array = []
var save_dir := ""
var scheduled: Dictionary = {}        # Vector3i -> due tick
var tick_count := 0
var random_tick_speed := 3
var random_tick_radius := 5
var _rng := RandomNumberGenerator.new()
var _be_nodes: Dictionary = {}        # Vector2i -> Dictionary(Vector3i -> Node3D)
var focus := Vector3.ZERO
var paused_streaming := false
var rs_state: Dictionary = {}         # Vector3i -> bool (last redstone power seen by edge-triggered blocks)


func setup(p_session, p_dim: int, p_seed: int, p_save_dir: String) -> void:
	session = p_session
	dim = p_dim
	dim_def = DimensionDB.get_def(dim)
	seed_value = p_seed
	save_dir = p_save_dir
	name = "World_%s" % dim_def.name
	gen = DimensionDB.make_generator(dim, p_seed)
	min_y = gen.min_y
	height = gen.height
	max_y = min_y + height - 1
	entities = Node3D.new()
	entities.name = "Entities"
	add_child(entities)
	be_renderers = Node3D.new()
	be_renderers.name = "BlockEntityRenderers"
	add_child(be_renderers)
	_make_materials()
	cm = ChunkManager.new()
	cm.setup(gen, get_world_3d().scenario if is_inside_tree() else RID(), materials)
	cm.render_distance = int(Game.settings.render_distance)
	cm.fancy_leaves = bool(Game.settings.fancy_leaves)
	cm.saved_loader = _load_saved_chunk
	cm.block_entity_hook = _on_block_entities
	cm.chunk_ready.connect(_on_chunk_ready)
	cm.chunk_unloading.connect(_on_chunk_unloading)
	_rng.seed = p_seed ^ 0x5eed


func _ready() -> void:
	if cm != null and not cm.scenario.is_valid():
		cm.scenario = get_world_3d().scenario


func _make_materials() -> void:
	materials.clear()
	for shader_path in ["res://game/world/shaders/voxel_opaque.gdshader", "res://game/world/shaders/voxel_cutout.gdshader",
			"res://game/world/shaders/voxel_translucent.gdshader"]:
		var m := ShaderMaterial.new()
		m.shader = load(shader_path)
		m.set_shader_parameter("blocks", TextureLibrary.block_array)
		materials.append(m)
	(materials[2] as ShaderMaterial).render_priority = 1


func shutdown() -> void:
	if cm != null:
		cm.shutdown()


func _notification(what: int) -> void:
	# leaving the world (title screen, quit): stop the light thread and worker jobs and free the
	# section instances, otherwise they outlive the world and its materials
	if what == NOTIFICATION_PREDELETE:
		shutdown()


func _process(_delta: float) -> void:
	if cm != null and not paused_streaming:
		cm.update(focus)


# ------------------------------------------------------------------------------------------------
# Block access
func get_block(x: int, y: int, z: int) -> int:
	if y < min_y or y > max_y:
		return 0
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c == null or c.state < Chunk.S_GENERATED:
		return 0
	return c.blocks[((y - min_y) << 8) | ((z & 15) << 4) | (x & 15)]


func get_blockv(p: Vector3i) -> int:
	return get_block(p.x, p.y, p.z)


func get_id(x: int, y: int, z: int) -> int:
	return get_block(x, y, z) & 0xFFF


func is_loaded(x: int, z: int) -> bool:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	return c != null and c.state >= Chunk.S_GENERATED


func is_ready_at(x: int, z: int) -> bool:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	return c != null and c.state >= Chunk.S_LIT


func chunk_at(x: int, z: int) -> Chunk:
	return cm.get_chunk(x >> 4, z >> 4)


func get_light(x: int, y: int, z: int) -> Vector2i:
	if y > max_y:
		return Vector2i(15 if gen.has_sky else 0, 0)
	if y < min_y:
		return Vector2i.ZERO
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c == null or not c.lit:
		return Vector2i(15 if gen.has_sky else 0, 0)
	var l := c.light[((y - min_y) << 8) | ((z & 15) << 4) | (x & 15)]
	return Vector2i(l >> 4, l & 15)


## Combined light level as Minecraft computes it for spawning/burning (sky reduced by time of day).
func light_level(x: int, y: int, z: int, sky_darken := 0) -> int:
	var l := get_light(x, y, z)
	return maxi(l.x - sky_darken, l.y)


func top_y(x: int, z: int) -> int:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c == null:
		return gen.surface_height(x, z)
	return c.heightmap[((z & 15) << 4) | (x & 15)]


## Highest block that has collision (for spawning / teleports).
func top_solid_y(x: int, z: int) -> int:
	var y := top_y(x, z)
	while y > min_y:
		var v := get_block(x, y, z)
		if v != 0 and BlockDB.solid[v & 0xFFF] == 1:
			return y
		y -= 1
	return min_y


func biome_at(x: int, y: int, z: int) -> int:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if dim == 0:
		return gen.biome_at_3d(x, y, z)
	if c != null:
		return c.biomes[((z & 15) << 4) | (x & 15)]
	return gen.biome_at(x, z)


func surface_biome(x: int, z: int) -> int:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c != null:
		return c.biomes[((z & 15) << 4) | (x & 15)]
	return gen.biome_at(x, z)


## Sets a block. Returns false when the target chunk is not loaded.
func set_block(x: int, y: int, z: int, v: int, flags: int = F_DEFAULT) -> bool:
	if y < min_y or y > max_y:
		return false
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c == null or c.state < Chunk.S_GENERATED:
		return false
	var i := ((y - min_y) << 8) | ((z & 15) << 4) | (x & 15)
	var old := c.blocks[i]
	if old == v:
		return true
	c.blocks[i] = v
	c.modified = true
	if (flags & F_DROP_BE) != 0 and c.block_entities.has(i) and (old & 0xFFF) != (v & 0xFFF):
		c.block_entities.erase(i)
	var oid := old & 0xFFF
	var nid := v & 0xFFF
	var op := BlockDB.light_opacity
	if op[oid] != op[nid] or BlockDB.emission(old) != BlockDB.emission(v):
		cm.light.block_changed(x, y, z, old, v)
	cm.mark_block_dirty(x, y, z, (flags & F_URGENT) != 0)
	if (flags & F_NOTIFY) != 0:
		_notify_neighbors(x, y, z, old, v)
	block_changed.emit(Vector3i(x, y, z), old, v)
	return true


func set_blockv(p: Vector3i, v: int, flags: int = F_DEFAULT) -> bool:
	return set_block(p.x, p.y, p.z, v, flags)


func _notify_neighbors(x: int, y: int, z: int, old_v: int, new_v: int) -> void:
	if BlockBehaviors.enabled:
		BlockBehaviors.on_changed(self, x, y, z, old_v, new_v)
		for d in 6:
			var nx: int = x + Vox.DIR_X[d]
			var ny: int = y + Vox.DIR_Y[d]
			var nz: int = z + Vox.DIR_Z[d]
			var nv := get_block(nx, ny, nz)
			if nv != 0 or d == Vox.UP:
				BlockBehaviors.neighbor_changed(self, nx, ny, nz, nv, Vox.OPPOSITE[d])


# ------------------------------------------------------------------------------------------------
# Block entities (containers, furnaces, signs...)
func get_be(x: int, y: int, z: int, create := false) -> Dictionary:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c == null:
		return {}
	var i := ((y - min_y) << 8) | ((z & 15) << 4) | (x & 15)
	if c.block_entities.has(i):
		return c.block_entities[i]
	if create:
		var d := {}
		c.block_entities[i] = d
		c.modified = true
		return d
	return {}


func has_be(x: int, y: int, z: int) -> bool:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c == null:
		return false
	return c.block_entities.has(((y - min_y) << 8) | ((z & 15) << 4) | (x & 15))


func set_be(x: int, y: int, z: int, data: Dictionary) -> void:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c == null:
		return
	c.block_entities[((y - min_y) << 8) | ((z & 15) << 4) | (x & 15)] = data
	c.modified = true


func remove_be(x: int, y: int, z: int) -> Dictionary:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c == null:
		return {}
	var i := ((y - min_y) << 8) | ((z & 15) << 4) | (x & 15)
	var d: Dictionary = c.block_entities.get(i, {})
	c.block_entities.erase(i)
	c.modified = true
	return d


func mark_modified(x: int, z: int) -> void:
	var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
	if c != null:
		c.modified = true


# ------------------------------------------------------------------------------------------------
## Voxel ray cast against selection shapes. Returns {} on miss or
## {pos: Vector3i, normal: Vector3i, face: int, point: Vector3, block: int, dist: float}.
func raycast(origin: Vector3, dir: Vector3, max_dist: float, fluids := false, ignore_non_solid := false) -> Dictionary:
	if dir.length_squared() < 1e-9:
		return {}
	dir = dir.normalized()
	var cell := Vector3i(floori(origin.x), floori(origin.y), floori(origin.z))
	var step := Vector3i(1 if dir.x > 0 else -1, 1 if dir.y > 0 else -1, 1 if dir.z > 0 else -1)
	var tdelta := Vector3(absf(1.0 / dir.x) if dir.x != 0.0 else 1e30, absf(1.0 / dir.y) if dir.y != 0.0 else 1e30,
		absf(1.0 / dir.z) if dir.z != 0.0 else 1e30)
	var tmax := Vector3(
		((cell.x + (1 if step.x > 0 else 0)) - origin.x) / dir.x if dir.x != 0.0 else 1e30,
		((cell.y + (1 if step.y > 0 else 0)) - origin.y) / dir.y if dir.y != 0.0 else 1e30,
		((cell.z + (1 if step.z > 0 else 0)) - origin.z) / dir.z if dir.z != 0.0 else 1e30)
	var t := 0.0
	var guard := 0
	while t <= max_dist and guard < 512:
		guard += 1
		var v := get_block(cell.x, cell.y, cell.z)
		if v != 0:
			var id := v & 0xFFF
			var boxes: Array
			if BlockDB.fluid[id] != 0:
				if fluids and ((v >> 12) & 15) == 0:
					boxes = [AABB(Vector3.ZERO, Vector3(1, 0.875, 1))]
				else:
					boxes = []
			elif ignore_non_solid and BlockDB.solid[id] == 0:
				boxes = []
			else:
				boxes = BlockShapes.selection(v, self, cell.x, cell.y, cell.z)
			var best := 1e30
			var bn := Vector3i.ZERO
			for b in boxes:
				var box: AABB = b
				var hit := _ray_box(origin, dir, Vector3(cell) + box.position, Vector3(cell) + box.end)
				if hit.x >= 0.0 and hit.x < best:
					best = hit.x
					bn = Vector3i(int(hit.y), int(hit.z), int(hit.w))
			if best <= max_dist and best < 1e29:
				var face := 0
				for f in 6:
					if Vox.DIR_VEC[f] == bn:
						face = f
				return {"pos": cell, "normal": bn, "face": face, "point": origin + dir * best, "block": v, "dist": best}
		if tmax.x < tmax.y and tmax.x < tmax.z:
			t = tmax.x
			tmax.x += tdelta.x
			cell.x += step.x
		elif tmax.y < tmax.z:
			t = tmax.y
			tmax.y += tdelta.y
			cell.y += step.y
		else:
			t = tmax.z
			tmax.z += tdelta.z
			cell.z += step.z
	return {}


## Slab test. Returns Vector4(t, nx, ny, nz) or t < 0 on miss.
static func _ray_box(o: Vector3, d: Vector3, mn: Vector3, mx: Vector3) -> Vector4:
	var tmin := -1e30
	var tmaxv := 1e30
	var n := Vector3.ZERO
	for a in 3:
		var oa := o[a]
		var da := d[a]
		if absf(da) < 1e-9:
			if oa < mn[a] or oa > mx[a]:
				return Vector4(-1, 0, 0, 0)
			continue
		var t1 := (mn[a] - oa) / da
		var t2 := (mx[a] - oa) / da
		var sgn := -1.0
		if t1 > t2:
			var tmp := t1
			t1 = t2
			t2 = tmp
			sgn = 1.0
		if t1 > tmin:
			tmin = t1
			n = Vector3.ZERO
			n[a] = sgn
		tmaxv = minf(tmaxv, t2)
		if tmin > tmaxv:
			return Vector4(-1, 0, 0, 0)
	if tmaxv < 0.0:
		return Vector4(-1, 0, 0, 0)
	if tmin < 0.0:
		return Vector4(0.0, n.x, n.y, n.z)
	return Vector4(tmin, n.x, n.y, n.z)


## All collision boxes (world space) of blocks intersecting an AABB.
func collect_boxes(area: AABB, out: Array) -> void:
	var x0 := floori(area.position.x)
	var y0 := floori(area.position.y) - 1
	var z0 := floori(area.position.z)
	var x1 := floori(area.end.x)
	var y1 := floori(area.end.y)
	var z1 := floori(area.end.z)
	var solid := BlockDB.solid
	var fullt := BlockDB.full
	for x in range(x0, x1 + 1):
		for z in range(z0, z1 + 1):
			var c: Chunk = cm.chunks.get(Vector2i(x >> 4, z >> 4))
			if c == null or c.state < Chunk.S_GENERATED:
				# unloaded chunks act as solid walls so nothing falls through the world edge
				out.append(AABB(Vector3(x, y0, z), Vector3(1, y1 - y0 + 1, 1)))
				continue
			var base := ((z & 15) << 4) | (x & 15)
			for y in range(maxi(y0, min_y), mini(y1, max_y) + 1):
				var v := c.blocks[((y - min_y) << 8) | base]
				if v == 0:
					continue
				var id := v & 0xFFF
				if solid[id] == 0:
					continue
				if fullt[id] == 1:
					out.append(AABB(Vector3(x, y, z), Vector3.ONE))
				else:
					for b in BlockShapes.collision(v, self, x, y, z):
						var box: AABB = b
						out.append(AABB(box.position + Vector3(x, y, z), box.size))
			if y0 < min_y and gen.min_y == min_y:
				pass


# ------------------------------------------------------------------------------------------------
# Ticks (called by the session at 20 TPS)
func schedule_tick(x: int, y: int, z: int, delay: int) -> void:
	var p := Vector3i(x, y, z)
	var due := tick_count + maxi(1, delay)
	if scheduled.has(p) and int(scheduled[p]) <= due:
		return
	scheduled[p] = due


func tick() -> void:
	tick_count += 1
	if not BlockBehaviors.enabled:
		return
	# scheduled ticks
	if not scheduled.is_empty():
		var due := []
		for p in scheduled:
			if int(scheduled[p]) <= tick_count:
				due.append(p)
		if due.size() > 600:
			due = due.slice(0, 600)
		for p in due:
			scheduled.erase(p)
			var pv: Vector3i = p
			if is_loaded(pv.x, pv.z):
				BlockBehaviors.scheduled_tick(self, pv.x, pv.y, pv.z, get_block(pv.x, pv.y, pv.z))
	# random ticks around the focus
	if random_tick_speed > 0:
		var fcx := floori(focus.x / 16.0)
		var fcz := floori(focus.z / 16.0)
		var R := random_tick_radius
		var tickable := BlockBehaviors.random_tickable
		for dz in range(-R, R + 1):
			for dx in range(-R, R + 1):
				var c: Chunk = cm.chunks.get(Vector2i(fcx + dx, fcz + dz))
				if c == null or c.state < Chunk.S_LIT:
					continue
				var blocks := c.blocks
				for s in c.sections:
					for k in random_tick_speed:
						var r := _rng.randi()
						var i := (s << 12) | (r & 4095)
						var v := blocks[i]
						if v != 0 and tickable[v & 0xFFF] == 1:
							var lx := i & 15
							var lz := (i >> 4) & 15
							var y := (i >> 8) + min_y
							BlockBehaviors.random_tick(self, c.cx * 16 + lx, y, c.cz * 16 + lz, v)


# ------------------------------------------------------------------------------------------------
# Save/load of modified chunks
func _chunk_path(cx: int, cz: int) -> String:
	return "%s/c.%d.%d.bin" % [save_dir, cx, cz]


func _load_saved_chunk(cx: int, cz: int):
	if save_dir == "":
		return null
	var p := _chunk_path(cx, cz)
	if not FileAccess.file_exists(p):
		return null
	var f := FileAccess.open(p, FileAccess.READ)
	if f == null:
		return null
	var data = f.get_var(false)
	f.close()
	if data is Dictionary:
		return SaveManager.remap_chunk(data)
	return null


func save_chunk(c: Chunk) -> void:
	if save_dir == "" or not c.modified:
		return
	DirAccess.make_dir_recursive_absolute(save_dir)
	var f := FileAccess.open(_chunk_path(c.cx, c.cz), FileAccess.WRITE)
	if f == null:
		push_error("Cannot write chunk file %s" % _chunk_path(c.cx, c.cz))
		return
	f.store_var(c.to_save_dict(), false)
	f.close()


## Saves every modified chunk. Live entities of the active dimension are written into their
## chunk's file too (they keep running); chunks whose saved entities have not been spawned yet
## keep their pending list.
func save_all() -> void:
	var live := {}
	if session != null and session.world == self and session.entities != null:
		live = session.entities.serialize_by_chunk(self)
	for k in cm.chunks:
		var c: Chunk = cm.chunks[k]
		if c.state < Chunk.S_GENERATED:
			continue
		if c.state >= Chunk.S_LIT and session != null and session.world == self:
			var ents: Array = live.get(k, [])
			if ents.is_empty() and not c.modified and not c.saved_entities:
				continue
			var keep := c.pending_entities
			c.pending_entities = ents
			c.modified = true
			save_chunk(c)
			c.pending_entities = keep
			c.saved_entities = not ents.is_empty()
		elif c.modified:
			save_chunk(c)


func _on_chunk_unloading(c: Chunk) -> void:
	if session != null and session.has_method("on_chunk_unloading"):
		session.on_chunk_unloading(self, c)
	if c.modified:
		save_chunk(c)
	var bn: Dictionary = _be_nodes.get(c.key(), {})
	for p in bn:
		var n: Node = bn[p]
		if is_instance_valid(n):
			n.queue_free()
	_be_nodes.erase(c.key())


func _on_chunk_ready(c: Chunk) -> void:
	if session != null and session.has_method("on_chunk_ready"):
		session.on_chunk_ready(self, c)


# ------------------------------------------------------------------------------------------------
# Block entity renderers (chests etc. are drawn as nodes instead of chunk geometry)
func _on_block_entities(k: Vector2i, list: Array, sections: PackedInt32Array, removed := false) -> void:
	var bn: Dictionary = _be_nodes.get(k, {})
	if removed:
		return
	var keep := {}
	for e in list:
		var lp: Vector3i = e[0]
		var wp := Vector3i(k.x * 16 + lp.x, lp.y, k.y * 16 + lp.z)
		keep[wp] = e[1]
	for p in bn.keys():
		var pv: Vector3i = p
		var sy := (pv.y - min_y) >> 4
		if not sections.has(sy):
			continue
		if not keep.has(pv) or (bn[p] as Node).get_meta("v", -1) != keep[pv]:
			(bn[p] as Node).queue_free()
			bn.erase(p)
	for wp in keep:
		if bn.has(wp):
			continue
		var node := BlockEntityRenderer.make(self, wp, keep[wp])
		if node != null:
			node.set_meta("v", keep[wp])
			be_renderers.add_child(node)
			node.global_position = Vector3(wp) + Vector3(0.5, 0, 0.5)
			bn[wp] = node
	_be_nodes[k] = bn


func be_node(p: Vector3i) -> Node3D:
	var bn: Dictionary = _be_nodes.get(Vector2i(p.x >> 4, p.z >> 4), {})
	return bn.get(p)
