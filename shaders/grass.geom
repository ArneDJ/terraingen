#version 430 core

layout (points) in;
layout (triangle_strip) out;
layout (max_vertices = 45) out;

layout(binding = 0) uniform sampler2D heightmap;
uniform mat4 VIEW_PROJECT;
uniform  mat4 model;
uniform float mapscale;
uniform float amplitude;
uniform vec3 camerapos;

out GEOM {
	vec3 position;
	vec2 texcoord;
} geom;

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

mat4 rotationY(in float angle) 
{
	return mat4(cos(angle), 0, sin(angle), 0, 0, 1.0, 0, 0, -sin(angle), 0, cos(angle), 0, 0, 0, 0, 1);
}

vec2 pos_to_texcoord(vec3 position)
{
	float x = 0.5 * (position.x + 1.0);
	float y = 0.5 * (position.y + 1.0);

	return vec2(x, 1.0 - y);
}

void make_triangle(vec3 a, vec3 b, vec3 c, vec3 position)
{
	float stretch = 2.0;

	position.y = amplitude * texture(heightmap, mapscale*position.xz).r;
	mat4 rotation = rotationY(rand(position.xz)*2.0);
	a = vec3(rotation * vec4(a, 1.0));
	b = vec3(rotation * vec4(b, 1.0));
	c = vec3(rotation * vec4(c, 1.0));

	mat4 mvpMatrix = VIEW_PROJECT;

	gl_Position = mvpMatrix * vec4((a*stretch)+position, 1.0);
	geom.texcoord = pos_to_texcoord(a);
	geom.position = a+position;
	EmitVertex();
	gl_Position = mvpMatrix * vec4((b*stretch)+position, 1.0);
	geom.texcoord = pos_to_texcoord(b);
	geom.position = b+position;
	EmitVertex();
	gl_Position = mvpMatrix * vec4((c*stretch)+position, 1.0);
	geom.texcoord = pos_to_texcoord(c);
	geom.position = c+position;
	EmitVertex();
	EndPrimitive();
}

void main(void)
{
	vec3 position = gl_in[0].gl_Position.xyz;
	// individual grass blade
	vec3 a = vec3(0.0, -1.0, 0.0);
	vec3 b = vec3(0.19, -1.0, 0.0);
	vec3 c = vec3(0.11, 0.41, 0.0);
	vec3 d = vec3(0.28, 0.41, 0.0);
	vec3 e = vec3(0.28, 0.78, 0.0);
	vec3 f = vec3(0.42, 0.78, 0.0);
	vec3 g = vec3(0.58, 1.0, 0.0);

	float dist = distance(camerapos, position);
	//float blending = 1.0 / (0.01*dist);

	if (dist < 150.0) {
		make_triangle(a, b, c, position);
		make_triangle(b, c, d, position);
		make_triangle(c, d, e, position);
		make_triangle(d, e, f, position);
		make_triangle(e, f, g, position);

		vec3 offset1 = vec3(1.0, 0.0, 0.0);
		vec3 offset2 = vec3(0.0, 0.0, 1.0);
		make_triangle(a, b, c, position+offset1);
		make_triangle(b, c, d, position+offset1);
		make_triangle(c, d, e, position+offset1);
		make_triangle(d, e, f, position+offset1);
		make_triangle(e, f, g, position+offset1);

		make_triangle(a, b, c, position+offset2);
		make_triangle(b, c, d, position+offset2);
		make_triangle(c, d, e, position+offset2);
		make_triangle(d, e, f, position+offset2);
		make_triangle(e, f, g, position+offset2);
	}

	// cardinal quad
	/*
	vec3 a = vec3(1.0, -1.0, 0.0);
	vec3 b = vec3(-1.0, -1.0, 0.0);
	vec3 c = vec3(-1.0, 1.0, 0.0);
	vec3 d = vec3(1.0, 1.0, 0.0);

	vec3 e = vec3(0.7, -1.0, 0.7);
	vec3 f = vec3(-0.7, -1.0, -0.7);
	vec3 g = vec3(-0.7, 1.0, -0.7);
	vec3 h = vec3(0.7, 1.0, 0.7);

	vec3 i = vec3(0.7, -1.0, -0.7);
	vec3 j = vec3(-0.7, -1.0, 0.7);
	vec3 k = vec3(-0.7, 1.0, 0.7);
	vec3 l = vec3(0.7, 1.0, -0.7);

	make_triangle(a, b, c);
	make_triangle(b, c, d);
	make_triangle(c, d, e);
	make_triangle(d, e, f);
	make_triangle(e, f, g);
	make_triangle(f, g, h);
	make_triangle(g, h, i);
	make_triangle(h, i, j);
	make_triangle(i, j, k);
	make_triangle(j, k, l);
	*/
}
