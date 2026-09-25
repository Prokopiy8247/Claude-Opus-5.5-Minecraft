class_name BlockInteract
extends RefCounted
## Right-click behaviour of interactive blocks (containers and stations open screens, doors
## toggle, levers/buttons drive redstone, beds sleep or explode, cakes are eaten, ...).

static var _rng := RandomNumberGenerator.new()

const NOTE_INSTR := {"stone": "basedrum", "sand": "snare", "glass": "hat", "wood": "bass", "gold_block": "bell", "clay": "flute",
	"packed_ice": "chime", "white_wool": "guitar", "bone_block": "xylophone", "iron_block": "iron_xylophone",
	"soul_sand": "cow_bell", "pumpkin": "didgeridoo", "emerald_block": "bit", "hay_block": "banjo", "glowstone": "pling"}


static func interact(world: World, pos: Vector3i, v: int, player, st: ItemStack, hit: Dictionary) -> bool:
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	var meta := (v >> 12) & 15
	var session = player.session
	var held := st.item_name() if st != null else ""
	match d.interact:
		"crafting_table":
			session.ui.open_screen("crafting", {"pos": pos})
			return true
		"furnace", "blast_furnace", "smoker":
			_ensure_container(world, pos, 3)
			session.ui.open_screen("furnace", {"pos": pos, "kind": d.interact})
			return true
		"chest":
			if d.props.get("ender", false):
				session.ui.open_screen("container", {"ender": true, "pos": pos, "title": "Ender Chest"})
				return true
			var above := world.get_blockv(pos + Vector3i(0, 1, 0))
			if BlockDB.full[above & 0xFFF] == 1 and not BlockDB.defs[above & 0xFFF].name.ends_with("_leaves"):
				return true
			var positions := [pos]
			var half := (meta >> 2) & 3
			if half != 0:
				var fvec: Vector3i = Vox.H_FACING_VEC[meta & 3]
				var right := Vector3i(-fvec.z, 0, fvec.x)
				var other: Vector3i = pos + (right if half == 1 else -right)
				if (world.get_blockv(other) & 0xFFF) == (v & 0xFFF):
					positions = [pos, other] if half == 2 else [other, pos]
			for p in positions:
				_ensure_container(world, p, 27)
			var title := "Large Chest" if positions.size() > 1 else BlockCatalog.pretty(d.name)
			session.ui.open_screen("container", {"positions": positions, "title": title, "pos": pos})
			session.chest_opened(world, positions, true)
			return true
		"barrel", "shulker_box", "dispenser", "dropper", "hopper":
			var size: int = d.props.get("container", 27)
			_ensure_container(world, pos, size)
			var screen := "container"
			if d.interact == "dispenser" or d.interact == "dropper":
				screen = "dispenser"
			elif d.interact == "hopper":
				screen = "hopper"
			session.ui.open_screen(screen, {"positions": [pos], "title": BlockCatalog.pretty(d.name), "pos": pos})
			if d.interact == "barrel":
				world.set_blockv(pos, Vox.make(v & 0xFFF, meta | 8), World.F_URGENT)
			Sfx.play_at("chest_open" if d.interact != "shulker_box" else "shulker_open", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.6)
			return true
		"brewing_stand":
			_ensure_container(world, pos, 5)
			session.ui.open_screen("brewing", {"pos": pos})
			return true
		"enchanting_table":
			session.ui.open_screen("enchanting", {"pos": pos})
			return true
		"anvil":
			session.ui.open_screen("anvil", {"pos": pos})
			return true
		"grindstone":
			session.ui.open_screen("grindstone", {"pos": pos})
			return true
		"stonecutter":
			session.ui.open_screen("stonecutter", {"pos": pos})
			return true
		"smithing_table":
			session.ui.open_screen("smithing", {"pos": pos})
			return true
		"beacon":
			session.ui.open_screen("beacon", {"pos": pos})
			return true
		"lectern":
			return false
		"door":
			if d.props.get("iron", false):
				return false
			_toggle_door(world, pos, v)
			return true
		"trapdoor", "gate":
			if d.props.get("iron", false):
				return false
			var nm := meta ^ 4
			if d.interact == "gate" and (nm & 4) != 0:
				# open away from the player
				var lf := Vox.facing_from_yaw(player.yaw)
				nm = (nm & ~3) | ((lf + 2) & 3 if false else (meta & 3))
			world.set_blockv(pos, Vox.make(v & 0xFFF, nm), World.F_URGENT | World.F_NOTIFY)
			Sfx.play_at(("trapdoor_" if d.interact == "trapdoor" else "fence_gate_") + ("open" if (nm & 4) != 0 else "close"),
				Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.7)
			return true
		"lever":
			RedstoneSystem.toggle_lever(world, pos)
			return true
		"button":
			RedstoneSystem.press_button(world, pos)
			return true
		"repeater":
			var delay := ((meta >> 2) + 1) & 3
			world.set_blockv(pos, Vox.make(v & 0xFFF, (meta & 3) | (delay << 2)), World.F_URGENT)
			Sfx.play_at("click", Vector3(pos) + Vector3(0.5, 0.2, 0.5), 0.4)
			return true
		"comparator":
			world.set_blockv(pos, Vox.make(v & 0xFFF, meta ^ 4), World.F_URGENT)
			world.schedule_tick(pos.x, pos.y, pos.z, 2)
			Sfx.play_at("click", Vector3(pos) + Vector3(0.5, 0.2, 0.5), 0.4, 0.55 if (meta & 4) == 0 else 0.5)
			return true
		"daylight_detector":
			world.set_blockv(pos, Vox.make(v & 0xFFF, meta ^ 1), World.F_URGENT)
			RedstoneSystem.update_daylight(world, pos, world.get_blockv(pos))
			return true
		"note_block":
			var pitch := (int(world.get_be(pos.x, pos.y, pos.z, true).get("note", 0)) + 1) % 25
			world.get_be(pos.x, pos.y, pos.z, true)["note"] = pitch
			world.mark_modified(pos.x, pos.z)
			play_note(world, pos)
			return true
		"tnt":
			if held == "flint_and_steel" or held == "fire_charge":
				world.set_blockv(pos, 0)
				Explosions.prime_tnt(world, Vector3(pos), 80, player)
				if held == "flint_and_steel":
					player.interact.damage_item(player.inventory.selected, 1)
				else:
					ItemUse.spend(player, player.inventory.selected, st)
				return true
			return false
		"bed":
			return _bed(world, pos, v, player)
		"respawn_anchor":
			return _respawn_anchor(world, pos, v, player, st)
		"cake":
			if player.stats.food >= 20 and not player.is_creative():
				return false
			player.stats.eat(2, 0.4)
			Sfx.play_at("eat", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.6)
			var bites := (meta & 7) + 1
			if bites >= 7:
				world.set_blockv(pos, 0)
			else:
				world.set_blockv(pos, Vox.make(v & 0xFFF, bites), World.F_URGENT)
			return true
		"berry_bush":
			if meta >= 2:
				session.entities.spawn_item(world, Vector3(pos) + Vector3(0.5, 0.5, 0.5), ItemStack.of("sweet_berries", _rng.randi_range(1, 2) + (1 if meta == 3 else 0)))
				world.set_blockv(pos, Vox.make(v & 0xFFF, 1), World.F_URGENT)
				Sfx.play_at("berry_pick", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.6)
				return true
			return false
		"cave_vines":
			if (meta & 1) == 1:
				session.entities.spawn_item(world, Vector3(pos) + Vector3(0.5, 0.5, 0.5), ItemStack.of("glow_berries", 1))
				world.set_blockv(pos, Vox.make(v & 0xFFF, meta & ~1), World.F_URGENT)
				return true
			return false
		"campfire":
			if held.begins_with("raw") or RecipeDB.smelt_result(held, "campfire").size() > 0:
				var be := world.get_be(pos.x, pos.y, pos.z, true)
				var items: Array = be.get("food", [])
				if items.size() < 4:
					items.append({"item": held, "t": 0})
					be["food"] = items
					be["type"] = "campfire"
					session.register_ticking_be(world, pos)
					ItemUse.spend(player, player.inventory.selected, st)
					world.mark_modified(pos.x, pos.z)
					return true
			return false
		"candle":
			if held == "flint_and_steel" and (meta & 4) == 0:
				world.set_blockv(pos, Vox.make(v & 0xFFF, meta | 4), World.F_URGENT)
				return true
			if (meta & 4) != 0 and held == "":
				world.set_blockv(pos, Vox.make(v & 0xFFF, meta & ~4), World.F_URGENT)
				Sfx.play_at("extinguish", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.4)
				return true
			return false
		"cauldron":
			return _cauldron(world, pos, v, player, st)
		"composter":
			return _composter(world, pos, v, player, st)
		"jukebox":
			return _jukebox(world, pos, player, st)
		"flower_pot":
			return _flower_pot(world, pos, v, player, st)
		"end_frame":
			if held == "ender_eye" and (meta & 4) == 0:
				world.set_blockv(pos, Vox.make(v & 0xFFF, meta | 4), World.F_URGENT)
				ItemUse.spend(player, player.inventory.selected, st)
				Sfx.play_at("end_portal_frame_fill", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.9)
				session.particles.burst(Vector3(pos) + Vector3(0.5, 1.0, 0.5), Color(0.4, 0.9, 0.7), 12, 1.5, "portal", 0.12)
				Portals.check_end_portal(world, pos)
				return true
			return false
		"dragon_egg":
			for i in 64:
				var q := pos + Vector3i(_rng.randi_range(-15, 15), _rng.randi_range(-7, 7), _rng.randi_range(-15, 15))
				if world.get_blockv(q) == 0 and BlockDB.solid[world.get_id(q.x, q.y - 1, q.z)] == 1:
					world.set_blockv(pos, 0)
					world.set_blockv(q, v)
					session.particles.portal(Vector3(pos) + Vector3(0.5, 0.5, 0.5))
					return true
			return true
		"pumpkin":
			if held == "shears":
				world.set_blockv(pos, Vox.make(BlockDB.id("carved_pumpkin"), (Vox.facing_from_yaw(player.yaw) + 2) & 3))
				session.entities.spawn_item(world, Vector3(pos) + Vector3(0.5, 0.8, 0.5), ItemStack.of("pumpkin_seeds", 4))
				player.interact.damage_item(player.inventory.selected, 1)
				return true
			return false
		"spawner":
			if st != null and st.item().kind == "spawn_egg":
				var sbe := world.get_be(pos.x, pos.y, pos.z, true)
				sbe["mob"] = String(st.item().props["mob"])
				sbe["type"] = "spawner"
				session.register_ticking_be(world, pos)
				world.mark_modified(pos.x, pos.z)
				ItemUse.spend(player, player.inventory.selected, st)
				return true
			return false
		"vault":
			if held == "trial_key" or held == "ominous_trial_key":
				var loot := LootDB.roll_chest("vault", _rng, 9)
				for it in loot:
					if not (it as Dictionary).is_empty():
						session.entities.spawn_item(world, Vector3(pos) + Vector3(0.5, 1.2, 0.5), ItemStack.from_dict(it))
				ItemUse.spend(player, player.inventory.selected, st)
				Sfx.play_at("vault_open", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.9)
				return true
			return false
		"bell":
			Sfx.play_at("bell", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 1.0)
			return true
		"chiseled_bookshelf", "shelf":
			return _shelf(world, pos, v, player, st)
		"redstone_ore":
			if (meta & 1) == 0:
				world.set_blockv(pos, Vox.make(v & 0xFFF, meta | 1), World.F_URGENT)
			return false
	return false


