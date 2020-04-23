#version 430 core

#define MAX_JOINT_MATRICES 128

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in ivec4 joints;
layout(location = 4) in vec4 weights;

uniform mat4 projectview;
uniform mat4 project, view, model;
uniform mat4 u_joint_matrix[MAX_JOINT_MATRICES];
uniform bool skinned;

out VERTEX {
	vec3 worldpos;
	vec3 normal;
	vec2 texcoord;
} vertex;

void main(void)
{
	//vertex.normal = normal;
	vertex.texcoord = texcoord;

	if (skinned == true) {
		mat4 skin_matrix =
		weights.x * u_joint_matrix[int(joints.x)] +
		weights.y * u_joint_matrix[int(joints.y)] +
		weights.z * u_joint_matrix[int(joints.z)] +
		weights.w * u_joint_matrix[int(joints.w)];

		vec4 pos = model * skin_matrix * vec4(position, 1.0);
		vertex.normal = normalize(mat3(transpose(inverse(model * skin_matrix))) * normal);
		vertex.worldpos = vec4(model * skin_matrix * vec4(position, 1.0)).xyz;
		gl_Position = project * view * pos;
	} else {
		vertex.normal = normalize(mat3(transpose(inverse(model))) * normal);
		vertex.worldpos = vec4(model * vec4(position, 1.0)).xyz;
		gl_Position = project * view * model * vec4(position, 1.0);
	}
}
