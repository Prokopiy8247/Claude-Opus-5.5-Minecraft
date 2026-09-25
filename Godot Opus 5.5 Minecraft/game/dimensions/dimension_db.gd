class_name DimensionDB
extends RefCounted
## Dimension registry: generator, vertical range, lighting/ambience and portal coordinate scale.

const OVERWORLD := 0
const NETHER := 1
const END := 2

const DEFS := [
	{"id": 0, "name": "overworld", "display": "Overworld", "min_y": -64, "height": 384, "sky": true, "ambient": 0.0,
		"scale": 1.0, "fog": Color(0.75, 0.85, 1.0), "sky_color": Color(0.47, 0.65, 1.0), "water_evaporates": false,
		"bed_works": true, "ceiling": false},
	{"id": 1, "name": "the_nether", "display": "The Nether", "min_y": 0, "height": 128, "sky": false, "ambient": 0.1,
		"scale": 8.0, "fog": Color(0.2, 0.03, 0.03), "sky_color": Color(0.2, 0.03, 0.03), "water_evaporates": true,
		"bed_works": false, "ceiling": true},
	{"id": 2, "name": "the_end", "display": "The End", "min_y": 0, "height": 256, "sky": false, "ambient": 0.25,
		"scale": 1.0, "fog": Color(0.05, 0.04, 0.08), "sky_color": Color(0.08, 0.06, 0.12), "water_evaporates": false,
		"bed_works": false, "ceiling": false},
]


static func get_def(id: int) -> Dictionary:
	return DEFS[clampi(id, 0, 2)]


static func make_generator(id: int, world_seed: int) -> WorldGen:
	var g: WorldGen
	match id:
		NETHER:
			g = NetherGen.new()
		END:
			g = EndGen.new()
		_:
			g = OverworldGen.new()
	g.setup(world_seed)
	var sm := StructureManager.new()
	sm.setup(world_seed, id, g)
	g.structures = sm
	return g


## Portal coordinate mapping (Overworld <-> Nether uses a 1:8 horizontal scale).
static func map_position(pos: Vector3, from_dim: int, to_dim: int) -> Vector3:
	if from_dim == OVERWORLD and to_dim == NETHER:
		return Vector3(pos.x / 8.0, pos.y, pos.z / 8.0)
	if from_dim == NETHER and to_dim == OVERWORLD:
		return Vector3(pos.x * 8.0, pos.y, pos.z * 8.0)
	return pos
