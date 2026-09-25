class_name LightEngine
extends RefCounted
## Dedicated light thread (Minecraft-style flood fill for sky light and block light).
## It is the only writer of Chunk.light arrays. The main thread posts tasks (chunk added/removed,
## block changed) and polls for sections whose light changed so they can be re-meshed.
##
## Cells inside a task are addressed in a 3x3 chunk region around a centre chunk:
##   e = (lx + 16) | ((lz + 16) << 6) | (yr << 12)     lx, lz in [-16, 31], yr = y - min_y

const DX := [1, -1, 0, 0, 0, 0]
const DY := [0, 0, 1, -1, 0, 0]
const DZ := [0, 0, 0, 0, 1, -1]

var min_y := 0
var height := 256
var sections := 16
var has_sky := true

var _thread: Thread
var _mutex := Mutex.new()
var _sem := Semaphore.new()
var _tasks: Array = []
var _edits: Array = []
var _quit := false
var _busy := false
var _edit_streak := 0                # edits handled since the last chunk task (see _run)

var _out_mutex := Mutex.new()
var _out_dirty: Dictionary = {}     # Vector3i(cx, sy, cz) -> true
var _out_lit: Array = []            # Vector2i keys that finished initial lighting

var _chunks: Dictionary = {}        # light-thread registry: Vector2i -> Chunk
var _op: PackedByteArray
var _em: PackedByteArray

# per-task region state
var _C: Array = []                  # 9 Chunk or null
var _L: Array = []                  # 9 PackedByteArray (light) or null
var _B: Array = []                  # 9 PackedInt32Array (blocks)
var _ok := PackedByteArray()        # 1 = chunk present & writable
var _dm := PackedByteArray()        # dirty mask 9 * sections
var _bmin := PackedInt32Array()     # per region chunk: min yr holding block light
var _bmax := PackedInt32Array()
var _mark_center := true
var _ccx := 0
var _ccz := 0
var _outer_marks: Dictionary = {}


func setup(dim_min_y: int, dim_height: int, sky: bool) -> void:
	min_y = dim_min_y
	height = dim_height
	sections = dim_height >> 4
	has_sky = sky
	_op = BlockDB.light_opacity
	_em = BlockDB.light_emit


func start() -> void:
	_quit = false
	_thread = Thread.new()
	_thread.start(_run, Thread.PRIORITY_NORMAL)


func stop() -> void:
	if _thread == null:
		return
	_mutex.lock()
	_quit = true
	_mutex.unlock()
	_sem.post()
	_thread.wait_to_finish()
	_thread = null


func add_chunk(c: Chunk) -> void:
	_mutex.lock()
	_tasks.append([0, c])
	_mutex.unlock()
	_sem.post()


func remove_chunk(k: Vector2i) -> void:
	_mutex.lock()
	_tasks.append([1, k])
	_mutex.unlock()
	_sem.post()


func block_changed(x: int, y: int, z: int, old_v: int, new_v: int) -> void:
	_mutex.lock()
	_edits.append([2, x, y, z, old_v, new_v])
	_mutex.unlock()
	_sem.post()


func pending() -> int:
	_mutex.lock()
	var n := _tasks.size() + _edits.size() + (1 if _busy else 0)
	_mutex.unlock()
	return n


## Main thread: returns {dirty: Array[Vector3i], lit: Array[Vector2i]} and clears the outbox.
func poll() -> Dictionary:
	_out_mutex.lock()
	var d := _out_dirty.keys()
	var l := _out_lit.duplicate()
	_out_dirty.clear()
	_out_lit.clear()
	_out_mutex.unlock()
	return {"dirty": d, "lit": l}


