class_name BTCommon
extends RefCounted
## Shared procedural painting routines for block textures (16x16).



static func C(s: String) -> Color:
	return Color.html(s)


static func cols(a: Array) -> Array:
	return TexPalettes.cols(a)


static func stone_like(cv: PixelCanvas, pal: Array, weights: Array = [], cluster: int = 2) -> void:
	cv.palette_noise(cols(pal), weights, cluster)


## Voronoi cobbles: rounded stones separated by dark mortar.
static func cobble(cv: PixelCanvas, light: Array, mortar: Color, seeds: int = 9) -> void:
	var pts := []
	for i in seeds:
		pts.append(Vector2(cv.rng.randf_range(0, 16), cv.rng.randf_range(0, 16)))
	var shades := []
	for i in seeds:
		shades.append(cv.rng.randf_range(-0.12, 0.12))
	var lc := cols(light)
	for y in 16:
		for x in 16:
			var best := 1e9
			var second := 1e9
			var bi := 0
			for i in seeds:
				for oy in [-16, 0, 16]:
					for ox in [-16, 0, 16]:
						var d := Vector2(x + 0.5 + ox, y + 0.5 + oy).distance_to(pts[i])
						if d < best:
							second = best
							best = d
							bi = i
						elif d < second:
							second = d
			if second - best < 1.1:
				cv.px(x, y, PixelCanvas.shade(mortar, cv.rng.randf_range(-0.06, 0.06)))
			else:
				var base: Color = lc[bi % lc.size()]
				var edge := clampf((second - best) / 4.0, 0.0, 1.0)
				var s: float = shades[bi] + (edge - 0.5) * 0.12 + cv.rng.randf_range(-0.05, 0.05)
				cv.px(x, y, PixelCanvas.shade(base, s))


## Classic bricks. rows = number of brick rows, bw = brick width.
static func bricks(cv: PixelCanvas, brick: Array, mortar: Color, rows: int = 4, bw: int = 8, offset: int = -1, mortar_jitter := 0.05) -> void:
	var rh := 16 / rows
	if offset < 0:
		offset = bw / 2
	var bc := cols(brick)
	for y in 16:
		var row := y / rh
		var ry := y % rh
		var shift := (row % 2) * offset
		for x in 16:
			var bx := posmod(x + shift, bw)
			if ry == rh - 1 or bx == bw - 1:
				cv.px(x, y, PixelCanvas.shade(mortar, cv.rng.randf_range(-mortar_jitter, mortar_jitter)))
			else:
				var idx := posmod(int((x + shift) / bw) * 7 + row * 3, bc.size())
				var base: Color = bc[idx]
				var s := cv.rng.randf_range(-0.05, 0.05)
				if ry == 0:
					s += 0.07
				if bx == bw - 2:
					s -= 0.05
				cv.px(x, y, PixelCanvas.shade(base, s))


## Large stone bricks: two rows, top brick spans the width, bottom split at 8.
static func big_bricks(cv: PixelCanvas, pal: Array, mortar: Color, highlight := 0.08) -> void:
	var pc := cols(pal)
	for y in 16:
		for x in 16:
			var top := y < 8
			var ry := y if top else y - 8
			var mort := ry == 7 or (not top and (x == 7)) or (top and x == 15)
			if mort:
				cv.px(x, y, PixelCanvas.shade(mortar, cv.rng.randf_range(-0.05, 0.03)))
			else:
				var base: Color = pc[cv.rng.randi_range(0, pc.size() - 1)]
				var s := 0.0
				if ry == 0:
					s += highlight
				if ry == 6:
					s -= highlight * 0.6
				cv.px(x, y, PixelCanvas.shade(base, s))


static func tiles(cv: PixelCanvas, pal: Array, mortar: Color, size: int = 4) -> void:
	var pc := cols(pal)
	for y in 16:
		for x in 16:
			var tx := x % size
			var ty := y % size
			if tx == size - 1 or ty == size - 1:
				cv.px(x, y, PixelCanvas.shade(mortar, cv.rng.randf_range(-0.04, 0.04)))
			else:
				var base: Color = pc[cv.rng.randi_range(0, pc.size() - 1)]
				var s := 0.05 if ty == 0 or tx == 0 else 0.0
				cv.px(x, y, PixelCanvas.shade(base, s + cv.rng.randf_range(-0.03, 0.03)))


