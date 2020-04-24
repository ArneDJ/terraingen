#define SHADOW_MAP_CASCADE_COUNT 4

struct depthmap {
	GLuint FBO;
	GLuint texture;
	GLsizei height;
	GLsizei width;
};

class Shadow {
public:
	const uint16_t cascade_count = SHADOW_MAP_CASCADE_COUNT;
	glm::mat4 scalebias;
	glm::mat4 shadowspace[SHADOW_MAP_CASCADE_COUNT];
	float splitdepth[SHADOW_MAP_CASCADE_COUNT];
public:
	Shadow(glm::mat4 view, float near, float far);
	void update(const Camera *cam, float fov, float aspect, float near, float far);
	void enable(void) const;
	void disable(void) const;
	void bindtextures(void) const; 
	void binddepth(unsigned int section) const;

private:
	glm::mat4 lightview;
	struct depthmap depth;

	void orthoprojection(const Camera *cam);
};
