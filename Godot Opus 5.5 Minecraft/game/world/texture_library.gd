class_name TextureLibrary
extends RefCounted
## Loads the generated block atlas (PNG + JSON index) and builds the Texture2DArray used by the voxel
## shaders. Falls back to generating the atlas in memory when the generated files are missing.

const ATLAS_PNG := "res://game/generated/textures/blocks_atlas.png"
const ATLAS_JSON := "res://game/generated/textures/blocks_atlas.json"

static var block_array: Texture2DArray
static var layers: Dictionary = {}          # texture name -> first layer
static var frames: Dictionary = {}          # first layer -> frame count
static var tiles: Array = []                # Image per layer (no mipmaps) - used for icons/particles/items
static var loaded := false


static func load_blocks() -> void:
	if loaded:
		return
	var atlas: Image = null
	var index: Dictionary = {}
	if ResourceLoader.exists(ATLAS_PNG) and FileAccess.file_exists(ATLAS_JSON):
		var tex := load(ATLAS_PNG) as Texture2D
		if tex != null:
			atlas = tex.get_image()
		var meta = JSON.parse_string(FileAccess.get_file_as_string(ATLAS_JSON))
		if meta is Dictionary:
			index = meta.get("textures", {})
	if atlas == null or index.is_empty():
		push_warning("TextureLibrary: generated atlas missing, generating in memory")
		var res := BlockTextureGen.build_atlas(BlockDB.texture_names)
		atlas = res.image
		index = res.index
	if atlas.is_compressed():
		atlas.decompress()
	if atlas.get_format() != Image.FORMAT_RGBA8:
		atlas.convert(Image.FORMAT_RGBA8)
	var cols := atlas.get_width() / 16
	var count := 0
	for n in index:
		var e: Array = index[n]
		layers[n] = int(e[0])
		frames[int(e[0])] = int(e[1])
		count = maxi(count, int(e[0]) + int(e[1]))
	tiles.clear()
	var imgs: Array[Image] = []
	for i in count:
		var tile := atlas.get_region(Rect2i((i % cols) * 16, (i / cols) * 16, 16, 16))
		tiles.append(tile)
		imgs.append(with_pixel_mips(tile))
	block_array = Texture2DArray.new()
	block_array.create_from_images(imgs)
	BlockDB.resolve_textures(layers, frames)
	loaded = true


static func tile(tname: String) -> Image:
	var l: int = layers.get(tname, layers.get("missing", 0))
	return tiles[l] if l < tiles.size() else null


static func tile_layer(l: int) -> Image:
	return tiles[l] if l >= 0 and l < tiles.size() else null


## Builds an RGBA8 image with custom mip levels: colour = mean of opaque texels, alpha = coverage >= 50%.
## Keeps pixel-art crisp at distance and prevents cut-out foliage from dissolving.
static func with_pixel_mips(src: Image) -> Image:
	var size := src.get_width()
	var data := PackedByteArray()
	var cur := src.get_data()
	data.append_array(cur)
	var s := size
	while s > 1:
		var ns := s >> 1
		var nxt := PackedByteArray()
		nxt.resize(ns * ns * 4)
		for y in ns:
			for x in ns:
				var r := 0
				var g := 0
				var b := 0
				var opaque := 0
				var ra := 0
				var ga := 0
				var ba := 0
				var asum := 0
				for dy in 2:
					for dx in 2:
						var o := ((y * 2 + dy) * s + (x * 2 + dx)) * 4
						var a := cur[o + 3]
						ra += cur[o]
						ga += cur[o + 1]
						ba += cur[o + 2]
						asum += a
						if a > 127:
							opaque += 1
							r += cur[o]
							g += cur[o + 1]
							b += cur[o + 2]
				var no := (y * ns + x) * 4
				if opaque > 0:
					nxt[no] = r / opaque
					nxt[no + 1] = g / opaque
					nxt[no + 2] = b / opaque
				else:
					nxt[no] = ra >> 2
					nxt[no + 1] = ga >> 2
					nxt[no + 2] = ba >> 2
				if opaque == 4 or opaque == 0:
					nxt[no + 3] = asum >> 2
				elif opaque >= 2:
					nxt[no + 3] = maxi(asum >> 2, 200)
				else:
					nxt[no + 3] = mini(asum >> 2, 90)
		data.append_array(nxt)
		cur = nxt
		s = ns
	return Image.create_from_data(size, size, true, Image.FORMAT_RGBA8, data)
