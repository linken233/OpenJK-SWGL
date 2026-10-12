/*
Copyright (C) 2018 Tobias Zirr
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef _LIGHT_LISTS_
#define _LIGHT_LISTS_

#define MAX_BRUTEFORCE_SAMPLING 8
#define MAX_DYNAMIC_SAMPLING    16
#define SPHERE_LIGHT_MAX_DEPTH  64.0	// how far behind a surface a sphere light is still mirrored in front of it

mat3 
project_triangle(mat3 positions, vec3 p)
{
	positions[0] = positions[0] - p;
	positions[1] = positions[1] - p;
	positions[2] = positions[2] - p;
	
	positions[0] = normalize(positions[0]);
	positions[1] = normalize(positions[1]);
	positions[2] = normalize(positions[2]);

	return positions;
}

float
projected_tri_area(mat3 positions, vec3 p, vec3 n, vec3 V, float phong_exp, float phong_scale, float phong_weight)
{
	positions[0] = positions[0] - p;
	positions[1] = positions[1] - p;
	positions[2] = positions[2] - p;
	
	vec3 g = cross(positions[1] - positions[0], positions[2] - positions[0]);
	if ( dot(n, positions[0]) <= 0 && dot(n, positions[1]) <= 0 && dot(n, positions[2]) <= 0 )
		return 0;
	if ( dot(g, positions[0]) >= 0 && dot(g, positions[1]) >= 0 && dot(g, positions[2]) >= 0 )
		return 0;

	vec3 L = normalize(positions * vec3(1.0 / 3.0));
	float specular = phong(n, L, V, phong_exp) * phong_scale;
	float brdf = mix(1.0, specular, phong_weight);

	// Project triangle to unit sphere
	vec3 A = normalize(positions[0]);
	vec3 B = normalize(positions[1]);
	vec3 C = normalize(positions[2]);

	// Area of spherical triangle
	float area = 2 * atan(abs(dot(A, cross(B, C))), 1 + dot(A, B) + dot(B, C) + dot(A, C));
	float pa = max(area - 1e-5, 0.);
	return pa * brdf;
}

// The centre of a sphere light for the surface (p, n). A fixed light (positions[1].z == 0) that sits
// just behind the surface is mirrored in front of it, so a light embedded in a wall lights that wall.
vec3
sphere_light_center(mat3 positions, vec3 p, vec3 n)
{
	vec3 c = positions[0];
	float s = dot(c - p, n);

	if (positions[1].z == 0 && s < 0 && s > -SPHERE_LIGHT_MAX_DEPTH)
		c -= 2 * s * n;

	return c;
}

// True when the surface point p is inside a fixed sphere light (a light against a wall: the wall
// crosses the sphere). Such a point sees the emitter over a hemisphere around sphere_inside_axis.
bool
sphere_light_contains(mat3 positions, vec3 center, vec3 p)
{
	vec3 d = center - p;
	return positions[1].z == 0 && dot(d, d) < square(positions[1].x);
}

// Weight of a sphere light at a distance, in the units of the solid angle it covers. The exact value
// falls with an infinite slope at the sphere surface, which shows as an edge on a wall that crosses a
// large sphere. A fixed light uses a soft near field instead, R^2 / (d^2 + R^2 / 2), which meets the
// exact value from 5 R. A dlight (positions[1].z > 0) keeps the exact value.
float
sphere_light_area(mat3 positions, float dist)
{
	float r = positions[1].x;
	float exact = 2 * (1 - sqrt(max(0, 1 - square(r / max(dist, positions[1].z)))));

	exact = min(exact, 2 * M_PI); //max solid angle
	if (positions[1].z != 0)
		return exact;

	return mix(square(r) / (dist * dist + 0.5 * square(r)), exact, smoothstep(r, 5.0 * r, dist));
}

// Axis of that hemisphere: the direction to the centre on the sphere surface, where the outside
// sampling ends, turning to the surface normal at the centre. No step at the sphere surface.
vec3
sphere_inside_axis(mat3 positions, vec3 center, vec3 p, vec3 n)
{
	vec3 d = center - p;
	float dist = max(length(d), 1e-4);
	float t = 1.0 - min(dist / positions[1].x, 1.0);

	return normalize(d / dist * (1.0 - t) + n * t);
}

float
projected_sphere_area(mat3 positions, vec3 p, vec3 n, vec3 V, float phong_exp, float phong_scale, float phong_weight)
{
	vec3 center = sphere_light_center(positions, p, n);
	float sphere_radius = positions[1].x;

	if (sphere_light_contains(positions, center, p))
	{
		float specular_in = phong(n, sphere_inside_axis(positions, center, p, n), V, phong_exp) * phong_scale;
		return sphere_light_area(positions, length(center - p)) * mix(1.0, specular_in, phong_weight);
	}

	vec3 position = center - p;
	float dist = length(position);
	float rdist = 1.0 / dist;
	vec3 L = position * rdist;

	if (dot(n, L) <= 0)
		return 0.0;

	float specular = phong(n, L, V, phong_exp) * phong_scale;
	float brdf = mix(1.0, specular, phong_weight);

	// positions[1].z: the minimum distance of a dlight (add_dlights), 0 on other lights.
	return sphere_light_area(positions, dist) * brdf;
}

float
spot_falloff(in mat3 positions, in float cosTheta)
{
	float falloff;
	const uint spot_style = floatBitsToUint(positions[1].y);
	const uint spot_data = floatBitsToUint(positions[1].z);

	if(spot_style == DYNLIGHT_SPOT_EMISSION_PROFILE_FALLOFF) {
		const vec2 spot_falloff = unpackHalf2x16(spot_data);
		const float cosTotalWidth = spot_falloff.x;
		const float cosFalloffStart = spot_falloff.y;

		if(cosTheta < cosTotalWidth)
			falloff = 0;
		else if (cosTheta > cosFalloffStart)
			falloff = 1;
		else {
			float delta = (cosTheta - cosTotalWidth) / (cosFalloffStart - cosTotalWidth);
			falloff = (delta * delta) * (delta * delta);
		}
	} else if(spot_style == DYNLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE) {
		const float theta = acos(cosTheta);
		const float totalWidth = unpackHalf2x16(spot_data).x;
		const uint texture_num = spot_data >> 16;

		if (cosTheta >= 0) {
			// Use the angle directly as texture coordinate for better angular resolution next to the center of the beam
			float tc = clamp(theta / totalWidth, 0, 1);
			falloff = global_texture(texture_num, vec2(tc, 0)).r;
		} else
			falloff = 0;
	}

	return falloff;
}

float
projected_spotlight_area(mat3 positions, vec3 p, vec3 n, vec3 V, float phong_exp, float phong_scale, float phong_weight)
{
	vec3 position = positions[0] - p;

	float dist = length(position);
	float rdist = 1.0 / dist;
	vec3 L = position * rdist;

	if (dot(n, L) <= 0)
		return 0.0;

	float specular = phong(n, L, V, phong_exp) * phong_scale;
	float brdf = mix(1.0, specular, phong_weight);

	float cosTheta = dot(-L, positions[2]); // cosine of angle to spot direction
	float falloff = spot_falloff(positions, cosTheta);

	float irradiance = 2 * falloff * square(rdist);

	irradiance = min(irradiance,  2 * M_PI); //max solid angle

	return irradiance * brdf;
}

float get_spherical_triangle_pdfw(mat3 positions)
{
	// Project triangle to unit sphere
	vec3 A = normalize(positions[0]);
	vec3 B = normalize(positions[1]);
	vec3 C = normalize(positions[2]);

	// Area of spherical triangle
	float area = 2 * atan(abs(dot(A, cross(B, C))), 1 + dot(A, B) + dot(B, C) + dot(A, C));

	// Since the solid angle is distributed uniformly, the PDF wrt to solid angle is simply:
	return 1 / area;
}

/* Sample a triangle, projected to a unit sphere.
 *
 * The implementation is based on the algorithm described in:
 * James Arvo. 1995. Stratified sampling of spherical triangles.
 * Proceedings of the 22nd annual conference on Computer graphics and interactive techniques (SIGGRAPH '95).
 * Association for Computing Machinery, New York, NY, USA, 437�438.
 * https://doi.org/10.1145/218380.218500
 */
