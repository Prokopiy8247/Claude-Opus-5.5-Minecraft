class_name ItemIcons
extends RefCounted
## Item icon + item model factory.
##  * 2D icons for the UI: baked 3D block renders (IconBaker), block texture layers, or 16x16
##    sprites rasterised from ItemSpriteData templates (tinted for potions, eggs, dyed leather).
##  * 3D item models: block meshes, or pixel-extruded sprite meshes (dropped items / held items).

static var _icons: Dictionary = {}          # item id -> Texture2D
static var _sprite_img: Dictionary = {}     # key -> Image (16x16)
static var _extruded: Dictionary = {}       # key -> ArrayMesh
static var _mats: Dictionary = {}
static var block_icons: Dictionary = {}     # block name -> Image (baked)
static var atlas_img: Image = null          # block texture atlas image (for layer extraction)
static var atlas_cols := 32


static func _atlas() -> Image:
	if atlas_img == null:
		var tex := load(BlockTextureGen.ATLAS_PNG) as Texture2D
		if tex != null:
			atlas_img = tex.get_image()
			if atlas_img.is_compressed():
				atlas_img.decompress()
			atlas_img.convert(Image.FORMAT_RGBA8)
	return atlas_img


## 16x16 image of a block texture layer (first animation frame).
static func layer_image(layer: int) -> Image:
	var key := "layer:%d" % layer
	if _sprite_img.has(key):
		return _sprite_img[key]
	var a := _atlas()
	var img := Image.create_empty(16, 16, false, Image.FORMAT_RGBA8)
	if a != null:
		var cols := a.get_width() / 16
		var x := (layer % cols) * 16
		var y := (layer / cols) * 16
		if y + 16 <= a.get_height():
			img.blit_rect(a, Rect2i(x, y, 16, 16), Vector2i.ZERO)
	_sprite_img[key] = img
	return img


static func block_texture_image(tex_name: String) -> Image:
	return layer_image(BlockDB.layer_of(tex_name))


# ------------------------------------------------------------------------------------------------
## UI icon for an item stack (colour-aware: potions, dyed leather, spawn eggs).
static func icon_for_stack(st: ItemStack) -> Texture2D:
	if st == null:
		return null
	if st.data.has("potion") or st.data.has("color"):
		var key := "%d|%s|%s" % [st.id, String(st.data.get("potion", "")), String(st.data.get("color", ""))]
		if _icons.has(key):
			return _icons[key]
		var img := icon_image(st.item(), st.data)
		var t := ImageTexture.create_from_image(img)
		_icons[key] = t
		return t
	return icon(st.id)


static func icon(item_id: int) -> Texture2D:
	if _icons.has(item_id):
		return _icons[item_id]
	var it := ItemDB.def(item_id)
	if it == null:
		return null
	var img := icon_image(it, {})
	var t := ImageTexture.create_from_image(img)
	_icons[item_id] = t
	return t


static func invalidate() -> void:
	_icons.clear()


static func icon_image(it: ItemDef, data: Dictionary) -> Image:
	var ic := it.icon
	if ic.begins_with("block:") or ic.begins_with("model:"):
		var bname := ic.substr(ic.find(":") + 1)
		if block_icons.has(bname):
			return block_icons[bname]
		var bd: BlockDef = BlockDB.defs[BlockDB.id(bname)]
		return layer_image(BlockDB.face_tex[(bd.id * 16) * 6 + Vox.SOUTH] & 0xFFFF)
	if ic.begins_with("blocktex:"):
		var tname := ic.substr(9)
		var img := block_texture_image(tname).duplicate()
		var bid := BlockDB.id(it.block)
		var bd2: BlockDef = BlockDB.defs[bid]
		if bd2.tint != 0 and bd2.tint != BlockDB.T_REDSTONE:
			_tint(img, _default_tint(bd2.tint))
		return img
	if ic.begins_with("egg:"):
		var mob := ic.substr(4)
		var cols: Array = MobDB.EGG_COLORS.get(mob, ["#888888", "#444444"])
		return sprite_image("spawn_egg", _ramp(Color.html(cols[0])), _ramp(Color.html(cols[1])))
	var sname := ic.substr(ic.find(":") + 1) if ic.contains(":") else it.name
	if not ItemSpriteData.has_item(sname):
		sname = it.name
	var img2 := item_sprite(sname)
	if data.has("potion") and it.name in ["potion", "splash_potion", "lingering_potion", "tipped_arrow"]:
		var pc := EffectDB.potion_color(String(data["potion"]))
		var e: Array = ItemSpriteData.ITEMS.get(sname, ["potion", [], []])
		img2 = sprite_image(String(e[0]), _ramp_from_list(e[1]), _ramp(pc))
	if data.has("color") and it.material == "leather":
		img2 = img2.duplicate()
		_tint(img2, Color.html(String(data["color"])))
	return img2


