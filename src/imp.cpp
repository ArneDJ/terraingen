#include <iostream>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "external/fastnoise/FastNoise.h"
#include "external/heman/heman.h"
#include "imp.h"

#define RED_CHANNEL 1
#define RGB_CHANNEL 3

static inline float sample_height(int x, int y, const struct rawimage *image)
{
	if (x < 0 || y < 0 || x > (image->width-1) || y > (image->height-1)) {
		return 0.f; 
	}

	int index = y * image->width * image->nchannels + x * image->nchannels;

	return image->data[index] / 255.f;
}

static glm::vec3 filter_normal(int x, int y, const struct rawimage *image)
{
	const float strength = 64.f; // sobel filter strength

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

float sample_image(int x, int y, const struct rawimage *image, unsigned int channel)
{
	if (channel > image->nchannels) {
		std::cerr << "error: invalid channel to sample\n";
		return 0.f;
	}
	if (x < 0 || y < 0 || x > (image->width-1) || y > (image->height-1)) {
		return 0.f;
	}

	int index = y * image->width * image->nchannels + x * image->nchannels;

	return image->data[index+channel] / 255.f;
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

struct rawimage gen_occlusmap(const struct rawimage *heightmap)
{
	struct rawimage occlusmap {
		.data = new unsigned char[heightmap->width * heightmap->height],
		.nchannels = RED_CHANNEL,
		.width = heightmap->width,
		.height = heightmap->height
	};

	heman_image *retval = heman_import_u8(heightmap->width, heightmap->height, heightmap->nchannels, heightmap->data, 0, 1);;

	heman_image *occ = heman_lighting_compute_occlusion(retval);
	heman_export_u8(occ, 0, 1, occlusmap.data);

	heman_image_destroy(retval);
	heman_image_destroy(occ);

	return occlusmap;
}

/* 
 * preset: Old Mountains
 * noise type: Cellular
 * cellular distance function: natural
 * cellular return type: distance 2
 * gradient perturb amp: 50
 *
 * noise type: Simplex Fractal
 * fractal type: rigid multi
 * frequency: 0.002
 * octaves: 6
 * lacunarity: 2
 */
void terrain_image(unsigned char *image, size_t sidelength, float freq)
{
	FastNoise cellnoise;
	cellnoise.SetSeed(444);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Natural);
	cellnoise.SetFrequency(freq);
	cellnoise.SetCellularReturnType(FastNoise::Distance2);
	cellnoise.SetGradientPerturbAmp(20.f);

	FastNoise fractnoise;
	fractnoise.SetSeed(444);
	fractnoise.SetNoiseType(FastNoise::SimplexFractal);
	fractnoise.SetFractalType(FastNoise::RigidMulti);
	fractnoise.SetFrequency(0.002);
	fractnoise.SetFractalOctaves(6);
	fractnoise.SetFractalLacunarity(2.0f);
	fractnoise.SetGradientPerturbAmp(50.f);

	float max = 1.f;
	for (int i = 0; i < sidelength; i++) {
		for (int j = 0; j < sidelength; j++) {
			float x = i; float y = j;
			cellnoise.GradientPerturb(x, y);
			//float val = sqrt(noise.GetNoise(x, y));
			float val = cellnoise.GetNoise(x, y);
			if (val > max) { max = val; }
		}
	}

	unsigned int index = 0;
	for (int i = 0; i < sidelength; i++) {
		for (int j = 0; j < sidelength; j++) {
			float x = i; float y = j;
			cellnoise.GradientPerturb(x, y);
			//float val = sqrt(noise.GetNoise(x, y));
			float val = cellnoise.GetNoise(x, y);
			val = val * (1.f / max);

			x = i; y = j;
			fractnoise.GradientPerturb(x, y);
			float fract = fractnoise.GetNoise(x, y);
			fract = (fract + 1.f) / 2.f;
			fract = glm::clamp(float(fract), 0.f, 1.f); 

			val = glm::clamp(float(val), 0.f, 1.f);
			val = 0.5 * (val + fract);
			image[index++] = val * 255.f;
		}
	}
}
