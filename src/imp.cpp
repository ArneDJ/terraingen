#include <iostream>
#include <noise/noise.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "imp.h"

#define RED_CHANNEL 1
#define RGB_CHANNEL 3

static inline float sample_height(int x, int y, const struct rawimage *image)
{
	if (x < 0 || y < 0 || x > (image->width-1) || y > (image->height-1)) {
		return 0.f; }

	int index = y * image->width * image->nchannels + x * image->nchannels;

	return image->data[index] / 255.f;
}

static glm::vec3 filter_normal(int x, int y, const struct rawimage *image)
{
	const float strength = 8.f; // sobel filter strength

	float T = sample_height(x, y + 1, image);
	float TR = sample_height(x + 1, y + 1, image);
	float TL = sample_height(x - 1, y + 1, image);
	float B = sample_height(x, y - 1, image);
	float BR = sample_height(x + 1, y - 1, image);
	float BL = sample_height(x - 1, y - 1, image);
	float R = sample_height(x + 1, y, image);
	float L = sample_height(x - 1, y, image);

	// sobel filter
	const float dX = (TR + 2.f * R + BR) - (TL + 2.f * L + BL);
	const float dY = (BL + 2.f * B + BR) - (TL + 2.f * T + TR);
	const float dZ = 1.f / strength;

	// in OpenGL the Y axis is up so switch it with the Z axis
	// the X axis has to be inverted too
	glm::vec3 normal(-dX, dZ, dY);
	normal = glm::normalize(normal);
	// convert to positive values to store in a texture
	normal.x = (normal.x + 1.f) / 2.f;
	normal.y = (normal.y + 1.f) / 2.f;
	normal.z = (normal.z + 1.f) / 2.f;

	return normal;
}

struct rawimage gen_normalmap(const struct rawimage *heightmap)
{
	struct rawimage normalmap {
		.data = new unsigned char[heightmap->width * heightmap->height * RGB_CHANNEL],
		.nchannels = RGB_CHANNEL,
		.width = heightmap->width,
		.height = heightmap->height
	};

	for (int x = 0; x < heightmap->width; x++) {
		for (int y = 0; y < heightmap->height; y++) {
			const unsigned int index = y * normalmap.width * RGB_CHANNEL + x * RGB_CHANNEL;
			const glm::vec3 normal = filter_normal(x, y, heightmap);
			normalmap.data[index] = normal.x * 255.f;
			normalmap.data[index+1] = normal.y * 255.f;
			normalmap.data[index+2] = normal.z * 255.f;
		}
	}

	return normalmap;
}

void perlin_image(unsigned char *image, size_t sidelength, float freq)
{
	using namespace noise;

	module::Perlin perlin;
	perlin.SetFrequency(freq);
	perlin.SetOctaveCount(6); // default is 6
	perlin.SetLacunarity(2.0); // default is 2
	perlin.SetSeed(0); // default is 0

	float max = 1.f;
	float min = 0.f;
	unsigned int index = 0;
	for (int i = 0; i < sidelength; i++) {
		for (int j = 0; j < sidelength; j++) {
			double val = (perlin.GetValue(i, j, 1.f) + 1.f) * 0.5f;
			if (val > max) { max = val; }
			if (val < min) { min = val; }
			val = glm::clamp(float(val), 0.f, 1.f); 
			image[index++] = val * 255.f;
		}
	}
	std::cout << "max value " << max << std::endl;
	std::cout << "min value " << min << std::endl;
}
