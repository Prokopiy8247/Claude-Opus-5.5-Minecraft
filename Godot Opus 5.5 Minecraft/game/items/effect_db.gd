class_name EffectDB
extends RefCounted
## Status effects, potion types (with Java durations/amplifiers) and brewing recipes.

const EFFECTS := {
	"speed": [Color(0.2, 0.75, 0.95), true], "slowness": [Color(0.35, 0.42, 0.55), false],
	"haste": [Color(0.85, 0.75, 0.2), true], "mining_fatigue": [Color(0.3, 0.3, 0.1), false],
	"strength": [Color(1.0, 0.78, 0.0), true], "instant_health": [Color(0.97, 0.14, 0.14), true],
	"instant_damage": [Color(0.66, 0.1, 0.1), false], "jump_boost": [Color(0.99, 1.0, 0.33), true],
	"nausea": [Color(0.33, 0.1, 0.33), false], "regeneration": [Color(0.8, 0.36, 0.63), true],
	"resistance": [Color(0.6, 0.27, 0.48), true], "fire_resistance": [Color(1.0, 0.6, 0.0), true],
	"water_breathing": [Color(0.6, 0.8, 1.0), true], "invisibility": [Color(0.96, 0.96, 0.96), true],
	"blindness": [Color(0.12, 0.12, 0.13), false], "night_vision": [Color(0.76, 1.0, 1.0), true],
	"hunger": [Color(0.53, 0.64, 0.36), false], "weakness": [Color(0.28, 0.3, 0.28), false],
	"poison": [Color(0.53, 0.64, 0.14), false], "wither": [Color(0.44, 0.3, 0.33), false],
	"health_boost": [Color(0.97, 0.49, 0.14), true], "absorption": [Color(0.14, 0.33, 0.66), true],
	"saturation": [Color(0.97, 0.14, 0.14), true], "glowing": [Color(0.58, 0.63, 0.38), false],
	"levitation": [Color(0.81, 1.0, 1.0), false], "luck": [Color(0.35, 0.66, 0.0), true],
	"slow_falling": [Color(0.95, 0.97, 0.88), true], "conduit_power": [Color(0.1, 0.65, 0.73), true],
	"dolphins_grace": [Color(0.53, 0.64, 0.87), true], "bad_omen": [Color(0.04, 0.38, 0.14), false],
	"hero_of_the_village": [Color(0.27, 0.87, 0.27), true], "darkness": [Color(0.16, 0.14, 0.18), false],
	"wind_charged": [Color(0.74, 0.79, 1.0), false], "weaving": [Color(0.47, 0.41, 0.37), false],
	"oozing": [Color(0.6, 1.0, 0.64), false], "infested": [Color(0.55, 0.6, 0.55), false],
}

# potion id -> [[effect, ticks, amplifier]...], display suffix
const POTIONS := {
	"water": [[], "Water Bottle"], "mundane": [[], "Mundane Potion"], "thick": [[], "Thick Potion"], "awkward": [[], "Awkward Potion"],
	"night_vision": [[["night_vision", 3600, 0]], "Night Vision"], "long_night_vision": [[["night_vision", 9600, 0]], "Night Vision"],
	"invisibility": [[["invisibility", 3600, 0]], "Invisibility"], "long_invisibility": [[["invisibility", 9600, 0]], "Invisibility"],
	"leaping": [[["jump_boost", 3600, 0]], "Leaping"], "long_leaping": [[["jump_boost", 9600, 0]], "Leaping"],
	"strong_leaping": [[["jump_boost", 1800, 1]], "Leaping"],
	"fire_resistance": [[["fire_resistance", 3600, 0]], "Fire Resistance"], "long_fire_resistance": [[["fire_resistance", 9600, 0]], "Fire Resistance"],
	"swiftness": [[["speed", 3600, 0]], "Swiftness"], "long_swiftness": [[["speed", 9600, 0]], "Swiftness"],
	"strong_swiftness": [[["speed", 1800, 1]], "Swiftness"],
	"slowness": [[["slowness", 1800, 0]], "Slowness"], "long_slowness": [[["slowness", 4800, 0]], "Slowness"],
	"strong_slowness": [[["slowness", 400, 3]], "Slowness"],
	"turtle_master": [[["slowness", 400, 3], ["resistance", 400, 2]], "the Turtle Master"],
	"water_breathing": [[["water_breathing", 3600, 0]], "Water Breathing"], "long_water_breathing": [[["water_breathing", 9600, 0]], "Water Breathing"],
	"healing": [[["instant_health", 1, 0]], "Healing"], "strong_healing": [[["instant_health", 1, 1]], "Healing"],
	"harming": [[["instant_damage", 1, 0]], "Harming"], "strong_harming": [[["instant_damage", 1, 1]], "Harming"],
	"poison": [[["poison", 900, 0]], "Poison"], "long_poison": [[["poison", 1800, 0]], "Poison"], "strong_poison": [[["poison", 432, 1]], "Poison"],
	"regeneration": [[["regeneration", 900, 0]], "Regeneration"], "long_regeneration": [[["regeneration", 1800, 0]], "Regeneration"],
	"strong_regeneration": [[["regeneration", 450, 1]], "Regeneration"],
	"strength": [[["strength", 3600, 0]], "Strength"], "long_strength": [[["strength", 9600, 0]], "Strength"],
	"strong_strength": [[["strength", 1800, 1]], "Strength"],
	"weakness": [[["weakness", 1800, 0]], "Weakness"], "long_weakness": [[["weakness", 4800, 0]], "Weakness"],
	"luck": [[["luck", 6000, 0]], "Luck"],
	"slow_falling": [[["slow_falling", 1800, 0]], "Slow Falling"], "long_slow_falling": [[["slow_falling", 4800, 0]], "Slow Falling"],
	"wind_charged": [[["wind_charged", 3600, 0]], "Wind Charging"], "weaving": [[["weaving", 3600, 0]], "Weaving"],
	"oozing": [[["oozing", 3600, 0]], "Oozing"], "infested": [[["infested", 3600, 0]], "Infestation"],
}

