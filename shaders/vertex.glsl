#version 430 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

uniform mat4 project, view, model;

out vec3 vcolor;

void main(void)
{
	vcolor = color;
	gl_Position = project * view * model * vec4(position, 1.0);
}
