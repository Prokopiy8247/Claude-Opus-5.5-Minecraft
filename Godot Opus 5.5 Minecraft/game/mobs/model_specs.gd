class_name ModelSpecs
extends RefCounted
## Cuboid rigs for every mob model (units: 1/16 block "pixels"). Built from archetype helpers so
## proportions stay consistent. The mob faces -Z; +Y is up; origin = feet centre.
##
## Part: [name, parent, px, py, pz, sx, sy, sz, region, inflate, pivot_top, rot_deg (optional Vector3)]
##   pivot_top = true  -> the box hangs below the pivot (legs, arms, tentacles, wings)
##   pivot_top = false -> the box is centred on the pivot
## Parent pivots are absolute model coordinates; MobRenderer converts them to local offsets.

static var SPECS: Dictionary = {}


static func _static_init() -> void:
	_build()


static func get_spec(n: String) -> Dictionary:
	if SPECS.is_empty():
		_build()
	return SPECS.get(n, SPECS.get("pig", {}))


# ------------------------------------------------------------------------------------------------
# helpers
static func _p(n: String, parent: int, pos: Vector3, size: Vector3, region := "body", inflate := 0.0, top := false,
		rot := Vector3.ZERO) -> Array:
	return [n, parent, pos.x, pos.y, pos.z, size.x, size.y, size.z, region, inflate, top, rot]


## Four-legged animal. body = (w, h, length); legs = (w, h); head = (w, h, d); head offset (y above body top, z overlap).
static func _quad(body: Vector3, legs: Vector2, head: Vector3, head_up := 0.0, head_in := 2.0, leg_inset := 0.0) -> Array:
	var parts := []
	var by := legs.y + body.y * 0.5
	parts.append(_p("body", -1, Vector3(0, by, 0), body, "body"))
	var hy := legs.y + body.y + head_up - head.y * 0.5 + 2.0
	var hz := -body.z * 0.5 - head.z * 0.5 + head_in
	parts.append(_p("head", 0, Vector3(0, hy, hz), head, "head"))
	var lx := body.x * 0.5 - legs.x * 0.5 - leg_inset
	var lz := body.z * 0.5 - legs.x * 0.5 - 1.0
	parts.append(_p("leg_fl", -1, Vector3(lx, legs.y, -lz), Vector3(legs.x, legs.y, legs.x), "limb", 0.0, true))
	parts.append(_p("leg_fr", -1, Vector3(-lx, legs.y, -lz), Vector3(legs.x, legs.y, legs.x), "limb", 0.0, true))
	parts.append(_p("leg_bl", -1, Vector3(lx, legs.y, lz), Vector3(legs.x, legs.y, legs.x), "limb", 0.0, true))
	parts.append(_p("leg_br", -1, Vector3(-lx, legs.y, lz), Vector3(legs.x, legs.y, legs.x), "limb", 0.0, true))
	return parts


## Biped. head (w,h,d); torso (w,h,d); arms (w,h,d); legs (w,h,d). Returns parts with head=1, torso=0.
static func _biped(head: Vector3, torso: Vector3, arm: Vector3, leg: Vector3, arms_forward := false) -> Array:
	var parts := []
	var ty := leg.y + torso.y * 0.5
	parts.append(_p("torso", -1, Vector3(0, ty, 0), torso, "body"))
	parts.append(_p("head", 0, Vector3(0, leg.y + torso.y + head.y * 0.5, 0), head, "head"))
	var ax := torso.x * 0.5 + arm.x * 0.5
	var ay := leg.y + torso.y - 0.5
	var arot := Vector3(90, 0, 0) if arms_forward else Vector3.ZERO
	parts.append(_p("arm_right", 0, Vector3(ax, ay, 0), arm, "limb", 0.0, true, arot))
	parts.append(_p("arm_left", 0, Vector3(-ax, ay, 0), arm, "limb", 0.0, true, arot))
	var lx := leg.x * 0.5
	parts.append(_p("leg_fl", -1, Vector3(lx, leg.y, 0), leg, "limb", 0.0, true))
	parts.append(_p("leg_fr", -1, Vector3(-lx, leg.y, 0), leg, "limb", 0.0, true))
	return parts


static func _add(parts: Array, part: Array) -> int:
	parts.append(part)
	return parts.size() - 1


static func _put(n: String, size: Vector2, parts: Array) -> void:
	SPECS[n] = {"size": [size.x, size.y], "parts": parts}


# ------------------------------------------------------------------------------------------------
static func _build() -> void:
	SPECS.clear()
	_passive()
	_humanoids()
	_hostile()
	_nether()
	_end_and_bosses()
	_water()
	_small()


