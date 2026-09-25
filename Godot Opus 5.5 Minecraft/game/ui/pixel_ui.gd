class_name PixelUI
extends RefCounted
## Original pixel-art UI toolkit: bitmap font (from PixelFontData), bevelled panels, slots,
## buttons, bars and HUD sprites (hearts, food, armour, bubbles) — all drawn at an integer GUI
## scale with nearest filtering.

const TEXT := Color(1, 1, 1)
const TEXT_DARK := Color(0.25, 0.25, 0.25)
const TEXT_GRAY := Color(0.66, 0.66, 0.66)
const TEXT_YELLOW := Color(1.0, 1.0, 0.33)
const TEXT_AQUA := Color(0.33, 1.0, 1.0)
const TEXT_RED := Color(1.0, 0.33, 0.33)
const TEXT_GREEN := Color(0.33, 1.0, 0.33)
const PANEL := Color8(198, 198, 198)
const PANEL_LIGHT := Color8(255, 255, 255)
const PANEL_DARK := Color8(85, 85, 85)
const SLOT := Color8(139, 139, 139)
const SLOT_DARK := Color8(55, 55, 55)
const OUTLINE := Color8(0, 0, 0)

static var font: FontFile = null
static var _sprites: Dictionary = {}
static var _widths: Dictionary = {}

# ---------------------------------------------------------------------------- font
static func get_font() -> FontFile:
	if font != null:
		return font
	font = FontFile.new()
	font.fixed_size = 8
	font.fixed_size_scale_mode = TextServer.FIXED_SIZE_SCALE_INTEGER_ONLY
	font.antialiasing = TextServer.FONT_ANTIALIASING_NONE
	font.hinting = TextServer.HINTING_NONE
	font.subpixel_positioning = TextServer.SUBPIXEL_POSITIONING_DISABLED
	font.generate_mipmaps = false
	var glyphs: Dictionary = PixelFontData.GLYPHS
	var cols := 16
	var cell := 8
	var n := glyphs.size()
	var rows := int(ceil(float(n) / cols))
	var img := Image.create_empty(cols * cell, maxi(rows, 1) * cell, false, Image.FORMAT_LA8)
	var i := 0
	var size_key := Vector2i(8, 0)
	for ch in glyphs:
		var rows_s: Array = glyphs[ch]
		var w := String(rows_s[0]).length()
		var gx := (i % cols) * cell
		var gy := (i / cols) * cell
		for y in rows_s.size():
			var row: String = rows_s[y]
			for x in row.length():
				if row.substr(x, 1) == "#":
					img.set_pixel(gx + x, gy + y, Color(1, 1, 1, 1))
		var code := String(ch).unicode_at(0)
		font.set_glyph_advance(0, 8, code, Vector2(w + 1, 0))
		font.set_glyph_offset(0, size_key, code, Vector2(0, -7))
		font.set_glyph_size(0, size_key, code, Vector2(w, 8))
		font.set_glyph_uv_rect(0, size_key, code, Rect2(gx, gy, w, 8))
		font.set_glyph_texture_idx(0, size_key, code, 0)
		_widths[code] = w + 1
		i += 1
	font.set_texture_image(0, size_key, 0, img)
	font.set_cache_ascent(0, 8, 7.0)
	font.set_cache_descent(0, 8, 1.0)
	return font


## Width of a string in GUI pixels (unscaled).
static func text_width(s: String) -> int:
	get_font()
	var w := 0
	for i in s.length():
		w += int(_widths.get(s.unicode_at(i), 6))
	return maxi(0, w - 1)


## Draws text with the classic drop shadow. pos = top-left in screen pixels.
static func text(ci: CanvasItem, pos: Vector2, s: String, color := TEXT, scale := 2, shadow := true) -> void:
	var f := get_font()
	var fs := 8 * scale
	var base := pos + Vector2(0, 7 * scale)
	if shadow:
		ci.draw_string(f, base + Vector2(scale, scale), s, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, Color(color.r * 0.25, color.g * 0.25, color.b * 0.25, color.a))
	ci.draw_string(f, base, s, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, color)


