#version 430 core

layout (points) in;
layout (triangle_strip) out;
layout (max_vertices = 60) out;

layout(binding = 0) uniform sampler2D heightmap;
layout(binding = 4) uniform sampler2D windmap;

uniform mat4 VIEW_PROJECT;
uniform  mat4 model;
uniform float mapscale;
uniform float amplitude;
uniform float time;
uniform vec3 camerapos;

out GEOM {
	vec3 position;
	vec2 texcoord;
	float zclipspace;
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
	geom.zclipspace = gl_Position.z;
	EmitVertex();
	gl_Position = mvpMatrix * vec4((b*stretch)+position, 1.0);
	geom.texcoord = pos_to_texcoord(b);
	geom.position = b+position;
	geom.zclipspace = gl_Position.z;
	EmitVertex();
	gl_Position = mvpMatrix * vec4((c*stretch)+position, 1.0);
	geom.texcoord = pos_to_texcoord(c);
	geom.position = c+position;
	geom.zclipspace = gl_Position.z;
	EmitVertex();
	EndPrimitive();
}

mat3 AngleAxis3x3(float angle, vec3 axis)
{
    float s = sin(angle);
    float c = cos(angle);

    float t = 1 - c;
    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return mat3(
        t * x * x + c,      t * x * y - s * z,  t * x * z + s * y,
        t * x * y + s * z,  t * y * y + c,      t * y * z - s * x,
        t * x * z - s * y,  t * y * z + s * x,  t * z * z + c
    );
}

const float frequency = 0.05;

void main(void)
{
	vec3 position = gl_in[0].gl_Position.xyz;
	float dist = distance(camerapos, position);
	//float blending = 1.0 / (0.01*dist);

	if (dist < 150.0) {

	vec2 uv = 0.005*position.xz + frequency * time;
	vec2 windsample = (texture(windmap, uv).rg * 2.0 - 1.0) * 2.0;
	vec3 wind = normalize(vec3(windsample.x, windsample.y, 0.0));
	mat3 rotation = AngleAxis3x3(windsample.x, wind) * AngleAxis3x3(windsample.y, wind);

	// individual grass blade
	vec3 a = vec3(0.0, -1.0, 0.0);
	vec3 b = vec3(0.19, -1.0, 0.0);
	vec3 c = vec3(0.11, 0.41, 0.0);
	vec3 d = rotation * vec3(0.28, 0.41, 0.0);
	vec3 e = rotation * vec3(0.28, 0.78, 0.0);
	vec3 f = rotation * vec3(0.42, 0.78, 0.0);
	vec3 g = rotation * vec3(0.58, 1.0, 0.0);

		make_triangle(a, b, c, position);
		make_triangle(b, c, d, position);
		make_triangle(c, d, e, position);
		make_triangle(d, e, f, position);
		make_triangle(e, f, g, position);

		vec3 offset1 = vec3(1.0, 0.0, 0.0);
		vec3 offset2 = vec3(0.0, 0.0, 1.0);
		vec3 offset3 = vec3(0.0, 0.0, -1.0);

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

		make_triangle(a, b, c, position+offset3);
		make_triangle(b, c, d, position+offset3);
		make_triangle(c, d, e, position+offset3);
		make_triangle(d, e, f, position+offset3);
		make_triangle(e, f, g, position+offset3);
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
