class_name SulfurCaves
extends RefCounted
## Minecraft 26.2-style Sulfur Caves runtime behaviour: potent sulfur vents erupt into short
## geysers (steam columns that launch entities upward and bubble water above them).

static var _rng := RandomNumberGenerator.new()


static func geyser_tick(world: World, x: int, y: int, z: int) -> void:
	if world.session == null:
		return
	# needs an open (air or water) column above
	var above := world.get_block(x, y + 1, z)
	if above != 0 and BlockDB.fluid[above & 0xFFF] == 0:
		return
	if _rng.randf() < 0.35:
		world.session.start_geyser(Vector3i(x, y, z), _rng.randi_range(40, 90))
