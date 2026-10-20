#version 450

// Depth+normal G-buffer extraction pass, r_velocityBuffer OFF variant (1 color
// attachment: normal only). See gbuffer.vert and gbuffer_velocity.frag (the other
// variant, selected instead whenever r_velocityBuffer is on - see
// vk_create_shader_modules() in vk_shaders.cpp). Two variants instead of one
// fragment shader that always declares both outputs: the validation layer flags a
// write to a location with no matching subpass color attachment as an "unused
// write", so keeping the unused output out of the shader entirely avoids that
// noise instead of relying on the (functionally harmless) spec-legal discard.

layout(location = 0) in vec3 var_ViewNormal;

layout(location = 7) flat in float var_ShadowCaster;

layout(location = 0) out vec4 out_normal;

void main() {
	vec3 n = normalize(var_ViewNormal);
	// Alpha carries the stencil-shadow-caster flag; nothing else uses this channel.
	out_normal = vec4(n * 0.5 + 0.5, var_ShadowCaster);
}
