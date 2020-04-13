#include <GL/glew.h>
#include <GL/gl.h>

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
	
void Terrain::genheightmap(size_t imageres, float freq)
{
	heightimage = new unsigned char[imageres*imageres];

	perlin_image(heightimage, imageres);

	heightmap = bind_texture_red(heightimage ,imageres);
}

void Terrain::display(void)
{
	glBindVertexArray(termesh.VAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, heightmap);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	glDrawArrays(GL_PATCHES, 0, termesh.ecount);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