vec3
sample_projected_triangle(vec3 pt, mat3 positions, vec2 rnd, out vec3 light_normal, out float pdfw)
{
	light_normal = cross(positions[1] - positions[0], positions[2] - positions[0]);
	light_normal = normalize(light_normal);

	// Use surface point as origin
	positions[0] = positions[0] - pt;
	positions[1] = positions[1] - pt;
	positions[2] = positions[2] - pt;

	// Distance of triangle to origin
	float o = dot(light_normal, positions[0]);

	// Project triangle to unit sphere
	vec3 A = normalize(positions[0]);
	vec3 B = normalize(positions[1]);
	vec3 C = normalize(positions[2]);
	// Planes passing through two vertices and origin. They'll be used to obtain the angles.
	vec3 cross_BC = cross(B, C);
	vec3 norm_AB = normalize(cross(A, B));
	vec3 norm_BC = normalize(cross_BC);
	vec3 norm_CA = normalize(cross(C, A));
	// Side of spherical triangle
	float cos_c = dot(A, B);
	// Angles at vertices
	float cos_alpha = dot(norm_AB, -norm_CA);
	float cos_beta = dot(norm_BC, -norm_AB);
	float cos_gamma = dot(norm_CA, -norm_BC);

	// Area of spherical triangle. From: "On the Measure of Solid Angles", F. Eriksson, 1990.
	float area = 2 * atan(abs(dot(A, cross_BC)), 1 + cos_c + dot(B, C) + dot(A, C));

	// Use one random variable to select the new area.
	float new_area = rnd.x * area;

	float sin_alpha = sqrt(1 - cos_alpha * cos_alpha); // = sin(acos(cos_alpha))
	float sin_new_area = sin(new_area);
	float cos_new_area = cos(new_area);
	// Save the sine and cosine of the angle phi.
	float p = sin_new_area * cos_alpha - cos_new_area * sin_alpha;
	float q = cos_new_area * cos_alpha + sin_new_area * sin_alpha;

	// Compute the pair (u, v) that determines new_beta.
	float u = q - cos_alpha;
	float v = p + sin_alpha * cos_c;

	// Let cos_b be the cosine of the new edge length new_b.
	float cos_b = clamp(((v * q - u * p) * cos_alpha - v) / ((v * p + u * q) * sin_alpha), -1, 1);

	// Compute the third vertex of the sub-triangle.
	vec3 new_C = cos_b * A + sqrt(1 - cos_b * cos_b) * normalize(C - dot(C, A) * A);

	// Use the other random variable to select cos(phi).
	float z = 1 - rnd.y * (1 - dot(new_C, B));

	// Construct the corresponding point on the sphere.
	vec3 direction = z * B + sqrt(1 - z * z) * normalize(new_C - dot(new_C, B) * B);
	// ...which is also the direction!

	// Line-plane intersection
	vec3 lo = direction * (o / dot(light_normal, direction));

	// Since the solid angle is distributed uniformly, the PDF wrt to solid angle is simply:
	pdfw = 1 / area;

	return pt + lo;
}

