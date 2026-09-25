class_name ChunkManager
extends RefCounted
## Streams chunk columns around a focus point: generation and meshing run on WorkerThreadPool,
## lighting on a dedicated LightEngine thread, mesh upload on the main thread with a time budget.
## Section meshes are drawn through RenderingServer instances (no scene nodes per section).

signal chunk_ready(c: Chunk)          # generated + lit (entities can be spawned)
signal chunk_unloading(c: Chunk)

var gen: WorldGen
var light: LightEngine
var scenario: RID
var materials: Array = []             # [opaque, cutout, translucent] ShaderMaterial
var render_distance := 8
var max_gen_jobs := 6
var max_mesh_jobs := 6
var upload_budget_ms := 4.0
var fancy_leaves := true

var chunks: Dictionary = {}           # Vector2i -> Chunk
var _gen_jobs: Dictionary = {}        # Vector2i -> [task_id, Chunk]
var _mesh_jobs: Dictionary = {}       # Vector2i -> [task_id, Mesher.Job]
var _dirty: Dictionary = {}           # Vector2i -> Dictionary(sy -> true)
var _urgent: Dictionary = {}          # Vector2i -> true (edits near player)
var _render: Dictionary = {}          # Vector3i(cx, sy, cz) -> [instance RID, mesh RID]
var _uploads: Array = []              # finished Mesher.Job queue
var _be_nodes: Dictionary = {}        # Vector2i -> Array of block entity instances (chests...)
var center := Vector2i(1 << 30, 0)
var _needed_sorted: Array = []
var saved_loader: Callable            # func(cx, cz) -> Dictionary or null (saved chunk data)
var block_entity_hook: Callable       # func(chunk_key, Array[[Vector3i, value]]) for block entity renderers
var stats := {"generated": 0, "meshed": 0, "uploaded": 0}
var sky_enabled := true
var shown := true                     # false while the dimension is inactive (instances hidden)


func setup(generator: WorldGen, world_scenario: RID, mats: Array) -> void:
	gen = generator
	scenario = world_scenario
	materials = mats
	light = LightEngine.new()
	light.setup(gen.min_y, gen.height, gen.has_sky)
	light.start()
	var cores := OS.get_processor_count()
	max_gen_jobs = clampi(cores / 2, 2, 8)
	max_mesh_jobs = clampi(cores / 2, 2, 8)


func shutdown() -> void:
	# wait for running jobs so worker threads do not touch freed data
	for k in _gen_jobs:
		WorkerThreadPool.wait_for_task_completion(_gen_jobs[k][0])
	for k in _mesh_jobs:
		WorkerThreadPool.wait_for_task_completion(_mesh_jobs[k][0])
	_gen_jobs.clear()
	_mesh_jobs.clear()
	if light != null:
		light.stop()
	for k in _render:
		var r: Array = _render[k]
		RenderingServer.free_rid(r[0])
		RenderingServer.free_rid(r[1])
	_render.clear()
	chunks.clear()


func get_chunk(cx: int, cz: int) -> Chunk:
	var c: Chunk = chunks.get(Vector2i(cx, cz))
	if c != null and c.state >= Chunk.S_GENERATED:
		return c
	return null


func is_loaded_at(x: int, z: int) -> bool:
	return get_chunk(x >> 4, z >> 4) != null


func loaded_count() -> int:
	return chunks.size()


func pending_work() -> int:
	return _gen_jobs.size() + _mesh_jobs.size() + _uploads.size() + light.pending()


## Marks a section (and neighbours for border cells) for re-meshing.
func mark_dirty(cx: int, sy: int, cz: int, urgent := false) -> void:
	if sy < 0 or sy >= gen.height >> 4:
		return
	var k := Vector2i(cx, cz)
	if not chunks.has(k):
		return
	var d: Dictionary = _dirty.get(k, {})
	d[sy] = true
	_dirty[k] = d
	if urgent:
		_urgent[k] = true


func mark_block_dirty(x: int, y: int, z: int, urgent := true) -> void:
	var cx := x >> 4
	var cz := z >> 4
	var yr := y - gen.min_y
	var sy := yr >> 4
	var lx := x & 15
	var lz := z & 15
	var ly := yr & 15
	mark_dirty(cx, sy, cz, urgent)
	if ly == 0:
		mark_dirty(cx, sy - 1, cz, urgent)
	elif ly == 15:
		mark_dirty(cx, sy + 1, cz, urgent)
	var dxs := [0]
	var dzs := [0]
	if lx == 0:
		dxs.append(-1)
	elif lx == 15:
		dxs.append(1)
	if lz == 0:
		dzs.append(-1)
	elif lz == 15:
		dzs.append(1)
	for dx in dxs:
		for dz in dzs:
			if dx != 0 or dz != 0:
				mark_dirty(cx + dx, sy, cz + dz, urgent)
				if ly == 0:
					mark_dirty(cx + dx, sy - 1, cz + dz, urgent)
				elif ly == 15:
					mark_dirty(cx + dx, sy + 1, cz + dz, urgent)


