shader_type spatial;
render_mode unshaded, cull_back, depth_draw_opaque;
// Block geometry rendered as an entity (dropped items, held blocks, falling blocks, TNT, block entity models).
// Same vertex format as chunk meshes, but light comes from per-instance uniforms.

#include "res://game/world/shaders/voxel_common.gdshaderinc"

instance uniform vec2 ent_light = vec2(15.0, 0.0);
instance uniform vec4 ent_flash = vec4(1.0, 1.0, 1.0, 0.0);   // rgb + amount (TNT flash, hurt tint)
uniform bool use_vertex_light = false;
uniform float alpha_cut = 0.5;

void vertex() {
	VOXEL_VERTEX(UV, CUSTOM0)
	if (!use_vertex_light) {
		v_light = ent_light;
	}
}

void fragment() {
	vec4 t = voxel_sample();
	if (t.a < alpha_cut) {
		discard;
	}
	vec3 c = voxel_tint(vec4(t.rgb, 1.0), COLOR.rgb) * COLOR.a * voxel_lightmap(v_light.x, v_light.y);
	c = mix(c, ent_flash.rgb, ent_flash.a);
	ALBEDO = srgb_to_linear(c);
}
