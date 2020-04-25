#include <iostream>
#include <vector>
#include <random>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imp.h"
#include "dds.h"
#include "glwrapper.h"
#include "shader.h"
#include "camera.h"
#include "terrain.h"
#include "gltf.h"
#include "shadow.h"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define FOV 90.f
#define NEAR_CLIP 0.1f
#define FAR_CLIP 800.f

#define GRASS_AMOUNT 500000
#define TREES_DENSITY 50000
#define FOG_DENSITY 0.025f
#define SHADOW_TEXTURE_SIZE 4096

Shader base_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/basev.glsl"},
		{GL_FRAGMENT_SHADER, "shaders/basef.glsl"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	const float aspect = (float)WINDOW_WIDTH/(float)WINDOW_HEIGHT;
	glm::mat4 project = glm::perspective(glm::radians(FOV), aspect, NEAR_CLIP, FAR_CLIP);
	shader.uniform_mat4("project", project);

	glm::mat4 model = glm::mat4(1.f);
	shader.uniform_mat4("model", model);
	shader.uniform_vec3("fogcolor", glm::vec3(0.46, 0.7, 0.99));
	shader.uniform_float("fogfactor", FOG_DENSITY);

	return shader;
}

Shader grass_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/grassv.glsl"},
		{GL_FRAGMENT_SHADER, "shaders/grassf.glsl"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	const float aspect = (float)WINDOW_WIDTH/(float)WINDOW_HEIGHT;
	glm::mat4 project = glm::perspective(glm::radians(FOV), aspect, NEAR_CLIP, FAR_CLIP);
	shader.uniform_mat4("project", project);

	glm::mat4 model = glm::mat4(1.f);
	shader.uniform_mat4("model", model);
	shader.uniform_vec3("fogcolor", glm::vec3(0.46, 0.7, 0.99));
	shader.uniform_float("fogfactor", FOG_DENSITY);

	return shader;
}

Shader tree_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/treev.glsl"},
		{GL_FRAGMENT_SHADER, "shaders/treef.glsl"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	const float aspect = (float)WINDOW_WIDTH/(float)WINDOW_HEIGHT;
	glm::mat4 project = glm::perspective(glm::radians(FOV), aspect, NEAR_CLIP, FAR_CLIP);
	shader.uniform_mat4("project", project);

	glm::mat4 model = glm::mat4(1.f);
	shader.uniform_mat4("model", model);
	shader.uniform_vec3("fogcolor", glm::vec3(0.46, 0.7, 0.99));
	shader.uniform_float("fogfactor", FOG_DENSITY);

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
	glm::mat4 project = glm::perspective(glm::radians(FOV), aspect, NEAR_CLIP, FAR_CLIP);
	shader.uniform_mat4("project", project);
	shader.uniform_vec3("fogcolor", glm::vec3(0.46, 0.7, 0.99));
	shader.uniform_float("fogfactor", FOG_DENSITY);

	return shader;
}

Shader skybox_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/skyboxv.glsl"},
		{GL_FRAGMENT_SHADER, "shaders/skyboxf.glsl"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	const float aspect = (float)WINDOW_WIDTH/(float)WINDOW_HEIGHT;
	glm::mat4 project = glm::perspective(glm::radians(FOV), aspect, NEAR_CLIP, FAR_CLIP);
	shader.uniform_mat4("project", project);

	return shader;
}