static func _passive() -> void:
	# pig
	var pig := _quad(Vector3(10, 8, 16), Vector2(4, 6), Vector3(8, 8, 8), -1.0, 2.0)
	_add(pig, _p("snout", 1, Vector3(0, 10.5, -14.5), Vector3(4, 3, 1), "extra"))
	_put("pig", Vector2(14, 14), pig)
	# cow (+ mooshroom)
	var cow := _quad(Vector3(12, 10, 18), Vector2(4, 12), Vector3(8, 8, 6), 0.0, 1.0)
	_add(cow, _p("snout", 1, Vector3(0, 21, -15.5), Vector3(6, 3, 1), "extra"))
	_add(cow, _p("horn_l", 1, Vector3(4.5, 26.5, -11), Vector3(1, 3, 1), "extra"))
	_add(cow, _p("horn_r", 1, Vector3(-4.5, 26.5, -11), Vector3(1, 3, 1), "extra"))
	_add(cow, _p("udder", 0, Vector3(0, 11, 5), Vector3(4, 2, 6), "extra"))
	_put("cow", Vector2(14, 22), cow)
	var moo: Array = cow.duplicate(true)
	_add(moo, _p("mushroom_1", 0, Vector3(2, 24.5, 2), Vector3(4, 5, 4), "extra"))
	_add(moo, _p("mushroom_2", 0, Vector3(-2, 24.5, 6), Vector3(4, 5, 4), "extra"))
	_put("mooshroom", Vector2(14, 22), moo)
	# sheep: body + wool coat
	var sheep := _quad(Vector3(8, 6, 16), Vector2(4, 12), Vector3(6, 6, 8), 0.0, 2.0)
	_add(sheep, _p("wool", 0, Vector3(0, 15, 0), Vector3(10, 8, 18), "extra", 0.5))
	_add(sheep, _p("ear_1", 1, Vector3(3.5, 22, -12), Vector3(2, 1, 1), "extra"))
	_add(sheep, _p("ear_2", 1, Vector3(-3.5, 22, -12), Vector3(2, 1, 1), "extra"))
	_put("sheep", Vector2(14, 20), sheep)
	# chicken
	var ch := []
	_add(ch, _p("body", -1, Vector3(0, 8, 0), Vector3(6, 6, 8), "body"))
	_add(ch, _p("head", 0, Vector3(0, 13, -4), Vector3(4, 6, 3), "head"))
	_add(ch, _p("beak", 1, Vector3(0, 13, -6.5), Vector3(4, 2, 2), "extra"))
	_add(ch, _p("wattle", 1, Vector3(0, 11, -6), Vector3(2, 2, 2), "extra"))
	_add(ch, _p("wing_left", 0, Vector3(3.5, 11, 0), Vector3(1, 4, 6), "limb", 0.0, true))
	_add(ch, _p("wing_right", 0, Vector3(-3.5, 11, 0), Vector3(1, 4, 6), "limb", 0.0, true))
	_add(ch, _p("leg_fl", -1, Vector3(1.5, 5, 1), Vector3(1, 5, 1), "limb", 0.0, true))
	_add(ch, _p("leg_fr", -1, Vector3(-1.5, 5, 1), Vector3(1, 5, 1), "limb", 0.0, true))
	_put("chicken", Vector2(8, 11), ch)
	# rabbit
	var rb := []
	_add(rb, _p("body", -1, Vector3(0, 4, 1), Vector3(6, 5, 9), "body"))
	_add(rb, _p("head", 0, Vector3(0, 7, -4.5), Vector3(5, 4, 5), "head"))
	_add(rb, _p("ear_1", 1, Vector3(1.5, 11.5, -3.5), Vector3(2, 5, 1), "extra"))
	_add(rb, _p("ear_2", 1, Vector3(-1.5, 11.5, -3.5), Vector3(2, 5, 1), "extra"))
	_add(rb, _p("tail", 0, Vector3(0, 5, 6), Vector3(2, 2, 2), "extra"))
	_add(rb, _p("leg_fl", -1, Vector3(1.5, 3, -3), Vector3(2, 3, 2), "limb", 0.0, true))
	_add(rb, _p("leg_fr", -1, Vector3(-1.5, 3, -3), Vector3(2, 3, 2), "limb", 0.0, true))
	_add(rb, _p("leg_bl", -1, Vector3(2.5, 2, 3.5), Vector3(2, 2, 5), "limb", 0.0, true))
	_add(rb, _p("leg_br", -1, Vector3(-2.5, 2, 3.5), Vector3(2, 2, 5), "limb", 0.0, true))
	_put("rabbit", Vector2(7, 8), rb)
	# horse family
	for hn in ["horse", "skeleton_horse"]:
		var legw := 4.0 if hn == "horse" else 3.0
		var hs := _quad(Vector3(10, 10, 22), Vector2(legw, 12), Vector3(5, 5, 10), 0.0, 0.0)
		# replace default head with neck + head
		hs[1] = _p("neck", 0, Vector3(0, 25, -11), Vector3(4, 12, 7), "body", 0.0, false, Vector3(-30, 0, 0))
		_add(hs, _p("head", 1, Vector3(0, 31, -16), Vector3(5, 5, 11), "head", 0.0, false, Vector3(-20, 0, 0)))
		_add(hs, _p("ear_1", 1, Vector3(1.5, 34, -12), Vector3(2, 3, 1), "extra"))
		_add(hs, _p("ear_2", 1, Vector3(-1.5, 34, -12), Vector3(2, 3, 1), "extra"))
		_add(hs, _p("mane", 1, Vector3(0, 26, -8), Vector3(2, 14, 4), "extra", 0.0, false, Vector3(-30, 0, 0)))
		_add(hs, _p("tail_1", 0, Vector3(0, 20, 11), Vector3(3, 14, 4), "extra", 0.0, true, Vector3(-30, 0, 0)))
		_put(hn, Vector2(20, 30), hs)
	# camel
	var cm := _quad(Vector3(14, 12, 24), Vector2(5, 20), Vector3(7, 7, 12), 6.0, -2.0)
	cm[1] = _p("neck", 0, Vector3(0, 34, -13), Vector3(5, 14, 6), "body")
	_add(cm, _p("head", 1, Vector3(0, 41, -17), Vector3(7, 7, 12), "head"))
	_add(cm, _p("hump", 0, Vector3(0, 34.5, 1), Vector3(9, 5, 12), "extra"))
	_add(cm, _p("tail_1", 0, Vector3(0, 30, 12.5), Vector3(2, 10, 1), "extra", 0.0, true))
	_put("camel", Vector2(26, 36), cm)
	# llama
	var ll := _quad(Vector3(12, 10, 18), Vector2(4, 14), Vector3(6, 6, 9), 0.0, 0.0)
	ll[1] = _p("neck", 0, Vector3(0, 30, -8), Vector3(8, 18, 6), "body")
	_add(ll, _p("head", 1, Vector3(0, 40, -11), Vector3(6, 6, 9), "head"))
	_add(ll, _p("ear_1", 2, Vector3(2, 45, -9), Vector3(2, 3, 2), "extra"))
	_add(ll, _p("ear_2", 2, Vector3(-2, 45, -9), Vector3(2, 3, 2), "extra"))
	_put("llama", Vector2(14, 30), ll)
	# wolf
	var wf := _quad(Vector3(6, 6, 9), Vector2(2, 8), Vector3(6, 6, 4), 0.0, 1.0)
	_add(wf, _p("mane", 0, Vector3(0, 11.5, -3), Vector3(8, 7, 6), "extra"))
	_add(wf, _p("snout", 1, Vector3(0, 11, -8.5), Vector3(3, 3, 3), "extra"))
	_add(wf, _p("ear_1", 1, Vector3(2, 16, -5), Vector3(2, 2, 1), "extra"))
	_add(wf, _p("ear_2", 1, Vector3(-2, 16, -5), Vector3(2, 2, 1), "extra"))
	_add(wf, _p("tail_1", 0, Vector3(0, 12, 4.5), Vector3(2, 8, 2), "extra", 0.0, true, Vector3(-40, 0, 0)))
	_put("wolf", Vector2(10, 14), wf)
	# cat / ocelot
	var ct := _quad(Vector3(4, 4, 14), Vector2(2, 6), Vector3(5, 4, 5), 0.0, 1.0)
	_add(ct, _p("snout", 1, Vector3(0, 9, -10.5), Vector3(3, 2, 1), "extra"))
	_add(ct, _p("ear_1", 1, Vector3(1.5, 12, -8), Vector3(1, 1, 2), "extra"))
	_add(ct, _p("ear_2", 1, Vector3(-1.5, 12, -8), Vector3(1, 1, 2), "extra"))
	_add(ct, _p("tail_1", 0, Vector3(0, 9, 7), Vector3(1, 8, 1), "extra", 0.0, true, Vector3(-60, 0, 0)))
	_put("cat", Vector2(8, 10), ct)
	# fox
	var fx := _quad(Vector3(6, 6, 11), Vector2(2, 6), Vector3(8, 6, 6), -1.0, 2.0)
	_add(fx, _p("snout", 1, Vector3(0, 10, -11), Vector3(4, 2, 3), "extra"))
	_add(fx, _p("ear_1", 1, Vector3(2.5, 15, -7), Vector3(2, 2, 1), "extra"))
	_add(fx, _p("ear_2", 1, Vector3(-2.5, 15, -7), Vector3(2, 2, 1), "extra"))
	_add(fx, _p("tail_1", 0, Vector3(0, 10, 6), Vector3(4, 9, 5), "extra", 0.0, true, Vector3(-70, 0, 0)))
	_put("fox", Vector2(10, 11), fx)
	# panda / polar bear
	var pd := _quad(Vector3(14, 12, 20), Vector2(6, 9), Vector3(10, 8, 7), -2.0, 3.0)
	_add(pd, _p("ear_1", 1, Vector3(4.5, 20.5, -12), Vector3(3, 2, 1), "extra"))
	_add(pd, _p("ear_2", 1, Vector3(-4.5, 20.5, -12), Vector3(3, 2, 1), "extra"))
	_add(pd, _p("snout", 1, Vector3(0, 15, -16), Vector3(5, 3, 2), "extra"))
	_put("panda", Vector2(20, 22), pd)
	var pb := _quad(Vector3(12, 10, 20), Vector2(4, 10), Vector3(7, 7, 7), -1.0, 3.0)
	_add(pb, _p("snout", 1, Vector3(0, 17, -16), Vector3(5, 3, 3), "extra"))
	_add(pb, _p("ear_1", 1, Vector3(3, 23, -12), Vector3(2, 2, 1), "extra"))
	_add(pb, _p("ear_2", 1, Vector3(-3, 23, -12), Vector3(2, 2, 1), "extra"))
	_put("polar_bear", Vector2(20, 22), pb)
	# goat
	var gt := _quad(Vector3(9, 10, 14), Vector2(3, 10), Vector3(5, 7, 10), 3.0, 4.0)
	_add(gt, _p("horn_l", 1, Vector3(1.5, 28, -9), Vector3(2, 6, 2), "extra", 0.0, false, Vector3(20, 0, 0)))
	_add(gt, _p("horn_r", 1, Vector3(-1.5, 28, -9), Vector3(2, 6, 2), "extra", 0.0, false, Vector3(20, 0, 0)))
	_add(gt, _p("beard", 1, Vector3(0, 18, -15), Vector3(1, 4, 2), "extra"))
	_put("goat", Vector2(14, 20), gt)
	# armadillo
	var ad := _quad(Vector3(7, 6, 10), Vector2(2, 3), Vector3(3, 4, 4), -2.0, 1.0)
	_add(ad, _p("shell", 0, Vector3(0, 6.5, 0), Vector3(8, 6, 11), "extra"))
	_add(ad, _p("ear_1", 1, Vector3(1, 9, -6), Vector3(1, 3, 1), "extra"))
	_add(ad, _p("ear_2", 1, Vector3(-1, 9, -6), Vector3(1, 3, 1), "extra"))
	_add(ad, _p("tail_1", 0, Vector3(0, 5, 5.5), Vector3(1, 1, 5), "extra", 0.0, false, Vector3(30, 0, 0)))
	_put("armadillo", Vector2(11, 10), ad)
	# sniffer: huge body, long face
	var sn := []
	_add(sn, _p("body", -1, Vector3(0, 16, 0), Vector3(24, 18, 36), "body"))
	_add(sn, _p("moss", 0, Vector3(0, 26, 2), Vector3(24, 3, 30), "extra"))
	_add(sn, _p("head", 0, Vector3(0, 16, -22), Vector3(12, 12, 10), "head"))
	_add(sn, _p("nose", 2, Vector3(0, 11, -29), Vector3(10, 6, 6), "extra"))
	for i in 3:
		var z := -12.0 + i * 12.0
		_add(sn, _p("leg_%d" % (i * 2 + 1), -1, Vector3(9, 7, z), Vector3(5, 7, 5), "limb", 0.0, true))
		_add(sn, _p("leg_%d" % (i * 2 + 2), -1, Vector3(-9, 7, z), Vector3(5, 7, 5), "limb", 0.0, true))
	_put("sniffer", Vector2(30, 28), sn)
	# frog
	var fr := []
	_add(fr, _p("body", -1, Vector3(0, 4, 0), Vector3(7, 4, 9), "body"))
	_add(fr, _p("head", 0, Vector3(0, 6.5, -2), Vector3(7, 3, 9), "head"))
	_add(fr, _p("eye_1", 1, Vector3(2.5, 8.5, -4), Vector3(3, 2, 3), "extra"))
	_add(fr, _p("eye_2", 1, Vector3(-2.5, 8.5, -4), Vector3(3, 2, 3), "extra"))
	_add(fr, _p("leg_fl", -1, Vector3(3.5, 2, -3), Vector3(2, 2, 3), "limb", 0.0, true))
	_add(fr, _p("leg_fr", -1, Vector3(-3.5, 2, -3), Vector3(2, 2, 3), "limb", 0.0, true))
	_add(fr, _p("leg_bl", -1, Vector3(4, 2, 3), Vector3(3, 2, 4), "limb", 0.0, true))
	_add(fr, _p("leg_br", -1, Vector3(-4, 2, 3), Vector3(3, 2, 4), "limb", 0.0, true))
	_put("frog", Vector2(8, 8), fr)
	# turtle
	var tt := []
	_add(tt, _p("body", -1, Vector3(0, 4, 0), Vector3(19, 5, 22), "body"))
	_add(tt, _p("shell", 0, Vector3(0, 7.5, 1), Vector3(17, 3, 18), "extra"))
	_add(tt, _p("head", 0, Vector3(0, 4, -13), Vector3(6, 5, 6), "head"))
	_add(tt, _p("leg_fl", -1, Vector3(9, 2, -8), Vector3(10, 1, 4), "limb", 0.0, true))
	_add(tt, _p("leg_fr", -1, Vector3(-9, 2, -8), Vector3(10, 1, 4), "limb", 0.0, true))
	_add(tt, _p("leg_bl", -1, Vector3(6, 2, 10), Vector3(4, 1, 8), "limb", 0.0, true))
	_add(tt, _p("leg_br", -1, Vector3(-6, 2, 10), Vector3(4, 1, 8), "limb", 0.0, true))
	_put("turtle", Vector2(20, 8), tt)
	# axolotl
	var ax := []
	_add(ax, _p("body", -1, Vector3(0, 3, 0), Vector3(8, 4, 10), "body"))
	_add(ax, _p("head", 0, Vector3(0, 3.5, -7.5), Vector3(8, 5, 5), "head"))
	_add(ax, _p("gill_1", 1, Vector3(4.5, 6, -7), Vector3(1, 5, 2), "extra"))
	_add(ax, _p("gill_2", 1, Vector3(-4.5, 6, -7), Vector3(1, 5, 2), "extra"))
	_add(ax, _p("gill_top", 1, Vector3(0, 7, -7), Vector3(8, 3, 1), "extra"))
	_add(ax, _p("tail", 0, Vector3(0, 3.5, 9.5), Vector3(1, 5, 12), "extra"))
	_add(ax, _p("leg_fl", -1, Vector3(4, 1.5, -3), Vector3(3, 1, 2), "limb", 0.0, true))
	_add(ax, _p("leg_fr", -1, Vector3(-4, 1.5, -3), Vector3(3, 1, 2), "limb", 0.0, true))
	_add(ax, _p("leg_bl", -1, Vector3(4, 1.5, 3), Vector3(3, 1, 2), "limb", 0.0, true))
	_add(ax, _p("leg_br", -1, Vector3(-4, 1.5, 3), Vector3(3, 1, 2), "limb", 0.0, true))
	_put("axolotl", Vector2(8, 6), ax)


