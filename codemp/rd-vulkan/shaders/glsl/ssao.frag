#version 450

// Ground-truth ambient occlusion over the G-buffer extraction pass's depth + normal
// attachments (r_ssao). Horizon-search GTAO after Jimenez et al. 2016: per slice, walk
// the depth buffer either side of the pixel to find the largest unoccluded angles, then
// integrate the cosine-weighted visibility over the arc between them.
//
// Everything here is in VIEW space. That is only true because the skinned gbuffer
// shaders were switched over to it - a normal attachment holding two different spaces
// would give characters occlusion computed against a normal pointing somewhere else
// entirely, with nothing in the image to say which pixels were wrong.

layout(set = 0, binding = 0) uniform sampler2D depthTexture;
layout(set = 1, binding = 0) uniform sampler2D normalTexture;

layout(location = 0) in vec2 frag_tex_coord;

// R = ambient visibility (GTAO), G = contact shadow visibility. One attachment for both
// so they share this pass's depth/normal binds and its horizon-search cost structure.
layout(location = 0) out vec4 out_ao;

layout(push_constant) uniform Params {
	// Projection terms, straight out of viewParms.projectionMatrix - see
	// R_SetupProjection()/R_SetupProjectionZ(). p5 is the UNFLIPPED value; the Y flip
	// the renderer applies for Vulkan is undone below rather than baked in here.
	float p0, p5, p8, p9, p10, p14;
	vec2  invScreen;		// 1 / render target size
	float radius;			// world-space sampling radius
	float intensity;		// final power curve
	float frameNoise;		// rotates the slice pattern per frame
	int   sliceCount;
	int   stepCount;

	// Contact shadows. Three loose floats rather than a vec3 on purpose: a vec3 in a push
	// constant block aligns to 16 bytes and would silently introduce padding the C struct
	// would have to mirror exactly. Direction points TOWARD the light, in view space.
	float lightX, lightY, lightZ;
	float csLength;		// world-space ray length; 0 disables
	float csThickness;	// how deep behind a surface a hit still counts as an occluder
	float csStrength;	// how much of the pixel a full contact shadow may take away
	int   csSteps;
};

// Which end of the depth range means "nothing was drawn here". USE_REVERSED_DEPTH is
// currently OFF in this renderer, so the gbuffer clears depth to 1.0 and empty pixels sit
// at the far end - assuming the reversed convention here made the sky read as geometry
// glued to the far plane, which GTAO then happily occluded.
layout(constant_id = 0) const int reversedDepth = 0;

// Which estimator fills the ambient term: 1 = hemisphere SSAO, 2 = GTAO (r_ssao). Both
// live in this shader rather than in two, so the contact-shadow half below - which is the
// larger and more delicate piece - is written once and shared.
layout(constant_id = 1) const int aoMode = 2;

bool DepthIsEmpty(float d) {
	return (reversedDepth == 1) ? (d <= 0.0) : (d >= 1.0);
}

const float PI = 3.14159265359;
const float HALF_PI = 1.57079632679;

// View-space position of a pixel, from its depth. Derived from the frustum in
// R_SetupProjection(): clip.w is -z, so ndc = (p0*x + p8*z) / -z and
// depth = (p10*z + p14) / -z, which inverts to the three lines below. Returns a z of
// zero for the cleared background, which callers must treat as "no surface".
vec3 ViewPosition(vec2 uv, float depth) {
	vec2 ndc = uv * 2.0 - 1.0;

	float z = -p14 / (depth + p10);
	float x = -z * (ndc.x + p8) / p0;
	// The mvp is built with p5 negated (Vulkan's Y-down clip space), so the sign here
	// differs from x rather than matching it.
	float y =  z * (ndc.y + p9) / p5;

	return vec3(x, y, z);
}

float SampleDepth(vec2 uv) {
	return texture(depthTexture, uv).r;
}

