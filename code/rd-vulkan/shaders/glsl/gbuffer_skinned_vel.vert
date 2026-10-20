#version 450

// Velocity-capable variant of gbuffer_skinned.vert - see that shader for the skinning
// and layout rationale, which is unchanged here. Selected instead of it for GPU-skinned
// Ghoul2 (SF_MDX with a VBO mesh) whenever r_velocityBuffer is on, and paired with
// gbuffer_worldvel.frag exactly like the rigid velocity path is.
//
// Deliberately still on vk.pipeline_layout and its 64-byte push constant range: the
// previous-frame camera transform arrives through the Camera UBO instead. A dedicated
// layout with a wider push range would not be push-constant compatible with
// vk.pipeline_layout, and vk_bind_descriptor_sets() binds with that one - which is the
// trap that produced the corruption on bolted Ghoul2 models. See vkUniformCamera_t.
//
// The motion vector is complete for this surface type: the vertex is skinned twice, once
// with this frame's bones and once with last frame's, and each result is placed with the
// matching frame's model->world transform and camera. So it carries all three terms -
// camera, the entity moving through the world, and the animation itself. CBoneCache owns
// that history and RB_TransformBones() rolls it forward; see vkUniformBones_t.

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
	mat3x4 u_PrevBoneMatrices[72];
	mat4   u_PrevModelMatrix;
};

layout(location = 0) in vec3 in_position;
layout(location = 5) in vec3 in_normal;
layout(location = 8) in uvec4 in_bones;
layout(location = 9) in vec4 in_weights;

// View space, matching every other gbuffer shader - see gbuffer_skinned.vert.
layout(location = 0) out vec3 var_ViewNormal;
layout(location = 7) flat out float var_ShadowCaster;
layout(location = 1) out vec4 var_CurrClip;
layout(location = 2) out vec4 var_PrevClip;

out gl_PerVertex {
	vec4 gl_Position;
};

mat4x3 ToBoneMatrix(mat3x4 bone)
{
	return mat4x3(
		bone[0].x, bone[1].x, bone[2].x,
		bone[0].y, bone[1].y, bone[2].y,
		bone[0].z, bone[1].z, bone[2].z,
		bone[0].w, bone[1].w, bone[2].w);
}

void main() {
	mat4x3 skin_matrix =
		ToBoneMatrix(u_BoneMatrices[in_bones.x]) * in_weights.x +
		ToBoneMatrix(u_BoneMatrices[in_bones.y]) * in_weights.y +
		ToBoneMatrix(u_BoneMatrices[in_bones.z]) * in_weights.z +
		ToBoneMatrix(u_BoneMatrices[in_bones.w]) * in_weights.w;

	mat4x3 prev_skin_matrix =
		ToBoneMatrix(u_PrevBoneMatrices[in_bones.x]) * in_weights.x +
		ToBoneMatrix(u_PrevBoneMatrices[in_bones.y]) * in_weights.y +
		ToBoneMatrix(u_PrevBoneMatrices[in_bones.z]) * in_weights.z +
		ToBoneMatrix(u_PrevBoneMatrices[in_bones.w]) * in_weights.w;

	vec3 position = skin_matrix * vec4(in_position, 1.0);
	vec3 prev_position = prev_skin_matrix * vec4(in_position, 1.0);
	vec3 normal = mat3(skin_matrix) * in_normal;

	gl_Position = mvp * vec4(position, 1.0);

	var_CurrClip = gl_Position;
	var_PrevClip = u_PrevViewProjection * u_PrevModelMatrix * vec4(prev_position, 1.0);
	var_ShadowCaster = u_SurfaceFlags.x;
	var_ViewNormal = mat3(u_ViewMatrix) * (mat3(u_ModelMatrix) * normal);
}