static func _humanoids() -> void:
	var zb := _biped(Vector3(8, 8, 8), Vector3(8, 12, 4), Vector3(4, 12, 4), Vector3(4, 12, 4), true)
	_put("zombie", Vector2(8, 32), zb)
	var pm := _biped(Vector3(8, 8, 8), Vector3(8, 12, 4), Vector3(4, 12, 4), Vector3(4, 12, 4), false)
	_add(pm, _p("hat", 1, Vector3(0, 28, 0), Vector3(8, 8, 8), "extra", 0.5))
	_put("player_model", Vector2(8, 32), pm)
	var sk := _biped(Vector3(8, 8, 8), Vector3(8, 12, 4), Vector3(2, 12, 2), Vector3(2, 12, 2))
	_put("skeleton", Vector2(8, 32), sk)
	var ws := _biped(Vector3(9, 9, 9), Vector3(9, 14, 5), Vector3(2, 14, 2), Vector3(2, 14, 2))
	_put("wither_skeleton", Vector2(10, 37), ws)
	# villager family: tall head, nose, robe, crossed arms
	for vn in ["villager", "villager_zombie", "witch", "pillager", "vindicator", "evoker"]:
		var v := []
		_add(v, _p("torso", -1, Vector3(0, 18, 0), Vector3(8, 12, 6), "body"))
		_add(v, _p("robe", 0, Vector3(0, 15, 0), Vector3(8, 18, 6), "extra", 0.5))
		_add(v, _p("head", 0, Vector3(0, 29, 0), Vector3(8, 10, 8), "head"))
		_add(v, _p("snout", 2, Vector3(0, 26, -5), Vector3(2, 4, 2), "extra"))
		if vn == "pillager" or vn == "vindicator" or vn == "evoker" or vn == "villager_zombie":
			_add(v, _p("arm_right", 0, Vector3(6, 23.5, 0), Vector3(4, 12, 4), "limb", 0.0, true))
			_add(v, _p("arm_left", 0, Vector3(-6, 23.5, 0), Vector3(4, 12, 4), "limb", 0.0, true))
		else:
			_add(v, _p("arms", 0, Vector3(0, 19, -4), Vector3(8, 4, 4), "limb"))
		_add(v, _p("leg_fl", -1, Vector3(2, 12, 0), Vector3(4, 12, 4), "limb", 0.0, true))
		_add(v, _p("leg_fr", -1, Vector3(-2, 12, 0), Vector3(4, 12, 4), "limb", 0.0, true))
		if vn == "witch":
			_add(v, _p("hat", 2, Vector3(0, 35, 0), Vector3(10, 2, 10), "extra"))
			_add(v, _p("hat_top", 2, Vector3(0, 38, 1), Vector3(6, 4, 6), "extra"))
			_add(v, _p("hat_tip", 2, Vector3(0, 41, 2), Vector3(3, 3, 3), "extra"))
		_put(vn, Vector2(8, 34), v)
	# iron golem
	var ig := []
	_add(ig, _p("torso", -1, Vector3(0, 30, 0), Vector3(18, 12, 11), "body"))
	_add(ig, _p("waist", 0, Vector3(0, 21.5, 0), Vector3(9, 5, 6), "body"))
	_add(ig, _p("head", 0, Vector3(0, 40, -3), Vector3(8, 10, 8), "head"))
	_add(ig, _p("snout", 2, Vector3(0, 37, -8), Vector3(2, 4, 2), "extra"))
	_add(ig, _p("arm_right", 0, Vector3(11, 35.5, 0), Vector3(4, 30, 6), "limb", 0.0, true))
	_add(ig, _p("arm_left", 0, Vector3(-11, 35.5, 0), Vector3(4, 30, 6), "limb", 0.0, true))
	_add(ig, _p("leg_fl", -1, Vector3(4, 19, 0), Vector3(6, 19, 5), "limb", 0.0, true))
	_add(ig, _p("leg_fr", -1, Vector3(-4, 19, 0), Vector3(6, 19, 5), "limb", 0.0, true))
	_put("iron_golem", Vector2(22, 44), ig)
	# snow golem
	var sg := []
	_add(sg, _p("body", -1, Vector3(0, 5, 0), Vector3(12, 10, 12), "body"))
	_add(sg, _p("torso", 0, Vector3(0, 14, 0), Vector3(10, 8, 10), "body"))
	_add(sg, _p("head", 1, Vector3(0, 22, 0), Vector3(8, 8, 8), "head"))
	_add(sg, _p("arm_right", 1, Vector3(7, 16, 0), Vector3(1, 10, 1), "limb", 0.0, true, Vector3(0, 0, 60)))
	_add(sg, _p("arm_left", 1, Vector3(-7, 16, 0), Vector3(1, 10, 1), "limb", 0.0, true, Vector3(0, 0, -60)))
	_put("snow_golem", Vector2(12, 26), sg)
	# copper golem
	var cg := []
	_add(cg, _p("torso", -1, Vector3(0, 8, 0), Vector3(8, 6, 6), "body"))
	_add(cg, _p("head", 0, Vector3(0, 14, 0), Vector3(8, 6, 6), "head"))
	_add(cg, _p("snout", 1, Vector3(0, 13, -3.5), Vector3(2, 3, 1), "extra"))
	_add(cg, _p("antenna", 1, Vector3(0, 18.5, 0), Vector3(2, 3, 2), "extra"))
	_add(cg, _p("antenna_top", 1, Vector3(0, 21, 0), Vector3(4, 2, 4), "extra"))
	_add(cg, _p("arm_right", 0, Vector3(5, 10.5, 0), Vector3(2, 8, 2), "limb", 0.0, true))
	_add(cg, _p("arm_left", 0, Vector3(-5, 10.5, 0), Vector3(2, 8, 2), "limb", 0.0, true))
	_add(cg, _p("leg_fl", -1, Vector3(2, 5, 0), Vector3(3, 5, 4), "limb", 0.0, true))
	_add(cg, _p("leg_fr", -1, Vector3(-2, 5, 0), Vector3(3, 5, 4), "limb", 0.0, true))
	_put("copper_golem", Vector2(10, 22), cg)
	# piglins
	for pn in ["piglin", "piglin_brute", "zombified_piglin"]:
		var pg := _biped(Vector3(10, 8, 8), Vector3(8, 12, 4), Vector3(4, 12, 4), Vector3(4, 12, 4), pn == "zombified_piglin")
		_add(pg, _p("snout", 1, Vector3(0, 27, -5), Vector3(4, 4, 1), "extra"))
		_add(pg, _p("ear_1", 1, Vector3(5.5, 29, 0), Vector3(1, 5, 4), "extra", 0.0, false, Vector3(0, 0, -30)))
		_add(pg, _p("ear_2", 1, Vector3(-5.5, 29, 0), Vector3(1, 5, 4), "extra", 0.0, false, Vector3(0, 0, 30)))
		_put(pn, Vector2(10, 32), pg)
	# warden
	var wd := []
	_add(wd, _p("torso", -1, Vector3(0, 28, 0), Vector3(18, 21, 11), "body"))
	_add(wd, _p("head", 0, Vector3(0, 46, -1), Vector3(16, 16, 10), "head"))
	_add(wd, _p("horn_l", 1, Vector3(10, 52, -1), Vector3(4, 8, 1), "extra"))
	_add(wd, _p("horn_r", 1, Vector3(-10, 52, -1), Vector3(4, 8, 1), "extra"))
	_add(wd, _p("arm_right", 0, Vector3(11, 37, 0), Vector3(8, 28, 8), "limb", 0.0, true))
	_add(wd, _p("arm_left", 0, Vector3(-11, 37, 0), Vector3(8, 28, 8), "limb", 0.0, true))
	_add(wd, _p("leg_fl", -1, Vector3(5, 17, 0), Vector3(6, 17, 6), "limb", 0.0, true))
	_add(wd, _p("leg_fr", -1, Vector3(-5, 17, 0), Vector3(6, 17, 6), "limb", 0.0, true))
	_put("warden", Vector2(16, 48), wd)
	# creaking: tall, thin, bark
	var ck := _biped(Vector3(6, 10, 6), Vector3(6, 16, 5), Vector3(3, 22, 3), Vector3(3, 18, 3))
	_add(ck, _p("branch_1", 1, Vector3(4, 44, 0), Vector3(2, 6, 2), "extra", 0.0, false, Vector3(0, 0, -25)))
	_add(ck, _p("branch_2", 1, Vector3(-4, 45, 0), Vector3(2, 7, 2), "extra", 0.0, false, Vector3(0, 0, 25)))
	_put("creaking", Vector2(12, 44), ck)
	# breeze: head over swirling wind rods
	var br := []
	_add(br, _p("body", -1, Vector3(0, 18, 0), Vector3(4, 10, 4), "body"))
	_add(br, _p("head", 0, Vector3(0, 27, 0), Vector3(8, 8, 8), "head"))
	for i in 4:
		var a := i * PI * 0.5
		_add(br, _p("rod_%d" % (i + 1), 0, Vector3(cos(a) * 5.0, 14, sin(a) * 5.0), Vector3(2, 8, 2), "limb", 0.0, true))
	_add(br, _p("wind_1", 0, Vector3(0, 8, 0), Vector3(10, 4, 10), "extra"))
	_add(br, _p("wind_2", 0, Vector3(0, 3, 0), Vector3(6, 4, 6), "extra"))
	_put("breeze", Vector2(10, 30), br)


