class_name StructureLayout
extends RefCounted
## Block placements of one generated structure, bucketed per chunk so every chunk that intersects
## the structure can copy its part. Values may carry a placement mode in bits 28..30:
##   0 = replace anything, 1 = only into air/replaceable, 2 = only into air/replaceable/leaves.

var empty := false
var x0 := 2147483647
var z0 := 2147483647
var x1 := -2147483647
var z1 := -2147483647
var y0 := 2147483647
var y1 := -2147483647
var blocks: Dictionary = {}     # Vector2i -> Array of ints (x, y, z, v) - untyped Array so appends stay in place
var bents: Dictionary = {}      # Vector2i -> Array [[x, y, z, data]]
var ents: Dictionary = {}       # Vector2i -> Array [[type, Vector3, data]]
var name := ""
var origin := Vector3i.ZERO


func put(x: int, y: int, z: int, v: int, mode: int = 0) -> void:
	var k := Vector2i(x >> 4, z >> 4)
	var arr: Array = blocks.get(k, [])
	if arr.is_empty():
		blocks[k] = arr
	arr.append(x)
	arr.append(y)
	arr.append(z)
	arr.append(v | (mode << 28))
	if x < x0: x0 = x
	if z < z0: z0 = z
	if x > x1: x1 = x
	if z > z1: z1 = z
	if y < y0: y0 = y
	if y > y1: y1 = y


## Block by name (or a raw block value when an int is passed, e.g. 0 for air).
static func value_of(block, meta: int = 0) -> int:
	if block is String or block is StringName:
		return Vox.make(BlockDB.id(String(block)), meta)
	return int(block)


func put_name(x: int, y: int, z: int, block, meta: int = 0, mode: int = 0) -> void:
	put(x, y, z, value_of(block, meta), mode)


## fill(box, block, mode) or fill(box, block, meta, mode).
func fill(ax: int, ay: int, az: int, bx: int, by: int, bz: int, block, a: int = 0, b: int = -1) -> void:
	var meta := 0 if b < 0 else a
	var mode := a if b < 0 else b
	var v := value_of(block, meta)
	for y in range(mini(ay, by), maxi(ay, by) + 1):
		for z in range(mini(az, bz), maxi(az, bz) + 1):
			for x in range(mini(ax, bx), maxi(ax, bx) + 1):
				put(x, y, z, v, mode)


func hollow(ax: int, ay: int, az: int, bx: int, by: int, bz: int, wall, inside = 0, mode: int = 0) -> void:
	var wv := value_of(wall)
	var iv := value_of(inside)
	for y in range(mini(ay, by), maxi(ay, by) + 1):
		for z in range(mini(az, bz), maxi(az, bz) + 1):
			for x in range(mini(ax, bx), maxi(ax, bx) + 1):
				var edge := x == mini(ax, bx) or x == maxi(ax, bx) or y == mini(ay, by) or y == maxi(ay, by) or z == mini(az, bz) or z == maxi(az, bz)
				put(x, y, z, wv if edge else iv, mode)


func block_entity(x: int, y: int, z: int, data: Dictionary) -> void:
	var k := Vector2i(x >> 4, z >> 4)
	if not bents.has(k):
		bents[k] = []
	bents[k].append([x, y, z, data])


func entity(type: String, pos: Vector3, data: Dictionary = {}) -> void:
	var k := Vector2i(floori(pos.x) >> 4, floori(pos.z) >> 4)
	if not ents.has(k):
		ents[k] = []
	ents[k].append([type, pos, data])


func chest(x: int, y: int, z: int, loot: String, facing: int = 0, block := "chest") -> void:
	put_name(x, y, z, block, facing)
	block_entity(x, y, z, {"type": "container", "loot": loot, "items": []})


func contains(p: Vector3i) -> bool:
	return not empty and p.x >= x0 and p.x <= x1 and p.z >= z0 and p.z <= z1 and p.y >= y0 - 2 and p.y <= y1 + 2


func intersects_chunk(cx: int, cz: int) -> bool:
	return blocks.has(Vector2i(cx, cz)) or bents.has(Vector2i(cx, cz)) or ents.has(Vector2i(cx, cz))


func write_chunk(w: WorldGen.Writer) -> void:
	var k := Vector2i(w.c.cx, w.c.cz)
	var arr: Array = blocks.get(k, [])
	var n := arr.size()
	var i := 0
	while i < n:
		var v: int = arr[i + 3]
		w.put(int(arr[i]), int(arr[i + 1]), int(arr[i + 2]), v & 0x0FFFFFFF, (v >> 28) & 7)
		i += 4
	for be in bents.get(k, []):
		w.add_block_entity(be[0], be[1], be[2], (be[3] as Dictionary).duplicate(true))
	for e in ents.get(k, []):
		w.add_entity(e[0], e[1], e[2])
