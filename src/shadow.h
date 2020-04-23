struct cascade {
	glm::vec3 frustumcorners[8];
	glm::mat4 orthoproject;
	float near; // from the camera frustum
	float far; // from the camera frustum
};

struct depthmap {
 GLuint FBO;
 GLuint texture;
 GLsizei height;
 GLsizei width;
};



enum splitsection {
	SPLITSECTION_NEAREST = 0,
	SPLITSECTION_MIDDLE,
	SPLITSECTION_FARTHEST
};

class Shadow {
public:
	glm::mat4 scalebias;
	glm::mat4 shadowspace[3];

	Shadow(glm::mat4 view, float near, float far);
	void update(const Camera *cam, float fov, float aspect, float near, float far);
	void enable(void) const;
	void disable(void) const;
	void bindtextures(GLenum near, GLenum middle, GLenum far) const; 
	void binddepth(enum splitsection section) const;

private:
	glm::mat4 lightview;
	struct depthmap depth[3];
	struct cascade nearby;
	struct cascade midway;
	struct cascade farthest;

	void orthoprojection(void);
};