static func _default_tint(kind: int) -> Color:
	match kind:
		BlockDB.T_GRASS, BlockDB.T_GRASS_MARK:
			return BiomeDB.grass[BiomeDB.PLAINS] if BiomeDB.inited else Color(0.49, 0.74, 0.35)
		BlockDB.T_FOLIAGE:
			return BiomeDB.foliage[BiomeDB.PLAINS] if BiomeDB.inited else Color(0.47, 0.67, 0.18)
		BlockDB.T_WATER:
			return Color(0.25, 0.46, 0.89)
		BlockDB.T_SPRUCE:
			return Color(0.38, 0.6, 0.38)
		BlockDB.T_BIRCH:
			return Color(0.5, 0.65, 0.33)
		BlockDB.T_LILY:
			return Color(0.13, 0.5, 0.19)
	return Color(0.49, 0.74, 0.35)


static func _tint(img: Image, c: Color) -> void:
	for y in img.get_height():
		for x in img.get_width():
			var p := img.get_pixel(x, y)
			if p.a > 0.0:
				img.set_pixel(x, y, Color(p.r * c.r, p.g * c.g, p.b * c.b, p.a))


static func _ramp(c: Color) -> Array:
	return [c.darkened(0.55), c.darkened(0.3), c, c.lightened(0.25), c.lightened(0.5)]


static func _ramp_from_list(lst: Array) -> Array:
	var out := []
	for s in lst:
		out.append(Color.html(String(s)))
	return out


static func item_sprite(sname: String) -> Image:
	var key := "item:" + sname
	if _sprite_img.has(key):
		return _sprite_img[key]
	var img: Image
	if ItemSpriteData.has_item(sname):
		var e: Array = ItemSpriteData.ITEMS[sname]
		img = sprite_image(String(e[0]), _ramp_from_list(e[1]), _ramp_from_list(e[2]))
	else:
		img = _fallback_sprite(sname)
	_sprite_img[key] = img
	return img


static func sprite_image(template: String, primary: Array, secondary: Array) -> Image:
	var img := Image.create_empty(16, 16, false, Image.FORMAT_RGBA8)
	var rows: Array = ItemSpriteData.TEMPLATES.get(template, [])
	if rows.is_empty():
		return _fallback_sprite(template)
	var hd := Color.html(ItemSpriteData.HANDLE_DARK)
	var hl := Color.html(ItemSpriteData.HANDLE_LIGHT)
	for y in mini(16, rows.size()):
		var row: String = rows[y]
		for x in mini(16, row.length()):
			var ch := row.substr(x, 1)
			var c := Color(0, 0, 0, 0)
			match ch:
				".":
					continue
				"1", "2", "3", "4", "5":
					var i := int(ch) - 1
					c = primary[i] if i < primary.size() else Color.MAGENTA
				"a", "b", "c", "d", "e":
					var j := ch.unicode_at(0) - 97
					if secondary.is_empty():
						c = primary[j] if j < primary.size() else Color.MAGENTA
					else:
						c = secondary[j] if j < secondary.size() else Color.MAGENTA
				"h":
					c = hd
				"H":
					c = hl
				"k":
					c = Color(0.1, 0.1, 0.1)
				"w":
					c = Color(0.97, 0.97, 0.97)
				_:
					continue
			img.set_pixel(x, y, c)
	return img


## Generic sprite when no template exists: a shaded gem-like blob in a colour derived from the name.
static func _fallback_sprite(sname: String) -> Image:
	var img := Image.create_empty(16, 16, false, Image.FORMAT_RGBA8)
	var h := absi(hash(sname))
	var base := Color.from_hsv(float(h % 360) / 360.0, 0.55, 0.85)
	for y in range(3, 13):
		for x in range(3, 13):
			var dx := x - 7.5
			var dy := y - 7.5
			if dx * dx + dy * dy < 22.0:
				var sh := 1.1 - (dx + dy) * 0.04
				img.set_pixel(x, y, Color(base.r * sh, base.g * sh, base.b * sh))
	return img