static func _ensure_container(world: World, pos: Vector3i, size: int) -> void:
	var be := world.get_be(pos.x, pos.y, pos.z, true)
	if not be.has("items"):
		var arr := []
		arr.resize(size)
		for i in size:
			arr[i] = {}
		be["items"] = arr
		be["type"] = "container"
		world.mark_modified(pos.x, pos.z)
	elif (be["items"] as Array).size() < size:
		var arr2: Array = be["items"]
		while arr2.size() < size:
			arr2.append({})


static func _toggle_door(world: World, pos: Vector3i, v: int) -> void:
	var meta := (v >> 12) & 15
	var lower := pos if (meta & 8) == 0 else pos + Vector3i(0, -1, 0)
	var lv := world.get_blockv(lower)
	var uv := world.get_blockv(lower + Vector3i(0, 1, 0))
	var lm := ((lv >> 12) & 15) ^ 4
	world.set_blockv(lower, Vox.make(lv & 0xFFF, lm), World.F_URGENT)
	if (uv & 0xFFF) == (lv & 0xFFF):
		world.set_blockv(lower + Vector3i(0, 1, 0), Vox.make(uv & 0xFFF, ((uv >> 12) & 15) ^ 4), World.F_URGENT)
	Sfx.play_at("door_" + ("open" if (lm & 4) != 0 else "close"), Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.7)