func _run() -> void:
	while true:
		_sem.wait()
		_mutex.lock()
		if _quit:
			_mutex.unlock()
			return
		var task = null
		# block edits first (they are what the player sees), but a burst of edits (explosions,
		# /fill, structure placement) must not starve newly generated chunks of their light:
		# after every 32 edits one queued chunk task runs
		if not _edits.is_empty() and (_edit_streak < 32 or _tasks.is_empty()):
			task = _edits.pop_front()
			_edit_streak += 1
		elif not _tasks.is_empty():
			task = _tasks.pop_front()
			_edit_streak = 0
		_busy = task != null
		_mutex.unlock()
		if task == null:
			continue
		match int(task[0]):
			0:
				var c: Chunk = task[1]
				_chunks[c.key()] = c
				_initial(c)
			1:
				_chunks.erase(task[1])
			2:
				_edit(task[1], task[2], task[3], task[4], task[5])
		_mutex.lock()
		_busy = false
		_mutex.unlock()


# ------------------------------------------------------------------------------------------------
func _begin_region(ccx: int, ccz: int, mark_center: bool) -> bool:
	_ccx = ccx
	_ccz = ccz
	_mark_center = mark_center
	_C = []
	_L = []
	_B = []
	_C.resize(9)
	_L.resize(9)
	_B.resize(9)
	_bmin = PackedInt32Array()
	_bmin.resize(9)
	_bmin.fill(1 << 20)
	_bmax = PackedInt32Array()
	_bmax.resize(9)
	_bmax.fill(-1)
	_ok = PackedByteArray()
	_ok.resize(9)
	_dm = PackedByteArray()
	_dm.resize(9 * sections)
	_outer_marks = {}
	var center_ok := false
	for dz in 3:
		for dx in 3:
			var k := Vector2i(ccx + dx - 1, ccz + dz - 1)
			var c: Chunk = _chunks.get(k)
			var ci := dx + dz * 3
			if c != null and (c.lit or ci == 4):
				_C[ci] = c
				_bmin[ci] = c.blk_ymin
				_bmax[ci] = c.blk_ymax
				_L[ci] = c.light
				# a reference, not a copy: the main thread keeps editing blocks while this thread reads;
				# readers bounds-check because a concurrent copy-on-write can hand out a stale buffer
				_B[ci] = c.blocks
				_ok[ci] = 1
				if ci == 4:
					center_ok = true
	return center_ok


func _flush_region() -> void:
	for ci in 9:
		var c: Chunk = _C[ci]
		if c != null:
			c.blk_ymin = _bmin[ci]
			c.blk_ymax = _bmax[ci]
	_out_mutex.lock()
	for ci in 9:
		if ci == 4 and not _mark_center:
			continue
		var base := ci * sections
		var acx := _ccx + (ci % 3) - 1
		var acz := _ccz + (ci / 3) - 1
		for s in sections:
			if _dm[base + s] != 0:
				_out_dirty[Vector3i(acx, s, acz)] = true
	for k in _outer_marks:
		_out_dirty[k] = true
	_out_mutex.unlock()


func _mark(nx: int, nz: int, ny: int) -> void:
	var ci := (nx >> 4) + (nz >> 4) * 3
	var sy := ny >> 4
	var S := sections
	_dm[ci * S + sy] = 1
	var ly := ny & 15
	if ly == 0 and sy > 0:
		_dm[ci * S + sy - 1] = 1
	elif ly == 15 and sy < S - 1:
		_dm[ci * S + sy + 1] = 1
	var lx := nx & 15
	var lz := nz & 15
	var ddx := -1 if lx == 0 else (1 if lx == 15 else 0)
	var ddz := -1 if lz == 0 else (1 if lz == 15 else 0)
	if ddx != 0:
		_mark_chunk(nx + ddx, nz, sy)
	if ddz != 0:
		_mark_chunk(nx, nz + ddz, sy)
	if ddx != 0 and ddz != 0:
		_mark_chunk(nx + ddx, nz + ddz, sy)


func _mark_chunk(nx: int, nz: int, sy: int) -> void:
	if nx >= 0 and nx < 48 and nz >= 0 and nz < 48:
		_dm[((nx >> 4) + (nz >> 4) * 3) * sections + sy] = 1
	else:
		var acx := _ccx + (nx >> 4) - 1
		var acz := _ccz + (nz >> 4) - 1
		_outer_marks[Vector3i(acx, sy, acz)] = true


