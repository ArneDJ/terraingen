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

Terrain::Terrain(size_t sidelen, float patchoffst, float amp) 
{
	sidelength = sidelen * patchoffst;
	amplitude = amp;
	termesh =  gen_patch_grid(sidelen, patchoffst);
	mapratio = 0.f;

	detailmap = load_DDS_texture("media/textures/detailmap.dds");

	tersurface.grass = load_DDS_texture("media/textures/grass.dds");
	tersurface.dirt = load_DDS_texture("media/textures/dirt.dds");
	tersurface.stone = load_DDS_texture("media/textures/stone.dds");
	tersurface.snow = load_DDS_texture("media/textures/snow.dds");
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

	terrain_image(image.data, imageres, freq);

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

Grass::Grass(const Terrain *ter, size_t density, GLuint texturebind)
{
	const float invratio = 1.f / ter->mapratio;
	const float minpos = 0.25f * (invratio * ter->sidelength); // max grass position
	const float maxpos = 0.75f * (invratio * ter->sidelength); // min grass position

	quads = gen_cardinal_quads();
	texture = texturebind;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.f, 1.57f);
	std::uniform_real_distribution<> map(minpos, maxpos);

	std::vector<glm::mat4> transforms;
	transforms.reserve(density);
	for (int i = 0; i < density; i++) {
		float x = map(gen);
		float z = map(gen);
		float y = ter->sampleheight(x, z);
		if (ter->sampleslope(x, z) < 0.6 && y < 0.4) {
			float angle = dis(gen);
			glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(x*ter->mapratio, ter->amplitude*y+dis(gen), z*ter->mapratio));
			glm::mat4 R = glm::rotate(angle, glm::vec3(0.0, 1.0, 0.0));
			glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(6.f, 2.f, 6.f));
			transforms.push_back(T * R * S);
		}
	}
	instancecount = transforms.size();
	transforms.resize(instancecount);
	instance_static_VAO(quads.VAO, &transforms);
}

void Grass::display(void) const
{
	glDisable(GL_CULL_FACE);
	activate_texture(GL_TEXTURE4, GL_TEXTURE_2D, texture);
	glBindVertexArray(quads.VAO);
	glDrawElementsInstanced(quads.mode, quads.ecount, GL_UNSIGNED_SHORT, NULL, instancecount);
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
