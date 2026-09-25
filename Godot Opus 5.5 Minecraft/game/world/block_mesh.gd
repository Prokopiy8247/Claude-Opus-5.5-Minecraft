class_name BlockMesh
extends RefCounted
## Builds a standalone ArrayMesh for a single block state using the same geometry code as chunk
## meshing (MeshModels). Used for held blocks, dropped items, falling blocks, TNT and icon renders.
## The mesh is centred on X/Z (x,z in -0.5..0.5) with y from 0 to 1.

static var _cache: Dictionary = {}
static var _mat_cache: Dictionary = {}


static func build(v: int, tint := Color(-1, 0, 0), centered := true) -> ArrayMesh:
	var key := v
	if _cache.has(key) and tint.r < 0.0:
		return _cache[key]
	var pb := PackedInt32Array()
	pb.resize(Mesher.PSIZE)
	var pl := PackedByteArray()
	pl.resize(Mesher.PSIZE)
	pl.fill(0xF0)
	var center := (1 * Mesher.PS + 1) * Mesher.PS + 1   # padded (1,1,1) -> local (0,0,0)
	pb[center] = v
	var surfaces := [MeshSurface.new(), MeshSurface.new(), MeshSurface.new()]
	var colors := PackedColorArray()
	colors.resize(768)
	var dcol := BiomeDB.grass[BiomeDB.PLAINS] if BiomeDB.inited else Color(0.55, 0.74, 0.35)
	var fcol := BiomeDB.foliage[BiomeDB.PLAINS] if BiomeDB.inited else Color(0.47, 0.67, 0.18)
	var wcol := BiomeDB.water[BiomeDB.PLAINS] if BiomeDB.inited else Color(0.25, 0.46, 0.89)
	for i in 256:
		colors[i * 3] = dcol
		colors[i * 3 + 1] = fcol
		colors[i * 3 + 2] = wcol
	var ctx := MeshModels.Ctx.new()
	ctx.pb = pb
	ctx.pl = pl
	ctx.surfaces = surfaces
	ctx.colors = colors
	var job := Mesher.Job.new()
	ctx.job = job
	var id := v & 0xFFF
	var m := BlockDB.model[id]
	if m == BlockDB.M_CUBE or m == BlockDB.M_LEAVES:
		_cube(surfaces[BlockDB.render[id]], v, colors)
	elif m == BlockDB.M_FLUID:
		MeshModels.fluid(ctx, center, 0, 0, 0, 0.0, v)
	elif m == BlockDB.M_CHEST:
		_chest(surfaces[1], v)
	else:
		MeshModels.emit(ctx, center, 0, 0, 0, 0.0, v, m)
	var mesh := ArrayMesh.new()
	var off := Vector3(-0.5, 0, -0.5) if centered else Vector3.ZERO
	for r in 3:
		var sf: MeshSurface = surfaces[r]
		if sf.is_empty():
			continue
		var arr := sf.to_arrays()
		var verts: PackedVector3Array = arr[Mesh.ARRAY_VERTEX]
		for i in verts.size():
			verts[i] += off
		arr[Mesh.ARRAY_VERTEX] = verts
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arr, [], {}, MeshSurface.format_flags())
		mesh.surface_set_material(mesh.get_surface_count() - 1, material(r))
	if tint.r < 0.0:
		_cache[key] = mesh
	return mesh


static func _cube(sf: MeshSurface, v: int, colors: PackedColorArray) -> void:
	var id := v & 0xFFF
	var meta := (v >> 12) & 15
	var ftb := ((id << 4) | meta) * 6
	for f in 6:
		var ft := BlockDB.face_tex[ftb + f]
		var layer := ft & 0xFFFF
		var rot := (ft >> 16) & 3
		var tk := (ft >> 18) & 63
		var frames := float(BlockDB.layer_frames[layer]) if layer < BlockDB.layer_frames.size() else 1.0
		var tint := Color(1, 1, 1)
		if tk != 0:
			tint = Mesher.tint_color(tk & 31, meta, colors, 0)
			if (tk & 32) != 0 or (tk & 31) == 10:
				frames = -frames
		var fi := f * 4
		var ui := rot * 24 + fi
		sf.quad(Mesher.FPOS[fi], Mesher.FPOS[fi + 1], Mesher.FPOS[fi + 2], Mesher.FPOS[fi + 3],
			Mesher.FUV[ui], Mesher.FUV[ui + 1], Mesher.FUV[ui + 2], Mesher.FUV[ui + 3], float(layer), frames, tint,
			Mesher.SHADE[f], 15.0, 0.0)


## Simple chest model (used until/unless a Blender model replaces it for block-entity rendering).
static func _chest(sf: MeshSurface, v: int) -> void:
	var id := v & 0xFFF
	var layer := MeshModels.lay(id, "all")
	var px := 1.0 / 16.0
	var boxes := [[Vector3(1, 0, 1), Vector3(15, 10, 15)], [Vector3(1, 10, 1), Vector3(15, 14, 15)], [Vector3(7, 7, 0), Vector3(9, 11, 1)]]
	for b in boxes:
		var a: Vector3 = b[0] * px
		var c: Vector3 = b[1] * px
		for f in 6:
			var cs: Array = MeshModels._face_corners(f, a, c)
			sf.quad(cs[0], cs[1], cs[2], cs[3], MeshModels.uv_for(f, cs[0]), MeshModels.uv_for(f, cs[1]),
				MeshModels.uv_for(f, cs[2]), MeshModels.uv_for(f, cs[3]), float(layer), 1.0, Color(1, 1, 1), Mesher.SHADE[f], 15.0, 0.0)


static func material(render_layer: int) -> ShaderMaterial:
	if _mat_cache.has(render_layer):
		return _mat_cache[render_layer]
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/voxel_entity.gdshader")
	m.set_shader_parameter("blocks", TextureLibrary.block_array)
	m.set_shader_parameter("alpha_cut", 0.5 if render_layer != 2 else 0.1)
	_mat_cache[render_layer] = m
	return m


static func clear_cache() -> void:
	_cache.clear()
