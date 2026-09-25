class_name EnchantDB
extends RefCounted
## Enchantment registry (targets, weights, level power ranges, exclusivity) plus the Java-style
## enchanting-table option generator and random enchanting for loot.

# name: [max level, weight, targets, min base, min per level, range, treasure, curse, exclusive group]
const ENCH := {
	"protection": [4, 10, "armor", 1, 11, 11, false, false, "prot"],
	"fire_protection": [4, 5, "armor", 10, 8, 8, false, false, "prot"],
	"blast_protection": [4, 2, "armor", 5, 8, 8, false, false, "prot"],
	"projectile_protection": [4, 5, "armor", 3, 6, 6, false, false, "prot"],
	"feather_falling": [4, 5, "feet", 5, 6, 6, false, false, ""],
	"respiration": [3, 2, "head", 10, 10, 30, false, false, ""],
	"aqua_affinity": [1, 2, "head", 1, 0, 40, false, false, ""],
	"thorns": [3, 1, "armor", 10, 20, 50, false, false, ""],
	"depth_strider": [3, 2, "feet", 10, 10, 15, false, false, "boots_fluid"],
	"frost_walker": [2, 2, "feet", 10, 10, 15, true, false, "boots_fluid"],
	"soul_speed": [3, 1, "feet", 10, 10, 15, true, false, ""],
	"swift_sneak": [3, 1, "legs", 25, 25, 50, true, false, ""],
	"sharpness": [5, 10, "sword", 1, 11, 20, false, false, "damage"],
	"smite": [5, 5, "sword", 5, 8, 20, false, false, "damage"],
	"bane_of_arthropods": [5, 5, "sword", 5, 8, 20, false, false, "damage"],
	"knockback": [2, 5, "sword_only", 5, 20, 50, false, false, ""],
	"fire_aspect": [2, 2, "sword_only", 10, 20, 50, false, false, ""],
	"looting": [3, 2, "sword_only", 15, 9, 50, false, false, ""],
	"sweeping_edge": [3, 2, "sword_blade", 5, 9, 15, false, false, ""],
	"efficiency": [5, 10, "digger", 1, 10, 50, false, false, ""],
	"silk_touch": [1, 1, "digger", 15, 0, 50, false, false, "drops"],
	"fortune": [3, 2, "digger", 15, 9, 50, false, false, "drops"],
	"unbreaking": [3, 5, "breakable", 5, 8, 50, false, false, ""],
	"power": [5, 10, "bow", 1, 10, 15, false, false, ""],
	"punch": [2, 2, "bow", 12, 20, 25, false, false, ""],
	"flame": [1, 2, "bow", 20, 0, 30, false, false, ""],
	"infinity": [1, 1, "bow", 20, 0, 30, false, false, "infinite"],
	"luck_of_the_sea": [3, 2, "fishing", 15, 9, 50, false, false, ""],
	"lure": [3, 2, "fishing", 15, 9, 50, false, false, ""],
	"loyalty": [3, 5, "trident", 12, 7, 38, false, false, "trident"],
	"impaling": [5, 2, "trident", 1, 8, 20, false, false, ""],
	"riptide": [3, 2, "trident", 17, 7, 33, false, false, "trident"],
	"channeling": [1, 1, "trident", 25, 0, 25, false, false, "trident_ch"],
	"multishot": [1, 2, "crossbow", 20, 0, 30, false, false, "crossbow"],
	"quick_charge": [3, 5, "crossbow", 12, 20, 38, false, false, ""],
	"piercing": [4, 10, "crossbow", 1, 10, 49, false, false, "crossbow"],
	"density": [5, 5, "mace", 5, 8, 20, false, false, "damage"],
	"breach": [4, 2, "mace", 15, 9, 20, false, false, "damage"],
	"wind_burst": [3, 2, "mace", 15, 9, 50, true, false, ""],
	"lunge": [3, 5, "spear", 5, 8, 20, false, false, ""],
	"mending": [1, 2, "breakable", 25, 25, 50, true, false, "infinite"],
	"binding_curse": [1, 1, "wearable", 25, 0, 25, true, true, ""],
	"vanishing_curse": [1, 1, "vanishable", 25, 0, 25, true, true, ""],
}

static var inited := false


static func init() -> void:
	inited = true


