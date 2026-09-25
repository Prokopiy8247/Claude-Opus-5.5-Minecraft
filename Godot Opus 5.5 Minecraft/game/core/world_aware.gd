class_name WorldAware
extends RefCounted
## Small helper mixin used by several systems: resolve the World / session from a node that may be
## the world itself, the session or a child of either.

static func world_of(node) -> World:
	if node == null:
		return null
	if node is World:
		return node
	var w = node.get("world")
	if w is World:
		return w
	return null
