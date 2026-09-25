class_name Mesher
extends RefCounted
## Builds render geometry for 16^3 chunk sections on worker threads.
## Cube faces use hidden-face culling, Minecraft-style ambient occlusion and smooth lighting.
## Special block models are delegated to MeshModels. Output is 3 passes: opaque, cutout, translucent.

const PS := 18            # padded section size
const PSS := 324          # PS * PS
const PSIZE := PSS * PS   # padded volume (18^3)
const SHADE := [0.6, 0.6, 1.0, 0.5, 0.8, 0.8]
const AOF := [1.0, 0.8, 0.62, 0.45]

static var FPOS := PackedVector3Array()   # 6 faces x 4 corners
static var FUV := PackedVector2Array()    # 4 rotations x 6 faces x 4 corners
static var NOFF := PackedInt32Array()     # face -> padded neighbour offset
static var AO := PackedInt32Array()       # face*12 + corner*3 + k -> padded offset (side1, side2, diagonal)
static var DYE_TINT := PackedColorArray()
static var snow_cover := PackedByteArray()
static var LUT := PackedInt32Array()
static var ZERO_SECTION := PackedInt32Array()
static var ready := false


static func init_tables() -> void:
	if ready:
		return
	var corners := [
		[Vector3(1, 0, 1), Vector3(1, 1, 1), Vector3(1, 1, 0), Vector3(1, 0, 0)],   # east
		[Vector3(0, 0, 0), Vector3(0, 1, 0), Vector3(0, 1, 1), Vector3(0, 0, 1)],   # west
		[Vector3(0, 1, 0), Vector3(1, 1, 0), Vector3(1, 1, 1), Vector3(0, 1, 1)],   # up
		[Vector3(0, 0, 0), Vector3(0, 0, 1), Vector3(1, 0, 1), Vector3(1, 0, 0)],   # down
		[Vector3(0, 0, 1), Vector3(0, 1, 1), Vector3(1, 1, 1), Vector3(1, 0, 1)],   # south
		[Vector3(1, 0, 0), Vector3(1, 1, 0), Vector3(0, 1, 0), Vector3(0, 0, 0)],   # north
	]
	var uvs := [
		[Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1)],
		[Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1)],
		[Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1)],
		[Vector2(0, 0), Vector2(0, 1), Vector2(1, 1), Vector2(1, 0)],
		[Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1)],
		[Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1)],
	]
	var normals := [Vector3i(1, 0, 0), Vector3i(-1, 0, 0), Vector3i(0, 1, 0), Vector3i(0, -1, 0), Vector3i(0, 0, 1), Vector3i(0, 0, -1)]
	FPOS.resize(24)
	FUV.resize(96)
	NOFF.resize(6)
	AO.resize(72)
	for f in 6:
		var nrm: Vector3i = normals[f]
		NOFF[f] = nrm.x + nrm.z * PS + nrm.y * PSS
		for i in 4:
			var c: Vector3 = corners[f][i]
			FPOS[f * 4 + i] = c
			var uv: Vector2 = uvs[f][i]
			FUV[0 * 24 + f * 4 + i] = uv
			FUV[1 * 24 + f * 4 + i] = Vector2(1.0 - uv.y, uv.x)
			FUV[2 * 24 + f * 4 + i] = Vector2(1.0 - uv.x, 1.0 - uv.y)
			FUV[3 * 24 + f * 4 + i] = Vector2(uv.y, 1.0 - uv.x)
			# tangent axes = the two axes not along the normal
			var t := []
			for axis in 3:
				var nv: int = [nrm.x, nrm.y, nrm.z][axis]
				if nv == 0:
					t.append(axis)
			var s := [0, 0]
			for k in 2:
				var cv: float = [c.x, c.y, c.z][t[k]]
				s[k] = 1 if cv > 0.5 else -1
			var e1 := Vector3i.ZERO
			var e2 := Vector3i.ZERO
			e1[t[0]] = s[0]
			e2[t[1]] = s[1]
			var side1 := nrm + e1
			var side2 := nrm + e2
			var diag := nrm + e1 + e2
			AO[f * 12 + i * 3 + 0] = side1.x + side1.z * PS + side1.y * PSS
			AO[f * 12 + i * 3 + 1] = side2.x + side2.z * PS + side2.y * PSS
			AO[f * 12 + i * 3 + 2] = diag.x + diag.z * PS + diag.y * PSS
	DYE_TINT.resize(16)
	var i2 := 0
	for d in BlockCatalog.DYES:
		DYE_TINT[i2] = Color.html(TexPalettes.DYE[d]).lerp(Color.WHITE, 0.08)
		i2 += 1
	LUT.resize(256)
	for b in 256:
		LUT[b] = ((b >> 4) << 8) | (b & 15)
	ZERO_SECTION.resize(4096)
	snow_cover.resize(BlockDB.count)
	for n in ["snow", "snow_block", "powder_snow"]:
		snow_cover[BlockDB.id(n)] = 1
	ready = true