static func _hostile() -> void:
	# creeper: head, tall torso, four stubby legs
	var cr := []
	_add(cr, _p("torso", -1, Vector3(0, 12, 0), Vector3(8, 12, 4), "body"))
	_add(cr, _p("head", 0, Vector3(0, 22, 0), Vector3(8, 8, 8), "head"))
	_add(cr, _p("leg_fl", -1, Vector3(2, 6, -4), Vector3(4, 6, 4), "limb", 0.0, true))
	_add(cr, _p("leg_fr", -1, Vector3(-2, 6, -4), Vector3(4, 6, 4), "limb", 0.0, true))
	_add(cr, _p("leg_bl", -1, Vector3(2, 6, 4), Vector3(4, 6, 4), "limb", 0.0, true))
	_add(cr, _p("leg_br", -1, Vector3(-2, 6, 4), Vector3(4, 6, 4), "limb", 0.0, true))
	_put("creeper", Vector2(8, 26), cr)
	# spider: thorax + abdomen + head + eight splayed legs
	var sp := []
	_add(sp, _p("body", -1, Vector3(0, 9, 0), Vector3(6, 6, 6), "body"))
	_add(sp, _p("abdomen", 0, Vector3(0, 10, 9), Vector3(10, 8, 12), "body"))
	_add(sp, _p("head", 0, Vector3(0, 9, -7), Vector3(8, 8, 8), "head"))
	for i in 4:
		var z := -2.0 + i * 1.6
		var swing: float = [-40.0, -15.0, 15.0, 40.0][i]
		_add(sp, _p("leg_%d" % (i * 2 + 1), 0, Vector3(3, 9, z), Vector3(2, 16, 2), "limb", 0.0, true, Vector3(0, swing, 60)))
		_add(sp, _p("leg_%d" % (i * 2 + 2), 0, Vector3(-3, 9, z), Vector3(2, 16, 2), "limb", 0.0, true, Vector3(0, -swing, -60)))
	_put("spider", Vector2(22, 14), sp)
	# enderman: long limbs
	var en := []
	_add(en, _p("torso", -1, Vector3(0, 34, 0), Vector3(8, 12, 4), "body"))
	_add(en, _p("head", 0, Vector3(0, 44, 0), Vector3(8, 8, 8), "head"))
	_add(en, _p("arm_right", 0, Vector3(5, 39.5, 0), Vector3(2, 30, 2), "limb", 0.0, true))
	_add(en, _p("arm_left", 0, Vector3(-5, 39.5, 0), Vector3(2, 30, 2), "limb", 0.0, true))
	_add(en, _p("leg_fl", -1, Vector3(2, 28, 0), Vector3(2, 28, 2), "limb", 0.0, true))
	_add(en, _p("leg_fr", -1, Vector3(-2, 28, 0), Vector3(2, 28, 2), "limb", 0.0, true))
	_put("enderman", Vector2(8, 48), en)
	# slime: translucent outer cube + inner core with face
	var sl := []
	_add(sl, _p("body", -1, Vector3(0, 4, 0), Vector3(8, 8, 8), "extra"))
	_add(sl, _p("head", 0, Vector3(0, 4, 0), Vector3(6, 6, 6), "head"))
	_put("slime", Vector2(8, 8), sl)
	var mc := []
	_add(mc, _p("body", -1, Vector3(0, 4, 0), Vector3(8, 8, 8), "body"))
	_add(mc, _p("head", 0, Vector3(0, 4.5, -3.6), Vector3(6, 3, 1), "head"))
	_add(mc, _p("core", 0, Vector3(0, 4, 0), Vector3(4, 4, 4), "extra"))
	_put("magma_cube", Vector2(8, 8), mc)
	var su := []
	_add(su, _p("body", -1, Vector3(0, 5, 0), Vector3(10, 10, 10), "extra"))
	_add(su, _p("head", 0, Vector3(0, 5, 0), Vector3(7, 7, 7), "head"))
	_add(su, _p("held", 0, Vector3(0, 5, 0), Vector3(4, 4, 4), "extra"))
	_put("sulfur_cube", Vector2(10, 10), su)
	# phantom
	var ph := []
	_add(ph, _p("body", -1, Vector3(0, 4, 0), Vector3(5, 3, 9), "body"))
	_add(ph, _p("head", 0, Vector3(0, 4.5, -6.5), Vector3(7, 3, 5), "head"))
	var pwl := _add(ph, _p("wing_left_1", 0, Vector3(2.5, 5, 0), Vector3(1, 6, 9), "extra", 0.0, true, Vector3(0, 0, 90)))
	_add(ph, _p("wing_left_2", pwl, Vector3(8.5, 5, 0), Vector3(1, 13, 9), "extra", 0.0, true, Vector3(0, 0, 90)))
	var pwr := _add(ph, _p("wing_right_1", 0, Vector3(-2.5, 5, 0), Vector3(1, 6, 9), "extra", 0.0, true, Vector3(0, 0, -90)))
	_add(ph, _p("wing_right_2", pwr, Vector3(-8.5, 5, 0), Vector3(1, 13, 9), "extra", 0.0, true, Vector3(0, 0, -90)))
	_add(ph, _p("tail_1", 0, Vector3(0, 4, 6), Vector3(3, 2, 6), "body"))
	_add(ph, _p("tail_2", 6, Vector3(0, 4, 11), Vector3(1, 1, 6), "body"))
	_put("phantom", Vector2(14, 8), ph)
	# silverfish / endermite: segmented bug
	var sf := []
	_add(sf, _p("body", -1, Vector3(0, 2, 0), Vector3(6, 4, 3), "body"))
	_add(sf, _p("head", 0, Vector3(0, 1.5, -3), Vector3(4, 3, 2), "head"))
	_add(sf, _p("segment_1", 0, Vector3(0, 2, 3), Vector3(4, 3, 3), "body"))
	_add(sf, _p("segment_2", 2, Vector3(0, 1.5, 5.5), Vector3(3, 2, 2), "body"))
	_add(sf, _p("tail_1", 3, Vector3(0, 1, 7.5), Vector3(2, 1, 2), "body"))
	_put("silverfish", Vector2(6, 4), sf)
	# guardian: spiky box with a big eye and a tail
	var gd := []
	_add(gd, _p("body", -1, Vector3(0, 8, 0), Vector3(12, 12, 16), "body"))
	_add(gd, _p("head", 0, Vector3(0, 8, -8.5), Vector3(8, 8, 1), "head"))
	for i in 4:
		var sx := 1.0 if i % 2 == 0 else -1.0
		var sy := 1.0 if i < 2 else -1.0
		_add(gd, _p("spike_%d" % (i + 1), 0, Vector3(sx * 6.5, 8 + sy * 6.5, 0), Vector3(2, 2, 6), "extra"))
	var gtail := _add(gd, _p("tail_1", 0, Vector3(0, 8, 10), Vector3(4, 4, 6), "body"))
	_add(gd, _p("tail_2", gtail, Vector3(0, 8, 15), Vector3(3, 3, 6), "body"))
	_add(gd, _p("tail_3", gtail + 1, Vector3(0, 8, 20), Vector3(2, 2, 6), "extra"))
	_put("guardian", Vector2(14, 14), gd)
	# vex / allay: tiny winged spirits
	for vn in ["vex", "allay"]:
		var vx := []
		_add(vx, _p("torso", -1, Vector3(0, 7, 0), Vector3(4, 5, 2), "body"))
		_add(vx, _p("head", 0, Vector3(0, 12.5, 0), Vector3(5, 5, 5), "head"))
		_add(vx, _p("arm_right", 0, Vector3(2.5, 9.5, 0), Vector3(1, 4, 2), "limb", 0.0, true))
		_add(vx, _p("arm_left", 0, Vector3(-2.5, 9.5, 0), Vector3(1, 4, 2), "limb", 0.0, true))
		_add(vx, _p("wing_left", 0, Vector3(1, 9, 1.5), Vector3(8, 5, 1), "extra", 0.0, false, Vector3(0, -30, 0)))
		_add(vx, _p("wing_right", 0, Vector3(-1, 9, 1.5), Vector3(8, 5, 1), "extra", 0.0, false, Vector3(0, 30, 0)))
		_add(vx, _p("tail_1", 0, Vector3(0, 3, 0), Vector3(2, 4, 2), "body"))
		_put(vn, Vector2(6, 15), vx)
	# ravager: huge beast
	var rv := _quad(Vector3(14, 16, 26), Vector2(8, 16), Vector3(16, 18, 14), -4.0, 6.0)
	_add(rv, _p("horn_l", 1, Vector3(9, 40, -18), Vector3(2, 10, 3), "extra", 0.0, false, Vector3(-30, 0, 20)))
	_add(rv, _p("horn_r", 1, Vector3(-9, 40, -18), Vector3(2, 10, 3), "extra", 0.0, false, Vector3(-30, 0, -20)))
	_add(rv, _p("jaw", 1, Vector3(0, 22, -24), Vector3(16, 3, 10), "extra"))
	_put("ravager", Vector2(28, 38), rv)
	# hoglin / zoglin
	var hg := _quad(Vector3(16, 14, 22), Vector2(6, 10), Vector3(14, 10, 16), -2.0, 8.0)
	_add(hg, _p("tusk_1", 1, Vector3(7.5, 20, -26), Vector3(2, 6, 2), "extra"))
	_add(hg, _p("tusk_2", 1, Vector3(-7.5, 20, -26), Vector3(2, 6, 2), "extra"))
	_add(hg, _p("ear_1", 1, Vector3(8.5, 29, -15), Vector3(6, 1, 4), "extra"))
	_add(hg, _p("ear_2", 1, Vector3(-8.5, 29, -15), Vector3(6, 1, 4), "extra"))
	_add(hg, _p("mane", 0, Vector3(0, 26, -2), Vector3(2, 6, 14), "extra"))
	_put("hoglin", Vector2(22, 26), hg)


