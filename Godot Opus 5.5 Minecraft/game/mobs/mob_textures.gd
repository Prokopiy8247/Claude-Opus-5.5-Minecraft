class_name MobTextures
extends RefCounted
## Original procedural pixel textures for mob parts (box-UV layout per part). Every pixel is
## generated from palettes + pattern rules here; nothing is copied from the original game.

# style -> region -> [base colour, pattern, accent colour]
const PAL := {
	"animal_pink": {"head": ["#eea3a0", "noise", "#d77c7a"], "body": ["#eea3a0", "noise", "#d77c7a"], "limb": ["#e4958f", "hoof", "#8a5a50"], "extra": ["#f5b9b6", "snout", "#b86462"]},
	"animal_cow": {"head": ["#3f2f22", "cowface", "#e9e4dc"], "body": ["#3f2f22", "spots", "#e9e4dc"], "limb": ["#3f2f22", "hoof_spots", "#e9e4dc"], "extra": ["#d8d0c4", "noise", "#a0988c"]},
	"animal_sheep": {"head": ["#d6c3a8", "noise", "#b8a488"], "body": ["#ebe9e4", "wool", "#d2cfc8"], "limb": ["#d6c3a8", "woolleg", "#ebe9e4"], "extra": ["#ebe9e4", "wool", "#d2cfc8"]},
	"animal_chicken": {"head": ["#f4f3ef", "noise", "#d8d6d0"], "body": ["#f4f3ef", "feathers", "#d8d6d0"], "limb": ["#e6a93a", "noise", "#b87c20"], "extra": ["#e8a030", "beak", "#c42a22"]},
	"animal_rabbit": {"head": ["#8e6b4c", "noise", "#6d4f36"], "body": ["#8e6b4c", "noise", "#6d4f36"], "limb": ["#7d5c40", "noise", "#5a4028"], "extra": ["#e8e2d8", "noise", "#c8c0b4"]},
	"animal_horse": {"head": ["#8d5a31", "noise", "#5f3a1c"], "body": ["#8d5a31", "noise", "#6d4424"], "limb": ["#7d4f2a", "hoof", "#2e2014"], "extra": ["#2e2014", "hair", "#1a120a"]},
	"animal_camel": {"head": ["#d7ae76", "noise", "#b98f5a"], "body": ["#d7ae76", "noise", "#b98f5a"], "limb": ["#caa06a", "hoof", "#6d5638"], "extra": ["#c49a62", "hair", "#a07a48"]},
	"animal_wolf": {"head": ["#cfcac6", "wolfface", "#9a9490"], "body": ["#c6c1bc", "fur", "#9a9490"], "limb": ["#cfcac6", "noise", "#a6a09a"], "extra": ["#bdb7b2", "fur", "#8a847e"]},
	"animal_cat": {"head": ["#e4a860", "stripes", "#b0763a"], "body": ["#e4a860", "stripes", "#b0763a"], "limb": ["#e4a860", "noise", "#b0763a"], "extra": ["#e4a860", "stripes", "#b0763a"]},
	"animal_parrot": {"head": ["#d8261e", "noise", "#a01810"], "body": ["#d8261e", "feathers", "#a01810"], "limb": ["#6a6a6a", "noise", "#404040"], "extra": ["#2c6ad8", "feathers", "#f0d030"]},
	"animal_fox": {"head": ["#e17c2e", "foxface", "#f2ece4"], "body": ["#e17c2e", "fur", "#b85e1c"], "limb": ["#2a2420", "noise", "#1a1612"], "extra": ["#e17c2e", "tailtip", "#f2ece4"]},
	"animal_panda": {"head": ["#ecebe8", "pandaface", "#232120"], "body": ["#ecebe8", "pandabody", "#232120"], "limb": ["#232120", "noise", "#141312"], "extra": ["#232120", "noise", "#141312"]},
	"animal_polar": {"head": ["#f1efe8", "noise", "#d8d4c8"], "body": ["#f1efe8", "fur", "#d8d4c8"], "limb": ["#e8e5dc", "noise", "#cfcabc"], "extra": ["#f1efe8", "noise", "#d8d4c8"]},
	"animal_goat": {"head": ["#e2ddd1", "noise", "#bdb5a4"], "body": ["#e2ddd1", "fur", "#bdb5a4"], "limb": ["#d8d2c4", "hoof", "#4a4034"], "extra": ["#c8bea6", "horn", "#8a8068"]},
	"animal_llama": {"head": ["#e0d0ae", "noise", "#bfae8a"], "body": ["#e0d0ae", "wool", "#c8b690"], "limb": ["#d8c6a0", "noise", "#b8a47e"], "extra": ["#d0bc94", "wool", "#b8a47e"]},
	"villager": {"head": ["#bf9272", "villagerface", "#4d8a3c"], "body": ["#6c4a30", "robe", "#543822"], "limb": ["#5a3e28", "noise", "#3e2a1a"], "extra": ["#b08466", "noise", "#946a50"]},
	"zombie_villager": {"head": ["#5f9c4a", "zvface", "#3a6a2a"], "body": ["#5d4b33", "robe", "#433523"], "limb": ["#4f3f2a", "noise", "#3a2e1e"], "extra": ["#58903e", "noise", "#3e6e2c"]},
	"villager_dark": {"head": ["#8f928c", "illagerface", "#2a2a30"], "body": ["#3e4450", "robe", "#2a2e38"], "limb": ["#343a46", "noise", "#262a34"], "extra": ["#83867f", "noise", "#6a6d66"]},
	"pillager": {"head": ["#8f928c", "illagerface", "#2a2a30"], "body": ["#4f5a3c", "noise", "#3a4430"], "limb": ["#3e3a34", "noise", "#2a2824"], "extra": ["#83867f", "noise", "#6a6d66"]},
	"witch": {"head": ["#9f9a7a", "witchface", "#4f8a3a"], "body": ["#4c2d5c", "robe", "#361e44"], "limb": ["#3e2a4a", "noise", "#2a1a34"], "extra": ["#2e2e2a", "noise", "#1e1e1a"]},
	"metal_iron": {"head": ["#d9d2c9", "golemface", "#8a2a22"], "body": ["#d9d2c9", "metalvine", "#4c8a3c"], "limb": ["#d0c8be", "metalvine", "#4c8a3c"], "extra": ["#c8c0b6", "noise", "#a8a098"]},
	"snow": {"head": ["#e38c1f", "pumpkinface", "#3a1e08"], "body": ["#f2f8fc", "noise", "#d8e4ec"], "limb": ["#6a4a2a", "noise", "#4a3018"], "extra": ["#f2f8fc", "noise", "#d8e4ec"]},
	"metal_copper": {"head": ["#c47550", "copperface", "#6fd0a8"], "body": ["#c47550", "patina", "#6fd0a8"], "limb": ["#b86a46", "patina", "#6fd0a8"], "extra": ["#d88a60", "noise", "#a05a3a"]},
	"animal_armadillo": {"head": ["#b7897a", "noise", "#8a6050"], "body": ["#a37d62", "plates", "#6f5440"], "limb": ["#b7897a", "noise", "#8a6050"], "extra": ["#a37d62", "plates", "#6f5440"]},
	"animal_sniffer": {"head": ["#8d4f3c", "noise", "#6a3828"], "body": ["#8d4f3c", "mossback", "#4f8a3a"], "limb": ["#7a4232", "noise", "#5a3022"], "extra": ["#c86a50", "noise", "#a05038"]},
	"animal_frog": {"head": ["#c9823e", "frogface", "#f2e8c8"], "body": ["#c9823e", "noise", "#a0662c"], "limb": ["#b87434", "noise", "#8a5424"], "extra": ["#c9823e", "noise", "#a0662c"]},
	"animal_turtle": {"head": ["#7fbf5f", "noise", "#5f9a44"], "body": ["#3d7a3b", "shell", "#2a5a28"], "limb": ["#7fbf5f", "noise", "#5f9a44"], "extra": ["#3d7a3b", "shell", "#2a5a28"]},
	"animal_axolotl": {"head": ["#f2a4d4", "noise", "#d880b8"], "body": ["#f2a4d4", "noise", "#d880b8"], "limb": ["#e894c8", "noise", "#c870a8"], "extra": ["#d0508a", "noise", "#a83a6a"]},
	"animal_bat": {"head": ["#4c3a2a", "noise", "#2e2218"], "body": ["#4c3a2a", "fur", "#2e2218"], "limb": ["#3a2c20", "noise", "#241a12"], "extra": ["#3a2c20", "membrane", "#241a12"]},
	"animal_bee": {"head": ["#f0c23c", "beeface", "#2a2016"], "body": ["#f0c23c", "beestripes", "#2a2016"], "limb": ["#2a2016", "noise", "#1a140e"], "extra": ["#e8f4fc", "wing", "#c0d8e8"]},
	"allay": {"head": ["#5ec8ef", "allayface", "#1e4a6a"], "body": ["#5ec8ef", "noise", "#3aa0c8"], "limb": ["#5ec8ef", "noise", "#3aa0c8"], "extra": ["#dcf2fc", "wing", "#a8dcf0"]},
	"squid": {"head": ["#2d3e5c", "squidface", "#b8c8e0"], "body": ["#2d3e5c", "noise", "#1e2c44"], "limb": ["#2a3a56", "noise", "#1e2c44"], "extra": ["#2a3a56", "noise", "#1e2c44"]},
	"glow_squid": {"head": ["#1e6e6e", "squidface", "#b0fff0"], "body": ["#1e6e6e", "glowspots", "#8affe8"], "limb": ["#1c6666", "glowspots", "#8affe8"], "extra": ["#1c6666", "glowspots", "#8affe8"]},
	"animal_dolphin": {"head": ["#8a99ab", "noise", "#6a7a8c"], "body": ["#8a99ab", "belly", "#dfe6ee"], "limb": ["#7a8a9c", "noise", "#5a6a7c"], "extra": ["#7a8a9c", "noise", "#5a6a7c"]},
	"fish": {"head": ["#b98d5c", "fishface", "#2a2016"], "body": ["#b98d5c", "scales", "#8e6a42"], "limb": ["#a8804e", "noise", "#8a6a3e"], "extra": ["#a8804e", "fin", "#8a6a3e"]},
	"nautilus": {"head": ["#e2c89e", "stripes", "#9a6a3e"], "body": ["#e2c89e", "stripes", "#9a6a3e"], "limb": ["#d4b88a", "noise", "#a88a5e"], "extra": ["#d4b88a", "noise", "#a88a5e"]},
	"zombie": {"head": ["#5ea04a", "zombieface", "#1e3a14"], "body": ["#2f9aa5", "shirt", "#23767e"], "limb": ["#5ea04a", "noise", "#4a8438"], "extra": ["#5ea04a", "noise", "#4a8438"], "leg": ["#3d3fa0", "pants", "#2c2e7a"]},
	"husk": {"head": ["#a2936f", "zombieface", "#3a2e1a"], "body": ["#7a6040", "shirt", "#5e4a30"], "limb": ["#a2936f", "noise", "#877a58"], "extra": ["#a2936f", "noise", "#877a58"], "leg": ["#5a4a36", "pants", "#44382a"]},
	"drowned": {"head": ["#4d9b8e", "drownedface", "#8cf0e0"], "body": ["#3e6e62", "torn", "#2a4e44"], "limb": ["#4d9b8e", "noise", "#3a7e72"], "extra": ["#4d9b8e", "noise", "#3a7e72"], "leg": ["#2e5a6a", "torn", "#20404e"]},
	"skeleton": {"head": ["#c9c9c7", "skullface", "#2e2e2e"], "body": ["#c1c1bf", "ribs", "#3a3a3a"], "limb": ["#c9c9c7", "bone", "#8a8a88"], "extra": ["#c9c9c7", "noise", "#a0a09e"]},
	"wither_skeleton": {"head": ["#2d2d2d", "skullface_dark", "#0e0e0e"], "body": ["#2a2a2a", "ribs", "#4e4e4e"], "limb": ["#2d2d2d", "bone", "#4a4a4a"], "extra": ["#2d2d2d", "noise", "#1e1e1e"]},
	"creeper": {"head": ["#5ba345", "creeperface", "#101010"], "body": ["#5ba345", "mottle", "#3b7a2c"], "limb": ["#5ba345", "mottle", "#3b7a2c"], "extra": ["#5ba345", "mottle", "#3b7a2c"]},
	"spider": {"head": ["#352c2a", "spiderface", "#c21e1e"], "body": ["#3a302d", "hairy", "#241c1a"], "limb": ["#3a302d", "noise", "#241c1a"], "extra": ["#3a302d", "hairy", "#241c1a"]},
	"enderman": {"head": ["#171517", "enderface", "#dc6cf2"], "body": ["#161416", "noise", "#0c0a0c"], "limb": ["#161416", "noise", "#0c0a0c"], "extra": ["#161416", "noise", "#0c0a0c"]},
	"slime": {"head": ["#72c251", "slimeface", "#2e6e22"], "body": ["#72c251", "slime", "#5aa83e"], "limb": ["#72c251", "slime", "#5aa83e"], "extra": ["#4e9a36", "noise", "#3a7a28"]},
	"phantom": {"head": ["#43507a", "phantomface", "#8ef06a"], "body": ["#43507a", "noise", "#2e3858"], "limb": ["#43507a", "noise", "#2e3858"], "extra": ["#566896", "membrane", "#3a4a70"]},
	"silver": {"head": ["#8a8a8a", "noise", "#6a6a6a"], "body": ["#8f8f8f", "segments", "#666666"], "limb": ["#7a7a7a", "noise", "#5a5a5a"], "extra": ["#8f8f8f", "noise", "#666666"]},
	"guardian": {"head": ["#5f9189", "guardianface", "#e6a85a"], "body": ["#5f9189", "plates", "#e6a85a"], "limb": ["#e6a85a", "noise", "#b87e3a"], "extra": ["#e6a85a", "noise", "#b87e3a"]},
	"vex": {"head": ["#96b6d6", "vexface", "#2e3e5a"], "body": ["#96b6d6", "noise", "#7896b6"], "limb": ["#96b6d6", "noise", "#7896b6"], "extra": ["#d8e8f6", "wing", "#a8c4de"]},
	"ravager": {"head": ["#56493f", "ravagerface", "#e0dcd2"], "body": ["#56493f", "fur", "#3a302a"], "limb": ["#4a3e36", "noise", "#2e2620"], "extra": ["#ded8c8", "horn", "#aaa494"]},
	"warden": {"head": ["#0d3a3c", "wardenface", "#4ae0d0"], "body": ["#0d3a3c", "wardenchest", "#4ae0d0"], "limb": ["#0b3234", "sculk", "#2aa0a0"], "extra": ["#0b3234", "sculk", "#2aa0a0"]},
	"creaking": {"head": ["#4a3a2c", "creakingface", "#f29a2e"], "body": ["#4a3a2c", "bark", "#2e2418"], "limb": ["#46382a", "bark", "#2e2418"], "extra": ["#6a5a44", "bark", "#2e2418"]},
	"breeze": {"head": ["#9d9ae0", "breezeface", "#e4f4ff"], "body": ["#8f8cd6", "wind", "#d8ecff"], "limb": ["#b0c8f0", "wind", "#e4f4ff"], "extra": ["#b0c8f0", "wind", "#e4f4ff"]},
	"ghast": {"head": ["#f0f0f0", "ghastface", "#8a8a8a"], "body": ["#f0f0f0", "noise", "#d8d8d8"], "limb": ["#eaeaea", "noise", "#cacaca"], "extra": ["#f0f0f0", "noise", "#d0d0d0"]},
	"happy_ghast": {"head": ["#f2f4f6", "happyface", "#6a7a8a"], "body": ["#f2f4f6", "noise", "#dadcde"], "limb": ["#eceef0", "noise", "#ccced0"], "extra": ["#6ab4e8", "noise", "#4a94c8"]},
	"blaze": {"head": ["#f0c232", "blazeface", "#3a2208"], "body": ["#e8b02a", "noise", "#c88a18"], "limb": ["#f0b428", "rod", "#c88418"], "extra": ["#f0b428", "rod", "#c88418"]},
	"piglin": {"head": ["#e3a293", "piglinface", "#6a3a2a"], "body": ["#6a4a2e", "belt", "#e8c040"], "limb": ["#e3a293", "noise", "#c8887a"], "extra": ["#f0b4a4", "snout", "#9a5a4a"], "leg": ["#5a3e26", "noise", "#3e2a18"]},
	"piglin_brute": {"head": ["#d49080", "piglinface", "#4a2a1a"], "body": ["#3a2e24", "belt", "#e8c040"], "limb": ["#d49080", "noise", "#b87a6a"], "extra": ["#e0a090", "snout", "#8a4a3a"], "leg": ["#2e241a", "noise", "#1e160e"]},
	"zombified_piglin": {"head": ["#e0a293", "zpiglinface", "#4c8a3a"], "body": ["#8a6a50", "rot", "#4c8a3a"], "limb": ["#d89a8a", "rot", "#4c8a3a"], "extra": ["#e8b0a0", "snout", "#9a5a4a"], "leg": ["#6a5a44", "rot", "#4c8a3a"]},
	"hoglin": {"head": ["#c78471", "hoglinface", "#f2ece0"], "body": ["#c78471", "fur", "#8a5444"], "limb": ["#b87462", "hoof", "#4a2e24"], "extra": ["#f2ece0", "horn", "#c8c0b0"]},
	"magma_cube": {"head": ["#3b1b12", "magmaface", "#f5a52a"], "body": ["#3b1b12", "cracks", "#f5a52a"], "limb": ["#3b1b12", "cracks", "#f5a52a"], "extra": ["#f5d03a", "noise", "#e0a020"]},
	"strider": {"head": ["#a8342e", "striderface", "#2a1210"], "body": ["#a8342e", "noise", "#7e2420"], "limb": ["#6e5e58", "noise", "#4e423e"], "extra": ["#a8342e", "hair", "#6e1e1a"]},
	"shulker": {"head": ["#e2d7a8", "shulkerface", "#3a2a1a"], "body": ["#9b6c9b", "shell", "#744e74"], "limb": ["#9b6c9b", "shell", "#744e74"], "extra": ["#9b6c9b", "shell", "#744e74"]},
	"sulfur": {"head": ["#e4d44c", "slimeface", "#7a6a14"], "body": ["#e4d44c", "slime", "#c8b832"], "limb": ["#e4d44c", "slime", "#c8b832"], "extra": ["#b8a42a", "noise", "#8a7a1a"]},
	"dragon": {"head": ["#1d1b1e", "dragonface", "#c86af2"], "body": ["#1d1b1e", "scales", "#3a363c"], "limb": ["#1d1b1e", "scales", "#3a363c"], "extra": ["#3c3842", "membrane", "#28242c"]},
	"wither": {"head": ["#262626", "witherface", "#9ad8f0"], "body": ["#262626", "ribs", "#4e4e4e"], "limb": ["#262626", "bone", "#4a4a4a"], "extra": ["#262626", "noise", "#1a1a1a"]},
	"player": {"head": ["#c9956f", "playerface", "#3b2615"], "body": ["#2e8b9a", "shirt", "#1f6b78"], "limb": ["#c9956f", "sleeve", "#2e8b9a"], "extra": ["#3b2615", "hair", "#2a1a0e"], "leg": ["#3a3f8f", "pants", "#2a2e6e"]},
	"skin": {"head": ["#c8966e", "villagerface", "#3a5aa0"], "body": ["#3a8ad0", "noise", "#2a6aa8"], "limb": ["#c8966e", "noise", "#a87a58"], "extra": ["#c8966e", "noise", "#a87a58"]},
}