func update(focus: Vector3) -> void:
	var fc := Vector2i(floori(focus.x / 16.0), floori(focus.z / 16.0))
	if fc != center:
		center = fc
		_rebuild_needed()
		_unload_far()
	_collect_gen()
	_poll_light()
	_schedule_gen()
	_schedule_mesh()
	_collect_mesh()
	_upload()


func _rebuild_needed() -> void:
	var R := render_distance + 2
	var list := []
	for dz in range(-R, R + 1):
		for dx in range(-R, R + 1):
			var d2 := dx * dx + dz * dz
			if d2 > (R + 0.5) * (R + 0.5):
				continue
			list.append([d2, Vector2i(center.x + dx, center.y + dz)])
	list.sort_custom(func(a, b): return a[0] < b[0])
	_needed_sorted = []
	for e in list:
		_needed_sorted.append(e[1])


func _in_range(k: Vector2i, extra: int) -> bool:
	var dx := k.x - center.x
	var dz := k.y - center.y
	var R := render_distance + extra
	return dx * dx + dz * dz <= (R + 0.5) * (R + 0.5)


func _schedule_gen() -> void:
	if _gen_jobs.size() >= max_gen_jobs:
		return
	for k in _needed_sorted:
		if _gen_jobs.size() >= max_gen_jobs:
			break
		if chunks.has(k):
			continue
		var c := Chunk.new(k.x, k.y, gen.min_y, gen.height)
		c.state = Chunk.S_GENERATING
		chunks[k] = c
		var saved = null
		if saved_loader.is_valid():
			saved = saved_loader.call(k.x, k.y)
		var tid := WorkerThreadPool.add_task(_gen_task.bind(c, saved), false, "chunk gen")
		_gen_jobs[k] = [tid, c]


func _gen_task(c: Chunk, saved) -> void:
	if saved is Dictionary:
		var raw: PackedByteArray = (saved["blocks"] as PackedByteArray).decompress(int(saved["size"]), FileAccess.COMPRESSION_ZSTD)
		var arr := raw.to_int32_array()
		if saved.has("remap"):
			SaveManager.apply_remap(arr, saved["remap"])
		if arr.size() == c.blocks.size():
			c.blocks = arr
			c.block_entities = saved.get("be", {})
			c.pending_entities = saved.get("ents", [])
			if saved.has("biomes"):
				c.biomes = saved["biomes"]
			c.loaded_from_save = true
			c.modified = true
			# colours are recomputed from the generator's biome data
			_recolor(c)
			c.rebuild_derived()
			return
	gen.generate(c)


func _recolor(c: Chunk) -> void:
	var grass := BiomeDB.grass
	var foliage := BiomeDB.foliage
	var water := BiomeDB.water
	for i in 256:
		var b := c.biomes[i]
		c.colors[i * 3] = grass[b]
		c.colors[i * 3 + 1] = foliage[b]
		c.colors[i * 3 + 2] = water[b]


func _collect_gen() -> void:
	var done := []
	for k in _gen_jobs:
		var e: Array = _gen_jobs[k]
		if WorkerThreadPool.is_task_completed(e[0]):
			WorkerThreadPool.wait_for_task_completion(e[0])
			done.append(k)
	for k in done:
		var c: Chunk = _gen_jobs[k][1]
		_gen_jobs.erase(k)
		if chunks.get(k) != c:
			continue
		if not _in_range(k, 3):
			chunks.erase(k)
			continue
		c.state = Chunk.S_GENERATED
		stats.generated += 1
		light.add_chunk(c)


func _poll_light() -> void:
	var r := light.poll()
	for k in r.lit:
		var c: Chunk = chunks.get(k)
		if c == null:
			continue
		c.state = Chunk.S_LIT
		chunk_ready.emit(c)
		# mesh this chunk and let lit neighbours refresh their borders
		var all := {}
		for s in gen.height >> 4:
			all[s] = true
		_dirty[k] = all
		for dz in range(-1, 2):
			for dx in range(-1, 2):
				if dx == 0 and dz == 0:
					continue
				var nk := Vector2i(k.x + dx, k.y + dz)
				var nc: Chunk = chunks.get(nk)
				if nc != null and nc.meshed:
					# border faces of neighbours may now be culled / relit
					for s in gen.height >> 4:
						if not nc.section_is_empty(s):
							mark_dirty(nk.x, s, nk.y)
	for v in r.dirty:
		var vk := Vector3i(v)
		mark_dirty(vk.x, vk.y, vk.z)


func _neighbors_ready(k: Vector2i) -> bool:
	for dz in range(-1, 2):
		for dx in range(-1, 2):
			var c: Chunk = chunks.get(Vector2i(k.x + dx, k.y + dz))
			if c == null or c.state < Chunk.S_GENERATED:
				return false
	var cc: Chunk = chunks.get(k)
	return cc.state >= Chunk.S_LIT