static func roman(n: int) -> String:
	var r := ["", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"]
	return r[n] if n >= 0 and n < r.size() else str(n)


static func display(ench: String, level: int) -> String:
	var n := BlockCatalog.pretty(ench)
	if ench == "binding_curse":
		n = "Curse of Binding"
	elif ench == "vanishing_curse":
		n = "Curse of Vanishing"
	var mx: int = ENCH.get(ench, [1])[0]
	if mx > 1 or level > 1:
		n += " " + roman(level)
	return n


static func is_curse(ench: String) -> bool:
	return ENCH.get(ench, [0, 0, "", 0, 0, 0, false, false])[7]


## Does an enchantment apply to an item?
static func applies(ench: String, it: ItemDef) -> bool:
	if it == null:
		return false
	if it.name == "book" or it.name == "enchanted_book":
		return true
	var target: String = ENCH[ench][2]
	var tool := it.tool
	match target:
		"armor":
			return it.kind == "armor" and it.armor_slot >= 0 and not it.props.get("elytra", false)
		"feet":
			return it.kind == "armor" and it.armor_slot == 0
		"legs":
			return it.kind == "armor" and it.armor_slot == 1
		"head":
			return it.kind == "armor" and it.armor_slot == 3
		"sword":
			return tool == "sword" or tool == "axe" or tool == "spear"
		"sword_only":
			return tool == "sword" or tool == "spear"
		"sword_blade":
			return tool == "sword"
		"digger":
			return tool in ["pickaxe", "axe", "shovel", "hoe"]
		"breakable":
			return it.is_damageable()
		"bow":
			return it.name == "bow"
		"crossbow":
			return it.name == "crossbow"
		"trident":
			return it.name == "trident"
		"fishing":
			return it.name == "fishing_rod"
		"mace":
			return it.name == "mace"
		"spear":
			return tool == "spear"
		"wearable":
			return it.kind == "armor"
		"vanishable":
			return it.is_damageable() or it.name == "compass"
	return false


static func compatible(a: String, b: String) -> bool:
	if a == b:
		return false
	var ga: String = ENCH[a][8]
	var gb: String = ENCH[b][8]
	return ga == "" or ga != gb


static func min_power(ench: String, level: int) -> int:
	var e: Array = ENCH[ench]
	return int(e[3]) + int(e[4]) * (level - 1)


static func max_power(ench: String, level: int) -> int:
	return min_power(ench, level) + int(ENCH[ench][5])


## Candidate [ench, level] pairs for a modified enchanting power.
static func _candidates(it: ItemDef, power: int, treasure: bool) -> Array:
	var out := []
	for e in ENCH:
		var d: Array = ENCH[e]
		if (d[6] and not treasure) or not applies(e, it):
			continue
		if it.name == "book" and d[7]:
			continue
		for lvl in range(int(d[0]), 0, -1):
			if power >= min_power(e, lvl) and power <= max_power(e, lvl):
				out.append([e, lvl, int(d[1])])
				break
	return out


static func _pick(cands: Array, rng: RandomNumberGenerator) -> Array:
	var total := 0
	for c in cands:
		total += int(c[2])
	if total <= 0:
		return []
	var r := rng.randi_range(0, total - 1)
	for c in cands:
		r -= int(c[2])
		if r < 0:
			return c
	return cands[0]


## Java-like enchantment selection for a level cost.
static func select(it: ItemDef, level: int, rng: RandomNumberGenerator, treasure := false) -> Array:
	var ench_value := maxi(1, it.enchantability if it.enchantability > 0 else (1 if it.name == "book" else 0))
	if it.name == "book":
		ench_value = 1
	var power := level + 1 + rng.randi_range(0, ench_value / 4) + rng.randi_range(0, ench_value / 4)
	var bonus := 1.0 + (rng.randf() + rng.randf() - 1.0) * 0.15
	power = clampi(roundi(power * bonus), 1, 1000)
	var result := []
	var cands := _candidates(it, power, treasure)
	var first := _pick(cands, rng)
	if first.is_empty():
		return result
	result.append([first[0], first[1]])
	var p := power
	while rng.randi_range(0, 49) <= p:
		cands = cands.filter(func(c):
			for r in result:
				if not compatible(String(c[0]), String(r[0])):
					return false
			return true)
		var nxt := _pick(cands, rng)
		if nxt.is_empty():
			break
		result.append([nxt[0], nxt[1]])
		p /= 2
	return result


## The three enchanting-table offers: [{cost, ench (preview), level, list}] for a given bookshelf count.
static func table_offers(it: ItemDef, bookshelves: int, seed_v: int) -> Array:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_v
	var b := mini(bookshelves, 15)
	var base := rng.randi_range(1, 8) + (b >> 1) + rng.randi_range(0, b)
	var costs := [maxi(base / 3, 1), (base * 2) / 3 + 1, maxi(base, b * 2)]
	var out := []
	for i in 3:
		var r2 := RandomNumberGenerator.new()
		r2.seed = seed_v + i * 7919
		var list := select(it, costs[i], r2)
		if list.is_empty():
			out.append({})
			continue
		out.append({"cost": costs[i], "lapis": i + 1, "list": list, "preview": list[0]})
	return out


static func apply(st: ItemStack, list: Array) -> void:
	var e: Dictionary = st.data.get("ench", {}).duplicate()
	for pair in list:
		e[String(pair[0])] = int(pair[1])
	st.data["ench"] = e
	if st.item_name() == "book":
		st.id = ItemDB.id("enchanted_book")


static func enchant_randomly(st: ItemStack, rng: RandomNumberGenerator, level: int, treasure := false) -> void:
	var it := st.item()
	if st.item_name() == "enchanted_book":
		it = ItemDB.get_by_name("book")
	var list := select(it, level, rng, treasure)
	if list.is_empty() and st.item_name() == "enchanted_book":
		var keys := ENCH.keys()
		var e: String = keys[rng.randi_range(0, keys.size() - 1)]
		list = [[e, rng.randi_range(1, int(ENCH[e][0]))]]
	apply(st, list)
