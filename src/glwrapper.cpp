#include <GL/glew.h>
#include <GL/gl.h>

#include "imp.h"
#include "glwrapper.h"

GLuint bind_texture(const struct rawimage *image, GLenum internalformat, GLenum format, GLenum type)
{
	GLuint texture;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, image->width, image->height);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height, format, type, image->data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

void activate_texture(GLenum unit, GLenum target, GLuint texture)
{
	glActiveTexture(unit);
	glBindTexture(target, texture);
}
