shader_type spatial;
render_mode unshaded, cull_disabled, blend_add, depth_draw_never;
// Beacon beam: scrolling bright column.

void fragment() {
	float s = fract(UV.y * 40.0 - TIME * 0.6);
	vec3 c = vec3(0.75, 0.9, 1.0) * (0.55 + 0.45 * s);
	ALBEDO = c;
	ALPHA = 0.6;
}
