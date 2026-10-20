#version 450

// Full-screen debug view of the G-buffer extraction pass (r_showGBuffer). Pairs with
// gamma.vert like every other post-process shader here, and is bound in place of the
// gamma blit at the end of the frame - so it replaces the image on screen rather than
// compositing over it. Purely a diagnostic: the depth mapping below is chosen for
// contrast, not for measuring anything.

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

// Matches r_showGBuffer: 1 = depth, 2 = normal, 3 = velocity.
layout(constant_id = 0) const int mode = 1;
// USE_REVERSED_DEPTH builds clear depth to 0 and put the near plane at 1.
layout(constant_id = 1) const int reversedDepth = 1;

void main() {
	vec4 t = texture(texture0, frag_tex_coord);

	if ( mode == 4 ) {
		// Single-channel visibility: 1 = fully open, 0 = fully occluded.
		out_color = vec4(vec3(t.r), 1.0);
	}
	else if ( mode == 5 ) {
		// G channel of the same attachment: contact shadow. 1 = lit, 0 = shadowed.
		out_color = vec4(vec3(t.g), 1.0);
	}
	else if ( mode == 2 ) {
		// Stored as n * 0.5 + 0.5, so an empty pixel reads mid-grey, +X leans red,
		// +Y green, +Z blue. View space throughout, Ghoul2 included.
		out_color = vec4(t.rgb, 1.0);
	}
	else if ( mode == 3 ) {
		// An NDC-space delta, a few thousandths in ordinary motion. Scaled hard, with
		// the sign split across channels: right/down green-ish, left/up blue-ish.
		vec2 v = t.rg * 32.0;
		out_color = vec4(max(v.x, 0.0) + max(v.y, 0.0),
						 max(v.x, 0.0),
						 max(-v.x, 0.0) + max(-v.y, 0.0), 1.0);
	}
	else {
		float d = t.r;

		if ( reversedDepth == 0 ) {
			d = 1.0 - d;
		}

		// d is hyperbolic, so almost everything in a normal view sits within a hair of
		// the near plane. The root spreads that range out enough to read shapes; the
		// cleared background stays black either way.
		out_color = vec4(vec3(pow(d, 0.25)), 1.0);
	}
}
