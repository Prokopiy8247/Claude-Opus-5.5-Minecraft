class_name PlayerInteraction
extends RefCounted
## Targeting, block breaking (with survival mining times), block placement, item use and melee attacks.

var p = null
var target: Dictionary = {}          # voxel ray hit
var target_entity = null
var breaking_pos := Vector3i(0, -99999, 0)
var break_progress := 0.0            # 0..1
var break_cooldown := 0
var place_cooldown := 0
var attack_strength_ticks := 100     # ticks since last swing (attack cooldown meter)
var using_item := false
var using_bow := false
var use_ticks := 0
var blocking := false
var swing := 0.0                     # hand swing animation 0..1
var equip := 0.0
var _left_held := false
var _right_held := false
var _last_selected := -1
var creative_break_delay := 5
var reach_survival := 4.5
var reach_creative := 5.0


func _init(player) -> void:
	p = player


func reach() -> float:
	var r := reach_creative if p.is_creative() else reach_survival
	var st: ItemStack = p.inventory.selected_stack()
	if st != null:
		r += st.item().reach_bonus
	return r


func frame(delta: float) -> void:
	swing = maxf(0.0, swing - delta * 3.2)


var _step_dist := 0.0


## Footsteps by material family of the block walked on (quieter while sneaking), swim strokes.
func on_walk(dist: float) -> void:
	_step_dist += dist
	if _step_dist < (2.0 if p.sneaking else 1.45):
		return
	_step_dist = 0.0
	var w: World = p.world
	var pos: Vector3 = p.body.pos
	if p.in_water:
		Sfx.play_at("swim", pos, 0.25)
		return
	var v := w.get_block(floori(pos.x), floori(pos.y - 0.2), floori(pos.z))
	if v == 0:
		v = w.get_block(floori(pos.x), floori(pos.y - 1.1), floori(pos.z))
	if v != 0:
		Sfx.play_block(v, "step", pos, 0.25 if p.sneaking else 0.55)


func attack_cooldown_ticks() -> float:
	var st: ItemStack = p.inventory.selected_stack()
	var spd := 4.0
	if st != null:
		spd = st.item().attack_speed
	if p.effects.has("haste"):
		spd *= 1.0 + 0.1 * (int(p.effects["haste"].amp) + 1)
	return 20.0 / spd


func attack_strength() -> float:
	return clampf((attack_strength_ticks + 0.5) / attack_cooldown_ticks(), 0.0, 1.0)


func tick() -> void:
	var world: World = p.world
	if break_cooldown > 0:
		break_cooldown -= 1
	if place_cooldown > 0:
		place_cooldown -= 1
	attack_strength_ticks += 1
	if p.inventory.selected != _last_selected:
		_last_selected = p.inventory.selected
		attack_strength_ticks = 0
		equip = 1.0
		_stop_using()
	equip = maxf(0.0, equip - 0.25)
	# targeting
	var eye: Vector3 = p.eye_position()
	var dir: Vector3 = p.look_dir()
	target = world.raycast(eye, dir, reach())
	target_entity = null
	if p.session != null and p.session.entities != null:
		var ehit = p.session.entities.raycast_entity(eye, dir, minf(reach(), target.get("dist", 99.0)), p)
		if ehit != null:
			target_entity = ehit
	var locked: bool = p.input_locked or p.dead or p.gamemode == Player.SPECTATOR
	var left := Input.is_action_pressed("attack") and not locked
	var right := Input.is_action_pressed("use") and not locked
	var left_pressed := left and not _left_held
	var right_pressed := right and not _right_held
	_left_held = left
	_right_held = right
	# ---- attack / mine
	if left_pressed and target_entity != null:
		_attack_entity(target_entity)
		reset_break()
	elif left and target_entity == null and not target.is_empty():
		_mine()
	else:
		reset_break()
		if left_pressed:
			swing = 1.0
			attack_strength_ticks = 0
	# ---- use
	if right:
		if using_item:
			use_ticks += 1
			ItemUse.using_tick(p, self, use_ticks)
		elif place_cooldown <= 0:
			place_cooldown = 4
			if not _use(right_pressed):
				place_cooldown = 0
	elif using_item:
		ItemUse.release(p, self, use_ticks)
		_stop_using()
	if Input.is_action_just_pressed("pick_block") and not locked:
		_pick_block()
	if Input.is_action_just_pressed("drop_item") and not locked:
		drop_selected(Input.is_key_pressed(KEY_CTRL))
	if Input.is_action_just_pressed("swap_hand") and not locked:
		var a: ItemStack = p.inventory.stack_at(p.inventory.selected)
		var b: ItemStack = p.inventory.stack_at(Inventory.OFFHAND)
		p.inventory.set_stack(p.inventory.selected, b)
		p.inventory.set_stack(Inventory.OFFHAND, a)


