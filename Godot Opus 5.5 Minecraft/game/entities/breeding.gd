class_name Breeding
extends RefCounted
## Animal breeding and growing up: feeding an adult its breed item puts it in love for 30 s;
## two animals of the same kind in love walk to each other and make a baby (+1..7 XP), then rest
## for 5 minutes. Babies grow up after 20 minutes; feeding them speeds that up by 10 %.

const LOVE_TICKS := 600
const COOLDOWN := 6000
const GROW_TICKS := 24000

## Extra breed foods beyond MobDB's single breed_item.
const FOODS := {
	"pig": ["carrot", "potato", "beetroot"],
	"cow": ["wheat"], "mooshroom": ["wheat"], "sheep": ["wheat"], "goat": ["wheat"],
	"chicken": ["wheat_seeds", "melon_seeds", "pumpkin_seeds", "beetroot_seeds", "torchflower_seeds", "pitcher_pod"],
	"horse": ["golden_apple", "golden_carrot", "enchanted_golden_apple"],
	"donkey": ["golden_apple", "golden_carrot"],
	"rabbit": ["carrot", "golden_carrot", "dandelion"],
	"wolf": ["beef", "cooked_beef", "porkchop", "cooked_porkchop", "chicken", "cooked_chicken", "mutton", "cooked_mutton", "rabbit", "cooked_rabbit", "rotten_flesh"],
	"cat": ["cod", "salmon"], "ocelot": ["cod", "salmon"],
	"llama": ["hay_block"], "fox": ["sweet_berries", "glow_berries"], "panda": ["bamboo"],
	"turtle": ["seagrass"], "bee": ["dandelion", "poppy", "sunflower", "oxeye_daisy", "cornflower"],
	"frog": ["slime_ball"], "axolotl": ["tropical_fish_bucket"], "camel": ["cactus"], "sniffer": ["torchflower_seeds"],
	"armadillo": ["spider_eye"], "strider": ["warped_fungus"], "hoglin": ["crimson_fungus"], "goat_kid": ["wheat"],
}


static func foods_for(m: Mob) -> Array:
	var out: Array = FOODS.get(m.mob, []).duplicate()
	var bi := String(m.def.get("breed_item", ""))
	if bi != "" and not out.has(bi):
		out.append(bi)
	return out


## Right-click with food. Returns true when the item was used.
static func feed(m: Mob, player, held: ItemStack) -> bool:
	if held == null or not foods_for(m).has(held.item_name()):
		return false
	var fed := false
	if m.baby:
		m.data["grow"] = int(m.data.get("grow", -GROW_TICKS)) + GROW_TICKS / 10
		fed = true
	elif int(m.data.get("love", 0)) <= 0 and int(m.data.get("breed_cd", 0)) <= 0:
		if m.def.get("tame_item", "") != "" and not m.tamed and m.mob in ["wolf", "cat", "horse", "donkey", "llama"]:
			return false        # tameable animals breed only once tamed
		m.data["love"] = LOVE_TICKS
		fed = true
	elif m.health < m.max_health:
		m.health = minf(m.max_health, m.health + 2.0)
		fed = true
	if not fed:
		return false
	if m.session != null:
		m.session.particles.hearts(m.body.pos + Vector3(0, m.body.height, 0), 5)
	Sfx.play_mob(m.mob, "eat", m.body.pos, 0.6)
	if not player.is_creative():
		held.count -= 1
		if held.count <= 0:
			player.inventory.set_stack(player.inventory.selected, null)
		player.inventory.changed.emit()
	m.data.erase("despawn")
	return true


## Per-tick love / cooldown / growth. Returns true when it steered the mob this tick (the AI is
## skipped then).
static func tick(m: Mob) -> bool:
	if m.baby:
		var g := int(m.data.get("grow", -GROW_TICKS)) + 1
		m.data["grow"] = g
		if g >= 0:
			grow_up(m)
		return false
	var cd := int(m.data.get("breed_cd", 0))
	if cd > 0:
		m.data["breed_cd"] = cd - 1
	var love := int(m.data.get("love", 0))
	if love <= 0:
		return false
	m.data["love"] = love - 1
	if m.session == null:
		return false
	if love % 10 == 0:
		m.session.particles.hearts(m.body.pos + Vector3(0, m.body.height + 0.2, 0), 1)
	var mate: Mob = null
	var best := 8.0
	for e in m.session.entities.all():
		if e == m or not (e is Mob):
			continue
		var o: Mob = e
		if o.mob != m.mob or o.baby or o.dead or int(o.data.get("love", 0)) <= 0:
			continue
		var d := o.body.pos.distance_to(m.body.pos)
		if d < best:
			best = d
			mate = o
	if mate == null:
		return false
	if best > 1.6:
		m.ai._walk_to(mate.body.pos, 1.0)
		return true
	# a baby between the two parents (only one of them spawns it)
	if m.get_instance_id() < mate.get_instance_id():
		var pos := (m.body.pos + mate.body.pos) * 0.5
		var baby: Mob = m.session.entities.spawn_mob(m.world, m.mob, pos, {"baby": true, "persistent": true})
		if baby != null:
			baby.data["grow"] = -GROW_TICKS
			baby.data.erase("despawn")
		m.session.entities.spawn_xp(m.world, pos, randi_range(1, 7))
		m.session.particles.hearts(pos + Vector3(0, 1, 0), 7)
		for p in [m, mate]:
			var pm: Mob = p
			pm.data["love"] = 0
			pm.data["breed_cd"] = COOLDOWN
	return true


static func grow_up(m: Mob) -> void:
	m.baby = false
	m.data.erase("grow")
	m.body.width = maxf(float(m.def.get("width", 0.6)), 0.2)
	m.body.height = maxf(float(m.def.get("height", 1.8)), 0.2)
	m.max_health = float(m.def.get("hp", 10))
	m.health = minf(m.max_health, m.health * 2.0)
	if m.rig != null:
		m.rig.scale = Vector3.ONE
