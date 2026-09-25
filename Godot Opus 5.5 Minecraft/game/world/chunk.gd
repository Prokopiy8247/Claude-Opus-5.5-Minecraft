class_name Chunk
extends RefCounted
## One 16 x H x 16 chunk column. Blocks are packed ints (id | meta << 12), light is packed bytes
## (sky << 4 | block). Index layout is y-major: ((y - min_y) << 8) | (z << 4) | x.

const S_NEW := 0
const S_GENERATING := 1
const S_GENERATED := 2
const S_LIT := 3

var cx: int
var cz: int
var min_y: int
var height: int
var sections: int
var blocks: PackedInt32Array
var light: PackedByteArray
var heightmap: PackedInt32Array      # 256: highest absolute y with light opacity > 0 (min_y - 1 when empty)
var biomes: PackedByteArray          # 256 surface biome ids
var colors: PackedColorArray         # 256 * 3: grass, foliage, water tint colours (biome blended)
var emitters: PackedInt32Array       # local indices of light emitting blocks (generation time)
var block_entities: Dictionary = {}  # local index -> Dictionary
var pending_entities: Array = []     # [{t, p, mob, data...}] spawned when the chunk becomes active
var state := S_NEW
var modified := false                # has player/world changes -> saved to disk
var version := 0
var lit := false
var meshed := false
var light_pending := false
var loaded_from_save := false
var saved_entities := false          # the last save wrote live entities for this chunk
var scheduled: Dictionary = {}       # local index -> tick due (scheduled block ticks)
var sky_top := 0                     # light: layers >= sky_top are fully sky-lit (set by LightEngine)
var blk_ymin := 1 << 20              # light: y range (relative) containing block light
var blk_ymax := -1


func _init(x: int = 0, z: int = 0, miny: int = 0, h: int = 256) -> void:
	cx = x
	cz = z
	min_y = miny
	height = h
	sections = h >> 4
	blocks = PackedInt32Array()
	blocks.resize(256 * h)
	light = PackedByteArray()
	light.resize(256 * h)
	heightmap = PackedInt32Array()
	heightmap.resize(256)
	heightmap.fill(miny - 1)
	biomes = PackedByteArray()
	biomes.resize(256)
	colors = PackedColorArray()
	colors.resize(768)
	colors.fill(Color(0.47, 0.72, 0.29))
	emitters = PackedInt32Array()


func key() -> Vector2i:
	return Vector2i(cx, cz)


static func index(lx: int, yr: int, lz: int) -> int:
	return (yr << 8) | (lz << 4) | lx


func get_block(lx: int, y: int, lz: int) -> int:
	var yr := y - min_y
	if yr < 0 or yr >= height:
		return 0
	return blocks[(yr << 8) | (lz << 4) | lx]


func set_block_raw(lx: int, y: int, lz: int, v: int) -> void:
	var yr := y - min_y
	if yr < 0 or yr >= height:
		return
	blocks[(yr << 8) | (lz << 4) | lx] = v


func get_sky(lx: int, y: int, lz: int) -> int:
	var yr := y - min_y
	if yr >= height:
		return 15
	if yr < 0:
		return 0
	return light[(yr << 8) | (lz << 4) | lx] >> 4


func get_blocklight(lx: int, y: int, lz: int) -> int:
	var yr := y - min_y
	if yr < 0 or yr >= height:
		return 0
	return light[(yr << 8) | (lz << 4) | lx] & 15


## Recomputes heightmap and emitter list from block data (used after generation / loading).
func rebuild_derived() -> void:
	var op := BlockDB.light_opacity
	var em := BlockDB.light_emit
	var b := blocks
	var hm := PackedInt32Array()
	hm.resize(256)
	hm.fill(min_y - 1)
	var col_done := 0
	var em_list := PackedInt32Array()
	var zero := PackedInt32Array()
	zero.resize(4096)
	var s := sections - 1
	while s >= 0:
		var start := s << 12
		if b.slice(start, start + 4096) == zero:
			s -= 1
			continue
		var i := start + 4095
		while i >= start:
			var v := b[i]
			if v != 0:
				var id := v & 0xFFF
				if col_done < 256 and op[id] > 0:
					var col := i & 255
					if hm[col] == min_y - 1:
						hm[col] = (i >> 8) + min_y
						col_done += 1
				if em[(id << 4) | ((v >> 12) & 15)] > 0:
					em_list.append(i)
			i -= 1
		s -= 1
	heightmap = hm
	emitters = em_list


func update_heightmap_column(lx: int, lz: int) -> void:
	var op := BlockDB.light_opacity
	var col := (lz << 4) | lx
	var yr := height - 1
	while yr >= 0:
		var v := blocks[(yr << 8) | col]
		if v != 0 and op[v & 0xFFF] > 0:
			heightmap[col] = yr + min_y
			return
		yr -= 1
	heightmap[col] = min_y - 1


func section_is_empty(s: int) -> bool:
	var start := s << 12
	var b := blocks
	for i in range(start, start + 4096):
		if b[i] != 0:
			return false
	return true


func to_save_dict() -> Dictionary:
	return {
		"cx": cx, "cz": cz,
		"blocks": blocks.to_byte_array().compress(FileAccess.COMPRESSION_ZSTD),
		"size": blocks.size() * 4,
		"be": block_entities,
		"biomes": biomes,
		"ents": pending_entities,
	}
