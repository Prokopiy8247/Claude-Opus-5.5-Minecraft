class_name Inventory
extends RefCounted
## Fixed-size container of ItemStacks. Player layout (46 slots):
##   0..8 hotbar, 9..35 main, 36 feet, 37 legs, 38 chest, 39 head, 40 offhand, 41..44 crafting grid, 45 craft result.

signal changed

const HOTBAR := 0
const MAIN := 9
const ARMOR := 36
const OFFHAND := 40
const CRAFT := 41

var slots: Array = []
var selected := 0


func _init(size: int = 27) -> void:
	slots.resize(size)


func size() -> int:
	return slots.size()


func stack_at(i: int) -> ItemStack:
	if i < 0 or i >= slots.size():
		return null
	var st: ItemStack = slots[i]
	if st != null and st.is_empty():
		slots[i] = null
		return null
	return st


func set_stack(i: int, s: ItemStack) -> void:
	if i < 0 or i >= slots.size():
		return
	slots[i] = s if (s != null and not s.is_empty()) else null
	changed.emit()


func selected_stack() -> ItemStack:
	return stack_at(selected)


func clear() -> void:
	for i in slots.size():
		slots[i] = null
	changed.emit()


## Adds a stack into [from, to) slots. Returns the remainder (or null when everything fit).
func add(stack: ItemStack, from: int = 0, to: int = -1) -> ItemStack:
	if stack == null or stack.is_empty():
		return null
	if to < 0:
		to = 36 if slots.size() >= 46 else slots.size()
	var s := stack.copy()
	for i in range(from, to):
		var cur := stack_at(i)
		if cur != null and cur.can_merge(s):
			var room := cur.max_stack() - cur.count
			if room > 0:
				var n := mini(room, s.count)
				cur.count += n
				s.count -= n
				if s.count <= 0:
					changed.emit()
					return null
	for i in range(from, to):
		if stack_at(i) == null:
			var n2 := mini(s.max_stack(), s.count)
			slots[i] = ItemStack.new(s.id, n2, s.damage, s.data.duplicate(true))
			s.count -= n2
			if s.count <= 0:
				changed.emit()
				return null
	changed.emit()
	return s


func can_fit(stack: ItemStack, from: int = 0, to: int = -1) -> bool:
	if to < 0:
		to = 36 if slots.size() >= 46 else slots.size()
	var left := stack.count
	for i in range(from, to):
		var cur := stack_at(i)
		if cur == null:
			left -= stack.max_stack()
		elif cur.can_merge(stack):
			left -= cur.max_stack() - cur.count
		if left <= 0:
			return true
	return false


func count_item(item_id: int) -> int:
	var n := 0
	for i in slots.size():
		var s := stack_at(i)
		if s != null and s.id == item_id:
			n += s.count
	return n


## Removes up to n items of an id (main inventory only for players). Returns removed count.
func remove_item(item_id: int, n: int, to: int = -1) -> int:
	var left := n
	var end := slots.size() if to < 0 else to
	for i in end:
		var s := stack_at(i)
		if s != null and s.id == item_id:
			var take := mini(left, s.count)
			s.count -= take
			left -= take
			if s.count <= 0:
				slots[i] = null
			if left <= 0:
				break
	changed.emit()
	return n - left


func find_item(item_id: int, to: int = -1) -> int:
	var end := slots.size() if to < 0 else to
	for i in end:
		var s := stack_at(i)
		if s != null and s.id == item_id:
			return i
	return -1


func first_empty(from: int = 0, to: int = -1) -> int:
	var end := slots.size() if to < 0 else to
	for i in range(from, end):
		if stack_at(i) == null:
			return i
	return -1


func to_array() -> Array:
	var out := []
	for i in slots.size():
		var s := stack_at(i)
		out.append(s.to_dict() if s != null else {})
	return out


func from_array(a: Array) -> void:
	for i in mini(a.size(), slots.size()):
		slots[i] = ItemStack.from_dict(a[i]) if a[i] is Dictionary else null
	changed.emit()
