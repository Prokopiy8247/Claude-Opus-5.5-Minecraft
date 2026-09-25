class_name StructureManager
extends RefCounted
## Deterministic structure placement (region grid with spacing/separation like Minecraft) and a
## thread-safe cache of generated structure layouts. Each chunk pulls the blocks of every structure
## whose bounding box intersects it.

var seed_value := 0
var dim := 0
var gen: WorldGen = null
var _mutex := Mutex.new()
var _cache: Dictionary = {}      # start key -> StructureLayout
var types: Array = []            # Array[StructureType]


func setup(world_seed: int, dimension: int, generator: WorldGen) -> void:
	seed_value = world_seed
	dim = dimension
	gen = generator
	types = StructureRegistry.types_for(dimension)


## Called from generator worker threads.
func generate_into(c: Chunk, g: WorldGen) -> void:
	if types.is_empty():
		return
	var w := WorldGen.Writer.new(c)
	for t in types:
		var st: StructureType = t
		var reach := st.max_radius_chunks
		var sp := st.spacing
		var rx0 := floori(float(c.cx - reach) / sp)
		var rx1 := floori(float(c.cx + reach) / sp)
		var rz0 := floori(float(c.cz - reach) / sp)
		var rz1 := floori(float(c.cz + reach) / sp)
		for rz in range(rz0, rz1 + 1):
			for rx in range(rx0, rx1 + 1):
				var start := st.start_chunk(seed_value, rx, rz)
				if start == Vector2i(2147483647, 0):
					continue
				if absi(start.x - c.cx) > reach or absi(start.y - c.cz) > reach:
					continue
				var layout := get_layout(st, start, g)
				if layout == null or layout.empty:
					continue
				if not layout.intersects_chunk(c.cx, c.cz):
					continue
				layout.write_chunk(w)


func get_layout(st: StructureType, start: Vector2i, g: WorldGen) -> StructureLayout:
	var key := "%s:%d:%d" % [st.name, start.x, start.y]
	_mutex.lock()
	var l: StructureLayout = _cache.get(key)
	_mutex.unlock()
	if l != null:
		return l
	l = st.build(seed_value, start, g)
	_mutex.lock()
	if _cache.has(key):
		l = _cache[key]
	else:
		_cache[key] = l
		if _cache.size() > 600:
			_cache.clear()
			_cache[key] = l
	_mutex.unlock()
	return l


## Name of the generated structure whose bounding box contains p ("" when none).
func structure_name_at(_w, p: Vector3i) -> String:
	_mutex.lock()
	var found := ""
	for k in _cache:
		var l: StructureLayout = _cache[k]
		if l != null and l.contains(p):
			found = l.name
			break
	_mutex.unlock()
	return found


## Nearest structure start of a type (for /locate and eyes of ender). Returns block position or INF.
func locate(type_name: String, from: Vector3, max_regions := 24) -> Vector3:
	for t in types:
		var st: StructureType = t
		if st.name != type_name:
			continue
		var cx := floori(from.x / 16.0)
		var cz := floori(from.z / 16.0)
		var best := Vector3.INF
		var bestd := 1e18
		var r0x := floori(float(cx) / st.spacing)
		var r0z := floori(float(cz) / st.spacing)
		for r in max_regions:
			for dz in range(-r, r + 1):
				for dx in range(-r, r + 1):
					if absi(dx) != r and absi(dz) != r:
						continue
					var s := st.start_chunk(seed_value, r0x + dx, r0z + dz)
					if s == Vector2i(2147483647, 0):
						continue
					var p := st.locate_point(seed_value, s, gen)
					if p == Vector3.INF:
						continue
					var d := Vector2(p.x - from.x, p.z - from.z).length_squared()
					if d < bestd:
						bestd = d
						best = p
			if best != Vector3.INF and r >= 2:
				return best
		return best
	return Vector3.INF
