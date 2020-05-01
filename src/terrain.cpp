#include <vector>
#include <random>
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
#include "terrain.h"

// fills a vertex buffer with the positions of the grass roots, to be used in a geometry shader
static struct mesh gen_grass_roots(const Terrain *terrain, glm::vec2 min, glm::vec2 max, size_t density)
{
	struct mesh grass = {
		.VAO = 0, .VBO = 0, .EBO = 0,
		.mode = GL_POINTS,
		.ecount = 0,
		.indexed = false
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
	grass.ecount = GLsizei(positions.size());

	glGenVertexArrays(1, &grass.VAO);
	glBindVertexArray(grass.VAO);

	glGenBuffers(1, &grass.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, grass.VBO);
	glBufferStorage(GL_ARRAY_BUFFER, sizeof(glm::vec2)*positions.size(), &positions[0], 0);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	return grass;
}

Terrain::Terrain(size_t sidelen, float patchoffst, float amp) 
{
	sidelength = sidelen * patchoffst;
	amplitude = amp;
	termesh =  gen_patch_grid(sidelen, patchoffst);
	mapratio = 0.f;

	detailmap = load_DDS_texture("media/textures/terrain/detailmap.dds");

	tersurface.grass = load_DDS_texture("media/textures/terrain/grass.dds");
	tersurface.dirt = load_DDS_texture("media/textures/terrain/dirt.dds");
	tersurface.stone = load_DDS_texture("media/textures/terrain/stone.dds");
	tersurface.snow = load_DDS_texture("media/textures/terrain/snow.dds");
}

Terrain::~Terrain(void) 
{
	if (heightimage.data != nullptr) { delete [] heightimage.data; }
	if (normalimage.data != nullptr) { delete [] normalimage.data; }
	if (occlusimage.data != nullptr) { delete [] occlusimage.data; }

	if (glIsTexture(heightmap) == GL_TRUE) { glDeleteTextures(1, &heightmap); }
	if (glIsTexture(normalmap) == GL_TRUE) { glDeleteTextures(1, &normalmap); }
	if (glIsTexture(occlusmap) == GL_TRUE) { glDeleteTextures(1, &occlusmap); }

	glDeleteBuffers(1, &termesh.VBO);
	glDeleteVertexArrays(1, &termesh.VAO);
};
	
void Terrain::genheightmap(size_t imageres, float freq)
{
	struct rawimage image {
		.data = new unsigned char[imageres*imageres],
		.nchannels = 1,
		.width = imageres,
		.height = imageres
	};

	terrain_image(image.data, imageres, 404, freq);

	heightmap = bind_texture(&image, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
	heightimage = image;
	mapratio = float(sidelength) / float(imageres);
}

void Terrain::gennormalmap(void)
{
	normalimage = gen_normalmap(&heightimage);
	normalmap = bind_texture(&normalimage, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
}

void Terrain::genocclusmap(void)
{
	occlusimage = gen_occlusmap(&heightimage);
	occlusmap = bind_texture(&occlusimage, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
}

void Terrain::display(void) const
{
	glBindVertexArray(termesh.VAO);

	activate_texture(GL_TEXTURE0, GL_TEXTURE_2D, heightmap);
	activate_texture(GL_TEXTURE1, GL_TEXTURE_2D, normalmap);
	activate_texture(GL_TEXTURE2, GL_TEXTURE_2D, occlusmap);
	activate_texture(GL_TEXTURE3, GL_TEXTURE_2D, detailmap);
	/* TODO replace with array texture */
	activate_texture(GL_TEXTURE4, GL_TEXTURE_2D, tersurface.grass);
	activate_texture(GL_TEXTURE5, GL_TEXTURE_2D, tersurface.dirt);
	activate_texture(GL_TEXTURE6, GL_TEXTURE_2D, tersurface.stone);
	activate_texture(GL_TEXTURE7, GL_TEXTURE_2D, tersurface.snow);

//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	glDrawArrays(GL_PATCHES, 0, termesh.ecount);
//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

float Terrain::sampleheight(float x, float z) const
{
	return sample_image((int)x, (int)z, &heightimage, 0);
}

float Terrain::sampleslope(float x, float z) const
{
	float slope = sample_image((int)x, (int)z, &normalimage, 1);
	return 1.f - ((slope * 2.f) - 1.f);
}

Grass::Grass(const Terrain *ter, size_t density, GLuint height, GLuint norm, GLuint occlus, GLuint detail, GLuint wind)
{
	const float minpos = 0.25 * ter->sidelength; // min grass position
	const float maxpos = 0.75 * ter->sidelength; // max grass position

	roots = gen_grass_roots(ter, glm::vec2(minpos, minpos), glm::vec2(maxpos, maxpos), density);
	heightmap = height;
	normalmap = norm;
	occlusmap = occlus;
	detailmap = detail;
	windmap = wind;
}

void Grass::display(void) const
{
	activate_texture(GL_TEXTURE0, GL_TEXTURE_2D, heightmap);
	activate_texture(GL_TEXTURE1, GL_TEXTURE_2D, normalmap);
	activate_texture(GL_TEXTURE2, GL_TEXTURE_2D, occlusmap);
	activate_texture(GL_TEXTURE3, GL_TEXTURE_2D, detailmap);
	activate_texture(GL_TEXTURE4, GL_TEXTURE_2D, windmap);
	glDisable(GL_CULL_FACE);
	glBindVertexArray(roots.VAO);
	glDrawArrays(GL_POINTS, 0, roots.ecount);
	glEnable(GL_CULL_FACE);
}

void Skybox::display(void) const
{
	glDepthFunc(GL_LEQUAL);
	activate_texture(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, cubemap);
	glBindVertexArray(cube.VAO);
	glDrawElements(cube.mode, cube.ecount, GL_UNSIGNED_SHORT, NULL);
	glDepthFunc(GL_LESS);
};
