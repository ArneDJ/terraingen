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

static struct mesh create_slices(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, size_t slices_count, float offset)
{
	struct mesh slices = {
		.VAO = 0, .VBO = 0, .EBO = 0,
		.mode = GL_TRIANGLES,
		.ecount = GLsizei(6*slices_count),
		.indexed = true
	};

	// starts with the top slice and goes down to the bottom slice
	std::vector<glm::vec3> positions;
	for (int i = 0; i < slices_count; i++) {
		float y = offset * i;
		positions.push_back(glm::vec3(d.x, d.y - y, d.z));
		positions.push_back(glm::vec3(c.x, c.y - y, c.z));
		positions.push_back(glm::vec3(a.x, a.y - y, a.z));
		positions.push_back(glm::vec3(b.x, b.y - y, b.z));
	}

	std::vector<GLushort> indices;
	for (int i = 0; i < slices_count; i++) {
		indices.push_back((i * 4) + 0);
		indices.push_back((i * 4) + 1);
		indices.push_back((i * 4) + 2);
		indices.push_back((i * 4) + 0);
		indices.push_back((i * 4) + 2);
		indices.push_back((i * 4) + 3);
	}

	glGenVertexArrays(1, &slices.VAO);
	glBindVertexArray(slices.VAO);

	glGenBuffers(1, &slices.EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, slices.EBO);
	glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort)*indices.size(), &indices[0], 0);

	glGenBuffers(1, &slices.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, slices.VBO);
	glBufferStorage(GL_ARRAY_BUFFER, sizeof(glm::vec3)*positions.size(), &positions[0], 0);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	return slices;
}

static GLuint create_cloud_texture(size_t texsize, float frequency, float cloud_distance)
{
	unsigned char *image = new unsigned char[texsize*texsize*texsize];
	billow_3D_image(image, texsize, frequency, cloud_distance);

	GLuint texture;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_3D, texture);

	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, texsize, texsize, texsize, 0, GL_RED, GL_UNSIGNED_BYTE, image);

	delete [] image;

	return texture;
}

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

	genheightmap(1024, 1.f);
	gennormalmap();
	genocclusmap();

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
	struct rawimage image = {
		.data = new unsigned char[imageres*imageres],
		.nchannels = 1,
		.width = imageres,
		.height = imageres
	};

	terrain_image(image.data, imageres, 333, freq);

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

Clouds::Clouds(size_t terrain_length, float terrain_amp, size_t texsize, float freq, float cloud_distance)
{
	float overcast = 0.25f * terrain_length;
	float height = 2.f * terrain_amp;
	slices = create_slices(glm::vec3(-overcast, height, terrain_length+overcast), glm::vec3(terrain_length+overcast, height, terrain_length+overcast), glm::vec3(-overcast, height, -overcast), glm::vec3(terrain_length+overcast, height, -overcast), 64, 2.f);

	texture = create_cloud_texture(texsize, freq, cloud_distance);
}

void Clouds::display(void)
{
	glDisable(GL_CULL_FACE);
	activate_texture(GL_TEXTURE0, GL_TEXTURE_3D, texture);
	glBindVertexArray(slices.VAO);
	glDrawElements(slices.mode, slices.ecount, GL_UNSIGNED_SHORT, NULL);
	glEnable(GL_CULL_FACE);
}