static func _nether() -> void:
	# ghast: big cube + face + hanging tentacles
	for gn in ["ghast", "happy_ghast"]:
		var big := 16.0 if gn == "ghast" else 20.0
		var g := []
		_add(g, _p("body", -1, Vector3(0, 14 + big * 0.5, 0), Vector3(big, big, big), "body"))
		_add(g, _p("head", 0, Vector3(0, 14 + big * 0.5, -big * 0.5 - 0.1), Vector3(big - 2, big - 4, 0.2), "head"))
		for i in 9:
			var ix := (i % 3) - 1
			var iz := (i / 3) - 1
			var ln := 9.0 + float((i * 7) % 5) * 1.5
			_add(g, _p("tentacle_%d" % (i + 1), 0, Vector3(ix * big * 0.3, 14, iz * big * 0.3), Vector3(2, ln, 2), "limb", 0.0, true))
		if gn == "happy_ghast":
			_add(g, _p("harness", 0, Vector3(0, 14 + big, 0), Vector3(big + 1, 2, big + 1), "extra"))
		_put(gn, Vector2(big, 14 + big), g)
	# blaze: floating head over a ring of rods
	var bl := []
	_add(bl, _p("body", -1, Vector3(0, 18, 0), Vector3(8, 8, 8), "head"))
	_add(bl, _p("head", 0, Vector3(0, 18, 0), Vector3(8, 8, 8), "head"))
	for i in 12:
		var ring := i / 4
		var a := (i % 4) * PI * 0.5 + ring * 0.4
		var r: float = [9.0, 7.0, 5.0][ring]
		var y: float = [16.0, 10.0, 4.0][ring]
		_add(bl, _p("rod_%d" % (i + 1), 0, Vector3(cos(a) * r, y, sin(a) * r), Vector3(2, 8, 2), "limb", 0.0, true))
	_put("blaze", Vector2(8, 24), bl)
	# strider
	var st := []
	_add(st, _p("body", -1, Vector3(0, 21, 0), Vector3(16, 14, 16), "body"))
	_add(st, _p("head", 0, Vector3(0, 21, -8.1), Vector3(12, 8, 0.2), "head"))
	for i in 3:
		_add(st, _p("hair_%d" % (i + 1), 0, Vector3(-5 + i * 5, 30, 0), Vector3(1, 6, 12), "extra"))
	_add(st, _p("leg_fl", -1, Vector3(4, 14, 0), Vector3(4, 14, 4), "limb", 0.0, true))
	_add(st, _p("leg_fr", -1, Vector3(-4, 14, 0), Vector3(4, 14, 4), "limb", 0.0, true))
	_put("strider", Vector2(16, 30), st)


