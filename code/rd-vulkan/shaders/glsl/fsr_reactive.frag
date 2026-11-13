#version 450

// FSR reactive mask on the raster path (vk_fsr_raster.cpp). Same rule as the SDK helper
// ffxDispatchDescUpscaleGenerateReactiveMask with the values of the AMD samples: tone map
// both colours, take the largest component of the difference, 0.9 above 0.2, else 0.
// The SDK helper is a second dispatch in the frame, and the SDK then frees its image views too soon.

layout(set = 0, binding = 0) uniform sampler2D opaqueColor;	// before the blended surfaces
layout(set = 1, binding = 0) uniform sampler2D finalColor;	// the complete scene

layout(location = 0) out vec4 out_reactive;

vec3 Tonemap(vec3 c)
{
	return c / (max(max(c.r, c.g), c.b) + 1.0);
}

void main()
{
	ivec2 p = ivec2(gl_FragCoord.xy);
	vec3 d = abs(Tonemap(texelFetch(finalColor, p, 0).rgb) - Tonemap(texelFetch(opaqueColor, p, 0).rgb));
	float r = max(max(d.r, d.g), d.b);

	out_reactive = vec4(r < 0.2 ? 0.0 : 0.9);
}
