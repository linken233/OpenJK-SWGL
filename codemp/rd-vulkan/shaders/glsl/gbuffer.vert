#version 450

// Depth+normal G-buffer extraction pass (r_depthPrepass). Dedicated, non-permuted
// pipeline (see vk_create_gbuffer_pipeline() in vk_pipelines.cpp) — deliberately
// independent from the big per-material gen_vert.tmpl system: this only ever needs
// position and normal, for opaque world (SF_FACE/SF_GRID/SF_TRIANGLES/SF_MDV) and
// rigid VBO model (SF_VBO_MDVMESH) surfaces. Ghoul2 (skinned) surfaces use
// gbuffer_skinned.vert instead; when r_velocityBuffer is on, these same surface types
// go through gbuffer_worldvel.vert. See RB_RenderGBufferSurfList() in tr_backend.cpp.

layout(push_constant) uniform Transform {
	// proj * modelview, identical to the value the main pass pushes for this
	// surface (see vk_get_mvp_transform()) so depth here matches the main pass.
	mat4 mvp;
	// modelview alone (no projection), used to bring the normal into view space.
	mat4 modelView;
	// x: 1 when this surface belongs to an entity that already casts a stencil shadow.
	// See vkGBufferPushConstants_t.
	vec4 surfaceFlags;
};

layout(location = 0) in vec3 in_position;
layout(location = 5) in vec3 in_normal;

layout(location = 0) out vec3 var_ViewNormal;
layout(location = 7) flat out float var_ShadowCaster;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = mvp * vec4(in_position, 1.0);

	// Translation doesn't apply to directions, so mat3() is enough here; no need
	// for a separate inverse-transpose normal matrix since this pass only ever
	// deals with rigid (rotation + uniform-ish scale) transforms.
	var_ShadowCaster = surfaceFlags.x;
	var_ViewNormal = mat3(modelView) * in_normal;
}
