struct rawimage {
	unsigned char *data = nullptr;
	unsigned int nchannels;
	size_t width;
	size_t height;
};

void perlin_image(unsigned char *image, size_t sidelength, float freq);
struct rawimage gen_normalmap(const struct rawimage *heightmap);
