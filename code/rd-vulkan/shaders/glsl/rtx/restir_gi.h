// ========================================================================== //
// ReSTIR GI for the first diffuse bounce.
//
// The bounce pass of a diffuse ray makes one candidate: the point that the
// bounce ray hits and the radiance Lo that it sends back to the primary surface.
// The candidate and the reservoir of the previous frame go into one RIS step.
// The selected sample gives the LF contribution of the pixel, and the reservoir
// stays in the images for the next frame.
//
// Estimator. The tracer without ReSTIR adds T * Lo for a direction w that the
// sampler draws with the solid angle density pdf_s(w). T is the throughput of
// the primary surface. The expectation is the integral of h(w) = T * Lo * pdf_s(w).
// With the target p_hat(w) = luminance(Lo) * pdf_s(w), a candidate has the
// weight p_hat / pdf_s = luminance(Lo). The reservoir selects a sample y and has
//   W = w_sum / (M * p_hat(y)),
// and the contribution is h(y) * W = T * Lo * pdf_s(y) * W. Without reuse
// M = 1 and W = 1 / pdf_s(y), so the contribution is T * Lo as before.
//
// A candidate that hits a dynamic surface (model, mover, deformed world) cannot be reused: its point
// does not stay in place. The class of the hit is a property of the candidate, so the reservoir chain
// holds static hits only. A dynamic candidate enters the RIS with weight 0 (it counts in M, it is never
// selected or stored) and adds its own T * Lo directly. The expectation is the sum of the two parts.
//
// Spatial reuse (pt_restir_gi 2, one bounce ray). The bounce pass stores the reservoir after the temporal
// step, and the spatial pass (restir_gi_spatial.rgen) shades it. The spatial pass resamples the reservoir
// of the pixel with those of random neighbours of the same frame, and stores nothing: the spatial result
// does not feed the next frame. The temporal chain stays as with pt_restir_gi 1, so the spatial result
// cannot feed back into itself with a growing M.
// A neighbour q has the solid angle measure of its own primary point x_q. The map of a sample at x_s to the
// receiver x_r changes the solid angle by the Jacobian
//   J = d_omega_r / d_omega_q = (cos(phi_r) / cos(phi_q)) * (|x_q - x_s|^2 / |x_r - x_s|^2),
// with phi the angle at x_s between its normal n_s and the direction to the primary point. The weight of a
// neighbour sample is p_hat_r(y) * W_q * M_q * J.
//
// The includer has to include path_tracer_rgen.h before this file.
// ========================================================================== //

#ifndef  _RESTIR_GI_H_
#define  _RESTIR_GI_H_

#include "restir_common.h"
#include "checkerboard.glsl"

#define RESTIR_GI_FLAG_SKY			1u		// pos is a unit direction: the sample is the sky
#define RESTIR_GI_FLAG_DYNAMIC		2u		// the sample point is on a model, a mover or a deformed surface: the candidate is never stored

#define RESTIR_GI_M_MASK			0xffffu	// bits of the reservoir meta word: M, then the age, then the flags
#define RESTIR_GI_AGE_SHIFT			16u
#define RESTIR_GI_AGE_MASK			0xffu
#define RESTIR_GI_FLAGS_SHIFT		24u

#define RESTIR_GI_RAY_BIT			0x10000u	// in RAD y above the half of lo.b: the pixel traced a diffuse ray this frame
#define RESTIR_GI_MAX_SPATIAL_SAMPLES	8u		// upper limit of pt_restir_gi_spatial_samples, bounds the RNG indices
#define RESTIR_GI_JACOBIAN_LIMIT	10.0		// a neighbour sample with J outside [1 / limit, limit] is rejected

#define RESTIR_GI_ORIGIN_OFFSET		0.01	// moves the visibility ray off the primary surface, as the bounce ray does
#define RESTIR_GI_RAY_LENGTH		10000.0	// length of the visibility ray of a sky sample, as the bounce ray

