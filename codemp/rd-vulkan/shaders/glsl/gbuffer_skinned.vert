#version 450

// Depth+normal(+velocity) G-buffer extraction pass, Ghoul2 (skinned) variant.
//
// Previous version of this shader read modelView from a dedicated push constant and
// used a separate, descriptor-set-incompatible pipeline layout with a raw
// vkCmdBindDescriptorSets bind for the Bones UBO (mirroring the GPU shadow volume
// pipeline's approach) - that produced a visible corruption on bolted/attached
// Ghoul2 models (weapons), root cause not fully confirmed. Rewritten to instead
// reuse the SAME uniform layout and binding mechanism (vk.pipeline_layout,
// vk_update_descriptor_offset()/vk_bind_descriptor_sets()) that the main renderer
// already uses correctly for every other Ghoul2 draw - see RB_RenderGBufferSurfList()
// in tr_backend.cpp. u_ModelMatrix comes from the same precomputed per-entity Entity
// UBO gen_vert.tmpl's USE_VBO_GHOUL2 path reads, instead of a push constant.

layout(push_constant) uniform Transform {
	mat4 mvp;
};

layout(set = 0, binding = 1) uniform Camera {
	vec4 u_ViewOrigin;
	mat4 u_PrevViewProjection;
	mat4 u_ViewMatrix;
};

layout(set = 0, binding = 2) uniform Entity {
	vec4 u_ambientLight;
	vec4 u_directedLight;
	vec4 u_LocalLightOrigin;
	vec4 u_ModelLightDir;
	vec4 u_localViewOrigin;
	mat4 u_ModelMatrix;
	// x: 1 when this entity already casts a stencil shadow volume - see vkUniformEntity_t.
	vec4 u_SurfaceFlags;
};

layout(set = 0, binding = 3) uniform Bones {
	mat3x4 u_BoneMatrices[72];
};

layout(location = 0) in vec3 in_position;
layout(location = 5) in vec3 in_normal;
layout(location = 8) in uvec4 in_bones;
layout(location = 9) in vec4 in_weights;

// View space, matching gbuffer.vert and gbuffer_worldvel.vert. The whole normal
// attachment has to hold one space: a consumer sampling it cannot tell which shader
// wrote a given pixel. u_ViewMatrix comes from the Camera UBO, which this shader was
// already free to bind - the old world-space output existed only because nothing here
// had reached for it yet.
layout(location = 0) out vec3 var_ViewNormal;
layout(location = 7) flat out float var_ShadowCaster;

out gl_PerVertex {
	vec4 gl_Position;
};

mat4x3 GetBoneMatrix(uint index)
{
	mat3x4 bone = u_BoneMatrices[index];
	return mat4x3(
		bone[0].x, bone[1].x, bone[2].x,
		bone[0].y, bone[1].y, bone[2].y,
		bone[0].z, bone[1].z, bone[2].z,
		bone[0].w, bone[1].w, bone[2].w);
}

void main() {
	mat4x3 skin_matrix =
		GetBoneMatrix(in_bones.x) * in_weights.x +
		GetBoneMatrix(in_bones.y) * in_weights.y +
		GetBoneMatrix(in_bones.z) * in_weights.z +
		GetBoneMatrix(in_bones.w) * in_weights.w;

	vec3 position = skin_matrix * vec4(in_position, 1.0);
	vec3 normal = mat3(skin_matrix) * in_normal;

	gl_Position = mvp * vec4(position, 1.0);
	var_ShadowCaster = u_SurfaceFlags.x;
	var_ViewNormal = mat3(u_ViewMatrix) * (mat3(u_ModelMatrix) * normal);
}
