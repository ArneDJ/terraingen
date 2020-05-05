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
#include "effects.h"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define FOV 90.f
#define NEAR_CLIP 0.1f
#define FAR_CLIP 1600.f

#define GRASS_DENSITY 800000
#define FOG_DENSITY 0.015f
#define SHADOW_TEXTURE_SIZE 2048

#define INSTANCE_COUNT 10000

Shader base_shader(const char *vertpath, const char *fragpath)
{
	struct shaderinfo pipeline[] = {
		{GL_VERTEX_SHADER, vertpath},
		{GL_FRAGMENT_SHADER, fragpath},
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

GLuint model_matrix_tbo;
GLuint model_matrix_buffer;

void instance_tbo(void)
{
	glGenTextures(1, &model_matrix_tbo);
	glBindTexture(GL_TEXTURE_BUFFER, model_matrix_tbo);

	glGenBuffers(1, &model_matrix_buffer);
	glBindBuffer(GL_TEXTURE_BUFFER, model_matrix_buffer);
	glBufferData(GL_TEXTURE_BUFFER, INSTANCE_COUNT * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, model_matrix_buffer);
}

void run_terraingen(SDL_Window *window)
{
	SDL_SetRelativeMouseMode(SDL_TRUE);

	Shader base_program = base_shader("shaders/base.vert", "shaders/base.frag");
	Shader skinned_program = base_shader("shaders/skinned.vert", "shaders/skinned.frag");
	Shader grass_program = grass_shader();
	Shader terrain_program = terrain_shader();
	Shader sky_program = skybox_shader();
	Shader depth_program = shadow_shader();
	Shader cloud_program = cloud_shader();

	Skybox skybox = init_skybox();

	Shadow shadow = { SHADOW_TEXTURE_SIZE, };

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
	character.instanced = true;
	character.instance_count = INSTANCE_COUNT;
	instance_tbo();
	//gltf::Model character { "media/models/samples/khronos/Fox/glTF-Binary/Fox.glb" };
	//gltf::Model character { "media/models/samples/khronos/BrainStem/glTF-Binary/BrainStem.glb" };

	Camera cam = { 
		glm::vec3(1024.f, 128.f, 1024.f),
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

		static int item_current = 0;
		timer += delta;
		if (character.animations.empty() == false) {
			if (timer > character.animations[item_current].end) { timer -= character.animations[item_current].end; }
			character.update_animation(item_current, timer);
		}

		// Set model matrices for each instance
		glm::mat4 matrices[INSTANCE_COUNT];
		int index = 0;
		for (int i = 0; i < 100; i++) {
				float a = 50.f * float(i) / 4.f;
				float b = 50.f * float(i) / 5.f;
				for (int j = 0; j < 100; j++) {
					float c = 50.f * float(j) / 6.f;
					glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(1000.f+a, 128.f+20*sin(start+b), 1000.f+c));
					//glm::mat4 R = glm::rotate(start, glm::vec3(0.0, 1.0, 0.0));
					glm::mat4 R = glm::rotate(start, glm::vec3(1.0, 0.0, 0.0));
					glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(0.05f));
					matrices[index++] = T * R;
			}
		}
		// Bind the weight VBO and change its data
		glBindBuffer(GL_TEXTURE_BUFFER, model_matrix_buffer);
		glBufferData(GL_TEXTURE_BUFFER, INSTANCE_COUNT*sizeof(glm::mat4), matrices, GL_DYNAMIC_DRAW);

		if (frames % 4 == 0) {
			shadow.update(&cam, glm::vec3(0.5, 0.5, 0.5));
			shadow.enable();
			depth_program.bind();
			for (int i = 0; i < shadow.CASCADE_COUNT; i++) {
				shadow.binddepth(i);
				depth_program.uniform_mat4("VIEW_PROJECT", shadow.shadowspace[i]);
				depth_program.uniform_bool("instanced", false);
				model.display(&depth_program, glm::vec3(1000.f, 50.f, 1000.f), 1.f);
				duck.display(&depth_program, glm::vec3(970.f, 50.f, 970.f), 5.f);
				//character.display(&depth_program, glm::vec3(1100.f, 38.f, 980.f), 1.f);
			}
			shadow.disable();
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		const glm::mat4 VIEW_PROJECT = cam.project * cam.view;
		sky_program.uniform_mat4("view", cam.view);
		terrain_program.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		grass_program.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		cloud_program.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		skinned_program.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		skinned_program.uniform_vec3("camerapos", cam.eye);
		skinned_program.uniform_vec3("viewdir", cam.center);
		base_program.uniform_mat4("VIEW_PROJECT", VIEW_PROJECT);
		base_program.uniform_vec3("camerapos", cam.eye);
		base_program.uniform_vec3("viewdir", cam.center);

		terrain_program.bind();
		terrain_program.uniform_float("amplitude", terrain.amplitude);
		terrain_program.uniform_float("mapscale", 1.f / terrain.sidelength);
		terrain_program.uniform_vec3("camerapos", cam.eye);
		terrain_program.uniform_vec4("split", shadow.splitdepth);
		std::vector<glm::mat4> shadowspace {
			shadow.scalebias * shadow.shadowspace[0],
			shadow.scalebias * shadow.shadowspace[1],
			shadow.scalebias * shadow.shadowspace[2],
			shadow.scalebias * shadow.shadowspace[3],
		};
		terrain_program.uniform_mat4_array("shadowspace", shadowspace);
		shadow.bindtextures(GL_TEXTURE10);
		terrain.display();

		model.display(&base_program, glm::vec3(1000.f, 50.f, 1000.f), 1.f);
		duck.display(&base_program, glm::vec3(970.f, 50.f, 970.f), 5.f);
		//character.display(&base_program, glm::vec3(1100.f, 38.f, 980.f), 1.f);

		activate_texture(GL_TEXTURE5, GL_TEXTURE_BUFFER, model_matrix_tbo);
		character.display(&skinned_program, translation, 1.f);

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
		grass_program.uniform_vec4("split", shadow.splitdepth);
		grass_program.uniform_mat4_array("shadowspace", shadowspace);
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