# ------------------------------------------------------------------------------------------------
func _initial(c: Chunk) -> void:
	var H := height
	var total := 256 * H
	var blocks := c.blocks
	var op := _op
	var light := PackedByteArray()
	var colH := PackedInt32Array()
	colH.resize(256)
	colH.fill(-1)
	if has_sky:
		var hmax := -1
		for col in 256:
			hmax = maxi(hmax, c.heightmap[col] - min_y)
		var top_yr := clampi(hmax + 1, 0, H)
		c.sky_top = top_yr
		light.resize(top_yr * 256)
		# everything above the highest terrain is fully sky-lit (kept dense so readers never go out of range)
		if top_yr < H:
			var upper := PackedByteArray()
			upper.resize((H - top_yr) * 256)
			upper.fill(0xF0)
			light.append_array(upper)
		for col in 256:
			var level := 15
			var yr := top_yr - 1
			var stop := -1
			while yr >= 0:
				var i := (yr << 8) | col
				var o := op[blocks[i] & 0xFFF]
				if o > 0:
					level -= o
					if level <= 0:
						stop = yr
						break
				light[i] = level << 4
				yr -= 1
			colH[col] = stop
	else:
		light.resize(total)
	c.light = light
	if not _begin_region(c.cx, c.cz, false):
		return
	var q := PackedInt32Array()
	# ---- sky seeds: exposure band against neighbouring columns
	if has_sky:
		for lz in 16:
			for lx in 16:
				var col := (lz << 4) | lx
				var own := colH[col]
				var band_top := own
				for d in 4:
					var nx: int = lx + DX[d] if d < 2 else lx
					var nz: int = lz if d < 2 else lz + DZ[d + 2]
					var nh := -1
					if nx >= 0 and nx < 16 and nz >= 0 and nz < 16:
						nh = colH[(nz << 4) | nx]
					else:
						var nk := Vector2i(c.cx + (1 if nx > 15 else (-1 if nx < 0 else 0)), c.cz + (1 if nz > 15 else (-1 if nz < 0 else 0)))
						var nc: Chunk = _chunks.get(nk)
						if nc == null:
							continue
						nh = nc.heightmap[((nz & 15) << 4) | (nx & 15)] - min_y
					band_top = maxi(band_top, nh)
				for yr in range(own + 1, mini(band_top + 1, H)):
					var i := (yr << 8) | col
					if (light[i] >> 4) > 1:
						q.append((lx + 16) | ((lz + 16) << 6) | (yr << 12))
		_pull_borders(q, true)
		_bfs_add(q, true)
	# ---- block light
	q = PackedInt32Array()
	var em := _em
	for li in c.emitters:
		if li >= blocks.size():
			continue
		var v := blocks[li]
		var e := em[((v & 0xFFF) << 4) | ((v >> 12) & 15)]
		if e > 0:
			light[li] = (light[li] & 0xF0) | e
			var eyr := li >> 8
			_bmin[4] = mini(_bmin[4], eyr)
			_bmax[4] = maxi(_bmax[4], eyr)
			q.append(((li & 15) + 16) | ((((li >> 4) & 15) + 16) << 6) | (eyr << 12))
	_pull_borders(q, false)
	_bfs_add(q, false)
	c.lit = true
	_flush_region()
	_out_mutex.lock()
	_out_lit.append(c.key())
	_out_mutex.unlock()