static func polished(cv: PixelCanvas, pal: Array, border := true) -> void:
	cv.palette_noise(cols(pal), [4, 2, 2], 1)
	if border:
		var base: Color = cols(pal)[0]
		for i in 16:
			cv.px(i, 0, PixelCanvas.shade(base, 0.12))
			cv.px(0, i, PixelCanvas.shade(base, 0.08))
			cv.px(i, 15, PixelCanvas.shade(base, -0.18))
			cv.px(15, i, PixelCanvas.shade(base, -0.14))


static func chiseled(cv: PixelCanvas, pal: Array, dark: Color) -> void:
	polished(cv, pal, true)
	var base: Color = cols(pal)[0]
	cv.rect_outline(2, 2, 12, 12, dark)
	cv.rect_outline(3, 3, 10, 10, PixelCanvas.shade(base, 0.1))
	cv.rect_outline(5, 5, 6, 6, dark)
	cv.rect(7, 7, 2, 2, PixelCanvas.shade(base, -0.25))


static func planks(cv: PixelCanvas, pal: Array) -> void:
	var pc := cols(pal)
	var base: Color = pc[0]
	for board in 4:
		var y0 := board * 4
		var seam: int = [3, 11, 6, 14][board]
		var bshade: float = [0.02, -0.03, 0.04, -0.01][board]
		for y in range(y0, y0 + 4):
			for x in 16:
				var c := base
				var r := cv.rng.randf()
				if r < 0.22:
					c = pc[1]
				elif r < 0.36:
					c = pc[2]
				var s := bshade
				if y == y0 + 3:
					c = pc[3]
					s = -0.05
				elif x == seam:
					c = pc[3]
					s = 0.0
				cv.px(x, y, PixelCanvas.shade(c, s))
		# grain streaks
		for k in 2:
			var gy := y0 + cv.rng.randi_range(0, 2)
			var gx := cv.rng.randi_range(0, 12)
			for x in range(gx, gx + cv.rng.randi_range(2, 5)):
				if x != seam:
					cv.px(x, gy, PixelCanvas.shade(pc[1], -0.06))


static func log_side(cv: PixelCanvas, bark: Array) -> void:
	var bc := cols(bark)
	var colshade := []
	for x in 16:
		colshade.append(cv.rng.randf_range(-0.08, 0.08))
	for y in 16:
		for x in 16:
			var c: Color = bc[0]
			var r := cv.rng.randf()
			if r < 0.3:
				c = bc[1]
			elif r < 0.45:
				c = bc[2]
			cv.px(x, y, PixelCanvas.shade(c, colshade[x]))
	# vertical cracks
	for k in 5:
		var x := cv.rng.randi_range(0, 15)
		var y0 := cv.rng.randi_range(0, 15)
		var ln := cv.rng.randi_range(3, 8)
		for y in range(y0, y0 + ln):
			cv.wrap_px(x, y, bc[3])


static func log_top(cv: PixelCanvas, ring: Array, bark: Array) -> void:
	var rc := cols(ring)
	var bc := cols(bark)
	for y in 16:
		for x in 16:
			var d := maxi(absi(x * 2 - 15), absi(y * 2 - 15)) / 2
			var c: Color
			if d >= 7:
				c = bc[cv.rng.randi_range(0, 2)]
			else:
				c = rc[0] if (d % 2 == 0) else rc[1]
				if d <= 1:
					c = rc[1]
				c = PixelCanvas.shade(c, cv.rng.randf_range(-0.04, 0.04))
			cv.px(x, y, c)


static func stripped_side(cv: PixelCanvas, ring: Array) -> void:
	var rc := cols(ring)
	for y in 16:
		for x in 16:
			var c: Color = rc[0]
			if x % 4 == 1 and cv.rng.randf() < 0.7:
				c = rc[1]
			elif cv.rng.randf() < 0.15:
				c = rc[2]
			cv.px(x, y, PixelCanvas.shade(c, cv.rng.randf_range(-0.03, 0.03)))


