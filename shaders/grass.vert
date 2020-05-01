#version 430 core

layout(location = 0) in vec3 coordinate;

void main(void)
{
	gl_Position = vec4(coordinate, 1.0);
}
