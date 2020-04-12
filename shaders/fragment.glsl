#version 430 core

out vec4 fcolor;

in vec3 vcolor;

void main(void)
{
	fcolor = vec4(vcolor, 1.0);
}

