class_name MeshModels
extends RefCounted
## Geometry for non-cube block models (slabs, stairs, fences, plants, fluids, redstone parts...).
## Models are authored in a canonical orientation and transformed:
##   xf 0..3 : rotations about Y, canonical front faces south (+Z)   (facing meta 0 S, 1 W, 2 N, 3 E)
##   xf 4..9 : canonical "up" models oriented to UP, DOWN, EAST, WEST, SOUTH, NORTH

const E := 0
const W := 1
const U := 2
const D := 3
const S := 4
const N := 5
const XF_BY_DIR := [6, 7, 4, 5, 8, 9]   # Vox dir -> xf for canonical-up models
const PX := 1.0 / 16.0

class Ctx:
	var pb: PackedInt32Array
	var pl: PackedByteArray
	var surfaces: Array
	var colors: PackedColorArray
	var job
	var sy := 0

static var XF_FACE := PackedInt32Array()
static var TL: Array = []            # per block id: Dictionary tex key -> layer
static var STAGES: Array = []        # per block id: PackedInt32Array(16) crop stage layers or null
static var ids := {}                 # frequently used ids
static var connect_fence := PackedByteArray()
static var connect_pane := PackedByteArray()
static var connect_wall := PackedByteArray()
static var redstone_conn := PackedByteArray()
static var inited := false


static func init() -> void:
	if inited:
		return
	XF_FACE.resize(60)
	var nrm := [Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, -1, 0), Vector3(0, 0, 1), Vector3(0, 0, -1)]
	var c := Vector3(0.5, 0.5, 0.5)
	for xf in 10:
		for f in 6:
			var t := xfp(xf, c + nrm[f] * 0.5) - xfp(xf, c)
			var best := 0
			var bd := -10.0
			for g in 6:
				var dt: float = t.dot(nrm[g])
				if dt > bd:
					bd = dt
					best = g
			XF_FACE[xf * 6 + f] = best
	TL.resize(BlockDB.count)
	STAGES.resize(BlockDB.count)
	for d in BlockDB.defs:
		var tl := {}
		for k in d.tex:
			tl[String(k)] = BlockDB.layer_of(d.tex[k])
		TL[d.id] = tl
		if d.props.has("stages"):
			var st := PackedInt32Array()
			st.resize(16)
			var arr: Array = d.props["stages"]
			for m in 16:
				st[m] = BlockDB.layer_of(arr[mini(m, arr.size() - 1)])
			STAGES[d.id] = st
	for n in ["water", "lava", "snow", "fire", "soul_fire", "redstone_wire", "repeater", "repeater_on", "comparator",
			"redstone_torch", "redstone_block", "lever", "glass_pane", "iron_bars", "piston_head", "cobblestone_wall",
			"chorus_plant", "chorus_flower", "end_stone", "netherrack", "soul_sand", "soul_soil", "torch", "observer",
			"target", "daylight_detector", "stone_button", "tripwire_hook", "trapped_chest", "sculk_sensor"]:
		ids[n] = BlockDB.id(n)
	connect_fence.resize(BlockDB.count)
	connect_pane.resize(BlockDB.count)
	connect_wall.resize(BlockDB.count)
	redstone_conn.resize(BlockDB.count)
	for d in BlockDB.defs:
		var i: int = d.id
		if d.model == BlockDB.M_FENCE or d.model == BlockDB.M_FENCE_GATE:
			connect_fence[i] = 1
		if d.model == BlockDB.M_PANE or d.tags.has("glass"):
			connect_pane[i] = 1
		if d.model == BlockDB.M_WALL or d.model == BlockDB.M_FENCE_GATE:
			connect_wall[i] = 1
		if d.full:
			connect_fence[i] = 1
			connect_pane[i] = 1
			connect_wall[i] = 1
		if d.model in [BlockDB.M_WIRE, BlockDB.M_REPEATER, BlockDB.M_COMPARATOR, BlockDB.M_LEVER, BlockDB.M_BUTTON,
				BlockDB.M_PLATE, BlockDB.M_DAYLIGHT] or d.name in ["redstone_torch", "redstone_block", "observer", "target",
				"tripwire_hook", "trapped_chest", "sculk_sensor", "lightning_rod"]:
			redstone_conn[i] = 1
	inited = true


static func xfp(xf: int, v: Vector3) -> Vector3:
	match xf:
		1: return Vector3(1.0 - v.z, v.y, v.x)
		2: return Vector3(1.0 - v.x, v.y, 1.0 - v.z)
		3: return Vector3(v.z, v.y, 1.0 - v.x)
		5: return Vector3(v.x, 1.0 - v.y, 1.0 - v.z)
		6: return Vector3(v.y, 1.0 - v.x, v.z)
		7: return Vector3(1.0 - v.y, v.x, v.z)
		8: return Vector3(v.x, 1.0 - v.z, v.y)
		9: return Vector3(v.x, v.z, 1.0 - v.y)
	return v


static func uv_for(f: int, p: Vector3) -> Vector2:
	match f:
		E: return Vector2(1.0 - p.z, 1.0 - p.y)
		W: return Vector2(p.z, 1.0 - p.y)
		U: return Vector2(p.x, p.z)
		D: return Vector2(p.x, p.z)
		S: return Vector2(p.x, 1.0 - p.y)
	return Vector2(1.0 - p.x, 1.0 - p.y)


static func lay(id: int, key: String, fallback: String = "all") -> int:
	var tl: Dictionary = TL[id]
	if tl.has(key):
		return tl[key]
	if tl.has(fallback):
		return tl[fallback]
	if tl.has("side"):
		return tl["side"]
	for k in tl:
		return tl[k]
	return 0


static func frames_of(layer: int) -> float:
	var lf := BlockDB.layer_frames
	return float(lf[layer]) if layer >= 0 and layer < lf.size() else 1.0


## Emits an axis aligned box (canonical coords in 0..1) transformed by xf.
## lays: Array of 6 layers (canonical face order) or an int for all faces. mask: canonical faces to emit.
static func box(c: Ctx, sf: MeshSurface, org: Vector3, p: int, a: Vector3, b: Vector3, lays, tint: Color,
		xf: int = 0, mask: int = 63, cull := true, uvs: Array = []) -> void:
	var pb := c.pb
	var pl := c.pl
	var full := BlockDB.full
	var own := pl[p]
	for f in 6:
		if (mask & (1 << f)) == 0:
			continue
		var tf := XF_FACE[xf * 6 + f]
		var cs: Array = _face_corners(f, a, b)
		var q0 := xfp(xf, cs[0])
		var q1 := xfp(xf, cs[1])
		var q2 := xfp(xf, cs[2])
		var q3 := xfp(xf, cs[3])
		var np := p + Mesher.NOFF[tf]
		var on_edge := false
		match tf:
			E: on_edge = q0.x > 0.999
			W: on_edge = q0.x < 0.001
			U: on_edge = q0.y > 0.999
			D: on_edge = q0.y < 0.001
			S: on_edge = q0.z > 0.999
			N: on_edge = q0.z < 0.001
		if cull and on_edge and full[pb[np] & 0xFFF] == 1:
			continue
		var nl := pl[np]
		var sky := float(maxi(own >> 4, nl >> 4))
		var blk := float(maxi(own & 15, nl & 15))
		var layer: int = lays[f] if lays is Array or lays is PackedInt32Array else int(lays)
		var u0: Vector2
		var u1: Vector2
		var u2: Vector2
		var u3: Vector2
		if uvs.size() == 6 and uvs[f] != null:
			var r: Rect2 = uvs[f]
			u0 = Vector2(r.position.x, r.end.y)
			u1 = r.position
			u2 = Vector2(r.end.x, r.position.y)
			u3 = r.end
			if f == U or f == D:
				u0 = r.position
				u1 = Vector2(r.end.x, r.position.y)
				u2 = r.end
				u3 = Vector2(r.position.x, r.end.y)
				if f == D:
					u1 = Vector2(r.position.x, r.end.y)
					u3 = Vector2(r.end.x, r.position.y)
		else:
			u0 = uv_for(f, cs[0])
			u1 = uv_for(f, cs[1])
			u2 = uv_for(f, cs[2])
			u3 = uv_for(f, cs[3])
		sf.quad(org + q0, org + q1, org + q2, org + q3, u0, u1, u2, u3, float(layer), frames_of(layer), tint,
			Mesher.SHADE[tf], sky, blk)


static func _face_corners(f: int, a: Vector3, b: Vector3) -> Array:
	match f:
		E: return [Vector3(b.x, a.y, b.z), Vector3(b.x, b.y, b.z), Vector3(b.x, b.y, a.z), Vector3(b.x, a.y, a.z)]
		W: return [Vector3(a.x, a.y, a.z), Vector3(a.x, b.y, a.z), Vector3(a.x, b.y, b.z), Vector3(a.x, a.y, b.z)]
		U: return [Vector3(a.x, b.y, a.z), Vector3(b.x, b.y, a.z), Vector3(b.x, b.y, b.z), Vector3(a.x, b.y, b.z)]
		D: return [Vector3(a.x, a.y, a.z), Vector3(a.x, a.y, b.z), Vector3(b.x, a.y, b.z), Vector3(b.x, a.y, a.z)]
		S: return [Vector3(a.x, a.y, b.z), Vector3(a.x, b.y, b.z), Vector3(b.x, b.y, b.z), Vector3(b.x, a.y, b.z)]
	return [Vector3(b.x, a.y, a.z), Vector3(b.x, b.y, a.z), Vector3(a.x, b.y, a.z), Vector3(a.x, a.y, a.z)]


