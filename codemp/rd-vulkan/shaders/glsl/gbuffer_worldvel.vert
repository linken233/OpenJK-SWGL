#version 450

// Velocity-capable variant of the depth+normal(+velocity) G-buffer extraction pass.
// Used instead of gbuffer.vert for every rigid surface (world, brush models, MDV -
// both the immediate and the VBO vertex layouts, same position/normal interface)
// when r_velocityBuffer is on; see RB_RenderGBufferSurfList() in tr_backend.cpp.
// Ghoul2 (skinned) surfaces keep gbuffer_skinned.vert paired with
// gbuffer_velocity.frag's zero-motion placeholder: vk.pipeline_layout's push
// constant range is only 64 bytes, and correct skinned motion vectors would need
// previous-frame bone matrices as well.
//
// Needs a 3rd matrix (prevMvp) on top of gbuffer.vert's two, hence its own,
// larger push-constant layout (vk.pipeline_layout_gbuffer_velocity, see
// vkGBufferVelocityPushConstants_t) instead of reusing pipeline_layout_gbuffer.

layout(push_constant) uniform Transform {
	mat4 mvp;
	mat4 modelView;
	// proj_prev * view_prev * model_now: last displayed frame's camera transform
	// (vk_world.prevProjection * vk_world.prevWorldModelView) composed with THIS
	// frame's model matrix, applied to this frame's object-space vertex position -
	// i.e. where the vertex would have landed last frame had only the camera moved.
	// World geometry's model matrix is identity, so it degenerates to the plain
	// camera-only case there. Objects that moved on their own are not tracked yet.
	mat4 prevMvp;
	// x: 1 when this surface belongs to an entity that already casts a stencil shadow.
	// See vkGBufferPushConstants_t.
	vec4 surfaceFlags;
};

layout(location = 0) in vec3 in_position;
layout(location = 5) in vec3 in_normal;

layout(location = 0) out vec3 var_ViewNormal;
layout(location = 7) flat out float var_ShadowCaster;
layout(location = 1) out vec4 var_CurrClip;
layout(location = 2) out vec4 var_PrevClip;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = mvp * vec4(in_position, 1.0);

	var_CurrClip = gl_Position;
	var_PrevClip = prevMvp * vec4(in_position, 1.0);
	var_ShadowCaster = surfaceFlags.x;
	var_ViewNormal = mat3(modelView) * in_normal;
}