## Seeds centre-chunk border cells with light from lit neighbours.
func _pull_borders(q: PackedInt32Array, sky: bool) -> void:
	var la: PackedByteArray = _L[4]
	var ba: PackedInt32Array = _B[4]
	var op := _op
	var H := height
	for side in 4:
		var nci: int = [5, 3, 7, 1][side]
		if _ok[nci] == 0:
			continue
		var nla: PackedByteArray = _L[nci]
		var y0 := 0
		var y1 := H
		if sky:
			var nc0: Chunk = _C[nci]
			var cc0: Chunk = _C[4]
			y1 = mini(H, maxi(nc0.sky_top, cc0.sky_top) + 1)
		else:
			if _bmax[nci] < 0:
				continue
			y0 = maxi(0, _bmin[nci])
			y1 = mini(H, _bmax[nci] + 1)
		for t in 16:
			var lx: int
			var lz: int
			var nlx: int
			var nlz: int
			match side:
				0:
					lx = 15; lz = t; nlx = 0; nlz = t
				1:
					lx = 0; lz = t; nlx = 15; nlz = t
				2:
					lx = t; lz = 15; nlx = t; nlz = 0
				_:
					lx = t; lz = 0; nlx = t; nlz = 15
			var col := (lz << 4) | lx
			var ncol := (nlz << 4) | nlx
			for yr in range(y0, y1):
				var nv := nla[(yr << 8) | ncol]
				var nlev: int = (nv >> 4) if sky else (nv & 15)
				if nlev <= 1:
					continue
				var i := (yr << 8) | col
				if i >= ba.size():
					break
				var o := op[ba[i] & 0xFFF]
				if o >= 15:
					continue
				var want := nlev - maxi(1, o)
				var cur := la[i]
				var clev: int = (cur >> 4) if sky else (cur & 15)
				if want > clev:
					if sky:
						la[i] = (want << 4) | (cur & 15)
					else:
						la[i] = (cur & 0xF0) | want
						_bmin[4] = mini(_bmin[4], yr)
						_bmax[4] = maxi(_bmax[4], yr)
					q.append((lx + 16) | ((lz + 16) << 6) | (yr << 12))


## Breadth-first light increase from the queued cells across the 3x3 region.
func _bfs_add(q: PackedInt32Array, sky: bool) -> void:
	var op := _op
	var H := height
	var head := 0
	var mark_c := _mark_center
	while head < q.size():
		var e := q[head]
		head += 1
		var ex := e & 63
		var ez := (e >> 6) & 63
		var yr := e >> 12
		var ci := (ex >> 4) + (ez >> 4) * 3
		var la: PackedByteArray = _L[ci]
		var lv := la[(yr << 8) | ((ez & 15) << 4) | (ex & 15)]
		var lvl: int = (lv >> 4) if sky else (lv & 15)
		if lvl <= 1:
			continue
		for d in 6:
			var nx: int = ex + DX[d]
			var nz: int = ez + DZ[d]
			var ny: int = yr + DY[d]
			if nx < 0 or nx > 47 or nz < 0 or nz > 47 or ny < 0 or ny >= H:
				continue
			var nci := (nx >> 4) + (nz >> 4) * 3
			if _ok[nci] == 0:
				continue
			var nli := (ny << 8) | ((nz & 15) << 4) | (nx & 15)
			var nb: PackedInt32Array = _B[nci]
			if nli >= nb.size():
				continue      # neighbour array swapped by the main thread mid-task (see note in _begin_region)
			var o := op[nb[nli] & 0xFFF]
			if o >= 15:
				continue
			var nl := lvl - maxi(1, o)
			if sky and d == 3 and lvl == 15 and o == 0:
				nl = 15
			if nl <= 0:
				continue
			var nla: PackedByteArray = _L[nci]
			var cur := nla[nli]
			if sky:
				if (cur >> 4) >= nl:
					continue
				nla[nli] = (nl << 4) | (cur & 15)
			else:
				if (cur & 15) >= nl:
					continue
				nla[nli] = (cur & 0xF0) | nl
				if ny < _bmin[nci]:
					_bmin[nci] = ny
				if ny > _bmax[nci]:
					_bmax[nci] = ny
			if nci != 4 or mark_c:
				_mark(nx, nz, ny)
			q.append(nx | (nz << 6) | (ny << 12))