struct GiReservoir
{
	// Persisted between frames
	vec3 pos;		// the sample point; for a sky sample the unit direction
	vec3 normal;	// the normal of the sample point, on the side of the ray that hit it
	vec3 lo;		// the radiance that the sample point sends to the primary surface, without its throughput
	float W;		// the unbiased contribution weight
	uint M;			// number of candidates that the sample stands for
	uint age;		// frames since the sample was a candidate
	uint flags;		// RESTIR_GI_FLAG_*

	// Not persisted
	float w_sum;	// sum of the weights of the candidates so far
	float p_hat;	// the target function of the selected sample
};

// The data of the primary surface that the resampling needs.
struct GiPrimary
{
	ivec2 ipos;
	vec3 position;
	vec3 view_direction;		// from the camera to the surface
	vec3 bounce_direction;		// the direction that the sampler drew for the candidate
	vec3 normal;
	vec3 basis_normal;			// the normal that the direction sampler used
	vec3 throughput;			// of the primary surface, with 1 / (1 - specular_pdf) but without the Fresnel term
	vec3 base_reflectivity;
	float specular_factor;
	int shadow_cull_mask;
	bool is_gradient;

	// Spatial pass only
	float view_depth;
	uint object;				// restir_object() of the pixel
	bool split_surface;			// the pixel belongs to a surface that both checkerboard fields show
};

void
init_gi_reservoir(out GiReservoir r)
{
	r.pos = vec3(0);
	r.normal = vec3(0, 0, 1);
	r.lo = vec3(0);
	r.W = 0.0;
	r.M = 0u;
	r.age = 0u;
	r.flags = 0u;
	r.w_sum = 0.0;
	r.p_hat = 0.0;
}

// A candidate for a ray that left the primary surface in the given direction. It is
// a sky sample without radiance. The caller sets lo, and pos, normal and flags for a surface hit.
GiReservoir
restir_gi_candidate(vec3 direction)
{
	GiReservoir r;
	init_gi_reservoir(r);
	r.pos = direction;
	r.M = 1u;
	r.flags = RESTIR_GI_FLAG_SKY;

	return r;
}

// The Fresnel term of the diffuse ray: the light that the specular lobe reflects does not go to the diffuse lobe.
// V points from the surface to the camera, L to the sample.
vec3
diffuse_fresnel(vec3 L, vec3 V, vec3 base_reflectivity, float specular_factor)
{
	vec3 H = normalize(V + L);
	float VoH = max(0, dot(V, H));

	return schlick_fresnel(base_reflectivity, VoH, specular_factor);
}

// The sampler of the first diffuse bounce makes the directions for the SH store with another density.
bool
restir_gi_sh_sampling()
{
#if ENABLE_SH
	return (global_ubo.pt_denoiser_flags & DENOISER_FLAG_LF_SH) != 0;
#else
	return false;
#endif
}

// The normal that the direction sampler of the first diffuse bounce uses.
vec3
restir_gi_basis_normal(vec3 normal, vec3 geo_normal)
{
	return restir_gi_sh_sampling() ? geo_normal : normal;
}

// The probability that the first bounce ray of a pixel is a specular ray.
float
first_bounce_specular_pdf(float metallic, float roughness)
{
	float fake_specular_weight = smoothstep(
		global_ubo.pt_fake_roughness_threshold,
		global_ubo.pt_fake_roughness_threshold + 0.1,
		roughness);

	return (metallic == 1 && fake_specular_weight == 0) ? 1.0 : 0.5;
}

// The instance flag that a bounce ray or a shadow ray of a primary surface sets so that it sees the
// weapon of the viewer or the models of the viewer.
int
get_viewer_cull_flags(uint material_id)
{
	bool primary_is_weapon = (material_id & MATERIAL_FLAG_WEAPON) != 0;

	if(global_ubo.first_person_model != 0 && !primary_is_weapon)
		return AS_FLAG_VIEWER_MODELS;

	return AS_FLAG_VIEWER_WEAPON;
}

// The spatial pass shades the reservoirs. It needs the G-buffer position of the first bounce, and the
// second bounce pass replaces it.
bool
restir_gi_spatial_enabled()
{
	return global_ubo.pt_restir_gi >= 2.0 && global_ubo.pt_num_bounce_rays == 1.0;
}

