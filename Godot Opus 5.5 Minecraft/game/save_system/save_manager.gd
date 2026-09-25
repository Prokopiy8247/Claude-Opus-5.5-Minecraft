class_name SaveManager
extends RefCounted
## World persistence: level metadata (seed, time, weather, game rules, player, dimension state) in
## level.json + per-dimension chunk deltas (only modified chunks are written). A block palette is
## stored so saves survive registry changes (ids are remapped on load).

const ROOT := "user://saves"

static var id_map := PackedInt32Array()      # saved id -> current id (empty = identity)


static func world_dir(name: String) -> String:
	return "%s/%s" % [ROOT, name.validate_filename()]


static func dim_dir(name: String, dim: int) -> String:
	return "%s/dim%d" % [world_dir(name), dim]


static func list_worlds() -> Array:
	var out := []
	DirAccess.make_dir_recursive_absolute(ROOT)
	var d := DirAccess.open(ROOT)
	if d == null:
		return out
	for n in d.get_directories():
		var info := read_level(n)
		if info.is_empty():
			continue
		info["folder"] = n
		out.append(info)
	out.sort_custom(func(a, b): return int(a.get("last_played", 0)) > int(b.get("last_played", 0)))
	return out


static func read_level(folder: String) -> Dictionary:
	var p := "%s/%s/level.json" % [ROOT, folder]
	if not FileAccess.file_exists(p):
		return {}
	var f := FileAccess.open(p, FileAccess.READ)
	if f == null:
		return {}
	var txt := f.get_as_text()
	f.close()
	var j = JSON.parse_string(txt)
	if j is Dictionary:
		return j
	return {}


static func write_level(folder: String, data: Dictionary) -> bool:
	DirAccess.make_dir_recursive_absolute(world_dir(folder))
	data["block_palette"] = palette()
	data["last_played"] = int(Time.get_unix_time_from_system())
	data["version"] = Game.VERSION
	var tmp := world_dir(folder) + "/level.json.tmp"
	var f := FileAccess.open(tmp, FileAccess.WRITE)
	if f == null:
		push_error("SaveManager: cannot write " + tmp)
		return false
	f.store_string(JSON.stringify(data, "\t"))
	f.close()
	var dst := world_dir(folder) + "/level.json"
	if FileAccess.file_exists(dst):
		DirAccess.remove_absolute(dst)
	DirAccess.rename_absolute(tmp, dst)
	return true


static func palette() -> PackedStringArray:
	var p := PackedStringArray()
	for d in BlockDB.defs:
		p.append((d as BlockDef).name)
	return p


## Prepares the id remap table for a saved palette.
static func prepare_palette(saved: Array) -> void:
	id_map = PackedInt32Array()
	if saved.is_empty():
		return
	var identity := saved.size() == BlockDB.count
	var m := PackedInt32Array()
	m.resize(maxi(saved.size(), 1))
	for i in saved.size():
		var nid := BlockDB.id(String(saved[i]))
		m[i] = nid
		if nid != i:
			identity = false
	if not identity:
		id_map = m


## Called by World when a saved chunk is read: attaches the remap table for the worker thread.
static func remap_chunk(data: Dictionary) -> Dictionary:
	if not id_map.is_empty():
		data["remap"] = id_map
	return data


static func apply_remap(arr: PackedInt32Array, m: PackedInt32Array) -> void:
	var n := m.size()
	for i in arr.size():
		var v := arr[i]
		if v == 0:
			continue
		var id := v & 0xFFF
		var nid := m[id] if id < n else 0
		arr[i] = (v & ~0xFFF) | nid


static func delete_world(folder: String) -> void:
	_rm_rf(world_dir(folder))


static func _rm_rf(path: String) -> void:
	var d := DirAccess.open(path)
	if d == null:
		return
	for f in d.get_files():
		DirAccess.remove_absolute(path + "/" + f)
	for sub in d.get_directories():
		_rm_rf(path + "/" + sub)
	DirAccess.remove_absolute(path)


static func unique_folder(name: String) -> String:
	var base := name.validate_filename()
	if base == "":
		base = "World"
	var n := base
	var i := 1
	while DirAccess.dir_exists_absolute(world_dir(n)):
		i += 1
		n = "%s (%d)" % [base, i]
	return n
