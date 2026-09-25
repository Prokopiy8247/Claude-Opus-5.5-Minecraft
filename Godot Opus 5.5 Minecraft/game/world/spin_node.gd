extends Node3D
## Floating, slowly spinning child (enchanting table book).

var t := 0.0


func _process(delta: float) -> void:
	t += delta
	var book: Node3D = get_meta("book", null)
	if book != null:
		book.rotation.y = t * 0.8
		book.position.y = 1.05 + sin(t * 2.0) * 0.05
