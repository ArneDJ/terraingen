#version 430

uniform mat4 VIEW_PROJECT;
uniform mat4 tc_rotate;

layout(location = 0) in vec3 position;
layout(location = 2) in vec2 texcoord;

out VERTEX {
	vec2 texcoord;
	vec3 position;
} vertex;

void main(void)
{
	float tiling = 1.0 / 2048.0;

	vec4 worldposition = tc_rotate * vec4(position, 1.0);

	vertex.position = tiling * worldposition.stp;
	vertex.texcoord = texcoord;
	gl_Position = VIEW_PROJECT * worldposition;
}
