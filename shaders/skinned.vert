#version 430 core

#define MAX_JOINT_MATRICES 128

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in ivec4 joints;
layout(location = 4) in vec4 weights;

layout(binding = 5) uniform samplerBuffer model_matrix_tbo;
layout(binding = 6) uniform samplerBuffer joint_matrix_tbo;

uniform mat4 VIEW_PROJECT;
uniform mat4 model;
uniform int joint_count;

out VERTEX {
	vec3 worldpos;
	vec3 normal;
	vec2 texcoord;
} vertex;

mat4 fetch_joint_matrix(int joint)
{
	int base_index = gl_InstanceID * 4 * joint_count + (4 * joint);

	vec4 col1 = texelFetch(joint_matrix_tbo, base_index);
	vec4 col2 = texelFetch(joint_matrix_tbo, base_index + 1);
	vec4 col3 = texelFetch(joint_matrix_tbo, base_index + 2);
	vec4 col4 = texelFetch(joint_matrix_tbo, base_index + 3);

	return mat4(col1, col2, col3, col4);
}

void main(void)
{
	vertex.texcoord = texcoord;

	// fetch the columns from the texture buffer
	vec4 col1 = texelFetch(model_matrix_tbo, gl_InstanceID * 4);
	vec4 col2 = texelFetch(model_matrix_tbo, gl_InstanceID * 4 + 1);
	vec4 col3 = texelFetch(model_matrix_tbo, gl_InstanceID * 4 + 2);
	vec4 col4 = texelFetch(model_matrix_tbo, gl_InstanceID * 4 + 3);

	// now assemble the four columns into a matrix
	mat4 final_model = mat4(col1, col2, col3, col4);

	//mat4 skin_matrix = fetch_joint_matrix(1);
	mat4 skin_matrix = 
	weights.x * fetch_joint_matrix(int(joints.x)) +
	weights.y * fetch_joint_matrix(int(joints.y)) +
	weights.z * fetch_joint_matrix(int(joints.z)) +
	weights.w * fetch_joint_matrix(int(joints.w));
	/*
	mat4 skin_matrix =
	weights.x * u_joint_matrix[int(joints.x)] +
	weights.y * u_joint_matrix[int(joints.y)] +
	weights.z * u_joint_matrix[int(joints.z)] +
	weights.w * u_joint_matrix[int(joints.w)];
	*/

	vec4 pos = final_model * skin_matrix * vec4(position, 1.0);
	vertex.normal = normalize(mat3(transpose(inverse(final_model * skin_matrix))) * normal);
	vertex.worldpos = vec4(final_model * skin_matrix * vec4(position, 1.0)).xyz;
	gl_Position = VIEW_PROJECT * pos;
}
