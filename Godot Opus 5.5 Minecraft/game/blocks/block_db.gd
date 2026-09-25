class_name BlockDB
extends RefCounted
## Block registry. Builds BlockDef resources from the data catalog (BlockCatalog) and exposes
## flat packed lookup tables that the mesher / lighting / physics use from worker threads.

# ---- model kinds -------------------------------------------------------------------------
const M_AIR := 0
const M_CUBE := 1
const M_CROSS := 2
const M_FLUID := 3
const M_SLAB := 4
const M_STAIRS := 5
const M_FENCE := 6
const M_FENCE_GATE := 7
const M_WALL := 8
const M_PANE := 9
const M_DOOR := 10
const M_TRAPDOOR := 11
const M_TORCH := 12
const M_LADDER := 13
const M_CARPET := 14
const M_SNOW := 15
const M_CROP := 16
const M_RAIL := 17
const M_WIRE := 18
const M_BUTTON := 19
const M_PLATE := 20
const M_LEVER := 21
const M_REPEATER := 22
const M_COMPARATOR := 23
const M_PISTON := 24
const M_PISTON_HEAD := 25
const M_CACTUS := 26
const M_SHORT := 27
const M_BED := 28
const M_CHEST := 29
const M_LANTERN := 30
const M_CHAIN := 31
const M_CAMPFIRE := 32
const M_ANVIL := 33
const M_HOPPER := 34
const M_CAULDRON := 35
const M_BREWING := 36
const M_ENCHANT := 37
const M_END_FRAME := 38
const M_PORTAL := 39
const M_END_PORTAL := 40
const M_SIGN := 41
const M_POT := 42
const M_CAKE := 43
const M_EGG := 44
const M_SKULL := 45
const M_CANDLE := 46
const M_PICKLE := 47
const M_DRIPSTONE := 48
const M_VINE := 49
const M_LILY := 50
const M_BAMBOO := 51
const M_SCAFFOLD := 52
const M_LECTERN := 53
const M_GRINDSTONE := 54
const M_STONECUTTER := 55
const M_BELL := 56
const M_ROD := 57
const M_CHORUS := 58
const M_FIRE := 59
const M_TALL_CROSS := 60
const M_LEAVES := 61
const M_OBSERVER := 62
const M_DAYLIGHT := 63
const M_BARREL := 64
const M_SHELF := 65
const M_CHAIN_H := 66
const M_BEACON := 67
const M_HEAVY := 68
const M_AMETHYST := 69
const M_CARVED := 70
const M_HANGING := 71
const M_TRIPWIRE := 72
const M_COBWEB := 73
const M_PATH := 74          # cube with top at 15/16 (dirt path, farmland)
const M_HEAD := 75          # mob heads / skulls
const M_SPIKE := 76         # sulfur spikes (dripstone-like)

# ---- render layers -----------------------------------------------------------------------
const R_OPAQUE := 0
const R_CUTOUT := 1
const R_TRANSLUCENT := 2
const R_NONE := 3

# ---- tint kinds --------------------------------------------------------------------------
const T_NONE := 0
const T_GRASS := 1
const T_FOLIAGE := 2
const T_WATER := 3
const T_SPRUCE := 4
const T_BIRCH := 5
const T_REDSTONE := 6
const T_STEM := 7
const T_LILY := 8
const T_MANGROVE := 9
const T_GRASS_MARK := 10   # tint only pixels marked in the texture alpha (grass block side)
const T_DRY_GRASS := 11

# face-table packing: layer (bits 0..15) | uv rotation (16..17) | tint kind (18..22)
const FT_ROT_SHIFT := 16
const FT_TINT_SHIFT := 18

static var defs: Array = []                 # Array[BlockDef] indexed by id
static var by_name: Dictionary = {}         # name -> id
static var count: int = 0
static var texture_names: PackedStringArray = PackedStringArray()   # every texture referenced

# flat tables (indexed by id)
static var model: PackedByteArray
static var render: PackedByteArray
static var full: PackedByteArray            # full opaque cube (culls + AO)
static var solid: PackedByteArray           # has collision
static var light_emit: PackedByteArray      # indexed by (id << 4) | meta
static var light_opacity: PackedByteArray
static var replaceable: PackedByteArray
static var fluid: PackedByteArray
static var cull_same: PackedByteArray
static var waterlogged: PackedByteArray
static var hardness: PackedFloat32Array
static var face_tex: PackedInt32Array       # ((id * 16 + meta) * 6 + face) -> packed layer|rot|tint
static var snowy_side: PackedInt32Array     # id -> layer used for side faces when snow is above (-1 none)
static var tex_layer: Dictionary = {}       # texture name -> layer
static var anim_frames: Dictionary = {}     # base layer -> frame count (for animated textures)
static var layer_frames: PackedByteArray    # layer -> frame count (1 = static)
static var ao_caster: PackedByteArray       # id -> casts ambient occlusion (cube-shaped with full collision)
static var initialized := false

