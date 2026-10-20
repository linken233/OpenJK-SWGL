#version 450

// One triangle that covers the viewport; frag_tex_coord is the NDC position.

layout(location = 0) out vec2 frag_tex_coord;

out gl_PerVertex {
	vec4 gl_Position;
};

void main()
{
	vec2 position = vec2( 4.0 * float( gl_VertexIndex & 1 ) - 1.0, 2.0 * float( gl_VertexIndex & 2 ) - 1.0 );
	gl_Position = vec4( position, 0.0, 1.0 );
	frag_tex_coord = position;
}