# ------------------------------------------------------------------------------------------------
# 3D item geometry
static func _sprite_for_item(it: ItemDef) -> Image:
	var ic := it.icon
	if ic.begins_with("blocktex:"):
		var img := block_texture_image(ic.substr(9)).duplicate()
		var bd: BlockDef = BlockDB.defs[BlockDB.id(it.block)]
		if bd.tint != 0 and bd.tint != BlockDB.T_REDSTONE:
			_tint(img, _default_tint(bd.tint))
		return img
	return icon_image(it, {})


## Extruded pixel mesh (1/16 thick) centred on the origin, 1 unit wide.
static func extruded_mesh(img: Image, key: String) -> ArrayMesh:
	if _extruded.has(key):
		return _extruded[key]
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var px := 1.0 / 16.0
	var th := px * 0.5
	var w := img.get_width()
	var h := img.get_height()
	# front and back full quads (alpha cut in the shader)
	_quad(st, Vector3(-0.5, -0.5, th), Vector3(0.5, -0.5, th), Vector3(0.5, 0.5, th), Vector3(-0.5, 0.5, th),
		Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 0), Vector3(0, 0, 1))
	_quad(st, Vector3(0.5, -0.5, -th), Vector3(-0.5, -0.5, -th), Vector3(-0.5, 0.5, -th), Vector3(0.5, 0.5, -th),
		Vector2(1, 1), Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector3(0, 0, -1))
	# edge faces between opaque and transparent pixels
	for y in h:
		for x in w:
			if img.get_pixel(x, y).a < 0.5:
				continue
			var x0 := -0.5 + x * px
			var x1 := x0 + px
			var y1 := 0.5 - y * px
			var y0 := y1 - px
			var uv := Vector2((x + 0.5) / w, (y + 0.5) / h)
			if x == 0 or img.get_pixel(x - 1, y).a < 0.5:
				_quad(st, Vector3(x0, y0, -th), Vector3(x0, y0, th), Vector3(x0, y1, th), Vector3(x0, y1, -th), uv, uv, uv, uv, Vector3(-1, 0, 0))
			if x == w - 1 or img.get_pixel(x + 1, y).a < 0.5:
				_quad(st, Vector3(x1, y0, th), Vector3(x1, y0, -th), Vector3(x1, y1, -th), Vector3(x1, y1, th), uv, uv, uv, uv, Vector3(1, 0, 0))
			if y == 0 or img.get_pixel(x, y - 1).a < 0.5:
				_quad(st, Vector3(x0, y1, th), Vector3(x1, y1, th), Vector3(x1, y1, -th), Vector3(x0, y1, -th), uv, uv, uv, uv, Vector3(0, 1, 0))
			if y == h - 1 or img.get_pixel(x, y + 1).a < 0.5:
				_quad(st, Vector3(x0, y0, -th), Vector3(x1, y0, -th), Vector3(x1, y0, th), Vector3(x0, y0, th), uv, uv, uv, uv, Vector3(0, -1, 0))
	var mesh := st.commit()
	_extruded[key] = mesh
	return mesh


static func _quad(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3, d: Vector3, ua: Vector2, ub: Vector2, uc: Vector2, ud: Vector2, n: Vector3) -> void:
	# a b c d counter-clockwise seen from outside -> emit clockwise triangles for Godot
	var shade := 1.0
	if n.x != 0.0:
		shade = 0.7
	elif n.y > 0.0:
		shade = 0.95
	elif n.y < 0.0:
		shade = 0.6
	elif n.z < 0.0:
		shade = 0.8
	var col := Color(shade, shade, shade)
	for pr in [[a, ua], [c, uc], [b, ub], [a, ua], [d, ud], [c, uc]]:
		st.set_color(col)
		st.set_normal(n)
		st.set_uv(pr[1])
		st.add_vertex(pr[0])