func start_using() -> void:
	using_item = true
	use_ticks = 0


func _stop_using() -> void:
	using_item = false
	using_bow = false
	blocking = false
	use_ticks = 0


func reset_break() -> void:
	if break_progress > 0.0 and p.session != null:
		p.session.set_break_overlay(Vector3i.ZERO, -1)
	break_progress = 0.0
	breaking_pos = Vector3i(0, -99999, 0)


## Survival mining speed (Minecraft formula: speed / hardness / (30 or 100) per tick).
func dig_speed(v: int) -> float:
	var id := v & 0xFFF
	var d: BlockDef = BlockDB.defs[id]
	var hardness := d.hardness
	if hardness < 0.0:
		return 0.0
	if hardness == 0.0:
		return 1.0
	var st: ItemStack = p.inventory.selected_stack()
	var speed := 1.0
	var correct := d.tool == "" or not d.needs_tool
	if st != null:
		var it := st.item()
		if it.tool != "" and (it.tool == d.tool or (it.tool == "sword" and d.tool == "sword") or (it.tool == "shears" and (d.model == BlockDB.M_LEAVES or d.tags.has("wool") or d.name == "cobweb"))):
			speed = it.mine_speed
			if it.tool == "sword":
				speed = 15.0 if d.name == "cobweb" else 1.5
			if it.tool == "shears":
				speed = 15.0 if (d.name == "cobweb" or d.model == BlockDB.M_LEAVES) else 5.0
			if it.tier >= d.tier or d.tier == 0:
				correct = true
			var eff := st.enchant_level("efficiency")
			if eff > 0:
				speed += eff * eff + 1
		elif it.tool == "sword" and d.model == BlockDB.M_LEAVES:
			speed = 1.5
	if d.needs_tool and st != null and st.item().tool == d.tool and st.item().tier >= d.tier:
		correct = true
	elif d.needs_tool:
		correct = false
	if p.effects.has("haste"):
		speed *= 1.0 + 0.2 * (int(p.effects["haste"].amp) + 1)
	if p.effects.has("mining_fatigue"):
		speed *= pow(0.3, mini(4, int(p.effects["mining_fatigue"].amp) + 1))
	if p.eyes_in_water:
		var helmet: ItemStack = p.inventory.stack_at(Inventory.ARMOR + 3)
		if helmet == null or helmet.enchant_level("aqua_affinity") == 0:
			speed /= 5.0
	if not p.body.on_ground and not p.flying:
		speed /= 5.0
	return speed / hardness / (30.0 if correct else 100.0)


func _mine() -> void:
	var world: World = p.world
	var pos: Vector3i = target.pos
	var v: int = target.block
	if pos != breaking_pos:
		reset_break()
		breaking_pos = pos
	swing = maxf(swing, 0.6) if int(p.session.tick) % 5 == 0 else swing
	if p.is_creative():
		if break_cooldown > 0:
			return
		var st: ItemStack = p.inventory.selected_stack()
		if st != null and st.item().tool == "sword":
			return
		if BlockDB.defs[v & 0xFFF].hardness < 0.0 and not p.is_creative():
			return
		break_block(pos, v, false)
		break_cooldown = creative_break_delay
		swing = 1.0
		return
	var ds := dig_speed(v)
	if ds <= 0.0:
		return
	break_progress += ds
	if int(p.session.tick) % 4 == 0:
		swing = 1.0
		Sfx.play_block(v, "hit", Vector3(pos) + Vector3(0.5, 0.5, 0.5), 0.35)
		p.session.particles.block_hit(v, Vector3(pos), target.face)
	p.session.set_break_overlay(pos, clampi(int(break_progress * 10.0), 0, 9))
	if break_progress >= 1.0:
		break_block(pos, v, true)
		break_cooldown = 5


## Breaks a block as the player (drops in survival, tool durability, exhaustion).
func break_block(pos: Vector3i, v: int, survival: bool) -> void:
	var world: World = p.world
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	if d.hardness < 0.0 and survival:
		return
	var st: ItemStack = p.inventory.selected_stack()
	if survival:
		BlockBehaviors.player_break(world, pos, v, st, p)
		p.stats.add_exhaustion(0.005)
		if st != null and st.item().is_damageable() and d.hardness > 0.0:
			var tool_cost := 1 if (st.item().tool != "sword") else 2
			damage_item(p.inventory.selected, tool_cost)
	else:
		BlockBehaviors.remove_block(world, pos, v, false)
	Sfx.play_block(v, "break", Vector3(pos) + Vector3(0.5, 0.5, 0.5))
	p.session.particles.block_break(v, Vector3(pos))
	reset_break()