## A meshing request prepared on the main thread and executed on a worker.
class Job:
	var cx := 0
	var cz := 0
	var min_y := 0
	var height := 256
	var chunks: Array = []          # 9 Chunk (dx + 1 + (dz + 1) * 3), centre = 4
	var sections := PackedInt32Array()
	var version := 0
	var results: Dictionary = {}    # sy -> [MeshSurface opaque, cutout, translucent]
	var block_entities: Array = []  # [Vector3i local pos, block value] needing entity renderers
	var fancy_leaves := true
	var done := false


static func run_job(job: Job) -> void:
	for sy in job.sections:
		job.results[sy] = mesh_section(job, sy)
	job.done = true


static func _fill_padded(job: Job, sy: int, pb: PackedInt32Array, pl: PackedByteArray, rowfull: PackedByteArray) -> int:
	var H := job.height
	var base_yr := sy * 16 - 1
	var full := BlockDB.full
	var bedrock := BlockDB.BEDROCK
	var nfull := 0
	var ch: Array = job.chunks
	for py in PS:
		var yr := base_yr + py
		var prow := py * PSS
		if yr < 0:
			for i in PSS:
				pb[prow + i] = bedrock
				pl[prow + i] = 0
			for i in PS:
				rowfull[py * PS + i] = 1
			nfull += PSS
			continue
		if yr >= H:
			for i in PSS:
				pb[prow + i] = 0
				pl[prow + i] = 0xF0
			for i in PS:
				rowfull[py * PS + i] = 0
			continue
		var ybase := yr << 8
		for pz in PS:
			var lz := pz - 1
			var zc := 0 if lz < 0 else (2 if lz > 15 else 1)
			var lz2 := (lz & 15) << 4
			var dst := prow + pz * PS
			var before := nfull
			# west cell
			var cw: Chunk = ch[zc * 3]
			var cm: Chunk = ch[zc * 3 + 1]
			var ce: Chunk = ch[zc * 3 + 2]
			if cw != null:
				var v0 := _bg(cw, ybase | lz2 | 15)
				pb[dst] = v0
				pl[dst] = _lg(cw, ybase | lz2 | 15)
				nfull += full[v0 & 0xFFF]
			else:
				pb[dst] = 0
				pl[dst] = 0xF0
			if cm != null:
				var src := ybase | lz2
				for x in 16:
					var v := _bg(cm, src + x)
					pb[dst + 1 + x] = v
					pl[dst + 1 + x] = _lg(cm, src + x)
					nfull += full[v & 0xFFF]
			else:
				for x in 16:
					pb[dst + 1 + x] = 0
					pl[dst + 1 + x] = 0xF0
			if ce != null:
				var v1 := _bg(ce, ybase | lz2)
				pb[dst + 17] = v1
				pl[dst + 17] = _lg(ce, ybase | lz2)
				nfull += full[v1 & 0xFFF]
			else:
				pb[dst + 17] = 0
				pl[dst + 17] = 0xF0
			rowfull[py * PS + pz] = 1 if nfull - before == PS else 0
	return nfull


## Safe light read: neighbours may still be waiting for the light thread or be sized for a shorter
## dimension, so the array can be short. Missing light reads as full sky.
static func _lg(c: Chunk, i: int) -> int:
	var arr := c.light       # one snapshot: the light thread may swap the array concurrently
	if i < 0 or i >= arr.size():
		return 0xF0
	return arr[i]


