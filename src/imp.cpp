#include <iostream>
#include <noise/noise.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

void perlin_image(unsigned char *image, size_t sidelength)
{
	using namespace noise;

	module::Perlin perlin;
	perlin.SetFrequency(0.01f);
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