vec3
sample_projected_sphere(vec3 p, vec3 n, mat3 positions, vec2 rnd, out vec3 light_normal, out float pdfw)
{
	vec3 light_center = sphere_light_center(positions, p, n);
	float sphere_radius = positions[1].x;

	if (sphere_light_contains(positions, light_center, p))
	{
		// Uniform direction over the hemisphere around the axis, to the point where it leaves the
		// sphere. The normal there points inwards, toward p, like the normal of a sphere seen from outside.
		float y = rnd.x;
		float r = sqrt(max(0, 1 - y * y));
		float phi = 2.0 * M_PI * rnd.y;
		mat3 basis = construct_ONB_frisvad(sphere_inside_axis(positions, light_center, p, n));
		vec3 dir = basis[0] * (r * cos(phi)) + basis[1] * y + basis[2] * (r * sin(phi));

		vec3 oc = p - light_center;
		float b = dot(oc, dir);
		float t = -b + sqrt(max(0, b * b - (dot(oc, oc) - square(sphere_radius))));

		vec3 hit = p + dir * t;
		// The callers weight a sample by -dot(light_normal, L), which a point outside the sphere
		// sees going to 0 at the surface. Inside, it grows from 0 at the surface to 1 at the centre.
		light_normal = -dir * (1.0 - min(length(light_center - p) / sphere_radius, 1.0));
		pdfw = 1.0 / sphere_light_area(positions, length(light_center - p));
		return hit;
	}

	vec3 position = light_center - p;
	float dist = length(position);
	float rdist = 1.0 / dist;
	vec3 L = position * rdist;

	pdfw = 1.0 / sphere_light_area(positions, dist);

	mat3 onb = construct_ONB_frisvad(L);
	vec3 diskpt;
	diskpt.xy = sample_disk(rnd);
	diskpt.z = sqrt(max(0, 1 - diskpt.x * diskpt.x - diskpt.y * diskpt.y));

	vec3 position_light = light_center + (onb[0] * diskpt.x + onb[2] * diskpt.y - L * diskpt.z) * sphere_radius;

	light_normal = normalize(position_light - light_center);

	return position_light;
}

