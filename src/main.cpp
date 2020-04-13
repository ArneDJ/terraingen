#include <iostream>
#include <vector>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "imp.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 960
#define FOV 90.f
#define NEAR 0.1f
#define FAR 400.f

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

struct mesh {
	GLuint VAO, VBO, EBO;
	GLenum mode; // rendering mode
	GLsizei ecount; // element count
	GLenum etype; // element type for indices
	bool indexed;
};

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

	const GLfloat colors[] = {
		1.f, 1.f, 1.f,
		1.f, 1.f, 0.f,
		1.f, 0.f, 1.f,
		1.f, 0.f, 0.f,
		0.f, 1.f, 1.f,
		0.f, 1.f, 0.f,
		0.f, 0.f, 1.f,
		0.5f, 0.5f, 0.5f
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
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions) + sizeof(colors), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(positions), positions);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(positions), sizeof(colors), colors);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sizeof(positions)));

	return cube;
}

Shader base_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/vertex.glsl"},
		{GL_FRAGMENT_SHADER, "shaders/fragment.glsl"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	const float aspect = (float)WINDOW_WIDTH/(float)WINDOW_HEIGHT;
	glm::mat4 project = glm::perspective(glm::radians(FOV), aspect, NEAR, FAR);
	shader.uniform_mat4("project", project);

	glm::mat4 model = glm::mat4(1.f);
	shader.uniform_mat4("model", model);

	return shader;
}

Shader terrain_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/terrainv.glsl"},
		{GL_TESS_CONTROL_SHADER, "shaders/terraintc.glsl"},
		{GL_TESS_EVALUATION_SHADER, "shaders/terrainte.glsl"},
		{GL_FRAGMENT_SHADER, "shaders/terrainf.glsl"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	const float aspect = (float)WINDOW_WIDTH/(float)WINDOW_HEIGHT;
	glm::mat4 project = glm::perspective(glm::radians(FOV), aspect, NEAR, FAR);
	shader.uniform_mat4("project", project);

	return shader;
}

GLuint perlin_texture(void)
{
	size_t sidelength = 512;
	unsigned char *image = new unsigned char[sidelength*sidelength];

	perlin_image(image, sidelength);

	GLuint texture;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, sidelength, sidelength);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sidelength, sidelength, GL_RED, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);
	delete [] image;

	return texture;
}

void delete_mesh(const struct mesh *m)
{
	glDeleteBuffers(1, &m->EBO);
	glDeleteBuffers(1, &m->VBO);
	glDeleteVertexArrays(1, &m->VAO);
}

void run_terraingen(SDL_Window *window)
{
	SDL_SetRelativeMouseMode(SDL_TRUE);

	Shader shader = base_shader();
	Shader terrain = terrain_shader();
	GLuint heightmap = perlin_texture();
	Camera cam{glm::vec3(1.f, 1.f, 1.f)};
	struct mesh ter = gen_patch_grid(32, 16.f);
	struct mesh cube = gen_mapcube();

	SDL_Event event;
	while (event.type != SDL_QUIT) {
		while(SDL_PollEvent(&event));
		cam.update(0.01f);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 view = cam.view();
		terrain.uniform_mat4("view", view);
		shader.uniform_mat4("view", view);

		glBindVertexArray(cube.VAO);
		glDrawElements(cube.mode, cube.ecount, GL_UNSIGNED_SHORT, NULL);

		terrain.bind();
		glBindVertexArray(ter.VAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, heightmap);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glPatchParameteri(GL_PATCH_VERTICES, 4);
		glDrawArrays(GL_PATCHES, 0, ter.ecount);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		SDL_GL_SwapWindow(window);
	}

	delete_mesh(&ter);
	delete_mesh(&cube);
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window *window = SDL_CreateWindow("terraingen", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
	if (window == NULL) {
		printf("Could not create window: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_GLContext glcontext = SDL_GL_CreateContext(window);

	GLenum err = glewInit();
	if (err != GLEW_OK) {
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		exit(EXIT_FAILURE);
	}

	glClearColor(0.f, 0.f, 0.f, 1.f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(0xFFFF);

	run_terraingen(window);

	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	exit(EXIT_SUCCESS);
}