// The cap of M for a reservoir that comes from another pixel or frame.
uint
restir_gi_m_clamp()
{
	return uint(clamp(global_ubo.pt_restir_gi_m_clamp, 1.0, float(RESTIR_GI_M_MASK)));
}

// Adds the LF contribution of a diffuse ray to the LF value of a pixel: flat, or SH with the direction.
void
accumulate_lf(inout SH low_freq, vec3 contrib, vec3 direction)
{
	contrib *= STORAGE_SCALE_LF;

#if ENABLE_SH
	if(!restir_gi_sh_sampling())
		low_freq.shY.xyz += contrib;
	else
		accumulate_SH(low_freq, irradiance_to_SH(contrib, direction), 1.0);
#else
	low_freq.shY.xyz += contrib;
#endif
}

// ========================================================================== //
// Storage
// ========================================================================== //

// diffuse_ray marks a pixel that the spatial pass has to shade, also when it has no sample.
void
restir_gi_store_invalid(ivec2 ipos, bool diffuse_ray)
{
	imageStore(IMG_PT_RESTIR_GI_RAD_A, ipos, uvec4(0, diffuse_ray ? RESTIR_GI_RAY_BIT : 0u, 0, 0));
}

// POS_A: pos and the encoded normal. RAD_A: lo as two halves in x and y, the meta word in z, W in w.
// A reservoir without weight is stored as invalid: the next frame does not read its POS_A.
void
restir_gi_store(ivec2 ipos, GiReservoir r, bool diffuse_ray)
{
	if(!(r.W > 0.0) || r.M == 0u)
	{
		restir_gi_store_invalid(ipos, diffuse_ray);
		return;
	}

	imageStore(IMG_PT_RESTIR_GI_POS_A, ipos, vec4(r.pos, uintBitsToFloat(encode_normal(r.normal))));

	uvec4 rad;
	rad.x = packHalf2x16(r.lo.rg);
	rad.y = packHalf2x16(vec2(r.lo.b, 0.0)) | (diffuse_ray ? RESTIR_GI_RAY_BIT : 0u);
	rad.z = min(r.M, RESTIR_GI_M_MASK)
		| (min(r.age, RESTIR_GI_AGE_MASK) << RESTIR_GI_AGE_SHIFT)
		| (r.flags << RESTIR_GI_FLAGS_SHIFT);
	rad.w = floatBitsToUint(r.W);
	imageStore(IMG_PT_RESTIR_GI_RAD_A, ipos, rad);
}

// Reads the reservoir of a pixel of the previous frame, or of the current frame when current is true.
// False when it holds no sample.
bool
restir_gi_load(ivec2 pos, bool current, out GiReservoir r)
{
	init_gi_reservoir(r);

	uvec4 rad = current ? texelFetch(TEX_PT_RESTIR_GI_RAD_A, pos, 0) : texelFetch(TEX_PT_RESTIR_GI_RAD_B, pos, 0);
	r.W = uintBitsToFloat(rad.w);
	r.M = rad.z & RESTIR_GI_M_MASK;

	if(r.M == 0u || !(r.W > 0.0) || isinf(r.W))
		return false;

	vec4 pos_normal = current ? texelFetch(TEX_PT_RESTIR_GI_POS_A, pos, 0) : texelFetch(TEX_PT_RESTIR_GI_POS_B, pos, 0);
	r.pos = pos_normal.xyz;
	r.normal = decode_normal(floatBitsToUint(pos_normal.w));
	r.lo = vec3(unpackHalf2x16(rad.x), unpackHalf2x16(rad.y).x);
	r.age = (rad.z >> RESTIR_GI_AGE_SHIFT) & RESTIR_GI_AGE_MASK;
	r.flags = rad.z >> RESTIR_GI_FLAGS_SHIFT;

	return true;
}

// ========================================================================== //
// Sample, target function and shading
// ========================================================================== //

// The direction from the primary surface to the sample.
vec3
restir_gi_direction(GiReservoir s, vec3 position)
{
	return (s.flags & RESTIR_GI_FLAG_SKY) != 0u ? s.pos : normalize(s.pos - position);
}