vec3
sample_projected_spotlight(vec3 p, mat3 positions, vec2 rnd, out vec3 light_normal, out float pdfw)
{
	vec3 light_center = positions[0];
	float emitter_radius = positions[1].x;

	mat3 onb = construct_ONB_frisvad(positions[2]);
	// Emit light from a small disk around the origin
	vec2 diskpt = sample_disk(rnd);
	vec3 position_light = light_center + (onb[0] * diskpt.x + onb[2] * diskpt.y) * emitter_radius;

	vec3 c = position_light - p;
	float dist = length(c);
	float rdist = 1.0 / dist;
	vec3 L = c * rdist;

	// Direction from emission point to surface, in a basis where +Y is the spot direction
	vec3 L_l = -L * onb;
	float cosTheta = L_l.y; // cosine of angle to spot direction
	float falloff = spot_falloff(positions, cosTheta);

	float projected_area = 2 * falloff * square(rdist);
	projected_area = min(projected_area,  2 * M_PI); //max solid angle
	pdfw = 1.0 / projected_area;

	light_normal = normalize(positions[2]);

	return position_light;
}

uint get_light_stats_addr(uint cluster, uint light, uint side)
{
	uint addr = cluster;
	addr = addr * global_ubo.num_static_lights + light;
	addr = addr * 6 + side;
	addr = addr * 2;
	return addr;
}

