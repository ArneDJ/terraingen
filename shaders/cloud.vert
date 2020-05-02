#version 430

uniform mat4 VIEW_PROJECT;

layout(location = 0) in vec3 position;
layout(location = 2) in vec2 texcoord;

out VERTEX {
	vec2 texcoord;
} vertex;

void main(void)
{
	vertex.texcoord = texcoord;
	gl_Position = VIEW_PROJECT * vec4(position, 1.0);
}
