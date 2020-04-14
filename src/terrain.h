class Terrain {
public:
	float amplitude;
public:
	Terrain(size_t sidelen, float patchoffst, float amp);
	~Terrain(void);
	void genheightmap(size_t imageres, float freq);
	void gennormalmap(void);
	void genocclusmap(void);
	void display(void);
private:
	size_t sidelength;
	float patchoffset;
private:
	GLuint heightmap;
	GLuint normalmap;
	GLuint occlusmap;
	struct rawimage heightimage;
	struct rawimage normalimage;
	struct rawimage occlusimage;
	struct mesh termesh;
};
