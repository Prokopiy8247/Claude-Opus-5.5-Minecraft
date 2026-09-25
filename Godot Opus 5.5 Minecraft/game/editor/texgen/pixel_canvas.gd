class_name PixelCanvas
extends RefCounted
## Small pixel-art drawing helper used by the procedural texture generators.
## All randomness is seeded from the texture name so regeneration is deterministic.

var w: int
var h: int
var img: Image
var rng := RandomNumberGenerator.new()


func _init(width: int = 16, height: int = 16, seed_text: String = "") -> void:
	w = width
	h = height
	img = Image.create(w, h, false, Image.FORMAT_RGBA8)
	img.fill(Color(0, 0, 0, 0))
	reseed(seed_text)


func reseed(seed_text: String) -> void:
	rng.seed = hash(seed_text) & 0x7FFFFFFF


func px(x: int, y: int, c: Color) -> void:
	if x >= 0 and y >= 0 and x < w and y < h:
		img.set_pixel(x, y, c)


func get_px(x: int, y: int) -> Color:
	return img.get_pixel(clampi(x, 0, w - 1), clampi(y, 0, h - 1))


func wrap_px(x: int, y: int, c: Color) -> void:
	img.set_pixel(posmod(x, w), posmod(y, h), c)


func fill(c: Color) -> PixelCanvas:
	img.fill(c)
	return self


func rect(x: int, y: int, rw: int, rh: int, c: Color) -> void:
	for yy in range(y, y + rh):
		for xx in range(x, x + rw):
			px(xx, yy, c)


func rect_outline(x: int, y: int, rw: int, rh: int, c: Color) -> void:
	for xx in range(x, x + rw):
		px(xx, y, c)
		px(xx, y + rh - 1, c)
	for yy in range(y, y + rh):
		px(x, yy, c)
		px(x + rw - 1, yy, c)


func hline(x0: int, x1: int, y: int, c: Color) -> void:
	for x in range(mini(x0, x1), maxi(x0, x1) + 1):
		px(x, y, c)


func vline(x: int, y0: int, y1: int, c: Color) -> void:
	for y in range(mini(y0, y1), maxi(y0, y1) + 1):
		px(x, y, c)


func line(x0: int, y0: int, x1: int, y1: int, c: Color) -> void:
	var dx := absi(x1 - x0)
	var dy := -absi(y1 - y0)
	var sx := 1 if x0 < x1 else -1
	var sy := 1 if y0 < y1 else -1
	var err := dx + dy
	while true:
		px(x0, y0, c)
		if x0 == x1 and y0 == y1:
			break
		var e2 := 2 * err
		if e2 >= dy:
			err += dy
			x0 += sx
		if e2 <= dx:
			err += dx
			y0 += sy


func disc(cx: float, cy: float, r: float, c: Color) -> void:
	for y in h:
		for x in w:
			var dx := x + 0.5 - cx
			var dy := y + 0.5 - cy
			if dx * dx + dy * dy <= r * r:
				px(x, y, c)


## Fill with per-pixel shade noise around a base colour. amp is +-brightness.
func noise(base: Color, amp: float = 0.08, cluster: float = 0.0) -> PixelCanvas:
	var vals := PackedFloat32Array()
	vals.resize(w * h)
	for i in w * h:
		vals[i] = rng.randf_range(-1.0, 1.0)
	if cluster > 0.0:
		var out := PackedFloat32Array()
		out.resize(w * h)
		for y in h:
			for x in w:
				var s := vals[y * w + x]
				s += (vals[y * w + posmod(x + 1, w)] + vals[y * w + posmod(x - 1, w)] + vals[posmod(y + 1, h) * w + x] + vals[posmod(y - 1, h) * w + x]) * cluster
				out[y * w + x] = s / (1.0 + 4.0 * cluster) * (1.0 + cluster)
		vals = out
	for y in h:
		for x in w:
			px(x, y, shade(base, vals[y * w + x] * amp))
	return self


## Palette noise: picks from palette with optional weights; clustered for Minecraft-like mottling.
func palette_noise(pal: Array, weights: Array = [], cluster: int = 0) -> PixelCanvas:
	var total := 0.0
	var ws := []
	for i in pal.size():
		var wv: float = weights[i] if i < weights.size() else 1.0
		ws.append(wv)
		total += wv
	for y in h:
		for x in w:
			var r := rng.randf() * total
			var k := 0
			while k < pal.size() - 1 and r > ws[k]:
				r -= ws[k]
				k += 1
			px(x, y, pal[k])
	for _i in cluster:
		var src := img.duplicate() as Image
		for y in h:
			for x in w:
				if rng.randf() < 0.35:
					var nx := posmod(x + rng.randi_range(-1, 1), w)
					var ny := posmod(y + rng.randi_range(-1, 1), h)
					px(x, y, src.get_pixel(nx, ny))
	return self


