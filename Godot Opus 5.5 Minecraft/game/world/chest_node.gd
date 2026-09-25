extends Node3D
## Chest block-entity node: animates the lid toward open/closed (set by the session).

var open_target := 0.0
var open := 0.0


func _ready() -> void:
	if has_meta("offset"):
		position += get_meta("offset")


func set_open(o: bool) -> void:
	open_target = 1.0 if o else 0.0


func _process(delta: float) -> void:
	if absf(open - open_target) < 0.001:
		return
	open = move_toward(open, open_target, delta * 4.0)
	var lid: Node3D = get_meta("lid", null)
	if lid != null:
		var e := 1.0 - pow(1.0 - open, 3.0)
		lid.rotation.x = -e * PI * 0.5