// The solid angle density of the direction sampler in indirect_lighting.rgen. It makes a point of
// a disk with the radius u^power, u uniform, and lifts it to the hemisphere: with the density
// r^(1 / power - 2) / (2 pi power) in the disk, and cos(theta) for the solid angle.
// The power is 0.5 for the cosine sampler, and HEMISPHERE_UNIFORMISH for the SH store.
float
restir_gi_source_pdf(vec3 direction, vec3 basis_normal)
{
	float cos_theta = dot(direction, basis_normal);

	if(!(cos_theta > 0.0))
		return 0.0;

	if(restir_gi_sh_sampling())
	{
		float radius = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

		return cos_theta * pow(radius, 1.0 / HEMISPHERE_UNIFORMISH - 2.0) / (2.0 * M_PI * HEMISPHERE_UNIFORMISH);
	}

	return cos_theta / M_PI;
}

// The target function of the sample for the primary surface at position.
float
restir_gi_target(GiReservoir s, vec3 position, vec3 basis_normal)
{
	return luminance(s.lo) * restir_gi_source_pdf(restir_gi_direction(s, position), basis_normal);
}

// Copies the selected sample, not the sums.
void
restir_gi_take(inout GiReservoir r, GiReservoir s, float p_hat)
{
	r.pos = s.pos;
	r.normal = s.normal;
	r.lo = s.lo;
	r.age = s.age;
	r.flags = s.flags;
	r.p_hat = p_hat;
}

// The LF contribution of a reservoir for a direction from the primary surface to its sample.
vec3
restir_gi_shade(GiReservoir r, GiPrimary p, vec3 direction)
{
	float pdf = restir_gi_source_pdf(direction, p.basis_normal);
	vec3 F = diffuse_fresnel(direction, -p.view_direction, p.base_reflectivity, p.specular_factor);

	return p.throughput * (vec3(1.0) - F) * r.lo * (pdf * r.W);
}

// 1 when nothing blocks the way from the primary surface to the sample.
float
restir_gi_visibility(GiReservoir s, GiPrimary p)
{
	vec3 origin = p.position - p.view_direction * RESTIR_GI_ORIGIN_OFFSET;
	vec3 target = (s.flags & RESTIR_GI_FLAG_SKY) != 0u ? origin + s.pos * RESTIR_GI_RAY_LENGTH : s.pos;

	return trace_shadow_ray(get_shadow_ray(origin, target, 0.0), p.shadow_cull_mask);
}

// ========================================================================== //
// Temporal reuse
// ========================================================================== //

// The reservoir of the previous frame for the surface of pixel ipos. False when there is none
// that fits: no history, other surface, other object, or too old.
bool
restir_gi_load_temporal(ivec2 ipos, vec3 normal, out GiReservoir prev)
{
	init_gi_reservoir(prev);

	if(global_ubo.restir_gi_history_valid == 0 || global_ubo.pt_restir_gi_max_age < 1.0)
		return false;

	vec4 motion = texelFetch(TEX_PT_MOTION, ipos, 0);
	float view_depth = texelFetch(TEX_PT_VIEW_DEPTH_A, ipos, 0).x;
	uint object = restir_object(texelFetch(TEX_PT_VISBUF_PRIM_A, ipos, 0).x, false);

	int field_left, field_right;
	restir_get_prev_field(ipos, field_left, field_right);
	ivec2 pos_prev = restir_get_prev_pos(ipos, motion);

	if(pos_prev.x < field_left || pos_prev.x >= field_right || pos_prev.y < 0 || pos_prev.y >= global_ubo.prev_height)
		return false;

	if(!restir_prev_is_same_surface(pos_prev, view_depth, normal, object, true))
		return false;

	if(!restir_gi_load(pos_prev, false, prev))
		return false;

	uint max_age = uint(min(global_ubo.pt_restir_gi_max_age, float(RESTIR_GI_AGE_MASK)));

	if(prev.age + 1u > max_age)
		return false;

	prev.M = min(prev.M, restir_gi_m_clamp());

	return true;
}

// A pixel that traces no diffuse ray this frame passes the reservoir of the previous frame on.
void
restir_gi_carry(ivec2 ipos, vec3 normal)
{
	GiReservoir prev;

	if(restir_gi_load_temporal(ipos, normal, prev))
	{
		prev.age += 1u;
		restir_gi_store(ipos, prev, false);
	}
	else
		restir_gi_store_invalid(ipos, false);
}

