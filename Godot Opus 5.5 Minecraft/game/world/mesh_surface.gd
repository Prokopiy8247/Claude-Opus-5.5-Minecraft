class_name MeshSurface
extends RefCounted
## Growable vertex/index buffers for one render pass of a chunk section (28 bytes per vertex).
##   VERTEX  : position
##   UV      : (u + 2 * layer, v + 2 * frames)   frames < 0 => tint only marked texels
##   COLOR   : (tint.r, tint.g, tint.b, shade * ao)
##   CUSTOM0 : RGBA8 (sky / 15, block / 15, 0, 1)
## Arrays are pre-sized and written by index (fast in GDScript); n / ni are the used counts.

var verts := PackedVector3Array()
var uvs := PackedVector2Array()
var cols := PackedColorArray()
var light := PackedByteArray()
var idx := PackedInt32Array()
var n := 0      # vertices used
var ni := 0     # indices used
var cap := 0    # vertex capacity


func is_empty() -> bool:
	return n == 0


func reserve(verts_needed: int) -> void:
	if n + verts_needed <= cap:
		return
	var nc := maxi(cap * 2, maxi(256, n + verts_needed))
	verts.resize(nc)
	uvs.resize(nc)
	cols.resize(nc)
	light.resize(nc * 4)
	idx.resize(nc / 4 * 6 + 6)
	cap = nc


## Quad with uniform shade/light (model faces). Vertices clockwise seen from the front.
func quad(p0: Vector3, p1: Vector3, p2: Vector3, p3: Vector3, u0: Vector2, u1: Vector2, u2: Vector2, u3: Vector2,
		layer: float, frames: float, tint: Color, shade: float, sky: float, blk: float) -> void:
	if n + 4 > cap:
		reserve(4)
	var off := Vector2(layer * 2.0, frames * 2.0)
	verts[n] = p0
	verts[n + 1] = p1
	verts[n + 2] = p2
	verts[n + 3] = p3
	uvs[n] = u0 + off
	uvs[n + 1] = u1 + off
	uvs[n + 2] = u2 + off
	uvs[n + 3] = u3 + off
	var c := Color(tint.r, tint.g, tint.b, shade)
	cols[n] = c
	cols[n + 1] = c
	cols[n + 2] = c
	cols[n + 3] = c
	var ks := clampi(int(sky * 17.0 + 0.5), 0, 255)
	var kb := clampi(int(blk * 17.0 + 0.5), 0, 255)
	var o := n * 4
	for k in 4:
		light[o] = ks
		light[o + 1] = kb
		light[o + 2] = 0
		light[o + 3] = 255
		o += 4
	idx[ni] = n
	idx[ni + 1] = n + 1
	idx[ni + 2] = n + 2
	idx[ni + 3] = n
	idx[ni + 4] = n + 2
	idx[ni + 5] = n + 3
	ni += 6
	n += 4


## Double sided quad (plants, panes seen from both sides).
func quad2(p0: Vector3, p1: Vector3, p2: Vector3, p3: Vector3, u0: Vector2, u1: Vector2, u2: Vector2, u3: Vector2,
		layer: float, frames: float, tint: Color, shade: float, sky: float, blk: float) -> void:
	quad(p0, p1, p2, p3, u0, u1, u2, u3, layer, frames, tint, shade, sky, blk)
	quad(p3, p2, p1, p0, u3, u2, u1, u0, layer, frames, tint, shade, sky, blk)


func to_arrays() -> Array:
	var a := []
	a.resize(Mesh.ARRAY_MAX)
	a[Mesh.ARRAY_VERTEX] = verts.slice(0, n)
	a[Mesh.ARRAY_TEX_UV] = uvs.slice(0, n)
	a[Mesh.ARRAY_COLOR] = cols.slice(0, n)
	a[Mesh.ARRAY_CUSTOM0] = light.slice(0, n * 4)
	a[Mesh.ARRAY_INDEX] = idx.slice(0, ni)
	return a


static func format_flags() -> int:
	return Mesh.ARRAY_CUSTOM_RGBA8_UNORM << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT
