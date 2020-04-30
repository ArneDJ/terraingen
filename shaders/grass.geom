#version 430 core

layout (points) in;
layout (triangle_strip) out;
layout (max_vertices = 30) out;

layout(binding = 0) uniform sampler2D heightmap;
uniform mat4 VIEW_PROJECT;
uniform  mat4 model;
uniform float mapscale;
uniform float amplitude;

out GEOM {
	vec3 position;
	vec2 texcoord;
} geom;

vec2 pos_to_texcoord(vec3 position)
{
	float x = 0.5 * (position.x + 1.0);
	float y = 0.5 * (position.y + 1.0);

	return vec2(x, 1.0 - y);
}

void make_triangle(vec3 a, vec3 b, vec3 c)
{
	mat4 mvpMatrix = VIEW_PROJECT;
	float stretch = 2.0;

	vec3 point = gl_in[0].gl_Position.xyz;
	point.y = amplitude * texture(heightmap, mapscale*point.xz).r + 1.0;

	gl_Position = mvpMatrix * vec4((a*stretch)+point, 1.0);
	geom.texcoord = pos_to_texcoord(a);
	geom.position = a+point;
	EmitVertex();
	gl_Position = mvpMatrix * vec4((b*stretch)+point, 1.0);
	geom.texcoord = pos_to_texcoord(b);
	geom.position = b+point;
	EmitVertex();
	gl_Position = mvpMatrix * vec4((c*stretch)+point, 1.0);
	geom.texcoord = pos_to_texcoord(c);
	geom.position = c+point;
	EmitVertex();
	EndPrimitive();
}

void main(void)
{
	/*
	// individual grass blade
	vec3 a = vec3(0.0, 0.0, 0.0) + point;
	vec3 b = vec3(0.19, 0.0, 0.0) + point;
	vec3 c = vec3(0.11, 0.41, 0.0) + point;
	vec3 d = vec3(0.28, 0.41, 0.0) + point;
	vec3 e = vec3(0.28, 0.78, 0.0) + point;
	vec3 f = vec3(0.42, 0.78, 0.0) + point;
	vec3 g = vec3(0.58, 1.0, 0.0) + point;

	make_triangle(a, b, c);
	make_triangle(b, c, d);
	make_triangle(c, d, e);
	make_triangle(d, e, f);
	make_triangle(e, f, g);
	*/

	// cardinal quad
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
}