// Resamples the candidate with the reservoir of the previous frame, stores the result for the
// next frame, and returns the LF contribution of the reservoir with its direction. The contribution of a
// dynamic candidate comes in extra_contrib with its own direction, for the SH store. The pixels that the A-SVGF re-shades as gradient
// samples take the candidate only: they have to repeat the sampling of the previous frame.
vec3
restir_gi_resolve(GiPrimary p, GiReservoir candidate, out vec3 direction, out vec3 extra_contrib, out vec3 extra_direction)
{
	GiReservoir r;
	init_gi_reservoir(r);

	float rng = get_rng(RNG_RESTIR_GI_SELECTION);

	// The weight of a candidate is p_hat / pdf of the sampler that made it. The candidate keeps the
	// direction that was drawn: the direction to its hit point differs by the ray origin offset.
	float pdf = restir_gi_source_pdf(p.bounce_direction, p.basis_normal);
	float p_hat = luminance(candidate.lo) * pdf;
	bool dynamic = (candidate.flags & RESTIR_GI_FLAG_DYNAMIC) != 0u;
	float w = (pdf > 0.0 && !dynamic) ? p_hat / pdf : 0.0;

	extra_contrib = vec3(0);
	extra_direction = p.bounce_direction;

	if(dynamic)
	{
		vec3 F = diffuse_fresnel(p.bounce_direction, -p.view_direction, p.base_reflectivity, p.specular_factor);
		extra_contrib = p.throughput * (vec3(1.0) - F) * candidate.lo;
	}

	if(ris_select(r.w_sum, r.M, w, candidate.M, rng))
		restir_gi_take(r, candidate, p_hat);

	bool from_previous = false;
	GiReservoir prev;

	if(!p.is_gradient && restir_gi_load_temporal(p.ipos, p.normal, prev))
	{
		float p_hat_prev = restir_gi_target(prev, p.position, p.basis_normal);
		float w_prev = p_hat_prev * prev.W * float(prev.M);

		if(isnan(w_prev) || isinf(w_prev))
			w_prev = 0.0;

		if(ris_select(r.w_sum, r.M, w_prev, prev.M, rng))
		{
			restir_gi_take(r, prev, p_hat_prev);
			r.age = prev.age + 1u;
			from_previous = true;
		}
	}

	if(r.p_hat > 0.0)
		r.W = r.w_sum / (float(r.M) * r.p_hat);

	if(isnan(r.W) || isinf(r.W))
		r.W = 0.0;

	// A sample of the previous frame has to be visible from this pixel.
	if(from_previous && r.W > 0.0 && restir_gi_visibility(r, p) == 0.0)
		r.W = 0.0;

	restir_gi_store(p.ipos, r, true);

	direction = from_previous ? restir_gi_direction(r, p.position) : p.bounce_direction;

	// The spatial pass shades the reservoir.
	if(restir_gi_spatial_enabled())
		return vec3(0);

	return r.W > 0.0 ? restir_gi_shade(r, p, direction) : vec3(0);
}

// ========================================================================== //
// Spatial reuse
// ========================================================================== //

// The factor that maps the solid angle of a sample at the primary point x_q to the primary point x_r.
// 0 when the sample is not on the visible side of the surface from x_q or x_r.
float
restir_gi_jacobian(GiReservoir s, vec3 x_q, vec3 x_r)
{
	if((s.flags & RESTIR_GI_FLAG_SKY) != 0u)
		return 1.0;

	vec3 to_q = x_q - s.pos;
	vec3 to_r = x_r - s.pos;
	float dist2_q = dot(to_q, to_q);
	float dist2_r = dot(to_r, to_r);
	float cos_q = dot(s.normal, to_q) * inversesqrt(dist2_q);
	float cos_r = dot(s.normal, to_r) * inversesqrt(dist2_r);

	if(!(cos_q > 0.0 && cos_r > 0.0))
		return 0.0;

	return (cos_r * dist2_q) / (cos_q * dist2_r);
}