Shader shadow_shader(void)
{
	struct shaderinfo pipeline[] = {
	{GL_VERTEX_SHADER, "shaders/depthmapv.glsl"},
	{GL_FRAGMENT_SHADER, "shaders/depthmapf.glsl"},
	{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	return shader;
}

Skybox init_skybox(void)
{
	const char *CUBEMAP_TEXTURES[6] = {
	"media/textures/skybox/dust_ft.tga",
	"media/textures/skybox/dust_bk.tga",
	"media/textures/skybox/dust_up.tga",
	"media/textures/skybox/dust_dn.tga",
	"media/textures/skybox/dust_rt.tga",
	"media/textures/skybox/dust_lf.tga",
	};

	GLuint cubemap = load_TGA_cubemap(CUBEMAP_TEXTURES);
	Skybox skybox { cubemap };

	return skybox;
}

void run_terraingen(SDL_Window *window)
{
	SDL_SetRelativeMouseMode(SDL_TRUE);

	Shader terra = terrain_shader();
	Shader trees = tree_shader();
	Shader sky = skybox_shader();

	Skybox skybox = init_skybox();

	Terrain terrain { 64, 32.f, 256.f };
	terrain.genheightmap(1024, 0.01);
	terrain.gennormalmap();
	terrain.genocclusmap();

	struct mesh tree = gen_cardinal_quads();
	GLuint tree_texture = load_DDS_texture("media/textures/tree.dds");

	const float invratio = 1.f / terrain.mapratio;
	//const float minpos = 0.25f * (invratio * terrain.sidelength); // max grass position
	//const float maxpos = 0.75f * (invratio * terrain.sidelength); // min grass position
	const float minpos = 0.f * (invratio * terrain.sidelength); // max grass position
	const float maxpos = 1.f * (invratio * terrain.sidelength); // min grass position

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.f, 1.57f);
	std::uniform_real_distribution<> size(5.f, 7.f);
	std::uniform_real_distribution<> map(minpos, maxpos);

	std::vector<glm::mat4> transforms;
	transforms.reserve(TREES_DENSITY);
	for (int i = 0; i < TREES_DENSITY; i++) {
		float x = map(gen);
		float z = map(gen);
		float y = terrain.sampleheight(x, z);
		if (terrain.sampleslope(x, z) < 0.6 && y < 0.35) {
			float angle = dis(gen);
			glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(x*terrain.mapratio, terrain.amplitude*y+dis(gen)+4.f, z*terrain.mapratio));
			glm::mat4 R = glm::rotate(angle, glm::vec3(0.0, 1.0, 0.0));
			glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(3.f, size(gen), 3.f));
			transforms.push_back(T * R * S);
		}
	}
	size_t instancecount = transforms.size();
	transforms.resize(instancecount);
	instance_static_VAO(tree.VAO, &transforms);

	Camera cam { 
		glm::vec3(1024.f, 128.f, 1024.f),
		FOV,
		float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
		NEAR_CLIP,
		FAR_CLIP
	};

	float start = 0.f;
 	float end = 0.f;

	SDL_Event event;
	while (event.type != SDL_QUIT) {
		while(SDL_PollEvent(&event));
		start = 0.001f * float(SDL_GetTicks());
		const float delta = start - end;
		cam.update(delta);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		terra.uniform_mat4("view", cam.view);
		sky.uniform_mat4("view", cam.view);
		trees.uniform_mat4("view", cam.view);

		terra.bind();
		terra.uniform_float("amplitude", terrain.amplitude);
		terra.uniform_float("mapscale", 1.f / terrain.sidelength);
		terra.uniform_vec3("camerapos", cam.eye);
		terrain.display();

		trees.bind();
		trees.uniform_float("mapscale", 1.f / terrain.sidelength);
		trees.uniform_vec3("camerapos", cam.eye);
	glDisable(GL_CULL_FACE);
	activate_texture(GL_TEXTURE4, GL_TEXTURE_2D, tree_texture);
	glBindVertexArray(tree.VAO);
	glDrawElementsInstanced(tree.mode, tree.ecount, GL_UNSIGNED_SHORT, NULL, instancecount);
	glEnable(GL_CULL_FACE);

		sky.bind();
		skybox.display();

		SDL_GL_SwapWindow(window);
		end = start;
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

	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(0xFFFF);
	glEnable(GL_DEPTH_TEST);
 	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	run_terraingen(window);

	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	exit(EXIT_SUCCESS);
}