static func text_centered(ci: CanvasItem, center_x: float, y: float, s: String, color := TEXT, scale := 2, shadow := true) -> void:
	var w := text_width(s) * scale
	text(ci, Vector2(roundf(center_x - w * 0.5), y), s, color, scale, shadow)


# ---------------------------------------------------------------------------- widgets
## Inventory-style panel with rounded bevelled border (like the classic container background).
static func panel(ci: CanvasItem, r: Rect2, s: int) -> void:
	var x := r.position.x
	var y := r.position.y
	var w := r.size.x
	var h := r.size.y
	ci.draw_rect(Rect2(x + s, y, w - 2 * s, h), OUTLINE)
	ci.draw_rect(Rect2(x, y + s, w, h - 2 * s), OUTLINE)
	ci.draw_rect(Rect2(x + s, y + s, w - 2 * s, h - 2 * s), PANEL)
	# highlight top-left, shadow bottom-right (2px bevel)
	ci.draw_rect(Rect2(x + s, y + s, w - 3 * s, 2 * s), PANEL_LIGHT)
	ci.draw_rect(Rect2(x + s, y + s, 2 * s, h - 3 * s), PANEL_LIGHT)
	ci.draw_rect(Rect2(x + 2 * s, y + h - 3 * s, w - 3 * s, 2 * s), PANEL_DARK)
	ci.draw_rect(Rect2(x + w - 3 * s, y + 2 * s, 2 * s, h - 3 * s), PANEL_DARK)
	ci.draw_rect(Rect2(x + 3 * s, y + 3 * s, w - 6 * s, h - 6 * s), PANEL)


## 18x18 item slot at a GUI pixel rect (x,y is the slot's top-left in screen pixels).
static func slot(ci: CanvasItem, p: Vector2, s: int, size := 18) -> void:
	var r := Rect2(p, Vector2(size, size) * s)
	ci.draw_rect(r, SLOT)
	ci.draw_rect(Rect2(p, Vector2(size - 1, 1) * s), SLOT_DARK)
	ci.draw_rect(Rect2(p, Vector2(1, size - 1) * s), SLOT_DARK)
	ci.draw_rect(Rect2(p + Vector2(1, size - 1) * s, Vector2(size - 1, 1) * s), PANEL_LIGHT)
	ci.draw_rect(Rect2(p + Vector2(size - 1, 1) * s, Vector2(1, size - 1) * s), PANEL_LIGHT)


## Stone-like button (normal / hover / disabled).
static func button(ci: CanvasItem, r: Rect2, label: String, s: int, hover := false, disabled := false) -> void:
	var base := Color8(111, 111, 111)
	if disabled:
		base = Color8(44, 44, 44)
	elif hover:
		base = Color8(122, 132, 188)
	ci.draw_rect(r, OUTLINE)
	var inner := r.grow(-s)
	ci.draw_rect(inner, base)
	ci.draw_rect(Rect2(inner.position, Vector2(inner.size.x, s)), base.lightened(0.35))
	ci.draw_rect(Rect2(inner.position, Vector2(s, inner.size.y)), base.lightened(0.25))
	ci.draw_rect(Rect2(inner.position + Vector2(0, inner.size.y - 2 * s), Vector2(inner.size.x, 2 * s)), base.darkened(0.35))
	var col := TEXT_GRAY if disabled else (TEXT_YELLOW if hover else TEXT)
	text_centered(ci, r.position.x + r.size.x * 0.5, r.position.y + (r.size.y - 8 * s) * 0.5 + s * 0.5, label, col, s)