# Frequently used ids (resolved at init)
static var AIR := 0
static var STONE := 0
static var DIRT := 0
static var GRASS := 0
static var WATER := 0
static var LAVA := 0
static var BEDROCK := 0
static var SAND := 0


static func init_registry() -> void:
	if initialized:
		return
	defs.clear()
	by_name.clear()
	var cat := BlockCatalog.new()
	cat.build()
	for d in cat.defs:
		d.id = defs.size()
		defs.append(d)
		by_name[d.name] = d.id
	count = defs.size()
	_collect_textures()
	_build_tables()
	AIR = id("air")
	STONE = id("stone")
	DIRT = id("dirt")
	GRASS = id("grass_block")
	WATER = id("water")
	LAVA = id("lava")
	BEDROCK = id("bedrock")
	SAND = id("sand")
	initialized = true


static func id(block_name: String) -> int:
	return by_name.get(block_name, 0)


static func has(block_name: String) -> bool:
	return by_name.has(block_name)


static func def(block_id: int) -> BlockDef:
	return defs[block_id & Vox.ID_MASK]


static func name_of(v: int) -> String:
	return defs[v & Vox.ID_MASK].name


static func _collect_textures() -> void:
	var seen := {}
	texture_names = PackedStringArray()
	for d in defs:
		for k in d.tex:
			var t: String = d.tex[k]
			if t != "" and not seen.has(t):
				seen[t] = true
				texture_names.append(t)
		var extra: Array = []
		for k in d.props:
			var ks := String(k)
			if ks == "extra_tex" or ks == "stages":
				extra.append_array(d.props[k])
			elif ks.ends_with("_tex") or ks == "snowy_side":
				extra.append(d.props[k])
		for t in extra:
			if not seen.has(t):
				seen[t] = true
				texture_names.append(t)


static func _build_tables() -> void:
	model = PackedByteArray(); model.resize(count)
	render = PackedByteArray(); render.resize(count)
	full = PackedByteArray(); full.resize(count)
	solid = PackedByteArray(); solid.resize(count)
	light_emit = PackedByteArray(); light_emit.resize(count * 16)
	light_opacity = PackedByteArray(); light_opacity.resize(count)
	replaceable = PackedByteArray(); replaceable.resize(count)
	fluid = PackedByteArray(); fluid.resize(count)
	cull_same = PackedByteArray(); cull_same.resize(count)
	waterlogged = PackedByteArray(); waterlogged.resize(count)
	hardness = PackedFloat32Array(); hardness.resize(count)
	ao_caster = PackedByteArray(); ao_caster.resize(count)
	for d in defs:
		var i: int = d.id
		model[i] = d.model
		render[i] = d.render
		full[i] = 1 if d.full else 0
		solid[i] = 1 if d.solid else 0
		var lm = d.props.get("light_meta", null)
		var lit_bit: int = d.props.get("lit_bit", 0)
		for m in 16:
			var e: int = d.light
			if lm != null:
				e = lm[m] if m < lm.size() else lm[lm.size() - 1]
			elif lit_bit != 0:
				e = d.light if (m & lit_bit) != 0 else 0
			light_emit[(i << 4) | m] = e
		light_opacity[i] = d.opacity
		replaceable[i] = 1 if d.replaceable else 0
		fluid[i] = d.fluid
		cull_same[i] = 1 if d.cull_same else 0
		waterlogged[i] = 1 if d.waterlogged else 0
		hardness[i] = d.hardness
		ao_caster[i] = 1 if (d.full or ((d.model == M_CUBE or d.model == M_LEAVES) and d.solid and d.render != R_TRANSLUCENT)) else 0


## Resolve texture layers once the atlas index is known and build the per-(id,meta) face table.
static func resolve_textures(layers: Dictionary, frames: Dictionary) -> void:
	tex_layer = layers
	anim_frames = frames
	var max_layer := 0
	for k in layers:
		max_layer = maxi(max_layer, int(layers[k]) + int(frames.get(int(layers[k]), 1)))
	layer_frames = PackedByteArray()
	layer_frames.resize(max_layer + 1)
	layer_frames.fill(1)
	for l in frames:
		layer_frames[int(l)] = clampi(int(frames[l]), 1, 255)
	face_tex = PackedInt32Array()
	face_tex.resize(count * 16 * 6)
	snowy_side = PackedInt32Array()
	snowy_side.resize(count)
	snowy_side.fill(-1)
	var missing := 0
	for d in defs:
		for meta in 16:
			for f in 6:
				var tname := face_texture_name(d, meta, f)
				var layer: int = layers.get(tname, -1)
				if layer < 0:
					layer = layers.get("missing", 0)
					if tname != "":
						missing += 1
				var rot := face_rotation(d, meta, f)
				var tint := face_tint(d, meta, f)
				face_tex[(d.id * 16 + meta) * 6 + f] = layer | (rot << FT_ROT_SHIFT) | (tint << FT_TINT_SHIFT)
		if d.props.has("snowy_side"):
			snowy_side[d.id] = layers.get(d.props["snowy_side"], -1)
	if missing > 0:
		push_warning("BlockDB: %d face textures missing from atlas" % missing)


