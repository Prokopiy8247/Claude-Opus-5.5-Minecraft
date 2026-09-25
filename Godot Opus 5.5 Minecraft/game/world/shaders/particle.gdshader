shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_opaque;
// Billboard particles sampling the block texture array (block fragments + generated particle sprites).
// INSTANCE_CUSTOM = (layer, u offset, v offset, uv scale); COLOR = tint rgb + alpha; transform scale = size.

#include "res://game/world/shaders/voxel_common.gdshaderinc"

varying vec4 p_custom;
varying vec4 p_color;

void vertex() {
	p_custom = INSTANCE_CUSTOM;
	p_color = COLOR;
	float size = length(MODEL_MATRIX[0].xyz);
	vec3 center = MODEL_MATRIX[3].xyz;
	vec3 right = INV_VIEW_MATRIX[0].xyz;
	vec3 up = INV_VIEW_MATRIX[1].xyz;
	vec3 wp = center + (right * VERTEX.x + up * VERTEX.y) * size;
	POSITION = PROJECTION_MATRIX * VIEW_MATRIX * vec4(wp, 1.0);
	v_uv = UV;
	v_light = vec2(15.0, 0.0);
}

void fragment() {
	vec2 uv = p_custom.yz + v_uv * p_custom.w;
	vec4 t = texture(blocks, vec3(uv, p_custom.x));
	if (t.a * p_color.a < 0.3) {
		discard;
	}
	vec3 c = t.rgb * p_color.rgb;
	ALBEDO = srgb_to_linear(c);
}
