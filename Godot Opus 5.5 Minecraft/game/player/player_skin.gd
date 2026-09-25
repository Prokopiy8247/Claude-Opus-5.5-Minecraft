class_name PlayerSkin
extends RefCounted
## Original default player skin (painted procedurally with the mob texture painter) and the
## third-person player rig.

static var _arm: Image = null


static func arm_image() -> Image:
	if _arm == null:
		_arm = Image.create_empty(16, 16, false, Image.FORMAT_RGBA8)
		MobTextures.paint(_arm, "player", "player", "limb", 4, 12, 4, "arm_right")
	return _arm


static func build_model() -> Node3D:
	var rig := MobRenderer.build("player", "player_model")
	return rig
