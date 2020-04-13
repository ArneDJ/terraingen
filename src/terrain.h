class Terrain {
public:
	float amplitude;
public:
	Terrain(size_t sidelen, float patchoffst, float amp);
	~Terrain(void);
	void genheightmap(size_t imageres, float freq);
	void gennormalmap(void);
	void display(void);
private:
	size_t sidelength;
	float patchoffset;
private:
	GLuint heightmap;
	GLuint normalmap;
	struct rawimage heightimage;
	struct rawimage normalimage;
	struct mesh termesh;
};