func _schedule_mesh() -> void:
	if _dirty.is_empty():
		return
	var keys := _dirty.keys()
	# urgent first, then by distance
	keys.sort_custom(func(a, b):
		var ua := _urgent.has(a)
		var ub := _urgent.has(b)
		if ua != ub:
			return ua
		return (a - center).length_squared() < (b - center).length_squared())
	for k in keys:
		if _mesh_jobs.size() >= max_mesh_jobs and not _urgent.has(k):
			break
		if _mesh_jobs.has(k):
			continue
		if not _in_range(k, 0):
			_dirty.erase(k)
			continue
		if not _neighbors_ready(k):
			continue
		var job := Mesher.Job.new()
		job.cx = k.x
		job.cz = k.y
		job.min_y = gen.min_y
		job.height = gen.height
		job.fancy_leaves = fancy_leaves
		job.chunks.resize(9)
		for dz in 3:
			for dx in 3:
				job.chunks[dx + dz * 3] = chunks.get(Vector2i(k.x + dx - 1, k.y + dz - 1))
		var secs := PackedInt32Array()
		for s in _dirty[k]:
			secs.append(s)
		secs.sort()
		job.sections = secs
		var c: Chunk = chunks[k]
		c.version += 1
		job.version = c.version
		_dirty.erase(k)
		var high := _urgent.has(k)
		_urgent.erase(k)
		var tid := WorkerThreadPool.add_task(Mesher.run_job.bind(job), high, "chunk mesh")
		_mesh_jobs[k] = [tid, job]


func _collect_mesh() -> void:
	var done := []
	for k in _mesh_jobs:
		var e: Array = _mesh_jobs[k]
		if WorkerThreadPool.is_task_completed(e[0]):
			WorkerThreadPool.wait_for_task_completion(e[0])
			done.append(k)
	for k in done:
		var job: Mesher.Job = _mesh_jobs[k][1]
		_mesh_jobs.erase(k)
		_uploads.append(job)
		stats.meshed += 1


func _upload() -> void:
	if _uploads.is_empty():
		return
	var t0 := Time.get_ticks_usec()
	var budget := upload_budget_ms * 1000.0
	while not _uploads.is_empty():
		var job: Mesher.Job = _uploads.pop_front()
		var k := Vector2i(job.cx, job.cz)
		var c: Chunk = chunks.get(k)
		if c == null:
			continue
		for sy in job.results:
			_apply_section(k, sy, job.results[sy])
		c.meshed = true
		if block_entity_hook.is_valid():
			block_entity_hook.call(k, job.block_entities, job.sections)
		stats.uploaded += 1
		if Time.get_ticks_usec() - t0 > budget:
			break


func _apply_section(k: Vector2i, sy: int, surfaces: Array) -> void:
	var rk := Vector3i(k.x, sy, k.y)
	var old: Array = _render.get(rk, [])
	var any := false
	for sf in surfaces:
		if not (sf as MeshSurface).is_empty():
			any = true
			break
	if not any:
		if not old.is_empty():
			RenderingServer.free_rid(old[0])
			RenderingServer.free_rid(old[1])
			_render.erase(rk)
		return
	var mesh := RenderingServer.mesh_create()
	var si := 0
	for r in 3:
		var sf: MeshSurface = surfaces[r]
		if sf.is_empty():
			continue
		RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, sf.to_arrays(), [], {},
			MeshSurface.format_flags())
		RenderingServer.mesh_surface_set_material(mesh, si, (materials[r] as Material).get_rid())
		si += 1
	var inst: RID
	if old.is_empty():
		inst = RenderingServer.instance_create()
		RenderingServer.instance_set_scenario(inst, scenario)
		RenderingServer.instance_geometry_set_cast_shadows_setting(inst, RenderingServer.SHADOW_CASTING_SETTING_OFF)
		RenderingServer.instance_set_transform(inst, Transform3D(Basis(), Vector3(k.x * 16, 0, k.y * 16)))
		RenderingServer.instance_set_visible(inst, shown)
	else:
		inst = old[0]
	RenderingServer.instance_set_base(inst, mesh)
	if not old.is_empty():
		RenderingServer.free_rid(old[1])
	_render[rk] = [inst, mesh]


func _unload_far() -> void:
	var drop := []
	for k in chunks:
		if not _in_range(k, 3) and not _gen_jobs.has(k) and not _mesh_jobs.has(k):
			drop.append(k)
	for k in drop:
		var c: Chunk = chunks[k]
		chunk_unloading.emit(c)
		chunks.erase(k)
		_dirty.erase(k)
		light.remove_chunk(k)
		for sy in gen.height >> 4:
			var rk := Vector3i(k.x, sy, k.y)
			if _render.has(rk):
				var r: Array = _render[rk]
				RenderingServer.free_rid(r[0])
				RenderingServer.free_rid(r[1])
				_render.erase(rk)
		if block_entity_hook.is_valid():
			block_entity_hook.call(k, [], PackedInt32Array(), true)


## Forces every loaded chunk to be re-meshed (settings change, reload).
func remesh_all() -> void:
	for k in chunks:
		var c: Chunk = chunks[k]
		if c.state >= Chunk.S_LIT:
			var all := {}
			for s in gen.height >> 4:
				all[s] = true
			_dirty[k] = all


func set_visible(v: bool) -> void:
	shown = v
	for rk in _render:
		RenderingServer.instance_set_visible(_render[rk][0], v)
