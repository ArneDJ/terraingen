#version 430 core

layout(binding = 0) uniform sampler2D heightmap;
layout(binding = 1) uniform sampler2D normalmap;

out vec4 fcolor;

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

void main(void)
{
	vec4 color = texture(heightmap, fragment.texcoord*0.001953125);
	//fcolor = vec4(color.r, color.r, color.r, 1.0);
	fcolor = texture(normalmap, fragment.texcoord*0.001953125);
}
