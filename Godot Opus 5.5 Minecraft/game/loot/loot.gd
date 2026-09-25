class_name Loot
extends RefCounted
## Spawns loot into the world (item entities + XP orbs) using LootDB tables and gamerules.

static var _rng := RandomNumberGenerator.new()


static func drop_block(world: World, pos: Vector3i, v: int, tool: ItemStack = null, player = null, be := {}, explosion := false) -> void:
	var session = world.session
	if session == null:
		return
	if player != null and player.is_creative():
		return
	if not bool(session.gamerule("doTileDrops", true)):
		return
	var drops := LootDB.block_drops(v, tool, _rng, be)
	var center := Vector3(pos) + Vector3(0.5, 0.35, 0.5)
	for st in drops:
		session.entities.spawn_item(world, center + Vector3(_rng.randf_range(-0.2, 0.2), 0, _rng.randf_range(-0.2, 0.2)), st)
	# experience from ores / spawners / sculk (not with silk touch, not from explosions)
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	if not explosion and d.xp != Vector2i.ZERO and (tool == null or tool.enchant_level("silk_touch") == 0) and player != null:
		var xp := _rng.randi_range(d.xp.x, d.xp.y)
		if xp > 0:
			session.entities.spawn_xp(world, center, xp)


static func drop_stacks(world: World, pos: Vector3, stacks: Array) -> void:
	if world.session == null:
		return
	for st in stacks:
		if st != null:
			world.session.entities.spawn_item(world, pos, st)
