#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
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
#define FAR_CLIP 1600.f

#define GRASS_DENSITY 10000
#define TREES_DENSITY 50000
#define FOG_DENSITY 0.015f
#define SHADOW_TEXTURE_SIZE 2048

Shader base_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/base.vert"},
		{GL_FRAGMENT_SHADER, "shaders/base.frag"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	glm::mat4 model = glm::mat4(1.f);
	shader.uniform_mat4("model", model);
	shader.uniform_vec3("fogcolor", glm::vec3(0.46, 0.7, 0.99));
	shader.uniform_float("fogfactor", FOG_DENSITY);

	return shader;
}

Shader grass_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/grass.vert"},
		{GL_GEOMETRY_SHADER, "shaders/grass.geom"},
		{GL_FRAGMENT_SHADER, "shaders/grass.frag"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	glm::mat4 model = glm::mat4(1.f);
	shader.uniform_mat4("model", model);
	shader.uniform_vec3("fogcolor", glm::vec3(0.46, 0.7, 0.99));
	shader.uniform_float("fogfactor", FOG_DENSITY);

	return shader;
}

Shader distant_tree_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/distant_tree.vert"},
		{GL_FRAGMENT_SHADER, "shaders/distant_tree.frag"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	glm::mat4 model = glm::mat4(1.f);
	shader.uniform_mat4("model", model);
	shader.uniform_vec3("fogcolor", glm::vec3(0.46, 0.7, 0.99));
	shader.uniform_float("fogfactor", FOG_DENSITY);

	return shader;
}

Shader terrain_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/terrain.vert"},
		{GL_TESS_CONTROL_SHADER, "shaders/terrain.tesc"},
		{GL_TESS_EVALUATION_SHADER, "shaders/terrain.tese"},
		{GL_FRAGMENT_SHADER, "shaders/terrain.frag"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);

	shader.bind();

	shader.uniform_vec3("fogcolor", glm::vec3(0.46, 0.7, 0.99));
	shader.uniform_float("fogfactor", FOG_DENSITY);

	return shader;
}

Shader skybox_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/skybox.vert"},
		{GL_FRAGMENT_SHADER, "shaders/skybox.frag"},
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
	{GL_VERTEX_SHADER, "shaders/depthmap.vert"},
	{GL_FRAGMENT_SHADER, "shaders/depthmap.frag"},
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

struct grass_patch {
	GLuint VAO;
	GLuint VBO; // vertex buffer object containing the root locations
	size_t count; // grass root count
	glm::vec2 min; // rectangle min
	glm::vec2 max; // rectangle max
};

struct grass_patch gen_grass_patch(const Terrain *terrain, glm::vec2 min, glm::vec2 max, size_t density)
{
	struct grass_patch grass = {
		.VAO = 0,
		.VBO = 0,
		.count = density,
		.min = min,
		.max = max,
	};

	const float mapscale = 0.5f;

	std::vector<glm::vec2> positions;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.f, 1.5f);
	std::uniform_real_distribution<> map_x(min.x, max.x);
	std::uniform_real_distribution<> map_z(min.y, max.y);

	for (int i = 0; i < density; i++) {
		float x = map_x(gen);
		float z = map_z(gen);
		float y = terrain->sampleheight(mapscale*x, mapscale*z);
		if (terrain->sampleslope(mapscale*x, mapscale*z) < 0.6 && y < 0.4) {
			glm::vec2 position = glm::vec2(x, z);
			positions.push_back(position);
		}
	}
	grass.count = positions.size();

	glGenVertexArrays(1, &grass.VAO);
	glBindVertexArray(grass.VAO);

	glGenBuffers(1, &grass.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, grass.VBO);
	glBufferStorage(GL_ARRAY_BUFFER, sizeof(glm::vec2)*positions.size(), &positions[0], 0);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	return grass;
}