// Sampling weight of one light for the point p: projected area x luminance x shadow statistics.
float
light_sampling_mass(
		uint list_idx,
		uint current_idx,
		vec3 p,
		vec3 n,
		vec3 V,
		float phong_exp,
		float phong_scale,
		float phong_weight,
		bool is_gradient)
{
	// In case of polygon light overflow, the host code will still populate the light lists
	// with invalid indices. Skip those lights here, so they have pdf=0 and will not be selected.
	if (current_idx >= MAX_LIGHT_POLYS)
		return 0;

	LightPolygon light = get_light_polygon(current_idx);

	float m = 0.0f;
	switch(uint(light.type)){
		case LIGHT_POLYGON:
			m = projected_tri_area(light.positions, p, n, V, phong_exp, phong_scale, phong_weight);
			break;
		case LIGHT_SPHERE:
			m = projected_sphere_area(light.positions, p, n, V, phong_exp, phong_scale, phong_weight);
			break;
		case LIGHT_SPOT:
			m = projected_spotlight_area(light.positions, p, n, V, phong_exp, phong_scale, phong_weight);
			break;
	}

	float light_lum = luminance(light.color);

	// Apply light style scaling.
	// For gradient pixels, use the style from the previous frame here
	// in order to keep the CDF consistent and make sure that the same light is picked,
	// regardless of animations. This makes the image more stable around blinking lights,
	// especially in shadowed areas.
	light_lum *= is_gradient ? light.prev_style_scale : light.light_style_scale;	

	if(light_lum < 0 && global_ubo.environment_type == ENVIRONMENT_DYNAMIC)
	{
		// Set limits on sky luminance to avoid oversampling the sky in shadowed areas, or undersampling at dusk and dawn.
		// Note: the log -> linear conversion of the cvars happens on the CPU, in main.c
		m *= clamp(sun_color_ubo.sky_luminance, global_ubo.pt_min_log_sky_luminance, global_ubo.pt_max_log_sky_luminance);
	}
	else
		m *= abs(light_lum); // abs because sky lights have negative color

	// Apply CDF adjustment based on light shadowing statistics from one of the previous frames.
	// See comments in function `get_direct_illumination` in `path_tracer_rgen.h`
	if(global_ubo.pt_light_stats != 0 
		&& m > 0 
		&& current_idx < global_ubo.num_static_lights)
	{
		uint buffer_idx = global_ubo.current_frame_idx;
		// Regular pixels get shadowing stats from the previous frame;
		// Gradient pixels get the stats from two frames ago because they need to match
		// the light sampling from the previous frame.
		buffer_idx += is_gradient ? (NUM_LIGHT_STATS_BUFFERS - 2) : (NUM_LIGHT_STATS_BUFFERS - 1);
		buffer_idx = buffer_idx % NUM_LIGHT_STATS_BUFFERS;

		uint addr = get_light_stats_addr(list_idx, current_idx, get_primary_direction(n));

		uint num_hits = light_stats_bufers[buffer_idx].stats[addr];
		uint num_misses = light_stats_bufers[buffer_idx].stats[addr + 1];
		uint num_total = num_hits + num_misses;

		if(num_total > 0)
		{
			// Adjust the mass, but set a lower limit on the factor to avoid
			// extreme changes in the sampling.
			m *= max(float(num_hits) / float(num_total), 0.1);
		}
	}

	return m;
}

