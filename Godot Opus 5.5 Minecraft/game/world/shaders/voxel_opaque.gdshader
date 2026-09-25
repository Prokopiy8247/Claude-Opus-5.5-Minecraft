shader_type spatial;
render_mode unshaded, cull_back, depth_draw_opaque;

#include "res://game/world/shaders/voxel_common.gdshaderinc"

void vertex() {
	VOXEL_VERTEX(UV, CUSTOM0)
}

void fragment() {
	vec4 t = voxel_sample();
	vec3 c = voxel_tint(t, COLOR.rgb) * COLOR.a * voxel_lightmap(v_light.x, v_light.y);
	ALBEDO = srgb_to_linear(c);
}
