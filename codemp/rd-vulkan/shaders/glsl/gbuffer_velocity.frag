#version 450

// Depth+normal+velocity G-buffer extraction pass, r_velocityBuffer ON variant (2
// color attachments: normal + velocity). See gbuffer.vert and gbuffer.frag (the
// other variant, used instead whenever r_velocityBuffer is off).

layout(location = 0) in vec3 var_ViewNormal;

layout(location = 7) flat in float var_ShadowCaster;

layout(location = 0) out vec4 out_normal;
layout(location = 1) out vec2 out_velocity;

void main() {
	vec3 n = normalize(var_ViewNormal);
	// Alpha carries the stencil-shadow-caster id; nothing else uses this channel.
	out_normal = vec4(n * 0.5 + 0.5, var_ShadowCaster);

	// Unused at runtime as of the skinned velocity path: with r_velocityBuffer on, rigid
	// surfaces take gbuffer_worldvel.frag and Ghoul2 now does too (gbuffer_skinned_vel.vert).
	// Kept as the well-defined zero-motion fallback for any future gbuffer pipeline that
	// needs the two-attachment signature without a real motion vector.
	out_velocity = vec2(0.0);
}
