class_name StructureRegistry
extends RefCounted
## All structure kinds per dimension. Every type is a small generator that writes blocks (and chest
## block entities) into a StructureLayout, which the chunk generator then clips per chunk.

static func types_for(dim: int) -> Array:
	match dim:
		0:
			return _overworld()
		1:
			return _nether()
		2:
			return _end()
	return []


static func names_for(dim: int) -> PackedStringArray:
	var out := PackedStringArray()
	for t in types_for(dim):
		out.append((t as StructureType).name)
	return out


# ------------------------------------------------------------------------------------------------
static func _overworld() -> Array:
	var out := []
	out.append(_make("village", 34, 8, 8101, ["plains", "desert", "savanna", "taiga", "snowy_plains", "meadow"], 4))
	out.append(_make("mineshaft", 10, 3, 8202, [], 6, 0.6))
	out.append(_make("ruined_portal", 40, 12, 8303, [], 3))
	out.append(_make("desert_pyramid", 40, 12, 8404, ["desert"], 3))
	out.append(_make("jungle_temple", 40, 12, 8505, ["jungle"], 3))
	out.append(_make("witch_hut", 40, 12, 8606, ["swamp", "mangrove_swamp"], 3))
	out.append(_make("igloo", 40, 12, 8707, ["snowy_plains", "snowy_taiga"], 3))
	out.append(_make("pillager_outpost", 40, 12, 8808, ["plains", "desert", "savanna", "taiga"], 4))
	out.append(_make("stronghold", 96, 24, 8909, [], 6, 0.35))
	out.append(_make("ocean_monument", 64, 16, 9010, ["ocean", "deep_ocean"], 5))
	out.append(_make("shipwreck", 40, 12, 9111, ["ocean", "deep_ocean", "beach"], 3))
	out.append(_make("buried_treasure", 12, 4, 9212, ["beach"], 1, 0.6))
	out.append(_make("ocean_ruin", 24, 6, 9313, ["ocean", "deep_ocean"], 2))
	out.append(_make("woodland_mansion", 160, 40, 9414, ["dark_forest"], 8, 0.6))
	out.append(_make("ancient_city", 96, 24, 9515, [], 6, 0.4))
	out.append(_make("pillager_tower", 48, 14, 9616, ["plains", "taiga"], 4, 0.6))
	return out


static func _nether() -> Array:
	var out := []
	out.append(_make("nether_fortress", 32, 8, 7101, [], 5, 1.0))
	out.append(_make("bastion", 64, 16, 7202, [], 6, 0.9))
	out.append(_make("ruined_portal", 40, 12, 7303, [], 3, 0.7))
	return out


static func _end() -> Array:
	var out := []
	out.append(_make("end_city", 48, 12, 6101, ["end_highlands"], 6, 1.0))
	return out


static func _make(n: String, spacing: int, sep: int, salt: int, biomes: Array, radius: int, chance := 1.0) -> StructureType:
	var t := StructureType.new()
	t.name = n
	t.spacing = spacing
	t.separation = sep
	t.salt = salt
	t.biomes = biomes
	t.max_radius_chunks = radius
	t.chance = chance
	return t
