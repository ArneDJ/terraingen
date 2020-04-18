struct shaderinfo {
	GLenum type;
	const char *fpath;
	GLuint shader;
};

class Shader {
public:
	Shader(struct shaderinfo *shaders);
	void bind(void) const { glUseProgram(program); }
	void uniform_bool(const GLchar *name, bool boolean) const
	{
		glUseProgram(program);
		glUniform1i(glGetUniformLocation(program, name), boolean);
	}
	void uniform_float(const GLchar *name, GLfloat scalar) const
	{
		glUseProgram(program);
		glUniform1f(glGetUniformLocation(program, name), scalar);
	}
	void uniform_vec3(const GLchar *name, glm::vec3 vector) const
	{
		glUseProgram(program);
		glUniform3fv(glGetUniformLocation(program, name), 1, glm::value_ptr(vector));
	}
	void uniform_mat4(const GLchar *name, glm::mat4 matrix) const
	{
		glUseProgram(program);
		glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, glm::value_ptr(matrix));
	}
	void uniform_array_mat4(const GLchar *name, size_t count, glm::mat4 *matrices) const
 	{
		glUseProgram(program);
		glUniformMatrix4fv(glGetUniformLocation(program, name), count, GL_FALSE, glm::value_ptr(matrices[0]));
 	}
private:
	GLuint program;
	GLuint loadshaders(struct shaderinfo *shaders);
	GLuint substitute(void);
};
