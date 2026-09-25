class_name MusicGen
extends RefCounted
## Generative calm ambient piano-like music (original, seeded). Produces a ~40-60 s AudioStreamWAV.

const SCALES := [[0, 2, 4, 7, 9], [0, 2, 3, 7, 8], [0, 4, 5, 7, 11], [0, 2, 5, 7, 9]]


static func piece(seed_v: int) -> AudioStreamWAV:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_v
	var rate := SoundSynth.RATE
	var dur := rng.randf_range(38.0, 55.0)
	var n := int(dur * rate)
	var out := PackedFloat32Array()
	out.resize(n)
	var scale: Array = SCALES[rng.randi_range(0, SCALES.size() - 1)]
	var root := 48 + rng.randi_range(-3, 4)
	var t := 1.0
	var beat := rng.randf_range(0.55, 0.85)
	var last_deg := 2
	while t < dur - 4.0:
		# melody note
		last_deg = clampi(last_deg + rng.randi_range(-2, 2), 0, 9)
		var midi: int = root + 12 + int(scale[last_deg % 5]) + 12 * (last_deg / 5)
		_note(out, rate, t, 440.0 * pow(2.0, (midi - 69) / 12.0), rng.randf_range(2.5, 4.0), 0.16)
		# occasional bass / chord
		if rng.randf() < 0.35:
			var bm: int = root + int(scale[rng.randi_range(0, 2)])
			_note(out, rate, t, 440.0 * pow(2.0, (bm - 69) / 12.0), 5.0, 0.12)
			if rng.randf() < 0.5:
				_note(out, rate, t + 0.02, 440.0 * pow(2.0, (bm + 7 - 69) / 12.0), 4.0, 0.08)
		t += beat * (1 + rng.randi_range(0, 3))
		if rng.randf() < 0.12:
			t += beat * 4.0
	return SoundSynth._stream(out)


static func _note(out: PackedFloat32Array, rate: int, start: float, freq: float, length: float, vol: float) -> void:
	var s0 := int(start * rate)
	var cnt := mini(int(length * rate), out.size() - s0)
	for i in cnt:
		var tt := float(i) / rate
		var env := exp(-tt * 1.6) * minf(1.0, tt * 200.0)
		var v := sin(tt * TAU * freq) + 0.35 * sin(tt * TAU * freq * 2.0) * exp(-tt * 3.0) + 0.1 * sin(tt * TAU * freq * 3.0) * exp(-tt * 5.0)
		out[s0 + i] += v * env * vol