// Screen-space contact shadow: march from the surface toward the light and see whether
// anything in the depth buffer stands in the way. Returns 1 for lit, 0 for shadowed.
//
// The thickness test is what keeps this honest: the depth buffer records a surface, not a
// solid, so a ray passing far behind a distant wall would otherwise report a hit. Only a
// hit within csThickness of the ray counts.
float ContactShadow(vec3 P, vec3 N, vec3 L, float jitter) {
	if (csLength <= 0.0 || csSteps <= 0) {
		return 1.0;
	}

	// A surface turned away from the light is already unlit - the shading term has that
	// answer. Marching it anyway is what painted every wall and ceiling solid black: they
	// were reported as shadowed for facing the wrong way, not for being blocked.
	float NdotL = dot(N, L);
	if (NdotL <= 0.0) {
		return 1.0;
	}

	for (int i = 1; i <= csSteps; i++) {
		// Quadratic step distribution, not linear. A contact shadow lives in the first few
		// units off the surface, but linear steps put the very first sample at
		// csLength/csSteps - 2.7 units at the defaults - so the contact itself was never
		// sampled: the ground directly under an object came out lit and the shadow only
		// started further along the ray. Squaring f packs the samples where they matter and
		// lets the tail stay coarse.
		// Jittered per pixel. Without this every pixel samples the ray at exactly the same
		// distances, so the shadow boundary quantises to the same step everywhere and comes
		// out as hard stair-steps. The jitter trades that banding for noise, which is what a
		// denoise pass is for - it cannot be tuned away.
		float f     = (float(i) - jitter) / float(csSteps);
		float fPrev = max(float(i) - 1.0 - jitter, 0.0) / float(csSteps);
		float t     = csLength * f * f;
		float dt    = max(t - csLength * fPrev * fPrev, 1e-4);

		vec3 Q = P + L * t;

		// Forward projection, the exact inverse of ViewPosition(): w is -z, and the Y term
		// carries the negated p5 the renderer uses for Vulkan's clip space.
		float w = -Q.z;
		if (w <= 0.0) {
			break;	// stepped behind the eye
		}

		vec2 ndc = vec2( (p0 * Q.x + p8 * Q.z) / w,
						 (-p5 * Q.y + p9 * Q.z) / w );
		vec2 uv = ndc * 0.5 + 0.5;

		if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
			break;
		}

		float sceneDepth = SampleDepth(uv);
		if (DepthIsEmpty(sceneDepth)) {
			continue;	// nothing was drawn there
		}

		// Alpha of the normal attachment marks a surface whose entity already casts a
		// stencil shadow volume. Its occlusion is drawn into the frame by that system, so
		// counting it here too would darken the same ground twice.
		if (texture(normalTexture, uv).a > 0.5) {
			continue;
		}

		vec3  S      = ViewPosition(uv, sceneDepth);	// the surface actually seen here
		float diff   = S.z - Q.z;	// both negative; positive means the scene is in front

		// Separation as a real distance, not as a depth difference. diff measures along the
		// VIEW axis, so for a fixed world configuration it shrinks when you look down a ray
		// and grows when you look across it - comparing it to a world-unit thickness made
		// acceptance a function of where the camera was pointing, and the shadow appeared
		// and vanished on rotation alone. length(S - Q) is invariant under rotation.
		float sep = length(S - Q);

		// Self-hit rejection scaled by THIS step, not by the average one. When the light
		// grazes a surface the ray hugs it and every sample reports a small positive diff,
		// which smears the shadow along the whole ray; a floor stops that. Sizing the floor
		// off csLength/csSteps instead would reject the near-contact hits as well, which is
		// the detached-shadow failure in the other direction. With quadratic steps dt is
		// tiny at the contact, so the floor there is tiny too.
		// dt, not abs(L.z) * dt. L is in VIEW space, so its z component changes as the
		// camera turns - which made both of these thresholds functions of the view angle and
		// the shadow appear and vanish on rotation alone. A shadow is a world-space fact.
		// dt is a world-space distance along the ray and is invariant under rotation, and it
		// is dimensionally the same kind of quantity as diff, so it compares directly.
		float minDiff = max(dt * 0.5, -Q.z * 0.0004);

		// Upper bound: how far behind a surface the ray may sit and still count as blocked.
		// The depth buffer holds one layer, so past this we assume the ray came out the other
		// side rather than staying inside the object.
		//
		// Deliberately NOT scaled by csLength. Length and thickness answer two unrelated
		// questions - how far to look for an occluder, versus how deep behind a surface the
		// ray is still inside it - and tying them together meant a ray long enough to find a
		// large object's occluder also opened the window to hundreds of units, so everything
		// behind a silhouette counted as blocking and every object dragged a trail.
		// Thickness is a property of the geometry; it belongs to csThickness alone.
		float maxSep = csThickness + dt;

		// diff still gates the SIGN - the surface has to be in front of the ray for the ray
		// to be hidden behind it - while sep decides whether it is close enough to count.
		if (diff > minDiff && sep < maxSep) {
			// Fade with how far along the ray the blocker sits: a blocker against the
			// surface is opaque, one at the end of the ray barely registers. NdotL softens
			// grazing surfaces, where the march is least reliable.
			// Bounded by csStrength. A contact shadow occludes DIRECT light only - ambient
			// still reaches the surface - so it can never take the whole pixel. Returning 0,
			// which the NdotL weighting did for any surface facing the light at all, punched
			// black holes instead of shading: a floor lit from above has a high NdotL
			// everywhere, so every one of its contact shadows came out maximal.
			//
			// NdotL now only ramps the shadow out near the terminator, where the march is
			// least reliable and where the lighting is already darkening the surface anyway.
			float fade = smoothstep(0.0, 1.0, t / csLength);
			float edge = smoothstep(0.0, 0.25, NdotL);

			return mix(1.0 - csStrength * edge, 1.0, fade);
		}
	}

	return 1.0;
}