static func stripped_top(cv: PixelCanvas, ring: Array) -> void:
	var rc := cols(ring)
	for y in 16:
		for x in 16:
			var d := maxi(absi(x * 2 - 15), absi(y * 2 - 15)) / 2
			var c: Color = rc[0] if d % 2 == 0 else rc[1]
			if d == 7:
				c = PixelCanvas.shade(rc[1], -0.1)
			cv.px(x, y, PixelCanvas.shade(c, cv.rng.randf_range(-0.03, 0.03)))


## Leaves: grayscale (for biome tint) or coloured, with transparent holes.
static func leaves(cv: PixelCanvas, pal: Array = [], hole_rate := 0.2, flowers: Array = []) -> void:
	var pc: Array = cols(pal) if pal.size() > 0 else [Color(0.62, 0.62, 0.62), Color(0.5, 0.5, 0.5), Color(0.72, 0.72, 0.72), Color(0.4, 0.4, 0.4)]
	cv.fill(Color(0, 0, 0, 0))
	for y in 16:
		for x in 16:
			if cv.rng.randf() < hole_rate:
				continue
			var r := cv.rng.randf()
			var c: Color = pc[0]
			if r < 0.3:
				c = pc[1]
			elif r < 0.5:
				c = pc[2]
			elif r < 0.62:
				c = pc[3]
			cv.px(x, y, c)
	for f in flowers:
		var fc: Color = C(f)
		for k in 5:
			var fx := cv.rng.randi_range(1, 14)
			var fy := cv.rng.randi_range(1, 14)
			cv.px(fx, fy, fc)
			cv.px(fx + 1, fy, PixelCanvas.shade(fc, -0.1))
			cv.px(fx, fy + 1, PixelCanvas.shade(fc, 0.1))


static func ore(cv: PixelCanvas, pal: Array, clusters: int = 5) -> void:
	var oc := cols(pal)
	for i in clusters:
		var cx := cv.rng.randi_range(1, 13)
		var cy := cv.rng.randi_range(1, 13)
		var shape := cv.rng.randi_range(0, 3)
		var cells := [[0, 0], [1, 0], [0, 1], [1, 1]]
		if shape == 1:
			cells = [[0, 0], [1, 0], [2, 0], [1, 1]]
		elif shape == 2:
			cells = [[0, 0], [0, 1], [1, 1], [1, 2]]
		elif shape == 3:
			cells = [[1, 0], [0, 1], [1, 1], [2, 1], [1, 2]]
		for cc in cells:
			cv.px(cx + cc[0], cy + cc[1], oc[0])
		cv.px(cx + cells[0][0], cy + cells[0][1], oc[2 if oc.size() > 2 else 0])
		var last = cells[cells.size() - 1]
		cv.px(cx + last[0], cy + last[1], oc[1])
		if oc.size() > 3 and cv.rng.randf() < 0.5:
			cv.px(cx + cells[1][0], cy + cells[1][1], oc[3])


static func wool(cv: PixelCanvas, col: Color) -> void:
	for y in 16:
		for x in 16:
			var s := cv.rng.randf_range(-0.05, 0.05)
			if (x + y) % 4 == 0:
				s += 0.04
			if (x - y + 32) % 4 == 0:
				s -= 0.04
			cv.px(x, y, PixelCanvas.shade(col, s))


static func concrete(cv: PixelCanvas, col: Color) -> void:
	for y in 16:
		for x in 16:
			cv.px(x, y, PixelCanvas.shade(col, cv.rng.randf_range(-0.02, 0.02)))


static func powder(cv: PixelCanvas, col: Color) -> void:
	for y in 16:
		for x in 16:
			var s := cv.rng.randf_range(-0.1, 0.1)
			if cv.rng.randf() < 0.12:
				s += 0.18
			cv.px(x, y, PixelCanvas.shade(col, s))


static func terracotta(cv: PixelCanvas, col: Color) -> void:
	for y in 16:
		for x in 16:
			cv.px(x, y, PixelCanvas.shade(col, cv.rng.randf_range(-0.05, 0.05)))
	cv.speckle(PixelCanvas.shade(col, -0.1), 10)