static func px3(x: float, y: float, z: float) -> Vector3:
	return Vector3(x * PX, y * PX, z * PX)


static func light_at(c: Ctx, p: int) -> Vector2:
	var l := c.pl[p]
	var up := c.pl[p + Mesher.PSS]
	return Vector2(maxi(l >> 4, up >> 4), maxi(l & 15, up & 15))


## Two crossed double-sided planes (plants).
static func cross(c: Ctx, sf: MeshSurface, org: Vector3, p: int, layer: int, tint: Color, scale := 1.0, h := 1.0,
		jitter := Vector3.ZERO, flip_v := false) -> void:
	var lt := light_at(c, p)
	var fr := frames_of(layer)
	var lo := 0.5 - 0.5 * scale * 0.9
	var hi := 0.5 + 0.5 * scale * 0.9
	var o := org + jitter
	var ta := Vector2(0, 1) if not flip_v else Vector2(0, 0)
	var tb := Vector2(0, 0) if not flip_v else Vector2(0, 1)
	var tc := Vector2(1, 0) if not flip_v else Vector2(1, 1)
	var td := Vector2(1, 1) if not flip_v else Vector2(1, 0)
	sf.quad2(o + Vector3(lo, 0, lo), o + Vector3(lo, h, lo), o + Vector3(hi, h, hi), o + Vector3(hi, 0, hi),
		ta, tb, tc, td, float(layer), fr, tint, 0.9, lt.x, lt.y)
	sf.quad2(o + Vector3(lo, 0, hi), o + Vector3(lo, h, hi), o + Vector3(hi, h, lo), o + Vector3(hi, 0, lo),
		ta, tb, tc, td, float(layer), fr, tint, 0.9, lt.x, lt.y)


## "#"-shaped crop planes.
static func hash_planes(c: Ctx, sf: MeshSurface, org: Vector3, p: int, layer: int, tint: Color, h := 1.0, y0 := -0.0625) -> void:
	var lt := light_at(c, p)
	var fr := frames_of(layer)
	for k in [0.25, 0.75]:
		sf.quad2(org + Vector3(k, y0, 0), org + Vector3(k, y0 + h, 0), org + Vector3(k, y0 + h, 1), org + Vector3(k, y0, 1),
			Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), float(layer), fr, tint, 0.85, lt.x, lt.y)
		sf.quad2(org + Vector3(0, y0, k), org + Vector3(0, y0 + h, k), org + Vector3(1, y0 + h, k), org + Vector3(1, y0, k),
			Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), float(layer), fr, tint, 0.85, lt.x, lt.y)


## Horizontal quad (rails, redstone dust, lily pads, carpets of flat plants). rot = uv quarter turns.
static func flat(c: Ctx, sf: MeshSurface, org: Vector3, p: int, layer: int, tint: Color, y: float, rot := 0,
		double := true, x0 := 0.0, z0 := 0.0, x1 := 1.0, z1 := 1.0) -> void:
	var lt := light_at(c, p)
	var uv := [Vector2(x0, z0), Vector2(x1, z0), Vector2(x1, z1), Vector2(x0, z1)]
	var r := [uv[(0 + rot) & 3], uv[(1 + rot) & 3], uv[(2 + rot) & 3], uv[(3 + rot) & 3]]
	var fr := frames_of(layer)
	if double:
		sf.quad2(org + Vector3(x0, y, z0), org + Vector3(x1, y, z0), org + Vector3(x1, y, z1), org + Vector3(x0, y, z1),
			r[0], r[1], r[2], r[3], float(layer), fr, tint, 1.0, lt.x, lt.y)
	else:
		sf.quad(org + Vector3(x0, y, z0), org + Vector3(x1, y, z0), org + Vector3(x1, y, z1), org + Vector3(x0, y, z1),
			r[0], r[1], r[2], r[3], float(layer), fr, tint, 1.0, lt.x, lt.y)


static func hash3(x: int, y: int, z: int) -> int:
	var h := (x * 73856093) ^ (y * 19349663) ^ (z * 83492791)
	h = (h ^ (h >> 13)) * 1274126177
	return h & 0x7FFFFFFF


static func same_fluid(kind: int, nv: int) -> bool:
	var nid := nv & 0xFFF
	var f := BlockDB.fluid[nid]
	if f == kind:
		return true
	return kind == 1 and BlockDB.waterlogged[nid] == 1


static func fluid_height(v: int) -> float:
	var id := v & 0xFFF
	if BlockDB.fluid[id] == 0:
		return 0.8889
	var meta := (v >> 12) & 15
	if meta == 0:
		return 0.8889
	if meta >= 8:
		return 1.0
	return (8.0 - meta) / 9.0


static func _corner_h(c: Ctx, p: int, kind: int, cxo: int, czo: int) -> float:
	var pb := c.pb
	var sum := 0.0
	var wsum := 0.0
	for j in 2:
		for i in 2:
			var q := p + (cxo - 1 + i) + (czo - 1 + j) * Mesher.PS
			var qv := pb[q]
			if same_fluid(kind, pb[q + Mesher.PSS]):
				return 1.0
			if same_fluid(kind, qv):
				var h := fluid_height(qv)
				var wgt := 10.0 if h >= 0.8 else 1.0
				sum += h * wgt
				wsum += wgt
			elif BlockDB.solid[qv & 0xFFF] == 0:
				wsum += 1.0
	if wsum <= 0.0:
		return 0.8889
	return sum / wsum


static func fluid(c: Ctx, p: int, lx: int, ly: int, lz: int, wy: float, v: int) -> void:
	var id := v & 0xFFF
	var kind := BlockDB.fluid[id]
	if kind == 0:
		kind = 1
	var pb := c.pb
	var full := BlockDB.full
	var water := kind == 1
	var fid: int = BlockDB.WATER if water else BlockDB.LAVA
	var sf: MeshSurface = c.surfaces[2 if water else 0]
	var tint := Color(1, 1, 1)
	if water:
		tint = c.colors[((lz << 4) | lx) * 3 + 2]
	var top_l := lay(fid, "top")
	var side_l := lay(fid, "side")
	var org := Vector3(lx, wy, lz)
	var own := c.pl[p]
	var above_same := same_fluid(kind, pb[p + Mesher.PSS])
	var h00 := 1.0
	var h10 := 1.0
	var h11 := 1.0
	var h01 := 1.0
	if not above_same:
		h00 = _corner_h(c, p, kind, 0, 0)
		h10 = _corner_h(c, p, kind, 1, 0)
		h11 = _corner_h(c, p, kind, 1, 1)
		h01 = _corner_h(c, p, kind, 0, 1)
		var al := c.pl[p + Mesher.PSS]
		var sky := float(maxi(own >> 4, al >> 4))
		var blk := float(maxi(own & 15, al & 15))
		var fr := frames_of(top_l)
		var a0 := org + Vector3(0, h00, 0)
		var a1 := org + Vector3(1, h10, 0)
		var a2 := org + Vector3(1, h11, 1)
		var a3 := org + Vector3(0, h01, 1)
		sf.quad(a0, a1, a2, a3, Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1), float(top_l), fr, tint, 1.0, sky, blk)
		if water:
			sf.quad(a3, a2, a1, a0, Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 0), float(top_l), fr, tint, 0.8, sky, blk)
	# sides
	var hs := {E: [h11, h10], W: [h00, h01], S: [h01, h11], N: [h10, h00]}
	for f in [E, W, S, N]:
		var np := p + Mesher.NOFF[f]
		var nv := pb[np]
		if same_fluid(kind, nv) or full[nv & 0xFFF] == 1:
			continue
		var cs: Array = _face_corners(f, Vector3.ZERO, Vector3.ONE)
		var hh: Array = hs[f]
		# corners order: bottom-a, top-a, top-b, bottom-b
		var ta: float = hh[0]
		var tb: float = hh[1]
		var c0: Vector3 = cs[0]
		var c1: Vector3 = cs[1]
		var c2: Vector3 = cs[2]
		var c3: Vector3 = cs[3]
		c1.y = ta
		c2.y = tb
		var nl := c.pl[np]
		var sky2 := float(maxi(own >> 4, nl >> 4))
		var blk2 := float(maxi(own & 15, nl & 15))
		var fr2 := frames_of(side_l)
		sf.quad(org + c0, org + c1, org + c2, org + c3, Vector2(0, 1), Vector2(0, 1.0 - ta), Vector2(1, 1.0 - tb), Vector2(1, 1),
			float(side_l), fr2, tint, Mesher.SHADE[f], sky2, blk2)
		if water:
			sf.quad(org + c3, org + c2, org + c1, org + c0, Vector2(1, 1), Vector2(1, 1.0 - tb), Vector2(0, 1.0 - ta), Vector2(0, 1),
				float(side_l), fr2, tint, Mesher.SHADE[f] * 0.9, sky2, blk2)
	# bottom
	var bv := pb[p - Mesher.PSS]
	if not same_fluid(kind, bv) and full[bv & 0xFFF] == 0:
		var bl := c.pl[p - Mesher.PSS]
		var sky3 := float(maxi(own >> 4, bl >> 4))
		var blk3 := float(maxi(own & 15, bl & 15))
		var fr3 := frames_of(top_l)
		sf.quad(org + Vector3(0, 0, 0), org + Vector3(0, 0, 1), org + Vector3(1, 0, 1), org + Vector3(1, 0, 0),
			Vector2(0, 0), Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), float(top_l), fr3, tint, 0.5, sky3, blk3)
		if water:
			sf.quad(org + Vector3(1, 0, 0), org + Vector3(1, 0, 1), org + Vector3(0, 0, 1), org + Vector3(0, 0, 0),
				Vector2(1, 0), Vector2(1, 1), Vector2(0, 1), Vector2(0, 0), float(top_l), fr3, tint, 0.5, sky3, blk3)


