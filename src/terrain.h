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
	float mapratio; // ratio terrain mesh size / heightmap resolution
	GLuint heightmap;
	GLuint normalmap;
	GLuint occlusmap;
	GLuint detailmap;
public:
	Terrain(size_t sidelen, float patchoffst, float amp);
	~Terrain(void);
	void genheightmap(size_t imageres, float freq);
	void gennormalmap(void);
	void genocclusmap(void);
	void display(void) const;
	float sampleheight(float x, float z) const;
	float sampleslope(float x, float z) const;
private:
	struct rawimage heightimage;
	struct rawimage normalimage;
	struct rawimage occlusimage;
	struct mesh termesh;
	struct surface tersurface;
};

class Grass {
public:
	Grass(const Terrain *ter, size_t density, GLuint texturebind, GLuint norm, GLuint occlus);
	~Grass(void) 
	{
		delete_mesh(&quads);
	}
	void display(void) const;
private:
	size_t instancecount;
	struct mesh quads;
	GLuint texture;
	GLuint normalmap;
	GLuint occlusmap;
};

class Skybox {
public:
	Skybox(GLuint cubemapbind)
	{
		cubemap = cubemapbind;
		cube = gen_mapcube();
	};
	void display(void) const;
private:
	GLuint cubemap;
	struct mesh cube;
};