// Classic normal-oriented hemisphere SSAO (r_ssao 1). Cheaper and blunter than the horizon
// search: it asks "how many points in the hemisphere above this surface turn out to be
// buried" rather than integrating a visibility arc, so it has no notion of how MUCH of the
// hemisphere a given occluder covers. The trade is sample count - the whole budget goes on
// independent points instead of on marching slices.
//
// Reuses sliceCount * stepCount as the sample count so r_ssaoSlices and r_ssaoSteps stay
// the single pair of quality knobs across both modes.
float HemisphereAO(vec3 P, vec3 N, float rnd) {
	int n = max(sliceCount * stepCount, 1);
	float occlusion = 0.0;

	// Tangent frame around the normal. The up vector is swapped near the pole so the cross
	// product never degenerates.
	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 T  = normalize(cross(up, N));
	vec3 B  = cross(N, T);

	for (int i = 0; i < n; i++) {
		// Golden-angle spiral: an even cosine-weighted spread over the hemisphere without a
		// kernel texture, rotated per pixel by rnd so neighbours do not share a pattern.
		float u   = (float(i) + 0.5) / float(n);
		float r   = sqrt(u);
		float phi = (float(i) + rnd) * 2.39996323;

		vec3 dir = T * (r * cos(phi)) + B * (r * sin(phi)) + N * sqrt(max(1.0 - u, 0.0));

		// Samples bunched toward the surface, where occlusion actually matters.
		float scale = mix(0.1, 1.0, u * u);
		vec3  Q = P + dir * (radius * scale);

		float w = -Q.z;
		if (w <= 0.0) {
			continue;
		}

		vec2 ndc = vec2( (p0 * Q.x + p8 * Q.z) / w,
						 (-p5 * Q.y + p9 * Q.z) / w );
		vec2 uv = ndc * 0.5 + 0.5;

		if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
			continue;
		}

		float d = SampleDepth(uv);
		if (DepthIsEmpty(d)) {
			continue;
		}

		vec3 S = ViewPosition(uv, d);

		// The sample is buried if the surface actually seen there sits in front of it. The
		// range check stops a distant wall behind the pixel from counting as a near
		// occluder, which is what makes naive SSAO halo around silhouettes.
		float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(P.z - S.z), 1e-4));

		if (S.z > Q.z + 0.02) {
			occlusion += rangeCheck;
		}
	}

	return clamp(1.0 - occlusion / float(n), 0.0, 1.0);
}