# ------------------------------------------------------------------------------------------------
static func emit(c: Ctx, p: int, lx: int, ly: int, lz: int, wy: float, v: int, m: int) -> void:
	var id := v & 0xFFF
	var meta := (v >> 12) & 15
	var sf: MeshSurface = c.surfaces[BlockDB.render[id]]
	var org := Vector3(lx, wy, lz)
	var white := Color(1, 1, 1)
	var pb := c.pb
	var col := ((lz << 4) | lx) * 3
	var d: BlockDef = BlockDB.defs[id]
	var tint := white
	if d.tint != 0:
		tint = Mesher.tint_color(d.tint & 31, meta, c.colors, col)
	var all := lay(id, "all", "side")
	match m:
		BlockDB.M_CROSS, BlockDB.M_COBWEB:
			var jit := Vector3.ZERO
			if d.place == "plant" and d.tint != 0:
				var hh := hash3(lx + c.job.cx * 16, int(wy), lz + c.job.cz * 16)
				jit = Vector3(((hh & 15) / 15.0 - 0.5) * 0.3, -((hh >> 4) & 3) * 0.04, (((hh >> 8) & 15) / 15.0 - 0.5) * 0.3)
			cross(c, sf, org, p, all, tint, 1.0, 1.0, jit)
		BlockDB.M_TALL_CROSS:
			var upper := (meta & 1) == 1
			cross(c, sf, org, p, lay(id, "top" if upper else "bottom"), tint)
		BlockDB.M_CROP:
			var st = STAGES[id]
			var layer := all
			if st != null:
				layer = (st as PackedInt32Array)[meta]
			if d.props.get("stem", false):
				cross(c, sf, org, p, layer, tint, 1.0, 0.25 + 0.1 * (meta & 7))
			elif d.props.get("cross", false):
				cross(c, sf, org, p, layer, tint)
			else:
				hash_planes(c, sf, org, p, layer, tint)
		BlockDB.M_SLAB:
			var t6 := _six(id)
			match meta & 3:
				0: box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 0.5, 1), t6, tint)
				1: box(c, sf, org, p, Vector3(0, 0.5, 0), Vector3(1, 1, 1), t6, tint)
				_: box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 1, 1), t6, tint)
		BlockDB.M_STAIRS:
			_stairs(c, sf, org, p, id, meta, tint)
		BlockDB.M_FENCE:
			_fence(c, sf, org, p, id, all, tint)
		BlockDB.M_FENCE_GATE:
			_gate(c, sf, org, p, meta, all, tint)
		BlockDB.M_WALL:
			_wall(c, sf, org, p, id, all, tint)
		BlockDB.M_PANE:
			_pane(c, sf, org, p, id, all, lay(id, "edge"), tint)
		BlockDB.M_DOOR:
			_door(c, sf, org, p, id, meta, tint)
		BlockDB.M_TRAPDOOR:
			_trapdoor(c, sf, org, p, meta, all, tint)
		BlockDB.M_TORCH:
			_torch(c, sf, org, p, id, meta)
		BlockDB.M_LADDER:
			_wall_plane(c, sf, org, p, meta & 3, all, 1.0 / 16.0)
		BlockDB.M_CARPET:
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, PX, 1), all, tint)
		BlockDB.M_SNOW:
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, float((meta & 7) + 1) / 8.0, 1), all, tint)
		BlockDB.M_PATH:
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 15.0 / 16.0, 1), _six(id, meta), tint)
		BlockDB.M_CACTUS:
			var t6c := _six(id)
			box(c, sf, org, p, Vector3(PX, 0, PX), Vector3(1 - PX, 1, 1 - PX), t6c, tint, 0, 0b110011, false)
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 1, 1), t6c, tint, 0, 0b001100, true)
		BlockDB.M_SHORT:
			if d.props.get("flat", false):
				flat(c, sf, org, p, all, tint, 0.02, hash3(lx, int(wy), lz) & 3)
			else:
				box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, float(d.props.get("height", 0.5)), 1), _six(id), tint)
		BlockDB.M_RAIL:
			_rail(c, sf, org, p, id, meta)
		BlockDB.M_WIRE:
			_wire(c, sf, org, p, id, meta)
		BlockDB.M_BUTTON:
			_button(c, sf, org, p, meta, all, tint)
		BlockDB.M_PLATE:
			var pressed := meta > 0
			box(c, sf, org, p, Vector3(PX, 0, PX), Vector3(1 - PX, PX * (0.5 if pressed else 1.0), 1 - PX), all, tint)
		BlockDB.M_LEVER:
			_lever(c, sf, org, p, id, meta)
		BlockDB.M_REPEATER, BlockDB.M_COMPARATOR:
			_diode(c, sf, org, p, id, meta, m == BlockDB.M_COMPARATOR)
		BlockDB.M_PISTON:
			_piston(c, sf, org, p, id, meta)
		BlockDB.M_PISTON_HEAD:
			_piston_head(c, sf, org, p, id, meta)
		BlockDB.M_LANTERN:
			_lantern(c, sf, org, p, all, (meta & 1) == 1)
		BlockDB.M_CHAIN:
			_chain(c, sf, org, p, all, meta & 3)
		BlockDB.M_CAMPFIRE:
			_campfire(c, sf, org, p, id, meta)
		BlockDB.M_ANVIL:
			var t6a := [lay(id, "side"), lay(id, "side"), lay(id, "top"), lay(id, "side"), lay(id, "side"), lay(id, "side")]
			var xa := (meta + 1) & 3
			box(c, sf, org, p, px3(2, 0, 2), px3(14, 4, 14), t6a, tint, xa)
			box(c, sf, org, p, px3(4, 4, 3), px3(12, 5, 13), t6a, tint, xa)
			box(c, sf, org, p, px3(6, 5, 4), px3(10, 10, 12), t6a, tint, xa)
			box(c, sf, org, p, px3(3, 10, 0), px3(13, 16, 16), t6a, tint, xa)
		BlockDB.M_HOPPER:
			_hopper(c, sf, org, p, id, meta)
		BlockDB.M_CAULDRON:
			_cauldron(c, sf, org, p, id, meta, d)
		BlockDB.M_BREWING:
			var base := lay(id, "base")
			for bp in [Vector2(2, 8), Vector2(9, 3), Vector2(9, 12)]:
				box(c, sf, org, p, px3(bp.x - 1, 0, bp.y - 1), px3(bp.x + 5, 2, bp.y + 5) if false else px3(bp.x + 4, 2, bp.y + 4), base, tint)
			box(c, sf, org, p, px3(7, 0, 7), px3(9, 14, 9), all, tint)
		BlockDB.M_ENCHANT:
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 0.75, 1), _six(id), tint)
			c.job.block_entities.append([Vector3i(lx, int(wy), lz), v])
		BlockDB.M_END_FRAME:
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 13.0 / 16.0, 1), _six(id), tint)
			if (meta & 4) != 0:
				box(c, sf, org, p, px3(4, 13, 4), px3(12, 16, 12), lay(id, "eye"), tint)
		BlockDB.M_PORTAL:
			var xfp_axis := 0 if (meta & 1) == 0 else 1
			box(c, sf, org, p, px3(0, 0, 6), px3(16, 16, 10), all, white, xfp_axis, 0b110011)
		BlockDB.M_END_PORTAL:
			if d.props.get("gateway", false):
				box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 1, 1), all, white)
			else:
				flat(c, sf, org, p, all, white, 0.75, 0, false)
		BlockDB.M_FIRE:
			cross(c, sf, org, p, all, white, 1.3, 1.1)
			var lt := light_at(c, p)
			var fr := frames_of(all)
			for f in [E, W, S, N]:
				var cs: Array = _face_corners(f, Vector3(0.05, 0, 0.05), Vector3(0.95, 1.2, 0.95))
				sf.quad2(org + cs[0], org + cs[1], org + cs[2], org + cs[3], Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1),
					float(all), fr, white, 1.0, lt.x, lt.y)
		BlockDB.M_BED:
			_bed(c, sf, org, p, id, meta)
		BlockDB.M_CHEST:
			c.job.block_entities.append([Vector3i(lx, int(wy), lz), v])
		BlockDB.M_POT:
			_pot(c, sf, org, p, id, meta, d)
		BlockDB.M_CAKE:
			var bites := meta & 7
			var t6k := [lay(id, "side"), lay(id, "inner") if bites > 0 else lay(id, "side"), lay(id, "top"), lay(id, "bottom"), lay(id, "side"), lay(id, "side")]
			box(c, sf, org, p, px3(1 + bites * 2, 0, 1), px3(15, 8, 15), t6k, white)
		BlockDB.M_EGG:
			var kind: String = d.props.get("kind", "dragon")
			if kind == "dragon":
				box(c, sf, org, p, px3(6, 15, 6), px3(10, 16, 10), all, white)
				box(c, sf, org, p, px3(5, 14, 5), px3(11, 15, 11), all, white)
				box(c, sf, org, p, px3(4, 13, 4), px3(12, 14, 12), all, white)
				box(c, sf, org, p, px3(3, 11, 3), px3(13, 13, 13), all, white)
				box(c, sf, org, p, px3(2, 8, 2), px3(14, 11, 14), all, white)
				box(c, sf, org, p, px3(1, 3, 1), px3(15, 8, 15), all, white)
				box(c, sf, org, p, px3(2, 1, 2), px3(14, 3, 14), all, white)
				box(c, sf, org, p, px3(3, 0, 3), px3(13, 1, 13), all, white)
			elif kind == "turtle":
				for k in (meta & 3) + 1:
					var ox: float = [5.0, 9.0, 3.0, 10.0][k]
					var oz: float = [5.0, 9.0, 10.0, 3.0][k]
					box(c, sf, org, p, px3(ox, 0, oz), px3(ox + 4, 5, oz + 4), all, white)
			else:
				box(c, sf, org, p, px3(1, 0, 2), px3(15, 16, 14), all, white)
		BlockDB.M_HEAD:
			_head(c, sf, org, p, id, meta)
		BlockDB.M_CANDLE:
			_candles(c, sf, org, p, id, meta)
		BlockDB.M_PICKLE:
			var cnt := (meta & 3) + 1
			for k in cnt:
				var pxs: float = [6.0, 3.0, 10.0, 8.0][k]
				var pzs: float = [6.0, 9.0, 4.0, 11.0][k]
				box(c, sf, org, p, px3(pxs, 0, pzs), px3(pxs + 4, 6, pzs + 4), all, white)
		BlockDB.M_DRIPSTONE, BlockDB.M_SPIKE:
			cross(c, sf, org, p, all, white, 1.0, 1.0, Vector3.ZERO, (meta & 1) == 1)
		BlockDB.M_AMETHYST:
			var size: float = d.props.get("size", 0.5)
			_amethyst(c, sf, org, p, all, meta & 7, size)
		BlockDB.M_VINE:
			_vine(c, sf, org, p, all, tint, meta)
		BlockDB.M_LILY:
			flat(c, sf, org, p, all, tint, 0.015, hash3(lx, int(wy), lz) & 3)
		BlockDB.M_BAMBOO:
			box(c, sf, org, p, px3(6.5, 0, 6.5), px3(9.5, 16, 9.5), all, white)
			if (meta & 3) > 0:
				cross(c, c.surfaces[1], org, p, lay(id, "leaves"), white, 1.0, 1.0)
		BlockDB.M_SCAFFOLD:
			var t6s := _six(id)
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 1, 1), t6s, white, 0, 63, true)
			box(c, sf, org, p, Vector3(0.999, 0.001, 0.999), Vector3(0.001, 0.999, 0.001), t6s, white, 0, 63, false)
		BlockDB.M_LECTERN:
			var xl := meta & 3
			box(c, sf, org, p, px3(0, 0, 0), px3(16, 2, 16), lay(id, "base"), white, xl)
			box(c, sf, org, p, px3(4, 2, 4), px3(12, 13, 12), lay(id, "side"), white, xl)
			box(c, sf, org, p, px3(0, 12, 1), px3(16, 16, 15), [lay(id, "side"), lay(id, "side"), lay(id, "top"), lay(id, "base"), lay(id, "front"), lay(id, "side")], white, xl)
		BlockDB.M_GRINDSTONE:
			var xg := meta & 3
			var leg := lay(id, "leg")
			box(c, sf, org, p, px3(2, 0, 6), px3(4, 7, 10), leg, white, xg)
			box(c, sf, org, p, px3(12, 0, 6), px3(14, 7, 10), leg, white, xg)
			box(c, sf, org, p, px3(1, 7, 5), px3(4, 13, 11), lay(id, "pivot"), white, xg)
			box(c, sf, org, p, px3(12, 7, 5), px3(15, 13, 11), lay(id, "pivot"), white, xg)
			box(c, sf, org, p, px3(4, 4, 2), px3(12, 16, 14), [lay(id, "all"), lay(id, "all"), lay(id, "round"), lay(id, "round"), lay(id, "round"), lay(id, "round")], white, xg)
		BlockDB.M_STONECUTTER:
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 9.0 / 16.0, 1), _six(id), white)
			var saw := lay(id, "saw")
			var lt2 := light_at(c, p)
			var xs := meta & 3
			var s0 := xfp(xs, Vector3(0.5, 9.0 / 16.0, 0.05))
			var s1 := xfp(xs, Vector3(0.5, 1.0, 0.05))
			var s2 := xfp(xs, Vector3(0.5, 1.0, 0.95))
			var s3 := xfp(xs, Vector3(0.5, 9.0 / 16.0, 0.95))
			c.surfaces[1].quad2(org + s0, org + s1, org + s2, org + s3, Vector2(0, 0.45), Vector2(0, 0), Vector2(1, 0), Vector2(1, 0.45),
				float(saw), frames_of(saw), white, 1.0, lt2.x, lt2.y)
		BlockDB.M_BELL:
			var xb := meta & 3
			var bar := lay(id, "bar")
			var post := lay(id, "post")
			box(c, sf, org, p, px3(0, 0, 6), px3(2, 16, 10), post, white, xb)
			box(c, sf, org, p, px3(14, 0, 6), px3(16, 16, 10), post, white, xb)
			box(c, sf, org, p, px3(2, 13, 7), px3(14, 15, 9), bar, white, xb)
			box(c, sf, org, p, px3(5, 6, 5), px3(11, 13, 11), all, white, xb)
			box(c, sf, org, p, px3(4, 4, 4), px3(12, 6, 12), all, white, xb)
		BlockDB.M_ROD:
			var xr: int = XF_BY_DIR[mini(meta & 7, 5)]
			box(c, sf, org, p, px3(7, 0, 7), px3(9, 15, 9), all, white, xr)
			box(c, sf, org, p, px3(6, 15, 6), px3(10, 16, 10), all, white, xr)
		BlockDB.M_CHORUS:
			_chorus(c, sf, org, p, id, meta, d)
		BlockDB.M_DAYLIGHT:
			var inv := (meta & 1) == 1
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 6.0 / 16.0, 1),
				[lay(id, "side"), lay(id, "side"), lay(id, "inverted" if inv else "top"), lay(id, "side"), lay(id, "side"), lay(id, "side")], white)
		BlockDB.M_BEACON:
			box(c, c.surfaces[1], org, p, Vector3(0, 0, 0), Vector3(1, 1, 1), lay(id, "all"), white)
			box(c, sf, org, p, px3(3, 3, 3), px3(13, 14, 13), lay(id, "core"), white, 0, 63, false)
			box(c, sf, org, p, px3(2, 0.1, 2), px3(14, 3, 14), lay(id, "base"), white, 0, 63, false)
			c.job.block_entities.append([Vector3i(lx, int(wy), lz), v])
		BlockDB.M_HEAVY:
			box(c, sf, org, p, px3(4, 0, 4), px3(12, 8, 12), all, white)
		BlockDB.M_SHELF:
			var xh := meta & 3
			var side := lay(id, "side")
			var t6h := [side, side, side, side, lay(id, "all"), side]
			box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 1, 1), t6h, white, xh)
		BlockDB.M_HANGING:
			cross(c, sf, org, p, all, white, 1.0, 0.8, Vector3(0, 0.2, 0))
		BlockDB.M_SIGN:
			box(c, sf, org, p, px3(0, 8, 7), px3(16, 16, 9), all, white, meta & 3)