static func _end_and_bosses() -> void:
	# shulker: base + lid + head inside
	var sh := []
	_add(sh, _p("body", -1, Vector3(0, 4, 0), Vector3(16, 8, 16), "body"))
	_add(sh, _p("lid", 0, Vector3(0, 10, 0), Vector3(16, 12, 16), "extra"))
	_add(sh, _p("head", 0, Vector3(0, 7, 0), Vector3(6, 6, 6), "head"))
	_put("shulker", Vector2(16, 16), sh)
	# ender dragon
	var dr := []
	var torso := _add(dr, _p("torso", -1, Vector3(0, 30, 0), Vector3(24, 24, 64), "body"))
	for i in 3:
		_add(dr, _p("spike_%d" % (i + 1), torso, Vector3(0, 44, -18 + i * 18), Vector3(2, 6, 12), "extra"))
	var prev := torso
	var z := -32.0
	for i in 5:
		z -= 10.0
		prev = _add(dr, _p("neck_%d" % (i + 1), prev, Vector3(0, 34 + i * 1.5, z), Vector3(10, 10, 10), "body"))
	var head := _add(dr, _p("head", prev, Vector3(0, 42, z - 14), Vector3(16, 16, 16), "head"))
	_add(dr, _p("snout", head, Vector3(0, 40, z - 28), Vector3(12, 5, 16), "head"))
	_add(dr, _p("jaw", head, Vector3(0, 35, z - 26), Vector3(12, 4, 16), "extra"))
	_add(dr, _p("horn_l", head, Vector3(5, 52, z - 10), Vector3(2, 4, 6), "extra"))
	_add(dr, _p("horn_r", head, Vector3(-5, 52, z - 10), Vector3(2, 4, 6), "extra"))
	prev = torso
	z = 32.0
	for i in 5:
		z += 10.0
		prev = _add(dr, _p("tail_%d" % (i + 1), prev, Vector3(0, 32 - i * 1.0, z), Vector3(10, 10, 10), "body"))
	# wings: an inner bone + membrane and a folding tip bone + membrane per side. Pivots are absolute
	# (model space, after the parent's rotation); children inherit the bone's 90 degree roll, so the
	# membranes lie flat and trail behind the bones.
	for side in [1.0, -1.0]:
		var nm := "left" if side > 0.0 else "right"
		var bone := _add(dr, _p("wing_%s_1" % nm, torso, Vector3(12 * side, 40, -12), Vector3(8, 56, 8), "limb", 0.0, true, Vector3(0, 0, 90 * side)))
		_add(dr, _p("wing_%s_skin_1" % nm, bone, Vector3(12 * side, 40, 20), Vector3(1, 56, 56), "extra", 0.0, true))
		var tip := _add(dr, _p("wing_%s_2" % nm, bone, Vector3(68 * side, 40, -12), Vector3(4, 56, 4), "limb", 0.0, true))
		_add(dr, _p("wing_%s_skin_2" % nm, tip, Vector3(68 * side, 40, 20), Vector3(1, 56, 56), "extra", 0.0, true))
	_add(dr, _p("leg_fl", torso, Vector3(12, 22, -20), Vector3(8, 22, 8), "limb", 0.0, true))
	_add(dr, _p("leg_fr", torso, Vector3(-12, 22, -20), Vector3(8, 22, 8), "limb", 0.0, true))
	_add(dr, _p("leg_bl", torso, Vector3(14, 24, 20), Vector3(10, 24, 10), "limb", 0.0, true))
	_add(dr, _p("leg_br", torso, Vector3(-14, 24, 20), Vector3(10, 24, 10), "limb", 0.0, true))
	_put("ender_dragon", Vector2(128, 64), dr)
	# wither: shoulder bar, three heads, spine and ribs
	var wt := []
	var bar := _add(wt, _p("body", -1, Vector3(0, 40, 0), Vector3(20, 3, 3), "body"))
	_add(wt, _p("head_centre", bar, Vector3(0, 46, 0), Vector3(8, 8, 8), "head"))
	_add(wt, _p("head_left", bar, Vector3(10, 44, 0), Vector3(6, 6, 6), "head"))
	_add(wt, _p("head_right", bar, Vector3(-10, 44, 0), Vector3(6, 6, 6), "head"))
	_add(wt, _p("spine", bar, Vector3(0, 30, 0), Vector3(3, 18, 3), "body"))
	for i in 3:
		_add(wt, _p("rib_%d" % (i + 1), bar, Vector3(0, 35 - i * 4, 0), Vector3(11, 2, 2), "extra"))
	_add(wt, _p("tail_1", bar, Vector3(0, 18, 2), Vector3(3, 8, 3), "body", 0.0, true, Vector3(-20, 0, 0)))
	_put("wither", Vector2(16, 56), wt)


