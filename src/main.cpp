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
	struct mesh mesh = {0};
	mesh.mode = GL_PATCHES;
	mesh.ecount = 4 * sidelength * sidelength;
	mesh.indexed = false;

	std::vector<glm::vec3> vertices;
	vertices.reserve(mesh.ecount);

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

	glGenVertexArrays(1, &mesh.VAO);
	glBindVertexArray(mesh.VAO);

	glGenBuffers(1, &mesh.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*vertices.size(), &vertices[0], GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

	glPatchParameteri(GL_PATCH_VERTICES, 4);

	glBindVertexArray(0);
	glDisableVertexAttribArray(0);

	return mesh;
}

GLuint gen_triangle(void)
{
	const GLfloat vertices[] = {
		-0.5, -0.5, 0.0, 
		0.5, -0.5, 0.0,
		0.0, 0.5, 0.0,

		0.0, 0.0, 1.0,
		0.0, 1.0, 0.0,
		1.0, 0.0, 0.0,
	};

	GLuint VAO;

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	GLuint VBO;

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(9*sizeof(float)));

	glBindVertexArray(0);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDeleteBuffers(1, &VBO);

	return VAO;
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

void display(GLuint VAO)
{
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void run_terraingen(SDL_Window *window)
{
	SDL_SetRelativeMouseMode(SDL_TRUE);

	Shader shader = base_shader();
	Shader terrain = terrain_shader();
	Camera cam{glm::vec3(1.f, 1.f, 1.f)};
	GLuint triangle = gen_triangle();
	struct mesh ter = gen_patch_grid(32, 16.f);

	SDL_Event event;
	while (event.type != SDL_QUIT) {
		while(SDL_PollEvent(&event));
		cam.update(0.01f);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 view = cam.view();
		terrain.uniform_mat4("view", view);
		shader.uniform_mat4("view", view);

		display(triangle);

		terrain.bind();
		glBindVertexArray(ter.VAO);
		glPatchParameteri(GL_PATCH_VERTICES, 4);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDrawArrays(GL_PATCHES, 0, ter.ecount);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		SDL_GL_SwapWindow(window);
	}
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
	glEnable(GL_DEPTH_TEST);

	run_terraingen(window);

	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	exit(EXIT_SUCCESS);
}