static func item_material(img: Image, key: String, hand := false) -> ShaderMaterial:
	var mk := key + ("|hand" if hand else "")
	if _mats.has(mk):
		return _mats[mk]
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/hand_item.gdshader" if hand else "res://game/world/shaders/item.gdshader")
	var tex := ImageTexture.create_from_image(img)
	m.set_shader_parameter("albedo_tex", tex)
	_mats[mk] = m
	return m


## Mesh node for a dropped / displayed flat item.
static func sprite_mesh(it: ItemDef, hand := false) -> Node3D:
	var img := _sprite_for_item(it)
	var mi := MeshInstance3D.new()
	mi.mesh = extruded_mesh(img, "it:" + it.name)
	mi.material_override = item_material(img, "it:" + it.name, hand)
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	if not hand:
		mi.scale = Vector3(0.5, 0.5, 0.5)
	return mi


static func orb_mesh(amount: int) -> Mesh:
	var q := QuadMesh.new()
	q.size = Vector2(0.25, 0.25) * clampf(0.6 + log(float(amount) + 1.0) * 0.25, 0.6, 1.6)
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/orb.gdshader")
	q.material = m
	return q


static func cube_mesh(c: Color, size: float) -> Node3D:
	var mi := MeshInstance3D.new()
	var b := BoxMesh.new()
	b.size = Vector3.ONE * size
	mi.mesh = b
	var mat := StandardMaterial3D.new()
	mat.albedo_color = c
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mi.material_override = mat
	return mi


static func fireball_mesh(size: float, small: bool) -> Node3D:
	var it := ItemDB.get_by_name("fire_charge")
	if it == null:
		return cube_mesh(Color(1, 0.5, 0.1), size)
	var img := _sprite_for_item(it)
	var q := MeshInstance3D.new()
	var qm := QuadMesh.new()
	qm.size = Vector2(size, size) * (1.0 if small else 1.4)
	q.mesh = qm
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/billboard_sprite.gdshader")
	m.set_shader_parameter("albedo_tex", ImageTexture.create_from_image(img))
	q.material_override = m
	return q


static func arrow_mesh(spectral: bool) -> Node3D:
	var root := Node3D.new()
	var it := ItemDB.get_by_name("spectral_arrow" if spectral else "arrow")
	if it == null:
		return cube_mesh(Color(0.6, 0.5, 0.3), 0.1)
	var n := sprite_mesh(it)
	n.scale = Vector3(0.6, 0.6, 0.6)
	# the arrow sprite points up-right: rotate so it points along -Z (look_at forward)
	n.rotation = Vector3(-PI * 0.5, 0, PI * 0.25)
	root.add_child(n)
	var n2 := sprite_mesh(it)
	n2.scale = Vector3(0.6, 0.6, 0.6)
	n2.rotation = Vector3(-PI * 0.5, PI * 0.5, PI * 0.25)
	root.add_child(n2)
	return root


## Model used for an item held in first person / by mobs.
static func held_model(it: ItemDef, hand := true) -> Node3D:
	if it == null:
		return null
	if it.block != "" and not it.icon.begins_with("blocktex:") and not it.icon.begins_with("sprite:") and not it.icon.begins_with("item:"):
		var v := Vox.make(BlockDB.id(it.block), 0)
		var mi := MeshInstance3D.new()
		mi.mesh = BlockMesh.build(v, Color(-1, 0, 0), true)
		if hand:
			var cnt := mi.mesh.get_surface_count()
			for i in cnt:
				mi.set_surface_override_material(i, _hand_block_material(i, mi.mesh))
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		return mi
	var glb := ModelLibrary.item_model(it.name)
	if glb != null:
		ModelLibrary.apply_hand_materials(glb, hand)
		return glb
	return sprite_mesh(it, hand)


static var _hand_block_mats := {}

static func _hand_block_material(surface: int, mesh: Mesh) -> ShaderMaterial:
	var base := mesh.surface_get_material(surface) as ShaderMaterial
	var cut := 0.5
	if base != null:
		cut = float(base.get_shader_parameter("alpha_cut"))
	var key := "hb%.2f" % cut
	if _hand_block_mats.has(key):
		return _hand_block_mats[key]
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/hand_block.gdshader")
	m.set_shader_parameter("blocks", TextureLibrary.block_array)
	m.set_shader_parameter("alpha_cut", cut)
	_hand_block_mats[key] = m
	return m
