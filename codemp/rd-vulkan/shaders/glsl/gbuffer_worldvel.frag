#version 450

// Pairs with gbuffer_worldvel.vert - see there for context. Computes a real
// screen-space motion vector from the current/previous clip-space positions,
// instead of gbuffer_velocity.frag's always-zero placeholder.

layout(location = 0) in vec3 var_ViewNormal;
layout(location = 1) in vec4 var_CurrClip;
layout(location = 2) in vec4 var_PrevClip;

layout(location = 7) flat in float var_ShadowCaster;

layout(location = 0) out vec4 out_normal;
layout(location = 1) out vec2 out_velocity;

void main() {
	vec3 n = normalize(var_ViewNormal);
	// Alpha carries the stencil-shadow-caster flag; nothing else uses this channel.
	out_normal = vec4(n * 0.5 + 0.5, var_ShadowCaster);

	vec2 currNDC = var_CurrClip.xy / var_CurrClip.w;
	vec2 prevNDC = var_PrevClip.xy / var_PrevClip.w;

	// NDC-space delta (each axis in [-2, 2]); left for the consumer to scale/bias
	// (e.g. by screen size, or to a [0, 1] storage range) however it needs.
	out_velocity = currNDC - prevNDC;
}
