class_name BTexBuilt
extends RefCounted
## Coloured families, functional blocks, redstone, Nether, End, copper, heads and misc textures.

const DYES := ["light_blue", "light_gray", "white", "orange", "magenta", "yellow", "lime", "pink", "gray", "cyan",
	"purple", "blue", "brown", "green", "red", "black"]
const COPPER := {
	"": ["#c06a4c", "#b15f43", "#d27b5a", "#9c5039", "#e59470"],
	"exposed_": ["#a87e68", "#9a715c", "#b88c75", "#86604e", "#c9a08a"],
	"weathered_": ["#6c9d6f", "#5e8e62", "#7bad7e", "#4f7a53", "#8fbf8f"],
	"oxidized_": ["#52a38a", "#46937b", "#5fb39a", "#3a7c67", "#75c7ad"],
}


static func C(s: String) -> Color:
	return Color.html(s)


static func cv_for(n: String) -> PixelCanvas:
	return PixelCanvas.new(16, 16, n)


static func _dye_of(n: String) -> String:
	for d in DYES:
		if n.begins_with(d + "_"):
			return d
	return ""


static func gen(n: String) -> Array:
	var d := _dye_of(n)
	if d != "":
		var r := _colored(n, d)
		if r != null:
			return [r.img]
	for ox in ["exposed_", "weathered_", "oxidized_", ""]:
		if ox == "" or n.begins_with(ox):
			var rest: String = n.substr(ox.length())
			var cc := _copper(n, ox, rest)
			if cc != null:
				return [cc.img]
	var cv := _functional(n)
	if cv != null:
		return [cv.img]
	return []


static func _colored(n: String, d: String) -> PixelCanvas:
	var rest := n.substr(d.length() + 1)
	var cv := cv_for(n)
	var dye := C(TexPalettes.DYE[d])
	match rest:
		"wool":
			BTCommon.wool(cv, dye)
		"concrete":
			BTCommon.concrete(cv, C(TexPalettes.CONCRETE[d]))
		"concrete_powder":
			BTCommon.powder(cv, PixelCanvas.shade(C(TexPalettes.CONCRETE[d]), 0.12))
		"terracotta":
			BTCommon.terracotta(cv, C(TexPalettes.TERRACOTTA[d]))
		"glazed_terracotta":
			BTCommon.glazed(cv, dye, n)
		"stained_glass":
			var c := dye
			c.a = 0.45
			cv.fill(c)
			var fr := PixelCanvas.shade(dye, -0.15)
			fr.a = 0.85
			for i in 16:
				cv.px(i, 0, fr)
				cv.px(i, 15, fr)
				cv.px(0, i, fr)
				cv.px(15, i, fr)
			var hl := PixelCanvas.shade(dye, 0.35)
			hl.a = 0.6
			for k in 3:
				cv.px(3 + k, 3 + k, hl)
		"stained_glass_pane_top":
			var fr2 := PixelCanvas.shade(dye, -0.15)
			fr2.a = 0.9
			cv.fill(Color(0, 0, 0, 0))
			for i in 16:
				cv.px(7, i, fr2)
				cv.px(8, i, fr2)
		"candle":
			var cc := dye
			cv.fill(Color(0, 0, 0, 0))
			cv.rect(6, 5, 4, 11, cc)
			cv.vline(6, 5, 15, PixelCanvas.shade(cc, 0.15))
			cv.vline(9, 5, 15, PixelCanvas.shade(cc, -0.2))
			cv.vline(8, 3, 4, C("#2b2b2b"))
		"shulker_box_top", "shulker_box_bottom", "shulker_box_side":
			_shulker(cv, dye, rest)
		"bed_head_top", "bed_foot_top", "bed_side", "bed_head_end", "bed_foot_end":
			_bed(cv, dye, rest)
		_:
			return null
	return cv


static func _shulker(cv: PixelCanvas, col: Color, part: String) -> void:
	BTCommon.concrete(cv, col)
	var dark := PixelCanvas.shade(col, -0.3)
	var light := PixelCanvas.shade(col, 0.2)
	cv.rect_outline(0, 0, 16, 16, dark)
	if part == "shulker_box_side":
		cv.hline(0, 15, 7, dark)
		cv.hline(0, 15, 8, light)
		cv.rect(6, 5, 4, 2, dark)
	elif part == "shulker_box_top":
		cv.rect_outline(3, 3, 10, 10, light)
		cv.rect(6, 6, 4, 4, dark)


static func _bed(cv: PixelCanvas, col: Color, part: String) -> void:
	var wood := TexPalettes.cols(TexPalettes.WOOD["oak"].plank)
	match part:
		"bed_head_top":
			BTCommon.wool(cv, col)
			cv.rect(2, 1, 12, 5, C("#f1f1ee"))
			cv.rect_outline(2, 1, 12, 5, C("#d8d8d2"))
		"bed_foot_top":
			BTCommon.wool(cv, col)
			cv.hline(0, 15, 12, PixelCanvas.shade(col, -0.2))
		"bed_side":
			cv.fill(Color(0, 0, 0, 0))
			for x in 16:
				for y in range(7, 13):
					cv.px(x, y, PixelCanvas.shade(col, cv.rng.randf_range(-0.05, 0.05)))
				cv.px(x, 13, wood[3])
			cv.rect(0, 13, 3, 3, wood[0])
			cv.rect(13, 13, 3, 3, wood[0])
		"bed_head_end", "bed_foot_end":
			cv.fill(Color(0, 0, 0, 0))
			for x in 16:
				for y in range(7, 13):
					cv.px(x, y, PixelCanvas.shade(col, cv.rng.randf_range(-0.05, 0.05)))
			cv.hline(0, 15, 13, wood[3])
			cv.rect(0, 13, 3, 3, wood[0])
			cv.rect(13, 13, 3, 3, wood[0])


