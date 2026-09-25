class_name FlowerPot
extends RefCounted
## Plants that can be placed into a flower pot (index = pot meta, 0 = empty).

const PLANTS := ["", "poppy", "dandelion", "blue_orchid", "allium", "azure_bluet", "red_tulip", "oxeye_daisy",
	"cornflower", "oak_sapling", "birch_sapling", "fern", "red_mushroom", "brown_mushroom", "cactus", "cherry_sapling"]


static func plant_for(meta: int) -> String:
	return PLANTS[meta] if meta >= 0 and meta < PLANTS.size() else ""


static func meta_for(item_name: String) -> int:
	return PLANTS.find(item_name)
