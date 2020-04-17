struct mesh {
	GLuint VAO, VBO, EBO;
	GLenum mode; // rendering mode
	GLsizei ecount; // element count
	bool indexed;
};

struct mesh gen_patch_grid(const size_t sidelength, const float offset);

struct mesh gen_mapcube(void);

struct mesh gen_cardinal_quads(void);

void instance_static_VAO(GLuint VAO, std::vector<glm::mat4> *transforms);
