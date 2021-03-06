#include <iostream>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "external/fastnoise/FastNoise.h"
#include "external/heman/heman.h"
#include "imp.h"

enum {
	RED_CHANNEL = 1,
	RG_CHANNEL = 2,
	RGB_CHANNEL = 3,
	RGBA_CHANNEL = 4
};

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
	const float strength = 32.f; // sobel filter strength

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
	const float dZ = (BL + 2.f * B + BR) - (TL + 2.f * T + TR);
	const float dY = 1.f / strength;

	glm::vec3 normal(-dX, dY, dZ);
	normal = glm::normalize(normal);
	// convert to positive values to store in a texture
	normal.x = (normal.x + 1.f) / 2.f;
	normal.y = (normal.y + 1.f) / 2.f;
	normal.z = (normal.z + 1.f) / 2.f;

	return normal;
}

float sample_image(int x, int y, const struct rawimage *image, unsigned int channel)
{
	if (image->data == nullptr) {
		std::cerr << "error: no image data\n";
		return 0.f;
	}
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
	struct rawimage normalmap = {
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
	struct rawimage occlusmap = {
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

void billow_3D_image(unsigned char *image, size_t sidelength, float frequency, float cloud_distance)
{
	FastNoise billow;
	billow.SetNoiseType(FastNoise::SimplexFractal);
	billow.SetFractalType(FastNoise::Billow);
	billow.SetFrequency(frequency);
	billow.SetFractalOctaves(6);
	billow.SetFractalLacunarity(2.0f);

	const float space = cloud_distance; // space between the clouds

	unsigned int index = 0;
	for (int i = 0; i < sidelength; i++) {
		for (int j = 0; j < sidelength; j++) {
			for (int k = 0; k < sidelength; k++) {
				float p = (billow.GetNoise(i, j, k) + 1.f) / 2.f;
				p = p - space;
				image[index++] = glm::clamp(p, 0.f, 1.f) * 255.f;
			}
		}
	}

}

void terrain_image(unsigned char *image, size_t sidelength, long seed, float freq)
{
	// detail
	FastNoise billow;
	billow.SetSeed(seed);
	billow.SetNoiseType(FastNoise::SimplexFractal);
	billow.SetFractalType(FastNoise::Billow);
	billow.SetFrequency(0.01f*freq);
	billow.SetFractalOctaves(6);
	billow.SetFractalLacunarity(2.0f);
	billow.SetGradientPerturbAmp(40.f);

	// ridges
	FastNoise cellnoise;
	cellnoise.SetSeed(seed);
	cellnoise.SetNoiseType(FastNoise::Cellular);
	cellnoise.SetCellularDistanceFunction(FastNoise::Euclidean);
	cellnoise.SetFrequency(0.01f*freq);
	cellnoise.SetCellularReturnType(FastNoise::Distance2Add);
	cellnoise.SetGradientPerturbAmp(30.f);

	// mask perturb
	FastNoise perturb;
	perturb.SetSeed(seed);
	perturb.SetNoiseType(FastNoise::SimplexFractal);
	perturb.SetFractalType(FastNoise::FBM);
	perturb.SetFrequency(0.002f*freq);
	perturb.SetFractalOctaves(5);
	perturb.SetFractalLacunarity(2.0f);
	perturb.SetGradientPerturbAmp(300.f);

	const float mountain_amp = 1.0f; // best values between 0.4 and 1.0
	const float field_amp = 0.3f; // best values between 0.2 and 0.4

	float max = 1.f;
	for (int i = 0; i < sidelength; i++) {
		for (int j = 0; j < sidelength; j++) {
			float x = i; float y = j;
			cellnoise.GradientPerturbFractal(x, y);
			float val = cellnoise.GetNoise(x, y);
			if (val > max) { max = val; }
		}
	}

	const glm::vec2 center = glm::vec2(0.5f*float(sidelength), 0.5f*float(sidelength));
	unsigned int index = 0;
	for (int i = 0; i < sidelength; i++) {
		for (int j = 0; j < sidelength; j++) {
			float x = i; float y = j;
			billow.GradientPerturbFractal(x, y);
			float detail = 1.f - (billow.GetNoise(x, y) + 1.f) / 2.f;

			x = i; y = j;
			cellnoise.GradientPerturbFractal(x, y);
			float ridge = cellnoise.GetNoise(x, y) / max;

			x = i; y = j;
			perturb.GradientPerturbFractal(x, y);

			// add detail
			float height = glm::mix(detail, ridge, 0.9f);

			// aply mask
			float mask = glm::distance(center, glm::vec2(float(x), float(y))) / float(0.5f*sidelength);
			mask = glm::smoothstep(0.4f, 0.8f, mask);
			mask = glm::clamp(mask, field_amp, mountain_amp);

			height *= mask;

			if (i > (sidelength-4) || j > (sidelength-4)) {
				image[index++] = 0.f;
			} else {
				image[index++] = height * 255.f;
			}
		}
	}
}
