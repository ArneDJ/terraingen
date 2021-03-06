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
#include "effects.h"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define FOV 90.f
#define NEAR_CLIP 0.1f
#define FAR_CLIP 1600.f

#define GRASS_DENSITY 1000000
#define FOG_DENSITY 0.015f

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

	Shader grass_program = grass_shader();
	Shader terrain_program = terrain_shader();
	Shader sky_program = skybox_shader();
	Shader cloud_program = cloud_shader();

	Skybox skybox = init_skybox();

	Terrain terrain = { 64, 32.f, 256.f };

	Clouds clouds = { terrain.sidelength, terrain.amplitude, 128, 0.03f, 0.5f, };

	Grass grass = {
		&terrain,
		GRASS_DENSITY,
		terrain.heightmap,
		terrain.normalmap,
		terrain.occlusmap,
		terrain.detailmap,
		load_DDS_texture("media/textures/distortion.dds"),
	};

	Camera cam = { 
		glm::vec3(1024.f, 128.f, 1024.f),
		FOV,
		float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
		NEAR_CLIP,
		FAR_CLIP
	};

	float start = 0.f;
 	float end = 0.f;
	unsigned long frames = 0;
	unsigned int msperframe = 0;

	SDL_Event event;
	while (event.type != SDL_QUIT) {
		while(SDL_PollEvent(&event));
		start = 0.001f * float(SDL_GetTicks());
		const float delta = start - end;
		cam.update(delta);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		const glm::mat4 VIEW_PROJECT = cam.project * cam.view;
		sky_program.uniform_mat4("view", cam.view);
		terrain_program.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		grass_program.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		cloud_program.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);

		terrain_program.bind();
		terrain_program.uniform_float("amplitude", terrain.amplitude);
		terrain_program.uniform_float("mapscale", 1.f / terrain.sidelength);
		terrain_program.uniform_vec3("camerapos", cam.eye);
		terrain.display();

		sky_program.bind();
		skybox.display();

		cloud_program.bind();
		cloud_program.uniform_float("time", start);
		clouds.display();

		grass_program.bind();
		grass_program.uniform_float("mapscale", 1.f / terrain.sidelength);
		grass_program.uniform_float("amplitude", terrain.amplitude);
		grass_program.uniform_float("time", start);
		grass_program.uniform_vec3("camerapos", cam.eye);
		grass.display();

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
		if (frames % 100 == 0) { 
			msperframe = (unsigned int)(delta*1000); 
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
