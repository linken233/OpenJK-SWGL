#version 450

// Alpha-tested variant of gbuffer.frag: same output, but the fragment is discarded where
// the material is transparent. Without this a grate or a fence lays down a solid
// rectangle of depth, so GTAO treats it as a wall and the shadow of a hole is missing.
//
// One threshold, 0.5, rather than the four alphaFunc modes create_pipeline() supports for
// the main pass. That matches alphaFunc GE128, which is what essentially every alpha
// tested world surface uses; GT0 and GE192 materials are cut slightly differently here
// than they are in the main pass. Widening this means a spec constant and four times the
// pipelines, for a difference of a few pixels along a cut edge.

layout(set = 0, binding = 0) uniform sampler2D alphaTexture;

layout(location = 0) in vec3 var_ViewNormal;
layout(location = 1) in vec2 var_TexCoord;

layout(location = 7) flat in float var_ShadowCaster;

layout(location = 0) out vec4 out_normal;

void main() {
	if (texture(alphaTexture, var_TexCoord).a < 0.5) {
		discard;
	}

	vec3 n = normalize(var_ViewNormal);
	// Alpha carries the stencil-shadow-caster flag; nothing else uses this channel.
	out_normal = vec4(n * 0.5 + 0.5, var_ShadowCaster);
}