static func _copper(n: String, ox: String, rest: String) -> PixelCanvas:
	var pal: Array = COPPER[ox]
	var pc := TexPalettes.cols(pal)
	var cv := cv_for(n)
	match rest:
		"copper", "copper_block":
			if rest == "copper" and ox == "":
				return null
			cv.palette_noise(pc, [4, 3, 2, 1, 1], 1)
			cv.rect_outline(0, 0, 16, 16, pc[3])
			cv.hline(1, 14, 1, pc[4])
			if ox != "":
				_patina(cv, ox)
		"cut_copper":
			BTCommon.tiles(cv, pal, pc[3], 8)
			if ox != "":
				_patina(cv, ox)
		"chiseled_copper":
			cv.palette_noise(pc, [4, 3, 2, 1, 1], 0)
			cv.rect_outline(0, 0, 16, 16, pc[3])
			cv.rect_outline(3, 3, 10, 10, pc[3])
			cv.rect(6, 6, 4, 4, pc[4])
			cv.rect(7, 7, 2, 2, pc[3])
		"copper_grate":
			cv.fill(Color(0, 0, 0, 0))
			for y in 16:
				for x in 16:
					if x % 4 == 0 or y % 4 == 0 or x == 15 or y == 15:
						cv.px(x, y, pc[cv.rng.randi_range(0, 2)])
		"copper_bulb", "copper_bulb_lit":
			cv.palette_noise(pc, [4, 3, 2, 1, 1], 0)
			cv.rect_outline(0, 0, 16, 16, pc[3])
			var lit := rest.ends_with("_lit")
			cv.rect(3, 3, 10, 10, C("#fff1b0") if lit else C("#6b4a3a"))
			cv.rect(5, 5, 6, 6, C("#ffd35a") if lit else C("#4a3226"))
			cv.rect_outline(3, 3, 10, 10, pc[3])
		"copper_door_top", "copper_door_bottom":
			BTCommon.door(cv, pal, rest == "copper_door_top", 1)
		"copper_trapdoor":
			BTCommon.trapdoor(cv, pal, 1)
		"copper_bars":
			cv.fill(Color(0, 0, 0, 0))
			for x in [1, 5, 10, 14]:
				cv.vline(x, 0, 15, pc[0])
				cv.vline(x + 1, 0, 15, pc[3])
			cv.hline(0, 15, 1, pc[2])
			cv.hline(0, 15, 14, pc[2])
		"copper_chain":
			cv.fill(Color(0, 0, 0, 0))
			for y in 16:
				if y % 6 < 4:
					cv.px(7, y, pc[0])
					cv.px(8, y, pc[3])
				else:
					cv.px(6, y, pc[2])
					cv.px(9, y, pc[2])
		"copper_lantern":
			_lantern(cv, pc[0], pc[3], C("#ffd66b"))
		"lightning_rod":
			cv.fill(Color(0, 0, 0, 0))
			cv.rect(7, 4, 2, 12, pc[0])
			cv.vline(7, 4, 15, pc[4])
			cv.rect(6, 0, 4, 4, pc[2])
		"copper_chest":
			cv.palette_noise(pc, [4, 3, 2, 1, 1], 0)
			cv.rect_outline(0, 0, 16, 16, pc[3])
			cv.hline(0, 15, 5, pc[3])
		"copper_torch":
			if ox != "":
				return null
			_torch(cv, C("#8a6a3f"), C("#8ff0b5"), C("#3fbf7f"))
		_:
			return null
	return cv


static func _patina(cv: PixelCanvas, ox: String) -> void:
	var amount: float = {"exposed_": 0.1, "weathered_": 0.25, "oxidized_": 0.4}[ox]
	for k in int(amount * 40):
		cv.px(cv.rng.randi_range(0, 15), cv.rng.randi_range(0, 15), C("#6fc2a4"))


static func _torch(cv: PixelCanvas, stick: Color, flame_hi: Color, flame: Color) -> void:
	cv.fill(Color(0, 0, 0, 0))
	cv.rect(7, 6, 2, 10, stick)
	cv.vline(7, 6, 15, PixelCanvas.shade(stick, 0.15))
	cv.rect(7, 4, 2, 2, flame)
	cv.px(7, 4, flame_hi)
	cv.px(8, 5, flame_hi)
	cv.px(7, 3, PixelCanvas.shade(flame, 0.3))


static func _lantern(cv: PixelCanvas, metal: Color, dark: Color, glow: Color) -> void:
	cv.fill(Color(0, 0, 0, 0))
	cv.rect(4, 5, 8, 9, metal)
	cv.rect(5, 6, 6, 7, glow)
	cv.rect_outline(4, 5, 8, 9, dark)
	cv.rect(5, 3, 6, 2, metal)
	cv.rect(7, 0, 2, 3, dark)