static var _rng := RandomNumberGenerator.new()


static func wood_color(wood: String) -> Color:
	match wood:
		"spruce": return Color(0.44, 0.31, 0.18)
		"birch": return Color(0.78, 0.7, 0.5)
		"jungle": return Color(0.66, 0.47, 0.32)
		"acacia": return Color(0.7, 0.38, 0.2)
		"dark_oak": return Color(0.26, 0.17, 0.08)
		"mangrove": return Color(0.46, 0.21, 0.19)
		"cherry": return Color(0.89, 0.7, 0.68)
		"pale_oak": return Color(0.9, 0.86, 0.82)
		"bamboo": return Color(0.78, 0.72, 0.36)
		"crimson": return Color(0.42, 0.2, 0.29)
		"warped": return Color(0.17, 0.41, 0.39)
	return Color(0.64, 0.5, 0.31)


## Second skin layer of the player head: transparent except the hair on top, at the back and a
## fringe along the sides and forehead (so the face stays visible).
static func _hair_overlay(img: Image, w: int, h: int, d: int, hair: Color) -> void:
	img.fill(Color(0, 0, 0, 0))
	var hc := Color(hair.r * 0.55, hair.g * 0.4, hair.b * 0.3, 1.0)
	for y in img.get_height():
		for x in img.get_width():
			var n := 0.85 + _rng.randf() * 0.2
			var c := Color(hc.r * n, hc.g * n, hc.b * n, 1.0)
			var top_face := y < d and x >= d and x < d + w
			var back_face := y >= d and x >= 2 * d + w
			var sides := y >= d and y < d + 3 and (x < d or (x >= d + w and x < 2 * d + w))
			var fringe := y == d and x >= d and x < d + w
			if top_face or back_face or sides or fringe:
				img.set_pixel(x, y, c)