## Dark translucent box used for tooltips / chat.
static func tooltip_box(ci: CanvasItem, r: Rect2, s: int) -> void:
	ci.draw_rect(r.grow(-s), Color(0.06, 0.0, 0.1, 0.94))
	ci.draw_rect(Rect2(r.position + Vector2(s, 0), Vector2(r.size.x - 2 * s, s)), Color(0.06, 0.0, 0.1, 0.94))
	ci.draw_rect(Rect2(r.position + Vector2(s, r.size.y - s), Vector2(r.size.x - 2 * s, s)), Color(0.06, 0.0, 0.1, 0.94))
	ci.draw_rect(Rect2(r.position + Vector2(0, s), Vector2(s, r.size.y - 2 * s)), Color(0.06, 0.0, 0.1, 0.94))
	ci.draw_rect(Rect2(r.position + Vector2(r.size.x - s, s), Vector2(s, r.size.y - 2 * s)), Color(0.06, 0.0, 0.1, 0.94))
	var edge := Color(0.31, 0.0, 1.0, 0.45)
	var edge2 := Color(0.16, 0.0, 0.5, 0.45)
	ci.draw_rect(Rect2(r.position + Vector2(s, s), Vector2(r.size.x - 2 * s, s)), edge)
	ci.draw_rect(Rect2(r.position + Vector2(s, r.size.y - 2 * s), Vector2(r.size.x - 2 * s, s)), edge2)
	ci.draw_rect(Rect2(r.position + Vector2(s, 2 * s), Vector2(s, r.size.y - 4 * s)), edge)
	ci.draw_rect(Rect2(r.position + Vector2(r.size.x - 2 * s, 2 * s), Vector2(s, r.size.y - 4 * s)), edge2)


## Draws an item stack icon + count + durability bar in a 16x16 area at p.
static func item(ci: CanvasItem, p: Vector2, st: ItemStack, s: int, show_count := true) -> void:
	if st == null or st.is_empty():
		return
	var tex := ItemIcons.icon_for_stack(st)
	if tex != null:
		ci.draw_texture_rect(tex, Rect2(p, Vector2(16, 16) * s), false)
	if st.has_glint():
		var t := Time.get_ticks_msec() / 1000.0
		var a := 0.18 + 0.12 * sin(t * 3.0)
		ci.draw_rect(Rect2(p, Vector2(16, 16) * s), Color(0.6, 0.3, 1.0, a))
	var it := st.item()
	if it != null and it.is_damageable() and st.damage > 0:
		var frac := 1.0 - float(st.damage) / float(it.durability)
		var bw := roundi(13.0 * frac)
		ci.draw_rect(Rect2(p + Vector2(2, 13) * s, Vector2(13, 2) * s), Color(0, 0, 0))
		ci.draw_rect(Rect2(p + Vector2(2, 13) * s, Vector2(bw, 1) * s), Color.from_hsv(frac / 3.0, 1.0, 1.0))
	if show_count and st.count > 1:
		var cs := str(st.count)
		var w := text_width(cs) * s
		text(ci, p + Vector2(17 * s - w, 9 * s), cs, TEXT, s)


