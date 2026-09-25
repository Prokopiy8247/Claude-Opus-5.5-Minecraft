shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_opaque;
// Dropped / displayed flat items (pixel-extruded sprites) with voxel light.

#include "res://game/world/shaders/voxel_common.gdshaderinc"

instance uniform vec2 ent_light = vec2(15.0, 0.0);
instance uniform vec4 ent_flash = vec4(1.0, 1.0, 1.0, 0.0);
uniform sampler2D albedo_tex : filter_nearest, repeat_disable;
uniform float glint = 0.0;

varying vec3 v_shade;

void vertex() {
	v_uv = UV;
	v_light = ent_light;
	v_layer = vec2(0.0, 1.0);
	v_shade = COLOR.rgb;
}

void fragment() {
	vec4 t = texture(albedo_tex, v_uv);
	if (t.a < 0.5) {
		discard;
	}
	vec3 c = t.rgb * v_shade * voxel_lightmap(v_light.x, v_light.y);
	if (glint > 0.0) {
		float g = sin((v_uv.x + v_uv.y) * 6.0 - TIME * 2.5) * 0.5 + 0.5;
		c += vec3(0.5, 0.25, 0.9) * g * g * 0.45 * glint;
	}
	c = mix(c, ent_flash.rgb, ent_flash.a);
	ALBEDO = srgb_to_linear(c);
}
