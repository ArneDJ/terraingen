#version 430 core

layout(binding = 0) uniform sampler2D basemap;

out vec4 fcolor;

in VERTEX {
	vec3 position;
	vec2 texcoord;
} fragment;

void main(void)
{
	fcolor = texture(basemap, fragment.texcoord);
}

