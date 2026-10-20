#version 450

// Alpha-tested variant of gbuffer_worldvel.frag - see gbuffer_at.frag for why the cut
// exists and why the threshold is fixed at 0.5.

layout(set = 0, binding = 0) uniform sampler2D alphaTexture;

layout(location = 0) in vec3 var_ViewNormal;
layout(location = 1) in vec4 var_CurrClip;
layout(location = 2) in vec4 var_PrevClip;
layout(location = 3) in vec2 var_TexCoord;

layout(location = 7) flat in float var_ShadowCaster;

layout(location = 0) out vec4 out_normal;
layout(location = 1) out vec2 out_velocity;

void main() {
	if (texture(alphaTexture, var_TexCoord).a < 0.5) {
		discard;
	}

	vec3 n = normalize(var_ViewNormal);
	// Alpha carries the stencil-shadow-caster flag; nothing else uses this channel.
	out_normal = vec4(n * 0.5 + 0.5, var_ShadowCaster);

	vec2 currNDC = var_CurrClip.xy / var_CurrClip.w;
	vec2 prevNDC = var_PrevClip.xy / var_PrevClip.w;

	out_velocity = currNDC - prevNDC;
}
