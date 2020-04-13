#version 430 core

uniform samplerCube cubemap;

out vec4 fcolor;

in vec3 texcoords;
	in vec3 vcolor;

void main()
{    
	//fcolor = texture(cubemap, texcoords);
	fcolor = vec4(vcolor, 1.0);
}