# [base potion, ingredient, result]
const BREWING := [
	["water", "nether_wart", "awkward"], ["water", "redstone", "mundane"], ["water", "glowstone_dust", "thick"],
	["water", "fermented_spider_eye", "weakness"],
	["awkward", "golden_carrot", "night_vision"], ["awkward", "rabbit_foot", "leaping"], ["awkward", "magma_cream", "fire_resistance"],
	["awkward", "sugar", "swiftness"], ["awkward", "turtle_helmet", "turtle_master"], ["awkward", "pufferfish", "water_breathing"],
	["awkward", "glistering_melon_slice", "healing"], ["awkward", "spider_eye", "poison"], ["awkward", "ghast_tear", "regeneration"],
	["awkward", "blaze_powder", "strength"], ["awkward", "phantom_membrane", "slow_falling"], ["awkward", "breeze_rod", "wind_charged"],
	["awkward", "cobweb", "weaving"], ["awkward", "slime_block", "oozing"], ["awkward", "stone", "infested"],
	["night_vision", "fermented_spider_eye", "invisibility"], ["long_night_vision", "fermented_spider_eye", "long_invisibility"],
	["swiftness", "fermented_spider_eye", "slowness"], ["long_swiftness", "fermented_spider_eye", "long_slowness"],
	["leaping", "fermented_spider_eye", "slowness"], ["long_leaping", "fermented_spider_eye", "long_slowness"],
	["healing", "fermented_spider_eye", "harming"], ["strong_healing", "fermented_spider_eye", "strong_harming"],
	["poison", "fermented_spider_eye", "harming"], ["long_poison", "fermented_spider_eye", "harming"],
	["strong_poison", "fermented_spider_eye", "strong_harming"],
]

const EXTEND := ["night_vision", "invisibility", "leaping", "fire_resistance", "swiftness", "slowness", "water_breathing", "poison",
	"regeneration", "strength", "weakness", "slow_falling"]
const AMPLIFY := ["leaping", "swiftness", "slowness", "healing", "harming", "poison", "regeneration", "strength"]

static var inited := false


static func init() -> void:
	inited = true


static func color_of(effect: String) -> Color:
	return EFFECTS.get(effect, [Color.WHITE, true])[0]


static func beneficial(effect: String) -> bool:
	return EFFECTS.get(effect, [Color.WHITE, true])[1]


static func display_effect(effect: String) -> String:
	return BlockCatalog.pretty(effect)


static func potion_effects(potion: String) -> Array:
	return POTIONS.get(potion, [[], ""])[0]


static func potion_color(potion: String) -> Color:
	var eff := potion_effects(potion)
	if eff.is_empty():
		return Color(0.22, 0.36, 0.84)
	var c := Color(0, 0, 0)
	for e in eff:
		c += color_of(String(e[0]))
	return c / float(eff.size())


static func potion_display(item_name: String, potion: String) -> String:
	var e: Array = POTIONS.get(potion, [[], "Uncraftable Potion"])
	var suffix: String = e[1]
	if potion in ["water", "mundane", "thick", "awkward"]:
		match item_name:
			"splash_potion": return "Splash " + suffix
			"lingering_potion": return "Lingering " + suffix
			"tipped_arrow": return "Tipped Arrow"
		return suffix
	match item_name:
		"splash_potion": return "Splash Potion of " + suffix
		"lingering_potion": return "Lingering Potion of " + suffix
		"tipped_arrow": return "Arrow of " + suffix
	return "Potion of " + suffix


static func potion_tooltip(potion: String) -> PackedStringArray:
	var out := PackedStringArray()
	var eff := potion_effects(potion)
	if eff.is_empty():
		out.append("No Effects")
		return out
	for e in eff:
		var ename: String = e[0]
		var ticks: int = e[1]
		var amp: int = e[2]
		var line := display_effect(ename)
		if amp > 0:
			line += " " + EnchantDB.roman(amp + 1)
		if ticks > 20:
			var s := ticks / 20
			line += " (%d:%02d)" % [s / 60, s % 60]
		out.append(line)
	return out


## Brewing result of ingredient on a potion stack (item name + potion id) or {} if none.
static func brew(item_name: String, potion: String, ingredient: String) -> Dictionary:
	if ingredient == "gunpowder" and item_name == "potion":
		return {"item": "splash_potion", "potion": potion}
	if ingredient == "dragon_breath" and item_name == "splash_potion":
		return {"item": "lingering_potion", "potion": potion}
	if ingredient == "redstone" and potion in EXTEND:
		return {"item": item_name, "potion": "long_" + potion}
	if ingredient == "glowstone_dust" and potion in AMPLIFY:
		return {"item": item_name, "potion": "strong_" + potion}
	for r in BREWING:
		if r[0] == potion and r[1] == ingredient:
			return {"item": item_name, "potion": r[2]}
	return {}


static func is_brewing_ingredient(item_name: String) -> bool:
	if item_name in ["gunpowder", "dragon_breath", "redstone", "glowstone_dust"]:
		return true
	for r in BREWING:
		if r[1] == item_name:
			return true
	return false
