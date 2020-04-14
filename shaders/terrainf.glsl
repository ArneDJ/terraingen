#version 430 core

layout(binding = 0) uniform sampler2D heightmap;
layout(binding = 1) uniform sampler2D normalmap;
layout(binding = 3) uniform sampler2D grassmap;
layout(binding = 4) uniform sampler2D stonemap;
layout(binding = 5) uniform sampler2D snowmap;

uniform float mapscale;

out vec4 fcolor;

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

struct material {
	vec3 grass;
	vec3 stone;
	vec3 snow;
};

void main(void)
{
	const vec3 lightdirection = vec3(0.2, 0.5, 0.5);
	const vec3 ambient = vec3(0.5, 0.5, 0.5);
	const vec3 lightcolor = vec3(1.0, 0.9, 0.8);

	vec2 uv = fragment.texcoord * mapscale;
	float height = texture(heightmap, uv).r;
	vec3 normal = texture(normalmap, uv).rgb;
	normal.x = (normal.x * 2.0) - 1.0;
	normal.y = (normal.y * 2.0) - 1.0;
	normal.z = (normal.z * 2.0) - 1.0;

	material mat = material(
		texture(grassmap, 0.1*fragment.texcoord).rgb,
		texture(stonemap, 0.05*fragment.texcoord).rgb,
		texture(snowmap, 0.05*fragment.texcoord).rgb
	);

	float slope = 1.0 - normal.y;
	vec3 color = mix(mat.grass, mat.snow, smoothstep(0.55, 0.6, height));
	color = mix(color, mat.stone, smoothstep(0.05, 0.2, slope));

	float diffuse = max(0.0, dot(normal, lightdirection));

	vec3 scatteredlight = ambient + lightcolor * diffuse;
	color.rgb = min(color.rgb * scatteredlight, vec3(1.0));

	fcolor = vec4(color, 1.0);
	//fcolor = vec4(vec3(height), 1.0);
}
