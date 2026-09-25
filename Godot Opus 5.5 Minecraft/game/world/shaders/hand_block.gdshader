shader_type spatial;
render_mode unshaded, cull_back, depth_draw_opaque, skip_vertex_transform;
// First-person held block: voxel vertex format, drawn in a compressed depth range right in front
// of the near plane so it never clips into walls (single-pass "viewmodel" technique).

#include "res://game/world/shaders/voxel_common.gdshaderinc"

instance uniform vec2 ent_light = vec2(15.0, 0.0);
uniform float alpha_cut = 0.5;
uniform float hand_fov = 70.0;

void vertex() {
	VOXEL_VERTEX(UV, CUSTOM0)
	v_light = ent_light;
	vec4 view = MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
	VERTEX = view.xyz;
	NORMAL = normalize((MODELVIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
	// fixed field of view for the hand (like the original, sprint FOV does not stretch the hand)
	// keep the engine projection (axis flips, aspect) and only swap its field of view for the
	// fixed viewmodel FOV so the hand does not stretch with sprint/zoom FOV changes
	float f = 1.0 / tan(radians(hand_fov) * 0.5);
	vec4 clip = PROJECTION_MATRIX * view;
	clip.xy *= f / max(abs(PROJECTION_MATRIX[1][1]), 0.0001);
	// reversed-Z: squeeze the hand into the front 2% of the depth range
	clip.z = clip.w - (clip.w - clip.z) * 0.02;
	POSITION = clip;
}

void fragment() {
	vec4 t = voxel_sample();
	if (t.a < alpha_cut) {
		discard;
	}
	vec3 c = voxel_tint(vec4(t.rgb, 1.0), COLOR.rgb) * COLOR.a * voxel_lightmap(v_light.x, v_light.y);
	ALBEDO = srgb_to_linear(c);
}
