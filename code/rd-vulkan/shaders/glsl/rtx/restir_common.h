// ========================================================================== //
// Code that the ReSTIR passes share: the selection of a reservoir sample, the
// reprojection to the previous frame, and the tests that decide if a pixel of
// the previous frame shows the same surface.
//
// The includer has to include path_tracer_rgen.h before this file.
// ========================================================================== //

#ifndef  _RESTIR_COMMON_H_
#define  _RESTIR_COMMON_H_

#define RESTIR_DEPTH_THRESHOLD   0.1	// maximum relative difference of the view depth
#define RESTIR_NORMAL_THRESHOLD  0.5	// minimum dot product of the normals

// Adds a candidate with the weight wi that stands for Mi samples to the sums of a reservoir.
// Returns true when the candidate replaces the selected sample. rng stays uniform in [0, 1)
// for the next candidate.
bool
ris_select(inout float w_sum, inout uint M, float wi, uint Mi, inout float rng)
{
	w_sum += wi;
	M += Mi;
	float p_s = w_sum > 0.0 ? (wi / w_sum) : 0.0;
	if(rng < p_s)
	{
		rng /= p_s;
		return true;
	}
	else
	{
		rng = (rng - p_s) / (1.0f - p_s);
		return false;
	}
}

// The object of a visibility buffer entry: ~0u for the world, else the model instance in
// the current frame. A model instance of the previous frame goes through model_prev_to_current.
uint restir_object(uint visbuf_instance, bool prev)
{
	if(visbuf_instance >= ~uint(VERTEX_BUFFER_WORLD_D_GEOMETRY))
		return ~0u;

	if(!prev)
		return visbuf_instance;

	if(visbuf_instance >= SHADER_MAX_ENTITIES)
		return ~1u;

	uint curr = instance_buffer.model_prev_to_current[visbuf_instance];
	return curr == ~0u ? ~1u : curr;
}

// The pixel of the previous frame that shows the surface point of pixel ipos.
ivec2
restir_get_prev_pos(ivec2 ipos, vec4 motion)
{
	return ivec2(floor(((vec2(ipos) + vec2(0.5)) * vec2(global_ubo.inv_width * 2, global_ubo.inv_height) + motion.xy) * vec2(global_ubo.prev_width * 0.5, global_ubo.prev_height)));
}

// The columns [left, right) of the checkerboard field of pixel ipos in the previous frame.
void
restir_get_prev_field(ivec2 ipos, out int left, out int right)
{
	left = 0;
	right = global_ubo.prev_width / 2;
	if(ipos.x >= global_ubo.width / 2)
	{
		left = right;
		right = global_ubo.prev_width;
	}
}

// The columns [left, right) of the checkerboard field of pixel ipos in the current frame.
void
restir_get_curr_field(ivec2 ipos, out int left, out int right)
{
	left = 0;
	right = global_ubo.width / 2;
	if(ipos.x >= right)
	{
		left = right;
		right = global_ubo.width;
	}
}

// True when another pixel has about the same depth and normal as the current pixel.
bool
restir_surface_matches(float depth_other, vec3 normal_other, float view_depth, vec3 normal)
{
	float dist_depth = abs(depth_other - view_depth) / abs(view_depth);

	if(!(dist_depth < RESTIR_DEPTH_THRESHOLD))
		return false;

	return dot(normal_other, normal) > RESTIR_NORMAL_THRESHOLD;
}

// True when the pixel of the previous frame shows about the same surface as the current pixel.
// With check_object it also has to show the same object: a reservoir of a model near a light
// is not valid for the floor below it.
bool
restir_prev_is_same_surface(ivec2 pos_prev, float view_depth, vec3 normal, uint object, bool check_object)
{
	if(!restir_surface_matches(texelFetch(TEX_PT_VIEW_DEPTH_B, pos_prev, 0).x, decode_normal(texelFetch(TEX_PT_NORMAL_B, pos_prev, 0).x), view_depth, normal))
		return false;

	return !check_object || restir_object(texelFetch(TEX_PT_VISBUF_PRIM_B, pos_prev, 0).x, true) == object;
}

// The same test for a pixel of the current frame, always with the object.
bool
restir_curr_is_same_surface(ivec2 pos, float view_depth, vec3 normal, uint object)
{
	if(!restir_surface_matches(texelFetch(TEX_PT_VIEW_DEPTH_A, pos, 0).x, decode_normal(texelFetch(TEX_PT_NORMAL_A, pos, 0).x), view_depth, normal))
		return false;

	return restir_object(texelFetch(TEX_PT_VISBUF_PRIM_A, pos, 0).x, false) == object;
}

#endif  /*_RESTIR_COMMON_H_*/