static func _functional(n: String) -> PixelCanvas:
	var cv := cv_for(n)
	var oak := TexPalettes.WOOD["oak"].plank
	var stone_pal := ["#7f7f7f", "#747474", "#8b8b8b", "#6a6a6a"]
	match n:
		"glass":
			BTCommon.glass(cv, Color(0.86, 0.93, 0.95, 0.95))
		"glass_pane_top":
			cv.fill(Color(0, 0, 0, 0))
			cv.vline(7, 0, 15, Color(0.86, 0.93, 0.95, 0.95))
			cv.vline(8, 0, 15, Color(0.7, 0.8, 0.84, 0.95))
		"tinted_glass":
			cv.fill(Color(0.18, 0.15, 0.2, 0.85))
			cv.rect_outline(0, 0, 16, 16, Color(0.3, 0.26, 0.34, 0.95))
		"terracotta":
			BTCommon.terracotta(cv, C("#985e43"))
		"candle":
			cv.fill(Color(0, 0, 0, 0))
			cv.rect(6, 5, 4, 11, C("#e8d9a6"))
			cv.vline(9, 5, 15, C("#c8b886"))
			cv.vline(8, 3, 4, C("#2b2b2b"))
		"shulker_box_top", "shulker_box_bottom", "shulker_box_side":
			_shulker(cv, C("#8f628f"), n)
		"crafting_table_top":
			BTCommon.planks(cv, oak)
			var dk := C("#5a4125")
			cv.rect_outline(0, 0, 16, 16, dk)
			for i in [5, 10]:
				cv.hline(1, 14, i, dk)
				cv.vline(i, 1, 14, dk)
			cv.rect(1, 1, 4, 4, C("#b58c55"))
		"crafting_table_side", "crafting_table_front":
			BTCommon.planks(cv, oak)
			cv.rect(0, 0, 16, 3, C("#6b4c2a"))
			cv.hline(0, 15, 3, C("#4a3319"))
			if n == "crafting_table_front":
				# original tool silhouettes: hammer + saw
				cv.rect(3, 5, 1, 7, C("#6b4c2a"))
				cv.rect(2, 5, 3, 2, C("#9a9a9a"))
				cv.rect(9, 6, 5, 2, C("#b8b8b8"))
				cv.rect(11, 8, 2, 4, C("#6b4c2a"))
				for x in range(9, 14):
					cv.px(x, 8 if x % 2 == 0 else 7, C("#8a8a8a"))
			else:
				cv.rect(4, 6, 2, 6, C("#6b4c2a"))
				cv.rect(3, 5, 4, 2, C("#a0a0a0"))
				cv.rect(10, 6, 3, 1, C("#b0b0b0"))
				cv.rect(11, 7, 1, 5, C("#6b4c2a"))
		"furnace_top", "furnace_side", "furnace_front", "furnace_front_on":
			cv.palette_noise(TexPalettes.cols(stone_pal), [4, 3, 2, 1], 1)
			cv.rect_outline(0, 0, 16, 16, C("#5a5a5a"))
			if n == "furnace_top":
				cv.rect_outline(2, 2, 12, 12, C("#6a6a6a"))
			if n.begins_with("furnace_front"):
				cv.rect(3, 3, 10, 3, C("#5a5a5a"))
				cv.rect(4, 4, 8, 1, C("#3a3a3a"))
				cv.rect(3, 8, 10, 6, C("#303030"))
				cv.rect_outline(3, 8, 10, 6, C("#4a4a4a"))
				if n == "furnace_front_on":
					cv.rect(4, 11, 8, 3, C("#f2891c"))
					cv.rect(5, 10, 6, 1, C("#ffcc3a"))
					cv.px(6, 9, C("#ffe07a"))
					cv.px(9, 9, C("#ffe07a"))
		"blast_furnace_top", "blast_furnace_side", "blast_furnace_front", "blast_furnace_front_on":
			BTCommon.polished(cv, ["#6f6f72", "#67676a", "#78787b"])
			if n == "blast_furnace_side":
				for x in [3, 7, 11]:
					cv.vline(x, 2, 13, C("#3f3f42"))
			if n.begins_with("blast_furnace_front"):
				cv.rect(3, 7, 10, 7, C("#2b2b2d"))
				for x in range(3, 13, 2):
					cv.vline(x, 7, 13, C("#555558"))
				if n.ends_with("_on"):
					for x in range(4, 13, 2):
						cv.vline(x, 9, 13, C("#ff8f2a"))
		"smoker_top", "smoker_bottom", "smoker_side", "smoker_front", "smoker_front_on":
			BTCommon.planks(cv, TexPalettes.WOOD["spruce"].plank)
			if n != "smoker_bottom":
				cv.rect(0, 12, 16, 4, C("#6f6f6f"))
			if n == "smoker_top":
				cv.rect(4, 4, 8, 8, C("#4a4a4a"))
			if n.begins_with("smoker_front"):
				cv.rect(3, 6, 10, 6, C("#2b2b2b"))
				if n.ends_with("_on"):
					cv.rect(4, 9, 8, 3, C("#ff8f2a"))
		"barrel_side":
			BTCommon.planks_vertical(cv, TexPalettes.WOOD["spruce"].plank)
			cv.hline(0, 15, 3, C("#3a3a3a"))
			cv.hline(0, 15, 12, C("#3a3a3a"))
		"barrel_top", "barrel_top_open", "barrel_bottom":
			BTCommon.planks(cv, TexPalettes.WOOD["spruce"].plank)
			cv.rect_outline(0, 0, 16, 16, C("#3a3a3a"))
			if n == "barrel_top_open":
				cv.rect(2, 2, 12, 12, C("#2a1c0e"))
			elif n == "barrel_top":
				cv.rect(6, 6, 4, 4, C("#4a3520"))
		"bookshelf":
			BTCommon.planks(cv, oak)
			var book_cols := TexPalettes.cols(["#8f2a1e", "#2a4a8f", "#3a7a2a", "#8f6a1e", "#6a2a7a", "#b0a080", "#2a6a6a"])
			for shelf in [1, 9]:
				cv.rect(0, shelf, 16, 6, C("#2a1c0e"))
				var x := 1
				while x < 15:
					var bw := cv.rng.randi_range(1, 2)
					var bh := cv.rng.randi_range(4, 6)
					var bc: Color = book_cols[cv.rng.randi_range(0, book_cols.size() - 1)]
					cv.rect(x, shelf + 6 - bh, bw, bh, bc)
					cv.px(x, shelf + 6 - bh, PixelCanvas.shade(bc, 0.3))
					x += bw + (1 if cv.rng.randf() < 0.3 else 0)
		"chiseled_bookshelf_top", "chiseled_bookshelf_side":
			BTCommon.planks(cv, oak)
			cv.rect_outline(0, 0, 16, 16, C("#5a4125"))
		"chiseled_bookshelf_empty", "chiseled_bookshelf_occupied":
			BTCommon.planks(cv, oak)
			cv.rect_outline(0, 0, 16, 16, C("#5a4125"))
			cv.hline(0, 15, 7, C("#5a4125"))
			cv.hline(0, 15, 8, C("#5a4125"))
			for sx in [1, 6, 11]:
				cv.rect(sx, 1, 4, 6, C("#2a1c0e"))
				cv.rect(sx, 9, 4, 6, C("#2a1c0e"))
				if n.ends_with("occupied"):
					cv.rect(sx, 2, 1, 5, C("#8f2a1e"))
					cv.rect(sx + 2, 3, 1, 4, C("#2a4a8f"))
					cv.rect(sx + 1, 10, 1, 5, C("#3a7a2a"))
					cv.rect(sx + 3, 11, 1, 4, C("#b08a2a"))
		"jukebox_side", "note_block":
			BTCommon.planks(cv, TexPalettes.WOOD["dark_oak"].plank if n == "note_block" else oak)
			cv.rect_outline(0, 0, 16, 16, C("#3a2710"))
			if n == "note_block":
				cv.rect(6, 4, 2, 7, C("#2a1a08"))
				cv.rect(4, 9, 3, 3, C("#2a1a08"))
				cv.rect(8, 4, 3, 2, C("#2a1a08"))
		"jukebox_top":
			BTCommon.planks(cv, oak)
			cv.rect_outline(0, 0, 16, 16, C("#3a2710"))
			cv.rect(3, 6, 10, 4, C("#1a1a1a"))
			cv.rect(4, 7, 8, 2, C("#3a3a3a"))
		"tnt_side":
			cv.fill(C("#d8392b"))
			for x in 16:
				if x % 4 == 0:
					cv.vline(x, 0, 15, C("#b52a1e"))
			cv.rect(0, 5, 16, 6, C("#ece7df"))
			# original lettering "TNT" in our pixel style
			var ink := C("#2a2a2a")
			cv.hline(2, 4, 6, ink); cv.vline(3, 6, 9, ink)
			cv.vline(6, 6, 9, ink); cv.vline(9, 6, 9, ink); cv.px(7, 7, ink); cv.px(8, 8, ink)
			cv.hline(11, 13, 6, ink); cv.vline(12, 6, 9, ink)
		"tnt_top", "tnt_bottom":
			cv.fill(C("#d8392b"))
			cv.rect(4, 4, 8, 8, C("#ece7df"))
			if n == "tnt_top":
				cv.rect(7, 7, 2, 2, C("#3a3a3a"))
			else:
				cv.rect_outline(4, 4, 8, 8, C("#b52a1e"))
		"loom_top", "loom_bottom", "loom_side", "loom_front":
			BTCommon.planks(cv, oak)
			if n == "loom_front":
				for x in range(2, 14):
					cv.vline(x, 3, 12, C("#e8e3d8") if x % 2 == 0 else C("#c9c2b3"))
				cv.hline(1, 14, 2, C("#6b4c2a"))
			elif n == "loom_top":
				cv.rect(2, 2, 12, 12, C("#c9a86a"))
		"cartography_table_top", "cartography_table_side1", "cartography_table_side3":
			BTCommon.planks(cv, TexPalettes.WOOD["dark_oak"].plank)
			if n == "cartography_table_top":
				cv.rect(2, 2, 12, 12, C("#e8dfc5"))
				cv.rect(4, 5, 5, 3, C("#6aa84f"))
				cv.rect(9, 8, 3, 4, C("#4a7ec4"))
		"fletching_table_top", "fletching_table_side", "fletching_table_front":
			BTCommon.planks(cv, TexPalettes.WOOD["birch"].plank)
			if n == "fletching_table_top":
				cv.rect(2, 2, 12, 12, C("#d9c89a"))
				cv.line(4, 12, 12, 4, C("#6b4c2a"))
			if n == "fletching_table_front":
				cv.rect(4, 3, 8, 2, C("#e8e8e8"))
		"smithing_table_top", "smithing_table_bottom", "smithing_table_side", "smithing_table_front":
			if n == "smithing_table_top":
				cv.palette_noise(TexPalettes.cols(["#393a44", "#31323b", "#42434d"]), [4, 3, 2], 0)
				cv.rect_outline(0, 0, 16, 16, C("#22232a"))
			else:
				BTCommon.planks(cv, TexPalettes.WOOD["dark_oak"].plank)
				cv.rect(0, 0, 16, 3, C("#393a44"))
				if n == "smithing_table_front":
					cv.rect(4, 6, 8, 3, C("#9a9aa8"))
					cv.rect(7, 9, 2, 4, C("#4a3520"))
		"stonecutter_top", "stonecutter_bottom", "stonecutter_side":
			cv.palette_noise(TexPalettes.cols(stone_pal), [4, 3, 2, 1], 1)
			cv.rect_outline(0, 0, 16, 16, C("#5a5a5a"))
			if n == "stonecutter_top":
				cv.rect(1, 7, 14, 2, C("#3a3a3a"))
		"stonecutter_saw":
			cv.fill(Color(0, 0, 0, 0))
			cv.disc(8, 8, 7.5, C("#b8b8b8"))
			for a in 16:
				var ang := a * TAU / 16.0
				cv.px(8 + int(round(cos(ang) * 7.4)), 8 + int(round(sin(ang) * 7.4)), C("#e8e8e8"))
			cv.disc(8, 8, 2.0, C("#6a6a6a"))
		"grindstone_side", "grindstone_round", "grindstone_pivot":
			cv.palette_noise(TexPalettes.cols(["#8a8a8a", "#7a7a7a", "#9a9a9a"]), [4, 3, 2], 1)
			if n == "grindstone_pivot":
				BTCommon.planks(cv, TexPalettes.WOOD["dark_oak"].plank)
			if n == "grindstone_side":
				cv.rect_outline(0, 0, 16, 16, C("#5a5a5a"))
				cv.rect(6, 6, 4, 4, C("#5a5a5a"))
		"composter_top", "composter_bottom", "composter_side":
			BTCommon.planks(cv, oak)
			cv.rect_outline(0, 0, 16, 16, C("#5a4125"))
			if n == "composter_top":
				cv.rect(2, 2, 12, 12, Color(0, 0, 0, 0))
		"composter_compost", "composter_ready":
			cv.palette_noise(TexPalettes.cols(["#4f3a1f", "#3f2d17", "#5f4726"] if n == "composter_compost" else ["#6b5a3a", "#8a7a4a", "#5a4a2a"]), [4, 3, 2], 1)
			if n == "composter_ready":
				cv.speckle(C("#e8e8d8"), 10)
		"lectern_top", "lectern_base", "lectern_sides", "lectern_front":
			BTCommon.planks(cv, oak)
			cv.rect_outline(0, 0, 16, 16, C("#5a4125"))
			if n == "lectern_front":
				cv.rect(3, 3, 10, 10, C("#8a6a3f"))
		"cauldron_top", "cauldron_side", "cauldron_bottom", "cauldron_inner":
			cv.palette_noise(TexPalettes.cols(["#4a4a4d", "#424245", "#535356"]), [4, 3, 2], 1)
			cv.rect_outline(0, 0, 16, 16, C("#2e2e30"))
			if n == "cauldron_top":
				cv.rect(2, 2, 12, 12, Color(0, 0, 0, 0))
			if n == "cauldron_side":
				cv.rect(4, 13, 8, 3, Color(0, 0, 0, 0))
		"anvil", "anvil_top", "chipped_anvil_top", "damaged_anvil_top":
			cv.palette_noise(TexPalettes.cols(["#444446", "#3c3c3e", "#4e4e50"]), [4, 3, 2], 1)
			cv.rect_outline(0, 0, 16, 16, C("#2a2a2c"))
			if n != "anvil":
				cv.rect_outline(3, 1, 10, 14, C("#5c5c5e"))
			if n.begins_with("chipped"):
				BTexTerrain._cracks(cv, C("#1e1e20"), 2)
			if n.begins_with("damaged"):
				BTexTerrain._cracks(cv, C("#1e1e20"), 5)
		"enchanting_table_top":
			cv.palette_noise(TexPalettes.cols(["#a8232d", "#961d26", "#b82b35"]), [4, 3, 2], 0)
			cv.rect_outline(0, 0, 16, 16, C("#1c1628"))
			for p in [[1, 1], [13, 1], [1, 13], [13, 13]]:
				cv.rect(p[0], p[1], 2, 2, C("#5decf5"))
		"enchanting_table_side", "enchanting_table_bottom":
			cv.palette_noise(TexPalettes.cols(["#15121e", "#1f1930", "#0d0b13", "#2b2143"]), [4, 3, 3, 1], 1)
			if n == "enchanting_table_side":
				cv.rect(0, 0, 16, 4, C("#a8232d"))
				cv.hline(0, 15, 4, C("#5decf5"))
		"brewing_stand", "brewing_stand_base":
			cv.fill(Color(0, 0, 0, 0))
			if n == "brewing_stand":
				cv.rect(7, 1, 2, 14, C("#c9a53a"))
				cv.px(7, 1, C("#fff0a0"))
			else:
				cv.palette_noise(TexPalettes.cols(["#6a6a6a", "#5a5a5a", "#7a7a7a"]), [4, 3, 2], 0)
		"beacon":
			cv.fill(C("#7ef2e8"))
			cv.rect_outline(1, 1, 14, 14, C("#4bc2b8"))
			cv.rect(5, 5, 6, 6, C("#e8fffd"))
		"bell_body":
			cv.palette_noise(TexPalettes.cols(["#e8b83a", "#d8a52a", "#f5ca4f"]), [4, 3, 2], 0)
		"flower_pot":
			cv.palette_noise(TexPalettes.cols(["#7a3a2a", "#8a4535", "#6a3020"]), [4, 3, 2], 0)
		"decorated_pot_side", "decorated_pot_top":
			cv.palette_noise(TexPalettes.cols(["#9a5a3a", "#8a4f32", "#a8653f"]), [4, 3, 2], 0)
			if n == "decorated_pot_side":
				cv.hline(0, 15, 3, C("#3a2a1f"))
				cv.hline(0, 15, 12, C("#3a2a1f"))
				cv.rect(5, 6, 6, 4, C("#3a2a1f"))
		"heavy_core":
			cv.palette_noise(TexPalettes.cols(["#4a4f5a", "#40454f", "#555a66"]), [4, 3, 2], 0)
			cv.rect_outline(0, 0, 16, 16, C("#2a2e36"))
			cv.rect(5, 5, 6, 6, C("#6a7080"))
		"vault_top", "vault_bottom", "vault_side", "vault_front", "trial_spawner_top", "trial_spawner_bottom", "trial_spawner_side":
			cv.fill(Color(0, 0, 0, 0))
			var frame := C("#3a3f4a")
			var accent := C("#c9772a") if n.begins_with("trial") else C("#5a86b8")
			cv.rect_outline(0, 0, 16, 16, frame)
			cv.rect_outline(1, 1, 14, 14, accent)
			for x in range(3, 14, 3):
				cv.vline(x, 1, 14, frame)
			if n == "vault_front":
				cv.rect(5, 5, 6, 6, accent)
				cv.rect(7, 7, 2, 2, C("#1a1a1a"))
		"crafter_top", "crafter_bottom", "crafter_side", "crafter_north":
			cv.palette_noise(TexPalettes.cols(["#6f6f6f", "#656565", "#7a7a7a"]), [4, 3, 2], 0)
			cv.rect_outline(0, 0, 16, 16, C("#3a3a3a"))
			if n == "crafter_north":
				for i in [5, 10]:
					cv.hline(2, 13, i, C("#3a3a3a"))
					cv.vline(i, 2, 13, C("#3a3a3a"))
				cv.rect_outline(2, 2, 12, 12, C("#b07a3a"))
			elif n == "crafter_top":
				BTCommon.planks(cv, oak)
				cv.rect_outline(0, 0, 16, 16, C("#3a3a3a"))
		"torch":
			_torch(cv, C("#8a6a3f"), C("#fff4b0"), C("#ffb82f"))
		"soul_torch":
			_torch(cv, C("#8a6a3f"), C("#c8ffff"), C("#3fd6e0"))
		"redstone_torch":
			_torch(cv, C("#8a6a3f"), C("#ff8a8a"), C("#ff1a1a"))
		"redstone_torch_off":
			_torch(cv, C("#8a6a3f"), C("#7a2a2a"), C("#5a1a1a"))
		"lantern":
			_lantern(cv, C("#4a4f5a"), C("#2a2e36"), C("#ffd66b"))
		"soul_lantern":
			_lantern(cv, C("#4a4f5a"), C("#2a2e36"), C("#7ff0f5"))
		"campfire_log", "campfire_log_lit", "soul_campfire_log_lit":
			BTCommon.log_side(cv, TexPalettes.WOOD["oak"].bark)
			if n != "campfire_log":
				var glow := C("#ff8a2a") if n == "campfire_log_lit" else C("#3fd6e0")
				for x in range(0, 16, 3):
					cv.px(x, 15, glow)
					cv.px(x + 1, 14, glow)
		"chain":
			cv.fill(Color(0, 0, 0, 0))
			for y in 16:
				if y % 6 < 4:
					cv.px(7, y, C("#4a4f5a"))
					cv.px(8, y, C("#2a2e36"))
				else:
					cv.px(6, y, C("#5a606c"))
					cv.px(9, y, C("#5a606c"))
		"iron_bars":
			cv.fill(Color(0, 0, 0, 0))
			for x in [1, 5, 10, 14]:
				cv.vline(x, 0, 15, C("#8a8d92"))
				cv.vline(x + 1, 0, 15, C("#5f6266"))
			cv.hline(0, 15, 1, C("#9a9da2"))
			cv.hline(0, 15, 14, C("#9a9da2"))
		"ladder":
			cv.fill(Color(0, 0, 0, 0))
			var lc := TexPalettes.cols(oak)
			for x in [2, 13]:
				cv.vline(x, 0, 15, lc[3])
				cv.vline(x - 1, 0, 15, lc[0])
			for y in [2, 6, 10, 14]:
				cv.hline(2, 13, y, lc[0])
				cv.hline(2, 13, y + 1, lc[3])
		"scaffolding_top", "scaffolding_side", "scaffolding_bottom":
			cv.fill(Color(0, 0, 0, 0))
			var bc2 := C("#d8c26a")
			cv.rect_outline(0, 0, 16, 16, bc2)
			cv.rect_outline(1, 1, 14, 14, C("#b8a24a"))
			if n == "scaffolding_side":
				cv.line(1, 14, 14, 1, bc2)
			if n == "scaffolding_top":
				cv.hline(0, 15, 7, bc2)
		"spawner":
			cv.fill(Color(0, 0, 0, 0))
			for i in [0, 5, 10, 15]:
				cv.vline(i, 0, 15, C("#2e3a45"))
				cv.hline(0, 15, i, C("#2e3a45"))
			cv.speckle(C("#52667a"), 12)
		"end_portal_frame_top":
			cv.palette_noise(TexPalettes.cols(["#3a6a5a", "#2f5a4c", "#447a68"]), [4, 3, 2], 0)
			cv.rect_outline(0, 0, 16, 16, C("#dbde9e"))
			cv.rect(4, 4, 8, 8, C("#1a2a26"))
		"end_portal_frame_side":
			cv.palette_noise(TexPalettes.cols(BTexTerrain.END_STONE), [4, 3, 2, 1, 1], 1)
			cv.rect(0, 0, 16, 3, C("#3a6a5a"))
		"end_portal_frame_eye":
			cv.fill(Color(0, 0, 0, 0))
			cv.rect(4, 4, 8, 8, C("#2f7a5a"))
			cv.rect(5, 5, 6, 6, C("#57c28f"))
			cv.rect(7, 6, 2, 4, C("#0f1a14"))
		"end_portal":
			cv.fill(C("#050510"))
			cv.speckle(C("#2c5f58"), 10)
			cv.speckle(C("#7a52a8"), 6)
			cv.speckle(C("#c8e8e0"), 3)
		"end_stone":
			cv.palette_noise(TexPalettes.cols(BTexTerrain.END_STONE), [4, 3, 2, 1, 1], 1)
			cv.speckle(C("#b3b673"), 6)
		"end_stone_bricks":
			BTCommon.bricks(cv, ["#dcdf9f", "#e6e9b0", "#d2d592"], C("#a8ab6c"), 4, 8, 4)
		"purpur_block":
			BTCommon.tiles(cv, ["#a97aa9", "#9d6e9d", "#b588b5"], C("#7e527e"), 8)
		"purpur_pillar", "purpur_pillar_top":
			cv.palette_noise(TexPalettes.cols(["#ab7dab", "#9f719f", "#b88bb8"]), [4, 3, 2], 0)
			if n == "purpur_pillar":
				for x in [0, 3, 12, 15]:
					cv.vline(x, 0, 15, C("#7e527e"))
			else:
				cv.rect_outline(0, 0, 16, 16, C("#7e527e"))
				cv.rect_outline(4, 4, 8, 8, C("#7e527e"))
		"end_rod":
			cv.fill(Color(0, 0, 0, 0))
			cv.rect(7, 2, 2, 14, C("#f2eee6"))
			cv.vline(7, 2, 15, C("#ffffff"))
			cv.rect(6, 0, 4, 2, C("#c8a8d8"))
		"chorus_plant":
			cv.palette_noise(TexPalettes.cols(["#6a4a6a", "#5a3d5a", "#7a5a7a"]), [4, 3, 2], 1)
			cv.speckle(C("#8f6f8f"), 8)
		"chorus_flower", "chorus_flower_dead":
			var pcol := C("#b58fc8") if n == "chorus_flower" else C("#7a6a70")
			cv.palette_noise([pcol, PixelCanvas.shade(pcol, -0.12), PixelCanvas.shade(pcol, 0.12)], [4, 3, 2], 1)
			cv.rect_outline(0, 0, 16, 16, PixelCanvas.shade(pcol, -0.3))
		"dragon_egg":
			cv.palette_noise(TexPalettes.cols(["#0d0a12", "#1a1024", "#120c1a"]), [4, 3, 2], 1)
			cv.speckle(C("#5a2a7a"), 10)
		"netherrack":
			cv.palette_noise(TexPalettes.cols(BTexTerrain.NETHERRACK), [4, 3, 2, 2, 1], 1)
		"soul_sand":
			cv.palette_noise(TexPalettes.cols(["#513e32", "#46352a", "#5c4739", "#3a2b22"]), [4, 3, 2, 1], 1)
			for p in [[3, 4], [10, 3], [6, 10], [12, 11]]:
				cv.rect(p[0], p[1], 2, 2, C("#2a1e17"))
				cv.px(p[0], p[1] + 2, C("#2a1e17"))
		"soul_soil":
			cv.palette_noise(TexPalettes.cols(["#4b3a2f", "#3f3027", "#574437", "#33261f"]), [4, 3, 2, 1], 2)
		"basalt_side", "polished_basalt_side":
			for x in 16:
				var cs := cv.rng.randf_range(-0.07, 0.07)
				for y in 16:
					cv.px(x, y, PixelCanvas.shade(C("#4c4b50").lerp(C("#3e3d42"), cv.rng.randf() * 0.6), cs))
			if n.begins_with("polished"):
				for x in [0, 5, 10, 15]:
					cv.vline(x, 0, 15, C("#5a595e"))
		"basalt_top", "polished_basalt_top":
			cv.palette_noise(TexPalettes.cols(["#5a595e", "#4c4b50", "#66656a"]), [4, 3, 2], 1)
			cv.rect_outline(1, 1, 14, 14, C("#3e3d42"))
		"blackstone":
			cv.palette_noise(TexPalettes.cols(BTexTerrain.BLACKSTONE), [4, 3, 2, 2, 1], 1)
		"blackstone_top":
			cv.palette_noise(TexPalettes.cols(BTexTerrain.BLACKSTONE), [4, 3, 2, 2, 1], 2)
			cv.rect_outline(2, 2, 12, 12, C("#1a161a"))
		"gilded_blackstone":
			cv.palette_noise(TexPalettes.cols(BTexTerrain.BLACKSTONE), [4, 3, 2, 2, 1], 1)
			BTCommon.ore(cv, ["#f2c230", "#c9941a", "#ffe07a"], 7)
		"polished_blackstone":
			BTCommon.polished(cv, ["#352e35", "#2e282e", "#3d353d"])
		"polished_blackstone_bricks", "cracked_polished_blackstone_bricks":
			BTCommon.bricks(cv, ["#342d34", "#3a333a", "#2e282e"], C("#1a161a"), 4, 8, 4)
			if n.begins_with("cracked"):
				BTexTerrain._cracks(cv, C("#120f12"), 4)
		"chiseled_polished_blackstone":
			BTCommon.chiseled(cv, ["#352e35", "#2e282e", "#3d353d"], C("#1a161a"))
		"magma":
			cv.palette_noise(TexPalettes.cols(["#8a2a0a", "#6a1f08", "#a8360c"]), [4, 3, 2], 1)
			BTexTerrain._cracks(cv, C("#ffb13a"), 7)
			cv.speckle(C("#ffe07a"), 4)
		"glowstone":
			cv.palette_noise(TexPalettes.cols(["#b98a3a", "#fcd97a", "#e8b24a", "#8a5f22", "#fff2c0"]), [3, 3, 3, 1, 1], 1)
		"nether_bricks", "cracked_nether_bricks":
			BTCommon.bricks(cv, ["#3c1c22", "#452026", "#34181d"], C("#1a0c0f"), 4, 8, 4)
			if n.begins_with("cracked"):
				BTexTerrain._cracks(cv, C("#120709"), 4)
		"chiseled_nether_bricks":
			BTCommon.chiseled(cv, ["#3c1c22", "#452026", "#34181d"], C("#1a0c0f"))
		"red_nether_bricks":
			BTCommon.bricks(cv, ["#5a0f11", "#681317", "#4c0c0e"], C("#2a0607"), 4, 8, 4)
		"nether_wart_block":
			cv.palette_noise(TexPalettes.cols(["#7a0e0e", "#8f1414", "#650a0a", "#a02020"]), [4, 3, 2, 1], 1)
		"warped_wart_block":
			cv.palette_noise(TexPalettes.cols(["#167d74", "#1c9489", "#10675f", "#2aa99c"]), [4, 3, 2, 1], 1)
		"shroomlight":
			cv.palette_noise(TexPalettes.cols(["#f2983a", "#ffb35a", "#e0802a", "#ffd08a"]), [4, 3, 2, 1], 1)
		"respawn_anchor_top", "respawn_anchor_side", "respawn_anchor_bottom":
			cv.palette_noise(TexPalettes.cols(["#15121e", "#1f1930", "#0d0b13", "#2b2143"]), [4, 3, 3, 1], 1)
			if n == "respawn_anchor_top":
				cv.rect(4, 4, 8, 8, C("#3a1a6a"))
			elif n == "respawn_anchor_side":
				cv.rect(0, 0, 16, 3, C("#2a2a2e"))
				for p in [[3, 7], [9, 9]]:
					cv.rect(p[0], p[1], 3, 3, C("#8b2df0"))
		"lodestone_top", "lodestone_side":
			BTCommon.polished(cv, ["#8a8a90", "#7e7e84", "#96969c"])
			if n == "lodestone_side":
				cv.rect(3, 3, 10, 10, C("#5a5a60"))
				cv.rect(5, 5, 6, 6, C("#9a9aa0"))
		"lever":
			cv.fill(Color(0, 0, 0, 0))
			cv.rect(7, 3, 2, 10, C("#8a6a3f"))
			cv.vline(7, 3, 12, C("#a5834f"))
		"tripwire_hook":
			cv.fill(Color(0, 0, 0, 0))
			cv.rect(7, 2, 2, 12, C("#8a6a3f"))
			cv.rect_outline(5, 1, 6, 4, C("#9a9aa0"))
		"redstone_dust_dot":
			cv.fill(Color(0, 0, 0, 0))
			cv.disc(8, 8, 3.2, Color(0.9, 0.9, 0.9))
			cv.px(7, 7, Color(1, 1, 1))
		"redstone_dust_line":
			cv.fill(Color(0, 0, 0, 0))
			for y in 16:
				cv.px(7, y, Color(0.9, 0.9, 0.9))
				cv.px(8, y, Color(0.8, 0.8, 0.8))
				if y % 3 == 0:
					cv.px(6, y, Color(0.7, 0.7, 0.7))
		"redstone_lamp", "redstone_lamp_on":
			var on := n == "redstone_lamp_on"
			cv.palette_noise(TexPalettes.cols(["#f8d58a", "#fff0b0", "#e8b050"] if on else ["#6a4a2a", "#5a3d22", "#7a5532"]), [4, 3, 2], 1)
			cv.rect_outline(0, 0, 16, 16, C("#b07a3a") if on else C("#3a2a1a"))
			for i in [5, 10]:
				cv.hline(1, 14, i, C("#d89a4a") if on else C("#4a3522"))
				cv.vline(i, 1, 14, C("#d89a4a") if on else C("#4a3522"))
		"repeater", "repeater_on", "comparator", "comparator_on":
			cv.palette_noise(TexPalettes.cols(["#9f9f9f", "#999999", "#a6a6a6"]), [4, 3, 2], 0)
			var wire := C("#ff2a1a") if n.ends_with("_on") else C("#5a1a12")
			cv.vline(8, 2, 13, wire)
			cv.vline(7, 2, 13, PixelCanvas.shade(wire, -0.2))
			cv.rect_outline(0, 0, 16, 16, C("#8a8a8a"))
		"piston_top", "piston_top_sticky":
			BTCommon.planks(cv, oak)
			cv.rect_outline(0, 0, 16, 16, C("#6a6a6a"))
			cv.rect(6, 6, 4, 4, C("#8a8a8a"))
			if n == "piston_top_sticky":
				cv.rect(2, 2, 12, 12, C("#6fb56a"))
				cv.rect(4, 4, 8, 8, C("#8ad082"))
		"piston_side":
			cv.palette_noise(TexPalettes.cols(stone_pal), [4, 3, 2, 1], 1)
			BTCommon.planks(cv, oak)
			for y in range(4, 16):
				for x in 16:
					cv.px(x, y, TexPalettes.cols(stone_pal)[cv.rng.randi_range(0, 3)])
			cv.hline(0, 15, 4, C("#5a5a5a"))
			cv.rect(6, 5, 4, 8, C("#9a8a6a"))
		"piston_bottom", "piston_inner":
			cv.palette_noise(TexPalettes.cols(stone_pal), [4, 3, 2, 1], 1)
			cv.rect_outline(0, 0, 16, 16, C("#5a5a5a"))
			if n == "piston_inner":
				cv.rect(5, 5, 6, 6, C("#9a8a6a"))
		"observer_front", "observer_back", "observer_back_on", "observer_side", "observer_top":
			cv.palette_noise(TexPalettes.cols(["#626262", "#5a5a5a", "#6a6a6a"]), [4, 3, 2], 0)
			cv.rect_outline(0, 0, 16, 16, C("#3a3a3a"))
			if n == "observer_front":
				cv.rect(2, 5, 4, 3, C("#1a1a1a"))
				cv.rect(10, 5, 4, 3, C("#1a1a1a"))
				cv.hline(3, 12, 11, C("#2a2a2a"))
			elif n.begins_with("observer_back"):
				cv.rect(6, 6, 4, 4, C("#ff2a1a") if n.ends_with("_on") else C("#5a1a12"))
			elif n == "observer_top":
				cv.vline(7, 2, 13, C("#3a3a3a"))
				cv.vline(8, 2, 13, C("#8a8a8a"))
		"dispenser_front", "dropper_front", "dispenser_front_vertical", "dropper_front_vertical":
			cv.palette_noise(TexPalettes.cols(stone_pal), [4, 3, 2, 1], 1)
			cv.rect_outline(0, 0, 16, 16, C("#5a5a5a"))
			if n.begins_with("dispenser"):
				cv.disc(8, 8, 3.5, C("#1a1a1a"))
			else:
				cv.rect(5, 6, 6, 4, C("#1a1a1a"))
		"hopper_outside", "hopper_inside", "hopper_top":
			cv.palette_noise(TexPalettes.cols(["#4a4a4d", "#424245", "#535356"]), [4, 3, 2], 1)
			cv.rect_outline(0, 0, 16, 16, C("#2e2e30"))
			if n == "hopper_top":
				cv.rect(2, 2, 12, 12, C("#2a2a2c"))
		"daylight_detector_top", "daylight_detector_inverted_top", "daylight_detector_side":
			BTCommon.planks(cv, oak)
			if n != "daylight_detector_side":
				var glass_c := C("#d8e8f0") if n == "daylight_detector_top" else C("#3a5a8a")
				for p in [[1, 1], [6, 1], [11, 1], [1, 6], [6, 6], [11, 6], [1, 11], [6, 11], [11, 11]]:
					cv.rect(p[0], p[1], 4, 4, glass_c)
		"target_top", "target_side":
			cv.fill(C("#e8dcc8"))
			for r in [7.5, 5.0, 2.5]:
				cv.disc(8, 8, r, C("#d8392b") if int(r) % 5 != 0 else C("#e8dcc8"))
			cv.disc(8, 8, 1.2, C("#d8392b"))
		"rail", "rail_corner", "powered_rail", "powered_rail_on", "detector_rail", "detector_rail_on", "activator_rail", "activator_rail_on":
			cv.fill(Color(0, 0, 0, 0))
			var tie := C("#6b4c2a")
			var metal2 := C("#8a8d92")
			if n.begins_with("powered"):
				metal2 = C("#e8c23a")
			elif n.begins_with("detector"):
				metal2 = C("#8a8d92")
			elif n.begins_with("activator"):
				metal2 = C("#8a8d92")
			if n == "rail_corner":
				for y in [1, 5, 9, 13]:
					cv.hline(y, 15, y, tie)
					cv.hline(y, 15, y + 1, tie)
				for i in 16:
					cv.px(i, 15 - i if i < 13 else 3, metal2)
				for i in range(4, 16):
					cv.px(15, i, metal2)
			else:
				for y in [1, 5, 9, 13]:
					cv.hline(2, 13, y, tie)
					cv.hline(2, 13, y + 1, PixelCanvas.shade(tie, -0.15))
				cv.vline(3, 0, 15, metal2)
				cv.vline(12, 0, 15, metal2)
				if n.ends_with("_on") or n == "detector_rail":
					var accent2 := C("#ff2a1a") if n.ends_with("_on") else C("#5a1a12")
					if n.begins_with("detector"):
						cv.rect(6, 6, 4, 4, accent2)
					else:
						cv.vline(7, 0, 15, accent2)
						cv.vline(8, 0, 15, accent2)
		"iron_door_top", "iron_door_bottom":
			BTCommon.door(cv, ["#d8d8d8", "#c8c8c8", "#e8e8e8", "#a0a0a0"], n == "iron_door_top", 2)
		"iron_trapdoor":
			BTCommon.trapdoor(cv, ["#d8d8d8", "#c8c8c8", "#e8e8e8", "#a0a0a0"], 0)
		"chest", "trapped_chest", "ender_chest":
			if n == "ender_chest":
				cv.palette_noise(TexPalettes.cols(["#1a2a2a", "#15201f", "#243636"]), [4, 3, 2], 1)
				cv.rect_outline(0, 0, 16, 16, C("#0d1414"))
				cv.rect(6, 4, 4, 5, C("#3fbfa0"))
			else:
				BTCommon.planks(cv, oak)
				cv.rect_outline(0, 0, 16, 16, C("#4a3319"))
				cv.hline(0, 15, 5, C("#4a3319"))
				cv.rect(7, 4, 2, 3, C("#d8d8d8") if n == "chest" else C("#b8272b"))
		"cake_top", "cake_bottom", "cake_side", "cake_inner":
			if n == "cake_top":
				cv.fill(C("#f5f0e8"))
				cv.speckle(C("#d8292b"), 6)
			elif n == "cake_side":
				cv.fill(C("#b87a4a"))
				cv.rect(0, 0, 16, 4, C("#f5f0e8"))
				for x in range(0, 16, 3):
					cv.px(x, 4, C("#f5f0e8"))
			elif n == "cake_inner":
				cv.fill(C("#c88a5a"))
				cv.rect(0, 0, 16, 4, C("#f5f0e8"))
			else:
				cv.fill(C("#b87a4a"))
		_:
			if n.ends_with("_front") or n.ends_with("_side") or n.ends_with("_top") or n.ends_with("_back"):
				var head := _head(n)
				if head != null:
					return head
			return null
	return cv


