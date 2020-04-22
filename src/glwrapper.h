#define BUFFER_OFFSET(i) ((char *)NULL + (i))

struct mesh {
	GLuint VAO, VBO, EBO;
	GLenum mode; // rendering mode
	GLsizei ecount; // element count
	bool indexed;
};

struct mesh gen_patch_grid(const size_t sidelength, const float offset);

struct mesh gen_mapcube(void);

struct mesh gen_cardinal_quads(void);

void delete_mesh(const struct mesh *m);

GLuint bind_texture(const struct rawimage *image, GLenum internalformat, GLenum format, GLenum type);

GLuint bind_mipmap_texture(struct rawimage *image, GLenum internalformat, GLenum format, GLenum type);

void activate_texture(GLenum unit, GLenum target, GLuint texture); 

GLuint load_TGA_cubemap(const char *fpath[6]);

void instance_static_VAO(GLuint VAO, std::vector<glm::mat4> *transforms);
