class_name StructureType
extends RefCounted
## Base class for a placeable structure kind. Placement uses Minecraft-like region grids:
## the world is divided into regions of `spacing` chunks and one candidate start is chosen per region.

var name := ""
var spacing := 32
var separation := 8
var salt := 0
var chance := 1.0
var max_radius_chunks := 4
var biomes: Array = []          # allowed surface biome names (empty = any)
const NONE := Vector2i(2147483647, 0)


func start_chunk(world_seed: int, rx: int, rz: int) -> Vector2i:
	var r := RandomNumberGenerator.new()
	r.seed = (world_seed ^ (rx * 341873128712) ^ (rz * 132897987541) ^ (salt * 7919)) & 0x7FFFFFFFFFFFFFFF
	if r.randf() > chance:
		return NONE
	var span := maxi(1, spacing - separation)
	return Vector2i(rx * spacing + r.randi_range(0, span - 1), rz * spacing + r.randi_range(0, span - 1))


func rng_for(world_seed: int, start: Vector2i) -> RandomNumberGenerator:
	var r := RandomNumberGenerator.new()
	r.seed = (world_seed ^ (start.x * 73428767) ^ (start.y * 91283467) ^ (salt * 1000003)) & 0x7FFFFFFFFFFFFFFF
	return r


func biome_ok(g: WorldGen, x: int, z: int) -> bool:
	if biomes.is_empty():
		return true
	return BiomeDB.name_of(g.biome_at(x, z)) in biomes


## Produces the layout (or an empty layout when the start is invalid for this kind).
func build(world_seed: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var l: StructureLayout = null
	if not biome_ok(g, x, z):
		l = StructureLayout.new()
		l.empty = true
		return l
	match name:
		"village":
			l = StructureGen.village(world_seed, start, g)
		"mineshaft":
			l = StructureGen.mineshaft(world_seed, start, g)
		"ruined_portal":
			l = StructureGen.ruined_portal(world_seed, start, g)
		"desert_pyramid":
			l = StructureGen.desert_pyramid(world_seed, start, g)
		"jungle_temple":
			l = StructureGen.jungle_temple(world_seed, start, g)
		"witch_hut":
			l = StructureGen.witch_hut(world_seed, start, g)
		"igloo":
			l = StructureGen.igloo(world_seed, start, g)
		"pillager_outpost":
			l = StructureGen.pillager_outpost(world_seed, start, g)
		"pillager_tower":
			l = StructureGen.pillager_tower(world_seed, start, g)
		"stronghold":
			l = StructureGen.stronghold(world_seed, start, g)
		"ocean_monument":
			l = StructureGen.ocean_monument(world_seed, start, g)
		"shipwreck":
			l = StructureGen.shipwreck(world_seed, start, g)
		"buried_treasure":
			l = StructureGen.buried_treasure(world_seed, start, g)
		"ocean_ruin":
			l = StructureGen.ocean_ruin(world_seed, start, g)
		"woodland_mansion":
			l = StructureGen.woodland_mansion(world_seed, start, g)
		"ancient_city":
			l = StructureGen.ancient_city(world_seed, start, g)
		"nether_fortress":
			l = StructureGen.nether_fortress(world_seed, start, g)
		"bastion":
			l = StructureGen.bastion(world_seed, start, g)
		"end_city":
			l = StructureGen.end_city(world_seed, start, g)
	if l == null:
		l = StructureLayout.new()
		l.empty = true
	return l


## Position used by /locate and eyes of ender. Default: start chunk centre if the biome is valid.
func locate_point(_world_seed: int, start: Vector2i, g: WorldGen) -> Vector3:
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	if not biome_ok(g, x, z):
		return Vector3.INF
	if name == "stronghold" or name == "mineshaft" or name == "ancient_city":
		return Vector3(x, 30, z)
	if name == "end_city" and (Vector2(x, z).length() < 900.0 or g.surface_height(x, z) < 40):
		return Vector3.INF
	if name == "village" and g.surface_height(x, z) < g.sea_level + 1:
		return Vector3.INF
	return Vector3(x, g.surface_height(x, z), z)