static func paint(img: Image, mob: String, style: String, region: String, w: int, h: int, d: int, part: String) -> void:
	var sp: Dictionary = PAL.get(style, PAL["skin"])
	var reg := region
	if sp.has("leg") and part.begins_with("leg"):
		reg = "leg"
	var e: Array = sp.get(reg, sp.get("body"))
	var base := Color.html(e[0])
	var pattern: String = e[1]
	var acc := Color.html(e[2])
	if mob == "glow_squid" and reg != "head":
		pattern = "glowspots"
	_rng.seed = hash(mob + "|" + region + "|" + part + "|%d%d%d" % [w, h, d])
	var tw := img.get_width()
	var th := img.get_height()
	# base fill with value noise + vertical shading
	for y in th:
		for x in tw:
			var n := 0.93 + _rng.randf() * 0.12
			img.set_pixel(x, y, Color(base.r * n, base.g * n, base.b * n, 1.0))
	var front := Rect2i(d, d, w, h)
	var faces := [Rect2i(0, d, d, h), front, Rect2i(d + w, d, d, h), Rect2i(2 * d + w, d, w, h)]
	var top := Rect2i(d, 0, w, d)
	if part == "hat" and style == "player":
		_hair_overlay(img, w, h, d, base)
		return
	match pattern:
		"spots", "hoof_spots":
			for i in maxi(2, (tw * th) / 40):
				_blob(img, _rng.randi_range(0, tw - 1), _rng.randi_range(0, th - 1), _rng.randi_range(1, 3), acc)
			if pattern == "hoof_spots":
				_bottom_band(img, faces, 2, Color(0.22, 0.17, 0.12))
		"hoof":
			_bottom_band(img, faces, 2, acc)
		"wool", "woolleg":
			for y in th:
				for x in tw:
					if (x + y * 3 + _rng.randi_range(0, 2)) % 4 == 0:
						img.set_pixel(x, y, acc)
			if pattern == "woolleg":
				for f in faces:
					var fr: Rect2i = f
					for y in range(fr.position.y + fr.size.y / 2, fr.end.y):
						for x in range(fr.position.x, fr.end.x):
							img.set_pixel(x, y, Color.html(e[0]).darkened(0.05 * _rng.randf()))
		"feathers", "fur", "hairy", "scales", "bark", "sculk", "wind":
			var step := 3 if pattern != "scales" else 2
			for y in th:
				for x in tw:
					if (x * 7 + y * 13 + _rng.randi_range(0, 3)) % (step + 2) == 0:
						img.set_pixel(x, y, acc.lerp(base, 0.4))
			if pattern == "scales":
				for y in range(0, th, 2):
					for x in range((y / 2) % 2, tw, 2):
						img.set_pixel(x, y, acc)
			if pattern == "sculk" or pattern == "wind":
				for i in maxi(2, (tw * th) / 30):
					img.set_pixel(_rng.randi_range(0, tw - 1), _rng.randi_range(0, th - 1), acc)
		"stripes", "beestripes":
			for y in th:
				for x in tw:
					var band := (y / 2) % 2 == 0 if pattern == "beestripes" else ((x + y) / 3) % 3 == 0
					if band:
						img.set_pixel(x, y, acc)
		"mottle", "slime", "cracks", "glowspots", "rot", "patina", "mossback", "segments", "plates", "shell":
			var cnt := maxi(3, (tw * th) / (8 if pattern != "cracks" else 6))
			for i in cnt:
				var px := _rng.randi_range(0, tw - 1)
				var py := _rng.randi_range(0, th - 1)
				var c := acc if _rng.randf() < 0.6 else base.lightened(0.25)
				if pattern == "cracks":
					for k in _rng.randi_range(1, 3):
						img.set_pixel(clampi(px + k, 0, tw - 1), py, acc)
				elif pattern == "mossback":
					if py < th / 2:
						_blob(img, px, py, 1, acc)
				elif pattern == "segments":
					for x in tw:
						if x % 3 == 0:
							for y in th:
								img.set_pixel(x, y, acc)
					break
				elif pattern == "plates" or pattern == "shell":
					for x in tw:
						for y in th:
							if x % 4 == 0 or y % 4 == 0:
								img.set_pixel(x, y, acc)
					break
				else:
					img.set_pixel(px, py, c)
		"ribs":
			for f in faces:
				var fr2: Rect2i = f
				for y in range(fr2.position.y, fr2.end.y):
					for x in range(fr2.position.x, fr2.end.x):
						var lx := x - fr2.position.x
						var ly := y - fr2.position.y
						if ly % 3 == 2 or (lx > 0 and lx < fr2.size.x - 1 and ly > 1 and (lx == fr2.size.x / 2)):
							pass
						elif ly % 3 == 1 and lx > 0 and lx < fr2.size.x - 1:
							img.set_pixel(x, y, acc)
		"bone":
			for f in faces:
				var fr3: Rect2i = f
				for y in range(fr3.position.y, fr3.end.y):
					if (y - fr3.position.y) % 5 == 4:
						for x in range(fr3.position.x, fr3.end.x):
							img.set_pixel(x, y, acc)
		"robe":
			for f in faces:
				var fr4: Rect2i = f
				for x in range(fr4.position.x, fr4.end.x):
					img.set_pixel(x, fr4.position.y, acc)
					if fr4.size.y > 6:
						img.set_pixel(x, fr4.position.y + fr4.size.y / 2, acc.darkened(0.2))
		"shirt", "belt":
			for f in faces:
				var fr5: Rect2i = f
				var yb := fr5.position.y + int(fr5.size.y * 0.72)
				for x in range(fr5.position.x, fr5.end.x):
					if pattern == "belt":
						img.set_pixel(x, yb, acc)
					else:
						img.set_pixel(x, fr5.end.y - 1, acc)
		"pants":
			_bottom_band(img, faces, 2, Color(0.35, 0.35, 0.36))
		"sleeve":
			for f in faces:
				var frs: Rect2i = f
				for y in range(frs.position.y, mini(frs.end.y, frs.position.y + 4)):
					for x in range(frs.position.x, frs.end.x):
						img.set_pixel(x, y, acc.lerp(acc.darkened(0.15), _rng.randf()))
			for y in range(0, d):
				for x in range(d, d + w):
					img.set_pixel(x, y, acc)
		"torn":
			for f in faces:
				var fr6: Rect2i = f
				for x in range(fr6.position.x, fr6.end.x):
					if _rng.randf() < 0.5:
						img.set_pixel(x, fr6.end.y - 1, Color(0, 0, 0, 0))
		"metalvine":
			for i in maxi(2, tw / 3):
				var vx := _rng.randi_range(0, tw - 1)
				var vy := _rng.randi_range(0, th - 1)
				for k in _rng.randi_range(2, 5):
					img.set_pixel(clampi(vx + _rng.randi_range(-1, 1), 0, tw - 1), clampi(vy + k, 0, th - 1), acc)
		"membrane", "wing":
			for y in th:
				for x in tw:
					if (x + y) % 4 == 0:
						img.set_pixel(x, y, acc)
		"belly":
			for f in faces:
				var fr7: Rect2i = f
				for y in range(fr7.end.y - maxi(1, fr7.size.y / 3), fr7.end.y):
					for x in range(fr7.position.x, fr7.end.x):
						img.set_pixel(x, y, acc)
		"tailtip":
			for f in faces:
				var fr8: Rect2i = f
				for y in range(fr8.end.y - maxi(1, fr8.size.y / 4), fr8.end.y):
					for x in range(fr8.position.x, fr8.end.x):
						img.set_pixel(x, y, acc)
		"pandabody":
			for f in faces:
				var fr9: Rect2i = f
				for y in range(fr9.position.y, fr9.position.y + fr9.size.y / 2):
					for x in range(fr9.position.x, fr9.end.x):
						img.set_pixel(x, y, acc)
		"rod":
			for y in th:
				if y % 3 == 0:
					for x in tw:
						img.set_pixel(x, y, acc)
		"snout", "beak":
			_nostrils(img, front, acc, pattern == "beak")
		"hair":
			for y in th:
				for x in tw:
					if (x + _rng.randi_range(0, 1)) % 2 == 0:
						img.set_pixel(x, y, acc)
		"horn", "fin":
			pass
	# darker bottom faces, lighter top
	_shade_rect(img, Rect2i(d + w, 0, w, d), 0.82)
	_shade_rect(img, top, 1.05)
	if region == "head":
		_face(img, pattern, front, acc, base)


