class_name ItemStack
extends RefCounted
## A stack of items: id, count, damage (durability used) and free-form data
## (enchantments "ench", custom name "name", potion type "potion", contents...).

var id := -1
var count := 0
var damage := 0
var data: Dictionary = {}


func _init(item_id: int = -1, n: int = 1, dmg: int = 0, d: Dictionary = {}) -> void:
	id = item_id
	count = n if item_id >= 0 else 0
	damage = dmg
	data = d


static func of(item_name: String, n: int = 1) -> ItemStack:
	var i := ItemDB.id(item_name)
	if i < 0:
		return null
	return ItemStack.new(i, n)


func is_empty() -> bool:
	return id < 0 or count <= 0


func item() -> ItemDef:
	return ItemDB.def(id)


func item_name() -> String:
	var d := item()
	return d.name if d != null else ""


func max_stack() -> int:
	var d := item()
	return d.max_stack if d != null else 64


func copy() -> ItemStack:
	return ItemStack.new(id, count, damage, data.duplicate(true))


func with_count(n: int) -> ItemStack:
	return ItemStack.new(id, n, damage, data.duplicate(true))


func split(n: int) -> ItemStack:
	var take := mini(n, count)
	var s := ItemStack.new(id, take, damage, data.duplicate(true))
	count -= take
	if count <= 0:
		id = -1
		count = 0
	return s


func can_merge(other: ItemStack) -> bool:
	if other == null or other.is_empty() or is_empty():
		return false
	return other.id == id and other.damage == damage and other.data.hash() == data.hash() and max_stack() > 1


func display_name() -> String:
	if data.has("name"):
		return String(data["name"])
	var d := item()
	if d == null:
		return "?"
	if data.has("potion"):
		return EffectDB.potion_display(d.name, String(data["potion"]))
	return d.display


func enchantments() -> Dictionary:
	return data.get("ench", {})


func enchant_level(e: String) -> int:
	return int(enchantments().get(e, 0))


func has_glint() -> bool:
	var d := item()
	return not enchantments().is_empty() or (d != null and d.props.get("glint", false))


func to_dict() -> Dictionary:
	if is_empty():
		return {}
	var d := {"n": ItemDB.def(id).name, "c": count}
	if damage != 0:
		d["d"] = damage
	if not data.is_empty():
		d["x"] = data
	return d


static func from_dict(d: Dictionary) -> ItemStack:
	if d.is_empty():
		return null
	var i := ItemDB.id(String(d.get("n", "")))
	if i < 0:
		return null
	return ItemStack.new(i, int(d.get("c", 1)), int(d.get("d", 0)), d.get("x", {}))