void run_terraingen(SDL_Window *window)
{
	SDL_SetRelativeMouseMode(SDL_TRUE);

	Shader base = base_shader();
	Shader woods = distant_tree_shader();
	Shader undergrowth = grass_shader();
	Shader terra = terrain_shader();
	Shader sky = skybox_shader();
	Shader depthpass = shadow_shader();

	Skybox skybox = init_skybox();

	Shadow shadow {
		SHADOW_TEXTURE_SIZE,
	};

	Terrain terrain { 64, 32.f, 256.f };
	terrain.genheightmap(1024, 1.f);
	terrain.gennormalmap();
	terrain.genocclusmap();

	GLuint distortion_map = load_DDS_texture("media/textures/distortion.dds");
	struct grass_patch grass = gen_grass_patch(&terrain, glm::vec2(512.f, 512.f), glm::vec2(1536.f, 1536.f), 800000);

	gltf::Model tree_close { "media/models/tree.glb" };
	gltf::Model model { "media/models/dragon.glb" };
	gltf::Model duck { "media/models/samples/khronos/Duck/glTF-Binary/Duck.glb" };
	//gltf::Model duck { "media/models/iron_sphere.glb" };
	const size_t DUCK_INSTANCE_COUNT = 4096;
	duck.instance(DUCK_INSTANCE_COUNT);
	gltf::Model character { "media/models/character.glb" };
	//gltf::Model character { "media/models/samples/khronos/BrainStem/glTF-Binary/BrainStem.glb" };

	Camera cam { 
		glm::vec3(1024.f, 128.f, 1024.f),
		FOV,
		float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
		NEAR_CLIP,
		FAR_CLIP
	};

	float start = 0.f;
 	float end = 0.f;
	static float timer = 0.f;

	long frames = 0;

	SDL_Event event;
	while (event.type != SDL_QUIT) {
		while(SDL_PollEvent(&event));
		start = 0.001f * float(SDL_GetTicks());
		const float delta = start - end;
		cam.update(delta);

		glBindBuffer(GL_ARRAY_BUFFER, duck.instance_buffer);
		glm::mat4 *matrices = (glm::mat4 *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		int index = 0;
		for (int i = 0; i < 64; i++) {
			float a = 50.f * float(i) / 4.f;
			float b = 50.f * float(i) / 5.f;
			for (int j = 0; j < 64; j++) {
				float c = 50.f * float(j) / 6.f;
				glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(1000.f+a, 128.f+20*sin(start+b), 1000.f+c));
				//glm::mat4 R = glm::rotate(start, glm::vec3(0.0, 1.0, 0.0));
				glm::mat4 R = glm::rotate(start, glm::vec3(1.0, 0.0, 0.0));
				glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(0.05f));
				matrices[index++] = T * R * S;
			}
		}
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		if (frames % 4 == 0) {
			shadow.update(&cam, glm::vec3(0.5, 0.5, 0.5));
			shadow.enable();
			depthpass.bind();
			for (int i = 0; i < shadow.CASCADE_COUNT; i++) {
				shadow.binddepth(i);
				depthpass.uniform_mat4("VIEW_PROJECT", shadow.shadowspace[i]);
				depthpass.uniform_bool("instanced", false);
				tree_close.display(&depthpass, glm::vec3(1020.f, 50.f, 1020.f), 1.f);
				model.display(&depthpass, glm::vec3(1000.f, 50.f, 1000.f), 1.f);
				duck.display(&depthpass, glm::vec3(970.f, 50.f, 970.f), 5.f);
			}
			shadow.disable();
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		const glm::mat4 VIEW_PROJECT = cam.project * cam.view;
		sky.uniform_mat4("view", cam.view);
		terra.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		undergrowth.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		woods.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		base.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		base.uniform_vec3("camerapos", cam.eye);
		base.uniform_vec3("viewdir", cam.center);

		terra.bind();
		terra.uniform_float("amplitude", terrain.amplitude);
		terra.uniform_float("mapscale", 1.f / terrain.sidelength);
		terra.uniform_vec3("camerapos", cam.eye);
		terra.uniform_vec4("split", shadow.splitdepth);
		std::vector<glm::mat4> shadowspace {
		shadow.scalebias * shadow.shadowspace[0],
		shadow.scalebias * shadow.shadowspace[1],
		shadow.scalebias * shadow.shadowspace[2],
		shadow.scalebias * shadow.shadowspace[3],
		};
		terra.uniform_mat4_array("shadowspace", shadowspace);
		shadow.bindtextures(GL_TEXTURE10);
		terrain.display();


		model.display(&base, glm::vec3(1000.f, 50.f, 1000.f), 1.f);
		duck.display(&base, glm::vec3(970.f, 50.f, 970.f), 5.f);

		sky.bind();
		skybox.display();

		undergrowth.bind();
		undergrowth.uniform_float("mapscale", 1.f / terrain.sidelength);
		undergrowth.uniform_float("amplitude", terrain.amplitude);
		undergrowth.uniform_float("time", start);
		undergrowth.uniform_vec3("camerapos", cam.eye);
		undergrowth.uniform_vec4("split", shadow.splitdepth);
		undergrowth.uniform_mat4_array("shadowspace", shadowspace);
		glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(1024.f, 128.f, 1024.f));
		undergrowth.uniform_mat4("model", T);
		glDisable(GL_CULL_FACE);
		activate_texture(GL_TEXTURE0, GL_TEXTURE_2D, terrain.heightmap);
		activate_texture(GL_TEXTURE1, GL_TEXTURE_2D, terrain.normalmap);
		activate_texture(GL_TEXTURE2, GL_TEXTURE_2D, terrain.occlusmap);
		activate_texture(GL_TEXTURE3, GL_TEXTURE_2D, terrain.detailmap);
		activate_texture(GL_TEXTURE4, GL_TEXTURE_2D, distortion_map);
		glBindVertexArray(grass.VAO);
		glDrawArrays(GL_POINTS, 0, grass.count);
		glEnable(GL_CULL_FACE);

		SDL_GL_SwapWindow(window);
		end = start;
		frames++;
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
