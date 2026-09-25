class_name SoundSynth
extends RefCounted
## Original procedural sound synthesis (no recorded or copied audio). Every sound is generated from
## noise, oscillators and envelopes into 16-bit mono AudioStreamWAV data.

const RATE := 22050


static func _stream(samples: PackedFloat32Array) -> AudioStreamWAV:
	var data := PackedByteArray()
	data.resize(samples.size() * 2)
	for i in samples.size():
		var v := clampi(int(samples[i] * 32000.0), -32767, 32767)
		data.encode_s16(i * 2, v)
	var s := AudioStreamWAV.new()
	s.format = AudioStreamWAV.FORMAT_16_BITS
	s.mix_rate = RATE
	s.stereo = false
	s.data = data
	return s


static func _env(t: float, a: float, d: float) -> float:
	if t < a:
		return t / a
	return exp(-(t - a) / d)


## Filtered noise burst. tone: 0 = dark/low, 1 = bright. grit adds crackle.
static func noise_burst(dur: float, tone: float, decay: float, seed_v: int, grit := 0.0, pitch := 1.0) -> AudioStreamWAV:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_v
	var n := int(dur * RATE)
	var out := PackedFloat32Array()
	out.resize(n)
	var lp := 0.0
	var lp2 := 0.0
	var alpha := lerpf(0.04, 0.6, tone) * pitch
	for i in n:
		var t := float(i) / RATE
		var w := rng.randf_range(-1.0, 1.0)
		if grit > 0.0 and rng.randf() < grit * 0.05:
			w *= 4.0
		lp += (w - lp) * alpha
		lp2 += (lp - lp2) * alpha
		var v := lerpf(lp2, lp, tone) * _env(t, 0.004, decay)
		out[i] = v * 2.2
	return _stream(out)


## Sine/square tone with pitch slide.
static func tone(dur: float, f0: float, f1: float, wave := 0, vol := 0.5, decay := 0.2, vib := 0.0) -> AudioStreamWAV:
	var n := int(dur * RATE)
	var out := PackedFloat32Array()
	out.resize(n)
	var ph := 0.0
	for i in n:
		var t := float(i) / RATE
		var f := lerpf(f0, f1, t / dur) * (1.0 + vib * sin(t * 30.0))
		ph += f / RATE
		var s := 0.0
		match wave:
			0: s = sin(ph * TAU)
			1: s = 1.0 if fmod(ph, 1.0) < 0.5 else -1.0
			2: s = 2.0 * absf(2.0 * fmod(ph, 1.0) - 1.0) - 1.0
			3: s = sin(ph * TAU) * 0.6 + sin(ph * TAU * 2.0) * 0.25 + sin(ph * TAU * 3.0) * 0.15
		out[i] = s * vol * _env(t, 0.005, decay)
	return _stream(out)


## Formant-ish grunt for mob voices: buzzy source through a moving band emphasis.
static func voice(dur: float, f0: float, f1: float, formant: float, rough: float, seed_v: int, vol := 0.6) -> AudioStreamWAV:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_v
	var n := int(dur * RATE)
	var out := PackedFloat32Array()
	out.resize(n)
	var ph := 0.0
	var bp1 := 0.0
	var bp2 := 0.0
	for i in n:
		var t := float(i) / RATE
		var f := lerpf(f0, f1, t / dur) * (1.0 + 0.03 * sin(t * 37.0))
		ph += f / RATE
		var src := (fmod(ph, 1.0) * 2.0 - 1.0) + rng.randf_range(-rough, rough)
		# two-pole resonator around the formant
		var w := TAU * formant / RATE
		var r := 0.94
		var y := src + 2.0 * r * cos(w) * bp1 - r * r * bp2
		bp2 = bp1
		bp1 = y
		out[i] = y * 0.06 * vol * _env(t, 0.03, dur * 0.45)
	return _stream(out)


static func explosion(seed_v: int) -> AudioStreamWAV:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_v
	var dur := 1.6
	var n := int(dur * RATE)
	var out := PackedFloat32Array()
	out.resize(n)
	var lp := 0.0
	var lp2 := 0.0
	for i in n:
		var t := float(i) / RATE
		var w := rng.randf_range(-1.0, 1.0)
		var a := lerpf(0.25, 0.02, clampf(t / 0.6, 0.0, 1.0))
		lp += (w - lp) * a
		lp2 += (lp - lp2) * 0.08
		var boom := sin(t * TAU * (55.0 - t * 20.0)) * exp(-t * 3.0) * 0.8
		out[i] = (lp * 1.8 + lp2 * 3.0 + boom) * _env(t, 0.002, 0.45)
	return _stream(out)


static func fuse(seed_v: int) -> AudioStreamWAV:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_v
	var dur := 1.2
	var n := int(dur * RATE)
	var out := PackedFloat32Array()
	out.resize(n)
	var hp := 0.0
	var prev := 0.0
	for i in n:
		var t := float(i) / RATE
		var w := rng.randf_range(-1.0, 1.0)
		hp = 0.9 * (hp + w - prev)
		prev = w
		out[i] = hp * 0.35 * (0.6 + 0.4 * sin(t * 50.0)) * minf(1.0, t * 8.0) * minf(1.0, (dur - t) * 4.0)
	return _stream(out)


static func chord(notes: Array, dur: float, vol := 0.3) -> AudioStreamWAV:
	var n := int(dur * RATE)
	var out := PackedFloat32Array()
	out.resize(n)
	for i in n:
		var t := float(i) / RATE
		var s := 0.0
		for k in notes.size():
			var f: float = notes[k]
			var st := float(k) * 0.06
			if t >= st:
				var tt := t - st
				s += (sin(tt * TAU * f) + 0.3 * sin(tt * TAU * f * 2.0)) * exp(-tt * 3.5)
		out[i] = s * vol / notes.size()
	return _stream(out)


static func pluck(freq: float, dur: float, vol := 0.35) -> AudioStreamWAV:
	# Karplus-Strong string
	var n := int(dur * RATE)
	var out := PackedFloat32Array()
	out.resize(n)
	var period := maxi(2, int(RATE / freq))
	var buf := PackedFloat32Array()
	buf.resize(period)
	var rng := RandomNumberGenerator.new()
	rng.seed = int(freq * 100)
	for i in period:
		buf[i] = rng.randf_range(-1.0, 1.0)
	var idx := 0
	for i in n:
		var a := buf[idx]
		var b := buf[(idx + 1) % period]
		buf[idx] = (a + b) * 0.498
		out[i] = a * vol
		idx = (idx + 1) % period
	return _stream(out)
