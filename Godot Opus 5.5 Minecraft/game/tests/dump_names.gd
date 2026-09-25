extends SceneTree
## Dumps all block and item names (used to validate recipe/loot references).
func _init() -> void:
	BlockDB.init_registry()
	ItemDB.init()
	var f := FileAccess.open("user://names.txt", FileAccess.WRITE)
	for d in BlockDB.defs:
		f.store_line("B " + d.name)
	for it in ItemDB.defs:
		f.store_line("I " + it.name)
	f.close()
	print("dumped ", BlockDB.count, " blocks ", ItemDB.count, " items to ", ProjectSettings.globalize_path("user://names.txt"))
	quit()