void main() {
	float centerDepth = SampleDepth(frag_tex_coord);

	// Sky, and anything else the extraction pass skips, lands in this branch and stays
	// fully visible - both for ambient occlusion and for the contact shadow.
	if (DepthIsEmpty(centerDepth)) {
		out_ao = vec4(1.0);
		return;
	}

	vec3 P = ViewPosition(frag_tex_coord, centerDepth);
	vec3 N = normalize(texture(normalTexture, frag_tex_coord).rgb * 2.0 - 1.0);
	vec3 V = normalize(-P);

	// Screen-space extent of the world-space radius at this depth. Clamped so a surface
	// close to the camera does not turn into a full-screen search.
	float screenRadius = clamp(radius * p0 / (-P.z) * 0.5 / invScreen.x, 4.0, 128.0);

	// Interleaved gradient noise, offset per frame so the slice pattern differs frame to
	// frame rather than baking a fixed rosette into every image.
	vec2 pixel = gl_FragCoord.xy;
	float noise = fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))) + frameNoise);

	// Visibility, not occlusion: 1 is fully open. Named for what it holds so the output
	// convention is not something you have to derive from the integral.
	float visibility = 1.0;

	// aoMode is a specialization constant, so exactly one of these two bodies survives
	// pipeline creation and the other costs nothing.
	if (aoMode == 1) {
		visibility = HemisphereAO(P, N, noise);
	}
	else {
	float visibilitySum = 0.0;

	for (int slice = 0; slice < sliceCount; slice++) {
		float phi = (float(slice) + noise) * PI / float(sliceCount);
		vec2 omega = vec2(cos(phi), sin(phi));

		// The slice plane is spanned by V and this direction; everything below is the
		// 2D problem inside it. The direction has to be the VIEW-space image of the screen
		// direction we are about to march, not the screen angle reused as if it were a view
		// angle: p0 and p5 differ (2*zProj/width vs 2*zProj/height), so 45 degrees on screen
		// is not 45 degrees in view space. Getting this wrong projects the normal into a
		// different plane from the one the horizons are found in - exact only dead centre.
		// Reconstructing one step along omega at the centre depth costs no texture fetch.
		vec3 sliceDir = ViewPosition(frag_tex_coord + omega * invScreen, centerDepth) - P;
		float sliceDirLen = length(sliceDir);
		if (sliceDirLen < 1e-6) {
			continue;
		}
		sliceDir /= sliceDirLen;
		vec3 orthoDir = sliceDir - dot(sliceDir, V) * V;
		vec3 axis = cross(sliceDir, V);
		vec3 projN = N - axis * dot(N, axis);

		float projNLen = length(projN);
		if (projNLen < 1e-5) {
			continue;
		}

		vec3 projNn = projN / projNLen;

		float sgn = sign(dot(orthoDir, projNn));
		float n = sgn * acos(clamp(dot(projNn, V), -1.0, 1.0));

		// Horizon angles either side, seeded at the tangent plane so a flat surface
		// integrates to no occlusion.
		float h[2];
		h[0] = -HALF_PI;
		h[1] =  HALF_PI;

		for (int side = 0; side < 2; side++) {
			float dirSign = (side == 0) ? -1.0 : 1.0;
			float cosHorizon = -1.0;

			for (int s = 1; s <= stepCount; s++) {
				// Jittered so the steps of neighbouring pixels do not line up into rings.
				float t = (float(s) - 0.5 + noise) / float(stepCount);
				vec2 offset = omega * dirSign * t * screenRadius * invScreen;
				vec2 sampleUV = frag_tex_coord + offset;

				if (any(lessThan(sampleUV, vec2(0.0))) || any(greaterThan(sampleUV, vec2(1.0)))) {
					break;
				}

				float sampleDepth = SampleDepth(sampleUV);
				if (DepthIsEmpty(sampleDepth)) {
					continue;
				}

				vec3 S = ViewPosition(sampleUV, sampleDepth) - P;
				float dist = length(S);
				if (dist < 1e-4) {
					continue;
				}

				// Attenuate past the radius instead of cutting off, so a sample crossing
				// the boundary does not pop, and never let a distant background pixel
				// count as a near occluder.
				float falloff = clamp(1.0 - (dist / radius), 0.0, 1.0);
				float cosH = dot(S / dist, V);
				cosHorizon = max(cosHorizon, mix(-1.0, cosH, falloff));
			}

			h[side] = dirSign * acos(clamp(cosHorizon, -1.0, 1.0));
		}

		// Clamp each horizon into the hemisphere around the normal, then apply the
		// cosine-weighted arc integral (Jimenez eq. 7).
		h[0] = n + max(h[0] - n, -HALF_PI);
		h[1] = n + min(h[1] - n,  HALF_PI);

		float sinN = sin(n);
		float sliceVisibility =
			0.25 * (-cos(2.0 * h[0] - n) + cos(n) + 2.0 * h[0] * sinN) +
			0.25 * (-cos(2.0 * h[1] - n) + cos(n) + 2.0 * h[1] * sinN);

		visibilitySum += projNLen * sliceVisibility;
	}

	visibility = clamp(visibilitySum / float(sliceCount), 0.0, 1.0);
	}

	visibility = pow(visibility, intensity);

	// Offset the ray start along the normal so a surface does not shadow itself at
	// grazing angles, which is the classic contact-shadow acne.
	vec3 L = vec3(lightX, lightY, lightZ);
	// Bias scaled with view depth: one fixed world-space offset is either useless up close
	// or visibly detaches the shadow far away.
	// Kept small on purpose. This offset exists to stop a surface self-shadowing at
	// grazing angles, but it also throws away the first samples - the ones that make the
	// contact. On an 8-unit ray a one-unit offset is an eighth of the whole march, which is
	// what detached the shadow from tight corners. Raise it if acne returns.
	float bias = max(0.05, -P.z * 0.0004);
	float shadow = ContactShadow(P + N * bias, N, L, noise);

	out_ao = vec4(visibility, shadow, 0.0, 1.0);
}
