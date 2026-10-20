#version 450

// GGX prefilter of the captured environment cubemap into one face of one mip level of
// the probe cubemap. Ported from JKSunny/EternalJK (Sascha Willems' Vulkan PBR sample).

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform samplerCube samplerEnv;

layout(push_constant) uniform PushConsts {
	float roughness;
	int face;
} consts;

const uint numSamples = 256u;
const float PI = 3.1415926536;

// http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float random( vec2 co )
{
	float a = 12.9898;
	float b = 78.233;
	float c = 43758.5453;
	float dt = dot( co.xy, vec2( a, b ) );
	float sn = mod( dt, 3.14 );
	return fract( sin( sn ) * c );
}

vec2 hammersley2d( uint i, uint N )
{
	// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
	uint bits = ( i << 16u ) | ( i >> 16u );
	bits = ( ( bits & 0x55555555u ) << 1u ) | ( ( bits & 0xAAAAAAAAu ) >> 1u );
	bits = ( ( bits & 0x33333333u ) << 2u ) | ( ( bits & 0xCCCCCCCCu ) >> 2u );
	bits = ( ( bits & 0x0F0F0F0Fu ) << 4u ) | ( ( bits & 0xF0F0F0F0u ) >> 4u );
	bits = ( ( bits & 0x00FF00FFu ) << 8u ) | ( ( bits & 0xFF00FF00u ) >> 8u );
	float rdi = float( bits ) * 2.3283064365386963e-10;
	return vec2( float( i ) / float( N ), rdi );
}

// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
vec3 importanceSample_GGX( vec2 Xi, float roughness, vec3 normal )
{
	float alpha = roughness * roughness;
	float phi = 2.0 * PI * Xi.x + random( normal.xz ) * 0.1;
	float cosTheta = sqrt( ( 1.0 - Xi.y ) / ( 1.0 + ( alpha * alpha - 1.0 ) * Xi.y ) );
	float sinTheta = sqrt( 1.0 - cosTheta * cosTheta );
	vec3 H = vec3( sinTheta * cos( phi ), sinTheta * sin( phi ), cosTheta );

	vec3 up = abs( normal.z ) < 0.999 ? vec3( 0.0, 0.0, 1.0 ) : vec3( 1.0, 0.0, 0.0 );
	vec3 tangentX = normalize( cross( up, normal ) );
	vec3 tangentY = normalize( cross( normal, tangentX ) );

	return normalize( tangentX * H.x + tangentY * H.y + normal * H.z );
}

float D_GGX( float dotNH, float roughness )
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * ( alpha2 - 1.0 ) + 1.0;
	return alpha2 / ( PI * denom * denom );
}

vec3 prefilterEnvMap( vec3 R, float roughness )
{
	vec3 N = R;
	vec3 V = R;
	vec3 color = vec3( 0.0 );
	float totalWeight = 0.0;
	float envMapDim = float( textureSize( samplerEnv, 0 ).s );

	for ( uint i = 0u; i < numSamples; i++ ) {
		vec2 Xi = hammersley2d( i, numSamples );
		vec3 H = importanceSample_GGX( Xi, roughness, N );
		vec3 L = 2.0 * dot( V, H ) * H - V;
		float dotNL = clamp( dot( N, L ), 0.0, 1.0 );
		if ( dotNL > 0.0 ) {
			// https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
			float dotNH = clamp( dot( N, H ), 0.0, 1.0 );
			float dotVH = clamp( dot( V, H ), 0.0, 1.0 );

			float pdf = D_GGX( dotNH, roughness ) * dotNH / ( 4.0 * dotVH ) + 0.0001;
			// solid angle of the sample, and of one texel of the source
			float omegaS = 1.0 / ( float( numSamples ) * pdf );
			float omegaP = 4.0 * PI / ( 6.0 * envMapDim * envMapDim );
			// source mip level, with a bias of 1
			float mipLevel = max( 0.5 * log2( omegaS / omegaP ) + 1.0, 0.0 );
			color += textureLod( samplerEnv, L, mipLevel ).rgb * dotNL;
			totalWeight += dotNL;
		}
	}

	return color / totalWeight;
}

void main()
{
	// the direction of the texel in the cubemap coordinates (Vulkan face order and orientation)
	vec2 p = frag_tex_coord;
	vec3 normal;

	if ( consts.face == 0 )
		normal = vec3( 1.0, -p.y, -p.x );
	else if ( consts.face == 1 )
		normal = vec3( -1.0, -p.y, p.x );
	else if ( consts.face == 2 )
		normal = vec3( p.x, 1.0, p.y );
	else if ( consts.face == 3 )
		normal = vec3( p.x, -1.0, -p.y );
	else if ( consts.face == 4 )
		normal = vec3( p.x, -p.y, 1.0 );
	else
		normal = vec3( -p.x, -p.y, -1.0 );

	normal = normalize( normal );

	if ( consts.roughness == 0.0 )
		out_color = vec4( textureLod( samplerEnv, normal, 0.0 ).rgb, 1.0 );
	else
		out_color = vec4( prefilterEnvMap( normal, consts.roughness ), 1.0 );
}
