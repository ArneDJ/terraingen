#version 430 core

out vec4 fcolor;

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

void main(void)
{
	fcolor = vec4(1.0, 1.0, 1.0, 1.0);
}