static func _six(id: int, meta: int = 0) -> Array:
	var d: BlockDef = BlockDB.defs[id]
	var out := []
	for f in 6:
		out.append(BlockDB.layer_of(BlockDB.face_texture_name(d, meta, f)))
	return out


static func _stairs(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int, tint: Color) -> void:
	var facing := meta & 3
	var top := (meta & 4) != 0
	var t6 := _six(id)
	var xf := facing
	var yb0 := 0.5 if top else 0.0
	var ys0 := 0.0 if top else 0.5
	# bottom (or top) half slab
	box(c, sf, org, p, Vector3(0, yb0, 0), Vector3(1, yb0 + 0.5, 1), t6, tint, 0)
	# shape from neighbours (Minecraft stair shape rules)
	var shape := 0   # 0 straight, 1 outer left, 2 outer right, 3 inner left, 4 inner right
	var fdir: int = Vox.facing_to_dir(facing)
	var back_v := c.pb[p + Mesher.NOFF[fdir]]
	var front_v := c.pb[p + Mesher.NOFF[Vox.OPPOSITE[fdir]]]
	if BlockDB.model[back_v & 0xFFF] == BlockDB.M_STAIRS and ((back_v >> 12) & 4) == (meta & 4):
		var nf := (back_v >> 12) & 3
		if (nf & 1) != (facing & 1):
			shape = 1 if nf == ((facing + 3) & 3) else 2
	if shape == 0 and BlockDB.model[front_v & 0xFFF] == BlockDB.M_STAIRS and ((front_v >> 12) & 4) == (meta & 4):
		var nf2 := (front_v >> 12) & 3
		if (nf2 & 1) != (facing & 1):
			shape = 3 if nf2 == ((facing + 3) & 3) else 4
	match shape:
		0:
			box(c, sf, org, p, Vector3(0, ys0, 0.5), Vector3(1, ys0 + 0.5, 1), t6, tint, xf)
		1:
			box(c, sf, org, p, Vector3(0, ys0, 0.5), Vector3(0.5, ys0 + 0.5, 1), t6, tint, xf)
		2:
			box(c, sf, org, p, Vector3(0.5, ys0, 0.5), Vector3(1, ys0 + 0.5, 1), t6, tint, xf)
		3:
			box(c, sf, org, p, Vector3(0, ys0, 0.5), Vector3(1, ys0 + 0.5, 1), t6, tint, xf)
			box(c, sf, org, p, Vector3(0, ys0, 0), Vector3(0.5, ys0 + 0.5, 0.5), t6, tint, xf)
		4:
			box(c, sf, org, p, Vector3(0, ys0, 0.5), Vector3(1, ys0 + 0.5, 1), t6, tint, xf)
			box(c, sf, org, p, Vector3(0.5, ys0, 0), Vector3(1, ys0 + 0.5, 0.5), t6, tint, xf)


