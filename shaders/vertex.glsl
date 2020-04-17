#version 430 core

layout(location = 0) in vec3 position;

uniform mat4 project, view, model;

void main(void)
{
	gl_Position = project * view * model * vec4(position, 1.0);
}