static func layer_of(tname: String) -> int:
	return tex_layer.get(tname, tex_layer.get("missing", 0))


## Texture name for a face of a cube-like block given its meta (orientation aware).
static func face_texture_name(d: BlockDef, meta: int, f: int) -> String:
	var t: Dictionary = d.tex
	if t.is_empty():
		return ""
	if t.has("all") and t.size() == 1:
		return t["all"]
	var up_t: String = t.get("top", t.get("end", t.get("all", "")))
	var down_t: String = t.get("bottom", up_t)
	var side_t: String = t.get("side", t.get("all", ""))
	match d.place:
		"axis":
			var axis := meta & 3
			var along := false
			if axis == 0:
				along = f == Vox.UP or f == Vox.DOWN
			elif axis == 1:
				along = f == Vox.EAST or f == Vox.WEST
			else:
				along = f == Vox.SOUTH or f == Vox.NORTH
			return up_t if along else side_t
		"facing", "facing_lit":
			var fd := Vox.facing_to_dir(meta & 3)
			var lit := d.place == "facing_lit" and (meta & 4) != 0
			if f == Vox.UP:
				return t.get("top_on", up_t) if lit else up_t
			if f == Vox.DOWN:
				return down_t
			if f == fd:
				var fr: String = t.get("front", side_t)
				return t.get("front_on", fr) if lit else fr
			if t.has("back") and f == Vox.OPPOSITE[fd]:
				return t["back"]
			return t.get("side_on", side_t) if lit else side_t
		"facing_all", "facing_all_player", "facing_all_observer", "facing_all_shulker":
			var fd6 := meta & 7
			if fd6 > 5:
				fd6 = 0
			var on := (meta & 8) != 0
			if f == fd6:
				if (fd6 == Vox.UP or fd6 == Vox.DOWN) and t.has("front_vertical"):
					return t["front_vertical"]
				if t.has("front"):
					return t.get("front_on", t["front"]) if on else t["front"]
				return up_t
			if f == Vox.OPPOSITE[fd6]:
				if t.has("back"):
					return t.get("back_on", t["back"]) if on else t["back"]
				return down_t
			if d.place == "facing_all_observer" and (fd6 == Vox.UP or fd6 == Vox.DOWN):
				return t.get("top", side_t)
			if d.place == "facing_all_observer" and (f == Vox.UP or f == Vox.DOWN):
				return t.get("top", side_t)
			return t.get("side_on", side_t) if on else side_t
		"glazed":
			return t.get("all", side_t)
		"lit":
			if (meta & 1) != 0:
				return t.get("on", t.get("all", side_t))
			return t.get("all", side_t)
	if f == Vox.UP:
		return up_t
	if f == Vox.DOWN:
		return down_t
	if t.has("front") and f == Vox.SOUTH:
		return t["front"]
	return side_t


static func face_rotation(d: BlockDef, meta: int, f: int) -> int:
	if d.place == "glazed":
		return (meta + (2 if f == Vox.DOWN else 0)) & 3
	if d.place.begins_with("facing_all") and d.model == M_CUBE:
		var fd6 := meta & 7
		# rotate side textures so their "up" points toward the facing direction (pistons, observers)
		if fd6 == Vox.DOWN and f != Vox.UP and f != Vox.DOWN:
			return 2
		if (fd6 == Vox.EAST or fd6 == Vox.WEST) and (f == Vox.UP or f == Vox.DOWN or f == Vox.NORTH or f == Vox.SOUTH):
			return 1 if (fd6 == Vox.EAST) == (f != Vox.NORTH) else 3
		if (fd6 == Vox.SOUTH or fd6 == Vox.NORTH) and (f == Vox.EAST or f == Vox.WEST):
			return 1 if (fd6 == Vox.SOUTH) == (f == Vox.EAST) else 3
		if (fd6 == Vox.SOUTH or fd6 == Vox.NORTH) and (f == Vox.UP or f == Vox.DOWN):
			return 0 if fd6 == Vox.NORTH else 2
		return 0
	if d.place == "axis":
		var axis := meta & 3
		if axis == 1 and (f == Vox.UP or f == Vox.DOWN or f == Vox.SOUTH or f == Vox.NORTH):
			return 1
		if axis == 2 and (f == Vox.EAST or f == Vox.WEST):
			return 1
		if axis == 2 and (f == Vox.UP or f == Vox.DOWN):
			return 0
	return 0


static func face_tint(d: BlockDef, _meta: int, f: int) -> int:
	if d.tint == T_NONE:
		return T_NONE
	if d.props.has("tint_top_only"):
		if f == Vox.UP:
			return d.tint
		if f == Vox.DOWN:
			return T_NONE
		return T_GRASS_MARK if d.props.get("tint_side_mark", false) else T_NONE
	return d.tint


static func emission(v: int) -> int:
	return light_emit[((v & Vox.ID_MASK) << 4) | ((v >> Vox.META_SHIFT) & 15)]


static func is_air(v: int) -> bool:
	return (v & Vox.ID_MASK) == 0
