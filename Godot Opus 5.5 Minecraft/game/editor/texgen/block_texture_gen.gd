class_name BlockTextureGen
extends RefCounted
## Dispatches texture names to the procedural painters and packs every block texture into an atlas
## (PNG + JSON index). Animated textures occupy consecutive layers.

const ANIMATED := {
	"water_still": 16, "water_flow": 16, "lava_still": 16, "lava_flow": 16, "nether_portal": 16,
	"fire": 16, "soul_fire": 16, "campfire_fire": 8, "soul_campfire_fire": 8,
}
const ALWAYS := ["missing", "destroy_stage_0", "destroy_stage_1", "destroy_stage_2", "destroy_stage_3",
	"destroy_stage_4", "destroy_stage_5", "destroy_stage_6", "destroy_stage_7", "destroy_stage_8", "destroy_stage_9",
	"bamboo_leaves"]

const ATLAS_PNG := "res://game/generated/textures/blocks_atlas.png"
const ATLAS_JSON := "res://game/generated/textures/blocks_atlas.json"
const COLUMNS := 32


static func generate(n: String) -> Array:
	if ANIMATED.has(n):
		return _animated(n, ANIMATED[n])
	var r: Array = BTexTerrain.gen(n)
	if r.is_empty():
		r = BTexWoodPlants.gen(n)
	if r.is_empty():
		r = BTexBuilt.gen(n)
	return r


static func _animated(n: String, frames: int) -> Array:
	var out := []
	match n:
		"fire":
			return BTCommon.fire_frames(frames, ["#fff3b0", "#ffc83a", "#f7811c", "#c9420f"], n)
		"soul_fire":
			return BTCommon.fire_frames(frames, ["#e0ffff", "#7ff0f5", "#2fc2d0", "#1a7a8a"], n)
		"campfire_fire":
			return BTCommon.fire_frames(frames, ["#fff3b0", "#ffc83a", "#f7811c", "#c9420f"], n)
		"soul_campfire_fire":
			return BTCommon.fire_frames(frames, ["#e0ffff", "#7ff0f5", "#2fc2d0", "#1a7a8a"], n)
	for f in frames:
		var cv := PixelCanvas.new(16, 16, n + str(f))
		match n:
			"water_still":
				BTCommon.water_frame(cv, f, frames, false)
			"water_flow":
				BTCommon.water_frame(cv, f, frames, true)
			"lava_still":
				BTCommon.lava_frame(cv, f, frames, false)
			"lava_flow":
				BTCommon.lava_frame(cv, f, frames, true)
			"nether_portal":
				BTCommon.portal_frame(cv, f, frames)
		out.append(cv.img)
	return out


## Builds the full atlas. Returns {image, index, missing}.
static func build_atlas(names: PackedStringArray) -> Dictionary:
	var all := PackedStringArray()
	for a in ALWAYS:
		if not all.has(a):
			all.append(a)
	for n in names:
		if not all.has(n):
			all.append(n)
	for n in ANIMATED:
		if not all.has(n):
			all.append(n)
	var tiles := []
	var index := {}
	var missing := PackedStringArray()
	for n in all:
		var frames: Array = generate(n)
		if frames.is_empty():
			missing.append(n)
			continue
		index[n] = [tiles.size(), frames.size()]
		for fimg in frames:
			var im: Image = fimg
			if im.get_width() != 16 or im.get_height() != 16:
				im = im.duplicate()
				im.resize(16, 16, Image.INTERPOLATE_NEAREST)
			tiles.append(im)
	var rows := int(ceil(tiles.size() / float(COLUMNS)))
	var atlas := Image.create(COLUMNS * 16, rows * 16, false, Image.FORMAT_RGBA8)
	atlas.fill(Color(0, 0, 0, 0))
	for i in tiles.size():
		var im2: Image = tiles[i]
		if im2.get_format() != Image.FORMAT_RGBA8:
			im2.convert(Image.FORMAT_RGBA8)
		atlas.blit_rect(im2, Rect2i(0, 0, 16, 16), Vector2i((i % COLUMNS) * 16, (i / COLUMNS) * 16))
	return {"image": atlas, "index": index, "missing": missing, "count": tiles.size()}


static func save_atlas(result: Dictionary) -> Error:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://game/generated/textures"))
	var img: Image = result.image
	var err := img.save_png(ATLAS_PNG)
	if err != OK:
		return err
	var meta := {"tile": 16, "columns": COLUMNS, "layers": result.count, "textures": result.index}
	var f := FileAccess.open(ATLAS_JSON, FileAccess.WRITE)
	f.store_string(JSON.stringify(meta, "\t", true))
	f.close()
	return OK