func damage_item(slot: int, amount: int) -> void:
	if p.is_creative():
		return
	var st: ItemStack = p.inventory.stack_at(slot)
	if st == null or not st.item().is_damageable():
		return
	var unbreaking := st.enchant_level("unbreaking")
	for i in amount:
		if unbreaking > 0 and randf() > 1.0 / (unbreaking + 1):
			continue
		st.damage += 1
	if st.damage >= st.item().durability:
		p.inventory.set_stack(slot, null)
		Sfx.play_at("item_break", p.body.pos)
		p.session.particles.item_break(st, p.eye_position() + p.look_dir() * 0.5)
	p.inventory.changed.emit()


## Wears one armour slot (elytra while gliding).
func damage_armor_slot(slot: int, amount: int) -> void:
	var st: ItemStack = p.inventory.stack_at(slot)
	if st == null or not st.item().is_damageable():
		return
	st.damage += amount
	if st.damage >= st.item().durability - 1 and st.item().props.get("elytra", false):
		st.damage = st.item().durability - 1       # a broken elytra stays equipped but stops working
		p.elytra_flying = false
	elif st.damage >= st.item().durability:
		p.inventory.set_stack(slot, null)
		Sfx.play_at("item_break", p.body.pos)
	p.inventory.changed.emit()


func damage_armor(amount: float) -> void:
	var n := maxi(1, int(amount / 4.0))
	for s in 4:
		var st: ItemStack = p.inventory.stack_at(Inventory.ARMOR + s)
		if st != null and st.item().is_damageable():
			damage_item(Inventory.ARMOR + s, n)


func on_shield_block(amount: float, attacker) -> void:
	Sfx.play_at("shield_block", p.body.pos)
	var slot := Inventory.OFFHAND
	var off: ItemStack = p.inventory.stack_at(Inventory.OFFHAND)
	if off == null or off.item_name() != "shield":
		slot = p.inventory.selected
	if amount >= 3.0:
		damage_item(slot, 1 + int(amount))
	if attacker != null and attacker.has_method("knockback"):
		var d: Vector3 = attacker.global_position - p.body.pos
		attacker.knockback(Vector2(d.x, d.z).normalized(), 0.5)


func _use(pressed: bool) -> bool:
	var st: ItemStack = p.inventory.selected_stack()
	var slot: int = p.inventory.selected
	# entity interaction first
	if target_entity != null and pressed:
		if target_entity.has_method("interact") and target_entity.interact(p, st):
			swing = 1.0
			return true
	# interactable blocks (unless sneaking with an item)
	if not target.is_empty():
		var v: int = target.block
		var d: BlockDef = BlockDB.defs[v & 0xFFF]
		if d.interact != "" and (not p.sneaking or st == null):
			if BlockInteract.interact(p.world, target.pos, v, p, st, target):
				swing = 1.0
				return true
	# items with a use action
	if st != null:
		if ItemUse.use(p, self, st, slot, target, pressed):
			return true
		if st.item().block != "" and not target.is_empty():
			return place_from_stack(st, slot)
	# offhand
	var off: ItemStack = p.inventory.stack_at(Inventory.OFFHAND)
	if off != null:
		if ItemUse.use(p, self, off, Inventory.OFFHAND, target, pressed):
			return true
		if off.item().block != "" and not target.is_empty():
			return place_from_stack(off, Inventory.OFFHAND)
	return false


func place_from_stack(st: ItemStack, slot: int) -> bool:
	var bid := BlockDB.id(st.item().block)
	if bid <= 0:
		return false
	return place_block(bid, slot)


func place_block(bid: int, slot: int) -> bool:
	var world: World = p.world
	var list := BlockPlacement.placements(world, bid, target, p, p.sneaking)
	if list.is_empty():
		return false
	# do not place solid blocks inside entities (player)
	var pbox: AABB = p.body.aabb().grow(-0.01)
	for e in list:
		var pos: Vector3i = e[0]
		var v: int = e[1]
		if BlockDB.solid[v & 0xFFF] == 1:
			for b in BlockShapes.collision(v, world, pos.x, pos.y, pos.z):
				var box: AABB = b
				if AABB(box.position + Vector3(pos), box.size).intersects(pbox) and p.gamemode != Player.SPECTATOR:
					return false
			if p.session.entities != null and p.session.entities.blocks_placement(AABB(Vector3(pos), Vector3.ONE)):
				return false
	for e in list:
		var pos2: Vector3i = e[0]
		var v2: int = e[1]
		world.set_block(pos2.x, pos2.y, pos2.z, v2, World.F_DEFAULT)
		BlockBehaviors.on_placed(world, pos2, v2, p)
	Sfx.play_block(list[0][1], "place", Vector3(list[0][0]) + Vector3(0.5, 0.5, 0.5))
	swing = 1.0
	if not p.is_creative():
		var st: ItemStack = p.inventory.stack_at(slot)
		if st != null:
			st.count -= 1
			if st.count <= 0:
				p.inventory.set_stack(slot, null)
			p.inventory.changed.emit()
	return true