static func _shade_rect(img: Image, r: Rect2i, f: float) -> void:
	for y in range(r.position.y, mini(r.end.y, img.get_height())):
		for x in range(r.position.x, mini(r.end.x, img.get_width())):
			var c := img.get_pixel(x, y)
			if c.a > 0.0:
				img.set_pixel(x, y, Color(minf(c.r * f, 1.0), minf(c.g * f, 1.0), minf(c.b * f, 1.0), c.a))


static func _blob(img: Image, cx: int, cy: int, r: int, c: Color) -> void:
	for y in range(cy - r, cy + r + 1):
		for x in range(cx - r, cx + r + 1):
			if x >= 0 and y >= 0 and x < img.get_width() and y < img.get_height():
				if (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r + _rng.randi_range(0, 1):
					img.set_pixel(x, y, c)


static func _bottom_band(img: Image, faces: Array, rows: int, c: Color) -> void:
	for f in faces:
		var fr: Rect2i = f
		for y in range(maxi(fr.position.y, fr.end.y - rows), fr.end.y):
			for x in range(fr.position.x, fr.end.x):
				img.set_pixel(x, y, c.lerp(c.lightened(0.1), _rng.randf()))


static func _nostrils(img: Image, fr: Rect2i, c: Color, beak: bool) -> void:
	if fr.size.x < 2 or fr.size.y < 1:
		return
	if beak:
		for x in range(fr.position.x, fr.end.x):
			img.set_pixel(x, fr.end.y - 1, c.darkened(0.2))
		return
	var y := fr.position.y + fr.size.y / 2
	img.set_pixel(fr.position.x, y, c)
	img.set_pixel(fr.end.x - 1, y, c)


static func _px(img: Image, fr: Rect2i, x: int, y: int, c: Color) -> void:
	if x < 0 or y < 0 or x >= fr.size.x or y >= fr.size.y:
		return
	img.set_pixel(fr.position.x + x, fr.position.y + y, c)


## Face features drawn on the front of head boxes (all designs original).
static func _face(img: Image, pattern: String, fr: Rect2i, acc: Color, base: Color) -> void:
	var w := fr.size.x
	var h := fr.size.y
	if w < 2 or h < 2:
		return
	var ey := int(h * 0.45)
	var ew := 2 if w >= 7 else 1
	var lx := int(w * 0.22)
	var rx := w - lx - ew
	var black := Color(0.06, 0.06, 0.07)
	var white := Color(0.95, 0.95, 0.95)
	match pattern:
		"creeperface":
			for yy in [ey - 1, ey]:
				for xx in [lx, lx + 1, rx, rx + 1]:
					_px(img, fr, xx, yy, black)
			var mx := w / 2 - 1
			for yy in range(ey + 1, mini(h, ey + 4)):
				_px(img, fr, mx, yy, black)
				_px(img, fr, mx + 1, yy, black)
			for yy in range(ey + 2, mini(h, ey + 4)):
				_px(img, fr, mx - 1, yy, black)
				_px(img, fr, mx + 2, yy, black)
		"zombieface", "zvface", "drownedface":
			var eye_c := Color(0.08, 0.14, 0.06) if pattern != "drownedface" else acc
			for xx in range(ew):
				_px(img, fr, lx + xx, ey, eye_c)
				_px(img, fr, rx + xx, ey, eye_c)
			for xx in range(lx, rx + ew):
				_px(img, fr, xx, ey - 2, base.darkened(0.3))
			if pattern == "zvface":
				_px(img, fr, w / 2, ey + 1, base.darkened(0.35))
				_px(img, fr, w / 2 - 1, ey + 1, base.darkened(0.35))
		"skullface", "skullface_dark":
			var hole := Color(0.1, 0.1, 0.1) if pattern == "skullface" else Color(0.02, 0.02, 0.02)
			for xx in range(ew):
				for yy in range(2):
					_px(img, fr, lx + xx, ey - 1 + yy, hole)
					_px(img, fr, rx + xx, ey - 1 + yy, hole)
			_px(img, fr, w / 2, ey + 1, hole)
			for xx in range(lx, rx + ew):
				if xx % 2 == 0:
					_px(img, fr, xx, ey + 3, hole)
		"playerface":
			# hair on top rows, eyes with white + iris, mouth
			for yy in range(0, maxi(1, h / 4)):
				for xx in range(w):
					_px(img, fr, xx, yy, acc)
			for xx in range(w):
				if xx < 1 or xx >= w - 1:
					_px(img, fr, xx, h / 4, acc)
			for xx in range(ew):
				_px(img, fr, lx + xx, ey, white)
				_px(img, fr, rx + xx, ey, white)
			_px(img, fr, lx + ew - 1, ey, Color(0.25, 0.3, 0.75))
			_px(img, fr, rx, ey, Color(0.25, 0.3, 0.75))
			for xx in range(lx + 1, rx + ew - 1):
				_px(img, fr, xx, ey + 2, base.darkened(0.35))
		"villagerface", "illagerface", "witchface":
			for xx in range(ew):
				_px(img, fr, lx + xx, ey, white)
				_px(img, fr, rx + xx, ey, white)
			_px(img, fr, lx + ew - 1, ey, acc)
			_px(img, fr, rx, ey, acc)
			for xx in range(lx, rx + ew):
				_px(img, fr, xx, ey - 1, base.darkened(0.45))
			if pattern == "witchface":
				_px(img, fr, w / 2, ey + 3, Color(0.3, 0.5, 0.2))
		"golemface":
			for xx in range(ew):
				_px(img, fr, lx + xx, ey, acc)
				_px(img, fr, rx + xx, ey, acc)
			for xx in range(lx, rx + ew):
				_px(img, fr, xx, ey - 1, base.darkened(0.35))
		"pumpkinface":
			var glow := Color(0.98, 0.82, 0.3)
			for xx in range(ew):
				_px(img, fr, lx + xx, ey - 1, acc)
				_px(img, fr, rx + xx, ey - 1, acc)
			for xx in range(lx, rx + ew):
				_px(img, fr, xx, ey + 2, acc)
			for yy in h:
				for xx in w:
					if xx % 3 == 0 and yy % 2 == 0:
						var c := img.get_pixel(fr.position.x + xx, fr.position.y + yy)
						img.set_pixel(fr.position.x + xx, fr.position.y + yy, c.darkened(0.1))
			var _unused := glow
		"copperface":
			for xx in range(ew):
				_px(img, fr, lx + xx, ey, Color(0.95, 0.85, 0.5))
				_px(img, fr, rx + xx, ey, Color(0.95, 0.85, 0.5))
		"spiderface":
			var red := acc
			for xx in [lx, rx + ew - 1]:
				_px(img, fr, xx, ey, red)
				_px(img, fr, xx, ey + 1, red)
			for xx in [lx + 1, rx + ew - 2]:
				_px(img, fr, xx, ey - 1, red)
			_px(img, fr, w / 2 - 1, ey - 2, red)
			_px(img, fr, w / 2, ey - 2, red)
		"enderface":
			for xx in range(lx - 1, lx + ew + 1):
				_px(img, fr, xx, ey, acc)
			for xx in range(rx - 1, rx + ew + 1):
				_px(img, fr, xx, ey, acc)
			_px(img, fr, lx, ey, Color(0.95, 0.55, 1.0))
			_px(img, fr, rx + ew - 1, ey, Color(0.95, 0.55, 1.0))
		"slimeface", "magmaface":
			var ec := acc if pattern == "slimeface" else Color(0.98, 0.9, 0.3)
			for xx in range(ew):
				_px(img, fr, lx + xx, ey - 1, ec)
				_px(img, fr, rx + xx, ey - 1, ec)
			_px(img, fr, w / 2, ey + 2, ec)
		"ghastface", "happyface":
			var ec2 := acc
			for xx in range(ew):
				_px(img, fr, lx + xx, ey, ec2)
				_px(img, fr, rx + xx, ey, ec2)
			if pattern == "ghastface":
				for xx in range(w / 2 - 1, w / 2 + 2):
					_px(img, fr, xx, ey + 3, ec2)
			else:
				_px(img, fr, lx, ey + 2, Color(0.95, 0.6, 0.65))
				_px(img, fr, rx + ew - 1, ey + 2, Color(0.95, 0.6, 0.65))
		"blazeface", "breezeface", "vexface", "allayface", "phantomface", "shulkerface", "beeface", "frogface", \
				"squidface", "fishface", "guardianface", "wardenface", "creakingface", "striderface", "ravagerface", \
				"hoglinface", "piglinface", "zpiglinface", "dragonface", "witherface", "cowface", "wolfface", "foxface", "pandaface":
			var ec3 := acc
			if pattern in ["cowface", "wolfface", "foxface", "hoglinface", "ravagerface"]:
				ec3 = black
			if pattern == "pandaface":
				for xx in range(ew + 1):
					for yy in range(2):
						_px(img, fr, lx - 1 + xx, ey - 1 + yy, acc)
						_px(img, fr, rx + xx, ey - 1 + yy, acc)
				_px(img, fr, lx, ey, white)
				_px(img, fr, rx + ew - 1, ey, white)
				return
			if pattern == "guardianface":
				var cx := w / 2 - 1
				for yy in range(ey - 1, ey + 1):
					for xx in range(cx, cx + 2):
						_px(img, fr, xx, yy, Color(0.9, 0.3, 0.3))
				return
			for xx in range(ew):
				_px(img, fr, lx + xx, ey, ec3)
				_px(img, fr, rx + xx, ey, ec3)
			if pattern == "cowface":
				for yy in range(ey + 2, h):
					for xx in range(lx, rx + ew):
						_px(img, fr, xx, yy, Color(0.9, 0.87, 0.82))
			if pattern == "foxface" or pattern == "wolfface":
				for yy in range(ey + 1, h):
					for xx in [lx - 1, rx + ew]:
						_px(img, fr, xx, yy, Color(0.95, 0.93, 0.9))
			if pattern == "wardenface":
				for xx in range(lx, rx + ew):
					_px(img, fr, xx, ey + 3, Color(0.2, 0.9, 0.85))
			if pattern == "creakingface":
				_px(img, fr, lx, ey + 1, Color(1.0, 0.7, 0.2))
				_px(img, fr, rx + ew - 1, ey + 1, Color(1.0, 0.7, 0.2))
			if pattern == "dragonface":
				for xx in range(ew + 1):
					_px(img, fr, lx + xx, ey, Color(0.85, 0.5, 1.0))
					_px(img, fr, rx - 1 + xx, ey, Color(0.85, 0.5, 1.0))
			if pattern == "witherface":
				for xx in range(ew):
					_px(img, fr, lx + xx, ey + 1, black)
					_px(img, fr, rx + xx, ey + 1, black)
				for xx in range(lx, rx + ew):
					_px(img, fr, xx, ey + 3, black)
