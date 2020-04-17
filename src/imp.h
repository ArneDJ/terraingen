struct rawimage {
	unsigned char *data = nullptr;
	unsigned int nchannels;
	size_t width;
	size_t height;
};

void terrain_image(unsigned char *image, size_t sidelength, float freq);
struct rawimage gen_normalmap(const struct rawimage *heightmap);
struct rawimage gen_occlusmap(const struct rawimage *heightmap);
float sample_image(int x, int y, const struct rawimage *image, unsigned int channel);