static func play_note(world: World, pos: Vector3i) -> void:
	if world.get_blockv(pos + Vector3i(0, 1, 0)) != 0:
		return
	var below: BlockDef = BlockDB.defs[world.get_id(pos.x, pos.y - 1, pos.z)]
	var instr := "harp"
	for k in NOTE_INSTR:
		if below.name == k or below.sound == k or (k == "stone" and below.tool == "pickaxe") or (k == "wood" and below.tool == "axe"):
			instr = NOTE_INSTR[k]
			break
	var note := int(world.get_be(pos.x, pos.y, pos.z).get("note", 0))
	var pitch := pow(2.0, (note - 12) / 12.0)
	Sfx.play_at("note_" + instr, Vector3(pos) + Vector3(0.5, 1.0, 0.5), 1.0, pitch)
	if world.session != null:
		world.session.particles.note(Vector3(pos) + Vector3(0.5, 1.2, 0.5), note / 24.0)


static func _bed(world: World, pos: Vector3i, v: int, player) -> bool:
	var session = player.session
	if world.dim != 0:
		world.set_blockv(pos, 0)
		Explosions.explode(world, Vector3(pos) + Vector3(0.5, 0.5, 0.5), 5.0, true, null)
		return true
	var meta := (v >> 12) & 15
	var head := pos
	if (meta & 4) == 0:
		head = pos + Vox.H_FACING_VEC[meta & 3]
	player.spawn_point = Vector3(head) + Vector3(0.5, 0.6, 0.5)
	player.spawn_dim = 0
	session.chat("Respawn point set")
	if session.is_night() or session.thundering():
		if session.hostiles_near(player.body.pos, 8.0) and not player.is_creative():
			session.chat("You may not rest now; there are monsters nearby")
			return true
		session.start_sleep(head)
	else:
		session.chat("You can only sleep at night or during thunderstorms")
	return true