static func _water() -> void:
	# squid / glow squid: tall body + 8 tentacles
	for sq in ["squid", "glow_squid"]:
		var s := []
		_add(s, _p("body", -1, Vector3(0, 14, 0), Vector3(12, 16, 12), "body"))
		_add(s, _p("head", 0, Vector3(0, 8, 0), Vector3(12, 4, 12), "head"))
		for i in 8:
			var a := i * PI / 4.0
			_add(s, _p("tentacle_%d" % (i + 1), 0, Vector3(cos(a) * 5.0, 6, sin(a) * 5.0), Vector3(2, 14, 2), "limb", 0.0, true))
		_put(sq, Vector2(12, 22), s)
	# dolphin
	var dl := []
	_add(dl, _p("body", -1, Vector3(0, 5, 0), Vector3(8, 7, 13), "body"))
	_add(dl, _p("head", 0, Vector3(0, 5, -9.5), Vector3(8, 7, 6), "head"))
	_add(dl, _p("snout", 1, Vector3(0, 3.5, -14), Vector3(2, 2, 4), "extra"))
	_add(dl, _p("fin_back", 0, Vector3(0, 10, 1), Vector3(1, 4, 5), "extra"))
	_add(dl, _p("fin_left", 0, Vector3(5, 3, -3), Vector3(3, 1, 4), "extra", 0.0, true))
	_add(dl, _p("fin_right", 0, Vector3(-5, 3, -3), Vector3(3, 1, 4), "extra", 0.0, true))
	_add(dl, _p("tail_1", 0, Vector3(0, 5, 9.5), Vector3(4, 5, 6), "body"))
	_add(dl, _p("tail_2", 6, Vector3(0, 5, 14), Vector3(10, 1, 4), "extra"))
	_put("dolphin", Vector2(14, 10), dl)
	# fish (cod / salmon / tropical / puffer / tadpole)
	var fs := []
	_add(fs, _p("body", -1, Vector3(0, 2.5, 0), Vector3(2, 4, 7), "body"))
	_add(fs, _p("head", 0, Vector3(0, 2.5, -4.5), Vector3(2, 3, 2), "head"))
	_add(fs, _p("fin_back", 0, Vector3(0, 5, 0), Vector3(1, 1, 6), "extra"))
	_add(fs, _p("tail", 0, Vector3(0, 2.5, 5), Vector3(1, 4, 3), "extra"))
	_put("fish", Vector2(4, 5), fs)
	# nautilus
	for nn in ["nautilus", "nautilus_zombie"]:
		var nt := []
		_add(nt, _p("body", -1, Vector3(0, 7, 2), Vector3(10, 12, 10), "body"))
		_add(nt, _p("shell_top", 0, Vector3(0, 13.5, 1), Vector3(8, 3, 12), "extra"))
		_add(nt, _p("head", 0, Vector3(0, 6, -4.5), Vector3(8, 6, 3), "head"))
		for i in 4:
			_add(nt, _p("tentacle_%d" % (i + 1), 0, Vector3(-3 + i * 2, 4, -6), Vector3(1, 6, 1), "limb", 0.0, true))
		_put(nn, Vector2(10, 14), nt)


