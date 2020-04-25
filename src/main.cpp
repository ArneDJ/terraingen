#include <iostream>
#include <vector>
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

GLuint array_texture(void)
{
	GLuint texture = 0;

	GLsizei width = 2;
	GLsizei height = 2;
	GLsizei layerCount = 2;
	GLsizei mipLevelCount = 1;

	// Read you texels here. In the current example, we have 2*2*2 = 8 texels, with each texel being 4 GLubytes.
	GLubyte texels[32] =
	{
	// Texels for first image.
	0,   0,   0,   255,
	255, 0,   0,   255,
	0,   255, 0,   255,
	0,   0,   255, 255,
	// Texels for second image.
	255, 255, 255, 255,
	255, 255,   0, 255,
	0,   255, 255, 255,
	255, 0,   255, 255,
	};

	glGenTextures(1,&texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
	// Allocate the storage.
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipLevelCount, GL_RGBA8, width, height, layerCount);
	// Upload pixel data.
	// The first 0 refers to the mipmap level (level 0, since there's only 1)
	// The following 2 zeroes refers to the x and y offsets in case you only want to specify a subrectangle.
	// The final 0 refers to the layer index offset (we start from index 0 and have 2 levels).
	// Altogether you can specify a 3D box subset of the overall texture, but only one mip level at a time.
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, layerCount, GL_RGBA, GL_UNSIGNED_BYTE, texels);

	// Always set reasonable texture parameters
	glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D_ARRAY,0);

	return texture;
}

void run_terraingen(SDL_Window *window)
{
	SDL_SetRelativeMouseMode(SDL_TRUE);

	Shader base = base_shader();
	Shader undergrowth = grass_shader();
	Shader terra = terrain_shader();
	Shader sky = skybox_shader();

	Shader shadow = shadow_shader();

	Skybox skybox = init_skybox();

	Terrain terrain { 64, 32.f, 256.f };
	terrain.genheightmap(1024, 0.01);
	terrain.gennormalmap();
	terrain.genocclusmap();

	gltf::Model model { "media/models/dragon.glb" };
	gltf::Model duck { "media/models/samples/khronos/Duck/glTF-Binary/Duck.glb" };

	gltf::Model brainstem { "media/models/samples/khronos/BrainStem/glTF-Binary/BrainStem.glb" };

	const float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	Camera cam { 
		glm::vec3(1024.f, 128.f, 1024.f),
		FOV,
		aspect,
		NEAR_CLIP,
		FAR_CLIP
	};

	struct mesh quad = gen_quad();
	GLuint array_tex = array_texture();

	const glm::vec3 light_position = glm::normalize(glm::vec3(0.2f, 0.5f, 0.5f));
	Shadow cascaded { SHADOW_TEXTURE_SIZE };

	float start = 0.f;
 	float end = 0.f;
	static float timer = 0.f;

	SDL_Event event;
	while (event.type != SDL_QUIT) {
		while(SDL_PollEvent(&event));
		float time = 0.01 * float(SDL_GetTicks());
		start = 0.001f * SDL_GetTicks();
		const float delta = start - end;
		cam.update(delta);

		timer += delta;
		if (brainstem.animations.empty() == false) {
			if (timer > brainstem.animations[0].end) { timer -= brainstem.animations[0].end; }
			brainstem.updateAnimation(0, timer);
		}

		// Draw from the light's point of view
		cascaded.update(&cam, light_position);
		cascaded.enable();
		for (int i = 0; i < cascaded.cascade_count; i++) {
			cascaded.binddepth(i);
			shadow.uniform_mat4("view_projection", cascaded.shadowspace[i]);
			shadow.uniform_bool("instanced", false);
			model.display(&shadow, glm::vec3(1024.f, 128.f, 1024.f), 1.f);
			duck.display(&shadow, glm::vec3(1054.f, 128.f, 1036.f), 5.f);
			brainstem.display(&shadow, glm::vec3(1024.f, 128.f, 1054.f), 5.f);
		}
		cascaded.disable();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		glm::mat4 view = cam.view();
		base.uniform_mat4("view", view);
		terra.uniform_mat4("view", view);
		undergrowth.uniform_mat4("view", view);
		sky.uniform_mat4("view", view);

		terra.bind();
		terra.uniform_vec4("split", cascaded.splitdepth);
		terra.uniform_float("amplitude", terrain.amplitude);
		terra.uniform_float("mapscale", 1.f / terrain.sidelength);
		terra.uniform_vec3("camerapos", cam.eye);
		std::vector<glm::mat4> shadowspace {
		cascaded.scalebias * cascaded.shadowspace[0],
		cascaded.scalebias * cascaded.shadowspace[1],
		cascaded.scalebias * cascaded.shadowspace[2],
		cascaded.scalebias * cascaded.shadowspace[3],
		};
		terra.uniform_mat4_array("shadowspace", shadowspace);
  		cascaded.bindtextures(GL_TEXTURE10);
		terrain.display();

		model.display(&base, glm::vec3(1024.f, 128.f, 1024.f), 1.f);
		duck.display(&base, glm::vec3(1054.f, 128.f, 1036.f), 5.f);
		brainstem.display(&base, glm::vec3(1024.f, 128.f, 1054.f), 5.f);

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