static func _respawn_anchor(world: World, pos: Vector3i, v: int, player, st: ItemStack) -> bool:
	var meta := (v >> 12) & 15
	var charges := meta & 7
	if st != null and st.item_name() == "glowstone" and charges < 4:
		world.set_blockv(pos, Vox.make(v & 0xFFF, charges + 1), World.F_URGENT)
		ItemUse.spend(player, player.inventory.selected, st)
		Sfx.play_at("respawn_anchor_charge", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.8)
		return true
	if charges == 0:
		return false
	if world.dim != 1:
		world.set_blockv(pos, 0)
		Explosions.explode(world, Vector3(pos) + Vector3(0.5, 0.5, 0.5), 5.0, true, null)
		return true
	player.spawn_point = Vector3(pos) + Vector3(0.5, 1.0, 0.5)
	player.spawn_dim = 1
	player.session.chat("Respawn point set")
	Sfx.play_at("respawn_anchor_set", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.8)
	return true


static func _cauldron(world: World, pos: Vector3i, v: int, player, st: ItemStack) -> bool:
	var meta := (v >> 12) & 15
	var level := meta & 3
	var held := st.item_name() if st != null else ""
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	if d.name == "composter":
		return false
	match held:
		"water_bucket":
			world.set_blockv(pos, Vox.make(v & 0xFFF, 3), World.F_URGENT)
			ItemUse.returns_bucket(player, player.inventory.selected, st)
			Sfx.play_at("bucket_empty", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.6)
			return true
		"bucket":
			if level == 3:
				world.set_blockv(pos, Vox.make(v & 0xFFF, 0), World.F_URGENT)
				if not player.is_creative():
					player.inventory.set_stack(player.inventory.selected, ItemStack.of("water_bucket", 1))
				return true
		"glass_bottle":
			if level > 0:
				world.set_blockv(pos, Vox.make(v & 0xFFF, level - 1), World.F_URGENT)
				if not player.is_creative():
					ItemUse.spend(player, player.inventory.selected, st)
					player.inventory.add(ItemStack.new(ItemDB.id("potion"), 1, 0, {"potion": "water"}), 0, 36)
				return true
		"potion":
			if String(st.data.get("potion", "")) == "water" and level < 3:
				world.set_blockv(pos, Vox.make(v & 0xFFF, level + 1), World.F_URGENT)
				ItemUse.returns_bucket(player, player.inventory.selected, st, "glass_bottle")
				return true
	if st != null and st.item().material == "leather" and level > 0 and st.data.has("color"):
		st.data.erase("color")
		world.set_blockv(pos, Vox.make(v & 0xFFF, level - 1), World.F_URGENT)
		return true
	return false