func _attack_entity(e) -> void:
	swing = 1.0
	var strength := attack_strength()
	attack_strength_ticks = 0
	if not e.has_method("hurt"):
		return
	var st: ItemStack = p.inventory.selected_stack()
	var dmg := 1.0
	if st != null:
		dmg = st.item().damage
	if p.effects.has("strength"):
		dmg += 3.0 * (int(p.effects["strength"].amp) + 1)
	if p.effects.has("weakness"):
		dmg -= 4.0
	var ench := 0.0
	if st != null:
		var sharp := st.enchant_level("sharpness")
		if sharp > 0:
			ench += 0.5 * sharp + 0.5
		if e.has_method("is_undead") and e.is_undead():
			ench += 2.5 * st.enchant_level("smite")
		if e.has_method("is_arthropod") and e.is_arthropod():
			ench += 2.5 * st.enchant_level("bane_of_arthropods")
	dmg = dmg * (0.2 + strength * strength * 0.8) + ench * strength
	var crit: bool = strength > 0.9 and p.body.vel.y < 0.0 and not p.body.on_ground and not p.on_ladder and not p.in_water and not p.flying
	if crit:
		dmg *= 1.5
		p.session.particles.crit(e.global_position + Vector3(0, 1, 0))
		Sfx.play_at("crit", e.global_position)
	var kb := 0.4 + (0.4 if (p.sprinting and strength > 0.9) else 0.0)
	if st != null:
		kb += 0.5 * st.enchant_level("knockback")
	# mace smash
	if st != null and st.item().props.get("mace", false) and p.body.fall_distance > 1.5:
		var fd: float = p.body.fall_distance
		dmg += 4.0 * minf(fd, 3.0) + 2.0 * clampf(fd - 3.0, 0.0, 5.0) + maxf(0.0, fd - 8.0)
		p.body.vel.y = 0.0
		p.body.fall_distance = 0.0
		p.session.particles.burst(e.global_position, Color(0.8, 0.8, 0.8), 30)
		Sfx.play_at("mace_smash", e.global_position)
	var dir := Vector2(-sin(p.yaw), -cos(p.yaw))
	var dealt: float = e.hurt(dmg, "player", p, dir, kb)
	ItemExtras.alert_pets(p.session, e)
	if st != null and st.item().props.get("mace", false) == false:
		var fa := st.enchant_level("fire_aspect")
		if fa > 0 and e.has_method("set_on_fire"):
			e.set_on_fire(80 * fa)
	if dealt > 0.0 and p.sprinting:
		p.sprinting = false
	# sweeping edge for swords at full strength while standing still
	if st != null and st.item().tool == "sword" and strength > 0.9 and not crit and not p.sprinting and p.body.on_ground:
		p.session.entities.sweep_attack(p, e, 1.0 + st.enchant_level("sweeping_edge") * 0.5)
	if st != null and st.item().is_damageable():
		damage_item(p.inventory.selected, 1 if st.item().tool in ["sword", "spear"] or st.item().name == "trident" or st.item().name == "mace" else 2)
	p.stats.add_exhaustion(0.1)


func _pick_block() -> void:
	if target.is_empty() or not p.is_creative():
		return
	var v: int = target.block
	var iid := ItemDB.for_block(v)
	if iid < 0:
		return
	var inv: Inventory = p.inventory
	for i in 9:
		var s := inv.stack_at(i)
		if s != null and s.id == iid:
			inv.selected = i
			return
	var slot := inv.selected
	if inv.stack_at(slot) != null:
		var empty := inv.first_empty(0, 9)
		if empty >= 0:
			slot = empty
	inv.set_stack(slot, ItemStack.new(iid, 1))
	inv.selected = slot


func drop_selected(whole: bool) -> void:
	var inv: Inventory = p.inventory
	var st := inv.selected_stack()
	if st == null:
		return
	var n := st.count if whole else 1
	var drop := st.split(n)
	if st.is_empty():
		inv.set_stack(inv.selected, null)
	inv.changed.emit()
	p.session.drop_item_from_player(drop)
	swing = 1.0
