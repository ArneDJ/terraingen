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

#include "external/imgui/imgui.h"
#include "external/imgui/imgui_impl_sdl.h"
#include "external/imgui/imgui_impl_opengl3.h"

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

#define GRASS_DENSITY 800000
#define FOG_DENSITY 0.015f
#define SHADOW_TEXTURE_SIZE 2048

#define MAX_PARTICLES 1000

enum {
	PARTICLE_GROUP_SIZE = 128,
	PARTICLE_GROUP_COUNT = 8000,
	PARTICLE_COUNT = (PARTICLE_GROUP_SIZE * PARTICLE_GROUP_COUNT),
	MAX_ATTRACTORS = 64
};

GLuint VAO;
GLuint VBO;
GLuint position_buffer;
GLuint velocity_buffer;
GLuint position_tbo;
GLuint velocity_tbo;
GLuint attractor_buffer;

// Mass of the attractors
float attractor_masses[MAX_ATTRACTORS];

static inline float random_float()
{
	float res;
	unsigned int tmp;
	static unsigned int seed = 0xFFFF0C59;

	seed *= 16807;

	tmp = seed ^ (seed >> 4) ^ (seed << 15);

	*((unsigned int *)&res) = (tmp >> 9) | 0x3F800000;

	return (res - 1.0f);
}

static glm::vec3 random_vector(float minmag = 0.0f, float maxmag = 1.0f)
{
	glm::vec3 randomvec = glm::vec3(random_float() * 2.0f - 1.0f, random_float() * 2.0f - 1.0f, random_float() * 2.0f - 1.0f);
	randomvec = glm::normalize(randomvec);
	randomvec *= (random_float() * (maxmag - minmag) + minmag);

	return randomvec;
}

void init_buffers(void)
{
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// init position buffer
	glGenBuffers(1, &position_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * sizeof(glm::vec4), NULL, GL_DYNAMIC_COPY);

	glm::vec4 *positions = (glm::vec4 *)glMapBufferRange(GL_ARRAY_BUFFER, 0, PARTICLE_COUNT * sizeof(glm::vec4), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	for (int i = 0; i < PARTICLE_COUNT; i++) {
		positions[i] = glm::vec4(random_vector(-10.0f, 10.0f), random_float());
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);


	// init velocity buffer
	glGenBuffers(1, &velocity_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, velocity_buffer);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * sizeof(glm::vec4), NULL, GL_DYNAMIC_COPY);

	glm::vec4 *velocities = (glm::vec4 *)glMapBufferRange(GL_ARRAY_BUFFER, 0, PARTICLE_COUNT * sizeof(glm::vec4), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	for (int i = 0; i < PARTICLE_COUNT; i++) {
		velocities[i] = glm::vec4(random_vector(-0.1f, 0.1f), 0.0f);
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);

	glGenTextures(1, &position_tbo);
	glBindTexture(GL_TEXTURE_BUFFER, position_tbo);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, position_buffer);

	glGenTextures(1, &velocity_tbo);
	glBindTexture(GL_TEXTURE_BUFFER, velocity_tbo);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, velocity_buffer);

	// attractor buffer
	glGenBuffers(1, &attractor_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, attractor_buffer);
	glBufferData(GL_UNIFORM_BUFFER, 32 * sizeof(glm::vec4), NULL, GL_STATIC_DRAW);

	for (int i = 0; i < MAX_ATTRACTORS; i++) {
		attractor_masses[i] = 0.5f + random_float() * 0.5f;
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, attractor_buffer);

}

void init_vertexarray()
{
	//glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);

	glBindVertexArray(0);
}

void update_particles(const Shader *compute, float time, float delta)
{
	// update the buffer containing the attractor positions and masses
	glBindBuffer(GL_UNIFORM_BUFFER, attractor_buffer);

	glm::vec4 *attractors = (glm::vec4 *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, 32 * sizeof(glm::vec4), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	for (int i = 0; i < 32; i++) {
  attractors[i] = glm::vec4(sinf(time * (float)(i + 4) * 7.5f * 20.0f) * 50.0f,
   cosf(time * (float)(i + 7) * 3.9f * 20.0f) * 50.0f,
   sinf(time * (float)(i + 3) * 5.3f * 20.0f) * cosf(time * (float)(i + 5) * 9.1f) * 100.0f,
   attractor_masses[i]);
 	}

	glUnmapBuffer(GL_UNIFORM_BUFFER);

	// If dt is too large, the system could explode, so cap it to
 // some maximum allowed value
	if (delta >= 2.0f) {
		delta = 2.0f;
	}

	// Activate the compute program and bind the position and velocity buffers
	compute->bind();

	glBindImageTexture(0, velocity_tbo, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glBindImageTexture(1, position_tbo, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	// Set delta time
	compute->uniform_float("dt", delta);
	// Dispatch
	glDispatchCompute(PARTICLE_GROUP_COUNT, 1, 1);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

}

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

Shader cloud_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/cloud.vert"},
		{GL_FRAGMENT_SHADER, "shaders/cloud.frag"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);
	return shader;
}

Shader particle_shader(void)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, "shaders/particle.vert"},
		{GL_FRAGMENT_SHADER, "shaders/particle.frag"},
		{GL_NONE, NULL}
	};

	Shader shader(pipeline);
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

