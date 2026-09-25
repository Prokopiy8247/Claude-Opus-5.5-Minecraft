shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_opaque;
// Camera-facing textured sprite (fireballs, special projectiles).

uniform sampler2D albedo_tex : filter_nearest, repeat_disable;

void vertex() {
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);
}

void fragment() {
	vec4 t = texture(albedo_tex, UV);
	if (t.a < 0.5) {
		discard;
	}
	ALBEDO = t.rgb;
}
