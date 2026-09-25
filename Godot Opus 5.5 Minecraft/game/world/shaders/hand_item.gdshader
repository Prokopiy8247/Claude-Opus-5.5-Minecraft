shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_opaque, skip_vertex_transform;
// First-person held item / arm: plain pixel texture, voxel light map, compressed depth range.

#include "res://game/world/shaders/voxel_common.gdshaderinc"

instance uniform vec2 ent_light = vec2(15.0, 0.0);
instance uniform vec4 ent_flash = vec4(1.0, 1.0, 1.0, 0.0);
uniform sampler2D albedo_tex : filter_nearest, repeat_disable;
uniform float hand_fov = 70.0;
uniform float glint = 0.0;

varying vec3 v_shade;

void vertex() {
	v_uv = UV;
	v_light = ent_light;
	v_layer = vec2(0.0, 1.0);
	v_shade = COLOR.rgb;
	vec4 view = MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
	VERTEX = view.xyz;
	// keep the engine projection (axis flips, aspect) and only swap its field of view for the
	// fixed viewmodel FOV so the hand does not stretch with sprint/zoom FOV changes
	float f = 1.0 / tan(radians(hand_fov) * 0.5);
	vec4 clip = PROJECTION_MATRIX * view;
	clip.xy *= f / max(abs(PROJECTION_MATRIX[1][1]), 0.0001);
	clip.z = clip.w - (clip.w - clip.z) * 0.02;
	POSITION = clip;
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