Shader compute_shader(void)
{
	struct shaderinfo pipeline[] = {
	{GL_COMPUTE_SHADER, "shaders/particle.comp"},
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

static inline void start_imguiframe(SDL_Window *window)
{
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();
}

static void init_imgui(SDL_Window *window, SDL_GLContext glcontext)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO(); (void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(window, glcontext);
	ImGui_ImplOpenGL3_Init("#version 430");
}

void run_terraingen(SDL_Window *window)
{
	SDL_SetRelativeMouseMode(SDL_TRUE);

	Shader base = base_shader();
	Shader undergrowth = grass_shader();
	Shader terra = terrain_shader();
	Shader sky = skybox_shader();
	Shader depthpass = shadow_shader();
	Shader cloud = cloud_shader();
	Shader compute = compute_shader();
	Shader particles = particle_shader();

	Skybox skybox = init_skybox();

	Shadow shadow = {
		SHADOW_TEXTURE_SIZE,
	};

	init_buffers();
	//init_vertexarray();

	Terrain terrain = { 64, 32.f, 256.f };
	terrain.genheightmap(1024, 1.f);
	terrain.gennormalmap();
	terrain.genocclusmap();

	Clouds clouds = {
		terrain.sidelength,
		terrain.amplitude,
		128,
		0.03f,
		0.5f,
	};

	Grass grass = {
		&terrain,
		GRASS_DENSITY,
		terrain.heightmap,
		terrain.normalmap,
		terrain.occlusmap,
		terrain.detailmap,
		load_DDS_texture("media/textures/distortion.dds"),
	};

	glm::vec3 translation = glm::vec3(1100.f, 38.f, 980.f);
	gltf::Model model = { "media/models/dragon.glb" };
	gltf::Model duck = { "media/models/samples/khronos/Duck/glTF-Binary/Duck.glb" };
	gltf::Model character = { "media/models/character.glb" };
	//gltf::Model character { "media/models/samples/khronos/Fox/glTF-Binary/Fox.glb" };
	//gltf::Model character { "media/models/samples/khronos/BrainStem/glTF-Binary/BrainStem.glb" };

	Camera cam = { 
		//glm::vec3(1024.f, 128.f, 1024.f),
		glm::vec3(0.f, 0.f, 0.f),
		FOV,
		float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
		NEAR_CLIP,
		FAR_CLIP
	};

	float start = 0.f;
 	float end = 0.f;
	static float timer = 0.f;
	unsigned long frames = 0;
	unsigned int msperframe = 0;

	SDL_Event event;
	while (event.type != SDL_QUIT) {
		while(SDL_PollEvent(&event));
		start = 0.001f * float(SDL_GetTicks());
		const float delta = start - end;
		cam.update(delta);

		/*
		static int item_current = 0;
		timer += delta;
		if (character.animations.empty() == false) {
			if (timer > character.animations[item_current].end) { timer -= character.animations[item_current].end; }
			character.update_animation(item_current, timer);
		}

		if (frames % 4 == 0) {
			shadow.update(&cam, glm::vec3(0.5, 0.5, 0.5));
			shadow.enable();
			depthpass.bind();
			for (int i = 0; i < shadow.CASCADE_COUNT; i++) {
				shadow.binddepth(i);
				depthpass.uniform_mat4("VIEW_PROJECT", shadow.shadowspace[i]);
				depthpass.uniform_bool("instanced", false);
				model.display(&depthpass, glm::vec3(1000.f, 50.f, 1000.f), 1.f);
				duck.display(&depthpass, glm::vec3(970.f, 50.f, 970.f), 5.f);
				//character.display(&depthpass, glm::vec3(1100.f, 38.f, 980.f), 1.f);
				character.display(&depthpass, translation, 1.f);
			}
			shadow.disable();
		}
		*/

		compute.bind();
		update_particles(&compute, 100.f*start, 100.f*delta);
		//glDispatchCompute(16, 1, 1);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		const glm::mat4 VIEW_PROJECT = cam.project * cam.view;
		sky.uniform_mat4("view", cam.view);
		terra.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		undergrowth.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		cloud.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		particles.uniform_mat4("MVP", VIEW_PROJECT * glm::mat4(1.0));
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

		/*
		model.display(&base, glm::vec3(1000.f, 50.f, 1000.f), 1.f);
		duck.display(&base, glm::vec3(970.f, 50.f, 970.f), 5.f);
		//character.display(&base, glm::vec3(1100.f, 38.f, 980.f), 1.f);
		character.display(&base, translation, 1.f);

		sky.bind();
		skybox.display();

		cloud.bind();
		cloud.uniform_float("time", start);
		clouds.display();

		undergrowth.bind();
		undergrowth.uniform_float("mapscale", 1.f / terrain.sidelength);
		undergrowth.uniform_float("amplitude", terrain.amplitude);
		undergrowth.uniform_float("time", start);
		undergrowth.uniform_vec3("camerapos", cam.eye);
		undergrowth.uniform_vec4("split", shadow.splitdepth);
		undergrowth.uniform_mat4_array("shadowspace", shadowspace);
		grass.display();
		*/

		particles.bind();
		glBindVertexArray(VAO);
		//glPointSize(2.f);
		glDrawArrays(GL_POINTS, 0, PARTICLE_COUNT);


		// debug UI
		start_imguiframe(window);

		ImGui::Begin("Debug");
		ImGui::SetWindowSize(ImVec2(400, 200));
		ImGui::Text("%d ms per frame", msperframe);
		ImGui::Text("camera position: %.2f, %.2f, %.2f", cam.eye.x, cam.eye.y, cam.eye.z);

		//if (ImGui::Button("Exit")) { running = false; }

		ImGui::End();

		// Render dear imgui into screen
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);
		end = start;
		frames++;
		if (frames > 100) { 
			msperframe = (unsigned int)(delta*1000); 
			frames = 0; 
		}
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

	init_imgui(window, glcontext);

	run_terraingen(window);

	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	exit(EXIT_SUCCESS);
}