## Original glazed-terracotta motif: a rotationally symmetric pattern built from the dye colour.
static func glazed(cv: PixelCanvas, col: Color, seed_name: String) -> void:
	var a := PixelCanvas.shade(col, 0.35)
	var b := col
	var d := PixelCanvas.shade(col, -0.35)
	var w := Color(0.92, 0.9, 0.85)
	cv.reseed(seed_name)
	var kind := cv.rng.randi_range(0, 3)
	var q := PixelCanvas.new(8, 8, seed_name)
	for y in 8:
		for x in 8:
			var v := 0
			match kind:
				0: v = (x + y) % 5
				1: v = (x * y + x) % 4
				2: v = maxi(x, y) % 3 + (1 if x == y else 0)
				3: v = (absi(x - 3) + absi(y - 4)) % 4
			var c := b
			match v:
				0: c = a
				1: c = b
				2: c = d
				3: c = w
				4: c = PixelCanvas.shade(col, 0.12)
			q.px(x, y, c)
	# 4-fold rotational tiling
	for y in 8:
		for x in 8:
			var c := q.get_px(x, y)
			cv.px(x, y, c)
			cv.px(15 - y, x, c)
			cv.px(15 - x, 15 - y, c)
			cv.px(y, 15 - x, c)


static func glass(cv: PixelCanvas, frame: Color, fill_alpha := 0.0, fill: Color = Color(1, 1, 1)) -> void:
	cv.fill(Color(fill.r, fill.g, fill.b, fill_alpha))
	for i in 16:
		cv.px(i, 0, frame)
		cv.px(i, 15, frame)
		cv.px(0, i, frame)
		cv.px(15, i, frame)
	var hl := Color(frame.r, frame.g, frame.b, maxf(frame.a * 0.8, 0.5))
	for k in 3:
		cv.px(3 + k, 3 + k, hl)
	cv.px(10, 4, hl)
	cv.px(11, 3, hl)
	cv.px(4, 11, hl)


static func door(cv: PixelCanvas, pal: Array, top: bool, style: int) -> void:
	var pc := cols(pal)
	var frame := PixelCanvas.shade(pc[0], -0.25)
	planks_vertical(cv, pal)
	for i in 16:
		cv.px(0, i, frame)
		cv.px(15, i, frame)
	if top:
		cv.hline(0, 15, 0, frame)
		match style:
			0:
				for wy in [2, 8]:
					for wx in [3, 9]:
						cv.rect(wx, wy, 4, 4, Color(0, 0, 0, 0))
			1:
				cv.rect(3, 3, 10, 9, Color(0, 0, 0, 0))
				cv.vline(8, 3, 11, frame)
				cv.hline(3, 12, 7, frame)
			2:
				cv.rect_outline(3, 3, 10, 12, frame)
			3:
				for wy in [3, 7, 11]:
					cv.rect(4, wy, 8, 2, Color(0, 0, 0, 0))
	else:
		cv.hline(0, 15, 15, frame)
		cv.rect_outline(3, 2, 10, 11, frame)
		cv.rect_outline(5, 4, 6, 7, PixelCanvas.shade(pc[0], 0.12))
		cv.px(12, 1, C("#303030"))
		cv.px(12, 0, C("#303030"))


static func planks_vertical(cv: PixelCanvas, pal: Array) -> void:
	var pc := cols(pal)
	for y in 16:
		for x in 16:
			var c: Color = pc[0]
			var r := cv.rng.randf()
			if r < 0.2:
				c = pc[1]
			elif r < 0.32:
				c = pc[2]
			if x % 4 == 3:
				c = pc[3]
			cv.px(x, y, c)


static func trapdoor(cv: PixelCanvas, pal: Array, style: int) -> void:
	var pc := cols(pal)
	var frame := PixelCanvas.shade(pc[0], -0.25)
	planks(cv, pal)
	cv.rect_outline(0, 0, 16, 16, frame)
	match style:
		0:
			for yy in [3, 9]:
				for xx in [3, 9]:
					cv.rect(xx, yy, 4, 4, Color(0, 0, 0, 0))
		1:
			cv.rect(3, 3, 10, 10, Color(0, 0, 0, 0))
			cv.vline(8, 3, 12, frame)
			cv.hline(3, 12, 8, frame)
		2:
			cv.rect_outline(2, 2, 12, 12, frame)
		3:
			for yy in [2, 6, 10]:
				cv.rect(3, yy, 10, 2, Color(0, 0, 0, 0))