// Resamples the reservoir of the pixel, as the bounce pass left it in the images of this frame, with the
// reservoirs of random neighbours. Returns the LF contribution and its direction. A neighbour has to
// show the same surface and the same object, and its sample must give a Jacobian in the limits.
vec3
restir_gi_spatial(GiPrimary p, out vec3 direction)
{
	GiReservoir r;
	init_gi_reservoir(r);

	float rng = get_rng(RNG_RESTIR_GI_SPATIAL_SELECTION);
	bool from_neighbour = false;

	GiReservoir own;
	if(restir_gi_load(p.ipos, true, own))
	{
		float p_hat_own = restir_gi_target(own, p.position, p.basis_normal);
		float w_own = p_hat_own * own.W * float(own.M);

		if(isnan(w_own) || isinf(w_own))
			w_own = 0.0;

		if(ris_select(r.w_sum, r.M, w_own, own.M, rng))
			restir_gi_take(r, own, p_hat_own);
	}

	int field_left, field_right;
	restir_get_curr_field(p.ipos, field_left, field_right);

	// A surface that one field shows can take neighbours from both fields.
	bool across_fields = !p.split_surface && global_ubo.current_gpu_slice_width == global_ubo.width;
	ivec2 flat_pos = checker_to_flat(p.ipos, global_ubo.width);

	uint samples = min(uint(max(global_ubo.pt_restir_gi_spatial_samples, 0.0)), RESTIR_GI_MAX_SPATIAL_SAMPLES);
	float radius = max(global_ubo.pt_restir_gi_spatial_radius, 1.0);
	uint m_clamp = restir_gi_m_clamp();

	for(uint i = 0u; i < samples; i++)
	{
		vec2 uv = vec2(get_rng(RNG_RESTIR_GI_SPATIAL_X(i)), get_rng(RNG_RESTIR_GI_SPATIAL_Y(i)));
		ivec2 offset = ivec2(round(sample_disk(uv) * radius));

		if(all(equal(offset, ivec2(0))))
			continue;

		ivec2 pos;
		if(across_fields)
		{
			ivec2 flat_neighbour = flat_pos + offset;

			if(flat_neighbour.x < 0 || flat_neighbour.x >= global_ubo.width || flat_neighbour.y < 0 || flat_neighbour.y >= global_ubo.height)
				continue;

			pos = flat_to_checker(flat_neighbour, global_ubo.width);
		}
		else
		{
			pos = p.ipos + offset;

			if(pos.x < field_left || pos.x >= field_right || pos.y < 0 || pos.y >= global_ubo.height)
				continue;
		}

		GiReservoir q;
		if(!restir_gi_load(pos, true, q))
			continue;

		if(!restir_curr_is_same_surface(pos, p.view_depth, p.normal, p.object))
			continue;

		float J = restir_gi_jacobian(q, texelFetch(TEX_PT_SHADING_POSITION, pos, 0).xyz, p.position);

		if(!(J >= 1.0 / RESTIR_GI_JACOBIAN_LIMIT && J <= RESTIR_GI_JACOBIAN_LIMIT))
			continue;

		q.M = min(q.M, m_clamp);

		float p_hat_q = restir_gi_target(q, p.position, p.basis_normal);
		float w_q = p_hat_q * q.W * float(q.M) * J;

		if(isnan(w_q) || isinf(w_q))
			w_q = 0.0;

		if(ris_select(r.w_sum, r.M, w_q, q.M, rng))
		{
			restir_gi_take(r, q, p_hat_q);
			from_neighbour = true;
		}
	}

	if(r.p_hat > 0.0)
		r.W = r.w_sum / (float(r.M) * r.p_hat);

	if(isnan(r.W) || isinf(r.W))
		r.W = 0.0;

	// The own sample was checked by the bounce pass.
	if(from_neighbour && r.W > 0.0 && restir_gi_visibility(r, p) == 0.0)
		r.W = 0.0;

	direction = restir_gi_direction(r, p.position);

	return r.W > 0.0 ? restir_gi_shade(r, p, direction) : vec3(0);
}

#endif  /*_RESTIR_GI_H_*/
