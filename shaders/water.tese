#version 430 core

layout(binding = 0) uniform sampler2D heightmap;

uniform mat4 view, project;
uniform mat4 shadownear;
uniform mat4 shadowmiddle;
uniform mat4 shadowfar;
uniform float amplitude;

layout(quads, fractional_even_spacing, ccw) in;

out TESSEVAL {
	vec3 position;
	vec2 texcoord;
	vec3 incident;
	vec4 shadowcoord[3];
 float zclipspace;
} tesseval;

void main(void)
{
	vec4 p1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.y);
	vec4 p2 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.y);
	vec4 pos = mix(p1, p2, gl_TessCoord.x);

	pos.y = 80.0;

	vec4 vertex = view * pos;

	tesseval.position = pos.xyz;
	tesseval.texcoord = pos.xz;
	tesseval.incident = inverse(mat3(view)) * vec3(vertex);
	tesseval.shadowcoord[0] = shadownear * pos;
 tesseval.shadowcoord[1] = shadowmiddle * pos;
 tesseval.shadowcoord[2] = shadowfar * pos;

	gl_Position = project * vertex;
	tesseval.zclipspace = gl_Position.z;
}
