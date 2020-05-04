#version 430 core

layout (location = 0) in vec4 position;

uniform mat4 MVP;

out float intensity;

void main(void)
{
	intensity = position.w;
	gl_Position = MVP * vec4(position.xyz, 1.0f);
}
