struct surface {
	GLuint grass;
	GLuint dirt;
	GLuint stone;
	GLuint snow;
};

class Terrain {
public:
	float amplitude;
	size_t sidelength;
	float patchoffset;
public:
	Terrain(size_t sidelen, float patchoffst, float amp);
	~Terrain(void);
	void genheightmap(size_t imageres, float freq);
	void gennormalmap(void);
	void genocclusmap(void);
	void display(void);
	float sampleheight(float x, float z);
	float sampleslope(float x, float z);
private:
	GLuint heightmap;
	GLuint normalmap;
	GLuint occlusmap;
	GLuint detailmap;
	struct rawimage heightimage;
	struct rawimage normalimage;
	struct rawimage occlusimage;
	struct mesh termesh;
	struct surface tersurface;
};

#define frand(x) (rand() / (1. + RAND_MAX) * x)

class Grass {
public:
	Grass(Terrain *ter, size_t density, GLuint texturebind)
	{
		quads = gen_cardinal_quads();
		texture = texturebind;

		std::vector<glm::mat4> transforms;
		transforms.reserve(density);
		for (int i = 0; i < density; i++) {
			float x = frand(1024.f);
			float z = frand(1024.f);
			float y = ter->sampleheight(x, z);
			if (ter->sampleslope(x, z) < 0.6 && y < 0.4) {
				glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(x*2.f, ter->amplitude*y+frand(1.5f), z*2.f));
				float angle = frand(1.57);
				glm::mat4 R = glm::rotate(angle, glm::vec3(0.0, 1.0, 0.0));
				glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(4.f, 2.f, 4.f));
				transforms.push_back(T * R * S);
			}
		}
		instancecount = transforms.size();
		transforms.resize(instancecount);
		instance_static_VAO(quads.VAO, &transforms);
	}
	void display(void) 
	{
		glDisable(GL_CULL_FACE);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, texture);
		glBindVertexArray(quads.VAO);
		glDrawElementsInstanced(quads.mode, quads.ecount, GL_UNSIGNED_SHORT, NULL, instancecount);
		glEnable(GL_CULL_FACE);
	}
private:
	size_t instancecount;
	struct mesh quads;
	GLuint texture;
};