static func _small() -> void:
	# bat
	var bt := []
	_add(bt, _p("body", -1, Vector3(0, 8, 0), Vector3(4, 6, 2), "body"))
	_add(bt, _p("head", 0, Vector3(0, 13, 0), Vector3(4, 4, 4), "head"))
	_add(bt, _p("ear_1", 1, Vector3(1.5, 16, 0), Vector3(1, 2, 1), "extra"))
	_add(bt, _p("ear_2", 1, Vector3(-1.5, 16, 0), Vector3(1, 2, 1), "extra"))
	_add(bt, _p("wing_left", 0, Vector3(2, 10, 0), Vector3(8, 6, 1), "extra", 0.0, false, Vector3(0, 0, 0)))
	_add(bt, _p("wing_right", 0, Vector3(-2, 10, 0), Vector3(8, 6, 1), "extra", 0.0, false, Vector3(0, 0, 0)))
	_put("bat", Vector2(8, 15), bt)
	# bee
	var be := []
	_add(be, _p("body", -1, Vector3(0, 5, 0), Vector3(7, 7, 10), "body"))
	_add(be, _p("head", 0, Vector3(0, 5, -5.1), Vector3(7, 7, 0.2), "head"))
	_add(be, _p("stinger", 0, Vector3(0, 4, 6), Vector3(1, 1, 2), "extra"))
	_add(be, _p("antenna_1", 0, Vector3(1.5, 10, -5), Vector3(1, 3, 1), "extra"))
	_add(be, _p("antenna_2", 0, Vector3(-1.5, 10, -5), Vector3(1, 3, 1), "extra"))
	_add(be, _p("wing_left", 0, Vector3(2, 9, -1), Vector3(6, 1, 5), "extra"))
	_add(be, _p("wing_right", 0, Vector3(-2, 9, -1), Vector3(6, 1, 5), "extra"))
	_add(be, _p("leg_fl", 0, Vector3(1.5, 1.5, -2), Vector3(1, 2, 1), "limb", 0.0, true))
	_add(be, _p("leg_fr", 0, Vector3(-1.5, 1.5, -2), Vector3(1, 2, 1), "limb", 0.0, true))
	_put("bee", Vector2(8, 10), be)
	# parrot
	var pr := []
	_add(pr, _p("body", -1, Vector3(0, 6, 0), Vector3(3, 6, 3), "body", 0.0, false, Vector3(15, 0, 0)))
	_add(pr, _p("head", 0, Vector3(0, 10.5, -1), Vector3(2, 3, 2), "head"))
	_add(pr, _p("beak", 1, Vector3(0, 10, -2.5), Vector3(1, 2, 1), "extra"))
	_add(pr, _p("crest", 1, Vector3(0, 13, 0), Vector3(1, 2, 3), "extra"))
	_add(pr, _p("wing_left", 0, Vector3(2, 8.5, 0), Vector3(1, 5, 3), "extra", 0.0, true))
	_add(pr, _p("wing_right", 0, Vector3(-2, 8.5, 0), Vector3(1, 5, 3), "extra", 0.0, true))
	_add(pr, _p("tail_1", 0, Vector3(0, 3, 2), Vector3(3, 4, 1), "extra", 0.0, true, Vector3(-30, 0, 0)))
	_add(pr, _p("leg_fl", -1, Vector3(1, 3, 0), Vector3(1, 3, 1), "limb", 0.0, true))
	_add(pr, _p("leg_fr", -1, Vector3(-1, 3, 0), Vector3(1, 3, 1), "limb", 0.0, true))
	_put("parrot", Vector2(6, 14), pr)
