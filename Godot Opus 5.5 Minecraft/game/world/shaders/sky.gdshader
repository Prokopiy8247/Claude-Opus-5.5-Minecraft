shader_type sky;
// Original blocky sky: gradient sky/fog colours driven by the time of day, square sun and moon,
// twinkling star field, sunrise/sunset glow, Nether/End variants (dim uniform).

uniform float day_time = 1000.0;          // 0..24000
uniform float rain = 0.0;                 // 0..1
uniform int dim = 0;                      // 0 overworld, 1 nether, 2 end
uniform vec3 day_top : source_color = vec3(0.47, 0.65, 1.0);
uniform vec3 day_horizon : source_color = vec3(0.75, 0.85, 1.0);
uniform vec3 night_top : source_color = vec3(0.0, 0.0, 0.02);
uniform vec3 night_horizon : source_color = vec3(0.03, 0.04, 0.08);
uniform vec3 sunset : source_color = vec3(0.95, 0.45, 0.2);

float hash13(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}

// Square billboard centred at dir d with half size s (in radians-ish on the sphere)
float square(vec3 eye, vec3 d, float s) {
	vec3 up = abs(d.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
	vec3 ax = normalize(cross(up, d));
	vec3 ay = cross(d, ax);
	float cd = dot(eye, d);
	if (cd <= 0.0) {
		return 0.0;
	}
	vec3 p = eye / cd - d;
	float u = dot(p, ax);
	float v = dot(p, ay);
	return step(abs(u), s) * step(abs(v), s);
}

void sky() {
	vec3 eye = EYEDIR;
	if (dim == 1) {
		COLOR = vec3(0.2, 0.03, 0.03);
	} else if (dim == 2) {
		// the End: dark purple haze with static noise
		float n = hash13(floor(eye * 180.0));
		// (linear values: this reads as a dim grey-violet on screen)
		COLOR = vec3(0.016, 0.011, 0.026) + vec3(0.018, 0.011, 0.028) * step(0.7, n) + vec3(0.008, 0.005, 0.012) * n;
	} else {
		float t = day_time / 24000.0;
		// celestial angle: 0 = sunrise (east), 0.25 = noon, 0.5 = sunset, 0.75 = midnight
		float ang = t * 6.2831853;
		vec3 sun_dir = normalize(vec3(cos(ang), sin(ang), 0.0));
		float day = clamp(sun_dir.y * 2.5 + 0.35, 0.0, 1.0);
		float h = clamp(eye.y, -1.0, 1.0);
		float g = pow(1.0 - max(h, 0.0), 2.5);
		vec3 top = mix(night_top, day_top, day);
		vec3 hor = mix(night_horizon, day_horizon, day);
		vec3 c = mix(top, hor, g);
		// sunrise / sunset glow toward the sun on the horizon
		float glow = clamp(1.0 - abs(sun_dir.y) * 4.0, 0.0, 1.0);
		float towards = pow(max(dot(normalize(vec3(eye.x, 0.0, eye.z)), normalize(vec3(sun_dir.x, 0.0, 0.0001))), 0.0), 3.0);
		c = mix(c, sunset, glow * towards * g * 0.85);
		// stars
		if (day < 0.6) {
			vec3 cell = floor(eye * 220.0);
			float s = hash13(cell);
			float tw = 0.6 + 0.4 * sin(TIME * 2.0 + s * 40.0);
			c += vec3(step(0.9965, s) * (0.6 - day) * tw * 1.6) * step(0.0, eye.y + 0.1);
		}
		// rain dims everything
		c = mix(c, vec3(dot(c, vec3(0.3, 0.59, 0.11))) * 0.6, rain * 0.75);
		// sun (square, bright) and moon (square, pale)
		float sun = square(eye, sun_dir, 0.085);
		c = mix(c, vec3(1.0, 0.95, 0.7) * 1.6, sun * (1.0 - rain));
		float sun_halo = square(eye, sun_dir, 0.14) * 0.25;
		c += vec3(1.0, 0.8, 0.5) * sun_halo * (1.0 - rain) * (1.0 - sun);
		float moon = square(eye, -sun_dir, 0.06);
		float moon_spot = square(eye, -sun_dir + vec3(0.0, 0.02, 0.02), 0.018);
		c = mix(c, vec3(0.86, 0.88, 0.95) - vec3(0.15) * moon_spot, moon * (1.0 - rain));
		COLOR = c;
	}
}