static func _head(n: String) -> PixelCanvas:
	var kinds := {"skeleton_skull": ["#c9c9c4", "#a8a8a2", "#3a3a3a"], "wither_skeleton_skull": ["#2e2e2e", "#1f1f1f", "#0a0a0a"],
		"zombie_head": ["#4f8a3a", "#3f7a2e", "#1a2a14"], "creeper_head": ["#4fa83a", "#3a8a2a", "#0a0a0a"],
		"piglin_head": ["#e8a08a", "#d38a74", "#3a2a22"], "player_head": ["#b58a6a", "#6b4a2a", "#2a3a8a"],
		"dragon_head": ["#1f1a24", "#141018", "#b05ad8"]}
	for k in kinds:
		if n.begins_with(k + "_"):
			var part := n.substr(k.length() + 1)
			var pal: Array = kinds[k]
			var cv := cv_for(n)
			cv.palette_noise(TexPalettes.cols([pal[0], pal[1]]), [4, 3], 1)
			if part == "front":
				match k:
					"creeper_head":
						cv.rect(3, 4, 3, 3, C(pal[2]))
						cv.rect(10, 4, 3, 3, C(pal[2]))
						cv.rect(6, 7, 4, 4, C(pal[2]))
						cv.rect(5, 9, 2, 4, C(pal[2]))
						cv.rect(9, 9, 2, 4, C(pal[2]))
					"skeleton_skull", "wither_skeleton_skull":
						cv.rect(3, 6, 3, 2, C(pal[2]))
						cv.rect(10, 6, 3, 2, C(pal[2]))
						cv.rect(7, 9, 2, 1, C(pal[2]))
						cv.hline(4, 11, 12, C(pal[2]))
					"piglin_head":
						cv.rect(5, 9, 6, 4, C("#f0b8a2"))
						cv.px(6, 10, C(pal[2]))
						cv.px(9, 10, C(pal[2]))
						cv.rect(3, 6, 2, 1, C("#ffffff"))
						cv.rect(11, 6, 2, 1, C("#ffffff"))
					"dragon_head":
						cv.rect(3, 5, 3, 2, C(pal[2]))
						cv.rect(10, 5, 3, 2, C(pal[2]))
					_:
						cv.rect(3, 7, 3, 2, C("#ffffff") if k == "player_head" else C(pal[2]))
						cv.rect(10, 7, 3, 2, C("#ffffff") if k == "player_head" else C(pal[2]))
						if k == "player_head":
							cv.px(4, 7, C(pal[2]))
							cv.px(11, 7, C(pal[2]))
							cv.rect(0, 0, 16, 4, C(pal[1]))
							cv.hline(5, 10, 12, C("#7a4a3a"))
			elif part == "top" and k == "player_head":
				cv.fill(C(pal[1]))
			return cv
	return null
