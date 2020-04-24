struct depthmap {
	GLuint FBO;
	GLuint texture;
	GLsizei height;
	GLsizei width;
};

class Shadow {
public:
	glm::mat4 scalebias;
	glm::mat4 shadowspace[3];
	float splitdepth[3];
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