## Removal pass: clears light that depended on the removed source, returns re-propagation seeds.
func _bfs_remove(rq: PackedInt32Array, rl: PackedInt32Array, sky: bool) -> PackedInt32Array:
	var seeds := PackedInt32Array()
	var H := height
	var head := 0
	while head < rq.size():
		var e := rq[head]
		var lvl := rl[head]
		head += 1
		var ex := e & 63
		var ez := (e >> 6) & 63
		var yr := e >> 12
		for d in 6:
			var nx: int = ex + DX[d]
			var nz: int = ez + DZ[d]
			var ny: int = yr + DY[d]
			if nx < 0 or nx > 47 or nz < 0 or nz > 47 or ny < 0 or ny >= H:
				continue
			var nci := (nx >> 4) + (nz >> 4) * 3
			if _ok[nci] == 0:
				continue
			var nli := (ny << 8) | ((nz & 15) << 4) | (nx & 15)
			var nla: PackedByteArray = _L[nci]
			var cur := nla[nli]
			var nl: int = (cur >> 4) if sky else (cur & 15)
			if nl == 0:
				continue
			var ne := nx | (nz << 6) | (ny << 12)
			var dependent := nl < lvl or (sky and d == 3 and lvl == 15 and nl == 15)
			if dependent:
				if sky:
					nla[nli] = cur & 15
				else:
					nla[nli] = cur & 0xF0
					# neighbouring emitters must be re-seeded
					var nb: PackedInt32Array = _B[nci]
					var v := nb[nli] if nli < nb.size() else 0
					var emv := _em[((v & 0xFFF) << 4) | ((v >> 12) & 15)]
					if emv > 0:
						nla[nli] = (cur & 0xF0) | emv
						seeds.append(ne)
				_mark(nx, nz, ny)
				rq.append(ne)
				rl.append(nl)
			else:
				seeds.append(ne)
	return seeds


func _edit(x: int, y: int, z: int, old_v: int, new_v: int) -> void:
	var ccx := x >> 4
	var ccz := z >> 4
	var cc: Chunk = _chunks.get(Vector2i(ccx, ccz))
	if cc == null or not cc.lit:
		return
	if not _begin_region(ccx, ccz, true):
		return
	var yr := y - min_y
	if yr < 0 or yr >= height:
		return
	var lx := x & 15
	var lz := z & 15
	var e := (lx + 16) | ((lz + 16) << 6) | (yr << 12)
	var li := (yr << 8) | (lz << 4) | lx
	var la: PackedByteArray = _L[4]
	var op := _op
	var new_op := op[new_v & 0xFFF]
	var old_op := op[old_v & 0xFFF]
	var new_em := _em[((new_v & 0xFFF) << 4) | ((new_v >> 12) & 15)]
	_mark(lx + 16, lz + 16, yr)
	# ---------------- block light
	var cur := la[li]
	var old_bl := cur & 15
	var seeds := PackedInt32Array()
	if old_bl > 0 and (new_em < old_bl or new_op > old_op):
		la[li] = cur & 0xF0
		var rq := PackedInt32Array([e])
		var rl := PackedInt32Array([old_bl])
		seeds = _bfs_remove(rq, rl, false)
	if new_em > 0:
		la[li] = (la[li] & 0xF0) | maxi(new_em, la[li] & 15)
		_bmin[4] = mini(_bmin[4], yr)
		_bmax[4] = maxi(_bmax[4], yr)
		seeds.append(e)
	if new_op < 15:
		_append_neighbours(seeds, e)
	_bfs_add(seeds, false)
	# ---------------- sky light
	if has_sky:
		cur = la[li]
		var old_sky := cur >> 4
		seeds = PackedInt32Array()
		if new_op > old_op and old_sky > 0:
			la[li] = cur & 15
			var rq2 := PackedInt32Array([e])
			var rl2 := PackedInt32Array([old_sky])
			seeds = _bfs_remove(rq2, rl2, true)
		if new_op < 15:
			_append_neighbours(seeds, e)
		_bfs_add(seeds, true)
	# heightmap maintenance for the edited column
	cc.update_heightmap_column(lx, lz)
	_flush_region()


func _append_neighbours(seeds: PackedInt32Array, e: int) -> void:
	var ex := e & 63
	var ez := (e >> 6) & 63
	var yr := e >> 12
	for d in 6:
		var nx: int = ex + DX[d]
		var nz: int = ez + DZ[d]
		var ny: int = yr + DY[d]
		if nx < 0 or nx > 47 or nz < 0 or nz > 47 or ny < 0 or ny >= height:
			continue
		if _ok[(nx >> 4) + (nz >> 4) * 3] == 0:
			continue
		seeds.append(nx | (nz << 6) | (ny << 12))
