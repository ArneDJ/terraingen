#version 430 core

layout (location = 0) out vec4 color;

in float intensity;

void main(void)
{
	color = vec4(1.0, 1.0, 1.0, 1.0);
}