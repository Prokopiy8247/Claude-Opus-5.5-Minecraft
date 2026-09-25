shader_type spatial;
render_mode unshaded, cull_back, depth_draw_opaque;
// Mob / vehicle / item part material. Original pixel textures, voxel-style light map, hurt flash
// and per-instance light. Written from scratch for this project.

#include "res://game/world/shaders/voxel_common.gdshaderinc"

instance uniform vec2 ent_light = vec2(15.0, 0.0);
instance uniform vec4 ent_flash = vec4(1.0, 1.0, 1.0, 0.0);

uniform sampler2D albedo_tex : filter_nearest, repeat_disable;
uniform float cutout = 0.0;
uniform vec4 tint_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform float emission = 0.0;

void vertex() {
	v_uv = UV;
	v_light = ent_light;
	v_layer = vec2(0.0, 1.0);
}

void fragment() {
	vec4 t = texture(albedo_tex, v_uv) * tint_color;
	if (t.a < 0.35) {
		discard;
	}
	vec3 c = t.rgb * voxel_lightmap(v_light.x, v_light.y);
	c = mix(c, ent_flash.rgb, ent_flash.a);
	if (emission > 0.0) {
		c = mix(c, t.rgb, emission);
	}
	ALBEDO = srgb_to_linear(c);
}