static func _conn(c: Ctx, p: int, table: PackedByteArray) -> Array:
	var out := [false, false, false, false]   # E W S N
	var dirs := [E, W, S, N]
	for i in 4:
		var nv := c.pb[p + Mesher.NOFF[dirs[i]]]
		out[i] = table[nv & 0xFFF] == 1
	return out


static func _fence(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, layer: int, tint: Color) -> void:
	box(c, sf, org, p, px3(6, 0, 6), px3(10, 16, 10), layer, tint)
	var cn := _conn(c, p, connect_fence)
	var rng := [[px3(10, 0, 7), px3(16, 0, 9)], [px3(0, 0, 7), px3(6, 0, 9)], [px3(7, 0, 10), px3(9, 0, 16)], [px3(7, 0, 0), px3(9, 0, 6)]]
	for i in 4:
		if cn[i]:
			for yy in [[6, 9], [12, 15]]:
				var a: Vector3 = rng[i][0]
				var b: Vector3 = rng[i][1]
				box(c, sf, org, p, Vector3(a.x, yy[0] * PX, a.z), Vector3(b.x, yy[1] * PX, b.z), layer, tint)


static func _gate(c: Ctx, sf: MeshSurface, org: Vector3, p: int, meta: int, layer: int, tint: Color) -> void:
	var xf := meta & 3
	var open := (meta & 4) != 0
	box(c, sf, org, p, px3(0, 5, 7), px3(2, 16, 9), layer, tint, xf)
	box(c, sf, org, p, px3(14, 5, 7), px3(16, 16, 9), layer, tint, xf)
	if not open:
		box(c, sf, org, p, px3(2, 6, 7), px3(14, 9, 9), layer, tint, xf)
		box(c, sf, org, p, px3(2, 12, 7), px3(14, 15, 9), layer, tint, xf)
		box(c, sf, org, p, px3(6, 9, 7), px3(10, 12, 9), layer, tint, xf)
	else:
		for sx in [0.0, 14.0]:
			box(c, sf, org, p, px3(sx, 6, 9), px3(sx + 2, 9, 16), layer, tint, xf)
			box(c, sf, org, p, px3(sx, 12, 9), px3(sx + 2, 15, 16), layer, tint, xf)
			box(c, sf, org, p, px3(sx, 9, 13), px3(sx + 2, 12, 15), layer, tint, xf)


static func _wall(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, layer: int, tint: Color) -> void:
	var cn := _conn(c, p, connect_wall)
	var straight: bool = (cn[0] and cn[1] and not cn[2] and not cn[3]) or (cn[2] and cn[3] and not cn[0] and not cn[1])
	var above := c.pb[p + Mesher.PSS] & 0xFFF
	if not straight or above != 0:
		box(c, sf, org, p, px3(4, 0, 4), px3(12, 16, 12), layer, tint)
	var arm := [[px3(12, 0, 5), px3(16, 14, 11)], [px3(0, 0, 5), px3(4, 14, 11)], [px3(5, 0, 12), px3(11, 14, 16)], [px3(5, 0, 0), px3(11, 14, 4)]]
	if straight and above == 0:
		if cn[0]:
			box(c, sf, org, p, px3(0, 0, 5), px3(16, 14, 11), layer, tint)
		else:
			box(c, sf, org, p, px3(5, 0, 0), px3(11, 14, 16), layer, tint)
		return
	for i in 4:
		if cn[i]:
			box(c, sf, org, p, arm[i][0], arm[i][1], layer, tint)


static func _pane(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, layer: int, edge: int, tint: Color) -> void:
	var cn := _conn(c, p, connect_pane)
	var t6 := [edge, edge, edge, edge, layer, layer]
	var t6x := [layer, layer, edge, edge, edge, edge]
	var any: bool = cn[0] or cn[1] or cn[2] or cn[3]
	if not any:
		box(c, sf, org, p, px3(7, 0, 7), px3(9, 16, 9), [layer, layer, edge, edge, layer, layer], tint)
		return
	box(c, sf, org, p, px3(7, 0, 7), px3(9, 16, 9), [edge, edge, edge, edge, edge, edge], tint, 0, 0b001100)
	if cn[0]:
		box(c, sf, org, p, px3(9, 0, 7), px3(16, 16, 9), t6, tint, 0, 0b111101)
	if cn[1]:
		box(c, sf, org, p, px3(0, 0, 7), px3(7, 16, 9), t6, tint, 0, 0b111110)
	if cn[2]:
		box(c, sf, org, p, px3(7, 0, 9), px3(9, 16, 16), t6x, tint, 0, 0b011111)
	if cn[3]:
		box(c, sf, org, p, px3(7, 0, 0), px3(9, 16, 7), t6x, tint, 0, 0b101111)


static func _door(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int, tint: Color) -> void:
	var facing := meta & 3
	var open := (meta & 4) != 0
	var upper := (meta & 8) != 0
	var tex := lay(id, "top" if upper else "bottom")
	var t6 := [tex, tex, tex, tex, tex, tex]
	if not open:
		box(c, sf, org, p, px3(0, 0, 0), px3(16, 16, 3), t6, tint, facing, 63, false,
			[Rect2(0, 0, 3.0 / 16.0, 1), Rect2(13.0 / 16.0, 0, 3.0 / 16.0, 1), Rect2(0, 0, 1, 3.0 / 16.0), Rect2(0, 13.0 / 16.0, 1, 3.0 / 16.0), Rect2(1, 0, -1, 1), Rect2(0, 0, 1, 1)])
	else:
		box(c, sf, org, p, px3(0, 0, 0), px3(3, 16, 16), t6, tint, facing, 63, false,
			[Rect2(0, 0, 1, 1), Rect2(1, 0, -1, 1), Rect2(0, 0, 3.0 / 16.0, 1), Rect2(0, 0, 3.0 / 16.0, 1), Rect2(0, 0, 3.0 / 16.0, 1), Rect2(13.0 / 16.0, 0, 3.0 / 16.0, 1)])


static func _trapdoor(c: Ctx, sf: MeshSurface, org: Vector3, p: int, meta: int, layer: int, tint: Color) -> void:
	var facing := meta & 3
	var open := (meta & 4) != 0
	var top := (meta & 8) != 0
	if open:
		box(c, sf, org, p, px3(0, 0, 0), px3(16, 16, 3), layer, tint, facing, 63, false)
	elif top:
		box(c, sf, org, p, px3(0, 13, 0), px3(16, 16, 16), layer, tint, facing, 63, false)
	else:
		box(c, sf, org, p, px3(0, 0, 0), px3(16, 3, 16), layer, tint, facing, 63, false)


static func _torch(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var d: BlockDef = BlockDB.defs[id]
	var layer := lay(id, "all")
	if d.name == "redstone_torch" and (meta & 8) != 0:
		layer = lay(id, "off")
	var att := meta & 7
	var lt := light_at(c, p)
	var fr := frames_of(layer)
	var white := Color(1, 1, 1)
	var bot := Vector3(0.5, 0.0, 0.5)
	var top := Vector3(0.5, 10.0 / 16.0, 0.5)
	if att >= 1 and att <= 4:
		# wall torch: base against the wall, leaning into the room (facing = att - 1)
		var fvec: Vector3i = Vox.H_FACING_VEC[att - 1]
		bot = Vector3(0.5 - fvec.x * 0.5 + fvec.x * 0.06, 3.5 / 16.0, 0.5 - fvec.z * 0.5 + fvec.z * 0.06)
		top = bot + Vector3(fvec.x * 0.25, 10.0 / 16.0, fvec.z * 0.25)
	var hw := 1.0 / 16.0
	var cs := [Vector3(-hw, 0, -hw), Vector3(hw, 0, -hw), Vector3(hw, 0, hw), Vector3(-hw, 0, hw)]
	var faces := [[3, 2, S], [1, 0, N], [2, 1, E], [0, 3, W]]
	var u0 := 7.0 / 16.0
	var u1 := 9.0 / 16.0
	for fc in faces:
		var a: Vector3 = cs[fc[0]]
		var b: Vector3 = cs[fc[1]]
		sf.quad(org + bot + a, org + top + a, org + top + b, org + bot + b, Vector2(u0, 1), Vector2(u0, 6.0 / 16.0),
			Vector2(u1, 6.0 / 16.0), Vector2(u1, 1), float(layer), fr, white, Mesher.SHADE[fc[2]] * 1.1, lt.x, lt.y)
	# top cap
	sf.quad(org + top + cs[0], org + top + cs[1], org + top + cs[2], org + top + cs[3], Vector2(u0, 6.0 / 16.0),
		Vector2(u1, 6.0 / 16.0), Vector2(u1, 8.0 / 16.0), Vector2(u0, 8.0 / 16.0), float(layer), fr, white, 1.2, lt.x, lt.y)


static func _wall_plane(c: Ctx, sf: MeshSurface, org: Vector3, p: int, facing: int, layer: int, off: float) -> void:
	var lt := light_at(c, p)
	var fr := frames_of(layer)
	var white := Color(1, 1, 1)
	# canonical: attached to north wall (plane at z = off), facing south; rotate by facing
	var q := [Vector3(0, 0, off), Vector3(0, 1, off), Vector3(1, 1, off), Vector3(1, 0, off)]
	var r := []
	for v in q:
		r.append(xfp(facing, v))
	c.surfaces[1].quad2(org + r[0], org + r[1], org + r[2], org + r[3], Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1),
		float(layer), fr, white, 0.9, lt.x, lt.y)