## Safe block read with the same reasoning (missing blocks read as air).
static func _bg(c: Chunk, i: int) -> int:
	var arr := c.blocks
	if i < 0 or i >= arr.size():
		return 0
	return arr[i]


static func mesh_section(job: Job, sy: int) -> Array:
	var surfaces := [MeshSurface.new(), MeshSurface.new(), MeshSurface.new()]
	var center: Chunk = job.chunks[4]
	# quick reject: empty section
	var start := sy << 12
	if center.blocks.slice(start, start + 4096) == ZERO_SECTION:
		return surfaces
	if start + 4096 > center.blocks.size():
		return surfaces
	var pb := PackedInt32Array()
	pb.resize(PSIZE)
	var pl := PackedByteArray()
	pl.resize(PSIZE)
	var rowfull := PackedByteArray()
	rowfull.resize(PSS)
	var nfull := _fill_padded(job, sy, pb, pl, rowfull)
	if nfull == PSIZE:
		return surfaces
	var model := BlockDB.model
	var render := BlockDB.render
	var full := BlockDB.full
	var cull_same := BlockDB.cull_same
	var aoc := BlockDB.ao_caster
	var face_tex := BlockDB.face_tex
	var lframes := BlockDB.layer_frames
	var snowy := BlockDB.snowy_side
	var waterlogged := BlockDB.waterlogged
	var fluid_t := BlockDB.fluid
	var LT := LUT
	var colors := center.colors
	var M_CUBE := BlockDB.M_CUBE
	var M_LEAVES := BlockDB.M_LEAVES
	var M_FLUID := BlockDB.M_FLUID
	var fancy := job.fancy_leaves
	var y0 := sy * 16 + job.min_y
	var ctx := MeshModels.Ctx.new()
	ctx.pb = pb
	ctx.pl = pl
	ctx.surfaces = surfaces
	ctx.colors = colors
	ctx.job = job
	ctx.sy = sy
	var white := Color(1, 1, 1)
	var S0: MeshSurface = surfaces[0]
	S0.reserve(4096)
	# cached write target (current render layer)
	var cur_r := -1
	var csf: MeshSurface = null
	var V: PackedVector3Array
	var U: PackedVector2Array
	var CO: PackedColorArray
	var LB: PackedByteArray
	var IX: PackedInt32Array
	var n := 0
	var ni := 0
	var cap := 0
	for ly in 16:
		var wy := float(y0 + ly)
		var rr := (ly + 1) * PS
		for lz in 16:
			var rc := rr + lz + 1
			if rowfull[rc] == 1 and rowfull[rc - PS] == 1 and rowfull[rc + PS] == 1 and rowfull[rc - 1] == 1 and rowfull[rc + 1] == 1:
				continue
			var prow := ((ly + 1) * PS + (lz + 1)) * PS + 1
			for lx in 16:
				var p := prow + lx
				var v := pb[p]
				if v == 0:
					continue
				var id := v & 0xFFF
				var m := model[id]
				if m != M_CUBE and m != M_LEAVES:
					if cur_r >= 0:
						csf.n = n
						csf.ni = ni
						cur_r = -1
					if m == M_FLUID:
						# fast skip for fluid cells fully enclosed by the same fluid or opaque blocks
						var fk := fluid_t[id]
						var enclosed := true
						for f2 in 6:
							var q := pb[p + NOFF[f2]] & 0xFFF
							if full[q] == 1 and f2 != 2:
								continue
							if fluid_t[q] == fk or (fk == 1 and waterlogged[q] == 1):
								continue
							enclosed = false
							break
						if enclosed:
							continue
						MeshModels.fluid(ctx, p, lx, ly, lz, wy, v)
					else:
						if waterlogged[id] == 1:
							MeshModels.fluid(ctx, p, lx, ly, lz, wy, BlockDB.WATER)
						MeshModels.emit(ctx, p, lx, ly, lz, wy, v, m)
					continue
				var meta := (v >> 12) & 15
				var ftb := ((id << 4) | meta) * 6
				var rl := render[id]
				var colbase := ((lz << 4) | lx) * 3
				var snow_layer := -1
				if snowy[id] >= 0 and snow_cover[pb[p + PSS] & 0xFFF] == 1:
					snow_layer = snowy[id]
				var cs := cull_same[id]
				var is_leaf := m == M_LEAVES
				var bx := float(lx)
				var bz := float(lz)
				for f in 6:
					var np := p + NOFF[f]
					var nv := pb[np]
					var nid := nv & 0xFFF
					if full[nid] == 1:
						continue
					if nid == id and (cs == 1 or (is_leaf and not fancy)):
						continue
					if rl != cur_r:
						if cur_r >= 0:
							csf.n = n
							csf.ni = ni
						csf = surfaces[rl]
						cur_r = rl
						n = csf.n
						ni = csf.ni
						cap = csf.cap
						V = csf.verts
						U = csf.uvs
						CO = csf.cols
						LB = csf.light
						IX = csf.idx
					if n + 4 > cap:
						csf.n = n
						csf.ni = ni
						csf.reserve(4)
						cap = csf.cap
						V = csf.verts
						U = csf.uvs
						CO = csf.cols
						LB = csf.light
						IX = csf.idx
					var ft := face_tex[ftb + f]
					var layer := ft & 0xFFFF
					var rot := (ft >> 16) & 3
					var tk := (ft >> 18) & 63
					if snow_layer >= 0 and f != 2 and f != 3:
						layer = snow_layer
						tk = 0
					var frames := float(lframes[layer]) if layer < lframes.size() else 1.0
					var tint := white
					if tk != 0:
						var kind := tk & 31
						if kind == 1 or kind == 10:
							tint = colors[colbase]
						elif kind == 2:
							tint = colors[colbase + 1]
						elif kind == 3:
							tint = colors[colbase + 2]
						else:
							tint = tint_color(kind, meta, colors, colbase)
						if (tk & 32) != 0 or kind == 10:
							frames = -frames
					var fl := LT[pl[np]]
					var sh: float = SHADE[f]
					var ab := f * 12
					# corner 0
					var o1 := p + AO[ab + 0]
					var o2 := p + AO[ab + 1]
					var o3 := p + AO[ab + 2]
					var a1 := aoc[pb[o1] & 0xFFF]
					var a2 := aoc[pb[o2] & 0xFFF]
					var occ := 3 if (a1 == 1 and a2 == 1) else (a1 + a2 + aoc[pb[o3] & 0xFFF])
					var l1 := LT[pl[o1]]
					var l2 := LT[pl[o2]]
					var l3 := LT[pl[o3]]
					var k0 := fl + (l1 if l1 != 0 else fl) + (l2 if l2 != 0 else fl) + (l3 if l3 != 0 else fl)
					var sh0: float = sh * AOF[occ]
					# corner 1
					o1 = p + AO[ab + 3]
					o2 = p + AO[ab + 4]
					o3 = p + AO[ab + 5]
					a1 = aoc[pb[o1] & 0xFFF]
					a2 = aoc[pb[o2] & 0xFFF]
					occ = 3 if (a1 == 1 and a2 == 1) else (a1 + a2 + aoc[pb[o3] & 0xFFF])
					l1 = LT[pl[o1]]
					l2 = LT[pl[o2]]
					l3 = LT[pl[o3]]
					var k1 := fl + (l1 if l1 != 0 else fl) + (l2 if l2 != 0 else fl) + (l3 if l3 != 0 else fl)
					var sh1: float = sh * AOF[occ]
					# corner 2
					o1 = p + AO[ab + 6]
					o2 = p + AO[ab + 7]
					o3 = p + AO[ab + 8]
					a1 = aoc[pb[o1] & 0xFFF]
					a2 = aoc[pb[o2] & 0xFFF]
					occ = 3 if (a1 == 1 and a2 == 1) else (a1 + a2 + aoc[pb[o3] & 0xFFF])
					l1 = LT[pl[o1]]
					l2 = LT[pl[o2]]
					l3 = LT[pl[o3]]
					var k2 := fl + (l1 if l1 != 0 else fl) + (l2 if l2 != 0 else fl) + (l3 if l3 != 0 else fl)
					var sh2: float = sh * AOF[occ]
					# corner 3
					o1 = p + AO[ab + 9]
					o2 = p + AO[ab + 10]
					o3 = p + AO[ab + 11]
					a1 = aoc[pb[o1] & 0xFFF]
					a2 = aoc[pb[o2] & 0xFFF]
					occ = 3 if (a1 == 1 and a2 == 1) else (a1 + a2 + aoc[pb[o3] & 0xFFF])
					l1 = LT[pl[o1]]
					l2 = LT[pl[o2]]
					l3 = LT[pl[o3]]
					var k3 := fl + (l1 if l1 != 0 else fl) + (l2 if l2 != 0 else fl) + (l3 if l3 != 0 else fl)
					var sh3: float = sh * AOF[occ]
					var fi := f * 4
					var ui := rot * 24 + fi
					var org := Vector3(bx, wy, bz)
					V[n] = org + FPOS[fi]
					V[n + 1] = org + FPOS[fi + 1]
					V[n + 2] = org + FPOS[fi + 2]
					V[n + 3] = org + FPOS[fi + 3]
					var uoff := Vector2(layer * 2.0, frames * 2.0)
					U[n] = FUV[ui] + uoff
					U[n + 1] = FUV[ui + 1] + uoff
					U[n + 2] = FUV[ui + 2] + uoff
					U[n + 3] = FUV[ui + 3] + uoff
					CO[n] = Color(tint.r, tint.g, tint.b, sh0)
					CO[n + 1] = Color(tint.r, tint.g, tint.b, sh1)
					CO[n + 2] = Color(tint.r, tint.g, tint.b, sh2)
					CO[n + 3] = Color(tint.r, tint.g, tint.b, sh3)
					var lo := n << 2
					# k = 4 corner samples summed, packed (sky << 8 | block); x17/4 maps 0..15 to 0..255
					LB[lo] = ((k0 >> 8) * 17) >> 2
					LB[lo + 1] = ((k0 & 255) * 17) >> 2
					LB[lo + 3] = 255
					LB[lo + 4] = ((k1 >> 8) * 17) >> 2
					LB[lo + 5] = ((k1 & 255) * 17) >> 2
					LB[lo + 7] = 255
					LB[lo + 8] = ((k2 >> 8) * 17) >> 2
					LB[lo + 9] = ((k2 & 255) * 17) >> 2
					LB[lo + 11] = 255
					LB[lo + 12] = ((k3 >> 8) * 17) >> 2
					LB[lo + 13] = ((k3 & 255) * 17) >> 2
					LB[lo + 15] = 255
					if sh0 + sh2 < sh1 + sh3 or (sh0 + sh2 == sh1 + sh3 and k0 + k2 < k1 + k3):
						IX[ni] = n + 1
						IX[ni + 1] = n + 2
						IX[ni + 2] = n + 3
						IX[ni + 3] = n + 1
						IX[ni + 4] = n + 3
						IX[ni + 5] = n
					else:
						IX[ni] = n
						IX[ni + 1] = n + 1
						IX[ni + 2] = n + 2
						IX[ni + 3] = n
						IX[ni + 4] = n + 2
						IX[ni + 5] = n + 3
					n += 4
					ni += 6
	if cur_r >= 0:
		csf.n = n
		csf.ni = ni
	return surfaces


static func tint_color(kind: int, meta: int, colors: PackedColorArray, colbase: int) -> Color:
	match kind:
		1, 10:
			return colors[colbase]
		2:
			return colors[colbase + 1]
		3:
			return colors[colbase + 2]
		4:
			return Color(0.38, 0.6, 0.38)
		5:
			return Color(0.5, 0.65, 0.33)
		6:
			var pw := float(meta) / 15.0
			return Color(0.3 + pw * 0.7, pw * pw * 0.12, 0.02)
		7:
			var age := float(meta & 7) / 7.0
			return Color(age * 0.9, 0.8 - age * 0.25, 0.1 + (1.0 - age) * 0.1)
		8:
			return Color(0.13, 0.5, 0.19)
		9:
			return Color(0.57, 0.75, 0.28)
		11:
			return colors[colbase].lerp(Color(0.78, 0.7, 0.42), 0.6)
	if kind >= 16:
		return DYE_TINT[kind - 16]
	return Color(1, 1, 1)
