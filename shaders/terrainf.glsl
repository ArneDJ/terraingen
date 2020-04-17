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
		texture(stonemap, 0.03*fragment.texcoord).rgb,
		texture(snowmap, 0.05*fragment.texcoord).rgb
	);

	vec3 color = mix(mat.grass, mat.snow, smoothstep(0.45, 0.5, height));
	vec3 rocks = mix(mat.dirt, mat.stone, smoothstep(0.25, 0.4, height));
	color = mix(color, rocks, smoothstep(0.65, 0.85, slope));

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
