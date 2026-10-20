#version 450

// Fragment stage of the GPU shadow volume extrusion, used in place of color.frag when the
// G-buffer is available. Its only job is to keep an entity's own volume from shadowing
// that entity: it discards where the pixel already belongs to the caster, so the caster's
// own surfaces never accumulate stencil from its own silhouette - while every OTHER
// entity's volume still counts there.
//
// This is the reason the exclusion happens here and not in RB_ShadowFinish(): by the time
// that runs, every caster's volume has been accumulated into one stencil buffer and "whose
// shadow is this" no longer exists. At extrusion time the caster is still known.
//
// The identity comes from the alpha of the G-buffer normal attachment, which holds
// entityNum + 1 for stencil casters and 0 otherwise - see vkGBufferPushConstants_t.

layout(set = 1, binding = 0) uniform sampler2D gbufferNormal;

// Offset 88 keeps this clear of the geometry stage's own 84-byte range; the two are
// declared as separate push constant ranges so neither stage has to mirror the other's
// block. See vk_create_pipeline_layout() in vk_pipelines.cpp.
layout(push_constant) uniform ShadowSelf {
	layout(offset = 88) int casterId;
};

layout(location = 0) out vec4 out_color;

void main() {
	// texelFetch rather than a sampled lookup: this pass is at the same resolution as the
	// G-buffer, so the pixel is addressed directly and no filtering can bleed a
	// neighbouring entity's id into the comparison.
	float id = texelFetch(gbufferNormal, ivec2(gl_FragCoord.xy), 0).a;

	if (abs(id - float(casterId)) < 0.5) {
		discard;
	}

	out_color = vec4(0.0);
}