## Grayscale water/lava style animated frame.
static func water_frame(cv: PixelCanvas, frame: int, frames: int, flow: bool) -> void:
	var t := float(frame) / float(frames) * TAU
	for y in 16:
		for x in 16:
			var fy := y + (frame * 16.0 / frames if flow else 0.0)
			var v := sin(x * 0.7 + t) * 0.5 + sin(fy * 0.9 - t * (1.0 if flow else 1.3) + x * 0.3) * 0.5
			v += sin((x + fy) * 0.45 + t * 2.0) * 0.3
			var l := 0.62 + v * 0.07
			if v > 0.95:
				l += 0.1
			cv.px(x, y, Color(l, l, l * 1.02, 0.72))


static func lava_frame(cv: PixelCanvas, frame: int, frames: int, flow: bool) -> void:
	var t := float(frame) / float(frames) * TAU
	var hot := C("#ffd24a")
	var mid := C("#f28a17")
	var dark := C("#b63d0c")
	var crust := C("#8f2a08")
	for y in 16:
		for x in 16:
			var fy := y + (frame * 16.0 / frames if flow else 0.0)
			var v := sin(x * 0.55 + t) + sin(fy * 0.6 + t * 0.7 + x * 0.2) + sin((x - fy) * 0.35 - t)
			v = v / 3.0
			var c: Color
			if v > 0.45:
				c = hot
			elif v > 0.0:
				c = mid.lerp(hot, v / 0.45)
			elif v > -0.45:
				c = dark.lerp(mid, (v + 0.45) / 0.45)
			else:
				c = crust
			cv.px(x, y, c)


## Heat-simulation fire animation (cutout).
static func fire_frames(frames: int, pal: Array, seed_name: String) -> Array:
	var out := []
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(seed_name)
	var heat := PackedFloat32Array()
	heat.resize(16 * 20)
	var pc := cols(pal)
	for step in frames + 12:
		for x in 16:
			heat[19 * 16 + x] = rng.randf_range(0.6, 1.0) if rng.randf() < 0.8 else 0.1
		var nh := heat.duplicate()
		for y in range(0, 19):
			for x in 16:
				var s := heat[(y + 1) * 16 + x] * 2.0 + heat[(y + 1) * 16 + posmod(x - 1, 16)] + heat[(y + 1) * 16 + posmod(x + 1, 16)]
				s += heat[mini(y + 2, 19) * 16 + x]
				nh[y * 16 + x] = maxf(0.0, s / 5.0 - rng.randf_range(0.02, 0.07))
		heat = nh
		if step >= 12:
			var cv := PixelCanvas.new(16, 16, seed_name)
			for y in 16:
				for x in 16:
					var hval := heat[(y + 4) * 16 + x]
					if hval < 0.18:
						continue
					var c: Color
					if hval > 0.75:
						c = pc[0]
					elif hval > 0.5:
						c = pc[1]
					elif hval > 0.3:
						c = pc[2]
					else:
						c = pc[3]
					cv.px(x, y, c)
			out.append(cv.img)
	return out


static func portal_frame(cv: PixelCanvas, frame: int, frames: int) -> void:
	var t := float(frame) / float(frames) * TAU
	for y in 16:
		for x in 16:
			var dx := x - 7.5
			var dy := y - 7.5
			var ang := atan2(dy, dx)
			var r := sqrt(dx * dx + dy * dy)
			var v := sin(ang * 3.0 + r * 0.9 - t * 2.0) * 0.5 + sin(r * 1.3 - t) * 0.5
			var c := C("#6a1fc9").lerp(C("#c77dff"), clampf(v * 0.5 + 0.5, 0.0, 1.0))
			if v > 0.8:
				c = C("#e9c2ff")
			c.a = 0.78
			cv.px(x, y, c)


## Plant stem + leaves helper for flower sprites.
static func stem(cv: PixelCanvas, x: int, y0: int, y1: int, col := Color("#3f7a25")) -> void:
	for y in range(y0, y1 + 1):
		cv.px(x, y, col)


static func leaf(cv: PixelCanvas, x: int, y: int, dir: int, col := Color("#4c8e2d")) -> void:
	cv.px(x + dir, y, col)
	cv.px(x + dir * 2, y - 1, col)
	cv.px(x + dir, y - 1, PixelCanvas.shade(col, 0.15))
