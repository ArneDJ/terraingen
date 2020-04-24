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
	const glm::mat4 scalebias = glm::mat4(
		glm::vec4(0.5f, 0.0f, 0.0f, 0.0f), 
		glm::vec4(0.0f, 0.5f, 0.0f, 0.0f), 
		glm::vec4(0.0f, 0.0f, 0.5f, 0.0f), 
		glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)
	);
	glm::mat4 shadowspace[SHADOW_MAP_CASCADE_COUNT];
	float splitdepth[SHADOW_MAP_CASCADE_COUNT];
public:
	Shadow(size_t texture_size);
	void update(const Camera *cam, glm::vec3 lightpos);
	void enable(void) const;
	void disable(void) const;
	void bindtextures(void) const; 
	void binddepth(unsigned int section) const;

private:
	struct depthmap depth;
	void orthoprojection(const Camera *cam, glm::vec3 lightpos);
};
