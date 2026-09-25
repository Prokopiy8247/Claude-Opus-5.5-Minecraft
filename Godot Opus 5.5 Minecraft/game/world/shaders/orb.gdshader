shader_type spatial;
render_mode unshaded, cull_disabled, blend_add, depth_draw_never;
// Experience orb: camera-facing pulsing green-yellow glow.

void vertex() {
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);
}

void fragment() {
	vec2 p = UV * 2.0 - 1.0;
	// blocky 8x8 orb shape
	vec2 q = floor((UV) * 8.0) / 8.0 * 2.0 - 1.0 + 0.125;
	float d = max(abs(q.x), abs(q.y)) * 0.6 + length(q) * 0.5;
	if (d > 0.95) {
		discard;
	}
	float pulse = sin(TIME * 6.0) * 0.5 + 0.5;
	vec3 c = mix(vec3(0.45, 1.0, 0.1), vec3(1.0, 1.0, 0.35), pulse * (1.0 - d));
	ALBEDO = c * (1.2 - d);
}
