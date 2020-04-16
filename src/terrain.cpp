#include <GL/glew.h>
#include <GL/gl.h>

#include "dds.h"
#include "mesh.h"
#include "imp.h"
#include "terrain.h"

static GLuint bind_texture_red(unsigned char *image, size_t sidelength)
{
	GLuint texture;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, sidelength, sidelength);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sidelength, sidelength, GL_RED, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

static GLuint bind_texture_rgb(const struct rawimage *image)
{
	GLuint texture;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, image->width, image->height);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height, GL_RGB, GL_UNSIGNED_BYTE, image->data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

Terrain::Terrain(size_t sidelen, float patchoffst, float amp) 
{
	sidelength = sidelen;
	amplitude = amp;
	patchoffset = patchoffst;
	termesh =  gen_patch_grid(sidelen, patchoffst);

	tersurface.grass = load_DDS_texture("media/textures/grass.dds");
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

	heightmap = bind_texture_red(image.data, imageres);
	heightimage = image;
}

void Terrain::gennormalmap(void)
{
	normalimage = gen_normalmap(&heightimage);
	normalmap = bind_texture_rgb(&normalimage);
}

void Terrain::genocclusmap(void)
{
	occlusimage = gen_occlusmap(&heightimage);
	occlusmap = bind_texture_red(occlusimage.data, occlusimage.width);
}

void Terrain::display(void)
{
	glBindVertexArray(termesh.VAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, heightmap);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, normalmap);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, occlusmap);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, tersurface.grass);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, tersurface.stone);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, tersurface.snow);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	glDrawArrays(GL_PATCHES, 0, termesh.ecount);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