## Random speckles of a colour.
func speckle(c: Color, count: int, size: int = 1) -> void:
	for i in count:
		var x := rng.randi_range(0, w - 1)
		var y := rng.randi_range(0, h - 1)
		for yy in size:
			for xx in size:
				wrap_px(x + xx, y + yy, c)


## Blob cluster (ore-like): count blobs of 2..4 pixels.
func blobs(pal: Array, count: int, min_size: int = 2, max_size: int = 4, margin: int = 1) -> void:
	for i in count:
		var cx := rng.randi_range(margin, w - 1 - margin)
		var cy := rng.randi_range(margin, h - 1 - margin)
		var n := rng.randi_range(min_size, max_size)
		var x := cx
		var y := cy
		for k in n:
			var c: Color = pal[rng.randi_range(0, pal.size() - 1)]
			px(x, y, c)
			match rng.randi_range(0, 3):
				0: x += 1
				1: x -= 1
				2: y += 1
				3: y -= 1
			x = clampi(x, margin, w - 1 - margin)
			y = clampi(y, margin, h - 1 - margin)


func darken_edges(amount: float = 0.1) -> void:
	for y in h:
		for x in w:
			if x == 0 or y == 0 or x == w - 1 or y == h - 1:
				px(x, y, shade(get_px(x, y), -amount))


func map_colors(f: Callable) -> void:
	for y in h:
		for x in w:
			img.set_pixel(x, y, f.call(img.get_pixel(x, y), x, y))


func tint(c: Color) -> void:
	for y in h:
		for x in w:
			var p := img.get_pixel(x, y)
			img.set_pixel(x, y, Color(p.r * c.r, p.g * c.g, p.b * c.b, p.a))


func to_gray(keep_alpha := true) -> void:
	for y in h:
		for x in w:
			var p := img.get_pixel(x, y)
			var l := p.r * 0.3 + p.g * 0.59 + p.b * 0.11
			img.set_pixel(x, y, Color(l, l, l, p.a if keep_alpha else 1.0))


func set_alpha_all(a: float) -> void:
	for y in h:
		for x in w:
			var p := img.get_pixel(x, y)
			if p.a > 0.0:
				p.a = a
				img.set_pixel(x, y, p)


## Blit another image (alpha aware).
func blit(src: Image, ox: int = 0, oy: int = 0) -> void:
	for y in src.get_height():
		for x in src.get_width():
			var c := src.get_pixel(x, y)
			if c.a > 0.01:
				if c.a >= 0.99:
					px(x + ox, y + oy, c)
				else:
					var d := get_px(x + ox, y + oy)
					px(x + ox, y + oy, d.lerp(Color(c.r, c.g, c.b, 1.0), c.a))


## Draw ASCII art: rows of chars, palette maps char -> Color. '.' or ' ' = transparent (skipped).
func art(rows: Array, pal: Dictionary, ox: int = 0, oy: int = 0) -> void:
	for y in rows.size():
		var row: String = rows[y]
		for x in row.length():
			var ch := row[x]
			if ch == "." or ch == " ":
				continue
			if pal.has(ch):
				px(x + ox, y + oy, pal[ch])


func copy() -> PixelCanvas:
	var c := PixelCanvas.new(w, h)
	c.img = img.duplicate() as Image
	c.rng.seed = rng.seed
	return c


static func shade(c: Color, amount: float) -> Color:
	if amount >= 0.0:
		return Color(c.r + (1.0 - c.r) * amount * 0.6 + amount * 0.4 * c.r, c.g + (1.0 - c.g) * amount * 0.6 + amount * 0.4 * c.g,
			c.b + (1.0 - c.b) * amount * 0.6 + amount * 0.4 * c.b, c.a).clamp()
	var k := 1.0 + amount
	return Color(c.r * k, c.g * k, c.b * k, c.a)


static func hex(s: String) -> Color:
	return Color.html(s)


static func mix(a: Color, b: Color, t: float) -> Color:
	return a.lerp(b, t)