void
sample_lights(
		uint list_idx,
		vec3 p,
		vec3 n,
		vec3 gn,
		vec3 V, 
		float phong_exp, 
		float phong_scale,
		float phong_weight,
		bool is_gradient,
		out vec3 position_light,
		out vec3 light_color,
		out int light_index,
		out float pdfw,
		out bool is_sky_light,
		vec3 rng)
{
	position_light = vec3(0);
	light_index = -1;
	light_color = vec3(0);
	pdfw = 0;
	is_sky_light = false;

	if(list_idx == ~0u)
		return;

	uint list_start = light_buffer.light_list_offsets[list_idx];
	uint list_end   = light_buffer.light_list_offsets[list_idx + 1];
	/* The light count we base light selection on may differ from the current count
	 * to avoid gradient estimation breaking (see comment on light_counts_history).
	 * Obtain the frame number for the historical count from the RNG seed
	 * (which is also the historical RNG seed) */
	uint history_index = (rng_seed >> RNG_SEED_SHIFT_FRAME) % LIGHT_COUNT_HISTORY;
	uint light_count = light_counts_history[history_index].sample_light_counts[list_idx];

	// The dynamic lights (sabers, dlights) are at the tail of the list. They are candidates
	// for every point. Only the static lights before them are partitioned.
	uint sample_end = min(list_start + light_count, list_end);
	uint dyn_start = sample_end;
	while(dyn_start > list_start && sample_end - dyn_start < MAX_DYNAMIC_SAMPLING
		&& light_buffer.light_list_lights[dyn_start - 1] >= global_ubo.num_static_lights)
	{
		dyn_start--;
	}
	uint static_end = list_start + light_count - (sample_end - dyn_start);

	float partitions = max(ceil(float(static_end - list_start) / float(MAX_BRUTEFORCE_SAMPLING)), 1.0);
	rng.x *= partitions;
	float fpart = min(floor(rng.x), partitions-1);
	rng.x -= fpart;
	uint part_start = list_start + int(fpart);
	int stride = int(partitions);

	// A static candidate stands for the `partitions` lights of its stride, so its weight
	// is m * partitions. The pdf of every light is then m / mass. 
	float mass = 0.;

	float light_masses[MAX_BRUTEFORCE_SAMPLING];
	float dyn_masses[MAX_DYNAMIC_SAMPLING];

	#pragma unroll
	for(uint i = 0, n_idx = part_start; i < MAX_BRUTEFORCE_SAMPLING; i++, n_idx += stride) {
		if (n_idx >= static_end)
			break;
		
		if(n_idx >= dyn_start)
		{
			light_masses[i] = 0;
			continue;
		}

		float m = light_sampling_mass(list_idx, light_buffer.light_list_lights[n_idx], p, n, V, phong_exp, phong_scale, phong_weight, is_gradient);

		mass += m * partitions;
		light_masses[i] = m * partitions;
	}

	for(uint i = 0, n_idx = dyn_start; n_idx < sample_end; i++, n_idx++) {
		float m = light_sampling_mass(list_idx, light_buffer.light_list_lights[n_idx], p, n, V, phong_exp, phong_scale, phong_weight, is_gradient);

		mass += m;
		dyn_masses[i] = m;
	}

	if (mass <= 0)
		return;

	rng.x *= mass;
	int current_idx = -1;
	float pdf = 0;

	#pragma unroll
	for(uint i = 0, n_idx = part_start; i < MAX_BRUTEFORCE_SAMPLING; i++, n_idx += stride) {
		if (n_idx >= static_end)
			break;
		pdf = light_masses[i] / partitions;
		current_idx = int(n_idx);
		rng.x -= light_masses[i];

		if (rng.x <= 0)
			break;
	}

	if (rng.x > 0)
	{
		for(uint i = 0, n_idx = dyn_start; n_idx < sample_end; i++, n_idx++) {
			pdf = dyn_masses[i];
			current_idx = int(n_idx);
			rng.x -= dyn_masses[i];

			if (rng.x <= 0)
				break;
		}
	}

	if(rng.x > 0)
		return;

	pdf /= mass;

	// assert: current_idx >= 0?
	if (current_idx >= 0) {
		current_idx = int(light_buffer.light_list_lights[current_idx]);

		LightPolygon light = get_light_polygon(current_idx);

		vec3 light_normal;

		switch(uint(light.type)){
			case LIGHT_POLYGON:
				position_light = sample_projected_triangle(p, light.positions, rng.yz, light_normal, pdfw);
				break;
			case LIGHT_SPHERE:
				position_light = sample_projected_sphere(p, gn, light.positions, rng.yz, light_normal, pdfw);
				break;
			case LIGHT_SPOT:
				position_light = sample_projected_spotlight(p, light.positions, rng.yz, light_normal, pdfw);
				break;
		}

		vec3 L = normalize(position_light - p);

		if(dot(L, gn) <= 0)
			pdfw = 0;

		if (pdfw > 0)
		{
			float LdotNL = max(0, -dot(light_normal, L));
			float spotlight = sqrt(LdotNL);
			float inv_pdfw = 1.0 / pdfw;

			if(light.color.r >= 0)
			{
				light_color = light.color * (inv_pdfw * spotlight * light.light_style_scale);
			}
			else
			{
				light_color = env_map(L, true) * inv_pdfw * global_ubo.pt_env_scale;
				is_sky_light = true;
			}
		}

		light_index = current_idx;
	}

	light_color /= pdf;
}

#endif /*_LIGHT_LISTS_*/

// vim: shiftwidth=4 noexpandtab tabstop=4 cindent
