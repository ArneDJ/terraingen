#version 430 core

layout(binding = 0) uniform sampler2D heightmap;
layout(binding = 1) uniform sampler2D normalmap;
layout(binding = 2) uniform sampler2D occlusmap;
layout(binding = 3) uniform sampler2D detailmap;
layout(binding = 4) uniform sampler2D grassmap;
layout(binding = 5) uniform sampler2D dirtmap;
layout(binding = 6) uniform sampler2D stonemap;
layout(binding = 7) uniform sampler2D snowmap;

uniform float mapscale;
uniform vec3 camerapos;

out vec4 fcolor;

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

struct material {
	vec3 grass;
	vec3 dirt;
	vec3 stone;
	vec3 snow;
};

float random(in vec2 st) 
{
	return fract(sin(dot(st.xy, vec2(12.9898,78.233)))* 43758.5453123);
}

// Based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
float noise(in vec2 st) 
{
	vec2 i = floor(st);
	vec2 f = fract(st);

	// Four corners in 2D of a tile
	float a = random(i);
	float b = random(i + vec2(1.0, 0.0));
	float c = random(i + vec2(0.0, 1.0));
	float d = random(i + vec2(1.0, 1.0));

	vec2 u = f * f * (3.0 - 2.0 * f);

	return mix(a, b, u.x) + (c - a)* u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

#define OCTAVES 6
/*
float fbm(in vec2 st) 
{
	// Initial values
	float value = 0.0;
	float amplitude = .5;
	float frequency = 0.;
	//
	// Loop of octaves
	for (int i = 0; i < OCTAVES; i++) {
		value += amplitude * noise(st);
		st *= 2.;
		amplitude *= .5;
	}
	return value;
}
*/
float fbm(in vec2 _st) 
{
	float v = 0.0;
	float a = 0.5;
	vec2 shift = vec2(100.0);
	// Rotate to reduce axial bias
	mat2 rot = mat2(cos(0.5), sin(0.5),
		    -sin(0.5), cos(0.50));
	for (int i = 0; i < OCTAVES; ++i) {
		v += a * noise(_st);
		_st = rot * _st * 2.0 + shift;
		a *= 0.5;
	}
	return v;
}

vec3 fog(vec3 c, float dist, float height)
{
	vec3 fog_color = {0.46, 0.7, 0.99};
	float de = 0.035 * smoothstep(0.0, 3.3, 1.0 - height);
	float di = 0.035 * smoothstep(0.0, 5.5, 1.0 - height);
	float extinction = exp(-dist * de);
	float inscattering = exp(-dist * di);

	return c * extinction + fog_color * (1.0 - inscattering);
}

void main(void)
{
	const vec3 lightdirection = vec3(0.5, 0.5, 0.5);
	const vec3 ambient = vec3(0.5, 0.5, 0.5);
	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);

	const vec3 viewspace = vec3(distance(fragment.position.x, camerapos.x), distance(fragment.position.y, camerapos.y), distance(fragment.position.z, camerapos.z));

	vec2 uv = fragment.texcoord * mapscale;
	float height = texture(heightmap, uv).r;
	vec3 normal = texture(normalmap, uv).rgb;
	normal = (normal * 2.0) - 1.0;

	float slope = 1.0 - normal.y;

	vec3 detail = texture(detailmap, fragment.texcoord*0.02).rgb;
	detail  = (detail * 2.0) - 1.0;
	float detaily = detail.y;
	detail.y = detail.z;
	detail.z = detaily;

	normal = normalize((slope * detail) + normal);

	material mat = material(
		texture(grassmap, 0.1*fragment.texcoord).rgb,
		texture(dirtmap, 0.1*fragment.texcoord).rgb,
		texture(stonemap, 0.03*fragment.texcoord).rgb * vec3(0.6, 0.6, 0.6),
		texture(snowmap, 0.05*fragment.texcoord).rgb
	);

	vec2 st = 0.05 * fragment.position.xz;
	vec2 q = vec2(0.);
	q.x = fbm(st);
	q.y = fbm(st + vec2(1.0));
	vec2 r = vec2(0.);
	r.x = fbm( st + 20.0*q + vec2(1.7,9.2)+ 0.15);
	r.y = fbm( st + 20.0*q + vec2(8.3,2.8)+ 0.126);
	float strata = fbm(st+r);

	vec3 color = mix(mat.grass, mat.snow, smoothstep(0.45, 0.5, height + (0.1*strata)));
	vec3 rocks = mix(mat.dirt, mat.stone, smoothstep(0.25, 0.4, height));
	color = mix(color, rocks, smoothstep(0.3, 0.55, slope - (0.5*strata)));

	float diffuse = max(0.0, dot(normal, lightdirection));

	vec3 scatteredlight = ambient + lightcolor * diffuse;
	color.rgb = min(color.rgb * scatteredlight, vec3(1.0));

	float occlusion = texture(occlusmap, uv).r;
	color *= occlusion;

	color = fog(color, length(viewspace), height);

	float gamma = 1.6;
	color.rgb = pow(color.rgb, vec3(1.0/gamma));
	fcolor = vec4(color, 1.0);
}
