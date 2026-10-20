// PBR shading of the lit stages, ported from JKSunny/EternalJK (branch pbr).
// Included by gen_frag.tmpl when PER_PIXEL_LIGHTING is defined.

#ifndef SHADER_PBR_GLSL
#define SHADER_PBR_GLSL

vec3 CalcNormal( in vec3 vertexNormal, in vec4 vertexTangent, in vec2 texCoord )
{
	if ( normal_texture_set > -1 ) {
		vec3 biTangent = vertexTangent.w * cross( vertexNormal, vertexTangent.xyz );
		vec3 n = texture( normal_texture, texCoord ).agb - vec3( 0.5 );

		n.xy *= u_normalScale.xy;
		n.z = sqrt( clamp( ( 0.25 - n.x * n.x ) - n.y * n.y, 0.0, 1.0 ) );
		n = n.x * vertexTangent.xyz + n.y * biTangent + n.z * vertexNormal;

		return normalize( n );
	}

	return normalize( vertexNormal );
}

vec3 Diffuse_Lambert( in vec3 diffuseColor )
{
	return diffuseColor * ( 1.0 / M_PI );
}

vec3 F_Schlick( in vec3 specularColor, in float VH )
{
	float Fc = pow( 1.0 - VH, 5.0 );
	// a reflectance under 2% has no Fresnel peak
	return clamp( 50.0 * specularColor.g, 0.0, 1.0 ) * Fc + ( 1.0 - Fc ) * specularColor;
}

float D_GGX( in float NH, in float a )
{
	float a2 = a * a;
	float d = ( NH * a2 - NH ) * NH + 1.0;
	return a2 / ( M_PI * d * d );
}

// Joint Smith term for GGX [Heitz 2014]
float V_SmithJointApprox( in float a, in float NV, in float NL )
{
	float Vis_SmithV = NL * ( NV * ( 1.0 - a ) + a );
	float Vis_SmithL = NV * ( NL * ( 1.0 - a ) + a );
	return 0.5 / ( Vis_SmithV + Vis_SmithL );
}

vec3 CalcSpecular( in vec3 specular, in float NH, in float NL, in float NE, in float VH, in float roughness )
{
	vec3  F = F_Schlick( specular, VH );
	float D = D_GGX( NH, roughness );
	float V = V_SmithJointApprox( roughness, NE, NL );

	return D * F * V;
}

vec3 CalcIBLContribution( in float roughness, in vec3 N, in vec3 E, in float NE, in vec3 specular )
{
	vec3 R = reflect( -E, N );
	R.y *= -1.0;

	vec3 cubeLightColor = textureLod( env_texture, R, roughness * 6.0 ).rgb;
	vec2 EnvBRDF = texture( brdflut_texture, vec2( NE, 1.0 - roughness ) ).rg;

	return cubeLightColor * ( specular * EnvBRDF.x + EnvBRDF.y );
}

// The light of the stage on its albedo. lightColor is the light that the GL chain multiplies
// the albedo with: on a flat surface the diffuse term gives the same result.
vec3 CalcPBR( in vec4 albedo, in vec3 lightColor, in vec3 ambientColor, in vec3 L, in vec2 texCoord )
{
	vec3 E = normalize( var_ViewDir.xyz );
	vec3 N = CalcNormal( normalize( var_Normal.xyz ), var_Tangent, texCoord );
	vec3 diffuse = albedo.rgb;

#if defined(USE_LIGHTMAP) || defined(USE_LIGHT_VERTEX)
	// The baked light holds the direct and the ambient light: give the direct light the
	// part that the geometric normal receives, the ambient light the rest.
	float surfNL = clamp( dot( normalize( var_Normal.xyz ), L ), 0.0, 1.0 );
	ambientColor = lightColor;
	lightColor /= max( surfNL, 0.25 );
	ambientColor = max( ambientColor - lightColor * surfNL, 0.0 );
#endif

	lightColor *= M_PI;

	vec3 specular;
	float roughness;
	float AO = 1.0;

	if ( physical_texture_set < 0 )
	{
		// metallic roughness workflow: occlusion, roughness, metalness, specular
		vec4 ORMS = texture( physical_texture, texCoord );
		ORMS.xyzw *= u_specularScale.zwxy;

		specular = mix( vec3( 0.08 ) * ORMS.w, diffuse, ORMS.z );
		diffuse *= 1.0 - ORMS.z;

		roughness = mix( 0.01, 1.0, ORMS.y );
		AO = min( ORMS.x, AO );
	}
	else
	{
		// specular gloss workflow
		vec4 specGloss = texture( physical_texture, texCoord );
		specular = specGloss.rgb * u_specularScale.xyz;
		roughness = mix( 1.0, 0.01, specGloss.a * ( 1.0 - u_specularScale.w ) );
	}

	ambientColor *= AO;

	vec3  H  = normalize( L + E );
	float NE = abs( dot( N, E ) ) + 1e-5;
	float NL = clamp( dot( N, L ), 0.0, 1.0 );
	float NH = clamp( dot( N, H ), 0.0, 1.0 );
	float VH = clamp( dot( E, H ), 0.0, 1.0 );

	vec3 Fd = Diffuse_Lambert( diffuse );
	vec3 Fs = CalcSpecular( specular, NH, NL, NE, VH, roughness ) * deluxe_specular_scale;

	vec3 color = lightColor * ( Fd + Fs ) * NL;
	color += ambientColor * diffuse;

	if ( env_texture_set > -1 )
		color += CalcIBLContribution( roughness, N, E, NE, specular * AO );

	return color;
}

#endif
