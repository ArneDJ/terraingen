#include <iostream>
#include <vector>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "mesh.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

void instance_static_VAO(GLuint VAO, std::vector<glm::mat4> *transforms)
{
	glBindVertexArray(VAO);
	GLuint VBO;

	auto amount = transforms->size();

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, amount * sizeof(glm::mat4), transforms->data(), GL_STATIC_DRAW);

	for (int i = 0; i < amount; i++) {
		size_t size = 4 * sizeof(float);

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * size, BUFFER_OFFSET(0));

		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * size, BUFFER_OFFSET(size));
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * size, BUFFER_OFFSET(2 * size)); glEnableVertexAttribArray(6); glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * size, BUFFER_OFFSET(3 * size));

		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
		glVertexAttribDivisor(6, 1);

		glBindVertexArray(0);
	}

	glDeleteBuffers(1, &VBO);
}

// make a square patch grid along the x and z axis, needs a tessellation shader to render
struct mesh gen_patch_grid(const size_t sidelength, const float offset)
{
	struct mesh patch = {
		.VAO = 0, .VBO = 0, .EBO = 0,
		.mode = GL_PATCHES,
		.ecount = GLsizei(4 * sidelength * sidelength),
		.indexed = false
	};

	std::vector<glm::vec3> vertices;
	vertices.reserve(patch.ecount);

	glm::vec3 origin = glm::vec3(0.f, 0.f, 0.f);
	for (int x = 0; x < sidelength; x++) {
		for (int z = 0; z < sidelength; z++) {
			vertices.push_back(glm::vec3(origin.x, origin.y, origin.z));
			vertices.push_back(glm::vec3(origin.x+offset, origin.y, origin.z));
			vertices.push_back(glm::vec3(origin.x, origin.y, origin.z+offset));
			vertices.push_back(glm::vec3(origin.x+offset, origin.y, origin.z+offset));
			origin.x += offset;
		}
		origin.x = 0.f;
		origin.z += offset;
	}

	glGenVertexArrays(1, &patch.VAO);
	glBindVertexArray(patch.VAO);

	glGenBuffers(1, &patch.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, patch.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*vertices.size(), &vertices[0], GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glPatchParameteri(GL_PATCH_VERTICES, 4);

	glBindVertexArray(0);
	glDisableVertexAttribArray(0);

	return patch;
}

// 3 quads used for grass meshes
struct mesh gen_cardinal_quads(void)
{
	struct mesh quads {
		.VAO = 0, .VBO = 0, .EBO = 0,
		.mode = GL_TRIANGLES,
		.ecount = 18,
		.indexed = true
	};

	const GLfloat positions[] = {
		1.0, -1.0, 0.0,
		-1.0, -1.0, 0.0,
		-1.0, 1.0, 0.0,
		1.0, 1.0, 0.0,

		0.7, -1.0, 0.7,
		-0.7, -1.0, -0.7,
		-0.7, 1.0, -0.7,
		0.7, 1.0, 0.7,

		0.7, -1.0, -0.7,
		-0.7, -1.0, 0.7,
		-0.7, 1.0, 0.7,
		0.7, 1.0, -0.7,
	};

	const GLfloat texcoords[] = {
		1.0, 1.0,
		0.0, 1.0,
		0.0, 0.0,
		1.0, 0.0,

		1.0, 1.0,
		0.0, 1.0,
		0.0, 0.0,
		1.0, 0.0,

		1.0, 1.0,
		0.0, 1.0,
		0.0, 0.0,
		1.0, 0.0,
	};

	const GLushort indices[] = {
		0, 1, 2,
		0, 2, 3,

		4, 5, 6, 
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,
	};

	glGenVertexArrays(1, &quads.VAO);
	glBindVertexArray(quads.VAO);

	glGenBuffers(1, &quads.EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quads.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW); 

	glGenBuffers(1, &quads.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, quads.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions)+sizeof(texcoords), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(positions), positions);
 	glBufferSubData(GL_ARRAY_BUFFER, sizeof(positions), sizeof(texcoords), texcoords);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sizeof(positions)));

	return quads;
}

struct mesh gen_mapcube(void)
{
	struct mesh cube = {
		.VAO = 0, .VBO = 0, .EBO = 0,
		.mode = GL_TRIANGLE_STRIP,
		.ecount = 17,
		.indexed = false
	};
	// 8 corners of a cube, side length 2
	const GLfloat positions[] = {
		1.f, 1.f, 1.f,
		1.f, 1.f, -1.f,
		1.f, -1.f, 1.f,
		1.f, -1.f, -1.f,
		-1.f, 1.f, 1.f,
		-1.f, 1.f, -1.f,
		-1.f, -1.f, 1.f,
		-1.f, -1.f, -1.f, 
	};

	// indices for the triangle strips
	const GLushort indices[] = {
		0, 1, 2, 3, 6, 7, 4, 5,
		0xFFFF,
		2, 6, 0, 4, 1, 5, 3, 7
	};

	glGenVertexArrays(1, &cube.VAO);
	glBindVertexArray(cube.VAO);

	glGenBuffers(1, &cube.EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW); 

	glGenBuffers(1, &cube.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, cube.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	return cube;
}