static func _rail(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var d: BlockDef = BlockDB.defs[id]
	var special: bool = String(d.props.get("rail", "")) != ""
	var shape := meta & (7 if special else 15)
	var powered := special and (meta & 8) != 0
	var layer := lay(id, "on" if powered else "all")
	var white := Color(1, 1, 1)
	var lt := light_at(c, p)
	var fr := frames_of(layer)
	var y := 1.0 / 16.0
	if shape >= 6 and not special:
		var cl := lay(id, "corner")
		var rotc: int = [0, 1, 2, 3][shape - 6]
		flat(c, sf, org, p, cl, white, y, rotc)
		return
	if shape <= 1:
		flat(c, sf, org, p, layer, white, y, 1 if shape == 1 else 0)
		return
	# ascending 2 east, 3 west, 4 north, 5 south (MC order)
	var h0 := [0.0, 0.0, 0.0, 0.0]   # corners (x0z0, x1z0, x1z1, x0z1)
	match shape:
		2: h0 = [0.0, 1.0, 1.0, 0.0]
		3: h0 = [1.0, 0.0, 0.0, 1.0]
		4: h0 = [1.0, 1.0, 0.0, 0.0]
		5: h0 = [0.0, 0.0, 1.0, 1.0]
	var rot := 1 if shape <= 3 else 0
	var uv := [Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1)]
	var ru := [uv[(0 + rot) & 3], uv[(1 + rot) & 3], uv[(2 + rot) & 3], uv[(3 + rot) & 3]]
	sf.quad2(org + Vector3(0, y + h0[0], 0), org + Vector3(1, y + h0[1], 0), org + Vector3(1, y + h0[2], 1), org + Vector3(0, y + h0[3], 1),
		ru[0], ru[1], ru[2], ru[3], float(layer), fr, white, 1.0, lt.x, lt.y)


static func _wire(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var power := meta & 15
	var tint := Mesher.tint_color(6, power, c.colors, 0)
	var dot := lay(id, "all")
	var line := lay(id, "line")
	var pb := c.pb
	var cn := [false, false, false, false]
	var up := [false, false, false, false]
	var dirs := [E, W, S, N]
	var above_solid := BlockDB.full[pb[p + Mesher.PSS] & 0xFFF] == 1
	for i in 4:
		var np := p + Mesher.NOFF[dirs[i]]
		var nid := pb[np] & 0xFFF
		if redstone_conn[nid] == 1:
			cn[i] = true
		elif BlockDB.full[nid] == 0 and (pb[np - Mesher.PSS] & 0xFFF) == ids["redstone_wire"]:
			cn[i] = true
		elif BlockDB.full[nid] == 1 and not above_solid and (pb[np + Mesher.PSS] & 0xFFF) == ids["redstone_wire"]:
			cn[i] = true
			up[i] = true
	var y := 1.0 / 64.0
	var ns: bool = cn[2] or cn[3]
	var ew: bool = cn[0] or cn[1]
	var nconn := int(cn[0]) + int(cn[1]) + int(cn[2]) + int(cn[3])
	if nconn == 0:
		flat(c, sf, org, p, dot, tint, y)
	else:
		if ns and not ew:
			flat(c, sf, org, p, line, tint, y, 0, true, 0.0, 0.0 if cn[3] else 0.3, 1.0, 1.0 if cn[2] else 0.7)
		elif ew and not ns:
			flat(c, sf, org, p, line, tint, y, 1, true, 0.0 if cn[1] else 0.3, 0.0, 1.0 if cn[0] else 0.7, 1.0)
		else:
			flat(c, sf, org, p, dot, tint, y)
			if cn[0]:
				flat(c, sf, org, p, line, tint, y + 0.001, 1, true, 0.5, 0.0, 1.0, 1.0)
			if cn[1]:
				flat(c, sf, org, p, line, tint, y + 0.001, 1, true, 0.0, 0.0, 0.5, 1.0)
			if cn[2]:
				flat(c, sf, org, p, line, tint, y + 0.001, 0, true, 0.0, 0.5, 1.0, 1.0)
			if cn[3]:
				flat(c, sf, org, p, line, tint, y + 0.001, 0, true, 0.0, 0.0, 1.0, 0.5)
	var lt := light_at(c, p)
	for i in 4:
		if up[i]:
			var off := 0.99 if i == 0 or i == 2 else 0.01
			var q := []
			match i:
				0: q = [Vector3(off, 0, 0), Vector3(off, 1, 0), Vector3(off, 1, 1), Vector3(off, 0, 1)]
				1: q = [Vector3(off, 0, 1), Vector3(off, 1, 1), Vector3(off, 1, 0), Vector3(off, 0, 0)]
				2: q = [Vector3(1, 0, off), Vector3(1, 1, off), Vector3(0, 1, off), Vector3(0, 0, off)]
				3: q = [Vector3(0, 0, off), Vector3(0, 1, off), Vector3(1, 1, off), Vector3(1, 0, off)]
			sf.quad2(org + q[0], org + q[1], org + q[2], org + q[3], Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1),
				float(line), 1.0, tint, 1.0, lt.x, lt.y)


## Attached parts (buttons, levers): meta & 7 = direction from the part toward its support block.
static func _attach_xf(att: int) -> int:
	# canonical models sit on the floor (support below) -> map support direction to orientation
	match att:
		Vox.DOWN: return 4
		Vox.UP: return 5
		Vox.EAST: return 7
		Vox.WEST: return 6
		Vox.SOUTH: return 9
		Vox.NORTH: return 8
	return 4


static func _button(c: Ctx, sf: MeshSurface, org: Vector3, p: int, meta: int, layer: int, tint: Color) -> void:
	var att := meta & 7
	var pressed := (meta & 8) != 0
	var h := 1.0 if pressed else 2.0
	box(c, sf, org, p, px3(5, 0, 6), px3(11, h, 10), layer, tint, _attach_xf(att), 63, false)


static func _lever(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var att := meta & 7
	var on := (meta & 8) != 0
	var xf := _attach_xf(att)
	var white := Color(1, 1, 1)
	box(c, sf, org, p, px3(5, 0, 4), px3(11, 3, 12), lay(id, "base"), white, xf, 63, false)
	var handle := lay(id, "all")
	var lt := light_at(c, p)
	var tilt := 0.22 if on else -0.22
	var b0 := Vector3(0.5, 2.0 / 16.0, 0.5)
	var t0 := Vector3(0.5, 11.0 / 16.0, 0.5 + tilt)
	var hw := 1.0 / 16.0
	for side in [Vector3(hw, 0, 0), Vector3(-hw, 0, 0), Vector3(0, 0, hw), Vector3(0, 0, -hw)]:
		var perp := Vector3(side.z, 0, -side.x)
		var q0 := xfp(xf, b0 + side - perp)
		var q1 := xfp(xf, t0 + side - perp)
		var q2 := xfp(xf, t0 + side + perp)
		var q3 := xfp(xf, b0 + side + perp)
		sf.quad2(org + q0, org + q1, org + q2, org + q3, Vector2(7.0 / 16.0, 1), Vector2(7.0 / 16.0, 0.2), Vector2(9.0 / 16.0, 0.2),
			Vector2(9.0 / 16.0, 1), float(handle), 1.0, white, 0.9, lt.x, lt.y)


static func _diode(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int, comparator: bool) -> void:
	var facing := meta & 3
	var d: BlockDef = BlockDB.defs[id]
	var powered: bool = d.props.get("powered", false) if not comparator else (meta & 8) != 0
	var top := lay(id, "top_on" if powered else "top")
	var side := lay(id, "side")
	var white := Color(1, 1, 1)
	# canonical: output toward south (+Z); facing meta points toward input (MC repeater facing = input side)
	var xf := (facing + 2) & 3
	box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 2.0 / 16.0, 1), [side, side, top, side, side, side], white, xf)
	var torch := lay(id, "torch" if powered else "torch_off")
	var t6 := torch
	if not comparator:
		var delay := (meta >> 2) & 3
		box(c, sf, org, p, px3(7, 2, 2), px3(9, 7, 4), t6, white, xf, 63, false)
		box(c, sf, org, p, px3(7, 2, 6 + delay * 2), px3(9, 7, 8 + delay * 2), t6, white, xf, 63, false)
	else:
		var sub := (meta & 4) != 0
		box(c, sf, org, p, px3(4, 2, 11), px3(6, 7, 13), t6, white, xf, 63, false)
		box(c, sf, org, p, px3(10, 2, 11), px3(12, 7, 13), t6, white, xf, 63, false)
		box(c, sf, org, p, px3(7, 2, 2), px3(9, 4 if not sub else 6, 4), lay(id, "torch" if sub else "torch_off"), white, xf, 63, false)