static func _composter(world: World, pos: Vector3i, v: int, player, st: ItemStack) -> bool:
	var meta := (v >> 12) & 15
	var level := meta & 7
	if level >= 7:
		world.set_blockv(pos, Vox.make(v & 0xFFF, 0), World.F_URGENT)
		player.session.entities.spawn_item(world, Vector3(pos) + Vector3(0.5, 1.0, 0.5), ItemStack.of("bone_meal", 1))
		Sfx.play_at("composter_empty", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.6)
		return true
	if st == null:
		return false
	var n := st.item_name()
	var chance := 0.0
	if n.ends_with("_seeds") or n.ends_with("_leaves") or n.ends_with("_sapling") or n in ["short_grass", "kelp", "dried_kelp", "sweet_berries", "glow_berries", "moss_carpet"]:
		chance = 0.3
	elif n in ["cactus", "sugar_cane", "melon_slice", "vine", "tall_grass", "nether_sprouts", "weeping_vines", "twisting_vines"]:
		chance = 0.5
	elif n in ["apple", "beetroot", "carrot", "potato", "wheat", "pumpkin", "carved_pumpkin", "melon", "moss_block", "lily_pad", "fern"] or n.ends_with("_mushroom") or BlockDB.has(n) and BlockDB.defs[BlockDB.id(n)].place == "plant":
		chance = 0.65
	elif n in ["bread", "baked_potato", "cookie", "hay_block", "nether_wart_block", "warped_wart_block"]:
		chance = 0.85
	elif n in ["cake", "pumpkin_pie"]:
		chance = 1.0
	if chance <= 0.0:
		return false
	ItemUse.spend(player, player.inventory.selected, st)
	if randf() < chance:
		world.set_blockv(pos, Vox.make(v & 0xFFF, level + 1), World.F_URGENT)
		Sfx.play_at("composter_fill_success", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.6)
	else:
		Sfx.play_at("composter_fill", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.5)
	return true


static func _jukebox(world: World, pos: Vector3i, player, st: ItemStack) -> bool:
	var be := world.get_be(pos.x, pos.y, pos.z, true)
	if be.has("disc") and not (be["disc"] as Dictionary).is_empty():
		player.session.entities.spawn_item(world, Vector3(pos) + Vector3(0.5, 1.1, 0.5), ItemStack.from_dict(be["disc"]))
		be["disc"] = {}
		Sfx.stop_jukebox(pos)
		return true
	if st != null and st.item_name().begins_with("music_disc_"):
		be["disc"] = st.with_count(1).to_dict()
		ItemUse.spend(player, player.inventory.selected, st)
		Sfx.play_jukebox(pos, st.item_name())
		player.session.action_bar("Now Playing: " + st.display_name())
		world.mark_modified(pos.x, pos.z)
		return true
	return false


static func _flower_pot(world: World, pos: Vector3i, v: int, player, st: ItemStack) -> bool:
	var meta := (v >> 12) & 15
	if meta != 0:
		var plant := FlowerPot.plant_for(meta)
		world.set_blockv(pos, Vox.make(v & 0xFFF, 0), World.F_URGENT)
		if plant != "":
			player.inventory.add(ItemStack.of(plant, 1), 0, 36)
		return true
	if st == null:
		return false
	var m := FlowerPot.meta_for(st.item_name())
	if m <= 0:
		return false
	world.set_blockv(pos, Vox.make(v & 0xFFF, m), World.F_URGENT)
	ItemUse.spend(player, player.inventory.selected, st)
	return true


static func _shelf(world: World, pos: Vector3i, v: int, player, st: ItemStack) -> bool:
	var be := world.get_be(pos.x, pos.y, pos.z, true)
	var items: Array = be.get("items", [{}, {}, {}])
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	var cap := 6 if d.name == "chiseled_bookshelf" else 3
	while items.size() < cap:
		items.append({})
	if st != null and (d.name != "chiseled_bookshelf" or st.item_name() in ["book", "enchanted_book", "written_book", "book_and_quill"]):
		for i in cap:
			if (items[i] as Dictionary).is_empty():
				items[i] = st.with_count(1).to_dict()
				ItemUse.spend(player, player.inventory.selected, st)
				be["items"] = items
				_update_shelf_state(world, pos, v, items)
				return true
		return false
	for i in range(cap - 1, -1, -1):
		if not (items[i] as Dictionary).is_empty():
			var got := ItemStack.from_dict(items[i])
			items[i] = {}
			be["items"] = items
			if got != null:
				player.inventory.add(got, 0, 36)
			_update_shelf_state(world, pos, v, items)
			return true
	return false


static func _update_shelf_state(world: World, pos: Vector3i, v: int, items: Array) -> void:
	var any := false
	for it in items:
		if not (it as Dictionary).is_empty():
			any = true
	var meta := (v >> 12) & 15
	var nm := (meta | 4) if any else (meta & ~4)
	world.set_blockv(pos, Vox.make(v & 0xFFF, nm), World.F_URGENT)
	world.mark_modified(pos.x, pos.z)
