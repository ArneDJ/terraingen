#version 430 core

layout(binding = 0) uniform samplerCube cubemap;
layout(binding = 4) uniform sampler2D normalmap;

uniform vec3 camerapos;
uniform vec3 fogcolor;
uniform float fogfactor;
uniform float time;

out vec4 fcolor;

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
	vec3 incident;
} fragment;


vec3 fog(vec3 c, float dist, float height)
{
	float de = fogfactor * smoothstep(0.0, 3.3, 1.0 - height);
	float di = fogfactor * smoothstep(0.0, 5.5, 1.0 - height);
	float extinction = exp(-dist * de);
	float inscattering = exp(-dist * di);

	return c * extinction + fogcolor * (1.0 - inscattering);
}

void main(void)
{
	const vec3 lightdirection = vec3(0.5, 0.5, 0.5);
	const vec3 ambient = vec3(0.5, 0.5, 0.5);
	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	const vec3 viewspace = vec3(distance(fragment.position.x, camerapos.x), distance(fragment.position.y, camerapos.y), distance(fragment.position.z, camerapos.z));

	vec2 D1 = vec2(0.5, 0.5) * (0.1*time);
	vec2 D2 = vec2(-0.5, -0.5) * (0.1*time);

	vec3 normal = texture(normalmap, 0.01*fragment.texcoord + D1).rgb;
	normal += texture(normalmap, 0.01*fragment.texcoord + D2).rgb;

	normal = (normal * 2.0) - 1.0;
	float normaly = normal.y;
	normal.y = normal.z;
	normal.z = normaly;
	normal = normalize(normal);

	const float Eta = 0.15; // Water
	vec3 worldIncident = normalize(fragment.incident);
	vec3 refraction = refract(worldIncident, normal, Eta);
	vec3 reflection = reflect(worldIncident, normal);

	float fresnel = Eta + (1.0 - Eta) * pow(max(0.0, 1.0 - dot(-worldIncident, normal)), 5.0);

	vec4 refractionColor = texture(cubemap, refraction);
	vec4 reflectionColor = texture(cubemap, reflection);

	fcolor = mix(refractionColor, reflectionColor, fresnel);
	fcolor.rgb *= vec3(0.9, 0.95, 1.0);
	//fcolor.rgb = fog(fcolor.rgb, length(viewspace), 0.5);
	fcolor.a = 0.95;
}

