#version 450

// Multiplies the AO buffer into the scene (r_ssao). Drawn as a full-screen quad INSIDE
// the main render pass, right after the opaque surfaces and before anything translucent -
// see RB_RenderDrawSurfList(). The darkening itself comes from the pipeline's blend
// factors (DST_COLOR, ZERO), so this shader only has to hand back the visibility term.
//
// Drawn inside the main pass rather than after it so translucent surfaces, the HUD and
// 2D never get multiplied. Anything the extraction pass skipped - sky above all - holds
// 1.0 in the AO buffer and so passes through untouched.

layout(set = 0, binding = 0) uniform sampler2D aoTexture;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

void main() {
	// R = ambient visibility, G = contact shadow visibility. Multiplied together here
	// rather than weighted separately: this renderer is forward-shaded and the ambient and
	// direct terms are already combined in the destination by the time we get to blend.
	vec2 terms = texture(aoTexture, frag_tex_coord).rg;
	float visibility = terms.r * terms.g;

	// Alpha stays 1: the blend only touches colour, and a 0 here would punch a hole in
	// destination alpha on the paths that keep it.
	out_color = vec4(visibility, visibility, visibility, 1.0);
}