# ---------------------------------------------------------------------------- HUD sprites (9x9)
const SPRITES := {
	"heart": [
		".kk...kk.",
		"k55k.k55k",
		"k5ww5555k",
		"k5w55555k",
		"k5555555k",
		".k55555k.",
		"..k555k..",
		"...k5k...",
		"....k....",
	],
	"heart_half": [
		".kk...kk.",
		"k55k.kddk",
		"k5w5kdddk",
		"k55ddddkk",
		"k5ddddddk",
		".k5dddkk.",
		"..k5dk...",
		"...kk....",
		"....k....",
	],
	"heart_empty": [
		".kk...kk.",
		"kddk.kddk",
		"kddddddk.",
		"kdddddddk",
		"kdddddddk",
		".kdddddk.",
		"..kdddk..",
		"...kdk...",
		"....k....",
	],
	"food": [
		"....kkk..",
		"...k555k.",
		"..k5w555k",
		"..k55555k",
		".kk5555k.",
		"kwk.kkk..",
		"kkwk.....",
		".kwk.....",
		"..k......",
	],
	"food_half": [
		"....kkk..",
		"...kdd5k.",
		"..kddd5k.",
		"..kddd55k",
		".kkddd5k.",
		"kwk.kkk..",
		"kkwk.....",
		".kwk.....",
		"..k......",
	],
	"food_empty": [
		"....kkk..",
		"...kdddk.",
		"..kddddk.",
		"..kddddk.",
		".kkdddk..",
		"kdk.kk...",
		"kkdk.....",
		".kdk.....",
		"..k......",
	],
	"armor": [
		"kk.kkk.kk",
		"k5k555k5k",
		"k5555555k",
		".k55555k.",
		".k5w555k.",
		".k55555k.",
		".k55555k.",
		"..kkkkk..",
		".........",
	],
	"armor_half": [
		"kk.kkk.kk",
		"k5k5ddkdk",
		"k555dddk.",
		".k55dddk.",
		".k5wdddk.",
		".k55dddk.",
		".k55dddk.",
		"..kkkkk..",
		".........",
	],
	"armor_empty": [
		"kk.kkk.kk",
		"kdkdddkdk",
		"kdddddddk",
		".kdddddk.",
		".kdddddk.",
		".kdddddk.",
		".kdddddk.",
		"..kkkkk..",
		".........",
	],
	"bubble": [
		"..kkkkk..",
		".k55555k.",
		"k5ww5555k",
		"k5w55555k",
		"k5555555k",
		"k5555555k",
		".k55555k.",
		"..kkkkk..",
		".........",
	],
	"bubble_pop": [
		".........",
		"..k...k..",
		".k.....k.",
		".........",
		"k...5...k",
		".........",
		".k.....k.",
		"..k...k..",
		".........",
	],
	"crosshair": [
		"....w....",
		"....w....",
		"....w....",
		"....w....",
		"wwwwwwwww",
		"....w....",
		"....w....",
		"....w....",
		"....w....",
	],
}


## Sprite texture tinted with a main colour (5), dark colour (d), highlight (w) and outline (k).
static func sprite(name: String, main: Color, dark := Color(0.2, 0.2, 0.2, 0.9), outline := Color(0.1, 0.02, 0.02),
		hl := Color(1, 1, 1)) -> Texture2D:
	var key := "%s|%s|%s|%s" % [name, main.to_html(), dark.to_html(), outline.to_html()]
	if _sprites.has(key):
		return _sprites[key]
	var rows: Array = SPRITES.get(name, SPRITES["heart"])
	var img := Image.create_empty(9, 9, false, Image.FORMAT_RGBA8)
	for y in mini(9, rows.size()):
		var row: String = rows[y]
		for x in mini(9, row.length()):
			var ch := row.substr(x, 1)
			match ch:
				"5":
					img.set_pixel(x, y, main)
				"d":
					img.set_pixel(x, y, dark)
				"w":
					img.set_pixel(x, y, hl)
				"k":
					img.set_pixel(x, y, outline)
	var t := ImageTexture.create_from_image(img)
	_sprites[key] = t
	return t


## Dirt-like tiled background texture (menus).
static var _bg: ImageTexture = null

static func menu_background() -> Texture2D:
	if _bg != null:
		return _bg
	var img := ItemIcons.block_texture_image("dirt").duplicate() as Image
	for y in img.get_height():
		for x in img.get_width():
			var c := img.get_pixel(x, y)
			img.set_pixel(x, y, Color(c.r * 0.25, c.g * 0.25, c.b * 0.25, 1.0))
	_bg = ImageTexture.create_from_image(img)
	return _bg


static func tile_background(ci: CanvasItem, r: Rect2, s: int, darkness := 1.0) -> void:
	var tex := menu_background()
	var step := 16 * s * 2
	var y := 0.0
	while y < r.size.y:
		var x := 0.0
		while x < r.size.x:
			ci.draw_texture_rect(tex, Rect2(r.position + Vector2(x, y), Vector2(step, step)), false, Color(darkness, darkness, darkness))
			x += step
		y += step