static func _piston(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var dir := meta & 7
	if dir > 5:
		dir = 2
	var extended := (meta & 8) != 0
	var xf: int = XF_BY_DIR[dir]
	var top := lay(id, "top")
	var side := lay(id, "side")
	var bottom := lay(id, "bottom")
	var inner := lay(id, "inner")
	var white := Color(1, 1, 1)
	if not extended:
		box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 1, 1), [side, side, top, bottom, side, side], white, xf)
	else:
		box(c, sf, org, p, Vector3(0, 0, 0), Vector3(1, 12.0 / 16.0, 1), [side, side, inner, bottom, side, side], white, xf, 63, true,
			[Rect2(0, 4.0 / 16.0, 1, 12.0 / 16.0), Rect2(0, 4.0 / 16.0, 1, 12.0 / 16.0), null, null, Rect2(0, 4.0 / 16.0, 1, 12.0 / 16.0), Rect2(0, 4.0 / 16.0, 1, 12.0 / 16.0)])


static func _piston_head(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var dir := meta & 7
	if dir > 5:
		dir = 2
	var sticky := (meta & 8) != 0
	var xf: int = XF_BY_DIR[dir]
	var face := lay(id, "sticky" if sticky else "top")
	var side := lay(id, "side")
	var white := Color(1, 1, 1)
	box(c, sf, org, p, Vector3(0, 12.0 / 16.0, 0), Vector3(1, 1, 1), [side, side, face, lay(id, "top"), side, side], white, xf, 63, true,
		[Rect2(0, 0, 1, 4.0 / 16.0), Rect2(0, 0, 1, 4.0 / 16.0), null, null, Rect2(0, 0, 1, 4.0 / 16.0), Rect2(0, 0, 1, 4.0 / 16.0)])
	box(c, sf, org, p, px3(6, -4, 6), px3(10, 12, 10), side, white, xf, 0b110011, false,
		[Rect2(0, 0, 1, 4.0 / 16.0), Rect2(0, 0, 1, 4.0 / 16.0), null, null, Rect2(0, 0, 1, 4.0 / 16.0), Rect2(0, 0, 1, 4.0 / 16.0)])


static func _lantern(c: Ctx, sf: MeshSurface, org: Vector3, p: int, layer: int, hanging: bool) -> void:
	var y0 := 1.0 if hanging else 0.0
	var white := Color(1, 1, 1)
	box(c, sf, org, p, px3(5, y0, 5), px3(11, y0 + 7, 11), layer, white, 0, 63, false,
		[Rect2(4.0 / 16.0, 5.0 / 16.0, 8.0 / 16.0, 9.0 / 16.0), Rect2(4.0 / 16.0, 5.0 / 16.0, 8.0 / 16.0, 9.0 / 16.0), Rect2(5.0 / 16.0, 3.0 / 16.0, 6.0 / 16.0, 2.0 / 16.0),
		Rect2(5.0 / 16.0, 3.0 / 16.0, 6.0 / 16.0, 2.0 / 16.0), Rect2(4.0 / 16.0, 5.0 / 16.0, 8.0 / 16.0, 9.0 / 16.0), Rect2(4.0 / 16.0, 5.0 / 16.0, 8.0 / 16.0, 9.0 / 16.0)])
	box(c, sf, org, p, px3(6, y0 + 7, 6), px3(10, y0 + 9, 10), layer, white, 0, 63, false,
		[Rect2(5.0 / 16.0, 3.0 / 16.0, 6.0 / 16.0, 2.0 / 16.0), Rect2(5.0 / 16.0, 3.0 / 16.0, 6.0 / 16.0, 2.0 / 16.0), Rect2(5.0 / 16.0, 3.0 / 16.0, 6.0 / 16.0, 2.0 / 16.0),
		Rect2(5.0 / 16.0, 3.0 / 16.0, 6.0 / 16.0, 2.0 / 16.0), Rect2(5.0 / 16.0, 3.0 / 16.0, 6.0 / 16.0, 2.0 / 16.0), Rect2(5.0 / 16.0, 3.0 / 16.0, 6.0 / 16.0, 2.0 / 16.0)])
	if hanging:
		var lt := light_at(c, p)
		c.surfaces[1].quad2(org + Vector3(0.45, y0 * PX + 9 * PX, 0.5), org + Vector3(0.45, 1, 0.5), org + Vector3(0.55, 1, 0.5),
			org + Vector3(0.55, y0 * PX + 9 * PX, 0.5), Vector2(7.0 / 16.0, 3.0 / 16.0), Vector2(7.0 / 16.0, 0), Vector2(9.0 / 16.0, 0),
			Vector2(9.0 / 16.0, 3.0 / 16.0), float(layer), 1.0, white, 0.9, lt.x, lt.y)


static func _chain(c: Ctx, sf: MeshSurface, org: Vector3, p: int, layer: int, axis: int) -> void:
	var lt := light_at(c, p)
	var white := Color(1, 1, 1)
	var xf := 4
	if axis == 1:
		xf = 6
	elif axis == 2:
		xf = 8
	for k in 2:
		var a := Vector3(0.4, 0, 0.4) if k == 0 else Vector3(0.4, 0, 0.6)
		var b := Vector3(0.6, 0, 0.6) if k == 0 else Vector3(0.6, 0, 0.4)
		var q0 := xfp(xf, a)
		var q1 := xfp(xf, a + Vector3(0, 1, 0))
		var q2 := xfp(xf, b + Vector3(0, 1, 0))
		var q3 := xfp(xf, b)
		sf.quad2(org + q0, org + q1, org + q2, org + q3, Vector2(0.35, 1), Vector2(0.35, 0), Vector2(0.65, 0), Vector2(0.65, 1),
			float(layer), 1.0, white, 0.9, lt.x, lt.y)


static func _campfire(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var xf := meta & 3
	var lit := (meta & 4) == 0
	var logl := lay(id, "lit" if lit else "all")
	var white := Color(1, 1, 1)
	box(c, sf, org, p, px3(1, 0, 0), px3(5, 4, 16), logl, white, xf)
	box(c, sf, org, p, px3(11, 0, 0), px3(15, 4, 16), logl, white, xf)
	box(c, sf, org, p, px3(0, 3, 1), px3(16, 7, 5), logl, white, xf)
	box(c, sf, org, p, px3(0, 3, 11), px3(16, 7, 15), logl, white, xf)
	box(c, sf, org, p, px3(5, 0, 5), px3(11, 1, 11), logl, white, xf)
	if lit:
		cross(c, c.surfaces[1], org, p, lay(id, "fire"), white, 1.0, 1.0)


static func _hopper(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var out := lay(id, "all")
	var inside := lay(id, "inside")
	var top := lay(id, "top")
	var white := Color(1, 1, 1)
	box(c, sf, org, p, px3(0, 10, 0), px3(16, 16, 2), [out, out, top, out, inside, out], white)
	box(c, sf, org, p, px3(0, 10, 14), px3(16, 16, 16), [out, out, top, out, out, inside], white)
	box(c, sf, org, p, px3(0, 10, 2), px3(2, 16, 14), [inside, out, top, out, out, out], white)
	box(c, sf, org, p, px3(14, 10, 2), px3(16, 16, 14), [out, inside, top, out, out, out], white)
	box(c, sf, org, p, px3(2, 10, 2), px3(14, 11, 14), [out, out, inside, out, out, out], white)
	box(c, sf, org, p, px3(4, 4, 4), px3(12, 10, 12), out, white)
	var dir := meta & 7
	if dir == Vox.DOWN or dir > 5 or dir == Vox.UP:
		box(c, sf, org, p, px3(6, 0, 6), px3(10, 4, 10), out, white)
	else:
		var hv: Vector3i = Vox.DIR_VEC[dir]
		var cx := 8.0 + hv.x * 6.0
		var cz := 8.0 + hv.z * 6.0
		box(c, sf, org, p, px3(cx - 2, 4, cz - 2), px3(cx + 2, 8, cz + 2), out, white)


static func _cauldron(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int, d: BlockDef) -> void:
	var side := lay(id, "side")
	var top := lay(id, "top")
	var inner := lay(id, "inner")
	var bottom := lay(id, "bottom")
	var white := Color(1, 1, 1)
	var t6 := [side, side, top, bottom, side, side]
	box(c, sf, org, p, px3(0, 3, 0), px3(16, 16, 2), [side, side, top, bottom, inner, side], white)
	box(c, sf, org, p, px3(0, 3, 14), px3(16, 16, 16), [side, side, top, bottom, side, inner], white)
	box(c, sf, org, p, px3(0, 3, 2), px3(2, 16, 14), [inner, side, top, bottom, side, side], white)
	box(c, sf, org, p, px3(14, 3, 2), px3(16, 16, 14), [side, inner, top, bottom, side, side], white)
	box(c, sf, org, p, px3(2, 3, 2), px3(14, 4, 14), [side, side, inner, bottom, side, side], white)
	for lg in [[0, 0], [12, 0], [0, 12], [12, 12]]:
		box(c, sf, org, p, px3(lg[0], 0, lg[1]), px3(lg[0] + 4, 3, lg[1] + 4), t6, white)
	var kind: String = d.props.get("kind", "cauldron")
	var level := meta & 3
	if kind == "composter":
		level = meta & 15
		if level > 0:
			var lh := 4.0 + mini(level, 7) * 1.5
			flat(c, sf, org, p, lay(id, "ready" if level >= 8 else "inner"), white, lh / 16.0, 0, false, 2.0 / 16.0, 2.0 / 16.0, 14.0 / 16.0, 14.0 / 16.0)
	elif level > 0:
		var lava := (meta & 4) != 0
		var fl: int = BlockDB.LAVA if lava else BlockDB.WATER
		var tint := white if lava else c.colors[2]
		var y := (4.0 + level * 3.0) / 16.0
		var tl := lay(fl, "top")
		var surf: MeshSurface = c.surfaces[0 if lava else 2]
		var lt := light_at(c, p)
		surf.quad(org + Vector3(2.0 / 16.0, y, 2.0 / 16.0), org + Vector3(14.0 / 16.0, y, 2.0 / 16.0), org + Vector3(14.0 / 16.0, y, 14.0 / 16.0),
			org + Vector3(2.0 / 16.0, y, 14.0 / 16.0), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1), float(tl), frames_of(tl),
			tint, 1.0, lt.x, maxf(lt.y, 15.0 if lava else 0.0))


static func _bed(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var facing := meta & 3
	var head := (meta & 4) != 0
	var top := lay(id, "top" if head else "foot")
	var side := lay(id, "side")
	var endt := lay(id, "end" if head else "foot_end")
	var bottom := lay(id, "bottom")
	var white := Color(1, 1, 1)
	# canonical: head toward south (+Z) when facing south
	var xf := facing
	var t6 := [side, side, top, bottom, endt if head else side, side if head else endt]
	box(c, sf, org, p, px3(0, 3, 0), px3(16, 9, 16), t6, white, xf, 63, true,
		[Rect2(0, 7.0 / 16.0, 1, 6.0 / 16.0), Rect2(0, 7.0 / 16.0, 1, 6.0 / 16.0), null, null, Rect2(0, 7.0 / 16.0, 1, 6.0 / 16.0), Rect2(0, 7.0 / 16.0, 1, 6.0 / 16.0)])
	var legs := [[0, 13], [13, 13]] if head else [[0, 0], [13, 0]]
	for lg in legs:
		box(c, sf, org, p, px3(lg[0], 0, lg[1]), px3(lg[0] + 3, 3, lg[1] + 3), side, white, xf)


static func _pot(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int, d: BlockDef) -> void:
	var white := Color(1, 1, 1)
	var all := lay(id, "all")
	if d.props.get("decorated", false):
		box(c, sf, org, p, px3(1, 0, 1), px3(15, 13, 15), [all, all, lay(id, "top"), lay(id, "top"), all, all], white)
		box(c, sf, org, p, px3(4, 13, 4), px3(12, 16, 12), all, white)
		return
	box(c, sf, org, p, px3(5, 0, 5), px3(11, 1, 11), all, white)
	box(c, sf, org, p, px3(5, 1, 5), px3(6, 6, 11), all, white)
	box(c, sf, org, p, px3(10, 1, 5), px3(11, 6, 11), all, white)
	box(c, sf, org, p, px3(6, 1, 5), px3(10, 6, 6), all, white)
	box(c, sf, org, p, px3(6, 1, 10), px3(10, 6, 11), all, white)
	box(c, sf, org, p, px3(6, 1, 6), px3(10, 4, 10), lay(id, "dirt"), white)
	if meta > 0:
		var plant := FlowerPot.plant_for(meta)
		if plant != "":
			var pid := BlockDB.id(plant)
			if pid > 0:
				var pl := lay(pid, "all", "top")
				var tint := white
				if BlockDB.defs[pid].tint != 0:
					tint = Mesher.tint_color(BlockDB.defs[pid].tint & 31, 0, c.colors, 0)
				cross(c, c.surfaces[1], org + Vector3(0, 4.0 / 16.0, 0), p, pl, tint, 0.75, 0.75)


static func _head(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var facing := meta & 3
	var wall := (meta & 4) != 0
	var white := Color(1, 1, 1)
	var t6 := [lay(id, "side"), lay(id, "side"), lay(id, "top"), lay(id, "top"), lay(id, "front"), lay(id, "back", "side")]
	if wall:
		box(c, sf, org, p, px3(4, 4, 0), px3(12, 12, 8), t6, white, facing, 63, false)
	else:
		box(c, sf, org, p, px3(4, 0, 4), px3(12, 8, 12), t6, white, facing, 63, false)


static func _candles(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int) -> void:
	var cnt := (meta & 3) + 1
	var lit := (meta & 4) != 0
	var all := lay(id, "all")
	var white := Color(1, 1, 1)
	var pos := [[7, 7], [5, 7], [9, 8], [7, 5]]
	var hs := [6, 5, 4, 5]
	for k in cnt:
		var x: float = pos[k][0]
		var z: float = pos[k][1]
		box(c, sf, org, p, px3(x, 0, z), px3(x + 2, hs[k], z + 2), all, white, 0, 63, false,
			[Rect2(6.0 / 16.0, 5.0 / 16.0, 2.0 / 16.0, 11.0 / 16.0), Rect2(6.0 / 16.0, 5.0 / 16.0, 2.0 / 16.0, 11.0 / 16.0), Rect2(6.0 / 16.0, 5.0 / 16.0, 2.0 / 16.0, 2.0 / 16.0),
			Rect2(6.0 / 16.0, 5.0 / 16.0, 2.0 / 16.0, 2.0 / 16.0), Rect2(6.0 / 16.0, 5.0 / 16.0, 2.0 / 16.0, 11.0 / 16.0), Rect2(6.0 / 16.0, 5.0 / 16.0, 2.0 / 16.0, 11.0 / 16.0)])
		if lit:
			var fire := BlockDB.layer_of("fire")
			var lt := light_at(c, p)
			var fp := org + px3(x + 1, hs[k], z + 1)
			c.surfaces[1].quad2(fp + Vector3(-0.06, 0, 0), fp + Vector3(-0.06, 0.18, 0), fp + Vector3(0.06, 0.18, 0), fp + Vector3(0.06, 0, 0),
				Vector2(0.3, 1), Vector2(0.3, 0.3), Vector2(0.7, 0.3), Vector2(0.7, 1), float(fire), frames_of(fire), white, 1.0, lt.x, 15.0)


static func _amethyst(c: Ctx, sf: MeshSurface, org: Vector3, p: int, layer: int, dir: int, size: float) -> void:
	var xf: int = XF_BY_DIR[mini(dir, 5)]
	var lt := light_at(c, p)
	var white := Color(1, 1, 1)
	var h := size
	var lo := 0.5 - size * 0.45
	var hi := 0.5 + size * 0.45
	for k in 2:
		var a := Vector3(lo, 0, lo) if k == 0 else Vector3(lo, 0, hi)
		var b := Vector3(hi, 0, hi) if k == 0 else Vector3(hi, 0, lo)
		var q0 := xfp(xf, a)
		var q1 := xfp(xf, a + Vector3(0, h, 0))
		var q2 := xfp(xf, b + Vector3(0, h, 0))
		var q3 := xfp(xf, b)
		sf.quad2(org + q0, org + q1, org + q2, org + q3, Vector2(0, 1), Vector2(0, 1.0 - h), Vector2(1, 1.0 - h), Vector2(1, 1),
			float(layer), 1.0, white, 1.0, lt.x, lt.y)


static func _vine(c: Ctx, sf: MeshSurface, org: Vector3, p: int, layer: int, tint: Color, meta: int) -> void:
	# meta bits: 1 = south, 2 = west, 4 = north, 8 = east (facing indices); 0 = on ceiling/floor style
	var lt := light_at(c, p)
	var fr := frames_of(layer)
	var m := meta if meta != 0 else 4
	for f in 4:
		if (m & (1 << f)) == 0:
			continue
		# attached to the wall in direction facing f
		var q := [Vector3(0, 0, 0.02), Vector3(0, 1, 0.02), Vector3(1, 1, 0.02), Vector3(1, 0, 0.02)]
		var xf := (f + 2) & 3
		var r := []
		for v in q:
			r.append(xfp(xf, v))
		sf.quad2(org + r[0], org + r[1], org + r[2], org + r[3], Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1),
			float(layer), fr, tint, 0.9, lt.x, lt.y)


static func _chorus(c: Ctx, sf: MeshSurface, org: Vector3, p: int, id: int, meta: int, d: BlockDef) -> void:
	var white := Color(1, 1, 1)
	var all := lay(id, "all")
	if d.props.get("flower", false):
		box(c, sf, org, p, px3(1, 1, 1), px3(15, 15, 15), all if meta < 5 else lay(id, "on"), white)
		return
	box(c, sf, org, p, px3(4, 4, 4), px3(12, 12, 12), all, white)
	var pb := c.pb
	var chp: int = ids["chorus_plant"]
	var chf: int = ids["chorus_flower"]
	var es: int = ids["end_stone"]
	var arms := [[E, px3(12, 4, 4), px3(16, 12, 12)], [W, px3(0, 4, 4), px3(4, 12, 12)], [U, px3(4, 12, 4), px3(12, 16, 12)],
		[D, px3(4, 0, 4), px3(12, 4, 12)], [S, px3(4, 4, 12), px3(12, 12, 16)], [N, px3(4, 4, 0), px3(12, 12, 4)]]
	for a in arms:
		var nid := pb[p + Mesher.NOFF[a[0]]] & 0xFFF
		if nid == chp or nid == chf or (a[0] == D and nid == es):
			box(c, sf, org, p, a[1], a[2], all, white)
