class_name Vox
extends RefCounted
## Shared voxel constants and small helpers (directions, packing of block values).

# Block value layout: bits 0..11 = block id, bits 12..15 = meta (4-bit block state).
const ID_MASK := 0xFFF
const META_SHIFT := 12

# Face / direction order used everywhere: +X(east), -X(west), +Y(up), -Y(down), +Z(south), -Z(north)
const EAST := 0
const WEST := 1
const UP := 2
const DOWN := 3
const SOUTH := 4
const NORTH := 5

const DIR_X := [1, -1, 0, 0, 0, 0]
const DIR_Y := [0, 0, 1, -1, 0, 0]
const DIR_Z := [0, 0, 0, 0, 1, -1]
const OPPOSITE := [1, 0, 3, 2, 5, 4]
const DIR_VEC := [Vector3i(1, 0, 0), Vector3i(-1, 0, 0), Vector3i(0, 1, 0), Vector3i(0, -1, 0), Vector3i(0, 0, 1), Vector3i(0, 0, -1)]
const DIR_NAMES := ["east", "west", "up", "down", "south", "north"]

# Horizontal facing (meta 2 bits): 0 = south(+Z), 1 = west(-X), 2 = north(-Z), 3 = east(+X)  (Minecraft order)
const H_FACING_DIR := [SOUTH, WEST, NORTH, EAST]
const H_FACING_VEC := [Vector3i(0, 0, 1), Vector3i(-1, 0, 0), Vector3i(0, 0, -1), Vector3i(1, 0, 0)]


static func make(id: int, meta: int = 0) -> int:
	return (id & ID_MASK) | ((meta & 15) << META_SHIFT)


static func id_of(v: int) -> int:
	return v & ID_MASK


static func meta_of(v: int) -> int:
	return (v >> META_SHIFT) & 15


## Horizontal facing index (0..3) from a yaw angle in radians (Godot: yaw 0 looks toward -Z).
static func facing_from_yaw(yaw: float) -> int:
	# Direction the player looks at
	var fx := -sin(yaw)
	var fz := -cos(yaw)
	if absf(fx) > absf(fz):
		return 3 if fx > 0.0 else 1
	return 0 if fz > 0.0 else 2


static func facing_to_dir(f: int) -> int:
	return H_FACING_DIR[f & 3]


static func dir_to_facing(d: int) -> int:
	match d:
		SOUTH: return 0
		WEST: return 1
		NORTH: return 2
		EAST: return 3
	return 0


static func floor_div(a: int, b: int) -> int:
	return floori(float(a) / float(b))


static func chunk_key(cx: int, cz: int) -> Vector2i:
	return Vector2i(cx, cz)
