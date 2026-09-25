// Shared voxel lighting (Minecraft-style light map).
global uniform float daylight;      // 0..1 sky light multiplier (day/night, weather)
global uniform vec3 sky_tint;       // colour of sky light
global uniform float ambient_light; // dimension ambient (nether/end > 0)
global uniform float gamma_boost;   // brightness setting 0..1
global uniform float night_vision;  // 0..1
global uniform float block_flicker; // subtle torch flicker multiplier

uniform sampler2DArray blocks : filter_nearest_mipmap, repeat_disable;

varying flat vec2 v_layer;   // x = layer, y = frames (negative = tint only marked texels)
varying vec2 v_uv;
varying vec2 v_light;        // x = sky 0..15, y = block 0..15

// Unpacks the chunk vertex format (UV carries layer/frames, CUSTOM0 carries sky/block light).
// A macro so the varyings are assigned inside vertex() (Godot forbids that in helper functions).
#define VOXEL_VERTEX(uv, custom0) { float _vl = floor((uv.x + 0.5) * 0.5); float _vf = floor((uv.y + 0.5) * 0.5); v_uv = vec2(uv.x - _vl * 2.0, uv.y - _vf * 2.0); v_layer = vec2(_vl, _vf); v_light = custom0.xy * 15.0; }

float mc_curve(float l) {
	l = clamp(l, 0.0, 1.0);
	return l / (4.0 - 3.0 * l);
}

vec3 voxel_lightmap(float sky, float blk) {
	float s = mc_curve(sky / 15.0);
	float b = mc_curve(blk / 15.0) * block_flicker;
	s = mix(s, 1.0 - pow(1.0 - s, 4.0), gamma_boost * 0.6);
	b = mix(b, 1.0 - pow(1.0 - b, 4.0), gamma_boost * 0.6);
	vec3 sc = vec3(s * daylight) * sky_tint;
	vec3 bc = vec3(b) * vec3(1.0, 0.9, 0.78);
	vec3 l = max(sc, bc);
	l = l + vec3(ambient_light) * (1.0 - l);
	l = max(l, vec3(0.035 + gamma_boost * 0.03));
	return max(l, vec3(night_vision));
}

vec4 voxel_sample() {
	float frames = abs(v_layer.y);
	float frame = 0.0;
	if (frames > 1.5) {
		frame = mod(floor(TIME * 10.0), frames);
	}
	return texture(blocks, vec3(v_uv, v_layer.x + frame));
}

vec3 voxel_tint(vec4 t, vec3 tint) {
	if (v_layer.y < 0.0) {
		// marked mode: only texels with alpha < 1 receive the tint (grass block side fringe)
		float m = step(t.a, 0.985);
		return t.rgb * mix(vec3(1.0), tint, m);
	}
	return t.rgb * tint;
}

vec3 srgb_to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}
